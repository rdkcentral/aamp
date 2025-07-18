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
#include <inttypes.h>
#include <queue>
#include <thread>

#define MY_PIPELINE_NAME "test-pipeline"

static void need_data_cb(GstElement *appSrc, guint length, MediaStream *stream );
static void enough_data_cb(GstElement *appSrc, MediaStream *stream );
static gboolean appsrc_seek_cb(GstElement * appSrc, guint64 offset, MediaStream *stream );
static void found_source_cb(GObject * object, GObject * orig, GParamSpec * pspec, class MediaStream *stream );

class MediaStream
{
public:
	MediaStream( MediaType mediaType, class PipelineContext *context ) : isConfigured(false), playbin(NULL), appsrc(NULL), injectedSeconds(), context(context), mediaType(mediaType), pad(NULL)
	{
	}
	
	~MediaStream( void )
	{
	}
	
	const char *GetMediaTypeAsString( void )
	{
		return gstutils_GetMediaTypeName(mediaType);
	}
	
	void SendBuffer( gpointer ptr, gsize len, double duration )
	{
		if( ptr )
		{
			GstBuffer *gstBuffer = gst_buffer_new_wrapped( ptr, len);
			GstFlowReturn ret = gst_app_src_push_buffer( GST_APP_SRC(appsrc), gstBuffer );
			switch( ret )
			{
				case GST_FLOW_OK:
					injectedSeconds += duration;
					break;
				default:
					g_print( "gst_app_src_push_buffer failed - %d\n", ret );
					//assert(0);
					break;
			}
		}
	}
	
	void SendBuffer( gpointer ptr, gsize len, double duration, double pts, double dts, GstStructure *metadata=NULL )
	{
		if( ptr )
		{
			GstBuffer *gstBuffer = gst_buffer_new_wrapped( ptr, len );
			GST_BUFFER_PTS(gstBuffer) = (GstClockTime)(pts * GST_SECOND);
			GST_BUFFER_DTS(gstBuffer) = (GstClockTime)(dts * GST_SECOND);
			GST_BUFFER_DURATION(gstBuffer) = (GstClockTime)(duration * 1000000000LL);
			if( metadata )
			{
				gst_buffer_add_protection_meta(gstBuffer, metadata);
			}
			GstFlowReturn ret = gst_app_src_push_buffer(GST_APP_SRC(appsrc), gstBuffer );
			switch( ret )
			{
				case GST_FLOW_OK:
					injectedSeconds += duration;
					break;
				default:
					g_print( "gst_app_src_push_buffer failed - %d\n", ret );
					assert(0);
					break;
			}
		}
	}
	
	void SendGap( double pts, double durationSeconds )
	{
		g_print( "MediaStream::SendGap(%s,pts=%f,dur=%f)\n", GetMediaTypeAsString(), pts, durationSeconds );
		GstClockTime timestamp = (GstClockTime)(pts * GST_SECOND);
		GstClockTime duration = (GstClockTime)(durationSeconds * GST_SECOND);
		GstEvent *event = gst_event_new_gap( timestamp, duration );
		if( !gst_element_send_event( GST_ELEMENT(appsrc), event) )
		{
			g_print( "Failed to send gap event\n" );
		}
	}
	
	void SendEOS( void )
	{
		g_print( "MediaStream::SendEOS %s\n", GetMediaTypeAsString() );
		gst_app_src_end_of_stream(GST_APP_SRC(appsrc));
	}
	
	/**
	 * @brief create, link, and confiugre a playbin element for specified media track
	 * @param mediaType tracktype, i.e. eMEDIATYPE_AUDIO or eMEDIATYPE_VIDEO
	 */
	void Configure( GstElement *pipeline )
	{
		g_print( "MediaStream::Configure %s\n", GetMediaTypeAsString() );
		if( isConfigured )
		{
			g_print( "NOP - aleady configured\n" );
		}
		else
		{
			isConfigured = true;
			playbin = gst_element_factory_make("playbin", NULL);
			assert( playbin );
			gboolean rc = gst_bin_add(GST_BIN(pipeline), playbin);
			assert( rc );
			g_object_set( playbin, "uri", "appsrc://", NULL);
			g_signal_connect( playbin, "deep-notify::source", G_CALLBACK(found_source_cb), this );
			if( !gstutils_quiet )
			{
				g_signal_connect( playbin, "element_setup", G_CALLBACK(gstutils_element_setup_cb), this );
				gstutils_DumpFlags( playbin );
			}
		}
	}
	
	void ClearInjectedSeconds( void )
	{
		injectedSeconds = 0;
	}
	
