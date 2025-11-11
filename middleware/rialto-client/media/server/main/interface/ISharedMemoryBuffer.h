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

#ifndef FIREBOLT_RIALTO_SERVER_I_SHARED_MEMORY_BUFFER_H_
#define FIREBOLT_RIALTO_SERVER_I_SHARED_MEMORY_BUFFER_H_

#include <cstdint>
#include <memory>

#include "MediaCommon.h"

namespace firebolt::rialto::server
{
class ISharedMemoryBuffer;
class ISharedMemoryBufferFactory
{
public:
    ISharedMemoryBufferFactory() = default;
    virtual ~ISharedMemoryBufferFactory() = default;

    static std::unique_ptr<ISharedMemoryBufferFactory> createFactory();
    virtual std::shared_ptr<ISharedMemoryBuffer> createSharedMemoryBuffer(unsigned numOfPlaybacks,
                                                                          unsigned numOfWebAudioPlayers) const = 0;
};

class ISharedMemoryBuffer
{
public:
    ISharedMemoryBuffer() = default;
    virtual ~ISharedMemoryBuffer() = default;

    ISharedMemoryBuffer(const ISharedMemoryBuffer &) = delete;
    ISharedMemoryBuffer(ISharedMemoryBuffer &&) = delete;
    ISharedMemoryBuffer &operator=(const ISharedMemoryBuffer &) = delete;
    ISharedMemoryBuffer &operator=(ISharedMemoryBuffer &&) = delete;

    /**
     * @brief The type of media playback.
     */
    enum class MediaPlaybackType
    {
        GENERIC,
        WEB_AUDIO
    };

    /**
     * @brief Maps the partition for playback.
     *
     * @param[in] playbackType  : The type of playback partition.
     * @param[in] id            : The id for the partition of playbackType.
     *
     * @retval true on success.
     */
    virtual bool mapPartition(MediaPlaybackType playbackType, int id) = 0;

    /**
     * @brief Unmaps the partition for playback.
     *
     * @param[in] playbackType  : The type of playback partition.
     * @param[in] id            : The id for the partition of playbackType.
     *
     * @retval true on success.
     */
    virtual bool unmapPartition(MediaPlaybackType playbackType, int id) = 0;

    /**
     * @brief Clears the data in the specified partition.
     *
     * @param[in] playbackType      : The type of playback partition.
     * @param[in] id                : The id for the partition of playbackType.
     * @param[in] mediaSourceType   : The type of media source partition.
     *
     * @retval true on success.
     */
    virtual bool clearData(MediaPlaybackType playbackType, int id, const MediaSourceType &mediaSourceType) const = 0;

    /**
     * @brief Gets the offset of the specified data partition.
     *
     * @param[in] playbackType      : The type of playback partition.
     * @param[in] id                : The id for the partition of playbackType.
     * @param[in] mediaSourceType   : The type of media source partition.
     *
     * @retval true on success.
     */
    virtual std::uint32_t getDataOffset(MediaPlaybackType playbackType, int id,
                                        const MediaSourceType &mediaSourceType) const = 0;

    /**
     * @brief Gets the maximum length of the specified data partition.
     *
     * @param[in] playbackType      : The type of playback partition.
     * @param[in] id                : The id for the partition of playbackType.
     * @param[in] mediaSourceType   : The type of media source partition.
     *
     * @retval true on success.
     */
    virtual std::uint32_t getMaxDataLen(MediaPlaybackType playbackType, int id,
                                        const MediaSourceType &mediaSourceType) const = 0;

    /**
     * @brief Gets the pointer to the start of the specified data partition.
     *
     * @param[in] playbackType      : The type of playback partition.
     * @param[in] id                : The id for the partition of playbackType.
     * @param[in] mediaSourceType   : The type of media source partition.
     *
     * @retval true on success.
     */
    virtual std::uint8_t *getDataPtr(MediaPlaybackType playbackType, int id,
                                     const MediaSourceType &mediaSourceType) const = 0;

    /**
     * @brief Gets file descriptor of the shared memory.
     *
     * @retval > -1 on success.
     */
    virtual int getFd() const = 0;

    /**
     * @brief Gets the allocated size of the shared memory.
     *
     * @retval > 0 on success.
     */
    virtual std::uint32_t getSize() const = 0;

    /**
     * @brief Gets the pointer to the shared memory.
     *
     * @retval None null ptr value on success.
     */
    virtual std::uint8_t *getBuffer() const = 0;
};
} // namespace firebolt::rialto::server

#endif // FIREBOLT_RIALTO_SERVER_I_SHARED_MEMORY_BUFFER_H_
