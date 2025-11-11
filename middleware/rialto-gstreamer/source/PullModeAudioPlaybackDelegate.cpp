/*
 * Copyright (C) 2025   
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

#include "PullModeAudioPlaybackDelegate.h"
#include "GStreamerMSEUtils.h"
#include "GstreamerCatLog.h"
#include <gst/audio/audio.h>
#include <gst/gst.h>
#include <gst/pbutils/pbutils.h>

#define GST_CAT_DEFAULT rialtoGStreamerCat

namespace
{
firebolt::rialto::EaseType convertCharToEaseType(char easeTypeChar)
{
    switch (easeTypeChar)
    {
    case 'L':
        return firebolt::rialto::EaseType::EASE_LINEAR;
    case 'I':
        return firebolt::rialto::EaseType::EASE_IN_CUBIC;
    case 'O':
        return firebolt::rialto::EaseType::EASE_OUT_CUBIC;
    default:
        return firebolt::rialto::EaseType::EASE_LINEAR;
    }
}
} // namespace

PullModeAudioPlaybackDelegate::PullModeAudioPlaybackDelegate(GstElement *sink) : PullModePlaybackDelegate(sink)
{
    m_mediaSourceType = firebolt::rialto::MediaSourceType::AUDIO;
}

GstStateChangeReturn PullModeAudioPlaybackDelegate::changeState(GstStateChange transition)
{
    switch (transition)
    {
    case GST_STATE_CHANGE_READY_TO_PAUSED:
    {
        if (!attachToMediaClientAndSetStreamsNumber())
        {
            return GST_STATE_CHANGE_FAILURE;
        }

        std::shared_ptr<GStreamerMSEMediaPlayerClient> client = m_mediaPlayerManager.getMediaPlayerClient();
        if (!client)
        {
            GST_ERROR_OBJECT(m_sink, "MediaPlayerClient is nullptr");
            return GST_STATE_CHANGE_FAILURE;
        }
        if (m_isVolumeQueued)
        {
            client->setVolume(m_targetVolume, kDefaultVolumeDuration, kDefaultEaseType);
            m_isVolumeQueued = false;
        }
        if (m_isAudioFadeQueued)
        {
            AudioFadeConfig audioFadeConfig;
            {
                std::lock_guard<std::mutex> lock(m_audioFadeConfigMutex);
                audioFadeConfig = m_audioFadeConfig;
            }
            client->setVolume(audioFadeConfig.volume, audioFadeConfig.duration, audioFadeConfig.easeType);
            m_isAudioFadeQueued = false;
        }
        break;
    }
    default:
        break;
    }
    return PullModePlaybackDelegate::changeState(transition);
}

gboolean PullModeAudioPlaybackDelegate::handleEvent(GstPad *pad, GstObject *parent, GstEvent *event)
{
    switch (GST_EVENT_TYPE(event))
    {
    case GST_EVENT_CAPS:
    {
        GstCaps *caps{nullptr};
        gst_event_parse_caps(event, &caps);
        if (m_sourceAttached)
        {
            GST_INFO_OBJECT(m_sink, "Source already attached. Skip calling attachSource");
            break;
        }

        GST_INFO_OBJECT(m_sink, "Attaching AUDIO source with caps %" GST_PTR_FORMAT, caps);

        std::unique_ptr<firebolt::rialto::IMediaPipeline::MediaSource> asource = createMediaSource(caps);
        if (asource)
        {
            std::shared_ptr<GStreamerMSEMediaPlayerClient> client = m_mediaPlayerManager.getMediaPlayerClient();
            if ((!client) || (!client->attachSource(asource, RIALTO_MSE_BASE_SINK(m_sink), shared_from_this())))
            {
                GST_ERROR_OBJECT(m_sink, "Failed to attach AUDIO source");
            }
            else
            {
                m_sourceAttached = true;

                if (m_isMuteQueued)
                {
                    client->setMute(m_mute, m_sourceId);
                    m_isMuteQueued = false;
                }
                if (m_isLowLatencyQueued)
                {
                    if (!client->setLowLatency(m_lowLatency))
                    {
                        GST_ERROR_OBJECT(m_sink, "Could not set queued low-latency");
                    }
                    m_isLowLatencyQueued = false;
                }
                if (m_isSyncQueued)
                {
                    if (!client->setSync(m_sync))
                    {
                        GST_ERROR_OBJECT(m_sink, "Could not set queued sync");
                    }
                    m_isSyncQueued = false;
                }
                if (m_isSyncOffQueued)
                {
                    if (!client->setSyncOff(m_syncOff))
                    {
                        GST_ERROR_OBJECT(m_sink, "Could not set queued sync-off");
                    }
                    m_isSyncOffQueued = false;
                }
                if (m_isStreamSyncModeQueued)
                {
                    if (!client->setStreamSyncMode(m_sourceId, m_streamSyncMode))
                    {
                        GST_ERROR_OBJECT(m_sink, "Could not set queued stream-sync-mode");
                    }
                    m_isStreamSyncModeQueued = false;
                }
                if (m_isBufferingLimitQueued)
                {
                    client->setBufferingLimit(m_bufferingLimit);
                    m_isBufferingLimitQueued = false;
                }
                if (m_isUseBufferingQueued)
                {
                    client->setUseBuffering(m_useBuffering);
                    m_isUseBufferingQueued = false;
                }
                // check if READY -> PAUSED was requested before source was attached
                if (GST_STATE_NEXT(m_sink) == GST_STATE_PAUSED)
                {
                    client->pause(m_sourceId);
                }
            }
        }
        else
        {
            GST_ERROR_OBJECT(m_sink, "Failed to create AUDIO source");
        }
        break;
    }
    case GST_EVENT_CUSTOM_DOWNSTREAM:
    case GST_EVENT_CUSTOM_DOWNSTREAM_OOB:
    {
        if (gst_event_has_name(event, "switch-source"))
        {
            GST_DEBUG_OBJECT(m_sink, "Switch source event received");
            const GstStructure *structure{gst_event_get_structure(event)};
            const GValue *value = gst_structure_get_value(structure, "caps");
            if (!value)
            {
                GST_ERROR_OBJECT(m_sink, "Caps not available in switch-source event");
                break;
            }
            const GstCaps *caps = gst_value_get_caps(value);
            GstCaps *mutableCaps = gst_caps_copy(caps);
            std::unique_ptr<firebolt::rialto::IMediaPipeline::MediaSource> asource = createMediaSource(mutableCaps);
            gst_caps_unref(mutableCaps);
            if (!asource)
            {
                GST_ERROR_OBJECT(m_sink, "Not able to parse caps");
                break;
            }
            std::shared_ptr<GStreamerMSEMediaPlayerClient> client = m_mediaPlayerManager.getMediaPlayerClient();
            if ((!client) || (!client->switchSource(asource)))
            {
                GST_ERROR_OBJECT(m_sink, "Failed to switch AUDIO source");
            }
        }
        break;
    }
    default:
        break;
    }
    return PullModePlaybackDelegate::handleEvent(pad, parent, event);
}

void PullModeAudioPlaybackDelegate::getProperty(const Property &type, GValue *value)
{
    std::shared_ptr<GStreamerMSEMediaPlayerClient> client{m_mediaPlayerManager.getMediaPlayerClient()};

    switch (type)
    {
    case IPlaybackDelegate::Property::Volume:
    {
        double volume;
        if (client)
        {
            if (client->getVolume(volume))
                m_targetVolume = volume;
            else
                volume = m_targetVolume; // Use last known volume
        }
        else
        {
            volume = m_targetVolume;
        }
        g_value_set_double(value, volume);
        break;
    }
    case IPlaybackDelegate::Property::Mute:
    {
        if (!client)
        {
            g_value_set_boolean(value, m_mute);
            return;
        }
        g_value_set_boolean(value, client->getMute(m_sourceId));
        break;
    }
    case IPlaybackDelegate::Property::Sync:
    {
        if (!client)
        {
            g_value_set_boolean(value, m_sync);
            return;
        }

        bool sync{kDefaultSync};
        if (!client->getSync(sync))
        {
            GST_ERROR_OBJECT(m_sink, "Could not get sync");
        }
        g_value_set_boolean(value, sync);
        break;
    }
    case IPlaybackDelegate::Property::StreamSyncMode:
    {
        if (!client)
        {
            g_value_set_int(value, m_streamSyncMode);
            return;
        }

        int32_t streamSyncMode{kDefaultStreamSyncMode};
        if (!client->getStreamSyncMode(streamSyncMode))
        {
            GST_ERROR_OBJECT(m_sink, "Could not get stream-sync-mode");
        }
        g_value_set_int(value, streamSyncMode);
        break;
    }
    case IPlaybackDelegate::Property::FadeVolume:
    {
        double volume{};
        if (!client || !client->getVolume(volume))
        {
            g_value_set_uint(value, kDefaultFadeVolume);
            return;
        }
        g_value_set_uint(value, static_cast<uint32_t>(volume * 100.0));
        break;
    }
    case IPlaybackDelegate::Property::LimitBufferingMs:
    {
        if (!client)
        {
            g_value_set_uint(value, m_bufferingLimit);
            return;
        }
        g_value_set_uint(value, client->getBufferingLimit());
        break;
    }
    case IPlaybackDelegate::Property::UseBuffering:
    {
        if (!client)
        {
            g_value_set_boolean(value, m_useBuffering);
            return;
        }
        g_value_set_boolean(value, client->getUseBuffering());
        break;
    }
    case IPlaybackDelegate::Property::Async:
    {
        // Rialto MSE Audio sink is always async
        g_value_set_boolean(value, TRUE);
        break;
    }
    default:
    {
        PullModePlaybackDelegate::getProperty(type, value);
        break;
    }
    }
}

void PullModeAudioPlaybackDelegate::setProperty(const Property &type, const GValue *value)
{
    std::shared_ptr<GStreamerMSEMediaPlayerClient> client = m_mediaPlayerManager.getMediaPlayerClient();

    switch (type)
    {
    case IPlaybackDelegate::Property::Volume:
    {
        m_targetVolume = g_value_get_double(value);
        if (!client || !m_sourceAttached)
        {
            GST_DEBUG_OBJECT(m_sink, "Enqueue volume setting");
            m_isVolumeQueued = true;
            return;
        }
        client->setVolume(m_targetVolume, kDefaultVolumeDuration, kDefaultEaseType);
        break;
    }
    case IPlaybackDelegate::Property::Mute:
    {
        m_mute = g_value_get_boolean(value);
        if (!client || !m_sourceAttached)
        {
            GST_DEBUG_OBJECT(m_sink, "Enqueue mute setting");
            m_isMuteQueued = true;
            return;
        }
        client->setMute(m_mute, m_sourceId);
        break;
    }
    case IPlaybackDelegate::Property::Gap:
    {
        gint64 position{0}, discontinuityGap{0};
        guint duration{0};
        gboolean audioAac{FALSE};

        GstStructure *gapData = GST_STRUCTURE_CAST(g_value_get_boxed(value));
        if (!gst_structure_get_int64(gapData, "position", &position))
        {
            GST_WARNING_OBJECT(m_sink, "Set gap: position is missing!");
        }
        if (!gst_structure_get_uint(gapData, "duration", &duration))
        {
            GST_WARNING_OBJECT(m_sink, "Set gap: duration is missing!");
        }
        if (!gst_structure_get_int64(gapData, "discontinuity-gap", &discontinuityGap))
        {
            GST_WARNING_OBJECT(m_sink, "Set gap: discontinuity gap is missing!");
        }
        if (!gst_structure_get_boolean(gapData, "audio-aac", &audioAac))
        {
            GST_WARNING_OBJECT(m_sink, "Set gap: audio aac is missing!");
        }

        GST_DEBUG_OBJECT(m_sink, "Processing audio gap.");
        client->processAudioGap(position, duration, discontinuityGap, audioAac);
        break;
    }
    case IPlaybackDelegate::Property::LowLatency:
    {
        m_lowLatency = g_value_get_boolean(value);
        if (!client)
        {
            GST_DEBUG_OBJECT(m_sink, "Enqueue low latency setting");
            m_isLowLatencyQueued = true;
            return;
        }

        if (!client->setLowLatency(m_lowLatency))
        {
            GST_ERROR_OBJECT(m_sink, "Could not set low-latency");
        }
        break;
    }
    case IPlaybackDelegate::Property::Sync:
    {
        m_sync = g_value_get_boolean(value);
        if (!client)
        {
            GST_DEBUG_OBJECT(m_sink, "Enqueue sync setting");
            m_isSyncQueued = true;
            return;
        }

        if (!client->setSync(m_sync))
        {
            GST_ERROR_OBJECT(m_sink, "Could not set sync");
        }
        break;
    }
    case IPlaybackDelegate::Property::SyncOff:
    {
        m_syncOff = g_value_get_boolean(value);
        if (!client)
        {
            GST_DEBUG_OBJECT(m_sink, "Enqueue sync off setting");
            m_isSyncOffQueued = true;
            return;
        }

        if (!client->setSyncOff(m_syncOff))
        {
            GST_ERROR_OBJECT(m_sink, "Could not set sync-off");
        }
        break;
    }
    case IPlaybackDelegate::Property::StreamSyncMode:
    {
        m_streamSyncMode = g_value_get_int(value);
        if (!client || !m_sourceAttached)
        {
            GST_DEBUG_OBJECT(m_sink, "Enqueue stream sync mode setting");
            m_isStreamSyncModeQueued = true;
            return;
        }

        if (!client->setStreamSyncMode(m_sourceId, m_streamSyncMode))
        {
            GST_ERROR_OBJECT(m_sink, "Could not set stream-sync-mode");
        }
        break;
    }
    case IPlaybackDelegate::Property::AudioFade:
    {
        const gchar *audioFadeStr = g_value_get_string(value);

        uint32_t fadeVolume = static_cast<uint32_t>(kDefaultVolume * 100);
        uint32_t duration = kDefaultVolumeDuration;
        char easeTypeChar = 'L';

        int parsedItems = sscanf(audioFadeStr, "%u,%u,%c", &fadeVolume, &duration, &easeTypeChar);

        if (parsedItems == 0)
        {
            GST_ERROR_OBJECT(m_sink, "Failed to parse any values from audio fade string: %s.", audioFadeStr);
            return;
        }
        else if (parsedItems == 1 || parsedItems == 2)
        {
            GST_WARNING_OBJECT(m_sink, "Partially parsed audio fade string: %s. Continuing with values: fadeVolume=%u, duration=%u, easeTypeChar=%c",
                               audioFadeStr, fadeVolume, duration, easeTypeChar);
        }

        if (fadeVolume > 100)
        {
            GST_WARNING_OBJECT(m_sink, "Fade volume is greater than 100. Setting it to 100.");
            fadeVolume = 100;
        }
        double volume = fadeVolume / 100.0;

        firebolt::rialto::EaseType easeType = convertCharToEaseType(easeTypeChar);

        {
            std::lock_guard<std::mutex> lock(m_audioFadeConfigMutex);
            m_audioFadeConfig.volume = volume;
            m_audioFadeConfig.duration = duration;
            m_audioFadeConfig.easeType = easeType;
        }

        if (!client)
        {
            GST_DEBUG_OBJECT(m_sink, "Enqueue audio fade setting");
            m_isAudioFadeQueued = true;
            return;
        }

        client->setVolume(volume, duration, easeType);
        break;
    }
    case IPlaybackDelegate::Property::LimitBufferingMs:
    {
        m_bufferingLimit = g_value_get_uint(value);
        if (!client)
        {
            GST_DEBUG_OBJECT(m_sink, "Enqueue buffering limit setting");
            m_isBufferingLimitQueued = true;
            return;
        }

        client->setBufferingLimit(m_bufferingLimit);
        break;
    }
    case IPlaybackDelegate::Property::UseBuffering:
    {
        m_useBuffering = g_value_get_boolean(value);
        if (!client)
        {
            GST_DEBUG_OBJECT(m_sink, "Enqueue use buffering setting");
            m_isUseBufferingQueued = true;
            return;
        }

        client->setUseBuffering(m_useBuffering);
        break;
    }
    case IPlaybackDelegate::Property::Async:
    {
        if (FALSE == g_value_get_boolean(value))
        {
            GST_WARNING_OBJECT(m_sink, "Cannot set ASYNC to false - not supported");
        }
        break;
    }
    default:
    {
        PullModePlaybackDelegate::setProperty(type, value);
        break;
    }
    }
}

void PullModeAudioPlaybackDelegate::handleQos(uint64_t processed, uint64_t dropped) const
{
    GstBus *bus = gst_element_get_bus(m_sink);
    /* Hardcode isLive to FALSE and set invalid timestamps */
    GstMessage *message = gst_message_new_qos(GST_OBJECT(m_sink), FALSE, GST_CLOCK_TIME_NONE, GST_CLOCK_TIME_NONE,
                                              GST_CLOCK_TIME_NONE, GST_CLOCK_TIME_NONE);
    gst_message_set_qos_stats(message, GST_FORMAT_DEFAULT, processed, dropped);
    gst_bus_post(bus, message);
    gst_object_unref(bus);
}

