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

#ifndef FIREBOLT_RIALTO_SERVER_SHARED_MEMORY_BUFFER_H_
#define FIREBOLT_RIALTO_SERVER_SHARED_MEMORY_BUFFER_H_

#include "ISharedMemoryBuffer.h"
#include <cstdint>
#include <memory>
#include <vector>

namespace firebolt::rialto::server
{
class SharedMemoryBufferFactory : public ISharedMemoryBufferFactory
{
public:
    ~SharedMemoryBufferFactory() override = default;
    std::shared_ptr<ISharedMemoryBuffer> createSharedMemoryBuffer(unsigned numOfPlaybacks,
                                                                  unsigned numOfWebAudioPlayers) const override;
};

class SharedMemoryBuffer : public ISharedMemoryBuffer
{
public:
    explicit SharedMemoryBuffer(unsigned numOfPlaybacks, unsigned numOfWebAudioPlayers);
    ~SharedMemoryBuffer() override;
    SharedMemoryBuffer(const SharedMemoryBuffer &) = delete;
    SharedMemoryBuffer(SharedMemoryBuffer &&) = delete;
    SharedMemoryBuffer &operator=(const SharedMemoryBuffer &) = delete;
    SharedMemoryBuffer &operator=(SharedMemoryBuffer &&) = delete;

    bool mapPartition(MediaPlaybackType playbackType, int id) override;
    bool unmapPartition(MediaPlaybackType playbackType, int id) override;

    bool clearData(MediaPlaybackType playbackType, int id, const MediaSourceType &mediaSourceType) const override;

    std::uint32_t getDataOffset(MediaPlaybackType playbackType, int id,
                                const MediaSourceType &mediaSourceType) const override;
    std::uint32_t getMaxDataLen(MediaPlaybackType playbackType, int id,
                                const MediaSourceType &mediaSourceType) const override;
    std::uint8_t *getDataPtr(MediaPlaybackType playbackType, int id,
                             const MediaSourceType &mediaSourceType) const override;

    int getFd() const override;
    std::uint32_t getSize() const override;
    std::uint8_t *getBuffer() const override;

    struct Partition
    {
        int id;
        std::uint32_t dataBufferAudioLen;
        std::uint32_t dataBufferVideoLen;
        std::uint32_t dataBufferSubtitleLen;
    };

private:
    size_t calculateBufferSize() const;
    bool getDataPtrForPartition(MediaPlaybackType playbackType, int id, std::uint8_t **ptr) const;
    const std::vector<Partition> *getPlaybackTypePartition(MediaPlaybackType playbackType) const;
    std::vector<Partition> *getPlaybackTypePartition(MediaPlaybackType playbackType);

private:
    std::vector<Partition> m_genericPartitions;
    std::vector<Partition> m_webAudioPartitions;
    std::uint32_t m_dataBufferLen;
    int m_dataBufferFd;
    std::uint8_t *m_dataBuffer;
};
} // namespace firebolt::rialto::server

#endif // FIREBOLT_RIALTO_SERVER_SHARED_MEMORY_BUFFER_H_
