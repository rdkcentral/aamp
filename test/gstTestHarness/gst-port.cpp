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
#include <memory>
#include <mutex>
#include <atomic>

#define MY_PIPELINE_NAME "test-pipeline"

// Logging category for this module
GST_DEBUG_CATEGORY_STATIC(gstport_cat);
#define GST_CAT_DEFAULT gstport_cat

static void need_data_cb(GstElement *appSrc, guint length, class MediaStream *stream );
static void enough_data_cb(GstElement *appSrc, class MediaStream *stream );
static gboolean appsrc_seek_cb( GstElement * appSrc, guint64 offset, class MediaStream *stream );
static void decodebin_pad_added_cb(GstElement * decodebin, GstPad * pad, class MediaStream *stream );

/**
 * @class MediaStream
 * @brief Manages a single media stream (audio or video) in the GStreamer pipeline
 * 
 * This class wraps GStreamer elements for a media stream and manages their lifecycle.
 * The GStreamer objects (appsrc, decodebin) are owned by the parent pipeline bin,
 * and this class holds non-owning references to them. The pipeline bin handles
 * reference counting and cleanup when the pipeline is destroyed.
 */
class MediaStream
{
public:
	MediaStream( MediaType mediaType, class PipelineContext *context )
	: injectedSeconds(),
	context(context),
	mediaType(mediaType),
	appsrc(nullptr),
	decodebin(nullptr),
	need_data_handle_id(0),
	enough_data_handle_id(0),
	appsrc_seek_handle_id(0)
	{
	}
	
	/**
	 * @brief Destructor - disconnects signal handlers
	 * 
	 * Note: Does NOT unref appsrc or decodebin as they are owned by the pipeline bin.
	 * The pipeline bin manages their lifecycle and will unref them when the pipeline
	 * is destroyed. This destructor only disconnects signal handlers that reference
	 * this MediaStream instance to prevent dangling pointers.
	 */
	~MediaStream( void )
	{
		// Disconnect signal handlers to prevent callbacks to a destroyed object
		if( appsrc )
		{
			if( need_data_handle_id != 0 )
			{
				g_signal_handler_disconnect(appsrc, need_data_handle_id);
			}
			if( enough_data_handle_id != 0 )
			{
				g_signal_handler_disconnect(appsrc, enough_data_handle_id);
			}
			if( appsrc_seek_handle_id != 0 )
			{
				g_signal_handler_disconnect(appsrc, appsrc_seek_handle_id);
			}
		}
		
		if( decodebin )
		{
			// Disconnect any signal handlers (e.g. "pad-added") that were
			// connected with this MediaStream instance as user data.
			g_signal_handlers_disconnect_by_data(decodebin, this);
		}
		
		// Note: appsrc and decodebin are NOT unreferenced here as they are owned
		// by the pipeline bin, which will handle their cleanup when destroyed.
	}
	
	
	MediaType GetMediaType( void ) const
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
	
	// Custom deleter for GstElement*
	struct GstElementDeleter {
		void operator()(GstElement* e) const noexcept {
			if (e) {
				gst_object_unref(GST_OBJECT(e));
			}
		}
	};
	
	using GstElemUPtr = std::unique_ptr<GstElement, GstElementDeleter>;
	
	// Helper to create an element and log an error if creation fails
	static GstElemUPtr make_element_or_log(const char* factory,
										   const char* name,
										   const char* media_str)
	{
		GstElement* raw = gst_element_factory_make(factory, name);
		if (!raw) {
			GST_ERROR("Failed to create %s for %s", factory, media_str);
			return GstElemUPtr(nullptr);
		}
		return GstElemUPtr(raw);
	}
	
