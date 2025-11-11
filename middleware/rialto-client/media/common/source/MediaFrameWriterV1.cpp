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

#include "MediaFrameWriterV1.h"
#include "RialtoCommonLogging.h"

namespace firebolt::rialto::common
{
MediaFrameWriterV1::MediaFrameWriterV1(uint8_t *shmBuffer, const std::shared_ptr<MediaPlayerShmInfo> &shmInfo)
    : m_shmBuffer(shmBuffer), m_kMaxMediaBytes(shmInfo->maxMediaBytes), m_kMaxMetadataBytes(shmInfo->maxMetadataBytes),
      m_mediaBytesWritten(0U), m_mediaDataOffset(shmInfo->mediaDataOffset), m_metadataOffset(shmInfo->metadataOffset)
{
    RIALTO_COMMON_LOG_INFO("We are using a writer for Metadata V1");

    // Zero metadata memory
    m_bytewriter.fillBytes(m_shmBuffer, m_metadataOffset, 0, m_kMaxMetadataBytes);

    // Set metadata version
    m_metadataOffset = m_bytewriter.writeUint32(m_shmBuffer, m_metadataOffset, m_kMetadataVersion);

    // Track the amount of metadata bytes written
    m_metadataBytesWritten = sizeof(m_kMetadataVersion);
}

AddSegmentStatus MediaFrameWriterV1::writeFrame(const std::unique_ptr<IMediaPipeline::MediaSegment> &data)
{
    if (!writeMetaDataGeneric(data))
    {
        RIALTO_COMMON_LOG_ERROR("Failed to write metadata");
        return AddSegmentStatus::NO_SPACE;
    }
    if (!writeData(data))
    {
        RIALTO_COMMON_LOG_ERROR("Failed to write segment data");
        return AddSegmentStatus::NO_SPACE;
    }
    if (!writeMetaDataTypeSpecific(data))
    {
        return AddSegmentStatus::ERROR;
    }

    // Track the amount of metadata bytes written
    m_numFrames++;
    m_metadataBytesWritten += METADATA_V1_SIZE_PER_FRAME_BYTES;

    return AddSegmentStatus::OK;
}

bool MediaFrameWriterV1::writeMetaDataGeneric(const std::unique_ptr<IMediaPipeline::MediaSegment> &data)
{
    // Check that there is enough metadata space for the next frame
    if (m_metadataBytesWritten + METADATA_V1_SIZE_PER_FRAME_BYTES > m_kMaxMetadataBytes)
    {
        RIALTO_COMMON_LOG_ERROR("Not enough space to write metadata, max size %u, current size %u, frame metadata size "
                                "%u",
                                m_kMaxMetadataBytes, m_metadataBytesWritten, METADATA_V1_SIZE_PER_FRAME_BYTES);
        return false;
    }
    else
    {
        m_metadataOffset = m_bytewriter.writeUint32(m_shmBuffer, m_metadataOffset, m_mediaDataOffset);
        m_metadataOffset = m_bytewriter.writeUint32(m_shmBuffer, m_metadataOffset, data->getDataLength());
        m_metadataOffset = m_bytewriter.writeInt64(m_shmBuffer, m_metadataOffset, data->getTimeStamp());
        m_metadataOffset = m_bytewriter.writeInt64(m_shmBuffer, m_metadataOffset, data->getDuration());
        m_metadataOffset = m_bytewriter.writeUint32(m_shmBuffer, m_metadataOffset, static_cast<uint32_t>(data->getId()));
        m_metadataOffset =
            m_bytewriter.writeUint32(m_shmBuffer, m_metadataOffset, static_cast<uint32_t>(data->getExtraData().size()));

        if (0 != data->getExtraData().size())
        {
            m_metadataOffset = m_bytewriter.writeBytes(m_shmBuffer, m_metadataOffset,
                                                       const_cast<uint8_t *>(&(data->getExtraData()[0])),
                                                       data->getExtraData().size());
        }
        m_metadataOffset =
            m_bytewriter.fillBytes(m_shmBuffer, m_metadataOffset, 0, MAX_EXTRA_DATA_SIZE - data->getExtraData().size());

        // Not encrypted so skip the encrypted section of the metadata
        m_metadataOffset += m_kEncryptionMetadataSizeBytes;
    }

    return true;
}

bool MediaFrameWriterV1::writeMetaDataTypeSpecific(const std::unique_ptr<IMediaPipeline::MediaSegment> &data)
{
    if (MediaSourceType::AUDIO == data->getType())
    {
        IMediaPipeline::MediaSegmentAudio *audioSegment = dynamic_cast<IMediaPipeline::MediaSegmentAudio *>(data.get());
        if (audioSegment)
        {
            m_metadataOffset = m_bytewriter.writeUint32(m_shmBuffer, m_metadataOffset,
                                                        static_cast<uint32_t>(audioSegment->getSampleRate()));
            m_metadataOffset = m_bytewriter.writeUint32(m_shmBuffer, m_metadataOffset,
                                                        static_cast<uint32_t>(audioSegment->getNumberOfChannels()));
        }
        else
        {
            RIALTO_COMMON_LOG_ERROR("Failed to get the audio segment");
            return false;
        }
    }
    else if (MediaSourceType::VIDEO == data->getType())
    {
        IMediaPipeline::MediaSegmentVideo *videoSegment = dynamic_cast<IMediaPipeline::MediaSegmentVideo *>(data.get());
        if (videoSegment)
        {
            m_metadataOffset = m_bytewriter.writeUint32(m_shmBuffer, m_metadataOffset, videoSegment->getWidth());
            m_metadataOffset = m_bytewriter.writeUint32(m_shmBuffer, m_metadataOffset, videoSegment->getHeight());
        }
        else
        {
            RIALTO_COMMON_LOG_ERROR("Failed to get the video segment");
            return false;
        }
    }
    else
    {
        RIALTO_COMMON_LOG_ERROR("Failed to write type specific metadata - media source type not known");
        return false;
    }
    return true;
}

bool MediaFrameWriterV1::writeData(const std::unique_ptr<IMediaPipeline::MediaSegment> &data)
{
    if (m_mediaBytesWritten + data->getDataLength() > m_kMaxMediaBytes)
    {
        RIALTO_COMMON_LOG_ERROR("Not enough space to write media data, max size %u, current size %u, size to write %u",
                                m_kMaxMediaBytes, m_mediaBytesWritten, data->getDataLength());
        return false;
    }

    m_mediaDataOffset = m_bytewriter.writeBytes(m_shmBuffer, m_mediaDataOffset, data->getData(), data->getDataLength());

    // Track the amount of media bytes written
    m_mediaBytesWritten += data->getDataLength();

    return true;
}
}; // namespace firebolt::rialto::common
