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
#include <gst/app/gstappsink.h>

#include "MockGStreamer.h"

MockGStreamer *g_mockGStreamer = nullptr;

GstDebugCategory *GST_CAT_DEFAULT;
GstDebugLevel _gst_debug_min;

void gst_debug_bin_to_dot_file(GstBin *bin, GstDebugGraphDetails details, const gchar *file_name)
{
}

GType gst_object_get_type(void)
{
	return 0;
}

GType gst_bin_get_type(void)
{
	return 0;
}

GstMiniObject *gst_mini_object_ref(GstMiniObject *mini_object)
{
	return NULL;
}

void gst_mini_object_unref(GstMiniObject *mini_object)
{
}

GType gst_app_src_get_type(void)
{
	return 0;
}

void gst_app_src_set_caps(GstAppSrc *appsrc, const GstCaps *caps)
{
}

void gst_app_src_set_stream_type(GstAppSrc *appsrc, GstAppStreamType type)
{
}

GstFlowReturn gst_app_src_push_buffer(GstAppSrc *appsrc, GstBuffer *buffer)
{
	return GST_FLOW_OK;
}

GstBuffer *gst_buffer_new(void)
{
	return NULL;
}

GstBuffer *gst_buffer_new_allocate(GstAllocator *allocator, gsize size, GstAllocationParams *params)
{
	return NULL;
}

GstBuffer *gst_buffer_new_wrapped(gpointer data, gsize size)
{
	return NULL;
}

gboolean gst_buffer_map(GstBuffer *buffer, GstMapInfo *info, GstMapFlags flags)
{
	return FALSE;
}

void gst_buffer_unmap(GstBuffer *buffer, GstMapInfo *info)
{
}

const gchar *gst_element_state_get_name(GstState state)
{
	return NULL;
}

GType gst_element_get_type(void)
{
	return 0;
}

void gst_message_parse_warning(GstMessage *message, GError **gerror, gchar **debug)
{
}

void gst_message_parse_error(GstMessage *message, GError **gerror, gchar **debug)
{
}

void gst_message_parse_state_changed(GstMessage *message, GstState *oldstate, GstState *newstate,
									 GstState *pending)
{
}

void gst_message_parse_qos(GstMessage *message, gboolean *live, guint64 *running_time,
						   guint64 *stream_time, guint64 *timestamp, guint64 *duration)
{
}

const GstStructure *gst_message_get_structure(GstMessage *message)
{
	return NULL;
}

gboolean gst_structure_has_name(const GstStructure *structure, const gchar *name)
{
	return FALSE;
}

const gchar *gst_message_type_get_name(GstMessageType type)
{
	return NULL;
}

gboolean gst_message_parse_context_type(GstMessage *message, const gchar **context_type)
{
	return FALSE;
}

GstContext *gst_context_new(const gchar *context_type, gboolean persistent)
{
	return NULL;
}

GstStructure *gst_context_writable_structure(GstContext *context)
{
	return NULL;
}

void gst_structure_set(GstStructure *structure, const gchar *fieldname, ...)
{
}

void gst_element_set_context(GstElement *element, GstContext *context)
{
}

GstElement *gst_pipeline_new(const gchar *name)
{
	return NULL;
}

GType gst_pipeline_get_type(void)
{
	return 0;
}

GstBus *gst_pipeline_get_bus(GstPipeline *pipeline)
{
	return NULL;
}

guint gst_bus_add_watch(GstBus *bus, GstBusFunc func, gpointer user_data)
{
	return 0;
}

void gst_bus_set_sync_handler(GstBus *bus, GstBusSyncHandler func, gpointer user_data,
							  GDestroyNotify notify)
{
}

GstQuery *gst_query_new_position(GstFormat format)
{
	return NULL;
}

gpointer gst_object_ref(gpointer object)
{
	return NULL;
}

void gst_object_unref(gpointer object)
{
}

void gst_debug_log(GstDebugCategory *category, GstDebugLevel level, const gchar *file,
				   const gchar *function, gint line, GObject *object, const gchar *format, ...)
{
}

GstElement *gst_element_factory_make(const gchar *factoryname, const gchar *name)
{
	return NULL;
}

gboolean gst_element_seek_simple(GstElement *element, GstFormat format, GstSeekFlags seek_flags,
								 gint64 seek_pos)
{
	return FALSE;
}

gboolean gst_bin_add(GstBin *bin, GstElement *element)
{
	return FALSE;
}

gboolean gst_bin_remove(GstBin *bin, GstElement *element)
{
	return FALSE;
}

void gst_bin_add_many(GstBin *bin, GstElement *element_1, ...)
{
}

gboolean gst_element_link(GstElement *src, GstElement *dest)
{
	return FALSE;
}

