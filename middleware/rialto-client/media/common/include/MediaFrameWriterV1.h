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

#ifndef FIREBOLT_RIALTO_COMMON_MEDIA_FRAME_WRITERV1_H_
#define FIREBOLT_RIALTO_COMMON_MEDIA_FRAME_WRITERV1_H_

#include "ByteWriter.h"
#include "IMediaFrameWriter.h"
#include <memory>
#include <vector>

namespace firebolt::rialto::common
{
/**
 * @brief The definition of the MediaFrameWriterV1.
 */
class MediaFrameWriterV1 : public IMediaFrameWriter
{
public:
    /**
     * @brief The constructor.
     *
     * @param[in] shmBuffer     : The shared buffer pointer.
     * @param[in] shmInfo       : The information for populating the shared memory.
     */
    MediaFrameWriterV1(uint8_t *shmBuffer, const std::shared_ptr<MediaPlayerShmInfo> &shminfo);

    /**
     * @brief Virtual destructor.
     */
    virtual ~MediaFrameWriterV1() {}

    /**
     * @brief Write the frame data.
     *
     * @param[in] data  : Media Segment data.
     *
     * @retval true on success.
     */
    AddSegmentStatus writeFrame(const std::unique_ptr<IMediaPipeline::MediaSegment> &data) override;

    /**
     * @brief Gets number of written frames
     *
     * @retval number of written frames
     */
    uint32_t getNumFrames() override { return m_numFrames; }

    /**
     * @brief The version of metadata this object shall write.
     */
    static const uint32_t m_kMetadataVersion = 1U;

    /**
     * @brief The size of the encryption metadata set if the frame is encrypted.
     */
    static const uint32_t m_kEncryptionMetadataSizeBytes = 32U;

private:
    /**
     * @brief Pointer to the shared memory buffer.
     */
    uint8_t *m_shmBuffer;

    /**
     * @brief The maximum amout of media data that can be written.
     */
    const uint32_t m_kMaxMediaBytes;

    /**
     * @brief The maximum amout of metadata the shared buffer can hold.
     */
    const uint32_t m_kMaxMetadataBytes;

    /**
     * @brief The amount of metadata bytes written to the shared buffer.
     */
    uint32_t m_metadataBytesWritten;

    /**
     * @brief The amount of media bytes written to the shared buffer.
     */
    uint32_t m_mediaBytesWritten;

    /**
     * @brief The offset of the shared memory to write the data.
     */
    uint32_t m_mediaDataOffset;

    /**
     * @brief The offset of the shared memory to write the metadata.
     */
    uint32_t m_metadataOffset;

    /**
     * @brief ByteWriter object.
     */
    ByteWriter m_bytewriter;

    uint32_t m_numFrames = 0;

    /**
     * @brief Write the generic frame meta data.
     *
     * @param[in] data  : Media Segment data.
     *
     * @retval true on success.
     */
    bool writeMetaDataGeneric(const std::unique_ptr<IMediaPipeline::MediaSegment> &data);

    /**
     * @brief Write the generic frame meta data.
     *
     * @param[in] data  : Media Segment data.
     *
     * @retval true on success.
     */
    bool writeMetaDataTypeSpecific(const std::unique_ptr<IMediaPipeline::MediaSegment> &data);

    /**
     * @brief Write the sample data.
     *
     * @param[in] data  : Media Segment data.
     *
     * @retval true on success.
     */
    bool writeData(const std::unique_ptr<IMediaPipeline::MediaSegment> &data);
};
}; // namespace firebolt::rialto::common

#endif // FIREBOLT_RIALTO_COMMON_MEDIA_FRAME_WRITERV1_H_
