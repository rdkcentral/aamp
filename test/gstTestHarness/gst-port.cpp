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
#include <gst/app/gstappsrc.h>
#include <math.h>
#include <inttypes.h>

static const char *MediaType2Name( MediaType type )
{
	switch( type )
	{
		case eMEDIATYPE_AUDIO: return "audio";
		case eMEDIATYPE_VIDEO: return "video";
		default: return "?";
	}
}

#define MY_PIPELINE_NAME "test-pipeline"
static bool gQuiet = false; // set to true for less chatty logging

static void need_data_cb(GstElement *appSrc, guint length, MediaStream *stream );
static void enough_data_cb(GstElement *appSrc, MediaStream *stream );
static gboolean appsrc_seek_cb(GstElement * appSrc, guint64 offset, MediaStream *stream );
static void found_source_cb(GObject * object, GObject * orig, GParamSpec * pspec, class MediaStream *stream );
static void element_setup_cb(GstElement * playbin, GstElement * element, class MediaStream *stream);
static void pad_added_cb(GstElement* object, GstPad* arg0, class MediaStream *stream);
static GstPadProbeReturn MyDemuxPadProbeCallback( GstPad * pad, GstPadProbeInfo * info, class MediaStream *stream );
static void MyDestroyDataNotify( gpointer data );

/**
 * @brief Check if string start with a prefix
 *
 * @retval TRUE if substring is found in bigstring
 */
static bool startsWith( const char *inputStr, const char *prefix )
{
	bool rc = true;
	while( *prefix )
	{
		if( *inputStr++ != *prefix++ )
		{
			rc = false;
			break;
		}
	}
	return rc;
}

class MediaStream
{
public:
	MediaStream( MediaType mediaType, class PipelineContext *context ) : isConfigured(false), sinkbin(NULL), source(NULL), rate(), start(), stop(), injectedSeconds(), context(context), mediaType(mediaType), startPos(-1) {
	}
	
	
	~MediaStream()
	{
		if( qtdemux_probe_id )
		{
			gst_pad_remove_probe( qtdemux_pad, qtdemux_probe_id );
		}
		if( qtdemux_pad )
		{
			gst_object_unref( qtdemux_pad );
		}
	}
	
	const char *getMediaTypeAsString( void )
	{
		switch( mediaType )
		{
			case eMEDIATYPE_AUDIO:
				return "audio";
			case eMEDIATYPE_VIDEO:
				return "video";
			default:
				return "unknown";
		}
	}
	
