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

#ifndef FIREBOLT_RIALTO_SERVER_SERVICE_I_PLAYBACK_SERVICE_H_
#define FIREBOLT_RIALTO_SERVER_SERVICE_I_PLAYBACK_SERVICE_H_

#include "IMediaPipelineService.h"
#include "ISharedMemoryBuffer.h"
#include "IWebAudioPlayerService.h"
#include "MediaCommon.h"
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace firebolt::rialto::server::service
{
class IPlaybackService
{
public:
    IPlaybackService() = default;
    virtual ~IPlaybackService() = default;

    IPlaybackService(const IPlaybackService &) = delete;
    IPlaybackService(IPlaybackService &&) = delete;
    IPlaybackService &operator=(const IPlaybackService &) = delete;
    IPlaybackService &operator=(IPlaybackService &&) = delete;

    virtual bool switchToActive() = 0;
    virtual void switchToInactive() = 0;
    virtual void setMaxPlaybacks(int maxPlaybacks) = 0;
    virtual void setMaxWebAudioPlayers(int maxWebAudio) = 0;
    virtual void setClientDisplayName(const std::string &clientDisplayName) const = 0;
    virtual void setResourceManagerAppName(const std::string &appName) const = 0;

    virtual bool isActive() const = 0;
    virtual bool getSharedMemory(int32_t &fd, uint32_t &size) const = 0;
    virtual int getMaxPlaybacks() const = 0;
    virtual int getMaxWebAudioPlayers() const = 0;
    virtual std::shared_ptr<ISharedMemoryBuffer> getShmBuffer() const = 0;
    virtual IMediaPipelineService &getMediaPipelineService() const = 0;
    virtual IWebAudioPlayerService &getWebAudioPlayerService() const = 0;
    virtual void ping(const std::shared_ptr<IHeartbeatProcedure> &heartbeatProcedure) const = 0;
};
} // namespace firebolt::rialto::server::service

#endif // FIREBOLT_RIALTO_SERVER_SERVICE_I_PLAYBACK_SERVICE_H_
