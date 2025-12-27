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

static void need_data_cb(GstElement *appSrc, guint length, class MediaStream *stream );
static void enough_data_cb(GstElement *appSrc, class MediaStream *stream );
static gboolean appsrc_seek_cb( GstElement * appSrc, guint64 offset, class MediaStream *stream );
static void decodebin_pad_added_cb(GstElement * decodebin, GstPad * pad, class MediaStream *stream );

class MediaStream
{
public:
	MediaStream( MediaType mediaType, class PipelineContext *context )
	: injectedSeconds(),
	context(context),
	mediaType(mediaType),
	appsrc(NULL),
	decodebin(NULL),
	need_data_handle_id(0),
	enough_data_handle_id(0),
	appsrc_seek_handle_id(0)
	{
	}
	
	~MediaStream( void )
	{
		if( appsrc )
		{
			if( need_data_handle_id!=0 )
			{
				g_signal_handler_disconnect(appsrc, need_data_handle_id);
			}
			if( enough_data_handle_id!=0 )
			{
				g_signal_handler_disconnect(appsrc, enough_data_handle_id);
			}
			if( appsrc_seek_handle_id!=0 )
			{
				g_signal_handler_disconnect(appsrc, appsrc_seek_handle_id);
			}
		}
	}
	
	
	MediaType GetMediaType( void )
	{
		return mediaType;
	}
	
	const char *GetMediaTypeAsString( void )
	{
		return gstutils_GetMediaTypeName(mediaType);
	}
	
	void SendBuffer( gpointer ptr, gsize len, double duration )
	{
		if( ptr && appsrc )
		{
			GstBuffer *gstBuffer = gst_buffer_new_wrapped_full(
															   (GstMemoryFlags)0,
															   ptr,                 // data allocated via g_malloc
															   len,                 // maxsize: total allocation size
															   0,                   // offset into data
															   len,                 // visible size
															   NULL,                // user_data
															   (GDestroyNotify)g_free );
			GstFlowReturn ret = gst_app_src_push_buffer( appsrc, gstBuffer );
			switch( ret )
			{
				case GST_FLOW_OK:
					injectedSeconds += duration;
					break;
				default:
					GST_ERROR_OBJECT(appsrc, "push_buffer failed - %d", ret);
					break;
			}
		}
	}
	
	void SendBuffer( gpointer ptr, gsize len, double duration, double pts, double dts, GstStructure *metadata=NULL )
	{
		if( ptr && appsrc )
		{
			GstBuffer *gstBuffer = gst_buffer_new_wrapped_full(
															   (GstMemoryFlags)0,
															   ptr,                 // data allocated via g_malloc
															   len,                 // maxsize: total allocation size
															   0,                   // offset into data
															   len,                 // visible size
															   NULL,                // user_data
															   (GDestroyNotify)g_free );
			GST_BUFFER_PTS(gstBuffer) = (GstClockTime)(pts * GST_SECOND);
			GST_BUFFER_DTS(gstBuffer) = (GstClockTime)(dts * GST_SECOND);
			GST_BUFFER_DURATION(gstBuffer) = (GstClockTime)(duration * GST_SECOND);
			if( metadata )
			{
				gst_buffer_add_protection_meta(gstBuffer, metadata);
			}
			GstFlowReturn ret = gst_app_src_push_buffer( appsrc, gstBuffer );
			switch( ret )
			{
				case GST_FLOW_OK:
					injectedSeconds += duration;
					break;
				default:
					GST_ERROR_OBJECT( appsrc, "push_buffer failed - %d", ret );
					break;
			}
		}
	}
	
	void SendGap( double pts, double durationSeconds )
	{
		GST_INFO("SendGap(%s, pts=%f, dur=%f)", GetMediaTypeAsString(), pts, durationSeconds );
		if( appsrc )
		{
			GstClockTime timestamp = (GstClockTime)(pts * GST_SECOND);
			GstClockTime duration = (GstClockTime)(durationSeconds * GST_SECOND);
			GstEvent *event = gst_event_new_gap( timestamp, duration );
			if( !gst_element_send_event( GST_ELEMENT(appsrc), event) )
			{
				GST_WARNING_OBJECT( appsrc, "Failed to send GAP event" );
			}
		}
	}
	
