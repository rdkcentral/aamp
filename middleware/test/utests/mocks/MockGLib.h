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

#ifndef AAMP_MOCK_GLIB_H
#define AAMP_MOCK_GLIB_H

#include <gmock/gmock.h>
#include <glib-object.h>
#include <glib.h>
#include <gst/gststructure.h>


class MockGLib
{
public:
	MOCK_METHOD(GParamSpec*, g_object_class_find_property, (GObjectClass* oclass, const gchar* property_name));
	MOCK_METHOD(guint, g_timeout_add, (guint interval, GSourceFunc function, gpointer data));
	MOCK_METHOD(gboolean, g_source_remove, (guint tag));
	MOCK_METHOD(gpointer, g_malloc, (gsize n_bytes));
	MOCK_METHOD(void, g_free, (gpointer mem));
	MOCK_METHOD(gpointer, g_realloc, (gpointer mem, gsize n_bytes));

	MOCK_METHOD(void, g_object_set, (gpointer object, const gchar *property_name, int value));
	MOCK_METHOD(void, g_object_set, (gpointer object, const gchar *property_name, char * value));
	MOCK_METHOD(void, g_object_set, (gpointer object, const gchar *property_name, double value));
	MOCK_METHOD(void, g_object_set, (gpointer object, const gchar *property_name, GstStructure *value));
	MOCK_METHOD(gulong, g_signal_connect_data, (gpointer instance, const gchar *detailed_signal, GCallback c_handler,
							 					gpointer data, GClosureNotify destroy_data, GConnectFlags connect_flags));
	MOCK_METHOD(gboolean, g_type_check_instance_is_a, (gpointer instance, GType iface_type));
	MOCK_METHOD(gboolean, g_signal_handler_is_connected, (gpointer instance, gulong handler_id));
	MOCK_METHOD(gboolean, g_signal_handler_disconnect, (gpointer instance, gulong handler_id));
	MOCK_METHOD(void, g_object_get, (gpointer object, const gchar *first_property_name, GstStructure *structure));
	MOCK_METHOD(void, g_object_get, (gpointer object, const gchar *first_property_name, uint *value));
	MOCK_METHOD(void, g_object_get, (gpointer object, const gchar *first_property_name, gpointer *value));
	MOCK_METHOD(void, g_object_get, (gpointer object, const gchar *first_property_name, gint64 *value));
 
};

extern MockGLib *g_mockGLib;

#endif /* AAMP_MOCK_GLIB_H */
