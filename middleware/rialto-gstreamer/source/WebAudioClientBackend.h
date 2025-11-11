/*
 * Copyright (C) 2023   
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation;
 * version 2.1 of the License.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */
#pragma once

#include "WebAudioClientBackendInterface.h"
#include <IWebAudioPlayer.h>
#include <IWebAudioPlayerClient.h>
#include <gst/gst.h>
#include <memory>

namespace firebolt::rialto::client
{
class WebAudioClientBackend final : public WebAudioClientBackendInterface
{
public:
    WebAudioClientBackend() : m_webAudioPlayerBackend(nullptr) {}
    ~WebAudioClientBackend() final { m_webAudioPlayerBackend.reset(); }

    bool createWebAudioBackend(std::weak_ptr<IWebAudioPlayerClient> client, const std::string &audioMimeType,
                               const uint32_t priority, std::weak_ptr<const WebAudioConfig> config) override
    {
        m_webAudioPlayerBackend =
            firebolt::rialto::IWebAudioPlayerFactory::createFactory()->createWebAudioPlayer(client, audioMimeType,
                                                                                            priority, config);

        if (!m_webAudioPlayerBackend)
        {
            GST_ERROR("Could not create web audio backend");
            return false;
        }
        return true;
    }
    void destroyWebAudioBackend() override { m_webAudioPlayerBackend.reset(); }

    bool play() override { return m_webAudioPlayerBackend->play(); }
    bool pause() override { return m_webAudioPlayerBackend->pause(); }
    bool setEos() override { return m_webAudioPlayerBackend->setEos(); }
    bool getBufferAvailable(uint32_t &availableFrames) override
    {
        std::shared_ptr<firebolt::rialto::WebAudioShmInfo> webAudioShmInfo;
        return m_webAudioPlayerBackend->getBufferAvailable(availableFrames, webAudioShmInfo);
    }
    bool getBufferDelay(uint32_t &delayFrames) override { return m_webAudioPlayerBackend->getBufferDelay(delayFrames); }
    bool writeBuffer(const uint32_t numberOfFrames, void *data) override
    {
        return m_webAudioPlayerBackend->writeBuffer(numberOfFrames, data);
    }
    bool getDeviceInfo(uint32_t &preferredFrames, uint32_t &maximumFrames, bool &supportDeferredPlay) override
    {
        return m_webAudioPlayerBackend->getDeviceInfo(preferredFrames, maximumFrames, supportDeferredPlay);
    }
    bool setVolume(double volume) override { return m_webAudioPlayerBackend->setVolume(volume); }
    bool getVolume(double &volume) override { return m_webAudioPlayerBackend->getVolume(volume); }

private:
    std::unique_ptr<IWebAudioPlayer> m_webAudioPlayerBackend;
};
} // namespace firebolt::rialto::client
