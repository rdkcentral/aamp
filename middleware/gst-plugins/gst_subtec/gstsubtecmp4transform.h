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

#ifndef _GST_SUBTECMP4TRANSFORM_H_
#define _GST_SUBTECMP4TRANSFORM_H_

#include <gst/base/gstbasetransform.h>

// Declared static here because this string exists in player and gstplugin .so 
// library files  This string needs to match the start
// of the gsteamer plugin name as created by the macros.
static const char* GstPluginNameSubtecMp4Transform = "subtecmp4transform";

G_BEGIN_DECLS

#define GST_TYPE_SUBTECMP4TRANSFORM   (gst_subtecmp4transform_get_type())
#define GST_SUBTECMP4TRANSFORM(obj)   (G_TYPE_CHECK_INSTANCE_CAST((obj),GST_TYPE_SUBTECMP4TRANSFORM,GstSubtecMp4Transform))
#define GST_SUBTECMP4TRANSFORM_CLASS(klass)   (G_TYPE_CHECK_CLASS_CAST((klass),GST_TYPE_SUBTECMP4TRANSFORM,GstSubtecMp4TransformClass))
#define GST_IS_SUBTECMP4TRANSFORM(obj)   (G_TYPE_CHECK_INSTANCE_TYPE((obj),GST_TYPE_SUBTECMP4TRANSFORM))
#define GST_IS_SUBTECMP4TRANSFORM_CLASS(obj)   (G_TYPE_CHECK_CLASS_TYPE((klass),GST_TYPE_SUBTECMP4TRANSFORM))

typedef struct _GstSubtecMp4Transform GstSubtecMp4Transform;
typedef struct _GstSubtecMp4TransformClass GstSubtecMp4TransformClass;

struct _GstSubtecMp4Transform
{
  GstBaseTransform base_subtecmp4transform;

};

struct _GstSubtecMp4TransformClass
{
  GstBaseTransformClass base_subtecmp4transform_class;
};

GType gst_subtecmp4transform_get_type (void);

G_END_DECLS

#endif
