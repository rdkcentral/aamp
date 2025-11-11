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

#ifndef FIREBOLT_RIALTO_SERVER_SERVICE_PLAYBACK_SERVICE_H_
#define FIREBOLT_RIALTO_SERVER_SERVICE_PLAYBACK_SERVICE_H_

#include "IDecryptionService.h"
#include "IHeartbeatProcedure.h"
#include "IMediaPipelineCapabilities.h"
#include "IMediaPipelineServerInternal.h"
#include "IPlaybackService.h"
#include "ISharedMemoryBuffer.h"
#include "IWebAudioPlayerServerInternal.h"
#include "MediaPipelineService.h"
#include "WebAudioPlayerService.h"
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
class PlaybackService : public IPlaybackService
{
public:
    PlaybackService(std::shared_ptr<IMediaPipelineServerInternalFactory> &&mediaPipelineFactory,
                    std::shared_ptr<IMediaPipelineCapabilitiesFactory> &&mediaPipelineCapabilitiesFactory,
                    std::shared_ptr<IWebAudioPlayerServerInternalFactory> &&webAudioPlayerFactory,
                    std::unique_ptr<ISharedMemoryBufferFactory> &&shmBufferFactory,
                    IDecryptionService &decryptionService);
    ~PlaybackService() override;
    PlaybackService(const PlaybackService &) = delete;
    PlaybackService(PlaybackService &&) = delete;
    PlaybackService &operator=(const PlaybackService &) = delete;
    PlaybackService &operator=(PlaybackService &&) = delete;

    bool switchToActive() override;
    void switchToInactive() override;
    void setMaxPlaybacks(int maxPlaybacks) override;
    void setMaxWebAudioPlayers(int maxWebAudio) override;
    void setClientDisplayName(const std::string &clientDisplayName) const override;
    void setResourceManagerAppName(const std::string &clientDisplayName) const override;

    bool isActive() const override;
    bool getSharedMemory(int32_t &fd, uint32_t &size) const override;
    int getMaxPlaybacks() const override;
    int getMaxWebAudioPlayers() const override;
    std::shared_ptr<ISharedMemoryBuffer> getShmBuffer() const override;
    IMediaPipelineService &getMediaPipelineService() const override;
    IWebAudioPlayerService &getWebAudioPlayerService() const override;
    void ping(const std::shared_ptr<IHeartbeatProcedure> &heartbeatProcedure) const override;

private:
    std::unique_ptr<ISharedMemoryBufferFactory> m_shmBufferFactory;
    std::atomic<bool> m_isActive;
    std::atomic<int> m_maxPlaybacks;
    std::atomic<int> m_maxWebAudioPlayers;
    std::shared_ptr<ISharedMemoryBuffer> m_shmBuffer;
    std::unique_ptr<MediaPipelineService> m_mediaPipelineService;
    std::unique_ptr<WebAudioPlayerService> m_webAudioPlayerService;
};
} // namespace firebolt::rialto::server::service

#endif // FIREBOLT_RIALTO_SERVER_SERVICE_PLAYBACK_SERVICE_H_
