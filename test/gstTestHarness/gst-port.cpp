/*
 * If not stated otherwise in this file or this component's license file the
 * following copyright and licenses apply:
 *
 * Copyright 2024 RDK Management
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
#include "gst-port.h"
#include "gst-utils.h"
#include <gst/gst.h>
#include <gst/app/gstappsrc.h>
#include <gst/gstdebugutils.h>
#include <inttypes.h>
#include <thread>

#define MY_PIPELINE_NAME "test-pipeline"

// Logging category for this module
GST_DEBUG_CATEGORY_STATIC(gstport_cat);
#define GST_CAT_DEFAULT gstport_cat

static void need_data_cb(GstElement *appSrc, guint length, class Pipeline *pipeline );
static void enough_data_cb(GstElement *appSrc, class Pipeline *pipeline );
static gboolean appsrc_seek_cb(GstElement * appSrc, guint64 offset, class Pipeline *pipeline );
static void found_source_cb(GObject * object, GObject * orig, GParamSpec * pspec, class Pipeline *pipeline );

gboolean bus_message_cb(GstBus * bus, GstMessage * msg, class Pipeline *pipeline )
{ // C to C++ glue
	return pipeline->bus_message( bus, msg );
}

Pipeline::Pipeline( class PipelineContext *context ) : context(context), pipeline(gst_pipeline_new( MY_PIPELINE_NAME )), bus(gst_pipeline_get_bus(GST_PIPELINE(pipeline)))
{
	GstBus *bus = gst_element_get_bus(pipeline);
	gst_bus_add_watch( bus, (GstBusFunc)bus_message_cb, this );
	gst_object_unref(bus);
	playbin = NULL;
	appsrc = NULL;
	caps[eMEDIATYPE_AUDIO] = NULL;
	caps[eMEDIATYPE_VIDEO] = NULL;
	injectedSeconds = 0.0;
	// Initialize logging category once
	GST_DEBUG_CATEGORY_INIT(gstport_cat, "gstport", 0, "Port/pipeline module logs");
	GST_INFO_OBJECT(pipeline, "Pipeline created: %s", MY_PIPELINE_NAME);
}

void Pipeline::ScheduleSeek( const SeekParam &seekParam )
{
	std::lock_guard<std::mutex> lock(context->segment_seek_mutex);
	if( context->mSegmentEndSeekQueue.size()==0 )
	{ // workaround: store pair of seek positions at start for use with each appsrc
		context->mSegmentEndSeekQueue.push(seekParam);
	}
	context->mSegmentEndSeekQueue.push(seekParam);
}

size_t Pipeline::GetNumPendingSeek(void) const
{
	std::lock_guard<std::mutex> lock(context->segment_seek_mutex);
	return context->mSegmentEndSeekQueue.size();
}

void Pipeline::Configure( MediaType /*mediaType*/ )
{
    if (playbin) { GST_DEBUG_OBJECT(pipeline, "Configure: playbin already present"); return; }
    playbin = gst_element_factory_make("playbin", NULL);
    if (!playbin) { GST_ERROR("Failed to create playbin"); return; }
    gboolean rc = gst_bin_add(GST_BIN(pipeline), playbin);
    if (!rc) { GST_ERROR("Failed to add playbin to pipeline"); return; }
    g_object_set(playbin, "uri", "appsrc://", NULL);
    g_signal_connect(playbin, "deep-notify::source", G_CALLBACK(found_source_cb), this);
    if (!gstutils_quiet) {
        g_signal_connect(playbin, "element_setup", G_CALLBACK(gstutils_element_setup_cb), this);
        gstutils_DumpFlags(playbin);
    }
}

void Pipeline::SetCaps( MediaType mediaType, const Mp4Demux *mp4Demux )
{
	if (appsrc && mp4Demux)
	{
		mp4Demux->setCaps(GST_APP_SRC(appsrc));
		caps[mediaType] = gst_app_src_get_caps(GST_APP_SRC(appsrc));
		gchar *caps_str = gst_caps_to_string(caps[mediaType]);
		printf( "SetCaps: %d = %s\n", mediaType, caps_str );
		g_free(caps_str); // Free the allocated string
	}
}

Pipeline::~Pipeline()
{
	gst_bus_remove_watch(bus);
	gst_object_unref(bus); // release ref acquired via gst_pipeline_get_bus
	gst_element_set_state(pipeline, GST_STATE_NULL);
	gst_object_unref(pipeline);
}

