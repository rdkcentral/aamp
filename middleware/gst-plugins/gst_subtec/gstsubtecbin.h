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

#ifndef _GST_SUBTECBIN_H_
#define _GST_SUBTECBIN_H_

#include <gst/gst.h>
#include <string>

// Declared static here because this string exists in player and gstplugin .so
// library files  This string needs to match the start
// of the gsteamer plugin name as created by the macros.
static const char* GstPluginNameSubtecBin = "subtecbin";

G_BEGIN_DECLS

#define GST_TYPE_SUBTECBIN   (gst_subtecbin_get_type())
#define GST_SUBTECBIN(obj)   (G_TYPE_CHECK_INSTANCE_CAST((obj),GST_TYPE_SUBTECBIN,GstSubtecBin))
#define GST_SUBTECBIN_CLASS(klass)   (G_TYPE_CHECK_CLASS_CAST((klass),GST_TYPE_SUBTECBIN,GstSubtecBinClass))
#define GST_IS_SUBTECBIN(obj)   (G_TYPE_CHECK_INSTANCE_TYPE((obj),GST_TYPE_SUBTECBIN))
#define GST_IS_SUBTECBIN_CLASS(obj)   (G_TYPE_CHECK_CLASS_TYPE((klass),GST_TYPE_SUBTECBIN))

typedef struct _GstSubtecBin GstSubtecBin;
typedef struct _GstSubtecBinClass GstSubtecBinClass;

struct _GstSubtecBin
{
  GstBin      parent;
  GstPad      *sinkpad;
  GstElement  *typefind;
  GstElement  *demux;
  GstElement  *formatter;
  GstElement  *sink;
  bool        no_eos = false;
  bool        mute = false;
  bool        async = true;
  bool        sync = true;
  std::string subtec_socket{};
  guint64     pts_offset{0};
};

struct _GstSubtecBinClass
{
  GstBinClass parent_class;
};

GType gst_subtecbin_get_type (void);

G_END_DECLS

#endif
