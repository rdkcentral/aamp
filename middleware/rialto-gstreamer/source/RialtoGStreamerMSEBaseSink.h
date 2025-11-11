/*
 * Copyright (C) 2022   
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

#pragma once

#include "GStreamerUtils.h"
#include "IPlaybackDelegate.h"
#include <IMediaPipeline.h>
#include <MediaCommon.h>
#include <gst/gst.h>
#include <memory>
#include <vector>

G_BEGIN_DECLS

#define RIALTO_TYPE_MSE_BASE_SINK (rialto_mse_base_sink_get_type())
#define RIALTO_MSE_BASE_SINK(obj) (G_TYPE_CHECK_INSTANCE_CAST((obj), RIALTO_TYPE_MSE_BASE_SINK, RialtoMSEBaseSink))
#define RIALTO_MSE_BASE_SINK_CLASS(klass)                                                                              \
    (G_TYPE_CHECK_CLASS_CAST((klass), RIALTO_TYPE_MSE_BASE_SINK, RialtoMSEBaseSinkClass))
#define RIALTO_IS_MSE_BASE_SINK(obj) (G_TYPE_CHECK_INSTANCE_TYPE((obj), RIALTO_TYPE_MSE_BASE_SINK))
#define RIALTO_IS_MSE_BASE_SINK_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE((klass), RIALTO_TYPE_MSE_BASE_SINK))

typedef struct _RialtoMSEBaseSink RialtoMSEBaseSink;
typedef struct _RialtoMSEBaseSinkClass RialtoMSEBaseSinkClass;
typedef struct _RialtoMSEBaseSinkPrivate RialtoMSEBaseSinkPrivate;

struct _RialtoMSEBaseSink
{
    GstElement parent;
    RialtoMSEBaseSinkPrivate *priv;
};

struct _RialtoMSEBaseSinkClass
{
    GstElementClass parent_class;
};

namespace firebolt::rialto::client
{
class MediaPlayerBackend;
}

GType rialto_mse_base_sink_get_type(void);

void rialto_mse_base_sink_initialise_delegate(RialtoMSEBaseSink *sink,
                                              const std::shared_ptr<IPlaybackDelegate> &delegate);
void rialto_mse_base_sink_handle_get_property(RialtoMSEBaseSink *sink, const IPlaybackDelegate::Property &property,
                                              GValue *value);
void rialto_mse_base_sink_handle_set_property(RialtoMSEBaseSink *sink, const IPlaybackDelegate::Property &property,
                                              const GValue *value);

GstFlowReturn rialto_mse_base_sink_chain(GstPad *pad, GstObject *parent, GstBuffer *buf);
gboolean rialto_mse_base_sink_event(GstPad *pad, GstObject *parent, GstEvent *event);
bool rialto_mse_base_sink_initialise_sinkpad(RialtoMSEBaseSink *sink);

void rialto_mse_base_handle_rialto_server_sent_buffer_underflow(RialtoMSEBaseSink *sink);

G_END_DECLS