void Pipeline::SetPipelineState(PipelineState pipelineState )
{
	GST_INFO_OBJECT(pipeline, "Set state -> %s", gst_element_state_get_name((GstState)pipelineState));
	gst_element_set_state( pipeline, (GstState) pipelineState );
}

PipelineState Pipeline::GetPipelineState( void )
{
	GstState state;
	GstState pending;
	GstClockTime timeout = 0; // non-blocking
	gst_element_get_state( pipeline, &state, &pending, timeout );
	GST_DEBUG_OBJECT(pipeline, "Get state: %s (pending %s)",
					 gst_element_state_get_name(state),
					 gst_element_state_get_name(pending));
	return (PipelineState) state;
}

void Pipeline::SendBufferMP4( MediaType mediaType, gpointer ptr, gsize len, double duration )
{
    if( appsrc && ptr )
	{
		GstBuffer *buf = gst_buffer_new_wrapped_full((GstMemoryFlags)0, ptr, len, 0, len, NULL, (GDestroyNotify)g_free);
		GstFlowReturn ret = gst_app_src_push_buffer(GST_APP_SRC(appsrc), buf);
		if (ret == GST_FLOW_OK) injectedSeconds += duration; else GST_ERROR_OBJECT(appsrc, "push_buffer failed - %d", ret);
	}
}

void Pipeline::SendBufferES( MediaType mediaType, gpointer ptr, gsize len, double duration, double pts, double dts, GstStructure *metadata )
{
    if( appsrc && ptr )
	{
		gst_app_src_set_caps(GST_APP_SRC(appsrc),caps[mediaType]);
		GstBuffer *buf = gst_buffer_new_wrapped_full((GstMemoryFlags)0, ptr, len, 0, len, NULL, (GDestroyNotify)g_free);
		GST_BUFFER_PTS(buf) = (GstClockTime)(pts * GST_SECOND);
		GST_BUFFER_DTS(buf) = (GstClockTime)(dts * GST_SECOND);
		GST_BUFFER_DURATION(buf) = (GstClockTime)(duration * GST_SECOND);
		if (metadata) gst_buffer_add_protection_meta(buf, metadata);
		GstFlowReturn ret = gst_app_src_push_buffer(GST_APP_SRC(appsrc), buf);
		if (ret == GST_FLOW_OK) injectedSeconds += duration; else GST_ERROR_OBJECT(appsrc, "push_buffer failed - %d", ret);
	}
}

void Pipeline::SendGap( MediaType /*mediaType*/, double pts, double durationSeconds )
{
    if( appsrc )
	{
		GST_INFO_OBJECT(pipeline, "SendGap pts=%f dur=%f", pts, durationSeconds);
		GstEvent *ev = gst_event_new_gap((GstClockTime)(pts * GST_SECOND), (GstClockTime)(durationSeconds * GST_SECOND));
		if (!gst_element_send_event(GST_ELEMENT(appsrc), ev)) GST_WARNING_OBJECT(appsrc, "Failed to send GAP event");
	}
}

void Pipeline::SendEOS( MediaType /*mediaType*/ )
{
	if (appsrc)
	{
		gst_app_src_end_of_stream(GST_APP_SRC(appsrc));
	}
}

bool Pipeline::DoSeekNow(const SeekParam& req)
{
	const gint64 start = (gint64)(req.start_s * GST_SECOND);
	const gint64 stop  = (gint64)(req.stop_s  * GST_SECOND);
	GST_INFO_OBJECT(pipeline, "DoSeekNow rate=%.2f start=%" GST_TIME_FORMAT " stop=%" GST_TIME_FORMAT " flags=0x%x", req.rate, GST_TIME_ARGS(start), GST_TIME_ARGS(stop), req.flags);
//	if (req.flags & GST_SEEK_FLAG_FLUSH) {
//		mediaStream[eMEDIATYPE_AUDIO]->ClearInjectedSeconds();
//		mediaStream[eMEDIATYPE_VIDEO]->ClearInjectedSeconds();
//	}
	const gboolean ok = gst_element_seek(
										 pipeline,
										 req.rate,
										 GST_FORMAT_TIME,
										 req.flags,
										 GST_SEEK_TYPE_SET, start,
										 (req.stop_s > 0.0 ? GST_SEEK_TYPE_SET : GST_SEEK_TYPE_NONE), stop );
	if (!ok) {
		GST_ERROR_OBJECT(pipeline, "gst_element_seek failed");
		return false;
	}
	return true;
}

