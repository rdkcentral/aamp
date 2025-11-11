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
#include <mutex>

#include <gst/audio/audio.h>
#include <gst/gst.h>

#include "GStreamerMSEUtils.h"
#include "IMediaPipelineCapabilities.h"
#include "PullModeAudioPlaybackDelegate.h"
#include "PushModeAudioPlaybackDelegate.h"
#include "RialtoGStreamerMSEAudioSink.h"
#include "RialtoGStreamerMSEBaseSinkPrivate.h"

using namespace firebolt::rialto::client;

GST_DEBUG_CATEGORY_STATIC(RialtoMSEAudioSinkDebug);
#define GST_CAT_DEFAULT RialtoMSEAudioSinkDebug

#define rialto_mse_audio_sink_parent_class parent_class
G_DEFINE_TYPE_WITH_CODE(RialtoMSEAudioSink, rialto_mse_audio_sink, RIALTO_TYPE_MSE_BASE_SINK,
                        G_IMPLEMENT_INTERFACE(GST_TYPE_STREAM_VOLUME, NULL)
                            GST_DEBUG_CATEGORY_INIT(RialtoMSEAudioSinkDebug, "rialtomseaudiosink", 0,
                                                    "rialto mse audio sink"));

enum
{
    PROP_0,
    PROP_VOLUME,
    PROP_MUTE,
    PROP_GAP,
    PROP_LOW_LATENCY,
    PROP_SYNC,
    PROP_SYNC_OFF,
    PROP_STREAM_SYNC_MODE,
    PROP_AUDIO_FADE,
    PROP_FADE_VOLUME,
    PROP_LIMIT_BUFFERING_MS,
    PROP_USE_BUFFERING,
    PROP_ASYNC,
    PROP_WEBAUDIO,
    PROP_LAST
};

static GstStateChangeReturn rialto_mse_audio_sink_change_state(GstElement *element, GstStateChange transition)
{
    RialtoMSEBaseSink *sink = RIALTO_MSE_BASE_SINK(element);
    if (GST_STATE_CHANGE_NULL_TO_READY == transition)
    {
        if (PlaybackMode::Pull == sink->priv->m_playbackMode)
        {
            GST_INFO_OBJECT(sink, "RialtoMSEAudioSink state change to READY. Initializing Pull Mode delegate");
            rialto_mse_base_sink_initialise_delegate(sink, std::make_shared<PullModeAudioPlaybackDelegate>(element));
        }
        else // Push playback mode
        {
            GST_INFO_OBJECT(sink, "RialtoMSEAudioSink state change to READY. Initializing Push Mode delegate");
            rialto_mse_base_sink_initialise_delegate(sink, std::make_shared<PushModeAudioPlaybackDelegate>(element));
        }
    }

    GstStateChangeReturn result = GST_ELEMENT_CLASS(parent_class)->change_state(element, transition);
    if (G_UNLIKELY(result == GST_STATE_CHANGE_FAILURE))
    {
        GST_WARNING_OBJECT(sink, "State change failed");
        return result;
    }

    return result;
}

