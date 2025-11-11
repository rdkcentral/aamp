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

#include <gst/gst.h>
#include <inttypes.h>
#include <stdint.h>

#include "GStreamerEMEUtils.h"
#include "GStreamerMSEUtils.h"
#include "IMediaPipelineCapabilities.h"
#include "PullModeVideoPlaybackDelegate.h"
#include "RialtoGStreamerMSEBaseSinkPrivate.h"
#include "RialtoGStreamerMSEVideoSink.h"

using namespace firebolt::rialto::client;

GST_DEBUG_CATEGORY_STATIC(RialtoMSEVideoSinkDebug);
#define GST_CAT_DEFAULT RialtoMSEVideoSinkDebug

#define rialto_mse_video_sink_parent_class parent_class
G_DEFINE_TYPE_WITH_CODE(RialtoMSEVideoSink, rialto_mse_video_sink, RIALTO_TYPE_MSE_BASE_SINK,
                        GST_DEBUG_CATEGORY_INIT(RialtoMSEVideoSinkDebug, "rialtomsevideosink", 0,
                                                "rialto mse video sink"));

enum
{
    PROP_0,
    PROP_WINDOW_SET,
    PROP_MAX_VIDEO_WIDTH,
    PROP_MAX_VIDEO_HEIGHT,
    PROP_MAX_VIDEO_WIDTH_DEPRECATED,
    PROP_MAX_VIDEO_HEIGHT_DEPRECATED,
    PROP_FRAME_STEP_ON_PREROLL,
    PROP_IMMEDIATE_OUTPUT,
    PROP_SYNCMODE_STREAMING,
    PROP_SHOW_VIDEO_WINDOW,
    PROP_IS_MASTER,
    PROP_LAST
};

static GstStateChangeReturn rialto_mse_video_sink_change_state(GstElement *element, GstStateChange transition)
{
    RialtoMSEVideoSink *sink = RIALTO_MSE_VIDEO_SINK(element);
    if (GST_STATE_CHANGE_NULL_TO_READY == transition)
    {
        GST_INFO_OBJECT(sink, "RialtoMSEVideoSink state change to READY. Initializing delegate");
        rialto_mse_base_sink_initialise_delegate(RIALTO_MSE_BASE_SINK(sink),
                                                 std::make_shared<PullModeVideoPlaybackDelegate>(element));
    }
    GstStateChangeReturn result = GST_ELEMENT_CLASS(parent_class)->change_state(element, transition);
    if (G_UNLIKELY(result == GST_STATE_CHANGE_FAILURE))
    {
        GST_WARNING_OBJECT(sink, "State change failed");
        return result;
    }

    return result;
}

static void rialto_mse_video_sink_get_property(GObject *object, guint propId, GValue *value, GParamSpec *pspec)
{
    switch (propId)
    {
    case PROP_WINDOW_SET:
    {
        g_value_set_string(value, "0,0,1920,1080"); // Set default value
        rialto_mse_base_sink_handle_get_property(RIALTO_MSE_BASE_SINK(object), IPlaybackDelegate::Property::WindowSet,
                                                 value);
        break;
    }
    case PROP_MAX_VIDEO_WIDTH_DEPRECATED:
        GST_WARNING_OBJECT(object, "MaxVideoWidth property is deprecated. Use 'max-video-width' instead");
    case PROP_MAX_VIDEO_WIDTH:
    {
        g_value_set_uint(value, 0); // Set default value
        rialto_mse_base_sink_handle_get_property(RIALTO_MSE_BASE_SINK(object),
                                                 IPlaybackDelegate::Property::MaxVideoWidth, value);
        break;
    }
    case PROP_MAX_VIDEO_HEIGHT_DEPRECATED:
        GST_WARNING_OBJECT(object, "MaxVideoHeight property is deprecated. Use 'max-video-height' instead");
    case PROP_MAX_VIDEO_HEIGHT:
    {
        g_value_set_uint(value, 0); // Set default value
        rialto_mse_base_sink_handle_get_property(RIALTO_MSE_BASE_SINK(object),
                                                 IPlaybackDelegate::Property::MaxVideoHeight, value);
        break;
    }
    case PROP_FRAME_STEP_ON_PREROLL:
    {
        g_value_set_boolean(value, FALSE); // Set default value
        rialto_mse_base_sink_handle_get_property(RIALTO_MSE_BASE_SINK(object),
                                                 IPlaybackDelegate::Property::FrameStepOnPreroll, value);
        break;
    }
    case PROP_IMMEDIATE_OUTPUT:
    {
        g_value_set_boolean(value, FALSE); // Set default value
        rialto_mse_base_sink_handle_get_property(RIALTO_MSE_BASE_SINK(object),
                                                 IPlaybackDelegate::Property::ImmediateOutput, value);
        break;
    }
    case PROP_IS_MASTER:
    {
        g_value_set_boolean(value, TRUE); // Set default value
        std::unique_ptr<firebolt::rialto::IMediaPipelineCapabilities> mediaPlayerCapabilities =
            firebolt::rialto::IMediaPipelineCapabilitiesFactory::createFactory()->createMediaPipelineCapabilities();
        bool isMaster{false};
        if (mediaPlayerCapabilities && mediaPlayerCapabilities->isVideoMaster(isMaster))
        {
            g_value_set_boolean(value, isMaster);
        }
        break;
    }
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, propId, pspec);
        break;
    }
}

