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

#include <gst/gst.h>
#include <gst/app/gstappsrc.h>

#include "MockGStreamer.h"

MockGStreamer *g_mockGStreamer = nullptr;

GstDebugCategory *GST_CAT_DEFAULT;
GstDebugLevel _gst_debug_min;

#if 0
#define TRACE_FUNC_ARG(format,...) printf("%s " ,__func__); printf(format,__VA_ARGS__)
#define TRACE_FUNC() printf("%s\n" ,__func__)
#else
#define TRACE_FUNC() ((void)0)
#define TRACE_FUNC_ARG(format,...) ((void)0)
#endif

void gst_debug_bin_to_dot_file(GstBin *bin, GstDebugGraphDetails details, const gchar *file_name)
{
}

GType gst_object_get_type(void)
{
	TRACE_FUNC();
	return 0;
}

GType gst_bin_get_type(void)
{
	TRACE_FUNC();
	return 0;
}

GstMiniObject *gst_mini_object_ref(GstMiniObject *mini_object)
{
	TRACE_FUNC();
	return NULL;
}

void gst_mini_object_unref(GstMiniObject *mini_object)
{
	TRACE_FUNC();
	if (g_mockGStreamer != nullptr)
	{
		g_mockGStreamer->gst_mini_object_unref(mini_object);
	}
}

GType gst_app_src_get_type(void)
{
	TRACE_FUNC();
	return 0;
}

void gst_app_src_set_caps(GstAppSrc *appsrc, const GstCaps *caps)
{
	TRACE_FUNC();
}

void gst_app_src_set_stream_type(GstAppSrc *appsrc, GstAppStreamType type)
{
	TRACE_FUNC();
}

GstFlowReturn gst_app_src_push_buffer(GstAppSrc *appsrc, GstBuffer *buffer)
{
	TRACE_FUNC();
	return GST_FLOW_OK;
}

GstBuffer *gst_buffer_new(void)
{

	TRACE_FUNC();
	return NULL;
}

GstBuffer *gst_buffer_new_allocate(GstAllocator *allocator, gsize size, GstAllocationParams *params)
{

	TRACE_FUNC();
	return NULL;
}

GstBuffer *gst_buffer_new_wrapped(gpointer data, gsize size)
{
	TRACE_FUNC();
	return NULL;
}

gboolean gst_buffer_map(GstBuffer *buffer, GstMapInfo *info, GstMapFlags flags)
{
	TRACE_FUNC();
	return FALSE;
}

void gst_buffer_unmap(GstBuffer *buffer, GstMapInfo *info)
{
	TRACE_FUNC();
}

const gchar *gst_element_state_get_name(GstState state)
{
	const char *ptr = NULL;
	const char *tbl[] = {
		"GST_STATE_VOID_PENDING", //(0) – no pending state.
		"GST_STATE_NULL", 	  // (1) – the NULL state or initial state of an element.
		"GST_STATE_READY",		  // (2) – the element is ready to go to PAUSED.
		"GST_STATE_PAUSED",		  // (3) – the element is PAUSED, it is ready to accept and process data. Sink elements however only accept one buffer and then block.
		"GST_STATE_PLAYING",	  // (4) –
	};

	if (state < (sizeof(tbl) / sizeof(tbl[0])))
	{
		ptr = tbl[state];
	}
	else
	{
		ptr = "error";
		printf("GstState %d\n",state);
	}
	TRACE_FUNC_ARG("%s \n",ptr);


	return (const gchar *)ptr;
}

GType gst_element_get_type(void)
{
	TRACE_FUNC();
	return 0;
}

void gst_message_parse_warning(GstMessage *message, GError **gerror, gchar **debug)
{

	TRACE_FUNC();
}

void gst_message_parse_error(GstMessage *message, GError **gerror, gchar **debug)
{
	TRACE_FUNC();
}

void gst_message_parse_state_changed(GstMessage *message, GstState *oldstate, GstState *newstate,
									 GstState *pending)
{
	TRACE_FUNC();
	if (g_mockGStreamer != nullptr)
	{
		g_mockGStreamer->gst_message_parse_state_changed(message, oldstate, newstate, pending);
	}
}

void gst_message_parse_qos(GstMessage *message, gboolean *live, guint64 *running_time,
						   guint64 *stream_time, guint64 *timestamp, guint64 *duration)
{
	TRACE_FUNC();
}

