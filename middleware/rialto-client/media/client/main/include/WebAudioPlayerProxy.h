/*
 * If not stated otherwise in this file or this component's LICENSE file the
 * following copyright and licenses apply:
 *
 * Copyright 2024 
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

#ifndef FIREBOLT_RIALTO_WEB_AUDIO_PLAYER_PROXY_H_
#define FIREBOLT_RIALTO_WEB_AUDIO_PLAYER_PROXY_H_

#include <memory>
#include <stdint.h>

#include "WebAudioPlayer.h"

namespace firebolt::rialto
{
class WebAudioPlayerProxy : public client::IWebAudioPlayerAndIControlClient
{
public:
    WebAudioPlayerProxy(const std::shared_ptr<IWebAudioPlayerAndIControlClient> &webAudioPlayer,
                        client::IClientController &clientController);
    virtual ~WebAudioPlayerProxy();

    bool play() override { return m_webAudioPlayer->play(); }

    bool pause() override { return m_webAudioPlayer->pause(); }

    bool setEos() override { return m_webAudioPlayer->setEos(); }

    bool getBufferAvailable(uint32_t &availableFrames, std::shared_ptr<WebAudioShmInfo> &webAudioShmInfo) override
    {
        return m_webAudioPlayer->getBufferAvailable(availableFrames, webAudioShmInfo);
    }

    bool getBufferDelay(uint32_t &delayFrames) override { return m_webAudioPlayer->getBufferDelay(delayFrames); }

    bool writeBuffer(const uint32_t numberOfFrames, void *data) override
    {
        return m_webAudioPlayer->writeBuffer(numberOfFrames, data);
    }

    bool getDeviceInfo(uint32_t &preferredFrames, uint32_t &maximumFrames, bool &supportDeferredPlay) override
    {
        return m_webAudioPlayer->getDeviceInfo(preferredFrames, maximumFrames, supportDeferredPlay);
    }

    bool setVolume(double volume) override { return m_webAudioPlayer->setVolume(volume); }

    bool getVolume(double &volume) override { return m_webAudioPlayer->getVolume(volume); }

    std::weak_ptr<IWebAudioPlayerClient> getClient() override { return m_webAudioPlayer->getClient(); }

    void notifyApplicationState(ApplicationState state) override { m_webAudioPlayer->notifyApplicationState(state); }

private:
    std::shared_ptr<IWebAudioPlayerAndIControlClient> m_webAudioPlayer;
    client::IClientController &m_clientController;
};

}; // namespace firebolt::rialto

#endif // FIREBOLT_RIALTO_WEB_AUDIO_PLAYER_PROXY_H_
