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
#include <stdexcept>

#include "ActiveRequests.h"
#include "DataReaderFactory.h"
#include "IDataReader.h"
#include "IRdkGstreamerUtilsWrapper.h"
#include "ISharedMemoryBuffer.h"
#include "MediaPipelineServerInternal.h"
#include "NeedMediaData.h"
#include "RialtoServerLogging.h"
#include "TypeConverters.h"

namespace
{
const char *toString(const firebolt::rialto::MediaSourceStatus &status)
{
    switch (status)
    {
    case firebolt::rialto::MediaSourceStatus::OK:
        return "OK";
    case firebolt::rialto::MediaSourceStatus::EOS:
        return "EOS";
    case firebolt::rialto::MediaSourceStatus::ERROR:
        return "ERROR";
    case firebolt::rialto::MediaSourceStatus::CODEC_CHANGED:
        return "CODEC_CHANGED";
    case firebolt::rialto::MediaSourceStatus::NO_AVAILABLE_SAMPLES:
        return "NO_AVAILABLE_SAMPLES";
    }
    return "Unknown";
}

std::int32_t generateSourceId()
{
    static std::int32_t sourceId{1};
    return sourceId++;
}
} // namespace

namespace firebolt::rialto
{
std::shared_ptr<IMediaPipelineFactory> IMediaPipelineFactory::createFactory()
{
    return server::MediaPipelineServerInternalFactory::createFactory();
}
}; // namespace firebolt::rialto