	void SendEOS( void )
	{
		GST_INFO("SendEOS %s", GetMediaTypeAsString());
		if( appsrc )
		{
			gst_app_src_end_of_stream( appsrc );
		}
	}
	
	/**
	 * @brief create, link, and configure a branch for specified media track (appsrc -> decodebin -> convert/resample -> sink)
	 * @param pipeline parent pipeline
	 */
	void Configure( GstElement *pipeline )
	{
		GST_INFO( "Configure %s", GetMediaTypeAsString() );
		if( appsrc )
		{
			GST_WARNING("already configured");
		}
		else
		{ // Create appsrc and downstream elements
			appsrc = GST_APP_SRC(gst_element_factory_make("appsrc", mediaType==eMEDIATYPE_VIDEO?"v_src":"a_src"));
			if( appsrc )
			{
				decodebin = gst_element_factory_make("decodebin", mediaType==eMEDIATYPE_VIDEO?"v_decode":"a_decode");
				if( decodebin )
				{
					GstElement* conv = (mediaType==eMEDIATYPE_VIDEO)?
					gst_element_factory_make("videoconvert","v_conv") :
					gst_element_factory_make("audioconvert","a_conv");
					if( conv )
					{
						GstElement* post = (mediaType==eMEDIATYPE_VIDEO)?
						gst_element_factory_make("videoscale","v_scale") :
						gst_element_factory_make("audioresample","a_res");
						if( post )
						{
							GstElement* sink = (mediaType==eMEDIATYPE_VIDEO)?
							gst_element_factory_make("autovideosink","v_sink") :
							gst_element_factory_make("autoaudiosink","a_sink");
							if( sink )
							{
								gst_bin_add_many(GST_BIN(pipeline), GST_ELEMENT(appsrc), decodebin, conv, post, sink, NULL);
								// link: appsrc -> decodebin (decodebin pad-added will link to conv)
								if( gst_element_link(GST_ELEMENT(appsrc), decodebin) )
								{
									g_signal_connect(decodebin, "pad-added", G_CALLBACK(decodebin_pad_added_cb), this);
									
									// Configure appsrc flow control
									switch( mediaType )
									{
										case eMEDIATYPE_VIDEO:
											g_object_set(appsrc, "max-bytes", (guint64)12582912, NULL );
											break;
										case eMEDIATYPE_AUDIO:
											g_object_set(appsrc, "max-bytes", (guint64)1536000, NULL );
											break;
										default:
											break;
									}
									g_object_set(appsrc, "min-percent", 50, NULL );
									need_data_handle_id = g_signal_connect(appsrc, "need-data",   G_CALLBACK(need_data_cb),  this );
									enough_data_handle_id = g_signal_connect(appsrc, "enough-data", G_CALLBACK(enough_data_cb), this );
									appsrc_seek_handle_id = g_signal_connect(appsrc, "seek-data",   G_CALLBACK(appsrc_seek_cb),this);
									gst_app_src_set_stream_type( appsrc, GST_APP_STREAM_TYPE_SEEKABLE );
									g_object_set(appsrc, "format", GST_FORMAT_TIME, NULL);
									if( context )
									{
										context->found_count--;
										if( context->found_count == 0 )
										{ // Initial lazy seek once both branches are configured
											std::lock_guard<std::mutex> lock(context->segment_seek_mutex);
											if( context->mSegmentEndSeekQueue.empty() )
											{
												GST_DEBUG("Configure %s: initial seek queue empty", GetMediaTypeAsString());
											}
											else
											{
												SeekParam param = context->mSegmentEndSeekQueue.front();
												context->mSegmentEndSeekQueue.pop();
												context->OnAppsrcReady(param);
											}
										}
										return; // success!
									}
									else
									{
										GST_ERROR( "context is NULL" );
									}
								}
								else
								{
									GST_ERROR("link appsrc->decodebin failed for %s", GetMediaTypeAsString() );
								}
							}
							gst_object_unref( sink );
						}
						gst_object_unref( post );
					}
					gst_object_unref( conv );
				}
				gst_object_unref(decodebin);
				decodebin = NULL;
			}
			gst_object_unref(appsrc);
			appsrc = NULL;
		}
	} // Configure
	
	void ClearInjectedSeconds( void ) { injectedSeconds = 0; }
	
	double GetInjectedSeconds( void ) const { return injectedSeconds; }
	
