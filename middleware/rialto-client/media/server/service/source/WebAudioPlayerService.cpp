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

#include "WebAudioPlayerService.h"
#include "IGstWebAudioPlayer.h"
#include "IMainThread.h"
#include "ITimer.h"
#include "IWebAudioPlayer.h"
#include "IWebAudioPlayerServerInternal.h"
#include "RialtoServerLogging.h"
#include <exception>
#include <future>
#include <string>
#include <utility>
#include <vector>

namespace firebolt::rialto::server::service
{
WebAudioPlayerService::WebAudioPlayerService(IPlaybackService &playbackService,
                                             std::shared_ptr<IWebAudioPlayerServerInternalFactory> &&webAudioPlayerFactory)
    : m_playbackService{playbackService}, m_webAudioPlayerFactory{webAudioPlayerFactory}
{
    RIALTO_SERVER_LOG_DEBUG("WebAudioPlayerService is constructed");
}

WebAudioPlayerService::~WebAudioPlayerService()
{
    RIALTO_SERVER_LOG_DEBUG("WebAudioPlayerService is destructed");
}

void WebAudioPlayerService::clearWebAudioPlayers()
{
    std::lock_guard<std::mutex> lock{m_webAudioPlayerMutex};
    m_webAudioPlayers.clear();
}

bool WebAudioPlayerService::createWebAudioPlayer(int handle,
                                                 const std::shared_ptr<IWebAudioPlayerClient> &webAudioPlayerClient,
                                                 const std::string &audioMimeType, const uint32_t priority,
                                                 std::weak_ptr<const WebAudioConfig> config)
{
    RIALTO_SERVER_LOG_DEBUG("WebAudioPlayerService requested to create new WebAudioPlayer with id: %d", handle);
    if (!m_playbackService.isActive())
    {
        RIALTO_SERVER_LOG_ERROR("Skip create WebAudioPlayer with id: %d - Session Server in Inactive state", handle);
        return false;
    }

    {
        std::lock_guard<std::mutex> lock{m_webAudioPlayerMutex};
        if (m_webAudioPlayers.size() == static_cast<size_t>(m_playbackService.getMaxWebAudioPlayers()))
        {
            RIALTO_SERVER_LOG_ERROR("Unable to create WebAudioPlayer with id: %d. Max instance number reached.", handle);
            return false;
        }
        if (m_webAudioPlayers.find(handle) != m_webAudioPlayers.end())
        {
            RIALTO_SERVER_LOG_ERROR("WebAudioPlayer with handle: %d already exists", handle);
            return false;
        }
        auto shmBuffer = m_playbackService.getShmBuffer();

        m_webAudioPlayers.emplace(
            std::make_pair(handle, m_webAudioPlayerFactory
                                       ->createWebAudioPlayerServerInternal(webAudioPlayerClient, audioMimeType,
                                                                            priority, config, shmBuffer, handle,
                                                                            IMainThreadFactory::createFactory(),
                                                                            IGstWebAudioPlayerFactory::getFactory(),
                                                                            common::ITimerFactory::getFactory())));
        if (!m_webAudioPlayers.at(handle))
        {
            RIALTO_SERVER_LOG_ERROR("Could not create WebAudioPlayer for handle: %d", handle);
            m_webAudioPlayers.erase(handle);
            return false;
        }
    }

    RIALTO_SERVER_LOG_INFO("New WebAudioPlayer: %d created", handle);
    return true;
}

bool WebAudioPlayerService::destroyWebAudioPlayer(int handle)
{
    RIALTO_SERVER_LOG_DEBUG("WebAudioPlayerService requested to destroy WebAudioPlayer with handle: %d", handle);
    {
        std::lock_guard<std::mutex> lock{m_webAudioPlayerMutex};
        auto webAudioPlayerIter = m_webAudioPlayers.find(handle);
        if (webAudioPlayerIter == m_webAudioPlayers.end())
        {
            RIALTO_SERVER_LOG_ERROR("WebAudioPlayer with handle: %d does not exists", handle);
            return false;
        }
        m_webAudioPlayers.erase(webAudioPlayerIter);
    }
    RIALTO_SERVER_LOG_INFO("WebAudioPlayer: %d destroyed", handle);
    return true;
}

bool WebAudioPlayerService::play(int handle)
{
    RIALTO_SERVER_LOG_INFO("WebAudioPlayerService requested to play WebAudioPlayer with handle: %d", handle);

    std::lock_guard<std::mutex> lock{m_webAudioPlayerMutex};
    auto webAudioPlayerIter = m_webAudioPlayers.find(handle);
    if (webAudioPlayerIter == m_webAudioPlayers.end())
    {
        RIALTO_SERVER_LOG_ERROR("WebAudioPlayer with handle: %d does not exists", handle);
        return false;
    }
    return webAudioPlayerIter->second->play();
}

bool WebAudioPlayerService::pause(int handle)
{
    RIALTO_SERVER_LOG_INFO("WebAudioPlayerService requested to pause WebAudioPlayer with handle: %d", handle);

    std::lock_guard<std::mutex> lock{m_webAudioPlayerMutex};
    auto webAudioPlayerIter = m_webAudioPlayers.find(handle);
    if (webAudioPlayerIter == m_webAudioPlayers.end())
    {
        RIALTO_SERVER_LOG_ERROR("WebAudioPlayer with handle: %d does not exists", handle);
        return false;
    }
    return webAudioPlayerIter->second->pause();
}

bool WebAudioPlayerService::setEos(int handle)
{
    RIALTO_SERVER_LOG_INFO("WebAudioPlayerService requested to setEos WebAudioPlayer with handle: %d", handle);

    std::lock_guard<std::mutex> lock{m_webAudioPlayerMutex};
    auto webAudioPlayerIter = m_webAudioPlayers.find(handle);
    if (webAudioPlayerIter == m_webAudioPlayers.end())
    {
        RIALTO_SERVER_LOG_ERROR("WebAudioPlayer with handle: %d does not exists", handle);
        return false;
    }
    return webAudioPlayerIter->second->setEos();
}

bool WebAudioPlayerService::getBufferAvailable(int handle, uint32_t &availableFrames,
                                               std::shared_ptr<WebAudioShmInfo> &webAudioShmInfo)
{
    RIALTO_SERVER_LOG_INFO("WebAudioPlayerService requested to getBufferAvailable WebAudioPlayer with handle: %d",
                           handle);

    std::lock_guard<std::mutex> lock{m_webAudioPlayerMutex};
    auto webAudioPlayerIter = m_webAudioPlayers.find(handle);
    if (webAudioPlayerIter == m_webAudioPlayers.end())
    {
        RIALTO_SERVER_LOG_ERROR("WebAudioPlayer with handle: %d does not exists", handle);
        return false;
    }
    return webAudioPlayerIter->second->getBufferAvailable(availableFrames, webAudioShmInfo);
}

bool WebAudioPlayerService::getBufferDelay(int handle, uint32_t &delayFrames)
{
    RIALTO_SERVER_LOG_INFO("WebAudioPlayerService requested to getBufferDelay WebAudioPlayer with handle: %d", handle);

    std::lock_guard<std::mutex> lock{m_webAudioPlayerMutex};
    auto webAudioPlayerIter = m_webAudioPlayers.find(handle);
    if (webAudioPlayerIter == m_webAudioPlayers.end())
    {
        RIALTO_SERVER_LOG_ERROR("WebAudioPlayer with handle: %d does not exists", handle);
        return false;
    }
    return webAudioPlayerIter->second->getBufferDelay(delayFrames);
}

bool WebAudioPlayerService::writeBuffer(int handle, const uint32_t numberOfFrames, void *data)
{
    RIALTO_SERVER_LOG_INFO("WebAudioPlayerService requested to writeBuffer WebAudioPlayer with handle: %d", handle);

    std::lock_guard<std::mutex> lock{m_webAudioPlayerMutex};
    auto webAudioPlayerIter = m_webAudioPlayers.find(handle);
    if (webAudioPlayerIter == m_webAudioPlayers.end())
    {
        RIALTO_SERVER_LOG_ERROR("WebAudioPlayer with handle: %d does not exists", handle);
        return false;
    }
    return webAudioPlayerIter->second->writeBuffer(numberOfFrames, data);
}

bool WebAudioPlayerService::getDeviceInfo(int handle, uint32_t &preferredFrames, uint32_t &maximumFrames,
                                          bool &supportDeferredPlay)
{
    RIALTO_SERVER_LOG_INFO("WebAudioPlayerService requested to getDeviceInfo WebAudioPlayer with handle: %d", handle);

    std::lock_guard<std::mutex> lock{m_webAudioPlayerMutex};
    auto webAudioPlayerIter = m_webAudioPlayers.find(handle);
    if (webAudioPlayerIter == m_webAudioPlayers.end())
    {
        RIALTO_SERVER_LOG_ERROR("WebAudioPlayer with handle: %d does not exists", handle);
        return false;
    }
    return webAudioPlayerIter->second->getDeviceInfo(preferredFrames, maximumFrames, supportDeferredPlay);
}

bool WebAudioPlayerService::setVolume(int handle, double volume)
{
    RIALTO_SERVER_LOG_INFO("WebAudioPlayerService requested to setVolume WebAudioPlayer with handle: %d", handle);

    std::lock_guard<std::mutex> lock{m_webAudioPlayerMutex};
    auto webAudioPlayerIter = m_webAudioPlayers.find(handle);
    if (webAudioPlayerIter == m_webAudioPlayers.end())
    {
        RIALTO_SERVER_LOG_ERROR("WebAudioPlayer with handle: %d does not exists", handle);
        return false;
    }
    return webAudioPlayerIter->second->setVolume(volume);
}

bool WebAudioPlayerService::getVolume(int handle, double &volume)
{
    RIALTO_SERVER_LOG_INFO("WebAudioPlayerService requested to getVolume WebAudioPlayer with handle: %d", handle);

    std::lock_guard<std::mutex> lock{m_webAudioPlayerMutex};
    auto webAudioPlayerIter = m_webAudioPlayers.find(handle);
    if (webAudioPlayerIter == m_webAudioPlayers.end())
    {
        RIALTO_SERVER_LOG_ERROR("WebAudioPlayer with handle: %d does not exists", handle);
        return false;
    }
    return webAudioPlayerIter->second->getVolume(volume);
}

void WebAudioPlayerService::ping(const std::shared_ptr<IHeartbeatProcedure> &heartbeatProcedure)
{
    RIALTO_SERVER_LOG_DEBUG("Ping requested");
    std::lock_guard<std::mutex> lock{m_webAudioPlayerMutex};
    for (const auto &webAudioPlayersPair : m_webAudioPlayers)
    {
        auto &webAudioPlayer = webAudioPlayersPair.second;
        webAudioPlayer->ping(heartbeatProcedure->createHandler());
    }
}
} // namespace firebolt::rialto::server::service
