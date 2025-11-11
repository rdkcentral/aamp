/*
 * Copyright (C) 2024   
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

#include <gst/gst.h>
#include <inttypes.h>
#include <stdint.h>

#include "GStreamerEMEUtils.h"
#include "GStreamerMSEUtils.h"
#include "IMediaPipelineCapabilities.h"
#include "PullModeSubtitlePlaybackDelegate.h"
#include "RialtoGStreamerMSEBaseSinkPrivate.h"
#include "RialtoGStreamerMSESubtitleSink.h"

using namespace firebolt::rialto::client;

GST_DEBUG_CATEGORY_STATIC(RialtoMSESubtitleSinkDebug);
#define GST_CAT_DEFAULT RialtoMSESubtitleSinkDebug

#define rialto_mse_subtitle_sink_parent_class parent_class
G_DEFINE_TYPE_WITH_CODE(RialtoMSESubtitleSink, rialto_mse_subtitle_sink, RIALTO_TYPE_MSE_BASE_SINK,
                        GST_DEBUG_CATEGORY_INIT(RialtoMSESubtitleSinkDebug, "rialtomsesubtitlesink", 0,
                                                "rialto mse subtitle sink"));

enum
{
    PROP_0,
    PROP_MUTE,
    PROP_TEXT_TRACK_IDENTIFIER,
    PROP_WINDOW_ID,
    PROP_ASYNC,
    PROP_LAST
};

static GstStateChangeReturn rialto_mse_subtitle_sink_change_state(GstElement *element, GstStateChange transition)
{
    RialtoMSESubtitleSink *sink = RIALTO_MSE_SUBTITLE_SINK(element);

    if (GST_STATE_CHANGE_NULL_TO_READY == transition)
    {
        GST_INFO_OBJECT(sink, "RialtoMSESubtitleSink state change to READY. Initializing delegate");
        rialto_mse_base_sink_initialise_delegate(RIALTO_MSE_BASE_SINK(sink),
                                                 std::make_shared<PullModeSubtitlePlaybackDelegate>(element));
    }

    GstStateChangeReturn result = GST_ELEMENT_CLASS(parent_class)->change_state(element, transition);
    if (G_UNLIKELY(result == GST_STATE_CHANGE_FAILURE))
    {
        GST_WARNING_OBJECT(sink, "State change failed");
        return result;
    }

    return result;
}

static void rialto_mse_subtitle_sink_get_property(GObject *object, guint propId, GValue *value, GParamSpec *pspec)
{
    switch (propId)
    {
    case PROP_MUTE:
    {
        g_value_set_boolean(value, FALSE); // Set default value first
        rialto_mse_base_sink_handle_get_property(RIALTO_MSE_BASE_SINK(object), IPlaybackDelegate::Property::Mute, value);
        break;
    }
    case PROP_TEXT_TRACK_IDENTIFIER:
    {
        g_value_set_string(value, ""); // Set default value first
        rialto_mse_base_sink_handle_get_property(RIALTO_MSE_BASE_SINK(object),
                                                 IPlaybackDelegate::Property::TextTrackIdentifier, value);
        break;
    }
    case PROP_WINDOW_ID:
    {
        g_value_set_uint(value, 0); // Set default value first
        rialto_mse_base_sink_handle_get_property(RIALTO_MSE_BASE_SINK(object), IPlaybackDelegate::Property::WindowId,
                                                 value);
        break;
    }
    case PROP_ASYNC:
    {
        g_value_set_boolean(value, FALSE); // Set default value first
        rialto_mse_base_sink_handle_get_property(RIALTO_MSE_BASE_SINK(object), IPlaybackDelegate::Property::Async, value);
        break;
    }
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, propId, pspec);
        break;
    }
}

static void rialto_mse_subtitle_sink_set_property(GObject *object, guint propId, const GValue *value, GParamSpec *pspec)
{
    switch (propId)
    {
    case PROP_MUTE:
        rialto_mse_base_sink_handle_set_property(RIALTO_MSE_BASE_SINK(object), IPlaybackDelegate::Property::Mute, value);
        break;
    case PROP_TEXT_TRACK_IDENTIFIER:
    {
        rialto_mse_base_sink_handle_set_property(RIALTO_MSE_BASE_SINK(object),
                                                 IPlaybackDelegate::Property::TextTrackIdentifier, value);
        break;
    }
    case PROP_WINDOW_ID:
    {
        rialto_mse_base_sink_handle_set_property(RIALTO_MSE_BASE_SINK(object), IPlaybackDelegate::Property::WindowId,
                                                 value);
        break;
    }
    case PROP_ASYNC:
    {
        rialto_mse_base_sink_handle_set_property(RIALTO_MSE_BASE_SINK(object), IPlaybackDelegate::Property::Async, value);
        break;
    }
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, propId, pspec);
        break;
    }
}

static void rialto_mse_subtitle_sink_init(RialtoMSESubtitleSink *sink)
{
    RialtoMSEBaseSinkPrivate *basePriv = sink->parent.priv;

    if (!rialto_mse_base_sink_initialise_sinkpad(RIALTO_MSE_BASE_SINK(sink)))
    {
        GST_ERROR_OBJECT(sink, "Failed to initialise SUBTITLE sink. Sink pad initialisation failed.");
        return;
    }

    gst_pad_set_chain_function(basePriv->m_sinkPad, rialto_mse_base_sink_chain);
    gst_pad_set_event_function(basePriv->m_sinkPad, rialto_mse_base_sink_event);
}

static void rialto_mse_subtitle_sink_class_init(RialtoMSESubtitleSinkClass *klass)
{
    GObjectClass *gobjectClass = G_OBJECT_CLASS(klass);
    GstElementClass *elementClass = GST_ELEMENT_CLASS(klass);
    gobjectClass->get_property = rialto_mse_subtitle_sink_get_property;
    gobjectClass->set_property = rialto_mse_subtitle_sink_set_property;
    elementClass->change_state = rialto_mse_subtitle_sink_change_state;

    g_object_class_install_property(gobjectClass, PROP_MUTE,
                                    g_param_spec_boolean("mute", "Mute", "Mute subtitles", FALSE, G_PARAM_READWRITE));

    g_object_class_install_property(gobjectClass, PROP_TEXT_TRACK_IDENTIFIER,
                                    g_param_spec_string("text-track-identifier", "Text Track Identifier",
                                                        "Identifier of text track. Valid input for service is "
                                                        "\"CC[1-4]\", \"TEXT[1-4]\", \"SERVICE[1-64]\"",
                                                        nullptr, GParamFlags(G_PARAM_READWRITE)));

    g_object_class_install_property(gobjectClass, PROP_WINDOW_ID,
                                    g_param_spec_uint("window-id", "Window ID", "Id of window (placeholder)", 0, 256, 0,
                                                      GParamFlags(G_PARAM_READWRITE)));

    g_object_class_install_property(gobjectClass, PROP_ASYNC,
                                    g_param_spec_boolean("async", "Async", "Asynchronous mode", FALSE, G_PARAM_READWRITE));

    std::unique_ptr<firebolt::rialto::IMediaPipelineCapabilities> mediaPlayerCapabilities =
        firebolt::rialto::IMediaPipelineCapabilitiesFactory::createFactory()->createMediaPipelineCapabilities();
    if (mediaPlayerCapabilities)
    {
        std::vector<std::string> supportedMimeTypes =
            mediaPlayerCapabilities->getSupportedMimeTypes(firebolt::rialto::MediaSourceType::SUBTITLE);

        rialto_mse_sink_setup_supported_caps(elementClass, supportedMimeTypes);
    }
    else
    {
        GST_ERROR("Failed to get supported mime types for Subtitle");
    }

    gst_element_class_set_details_simple(elementClass, "Rialto Subtitle Sink", "Parser/Subtitle/Sink/Subtitle",
                                         "Communicates with Rialto Server", "   ");
}
