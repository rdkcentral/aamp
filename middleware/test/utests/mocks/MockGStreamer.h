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

#ifndef AAMP_MOCK_GSTREAMER_H
#define AAMP_MOCK_GSTREAMER_H

#include <gmock/gmock.h>
#include <gst/gstcaps.h>
#include <gst/app/gstappsink.h>

class MockGStreamer
{
public:
	MOCK_METHOD(GstCaps *, gst_caps_new_simple,
				(const char *media_type, const char *fieldname, GType var, const int val,
				 void *ptr));
	MOCK_METHOD(void, gst_debug_set_threshold_from_string, (const gchar *list, gboolean reset));
	MOCK_METHOD(GstElement *, gst_pipeline_new, (const gchar *name));
	MOCK_METHOD(GstBus *, gst_pipeline_get_bus, (GstPipeline *pipeline));
	MOCK_METHOD(guint , gst_bus_add_watch, (GstBus *bus, GstBusFunc func, gpointer user_data));
	MOCK_METHOD(void, gst_bus_set_sync_handler, (GstBus *bus, GstBusSyncHandler func, gpointer user_data,  GDestroyNotify notify));
	MOCK_METHOD(GstQuery *, gst_query_new_position, (GstFormat format));
	MOCK_METHOD(GstStateChangeReturn, gst_element_get_state, (GstElement *element, GstState *state, GstState *pending,
										   GstClockTime timeout));
	MOCK_METHOD(GstElement *, gst_element_factory_make, (const gchar *factoryname,const gchar *name));
	MOCK_METHOD(GstStateChangeReturn, gst_element_set_state,(GstElement *element, GstState state));
	MOCK_METHOD(gboolean, gst_bin_add, (GstBin *bin, GstElement *element));
	MOCK_METHOD(void, gst_object_unref,(gpointer object));
	MOCK_METHOD(void, gst_mini_object_unref,(GstMiniObject *mini_object));
	
	MOCK_METHOD(GstSample *,gst_app_sink_pull_sample,(GstAppSink *appsink));
	MOCK_METHOD(GstStructure *,gst_app_sink_set_caps,(GstAppSink *appsink, const GstCaps *caps));
	MOCK_METHOD(GstStructure *,gst_caps_get_structure,(const GstCaps *caps, guint index));
	MOCK_METHOD(void, gst_message_parse_state_changed, (GstMessage * message, GstState * oldstate, GstState * newstate, GstState * pending));

	MOCK_METHOD(gboolean, gst_object_replace, (GstObject ** oldobj, GstObject * newobj));
	MOCK_METHOD(gboolean, gst_element_send_event, (GstElement *element, GstEvent *event));
	MOCK_METHOD(GstEvent *, gst_event_new_step, (GstFormat format, guint64 amount, gdouble rate, gboolean flush, gboolean intermediate));
	MOCK_METHOD(gboolean, gst_element_query_position, (GstElement *element, GstFormat format, gint64 *cur));

	/*
gst_app_sink_get_type
gst_app_sink_pull_sample
gst_app_sink_set_caps
gst_caps_get_structure
gst_sample_get_buffer
gst_sample_get_caps
gst_structure_get_int
	 */
};

extern MockGStreamer *g_mockGStreamer;

#endif /* AAMP_MOCK_GSTREAMER_H */