static void rialto_mse_audio_sink_get_property(GObject *object, guint propId, GValue *value, GParamSpec *pspec)
{
    RialtoMSEBaseSink *sink = RIALTO_MSE_BASE_SINK(object);
    switch (propId)
    {
    case PROP_VOLUME:
    {
        g_value_set_double(value, kDefaultVolume);
        rialto_mse_base_sink_handle_get_property(sink, IPlaybackDelegate::Property::Volume, value);
        break;
    }
    case PROP_MUTE:
    {
        g_value_set_boolean(value, kDefaultMute);
        rialto_mse_base_sink_handle_get_property(sink, IPlaybackDelegate::Property::Mute, value);
        break;
    }
    case PROP_SYNC:
    {
        g_value_set_boolean(value, kDefaultSync);
        rialto_mse_base_sink_handle_get_property(sink, IPlaybackDelegate::Property::Sync, value);
        break;
    }
    case PROP_STREAM_SYNC_MODE:
    {
        g_value_set_int(value, kDefaultStreamSyncMode);
        rialto_mse_base_sink_handle_get_property(sink, IPlaybackDelegate::Property::StreamSyncMode, value);
        break;
    }
    case PROP_FADE_VOLUME:
    {
        g_value_set_uint(value, kDefaultFadeVolume);
        rialto_mse_base_sink_handle_get_property(sink, IPlaybackDelegate::Property::FadeVolume, value);
        break;
    }
    case PROP_LIMIT_BUFFERING_MS:
    {
        g_value_set_uint(value, kDefaultBufferingLimit);
        rialto_mse_base_sink_handle_get_property(sink, IPlaybackDelegate::Property::LimitBufferingMs, value);
        break;
    }
    case PROP_USE_BUFFERING:
    {
        g_value_set_boolean(value, kDefaultUseBuffering);
        rialto_mse_base_sink_handle_get_property(sink, IPlaybackDelegate::Property::UseBuffering, value);
        break;
    }
    case PROP_ASYNC:
    {
        g_value_set_boolean(value, TRUE);
        rialto_mse_base_sink_handle_get_property(sink, IPlaybackDelegate::Property::Async, value);
        break;
    }
    case PROP_WEBAUDIO:
    {
        g_value_set_boolean(value, (sink->priv->m_playbackMode == PlaybackMode::Push));
        break;
    }
    default:
    {
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, propId, pspec);
        break;
    }
    }
}

static void rialto_mse_audio_sink_set_property(GObject *object, guint propId, const GValue *value, GParamSpec *pspec)
{
    RialtoMSEBaseSink *sink = RIALTO_MSE_BASE_SINK(object);
    switch (propId)
    {
    case PROP_VOLUME:
    {
        rialto_mse_base_sink_handle_set_property(sink, IPlaybackDelegate::Property::Volume, value);
        break;
    }
    case PROP_MUTE:
    {
        rialto_mse_base_sink_handle_set_property(sink, IPlaybackDelegate::Property::Mute, value);
        break;
    }
    case PROP_GAP:
    {
        rialto_mse_base_sink_handle_set_property(sink, IPlaybackDelegate::Property::Gap, value);
        break;
    }
    case PROP_LOW_LATENCY:
    {
        rialto_mse_base_sink_handle_set_property(sink, IPlaybackDelegate::Property::LowLatency, value);
        break;
    }
    case PROP_SYNC:
    {
        rialto_mse_base_sink_handle_set_property(sink, IPlaybackDelegate::Property::Sync, value);
        break;
    }
    case PROP_SYNC_OFF:
    {
        rialto_mse_base_sink_handle_set_property(sink, IPlaybackDelegate::Property::SyncOff, value);
        break;
    }
    case PROP_STREAM_SYNC_MODE:
    {
        rialto_mse_base_sink_handle_set_property(sink, IPlaybackDelegate::Property::StreamSyncMode, value);
        break;
    }
    case PROP_AUDIO_FADE:
    {
        rialto_mse_base_sink_handle_set_property(sink, IPlaybackDelegate::Property::AudioFade, value);
        break;
    }
    case PROP_LIMIT_BUFFERING_MS:
    {
        rialto_mse_base_sink_handle_set_property(sink, IPlaybackDelegate::Property::LimitBufferingMs, value);
        break;
    }
    case PROP_USE_BUFFERING:
    {
        rialto_mse_base_sink_handle_set_property(sink, IPlaybackDelegate::Property::UseBuffering, value);
        break;
    }
    case PROP_ASYNC:
    {
        rialto_mse_base_sink_handle_set_property(sink, IPlaybackDelegate::Property::Async, value);
        break;
    }
    case PROP_WEBAUDIO:
    {
        if (GST_STATE(sink) > GST_STATE_NULL)
        {
            GST_ERROR_OBJECT(object, "Playback mode set too late - sink is not in NULL state");
            break;
        }
        if (TRUE == g_value_get_boolean(value))
        {
            sink->priv->m_playbackMode = PlaybackMode::Push;
        }
        else
        {
            sink->priv->m_playbackMode = PlaybackMode::Pull;
        }
        break;
    }
    default:
    {
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, propId, pspec);
        break;
    }
    }
}

