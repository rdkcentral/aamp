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

#include <algorithm>
#include <cstring>
#include <numeric>
#include <stdexcept>

#include <fcntl.h>
#include <sys/mman.h>
#include <syscall.h>
#include <unistd.h>

#include "RialtoServerLogging.h"
#include "SharedMemoryBuffer.h"
#include "TypeConverters.h"

#if !defined(SYS_memfd_create)
#if defined(__NR_memfd_create)
#define SYS_memfd_create __NR_memfd_create
#elif defined(__arm__)
#define SYS_memfd_create 385
#endif
#endif

#if !defined(MFD_CLOEXEC)
#define MFD_CLOEXEC 0x0001U
#endif

#if !defined(MFD_ALLOW_SEALING)
#define MFD_ALLOW_SEALING 0x0002U
#endif

#if !defined(F_ADD_SEALS)
#if !defined(F_LINUX_SPECIFIC_BASE)
#define F_LINUX_SPECIFIC_BASE 1024
#endif
#define F_ADD_SEALS (F_LINUX_SPECIFIC_BASE + 9)
#define F_GET_SEALS (F_LINUX_SPECIFIC_BASE + 10)

#define F_SEAL_SEAL 0x0001
#define F_SEAL_SHRINK 0x0002
#define F_SEAL_GROW 0x0004
#define F_SEAL_WRITE 0x0008
#endif

namespace
{
const char *kMemoryBufferName{"rialto_avbuf"};
constexpr int kNoIdAssigned{-1};
constexpr uint32_t kVideoRegionSize = 7 * 1024 * 1024; // 7MB
constexpr uint32_t kAudioRegionSize = 1 * 1024 * 1024; // 1MB
constexpr uint32_t kSubtitleRegionSize = 256 * 1024;   // 256kB
constexpr uint32_t kWebAudioRegionSize = 10 * 1024;    // 10KB

std::vector<firebolt::rialto::server::SharedMemoryBuffer::Partition>
calculatePartitionSize(firebolt::rialto::server::ISharedMemoryBuffer::MediaPlaybackType playbackType, int num)
{
    if (firebolt::rialto::server::ISharedMemoryBuffer::MediaPlaybackType::GENERIC == playbackType)
    {
        // As (for now) resolution of playback (for example HD or UHD) is not known, partitions have the same size.
        firebolt::rialto::server::SharedMemoryBuffer::Partition singlePlaybackDataBuffer{kNoIdAssigned, kAudioRegionSize,
                                                                                         kVideoRegionSize,
                                                                                         kSubtitleRegionSize};
        return std::vector<firebolt::rialto::server::SharedMemoryBuffer::Partition>(num, singlePlaybackDataBuffer);
    }
    else if (firebolt::rialto::server::ISharedMemoryBuffer::MediaPlaybackType::WEB_AUDIO == playbackType)
    {
        firebolt::rialto::server::SharedMemoryBuffer::Partition webAudioDataBuffer{kNoIdAssigned, kWebAudioRegionSize, 0};
        return std::vector<firebolt::rialto::server::SharedMemoryBuffer::Partition>(num, webAudioDataBuffer);
    }
    else
    {
        return std::vector<firebolt::rialto::server::SharedMemoryBuffer::Partition>();
    }
}

const char *toString(const firebolt::rialto::server::ISharedMemoryBuffer::MediaPlaybackType &type)
{
    switch (type)
    {
    case firebolt::rialto::server::ISharedMemoryBuffer::MediaPlaybackType::GENERIC:
    {
        return "GENERIC";
    }
    case firebolt::rialto::server::ISharedMemoryBuffer::MediaPlaybackType::WEB_AUDIO:
    {
        return "WEB_AUDIO";
    }
    }
    return "UNKNOWN";
}
} // namespace

