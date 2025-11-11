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

#include <glib.h>
#include <gst/gst.h>
#include <gst/base/gstbytereader.h>
#include <gst/video/gstvideotimecode.h>
#include <gst/video/gstvideometa.h>

GType _gst_caps_type = 0;
GType _gst_buffer_type = 0;


GstVideoTimeCodeMeta * gst_buffer_add_video_time_code_meta_full(
																GstBuffer * buffer,
																guint fps_n,
																guint fps_d,
																GDateTime * latest_daily_jam,
																GstVideoTimeCodeFlags flags,
																guint hours,
																guint minutes,
																guint seconds,
																guint frames,
																guint field_count)
{
	return NULL;
}

gboolean gst_buffer_map(GstBuffer *buffer, GstMapInfo *info, GstMapFlags flags)
{
	return TRUE;
}

void gst_buffer_unmap(GstBuffer *buffer, GstMapInfo *info)
{
}

GstByteReader *gst_byte_reader_new(const guint8 *data, guint size)
{
	static GstByteReader byte_reader;
	return &byte_reader;
}

gboolean gst_byte_reader_set_pos(GstByteReader *reader, guint pos)
{
	return TRUE;
}

void gst_byte_reader_free(GstByteReader *reader)
{
}

gboolean gst_caps_is_empty (const GstCaps * caps)
{
	return FALSE;
}

GstMeta * gst_buffer_get_meta (GstBuffer * buffer, GType api)
{
	return NULL;
}

GstStructure * gst_structure_new (const gchar * name, const gchar * firstfield, ...)
{
	return NULL;
}

void gst_structure_set (GstStructure * structure, const gchar * field, ...)
{
}

GstProtectionMeta * gst_buffer_add_protection_meta (GstBuffer * buffer, GstStructure * info)
{
	return NULL;
}

GType gst_protection_meta_api_get_type (void)
{
	return 0;
}

void g_object_get(gpointer object, const gchar *first_property_name, ...)
{
}