void Pipeline::Reset( void )
{
	std::lock_guard<std::mutex> lock(context->segment_seek_mutex);
	std::queue<SeekParam> empty;
	std::swap( context->mSegmentEndSeekQueue, empty );
	GST_DEBUG_OBJECT(pipeline, "Reset seek queue");
}

void Pipeline::HandleGstMessageError( GstMessage *msg, const char *messageName )
{
	GError *error = NULL;
	gchar *dbg_info = NULL;
	gst_message_parse_error(msg, &error, &dbg_info);
	GST_ERROR("%s: from %s %s", messageName, GST_OBJECT_NAME(msg->src), error->message);
	g_clear_error(&error);
	g_free(dbg_info);
}

void Pipeline::HandleGstMessageWarning( GstMessage *msg, const char *messageName )
{
	GError *error = NULL;
	gchar *dbg_info = NULL;
	gst_message_parse_warning(msg, &error, &dbg_info);
	GST_WARNING("%s: from %s %s", messageName, GST_OBJECT_NAME(msg->src), error->message);
	g_clear_error(&error);
	g_free(dbg_info);
}

void Pipeline::ReachedEOS( void )
{
	std::lock_guard<std::mutex> lock(context->segment_seek_mutex);
	if (!context->mSegmentEndSeekQueue.empty()) {
		SeekParam p = context->mSegmentEndSeekQueue.front();
		SeekParam req;
		req.rate    = 1.0;
		req.start_s = p.start_s;
		req.stop_s  = p.stop_s;
		req.flags   = p.flags;
		req.reason  = SeekParam::SegmentEnd;
		(void)DoSeekNow(req);
		context->mSegmentEndSeekQueue.pop();
	}
}

void Pipeline::HandleGstMessageEOS( GstMessage *msg, const char *messageName )
{ // received after all sinks are EOS
	GST_INFO( "%s from %s", messageName, GST_OBJECT_NAME(msg->src) );
	ReachedEOS();
}

void Pipeline::HandleGstMessageSegmentDone( GstMessage *message, const char *messageName )
{ // received after all sinks are EOS
	GstFormat format;
	gint64 position;
	gst_message_parse_segment_done( message, &format, &position );
	if (format != GST_FORMAT_TIME) {
		GST_WARNING("SegmentDone format != TIME");
		return;
	}
	GST_INFO( "%s from %s position=%" GST_TIME_FORMAT,
			 messageName,
			 GST_OBJECT_NAME(message->src),
			 GST_TIME_ARGS(position) ); // this is time of the START of just completed segment
	ReachedEOS();
}

gboolean Pipeline::bus_message( _GstBus * bus, _GstMessage * msg )
{
	GstMessageType messageType = GST_MESSAGE_TYPE(msg);
	const char *messageName = gst_message_type_get_name( messageType );
	switch( messageType )
	{
		case GST_MESSAGE_ERROR:
			HandleGstMessageError( msg, messageName );
			break;
		case GST_MESSAGE_WARNING:
			HandleGstMessageWarning( msg, messageName );
			break;
		case GST_MESSAGE_SEGMENT_DONE:
			HandleGstMessageSegmentDone( msg, messageName );
			break;
		case GST_MESSAGE_EOS:
			HandleGstMessageEOS( msg, messageName );
			break;
		case GST_MESSAGE_STATE_CHANGED:
			gstutils_HandleGstMessageStateChanged( msg, messageName );
			break;
		case GST_MESSAGE_TAG:
			gstutils_HandleGstMessageTag( msg, messageName );
			break;
		case GST_MESSAGE_QOS:
			gstutils_HandleGstMessageQOS( msg, messageName );
			break;
		case GST_MESSAGE_STREAM_STATUS:
			gstutils_HandleGstMessageStreamStatus( msg, messageName );
			break;
		default:
			//g_print( "%s\n", messageName );
			break;
	}
	return TRUE;
}