static void rialto_mse_video_sink_set_property(GObject *object, guint propId, const GValue *value, GParamSpec *pspec)
{
    switch (propId)
    {
    case PROP_WINDOW_SET:
    {
        rialto_mse_base_sink_handle_set_property(RIALTO_MSE_BASE_SINK(object), IPlaybackDelegate::Property::WindowSet,
                                                 value);
        break;
    }
    case PROP_MAX_VIDEO_WIDTH:
    case PROP_MAX_VIDEO_WIDTH_DEPRECATED:
        rialto_mse_base_sink_handle_set_property(RIALTO_MSE_BASE_SINK(object),
                                                 IPlaybackDelegate::Property::MaxVideoWidth, value);
        break;
    case PROP_MAX_VIDEO_HEIGHT:
    case PROP_MAX_VIDEO_HEIGHT_DEPRECATED:
        rialto_mse_base_sink_handle_set_property(RIALTO_MSE_BASE_SINK(object),
                                                 IPlaybackDelegate::Property::MaxVideoHeight, value);
        break;
    case PROP_FRAME_STEP_ON_PREROLL:
    {
        rialto_mse_base_sink_handle_set_property(RIALTO_MSE_BASE_SINK(object),
                                                 IPlaybackDelegate::Property::FrameStepOnPreroll, value);
        break;
    }
    case PROP_IMMEDIATE_OUTPUT:
    {
        rialto_mse_base_sink_handle_set_property(RIALTO_MSE_BASE_SINK(object),
                                                 IPlaybackDelegate::Property::ImmediateOutput, value);
        break;
    }
    case PROP_SYNCMODE_STREAMING:
    {
        rialto_mse_base_sink_handle_set_property(RIALTO_MSE_BASE_SINK(object),
                                                 IPlaybackDelegate::Property::SyncmodeStreaming, value);
        break;
    }
    case PROP_SHOW_VIDEO_WINDOW:
    {
        rialto_mse_base_sink_handle_set_property(RIALTO_MSE_BASE_SINK(object),
                                                 IPlaybackDelegate::Property::ShowVideoWindow, value);
        break;
    }
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, propId, pspec);
        break;
    }
}

static void rialto_mse_video_sink_init(RialtoMSEVideoSink *sink)
{
    RialtoMSEBaseSinkPrivate *basePriv = sink->parent.priv;

    if (!rialto_mse_base_sink_initialise_sinkpad(RIALTO_MSE_BASE_SINK(sink)))
    {
        GST_ERROR_OBJECT(sink, "Failed to initialise VIDEO sink. Sink pad initialisation failed.");
        return;
    }

    gst_pad_set_chain_function(basePriv->m_sinkPad, rialto_mse_base_sink_chain);
    gst_pad_set_event_function(basePriv->m_sinkPad, rialto_mse_base_sink_event);
}