	double GetInjectedSeconds( void )
	{
		return injectedSeconds;
	}
	
	long long GetPositionMilliseconds( void )
	{
		long long ms = -1;
		if( playbin )
		{
			gint64 position = GST_CLOCK_TIME_NONE;
			if( gst_element_query_position(playbin, GST_FORMAT_TIME, &position) )
			{
				ms = GST_TIME_AS_MSECONDS(position);
			}
		}
		return ms;
	}
	
	void need_data( GstElement *appSrc, guint length )
	{ // noisy when time based buffering in use
		context->NeedData( mediaType );
	}
	
	void enough_data( GstElement *appSrc )
	{
		g_print( "MediaStream::enough_data %s", GetMediaTypeAsString() );
		context->EnoughData( mediaType );
	}
	
	gboolean appsrc_seek( GstElement *appSrc, guint64 offset )
	{
		g_print( "MediaStream::appsrc_seek %s position=%" GST_TIME_FORMAT "\n", GetMediaTypeAsString(), GST_TIME_ARGS(offset) );
		return TRUE; // success
	}
	
	/**
	 * @brief apply/update caps for audio/video to be presented
	 * @param pipeline AV pipeline to update
	 * @param info contains metadata extracted from mp4 initialization fragment
	 */
	void SetCaps( const Mp4Demux *mp4Demux )
	{
		mp4Demux->setCaps( GST_APP_SRC(appsrc) );
	}
	
	void Seek( const SeekParam &param )
	{
		gint64 start = (gint64)(param.start_s*GST_SECOND);
		gint64 stop = (gint64)(param.stop_s*GST_SECOND);
		g_print( "MediaStream::Seek %s flags=%d start=%" GST_TIME_FORMAT " stop=%" GST_TIME_FORMAT "\n",
				GetMediaTypeAsString(),
				param.flags,
				GST_TIME_ARGS(start),
				GST_TIME_ARGS(stop) );
		gboolean rc = gst_element_seek(
									   appsrc,
									   1.0, // rate
									   GST_FORMAT_TIME,
									   param.flags,
									   GST_SEEK_TYPE_SET, start,
									   GST_SEEK_TYPE_SET, stop );
		assert( rc );
	}
	
	void found_source( GObject * object, GObject * orig, GParamSpec * pspec )
	{
		g_print( "MediaStream::found_source %s\n", GetMediaTypeAsString() );
		g_object_get( orig, pspec->name, &appsrc, NULL );
		
		// configuration to drive need-data and enough-data signaling
		switch( mediaType )
		{
			case eMEDIATYPE_VIDEO:
				g_object_set(appsrc, "max-bytes", 12582912, NULL ); // default = 200000
				break;
			case eMEDIATYPE_AUDIO:
				g_object_set(appsrc, "max-bytes", 1536000, NULL ); // default = 200000
				break;
			default:
				break;
		}
		g_object_set(appsrc, "min-percent", 50, NULL ); // default = 0
		
		g_signal_connect(appsrc, "need-data", G_CALLBACK(need_data_cb), this );
		g_signal_connect(appsrc, "enough-data", G_CALLBACK(enough_data_cb), this );
		
		g_signal_connect(appsrc, "seek-data", G_CALLBACK(appsrc_seek_cb), this);
		gst_app_src_set_stream_type( GST_APP_SRC(appsrc), GST_APP_STREAM_TYPE_SEEKABLE );
		g_object_set(appsrc, "format", GST_FORMAT_TIME, NULL);
		
		// define or discover expected media formats
		if( 0 )
		{
			GstCaps * caps = gst_caps_new_simple("video/quicktime", NULL, NULL);
			gst_app_src_set_caps(GST_APP_SRC(appsrc), caps );
			gst_caps_unref(caps);
		}
		else
		{
			g_object_set(appsrc, "typefind", TRUE, NULL);
		}
		pad = gst_element_get_static_pad(appsrc, "src");
		
		// seek here avoids freeze at start for non-zero first_pts
		assert( context->mSegmentEndSeekQueue.size()>0 );
		SeekParam param = context->mSegmentEndSeekQueue.front();
		context->mSegmentEndSeekQueue.pop();
		Seek( param );
	}
	
	MediaStream(const MediaStream&)=delete; // copy constructor
	MediaStream& operator=(const MediaStream&)=delete; // copy assignment operator
	
private:
	GstPad * pad;
	bool isConfigured; // avoid double configure
	double injectedSeconds;
	class PipelineContext *context;
	MediaType mediaType;
	GstElement *playbin;
	GstElement *appsrc;
};

