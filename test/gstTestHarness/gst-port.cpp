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
#include <gst/gstcaps.h>
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
	MediaStream( MediaType mediaType, class PipelineContext *context ) : injectedSeconds(), context(context), mediaType(mediaType), appsrc(nullptr), decodebin(nullptr), need_data_handle_id(0), enough_data_handle_id(0), appsrc_seek_handle_id(0)
	{
	}
	
	/**
	 * @brief Destructor - disconnects signal handlers
	 *
	 * Uses weak pointers to avoid calling into finalized GObjects
	 * Does NOT unref appsrc or decodebin (owned by pipeline bin).
	 */
	~MediaStream( void )
	{
		// Disconnect appsrc signal handlers only if object is still valid
		if (appsrc)
		{
			if (need_data_handle_id != 0)
			{
				g_signal_handler_disconnect(appsrc, need_data_handle_id);
			}
			if (enough_data_handle_id != 0)
			{
				g_signal_handler_disconnect(appsrc, enough_data_handle_id);
			}
			if (appsrc_seek_handle_id != 0)
			{
				g_signal_handler_disconnect(appsrc, appsrc_seek_handle_id);
			}
		}
		// Disconnect any decodebin signal handlers connected with 'this' as user data.
		// Safe even if some were already removed.
		if (decodebin)
		{
			// Disconnect any signal handlers (e.g. "pad-added") that were
			// connected with this MediaStream instance as user data.
			g_signal_handlers_disconnect_by_data(decodebin, this);
		}
		
		// Clear weak pointers; if the GObject was already finalized, GLib will
		// have set these to nullptr, and the guards prevent calling remove on
		// an already-cleared weak pointer (avoids double-remove).
		if (appsrc)
		{
			g_object_remove_weak_pointer(G_OBJECT(appsrc), reinterpret_cast<gpointer*>(&appsrc));
			appsrc = nullptr;
		}
		if (decodebin)
		{
			g_object_remove_weak_pointer(G_OBJECT(decodebin), reinterpret_cast<gpointer*>(&decodebin));
			decodebin = nullptr;
		}
	}
	
	MediaType GetMediaType( void ) const
	{
		return mediaType;
	}
	
	const char *GetMediaTypeAsString( void ) const
	{
		return gstutils_GetMediaTypeName(mediaType);
	}
	
	void SendBuffer( gpointer ptr, gsize len, double duration )
	{
		if (ptr && appsrc)
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
	
	void SendBuffer( gpointer ptr, gsize len, double duration, double pts, double dts, GstStructure *metadata=nullptr )
	{
		if (ptr && appsrc)
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
			if (metadata)
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
		if (appsrc)
		{
			GstClockTime timestamp = (GstClockTime)(pts * GST_SECOND);
			GstClockTime duration = (GstClockTime)(durationSeconds * GST_SECOND);
			GstEvent *event = gst_event_new_gap( timestamp, duration );
			if (!gst_element_send_event( GST_ELEMENT(appsrc), event))
			{
				GST_WARNING_OBJECT( appsrc, "Failed to send GAP event" );
			}
		}
	}
	
	void SendEOS( void )
	{
		GST_INFO("SendEOS %s", GetMediaTypeAsString());
		if (appsrc)
		{
			gst_app_src_end_of_stream( appsrc );
		}
	}
	
	static ScopedGstElement make_element( const char* factory, const char* name )
	{
		GstElement* raw = gst_element_factory_make(factory, name);
		if (!raw)
		{
			GST_ERROR("failed to create %s", factory);
			return ScopedGstElement(nullptr);
		}
		return ScopedGstElement(raw);
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
		if (!context)
		{
			GST_ERROR("no context");
		}
		else if (appsrc)
		{
			GST_WARNING("already configured");
		}
		else
		{
			// --- Create all elements with RAII wrappers (auto-unref on early return) ---
			ScopedGstElement appsrcLocal, decodebinLocal, convLocal, postLocal, sinkLocal;
			guint64 maxBytes;
			if (mediaType == eMEDIATYPE_VIDEO)
			{
				appsrcLocal = make_element("appsrc", "v_src");
				decodebinLocal = make_element("decodebin", "v_decode");
				convLocal = make_element("videoconvert", "v_conv");
				postLocal = make_element("videoscale", "v_scale");
				sinkLocal = make_element("autovideosink","v_sink");
				maxBytes = 12582912;
			}
			else
			{
				appsrcLocal = make_element("appsrc", "a_src");
				decodebinLocal = make_element("decodebin", "a_decode");
				convLocal = make_element("audioconvert","a_conv");
				postLocal = make_element("audioresample","a_res");
				sinkLocal = make_element("autoaudiosink","a_sink");
				maxBytes = 1536000;
			}
			
			if (appsrcLocal && decodebinLocal && convLocal && postLocal && sinkLocal)
			{
				// --- Add all elements to the pipeline (bin takes ownership by increasing ref) ---
				gst_bin_add_many(
								 GST_BIN(pipeline),
								 appsrcLocal.get(),
								 decodebinLocal.get(),
								 convLocal.get(),
								 postLocal.get(),
								 sinkLocal.get(),
								 nullptr );
				
				// --- Link appsrc -> decodebin ---
				if (!gst_element_link(appsrcLocal.get(), decodebinLocal.get()))
				{
					GST_ERROR("gst_element_link(appsrc->decodebin) failed");
					decodebin = nullptr;
					appsrc = nullptr;
					return;
				}
				
				// --- NEW: statically link conv -> post -> sink here ---
				if (!gst_element_link_many(convLocal.get(), postLocal.get(), sinkLocal.get(), nullptr))
				{
					GST_ERROR_OBJECT(convLocal.get(), "gst_element_link_many failure conv->post->sink" );
					return; // Early exit; RAII locals will unref, bin owns added refs
				}
				
				// --- Store non-owning references: release RAII so unique_ptrs don't unref ---
				// The pipeline bin now owns these elements and will manage their lifecycle.
				// We store raw pointers as non-owning references for signal handling and operations.
				appsrc = GST_APP_SRC(appsrcLocal.release());     // member expects GstAppSrc*
				decodebin = decodebinLocal.release();            // member is GstElement*
				
				// --- NEW: set weak pointers so raw members become nullptr if objects are finalized ---
				// This prevents destructor from calling g_signal_handler_disconnect on dangling pointers.
				// Safe to call multiple times; GLib manages internal weak-ref list.
				if (appsrc)
				{
					g_object_add_weak_pointer(G_OBJECT(appsrc), reinterpret_cast<gpointer*>(&appsrc));
				}
				if (decodebin)
				{
					g_object_add_weak_pointer(G_OBJECT(decodebin), reinterpret_cast<gpointer*>(&decodebin));
				}
				
				// conv/post/sink are not stored as members; release RAII so bin remains owner
				convLocal.release();
				postLocal.release();
				sinkLocal.release();
				
				// pad-added callback on decodebin
				g_signal_connect(decodebin, "pad-added", G_CALLBACK(decodebin_pad_added_cb), this);
				
				// --- Configure appsrc flow control ---
				g_object_set(appsrc, "max-bytes", maxBytes, nullptr);
				g_object_set(appsrc, "min-percent", 50, nullptr);
				
				need_data_handle_id   = g_signal_connect(appsrc, "need-data",    G_CALLBACK(need_data_cb),    this);
				enough_data_handle_id = g_signal_connect(appsrc, "enough-data",  G_CALLBACK(enough_data_cb),  this);
				appsrc_seek_handle_id = g_signal_connect(appsrc, "seek-data",    G_CALLBACK(appsrc_seek_cb),  this);
				
				gst_app_src_set_stream_type(appsrc, GST_APP_STREAM_TYPE_SEEKABLE);
				g_object_set(appsrc, "format",   GST_FORMAT_TIME, nullptr);
				g_object_set(appsrc, "typefind", TRUE,            nullptr);
				
				// Success - elements are owned by pipeline, and our members hold required refs
			}
			else
			{
				GST_ERROR("failed to create one or more GStreamer elements");
			}
		}
	}
	
	void ClearInjectedSeconds( void )
	{
		injectedSeconds = 0;
	}
	
	double GetInjectedSeconds( void ) const
	{
		return injectedSeconds;
	}
	
	long long GetPositionMilliseconds( void ) const
	{ // Pipeline queries global position now
		return -1;
	}
	
	void need_data( GstElement *appSrc, guint length )
	{
		context->NeedData( mediaType );
	}
	
	void enough_data( GstElement *appSrc )
	{
		context->EnoughData( mediaType );
	}
	
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
	
	/// Non-owning pointer to the pipeline context. Pointer is const (cannot be reseated).
	/// The pointed-to PipelineContext is not const as it has mutable state (queues, atomics).
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
 * @brief link newly exposed pad to the convert element (dynamic pad -> conv.sink only)
 *
 * @param decodebin - decoder
 * @param pad - newly exposed pad
 * @param stream - class encapsulating audio or video path
 */
static void decodebin_pad_added_cb(GstElement * decodebin, GstPad * pad, class MediaStream *stream )
{
	const bool isVideo = (stream->GetMediaType() == eMEDIATYPE_VIDEO);
	const char* convName = isVideo ? "v_conv" : "a_conv";
	
	// Parent bin (RAII)
	ScopedGstObject parent = ScopedGstObject(gst_element_get_parent(decodebin));
	if (!parent)
	{
		GST_ERROR_OBJECT(decodebin, "decodebin has no parent; cannot link newly added pad");
		return;
	}
	GstBin* parentBin = GST_BIN(parent.get());
	
	// Convert element (RAII)
	ScopedGstElement conv{ gst_bin_get_by_name(parentBin, convName) };
	if (!conv)
	{
		GST_ERROR_OBJECT(decodebin, "Failed to find convert element '%s' in bin", convName);
		return;
	}
	
	// Sink pad of conv (RAII)
	ScopedGstPad sinkpad{ gst_element_get_static_pad(conv.get(), "sink") };
	if (!sinkpad)
	{
		GST_ERROR_OBJECT(conv.get(), "Failed to get 'sink' pad from convert element '%s'", convName);
		return;
	}
	
	// Already linked? nothing to do
	if (gst_pad_is_linked(sinkpad.get()))
	{
		GST_DEBUG_OBJECT(conv.get(), "conv.sink already linked; skipping");
		return;
	}
	
	// Dynamic link: decodebin src pad -> conv.sink
	if (gst_pad_link(pad, sinkpad.get()) == GST_PAD_LINK_OK)
	{
		GST_INFO_OBJECT(conv.get(), "Linked decodebin src pad -> %s.sink", convName);
	}
	else
	{
		GST_ERROR_OBJECT(conv.get(), "Failed to link decodebin src pad -> %s.sink", convName);
	}
	// All acquired refs auto-unref via RAII on scope exit
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
	gst_bus_add_watch( bus, reinterpret_cast<GstBusFunc>(bus_message_cb), this );
	for( int i=0; i<NUM_MEDIA_TYPES; i++ )
	{
		mediaStream[i] = std::make_unique<MediaStream>( static_cast<MediaType>(i), context );
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
	mediaStream[mediaType]->Configure(pipeline);
	// Increment count and perform initial seek if both branches are configured
	// All operations protected by mutex to prevent race conditions
	std::lock_guard<std::mutex> lock(context->segment_seek_mutex);
	
	// Increment the configured stream count (protected by mutex above)
	int count = ++context->configured_stream_count;
	
	// When both branches are configured and initial seek hasn't been performed yet
	if (count == NUM_MEDIA_TYPES &&
	   !context->initial_seek_performed &&
	   !context->mSegmentEndSeekQueue.empty())
	{
		SeekParam param = context->mSegmentEndSeekQueue.front();
		context->mSegmentEndSeekQueue.pop();
		(void)DoSeekNow(param);
		context->initial_seek_performed = true;
	}
}

void Pipeline::SetCaps( MediaType mediaType, const Mp4Demux *mp4Demux )
{
	mediaStream[mediaType]->SetCaps(mp4Demux);
}

Pipeline::~Pipeline()
{
	// 1) Stop bus watch now, while pipeline is still alive.
	if (bus)
	{
		gst_bus_remove_watch(bus);
		gst_object_unref(bus);
		bus = nullptr;
	}
	// 2) Destroy MediaStream instances first, so they can safely disconnect signals
	//    while their elements (appsrc/decodebin) still exist in the bin.
	for (auto& ms : mediaStream)
	{
		ms.reset(); // invokes MediaStream::~MediaStream()
	}
	// 3) Now tear down the pipeline
	if (pipeline)
	{
		gst_element_set_state(pipeline, GST_STATE_NULL);
		gst_object_unref(pipeline);
		pipeline = nullptr;
	}
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

void Pipeline::SendEOS( MediaType mediaType )
{
	mediaStream[mediaType]->SendEOS();
}

bool Pipeline::DoSeekNow( const SeekParam& req )
{
	const gint64 start = (gint64)(req.start_seconds * GST_SECOND);
	const gint64 stop  = (gint64)(req.stop_seconds  * GST_SECOND);
	GST_INFO_OBJECT(pipeline, "DoSeekNow rate=%.2f start=%" GST_TIME_FORMAT " stop=%" GST_TIME_FORMAT " flush=%d segment=%d", req.playback_rate, GST_TIME_ARGS(start), GST_TIME_ARGS(stop), req.flush, req.segment);
	GstSeekFlags flags = GST_SEEK_FLAG_NONE;
	if (req.flush)
	{
		flags = static_cast<GstSeekFlags>(flags|GST_SEEK_FLAG_FLUSH);
	}
	if (req.segment)
	{
		flags = static_cast<GstSeekFlags>(flags|GST_SEEK_FLAG_SEGMENT);
	}
	bool open = (req.stop_seconds==req.start_seconds);
	const gboolean ok = gst_element_seek( pipeline,
										 req.playback_rate,
										 GST_FORMAT_TIME,
										 flags,
										 GST_SEEK_TYPE_SET, start,
										 open? GST_SEEK_TYPE_NONE : GST_SEEK_TYPE_SET,
										 open? GST_CLOCK_TIME_NONE : stop );
	if (ok)
	{
		if (req.flush)
		{
			mediaStream[eMEDIATYPE_AUDIO]->ClearInjectedSeconds();
			mediaStream[eMEDIATYPE_VIDEO]->ClearInjectedSeconds();
		}
	}
	else
	{
		GST_ERROR_OBJECT(pipeline, "gst_element_seek failed");
	}
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
	if (gst_element_query_position(pipeline, GST_FORMAT_TIME, &position))
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
	if (f)
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
