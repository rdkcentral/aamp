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

#include <glib-object.h>
#include <glib.h>

void g_object_set(gpointer object, const gchar *first_property_name, ...)
{
}

void g_object_get(gpointer object, const gchar *first_property_name, ...)
{
}

gulong g_signal_connect_data(gpointer instance, const gchar *detailed_signal, GCallback c_handler,
							 gpointer data, GClosureNotify destroy_data,
							 GConnectFlags connect_flags)
{
	return 0;
}

gboolean g_type_check_instance_is_a(GTypeInstance *instance, GType iface_type)
{
	return FALSE;
}

gboolean g_signal_handler_is_connected(gpointer instance, gulong handler_id)
{
	return FALSE;
}

void g_signal_handler_disconnect(gpointer instance, gulong handler_id)
{
}

GTypeInstance *g_type_check_instance_cast(GTypeInstance *instance, GType iface_type)
{
	return NULL;
}

GValue *g_value_init(GValue *value, GType g_type)
{
	return NULL;
}

void g_value_set_pointer(GValue *value, gpointer v_pointer)
{
}

void g_object_set_property(GObject *object, const gchar *property_name, const GValue *value)
{
}

void g_signal_emit_by_name(gpointer instance, const gchar *detailed_signal, ...)
{
}

gboolean g_type_check_instance(GTypeInstance *instance)
{
	return FALSE;
}

gpointer g_value_get_object(const GValue *value)
{
	return NULL;
}

gpointer g_object_new(GType object_type, const gchar *first_property_name, ...)
{
	return NULL;
}

void g_object_unref(gpointer object)
{
}
