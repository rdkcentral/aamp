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

#ifndef FIREBOLT_RIALTO_COMMON_MEDIA_FRAME_WRITERV2_H_
#define FIREBOLT_RIALTO_COMMON_MEDIA_FRAME_WRITERV2_H_

#include "ByteWriter.h"
#include "IMediaFrameWriter.h"
#include "metadata.pb.h"
#include <memory>
#include <vector>

namespace firebolt::rialto::common
{
/**
 * @brief The definition of the MediaFrameWriterV2.
 */
class MediaFrameWriterV2 : public IMediaFrameWriter
{
public:
    /**
     * @brief The constructor.
     *
     * @param[in] shmBuffer     : The shared buffer pointer.
     * @param[in] shmInfo       : The information for populating the shared memory.
     */
    MediaFrameWriterV2(uint8_t *shmBuffer, const std::shared_ptr<MediaPlayerShmInfo> &shmInfo);

    /**
     * @brief Virtual destructor.
     */
    virtual ~MediaFrameWriterV2() = default;

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

private:
    /**
     * @brief Builds metadata proto object
     *
     * @param[in] data  : Media Segment data.
     *
     * @warning Method may throw!
     *
     * @retval MediaSegmentMetadata proto object
     */
    MediaSegmentMetadata buildMetadata(const std::unique_ptr<IMediaPipeline::MediaSegment> &data) const;

private:
    /**
     * @brief ByteWriter object.
     */
    ByteWriter m_byteWriter;

    /**
     * @brief Pointer to the shared memory buffer.
     */
    uint8_t *m_shmBuffer;

    /**
     * @brief The maximum amout of data that can be written.
     */
    const uint32_t m_kMaxBytes;

    /**
     * @brief The amount of media bytes written to the shared buffer.
     */
    uint32_t m_bytesWritten;

    /**
     * @brief The offset of the shared memory to write the data.
     */
    uint32_t m_dataOffset;

    /**
     * @brief Number of frames written.
     */
    uint32_t m_numFrames;
};
} // namespace firebolt::rialto::common

#endif // FIREBOLT_RIALTO_COMMON_MEDIA_FRAME_WRITERV2_H_