	long long GetPositionMilliseconds( void ) const { return -1; } // Pipeline queries global position now
	
	void need_data( GstElement *appSrc, guint length ) { context->NeedData( mediaType ); }
	
	void enough_data( GstElement *appSrc ) { context->EnoughData( mediaType ); }
	
	gboolean appsrc_seek( GstElement *appSrc, guint64 offset )
	{
		GST_INFO("appsrc_seek %s pos=%" GST_TIME_FORMAT, GetMediaTypeAsString(), GST_TIME_ARGS(offset) );
		return TRUE;
	}
	
	void SetCaps( const Mp4Demux *mp4Demux )
	{
		mp4Demux->setCaps( appsrc );
	}
	
	MediaStream(const MediaStream&)=delete;
	
	MediaStream& operator=(const MediaStream&)=delete;
	
private:
	double injectedSeconds;
	class PipelineContext *context;
	MediaType mediaType;
	GstAppSrc *appsrc;
	GstElement *decodebin;
	
	gulong need_data_handle_id;
	gulong enough_data_handle_id;
	gulong appsrc_seek_handle_id;
};

// C glue callbacks

/**
 * @brief handle gstreamer signal that buffers need to be filled - start/continue injecting AV data
 *
 * @param appSrc element that emitted the signal
 * @param length number of bytes needed, or -1 for "any"
 */
static void need_data_cb(GstElement *appSrc, guint length, class MediaStream *stream )
{
	stream->need_data( appSrc, length );
}

/**
 * @brief handle gstreamer signal that buffers are sufficiently full - stop injecting AV data
 * @param appSrc element that emitted the signal
 */
static void enough_data_cb(GstElement *appSrc, class MediaStream *stream )
{
	stream->enough_data( appSrc );
}

/**
 * @brief sent when a seek event reaches the appsrc - called when appsrc wants us to return data from a new position with the next call to push-buffer.
 *
 * @param appSrc element that emitted the signal
 * @param offset seek target
 * @return TRUE if seek successful
 */
static gboolean appsrc_seek_cb( GstElement * appSrc, guint64 offset, class MediaStream *stream )
{
	return stream->appsrc_seek( appSrc, offset );
}

static void decodebin_pad_added_cb(GstElement * decodebin, GstPad * pad, class MediaStream *stream )
{ // link newly exposed pad to the convert element
	const bool isVideo = (stream->GetMediaType() == eMEDIATYPE_VIDEO);
	const char* convName = isVideo? "v_conv" : "a_conv";
	GstElement* conv = GST_ELEMENT(gst_bin_get_by_name(GST_BIN(gst_element_get_parent(decodebin)), convName));
	GstPad* sinkpad = gst_element_get_static_pad(conv, "sink");
	if( gst_pad_is_linked(sinkpad) ) { gst_object_unref(sinkpad); gst_object_unref(conv); return; }
	if( gst_pad_link(pad, sinkpad) == GST_PAD_LINK_OK )
	{
		gst_object_unref(sinkpad);
		// Finish link to sink chain
		GstElement* post = GST_ELEMENT(gst_bin_get_by_name(GST_BIN(gst_element_get_parent(decodebin)), isVideo?"v_scale":"a_res"));
		GstElement* sink = GST_ELEMENT(gst_bin_get_by_name(GST_BIN(gst_element_get_parent(decodebin)), isVideo?"v_sink":"a_sink"));
		gst_element_link_many(conv, post, sink, NULL);
		gst_object_unref(post); gst_object_unref(sink);
	}
	gst_object_unref(conv);
}

gboolean bus_message_cb(GstBus * bus, GstMessage * msg, class Pipeline *pipeline ) { return pipeline->bus_message( bus, msg ); }

Pipeline::Pipeline( class PipelineContext *context ) : context(context), pipeline(gst_pipeline_new( MY_PIPELINE_NAME )), bus(NULL)
{
	GST_DEBUG_CATEGORY_INIT(gstport_cat, "gstport", 0, "Port/pipeline module logs");
	GST_INFO_OBJECT(pipeline, "Pipeline created: %s", MY_PIPELINE_NAME);
	gstutils_Init();
	bus = gst_element_get_bus(pipeline);
	gst_bus_add_watch( bus, (GstBusFunc)bus_message_cb, this );
	for( int i=0; i<NUM_MEDIA_TYPES; i++ )
	{
		mediaStream[i] = new MediaStream( (MediaType)i, context );
	}
}