/**
 * @brief handle gstreamer signal that buffers need to be filled - start/continue injecting AV data
 *
 * @param appSrc element that emitted the signal
 * @param length number of bytes needed, or -1 for "any"
 */
static void need_data_cb(GstElement *appSrc, guint length, MediaStream *stream )
{ // C to C++ glue
	stream->need_data( appSrc, length );
}

/**
 * @brief handle gstreamer signal that buffers are sufficiently full - stop injecting AV data
 * @param appSrc element that emitted the signal
 */
static void enough_data_cb(GstElement *appSrc, MediaStream *stream )
{ // C to C++ glue
	stream->enough_data( appSrc );
}

/**
 * @brief sent when a seek event reaches the appsrc - called when appsrc wants us to return data from a new position with the next call to push-buffer.
 *
 * @param appSrc element that emitted the signal
 * @param offset seek target
 * @return TRUE if seek successful
 */
static gboolean appsrc_seek_cb( GstElement * appSrc, guint64 offset, MediaStream *stream )
{ // C to C++ glue
	return stream->appsrc_seek( appSrc, offset );
}

/**
 * @brief handle gstreamer signal that an appropriate sink source has been identified, and can now be tracked/used by app
 */
static void found_source_cb(GObject * object, GObject * orig, GParamSpec * pspec, class MediaStream *stream )
{ // C to C++ glue
	stream->found_source( object, orig, pspec );
}

gboolean bus_message_cb(GstBus * bus, GstMessage * msg, class Pipeline *pipeline )
{ // C to C++ glue
	return pipeline->bus_message( bus, msg );
}

Pipeline::Pipeline( class PipelineContext *context ) : context(context), pipeline(gst_pipeline_new( MY_PIPELINE_NAME )), bus(gst_pipeline_get_bus(GST_PIPELINE(pipeline)))
{
	GstBus *bus = gst_element_get_bus(pipeline);
	gst_bus_add_watch( bus, (GstBusFunc)bus_message_cb, this );
	gst_object_unref(bus);
	for( int i=0; i<NUM_MEDIA_TYPES; i++ )
	{
		mediaStream[i] = new MediaStream( (MediaType)i, context );
	}
}

void Pipeline::ScheduleSeek( const SeekParam &seekParam )
{
	if( context->mSegmentEndSeekQueue.size()==0 )
	{ // workaround: store pair of seek positions at start, for use with each appsrc
		context->mSegmentEndSeekQueue.push(seekParam);
	}
	context->mSegmentEndSeekQueue.push(seekParam);
}

size_t Pipeline::GetNumPendingSeek(void)
{
	return context->mSegmentEndSeekQueue.size();
}

void Pipeline::Configure( MediaType mediaType )
{
	mediaStream[mediaType]->Configure(pipeline);
}

void Pipeline::SetCaps( MediaType mediaType, const Mp4Demux *mp4Demux )
{
	mediaStream[mediaType]->SetCaps(mp4Demux);
}

Pipeline::~Pipeline()
{
	gst_bus_remove_watch(bus);
	gst_element_set_state(pipeline, GST_STATE_NULL);
	for( int i=0; i<NUM_MEDIA_TYPES; i++ )
	{
		delete( mediaStream[i] );
	}
	gst_object_unref(pipeline);
}

void Pipeline::SetPipelineState(PipelineState pipelineState )
{
	gst_element_set_state( pipeline, (GstState) pipelineState );
}

PipelineState Pipeline::GetPipelineState( void )
{
	GstState state;
	GstState pending;
	GstClockTime timeout = 0;
	gst_element_get_state( pipeline, &state, &pending, timeout );
	return (PipelineState) state;
}

void Pipeline::SendBufferMP4( MediaType mediaType, gpointer ptr, gsize len, double duration, const char * url )
{
	if( url )
	{
		g_print( "Pipeline::SendBuffer %s len=%zu %s\n", gstutils_GetMediaTypeName(mediaType), len, url );
	}
	mediaStream[mediaType]->SendBuffer(ptr,len,duration);
}
void Pipeline::SendBufferES( MediaType mediaType, gpointer ptr, gsize len, double duration, double pts, double dts, GstStructure *metadata )
{
	//g_print( "Pipeline::SendBuffer %s, len=%zu\n", gstutils_GetMediaTypeName(mediaType), len );
	mediaStream[mediaType]->SendBuffer(ptr,len,duration,pts,dts,metadata);
}

void Pipeline::SendGap( MediaType mediaType, double pts, double durationSeconds )
{
	g_print( "Pipeline::SendGap %s,pts=%f,dur=%f\n", gstutils_GetMediaTypeName(mediaType), pts, durationSeconds );
	mediaStream[mediaType]->SendGap(pts,durationSeconds);
}

