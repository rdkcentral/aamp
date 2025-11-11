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

#include <limits.h>
#include <stdexcept>

#include "RialtoServerLogging.h"
#include "WebAudioPlayerServerInternal.h"

namespace
{
constexpr uint32_t kPreferredFrames{640};
constexpr std::chrono::milliseconds kWriteDataTimeMs{100};
} // namespace

namespace firebolt::rialto
{
std::shared_ptr<IWebAudioPlayerFactory> IWebAudioPlayerFactory::createFactory()
{
    return server::IWebAudioPlayerServerInternalFactory::createFactory();
}
}; // namespace firebolt::rialto

namespace firebolt::rialto::server
{
std::shared_ptr<IWebAudioPlayerServerInternalFactory> IWebAudioPlayerServerInternalFactory::createFactory()
{
    std::shared_ptr<IWebAudioPlayerServerInternalFactory> factory;

    try
    {
        factory = std::make_shared<server::WebAudioPlayerServerInternalFactory>();
    }
    catch (const std::exception &e)
    {
        RIALTO_SERVER_LOG_ERROR("Failed to create the web audio player factory, reason: %s", e.what());
    }

    return factory;
}

std::unique_ptr<IWebAudioPlayer>
WebAudioPlayerServerInternalFactory::createWebAudioPlayer(std::weak_ptr<IWebAudioPlayerClient> client,
                                                          const std::string &audioMimeType, const uint32_t priority,
                                                          std::weak_ptr<const WebAudioConfig> config) const
{
    RIALTO_SERVER_LOG_ERROR(
        "This function can't be used by rialto server. Please use createWebAudioPlayerServerInternal");
    return nullptr;
}

std::unique_ptr<IWebAudioPlayerServerInternal> WebAudioPlayerServerInternalFactory::createWebAudioPlayerServerInternal(
    std::weak_ptr<IWebAudioPlayerClient> client, const std::string &audioMimeType, const uint32_t priority,
    std::weak_ptr<const WebAudioConfig> config, const std::shared_ptr<ISharedMemoryBuffer> &shmBuffer, int handle,
    const std::shared_ptr<IMainThreadFactory> &mainThreadFactory,
    const std::shared_ptr<IGstWebAudioPlayerFactory> &gstPlayerFactory,
    std::weak_ptr<common::ITimerFactory> timerFactory) const
{
    std::unique_ptr<IWebAudioPlayerServerInternal> webAudioPlayer;
    try
    {
        webAudioPlayer = std::make_unique<server::WebAudioPlayerServerInternal>(client, audioMimeType, priority, config,
                                                                                shmBuffer, handle, mainThreadFactory,
                                                                                gstPlayerFactory, timerFactory);
    }
    catch (const std::exception &e)
    {
        RIALTO_SERVER_LOG_ERROR("Failed to create the web audio player, reason: %s", e.what());
    }

    return webAudioPlayer;
}

WebAudioPlayerServerInternal::WebAudioPlayerServerInternal(
    std::weak_ptr<IWebAudioPlayerClient> client, const std::string &audioMimeType, const uint32_t priority,
    std::weak_ptr<const WebAudioConfig> webAudioConfig, const std::shared_ptr<ISharedMemoryBuffer> &shmBuffer,
    int handle, const std::shared_ptr<IMainThreadFactory> &mainThreadFactory,
    const std::shared_ptr<IGstWebAudioPlayerFactory> &gstPlayerFactory, std::weak_ptr<common::ITimerFactory> timerFactory)
    : m_webAudioPlayerClient(client), m_shmBuffer{shmBuffer}, m_priority{priority}, m_shmId{handle}, m_shmPtr{nullptr},
      m_partitionOffset{0}, m_maxDataLength{0}, m_availableBuffer{}, m_expectWriteBuffer{false},
      m_timerFactory{timerFactory}, m_bytesPerFrame{0}, m_isEosRequested{false}
{
    RIALTO_SERVER_LOG_DEBUG("entry:");

    if (audioMimeType == "audio/x-raw")
    {
        std::shared_ptr<const WebAudioConfig> kConfig = webAudioConfig.lock();
        if (kConfig == nullptr)
        {
            throw std::runtime_error("Config is null for 'audio/x-raw'");
        }
        m_bytesPerFrame = kConfig->pcm.channels * (kConfig->pcm.sampleSize / CHAR_BIT);
        if (m_bytesPerFrame == 0)
        {
            throw std::runtime_error("Bytes per frame cannot be 0, channels " + std::to_string(kConfig->pcm.channels) +
                                     ", sampleSize " + std::to_string(kConfig->pcm.sampleSize));
        }
    }
    else
    {
        throw std::runtime_error("Mimetype '" + audioMimeType + "' not supported");
    }

    m_mainThread = mainThreadFactory->getMainThread();
    if (!m_mainThread)
    {
        throw std::runtime_error("Failed to get the main thread");
    }
    m_mainThreadClientId = m_mainThread->registerClient();

    bool result = false;
    auto task = [&]() { result = initWebAudioPlayerInternal(audioMimeType, webAudioConfig, gstPlayerFactory); };

    m_mainThread->enqueueTaskAndWait(m_mainThreadClientId, task);
    if (!result)
    {
        throw std::runtime_error("WebAudioPlayerServerInternal construction failed");
    }
}

bool WebAudioPlayerServerInternal::initWebAudioPlayerInternal(
    const std::string &audioMimeType, std::weak_ptr<const WebAudioConfig> config,
    const std::shared_ptr<IGstWebAudioPlayerFactory> &gstPlayerFactory)
{
    if (!m_shmBuffer->mapPartition(ISharedMemoryBuffer::MediaPlaybackType::WEB_AUDIO, m_shmId))
    {
        RIALTO_SERVER_LOG_ERROR("Unable to map shm partition");
        return false;
    }

    if (!(m_shmPtr = m_shmBuffer->getBuffer()))
    {
        RIALTO_SERVER_LOG_ERROR("Failed to get the data pointer for the shared memory");
        return false;
    }

    try
    {
        m_partitionOffset = m_shmBuffer->getDataOffset(ISharedMemoryBuffer::MediaPlaybackType::WEB_AUDIO, m_shmId,
                                                       MediaSourceType::AUDIO);
    }
    catch (const std::exception &e)
    {
        RIALTO_SERVER_LOG_ERROR("Failed to get the data pointer of the partition");
        return false;
    }

    if (!(m_maxDataLength = m_shmBuffer->getMaxDataLen(ISharedMemoryBuffer::MediaPlaybackType::WEB_AUDIO, m_shmId,
                                                       MediaSourceType::AUDIO)))
    {
        RIALTO_SERVER_LOG_ERROR("Failed to get the length of the partition");
        return false;
    }

    if (!initGstWebAudioPlayer(audioMimeType, config, gstPlayerFactory))
    {
        RIALTO_SERVER_LOG_ERROR("Failed to initalise the GstPlayer");
        return false;
    }

    // Set the available bytes
    m_availableBuffer.lengthMain = m_maxDataLength;
    m_availableBuffer.offsetMain = m_partitionOffset;
    m_availableBuffer.offsetWrap = m_partitionOffset;

    return true;
}

WebAudioPlayerServerInternal::~WebAudioPlayerServerInternal()
{
    RIALTO_SERVER_LOG_DEBUG("entry:");

    auto task = [&]()
    {
        if (m_writeDataTimer && (m_writeDataTimer->isActive()))
        {
            m_writeDataTimer->cancel();
        }

        if (!m_shmBuffer->unmapPartition(ISharedMemoryBuffer::MediaPlaybackType::WEB_AUDIO, m_shmId))
        {
            RIALTO_SERVER_LOG_ERROR("Unable to unmap shm partition");
        }

        m_shmBuffer.reset();
        m_mainThread->unregisterClient(m_mainThreadClientId);
    };
    m_mainThread->enqueueTaskAndWait(m_mainThreadClientId, task);
}

bool WebAudioPlayerServerInternal::play()
{
    RIALTO_SERVER_LOG_DEBUG("entry:");

    auto task = [&]() { m_gstPlayer->play(); };

    m_mainThread->enqueueTask(m_mainThreadClientId, task);

    return true;
}

bool WebAudioPlayerServerInternal::pause()
{
    RIALTO_SERVER_LOG_DEBUG("entry:");

    auto task = [&]() { m_gstPlayer->pause(); };

    m_mainThread->enqueueTask(m_mainThreadClientId, task);

    return true;
}

bool WebAudioPlayerServerInternal::setEos()
{
    RIALTO_SERVER_LOG_DEBUG("entry:");

    auto task = [&]()
    {
        if (0 != getQueuedFramesInShm())
        {
            m_isEosRequested = true;
        }
        else
        {
            m_gstPlayer->setEos();
        }
    };

    m_mainThread->enqueueTask(m_mainThreadClientId, task);

    return true;
}

bool WebAudioPlayerServerInternal::getBufferAvailable(uint32_t &availableFrames,
                                                      std::shared_ptr<WebAudioShmInfo> &webAudioShmInfo)
{
    RIALTO_SERVER_LOG_DEBUG("entry:");

    if (!webAudioShmInfo)
    {
        RIALTO_SERVER_LOG_ERROR("WebAudioShmInfo is null");
        return false;
    }

    auto task = [&]()
    {
        *webAudioShmInfo = m_availableBuffer;
        availableFrames = (m_availableBuffer.lengthMain + m_availableBuffer.lengthWrap) / m_bytesPerFrame;

        // A new getBufferAvailable shall overwrite the previous if writeBuffer is not called inbetween
        m_expectWriteBuffer = true;
    };

    m_mainThread->enqueueTaskAndWait(m_mainThreadClientId, task);

    return true;
}

bool WebAudioPlayerServerInternal::getBufferDelay(uint32_t &delayFrames)
{
    RIALTO_SERVER_LOG_DEBUG("entry:");

    bool status = false;
    auto task = [&]()
    {
        // Gstreamer returns a uint64, so check the value first
        uint64_t queuedFrames = (m_gstPlayer->getQueuedBytes() / m_bytesPerFrame) + getQueuedFramesInShm();
        if (queuedFrames > std::numeric_limits<uint32_t>::max())
        {
            RIALTO_SERVER_LOG_ERROR("Queued frames are larger than the max uint32_t");
        }
        else
        {
            delayFrames = static_cast<uint32_t>(queuedFrames);
            status = true;
        }
    };

    m_mainThread->enqueueTaskAndWait(m_mainThreadClientId, task);
    return status;
}

bool WebAudioPlayerServerInternal::writeBuffer(const uint32_t numberOfFrames, void *data)
{
    RIALTO_SERVER_LOG_DEBUG("entry:");

    bool result = false;
    auto task = [&]()
    {
        // data can be ignored, the frames should be written to the shared memory
        result = writeBufferInternal(numberOfFrames);
    };

    m_mainThread->enqueueTaskAndWait(m_mainThreadClientId, task);

    return result;
}

bool WebAudioPlayerServerInternal::writeBufferInternal(const uint32_t numberOfFrames)
{
    if (!m_expectWriteBuffer)
    {
        RIALTO_SERVER_LOG_ERROR("No getBufferAvailable to match with this writeBuffer call");
        return false;
    }
    m_expectWriteBuffer = false;

    // Cancel timer
    if (m_writeDataTimer && (m_writeDataTimer->isActive()))
    {
        m_writeDataTimer->cancel();
        m_writeDataTimer = nullptr;
    }

    // Write stored frames first
    uint32_t numberOfBytesToWrite = numberOfFrames * m_bytesPerFrame;
    if (!writeStoredBuffers())
    {
        // Update the available buffer with the new data that will not be written to gst
        updateAvailableBuffer(numberOfBytesToWrite, 0);
        m_writeDataTimer =
            m_timerFactory->createTimer(kWriteDataTimeMs,
                                        std::bind(&WebAudioPlayerServerInternal::handleWriteDataTimer, this));
        return true;
    }

    if (0 == numberOfFrames)
    {
        // No frames to write to gst
        return true;
    }

    // Write new frames
    uint8_t *mainPtr = m_shmPtr + m_availableBuffer.offsetMain;
    uint8_t *wrapPtr = m_shmPtr + m_availableBuffer.offsetWrap;
    uint32_t mainLength = 0;
    uint32_t wrapLength = 0;
    if (numberOfBytesToWrite <= m_availableBuffer.lengthMain)
    {
        mainLength = numberOfBytesToWrite;
    }
    else
    {
        mainLength = m_availableBuffer.lengthMain;
        wrapLength = numberOfBytesToWrite - mainLength;
    }

    uint32_t newBytesWritten = m_gstPlayer->writeBuffer(mainPtr, mainLength, wrapPtr, wrapLength);
    updateAvailableBuffer(numberOfBytesToWrite, newBytesWritten);
    if (newBytesWritten != numberOfBytesToWrite)
    {
        m_writeDataTimer =
            m_timerFactory->createTimer(kWriteDataTimeMs,
                                        std::bind(&WebAudioPlayerServerInternal::handleWriteDataTimer, this));
    }

    return true;
}

bool WebAudioPlayerServerInternal::writeStoredBuffers()
{
    uint8_t *mainPtr = nullptr;
    uint32_t mainLength = 0;
    uint8_t *wrapPtr = nullptr;
    uint32_t wrapLength = 0;

    if (m_availableBuffer.lengthWrap + m_availableBuffer.lengthMain == m_maxDataLength)
    {
        // No data stored
        return true;
    }
    else if (m_availableBuffer.lengthWrap != 0)
    {
        // Data stored in the shared memory has not wrapped, data only stored in the middle of the shared memory region
        uint32_t startOfDataOffset = m_availableBuffer.offsetWrap + m_availableBuffer.lengthWrap;
        mainPtr = m_shmPtr + startOfDataOffset;
        mainLength = m_availableBuffer.offsetMain - startOfDataOffset;
    }
    else
    {
        // Data stored in the shared memory has wrapped, data stored at the end and start of the shared memory region
        uint32_t startOfDataOffset = m_availableBuffer.offsetMain + m_availableBuffer.lengthMain;
        mainPtr = m_shmPtr + startOfDataOffset;
        mainLength = m_maxDataLength - (startOfDataOffset - m_partitionOffset);
        // Wrapped data stored at the start of the shared memory region up to where the availableBuffer starts
        wrapPtr = m_shmPtr + m_partitionOffset;
        wrapLength = m_availableBuffer.offsetMain - m_partitionOffset;
    }

    uint32_t storedBytesWritten = m_gstPlayer->writeBuffer(mainPtr, mainLength, wrapPtr, wrapLength);

    // Update available buffer with the bytes written to gst.
    // Bytes written to shm is 0 becuase they have already been handled.
    updateAvailableBuffer(0, storedBytesWritten);

    if (storedBytesWritten == mainLength + wrapLength)
    {
        // All data written to gstreamer
        if (m_isEosRequested)
        {
            m_gstPlayer->setEos();
            m_isEosRequested = false;
        }
        return true;
    }

    return false;
}

void WebAudioPlayerServerInternal::updateAvailableBuffer(uint32_t bytesWrittenToShm, uint32_t bytesWrittenToGst)
{
    if (bytesWrittenToShm <= m_availableBuffer.lengthMain)
    {
        // Data written to the shared memory has not wrapped
        uint32_t offetRelativeToPartition = m_availableBuffer.offsetMain - m_partitionOffset;
        uint32_t storedDataLengthAtEndOfShm = m_maxDataLength - (offetRelativeToPartition + m_availableBuffer.lengthMain);

        m_availableBuffer.offsetMain = m_availableBuffer.offsetMain + bytesWrittenToShm;
        if (bytesWrittenToGst <= storedDataLengthAtEndOfShm)
        {
            // Data written to gst has not wrapped, data written is taken from the main buffer only
            m_availableBuffer.lengthMain = m_availableBuffer.lengthMain - bytesWrittenToShm + bytesWrittenToGst;
        }
        else
        {
            // Data written to gst has wrapped, data written is taken from the main buffer and wrapped buffer
            m_availableBuffer.lengthMain = m_availableBuffer.lengthMain - bytesWrittenToShm + storedDataLengthAtEndOfShm;
            m_availableBuffer.lengthWrap = m_availableBuffer.lengthWrap +
                                           (bytesWrittenToGst - storedDataLengthAtEndOfShm);
        }
    }
    else
    {
        // Data written to the shared memory has wrapped, available buffer should now point to where the wrapped buffer was
        uint32_t newDataLengthAtEndOfShm = m_availableBuffer.lengthMain;

        m_availableBuffer.offsetMain = m_availableBuffer.offsetWrap + (bytesWrittenToShm - m_availableBuffer.lengthMain);
        uint32_t newOffetRelativeToPartition = m_availableBuffer.offsetMain - m_partitionOffset;
        if (bytesWrittenToGst <= m_availableBuffer.lengthMain)
        {
            // Data written to gstreamer has not wrapped, data written is taken from the main buffer only
            m_availableBuffer.lengthMain = (m_availableBuffer.lengthWrap - newOffetRelativeToPartition) +
                                           bytesWrittenToGst;
            m_availableBuffer.lengthWrap = 0;
        }
        else
        {
            // Data written to gstreamer has wrapped, data written is taken from the main buffer and wrapped buffer
            m_availableBuffer.lengthMain = m_maxDataLength - newOffetRelativeToPartition;
            m_availableBuffer.lengthWrap = bytesWrittenToGst - newDataLengthAtEndOfShm;
        }
    }
}

bool WebAudioPlayerServerInternal::getDeviceInfo(uint32_t &preferredFrames, uint32_t &maximumFrames,
                                                 bool &supportDeferredPlay)
{
    RIALTO_SERVER_LOG_DEBUG("entry:");

    // Can be called from any thread
    maximumFrames = m_maxDataLength / m_bytesPerFrame;
    preferredFrames = std::min(kPreferredFrames, maximumFrames);
    supportDeferredPlay = true;

    return true;
}

bool WebAudioPlayerServerInternal::setVolume(double volume)
{
    RIALTO_SERVER_LOG_DEBUG("entry:");

    auto task = [&, volume]() { m_gstPlayer->setVolume(volume); };

    m_mainThread->enqueueTask(m_mainThreadClientId, task);

    return true;
}

bool WebAudioPlayerServerInternal::getVolume(double &volume)
{
    RIALTO_SERVER_LOG_DEBUG("entry:");

    bool result = false;
    auto task = [&]() { result = m_gstPlayer->getVolume(volume); };

    m_mainThread->enqueueTaskAndWait(m_mainThreadClientId, task);

    return result;
}

std::weak_ptr<IWebAudioPlayerClient> WebAudioPlayerServerInternal::getClient()
{
    return m_webAudioPlayerClient;
}

void WebAudioPlayerServerInternal::notifyState(WebAudioPlayerState state)
{
    RIALTO_SERVER_LOG_DEBUG("entry:");

    auto task = [&, state]()
    {
        if (m_webAudioPlayerClient)
        {
            m_webAudioPlayerClient->notifyState(state);
        }
    };

    m_mainThread->enqueueTask(m_mainThreadClientId, task);
}

bool WebAudioPlayerServerInternal::initGstWebAudioPlayer(const std::string &audioMimeType,
                                                         std::weak_ptr<const WebAudioConfig> config,
                                                         const std::shared_ptr<IGstWebAudioPlayerFactory> &gstPlayerFactory)
{
    m_gstPlayer = gstPlayerFactory->createGstWebAudioPlayer(this, m_priority);
    if (!m_gstPlayer)
    {
        RIALTO_SERVER_LOG_ERROR("Failed to load gstreamer player");
        return false;
    }

    m_gstPlayer->setCaps(audioMimeType, config);

    return true;
}

void WebAudioPlayerServerInternal::handleWriteDataTimer()
{
    RIALTO_SERVER_LOG_DEBUG("entry:");

    auto task = [&]()
    {
        if (!writeStoredBuffers())
        {
            // Not all data was written to gstreamer, restart the timer
            m_writeDataTimer =
                m_timerFactory->createTimer(kWriteDataTimeMs,
                                            std::bind(&WebAudioPlayerServerInternal::handleWriteDataTimer, this));
        }
        else
        {
            m_writeDataTimer = nullptr;
        }
    };

    m_mainThread->enqueueTask(m_mainThreadClientId, task);
}

uint32_t WebAudioPlayerServerInternal::getQueuedFramesInShm()
{
    return (m_maxDataLength - (m_availableBuffer.lengthMain + m_availableBuffer.lengthWrap)) / m_bytesPerFrame;
}

void WebAudioPlayerServerInternal::ping(std::unique_ptr<IHeartbeatHandler> &&heartbeatHandler)
{
    RIALTO_SERVER_LOG_DEBUG("entry:");

    auto task = [&]() { m_gstPlayer->ping(std::move(heartbeatHandler)); };
    m_mainThread->enqueueTaskAndWait(m_mainThreadClientId, task);
}
}; // namespace firebolt::rialto::server
