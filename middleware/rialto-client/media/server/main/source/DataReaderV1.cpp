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

#include "DataReaderV1.h"
#include "RialtoServerLogging.h"
#include "ShmCommon.h"
#include "TypeConverters.h"

namespace firebolt::rialto::server
{
DataReaderV1::DataReaderV1(const MediaSourceType &mediaSourceType, std::uint8_t *buffer, std::uint32_t metadataOffset,
                           std::uint32_t numFrames)
    : m_mediaSourceType{mediaSourceType}, m_buffer{buffer}, m_metadataOffset{metadataOffset}, m_numFrames{numFrames}
{
    RIALTO_SERVER_LOG_DEBUG("Detected Metadata in Version 1. Media source type: %s",
                            common::convertMediaSourceType(m_mediaSourceType));
    static_assert(sizeof(DataReaderV1::MetadataV1) == common::METADATA_V1_SIZE_PER_FRAME_BYTES,
                  "Size of firebolt::rialto::server::DataReaderV1::MetadataV1 is not equal to "
                  "firebolt::rialto::common::METADATA_V1_SIZE_PER_FRAME_BYTES");
}

IMediaPipeline::MediaSegmentVector DataReaderV1::readData() const
{
    IMediaPipeline::MediaSegmentVector mediaSegments;
    auto metadatas = readMetadata();
    for (const auto &metadata : metadatas)
    {
        if (m_mediaSourceType == MediaSourceType::AUDIO)
        {
            mediaSegments.emplace_back(createSegment<IMediaPipeline::MediaSegmentAudio>(metadata));
        }
        else if (m_mediaSourceType == MediaSourceType::VIDEO)
        {
            mediaSegments.emplace_back(createSegment<IMediaPipeline::MediaSegmentVideo>(metadata));
        }
    }
    return mediaSegments;
}

std::vector<DataReaderV1::MetadataV1> DataReaderV1::readMetadata() const
{
    std::vector<DataReaderV1::MetadataV1> result;
    std::uint8_t *regionMetadataOffset = m_buffer + m_metadataOffset;
    for (std::uint32_t frame = 0; frame < m_numFrames; ++frame)
    {
        std::uint8_t *frameMetadataOffset = regionMetadataOffset + (frame * common::METADATA_V1_SIZE_PER_FRAME_BYTES);
        DataReaderV1::MetadataV1 *metadataPtr = reinterpret_cast<DataReaderV1::MetadataV1 *>(frameMetadataOffset);
        result.emplace_back(*metadataPtr);
    }
    return result;
}
} // namespace firebolt::rialto::server
