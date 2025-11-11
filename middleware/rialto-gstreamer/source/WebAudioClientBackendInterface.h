/*
 
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
#include <IWebAudioPlayerClient.h>
#include <string>

namespace firebolt::rialto::client
{
class WebAudioClientBackendInterface
{
public:
    virtual ~WebAudioClientBackendInterface() = default;
    virtual bool createWebAudioBackend(std::weak_ptr<IWebAudioPlayerClient> client, const std::string &audioMimeType,
                                       const uint32_t priority, std::weak_ptr<const WebAudioConfig> config) = 0;
    virtual void destroyWebAudioBackend() = 0;

    virtual bool play() = 0;
    virtual bool pause() = 0;
    virtual bool setEos() = 0;
    virtual bool getBufferAvailable(uint32_t &availableFrames) = 0;
    virtual bool getBufferDelay(uint32_t &delayFrames) = 0;
    virtual bool writeBuffer(const uint32_t numberOfFrames, void *data) = 0;
    virtual bool getDeviceInfo(uint32_t &preferredFrames, uint32_t &maximumFrames, bool &supportDeferredPlay) = 0;
    virtual bool setVolume(double volume) = 0;
    virtual bool getVolume(double &volume) = 0;
};
} // namespace firebolt::rialto::client
