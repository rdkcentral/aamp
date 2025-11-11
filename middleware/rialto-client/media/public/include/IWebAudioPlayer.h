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

#ifndef FIREBOLT_RIALTO_I_WEB_AUDIO_PLAYER_H_
#define FIREBOLT_RIALTO_I_WEB_AUDIO_PLAYER_H_

/**
 * @file IWebAudioPlayer.h
 *
 * The definition of the IWebAudioPlayer interface.
 *
 * This interface defines the public API of Rialto for mixing PCM audio with
 * current audio output.
 */

#include <memory>
#include <stdint.h>
#include <string>

#include "IWebAudioPlayerClient.h"
#include "MediaCommon.h"

namespace firebolt::rialto
{
class IWebAudioPlayer;

/**
 * @brief IWebAudioPlayer factory class, returns a concrete implementation of IWebAudioPlayer
 */
class IWebAudioPlayerFactory
{
public:
    IWebAudioPlayerFactory() = default;
    virtual ~IWebAudioPlayerFactory() = default;

    /**
     * @brief Create a IWebAudioPlayerFactory instance.
     *
     * @retval the factory instance or null on error.
     */
    static std::shared_ptr<IWebAudioPlayerFactory> createFactory();

    /**
     * @brief IWebAudioPlayer factory method, returns a concrete implementation of
     * IWebAudioPlayer for playback of audio.
     *
     * @param[in] client:        The Web Audio Player client
     * @param[in] audioMimeType: The audio encoding format, currently only "audio/x-raw" (PCM)
     * @param[in] priority:      Priority value for this pipeline.
     * @param[in] config:        Additional type dependent configuration data or nullptr
     *
     * Some platforms have limited numbers of audio playbacks that can be mixed. The client application
     * should therefore assign a priority to each audio player it creates, starting with priority 1 for
     * the most important, priority 2 for the next etc. If a platform supports 'n' players, then any
     * player with priority>n will appear to function normally by but the audio data will be silently
     * discarded.
     *
     * @retval the new Web Audio Player instance or null on error.
     */
    virtual std::unique_ptr<IWebAudioPlayer> createWebAudioPlayer(std::weak_ptr<IWebAudioPlayerClient> client,
                                                                  const std::string &audioMimeType,
                                                                  const uint32_t priority,
                                                                  std::weak_ptr<const WebAudioConfig> config) const = 0;
};

/**
 * @brief The definition of the IWebAudioPlayer interface.
 *
 * This interface defines the public API of Rialto for mixing PCM audio with
 * current audio output. It should be implemented by both Rialto Client &
 * Rialto Server.
 */
class IWebAudioPlayer
{
public:
    IWebAudioPlayer() = default;
    virtual ~IWebAudioPlayer() = default;

    IWebAudioPlayer(const IWebAudioPlayer &) = delete;
    IWebAudioPlayer &operator=(const IWebAudioPlayer &) = delete;
    IWebAudioPlayer(IWebAudioPlayer &&) = delete;
    IWebAudioPlayer &operator=(IWebAudioPlayer &&) = delete;

    /**
     * @brief Play the web audio.
     *
     * Sets the player to the PLAYING state (it is IDLE when created).
     *
     * @retval true on success.
     */
    virtual bool play() = 0;

    /**
     * @brief Pause the web audio.
     *
     * Sets the player to the PAUSED state (it is IDLE when created).
     *
     * @retval true on success.
     */
    virtual bool pause() = 0;

    /**
     * @brief Notify EOS.
     *
     * Notifies the player that no further frames will be provided. When
     * all buffered frames are played the player will enter the EOS state.
     *
     * @retval true on success.
     */
    virtual bool setEos() = 0;

    /**
     * @brief Get the available frames.
     *
     * Gets the available buffer space for sending more frames. Client should not
     * write more than the number of frames returned by this API.
     *
     * webAudioShmInfo is not required by the client application and can be ignored.
     *
     * @param[out] availableFrames : Number of frames available to be written.
     * @param[out] webAudioShmInfo : Location in shm to write the data (Server only).
     *
     * @retval true on success.
     */
    virtual bool getBufferAvailable(uint32_t &availableFrames, std::shared_ptr<WebAudioShmInfo> &webAudioShmInfo) = 0;

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
     * @param[in]  numberOfFrames : Number of frames of audio in 'data'.
     * @param[in]  data           : Pointer to the data, byte length = numberOfFrames*sampleSize
     *
     * @retval true on success.
     */
    virtual bool writeBuffer(const uint32_t numberOfFrames, void *data) = 0;

    /**
     * @brief Get device information.
     *
     * Gets information for the web audio playback.
     * This information is used to determine the preferred buffer size to commit,
     * the maximum buffer size an application can commit and whether buffers can
     * be committed before a Play request.
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
     * Sets the current volume for the pipeline (0.0 silent -> 1.0 full volume)
     *
     * @param[in] volume    : Target volume level (0.0 - 1.0)
     *
     * @retval true on success false otherwise
     */
    virtual bool setVolume(double volume) = 0;

    /**
     * @brief Get current audio level
     *
     * Fetches the current volume level for the pipeline.
     *
     * @param[out] volume   : Current volume level (range 0.0 - 1.0)
     *
     * @retval true on success false otherwise
     */
    virtual bool getVolume(double &volume) = 0;

    /**
     * @brief Returns the web audio player client.
     *
     * @retval The web audio player client.
     */
    virtual std::weak_ptr<IWebAudioPlayerClient> getClient() = 0;
};

}; // namespace firebolt::rialto

#endif // FIREBOLT_RIALTO_I_WEB_AUDIO_PLAYER_H_
