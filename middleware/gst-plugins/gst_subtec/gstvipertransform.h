/*
 * Copyright (C) 2022 RDK Management
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation;
 * version 2.1 of the License.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */

#ifndef _GST_VIPERTRANSFORM_H_
#define _GST_VIPERTRANSFORM_H_

#include <gst/base/gstbasetransform.h>

#include <cinttypes>

// Declared static here because this string exists in player and gstplugin .so
// library files  This string needs to match the start
// of the gsteamer plugin name as created by the macros.
static const char* GstPluginNameViperTransform = "vipertransform";

G_BEGIN_DECLS

#define GST_TYPE_VIPERTRANSFORM   (gst_vipertransform_get_type())
#define GST_VIPERTRANSFORM(obj)   (G_TYPE_CHECK_INSTANCE_CAST((obj),GST_TYPE_VIPERTRANSFORM,GstViperTransform))
#define GST_VIPERTRANSFORM_CLASS(klass)   (G_TYPE_CHECK_CLASS_CAST((klass),GST_TYPE_VIPERTRANSFORM,GstViperTransformClass))
#define GST_IS_VIPERTRANSFORM(obj)   (G_TYPE_CHECK_INSTANCE_TYPE((obj),GST_TYPE_VIPERTRANSFORM))
#define GST_IS_VIPERTRANSFORM_CLASS(obj)   (G_TYPE_CHECK_CLASS_TYPE((klass),GST_TYPE_VIPERTRANSFORM))

typedef struct _GstViperTransform GstViperTransform;
typedef struct _GstViperTransformClass GstViperTransformClass;

enum class ViperContentType
{
  UNKNOWN,
  PASSTHROUGH,
  LINEAR_OFFSET,
  LINEAR_OFFSET_PRELIM,
  HARMONIC_UHD
};

struct _GstViperTransform
{
  GstBaseTransform  base_vipertransform;
  ViperContentType       m_content_type        {ViperContentType::UNKNOWN};
  gint64            m_linear_begin_offset {0};
  gboolean          m_search_first_begin  {true};
};

struct _GstViperTransformClass
{
  GstBaseTransformClass base_vipertransform_class;
};

GType gst_vipertransform_get_type (void);

G_END_DECLS

#endif
