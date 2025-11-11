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

#ifndef FIREBOLT_RIALTO_CLIENT_I_WEB_AUDIO_PLAYER_IPC_H_
#define FIREBOLT_RIALTO_CLIENT_I_WEB_AUDIO_PLAYER_IPC_H_

#include <stdint.h>

#include <memory>
#include <string>

#include <IMediaPipeline.h>
#include <MediaCommon.h>

#include "IWebAudioPlayerIpcClient.h"

namespace firebolt::rialto::client
{
class IWebAudioPlayerIpc;
class IIpcClient;

/**
 * @brief IWebAudioPlayerIpc factory class, returns a concrete implementation of IWebAudioPlayerIpc
 */
class IWebAudioPlayerIpcFactory
{
public:
    IWebAudioPlayerIpcFactory() = default;
    virtual ~IWebAudioPlayerIpcFactory() = default;

    /**
     * @brief Gets the IWebAudioPlayerIpcFactory instance.
     *
     * @retval the factory instance or null on error.
     */
    static std::shared_ptr<IWebAudioPlayerIpcFactory> getFactory();

    /**
     * @brief Creates a IWebAudioPlayerIpc object.
     *
     * @param[in] client            : The Rialto ipc web audio player client.
     *
     * @retval the new web audio player ipc instance or null on error.
     */
    virtual std::unique_ptr<IWebAudioPlayerIpc>
    createWebAudioPlayerIpc(IWebAudioPlayerIpcClient *client, const std::string &audioMimeType, const uint32_t priority,
                            std::weak_ptr<const WebAudioConfig> config, std::weak_ptr<IIpcClient> ipcClient = {}) = 0;
};

/**
 * @brief The definition of the IWebAudioPlayerIpc interface.
 *
 * This interface defines the web audio player ipc APIs that are used to communicate with the Rialto server.
 */
class IWebAudioPlayerIpc
{
public:
    IWebAudioPlayerIpc() = default;
    virtual ~IWebAudioPlayerIpc() = default;

    IWebAudioPlayerIpc(const IWebAudioPlayerIpc &) = delete;
    IWebAudioPlayerIpc &operator=(const IWebAudioPlayerIpc &) = delete;
    IWebAudioPlayerIpc(IWebAudioPlayerIpc &&) = delete;
    IWebAudioPlayerIpc &operator=(IWebAudioPlayerIpc &&) = delete;

    /**
     * @brief Play the web audio.
     *
     * @retval true on success.
     */
    virtual bool play() = 0;

    /**
     * @brief Pause the web audio.
     *
     * @retval true on success.
     */
    virtual bool pause() = 0;

    /**
     * @brief Notify EOS.
     *
     * @retval true on success.
     */
    virtual bool setEos() = 0;

    /**
     * @brief Get the available frames.
     *
     * @param[out] availableFrames : Number of frames available to be written.
     * @param[out] webAudioShmInfo : Location in shm to write the data.
     *
     * @retval true on success.
     */
    virtual bool getBufferAvailable(uint32_t &availableFrames,
                                    const std::shared_ptr<WebAudioShmInfo> &webAudioShmInfo) = 0;

    /**
     * @brief Get the delay frames.
     *
     * Gets the frame delay of the playback from Rialto. Frame delay is the number
     * of frames left to play by the server.
     *
     * @param[out] delayFrames : Number of frames to be played.
     *
     * @retval true on success.
     */
    virtual bool getBufferDelay(uint32_t &delayFrames) = 0;

    /**
     * @brief Write audio frames
     *
     * Sends a buffer of audio data for playback
     *
     * @param[in]  numberOfFrames : Number of frames written to shared memory.
     *
     * @retval true on success.
     */
    virtual bool writeBuffer(const uint32_t numberOfFrames) = 0;

    /**
     * @brief Get device information.
     *
     * @param[out] preferredFrames     : preferred number of frames to be commited.
     * @param[out] maximumFrames       : Maximum number of frames that can be commited.
     * @param[out] supportDeferredPlay : Whether defered play is supported.
     *
     * @retval true on success.
     */
    virtual bool getDeviceInfo(uint32_t &preferredFrames, uint32_t &maximumFrames, bool &supportDeferredPlay) = 0;

    /**
     *
     * @brief Set level and transition of audio attenuation
     *
     * @param[in] volume    : Target volume level (0.0 - 1.0)
     *
     * @retval true on success false otherwise
     */
    virtual bool setVolume(double volume) = 0;

    /**
     * @brief Get current audio level
     *
     * @param[out] volume   : Current volume level (range 0.0 - 1.0)
     *
     * @retval true on success false otherwise
     */
    virtual bool getVolume(double &volume) = 0;
};

}; // namespace firebolt::rialto::client

#endif // FIREBOLT_RIALTO_CLIENT_I_WEB_AUDIO_PLAYER_IPC_H_
