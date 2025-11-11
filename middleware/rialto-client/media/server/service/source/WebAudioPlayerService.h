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

#ifndef FIREBOLT_RIALTO_SERVER_SERVICE_WEB_AUDIO_PLAYER_SERVICE_H_
#define FIREBOLT_RIALTO_SERVER_SERVICE_WEB_AUDIO_PLAYER_SERVICE_H_

#include "IPlaybackService.h"
#include "IWebAudioPlayerServerInternal.h"
#include "IWebAudioPlayerService.h"
#include <atomic>
#include <condition_variable>
#include <functional>
#include <map>
#include <memory>
#include <mutex>
#include <queue>
#include <string>
#include <thread>
#include <vector>

namespace firebolt::rialto::server::service
{
class WebAudioPlayerService : public IWebAudioPlayerService
{
public:
    WebAudioPlayerService(IPlaybackService &playbackService,
                          std::shared_ptr<IWebAudioPlayerServerInternalFactory> &&webAudioPlayerFactory);
    ~WebAudioPlayerService() override;
    WebAudioPlayerService(const WebAudioPlayerService &) = delete;
    WebAudioPlayerService(WebAudioPlayerService &&) = delete;
    WebAudioPlayerService &operator=(const WebAudioPlayerService &) = delete;
    WebAudioPlayerService &operator=(WebAudioPlayerService &&) = delete;

    bool createWebAudioPlayer(int handle, const std::shared_ptr<IWebAudioPlayerClient> &webAudioPlayerClient,
                              const std::string &audioMimeType, const uint32_t priority,
                              std::weak_ptr<const WebAudioConfig> config) override;
    bool destroyWebAudioPlayer(int handle) override;
    bool play(int handle) override;
    bool pause(int handle) override;
    bool setEos(int handle) override;
    bool getBufferAvailable(int handle, uint32_t &availableFrames,
                            std::shared_ptr<WebAudioShmInfo> &webAudioShmInfo) override;
    bool getBufferDelay(int handle, uint32_t &delayFrames) override;
    bool writeBuffer(int handle, const uint32_t numberOfFrames, void *data) override;
    bool getDeviceInfo(int handle, uint32_t &preferredFrames, uint32_t &maximumFrames, bool &supportDeferredPlay) override;
    bool setVolume(int handle, double volume) override;
    bool getVolume(int handle, double &volume) override;
    void ping(const std::shared_ptr<IHeartbeatProcedure> &heartbeatProcedure) override;

    void clearWebAudioPlayers();

private:
    IPlaybackService &m_playbackService;
    std::shared_ptr<IWebAudioPlayerServerInternalFactory> m_webAudioPlayerFactory;
    std::map<int, std::unique_ptr<IWebAudioPlayerServerInternal>> m_webAudioPlayers;
    std::mutex m_webAudioPlayerMutex;
};
} // namespace firebolt::rialto::server::service

#endif // FIREBOLT_RIALTO_SERVER_SERVICE_WEB_AUDIO_PLAYER_SERVICE_H_
