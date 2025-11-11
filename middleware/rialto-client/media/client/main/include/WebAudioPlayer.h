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

#ifndef FIREBOLT_RIALTO_CLIENT_WEB_AUDIO_PLAYER_H_
#define FIREBOLT_RIALTO_CLIENT_WEB_AUDIO_PLAYER_H_

#include "IClientController.h"
#include "IControlClient.h"
#include "IWebAudioPlayer.h"
#include "IWebAudioPlayerIpc.h"
#include "IWebAudioPlayerIpcClient.h"
#include <atomic>
#include <memory>
#include <mutex>
#include <stdint.h>
#include <string>

namespace firebolt::rialto
{
/**
 * @brief IWebAudioPlayer factory class definition.
 */
class WebAudioPlayerFactory : public IWebAudioPlayerFactory
{
public:
    WebAudioPlayerFactory() = default;
    ~WebAudioPlayerFactory() override = default;

    std::unique_ptr<IWebAudioPlayer> createWebAudioPlayer(std::weak_ptr<IWebAudioPlayerClient> client,
                                                          const std::string &audioMimeType, const uint32_t priority,
                                                          std::weak_ptr<const WebAudioConfig> config) const override;

    /**
     * @brief IWebAudioPlayer factory method with factory parameters for mock injection.
     *
     * @param[in] client                    : The Web Audio Player client.
     * @param[in] audioMimeType             : The audio encoding format, currently only "audio/x-raw" (PCM).
     * @param[in] priority                  : Priority value for this pipeline.
     * @param[in] config                    : Additional type dependent configuration data or nullptr.
     * @param[in] webAudioPlayerIpcFactory  : This was added for the test environment where a mock object needs to be
     * passed in.
     * @param[in] clientController          : This was added for the test environment where a mock object needs to be
     * passed in.
     *
     * @retval the new Web Audio Player instance or null on error.
     */
    virtual std::unique_ptr<IWebAudioPlayer>
    createWebAudioPlayer(std::weak_ptr<IWebAudioPlayerClient> client, const std::string &audioMimeType,
                         const uint32_t priority, std::weak_ptr<const WebAudioConfig> config,
                         std::weak_ptr<client::IWebAudioPlayerIpcFactory> webAudioPlayerIpcFactory,
                         std::weak_ptr<client::IClientController> clientController) const;
};

}; // namespace firebolt::rialto

namespace firebolt::rialto::client
{
class IWebAudioPlayerAndIControlClient : public IWebAudioPlayer, public IControlClient
{
};

/**
 * @brief The definition of the WebAudioPlayer.
 */
class WebAudioPlayer : public IWebAudioPlayerAndIControlClient, public client::IWebAudioPlayerIpcClient
{
public:
    /**
     * @brief The constructor.
     *
     * @param[in] client        : The Web Audio Player client
     * @param[in] audioMimeType : The audio encoding format, currently only "audio/x-raw" (PCM)
     * @param[in] priority      : Priority value for this pipeline.
     * @param[in] config        : Additional type dependent configuration data or nullptr
     */
    WebAudioPlayer(std::weak_ptr<IWebAudioPlayerClient> client, const std::string &audioMimeType,
                   const uint32_t priority, std::weak_ptr<const WebAudioConfig> config,
                   const std::shared_ptr<IWebAudioPlayerIpcFactory> &webAudioPlayerIpcFactory,
                   IClientController &clientController);

    /**
     * @brief Virtual destructor.
     */
    virtual ~WebAudioPlayer();

    bool play() override;

    bool pause() override;

    bool setEos() override;

    bool getBufferAvailable(uint32_t &availableFrames, std::shared_ptr<WebAudioShmInfo> &webAudioShmInfo) override;

    bool getBufferDelay(uint32_t &delayFrames) override;

    bool writeBuffer(const uint32_t numberOfFrames, void *data) override;

    bool getDeviceInfo(uint32_t &preferredFrames, uint32_t &maximumFrames, bool &supportDeferredPlay) override;

    bool setVolume(double volume) override;

    bool getVolume(double &volume) override;

    std::weak_ptr<IWebAudioPlayerClient> getClient() override;

    void notifyState(WebAudioPlayerState state) override;

    void notifyApplicationState(ApplicationState state) override;

protected:
    /**
     * @brief The web audio player client.
     */
    std::weak_ptr<IWebAudioPlayerClient> m_webAudioPlayerClient;

    /**
     * @brief The media player ipc object.
     */
    std::unique_ptr<IWebAudioPlayerIpc> m_webAudioPlayerIpc;

    /**
     * @brief The rialto client controller object.
     */
    IClientController &m_clientController;

    /**
     * @brief The shared memory region info.
     */
    std::shared_ptr<WebAudioShmInfo> m_webAudioShmInfo;

    /**
     * @brief Ensure thread safety for clients by preventing concurrent writing to the buffer.
     */
    std::mutex m_bufLock;

    /**
     * @brief The bytes per frame for this audio playback.
     */
    uint32_t m_bytesPerFrame;

    /**
     * @brief The current application state.
     */
    std::atomic<ApplicationState> m_currentAppState;
};

}; // namespace firebolt::rialto::client

#endif // FIREBOLT_RIALTO_CLIENT_WEB_AUDIO_PLAYER_H_
