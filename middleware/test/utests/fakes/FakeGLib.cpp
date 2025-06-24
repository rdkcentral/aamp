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
#include <iostream>

#include "MockGLib.h"

using namespace std;

#if 0
#define TRACE_FUNC() printf("%s\n" ,__func__)
#else
#define TRACE_FUNC() ((void)0)
#endif

MockGLib *g_mockGLib = nullptr;

void g_object_set(gpointer object, const gchar *first_property_name, ...)
{
	TRACE_FUNC();

	if (g_mockGLib != nullptr)
	{
		va_list args_list;
		va_start(args_list, first_property_name);
		const gchar *property_name = first_property_name;

		while (property_name)
		{
			if ((strcmp(property_name, "mute") == 0) ||
				(strcmp(property_name, "show-video-window") == 0) ||
				(strcmp(property_name, "zoom-mode") == 0)
			   )
			{				
				g_mockGLib->g_object_set(object, property_name, va_arg(args_list, int));
			}
			else if((strcmp(property_name, "rectangle") == 0))
			{
				g_mockGLib->g_object_set(object, property_name, va_arg(args_list, char *));
			}
			else if((strcmp(property_name, "volume") == 0))
			{
				g_mockGLib->g_object_set(object, property_name, va_arg(args_list, double));
			}
			else
			{
				va_arg(args_list, int);
			}
			property_name = va_arg(args_list, gchar *);
		}
		va_end(args_list);
	}
}

void g_object_get(gpointer object, const gchar *first_property_name, ...)
{
	TRACE_FUNC();
}

gulong g_signal_connect_data(gpointer instance, const gchar *detailed_signal, GCallback c_handler,
							 gpointer data, GClosureNotify destroy_data,
							 GConnectFlags connect_flags)
{
	TRACE_FUNC();
	return 0;
}

gboolean g_type_check_instance_is_a(GTypeInstance *instance, GType iface_type)
{
	TRACE_FUNC();
	return FALSE;
}

gboolean g_signal_handler_is_connected(gpointer instance, gulong handler_id)
{
	TRACE_FUNC();
	return FALSE;
}

void g_signal_handler_disconnect(gpointer instance, gulong handler_id)
{
	TRACE_FUNC();
}

GTypeInstance *g_type_check_instance_cast(GTypeInstance *instance, GType iface_type)
{
	TRACE_FUNC();
	return instance;
}

GValue *g_value_init(GValue *value, GType g_type)
{
	TRACE_FUNC();
	return NULL;
}

void g_value_set_pointer(GValue *value, gpointer v_pointer)
{
	TRACE_FUNC();
}

GParamSpec* g_object_class_find_property(GObjectClass* oclass, const gchar* property_name)
{
	GParamSpec* retval = NULL;

	if (g_mockGLib != nullptr)
	{
		retval = g_mockGLib->g_object_class_find_property(oclass, property_name);
	}

	return retval;
}


void g_object_set_property(GObject *object, const gchar *property_name, const GValue *value)
{
	TRACE_FUNC();
}

void g_signal_emit_by_name(gpointer instance, const gchar *detailed_signal, ...)
{
}

gboolean g_type_check_instance(GTypeInstance *instance)
{
	TRACE_FUNC();
	return FALSE;
}

gpointer g_value_get_object(const GValue *value)
{
	TRACE_FUNC();
	return NULL;
}

gpointer g_object_new(GType object_type, const gchar *first_property_name, ...)
{
	TRACE_FUNC();
	return NULL;
}

void g_object_unref(gpointer object)
{
	TRACE_FUNC();
}

guint g_timeout_add(guint interval, GSourceFunc function, gpointer data)
{
	guint retval = 0;

	if (g_mockGLib != nullptr)
	{
		retval = g_mockGLib->g_timeout_add(interval, function, data);
	}

	return retval;
}

gboolean g_source_remove(guint tag)
{
	gboolean retval = FALSE;

	if (g_mockGLib != nullptr)
	{
		retval = g_mockGLib->g_source_remove(tag);
	}
	return retval;
}

GSource *g_main_current_source(void)
{
	return nullptr;
}

guint g_source_get_id(GSource *source)
{
	return 0;
}

void g_printerr(const gchar *format, ...)
{

}

void g_clear_error(GError **err)
{

}

void g_free(gpointer mem)
{
	if (g_mockGLib != nullptr)
	{
		g_mockGLib->g_free(mem);
	}
}

int g_strcmp0(const char *str1, const char *str2)
{
	return 0;
}

guint g_timeout_add_full(gint priority, guint interval, GSourceFunc function, gpointer data, GDestroyNotify notify)
{
	return 0;
}

void g_usleep(gulong microseconds)
{

}

gpointer g_malloc(gsize	 n_bytes)
{
	gpointer ptr = NULL;

	if (g_mockGLib != nullptr)
	{
		ptr = g_mockGLib->g_malloc(n_bytes);
	}
	return ptr;
}

gpointer g_realloc (gpointer mem, gsize n_bytes)
{
	gpointer ptr = NULL;

	if (g_mockGLib != nullptr)
	{
		ptr = g_mockGLib->g_realloc(mem, n_bytes);
	}
	return ptr;

}
