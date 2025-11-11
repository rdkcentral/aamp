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

#ifndef FIREBOLT_RIALTO_CLIENT_WEB_AUDIO_PLAYER_IPC_H_
#define FIREBOLT_RIALTO_CLIENT_WEB_AUDIO_PLAYER_IPC_H_

#include "IEventThread.h"
#include "IWebAudioPlayerIpc.h"
#include "IpcModule.h"
#include <IWebAudioPlayer.h>
#include <memory>
#include <string>

#include "webaudioplayermodule.pb.h"

namespace firebolt::rialto::client
{
/**
 * @brief IWebAudioPlayerIpc factory class definition.
 */
class WebAudioPlayerIpcFactory : public IWebAudioPlayerIpcFactory
{
public:
    std::unique_ptr<IWebAudioPlayerIpc>
    createWebAudioPlayerIpc(IWebAudioPlayerIpcClient *client, const std::string &audioMimeType, const uint32_t priority,
                            std::weak_ptr<const WebAudioConfig> config, std::weak_ptr<IIpcClient> ipcClient) override;
};

/**
 * @brief The definition of the WebAudioPlayerIpc.
 */
class WebAudioPlayerIpc : public IWebAudioPlayerIpc, public IpcModule
{
public:
    /**
     * @brief The constructor.
     *
     * @param[in] client                : The Rialto ipc web audio player client.
     * @param[in] audioMimeType         : The audio encoding format, currently only "audio/x-raw" (PCM)
     * @param[in] priority              : Priority value for this pipeline.
     * @param[in] config                : Additional type dependent configuration data or nullptr
     * @param[in] ipcClient             : The ipc client
     * @param[in] eventThreadFactory    : The event thread factory
     */
    WebAudioPlayerIpc(IWebAudioPlayerIpcClient *client, const std::string &audioMimeType, const uint32_t priority,
                      std::weak_ptr<const WebAudioConfig> config, IIpcClient &ipcClient,
                      const std::shared_ptr<common::IEventThreadFactory> &eventThreadFactory);

    virtual ~WebAudioPlayerIpc();

    bool play() override;

    bool pause() override;

    bool setEos() override;

    bool getBufferAvailable(uint32_t &availableFrames, const std::shared_ptr<WebAudioShmInfo> &webAudioShmInfo) override;

    bool getBufferDelay(uint32_t &delayFrames) override;

    bool writeBuffer(const uint32_t numberOfFrames) override;

    bool getDeviceInfo(uint32_t &preferredFrames, uint32_t &maximumFrames, bool &supportDeferredPlay) override;

    bool setVolume(double volume) override;

    bool getVolume(double &volume) override;

private:
    bool createWebAudioPlayer(const std::string &audioMimeType, const uint32_t priority,
                              std::weak_ptr<const WebAudioConfig> config);

    void destroyWebAudioPlayer();

    bool createRpcStubs(const std::shared_ptr<ipc::IChannel> &ipcChannel) override;

    bool subscribeToEvents(const std::shared_ptr<ipc::IChannel> &ipcChannel) override;

    void onPlaybackStateUpdated(const std::shared_ptr<firebolt::rialto::WebAudioPlayerStateEvent> &event);
    /**
     * @brief The web audio player client ipc.
     */
    IWebAudioPlayerIpcClient *m_webAudioPlayerIpcClient;

    /**
     * @brief The ipc protobuf web audio player stub.
     */
    std::unique_ptr<::firebolt::rialto::WebAudioPlayerModule_Stub> m_webAudioPlayerStub;

    /**
     * @brief Thread for handling web audio player events from the server.
     */
    std::unique_ptr<common::IEventThread> m_eventThread;

    /**
     * @brief Handle to an IPC connection to the server.
     */
    int32_t m_webAudioPlayerHandle;
};

}; // namespace firebolt::rialto::client

#endif // FIREBOLT_RIALTO_CLIENT_WEB_AUDIO_PLAYER_IPC_H_
