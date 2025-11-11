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

#ifndef FIREBOLT_RIALTO_COMMON_I_MEDIA_FRAME_WRITER_H_
#define FIREBOLT_RIALTO_COMMON_I_MEDIA_FRAME_WRITER_H_

#include <stdint.h>

#include <memory>
#include <string>

#include "IMediaPipeline.h"
#include <MediaCommon.h>
#include <ShmCommon.h>

namespace firebolt::rialto::common
{
class IMediaFrameWriter;

/**
 * @brief IMediaFrameWriter factory class, returns a concrete implementation of IMediaFrameWriter
 */
class IMediaFrameWriterFactory
{
public:
    IMediaFrameWriterFactory() = default;
    virtual ~IMediaFrameWriterFactory() = default;

    /**
     * @brief Gets the IMediaFrameWriterFactory instance.
     *
     * @retval the factory instance or null on error.
     */
    static std::shared_ptr<IMediaFrameWriterFactory> getFactory();

    /**
     * @brief Creates a IMediaFrameWriter object.
     *
     * @param[in] shmBuffer     : The shared buffer pointer.
     * @param[in] shmInfo       : The information for populating the shared memory.
     *
     * @retval the new media frame writer audio instance or null on error.
     */
    virtual std::unique_ptr<IMediaFrameWriter> createFrameWriter(uint8_t *shmBuffer,
                                                                 const std::shared_ptr<MediaPlayerShmInfo> &shminfo) = 0;
};

/**
 * @brief The definition of the IMediaFrameWriter interface.
 *
 * This interface defines the media frame writer APIs that are used to write data the shared memory.
 */
class IMediaFrameWriter
{
public:
    IMediaFrameWriter() = default;
    virtual ~IMediaFrameWriter() = default;

    IMediaFrameWriter(const IMediaFrameWriter &) = delete;
    IMediaFrameWriter &operator=(const IMediaFrameWriter &) = delete;
    IMediaFrameWriter(IMediaFrameWriter &&) = delete;
    IMediaFrameWriter &operator=(IMediaFrameWriter &&) = delete;

    /**
     * @brief Write the frame data.
     *
     * @param[in] data  : Media Segment data.
     *
     * @retval true on success.
     */
    virtual AddSegmentStatus writeFrame(const std::unique_ptr<IMediaPipeline::MediaSegment> &data) = 0;

    /**
     * @brief Gets number of written frames
     *
     * @retval number of written frames
     */
    virtual uint32_t getNumFrames() = 0;
};

}; // namespace firebolt::rialto::common

#endif // FIREBOLT_RIALTO_COMMON_I_MEDIA_FRAME_WRITER_H_
