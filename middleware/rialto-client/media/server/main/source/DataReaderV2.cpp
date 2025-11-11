/*
 * If not stated otherwise in this file or this component's LICENSE file the
 * following copyright and licenses apply:
 *
 * Copyright 2022 
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

#include "DataReaderV2.h"
#include "RialtoServerLogging.h"
#include "ShmCommon.h"
#include "TypeConverters.h"
#include "metadata.pb.h"

namespace
{
firebolt::rialto::SegmentAlignment
convertSegmentAlignment(const firebolt::rialto::MediaSegmentMetadata_SegmentAlignment &segmentAlignment)
{
    switch (segmentAlignment)
    {
    case firebolt::rialto::MediaSegmentMetadata_SegmentAlignment_ALIGNMENT_UNDEFINED:
    {
        return firebolt::rialto::SegmentAlignment::UNDEFINED;
    }
    case firebolt::rialto::MediaSegmentMetadata_SegmentAlignment_ALIGNMENT_NAL:
    {
        return firebolt::rialto::SegmentAlignment::NAL;
    }
    case firebolt::rialto::MediaSegmentMetadata_SegmentAlignment_ALIGNMENT_AU:
    {
        return firebolt::rialto::SegmentAlignment::AU;
    }
    }
    return firebolt::rialto::SegmentAlignment::UNDEFINED;
}

firebolt::rialto::CipherMode convertCipherMode(const firebolt::rialto::MediaSegmentMetadata_CipherMode &cipherMode)
{
    switch (cipherMode)
    {
    case firebolt::rialto::MediaSegmentMetadata_CipherMode_UNKNOWN:
    {
        return firebolt::rialto::CipherMode::UNKNOWN;
    }
    case firebolt::rialto::MediaSegmentMetadata_CipherMode_CENC:
    {
        return firebolt::rialto::CipherMode::CENC;
    }
    case firebolt::rialto::MediaSegmentMetadata_CipherMode_CBC1:
    {
        return firebolt::rialto::CipherMode::CBC1;
    }
    case firebolt::rialto::MediaSegmentMetadata_CipherMode_CENS:
    {
        return firebolt::rialto::CipherMode::CENS;
    }
    case firebolt::rialto::MediaSegmentMetadata_CipherMode_CBCS:
    {
        return firebolt::rialto::CipherMode::CBCS;
    }
    }
    return firebolt::rialto::CipherMode::UNKNOWN;
}

firebolt::rialto::CodecDataType convertCodecDataType(const firebolt::rialto::MediaSegmentMetadata_CodecData_Type &type)
{
    if (firebolt::rialto::MediaSegmentMetadata_CodecData_Type_STRING == type)
    {
        return firebolt::rialto::CodecDataType::STRING;
    }
    return firebolt::rialto::CodecDataType::BUFFER;
}

std::unique_ptr<firebolt::rialto::IMediaPipeline::MediaSegment>
createSegment(const firebolt::rialto::MediaSegmentMetadata &metadata, const firebolt::rialto::MediaSourceType &type)
{
    // Create segment
    std::unique_ptr<firebolt::rialto::IMediaPipeline::MediaSegment> segment;
    if (type == firebolt::rialto::MediaSourceType::AUDIO)
    {
        if (!metadata.has_sample_rate() || !metadata.has_channels_num())
        {
            RIALTO_SERVER_LOG_ERROR("SampleRate/ChannelsNum not present in audio metadata");
            return nullptr;
        }
        segment = std::make_unique<firebolt::rialto::IMediaPipeline::MediaSegmentAudio>(metadata.stream_id(),
                                                                                        metadata.time_position(),
                                                                                        metadata.sample_duration(),
                                                                                        metadata.sample_rate(),
                                                                                        metadata.channels_num(),
                                                                                        metadata.clipping_start(),
                                                                                        metadata.clipping_end());
    }
    else if (type == firebolt::rialto::MediaSourceType::VIDEO)
    {
        if (!metadata.has_width() || !metadata.has_height())
        {
            RIALTO_SERVER_LOG_ERROR("Width/height not present in video metadata");
            return nullptr;
        }

        firebolt::rialto::Fraction frameRate{firebolt::rialto::kUndefinedSize, firebolt::rialto::kUndefinedSize};
        if (metadata.has_frame_rate())
        {
            frameRate = {metadata.frame_rate().numerator(), metadata.frame_rate().denominator()};
        }
        segment = std::make_unique<firebolt::rialto::IMediaPipeline::MediaSegmentVideo>(metadata.stream_id(),
                                                                                        metadata.time_position(),
                                                                                        metadata.sample_duration(),
                                                                                        metadata.width(),
                                                                                        metadata.height(), frameRate);
    }
    else if (type == firebolt::rialto::MediaSourceType::SUBTITLE)
    {
        segment =
            std::make_unique<firebolt::rialto::IMediaPipeline::MediaSegment>(metadata.stream_id(),
                                                                             firebolt::rialto::MediaSourceType::SUBTITLE,
                                                                             metadata.time_position(),
                                                                             metadata.sample_duration());
    }
    else
    {
        RIALTO_SERVER_LOG_ERROR("Unknown segment type");
        return nullptr;
    }

    // Read optional data
    if (metadata.has_segment_alignment())
    {
        segment->setSegmentAlignment(convertSegmentAlignment(metadata.segment_alignment()));
    }
    if (metadata.has_extra_data())
    {
        segment->setExtraData(std::vector<uint8_t>(metadata.extra_data().begin(), metadata.extra_data().end()));
    }
    if (metadata.has_codec_data())
    {
        auto codecData = std::make_shared<firebolt::rialto::CodecData>();
        codecData->type = convertCodecDataType(metadata.codec_data().type());
        codecData->data = std::vector<uint8_t>(metadata.codec_data().data().begin(), metadata.codec_data().data().end());
        segment->setCodecData(codecData);
    }

    // Read encryption data
    if (metadata.has_media_key_session_id() || metadata.has_key_id() || metadata.has_init_vector() ||
        metadata.has_init_with_last_15())
    {
        segment->setEncrypted(true);
    }
    else
    {
        segment->setEncrypted(false);
    }
    if (metadata.has_media_key_session_id())
    {
        segment->setMediaKeySessionId(metadata.media_key_session_id());
    }
    if (metadata.has_key_id())
    {
        segment->setKeyId(std::vector<uint8_t>(metadata.key_id().begin(), metadata.key_id().end()));
    }
    if (metadata.has_init_vector())
    {
        segment->setInitVector(std::vector<uint8_t>(metadata.init_vector().begin(), metadata.init_vector().end()));
    }
    if (metadata.has_init_with_last_15())
    {
        segment->setInitWithLast15(metadata.init_with_last_15());
    }
    if (metadata.has_cipher_mode())
    {
        segment->setCipherMode(convertCipherMode(metadata.cipher_mode()));
    }
    if (metadata.has_crypt() && metadata.has_skip())
    {
        segment->setEncryptionPattern(metadata.crypt(), metadata.skip());
    }
    if (metadata.has_display_offset())
    {
        segment->setDisplayOffset(metadata.display_offset());
    }

    for (const auto &info : metadata.sub_sample_info())
    {
        segment->addSubSample(info.num_clear_bytes(), info.num_encrypted_bytes());
    }
    return segment;
}
} // namespace

namespace firebolt::rialto::server
{
DataReaderV2::DataReaderV2(const MediaSourceType &mediaSourceType, std::uint8_t *buffer, std::uint32_t dataOffset,
                           std::uint32_t numFrames)
    : m_mediaSourceType{mediaSourceType}, m_buffer{buffer}, m_dataOffset{dataOffset}, m_numFrames{numFrames}
{
    RIALTO_SERVER_LOG_DEBUG("Detected Metadata in Version 2. Media source type: %s",
                            common::convertMediaSourceType(m_mediaSourceType));
}

IMediaPipeline::MediaSegmentVector DataReaderV2::readData() const
{
    IMediaPipeline::MediaSegmentVector mediaSegments;
    uint8_t *currentReadPosition{m_buffer + m_dataOffset};
    for (auto i = 0U; i < m_numFrames; ++i)
    {
        std::uint32_t *metadataSize{reinterpret_cast<uint32_t *>(currentReadPosition)};
        currentReadPosition += sizeof(uint32_t);
        MediaSegmentMetadata metadata;
        if (!metadata.ParseFromArray(currentReadPosition, *metadataSize))
        {
            RIALTO_SERVER_LOG_ERROR("Metadata parsing failed!");
            return IMediaPipeline::MediaSegmentVector{};
        }
        auto newSegment{createSegment(metadata, m_mediaSourceType)};
        if (!newSegment)
        {
            RIALTO_SERVER_LOG_ERROR("Segment parsing failed!");
            return IMediaPipeline::MediaSegmentVector{};
        }
        currentReadPosition += *metadataSize;
        newSegment->setData(metadata.length(), currentReadPosition);
        currentReadPosition += metadata.length();
        mediaSegments.emplace_back(std::move(newSegment));
    }
    return mediaSegments;
}
} // namespace firebolt::rialto::server
