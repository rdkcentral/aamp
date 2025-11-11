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

#ifndef FIREBOLT_RIALTO_SERVER_I_GST_WEB_AUDIO_PLAYER_H_
#define FIREBOLT_RIALTO_SERVER_I_GST_WEB_AUDIO_PLAYER_H_

#include "IGstWebAudioPlayerClient.h"
#include "IHeartbeatHandler.h"
#include <memory>
#include <stdint.h>
#include <string>

namespace firebolt::rialto::server
{
class IGstWebAudioPlayer;

/**
 * @brief IGstWebAudioPlayer factory class, returns a concrete implementation of IGstWebAudioPlayer
 */
class IGstWebAudioPlayerFactory
{
public:
    IGstWebAudioPlayerFactory() = default;
    virtual ~IGstWebAudioPlayerFactory() = default;

    /**
     * @brief Gets the IGstWebAudioPlayerFactory instance.
     *
     * @retval the factory instance or null on error.
     */
    static std::shared_ptr<IGstWebAudioPlayerFactory> getFactory();

    /**
     * @brief Creates a IGstWebAudioPlayer object.
     *
     * @param[in] client    : The gstreamer web audio player client.
     * @param[in] priority  : Priority value for this pipeline.
     *
     * @retval the new player instance or null on error.
     */
    virtual std::unique_ptr<IGstWebAudioPlayer> createGstWebAudioPlayer(IGstWebAudioPlayerClient *client,
                                                                        const uint32_t priority) = 0;
};

class IGstWebAudioPlayer
{
public:
    IGstWebAudioPlayer() = default;
    virtual ~IGstWebAudioPlayer() = default;

    IGstWebAudioPlayer(const IGstWebAudioPlayer &) = delete;
    IGstWebAudioPlayer &operator=(const IGstWebAudioPlayer &) = delete;
    IGstWebAudioPlayer(IGstWebAudioPlayer &&) = delete;
    IGstWebAudioPlayer &operator=(IGstWebAudioPlayer &&) = delete;

    /**
     * @brief Sets the capabilities on the audio source.
     *
     * @param[in] audioMimeType: The audio encoding format, currently only "audio/x-raw" (PCM).
     * @param[in] config:        Additional type dependent configuration data or nullptr,
     */
    virtual void setCaps(const std::string &audioMimeType, std::weak_ptr<const WebAudioConfig> config) = 0;

    /**
     * @brief Starts playback of the web audio.
     *
     * This method is considered to be asynchronous and MUST NOT block
     * but should request playback and then return.
     *
     * Once the backend is successfully playing it should notify the
     * web audio player client of state WebAudioPlayerState::PLAYING.
     */
    virtual void play() = 0;

    /**
     * @brief Pauses playback of the web audio.
     *
     * This method is considered to be asynchronous and MUST NOT block
     * but should request the playback pause and then return.
     *
     * Once the backend is successfully paused it should notify the
     * web audio player client of state WebAudioPlayerState::PAUSED.
     */
    virtual void pause() = 0;

    /**
     * @brief Set level and transition of audio attenuation.
     *        Sets the current volume for the pipeline (0.0 silent -> 1.0 full volume).
     *
     * @param[in] volume : Target volume level (0.0 - 1.0)
     */
    virtual void setVolume(double volume) = 0;

    /**
     * @brief Get current audio level. Fetches the current volume level for the pipeline.
     *
     * @param[out] volume : Current volume level (range 0.0 - 1.0)
     *
     * @retval True on success.
     */
    virtual bool getVolume(double &volume) = 0;

    /**
     * @brief Write the buffer to gstreamer buffer.
     *
     * @param[in] mainPtr       : Pointer to the start of the data.
     * @param[in] mainLength    : Amount of bytes to write from the mainPtr.
     * @param[in] wrapPtr       : Pointer to the start of the wrapped data.
     * @param[in] wrapLength    : Amount of bytes to write from the wrapPtr.
     *
     * @retval The number of bytes written to gstreamer.
     */
    virtual uint32_t writeBuffer(uint8_t *mainPtr, uint32_t mainLength, uint8_t *wrapPtr, uint32_t wrapLength) = 0;

    /**
     * @brief Notify EOS at the end of the gstreamer buffers.
     */
    virtual void setEos() = 0;

    /**
     * @brief Gets the amount of bytes queued for playback on the app source.
     *
     * @retval The number of bytes queued.
     */
    virtual uint64_t getQueuedBytes() = 0;

    /**
     * @brief Checks if worker thread is not deadlocked
     *
     * @param[out] heartbeatHandler : The heartbeat handler instance
     *
     */
    virtual void ping(std::unique_ptr<IHeartbeatHandler> &&heartbeatHandler) = 0;
};

}; // namespace firebolt::rialto::server

#endif // FIREBOLT_RIALTO_SERVER_I_GST_WEB_AUDIO_PLAYER_H_