gboolean gst_element_link_many(GstElement *element_1, GstElement *element_2, ...)
{
	return FALSE;
}

gboolean gst_element_sync_state_with_parent(GstElement *element)
{
	return FALSE;
}

GstPad *gst_element_get_static_pad(GstElement *element, const gchar *name)
{
	return NULL;
}

GstEvent *gst_event_new_segment(const GstSegment *segment)
{
	return NULL;
}

GstEvent *gst_event_new_flush_start(void)
{
	return NULL;
}

GstEvent *gst_event_new_flush_stop(gboolean reset_time)
{
	return NULL;
}

gboolean gst_pad_push_event(GstPad *pad, GstEvent *event)
{
	return FALSE;
}

void gst_segment_init(GstSegment *segment, GstFormat format)
{
}

const gchar *gst_flow_get_name(GstFlowReturn ret)
{
	return NULL;
}

GstStateChangeReturn gst_element_get_state(GstElement *element, GstState *state, GstState *pending,
										   GstClockTime timeout)
{
	return GST_STATE_CHANGE_FAILURE;
}

GstEvent *gst_event_new_eos(void)
{
	return NULL;
}

GstMessage *gst_bus_timed_pop_filtered(GstBus *bus, GstClockTime timeout, GstMessageType types)
{
	return NULL;
}

gboolean gst_bus_remove_watch(GstBus *bus)
{
	return FALSE;
}

gchar *gst_object_get_name(GstObject *object)
{
	return NULL;
}

GstStateChangeReturn gst_element_set_state(GstElement *element, GstState state)
{
	return GST_STATE_CHANGE_FAILURE;
}

gboolean gst_element_send_event(GstElement *element, GstEvent *event)
{
	return FALSE;
}

GstQuery *gst_query_new_duration(GstFormat format)
{
	return NULL;
}

gboolean gst_element_query(GstElement *element, GstQuery *query)
{
	return FALSE;
}

void gst_query_parse_duration(GstQuery *query, GstFormat *format, gint64 *duration)
{
}

GstQuery *gst_query_new_segment(GstFormat format)
{
	return NULL;
}

void gst_query_parse_segment(GstQuery *query, gdouble *rate, GstFormat *format, gint64 *start_value,
							 gint64 *stop_value)
{
}

void gst_query_parse_position(GstQuery *query, GstFormat *format, gint64 *cur)
{
}

gboolean gst_element_seek(GstElement *element, gdouble rate, GstFormat format, GstSeekFlags flags,
						  GstSeekType start_type, gint64 start, GstSeekType stop_type, gint64 stop)
{
	return FALSE;
}

guint64 gst_app_src_get_current_level_bytes(GstAppSrc *appsrc)
{
	return 0;
}

GType gst_registry_get_type(void)
{
	return 0;
}

GstRegistry *gst_registry_get(void)
{
	return NULL;
}

GstPluginFeature *gst_registry_lookup_feature(GstRegistry *registry, const char *name)
{
	return NULL;
}

GstStructure *gst_structure_new(const gchar *name, const gchar *firstfield, ...)
{
	return NULL;
}

GstEvent *gst_event_new_custom(GstEventType type, GstStructure *structure)
{
	return NULL;
}

gboolean gst_buffer_copy_into(GstBuffer *dest, GstBuffer *src, GstBufferCopyFlags flags,
							  gsize offset, gsize size)
{
	return FALSE;
}

void gst_plugin_feature_set_rank(GstPluginFeature *feature, guint rank)
{
}

gboolean gst_object_replace(GstObject **oldobj, GstObject *newobj)
{
	return FALSE;
}

void gst_message_parse_stream_status(GstMessage *message, GstStreamStatusType *type,
									 GstElement **owner)
{
}

const GValue *gst_message_get_stream_status_object(GstMessage *message)
{
	return NULL;
}

GType gst_task_get_type(void)
{
	return 0;
}

void gst_task_set_pool(GstTask *task, GstTaskPool *pool)
{
}

gpointer gst_object_ref_sink(gpointer object)
{
	return NULL;
}

const GValue *gst_structure_get_value(const GstStructure *structure, const gchar *fieldname)
{
	return NULL;
}

guint64 g_value_get_uint64(const GValue *value)
{
	return 0;
}

void gst_structure_free(GstStructure *structure)
{
}

gboolean gst_init_check(int *argc, char **argv[], GError **error)
{
	return FALSE;
}

GstCaps *gst_caps_new_simple(const char *media_type, const char *fieldname, ...)
{
	GstCaps *return_ptr = NULL;

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
	if (g_mockGStreamer != nullptr)
	{
		g_mockGStreamer->gst_debug_set_threshold_from_string(list, reset);
	}
}
