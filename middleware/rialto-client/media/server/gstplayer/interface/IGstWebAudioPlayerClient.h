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

#ifndef FIREBOLT_RIALTO_SERVER_I_GST_WEB_AUDIO_PLAYER_CLIENT_H_
#define FIREBOLT_RIALTO_SERVER_I_GST_WEB_AUDIO_PLAYER_CLIENT_H_

#include <MediaCommon.h>
#include <stdint.h>

/**
 * @file IGstWebAudioPlayerClient.h
 *
 * The definition of the IGstWebAudioPlayerClient interface.
 *
 * This file comprises the definition of the IGstWebAudioPlayerClient abstract
 * class. This is the API by which a IGstWebAudioPlayer implementation will
 * pass notifications to its client.
 */

namespace firebolt::rialto::server
{
/**
 * @brief The Rialto gstreamer player client interface.
 *
 * This is The Rialto gstreamer player client abstract base class. It should be
 * implemented by any object that wishes to be notified by changes in the
 * state of the gstreamer player or that provides data for playback.
 */
class IGstWebAudioPlayerClient
{
public:
    IGstWebAudioPlayerClient() = default;
    virtual ~IGstWebAudioPlayerClient() = default;

    IGstWebAudioPlayerClient(const IGstWebAudioPlayerClient &) = delete;
    IGstWebAudioPlayerClient &operator=(const IGstWebAudioPlayerClient &) = delete;
    IGstWebAudioPlayerClient(IGstWebAudioPlayerClient &&) = delete;
    IGstWebAudioPlayerClient &operator=(IGstWebAudioPlayerClient &&) = delete;

    /**
     * @brief Notifies the client of the web audio state.
     *
     * The player will start IDLE. Once play() has been called the player
     * will be PLAYING, or once pause() has been called the player will be
     * PAUSED. Once the stream has reached end of the media EOS wibe notified.
     *
     * @param[in] state : The new web audio state.
     */
    virtual void notifyState(WebAudioPlayerState state) = 0;
};

}; // namespace firebolt::rialto::server

#endif // FIREBOLT_RIALTO_SERVER_I_GST_WEB_AUDIO_PLAYER_CLIENT_H_