namespace firebolt::rialto::server
{
std::unique_ptr<ISharedMemoryBufferFactory> ISharedMemoryBufferFactory::createFactory()
{
    return std::make_unique<SharedMemoryBufferFactory>();
}

std::shared_ptr<ISharedMemoryBuffer>
SharedMemoryBufferFactory::createSharedMemoryBuffer(unsigned numOfPlaybacks, unsigned numOfWebAudioPlayers) const
{
    return std::make_shared<SharedMemoryBuffer>(numOfPlaybacks, numOfWebAudioPlayers);
}

SharedMemoryBuffer::SharedMemoryBuffer(unsigned numOfPlaybacks, unsigned numOfWebAudioPlayers)
    : m_genericPartitions{calculatePartitionSize(MediaPlaybackType::GENERIC, numOfPlaybacks)},
      m_webAudioPartitions{calculatePartitionSize(MediaPlaybackType::WEB_AUDIO, numOfWebAudioPlayers)},
      m_dataBufferLen{0}, m_dataBufferFd{-1}, m_dataBuffer{nullptr}
{
    int fd = syscall(SYS_memfd_create, kMemoryBufferName, MFD_CLOEXEC | MFD_ALLOW_SEALING);
    if (fd < 0)
    {
        RIALTO_SERVER_LOG_SYS_ERROR(errno, "failed to create memory buffer");
    }
    else
    {
        const size_t kBufferSize{calculateBufferSize()};
        if (ftruncate(fd, static_cast<off_t>(kBufferSize)) == -1)
        {
            RIALTO_SERVER_LOG_SYS_ERROR(errno, "failed to resize memfd");
        }
        else if (fcntl(fd, F_ADD_SEALS, (F_SEAL_SEAL | F_SEAL_GROW | F_SEAL_SHRINK)) == -1)
        {
            RIALTO_SERVER_LOG_SYS_ERROR(errno, "failed to seal memfd");
        }
        else
        {
            void *addr = mmap(nullptr, kBufferSize, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
            if (addr != MAP_FAILED)
            {
                m_dataBufferLen = kBufferSize;
                m_dataBufferFd = fd;
                m_dataBuffer = reinterpret_cast<uint8_t *>(addr);
                RIALTO_SERVER_LOG_INFO("Shared Memory Buffer size: %d, ptr: %p", m_dataBufferLen, m_dataBuffer);
            }
        }

        if (m_dataBufferFd == -1)
        {
            if (close(fd) != 0)
                RIALTO_SERVER_LOG_SYS_ERROR(errno, "failed to close fd");
        }
    }

    if (m_dataBufferFd == -1)
    {
        RIALTO_SERVER_LOG_ERROR("Shared Memory Buffer initialization failed");
        throw std::runtime_error("Shared Memory Buffer initialization failed");
    }
}

SharedMemoryBuffer::~SharedMemoryBuffer()
{
    RIALTO_SERVER_LOG_INFO("Destroying Shared Memory Buffer");
    if (m_dataBufferFd != -1)
    {
        if (munmap(m_dataBuffer, m_dataBufferLen) != 0)
            RIALTO_SERVER_LOG_SYS_ERROR(errno, "failed to unmap buffer");
        if (close(m_dataBufferFd) != 0)
            RIALTO_SERVER_LOG_SYS_ERROR(errno, "failed to close data buffer fd");

        m_dataBufferLen = 0;
        m_dataBufferFd = -1;
        m_dataBuffer = nullptr;
    }
}

bool SharedMemoryBuffer::mapPartition(MediaPlaybackType playbackType, int id)
{
    std::vector<Partition> *partitions = getPlaybackTypePartition(playbackType);
    if (!partitions)
    {
        RIALTO_SERVER_LOG_ERROR("Cannot map the partition for playback type %s with id: %d", toString(playbackType), id);
        return false;
    }

    auto partition = std::find_if(partitions->begin(), partitions->end(), [id](const auto &p) { return p.id == id; });
    if (partition != partitions->end())
    {
        RIALTO_SERVER_LOG_DEBUG("Skip to map shm partition for id: %d. - partition already assigned", id);
        return true;
    }
    auto freePartition =
        std::find_if(partitions->begin(), partitions->end(), [](const auto &p) { return p.id == kNoIdAssigned; });
    if (freePartition == partitions->end())
    {
        RIALTO_SERVER_LOG_ERROR("Failed to map Shm partition for id: %d. No free partition available.", id);
        return false;
    }
    freePartition->id = id;
    return true;
}

bool SharedMemoryBuffer::unmapPartition(MediaPlaybackType playbackType, int id)
{
    std::vector<Partition> *partitions = getPlaybackTypePartition(playbackType);
    if (!partitions)
    {
        RIALTO_SERVER_LOG_ERROR("Cannot unmap the partition for playback type %s with id: %d", toString(playbackType),
                                id);
        return false;
    }

    auto partition = std::find_if(partitions->begin(), partitions->end(), [id](const auto &p) { return p.id == id; });
    if (partition == partitions->end())
    {
        RIALTO_SERVER_LOG_WARN("Failed to unmap Shm partition for id: %d. - partition could not be found", id);
        return false;
    }
    partition->id = kNoIdAssigned;
    return true;
}

bool SharedMemoryBuffer::clearData(MediaPlaybackType playbackType, int id, const MediaSourceType &mediaSourceType) const
{
    const std::vector<Partition> *kPartitions = getPlaybackTypePartition(playbackType);
    if (!kPartitions)
    {
        RIALTO_SERVER_LOG_ERROR("Cannot clear the %s data for playback type %s with id: %d",
                                common::convertMediaSourceType(mediaSourceType), toString(playbackType), id);
        return false;
    }

    auto partition = std::find_if(kPartitions->begin(), kPartitions->end(), [id](const auto &p) { return p.id == id; });
    if (partition == kPartitions->end())
    {
        RIALTO_SERVER_LOG_WARN("Failed to clear %s data for playback type %s with id: %d. - partition could not be "
                               "found",
                               common::convertMediaSourceType(mediaSourceType), toString(playbackType), id);
        return false;
    }
    std::uint8_t *partitionDataPtr = nullptr;
    if (!getDataPtrForPartition(playbackType, id, &partitionDataPtr))
    {
        RIALTO_SERVER_LOG_ERROR("Failed to clear %s data for playback type %s with id: %d. - could not get partition "
                                "data "
                                "ptr",
                                common::convertMediaSourceType(mediaSourceType), toString(playbackType), id);
        return false;
    }

    if (MediaSourceType::VIDEO == mediaSourceType)
    {
        std::uint8_t *videoData = partitionDataPtr;
        memset(videoData, 0x00, partition->dataBufferVideoLen);
        return true;
    }
    if (MediaSourceType::AUDIO == mediaSourceType)
    {
        std::uint8_t *audioData = partitionDataPtr + partition->dataBufferVideoLen;
        memset(audioData, 0x00, partition->dataBufferAudioLen);
        return true;
    }
    if (MediaSourceType::SUBTITLE == mediaSourceType)
    {
        std::uint8_t *subtitleoData = partitionDataPtr + partition->dataBufferVideoLen + partition->dataBufferAudioLen;
        memset(subtitleoData, 0x00, partition->dataBufferSubtitleLen);
        return true;
    }

    return false;
}

std::uint32_t SharedMemoryBuffer::getDataOffset(MediaPlaybackType playbackType, int id,
                                                const MediaSourceType &mediaSourceType) const
{
    std::uint8_t *buffer = getDataPtr(playbackType, id, mediaSourceType);
    if (!buffer)
    {
        throw std::runtime_error("Buffer not found for playback type " + std::string(toString(playbackType)) +
                                 " with id: " + std::to_string(id));
    }
    return buffer - m_dataBuffer;
}

std::uint32_t SharedMemoryBuffer::getMaxDataLen(MediaPlaybackType playbackType, int id,
                                                const MediaSourceType &mediaSourceType) const
{
    const std::vector<Partition> *kPartitions = getPlaybackTypePartition(playbackType);
    if (!kPartitions)
    {
        RIALTO_SERVER_LOG_ERROR("Cannot get the max data length for playback type %s with id: %d type: %s",
                                toString(playbackType), id, common::convertMediaSourceType(mediaSourceType));
        return false;
    }

    auto partition = std::find_if(kPartitions->begin(), kPartitions->end(), [id](const auto &p) { return p.id == id; });
    if (partition == kPartitions->end())
    {
        RIALTO_SERVER_LOG_WARN("Failed to get buffer length for playback type %s with id: %d. type: %s- partition "
                               "could not be "
                               "found",
                               toString(playbackType), id, common::convertMediaSourceType(mediaSourceType));
        return 0;
    }

    if (MediaSourceType::VIDEO == mediaSourceType)
    {
        return partition->dataBufferVideoLen;
    }
    if (MediaSourceType::AUDIO == mediaSourceType)
    {
        return partition->dataBufferAudioLen;
    }
    if (MediaSourceType::SUBTITLE == mediaSourceType)
    {
        return partition->dataBufferSubtitleLen;
    }

    return 0;
}

std::uint8_t *SharedMemoryBuffer::getDataPtr(MediaPlaybackType playbackType, int id,
                                             const MediaSourceType &mediaSourceType) const
{
    const std::vector<Partition> *kPartitions = getPlaybackTypePartition(playbackType);
    if (!kPartitions)
    {
        RIALTO_SERVER_LOG_ERROR("Cannot get the buffer offset for playback type %s with id: %d, type: %s",
                                toString(playbackType), id, common::convertMediaSourceType(mediaSourceType));
        return nullptr;
    }

    auto partition = std::find_if(kPartitions->begin(), kPartitions->end(), [id](const auto &p) { return p.id == id; });
    if (partition == kPartitions->end())
    {
        RIALTO_SERVER_LOG_WARN("Failed to get buffer offset for playback type %s with id: %d, type: %s. - partition "
                               "could not be "
                               "found",
                               toString(playbackType), id, common::convertMediaSourceType(mediaSourceType));
        return nullptr;
    }
    std::uint8_t *partitionDataPtr = nullptr;
    if (!getDataPtrForPartition(playbackType, id, &partitionDataPtr))
    {
        RIALTO_SERVER_LOG_ERROR("Failed to get buffer offset for playback type %s with id: %d. - could not get "
                                "partition data ptr",
                                toString(playbackType), id);
        return nullptr;
    }

    if ((MediaSourceType::VIDEO == mediaSourceType) && (0 != partition->dataBufferVideoLen))
    {
        return partitionDataPtr;
    }
    if ((MediaSourceType::AUDIO == mediaSourceType) && (0 != partition->dataBufferAudioLen))
    {
        return partitionDataPtr + partition->dataBufferVideoLen;
    }
    if ((MediaSourceType::SUBTITLE == mediaSourceType) && (0 != partition->dataBufferSubtitleLen))
    {
        return partitionDataPtr + partition->dataBufferVideoLen + partition->dataBufferAudioLen;
    }

    return nullptr;
}

int SharedMemoryBuffer::getFd() const
{
    return m_dataBufferFd;
}

std::uint32_t SharedMemoryBuffer::getSize() const
{
    return m_dataBufferLen;
}

std::uint8_t *SharedMemoryBuffer::getBuffer() const
{
    return m_dataBuffer;
}

size_t SharedMemoryBuffer::calculateBufferSize() const
{
    size_t genericSum =
        std::accumulate(m_genericPartitions.begin(), m_genericPartitions.end(), 0, [](size_t sum, const Partition &p)
                        { return sum + p.dataBufferAudioLen + p.dataBufferVideoLen + p.dataBufferSubtitleLen; });
    size_t webAudioSum = std::accumulate(m_webAudioPartitions.begin(), m_webAudioPartitions.end(), 0,
                                         [](size_t sum, const Partition &p)
                                         { return sum + p.dataBufferAudioLen + p.dataBufferVideoLen; });
    return genericSum + webAudioSum;
}

bool SharedMemoryBuffer::getDataPtrForPartition(MediaPlaybackType playbackType, int id, std::uint8_t **ptr) const
{
    std::uint8_t *result = m_dataBuffer;

    for (const auto &partition : m_genericPartitions)
    {
        if ((MediaPlaybackType::GENERIC == playbackType) && (partition.id == id))
        {
            *ptr = result;
            return true;
        }
        result += (partition.dataBufferVideoLen + partition.dataBufferAudioLen + partition.dataBufferSubtitleLen);
    }

    for (const auto &partition : m_webAudioPartitions)
    {
        if ((MediaPlaybackType::WEB_AUDIO == playbackType) && (partition.id == id))
        {
            *ptr = result;
            return true;
        }
        result += (partition.dataBufferVideoLen + partition.dataBufferAudioLen + partition.dataBufferSubtitleLen);
    }

    RIALTO_SERVER_LOG_ERROR("Could not find the data ptr for playback type %s with id: %d", toString(playbackType), id);
    return false;
}
const std::vector<SharedMemoryBuffer::Partition> *
SharedMemoryBuffer::getPlaybackTypePartition(MediaPlaybackType playbackType) const
{
    if (firebolt::rialto::server::ISharedMemoryBuffer::MediaPlaybackType::GENERIC == playbackType)
    {
        return &m_genericPartitions;
    }
    else if (firebolt::rialto::server::ISharedMemoryBuffer::MediaPlaybackType::WEB_AUDIO == playbackType)
    {
        return &m_webAudioPartitions;
    }
    else
    {
        RIALTO_SERVER_LOG_ERROR("Invalid playback type");
        return nullptr;
    }
}

std::vector<SharedMemoryBuffer::Partition> *SharedMemoryBuffer::getPlaybackTypePartition(MediaPlaybackType playbackType)
{
    return const_cast<std::vector<SharedMemoryBuffer::Partition> *>(
        const_cast<const SharedMemoryBuffer *>(this)->getPlaybackTypePartition(playbackType));
}
} // namespace firebolt::rialto::server