const GstStructure *gst_message_get_structure(GstMessage *message)
{

	TRACE_FUNC();
	return NULL;
}

gboolean gst_structure_has_name(const GstStructure *structure, const gchar *name)
{
	TRACE_FUNC();
	return FALSE;
}

const gchar *gst_message_type_get_name(GstMessageType type)
{
	TRACE_FUNC();
	return NULL;
}

gboolean gst_message_parse_context_type(GstMessage *message, const gchar **context_type)
{

	TRACE_FUNC();
	return FALSE;
}

GstContext *gst_context_new(const gchar *context_type, gboolean persistent)
{
	TRACE_FUNC();
	return NULL;
}

GstStructure *gst_context_writable_structure(GstContext *context)
{
	TRACE_FUNC();
	return NULL;
}

void gst_structure_set(GstStructure *structure, const gchar *fieldname, ...)
{
	TRACE_FUNC();
}

void gst_element_set_context(GstElement *element, GstContext *context)
{
	TRACE_FUNC();
}

GstElement *gst_pipeline_new(const gchar *name)
{

	TRACE_FUNC();
	GstElement *return_ptr = nullptr;
	if (g_mockGStreamer != nullptr)
	{
		return_ptr = g_mockGStreamer->gst_pipeline_new(name);
	}
	return return_ptr;
}

GType gst_pipeline_get_type(void)
{
	TRACE_FUNC();
	return 0;
}

GstBus *gst_pipeline_get_bus(GstPipeline *pipeline)
{
	GstBus *return_ptr = nullptr;
	TRACE_FUNC();
	if (g_mockGStreamer != nullptr)
	{
		return_ptr = g_mockGStreamer->gst_pipeline_get_bus(pipeline);
	}
	return return_ptr;
}

guint gst_bus_add_watch(GstBus *bus, GstBusFunc func, gpointer user_data)
{
	guint id = 0;
	TRACE_FUNC();
	if (g_mockGStreamer != nullptr)
	{
		id = g_mockGStreamer->gst_bus_add_watch(bus,func,user_data);
	}
	return id;
}

void gst_bus_set_sync_handler(GstBus *bus, GstBusSyncHandler func, gpointer user_data,
							  GDestroyNotify notify)
{
	TRACE_FUNC();
	if (g_mockGStreamer != nullptr)
	{
		g_mockGStreamer->gst_bus_set_sync_handler(bus, func, user_data, notify);
	}
}

GstQuery *gst_query_new_position(GstFormat format)
{
	GstQuery *return_ptr = nullptr;
	TRACE_FUNC();
	if (g_mockGStreamer != nullptr)
	{
		return_ptr = g_mockGStreamer->gst_query_new_position(format);
	}
	return return_ptr;
}

gpointer gst_object_ref(gpointer object)
{
	TRACE_FUNC();
	return NULL;
}

void gst_object_unref(gpointer object)
{
	TRACE_FUNC();

	if (g_mockGStreamer != nullptr)
	{
		g_mockGStreamer->gst_object_unref(object);
	}
}

void gst_debug_log(GstDebugCategory *category, GstDebugLevel level, const gchar *file,
				   const gchar *function, gint line, GObject *object, const gchar *format, ...)
{
	TRACE_FUNC();
}

GstElement *gst_element_factory_make(const gchar *factoryname, const gchar *name)
{
	TRACE_FUNC_ARG("%s\n",(char *)factoryname);

	GstElement *return_ptr = nullptr;
	if (g_mockGStreamer != nullptr)
	{
		return_ptr = g_mockGStreamer->gst_element_factory_make(factoryname,name);
	}
	return return_ptr;
}

gboolean gst_element_seek_simple(GstElement *element, GstFormat format, GstSeekFlags seek_flags,
								 gint64 seek_pos)
{
	TRACE_FUNC();
	return FALSE;
}

gboolean gst_bin_add(GstBin *bin, GstElement *element)
{
	TRACE_FUNC();
	gboolean rtn = FALSE;
	if (g_mockGStreamer != nullptr)
	{
		rtn = g_mockGStreamer->gst_bin_add(bin, element);
	}

	return rtn;
}

gboolean gst_bin_remove(GstBin *bin, GstElement *element)
{
	TRACE_FUNC();
	return FALSE;
}