void Pipeline::DumpDOT( void )
{
	gchar *graphviz = gst_debug_bin_to_dot_data( (GstBin *)pipeline, GST_DEBUG_GRAPH_SHOW_ALL );
	// refer: https://graphviz.org/
	// brew install graphviz
	// dot -Tsvg gst-test.dot  > gst-test.svg
	FILE *f = fopen( "gst-test.dot", "wb" );
	if( f )
	{
		fputs( graphviz, f );
		fclose( f );
	}
	g_free( graphviz );
}

void Pipeline::Step( void )
{
	GST_INFO_OBJECT(pipeline, "Step");
	gst_element_send_event( pipeline, gst_event_new_step(GST_FORMAT_BUFFERS, 1, 1, TRUE, FALSE) );
}

void Pipeline::InstantaneousRateChange( double newRate )
{
	GST_INFO_OBJECT(pipeline, "InstantaneousRateChange(%lf)", newRate );
	auto rc = gst_element_seek(
							   GST_ELEMENT(pipeline),
							   newRate,
							   GST_FORMAT_TIME,
							   GST_SEEK_FLAG_INSTANT_RATE_CHANGE,
							   GST_SEEK_TYPE_NONE, 0,
							   GST_SEEK_TYPE_NONE, 0 );
	if (!rc) {
		GST_ERROR_OBJECT(pipeline, "Instantaneous rate change seek failed");
		return;
	}
}

double Pipeline::GetInjectedSeconds( MediaType /*mediaType*/ ) const
{
	return injectedSeconds;
}

long long Pipeline::GetPositionMilliseconds( MediaType /*mediaType*/ ) const
{
    long long ms = -1;
    if (playbin) {
        gint64 pos = GST_CLOCK_TIME_NONE;
        if (gst_element_query_position(playbin, GST_FORMAT_TIME, &pos)) ms = GST_TIME_AS_MSECONDS(pos);
    }
    return ms;
}

static void need_data_cb(GstElement *src, guint length, class Pipeline *pipeline)
{
    if (pipeline && pipeline->context)
	{
		pipeline->context->NeedData(eMEDIATYPE_VIDEO);
		pipeline->context->NeedData(eMEDIATYPE_AUDIO);
	}
}

static void enough_data_cb(GstElement *src, class Pipeline *pipeline)
{
    if (pipeline && pipeline->context)
	{
		//pipeline->context->EnoughData(eMEDIATYPE_VIDEO);
		//pipeline->context->EnoughData(eMEDIATYPE_AUDIO);
	}
}

static gboolean appsrc_seek_cb(GstElement *src, guint64 offset, class Pipeline *pipeline)
{
    GST_INFO_OBJECT(src, "appsrc_seek pos=%" GST_TIME_FORMAT, GST_TIME_ARGS(offset));
    return TRUE;
}

static void found_source_cb(GObject *object, GObject *orig, GParamSpec *pspec, class Pipeline *pipeline)
{
    if (!pipeline) return;
    g_object_get(orig, pspec->name, &pipeline->appsrc, NULL);
	if (!GST_IS_APP_SRC(pipeline->appsrc)) {
		GST_ERROR("deep-notify source is not appsrc");
		pipeline->appsrc = NULL;
		return;
	}
    g_object_set(pipeline->appsrc, "min-percent", 50, NULL);
    g_signal_connect(pipeline->appsrc, "need-data", G_CALLBACK(need_data_cb), pipeline);
    g_signal_connect(pipeline->appsrc, "enough-data", G_CALLBACK(enough_data_cb), pipeline);
    g_signal_connect(pipeline->appsrc, "seek-data", G_CALLBACK(appsrc_seek_cb), pipeline);
    gst_app_src_set_stream_type(GST_APP_SRC(pipeline->appsrc), GST_APP_STREAM_TYPE_SEEKABLE);
    g_object_set(pipeline->appsrc, "format", GST_FORMAT_TIME, NULL);
    g_object_set(pipeline->appsrc, "typefind", TRUE, NULL);
    if (pipeline->context) {
        std::lock_guard<std::mutex> lock(pipeline->context->segment_seek_mutex);
        if (!pipeline->context->mSegmentEndSeekQueue.empty()) {
            SeekParam param = pipeline->context->mSegmentEndSeekQueue.front();
            pipeline->context->mSegmentEndSeekQueue.pop();
			pipeline->context->OnAppsrcReady(eMEDIATYPE_VIDEO, param);
			pipeline->context->OnAppsrcReady(eMEDIATYPE_AUDIO, param);
        }
    }
}