void Pipeline::SendEOS( MediaType mediaType )
{
	mediaStream[mediaType]->SendEOS();
}

void Pipeline::Seek( MediaType mediaType, const SeekParam &param )
{
	mediaStream[mediaType]->ClearInjectedSeconds();
	mediaStream[mediaType]->Seek( param );
}

void Pipeline::Seek( const SeekParam &param )
{
	gint64 start = (gint64)(param.start_s*GST_SECOND);
	gint64 stop = (gint64)(param.stop_s*GST_SECOND);
	g_print( "Pipeline::Seek flags=%d start=%" GST_TIME_FORMAT " stop=%" GST_TIME_FORMAT "\n",
				param.flags, GST_TIME_ARGS(start), GST_TIME_ARGS(stop) );
	gboolean success = gst_element_seek(
					 pipeline,
					 1.0, //rate
					 GST_FORMAT_TIME,
					 param.flags,
					 GST_SEEK_TYPE_SET, start,
					 GST_SEEK_TYPE_SET, stop );
	assert( success );
	if( param.flags & GST_SEEK_FLAG_FLUSH )
	{
		mediaStream[eMEDIATYPE_AUDIO]->ClearInjectedSeconds();
		mediaStream[eMEDIATYPE_VIDEO]->ClearInjectedSeconds();
	}
}

void Pipeline::Reset( void )
{
	std::queue<SeekParam> empty;
	std::swap( context->mSegmentEndSeekQueue, empty );
}

long long Pipeline::GetPositionMilliseconds( MediaType mediaType )
{
	return mediaStream[mediaType]->GetPositionMilliseconds();
}

double Pipeline::GetInjectedSeconds( MediaType mediaType )
{
	return mediaStream[mediaType]->GetInjectedSeconds();
}

void Pipeline::HandleGstMessageError( GstMessage *msg, const char *messageName )
{
	GError *error = NULL;
	gchar *dbg_info = NULL;
	gst_message_parse_error(msg, &error, &dbg_info);
	g_printerr("%s: t%s %s\n", messageName, GST_OBJECT_NAME(msg->src), error->message);
	g_clear_error(&error);
	g_free(dbg_info);
}

void Pipeline::HandleGstMessageWarning( GstMessage *msg, const char *messageName )
{
	GError *error = NULL;
	gchar *dbg_info = NULL;
	gst_message_parse_warning(msg, &error, &dbg_info);
	g_printerr("%s: %s %s\n", messageName, GST_OBJECT_NAME(msg->src), error->message);
	g_clear_error(&error);
	g_free(dbg_info);
}

void Pipeline::ReachedEOS( void )
{
	if( context->mSegmentEndSeekQueue.size()>0 )
	{
		SeekParam param = context->mSegmentEndSeekQueue.front();
		Seek( param );
		context->mSegmentEndSeekQueue.pop();
	}
}

void Pipeline::HandleGstMessageEOS( GstMessage *msg, const char *messageName )
{ // received after all sinks are EOS
	g_print( "%s from %s\n", messageName, GST_OBJECT_NAME(msg->src) );
	ReachedEOS();
}

void Pipeline::HandleGstMessageSegmentDone( GstMessage *message, const char *messageName )
{ // received after all sinks are EOS
	GstFormat format;
	gint64 position;
	gst_message_parse_segment_done( message, &format, &position );
	assert( format == GST_FORMAT_TIME );
	g_print( "%s from %s position=%" GST_TIME_FORMAT "\n",
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
	g_print( "Pipeline::Step\n" );
	gst_element_send_event( pipeline, gst_event_new_step(GST_FORMAT_BUFFERS, 1, 1, TRUE, FALSE) );
}

void Pipeline::InstantaneousRateChange( double newRate )
{
	g_print( "Pipeline::InstantaneousRateChange(%lf)\n", newRate );
#if GST_CHECK_VERSION(1,18,0)
	auto rc = gst_element_seek(
							   GST_ELEMENT(pipeline),
							   newRate,
							   GST_FORMAT_TIME,
							   GST_SEEK_FLAG_INSTANT_RATE_CHANGE,
							   GST_SEEK_TYPE_NONE, 0,
							   GST_SEEK_TYPE_NONE, 0 );
	assert( rc );
#else
	g_print( "Instantaneous Rate Change not supported in gstreamer version %d.%d.%d, requires version 1.18\n",
			GST_VERSION_MAJOR, GST_VERSION_MINOR, GST_VERSION_MICRO );
	assert( false );
#endif
}