void gst_bin_add_many(GstBin *bin, GstElement *element_1, ...)
{
	TRACE_FUNC();
}

gboolean gst_element_link(GstElement *src, GstElement *dest)
{
	TRACE_FUNC();
	return FALSE;
}

gboolean gst_element_link_many(GstElement *element_1, GstElement *element_2, ...)
{
	TRACE_FUNC();
	return FALSE;
}

gboolean gst_element_sync_state_with_parent(GstElement *element)
{
	TRACE_FUNC();
	return FALSE;
}

GstPad *gst_element_get_static_pad(GstElement *element, const gchar *name)
{
	TRACE_FUNC();
	return NULL;
}

GstEvent *gst_event_new_segment(const GstSegment *segment)
{
	TRACE_FUNC();
	return NULL;
}

GstEvent *gst_event_new_flush_start(void)
{
	TRACE_FUNC();
	return NULL;
}

GstEvent *gst_event_new_flush_stop(gboolean reset_time)
{
	TRACE_FUNC();
	return NULL;
}

GstEvent *gst_event_new_step (GstFormat format,
                    guint64 amount,
                    gdouble rate,
                    gboolean flush,
                    gboolean intermediate)
{
	TRACE_FUNC();
	GstEvent *rtn = nullptr;
	if (g_mockGStreamer != nullptr)
	{
		rtn = g_mockGStreamer->gst_event_new_step(format, amount, rate, flush, intermediate);
	}
	return rtn;
}

gboolean gst_pad_push_event(GstPad *pad, GstEvent *event)
{
	TRACE_FUNC();
	return FALSE;
}

void gst_segment_init(GstSegment *segment, GstFormat format)
{
	TRACE_FUNC();
}

const gchar *gst_flow_get_name(GstFlowReturn ret)
{
	TRACE_FUNC();
	return NULL;
}

gboolean gst_element_query_position(GstElement *element, GstFormat format, gint64 *cur)
{
	TRACE_FUNC();
	bool rtn = FALSE;
	if (g_mockGStreamer != nullptr)
	{
		rtn = g_mockGStreamer->gst_element_query_position(element, format, cur );
	}
	return rtn;
}

GstStateChangeReturn gst_element_get_state(GstElement *element, GstState *state, GstState *pending,
										   GstClockTime timeout)
{
	TRACE_FUNC();
	GstStateChangeReturn rtn = GST_STATE_CHANGE_FAILURE;
	if (g_mockGStreamer != nullptr)
	{
		rtn = g_mockGStreamer->gst_element_get_state(element, state, pending, timeout);
	}
	return rtn;
}

GstEvent *gst_event_new_eos(void)
{
	TRACE_FUNC();
	return NULL;
}

GstMessage *gst_bus_timed_pop_filtered(GstBus *bus, GstClockTime timeout, GstMessageType types)
{
	TRACE_FUNC();
	return NULL;
}

gboolean gst_bus_remove_watch(GstBus *bus)
{
	TRACE_FUNC();
	return FALSE;
}

gchar *gst_object_get_name(GstObject *object)
{
	if (object && object->name)
	{
		TRACE_FUNC_ARG("%s \n", object->name);
		// See SafeName() in gstplayer.cpp
		char *ptr = object->name ;
		char *g_ptr = (char *)malloc(strlen(ptr) + 1);
		strcpy(g_ptr, ptr);

		return (gchar *)g_ptr;
	}

	return NULL;
}

GstStateChangeReturn gst_element_set_state(GstElement *element, GstState state)
{
	TRACE_FUNC_ARG("%d \n",state);

	GstStateChangeReturn rtn = GST_STATE_CHANGE_FAILURE;
	if (g_mockGStreamer != nullptr)
	{
		rtn = g_mockGStreamer->gst_element_set_state(element, state);
	}

	return rtn;
}

gboolean gst_element_send_event(GstElement *element, GstEvent *event)
{
	TRACE_FUNC();
	gboolean rtn = FALSE;
	if (g_mockGStreamer != nullptr)
	{
		rtn = g_mockGStreamer->gst_element_send_event(element, event);
	}
	return rtn;
}

GstQuery *gst_query_new_duration(GstFormat format)
{
	TRACE_FUNC();
	return NULL;
}

