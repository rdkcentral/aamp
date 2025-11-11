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

#include <inttypes.h>
#include <stdexcept>
#include <stdint.h>

#include "KeyIdMap.h"
#include "MediaPipeline.h"
#include "MediaPipelineProxy.h"
#include "RialtoClientLogging.h"

namespace
{
const char *toString(const firebolt::rialto::client::MediaPipeline::State &state)
{
    switch (state)
    {
    case firebolt::rialto::client::MediaPipeline::State::IDLE:
        return "IDLE";
    case firebolt::rialto::client::MediaPipeline::State::BUFFERING:
        return "BUFFERING";
    case firebolt::rialto::client::MediaPipeline::State::PLAYING:
        return "PLAYING";
    case firebolt::rialto::client::MediaPipeline::State::SEEKING:
        return "SEEKING";
    case firebolt::rialto::client::MediaPipeline::State::FAILURE:
        return "FAILURE";
    case firebolt::rialto::client::MediaPipeline::State::END_OF_STREAM:
        return "END_OF_STREAM";
    }
    return "UNKNOWN";
}

const char *toString(const firebolt::rialto::PlaybackState &state)
{
    switch (state)
    {
    case firebolt::rialto::PlaybackState::IDLE:
        return "IDLE";
    case firebolt::rialto::PlaybackState::PLAYING:
        return "PLAYING";
    case firebolt::rialto::PlaybackState::PAUSED:
        return "PAUSED";
    case firebolt::rialto::PlaybackState::SEEKING:
        return "SEEKING";
    case firebolt::rialto::PlaybackState::SEEK_DONE:
        return "SEEK_DONE";
    case firebolt::rialto::PlaybackState::STOPPED:
        return "STOPPED";
    case firebolt::rialto::PlaybackState::END_OF_STREAM:
        return "END_OF_STREAM";
    case firebolt::rialto::PlaybackState::FAILURE:
        return "FAILURE";
    case firebolt::rialto::PlaybackState::UNKNOWN:
        return "UNKNOWN";
    }
    return "UNKNOWN";
}

const char *toString(const firebolt::rialto::NetworkState &state)
{
    switch (state)
    {
    case firebolt::rialto::NetworkState::IDLE:
        return "IDLE";
    case firebolt::rialto::NetworkState::BUFFERING:
        return "BUFFERING";
    case firebolt::rialto::NetworkState::BUFFERING_PROGRESS:
        return "BUFFERING_PROGRESS";
    case firebolt::rialto::NetworkState::BUFFERED:
        return "BUFFERED";
    case firebolt::rialto::NetworkState::STALLED:
        return "STALLED";
    case firebolt::rialto::NetworkState::FORMAT_ERROR:
        return "FORMAT_ERROR";
    case firebolt::rialto::NetworkState::NETWORK_ERROR:
        return "NETWORK_ERROR";
    case firebolt::rialto::NetworkState::DECODE_ERROR:
        return "DECODE_ERROR";
    case firebolt::rialto::NetworkState::UNKNOWN:
        return "UNKNOWN";
    }
    return "UNKNOWN";
}
} // namespace

namespace firebolt::rialto
{
std::shared_ptr<IMediaPipelineFactory> IMediaPipelineFactory::createFactory()
{
    std::shared_ptr<IMediaPipelineFactory> factory;

    try
    {
        factory = std::make_shared<MediaPipelineFactory>();
    }
    catch (const std::exception &e)
    {
        RIALTO_CLIENT_LOG_ERROR("Failed to create the media player factory, reason: %s", e.what());
    }

    return factory;
}

std::unique_ptr<IMediaPipeline> MediaPipelineFactory::createMediaPipeline(std::weak_ptr<IMediaPipelineClient> client,
                                                                          const VideoRequirements &videoRequirements) const
{
    return createMediaPipeline(client, videoRequirements, {}, {});
}

std::unique_ptr<IMediaPipeline>
MediaPipelineFactory::createMediaPipeline(std::weak_ptr<IMediaPipelineClient> client,
                                          const VideoRequirements &videoRequirements,
                                          std::weak_ptr<client::IMediaPipelineIpcFactory> mediaPipelineIpcFactory,
                                          std::weak_ptr<client::IClientController> clientController) const
{
    std::unique_ptr<IMediaPipeline> mediaPipeline;
    try
    {
        std::shared_ptr<client::IMediaPipelineIpcFactory> mediaPipelineIpcFactoryLocked = mediaPipelineIpcFactory.lock();
        std::shared_ptr<client::IClientController> clientControllerLocked = clientController.lock();
        firebolt::rialto::client::IClientController &cc =
            clientControllerLocked ? *clientControllerLocked
                                   : client::IClientControllerAccessor::instance().getClientController();

        auto mp{std::make_shared<client::MediaPipeline>(client, videoRequirements,
                                                        mediaPipelineIpcFactoryLocked
                                                            ? mediaPipelineIpcFactoryLocked
                                                            : client::IMediaPipelineIpcFactory::getFactory(),
                                                        common::IMediaFrameWriterFactory::getFactory(), cc)};
        mediaPipeline = std::move(std::make_unique<client::MediaPipelineProxy>(mp, cc));
    }
    catch (const std::exception &e)
    {
        RIALTO_CLIENT_LOG_ERROR("Failed to create the media player, reason: %s", e.what());
    }

    return mediaPipeline;
}

}; // namespace firebolt::rialto

