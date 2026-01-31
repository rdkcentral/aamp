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
#include "gst-utils.h"
#include <cstdio>

bool gstutils_quiet = true;

// Logging category for utilities
GST_DEBUG_CATEGORY_STATIC(gstutils_cat);
#define GST_CAT_DEFAULT gstutils_cat

const char *gstutils_GetMediaTypeName( MediaType mediaType )
{
	switch( mediaType )
	{
		case eMEDIATYPE_AUDIO:
			return "audio";
		case eMEDIATYPE_VIDEO:
			return "video";
		default:
			return "?";
	}
}

void gstutils_DumpFlags( GstElement *playbin )
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
	g_object_get( playbin, "flags", &flags, NULL);
	GST_INFO( "GST_PLAY_FLAG:" );
	for( int i=0; i<numFlags; i++ )
	{
		if( flags&(1<<i) )
		{
			GST_INFO( "\t%s", mGetPlayFlagName[i] );
		}
	}
}

void gstutils_element_setup_cb(GstElement * playbin, GstElement * element, class MediaStream *stream)
{
	gchar* elemName = gst_element_get_name(element);
	GST_INFO( "element_setup : %s", elemName ? elemName : "NULL");
	g_free(elemName);
}

static void buffer_underflow_callback_cb(GstElement* object, guint arg0, gpointer arg1, class Pipeline *pipeline )
{
	GST_WARNING_OBJECT(object, "%s", __FUNCTION__ );
}
static void pts_error_callback_cb( GstElement* object, guint arg0, gpointer arg1, class Pipeline *pipeline )
{
	GST_WARNING_OBJECT(object, "%s", __FUNCTION__ );
}
static void decode_error_callback_cb( GstElement* object, guint arg0, gpointer arg1, class Pipeline *pipeline )
{
	GST_WARNING_OBJECT(object, "%s", __FUNCTION__ );
}

#define BUFFER_UNDERFLOW_CALLBACK "buffer-underflow-callback"
#define PTS_ERROR_CALLBACK "pts-error-callback"
#define DECODE_ERROR_CALLBACK "decode-error-callback"

void gstutils_Init( void )
{
	GST_DEBUG_CATEGORY_INIT(gstutils_cat, "gstutils", 0, "GStreamer utility logs");
}

void gstutils_HandleGstMessageStateChanged( GstMessage *msg, const char *messageName )
{
	void *userData = NULL;
	const char *name = GST_OBJECT_NAME(msg->src);
	GstState old_state, new_state, pending_state;
	gst_message_parse_state_changed(msg, &old_state, &new_state, &pending_state);
	if( strcmp(name,"test-pipeline")==0 )
	{
		printf( "%s -> %s\n",
			gst_element_state_get_name(old_state),
			gst_element_state_get_name(new_state) );
	}
	if( !gstutils_quiet )
	{
		GST_INFO("%s: %s %s -> %s (pending %s)",
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
				GST_INFO( "found sink: %s", name );
				auto obj = G_OBJECT_GET_CLASS(msg->src);
				if( g_object_class_find_property( obj, BUFFER_UNDERFLOW_CALLBACK)!=NULL)
				{
					GST_DEBUG( "g_signal_connect: %s", BUFFER_UNDERFLOW_CALLBACK );
					g_signal_connect(msg->src, BUFFER_UNDERFLOW_CALLBACK, G_CALLBACK(buffer_underflow_callback_cb), userData );
				}
				if( g_object_class_find_property( obj,PTS_ERROR_CALLBACK)!=NULL)
				{
					GST_DEBUG( "g_signal_connect: %s", PTS_ERROR_CALLBACK );
					g_signal_connect(msg->src, PTS_ERROR_CALLBACK, G_CALLBACK(pts_error_callback_cb), userData );
				}
				if( g_object_class_find_property( obj,DECODE_ERROR_CALLBACK)!=NULL)
				{
					GST_DEBUG( "g_signal_connect: %s", DECODE_ERROR_CALLBACK );
					g_signal_connect(msg->src, DECODE_ERROR_CALLBACK, G_CALLBACK(decode_error_callback_cb), userData );
				}
			}
		}
	}
}

void gstutils_HandleGstMessageStreamStatus( GstMessage *message, const char *messageName )
{
	if( !gstutils_quiet )
	{
		GstStreamStatusType type;
		GstElement *owner;
		GST_DEBUG( "%s", messageName );
		gst_message_parse_stream_status(message, &type, &owner);
		const GValue *val = gst_message_get_stream_status_object (message);
		GST_DEBUG( "type: %d", type );
		gchar *path = gst_object_get_path_string (GST_MESSAGE_SRC (message));
		GST_DEBUG( "source: %s", path );
		g_free(path);
		path = gst_object_get_path_string (GST_OBJECT (owner));
		GST_DEBUG( "owner: %s", path );
		g_free (path);
		GST_DEBUG( "object: type %s, value %p", G_VALUE_TYPE_NAME(val), g_value_get_object(val) );
		GstTask *task = NULL;
		if( G_VALUE_TYPE(val) == GST_TYPE_TASK )
		{
			task = (GstTask *)g_value_get_object(val);
		}
		switch( type )
		{
			case GST_STREAM_STATUS_TYPE_CREATE:
				GST_DEBUG( "GST_STREAM_STATUS_TYPE_CREATE %p", task);
				break;
			case GST_STREAM_STATUS_TYPE_ENTER:
				break;
			case GST_STREAM_STATUS_TYPE_LEAVE:
				break;
			default:
				break;
		}
	}
}

void gstutils_HandleGstMessageQOS( GstMessage * msg, const char *messageName )
{
	if( !gstutils_quiet )
	{
		gboolean live;
		guint64 running_time;
		guint64 stream_time;
		guint64 timestamp;
		guint64 duration;
		gst_message_parse_qos(msg, &live, &running_time, &stream_time, &timestamp, &duration);
		GST_DEBUG(
				  "%s: tlive=%d"
				  " running_time=%" G_GUINT64_FORMAT
				  " stream_time=%" G_GUINT64_FORMAT
				  " timestamp=%" G_GUINT64_FORMAT
				  " duration=%" G_GUINT64_FORMAT,
				  messageName, live, running_time, stream_time, timestamp, duration );
	}
}

static void myGstTagForeachFunc( const GstTagList * list, const gchar * tag, gpointer user_data )
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

void gstutils_HandleGstMessageTag( GstMessage *msg, const char *messageName )
{
	if( !gstutils_quiet )
	{
		GST_INFO( "%s", messageName );
		GstTagList *list = NULL;
		gst_message_parse_tag( msg, &list );
		if( list )
		{
			gpointer user_data = NULL;
			gst_tag_list_foreach( list, myGstTagForeachFunc, user_data );
			gst_tag_list_unref( list );
		}
	}
}