	void SendBuffer( gpointer ptr, gsize len, double duration )
	{
		if( ptr )
		{
			GstBuffer *gstBuffer = gst_buffer_new_wrapped( ptr, len );
			GstFlowReturn ret = gst_app_src_push_buffer(GST_APP_SRC(source), gstBuffer );
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

	void SendBuffer( gpointer ptr, gsize len, double duration, double pts, double dts )
	{
		if( ptr )
		{
			GstBuffer *gstBuffer = gst_buffer_new_wrapped( ptr, len );
			GST_BUFFER_PTS(gstBuffer) = (GstClockTime)(pts * GST_SECOND);
			GST_BUFFER_DTS(gstBuffer) = (GstClockTime)(dts * GST_SECOND);
			GST_BUFFER_DURATION(gstBuffer) = (GstClockTime)(duration * 1000000000LL);
			GstFlowReturn ret = gst_app_src_push_buffer(GST_APP_SRC(source), gstBuffer );
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
		g_print( "MediaStream::SendGap(%s,pts=%f,dur=%f)\n", MediaType2Name(mediaType), pts, durationSeconds );
		GstClockTime timestamp = (GstClockTime)(pts * GST_SECOND);
		GstClockTime duration = (GstClockTime)(durationSeconds * GST_SECOND);
		GstEvent *event = gst_event_new_gap( timestamp, duration );
		if( !gst_element_send_event( GST_ELEMENT(source), event) )
		{
			g_print( "Failed to send gap event\n" );
		}
	}
	
	void SendEOS( void )
	{
		printf( "MediaStream::SendEOS(%d)\n", mediaType );
		gst_app_src_end_of_stream(GST_APP_SRC(source));
	}
	
	/**
	 * @brief record requested rate, start, stop, so that it can be applied at time of found_source
	 */
	void SetSeekInfo( double rate, gint64 start, gint64 stop )
	{
		this->rate = rate;
		this->start = start;
		this->stop = stop;
		this->injectedSeconds = 0.0;
	}
	
	/**
	 * @brief create, link, and confiugre a playbin element for specified media track
	 * @param mediaType tracktype, i.e. eMEDIATYPE_AUDIO or eMEDIATYPE_VIDEO
	 */
	void Configure( GstElement *pipeline )
	{
		g_print( "MediaStream::Configure\n" );
		if( isConfigured )
		{
			g_print( "NOP - aleady configured\n" );
		}
		else
		{
			isConfigured = true;
			sinkbin = gst_element_factory_make("playbin", NULL);
			gst_bin_add(GST_BIN(pipeline), sinkbin);
			g_object_set( sinkbin, "uri", "appsrc://", NULL);
			g_signal_connect( sinkbin, "deep-notify::source", G_CALLBACK(found_source_cb), this );
			g_signal_connect( sinkbin, "element_setup", G_CALLBACK(element_setup_cb), this );
			DumpFlags();
		}
	}

	double GetInjectedSeconds( void )
	{
		return injectedSeconds;
	}

	long long GetPositionMilliseconds( void )
	{
		long long ms = -1;
		if( sinkbin )
		{
			gint64 position = GST_CLOCK_TIME_NONE;
			if( gst_element_query_position(sinkbin, GST_FORMAT_TIME, &position) )
			{
				// g_print("position: %" GST_TIME_FORMAT "\n", GST_TIME_ARGS(position));
				ms = GST_TIME_AS_MSECONDS(position);
			}
		}
		return ms;
	}
	
	void need_data( GstElement *appSrc, guint length )
	{
		// noisy when time based buffering in use
		//g_print( "MediaStream::need_data(%s) length=%d\n", getMediaTypeAsString(), length );
		context->NeedData( mediaType );
	}
	
	void enough_data( GstElement *appSrc )
	{
		g_print( "MediaStream::enough_data\n" );
		context->EnoughData( mediaType );
	}
	
	gboolean appsrc_seek( GstElement *appSrc, guint64 offset )
	{
		double positionSeconds = static_cast<double>(offset)/GST_SECOND;
		g_print( "MediaStream::appsrc_seek %fs\n", positionSeconds );
		return TRUE;
	}
	
	/**
	 * @brief apply/update caps for audio/video to be presented
	 * @param pipeline AV pipeline to update
	 * @param info contains metadata extracted from mp4 initialization fragment
	 */
	void SetCaps( GstElement *pipeline, InitializationHeaderInfo *info )
	{
		GstCaps * caps = NULL;
		GstBuffer *buf = gst_buffer_new_and_alloc(info->codec_data_len);
		gst_buffer_fill(buf, 0, info->codec_data, info->codec_data_len);
		if( mediaType == eMEDIATYPE_VIDEO )
		{
			const char *media_type = NULL;
			const char *stream_format = NULL;
			//const char *level = NULL;
			
			switch( info->codec_type )
			{
				case 'hvcC':
					media_type = "video/x-h265";
					stream_format = "hvc1";
					// TODO: leverage gst_codec_utils_h265_caps_set_level_tier_and_profile
					//level = "4.1";
					break;
				case 'avcC':
					media_type = "video/x-h264";
					stream_format = "avc";
					// TODO: leverage gst_codec_utils_h264_caps_set_level_and_profile
					//level = "1";
					break;
				default:
					printf( "unk codec_type: %" PRIu32 "\n", info->codec_type );
					return;
			}
			caps = gst_caps_new_simple(
									   media_type,
									   "stream-format", G_TYPE_STRING, stream_format,
									   "alignment", G_TYPE_STRING, "au",
									   "codec_data", GST_TYPE_BUFFER, buf,
									   "width", G_TYPE_INT, info->width,
									   "height", G_TYPE_INT, info->height,
									   "pixel-aspect-ratio", GST_TYPE_FRACTION, 1, 1,
									   NULL );
		}
		else
		{
			switch( info->codec_type )
			{
				case 'esds':
					caps = gst_caps_new_simple(
											   "audio/mpeg",
											   "mpegversion",G_TYPE_INT,4,
											   "framed", G_TYPE_BOOLEAN, TRUE,
											   "stream-format",G_TYPE_STRING,"raw", // FIXME
											   "codec_data", GST_TYPE_BUFFER, buf,
											   NULL );
					break;
					
				case 'dec3':
					caps = gst_caps_new_simple(
											   "audio/x-eac3",
											   "framed", G_TYPE_BOOLEAN, TRUE,
											   "rate", G_TYPE_INT, info->samplerate,
											   "channels", G_TYPE_INT, info->channel_count,
											   NULL );
					break;
					
				default:
					assert(0);
					break;
			}
		}
		gst_app_src_set_caps(GST_APP_SRC(sourceObj), caps);
		gst_caps_unref(caps);
		gst_buffer_unref (buf);
	}
	
	void found_source( GObject * object, GObject * orig, GParamSpec * pspec )
	{
		g_print( "MediaStream::found_source %s\n", getMediaTypeAsString() );
		g_object_get( orig, pspec->name, &source, NULL );
		sourceObj = G_OBJECT(source);
		
		// initialize max-bytes based on default aampcli configuration
		// this drives byte-based need-data and enough-data signaling
		switch( mediaType )
		{
			case eMEDIATYPE_VIDEO:
				g_object_set(sourceObj, "max-bytes", 12582912, NULL ); // default = 200000
				break;
			case eMEDIATYPE_AUDIO:
				g_object_set(sourceObj, "max-bytes", 1536000, NULL ); // default = 200000
				break;
			default:
				break;
		}
		g_object_set(sourceObj, "min-percent", 50, NULL ); // default = 0
		
		g_signal_connect(sourceObj, "need-data", G_CALLBACK(need_data_cb), this );
		g_signal_connect(sourceObj, "enough-data", G_CALLBACK(enough_data_cb), this );
		g_signal_connect(sourceObj, "seek-data", G_CALLBACK(appsrc_seek_cb), this);
		gst_app_src_set_stream_type( GST_APP_SRC(sourceObj), GST_APP_STREAM_TYPE_SEEKABLE );
		g_object_set(sourceObj, "format", GST_FORMAT_TIME, NULL);

		
		if( 0 )
		{
			GstCaps * caps = gst_caps_new_simple("video/quicktime", NULL, NULL);
			gst_app_src_set_caps(GST_APP_SRC(sourceObj), caps );
			gst_caps_unref(caps);
		}
		else
		{
			g_object_set(sourceObj, "typefind", TRUE, NULL);
		}
		gboolean rc;
		if( rate<0 )
		{
			rc = gst_element_seek(
								  GST_ELEMENT(source),
								  rate,
								  GST_FORMAT_TIME,
								  GST_SEEK_FLAG_NONE,
								  GST_SEEK_TYPE_SET, 0,
								  GST_SEEK_TYPE_SET, start );
		}
		else
		{
			GstSeekType stop_type = (stop<0)?GST_SEEK_TYPE_NONE:GST_SEEK_TYPE_SET;
			rc = gst_element_seek(
								  GST_ELEMENT(sourceObj),
								  rate,
								  GST_FORMAT_TIME,
								  GST_SEEK_FLAG_NONE,
								  GST_SEEK_TYPE_SET, start,
								  stop_type, stop );
		}
		assert( rc );
	}

	void element_setup( GstElement * playbin, GstElement * element)
	{
		gchar* elemName = gst_element_get_name(element);
		g_print( "MediaStream::element_setup : %s\n", elemName ? elemName : "NULL");
		if (elemName && startsWith(elemName, "qtdemux"))
		{
			g_signal_connect(element, "pad-added", G_CALLBACK(pad_added_cb), this);
		}
		g_free(elemName);
	}

	void pad_added(GstElement* object, GstPad* arg0)
	{
		gchar* elemName = gst_element_get_name(object);
		gchar* padName = gst_pad_get_name(arg0);
		g_print( "MediaStream::pad_added : %s %s\n", elemName ? elemName : "NULL", padName ? padName : "NULL");
		qtdemux_pad = arg0;
		gst_object_ref(qtdemux_pad); // we need to ref here to hold on to this instance till unref
		qtdemux_probe_id = gst_pad_add_probe( qtdemux_pad, GST_PAD_PROBE_TYPE_BUFFER, (GstPadProbeCallback)MyDemuxPadProbeCallback, this, MyDestroyDataNotify );

		g_free(elemName);
		g_free(padName);
	}

	GstPadProbeReturn DemuxProbeCallback( GstBuffer* buffer , GstPad* pad )
	{
		if (startPos >= 0)
		{
			gchar* padName = gst_pad_get_name(pad);
			double pts = ((double) GST_BUFFER_PTS(buffer) / (double) GST_SECOND);
			if ((pts < startPos) & startsWith(padName, "audio"))
			{
				printf("DemuxProbeCallback: %s buffer: start pos=%f dropping pts=%f\n", getMediaTypeAsString(), startPos, pts);
				g_free(padName);
				return GST_PAD_PROBE_DROP;
			}
			g_free(padName);
		}
		return GST_PAD_PROBE_OK;
	}

	void Flush ( double pos )
	{ // use GST_SEEK_FLAG_ACCURATE?
		GstSeekFlags flags = (GstSeekFlags)(GST_SEEK_FLAG_FLUSH);
		gboolean rc = gst_element_seek(
								GST_ELEMENT(source),
								1.0,
								GST_FORMAT_TIME,
								flags,
								GST_SEEK_TYPE_SET, pos * GST_SECOND,
								GST_SEEK_TYPE_NONE, GST_CLOCK_TIME_NONE );
		injectedSeconds = 0.0;
		assert( rc );
	}

	void SetStartPos(double pos)
	{
		startPos = pos;
	}

	//copy constructor
	MediaStream(const MediaStream&)=delete;
	//copy assignment operator
	MediaStream& operator=(const MediaStream&)=delete;
private:
	GObject *sourceObj = NULL;
	GstPad* qtdemux_pad = NULL;
	gulong qtdemux_probe_id = 0;
	bool isConfigured; // avoid double configure
	double rate;
	gint64 start;
	gint64 stop;
	double flushPosition;
	double injectedSeconds;
	class PipelineContext *context;
	MediaType mediaType;
	GstElement *sinkbin;
	GstElement *source;
	double startPos;
		
	void DumpFlags( void )
	{
		static const char *mGetPlayFlagName[] =
		{
			"VIDEO",
			"AUDIO",
			"TEXT",
			"VIS",
			"SOFT_VOLUME",
			"NATIVE_AUDIO",
			"NATIVE_VIDEO",
			"DOWNLOAD",
			"BUFFERING",
			"DEINTERLACE",
			"SOFT_COLORBALANCE"
		};
		int numFlags = sizeof(mGetPlayFlagName)/sizeof(mGetPlayFlagName[0]);
		gint flags;
		g_object_get( sinkbin, "flags", &flags, NULL);
		g_print( "GST_PLAY_FLAG:\n" );
		for( int i=0; i<numFlags; i++ )
		{
			if( flags&(1<<i) )
			{
				g_print( "\t%s\n", mGetPlayFlagName[i] );
			}
		}
	}
};

static GstPadProbeReturn MyDemuxPadProbeCallback( GstPad * pad, GstPadProbeInfo * info, MediaStream *stream )
{ // C to C++ glue
	GstBuffer *buffer = GST_PAD_PROBE_INFO_BUFFER(info);
	return stream->DemuxProbeCallback(buffer,pad);
}

static void MyDestroyDataNotify( gpointer data )
{ // C to C++ glue
	printf( "MyDestroyDataNotify( %p )\n", data );
}

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
 * @brief sent when a seek event reaches the appsrc
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

static void element_setup_cb(GstElement * playbin, GstElement * element, class MediaStream *stream)
{ // C to C++ glue
	stream->element_setup( playbin, element);
}

static void pad_added_cb(GstElement* object, GstPad* arg0, class MediaStream *stream)
{ // C to C++ glue
	stream->pad_added(object, arg0);
}

gboolean bus_message_cb(GstBus * bus, GstMessage * msg, class Pipeline *pipeline )
{ // C to C++ glue
	return pipeline->bus_message( bus, msg );
}

GstBusSyncReply bus_sync_handler_cb(GstBus * bus, GstMessage * msg, Pipeline * pipeline )
{ // C to C++ glue
	return pipeline->bus_sync_handler( bus, msg );
}

Pipeline::Pipeline( class PipelineContext *context ) : position_adjust(0.0), start(0.0), stop(0.0), rate(0.0)
														,context(context),pipeline(gst_pipeline_new( MY_PIPELINE_NAME ))
														,bus(gst_pipeline_get_bus(GST_PIPELINE(pipeline)))
{
	gst_bus_add_watch( bus, (GstBusFunc) bus_message_cb, this );
	gst_object_unref(bus);
	gst_bus_set_sync_handler( bus, (GstBusSyncHandler) bus_sync_handler_cb, this, NULL);
	for( int i=0; i<NUM_MEDIA_TYPES; i++ )
	{
		mediaStream[i] = new MediaStream( (MediaType)i, context );
	}
}

void Pipeline::Configure( MediaType mediaType )
{
	mediaStream[mediaType]->Configure(pipeline);
}

void Pipeline::SetCaps( MediaType mediaType, InitializationHeaderInfo *info )
{
	mediaStream[mediaType]->SetCaps(pipeline, info);
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
	GstClockTime timeout = 0; // GST_CLOCK_TIME_NONE
	gst_element_get_state( pipeline, &state, &pending, timeout );
	return (PipelineState) state;
}

void Pipeline::SendBufferMP4( MediaType mediaType, gpointer ptr, gsize len, double duration )
{
	g_print( "Pipeline::SendBuffer(%s, len=%zu)\n", MediaType2Name(mediaType), len );
	mediaStream[mediaType]->SendBuffer(ptr,len,duration);
}
void Pipeline::SendBufferES( MediaType mediaType, gpointer ptr, gsize len, double duration, double pts, double dts )
{
	g_print( "Pipeline::SendBuffer(%s, len=%zu)\n", MediaType2Name(mediaType), len );
	mediaStream[mediaType]->SendBuffer(ptr,len,duration,pts,dts);
}

void Pipeline::SendGap( MediaType mediaType, double pts, double durationSeconds )
{
	g_print( "Pipeline::SendGap(%s,pts=%f,dur=%f)\n", MediaType2Name(mediaType), pts, durationSeconds );
	mediaStream[mediaType]->SendGap(pts,durationSeconds);
}

void Pipeline::SendEOS( MediaType mediaType )
{
	mediaStream[mediaType]->SendEOS();
}

void Pipeline::Flush( double segment_rate, double segment_start, double segment_stop, double baseTime )
{
	g_print( "Pipeline::Flush rate=%f start=%f stop=%f time=%f\n", segment_rate, segment_start, segment_stop, baseTime );
	
	rate = segment_rate;
	start = (gint64)(segment_start * GST_SECOND);
	
	if( segment_stop>0 )
	{
		stop = (gint64)(segment_stop*GST_SECOND);
	}
	else
	{
		stop = GST_CLOCK_TIME_NONE;
	}
	
	position_adjust = (gint64)(1000.0 * (baseTime - segment_start) );
	
	for( int i=0; i<NUM_MEDIA_TYPES; i++ )
	{
		mediaStream[i]->SetSeekInfo(rate,start,stop);
	}
	
	gboolean rc;
	// use GST_SEEK_FLAG_ACCURATE?
	GstSeekFlags flags = (GstSeekFlags)(GST_SEEK_FLAG_FLUSH);
	if( rate<0 )
	{
		rc = gst_element_seek(
							  GST_ELEMENT(pipeline),
							  rate,
							  GST_FORMAT_TIME,
							  flags,
							  GST_SEEK_TYPE_SET,
							  0,
							  GST_SEEK_TYPE_SET,
							  start );
	}
	else
	{
		rc = gst_element_seek(
							  GST_ELEMENT(pipeline),
							  rate,
							  GST_FORMAT_TIME,
							  flags,
							  GST_SEEK_TYPE_SET,
							  start,
							  (stop<0)?GST_SEEK_TYPE_NONE:GST_SEEK_TYPE_SET,
							  stop );
	}
	assert( rc );
}

void Pipeline::Flush( MediaType mediaType, double pos )
{
	printf( "Pipeline::Flush(%s, pos=%f)\n", MediaType2Name(mediaType), pos );
	mediaStream[mediaType]->Flush(pos);
	mediaStream[mediaType]->SetStartPos(pos);
}

long long Pipeline::GetPositionMilliseconds( MediaType mediaType )
{
	return mediaStream[mediaType]->GetPositionMilliseconds() + position_adjust;
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

void Pipeline::HandleGstMessageEOS( GstMessage *msg, const char *messageName )
{
	g_print( "%s from %s\n", messageName, GST_OBJECT_NAME(msg->src) );
	context->ReachedEOS();
}

static void buffer_underflow_callback_cb(GstElement* object, guint arg0, gpointer arg1, class Pipeline *pipeline )
{ // C to C++ glue
	g_print( "%s\n", __FUNCTION__ );
}
static void pts_error_callback_cb( GstElement* object, guint arg0, gpointer arg1, class Pipeline *pipeline )
{ // C to C++ glue
	g_print( "%s\n", __FUNCTION__ );
}
static void decode_error_callback_cb( GstElement* object, guint arg0, gpointer arg1, class Pipeline *pipeline )
{ // C to C++ glue
	g_print( "%s\n", __FUNCTION__ );
}

static const char *buffer_underflow_cb = "buffer-underflow-callback";
static const char *pts_error_cb = "pts-error-callback";
static const char *decode_error_cb = "decode-error-callback";
															   
void Pipeline::HandleGstMessageStateChanged( GstMessage *msg, const char *messageName )
{
	if( (!gQuiet) || strcmp( GST_OBJECT_NAME(msg->src), MY_PIPELINE_NAME ) == 0 )
	{
		GstState old_state, new_state, pending_state;
		gst_message_parse_state_changed(msg, &old_state, &new_state, &pending_state);
		const char *name = GST_OBJECT_NAME(msg->src);
		g_print("%s: %s %s -> %s (pending %s)\n",
				messageName,
				name,
				gst_element_state_get_name(old_state),
				gst_element_state_get_name(new_state),
				gst_element_state_get_name(pending_state));
		

		if (old_state == GST_STATE_NULL && new_state == GST_STATE_READY)
		{
			if(
			   strstr(name,"westerossink") ||
			   strstr(name,"audiosink") || strstr(name,"videosink") || // rialtomsevideosink, rialtomseaudiosink
			   strstr(name,"videodecoder") || strstr(name,"audiodecoder") || // brcmvideodecoder, brcmaudiodecoder
			   strstr(name,"omx") || strstr(name,"rtkv1sink") )
			{
				g_print( "found sink: %s\n", name );
				auto obj = G_OBJECT_GET_CLASS(msg->src);
				if( g_object_class_find_property( obj, buffer_underflow_cb)!=NULL)
				{
					g_print( "g_signal_connect: %s\n", buffer_underflow_cb );
					g_signal_connect(msg->src, buffer_underflow_cb, G_CALLBACK(buffer_underflow_callback_cb), this );
				}
				if( g_object_class_find_property( obj,pts_error_cb)!=NULL)
				{
					g_print( "g_signal_connect: %s\n", pts_error_cb );
					g_signal_connect(msg->src, pts_error_cb, G_CALLBACK(pts_error_callback_cb), this );
				}
				if( g_object_class_find_property( obj,decode_error_cb)!=NULL)
				{
					g_print( "g_signal_connect: %s\n", decode_error_cb );
					g_signal_connect(msg->src, decode_error_cb, G_CALLBACK(decode_error_callback_cb), this );
				}
			}
		}
	}
}

void Pipeline::HandleGstMessageQOS( GstMessage * msg, const char *messageName )
{
	gboolean live;
	guint64 running_time;
	guint64 stream_time;
	guint64 timestamp;
	guint64 duration;
	gst_message_parse_qos(msg, &live, &running_time, &stream_time, &timestamp, &duration);
	g_print(
			"%s: tlive=%d"
			" running_time=%" G_GUINT64_FORMAT
			" stream_time=%" G_GUINT64_FORMAT
			" timestamp=%" G_GUINT64_FORMAT
			" duration=%" G_GUINT64_FORMAT
			"\n",
			messageName,
			live, running_time, stream_time, timestamp, duration );
}

void myGstTagForeachFunc( const GstTagList * list, const gchar * tag, gpointer user_data )
{
	guint size = gst_tag_list_get_tag_size( list, tag );
	for( auto index=0; index<size; index++ )
	{
		const GValue *value = gst_tag_list_get_value_index( list, tag, index );
		gchar * valueString = g_strdup_value_contents(value);
		g_print( "\t%s:%s\n", tag, valueString );
		g_free( valueString );
	}
}

void Pipeline::HandleGstMessageTag( GstMessage *msg, const char *messageName )
{
	g_print( "%s\n", messageName );
	GstTagList *list = NULL;
	gst_message_parse_tag( msg, &list );
	if( list )
	{
		gpointer user_data = NULL;
		gst_tag_list_foreach( list, myGstTagForeachFunc, user_data );
		gst_tag_list_unref( list );
	}
}

void Pipeline::HandleGstMsg( GstMessage *msg )
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
		case GST_MESSAGE_EOS:
			HandleGstMessageEOS( msg, messageName );
			break;
		case GST_MESSAGE_STATE_CHANGED:
			HandleGstMessageStateChanged( msg, messageName );
			break;
		case GST_MESSAGE_TAG:
			if( !gQuiet )
			{
				HandleGstMessageTag( msg, messageName );
			}
			break;
		case GST_MESSAGE_QOS:
			if( !gQuiet )
			{
				HandleGstMessageQOS( msg, messageName );
			}
			break;
		default:
			if( !gQuiet )
			{
				g_print( "%s\n", messageName );
			}
			break;
	}
}

gboolean Pipeline::bus_message( _GstBus * bus, _GstMessage * msg )
{
	HandleGstMsg( msg );
	return TRUE;
}

GstBusSyncReply Pipeline::bus_sync_handler( _GstBus * bus, _GstMessage * msg )
{
	HandleGstMsg( msg );
	return GST_BUS_PASS;
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
	printf( "Pipeline::Step\n" );
	gst_element_send_event( pipeline, gst_event_new_step(GST_FORMAT_BUFFERS, 1, 1, TRUE, FALSE) );
}

void Pipeline::InstantaneousRateChange( double newRate )
{
	printf( "Pipeline::InstantaneousRateChange(%lf)\n", newRate );
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
	printf( "Instantaneous Rate Change not supported in gstreamer version %d.%d.%d, requires version 1.18\n",
			GST_VERSION_MAJOR, GST_VERSION_MINOR, GST_VERSION_MICRO );
	assert( false );
#endif
}