namespace firebolt::rialto::client
{
MediaPipelineProxy::MediaPipelineProxy(const std::shared_ptr<IMediaPipelineAndIControlClient> &mediaPipeline,
                                       IClientController &clientController)
    : m_mediaPipeline{mediaPipeline}, m_clientController{clientController}
{
    ApplicationState state{ApplicationState::UNKNOWN};
    if (!m_clientController.registerClient(m_mediaPipeline, state))
    {
        throw std::runtime_error("Failed to register client with clientController");
    }
    m_mediaPipeline->notifyApplicationState(state);
}

MediaPipelineProxy::~MediaPipelineProxy()
{
    if (!m_clientController.unregisterClient(m_mediaPipeline))
    {
        RIALTO_CLIENT_LOG_WARN("Failed to unregister client with clientController");
    }
}

MediaPipeline::MediaPipeline(std::weak_ptr<IMediaPipelineClient> client, const VideoRequirements &videoRequirements,
                             const std::shared_ptr<IMediaPipelineIpcFactory> &mediaPipelineIpcFactory,
                             const std::shared_ptr<common::IMediaFrameWriterFactory> &mediaFrameWriterFactory,
                             IClientController &clientController)
    : m_mediaPipelineClient(client), m_clientController{clientController}, m_currentAppState{ApplicationState::UNKNOWN},
      m_mediaFrameWriterFactory(mediaFrameWriterFactory), m_currentState(State::IDLE)
{
    RIALTO_CLIENT_LOG_DEBUG("entry:");

    m_mediaPipelineIpc = mediaPipelineIpcFactory->createMediaPipelineIpc(this, videoRequirements);

    if (!m_mediaPipelineIpc)
    {
        throw std::runtime_error("Media player ipc could not be created");
    }
}

MediaPipeline::~MediaPipeline()
{
    RIALTO_CLIENT_LOG_DEBUG("entry:");

    m_mediaPipelineIpc.reset();
}

bool MediaPipeline::load(MediaType type, const std::string &mimeType, const std::string &url)
{
    RIALTO_CLIENT_LOG_DEBUG("entry:");

    return m_mediaPipelineIpc->load(type, mimeType, url);
}

bool MediaPipeline::attachSource(const std::unique_ptr<IMediaPipeline::MediaSource> &source)
{
    RIALTO_CLIENT_LOG_DEBUG("entry:");

    // We should not process needDatas while attach source is ongoing
    {
        std::unique_lock<std::mutex> lock{m_attachSourceMutex};
        m_attachingSource = true;
    }

    int32_t sourceId = -1;

    bool status = m_mediaPipelineIpc->attachSource(source, sourceId);
    if (status)
    {
        source->setId(sourceId);
        m_attachedSources.add(sourceId, source->getType());
    }

    // Unblock needDatas
    {
        std::unique_lock<std::mutex> lock{m_attachSourceMutex};
        m_attachingSource = false;
        m_attachSourceCond.notify_all();
    }
    return status;
}

bool MediaPipeline::removeSource(int32_t id)
{
    RIALTO_CLIENT_LOG_DEBUG("entry:");
    m_attachedSources.remove(id);
    return m_mediaPipelineIpc->removeSource(id);
}

bool MediaPipeline::allSourcesAttached()
{
    RIALTO_CLIENT_LOG_DEBUG("entry:");

    return m_mediaPipelineIpc->allSourcesAttached();
}

bool MediaPipeline::play()
{
    RIALTO_CLIENT_LOG_DEBUG("entry:");

    return m_mediaPipelineIpc->play();
}

bool MediaPipeline::pause()
{
    RIALTO_CLIENT_LOG_DEBUG("entry:");

    return m_mediaPipelineIpc->pause();
}

bool MediaPipeline::stop()
{
    RIALTO_CLIENT_LOG_DEBUG("entry:");

    m_currentState = State::IDLE;

    return m_mediaPipelineIpc->stop();
}

bool MediaPipeline::setPlaybackRate(double rate)
{
    RIALTO_CLIENT_LOG_DEBUG("entry:");

    return m_mediaPipelineIpc->setPlaybackRate(rate);
}

bool MediaPipeline::setPosition(int64_t position)
{
    switch (m_currentState)
    {
    case State::PLAYING:
    case State::BUFFERING:
    case State::SEEKING:
    case State::END_OF_STREAM:
    {
        return handleSetPosition(position);
    }
    case State::IDLE:
    case State::FAILURE:
    default:
    {
        RIALTO_CLIENT_LOG_WARN("SetPosition received in unexpected state '%s'", toString(m_currentState));
        return false;
    }
    }
}

bool MediaPipeline::getPosition(int64_t &position)
{
    return m_mediaPipelineIpc->getPosition(position);
}

bool MediaPipeline::setImmediateOutput(int32_t sourceId, bool immediateOutput)
{
    return m_mediaPipelineIpc->setImmediateOutput(sourceId, immediateOutput);
}

bool MediaPipeline::getImmediateOutput(int32_t sourceId, bool &immediateOutput)
{
    return m_mediaPipelineIpc->getImmediateOutput(sourceId, immediateOutput);
}

bool MediaPipeline::getStats(int32_t sourceId, uint64_t &renderedFrames, uint64_t &droppedFrames)
{
    return m_mediaPipelineIpc->getStats(sourceId, renderedFrames, droppedFrames);
}

bool MediaPipeline::handleSetPosition(int64_t position)
{
    // needData requests no longer valid
    {
        std::lock_guard<std::mutex> lock{m_needDataRequestMapMutex};
        m_needDataRequestMap.clear();
    }
    return m_mediaPipelineIpc->setPosition(position);
}

bool MediaPipeline::setVideoWindow(uint32_t x, uint32_t y, uint32_t width, uint32_t height)
{
    RIALTO_CLIENT_LOG_DEBUG("entry:");

    return m_mediaPipelineIpc->setVideoWindow(x, y, width, height);
}

bool MediaPipeline::haveData(MediaSourceStatus status, uint32_t needDataRequestId)
{
    RIALTO_CLIENT_LOG_DEBUG("entry:");

    switch (m_currentState)
    {
    case State::BUFFERING:
    case State::PLAYING:
    {
        return handleHaveData(status, needDataRequestId);
    }
    case State::SEEKING:
    {
        RIALTO_CLIENT_LOG_INFO("HaveData received while seeking, discarding NeedData request %u", needDataRequestId);
        discardNeedDataRequest(needDataRequestId);
        return true;
    }
    case State::IDLE:
    case State::END_OF_STREAM:
    case State::FAILURE:
    default:
    {
        RIALTO_CLIENT_LOG_WARN("HaveData received in unexpected state '%s', discarding NeedData request %u",
                               toString(m_currentState), needDataRequestId);
        discardNeedDataRequest(needDataRequestId);
        return false;
    }
    }
}

bool MediaPipeline::handleHaveData(MediaSourceStatus status, uint32_t needDataRequestId)
{
    RIALTO_CLIENT_LOG_DEBUG("entry:");

    std::shared_ptr<NeedDataRequest> needDataRequest;

    // Find the needDataRequest for this needDataRequestId
    // The needData request can be cancelled from another thread
    {
        std::lock_guard<std::mutex> lock{m_needDataRequestMapMutex};

        auto needDataRequestIt = m_needDataRequestMap.find(needDataRequestId);
        if (needDataRequestIt == m_needDataRequestMap.end())
        {
            // Return success here as the data written is just ignored
            RIALTO_CLIENT_LOG_WARN("Could not find need data request, with id %u", needDataRequestId);
            return true;
        }

        needDataRequest = needDataRequestIt->second;
        m_needDataRequestMap.erase(needDataRequestIt);
    }
    if (m_attachedSources.isFlushing(needDataRequest->sourceId))
    {
        RIALTO_CLIENT_LOG_WARN("Source %d is flushing. Ignoring need data request, with id %u",
                               needDataRequest->sourceId, needDataRequestId);
        return true;
    }

    uint32_t numFrames = needDataRequest->frameWriter ? needDataRequest->frameWriter->getNumFrames() : 0;
    return m_mediaPipelineIpc->haveData(status, numFrames, needDataRequestId);
}

AddSegmentStatus MediaPipeline::addSegment(uint32_t needDataRequestId, const std::unique_ptr<MediaSegment> &mediaSegment)
{
    RIALTO_CLIENT_LOG_DEBUG("entry:");

    if (nullptr == mediaSegment || nullptr == mediaSegment->getData())
    {
        return AddSegmentStatus::ERROR;
    }

    std::lock_guard<std::mutex> lock{m_needDataRequestMapMutex};
    auto needDataRequestIt = m_needDataRequestMap.find(needDataRequestId);
    if (needDataRequestIt == m_needDataRequestMap.end())
    {
        RIALTO_CLIENT_LOG_ERROR("Could not find need data request, with id %u", needDataRequestId);
        return AddSegmentStatus::ERROR;
    }

    std::shared_ptr<NeedDataRequest> needDataRequest = needDataRequestIt->second;
    std::shared_ptr<ISharedMemoryHandle> shmHandle = m_clientController.getSharedMemoryHandle();
    if (nullptr == shmHandle || nullptr == shmHandle->getShm())
    {
        RIALTO_CLIENT_LOG_ERROR("Shared buffer no longer valid");
        return AddSegmentStatus::ERROR;
    }

    // This block of code is only for playready apps using rialto c++ interface
    // Widevine apps and playready apps using rialto-ocdm set MediaSegment::keyId earlier
    if (mediaSegment->isEncrypted())
    {
        auto keyId = KeyIdMap::instance().get(mediaSegment->getMediaKeySessionId());
        if (!keyId.empty() && mediaSegment->getKeyId().empty())
        {
            RIALTO_CLIENT_LOG_DEBUG("Adding Playready keyID to media segment");
            mediaSegment->setKeyId(keyId);
        }
    }

    if (!needDataRequest->frameWriter)
    {
        if (firebolt::rialto::MediaSourceType::UNKNOWN != mediaSegment->getType())
        {
            needDataRequest->frameWriter =
                m_mediaFrameWriterFactory->createFrameWriter(shmHandle->getShm(), needDataRequest->shmInfo);
        }
        else
        {
            RIALTO_CLIENT_LOG_ERROR("Unrecognised type %u", static_cast<uint32_t>(mediaSegment->getType()));
            return AddSegmentStatus::ERROR;
        }

        if (!needDataRequest->frameWriter)
        {
            RIALTO_CLIENT_LOG_ERROR("Could not create frame writer");
            return AddSegmentStatus::ERROR;
        }
    }

    return needDataRequest->frameWriter->writeFrame(mediaSegment);
}

bool MediaPipeline::renderFrame()
{
    RIALTO_CLIENT_LOG_DEBUG("entry:");
    return m_mediaPipelineIpc->renderFrame();
}

bool MediaPipeline::setVolume(double targetVolume, uint32_t volumeDuration, EaseType easeType)
{
    RIALTO_CLIENT_LOG_DEBUG("entry:");
    return m_mediaPipelineIpc->setVolume(targetVolume, volumeDuration, easeType);
}

bool MediaPipeline::getVolume(double &currentVolume)
{
    RIALTO_CLIENT_LOG_DEBUG("entry:");
    return m_mediaPipelineIpc->getVolume(currentVolume);
}

bool MediaPipeline::setMute(int32_t sourceId, bool mute)
{
    RIALTO_CLIENT_LOG_DEBUG("entry:");
    return m_mediaPipelineIpc->setMute(sourceId, mute);
}

bool MediaPipeline::getMute(int32_t sourceId, bool &mute)
{
    RIALTO_CLIENT_LOG_DEBUG("entry:");
    return m_mediaPipelineIpc->getMute(sourceId, mute);
}

bool MediaPipeline::setTextTrackIdentifier(const std::string &textTrackIdentifier)
{
    RIALTO_CLIENT_LOG_DEBUG("entry:");
    return m_mediaPipelineIpc->setTextTrackIdentifier(textTrackIdentifier);
}

bool MediaPipeline::getTextTrackIdentifier(std::string &textTrackIdentifier)
{
    RIALTO_CLIENT_LOG_DEBUG("entry:");
    return m_mediaPipelineIpc->getTextTrackIdentifier(textTrackIdentifier);
}

bool MediaPipeline::setLowLatency(bool lowLatency)
{
    RIALTO_CLIENT_LOG_DEBUG("entry:");
    return m_mediaPipelineIpc->setLowLatency(lowLatency);
}

bool MediaPipeline::setSync(bool sync)
{
    RIALTO_CLIENT_LOG_DEBUG("entry:");
    return m_mediaPipelineIpc->setSync(sync);
}

bool MediaPipeline::getSync(bool &sync)
{
    RIALTO_CLIENT_LOG_DEBUG("entry:");
    return m_mediaPipelineIpc->getSync(sync);
}

bool MediaPipeline::setSyncOff(bool syncOff)
{
    RIALTO_CLIENT_LOG_DEBUG("entry:");
    return m_mediaPipelineIpc->setSyncOff(syncOff);
}

bool MediaPipeline::setStreamSyncMode(int32_t sourceId, int32_t streamSyncMode)
{
    RIALTO_CLIENT_LOG_DEBUG("entry:");
    return m_mediaPipelineIpc->setStreamSyncMode(sourceId, streamSyncMode);
}

bool MediaPipeline::getStreamSyncMode(int32_t &streamSyncMode)
{
    RIALTO_CLIENT_LOG_DEBUG("entry:");
    return m_mediaPipelineIpc->getStreamSyncMode(streamSyncMode);
}

bool MediaPipeline::flush(int32_t sourceId, bool resetTime, bool &async)
{
    RIALTO_CLIENT_LOG_DEBUG("entry:");

    std::unique_lock<std::mutex> flushLock{m_flushMutex};
    if (m_mediaPipelineIpc->flush(sourceId, resetTime, async))
    {
        m_attachedSources.setFlushing(sourceId, true);
        flushLock.unlock();

        // Clear all need datas for flushed source
        std::lock_guard<std::mutex> lock{m_needDataRequestMapMutex};
        for (auto it = m_needDataRequestMap.begin(); it != m_needDataRequestMap.end();)
        {
            if (it->second->sourceId == sourceId)
            {
                it = m_needDataRequestMap.erase(it);
            }
            else
            {
                ++it;
            }
        }
        return true;
    }
    return false;
}

bool MediaPipeline::setSourcePosition(int32_t sourceId, int64_t position, bool resetTime, double appliedRate,
                                      uint64_t stopPosition)
{
    RIALTO_CLIENT_LOG_DEBUG("entry:");

    return m_mediaPipelineIpc->setSourcePosition(sourceId, position, resetTime, appliedRate, stopPosition);
}

bool MediaPipeline::setSubtitleOffset(int32_t sourceId, int64_t position)
{
    RIALTO_CLIENT_LOG_DEBUG("entry:");

    return m_mediaPipelineIpc->setSubtitleOffset(sourceId, position);
}

bool MediaPipeline::processAudioGap(int64_t position, uint32_t duration, int64_t discontinuityGap, bool audioAac)
{
    RIALTO_CLIENT_LOG_DEBUG("entry:");

    return m_mediaPipelineIpc->processAudioGap(position, duration, discontinuityGap, audioAac);
}

bool MediaPipeline::setBufferingLimit(uint32_t limitBufferingMs)
{
    RIALTO_CLIENT_LOG_DEBUG("entry:");

    return m_mediaPipelineIpc->setBufferingLimit(limitBufferingMs);
}

bool MediaPipeline::getBufferingLimit(uint32_t &limitBufferingMs)
{
    RIALTO_CLIENT_LOG_DEBUG("entry:");

    return m_mediaPipelineIpc->getBufferingLimit(limitBufferingMs);
}

bool MediaPipeline::setUseBuffering(bool useBuffering)
{
    RIALTO_CLIENT_LOG_DEBUG("entry:");

    return m_mediaPipelineIpc->setUseBuffering(useBuffering);
}

bool MediaPipeline::getUseBuffering(bool &useBuffering)
{
    RIALTO_CLIENT_LOG_DEBUG("entry:");

    return m_mediaPipelineIpc->getUseBuffering(useBuffering);
}

bool MediaPipeline::switchSource(const std::unique_ptr<MediaSource> &source)
{
    RIALTO_CLIENT_LOG_DEBUG("entry:");

    return m_mediaPipelineIpc->switchSource(source);
}

void MediaPipeline::discardNeedDataRequest(uint32_t needDataRequestId)
{
    // Find the needDataRequest for this needDataRequestId
    // The needData request can be cancelled from another thread
    {
        std::lock_guard<std::mutex> lock{m_needDataRequestMapMutex};

        auto needDataRequestIt = m_needDataRequestMap.find(needDataRequestId);
        if (needDataRequestIt == m_needDataRequestMap.end())
        {
            RIALTO_CLIENT_LOG_INFO("Could not find need data request, with id %u", needDataRequestId);
        }
        else
        {
            RIALTO_CLIENT_LOG_INFO("Discarding need data request with id %u", needDataRequestId);
            m_needDataRequestMap.erase(needDataRequestIt);
        }
    }
}

std::weak_ptr<IMediaPipelineClient> MediaPipeline::getClient()
{
    return m_mediaPipelineClient;
}

void MediaPipeline::updateState(NetworkState state)
{
    State newState = m_currentState;

    switch (state)
    {
    case NetworkState::BUFFERING:
    case NetworkState::BUFFERING_PROGRESS:
    case NetworkState::STALLED:
    {
        newState = State::BUFFERING;
        break;
    }
    case NetworkState::FORMAT_ERROR:
    case NetworkState::NETWORK_ERROR:
    case NetworkState::DECODE_ERROR:
    {
        newState = State::FAILURE;
        break;
    }
    default:
    {
        break;
    }
    }

    RIALTO_CLIENT_LOG_DEBUG("Received network state '%s', old state '%s', new state '%s'", toString(state),
                            toString(m_currentState), toString(newState));
    m_currentState = newState;
}

void MediaPipeline::updateState(PlaybackState state)
{
    State newState = m_currentState;

    switch (state)
    {
    case PlaybackState::PLAYING:
    case PlaybackState::PAUSED:
    {
        newState = State::PLAYING;
        break;
    }
    case PlaybackState::SEEKING:
    {
        newState = State::SEEKING;
        break;
    }
    case PlaybackState::STOPPED:
    {
        newState = State::IDLE;
        break;
    }
    case PlaybackState::SEEK_DONE:
    {
        newState = State::BUFFERING;
        break;
    }
    case PlaybackState::END_OF_STREAM:
    {
        newState = State::END_OF_STREAM;
        break;
    }
    case PlaybackState::FAILURE:
    {
        newState = State::FAILURE;
        break;
    }
    default:
    {
        break;
    }
    }

    RIALTO_CLIENT_LOG_DEBUG("Received playback state '%s', old state '%s', new state '%s'", toString(state),
                            toString(m_currentState), toString(newState));
    m_currentState = newState;
}

void MediaPipeline::notifyPlaybackState(PlaybackState state)
{
    RIALTO_CLIENT_LOG_DEBUG("entry:");

    updateState(state);

    std::shared_ptr<IMediaPipelineClient> client = m_mediaPipelineClient.lock();
    if (client)
    {
        client->notifyPlaybackState(state);
    }
}

void MediaPipeline::notifyPosition(int64_t position)
{
    RIALTO_CLIENT_LOG_DEBUG("entry:");

    std::shared_ptr<IMediaPipelineClient> client = m_mediaPipelineClient.lock();
    if (client)
    {
        client->notifyPosition(position);
    }
}

void MediaPipeline::notifyNetworkState(NetworkState state)
{
    RIALTO_CLIENT_LOG_DEBUG("entry:");

    updateState(state);

    std::shared_ptr<IMediaPipelineClient> client = m_mediaPipelineClient.lock();
    if (client)
    {
        client->notifyNetworkState(state);
    }
}

void MediaPipeline::notifyNeedMediaData(int32_t sourceId, size_t frameCount, uint32_t requestId,
                                        const std::shared_ptr<MediaPlayerShmInfo> &shmInfo)
{
    RIALTO_CLIENT_LOG_DEBUG("entry:");

    // If attach source is ongoing wait till it has completed so that all sources are attached
    {
        std::unique_lock<std::mutex> lock{m_attachSourceMutex};
        if (m_attachingSource)
            m_attachSourceCond.wait(lock, [this] { return !m_attachingSource; });
    }

    if (MediaSourceType::UNKNOWN == m_attachedSources.getType(sourceId))
    {
        RIALTO_CLIENT_LOG_WARN("NeedMediaData received for unknown source %d, ignoring request id %u", sourceId,
                               requestId);
        return;
    }
    if (m_attachedSources.isFlushing(sourceId))
    {
        RIALTO_CLIENT_LOG_WARN("NeedMediaData received for flushing source %d, ignoring request id %u", sourceId,
                               requestId);
        return;
    }

    switch (m_currentState)
    {
    case State::BUFFERING:
    case State::PLAYING:
    {
        std::shared_ptr<NeedDataRequest> needDataRequest = std::make_shared<NeedDataRequest>();
        needDataRequest->sourceId = sourceId;
        needDataRequest->shmInfo = shmInfo;

        {
            std::lock_guard<std::mutex> lock{m_needDataRequestMapMutex};
            if (ApplicationState::RUNNING != m_currentAppState)
            {
                RIALTO_CLIENT_LOG_INFO("NeedMediaData received in state != RUNNING, ignoring request id %u", requestId);
                break;
            }
            m_needDataRequestMap[requestId] = needDataRequest;
        }

        std::shared_ptr<IMediaPipelineClient> client = m_mediaPipelineClient.lock();
        if (client)
        {
            client->notifyNeedMediaData(sourceId, frameCount, requestId, nullptr);
        }

        break;
    }
    case State::SEEKING:
    {
        RIALTO_CLIENT_LOG_INFO("NeedMediaData received while seeking, ignoring request id %u", requestId);
        break;
    }
    case State::IDLE:
    case State::END_OF_STREAM:
    case State::FAILURE:
    default:
    {
        RIALTO_CLIENT_LOG_WARN("NeedMediaData received in unexpected state '%s', ignoring request id %u",
                               toString(m_currentState), requestId);
        break;
    }
    }
}

void MediaPipeline::notifyApplicationState(ApplicationState state)
{
    RIALTO_CLIENT_LOG_DEBUG("entry:");
    std::lock_guard<std::mutex> lock{m_needDataRequestMapMutex};
    m_currentAppState = state;
    if (ApplicationState::RUNNING != state)
    {
        // If shared memory in use, wait for it to finish before returning
        m_needDataRequestMap.clear();
    }
}

void MediaPipeline::notifyQos(int32_t sourceId, const QosInfo &qosInfo)
{
    RIALTO_CLIENT_LOG_DEBUG("entry:");

    std::shared_ptr<IMediaPipelineClient> client = m_mediaPipelineClient.lock();
    if (client)
    {
        client->notifyQos(sourceId, qosInfo);
    }
}

void MediaPipeline::notifyBufferUnderflow(int32_t sourceId)
{
    RIALTO_CLIENT_LOG_DEBUG("entry:");

    std::shared_ptr<IMediaPipelineClient> client = m_mediaPipelineClient.lock();
    if (client)
    {
        client->notifyBufferUnderflow(sourceId);
    }
}

void MediaPipeline::notifyPlaybackError(int32_t sourceId, PlaybackError error)
{
    RIALTO_CLIENT_LOG_DEBUG("entry:");

    std::shared_ptr<IMediaPipelineClient> client = m_mediaPipelineClient.lock();
    if (client)
    {
        client->notifyPlaybackError(sourceId, error);
    }
}

void MediaPipeline::notifySourceFlushed(int32_t sourceId)
{
    RIALTO_CLIENT_LOG_DEBUG("entry:");
    {
        std::lock_guard<std::mutex> lock{m_flushMutex};
        m_attachedSources.setFlushing(sourceId, false);
    }
    std::shared_ptr<IMediaPipelineClient> client = m_mediaPipelineClient.lock();
    if (client)
    {
        client->notifySourceFlushed(sourceId);
    }

    State expected = State::END_OF_STREAM;
    m_currentState.compare_exchange_strong(expected, State::BUFFERING);
}

}; // namespace firebolt::rialto::client
