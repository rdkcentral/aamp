/*
 * If not stated otherwise in this file or this component's license file the
 * following copyright and licenses apply:
 *
 * Copyright 2022 RDK Management
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
#include <gst/gst.h>
#include <gst/app/gstappsrc.h>
#include <gst/app/gstappsink.h>
#include <math.h>

#define MY_PIPELINE_NAME "test-pipeline"
static bool gQuiet = false; // set to true for less chatty logging

static void need_data_cb(GstElement *appSrc, guint length, MediaStream *stream );
static void enough_data_cb(GstElement *appSrc, MediaStream *stream );
static gboolean appsrc_seek_cb(GstElement * appSrc, guint64 offset, MediaStream *stream );
static void found_source_cb(GObject * object, GObject * orig, GParamSpec * pspec, class MediaStream *stream );
static void element_setup_cb(GstElement * playbin, GstElement * element, class MediaStream *stream);
static void pad_added_cb(GstElement* object, GstPad* arg0, class MediaStream *stream);
static void pad_added_cb2(GstElement* object, GstPad* arg0, class MediaStream *stream);
static GstPadProbeReturn MyPadProbeCallback( GstPad * pad, GstPadProbeInfo * info, class MediaStream *stream );
static GstPadProbeReturn MyDemuxPadProbeCallback( GstPad * pad, GstPadProbeInfo * info, class MediaStream *stream );
static GstPadProbeReturn QtMonitorProbeCallback( GstPad * pad, GstPadProbeInfo * info, class MediaStream *stream );
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
	MediaStream( MediaType mediaType, class PipelineContext *context ) : isConfigured(false), sinkbin(NULL), source(NULL), required_caps(NULL), rate(), start(), stop(), pts_offset(), context(context), mediaType(mediaType), pts(), dts(), duration(), startPos(-1) {
	}
	
	
	~MediaStream()
	{
		if( probe_id )
		{
			gst_pad_remove_probe( pad, probe_id );
		}
		if( pad )
		{
			gst_object_unref( pad );
		}

		if( qtdemux_probe_id )
		{
			gst_pad_remove_probe( qtdemux_pad, qtdemux_probe_id );
		}
		if( qtdemux_probe_id2 )
		{
			gst_pad_remove_probe( qtdemux_pad2, qtdemux_probe_id2 );
		}
		if( qtdemux_pad )
		{
			gst_object_unref( qtdemux_pad );
		}
		if( qtdemux_pad2 )
		{
			gst_object_unref( qtdemux_pad2 );
		}
	}
	
	void SetTimestampOffset( double pts )
	{
		assert( pad );
		pts_offset = pts;
		gint64 offs = pts * 1000000000LL;
		printf("gst_pad_set_offset(%" G_GINT64_FORMAT ")\n", offs );
		gst_pad_set_offset( pad, offs );
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
	
	void SendBuffer( gpointer ptr, gsize len )
	{
		if( ptr )
		{
			GstBuffer *gstBuffer = gst_buffer_new_wrapped( ptr, len );
			GstFlowReturn ret = gst_app_src_push_buffer(GST_APP_SRC(source), gstBuffer );
			switch( ret )
			{
				case GST_FLOW_OK:
				case GST_FLOW_EOS:
					break;
				default:
					g_print( "unexpected GstFlowReturn: %d\n", ret );
					break;
			}
		}
	}

	void SendBuffer( gpointer ptr, gsize len, double pts, double dts, double duration )
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
				case GST_FLOW_EOS:
					break;
				default:
					g_print( "unexpected GstFlowReturn: %d\n", ret );
					break;
			}
		}
	}
	
	void SendGap( double pts, double durationSeconds )
	{
		g_print( "MediaStream::SendGap(mediaType=%d,pts=%f,dur=%f)\n", mediaType, pts, durationSeconds );
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
	void SetSeekInfo( double rate, gint64 start, gint64 stop, double pts_offset )
	{
		this->rate = rate;
		this->start = start;
		this->stop = stop;
		this->pts_offset = pts_offset;
		if( pad )
		{
			SetTimestampOffset(pts_offset);
		}
	}
	
	/**
	 * @brief create, link, and confiugre a playbin element for specified media track
	 * @param mediaType tracktype, i.e. eMEDIATYPE_AUDIO or eMEDIATYPE_VIDEO
	 * @param required_caps if NULL, use typefind, otherwise apply specified caps, i.e. "video/quicktime"
	 */
	void Configure( GstElement *pipeline, const char *media_type )
	{
		g_print( "MediaStream::Configure\n" );
		if( isConfigured )
		{
			g_print( "NOP - aleady configured\n" );
		}
		else
		{
			isConfigured = true;
			this->required_caps = media_type;
			sinkbin = gst_element_factory_make("playbin", NULL);
			this->pipeline=pipeline;
			gst_bin_add(GST_BIN(pipeline), sinkbin);
			g_object_set( sinkbin, "uri", "appsrc://", NULL);
			g_signal_connect( sinkbin, "deep-notify::source", G_CALLBACK(found_source_cb), this );
			g_signal_connect( sinkbin, "element_setup", G_CALLBACK(element_setup_cb), this );
			DumpFlags();
		}
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
		
		/* below adjustment is ALMOST right, but we have race condition due to lag between fragments already injected with old pts_offset
		 */
		ms += pts_offset*1000;
		return ms;
	}
	
	void need_data( GstElement *appSrc, guint length )
	{
		g_print( "MediaStream::need_data(%s)\n", getMediaTypeAsString() );
		context->NeedData( mediaType );
	}
	
	void enough_data( GstElement *appSrc )
	{
		g_print( "MediaStream::enough_data\n" );
		context->EnoughData( mediaType );
	}
	
	void appsrc_seek( GstElement *appSrc, guint64 offset )
	{
		double positionSeconds = static_cast<double>(offset)/GST_SECOND;
		g_print( "MediaStream::appsrc_seek %fs\n", positionSeconds );
	}
	
	void found_source( GObject * object, GObject * orig, GParamSpec * pspec )
	{
		g_print( "MediaStream::found_source %s\n", getMediaTypeAsString() );
		g_object_get( orig, pspec->name, &source, NULL );
		GObject *sourceObj = G_OBJECT(source);

		pad = gst_element_get_static_pad(source, "src");
		probe_id = gst_pad_add_probe( pad, GST_PAD_PROBE_TYPE_BUFFER, (GstPadProbeCallback)MyPadProbeCallback, this, MyDestroyDataNotify );
		gst_pad_set_offset( pad, pts_offset*1000000000LL );
		
		g_signal_connect(sourceObj, "need-data", G_CALLBACK(need_data_cb), this );
		g_signal_connect(sourceObj, "enough-data", G_CALLBACK(enough_data_cb), this );
		g_signal_connect(sourceObj, "seek-data", G_CALLBACK(appsrc_seek_cb), this);
		gst_app_src_set_stream_type( GST_APP_SRC(sourceObj), GST_APP_STREAM_TYPE_SEEKABLE );
		g_object_set(sourceObj, "format", GST_FORMAT_TIME, NULL);
		
		if( required_caps )
		{
			GstCaps * caps = gst_caps_new_simple(required_caps, NULL, NULL);
			gst_app_src_set_caps(GST_APP_SRC(sourceObj), caps);
			gst_caps_unref(caps);
		}
		else
		{
#ifdef REALTEK_HACK
			if( mediaType == eMEDIATYPE_VIDEO )
			{
				GstCaps * caps = gst_caps_new_simple ("video/x-h264", "enable-fastplayback", G_TYPE_STRING, "true", NULL );
				gst_app_src_set_caps(GST_APP_SRC(sourceObj), caps);
				gst_caps_unref(caps);
			}
			else
			{
				g_object_set(sourceObj, "typefind", TRUE, NULL);
			}
#else
			g_object_set(sourceObj, "typefind", TRUE, NULL);
#endif
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
		gchar* elemParent = gst_element_get_name(gst_element_get_parent(element));
		g_print( "MediaStream:element_setup  : %s ( by %s )\n", elemName ? elemName : "NULL",elemParent ? elemParent : "NULL");
		if (elemName && startsWith(elemName, "qtdemux"))
		{
			g_signal_connect(element, "pad-added", G_CALLBACK(pad_added_cb), this);
		}
		if (elemName && startsWith(elemName, "multiqueue"))
		{
			g_signal_connect(element, "pad-added", G_CALLBACK(pad_added_cb2), this);
		}
		g_free(elemName);g_free(elemParent);
	}

	void pad_added2(GstElement* object, GstPad* arg0)
	{
		gchar* elemName = gst_element_get_name(object);
		gchar* padName = gst_pad_get_name(arg0);
		g_print( "Multiqueue ::pad_added : %s %s\n", elemName ? elemName : "NULL", padName ? padName : "NULL");
		qtdemux_pad2= gst_element_get_static_pad(object, "sink_0");
		qtdemux_probe_id2=gst_pad_add_probe( qtdemux_pad2, GST_PAD_PROBE_TYPE_EVENT_DOWNSTREAM, (GstPadProbeCallback)QtMonitorProbeCallback, this, MyDestroyDataNotify );
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

	void PadProbeCallback( void )
	{
		context->PadProbeCallback( mediaType );
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
	{
		gboolean rc = gst_element_seek(
								GST_ELEMENT(source),
								1.0,
								GST_FORMAT_TIME,
								GST_SEEK_FLAG_FLUSH,
								GST_SEEK_TYPE_SET, pos * GST_SECOND,
								GST_SEEK_TYPE_NONE, GST_CLOCK_TIME_NONE );
		assert( rc );
	}

	void SetStartPos(double pos)
	{
		startPos = pos;
	}

	GstElement* getPipeline()
	{
		return pipeline;
	}

	//copy constructor
	MediaStream(const MediaStream&)=delete;
	//copy assignment operator
	MediaStream& operator=(const MediaStream&)=delete;
private:
	double pts, dts, duration;
	
	GstPad* pad = NULL;
	gulong probe_id = 0;
	GstPad* qtdemux_pad = NULL;
	GstPad* qtdemux_pad2 = NULL;
	gulong qtdemux_probe_id = 0;
	gulong qtdemux_probe_id2 = 0;
	bool isConfigured; // avoid double configure
	double rate;
	gint64 start;
	gint64 stop;
	double pts_offset;
	const char *required_caps;
	class PipelineContext *context;
	MediaType mediaType;
	GstElement *sinkbin;
	GstElement *pipeline;
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

GstElement* createAudioParser(GstCaps* audioCaps) {
    GstElement* parser = nullptr;
    GList* parsers = gst_element_factory_list_get_elements(GST_ELEMENT_FACTORY_TYPE_PARSER, GST_RANK_MARGINAL);

    for (GList* walk = parsers; !parser && walk; walk = g_list_next(walk)) {
        GstElementFactory* factory = reinterpret_cast<GstElementFactory*>(walk->data);

        if (gst_element_factory_can_sink_all_caps(factory, audioCaps)) {
            parser = gst_element_factory_create(factory, nullptr);
            if (parser) {
                g_print("Found parser element: %s\n", gst_element_factory_get_longname(factory));
				g_print("Found parser element: %s\n", gst_element_get_name(parser));
            } else {
                g_printerr("Failed to create parser element\n");
            }
        }
    }

    gst_plugin_feature_list_free(parsers);

    return parser;
}

GstElement* createAudioDecoder(GstCaps* audioCaps) {
    GstElement* decoder = nullptr;
    GList* decoders = gst_element_factory_list_get_elements(GST_ELEMENT_FACTORY_TYPE_DECODER, GST_RANK_MARGINAL);

	for (GList* walk = decoders; !decoder && walk; walk = g_list_next(walk)) {
        GstElementFactory* factory = reinterpret_cast<GstElementFactory*>(walk->data);

        if (gst_element_factory_can_sink_all_caps(factory, audioCaps)) {
            decoder = gst_element_factory_create(factory, nullptr);
            if (decoder) {
                g_print("Found decoder element: %s\n", gst_element_factory_get_longname(factory));
                g_print("Found decoder element: %s\n", gst_element_get_name(decoder));
            } else {
                g_printerr("Failed to create decoder element\n");
            }
        }
    }

    gst_plugin_feature_list_free(decoders);

    return decoder;
}

static GstPadProbeReturn MyPadProbeCallback( GstPad * pad, GstPadProbeInfo * info, MediaStream *stream )
{ // C to C++ glue
	printf("pad probe: type=%d size=%d data=%p offset=%" G_GUINT64_FORMAT " id=%lu\n",
		   info->type,
		   info->size,
		   info->data,
		   info->offset,
		   info->id );
	stream->PadProbeCallback();
	return GST_PAD_PROBE_OK;
}

static GstPadProbeReturn MyDemuxPadProbeCallback( GstPad * pad, GstPadProbeInfo * info, MediaStream *stream )
{ // C to C++ glue
	// printf("pad probe: type=%d size=%d data=%p offset=%" G_GUINT64_FORMAT " id=%lu\n",
	// 	   info->type,
	// 	   info->size,
	// 	   info->data,
	// 	   info->offset,
	// 	   info->id );
	GstBuffer *buffer = GST_PAD_PROBE_INFO_BUFFER(info);
	return stream->DemuxProbeCallback(buffer,pad);
}

GstPadProbeReturn QtMonitorProbeCallback(GstPad *pad, GstPadProbeInfo *info, MediaStream *stream)
{
	GstEvent *event = GST_PAD_PROBE_INFO_EVENT(info);
    if (GST_EVENT_TYPE(event) == GST_EVENT_CAPS) {
		gchar * qname;
        GstCaps *caps;
        gst_event_parse_caps(event, &caps);

        GstStructure *structure = gst_caps_get_structure(caps, 0);
        const gchar *media_type = gst_structure_get_name(structure);

		GstElement *pipeline = stream->getPipeline();
		GstElement *mqueue =  gst_pad_get_parent_element(pad);

		GstBin *parent_bin = GST_BIN(gst_element_get_parent(mqueue));
		GstPad* queuesrc = gst_element_get_static_pad(mqueue, "src_0");
		GstPad* queuesink = gst_element_get_static_pad(mqueue, "sink_0");
		GstPad* currentparsersink = gst_pad_get_peer(queuesrc);

        if (g_str_has_prefix(media_type,"audio") && !gst_pad_query_accept_caps(currentparsersink,caps)) {
            g_print("Detected codec change to %s\n",media_type);

            // Create new parser and decoder elements
			 g_print("creating parser element \n");
            GstElement *newparser = createAudioParser(caps);
			 g_print("creating decoder element\n");
            GstElement *newdecoder = createAudioDecoder(caps);

            if (!newparser || !newdecoder) {
                g_printerr("Failed to create new elements\n");
            }
			else
			{
				g_print("New elements created successfully\n");
				g_print("Adding to decodebin\n");
				gst_bin_add_many(parent_bin, newparser, newdecoder, NULL);
			}

 			// Link newparser and newdecoder
            if (!gst_element_link_many(newparser, newdecoder, NULL)) {
                g_printerr("Failed to link new parser and decoder elements\n");
            }
			else
			{
				g_print("New parser and decoder elements linked successfully\n");
			}

            // Get existing aacparse and avdec_aac elements
            GstElement *oldparser = gst_pad_get_parent_element(currentparsersink);
			GstPad* oldparsersrc = gst_element_get_static_pad(oldparser, "src");
			GstPad* olddecodersink = gst_pad_get_peer(oldparsersrc);
			GstElement *olddecoder = gst_pad_get_parent_element(olddecodersink);
			GstPad* olddecsrc = gst_element_get_static_pad(olddecoder, "src");
			GstPad* audiosink = gst_pad_get_peer(olddecsrc);

			// Unlink old parser from queue
			if(!gst_pad_unlink(queuesrc, currentparsersink))
			{
				g_printerr("Failed to unlink old parser\n");
			}
			else
			{
				g_print("old parser unlinked successfully\n");
			}

			GstPad* newpsink = gst_element_get_static_pad(newparser, "sink");

			// Link queue to new parser
            if (!gst_pad_link(queuesrc, newpsink)) {
                g_printerr("Failed to link queue to newparser\n");
            }
			else
			{
				g_print("New parser linked to queue successfully \n");
			}

			// Unlink old decoder from audio sink
			if(!gst_pad_unlink(olddecsrc, audiosink))
			{
				g_printerr("Failed to unlink old decoder\n");
			}
			else
			{
				g_print("old decoder unlinked successfully\n");
			}


			// Link new decoder to audio sink
			g_print("Trying to link to audio sink (%s) \n",gst_pad_get_name(audiosink));
			GstPad *audiosrc = gst_element_get_static_pad(newdecoder, "src");
			if(!gst_pad_link(audiosrc, audiosink))
			{
				g_printerr("Failed to link to audio sink\n");
			}
			else
			{
				g_print("audio sink linked to decoder successfully\n");
			}

			gst_element_set_state(newparser, GST_STATE_PLAYING);
			gst_element_set_state(newdecoder, GST_STATE_PLAYING);
            gst_element_set_state(pipeline, GST_STATE_PLAYING);

            // Cleanup
			gst_bin_remove_many(parent_bin, oldparser, olddecoder, NULL);

			gst_element_set_state(oldparser, GST_STATE_NULL);
			gst_element_set_state(olddecoder, GST_STATE_NULL);
			gst_object_unref(olddecoder);
			gst_object_unref(oldparser);

            g_print("Pipeline reconfigured for %s decoding\n",media_type);
			return GST_PAD_PROBE_OK;
        }

		gst_object_unref(mqueue);
        gst_caps_unref(caps);
    }

    return GST_PAD_PROBE_OK;
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
	stream->appsrc_seek( appSrc, offset );
	return TRUE;
}

/**
 * @brief handle gstreamer signal that an appropriate sink source has been identified, and can now be tracked/used by app
 */
static void found_source_cb(GObject * object, GObject * orig, GParamSpec * pspec, class MediaStream *stream )
{ // C to C++ glue
	stream->found_source( object, orig, pspec );
}

static void element_setup_cb(GstElement * playbin, GstElement * element, class MediaStream *stream)
{
	stream->element_setup( playbin, element);
}

static void pad_added_cb(GstElement* object, GstPad* arg0, class MediaStream *stream)
{
	stream->pad_added(object, arg0);
}

static void pad_added_cb2(GstElement* object, GstPad* arg0, class MediaStream *stream)
{
	stream->pad_added2(object, arg0);
}

gboolean bus_message_cb(GstBus * bus, GstMessage * msg, class Pipeline *pipeline )
{
	pipeline->bus_message( bus, msg );
	return TRUE;
}

GstBusSyncReply bus_sync_handler_cb(GstBus * bus, GstMessage * msg, Pipeline * pipeline )
{
	pipeline->bus_sync_handler( bus, msg );
	return GST_BUS_PASS;
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

void Pipeline::SetTimestampOffset( MediaType mediaType, double pts_offset )
{
	mediaStream[mediaType]->SetTimestampOffset(pts_offset);
}

void Pipeline::Configure( MediaType mediaType, const char *required_caps )
{
	mediaStream[mediaType]->Configure(pipeline, required_caps);
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

void Pipeline::SendBuffer( MediaType mediaType, gpointer ptr, gsize len)
{
	g_print( "Pipeline::SendBuffer(mediaType=%d, len=%lu)\n", mediaType, len );
	mediaStream[mediaType]->SendBuffer(ptr,len);
}
void Pipeline::SendBuffer( MediaType mediaType, gpointer ptr, gsize len, double pts, double dts, double duration )
{
	g_print( "Pipeline::SendBuffer(mediaType=%d, len=%lu)\n", mediaType, len );
	mediaStream[mediaType]->SendBuffer(ptr,len,pts,dts,duration);
}

void Pipeline::SendGap( MediaType mediaType, double pts, double durationSeconds )
{
	g_print( "Pipeline::SendGap(mediaType=%d,pts=%f,dur=%f)\n", mediaType, pts, durationSeconds );
	mediaStream[mediaType]->SendGap(pts,durationSeconds);
}

void Pipeline::SendEOS( MediaType mediaType )
{
	mediaStream[mediaType]->SendEOS();
}

void Pipeline::Flush( double segment_rate, double segment_start, double segment_stop, double baseTime, double pts_offset )
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
		mediaStream[i]->SetSeekInfo(rate,start,stop,pts_offset);
	}
	
	gboolean rc;
	GstSeekFlags flags = GST_SEEK_FLAG_FLUSH;
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
	printf( "Pipeline::Flush(mediaType=%d, pos=%f)\n", mediaType, pos );
	mediaStream[mediaType]->Flush(pos);
	mediaStream[mediaType]->SetStartPos(pos);
}

long long Pipeline::GetPositionMilliseconds()
{
	long long position = mediaStream[eMEDIATYPE_VIDEO]->GetPositionMilliseconds();
	if( position<0 )
	{
		position = mediaStream[eMEDIATYPE_AUDIO]->GetPositionMilliseconds();
	}
	return position + position_adjust;
}

static void HandleGstMessageError( GstMessage *msg, const char *messageName )
{
	GError *error = NULL;
	gchar *dbg_info = NULL;
	gst_message_parse_error(msg, &error, &dbg_info);
	g_printerr("%s: %s %s\n", messageName, GST_OBJECT_NAME(msg->src), error->message);
	g_clear_error(&error);
	g_free(dbg_info);
}

static void HandleGstMessageWarning( GstMessage *msg, const char *messageName )
{
	GError *error = NULL;
	gchar *dbg_info = NULL;
	gst_message_parse_warning(msg, &error, &dbg_info);
	g_printerr("%s: %s %s\n", messageName, GST_OBJECT_NAME(msg->src), error->message);
	g_clear_error(&error);
	g_free(dbg_info);
}

static void HandleGstMessageEOS( GstMessage *msg, PipelineContext *context, const char *messageName )
{
	g_print( "%s from %s\n", messageName, GST_OBJECT_NAME(msg->src) );
	context->ReachedEOS();
}

static void HandleGstMessageStateChanged( GstMessage *msg, const char *messageName )
{
	if( (!gQuiet) || strcmp( GST_OBJECT_NAME(msg->src), MY_PIPELINE_NAME ) == 0 )
	{
		GstState old_state, new_state, pending_state;
		gst_message_parse_state_changed(msg, &old_state, &new_state, &pending_state);
		g_print("%s: %s %s -> %s (pending %s)\n",
				messageName,
				GST_OBJECT_NAME(msg->src),
				gst_element_state_get_name(old_state),
				gst_element_state_get_name(new_state),
				gst_element_state_get_name(pending_state));
	}
}

static void HandleGstMessageQOS( GstMessage * msg, const char *messageName )
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
static void HandleGstMessageTag( GstMessage *msg, const char *messageName )
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

static void HandleGstMsg( GstMessage *msg, PipelineContext *context )
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
			HandleGstMessageEOS( msg, context, messageName );
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

void Pipeline::bus_message( _GstBus * bus, _GstMessage * msg )
{
	HandleGstMsg( msg, context );
}

void Pipeline::bus_sync_handler( _GstBus * bus, _GstMessage * msg )
{
	HandleGstMsg( msg, context );
}

void Pipeline::DumpDOT( void )
{
	gchar *graphviz = gst_debug_bin_to_dot_data( (GstBin *)pipeline, GST_DEBUG_GRAPH_SHOW_ALL );
	// refer: https://graphviz.org/
	// brew install graphviz
	// dot -Tsvg gst-test.dot  > gst-test.svg
	FILE *f = fopen( "gst-test.dot", "wb" ); // TODO: what path to use?
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
