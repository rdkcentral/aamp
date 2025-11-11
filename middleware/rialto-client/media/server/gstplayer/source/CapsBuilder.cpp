/*
 * If not stated otherwise in this file or this component's LICENSE file the
 * following copyright and licenses apply:
 *
 * Copyright 2024 
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "CapsBuilder.h"
#include "GstMimeMapping.h"
#include "RialtoServerLogging.h"
#include "TypeConverters.h"

namespace firebolt::rialto::server
{
MediaSourceCapsBuilder::MediaSourceCapsBuilder(std::shared_ptr<firebolt::rialto::wrappers::IGstWrapper> gstWrapper,
                                               std::shared_ptr<firebolt::rialto::wrappers::IGlibWrapper> glibWrapper,
                                               const firebolt::rialto::IMediaPipeline::MediaSourceAV &source)
    : m_gstWrapper(gstWrapper), m_glibWrapper(glibWrapper), m_attachedSource(source)
{
}
GstCaps *MediaSourceCapsBuilder::buildCaps()
{
    return buildCommonCaps();
}

GstCaps *MediaSourceCapsBuilder::buildCommonCaps()
{
    GstCaps *caps = firebolt::rialto::server::createSimpleCapsFromMimeType(m_gstWrapper, m_attachedSource);

    addAlignmentToCaps(caps);
    addCodecDataToCaps(caps);
    addStreamFormatToCaps(caps);

    return caps;
}

void MediaSourceCapsBuilder::addAlignmentToCaps(GstCaps *caps) const
{
    static const std::unordered_map<firebolt::rialto::SegmentAlignment, std::string> aligmentMap =
        {{firebolt::rialto::SegmentAlignment::AU, "au"}, {firebolt::rialto::SegmentAlignment::NAL, "nal"}};

    auto aligmentMapIt = aligmentMap.find(m_attachedSource.getSegmentAlignment());
    if (aligmentMapIt != aligmentMap.end())
    {
        m_gstWrapper->gstCapsSetSimple(caps, "alignment", G_TYPE_STRING, aligmentMapIt->second.c_str(), nullptr);
    }
}

void MediaSourceCapsBuilder::addCodecDataToCaps(GstCaps *caps) const
{
    const std::shared_ptr<CodecData> &kCodecData = m_attachedSource.getCodecData();
    if (kCodecData && CodecDataType::BUFFER == kCodecData->type)
    {
        gpointer memory = m_glibWrapper->gMemdup(kCodecData->data.data(), kCodecData->data.size());
        GstBuffer *buf = m_gstWrapper->gstBufferNewWrapped(memory, kCodecData->data.size());
        m_gstWrapper->gstCapsSetSimple(caps, "codec_data", GST_TYPE_BUFFER, buf, nullptr);
        m_gstWrapper->gstBufferUnref(buf);
    }
    else if (kCodecData && CodecDataType::STRING == kCodecData->type)
    {
        std::string codecDataStr{kCodecData->data.begin(), kCodecData->data.end()};
        m_gstWrapper->gstCapsSetSimple(caps, "codec_data", G_TYPE_STRING, codecDataStr.c_str(), nullptr);
    }
}

void MediaSourceCapsBuilder::addStreamFormatToCaps(GstCaps *caps) const
{
    static const std::unordered_map<firebolt::rialto::StreamFormat, std::string> formatMap =
        {{firebolt::rialto::StreamFormat::RAW, "raw"},
         {firebolt::rialto::StreamFormat::AVC, "avc"},
         {firebolt::rialto::StreamFormat::BYTE_STREAM, "byte-stream"},
         {firebolt::rialto::StreamFormat::HVC1, "hvc1"},
         {firebolt::rialto::StreamFormat::HEV1, "hev1"}};

    auto formatMapIt = formatMap.find(m_attachedSource.getStreamFormat());
    if (formatMapIt != formatMap.end())
    {
        m_gstWrapper->gstCapsSetSimple(caps, "stream-format", G_TYPE_STRING, formatMapIt->second.c_str(), nullptr);
    }
}

MediaSourceAudioCapsBuilder::MediaSourceAudioCapsBuilder(
    std::shared_ptr<firebolt::rialto::wrappers::IGstWrapper> gstWrapper,
    std::shared_ptr<firebolt::rialto::wrappers::IGlibWrapper> glibWrapper, const IMediaPipeline::MediaSourceAudio &source)
    : MediaSourceCapsBuilder(gstWrapper, glibWrapper, source), m_attachedAudioSource(source)
{
}

GstCaps *MediaSourceAudioCapsBuilder::buildCaps()
{
    std::string mimeType = m_attachedSource.getMimeType();
    if (mimeType == "audio/x-opus")
    {
        return createOpusCaps();
    }

    GstCaps *caps = MediaSourceCapsBuilder::buildCaps();
    if (mimeType == "audio/mp4" || mimeType == "audio/aac")
    {
        addMpegVersion4ToCaps(caps);
    }
    else if (mimeType == "audio/mp3")
    {
        addMp3Caps(caps);
    }
    else if (mimeType == "audio/b-wav" || mimeType == "audio/x-raw")
    {
        addRawAudioData(caps);
    }
    else if (mimeType == "audio/x-flac")
    {
        addFlacSpecificData(caps);
    }
    addSampleRateAndChannelsToCaps(caps);

    return caps;
}

GstCaps *MediaSourceAudioCapsBuilder::createOpusCaps()
{
    GstCaps *caps = getAudioSpecificConfiguration();
    if (!caps)
    {
        caps = m_gstWrapper->gstCapsNewSimple("audio/x-opus", "channel-mapping-family", G_TYPE_INT, 0, nullptr);
        addSampleRateAndChannelsToCaps(caps);
    }

    return caps;
}

GstCaps *MediaSourceAudioCapsBuilder::getAudioSpecificConfiguration() const
{
    GstCaps *caps = nullptr;
    firebolt::rialto::AudioConfig audioConfig = m_attachedAudioSource.getAudioConfig();
    if (audioConfig.codecSpecificConfig.size())
    {
        gsize codec_priv_size = audioConfig.codecSpecificConfig.size();
        gconstpointer codec_priv = audioConfig.codecSpecificConfig.data();

        caps = m_gstWrapper->gstCodecUtilsOpusCreateCapsFromHeader(codec_priv, codec_priv_size);
        if (!caps)
        {
            RIALTO_SERVER_LOG_ERROR("Failed to parse opus header");
        }
    }

    return caps;
}

void MediaSourceAudioCapsBuilder::addSampleRateAndChannelsToCaps(GstCaps *caps) const
{
    firebolt::rialto::AudioConfig audioConfig = m_attachedAudioSource.getAudioConfig();

    if (audioConfig.numberOfChannels != firebolt::rialto::kInvalidAudioChannels)
        m_gstWrapper->gstCapsSetSimple(caps, "channels", G_TYPE_INT, audioConfig.numberOfChannels, nullptr);

    if (audioConfig.sampleRate != firebolt::rialto::kInvalidAudioSampleRate)
        m_gstWrapper->gstCapsSetSimple(caps, "rate", G_TYPE_INT, audioConfig.sampleRate, nullptr);
}

void MediaSourceAudioCapsBuilder::addMpegVersion4ToCaps(GstCaps *caps) const
{
    m_gstWrapper->gstCapsSetSimple(caps, "mpegversion", G_TYPE_INT, 4, nullptr);
}

void MediaSourceAudioCapsBuilder::addMp3Caps(GstCaps *caps) const
{
    m_gstWrapper->gstCapsSetSimple(caps, "mpegversion", G_TYPE_INT, 1, nullptr);
    m_gstWrapper->gstCapsSetSimple(caps, "layer", G_TYPE_INT, 3, nullptr);
}

void MediaSourceAudioCapsBuilder::addRawAudioData(GstCaps *caps) const
{
    firebolt::rialto::AudioConfig audioConfig = m_attachedAudioSource.getAudioConfig();
    if (audioConfig.format.has_value())
        m_gstWrapper->gstCapsSetSimple(caps, "format", G_TYPE_STRING, common::convertFormat(audioConfig.format.value()),
                                       nullptr);
    if (audioConfig.layout.has_value())
        m_gstWrapper->gstCapsSetSimple(caps, "layout", G_TYPE_STRING, common::convertLayout(audioConfig.layout.value()),
                                       nullptr);
    if (audioConfig.channelMask.has_value())
        m_gstWrapper->gstCapsSetSimple(caps, "channel-mask", GST_TYPE_BITMASK, audioConfig.channelMask.value(), nullptr);
}

void MediaSourceAudioCapsBuilder::addFlacSpecificData(GstCaps *caps) const
{
    firebolt::rialto::AudioConfig audioConfig = m_attachedAudioSource.getAudioConfig();
    if (audioConfig.streamHeader.size())
    {
        GValue streamHeaderArray = G_VALUE_INIT;
        m_glibWrapper->gValueInit(&streamHeaderArray, GST_TYPE_ARRAY);
        for (const auto &header : audioConfig.streamHeader)
        {
            gpointer memory = m_glibWrapper->gMemdup(header.data(), header.size());
            GstBuffer *buf = m_gstWrapper->gstBufferNewWrapped(memory, header.size());
            GST_BUFFER_FLAG_SET(buf, GST_BUFFER_FLAG_HEADER);

            GValue value = G_VALUE_INIT;
            m_glibWrapper->gValueInit(&value, GST_TYPE_BUFFER);
            m_gstWrapper->gstValueSetBuffer(&value, buf);
            m_gstWrapper->gstValueArrayAppendValue(&streamHeaderArray, &value);

            m_glibWrapper->gValueUnset(&value);
            m_gstWrapper->gstBufferUnref(buf);
        }
        m_gstWrapper->gstStructureSetValue(m_gstWrapper->gstCapsGetStructure(caps, 0), "streamheader",
                                           &streamHeaderArray);
        m_glibWrapper->gValueUnset(&streamHeaderArray);
    }
    if (audioConfig.framed.has_value())
    {
        m_gstWrapper->gstCapsSetSimple(caps, "framed", G_TYPE_BOOLEAN, audioConfig.framed.value(), nullptr);
    }
}

MediaSourceVideoCapsBuilder::MediaSourceVideoCapsBuilder(
    std::shared_ptr<firebolt::rialto::wrappers::IGstWrapper> gstWrapper,
    std::shared_ptr<firebolt::rialto::wrappers::IGlibWrapper> glibWrapper, const IMediaPipeline::MediaSourceVideo &source)
    : MediaSourceCapsBuilder(gstWrapper, glibWrapper, source), m_attachedVideoSource(source)
{
}

GstCaps *MediaSourceVideoCapsBuilder::buildCaps()
{
    GstCaps *caps = MediaSourceCapsBuilder::buildCaps();
    if (m_attachedVideoSource.getWidth() != firebolt::rialto::kUndefinedSize)
        m_gstWrapper->gstCapsSetSimple(caps, "width", G_TYPE_INT, m_attachedVideoSource.getWidth(), nullptr);
    if (m_attachedVideoSource.getHeight() != firebolt::rialto::kUndefinedSize)
        m_gstWrapper->gstCapsSetSimple(caps, "height", G_TYPE_INT, m_attachedVideoSource.getHeight(), nullptr);

    return caps;
}

MediaSourceVideoDolbyVisionCapsBuilder::MediaSourceVideoDolbyVisionCapsBuilder(
    std::shared_ptr<firebolt::rialto::wrappers::IGstWrapper> gstWrapper,
    std::shared_ptr<firebolt::rialto::wrappers::IGlibWrapper> glibWrapper,
    const IMediaPipeline::MediaSourceVideoDolbyVision &source)
    : MediaSourceVideoCapsBuilder(gstWrapper, glibWrapper, source), m_attachedDolbySource(source)
{
}

GstCaps *MediaSourceVideoDolbyVisionCapsBuilder::buildCaps()
{
    GstCaps *caps = MediaSourceVideoCapsBuilder::buildCaps();
    m_gstWrapper->gstCapsSetSimple(caps, "dovi-stream", G_TYPE_BOOLEAN, true, nullptr);
    m_gstWrapper->gstCapsSetSimple(caps, "dv_profile", G_TYPE_UINT, m_attachedDolbySource.getDolbyVisionProfile(),
                                   nullptr);
    return caps;
}
} // namespace firebolt::rialto::server