std::unique_ptr<firebolt::rialto::IMediaPipeline::MediaSource>
PullModeAudioPlaybackDelegate::createMediaSource(GstCaps *caps) const
{
    GstStructure *structure = gst_caps_get_structure(caps, 0);
    const gchar *structName = gst_structure_get_name(structure);

    firebolt::rialto::AudioConfig audioConfig{};
    firebolt::rialto::SegmentAlignment alignment{get_segment_alignment(structure)};
    std::shared_ptr<firebolt::rialto::CodecData> codecData{get_codec_data(structure)};
    firebolt::rialto::StreamFormat format{get_stream_format(structure)};

    if (structName)
    {
        std::string mimeType{};
        if (g_str_has_prefix(structName, "audio/mpeg") || g_str_has_prefix(structName, "audio/x-eac3") ||
            g_str_has_prefix(structName, "audio/x-ac3"))
        {
            gint sample_rate{0};
            gint number_of_channels{0};
            gst_structure_get_int(structure, "rate", &sample_rate);
            gst_structure_get_int(structure, "channels", &number_of_channels);

            audioConfig = firebolt::rialto::AudioConfig{static_cast<uint32_t>(number_of_channels),
                                                        static_cast<uint32_t>(sample_rate),
                                                        {}};

            if (g_str_has_prefix(structName, "audio/mpeg"))
            {
                gint mpegversion{0};
                gint layer{0};
                gst_structure_get_int(structure, "mpegversion", &mpegversion);
                gst_structure_get_int(structure, "layer", &layer);
                if (1 == mpegversion && 3 == layer)
                {
                    mimeType = "audio/mp3";
                }
                else
                {
                    mimeType = "audio/mp4";
                }
            }
            else
            {
                mimeType = "audio/x-eac3";
            }
        }
        else if (g_str_has_prefix(structName, "audio/x-opus"))
        {
            mimeType = "audio/x-opus";
            guint32 sampleRate{48000};
            guint8 numberOfChannels{}, streams{}, stereoStreams{}, channelMappingFamily{};
            guint8 channelMapping[256]{};
            guint16 preSkip{0};
            gint16 gain{0};
            if (gst_codec_utils_opus_parse_caps(caps, &sampleRate, &numberOfChannels, &channelMappingFamily, &streams,
                                                &stereoStreams, channelMapping))
            {
                GstBuffer *idHeader{};
                idHeader = gst_codec_utils_opus_create_header(sampleRate, numberOfChannels, channelMappingFamily,
                                                              streams, stereoStreams, channelMapping, preSkip, gain);
                std::vector<uint8_t> codecSpecificConfig{};
                GstMapInfo lsMap{};
                if (gst_buffer_map(idHeader, &lsMap, GST_MAP_READ))
                {
                    codecSpecificConfig.assign(lsMap.data, lsMap.data + lsMap.size);
                    gst_buffer_unmap(idHeader, &lsMap);
                }
                else
                {
                    GST_ERROR_OBJECT(m_sink, "Failed to read opus header details from a GstBuffer!");
                }
                gst_buffer_unref(idHeader);

                audioConfig = firebolt::rialto::AudioConfig{numberOfChannels, sampleRate, codecSpecificConfig};
            }
            else
            {
                GST_ERROR("Failed to parse opus caps!");
                return nullptr;
            }
        }
        else if (g_str_has_prefix(structName, "audio/b-wav") || g_str_has_prefix(structName, "audio/x-raw"))
        {
            gint sampleRate{0};
            gint numberOfChannels{0};
            std::optional<uint64_t> channelMask;
            gst_structure_get_int(structure, "rate", &sampleRate);
            gst_structure_get_int(structure, "channels", &numberOfChannels);
            std::optional<firebolt::rialto::Layout> layout =
                rialto_mse_sink_convert_layout(gst_structure_get_string(structure, "layout"));
            std::optional<firebolt::rialto::Format> rialtoFormat =
                rialto_mse_sink_convert_format(gst_structure_get_string(structure, "format"));
            const GValue *channelMaskValue = gst_structure_get_value(structure, "channel-mask");
            if (channelMaskValue)
            {
                channelMask = gst_value_get_bitmask(channelMaskValue);
            }

            if (g_str_has_prefix(structName, "audio/b-wav"))
            {
                mimeType = "audio/b-wav";
            }
            else
            {
                mimeType = "audio/x-raw";
            }

            audioConfig = firebolt::rialto::AudioConfig{static_cast<uint32_t>(numberOfChannels),
                                                        static_cast<uint32_t>(sampleRate),
                                                        {},
                                                        rialtoFormat,
                                                        layout,
                                                        channelMask};
        }
        else if (g_str_has_prefix(structName, "audio/x-flac"))
        {
            mimeType = "audio/x-flac";
            gint sampleRate{0};
            gint numberOfChannels{0};
            gst_structure_get_int(structure, "rate", &sampleRate);
            gst_structure_get_int(structure, "channels", &numberOfChannels);
            std::vector<std::vector<uint8_t>> streamHeaderVec;
            const GValue *streamheader = gst_structure_get_value(structure, "streamheader");
            if (streamheader)
            {
                for (guint i = 0; i < gst_value_array_get_size(streamheader); ++i)
                {
                    const GValue *headerValue = gst_value_array_get_value(streamheader, i);
                    GstBuffer *headerBuffer = gst_value_get_buffer(headerValue);
                    if (headerBuffer)
                    {
                        GstMappedBuffer mappedBuf(headerBuffer, GST_MAP_READ);
                        if (mappedBuf)
                        {
                            streamHeaderVec.push_back(
                                std::vector<std::uint8_t>(mappedBuf.data(), mappedBuf.data() + mappedBuf.size()));
                        }
                    }
                }
            }
            std::optional<bool> framed;
            gboolean framedValue{FALSE};
            if (gst_structure_get_boolean(structure, "framed", &framedValue))
            {
                framed = framedValue;
            }

            audioConfig = firebolt::rialto::AudioConfig{static_cast<uint32_t>(numberOfChannels),
                                                        static_cast<uint32_t>(sampleRate),
                                                        {},
                                                        std::nullopt,
                                                        std::nullopt,
                                                        std::nullopt,
                                                        streamHeaderVec,
                                                        framed};
        }
        else
        {
            GST_INFO_OBJECT(m_sink, "%s audio media source created", structName);
            mimeType = structName;
        }

        return std::make_unique<firebolt::rialto::IMediaPipeline::MediaSourceAudio>(mimeType, m_hasDrm, audioConfig,
                                                                                    alignment, format, codecData);
    }

    GST_ERROR_OBJECT(m_sink, "Empty caps' structure name! Failed to set mime type for audio media source.");
    return nullptr;
}