/*
 
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

#include "BufferParser.h"
#include "GStreamerEMEUtils.h"
#include "GStreamerUtils.h"
#include "GstreamerCatLog.h"
#include <cstdint>
#include <cstring>
#include <gst/audio/audio.h>
#include <inttypes.h>

using namespace firebolt::rialto;
#define GST_CAT_DEFAULT rialtoGStreamerCat

std::unique_ptr<IMediaPipeline::MediaSegment> BufferParser::parseBuffer(const GstRefSample &sample, GstBuffer *buffer,
                                                                        GstMapInfo map, int streamId)
{
    int64_t timeStamp = static_cast<int64_t>(GST_BUFFER_PTS(buffer));
    int64_t duration = static_cast<int64_t>(GST_BUFFER_DURATION(buffer));
    GstCaps *caps = sample.getCaps();
    GstStructure *structure = gst_caps_get_structure(caps, 0);

    std::unique_ptr<IMediaPipeline::MediaSegment> mseData =
        parseSpecificPartOfBuffer(buffer, streamId, structure, timeStamp, duration);

    mseData->setData(map.size, map.data);

    addCodecDataToSegment(mseData, structure);
    addProtectionMetadataToSegment(mseData, buffer, map, structure);
    addDisplayOffsetToSegment(mseData, GST_BUFFER_OFFSET(buffer));

    return mseData;
}

void BufferParser::addProtectionMetadataToSegment(std::unique_ptr<IMediaPipeline::MediaSegment> &segment,
                                                  GstBuffer *buffer, const GstMapInfo &map, GstStructure *structure)
{
    EncryptionFormat encryptionFormat = EncryptionFormat::CLEAR;
    BufferProtectionMetadata metadata;
    ProcessProtectionMetadata(buffer, metadata);

    std::string mediaType = gst_structure_get_name(structure);
    if (mediaType == "application/x-cenc")
    {
        encryptionFormat = EncryptionFormat::CENC;
    }
    else if (mediaType == "application/x-webm-enc")
    {
        encryptionFormat = EncryptionFormat::WEBM;
    }

    // For WEBM encrypted sample without partitioning: add subsample which contains only encrypted data.
    // More details: https://www.webmproject.org/docs/webm-encryption/#45-full-sample-encrypted-block-format
    // For CENC see CENC specification, section 9.2
    if ((encryptionFormat == EncryptionFormat::WEBM) || (encryptionFormat == EncryptionFormat::CENC))
    {
        if ((metadata.encrypted) && (metadata.subsamples.size() == 0))
        {
            metadata.subsamples.push_back(std::make_pair((uint32_t)0, (uint32_t)map.size));
        }
    }

    if (metadata.encrypted)
    {
        GST_DEBUG("encrypted: %d mksId: %d key len: %zu iv len: %zu SUBSAMPLES: %zu, initWithLast15: %u",
                  metadata.encrypted, metadata.mediaKeySessionId, metadata.kid.size(), metadata.iv.size(),
                  metadata.subsamples.size(), metadata.initWithLast15);

        segment->setEncrypted(true);
        segment->setMediaKeySessionId(metadata.mediaKeySessionId);
        segment->setKeyId(metadata.kid);
        segment->setInitVector(metadata.iv);
        segment->setInitWithLast15(metadata.initWithLast15);
        segment->setCipherMode(metadata.cipherMode);
        if (metadata.encryptionPatternSet)
        {
            segment->setEncryptionPattern(metadata.cryptBlocks, metadata.skipBlocks);
        }

        size_t subSampleCount = metadata.subsamples.size();
        for (size_t subSampleIdx = 0; subSampleIdx < subSampleCount; ++subSampleIdx)
        {
            GST_DEBUG("SUBSAMPLE: %zu/%zu C: %d E: %d", subSampleIdx, subSampleCount,
                      metadata.subsamples[subSampleIdx].first, metadata.subsamples[subSampleIdx].second);
            segment->addSubSample(metadata.subsamples[subSampleIdx].first, metadata.subsamples[subSampleIdx].second);
        }
    }
}

void BufferParser::addCodecDataToSegment(std::unique_ptr<firebolt::rialto::IMediaPipeline::MediaSegment> &segment,
                                         GstStructure *structure)
{
    const GValue *codec_data;
    codec_data = gst_structure_get_value(structure, "codec_data");
    if (codec_data)
    {
        GstBuffer *buf = gst_value_get_buffer(codec_data);
        if (buf)
        {
            GstMappedBuffer mappedBuf(buf, GST_MAP_READ);
            if (mappedBuf)
            {
                auto codecData = std::make_shared<firebolt::rialto::CodecData>();
                codecData->data = std::vector<std::uint8_t>(mappedBuf.data(), mappedBuf.data() + mappedBuf.size());
                codecData->type = firebolt::rialto::CodecDataType::BUFFER;
                segment->setCodecData(codecData);
            }
            else
            {
                GST_ERROR("Failed to read codec_data");
            }
            return;
        }
        const gchar *str = g_value_get_string(codec_data);
        if (str)
        {
            auto codecData = std::make_shared<firebolt::rialto::CodecData>();
            codecData->data = std::vector<std::uint8_t>(str, str + std::strlen(str));
            codecData->type = firebolt::rialto::CodecDataType::STRING;
            segment->setCodecData(codecData);
        }
    }
}

void BufferParser::addDisplayOffsetToSegment(std::unique_ptr<firebolt::rialto::IMediaPipeline::MediaSegment> &segment,
                                             guint64 displayOffset)
{
    if (GST_BUFFER_OFFSET_NONE != displayOffset)
    {
        segment->setDisplayOffset(displayOffset);
    }
}

std::unique_ptr<IMediaPipeline::MediaSegment>
AudioBufferParser::parseSpecificPartOfBuffer(GstBuffer *buffer, int streamId, GstStructure *structure,
                                             int64_t timeStamp, int64_t duration)
{
    gint sampleRate = 0;
    gint numberOfChannels = 0;
    guint64 clippingStart = 0;
    guint64 clippingEnd = 0;
    gst_structure_get_int(structure, "rate", &sampleRate);
    gst_structure_get_int(structure, "channels", &numberOfChannels);

    const GstAudioClippingMeta *clippingMeta = gst_buffer_get_audio_clipping_meta(buffer);
    if (clippingMeta)
    {
        clippingStart = clippingMeta->start;
        clippingEnd = clippingMeta->end;
    }

    GST_LOG("New audio frame; buffer %p, pts=%" PRId64 " duration=%" PRId64
            " sampleRate=%d numberOfChannels=%d, clippingStart=%" PRIu64 ", clippingEnd=%" PRIu64,
            buffer, timeStamp, duration, sampleRate, numberOfChannels, clippingStart, clippingEnd);

    std::unique_ptr<IMediaPipeline::MediaSegmentAudio> mseData =
        std::make_unique<IMediaPipeline::MediaSegmentAudio>(streamId, timeStamp, duration, sampleRate, numberOfChannels,
                                                            clippingStart, clippingEnd);

    return mseData;
}

std::unique_ptr<IMediaPipeline::MediaSegment>
VideoBufferParser::parseSpecificPartOfBuffer(GstBuffer *buffer, int streamId, GstStructure *structure,
                                             int64_t timeStamp, int64_t duration)
{
    gint width = 0;
    gint height = 0;
    firebolt::rialto::Fraction frameRate{firebolt::rialto::kUndefinedSize, firebolt::rialto::kUndefinedSize};
    gst_structure_get_int(structure, "width", &width);
    gst_structure_get_int(structure, "height", &height);
    gst_structure_get_fraction(structure, "framerate", &frameRate.numerator, &frameRate.denominator);

    GST_LOG("New video frame; buffer %p, pts=%" PRId64 " duration=%" PRId64 " width=%d height=%d framerate=%d/%d",
            buffer, timeStamp, duration, width, height, frameRate.numerator, frameRate.denominator);

    std::unique_ptr<IMediaPipeline::MediaSegmentVideo> mseData =
        std::make_unique<IMediaPipeline::MediaSegmentVideo>(streamId, timeStamp, duration, width, height, frameRate);

    return mseData;
}

std::unique_ptr<IMediaPipeline::MediaSegment>
SubtitleBufferParser::parseSpecificPartOfBuffer(GstBuffer *buffer, int streamId, GstStructure *structure,
                                                int64_t timeStamp, int64_t duration)
{
    std::unique_ptr<IMediaPipeline::MediaSegment> mseData =
        std::make_unique<IMediaPipeline::MediaSegment>(streamId, MediaSourceType::SUBTITLE, timeStamp, duration);

    return mseData;
}
