/*
 * If not stated otherwise in this file or this component's LICENSE file the
 * following copyright and licenses apply:
 *
 * Copyright 2023 
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

#include <cstring>
#include <limits.h>
#include <mutex>
#include <stdexcept>

#include "IWebAudioPlayerIpc.h"
#include "IWebAudioPlayerIpcClient.h"
#include "RialtoClientLogging.h"
#include "WebAudioPlayer.h"
#include "WebAudioPlayerProxy.h"

namespace firebolt::rialto
{
std::shared_ptr<IWebAudioPlayerFactory> IWebAudioPlayerFactory::createFactory()
{
    std::shared_ptr<IWebAudioPlayerFactory> factory;

    try
    {
        factory = std::make_shared<WebAudioPlayerFactory>();
    }
    catch (const std::exception &e)
    {
        RIALTO_CLIENT_LOG_ERROR("Failed to create the web audio player factory, reason: %s", e.what());
    }

    return factory;
}

std::unique_ptr<IWebAudioPlayer>
WebAudioPlayerFactory::createWebAudioPlayer(std::weak_ptr<IWebAudioPlayerClient> client, const std::string &audioMimeType,
                                            const uint32_t priority, std::weak_ptr<const WebAudioConfig> config) const
{
    return createWebAudioPlayer(client, audioMimeType, priority, config, {}, {});
}

std::unique_ptr<IWebAudioPlayer>
WebAudioPlayerFactory::createWebAudioPlayer(std::weak_ptr<IWebAudioPlayerClient> client, const std::string &audioMimeType,
                                            const uint32_t priority, std::weak_ptr<const WebAudioConfig> config,
                                            std::weak_ptr<client::IWebAudioPlayerIpcFactory> webAudioPlayerIpcFactory,
                                            std::weak_ptr<client::IClientController> clientController) const
{
    std::unique_ptr<IWebAudioPlayer> webAudioPlayer;
    try
    {
        std::shared_ptr<client::IWebAudioPlayerIpcFactory> webAudioPlayerIpcFactoryLocked =
            webAudioPlayerIpcFactory.lock();
        std::shared_ptr<client::IClientController> clientControllerLocked = clientController.lock();
        client::IClientController &cc = clientControllerLocked
                                            ? *clientControllerLocked
                                            : client::IClientControllerAccessor::instance().getClientController();

        auto wap{std::make_shared<client::WebAudioPlayer>(client, audioMimeType, priority, config,
                                                          webAudioPlayerIpcFactoryLocked
                                                              ? webAudioPlayerIpcFactoryLocked
                                                              : client::IWebAudioPlayerIpcFactory::getFactory(),
                                                          cc)};
        webAudioPlayer = std::make_unique<WebAudioPlayerProxy>(wap, cc);
    }
    catch (const std::exception &e)
    {
        RIALTO_CLIENT_LOG_ERROR("Failed to create the web audio player, reason: %s", e.what());
    }

    return webAudioPlayer;
}

WebAudioPlayerProxy::WebAudioPlayerProxy(const std::shared_ptr<IWebAudioPlayerAndIControlClient> &ptr,
                                         client::IClientController &clientController)
    : m_webAudioPlayer(ptr), m_clientController{clientController}
{
    ApplicationState state{ApplicationState::UNKNOWN};
    if (!m_clientController.registerClient(m_webAudioPlayer, state))
    {
        throw std::runtime_error("Failed to register client with clientController");
    }
    m_webAudioPlayer->notifyApplicationState(state);
}

WebAudioPlayerProxy::~WebAudioPlayerProxy()
{
    if (!m_clientController.unregisterClient(m_webAudioPlayer))
    {
        RIALTO_CLIENT_LOG_WARN("Failed to unregister client with clientController");
    }
}

}; // namespace firebolt::rialto

namespace firebolt::rialto::client
{
WebAudioPlayer::WebAudioPlayer(std::weak_ptr<IWebAudioPlayerClient> client, const std::string &audioMimeType,
                               const uint32_t priority, std::weak_ptr<const WebAudioConfig> webAudioConfig,
                               const std::shared_ptr<IWebAudioPlayerIpcFactory> &webAudioPlayerIpcFactory,
                               IClientController &clientController)
    : m_webAudioPlayerClient(client), m_clientController{clientController}, m_bytesPerFrame{0},
      m_currentAppState{ApplicationState::UNKNOWN}
{
    RIALTO_CLIENT_LOG_DEBUG("entry:");

    if (audioMimeType == "audio/x-raw")
    {
        std::shared_ptr<const WebAudioConfig> kConfig = webAudioConfig.lock();
        if (!kConfig)
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

    if (!webAudioPlayerIpcFactory)
    {
        throw std::runtime_error("Web audio player ipc factory could not be null");
    }

    m_webAudioPlayerIpc =
        webAudioPlayerIpcFactory->createWebAudioPlayerIpc(this, audioMimeType, priority, webAudioConfig);
    if (!m_webAudioPlayerIpc)
    {
        throw std::runtime_error("Web audio player ipc could not be created");
    }
}

WebAudioPlayer::~WebAudioPlayer()
{
    RIALTO_CLIENT_LOG_DEBUG("entry:");

    m_webAudioPlayerIpc.reset();
}

bool WebAudioPlayer::play()
{
    RIALTO_CLIENT_LOG_DEBUG("entry:");

    return m_webAudioPlayerIpc->play();
}

bool WebAudioPlayer::pause()
{
    RIALTO_CLIENT_LOG_DEBUG("entry:");

    return m_webAudioPlayerIpc->pause();
}

bool WebAudioPlayer::setEos()
{
    RIALTO_CLIENT_LOG_DEBUG("entry:");

    return m_webAudioPlayerIpc->setEos();
}

bool WebAudioPlayer::getBufferAvailable(uint32_t &availableFrames, std::shared_ptr<WebAudioShmInfo> &)
{
    RIALTO_CLIENT_LOG_DEBUG("entry:");
    std::lock_guard<std::mutex> bufLocker(m_bufLock);
    if (!m_webAudioShmInfo)
    {
        m_webAudioShmInfo = std::make_shared<WebAudioShmInfo>();
    }
    return m_webAudioPlayerIpc->getBufferAvailable(availableFrames, m_webAudioShmInfo);
}

bool WebAudioPlayer::getBufferDelay(uint32_t &delayFrames)
{
    RIALTO_CLIENT_LOG_DEBUG("entry:");

    return m_webAudioPlayerIpc->getBufferDelay(delayFrames);
}

bool WebAudioPlayer::writeBuffer(const uint32_t numberOfFrames, void *data)
{
    RIALTO_CLIENT_LOG_DEBUG("entry:");

    std::lock_guard<std::mutex> bufLocker(m_bufLock);
    if (!m_webAudioShmInfo)
    {
        RIALTO_CLIENT_LOG_ERROR("Web audio shared info is null!");
        return false;
    }

    uint32_t dataLength = numberOfFrames * m_bytesPerFrame;
    uint32_t availableDataLength = m_webAudioShmInfo->lengthMain + m_webAudioShmInfo->lengthWrap;
    if (dataLength > availableDataLength)
    {
        RIALTO_CLIENT_LOG_ERROR("The number of frames to write exceeds the available space!");
        return false;
    }

    if (ApplicationState::RUNNING != m_currentAppState)
    {
        RIALTO_CLIENT_LOG_ERROR("Current ApplicationState is not RUNNING!");
        return false;
    }

    std::shared_ptr<ISharedMemoryHandle> shmHandle = m_clientController.getSharedMemoryHandle();
    if (nullptr == shmHandle || nullptr == shmHandle->getShm())
    {
        RIALTO_CLIENT_LOG_ERROR("Shared buffer no longer valid");
        return false;
    }

    if (dataLength > m_webAudioShmInfo->lengthMain)
    {
        std::memcpy(shmHandle->getShm() + m_webAudioShmInfo->offsetMain, data, m_webAudioShmInfo->lengthMain);
        std::memcpy(shmHandle->getShm() + m_webAudioShmInfo->offsetWrap,
                    reinterpret_cast<uint8_t *>(data) + m_webAudioShmInfo->lengthMain,
                    dataLength - m_webAudioShmInfo->lengthMain);
    }
    else
    {
        std::memcpy(shmHandle->getShm() + m_webAudioShmInfo->offsetMain, data, dataLength);
    }
    return m_webAudioPlayerIpc->writeBuffer(numberOfFrames);
}

bool WebAudioPlayer::getDeviceInfo(uint32_t &preferredFrames, uint32_t &maximumFrames, bool &supportDeferredPlay)
{
    RIALTO_CLIENT_LOG_DEBUG("entry:");

    return m_webAudioPlayerIpc->getDeviceInfo(preferredFrames, maximumFrames, supportDeferredPlay);
}

bool WebAudioPlayer::setVolume(double volume)
{
    RIALTO_CLIENT_LOG_DEBUG("entry:");

    return m_webAudioPlayerIpc->setVolume(volume);
}

bool WebAudioPlayer::getVolume(double &volume)
{
    RIALTO_CLIENT_LOG_DEBUG("entry:");

    return m_webAudioPlayerIpc->getVolume(volume);
}

std::weak_ptr<IWebAudioPlayerClient> WebAudioPlayer::getClient()
{
    return m_webAudioPlayerClient;
}

void WebAudioPlayer::notifyState(WebAudioPlayerState state)
{
    std::shared_ptr<IWebAudioPlayerClient> client = m_webAudioPlayerClient.lock();
    if (client)
    {
        client->notifyState(state);
    }
}

void WebAudioPlayer::notifyApplicationState(ApplicationState state)
{
    m_currentAppState = state;
}

}; // namespace firebolt::rialto::client
