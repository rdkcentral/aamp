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

#include "MediaPipelineService.h"
#include "IMediaPipelineServerInternal.h"
#include "RialtoServerLogging.h"
#include <exception>
#include <future>
#include <string>
#include <utility>
#include <vector>

namespace firebolt::rialto::server::service
{
MediaPipelineService::MediaPipelineService(
    IPlaybackService &playbackService, std::shared_ptr<IMediaPipelineServerInternalFactory> &&mediaPipelineFactory,
    std::shared_ptr<IMediaPipelineCapabilitiesFactory> &&mediaPipelineCapabilitiesFactory,
    IDecryptionService &decryptionService)
    : m_playbackService{playbackService}, m_mediaPipelineFactory{mediaPipelineFactory},
      m_mediaPipelineCapabilities{mediaPipelineCapabilitiesFactory->createMediaPipelineCapabilities()},
      m_decryptionService{decryptionService}
{
    if (!m_mediaPipelineCapabilities)
    {
        throw std::runtime_error("Could not create Media Pipeline Capabilities");
    }

    RIALTO_SERVER_LOG_DEBUG("MediaPipelineService is constructed");
}

MediaPipelineService::~MediaPipelineService()
{
    RIALTO_SERVER_LOG_DEBUG("MediaPipelineService is destructed");
}

void MediaPipelineService::clearMediaPipelines()
{
    std::lock_guard<std::mutex> lock{m_mediaPipelineMutex};
    m_mediaPipelines.clear();
}

bool MediaPipelineService::createSession(int sessionId, const std::shared_ptr<IMediaPipelineClient> &mediaPipelineClient,
                                         std::uint32_t maxWidth, std::uint32_t maxHeight)
{
    RIALTO_SERVER_LOG_DEBUG("MediaPipelineService requested to create new session with id: %d", sessionId);
    if (!m_playbackService.isActive())
    {
        RIALTO_SERVER_LOG_ERROR("Skip to create session with id: %d - Session Server in Inactive state", sessionId);
        return false;
    }

    {
        std::lock_guard<std::mutex> lock{m_mediaPipelineMutex};
        if (m_mediaPipelines.size() == static_cast<size_t>(m_playbackService.getMaxPlaybacks()))
        {
            RIALTO_SERVER_LOG_ERROR("Unable to create a session with id: %d. Max session number reached.", sessionId);
            return false;
        }
        if (m_mediaPipelines.find(sessionId) != m_mediaPipelines.end())
        {
            RIALTO_SERVER_LOG_ERROR("Session with id: %d already exists", sessionId);
            return false;
        }
        auto shmBuffer = m_playbackService.getShmBuffer();
        m_mediaPipelines.emplace(
            std::make_pair(sessionId,
                           m_mediaPipelineFactory->createMediaPipelineServerInternal(mediaPipelineClient,
                                                                                     VideoRequirements{maxWidth, maxHeight},
                                                                                     sessionId, shmBuffer,
                                                                                     m_decryptionService)));
        if (!m_mediaPipelines.at(sessionId))
        {
            RIALTO_SERVER_LOG_ERROR("Could not create MediaPipeline for session with id: %d", sessionId);
            m_mediaPipelines.erase(sessionId);
            return false;
        }
    }

    RIALTO_SERVER_LOG_INFO("New session with id: %d created", sessionId);
    return true;
}

bool MediaPipelineService::destroySession(int sessionId)
{
    RIALTO_SERVER_LOG_DEBUG("MediaPipelineService requested to destroy session with id: %d", sessionId);
    {
        std::lock_guard<std::mutex> lock{m_mediaPipelineMutex};
        auto mediaPipelineIter = m_mediaPipelines.find(sessionId);
        if (mediaPipelineIter == m_mediaPipelines.end())
        {
            RIALTO_SERVER_LOG_ERROR("Session with id: %d does not exists", sessionId);
            return false;
        }
        m_mediaPipelines.erase(mediaPipelineIter);
    }
    RIALTO_SERVER_LOG_INFO("Session with id: %d destroyed", sessionId);
    return true;
}

bool MediaPipelineService::load(int sessionId, MediaType type, const std::string &mimeType, const std::string &url)
{
    RIALTO_SERVER_LOG_INFO("MediaPipelineService requested to load session with id: %d", sessionId);

    std::lock_guard<std::mutex> lock{m_mediaPipelineMutex};
    auto mediaPipelineIter = m_mediaPipelines.find(sessionId);
    if (mediaPipelineIter == m_mediaPipelines.end())
    {
        RIALTO_SERVER_LOG_ERROR("Session with id: %d does not exists", sessionId);
        return false;
    }
    return mediaPipelineIter->second->load(type, mimeType, url);
}

bool MediaPipelineService::attachSource(int sessionId, const std::unique_ptr<IMediaPipeline::MediaSource> &source)
{
    RIALTO_SERVER_LOG_INFO("MediaPipelineService requested to attach source, session id: %d", sessionId);

    std::lock_guard<std::mutex> lock{m_mediaPipelineMutex};
    auto mediaPipelineIter = m_mediaPipelines.find(sessionId);
    if (mediaPipelineIter == m_mediaPipelines.end())
    {
        RIALTO_SERVER_LOG_ERROR("Session with id: %d does not exists", sessionId);
        return false;
    }
    return mediaPipelineIter->second->attachSource(source);
}

bool MediaPipelineService::removeSource(int sessionId, std::int32_t sourceId)
{
    RIALTO_SERVER_LOG_INFO("MediaPipelineService requested to remove source, session id: %d", sessionId);

    std::lock_guard<std::mutex> lock{m_mediaPipelineMutex};
    auto mediaPipelineIter = m_mediaPipelines.find(sessionId);
    if (mediaPipelineIter == m_mediaPipelines.end())
    {
        RIALTO_SERVER_LOG_ERROR("Session with id: %d does not exists", sessionId);
        return false;
    }
    return mediaPipelineIter->second->removeSource(sourceId);
}

bool MediaPipelineService::allSourcesAttached(int sessionId)
{
    RIALTO_SERVER_LOG_INFO("MediaPipelineService notified that all sources were attached, session id: %d", sessionId);

    std::lock_guard<std::mutex> lock{m_mediaPipelineMutex};
    auto mediaPipelineIter = m_mediaPipelines.find(sessionId);
    if (mediaPipelineIter == m_mediaPipelines.end())
    {
        RIALTO_SERVER_LOG_ERROR("Session with id: %d does not exists", sessionId);
        return false;
    }
    return mediaPipelineIter->second->allSourcesAttached();
}

bool MediaPipelineService::play(int sessionId)
{
    RIALTO_SERVER_LOG_INFO("MediaPipelineService requested to play, session id: %d", sessionId);

    std::lock_guard<std::mutex> lock{m_mediaPipelineMutex};
    auto mediaPipelineIter = m_mediaPipelines.find(sessionId);
    if (mediaPipelineIter == m_mediaPipelines.end())
    {
        RIALTO_SERVER_LOG_ERROR("Session with id: %d does not exists", sessionId);
        return false;
    }
    return mediaPipelineIter->second->play();
}

bool MediaPipelineService::pause(int sessionId)
{
    RIALTO_SERVER_LOG_INFO("MediaPipelineService requested to pause, session id: %d", sessionId);

    std::lock_guard<std::mutex> lock{m_mediaPipelineMutex};
    auto mediaPipelineIter = m_mediaPipelines.find(sessionId);
    if (mediaPipelineIter == m_mediaPipelines.end())
    {
        RIALTO_SERVER_LOG_ERROR("Session with id: %d does not exists", sessionId);
        return false;
    }
    return mediaPipelineIter->second->pause();
}

bool MediaPipelineService::stop(int sessionId)
{
    RIALTO_SERVER_LOG_INFO("MediaPipelineService requested to stop, session id: %d", sessionId);

    std::lock_guard<std::mutex> lock{m_mediaPipelineMutex};
    auto mediaPipelineIter = m_mediaPipelines.find(sessionId);
    if (mediaPipelineIter == m_mediaPipelines.end())
    {
        RIALTO_SERVER_LOG_ERROR("Session with id: %d does not exists", sessionId);
        return false;
    }
    return mediaPipelineIter->second->stop();
}

bool MediaPipelineService::setPlaybackRate(int sessionId, double rate)
{
    RIALTO_SERVER_LOG_INFO("MediaPipelineService requested to set playback rate, session id: %d", sessionId);

    std::lock_guard<std::mutex> lock{m_mediaPipelineMutex};
    auto mediaPipelineIter = m_mediaPipelines.find(sessionId);
    if (mediaPipelineIter == m_mediaPipelines.end())
    {
        RIALTO_SERVER_LOG_ERROR("Session with id: %d does not exists", sessionId);
        return false;
    }
    return mediaPipelineIter->second->setPlaybackRate(rate);
}

bool MediaPipelineService::setPosition(int sessionId, std::int64_t position)
{
    RIALTO_SERVER_LOG_INFO("MediaPipelineService requested to set position, session id: %d", sessionId);

    std::lock_guard<std::mutex> lock{m_mediaPipelineMutex};
    auto mediaPipelineIter = m_mediaPipelines.find(sessionId);
    if (mediaPipelineIter == m_mediaPipelines.end())
    {
        RIALTO_SERVER_LOG_ERROR("Session with id: %d does not exists", sessionId);
        return false;
    }
    return mediaPipelineIter->second->setPosition(position);
}

bool MediaPipelineService::getPosition(int sessionId, std::int64_t &position)
{
    RIALTO_SERVER_LOG_INFO("MediaPipelineService requested to get position, session id: %d", sessionId);

    std::lock_guard<std::mutex> lock{m_mediaPipelineMutex};
    auto mediaPipelineIter = m_mediaPipelines.find(sessionId);
    if (mediaPipelineIter == m_mediaPipelines.end())
    {
        RIALTO_SERVER_LOG_ERROR("Session with id: %d does not exists", sessionId);
        return false;
    }
    return mediaPipelineIter->second->getPosition(position);
}

bool MediaPipelineService::setImmediateOutput(int sessionId, int32_t sourceId, bool immediateOutput)
{
    RIALTO_SERVER_LOG_INFO("MediaPipelineService requested to setImmediateOutput, session id: %d", sessionId);

    std::lock_guard<std::mutex> lock{m_mediaPipelineMutex};
    auto mediaPipelineIter = m_mediaPipelines.find(sessionId);
    if (mediaPipelineIter == m_mediaPipelines.end())
    {
        RIALTO_SERVER_LOG_ERROR("Session with id: %d does not exists", sessionId);
        return false;
    }
    return mediaPipelineIter->second->setImmediateOutput(sourceId, immediateOutput);
}

bool MediaPipelineService::getImmediateOutput(int sessionId, int32_t sourceId, bool &immediateOutput)
{
    RIALTO_SERVER_LOG_INFO("MediaPipelineService requested to getImmediateOutput, session id: %d", sessionId);

    std::lock_guard<std::mutex> lock{m_mediaPipelineMutex};
    auto mediaPipelineIter = m_mediaPipelines.find(sessionId);
    if (mediaPipelineIter == m_mediaPipelines.end())
    {
        RIALTO_SERVER_LOG_ERROR("Session with id: %d does not exists", sessionId);
        return false;
    }
    return mediaPipelineIter->second->getImmediateOutput(sourceId, immediateOutput);
}

bool MediaPipelineService::getStats(int sessionId, int32_t sourceId, uint64_t &renderedFrames, uint64_t &droppedFrames)
{
    RIALTO_SERVER_LOG_INFO("MediaPipelineService requested to get stats, session id: %d", sessionId);

    std::lock_guard<std::mutex> lock{m_mediaPipelineMutex};
    auto mediaPipelineIter = m_mediaPipelines.find(sessionId);
    if (mediaPipelineIter == m_mediaPipelines.end())
    {
        RIALTO_SERVER_LOG_ERROR("Session with id: %d does not exists", sessionId);
        return false;
    }
    return mediaPipelineIter->second->getStats(sourceId, renderedFrames, droppedFrames);
}

bool MediaPipelineService::setVideoWindow(int sessionId, std::uint32_t x, std::uint32_t y, std::uint32_t width,
                                          std::uint32_t height)
{
    RIALTO_SERVER_LOG_INFO("MediaPipelineService requested to set video window, session id: %d", sessionId);

    std::lock_guard<std::mutex> lock{m_mediaPipelineMutex};
    auto mediaPipelineIter = m_mediaPipelines.find(sessionId);
    if (mediaPipelineIter == m_mediaPipelines.end())
    {
        RIALTO_SERVER_LOG_ERROR("Session with id: %d does not exists", sessionId);
        return false;
    }
    return mediaPipelineIter->second->setVideoWindow(x, y, width, height);
}

bool MediaPipelineService::haveData(int sessionId, MediaSourceStatus status, std::uint32_t numFrames,
                                    std::uint32_t needDataRequestId)
{
    RIALTO_SERVER_LOG_DEBUG("New data available, session id: %d", sessionId);

    std::lock_guard<std::mutex> lock{m_mediaPipelineMutex};
    auto mediaPipelineIter = m_mediaPipelines.find(sessionId);
    if (mediaPipelineIter == m_mediaPipelines.end())
    {
        RIALTO_SERVER_LOG_ERROR("Session with id: %d does not exists", sessionId);
        return false;
    }
    return mediaPipelineIter->second->haveData(status, numFrames, needDataRequestId);
}

bool MediaPipelineService::renderFrame(int sessionId)
{
    RIALTO_SERVER_LOG_DEBUG("Render frame requested, session id: %d", sessionId);

    std::lock_guard<std::mutex> lock{m_mediaPipelineMutex};
    auto mediaPipelineIter = m_mediaPipelines.find(sessionId);
    if (mediaPipelineIter == m_mediaPipelines.end())
    {
        RIALTO_SERVER_LOG_ERROR("Session with id: %d does not exists", sessionId);
        return false;
    }
    return mediaPipelineIter->second->renderFrame();
}
bool MediaPipelineService::setVolume(int sessionId, double targetVolume, uint32_t volumeDuration, EaseType easeType)
{
    RIALTO_SERVER_LOG_DEBUG("Set volume requested, session id: %d", sessionId);

    std::lock_guard<std::mutex> lock{m_mediaPipelineMutex};
    auto mediaPipelineIter = m_mediaPipelines.find(sessionId);
    if (mediaPipelineIter == m_mediaPipelines.end())
    {
        RIALTO_SERVER_LOG_ERROR("Session with id: %d does not exists", sessionId);
        return false;
    }
    return mediaPipelineIter->second->setVolume(targetVolume, volumeDuration, easeType);
}

bool MediaPipelineService::getVolume(int sessionId, double &volume)
{
    RIALTO_SERVER_LOG_DEBUG("Get volume requested, session id: %d", sessionId);

    std::lock_guard<std::mutex> lock{m_mediaPipelineMutex};
    auto mediaPipelineIter = m_mediaPipelines.find(sessionId);
    if (mediaPipelineIter == m_mediaPipelines.end())
    {
        RIALTO_SERVER_LOG_ERROR("Session with id: %d does not exists", sessionId);
        return false;
    }
    return mediaPipelineIter->second->getVolume(volume);
}

bool MediaPipelineService::setMute(int sessionId, std::int32_t sourceId, bool mute)
{
    RIALTO_SERVER_LOG_DEBUG("Set mute requested, session id: %d", sessionId);

    std::lock_guard<std::mutex> lock{m_mediaPipelineMutex};
    auto mediaPipelineIter = m_mediaPipelines.find(sessionId);
    if (mediaPipelineIter == m_mediaPipelines.end())
    {
        RIALTO_SERVER_LOG_ERROR("Session with id: %d does not exist", sessionId);
        return false;
    }
    return mediaPipelineIter->second->setMute(sourceId, mute);
}

bool MediaPipelineService::getMute(int sessionId, std::int32_t sourceId, bool &mute)
{
    RIALTO_SERVER_LOG_DEBUG("Get mute requested, session id: %d", sessionId);

    std::lock_guard<std::mutex> lock{m_mediaPipelineMutex};
    auto mediaPipelineIter = m_mediaPipelines.find(sessionId);
    if (mediaPipelineIter == m_mediaPipelines.end())
    {
        RIALTO_SERVER_LOG_ERROR("Session with id: %d does not exist", sessionId);
        return false;
    }
    return mediaPipelineIter->second->getMute(sourceId, mute);
}

bool MediaPipelineService::setTextTrackIdentifier(int sessionId, const std::string &textTrackIdentifier)
{
    RIALTO_SERVER_LOG_DEBUG("Set text track identifier requested, session id: %d", sessionId);

    std::lock_guard<std::mutex> lock{m_mediaPipelineMutex};
    auto mediaPipelineIter = m_mediaPipelines.find(sessionId);
    if (mediaPipelineIter == m_mediaPipelines.end())
    {
        RIALTO_SERVER_LOG_ERROR("Session with id: %d does not exist", sessionId);
        return false;
    }
    return mediaPipelineIter->second->setTextTrackIdentifier(textTrackIdentifier);
}

bool MediaPipelineService::getTextTrackIdentifier(int sessionId, std::string &textTrackIdentifier)
{
    RIALTO_SERVER_LOG_DEBUG("Get text track identifier requested, session id: %d", sessionId);

    std::lock_guard<std::mutex> lock{m_mediaPipelineMutex};
    auto mediaPipelineIter = m_mediaPipelines.find(sessionId);
    if (mediaPipelineIter == m_mediaPipelines.end())
    {
        RIALTO_SERVER_LOG_ERROR("Session with id: %d does not exist", sessionId);
        return false;
    }
    return mediaPipelineIter->second->getTextTrackIdentifier(textTrackIdentifier);
}

bool MediaPipelineService::setLowLatency(int sessionId, bool lowLatency)
{
    RIALTO_SERVER_LOG_DEBUG("Set low latency requested, session id: %d", sessionId);

    std::lock_guard<std::mutex> lock{m_mediaPipelineMutex};
    auto mediaPipelineIter = m_mediaPipelines.find(sessionId);
    if (mediaPipelineIter == m_mediaPipelines.end())
    {
        RIALTO_SERVER_LOG_ERROR("Session with id: %d does not exist", sessionId);
        return false;
    }
    return mediaPipelineIter->second->setLowLatency(lowLatency);
}

bool MediaPipelineService::setSync(int sessionId, bool sync)
{
    RIALTO_SERVER_LOG_DEBUG("Set sync requested, session id: %d", sessionId);

    std::lock_guard<std::mutex> lock{m_mediaPipelineMutex};
    auto mediaPipelineIter = m_mediaPipelines.find(sessionId);
    if (mediaPipelineIter == m_mediaPipelines.end())
    {
        RIALTO_SERVER_LOG_ERROR("Session with id: %d does not exist", sessionId);
        return false;
    }
    return mediaPipelineIter->second->setSync(sync);
}

bool MediaPipelineService::getSync(int sessionId, bool &sync)
{
    RIALTO_SERVER_LOG_DEBUG("Get sync requested, session id: %d", sessionId);

    std::lock_guard<std::mutex> lock{m_mediaPipelineMutex};
    auto mediaPipelineIter = m_mediaPipelines.find(sessionId);
    if (mediaPipelineIter == m_mediaPipelines.end())
    {
        RIALTO_SERVER_LOG_ERROR("Session with id: %d does not exist", sessionId);
        return false;
    }
    return mediaPipelineIter->second->getSync(sync);
}

bool MediaPipelineService::setSyncOff(int sessionId, bool syncOff)
{
    RIALTO_SERVER_LOG_DEBUG("Set sync off requested, session id: %d", sessionId);

    std::lock_guard<std::mutex> lock{m_mediaPipelineMutex};
    auto mediaPipelineIter = m_mediaPipelines.find(sessionId);
    if (mediaPipelineIter == m_mediaPipelines.end())
    {
        RIALTO_SERVER_LOG_ERROR("Session with id: %d does not exist", sessionId);
        return false;
    }
    return mediaPipelineIter->second->setSyncOff(syncOff);
}

bool MediaPipelineService::setStreamSyncMode(int sessionId, int32_t sourceId, int32_t streamSyncMode)
{
    RIALTO_SERVER_LOG_DEBUG("Set stream sync mode requested, session id: %d", sessionId);

    std::lock_guard<std::mutex> lock{m_mediaPipelineMutex};
    auto mediaPipelineIter = m_mediaPipelines.find(sessionId);
    if (mediaPipelineIter == m_mediaPipelines.end())
    {
        RIALTO_SERVER_LOG_ERROR("Session with id: %d does not exist", sessionId);
        return false;
    }
    return mediaPipelineIter->second->setStreamSyncMode(sourceId, streamSyncMode);
}

bool MediaPipelineService::getStreamSyncMode(int sessionId, int32_t &streamSyncMode)
{
    RIALTO_SERVER_LOG_DEBUG("Get stream sync mode requested, session id: %d", sessionId);

    std::lock_guard<std::mutex> lock{m_mediaPipelineMutex};
    auto mediaPipelineIter = m_mediaPipelines.find(sessionId);
    if (mediaPipelineIter == m_mediaPipelines.end())
    {
        RIALTO_SERVER_LOG_ERROR("Session with id: %d does not exist", sessionId);
        return false;
    }
    return mediaPipelineIter->second->getStreamSyncMode(streamSyncMode);
}

bool MediaPipelineService::flush(int sessionId, std::int32_t sourceId, bool resetTime, bool &isAsync)
{
    RIALTO_SERVER_LOG_DEBUG("Flush requested, session id: %d", sessionId);

    std::lock_guard<std::mutex> lock{m_mediaPipelineMutex};
    auto mediaPipelineIter = m_mediaPipelines.find(sessionId);
    if (mediaPipelineIter == m_mediaPipelines.end())
    {
        RIALTO_SERVER_LOG_ERROR("Session with id: %d does not exist", sessionId);
        return false;
    }
    return mediaPipelineIter->second->flush(sourceId, resetTime, isAsync);
}

bool MediaPipelineService::setSourcePosition(int sessionId, int32_t sourceId, int64_t position, bool resetTime,
                                             double appliedRate, uint64_t stopPosition)
{
    RIALTO_SERVER_LOG_DEBUG("Set Source Position requested, session id: %d", sessionId);

    std::lock_guard<std::mutex> lock{m_mediaPipelineMutex};
    auto mediaPipelineIter = m_mediaPipelines.find(sessionId);
    if (mediaPipelineIter == m_mediaPipelines.end())
    {
        RIALTO_SERVER_LOG_ERROR("Session with id: %d does not exist", sessionId);
        return false;
    }
    return mediaPipelineIter->second->setSourcePosition(sourceId, position, resetTime, appliedRate, stopPosition);
}

bool MediaPipelineService::setSubtitleOffset(int sessionId, int32_t sourceId, int64_t position)
{
    RIALTO_SERVER_LOG_DEBUG("Set Subtitle Offset requested, session id: %d", sessionId);

    std::lock_guard<std::mutex> lock{m_mediaPipelineMutex};
    auto mediaPipelineIter = m_mediaPipelines.find(sessionId);
    if (mediaPipelineIter == m_mediaPipelines.end())
    {
        RIALTO_SERVER_LOG_ERROR("Session with id: %d does not exist", sessionId);
        return false;
    }
    return mediaPipelineIter->second->setSubtitleOffset(sourceId, position);
}

bool MediaPipelineService::processAudioGap(int sessionId, int64_t position, uint32_t duration, int64_t discontinuityGap,
                                           bool audioAac)
{
    RIALTO_SERVER_LOG_DEBUG("Process Audio Gap requested, session id: %d", sessionId);

    std::lock_guard<std::mutex> lock{m_mediaPipelineMutex};
    auto mediaPipelineIter = m_mediaPipelines.find(sessionId);
    if (mediaPipelineIter == m_mediaPipelines.end())
    {
        RIALTO_SERVER_LOG_ERROR("Session with id: %d does not exist", sessionId);
        return false;
    }
    return mediaPipelineIter->second->processAudioGap(position, duration, discontinuityGap, audioAac);
}

bool MediaPipelineService::setBufferingLimit(int sessionId, uint32_t limitBufferingMs)
{
    RIALTO_SERVER_LOG_DEBUG("Set buffering limit requested, session id: %d", sessionId);

    std::lock_guard<std::mutex> lock{m_mediaPipelineMutex};
    auto mediaPipelineIter = m_mediaPipelines.find(sessionId);
    if (mediaPipelineIter == m_mediaPipelines.end())
    {
        RIALTO_SERVER_LOG_ERROR("Session with id: %d does not exist", sessionId);
        return false;
    }
    return mediaPipelineIter->second->setBufferingLimit(limitBufferingMs);
}

bool MediaPipelineService::getBufferingLimit(int sessionId, uint32_t &limitBufferingMs)
{
    RIALTO_SERVER_LOG_DEBUG("Get buffering limit requested, session id: %d", sessionId);

    std::lock_guard<std::mutex> lock{m_mediaPipelineMutex};
    auto mediaPipelineIter = m_mediaPipelines.find(sessionId);
    if (mediaPipelineIter == m_mediaPipelines.end())
    {
        RIALTO_SERVER_LOG_ERROR("Session with id: %d does not exist", sessionId);
        return false;
    }
    return mediaPipelineIter->second->getBufferingLimit(limitBufferingMs);
}

bool MediaPipelineService::setUseBuffering(int sessionId, bool useBuffering)
{
    RIALTO_SERVER_LOG_DEBUG("Set use buffering requested, session id: %d", sessionId);

    std::lock_guard<std::mutex> lock{m_mediaPipelineMutex};
    auto mediaPipelineIter = m_mediaPipelines.find(sessionId);
    if (mediaPipelineIter == m_mediaPipelines.end())
    {
        RIALTO_SERVER_LOG_ERROR("Session with id: %d does not exist", sessionId);
        return false;
    }
    return mediaPipelineIter->second->setUseBuffering(useBuffering);
}

bool MediaPipelineService::getUseBuffering(int sessionId, bool &useBuffering)
{
    RIALTO_SERVER_LOG_DEBUG("Get use buffering requested, session id: %d", sessionId);

    std::lock_guard<std::mutex> lock{m_mediaPipelineMutex};
    auto mediaPipelineIter = m_mediaPipelines.find(sessionId);
    if (mediaPipelineIter == m_mediaPipelines.end())
    {
        RIALTO_SERVER_LOG_ERROR("Session with id: %d does not exist", sessionId);
        return false;
    }
    return mediaPipelineIter->second->getUseBuffering(useBuffering);
}

bool MediaPipelineService::switchSource(int sessionId, const std::unique_ptr<IMediaPipeline::MediaSource> &source)
{
    RIALTO_SERVER_LOG_INFO("MediaPipelineService requested to switch source, session id: %d", sessionId);

    std::lock_guard<std::mutex> lock{m_mediaPipelineMutex};
    auto mediaPipelineIter = m_mediaPipelines.find(sessionId);
    if (mediaPipelineIter == m_mediaPipelines.end())
    {
        RIALTO_SERVER_LOG_ERROR("Session with id: %d does not exists", sessionId);
        return false;
    }
    return mediaPipelineIter->second->switchSource(source);
}

bool MediaPipelineService::isVideoMaster(bool &isVideoMaster)
{
    RIALTO_SERVER_LOG_INFO("MediaPipelineService requested check if video is master");

    return m_mediaPipelineCapabilities->isVideoMaster(isVideoMaster);
}

std::vector<std::string> MediaPipelineService::getSupportedMimeTypes(MediaSourceType type)
{
    return m_mediaPipelineCapabilities->getSupportedMimeTypes(type);
}

bool MediaPipelineService::isMimeTypeSupported(const std::string &mimeType)
{
    return m_mediaPipelineCapabilities->isMimeTypeSupported(mimeType);
}

std::vector<std::string> MediaPipelineService::getSupportedProperties(MediaSourceType mediaType,
                                                                      const std::vector<std::string> &propertyNames)
{
    return m_mediaPipelineCapabilities->getSupportedProperties(mediaType, propertyNames);
}

void MediaPipelineService::ping(const std::shared_ptr<IHeartbeatProcedure> &heartbeatProcedure)
{
    RIALTO_SERVER_LOG_DEBUG("Ping requested");
    std::lock_guard<std::mutex> lock{m_mediaPipelineMutex};
    for (const auto &mediaPipelinePair : m_mediaPipelines)
    {
        auto &mediaPipeline = mediaPipelinePair.second;
        mediaPipeline->ping(heartbeatProcedure->createHandler());
    }
}

} // namespace firebolt::rialto::server::service