gboolean gst_element_query(GstElement *element, GstQuery *query)
{
	TRACE_FUNC();
	return FALSE;
}

void gst_query_parse_duration(GstQuery *query, GstFormat *format, gint64 *duration)
{
	TRACE_FUNC();
}

GstQuery *gst_query_new_segment(GstFormat format)
{
	TRACE_FUNC();
	return NULL;
}

void gst_query_parse_segment(GstQuery *query, gdouble *rate, GstFormat *format, gint64 *start_value,
							 gint64 *stop_value)
{

	TRACE_FUNC();
}

void gst_query_parse_position(GstQuery *query, GstFormat *format, gint64 *cur)
{
	TRACE_FUNC();
}

gboolean gst_element_seek(GstElement *element, gdouble rate, GstFormat format, GstSeekFlags flags,
						  GstSeekType start_type, gint64 start, GstSeekType stop_type, gint64 stop)
{
	TRACE_FUNC();
	return FALSE;
}

guint64 gst_app_src_get_current_level_bytes(GstAppSrc *appsrc)
{
	TRACE_FUNC();
	return 0;
}

GType gst_registry_get_type(void)
{

	TRACE_FUNC();
	return 0;
}

GstRegistry *gst_registry_get(void)
{
	TRACE_FUNC();
	return NULL;
}

GstPluginFeature *gst_registry_lookup_feature(GstRegistry *registry, const char *name)
{

	TRACE_FUNC();
	return NULL;
}

GstStructure *gst_structure_new(const gchar *name, const gchar *firstfield, ...)
{
	TRACE_FUNC();
	return NULL;
}

GstEvent *gst_event_new_custom(GstEventType type, GstStructure *structure)
{
	TRACE_FUNC();
	return NULL;
}

gboolean gst_buffer_copy_into(GstBuffer *dest, GstBuffer *src, GstBufferCopyFlags flags,
							  gsize offset, gsize size)
{

	TRACE_FUNC();
	return FALSE;
}

void gst_plugin_feature_set_rank(GstPluginFeature *feature, guint rank)
{
}

gboolean gst_object_replace(GstObject **oldobj, GstObject *newobj)
{
	TRACE_FUNC();
	gboolean rtn = FALSE;
	if (g_mockGStreamer != nullptr)
	{
		rtn = g_mockGStreamer->gst_object_replace(oldobj, newobj);
	}

	return rtn;
}

void gst_message_parse_stream_status(GstMessage *message, GstStreamStatusType *type,
									 GstElement **owner)
{
	TRACE_FUNC();
}

const GValue *gst_message_get_stream_status_object(GstMessage *message)
{
	TRACE_FUNC();
	return NULL;
}

GType gst_task_get_type(void)
{
	TRACE_FUNC();
	return 0;
}

void gst_task_set_pool(GstTask *task, GstTaskPool *pool)
{
	TRACE_FUNC();
}

gpointer gst_object_ref_sink(gpointer object)
{
	TRACE_FUNC_ARG("%p\n",object);
	return object;
}

const GValue *gst_structure_get_value(const GstStructure *structure, const gchar *fieldname)
{
	TRACE_FUNC();
	return NULL;
}

guint64 g_value_get_uint64(const GValue *value)
{
	TRACE_FUNC();
	return 0;
}

void gst_structure_free(GstStructure *structure)
{
	TRACE_FUNC();
}

gboolean gst_init_check(int *argc, char **argv[], GError **error)
{
	TRACE_FUNC();
	return TRUE;
}

GstCaps *gst_caps_new_simple(const char *media_type, const char *fieldname, ...)
{
	GstCaps *return_ptr = NULL;
	TRACE_FUNC();
	if (g_mockGStreamer != nullptr)
	{
		va_list ap;
		va_start(ap, fieldname);
		GType var1 = va_arg(ap, GType);
		int var2 = va_arg(ap, int);
		void *ptr = va_arg(ap, void *);
		return_ptr = g_mockGStreamer->gst_caps_new_simple(media_type, fieldname, var1, var2, ptr);
		va_end(ap);
	}

	return return_ptr;
}

void gst_debug_set_threshold_from_string(const gchar *list, gboolean reset)
{
	TRACE_FUNC();
	if (g_mockGStreamer != nullptr)
	{
		g_mockGStreamer->gst_debug_set_threshold_from_string(list, reset);
	}
}

