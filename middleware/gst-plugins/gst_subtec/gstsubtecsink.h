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

#ifndef _GST_SUBTECSINK_H_
#define _GST_SUBTECSINK_H_

#include <gst/base/gstbasesink.h>
#include <string>
#include <cstdint>
#include <array>
#include "SubtecChannel.hpp"

G_BEGIN_DECLS

#define GST_TYPE_SUBTECSINK   (gst_subtecsink_get_type())
#define GST_SUBTECSINK(obj)   (G_TYPE_CHECK_INSTANCE_CAST((obj),GST_TYPE_SUBTECSINK,GstSubtecSink))
#define GST_SUBTECSINK_CLASS(klass)   (G_TYPE_CHECK_CLASS_CAST((klass),GST_TYPE_SUBTECSINK,GstSubtecSinkClass))
#define GST_IS_SUBTECSINK(obj)   (G_TYPE_CHECK_INSTANCE_TYPE((obj),GST_TYPE_SUBTECSINK))
#define GST_IS_SUBTECSINK_CLASS(obj)   (G_TYPE_CHECK_CLASS_TYPE((klass),GST_TYPE_SUBTECSINK))

typedef struct _GstSubtecSink GstSubtecSink;
typedef struct _GstSubtecSinkClass GstSubtecSinkClass;

struct _GstSubtecSink
{
  GstBaseSink base_subtecsink;

	std::unique_ptr<SubtecChannel> m_channel;
  gboolean m_mute;
  gboolean m_no_eos;
  gboolean m_send_timestamp{true};
  guint64  m_segmentstart{0};
  guint64  m_pts_offset{0};
  std::string   m_subtec_socket{};
  attributesType m_attribute_values{0};
  guint m_attribute_mask{0};
};

struct _GstSubtecSinkClass
{
  GstBaseSinkClass base_subtecsink_class;
};

GType gst_subtecsink_get_type (void);

G_END_DECLS

#endif