void Pipeline::ScheduleSeek( const SeekParam &seekParam )
{
	std::lock_guard<std::mutex> lock(context->segment_seek_mutex);
	context->mSegmentEndSeekQueue.push(seekParam);
}

size_t Pipeline::GetNumPendingSeek(void) const
{
	std::lock_guard<std::mutex> lock(context->segment_seek_mutex);
	return context->mSegmentEndSeekQueue.size();
}

void Pipeline::Configure( MediaType mediaType )
{
	context->found_count++;
	mediaStream[mediaType]->Configure(pipeline);
}

void Pipeline::SetCaps( MediaType mediaType, const Mp4Demux *mp4Demux )
{
	mediaStream[mediaType]->SetCaps(mp4Demux);
}

Pipeline::~Pipeline()
{
	gst_bus_remove_watch(bus);
	gst_object_unref(bus);
	bus = NULL;
	gst_element_set_state(pipeline, GST_STATE_NULL);
	for( int i=0; i<NUM_MEDIA_TYPES; i++ ) { delete mediaStream[i]; }
	gst_object_unref(pipeline);
}

void Pipeline::SetPipelineState(PipelineState pipelineState )
{
	GST_INFO_OBJECT(pipeline, "Set state -> %s", gst_element_state_get_name((GstState)pipelineState));
	gst_element_set_state( pipeline, (GstState) pipelineState );
}

PipelineState Pipeline::GetPipelineState( void ) const
{
	GstState state; GstState pending; GstClockTime timeout = 0;
	gst_element_get_state( pipeline, &state, &pending, timeout );
	GST_DEBUG_OBJECT(pipeline, "Get state: %s (pending %s)", gst_element_state_get_name(state), gst_element_state_get_name(pending));
	return (PipelineState) state;
}

void Pipeline::SendBufferMP4( MediaType mediaType, gpointer ptr, gsize len, double duration )
{
	mediaStream[mediaType]->SendBuffer(ptr,len,duration);
}

void Pipeline::SendBufferES( MediaType mediaType, gpointer ptr, gsize len, double duration, double pts, double dts, GstStructure *metadata )
{
	mediaStream[mediaType]->SendBuffer(ptr,len,duration,pts,dts,metadata);
}

void Pipeline::SendGap( MediaType mediaType, double pts, double durationSeconds )
{
	GST_INFO_OBJECT(pipeline, "SendGap %s pts=%f dur=%f", gstutils_GetMediaTypeName(mediaType), pts, durationSeconds );
	mediaStream[mediaType]->SendGap(pts,durationSeconds);
}

void Pipeline::SendEOS( MediaType mediaType ) { mediaStream[mediaType]->SendEOS(); }

bool Pipeline::DoSeekNow( const SeekParam& req )
{
	const gint64 start = (gint64)(req.start_seconds * GST_SECOND);
	const gint64 stop  = (gint64)(req.stop_seconds  * GST_SECOND);
	GST_INFO_OBJECT(pipeline, "DoSeekNow rate=%.2f start=%" GST_TIME_FORMAT " stop=%" GST_TIME_FORMAT " flush=%d", req.playback_rate, GST_TIME_ARGS(start), GST_TIME_ARGS(stop), req.flush);
	if( req.flush ) { mediaStream[eMEDIATYPE_AUDIO]->ClearInjectedSeconds(); mediaStream[eMEDIATYPE_VIDEO]->ClearInjectedSeconds(); }
	GstSeekFlags flags = GST_SEEK_FLAG_NONE;
	if( req.flush )   flags = (GstSeekFlags)(flags | GST_SEEK_FLAG_FLUSH);
	if( req.segment ) flags = (GstSeekFlags)(flags | GST_SEEK_FLAG_SEGMENT);
	const gboolean ok = gst_element_seek( pipeline,
										 req.playback_rate,
										 GST_FORMAT_TIME,
										 flags,
										 GST_SEEK_TYPE_SET, start,
										 req.stop_seconds>req.start_seconds? GST_SEEK_TYPE_SET : GST_SEEK_TYPE_NONE,
										 stop );
	if (!ok) { GST_ERROR_OBJECT(pipeline, "gst_element_seek failed"); }
	return ok;
}