	/**
	 * @brief Configure the media stream by creating and linking GStreamer elements
	 * 
	 * Creates the GStreamer pipeline elements for this media stream and adds them
	 * to the provided pipeline. Elements are owned by the pipeline bin after being
	 * added, and this class stores non-owning references to appsrc and decodebin.
	 * 
	 * @param pipeline The GStreamer pipeline to add elements to
	 */
	void Configure(GstElement* pipeline)
	{
		GST_INFO("Configure %s", GetMediaTypeAsString());

		if (!context) {
			GST_ERROR("context is nullptr");
			return;
		}
		
		if (appsrc) {
			GST_WARNING("already configured");
			return;
		}
		
		const bool isVideo = (mediaType == eMEDIATYPE_VIDEO);
		const char* mediaStr = GetMediaTypeAsString();
		
		// --- Create all elements with RAII wrappers (auto-unref on early return) ---
		auto appsrcLocal = make_element_or_log("appsrc", isVideo ? "v_src" : "a_src", mediaStr);
		if (!appsrcLocal) return;
		
		auto decodebinLocal = make_element_or_log("decodebin", isVideo ? "v_decode" : "a_decode", mediaStr);
		if (!decodebinLocal) return;
		
		auto convLocal = make_element_or_log(isVideo ? "videoconvert"  : "audioconvert",
											 isVideo ? "v_conv"        : "a_conv", mediaStr);
		if (!convLocal) return;
		
		auto postLocal = make_element_or_log(isVideo ? "videoscale"    : "audioresample",
											 isVideo ? "v_scale"       : "a_res", mediaStr);
		if (!postLocal) return;
		
		auto sinkLocal = make_element_or_log(isVideo ? "autovideosink" : "autoaudiosink",
											 isVideo ? "v_sink"        : "a_sink", mediaStr);
		if (!sinkLocal) return;
		
		// --- Add all elements to the pipeline (bin takes ownership by increasing ref) ---
		gst_bin_add_many(
						 GST_BIN(pipeline),
						 appsrcLocal.get(),
						 decodebinLocal.get(),
						 convLocal.get(),
						 postLocal.get(),
						 sinkLocal.get(),
						 NULL
						 );
		
		// --- Link appsrc -> decodebin ---
		if (!gst_element_link(appsrcLocal.get(), decodebinLocal.get())) {
			GST_ERROR("link appsrc->decodebin failed for %s", mediaStr);
			// Elements are already in the bin; let the bin clean them up.
			// Set our member references to nullptr to indicate failure.
			decodebin = nullptr;
			appsrc    = nullptr;
			return;
		}
		
		// --- Store non-owning references: release RAII so unique_ptrs don't unref ---
		// The pipeline bin now owns these elements and will manage their lifecycle.
		// We store raw pointers as non-owning references for signal handling and operations.
		appsrc    = GST_APP_SRC(appsrcLocal.release());     // member expects GstAppSrc*
		decodebin = decodebinLocal.release();               // member is GstElement*
		// conv/post/sink are not stored as members; release them so bin owns the only ref
		convLocal.release();
		postLocal.release();
		sinkLocal.release();
		
		// pad-added callback on decodebin
		g_signal_connect(decodebin, "pad-added", G_CALLBACK(decodebin_pad_added_cb), this);
		
		// --- Configure appsrc flow control ---
		switch (mediaType) {
			case eMEDIATYPE_VIDEO:
				g_object_set(appsrc, "max-bytes", (guint64)12582912, NULL);
				break;
			case eMEDIATYPE_AUDIO:
				g_object_set(appsrc, "max-bytes", (guint64)1536000, NULL);
				break;
			default:
				break;
		}
		g_object_set(appsrc, "min-percent", 50, NULL);
		
		need_data_handle_id   = g_signal_connect(appsrc, "need-data",    G_CALLBACK(need_data_cb),    this);
		enough_data_handle_id = g_signal_connect(appsrc, "enough-data",  G_CALLBACK(enough_data_cb),  this);
		appsrc_seek_handle_id = g_signal_connect(appsrc, "seek-data",    G_CALLBACK(appsrc_seek_cb),  this);
		
		gst_app_src_set_stream_type(appsrc, GST_APP_STREAM_TYPE_SEEKABLE);
		g_object_set(appsrc, "format",   GST_FORMAT_TIME, NULL);
		g_object_set(appsrc, "typefind", TRUE,            NULL);
		
		// --- Atomic coordination with the other branch ---
		auto prevCount = context->found_count.fetch_sub(1, std::memory_order_acq_rel);
		if (prevCount == 1) {
			// Initial lazy seek once both branches are configured
			std::lock_guard<std::mutex> lock(context->segment_seek_mutex);
			if (context->mSegmentEndSeekQueue.empty()) {
				GST_DEBUG("Configure %s: initial seek queue empty", mediaStr);
			} else {
				SeekParam param = context->mSegmentEndSeekQueue.front();
				context->mSegmentEndSeekQueue.pop();
				context->OnAppsrcReady(param);
			}
		}
		
		// Success - elements are owned by pipeline, and our members hold required refs
	}
	
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
	