GstFlowReturn gst_app_src_end_of_stream (GstAppSrc * appsrc)
{
	return GST_FLOW_OK;
}

gulong gst_pad_add_probe (GstPad * pad,
                   GstPadProbeType mask,
                   GstPadProbeCallback callback,
                   gpointer user_data,
                   GDestroyNotify destroy_data)
{
	TRACE_FUNC();
	return 0;
}

GstCaps *gst_pad_get_current_caps (GstPad * pad)
{
	TRACE_FUNC();
	return NULL;
}

gchar *gst_caps_to_string (const GstCaps * caps)
{
	TRACE_FUNC();
	return NULL;
}

void gst_pad_remove_probe (GstPad * pad, gulong id)
{
	TRACE_FUNC();
}

gboolean gst_mini_object_replace (GstMiniObject **olddata, GstMiniObject *newdata)
{
	TRACE_FUNC();
	return FALSE;
}

void gst_event_copy_segment (GstEvent *event, GstSegment *segment)
{
	TRACE_FUNC();
}

void gst_event_parse_seek (GstEvent *event, gdouble *rate, GstFormat *format,
		GstSeekFlags *flags,
		GstSeekType *start_type, gint64 *start,
		GstSeekType *stop_type, gint64 *stop)
{
	TRACE_FUNC();
}

GstSegment * gst_segment_new(void)
{
	TRACE_FUNC();
	return NULL;
}

void gst_segment_free (GstSegment *segment)
{
	TRACE_FUNC();
}

GstEvent * gst_event_new_instant_rate_change (gdouble rate_multiplier,
		GstSegmentFlags new_flags)
{
	TRACE_FUNC();
	return NULL;
}

void gst_event_set_seqnum(GstEvent *event, guint32 seqnum)
{
	TRACE_FUNC();
}

GstPad*	gst_pad_get_peer(GstPad *pad)
{
	TRACE_FUNC();
	return NULL;
}

gboolean gst_pad_send_event(GstPad *pad, GstEvent *event)
{
	TRACE_FUNC();
	return false;
}

guint32 gst_event_get_seqnum(GstEvent *event)
{
	TRACE_FUNC();
	return 0;
}

gboolean gst_base_sink_is_async_enabled (GstBaseSink * sink)
{
        TRACE_FUNC();
        return false;
}

void gst_base_sink_set_async_enabled (GstBaseSink * sink, gboolean enabled)
{
	TRACE_FUNC();
}

GstEvent* gst_event_new_seek(gdouble rate, GstFormat format, GstSeekFlags flags,
                                                 GstSeekType start_type, gint64 start,
                                                 GstSeekType stop_type, gint64 stop)
{
        TRACE_FUNC();
        return NULL;
}

void gst_caps_set_simple (GstCaps * caps, const char *field, ...)
{
	TRACE_FUNC();
}

GType gst_base_sink_get_type (void)
{
        TRACE_FUNC();
        return 0;
}

gboolean gst_element_add_pad (GstElement * element, GstPad * pad)
{
	TRACE_FUNC();
	return FALSE;
}

GstEvent* gst_event_new_protection(const gchar * system_id, GstBuffer * data, const gchar * origin)
{
        TRACE_FUNC();
        return NULL;
}

void gst_message_parse_reset_time(GstMessage *message, GstClockTime *running_time)
{
	TRACE_FUNC();
}

gchar * gst_structure_to_string(const GstStructure *structure)
{
	TRACE_FUNC();
	return NULL;
}

GstElement * gst_bin_new (const gchar * name)
{
	TRACE_FUNC();
	return NULL;
}
void gst_deinit (void)
{
       TRACE_FUNC();
}

void gst_init(int *argc, char **argv[])
{
       TRACE_FUNC();
}

GstPlugin *gst_plugin_load_by_name (const gchar * name)
{
	TRACE_FUNC();
	return NULL;
}

void gst_registry_remove_feature (GstRegistry * registry,
                             GstPluginFeature * feature)
{
	TRACE_FUNC();
}

gboolean
gst_registry_add_feature (GstRegistry * registry,
                          GstPluginFeature * feature)

{
	TRACE_FUNC();
	return false;
}

GstPad * gst_ghost_pad_new (const gchar * name, GstPad * target)
{
	TRACE_FUNC();
	return NULL;
}

