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

#include "GStreamerMSEMediaPlayerClient.h"
#include "RialtoGStreamerMSEBaseSink.h"
#include <gst/base/gstbasesink.h>
#include <gst/gst.h>

G_BEGIN_DECLS

#define RIALTO_TYPE_MSE_AUDIO_SINK (rialto_mse_audio_sink_get_type())
#define RIALTO_MSE_AUDIO_SINK(obj) (G_TYPE_CHECK_INSTANCE_CAST((obj), RIALTO_TYPE_MSE_AUDIO_SINK, RialtoMSEAudioSink))
#define RIALTO_MSE_AUDIO_SINK_CLASS(klass)                                                                             \
    (G_TYPE_CHECK_CLASS_CAST((klass), RIALTO_TYPE_MSE_AUDIO_SINK, RialtoMSEAudioSinkClass))
#define RIALTO_IS_MSE_AUDIO_SINK(obj) (G_TYPE_CHECK_INSTANCE_TYPE((obj), RIALTO_TYPE_MSE_AUDIO_SINK))
#define RIALTO_IS_MSE_AUDIO_SINK_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE((klass), RIALTO_TYPE_MSE_AUDIO_SINK))

typedef struct _RialtoMSEAudioSink RialtoMSEAudioSink;
typedef struct _RialtoMSEAudioSinkClass RialtoMSEAudioSinkClass;

struct _RialtoMSEAudioSink
{
    RialtoMSEBaseSink parent;
};

struct _RialtoMSEAudioSinkClass
{
    RialtoMSEBaseSinkClass parent_class;
};

GType rialto_mse_audio_sink_get_type(void);

void rialto_mse_audio_sink_set_client_backend(GstElement *sink,
                                              const std::shared_ptr<GStreamerMSEMediaPlayerClient> &mediaPlayerClient);
G_END_DECLS
