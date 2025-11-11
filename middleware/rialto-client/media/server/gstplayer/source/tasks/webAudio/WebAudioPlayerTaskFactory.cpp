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
#include "tasks/webAudio/WebAudioPlayerTaskFactory.h"
#include "tasks/webAudio/Eos.h"
#include "tasks/webAudio/HandleBusMessage.h"
#include "tasks/webAudio/Pause.h"
#include "tasks/webAudio/Ping.h"
#include "tasks/webAudio/Play.h"
#include "tasks/webAudio/SetCaps.h"
#include "tasks/webAudio/SetVolume.h"
#include "tasks/webAudio/Shutdown.h"
#include "tasks/webAudio/Stop.h"
#include "tasks/webAudio/WriteBuffer.h"

namespace firebolt::rialto::server
{
WebAudioPlayerTaskFactory::WebAudioPlayerTaskFactory(
    IGstWebAudioPlayerClient *client, const std::shared_ptr<firebolt::rialto::wrappers::IGstWrapper> &gstWrapper,
    const std::shared_ptr<firebolt::rialto::wrappers::IGlibWrapper> &glibWrapper)
    : m_client{client}, m_gstWrapper{gstWrapper}, m_glibWrapper{glibWrapper}
{
}

std::unique_ptr<IPlayerTask> WebAudioPlayerTaskFactory::createShutdown(IGstWebAudioPlayerPrivate &player) const
{
    return std::make_unique<tasks::webaudio::Shutdown>(player);
}

std::unique_ptr<IPlayerTask> WebAudioPlayerTaskFactory::createStop(IGstWebAudioPlayerPrivate &player) const
{
    return std::make_unique<tasks::webaudio::Stop>(player);
}

std::unique_ptr<IPlayerTask> WebAudioPlayerTaskFactory::createPlay(IGstWebAudioPlayerPrivate &player) const
{
    return std::make_unique<tasks::webaudio::Play>(player, m_client);
}

std::unique_ptr<IPlayerTask> WebAudioPlayerTaskFactory::createPause(IGstWebAudioPlayerPrivate &player) const
{
    return std::make_unique<tasks::webaudio::Pause>(player, m_client);
}

std::unique_ptr<IPlayerTask> WebAudioPlayerTaskFactory::createEos(WebAudioPlayerContext &context) const
{
    return std::make_unique<tasks::webaudio::Eos>(context, m_gstWrapper);
}

std::unique_ptr<IPlayerTask> WebAudioPlayerTaskFactory::createSetCaps(WebAudioPlayerContext &context,
                                                                      const std::string &audioMimeType,
                                                                      std::weak_ptr<const WebAudioConfig> config) const
{
    return std::make_unique<tasks::webaudio::SetCaps>(context, m_gstWrapper, m_glibWrapper, audioMimeType, config);
}

std::unique_ptr<IPlayerTask> WebAudioPlayerTaskFactory::createSetVolume(WebAudioPlayerContext &context, double volume) const
{
    return std::make_unique<tasks::webaudio::SetVolume>(context, m_gstWrapper, volume);
}

std::unique_ptr<IPlayerTask> WebAudioPlayerTaskFactory::createWriteBuffer(WebAudioPlayerContext &context,
                                                                          uint8_t *mainPtr, uint32_t mainLength,
                                                                          uint8_t *wrapPtr, uint32_t wrapLength) const
{
    return std::make_unique<tasks::webaudio::WriteBuffer>(context, m_gstWrapper, mainPtr, mainLength, wrapPtr,
                                                          wrapLength);
}

std::unique_ptr<IPlayerTask> WebAudioPlayerTaskFactory::createHandleBusMessage(WebAudioPlayerContext &context,
                                                                               IGstWebAudioPlayerPrivate &player,
                                                                               GstMessage *message) const
{
    return std::make_unique<tasks::webaudio::HandleBusMessage>(context, player, m_client, m_gstWrapper, m_glibWrapper,
                                                               message);
}

std::unique_ptr<IPlayerTask> WebAudioPlayerTaskFactory::createPing(std::unique_ptr<IHeartbeatHandler> &&heartbeatHandler) const
{
    return std::make_unique<tasks::webaudio::Ping>(std::move(heartbeatHandler));
}
} // namespace firebolt::rialto::server
