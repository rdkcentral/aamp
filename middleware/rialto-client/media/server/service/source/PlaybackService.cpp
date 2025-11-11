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

#include "PlaybackService.h"
#include "IMediaPipelineServerInternal.h"
#include "IWebAudioPlayerServerInternal.h"
#include "RialtoServerLogging.h"
#include <exception>
#include <future>
#include <string>
#include <utility>
#include <vector>

namespace firebolt::rialto::server::service
{
PlaybackService::PlaybackService(std::shared_ptr<IMediaPipelineServerInternalFactory> &&mediaPipelineFactory,
                                 std::shared_ptr<IMediaPipelineCapabilitiesFactory> &&mediaPipelineCapabilitiesFactory,
                                 std::shared_ptr<IWebAudioPlayerServerInternalFactory> &&webAudioPlayerFactory,
                                 std::unique_ptr<ISharedMemoryBufferFactory> &&shmBufferFactory,
                                 IDecryptionService &decryptionService)
    : m_shmBufferFactory{std::move(shmBufferFactory)}, m_isActive{false}, m_maxPlaybacks{0}, m_maxWebAudioPlayers{0},
      m_mediaPipelineService{std::make_unique<MediaPipelineService>(*this, std::move(mediaPipelineFactory),
                                                                    std::move(mediaPipelineCapabilitiesFactory),
                                                                    decryptionService)},
      m_webAudioPlayerService{std::make_unique<WebAudioPlayerService>(*this, std::move(webAudioPlayerFactory))}
{
    RIALTO_SERVER_LOG_DEBUG("PlaybackService is constructed");
}

PlaybackService::~PlaybackService()
{
    RIALTO_SERVER_LOG_DEBUG("PlaybackService is destructed");
}

bool PlaybackService::switchToActive()
{
    try
    {
        RIALTO_SERVER_LOG_INFO("Switching SessionServer to Active state.");
        m_shmBuffer = m_shmBufferFactory->createSharedMemoryBuffer(m_maxPlaybacks, m_maxWebAudioPlayers);
        m_isActive = true;
        return true;
    }
    catch (const std::exception &e)
    {
        RIALTO_SERVER_LOG_ERROR("SessionServer failed to switch to active: %s", e.what());
        m_isActive = false;
        return false;
    }
}

void PlaybackService::switchToInactive()
{
    RIALTO_SERVER_LOG_INFO("Switching SessionServer to Inactive state. Cleaning resources...");
    m_isActive = false;
    m_mediaPipelineService->clearMediaPipelines();
    m_webAudioPlayerService->clearWebAudioPlayers();
    m_shmBuffer.reset();
}

void PlaybackService::setMaxPlaybacks(int maxPlaybacks)
{
    // Method called during initialization only (before setting any state), no need to execute it on a task thread.
    m_maxPlaybacks = maxPlaybacks;
}

void PlaybackService::setMaxWebAudioPlayers(int maxWebAudio)
{
    // Method called during initialization only (before setting any state), no need to execute it on a task thread.
    m_maxWebAudioPlayers = maxWebAudio;
}

void PlaybackService::setClientDisplayName(const std::string &clientDisplayName) const
{
    // Method called during initialization only (before setting any state), no need to execute it on a task thread.
    if (!clientDisplayName.empty())
    {
        setenv("WAYLAND_DISPLAY", clientDisplayName.c_str(), 1);
    }
}

void PlaybackService::setResourceManagerAppName(const std::string &appName) const
{
    // Method called during initialization only (before setting any state), no need to execute it on a task thread.
    if (!appName.empty())
    {
        setenv("ESSRMGR_APPID", appName.c_str(), 1);
    }
}

bool PlaybackService::getSharedMemory(int32_t &fd, uint32_t &size) const
{
    auto shmBuffer = m_shmBuffer;

    if (!shmBuffer)
    {
        return false;
    }
    fd = shmBuffer->getFd();
    size = shmBuffer->getSize();
    return true;
}

bool PlaybackService::isActive() const
{
    return m_isActive;
}

int PlaybackService::getMaxPlaybacks() const
{
    return m_maxPlaybacks;
}

int PlaybackService::getMaxWebAudioPlayers() const
{
    return m_maxWebAudioPlayers;
}

std::shared_ptr<ISharedMemoryBuffer> PlaybackService::getShmBuffer() const
{
    return m_shmBuffer;
}

IMediaPipelineService &PlaybackService::getMediaPipelineService() const
{
    return *m_mediaPipelineService;
}

IWebAudioPlayerService &PlaybackService::getWebAudioPlayerService() const
{
    return *m_webAudioPlayerService;
}

void PlaybackService::ping(const std::shared_ptr<IHeartbeatProcedure> &heartbeatProcedure) const
{
    m_mediaPipelineService->ping(heartbeatProcedure);
    m_webAudioPlayerService->ping(heartbeatProcedure);
}
} // namespace firebolt::rialto::server::service