static void rialto_mse_video_sink_class_init(RialtoMSEVideoSinkClass *klass)
{
    GObjectClass *gobjectClass = G_OBJECT_CLASS(klass);
    GstElementClass *elementClass = GST_ELEMENT_CLASS(klass);
    gobjectClass->get_property = rialto_mse_video_sink_get_property;
    gobjectClass->set_property = rialto_mse_video_sink_set_property;
    elementClass->change_state = rialto_mse_video_sink_change_state;

    g_object_class_install_property(gobjectClass, PROP_WINDOW_SET,
                                    g_param_spec_string("rectangle", "rectangle", "Window Set Format: x,y,width,height",
                                                        nullptr, GParamFlags(G_PARAM_READWRITE)));

    g_object_class_install_property(gobjectClass, PROP_MAX_VIDEO_WIDTH,
                                    g_param_spec_uint("max-video-width",
                                                      "max video width", "Maximum width of video frames to be decoded. Should only be set for video only streams.",
                                                      0, 3840, DEFAULT_MAX_VIDEO_WIDTH, GParamFlags(G_PARAM_READWRITE)));

    g_object_class_install_property(gobjectClass, PROP_MAX_VIDEO_HEIGHT,
                                    g_param_spec_uint("max-video-height",
                                                      "max video height", "Maximum height of video frames to be decoded. should only be set for video only streams.",
                                                      0, 2160, DEFAULT_MAX_VIDEO_HEIGHT, GParamFlags(G_PARAM_READWRITE)));

    g_object_class_install_property(gobjectClass, PROP_MAX_VIDEO_WIDTH_DEPRECATED,
                                    g_param_spec_uint("maxVideoWidth", "maxVideoWidth", "[DEPRECATED] Use max-video-width",
                                                      0, 3840, DEFAULT_MAX_VIDEO_WIDTH, GParamFlags(G_PARAM_READWRITE)));

    g_object_class_install_property(gobjectClass, PROP_MAX_VIDEO_HEIGHT_DEPRECATED,
                                    g_param_spec_uint("maxVideoHeight", "maxVideoHeight", "[DEPRECATED] max-video-height",
                                                      0, 2160, DEFAULT_MAX_VIDEO_HEIGHT, GParamFlags(G_PARAM_READWRITE)));

    g_object_class_install_property(gobjectClass, PROP_FRAME_STEP_ON_PREROLL,
                                    g_param_spec_boolean("frame-step-on-preroll", "frame step on preroll",
                                                         "allow frame stepping on preroll into pause", FALSE,
                                                         G_PARAM_READWRITE));
    g_object_class_install_property(gobjectClass, PROP_IS_MASTER,
                                    g_param_spec_boolean("is-master", "is master",
                                                         "Checks if the platform is video master", TRUE,
                                                         G_PARAM_READABLE));

    std::unique_ptr<firebolt::rialto::IMediaPipelineCapabilities> mediaPlayerCapabilities =
        firebolt::rialto::IMediaPipelineCapabilitiesFactory::createFactory()->createMediaPipelineCapabilities();
    if (mediaPlayerCapabilities)
    {
        std::vector<std::string> supportedMimeTypes =
            mediaPlayerCapabilities->getSupportedMimeTypes(firebolt::rialto::MediaSourceType::VIDEO);

        rialto_mse_sink_setup_supported_caps(elementClass, supportedMimeTypes);

        const std::string kImmediateOutputPropertyName{"immediate-output"};
        const std::string kSyncmodeStreamingPropertyName{"syncmode-streaming"};
        const std::string kShowVideoWindowPropertyName{"show-video-window"};
        const std::vector<std::string> kPropertyNamesToSearch{kImmediateOutputPropertyName,
                                                              kSyncmodeStreamingPropertyName,
                                                              kShowVideoWindowPropertyName};
        std::vector<std::string> supportedProperties{
            mediaPlayerCapabilities->getSupportedProperties(firebolt::rialto::MediaSourceType::VIDEO,
                                                            kPropertyNamesToSearch)};

        for (const auto &propertyName : supportedProperties)
        {
            if (kImmediateOutputPropertyName == propertyName)
            {
                g_object_class_install_property(gobjectClass, PROP_IMMEDIATE_OUTPUT,
                                                g_param_spec_boolean(kImmediateOutputPropertyName.c_str(),
                                                                     "immediate output", "immediate output", TRUE,
                                                                     GParamFlags(G_PARAM_READWRITE)));
            }
            else if (kSyncmodeStreamingPropertyName == propertyName)
            {
                g_object_class_install_property(gobjectClass, PROP_SYNCMODE_STREAMING,
                                                g_param_spec_boolean("syncmode-streaming", "Streaming Sync Mode",
                                                                     "Enable/disable OTT streaming sync mode", FALSE,
                                                                     G_PARAM_WRITABLE));
            }
            else if (kShowVideoWindowPropertyName == propertyName)
            {
                g_object_class_install_property(gobjectClass, PROP_SHOW_VIDEO_WINDOW,
                                                g_param_spec_boolean(kShowVideoWindowPropertyName.c_str(),
                                                                     "make video window visible",
                                                                     "true: visible, false: hidden", TRUE,
                                                                     G_PARAM_WRITABLE));
            }
        }
    }
    else
    {
        GST_ERROR("Failed to get supported mime types for VIDEO");
    }

    gst_element_class_set_details_simple(elementClass, "Rialto Video Sink", "Decoder/Video/Sink/Video",
                                         "Communicates with Rialto Server", "   ");
}