static void rialto_mse_audio_sink_init(RialtoMSEAudioSink *sink)
{
    RialtoMSEBaseSinkPrivate *priv = sink->parent.priv;

    if (!rialto_mse_base_sink_initialise_sinkpad(RIALTO_MSE_BASE_SINK(sink)))
    {
        GST_ERROR_OBJECT(sink, "Failed to initialise AUDIO sink. Sink pad initialisation failed.");
        return;
    }

    gst_pad_set_chain_function(priv->m_sinkPad, rialto_mse_base_sink_chain);
    gst_pad_set_event_function(priv->m_sinkPad, rialto_mse_base_sink_event);
}

static void rialto_mse_audio_sink_class_init(RialtoMSEAudioSinkClass *klass)
{
    GObjectClass *gobjectClass = G_OBJECT_CLASS(klass);
    GstElementClass *elementClass = GST_ELEMENT_CLASS(klass);
    gobjectClass->get_property = rialto_mse_audio_sink_get_property;
    gobjectClass->set_property = rialto_mse_audio_sink_set_property;
    elementClass->change_state = rialto_mse_audio_sink_change_state;

    g_object_class_install_property(gobjectClass, PROP_VOLUME,
                                    g_param_spec_double("volume", "Volume", "Volume of this stream", 0, 1.0,
                                                        kDefaultVolume,
                                                        GParamFlags(G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)));

    g_object_class_install_property(gobjectClass, PROP_MUTE,
                                    g_param_spec_boolean("mute", "Mute", "Mute status of this stream", kDefaultMute,
                                                         GParamFlags(G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)));

    g_object_class_install_property(gobjectClass, PROP_GAP,
                                    g_param_spec_boxed("gap", "Gap", "Audio Gap", GST_TYPE_STRUCTURE,
                                                       (GParamFlags)(G_PARAM_WRITABLE | G_PARAM_STATIC_STRINGS)));

    g_object_class_install_property(gobjectClass, PROP_USE_BUFFERING,
                                    g_param_spec_boolean("use-buffering",
                                                         "Use buffering", "Emit GST_MESSAGE_BUFFERING based on low-/high-percent thresholds",
                                                         kDefaultUseBuffering, G_PARAM_READWRITE));
    g_object_class_install_property(gobjectClass, PROP_ASYNC,
                                    g_param_spec_boolean("async", "Async", "Asynchronous mode", FALSE, G_PARAM_READWRITE));
    g_object_class_install_property(gobjectClass, PROP_WEBAUDIO,
                                    g_param_spec_boolean("web-audio",
                                                         "Webaudio mode", "Enable webaudio mode. Property should be set before NULL->READY transition",
                                                         FALSE, G_PARAM_READWRITE));

    std::unique_ptr<firebolt::rialto::IMediaPipelineCapabilities> mediaPlayerCapabilities =
        firebolt::rialto::IMediaPipelineCapabilitiesFactory::createFactory()->createMediaPipelineCapabilities();
    if (mediaPlayerCapabilities)
    {
        std::vector<std::string> supportedMimeTypes =
            mediaPlayerCapabilities->getSupportedMimeTypes(firebolt::rialto::MediaSourceType::AUDIO);

        rialto_mse_sink_setup_supported_caps(elementClass, supportedMimeTypes);

        const std::string kLowLatencyPropertyName{"low-latency"};
        const std::string kSyncPropertyName{"sync"};
        const std::string kSyncOffPropertyName{"sync-off"};
        const std::string kStreamSyncModePropertyName{"stream-sync-mode"};
        const std::string kAudioFadePropertyName{"audio-fade"};
        const std::string kFadeVolumePropertyName{"fade-volume"};
        const std::string kBufferingLimitPropertyName{"limit-buffering-ms"};
        const std::vector<std::string> kPropertyNamesToSearch{kLowLatencyPropertyName,     kSyncPropertyName,
                                                              kSyncOffPropertyName,        kStreamSyncModePropertyName,
                                                              kBufferingLimitPropertyName, kAudioFadePropertyName,
                                                              kFadeVolumePropertyName};
        std::vector<std::string> supportedProperties{
            mediaPlayerCapabilities->getSupportedProperties(firebolt::rialto::MediaSourceType::AUDIO,
                                                            kPropertyNamesToSearch)};

        for (auto it = supportedProperties.begin(); it != supportedProperties.end(); ++it)
        {
            if (kLowLatencyPropertyName == *it)
            {
                g_object_class_install_property(gobjectClass, PROP_LOW_LATENCY,
                                                g_param_spec_boolean(kLowLatencyPropertyName.c_str(),
                                                                     "low latency", "Turn on low latency mode, for use with gaming (no audio decoding, no a/v sync)",
                                                                     kDefaultLowLatency, GParamFlags(G_PARAM_WRITABLE)));
            }
            else if (kSyncPropertyName == *it)
            {
                g_object_class_install_property(gobjectClass, PROP_SYNC,
                                                g_param_spec_boolean(kSyncPropertyName.c_str(), "sync", "Clock sync",
                                                                     kDefaultSync, GParamFlags(G_PARAM_READWRITE)));
            }
            else if (kSyncOffPropertyName == *it)
            {
                g_object_class_install_property(gobjectClass, PROP_SYNC_OFF,
                                                g_param_spec_boolean(kSyncOffPropertyName.c_str(),
                                                                     "sync off", "Turn on free running audio. Must be set before pipeline is PLAYING state.",
                                                                     kDefaultSyncOff, GParamFlags(G_PARAM_WRITABLE)));
            }
            else if (kStreamSyncModePropertyName == *it)
            {
                g_object_class_install_property(gobjectClass, PROP_STREAM_SYNC_MODE,
                                                g_param_spec_int(kStreamSyncModePropertyName.c_str(),
                                                                 "stream sync mode", "1 - Frame to decode frame will immediately proceed next frame sync, 0 - Frame decoded with no frame sync",
                                                                 0, G_MAXINT, kDefaultStreamSyncMode,
                                                                 GParamFlags(G_PARAM_READWRITE)));
            }
            else if (kAudioFadePropertyName == *it)
            {
                g_object_class_install_property(gobjectClass, PROP_AUDIO_FADE,
                                                g_param_spec_string(kAudioFadePropertyName.c_str(),
                                                                    "audio fade", "Start audio fade (vol[0-100],duration ms,easetype[(L)inear,Cubic(I)n,Cubic(O)ut])",
                                                                    kDefaultAudioFade, GParamFlags(G_PARAM_WRITABLE)));
            }
            else if (kFadeVolumePropertyName == *it)
            {
                g_object_class_install_property(gobjectClass, PROP_FADE_VOLUME,
                                                g_param_spec_uint(kFadeVolumePropertyName.c_str(), "fade volume",
                                                                  "Get current fade volume", 0, 100, kDefaultFadeVolume,
                                                                  G_PARAM_READABLE));
            }
            else if (kBufferingLimitPropertyName == *it)
            {
                constexpr uint32_t kMaxValue{20000};
                g_object_class_install_property(gobjectClass, PROP_LIMIT_BUFFERING_MS,
                                                g_param_spec_uint("limit-buffering-ms",
                                                                  "limit buffering ms", "Set millisecond threshold used if limit_buffering is set. Changing this value does not enable/disable limit_buffering",
                                                                  0, kMaxValue, kDefaultBufferingLimit,
                                                                  G_PARAM_READWRITE));
            }
            else
            {
                GST_ERROR("Unexpected property %s returned from rialto", it->c_str());
            }
        }
    }
    else
    {
        GST_ERROR("Failed to get supported mime types for AUDIO");
    }

    gst_element_class_set_details_simple(elementClass, "Rialto Audio Sink", "Decoder/Audio/Sink/Audio",
                                         "Communicates with Rialto Server", "   ");
}
