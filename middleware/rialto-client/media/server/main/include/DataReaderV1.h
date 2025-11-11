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

#ifndef FIREBOLT_RIALTO_SERVER_DATA_READERV1_H_
#define FIREBOLT_RIALTO_SERVER_DATA_READERV1_H_

#include "IDataReader.h"
#include "MediaCommon.h"
#include <algorithm>
#include <cstdint>
#include <memory>
#include <utility>
#include <vector>

namespace firebolt::rialto::server
{
class DataReaderV1 : public IDataReader
{
    struct MetadataV1
    {
        std::uint32_t offset;        /* Offset of first byte of sample in CM buffer */
        std::uint32_t length;        /* Number of bytes in sample */
        std::int64_t timePosition;   /* Position in stream in nanoseconds */
        std::int64_t sampleDuration; /* Frame/sample duration in nanoseconds */
        std::uint32_t streamId;      /* stream id (unique ID for ES, as defined in attachSource()) */
        std::uint32_t extraDataSize; /* extraData size */
        std::uint8_t extraData[32];  /* buffer containing extradata */
        std::uint32_t mediaKeysId;   /* Identifier of MediaKeys instance to use for decryption. If 0 use any CDM
                                        containing the MKS ID */
        std::uint32_t mediaKeySessionIdentifierOffset; /* Offset to the location of the MediaKeySessionIdentifier */
        std::uint32_t mediaKeySessionIdentifierLength; /* Length of the MediaKeySessionIdentifier */
        std::uint32_t initVectorOffset;                /* Offset to the location of the initialization vector */
        std::uint32_t initVectorLength;                /* Length of initialization vector */
        std::uint32_t subSampleInfoOffset;             /* Offset to the location of the sub sample info table */
        std::uint32_t subSampleInfoLen;                /* Length of sub-sample Info table */
        std::uint32_t initWithLast15;
        std::uint32_t extra1; /* Samples per second for audio; Video width in pixels for video */
        std::uint32_t extra2; /* Number of channels for audio; Video height in pixels for video */
    };

public:
    DataReaderV1(const MediaSourceType &mediaSourceType, std::uint8_t *buffer, std::uint32_t metadataOffset,
                 std::uint32_t numFrames);
    ~DataReaderV1() override = default;

    IMediaPipeline::MediaSegmentVector readData() const override;

private:
    std::vector<MetadataV1> readMetadata() const;

private:
    MediaSourceType m_mediaSourceType;
    std::uint8_t *m_buffer;
    std::uint32_t m_metadataOffset;
    std::uint32_t m_numFrames;

    template <typename SegmentType> std::unique_ptr<SegmentType> createSegment(const MetadataV1 &metadata) const
    {
        std::unique_ptr<SegmentType> mediaSegment{std::make_unique<SegmentType>(0, metadata.timePosition,
                                                                                metadata.sampleDuration,
                                                                                metadata.extra1, metadata.extra2)};
        mediaSegment->setData(metadata.length, m_buffer + metadata.offset);
        std::vector<std::uint8_t> extraDataVec;
        std::copy(&metadata.extraData[0], &metadata.extraData[metadata.extraDataSize], std::back_inserter(extraDataVec));
        mediaSegment->setExtraData(extraDataVec);
        mediaSegment->setEncrypted(false); // temporary, only clear content playback should be supported now
        return mediaSegment;
    }
};
} // namespace firebolt::rialto::server

#endif // FIREBOLT_RIALTO_SERVER_DATA_READERV1_H_