	/// Non-owning pointer to the pipeline context. Const to indicate the pointer never changes.
	PipelineContext* const context;
	
	const MediaType mediaType;
	
	/// Non-owning reference to appsrc element (owned by pipeline bin)
	GstAppSrc *appsrc;
	
	/// Non-owning reference to decodebin element (owned by pipeline bin)
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

/**
 * @brief link newly exposed pad to the convert element
 *
 * @param decodebin - decoder
 * @param pad - newly exposed pad
 * @param stream - class encapsulating audio or video path
 */
static void decodebin_pad_added_cb(GstElement * decodebin, GstPad * pad, class MediaStream *stream )
{
	const bool isVideo = (stream->GetMediaType() == eMEDIATYPE_VIDEO);
	const char* convName = isVideo ? "v_conv" : "a_conv";
	
	// Get parent and check for null
	auto parent = gst_element_get_parent(decodebin);
	if( !parent )
	{
		GST_ERROR_OBJECT(decodebin, "decodebin has no parent; cannot link newly added pad");
		return;
	}
	
	GstBin* parentBin = GST_BIN(parent);
	
	// Get conv element and check for null
	GstElement* conv = GST_ELEMENT(gst_bin_get_by_name(parentBin, convName));
	if( !conv )
	{
		GST_ERROR_OBJECT(decodebin, "Failed to find convert element '%s' in bin", convName);
		gst_object_unref(parent);
		return;
	}
	
	// Get sinkpad and check for null
	GstPad* sinkpad = gst_element_get_static_pad(conv, "sink");
	if( !sinkpad )
	{
		GST_ERROR_OBJECT(conv, "Failed to get 'sink' pad from convert element '%s'", convName);
		gst_object_unref(conv);
		gst_object_unref(parent);
		return;
	}
	
	// Check if already linked
	if( gst_pad_is_linked(sinkpad) )
	{
		gst_object_unref(sinkpad);
		gst_object_unref(conv);
		gst_object_unref(parent);
		return;
	}
	
	// Try to link the pads
	if( gst_pad_link(pad, sinkpad) == GST_PAD_LINK_OK )
	{
		gst_object_unref(sinkpad);
		
		// Finish link to sink chain
		GstElement* post = GST_ELEMENT(gst_bin_get_by_name(parentBin, isVideo ? "v_scale" : "a_res"));
		if( !post )
		{
			GST_ERROR_OBJECT(decodebin, "Failed to find post element '%s' in bin",
							 isVideo ? "v_scale" : "a_res");
			gst_object_unref(conv);
			gst_object_unref(parent);
			return;
		}
		
		GstElement* sink = GST_ELEMENT(gst_bin_get_by_name(parentBin, isVideo ? "v_sink" : "a_sink"));
		if( !sink )
		{
			GST_ERROR_OBJECT(decodebin, "Failed to find sink element '%s' in bin",
							 isVideo ? "v_sink" : "a_sink");
			gst_object_unref(post);
			gst_object_unref(conv);
			gst_object_unref(parent);
			return;
		}
		
		gboolean linkOk = gst_element_link_many(conv, post, sink, NULL);
		if( !linkOk )
		{
			GST_ERROR_OBJECT(conv, "Failed to link %s elements in decodebin_pad_added_cb",
							 isVideo ? "video" : "audio");
		}
		
		gst_object_unref(post);
		gst_object_unref(sink);
	}
	else
	{
		// On link failure, release sinkpad reference
		gst_object_unref(sinkpad);
	}
	
	gst_object_unref(conv);
	gst_object_unref(parent);
}

gboolean bus_message_cb(GstBus * bus, GstMessage * msg, class Pipeline *pipeline )
{
	return pipeline->bus_message( bus, msg );
}

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
	if( req.flush )   flags = static_cast<GstSeekFlags>(flags | GST_SEEK_FLAG_FLUSH);
	if( req.segment ) flags = static_cast<GstSeekFlags>(flags | GST_SEEK_FLAG_SEGMENT);
	bool open = req.stop_seconds>req.start_seconds;
	const gboolean ok = gst_element_seek( pipeline,
										 req.playback_rate,
										 GST_FORMAT_TIME,
										 flags,
										 GST_SEEK_TYPE_SET, start,
										 open? GST_SEEK_TYPE_SET : GST_SEEK_TYPE_NONE,
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