void Pipeline::Reset( void )
{
	std::lock_guard<std::mutex> lock(context->segment_seek_mutex);
	std::queue<SeekParam> empty; std::swap( context->mSegmentEndSeekQueue, empty );
	GST_DEBUG_OBJECT(pipeline, "Reset seek queue");
}

long long Pipeline::GetPositionMilliseconds( MediaType /*mediaType*/ ) const
{
	gint64 position = GST_CLOCK_TIME_NONE;
	if( gst_element_query_position(pipeline, GST_FORMAT_TIME, &position) )
		return GST_TIME_AS_MSECONDS(position);
	return -1;
}

double Pipeline::GetInjectedSeconds( MediaType mediaType ) const
{
	return mediaStream[mediaType]->GetInjectedSeconds();
}

void Pipeline::HandleGstMessageError( GstMessage *msg, const char *messageName )
{
	GError *error = NULL; gchar *dbg_info = NULL;
	gst_message_parse_error(msg, &error, &dbg_info);
	GST_ERROR("%s: from %s %s", messageName, GST_OBJECT_NAME(msg->src), error->message);
	g_clear_error(&error); g_free(dbg_info);
}

void Pipeline::HandleGstMessageWarning( GstMessage *msg, const char *messageName )
{
	GError *error = NULL; gchar *dbg_info = NULL;
	gst_message_parse_warning(msg, &error, &dbg_info);
	GST_WARNING("%s: from %s %s", messageName, GST_OBJECT_NAME(msg->src), error->message);
	g_clear_error(&error); g_free(dbg_info);
}

void Pipeline::ReachedEOS( void )
{
	std::lock_guard<std::mutex> lock(context->segment_seek_mutex);
	if (!context->mSegmentEndSeekQueue.empty()) {
		const SeekParam &param = context->mSegmentEndSeekQueue.front();
		(void)DoSeekNow(param);
		context->mSegmentEndSeekQueue.pop();
	}
}

void Pipeline::HandleGstMessageEOS( GstMessage *msg, const char *messageName )
{
	GST_INFO( "%s from %s", messageName, GST_OBJECT_NAME(msg->src) );
	ReachedEOS();
}

void Pipeline::HandleGstMessageSegmentDone( GstMessage *message, const char *messageName )
{
	GstFormat format; gint64 position; gst_message_parse_segment_done( message, &format, &position );
	if (format != GST_FORMAT_TIME) { GST_WARNING("SegmentDone format != TIME"); return; }
	GST_INFO( "%s from %s position=%" GST_TIME_FORMAT, messageName, GST_OBJECT_NAME(message->src), GST_TIME_ARGS(position) );
	ReachedEOS();
}

gboolean Pipeline::bus_message( _GstBus * bus, _GstMessage * msg )
{
	GstMessageType messageType = GST_MESSAGE_TYPE(msg);
	const char *messageName = gst_message_type_get_name( messageType );
	switch( messageType )
	{
		case GST_MESSAGE_ERROR:         HandleGstMessageError( msg, messageName ); break;
		case GST_MESSAGE_WARNING:       HandleGstMessageWarning( msg, messageName ); break;
		case GST_MESSAGE_SEGMENT_DONE:  HandleGstMessageSegmentDone( msg, messageName ); break;
		case GST_MESSAGE_EOS:           HandleGstMessageEOS( msg, messageName ); break;
		case GST_MESSAGE_STATE_CHANGED: gstutils_HandleGstMessageStateChanged( msg, messageName ); break;
		case GST_MESSAGE_TAG:           gstutils_HandleGstMessageTag( msg, messageName ); break;
		case GST_MESSAGE_QOS:           gstutils_HandleGstMessageQOS( msg, messageName ); break;
		case GST_MESSAGE_STREAM_STATUS: gstutils_HandleGstMessageStreamStatus( msg, messageName ); break;
		default: break;
	}
	return TRUE;
}

void Pipeline::DumpDOT( void ) const
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
	auto rc = gst_element_seek( GST_ELEMENT(pipeline), newRate, GST_FORMAT_TIME, GST_SEEK_FLAG_INSTANT_RATE_CHANGE, GST_SEEK_TYPE_NONE, 0, GST_SEEK_TYPE_NONE, 0 );
	if (!rc) { GST_ERROR_OBJECT(pipeline, "Instantaneous rate change seek failed"); return; }
}
