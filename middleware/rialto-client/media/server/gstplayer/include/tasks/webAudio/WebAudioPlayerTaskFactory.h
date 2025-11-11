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

#ifndef FIREBOLT_RIALTO_SERVER_WEB_AUDIO_PLAYER_TASK_FACTORY_H_
#define FIREBOLT_RIALTO_SERVER_WEB_AUDIO_PLAYER_TASK_FACTORY_H_

#include "IGlibWrapper.h"
#include "IGstWebAudioPlayerClient.h"
#include "IGstWrapper.h"
#include "IWebAudioPlayerTaskFactory.h"
#include <memory>
#include <string>

namespace firebolt::rialto::server
{
class WebAudioPlayerTaskFactory : public IWebAudioPlayerTaskFactory
{
public:
    WebAudioPlayerTaskFactory(IGstWebAudioPlayerClient *client,
                              const std::shared_ptr<firebolt::rialto::wrappers::IGstWrapper> &gstWrapper,
                              const std::shared_ptr<firebolt::rialto::wrappers::IGlibWrapper> &glibWrapper);
    ~WebAudioPlayerTaskFactory() override = default;

    std::unique_ptr<IPlayerTask> createShutdown(IGstWebAudioPlayerPrivate &player) const override;
    std::unique_ptr<IPlayerTask> createStop(IGstWebAudioPlayerPrivate &player) const override;
    std::unique_ptr<IPlayerTask> createPlay(IGstWebAudioPlayerPrivate &player) const override;
    std::unique_ptr<IPlayerTask> createPause(IGstWebAudioPlayerPrivate &player) const override;
    std::unique_ptr<IPlayerTask> createSetCaps(WebAudioPlayerContext &context, const std::string &audioMimeType,
                                               std::weak_ptr<const WebAudioConfig> config) const override;
    std::unique_ptr<IPlayerTask> createEos(WebAudioPlayerContext &context) const override;
    std::unique_ptr<IPlayerTask> createSetVolume(WebAudioPlayerContext &context, double volume) const override;
    std::unique_ptr<IPlayerTask> createWriteBuffer(WebAudioPlayerContext &context, uint8_t *mainPtr, uint32_t mainLength,
                                                   uint8_t *wrapPtr, uint32_t wrapLength) const override;
    std::unique_ptr<IPlayerTask> createHandleBusMessage(WebAudioPlayerContext &context, IGstWebAudioPlayerPrivate &player,
                                                        GstMessage *message) const override;
    std::unique_ptr<IPlayerTask> createPing(std::unique_ptr<IHeartbeatHandler> &&heartbeatHandler) const override;

private:
    IGstWebAudioPlayerClient *m_client;
    std::shared_ptr<firebolt::rialto::wrappers::IGstWrapper> m_gstWrapper;
    std::shared_ptr<firebolt::rialto::wrappers::IGlibWrapper> m_glibWrapper;
};
} // namespace firebolt::rialto::server

#endif // FIREBOLT_RIALTO_SERVER_WEB_AUDIO_PLAYER_TASK_FACTORY_H_