namespace firebolt::rialto::server
{
std::shared_ptr<server::IMediaPipelineServerInternalFactory> IMediaPipelineServerInternalFactory::createFactory()
{
    return MediaPipelineServerInternalFactory::createFactory();
}

std::shared_ptr<MediaPipelineServerInternalFactory> MediaPipelineServerInternalFactory::createFactory()
{
    std::shared_ptr<MediaPipelineServerInternalFactory> factory;

    try
    {
        factory = std::make_shared<MediaPipelineServerInternalFactory>();
    }
    catch (const std::exception &e)
    {
        RIALTO_SERVER_LOG_ERROR("Failed to create the media player server internal factory, reason: %s", e.what());
    }

    return factory;
}

std::unique_ptr<IMediaPipeline>
MediaPipelineServerInternalFactory::createMediaPipeline(std::weak_ptr<IMediaPipelineClient> client,
                                                        const VideoRequirements &videoRequirements) const
{
    RIALTO_SERVER_LOG_ERROR(
        "This function can't be used by rialto server. Please use createMediaPipelineServerInternal");
    return nullptr;
}

std::unique_ptr<server::IMediaPipelineServerInternal> MediaPipelineServerInternalFactory::createMediaPipelineServerInternal(
    std::weak_ptr<IMediaPipelineClient> client, const VideoRequirements &videoRequirements, int sessionId,
    const std::shared_ptr<ISharedMemoryBuffer> &shmBuffer, IDecryptionService &decryptionService) const
{
    std::shared_ptr<IMediaPipelineClient> sharedClient = client.lock();
    if (!sharedClient)
    {
        RIALTO_SERVER_LOG_ERROR("Couldn't create client's shared pointer");
        return nullptr;
    }

    std::unique_ptr<server::MediaPipelineServerInternal> mediaPipeline;
    try
    {
        mediaPipeline =
            std::make_unique<server::MediaPipelineServerInternal>(sharedClient, videoRequirements,
                                                                  server::IGstGenericPlayerFactory::getFactory(),
                                                                  sessionId, shmBuffer,
                                                                  server::IMainThreadFactory::createFactory(),
                                                                  common::ITimerFactory::getFactory(),
                                                                  std::make_unique<DataReaderFactory>(),
                                                                  std::make_unique<ActiveRequests>(), decryptionService);
    }
    catch (const std::exception &e)
    {
        RIALTO_SERVER_LOG_ERROR("Failed to create the media player server internal, reason: %s", e.what());
    }

    return mediaPipeline;
}

MediaPipelineServerInternal::MediaPipelineServerInternal(
    std::shared_ptr<IMediaPipelineClient> client, const VideoRequirements &videoRequirements,
    const std::shared_ptr<IGstGenericPlayerFactory> &gstPlayerFactory, int sessionId,
    const std::shared_ptr<ISharedMemoryBuffer> &shmBuffer, const std::shared_ptr<IMainThreadFactory> &mainThreadFactory,
    std::shared_ptr<common::ITimerFactory> timerFactory, std::unique_ptr<IDataReaderFactory> &&dataReaderFactory,
    std::unique_ptr<IActiveRequests> &&activeRequests, IDecryptionService &decryptionService)
    : m_mediaPipelineClient(client), m_kGstPlayerFactory(gstPlayerFactory), m_kVideoRequirements(videoRequirements),
      m_sessionId{sessionId}, m_shmBuffer{shmBuffer}, m_dataReaderFactory{std::move(dataReaderFactory)},
      m_timerFactory{timerFactory}, m_activeRequests{std::move(activeRequests)}, m_decryptionService{decryptionService},
      m_currentPlaybackState{PlaybackState::UNKNOWN}, m_wasAllSourcesAttachedCalled{false}
{
    RIALTO_SERVER_LOG_DEBUG("entry:");

    m_mainThread = mainThreadFactory->getMainThread();
    if (!m_mainThread)
    {
        throw std::runtime_error("Failed to get the main thread");
    }
    m_mainThreadClientId = m_mainThread->registerClient();

    bool result = false;
    auto task = [&]()
    {
        if (!m_shmBuffer->mapPartition(ISharedMemoryBuffer::MediaPlaybackType::GENERIC, m_sessionId))
        {
            RIALTO_SERVER_LOG_ERROR("Unable to map shm partition");
        }
        else
        {
            result = true;
        }
    };

    m_mainThread->enqueueTaskAndWait(m_mainThreadClientId, task);
    if (!result)
    {
        throw std::runtime_error("MediaPipelineServerInternal construction failed");
    }
}

MediaPipelineServerInternal::~MediaPipelineServerInternal()
{
    RIALTO_SERVER_LOG_DEBUG("entry:");

    auto task = [&]()
    {
        for (const auto &timer : m_needMediaDataTimers)
        {
            if (timer.second && timer.second->isActive())
            {
                timer.second->cancel();
            }
        }
        if (!m_shmBuffer->unmapPartition(ISharedMemoryBuffer::MediaPlaybackType::GENERIC, m_sessionId))
        {
            RIALTO_SERVER_LOG_ERROR("Unable to unmap shm partition");
        }

        m_shmBuffer.reset();
        m_mainThread->unregisterClient(m_mainThreadClientId);
    };
    m_mainThread->enqueueTaskAndWait(m_mainThreadClientId, task);
}

bool MediaPipelineServerInternal::load(MediaType type, const std::string &mimeType, const std::string &url)
{
    RIALTO_SERVER_LOG_DEBUG("entry:");

    bool result;
    auto task = [&]() { result = loadInternal(type, mimeType, url); };

    m_mainThread->enqueueTaskAndWait(m_mainThreadClientId, task);
    return result;
}

bool MediaPipelineServerInternal::loadInternal(MediaType type, const std::string &mimeType, const std::string &url)
{
    std::unique_lock lock{m_getPositionMutex};
    /* If gstreamer player already created, destroy the old one first */
    if (m_gstPlayer)
    {
        m_gstPlayer.reset();
    }

    m_gstPlayer =
        m_kGstPlayerFactory
            ->createGstGenericPlayer(this, m_decryptionService, type, m_kVideoRequirements,
                                     firebolt::rialto::wrappers::IRdkGstreamerUtilsWrapperFactory::getFactory());
    if (!m_gstPlayer)
    {
        RIALTO_SERVER_LOG_ERROR("Failed to load gstreamer player");
        return false;
    }

    notifyNetworkState(NetworkState::BUFFERING);

    return true;
}

bool MediaPipelineServerInternal::attachSource(const std::unique_ptr<MediaSource> &source)
{
    RIALTO_SERVER_LOG_DEBUG("entry:");

    bool result;
    auto task = [&]() { result = attachSourceInternal(source); };

    m_mainThread->enqueueTaskAndWait(m_mainThreadClientId, task);
    return result;
}

bool MediaPipelineServerInternal::attachSourceInternal(const std::unique_ptr<MediaSource> &source)
{
    source->setId(-1);

    if (!m_gstPlayer)
    {
        RIALTO_SERVER_LOG_ERROR("Gstreamer player has not been loaded");
        return false;
    }

    if (source->getType() == MediaSourceType::UNKNOWN)
    {
        RIALTO_SERVER_LOG_ERROR("Media source type unknown");
        return false;
    }

    m_gstPlayer->attachSource(source);

    const auto kSourceIter = m_attachedSources.find(source->getType());
    if (m_attachedSources.cend() == kSourceIter)
    {
        source->setId(generateSourceId());
        RIALTO_SERVER_LOG_DEBUG("New ID generated for MediaSourceType: %s: %d",
                                common::convertMediaSourceType(source->getType()), source->getId());
        m_attachedSources.emplace(source->getType(), source->getId());
    }
    else
    {
        RIALTO_SERVER_LOG_WARN("SourceType '%s' already attached", common::convertMediaSourceType(source->getType()));
        return false;
    }

    return true;
}

bool MediaPipelineServerInternal::removeSource(int32_t id)
{
    RIALTO_SERVER_LOG_DEBUG("entry:");

    bool result;
    auto task = [&]() { result = removeSourceInternal(id); };

    m_mainThread->enqueueTaskAndWait(m_mainThreadClientId, task);
    return result;
}

bool MediaPipelineServerInternal::removeSourceInternal(int32_t id)
{
    if (!m_gstPlayer)
    {
        RIALTO_SERVER_LOG_ERROR("Failed to remove source - Gstreamer player has not been loaded");
        return false;
    }
    auto sourceIter = std::find_if(m_attachedSources.begin(), m_attachedSources.end(),
                                   [id](const auto &src) { return src.second == id; });
    if (sourceIter == m_attachedSources.end())
    {
        RIALTO_SERVER_LOG_ERROR("Failed to remove source with id %d- Source not found", id);
        return false;
    }

    m_gstPlayer->removeSource(sourceIter->first);
    m_needMediaDataTimers.erase(sourceIter->first);
    m_attachedSources.erase(sourceIter);
    return true;
}

bool MediaPipelineServerInternal::allSourcesAttached()
{
    RIALTO_SERVER_LOG_DEBUG("entry:");

    bool result;
    auto task = [&]() { result = allSourcesAttachedInternal(); };

    m_mainThread->enqueueTaskAndWait(m_mainThreadClientId, task);
    return result;
}

bool MediaPipelineServerInternal::allSourcesAttachedInternal()
{
    if (!m_gstPlayer)
    {
        RIALTO_SERVER_LOG_ERROR("Failed to notify all sources attached - Gstreamer player has not been loaded");
        return false;
    }

    if (m_wasAllSourcesAttachedCalled)
    {
        RIALTO_SERVER_LOG_WARN("Failed to notify all sources attached - It was already called");
        return false;
    }

    m_gstPlayer->allSourcesAttached();
    m_wasAllSourcesAttachedCalled = true;
    return true;
}

bool MediaPipelineServerInternal::play()
{
    RIALTO_SERVER_LOG_DEBUG("entry:");

    bool result;
    auto task = [&]() { result = playInternal(); };

    m_mainThread->enqueueTaskAndWait(m_mainThreadClientId, task);
    return result;
}

bool MediaPipelineServerInternal::playInternal()
{
    if (!m_gstPlayer)
    {
        RIALTO_SERVER_LOG_ERROR("Failed to play - Gstreamer player has not been loaded");
        return false;
    }

    m_gstPlayer->play();
    return true;
}

bool MediaPipelineServerInternal::pause()
{
    RIALTO_SERVER_LOG_DEBUG("entry:");

    bool result;
    auto task = [&]() { result = pauseInternal(); };

    m_mainThread->enqueueTaskAndWait(m_mainThreadClientId, task);
    return result;
}

bool MediaPipelineServerInternal::pauseInternal()
{
    if (!m_gstPlayer)
    {
        RIALTO_SERVER_LOG_ERROR("Failed to pause - Gstreamer player has not been loaded");
        return false;
    }

    m_gstPlayer->pause();
    return true;
}

bool MediaPipelineServerInternal::stop()
{
    RIALTO_SERVER_LOG_DEBUG("entry:");

    bool result;
    auto task = [&]() { result = stopInternal(); };

    m_mainThread->enqueueTaskAndWait(m_mainThreadClientId, task);
    return result;
}

bool MediaPipelineServerInternal::stopInternal()
{
    if (!m_gstPlayer)
    {
        RIALTO_SERVER_LOG_ERROR("Failed to stop - Gstreamer player has not been loaded");
        return false;
    }

    m_gstPlayer->stop();
    return true;
}

bool MediaPipelineServerInternal::setPlaybackRate(double rate)
{
    RIALTO_SERVER_LOG_DEBUG("entry:");

    bool result;
    auto task = [&]() { result = setPlaybackRateInternal(rate); };

    m_mainThread->enqueueTaskAndWait(m_mainThreadClientId, task);
    return result;
}

bool MediaPipelineServerInternal::setPlaybackRateInternal(double rate)
{
    if (!m_gstPlayer)
    {
        RIALTO_SERVER_LOG_ERROR("Failed to set playback rate - Gstreamer player has not been loaded");
        return false;
    }

    if (0.0 == rate)
    {
        RIALTO_SERVER_LOG_ERROR("Failed to set playback rate to 0.0 - pause method should be used instead.");
        return false;
    }

    m_gstPlayer->setPlaybackRate(rate);
    return true;
}

bool MediaPipelineServerInternal::setPosition(int64_t position)
{
    RIALTO_SERVER_LOG_DEBUG("entry:");

    bool result;
    auto task = [&]() { result = setPositionInternal(position); };

    m_mainThread->enqueueTaskAndWait(m_mainThreadClientId, task);
    return result;
}

bool MediaPipelineServerInternal::setPositionInternal(int64_t position)
{
    if (!m_gstPlayer)
    {
        RIALTO_SERVER_LOG_ERROR("Failed to set position - Gstreamer player has not been loaded");
        return false;
    }

    m_gstPlayer->setPosition(position);

    // Reset Eos on seek
    for (auto &isMediaTypeEos : m_isMediaTypeEosMap)
    {
        isMediaTypeEos.second = false;
    }

    return true;
}

bool MediaPipelineServerInternal::getPosition(int64_t &position)
{
    RIALTO_SERVER_LOG_DEBUG("entry:");

    std::shared_lock lock{m_getPositionMutex};

    if (!m_gstPlayer)
    {
        RIALTO_SERVER_LOG_ERROR("Failed to get position - Gstreamer player has not been loaded");
        return false;
    }
    return m_gstPlayer->getPosition(position);
}

bool MediaPipelineServerInternal::getStats(int32_t sourceId, uint64_t &renderedFrames, uint64_t &droppedFrames)
{
    RIALTO_SERVER_LOG_DEBUG("entry:");

    bool result;
    auto task = [&]() { result = getStatsInternal(sourceId, renderedFrames, droppedFrames); };

    m_mainThread->enqueueTaskAndWait(m_mainThreadClientId, task);
    return result;
}

bool MediaPipelineServerInternal::getStatsInternal(int32_t sourceId, uint64_t &renderedFrames, uint64_t &droppedFrames)
{
    if (!m_gstPlayer)
    {
        RIALTO_SERVER_LOG_ERROR("Failed to get stats - Gstreamer player has not been loaded");
        return false;
    }
    auto sourceIter = std::find_if(m_attachedSources.begin(), m_attachedSources.end(),
                                   [sourceId](const auto &src) { return src.second == sourceId; });
    if (sourceIter == m_attachedSources.end())
    {
        RIALTO_SERVER_LOG_ERROR("Failed to get stats - Source not found");
        return false;
    }
    return m_gstPlayer->getStats(sourceIter->first, renderedFrames, droppedFrames);
}

bool MediaPipelineServerInternal::setImmediateOutput(int32_t sourceId, bool immediateOutput)
{
    RIALTO_SERVER_LOG_DEBUG("entry:");

    bool result;
    auto task = [&]() { result = setImmediateOutputInternal(sourceId, immediateOutput); };

    m_mainThread->enqueueTaskAndWait(m_mainThreadClientId, task);
    return result;
}

bool MediaPipelineServerInternal::setImmediateOutputInternal(int32_t sourceId, bool immediateOutput)
{
    if (!m_gstPlayer)
    {
        RIALTO_SERVER_LOG_ERROR("Failed - Gstreamer player has not been loaded");
        return false;
    }
    auto sourceIter = std::find_if(m_attachedSources.begin(), m_attachedSources.end(),
                                   [sourceId](const auto &src) { return src.second == sourceId; });
    if (sourceIter == m_attachedSources.end())
    {
        RIALTO_SERVER_LOG_ERROR("Failed - Source not found");
        return false;
    }

    m_IsLowLatencyVideoPlayer = immediateOutput;
    return m_gstPlayer->setImmediateOutput(sourceIter->first, immediateOutput);
}

bool MediaPipelineServerInternal::getImmediateOutput(int32_t sourceId, bool &immediateOutput)
{
    RIALTO_SERVER_LOG_DEBUG("entry:");

    bool result;
    auto task = [&]() { result = getImmediateOutputInternal(sourceId, immediateOutput); };

    m_mainThread->enqueueTaskAndWait(m_mainThreadClientId, task);
    return result;
}

bool MediaPipelineServerInternal::getImmediateOutputInternal(int32_t sourceId, bool &immediateOutput)
{
    if (!m_gstPlayer)
    {
        RIALTO_SERVER_LOG_ERROR("Failed - Gstreamer player has not been loaded");
        return false;
    }
    auto sourceIter = std::find_if(m_attachedSources.begin(), m_attachedSources.end(),
                                   [sourceId](const auto &src) { return src.second == sourceId; });
    if (sourceIter == m_attachedSources.end())
    {
        RIALTO_SERVER_LOG_ERROR("Failed - Source not found");
        return false;
    }
    return m_gstPlayer->getImmediateOutput(sourceIter->first, immediateOutput);
}

bool MediaPipelineServerInternal::setVideoWindow(uint32_t x, uint32_t y, uint32_t width, uint32_t height)
{
    RIALTO_SERVER_LOG_DEBUG("entry:");

    bool result;
    auto task = [&]() { result = setVideoWindowInternal(x, y, width, height); };

    m_mainThread->enqueueTaskAndWait(m_mainThreadClientId, task);
    return result;
}

bool MediaPipelineServerInternal::setVideoWindowInternal(uint32_t x, uint32_t y, uint32_t width, uint32_t height)
{
    if (!m_gstPlayer)
    {
        RIALTO_SERVER_LOG_ERROR("Failed to set video window - Gstreamer player has not been loaded");
        return false;
    }

    m_gstPlayer->setVideoGeometry(x, y, width, height);
    return true;
}

bool MediaPipelineServerInternal::haveData(MediaSourceStatus status, uint32_t needDataRequestId)
{
    RIALTO_SERVER_LOG_DEBUG("entry:");

    bool result;
    auto task = [&]() { result = haveDataInternal(status, needDataRequestId); };

    m_mainThread->enqueueTaskAndWait(m_mainThreadClientId, task);
    return result;
}

bool MediaPipelineServerInternal::haveDataInternal(MediaSourceStatus status, uint32_t needDataRequestId)
{
    if (!m_gstPlayer)
    {
        RIALTO_SERVER_LOG_ERROR("HaveData failed - Gstreamer player has not been loaded");
        return false;
    }

    MediaSourceType mediaSourceType = m_activeRequests->getType(needDataRequestId);
    if (MediaSourceType::UNKNOWN == mediaSourceType)
    {
        RIALTO_SERVER_LOG_WARN("NeedData RequestID is not valid: %u", needDataRequestId);
        return true;
    }

    unsigned int &counter = m_noAvailableSamplesCounter[mediaSourceType];
    if (status != MediaSourceStatus::OK && status != MediaSourceStatus::EOS)
    {
        // Incrementing the counter allows us to track the occurrences where the status is other than OK or EOS.

        ++counter;
        if (status == MediaSourceStatus::NO_AVAILABLE_SAMPLES)
        {
            RIALTO_SERVER_LOG_DEBUG("Data request for needDataRequestId: %u. NO_AVAILABLE_SAMPLES received: %u "
                                    "consecutively for mediaSourceType: %s",
                                    needDataRequestId, counter, common::convertMediaSourceType(mediaSourceType));
        }
        else
        {
            RIALTO_SERVER_LOG_WARN("%s Data request for needDataRequestId: %u received with wrong status: %s",
                                   common::convertMediaSourceType(mediaSourceType), needDataRequestId, toString(status));
            counter = 0;
        }

        m_activeRequests->erase(needDataRequestId);
        scheduleNotifyNeedMediaData(mediaSourceType);
        return true;
    }
    else
    {
        RIALTO_SERVER_LOG_DEBUG("%s Data request for needDataRequestId: %u received with correct status",
                                common::convertMediaSourceType(mediaSourceType), needDataRequestId);
        counter = 0;
    }

    try
    {
        const IMediaPipeline::MediaSegmentVector &kSegments = m_activeRequests->getSegments(needDataRequestId);
        m_gstPlayer->attachSamples(kSegments);
    }
    catch (const std::runtime_error &e)
    {
        RIALTO_SERVER_LOG_ERROR("Failed to get segments %s", e.what());
        m_activeRequests->erase(needDataRequestId);
        return false;
    }

    m_activeRequests->erase(needDataRequestId);
    if (status == MediaSourceStatus::EOS)
    {
        m_gstPlayer->setEos(mediaSourceType);
        m_isMediaTypeEosMap[mediaSourceType] = true;
    }

    return true;
}

bool MediaPipelineServerInternal::haveData(MediaSourceStatus status, uint32_t numFrames, uint32_t needDataRequestId)
{
    RIALTO_SERVER_LOG_DEBUG("entry:");

    bool result;
    auto task = [&]() { result = haveDataInternal(status, numFrames, needDataRequestId); };

    m_mainThread->enqueueTaskAndWait(m_mainThreadClientId, task);
    return result;
}

bool MediaPipelineServerInternal::haveDataInternal(MediaSourceStatus status, uint32_t numFrames,
                                                   uint32_t needDataRequestId)
{
    if (!m_gstPlayer)
    {
        RIALTO_SERVER_LOG_ERROR("HaveData failed - Gstreamer player has not been loaded");
        return false;
    }
    MediaSourceType mediaSourceType = m_activeRequests->getType(needDataRequestId);
    if (MediaSourceType::UNKNOWN == mediaSourceType)
    {
        RIALTO_SERVER_LOG_WARN("NeedData RequestID is not valid: %u", needDataRequestId);
        return true;
    }
    m_activeRequests->erase(needDataRequestId);

    unsigned int &counter = m_noAvailableSamplesCounter[mediaSourceType];
    if (status != MediaSourceStatus::OK && status != MediaSourceStatus::EOS)
    {
        // Incrementing the counter allows us to track the occurrences where the status is other than OK or EOS.

        ++counter;
        if (status == MediaSourceStatus::NO_AVAILABLE_SAMPLES)
        {
            RIALTO_SERVER_LOG_DEBUG("Data request for needDataRequestId: %u. NO_AVAILABLE_SAMPLES received: %u "
                                    "consecutively for mediaSourceType: %s",
                                    needDataRequestId, counter, common::convertMediaSourceType(mediaSourceType));
        }
        else
        {
            RIALTO_SERVER_LOG_WARN("%s Data request for needDataRequestId: %u received with wrong status",
                                   common::convertMediaSourceType(mediaSourceType), needDataRequestId);
            counter = 0;
        }
        scheduleNotifyNeedMediaData(mediaSourceType);
        return true;
    }
    else
    {
        RIALTO_SERVER_LOG_DEBUG("%s Data request for needDataRequestId: %u received with correct status",
                                common::convertMediaSourceType(mediaSourceType), needDataRequestId);
        counter = 0;
    }

    uint8_t *buffer = m_shmBuffer->getBuffer();
    if (!buffer)
    {
        RIALTO_SERVER_LOG_ERROR("No buffer available");
        notifyPlaybackState(PlaybackState::FAILURE);
        return false;
    }

    std::uint32_t regionOffset = 0;
    try
    {
        regionOffset =
            m_shmBuffer->getDataOffset(ISharedMemoryBuffer::MediaPlaybackType::GENERIC, m_sessionId, mediaSourceType);
    }
    catch (const std::runtime_error &e)
    {
        RIALTO_SERVER_LOG_ERROR("Failed to get region's buffer offset, reason: %s", e.what());
        notifyPlaybackState(PlaybackState::FAILURE);
        return false;
    }

    if (0 != numFrames)
    {
        std::shared_ptr<IDataReader> dataReader =
            m_dataReaderFactory->createDataReader(mediaSourceType, buffer, regionOffset, numFrames);
        if (!dataReader)
        {
            RIALTO_SERVER_LOG_ERROR("Metadata version not supported for %s request id: %u",
                                    common::convertMediaSourceType(mediaSourceType), needDataRequestId);
            notifyPlaybackState(PlaybackState::FAILURE);
            return false;
        }
        m_gstPlayer->attachSamples(dataReader);
    }
    if (status == MediaSourceStatus::EOS)
    {
        m_gstPlayer->setEos(mediaSourceType);
        m_isMediaTypeEosMap[mediaSourceType] = true;
    }

    return true;
}

void MediaPipelineServerInternal::ping(std::unique_ptr<IHeartbeatHandler> &&heartbeatHandler)
{
    RIALTO_SERVER_LOG_DEBUG("entry:");

    auto task = [&]() { pingInternal(std::move(heartbeatHandler)); };
    m_mainThread->enqueueTaskAndWait(m_mainThreadClientId, task);
}

void MediaPipelineServerInternal::pingInternal(std::unique_ptr<IHeartbeatHandler> &&heartbeatHandler)
{
    if (!m_gstPlayer)
    {
        // No need to check GstPlayer worker thread, we reached this function, so main thread is working fine.
        heartbeatHandler.reset();
        return;
    }
    // Check GstPlayer worker thread
    m_gstPlayer->ping(std::move(heartbeatHandler));
}

bool MediaPipelineServerInternal::renderFrame()
{
    RIALTO_SERVER_LOG_DEBUG("entry:");

    bool result;
    auto task = [&]() { result = renderFrameInternal(); };

    m_mainThread->enqueueTaskAndWait(m_mainThreadClientId, task);
    return result;
}

bool MediaPipelineServerInternal::renderFrameInternal()
{
    if (!m_gstPlayer)
    {
        RIALTO_SERVER_LOG_ERROR("renderFrame failed - Gstreamer player has not been loaded");
        return false;
    }

    m_gstPlayer->renderFrame();
    return true;
}

bool MediaPipelineServerInternal::setVolume(double targetVolume, uint32_t volumeDuration, EaseType easeType)
{
    RIALTO_SERVER_LOG_DEBUG("entry:");

    bool result;
    auto task = [&]() { result = setVolumeInternal(targetVolume, volumeDuration, easeType); };

    m_mainThread->enqueueTaskAndWait(m_mainThreadClientId, task);
    return result;
}

bool MediaPipelineServerInternal::setVolumeInternal(double targetVolume, uint32_t volumeDuration, EaseType easeType)
{
    RIALTO_SERVER_LOG_DEBUG("entry:");

    if (!m_gstPlayer)
    {
        RIALTO_SERVER_LOG_ERROR("Failed to set volume - Gstreamer player has not been loaded");
        return false;
    }
    m_gstPlayer->setVolume(targetVolume, volumeDuration, easeType);
    return true;
}

bool MediaPipelineServerInternal::getVolume(double &currentVolume)
{
    RIALTO_SERVER_LOG_DEBUG("entry:");

    bool result;
    auto task = [&]() { result = getVolumeInternal(currentVolume); };

    m_mainThread->enqueueTaskAndWait(m_mainThreadClientId, task);
    return result;
}

bool MediaPipelineServerInternal::getVolumeInternal(double &currentVolume)
{
    RIALTO_SERVER_LOG_DEBUG("entry:");

    if (!m_gstPlayer)
    {
        RIALTO_SERVER_LOG_ERROR("Failed to get volume - Gstreamer player has not been loaded");
        return false;
    }
    return m_gstPlayer->getVolume(currentVolume);
}

bool MediaPipelineServerInternal::setMute(std::int32_t sourceId, bool mute)
{
    RIALTO_SERVER_LOG_DEBUG("entry:");

    bool result;
    auto task = [&]() { result = setMuteInternal(sourceId, mute); };

    m_mainThread->enqueueTaskAndWait(m_mainThreadClientId, task);
    return result;
}

bool MediaPipelineServerInternal::setMuteInternal(std::int32_t sourceId, bool mute)
{
    RIALTO_SERVER_LOG_DEBUG("entry:");

    if (!m_gstPlayer)
    {
        RIALTO_SERVER_LOG_ERROR("Failed to set mute - Gstreamer player has not been loaded");
        return false;
    }

    auto sourceIter = std::find_if(m_attachedSources.begin(), m_attachedSources.end(),
                                   [sourceId](const auto &src) { return src.second == sourceId; });
    if (sourceIter == m_attachedSources.end())
    {
        RIALTO_SERVER_LOG_ERROR("Failed to set mute - Source with id: %d not found", sourceId);
        return false;
    }

    m_gstPlayer->setMute(sourceIter->first, mute);

    return true;
}

bool MediaPipelineServerInternal::getMute(std::int32_t sourceId, bool &mute)
{
    RIALTO_SERVER_LOG_DEBUG("entry:");

    bool result;
    auto task = [&]() { result = getMuteInternal(sourceId, mute); };

    m_mainThread->enqueueTaskAndWait(m_mainThreadClientId, task);
    return result;
}

bool MediaPipelineServerInternal::getMuteInternal(std::int32_t sourceId, bool &mute)
{
    RIALTO_SERVER_LOG_DEBUG("entry:");

    if (!m_gstPlayer)
    {
        RIALTO_SERVER_LOG_ERROR("Failed to get mute - Gstreamer player has not been loaded");
        return false;
    }

    auto sourceIter = std::find_if(m_attachedSources.begin(), m_attachedSources.end(),
                                   [sourceId](const auto &src) { return src.second == sourceId; });
    if (sourceIter == m_attachedSources.end())
    {
        RIALTO_SERVER_LOG_ERROR("Failed to get mute - Source with id: %d not found", sourceId);
        return false;
    }

    return m_gstPlayer->getMute(sourceIter->first, mute);
}

bool MediaPipelineServerInternal::setTextTrackIdentifier(const std::string &textTrackIdentifier)
{
    RIALTO_SERVER_LOG_DEBUG("entry:");

    bool result;
    auto task = [&]() { result = setTextTrackIdentifierInternal(textTrackIdentifier); };

    m_mainThread->enqueueTaskAndWait(m_mainThreadClientId, task);
    return result;
}

bool MediaPipelineServerInternal::setTextTrackIdentifierInternal(const std::string &textTrackIdentifier)
{
    RIALTO_SERVER_LOG_DEBUG("entry:");

    if (!m_gstPlayer)
    {
        RIALTO_SERVER_LOG_ERROR("Failed to set text track identifier - Gstreamer player has not been loaded");
        return false;
    }

    m_gstPlayer->setTextTrackIdentifier(textTrackIdentifier);

    return true;
}

bool MediaPipelineServerInternal::getTextTrackIdentifier(std::string &textTrackIdentifier)
{
    RIALTO_SERVER_LOG_DEBUG("entry:");

    bool result;
    auto task = [&]() { result = getTextTrackIdentifierInternal(textTrackIdentifier); };

    m_mainThread->enqueueTaskAndWait(m_mainThreadClientId, task);
    return result;
}

bool MediaPipelineServerInternal::getTextTrackIdentifierInternal(std::string &textTrackIdentifier)
{
    RIALTO_SERVER_LOG_DEBUG("entry:");

    if (!m_gstPlayer)
    {
        RIALTO_SERVER_LOG_ERROR("Failed to get mute - Gstreamer player has not been loaded");
        return false;
    }

    return m_gstPlayer->getTextTrackIdentifier(textTrackIdentifier);
}

bool MediaPipelineServerInternal::flush(int32_t sourceId, bool resetTime, bool &async)
{
    RIALTO_SERVER_LOG_DEBUG("entry:");

    bool result;
    auto task = [&]() { result = flushInternal(sourceId, resetTime, async); };

    m_mainThread->enqueueTaskAndWait(m_mainThreadClientId, task);
    return result;
}

bool MediaPipelineServerInternal::setLowLatency(bool lowLatency)
{
    RIALTO_SERVER_LOG_DEBUG("entry:");

    bool result;
    auto task = [&]() { result = setLowLatencyInternal(lowLatency); };

    m_mainThread->enqueueTaskAndWait(m_mainThreadClientId, task);
    return result;
}

bool MediaPipelineServerInternal::setLowLatencyInternal(bool lowLatency)
{
    RIALTO_SERVER_LOG_DEBUG("entry:");

    if (!m_gstPlayer)
    {
        RIALTO_SERVER_LOG_ERROR("Failed to set low latency - Gstreamer player has not been loaded");
        return false;
    }
    m_IsLowLatencyAudioPlayer = lowLatency;

    return m_gstPlayer->setLowLatency(lowLatency);
}

bool MediaPipelineServerInternal::setSync(bool sync)
{
    RIALTO_SERVER_LOG_DEBUG("entry:");

    bool result;
    auto task = [&]() { result = setSyncInternal(sync); };

    m_mainThread->enqueueTaskAndWait(m_mainThreadClientId, task);
    return result;
}

bool MediaPipelineServerInternal::setSyncInternal(bool sync)
{
    RIALTO_SERVER_LOG_DEBUG("entry:");

    if (!m_gstPlayer)
    {
        RIALTO_SERVER_LOG_ERROR("Failed to set sync - Gstreamer player has not been loaded");
        return false;
    }
    return m_gstPlayer->setSync(sync);
}

bool MediaPipelineServerInternal::getSync(bool &sync)
{
    RIALTO_SERVER_LOG_DEBUG("entry:");

    bool result;
    auto task = [&]() { result = getSyncInternal(sync); };

    m_mainThread->enqueueTaskAndWait(m_mainThreadClientId, task);
    return result;
}

bool MediaPipelineServerInternal::getSyncInternal(bool &sync)
{
    RIALTO_SERVER_LOG_DEBUG("entry:");

    if (!m_gstPlayer)
    {
        RIALTO_SERVER_LOG_ERROR("Failed to get sync - Gstreamer player has not been loaded");
        return false;
    }
    return m_gstPlayer->getSync(sync);
}

bool MediaPipelineServerInternal::setSyncOff(bool syncOff)
{
    RIALTO_SERVER_LOG_DEBUG("entry:");

    bool result;
    auto task = [&]() { result = setSyncOffInternal(syncOff); };

    m_mainThread->enqueueTaskAndWait(m_mainThreadClientId, task);
    return result;
}

bool MediaPipelineServerInternal::setSyncOffInternal(bool syncOff)
{
    RIALTO_SERVER_LOG_DEBUG("entry:");

    if (!m_gstPlayer)
    {
        RIALTO_SERVER_LOG_ERROR("Failed to set sync off - Gstreamer player has not been loaded");
        return false;
    }
    return m_gstPlayer->setSyncOff(syncOff);
}

bool MediaPipelineServerInternal::setStreamSyncMode(int32_t sourceId, int32_t streamSyncMode)
{
    RIALTO_SERVER_LOG_DEBUG("entry:");

    bool result;
    auto task = [&]() { result = setStreamSyncModeInternal(sourceId, streamSyncMode); };

    m_mainThread->enqueueTaskAndWait(m_mainThreadClientId, task);
    return result;
}

bool MediaPipelineServerInternal::setStreamSyncModeInternal(int32_t sourceId, int32_t streamSyncMode)
{
    RIALTO_SERVER_LOG_DEBUG("entry:");

    if (!m_gstPlayer)
    {
        RIALTO_SERVER_LOG_ERROR("Failed to set stream sync mode - Gstreamer player has not been loaded");
        return false;
    }

    auto sourceIter = std::find_if(m_attachedSources.begin(), m_attachedSources.end(),
                                   [sourceId](const auto &src) { return src.second == sourceId; });
    if (sourceIter == m_attachedSources.end())
    {
        RIALTO_SERVER_LOG_ERROR("Failed to set stream sync mode - Source with id: %d not found", sourceId);
        return false;
    }

    return m_gstPlayer->setStreamSyncMode(sourceIter->first, streamSyncMode);
}

bool MediaPipelineServerInternal::getStreamSyncMode(int32_t &streamSyncMode)
{
    RIALTO_SERVER_LOG_DEBUG("entry:");

    bool result;
    auto task = [&]() { result = getStreamSyncModeInternal(streamSyncMode); };

    m_mainThread->enqueueTaskAndWait(m_mainThreadClientId, task);
    return result;
}

bool MediaPipelineServerInternal::getStreamSyncModeInternal(int32_t &streamSyncMode)
{
    RIALTO_SERVER_LOG_DEBUG("entry:");

    if (!m_gstPlayer)
    {
        RIALTO_SERVER_LOG_ERROR("Failed to get stream sync mode - Gstreamer player has not been loaded");
        return false;
    }
    return m_gstPlayer->getStreamSyncMode(streamSyncMode);
}

bool MediaPipelineServerInternal::flushInternal(int32_t sourceId, bool resetTime, bool &async)
{
    if (!m_gstPlayer)
    {
        RIALTO_SERVER_LOG_ERROR("Failed to flush - Gstreamer player has not been loaded");
        return false;
    }
    auto sourceIter = std::find_if(m_attachedSources.begin(), m_attachedSources.end(),
                                   [sourceId](const auto &src) { return src.second == sourceId; });
    if (sourceIter == m_attachedSources.end())
    {
        RIALTO_SERVER_LOG_ERROR("Failed to flush - Source with id: %d not found", sourceId);
        return false;
    }

    m_gstPlayer->flush(sourceIter->first, resetTime, async);

    // Reset Eos on flush
    auto it = m_isMediaTypeEosMap.find(sourceIter->first);
    if (it != m_isMediaTypeEosMap.end() && it->second)
    {
        it->second = false;
    }

    return true;
}

bool MediaPipelineServerInternal::setSourcePosition(int32_t sourceId, int64_t position, bool resetTime,
                                                    double appliedRate, uint64_t stopPosition)
{
    RIALTO_SERVER_LOG_DEBUG("entry:");

    bool result;
    auto task = [&]() { result = setSourcePositionInternal(sourceId, position, resetTime, appliedRate, stopPosition); };

    m_mainThread->enqueueTaskAndWait(m_mainThreadClientId, task);
    return result;
}

bool MediaPipelineServerInternal::setSourcePositionInternal(int32_t sourceId, int64_t position, bool resetTime,
                                                            double appliedRate, uint64_t stopPosition)
{
    if (!m_gstPlayer)
    {
        RIALTO_SERVER_LOG_ERROR("Failed to set source position - Gstreamer player has not been loaded");
        return false;
    }
    auto sourceIter = std::find_if(m_attachedSources.begin(), m_attachedSources.end(),
                                   [sourceId](const auto &src) { return src.second == sourceId; });
    if (sourceIter == m_attachedSources.end())
    {
        RIALTO_SERVER_LOG_ERROR("Failed to set source position - Source with id: %d not found", sourceId);
        return false;
    }

    m_gstPlayer->setSourcePosition(sourceIter->first, position, resetTime, appliedRate, stopPosition);

    // Reset Eos on seek
    auto it = m_isMediaTypeEosMap.find(sourceIter->first);
    if (it != m_isMediaTypeEosMap.end() && it->second)
    {
        it->second = false;
    }

    return true;
}

bool MediaPipelineServerInternal::setSubtitleOffset(int32_t sourceId, int64_t position)
{
    RIALTO_SERVER_LOG_DEBUG("entry:");

    bool result;
    auto task = [&]() { result = setSubtitleOffsetInternal(sourceId, position); };

    m_mainThread->enqueueTaskAndWait(m_mainThreadClientId, task);
    return result;
}

bool MediaPipelineServerInternal::setSubtitleOffsetInternal(int32_t sourceId, int64_t position)
{
    if (!m_gstPlayer)
    {
        RIALTO_SERVER_LOG_ERROR("Failed to set subtitle offset - Gstreamer player has not been loaded");
        return false;
    }
    auto sourceIter = std::find_if(m_attachedSources.begin(), m_attachedSources.end(),
                                   [sourceId](const auto &src) { return src.second == sourceId; });
    if (sourceIter == m_attachedSources.end())
    {
        RIALTO_SERVER_LOG_ERROR("Failed to set subtitle offset - Source with id: %d not found", sourceId);
        return false;
    }

    m_gstPlayer->setSubtitleOffset(position);
    return true;
}

bool MediaPipelineServerInternal::processAudioGap(int64_t position, uint32_t duration, int64_t discontinuityGap,
                                                  bool audioAac)
{
    RIALTO_SERVER_LOG_DEBUG("entry:");

    bool result;
    auto task = [&]() { result = processAudioGapInternal(position, duration, discontinuityGap, audioAac); };

    m_mainThread->enqueueTaskAndWait(m_mainThreadClientId, task);
    return result;
}

bool MediaPipelineServerInternal::processAudioGapInternal(int64_t position, uint32_t duration, int64_t discontinuityGap,
                                                          bool audioAac)
{
    if (!m_gstPlayer)
    {
        RIALTO_SERVER_LOG_ERROR("Failed to process audio gap - Gstreamer player has not been loaded");
        return false;
    }
    m_gstPlayer->processAudioGap(position, duration, discontinuityGap, audioAac);
    return true;
}

bool MediaPipelineServerInternal::setBufferingLimit(uint32_t limitBufferingMs)
{
    bool result;
    auto task = [&]() { result = setBufferingLimitInternal(limitBufferingMs); };

    m_mainThread->enqueueTaskAndWait(m_mainThreadClientId, task);
    return result;
}

bool MediaPipelineServerInternal::setBufferingLimitInternal(uint32_t limitBufferingMs)
{
    if (!m_gstPlayer)
    {
        RIALTO_SERVER_LOG_ERROR("Failed to set buffering limit - Gstreamer player has not been loaded");
        return false;
    }
    m_gstPlayer->setBufferingLimit(limitBufferingMs);
    return true;
}

bool MediaPipelineServerInternal::getBufferingLimit(uint32_t &limitBufferingMs)
{
    bool result;
    auto task = [&]() { result = getBufferingLimitInternal(limitBufferingMs); };

    m_mainThread->enqueueTaskAndWait(m_mainThreadClientId, task);
    return result;
}

bool MediaPipelineServerInternal::getBufferingLimitInternal(uint32_t &limitBufferingMs)
{
    if (!m_gstPlayer)
    {
        RIALTO_SERVER_LOG_ERROR("Failed to get buffering limit - Gstreamer player has not been loaded");
        return false;
    }
    return m_gstPlayer->getBufferingLimit(limitBufferingMs);
}

bool MediaPipelineServerInternal::setUseBuffering(bool useBuffering)
{
    bool result;
    auto task = [&]() { result = setUseBufferingInternal(useBuffering); };

    m_mainThread->enqueueTaskAndWait(m_mainThreadClientId, task);
    return result;
}

bool MediaPipelineServerInternal::setUseBufferingInternal(bool useBuffering)
{
    if (!m_gstPlayer)
    {
        RIALTO_SERVER_LOG_ERROR("Failed to set use buffering - Gstreamer player has not been loaded");
        return false;
    }
    m_gstPlayer->setUseBuffering(useBuffering);
    return true;
}

bool MediaPipelineServerInternal::getUseBuffering(bool &useBuffering)
{
    bool result;
    auto task = [&]() { result = getUseBufferingInternal(useBuffering); };

    m_mainThread->enqueueTaskAndWait(m_mainThreadClientId, task);
    return result;
}

bool MediaPipelineServerInternal::getUseBufferingInternal(bool &useBuffering)
{
    if (!m_gstPlayer)
    {
        RIALTO_SERVER_LOG_ERROR("Failed to get use buffering - Gstreamer player has not been loaded");
        return false;
    }
    return m_gstPlayer->getUseBuffering(useBuffering);
}

bool MediaPipelineServerInternal::switchSource(const std::unique_ptr<MediaSource> &source)
{
    bool result;
    auto task = [&]() { result = switchSourceInternal(source); };

    m_mainThread->enqueueTaskAndWait(m_mainThreadClientId, task);
    return result;
}

bool MediaPipelineServerInternal::switchSourceInternal(const std::unique_ptr<MediaSource> &source)
{
    if (!m_gstPlayer)
    {
        RIALTO_SERVER_LOG_ERROR("Failed to switch source - Gstreamer player has not been loaded");
        return false;
    }
    m_gstPlayer->switchSource(source);
    return true;
}

AddSegmentStatus MediaPipelineServerInternal::addSegment(uint32_t needDataRequestId,
                                                         const std::unique_ptr<MediaSegment> &mediaSegment)
{
    RIALTO_SERVER_LOG_DEBUG("entry:");

    AddSegmentStatus status{AddSegmentStatus::ERROR};
    auto task = [&]() { status = addSegmentInternal(needDataRequestId, mediaSegment); };

    m_mainThread->enqueueTaskAndWait(m_mainThreadClientId, task);
    return status;
}

AddSegmentStatus MediaPipelineServerInternal::addSegmentInternal(uint32_t needDataRequestId,
                                                                 const std::unique_ptr<MediaSegment> &mediaSegment)
{
    AddSegmentStatus status = m_activeRequests->addSegment(needDataRequestId, mediaSegment);
    if (status != AddSegmentStatus::OK)
    {
        RIALTO_SERVER_LOG_ERROR("Failed to add segment for request id: %u", needDataRequestId);
    }

    return status;
}

std::weak_ptr<IMediaPipelineClient> MediaPipelineServerInternal::getClient()
{
    return m_mediaPipelineClient;
}

void MediaPipelineServerInternal::notifyPlaybackState(PlaybackState state)
{
    RIALTO_SERVER_LOG_DEBUG("entry:");

    if (m_mediaPipelineClient)
    {
        m_mediaPipelineClient->notifyPlaybackState(state);
    }
    auto task = [&, state]() { m_currentPlaybackState = state; };
    m_mainThread->enqueueTask(m_mainThreadClientId, task);
}

bool MediaPipelineServerInternal::notifyNeedMediaData(MediaSourceType mediaSourceType)
{
    RIALTO_SERVER_LOG_DEBUG("entry:");

    // the task won't execute for a disconnected client therefore
    // set a default value of true which will help to stop any further
    // action being taken
    bool result{true};

    auto task = [&]() { result = notifyNeedMediaDataInternal(mediaSourceType); };

    m_mainThread->enqueueTaskAndWait(m_mainThreadClientId, task);

    return result;
}

bool MediaPipelineServerInternal::notifyNeedMediaDataInternal(MediaSourceType mediaSourceType)
{
    m_needMediaDataTimers.erase(mediaSourceType);
    m_shmBuffer->clearData(ISharedMemoryBuffer::MediaPlaybackType::GENERIC, m_sessionId, mediaSourceType);
    const auto kSourceIter = m_attachedSources.find(mediaSourceType);

    if (m_attachedSources.cend() == kSourceIter)
    {
        RIALTO_SERVER_LOG_WARN("NeedMediaData event sending failed for %s - sourceId not found",
                               common::convertMediaSourceType(mediaSourceType));
        return false;
    }
    auto it = m_isMediaTypeEosMap.find(mediaSourceType);
    if (it != m_isMediaTypeEosMap.end() && it->second)
    {
        RIALTO_SERVER_LOG_INFO("EOS, NeedMediaData not needed for %s", common::convertMediaSourceType(mediaSourceType));
        return false;
    }
    NeedMediaData event{m_mediaPipelineClient, *m_activeRequests,   *m_shmBuffer,          m_sessionId,
                        mediaSourceType,       kSourceIter->second, m_currentPlaybackState};
    if (!event.send())
    {
        RIALTO_SERVER_LOG_WARN("NeedMediaData event sending failed for %s",
                               common::convertMediaSourceType(mediaSourceType));
        return false;
    }

    RIALTO_SERVER_LOG_DEBUG("%s NeedMediaData sent.", common::convertMediaSourceType(mediaSourceType));

    return true;
}

void MediaPipelineServerInternal::notifyPosition(std::int64_t position)
{
    RIALTO_SERVER_LOG_DEBUG("entry:");

    auto task = [&, position]()
    {
        if (m_mediaPipelineClient)
        {
            m_mediaPipelineClient->notifyPosition(position);
        }
    };

    m_mainThread->enqueueTask(m_mainThreadClientId, task);
}

void MediaPipelineServerInternal::notifyNetworkState(NetworkState state)
{
    RIALTO_SERVER_LOG_DEBUG("entry:");

    auto task = [&, state]()
    {
        if (m_mediaPipelineClient)
        {
            m_mediaPipelineClient->notifyNetworkState(state);
        }
    };

    m_mainThread->enqueueTask(m_mainThreadClientId, task);
}

void MediaPipelineServerInternal::clearActiveRequestsCache()
{
    RIALTO_SERVER_LOG_DEBUG("entry:");

    auto task = [&]() { m_activeRequests->clear(); };

    m_mainThread->enqueueTask(m_mainThreadClientId, task);
}

void MediaPipelineServerInternal::invalidateActiveRequests(const MediaSourceType &type)
{
    RIALTO_SERVER_LOG_DEBUG("entry:");

    auto task = [&, type]() { m_activeRequests->erase(type); };

    m_mainThread->enqueueTask(m_mainThreadClientId, task);
}

void MediaPipelineServerInternal::notifyQos(MediaSourceType mediaSourceType, const QosInfo &qosInfo)
{
    RIALTO_SERVER_LOG_DEBUG("entry:");

    auto task = [&, mediaSourceType, qosInfo]()
    {
        if (m_mediaPipelineClient)
        {
            const auto kSourceIter = m_attachedSources.find(mediaSourceType);
            if (m_attachedSources.cend() == kSourceIter)
            {
                RIALTO_SERVER_LOG_WARN("Qos notification failed - sourceId not found for %s",
                                       common::convertMediaSourceType(mediaSourceType));
                return;
            }
            m_mediaPipelineClient->notifyQos(kSourceIter->second, qosInfo);
        }
    };

    m_mainThread->enqueueTask(m_mainThreadClientId, task);
}

void MediaPipelineServerInternal::notifyBufferUnderflow(MediaSourceType mediaSourceType)
{
    RIALTO_SERVER_LOG_DEBUG("entry:");

    auto task = [&, mediaSourceType]()
    {
        if (m_mediaPipelineClient)
        {
            const auto kSourceIter = m_attachedSources.find(mediaSourceType);
            if (m_attachedSources.cend() == kSourceIter)
            {
                RIALTO_SERVER_LOG_WARN("Buffer underflow notification failed - sourceId not found for %s",
                                       common::convertMediaSourceType(mediaSourceType));
                return;
            }
            m_mediaPipelineClient->notifyBufferUnderflow(kSourceIter->second);
        }
    };

    m_mainThread->enqueueTask(m_mainThreadClientId, task);
}

void MediaPipelineServerInternal::notifyPlaybackError(MediaSourceType mediaSourceType, PlaybackError error)
{
    RIALTO_SERVER_LOG_DEBUG("entry:");

    auto task = [&, mediaSourceType, error]()
    {
        if (m_mediaPipelineClient)
        {
            const auto kSourceIter = m_attachedSources.find(mediaSourceType);
            if (m_attachedSources.cend() == kSourceIter)
            {
                RIALTO_SERVER_LOG_WARN("Playback error notification failed - sourceId not found for %s",
                                       common::convertMediaSourceType(mediaSourceType));
                return;
            }
            m_mediaPipelineClient->notifyPlaybackError(kSourceIter->second, error);
        }
    };

    m_mainThread->enqueueTask(m_mainThreadClientId, task);
}

void MediaPipelineServerInternal::notifySourceFlushed(MediaSourceType mediaSourceType)
{
    RIALTO_SERVER_LOG_DEBUG("entry:");

    auto task = [&, mediaSourceType]()
    {
        if (m_mediaPipelineClient)
        {
            const auto kSourceIter = m_attachedSources.find(mediaSourceType);
            if (m_attachedSources.cend() == kSourceIter)
            {
                RIALTO_SERVER_LOG_WARN("Source flushed notification failed - sourceId not found for: %s",
                                       common::convertMediaSourceType(mediaSourceType));
                return;
            }
            m_mediaPipelineClient->notifySourceFlushed(kSourceIter->second);
            RIALTO_SERVER_LOG_DEBUG("%s source flushed", common::convertMediaSourceType(mediaSourceType));
        }
    };

    m_mainThread->enqueueTask(m_mainThreadClientId, task);
}

void MediaPipelineServerInternal::scheduleNotifyNeedMediaData(MediaSourceType mediaSourceType)
{
    RIALTO_SERVER_LOG_DEBUG("entry:");
    auto timer = m_needMediaDataTimers.find(mediaSourceType);
    if (m_needMediaDataTimers.end() != timer && timer->second && timer->second->isActive())
    {
        RIALTO_SERVER_LOG_DEBUG("Skip scheduling need media data for %s - it is already scheduled",
                                common::convertMediaSourceType(mediaSourceType));
        return;
    }

    m_needMediaDataTimers[mediaSourceType] =
        m_timerFactory
            ->createTimer(getNeedMediaDataTimeout(mediaSourceType),
                          [this, mediaSourceType]()
                          {
                              m_mainThread
                                  ->enqueueTask(m_mainThreadClientId,
                                                [this, mediaSourceType]()
                                                {
                                                    m_needMediaDataTimers.erase(mediaSourceType);
                                                    if (!notifyNeedMediaDataInternal(mediaSourceType))
                                                    {
                                                        RIALTO_SERVER_LOG_WARN("Scheduled Need media data sending "
                                                                               "failed for: %s. Scheduling again...",
                                                                               common::convertMediaSourceType(
                                                                                   mediaSourceType));
                                                        scheduleNotifyNeedMediaData(mediaSourceType);
                                                    }
                                                });
                          });
}

std::chrono::milliseconds MediaPipelineServerInternal::getNeedMediaDataTimeout(MediaSourceType mediaSourceType) const
{
    constexpr std::chrono::milliseconds kDefaultNeedMediaDataResendTimeMs{15};
    constexpr std::chrono::milliseconds kNeedMediaDataResendTimeMsForLowLatency{5};
    if ((mediaSourceType == MediaSourceType::VIDEO && m_IsLowLatencyVideoPlayer) ||
        (mediaSourceType == MediaSourceType::AUDIO && m_IsLowLatencyAudioPlayer))
    {
        return kNeedMediaDataResendTimeMsForLowLatency;
    }
    return kDefaultNeedMediaDataResendTimeMs;
}
}; // namespace firebolt::rialto::server
