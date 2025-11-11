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

#ifndef FIREBOLT_RIALTO_SERVER_SERVICE_I_WEB_AUDIO_PLAYER_SERVICE_H_
#define FIREBOLT_RIALTO_SERVER_SERVICE_I_WEB_AUDIO_PLAYER_SERVICE_H_

#include "IHeartbeatProcedure.h"
#include "IWebAudioPlayerClient.h"
#include "MediaCommon.h"
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace firebolt::rialto::server::service
{
class IWebAudioPlayerService
{
public:
    IWebAudioPlayerService() = default;
    virtual ~IWebAudioPlayerService() = default;

    IWebAudioPlayerService(const IWebAudioPlayerService &) = delete;
    IWebAudioPlayerService(IWebAudioPlayerService &&) = delete;
    IWebAudioPlayerService &operator=(const IWebAudioPlayerService &) = delete;
    IWebAudioPlayerService &operator=(IWebAudioPlayerService &&) = delete;

    virtual bool createWebAudioPlayer(int handle, const std::shared_ptr<IWebAudioPlayerClient> &webAudioPlayerClient,
                                      const std::string &audioMimeType, const uint32_t priority,
                                      std::weak_ptr<const WebAudioConfig> config) = 0;
    virtual bool destroyWebAudioPlayer(int handle) = 0;
    virtual bool play(int handle) = 0;
    virtual bool pause(int handle) = 0;
    virtual bool setEos(int handle) = 0;
    virtual bool getBufferAvailable(int handle, uint32_t &availableFrames,
                                    std::shared_ptr<WebAudioShmInfo> &webAudioShmInfo) = 0;
    virtual bool getBufferDelay(int handle, uint32_t &delayFrames) = 0;
    virtual bool writeBuffer(int handle, const uint32_t numberOfFrames, void *data) = 0;
    virtual bool getDeviceInfo(int handle, uint32_t &preferredFrames, uint32_t &maximumFrames,
                               bool &supportDeferredPlay) = 0;
    virtual bool setVolume(int handle, double volume) = 0;
    virtual bool getVolume(int handle, double &volume) = 0;
    virtual void ping(const std::shared_ptr<IHeartbeatProcedure> &heartbeatProcedure) = 0;
};
} // namespace firebolt::rialto::server::service

#endif // FIREBOLT_RIALTO_SERVER_SERVICE_I_WEB_AUDIO_PLAYER_SERVICE_H_
