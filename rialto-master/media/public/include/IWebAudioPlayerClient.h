/*
 * If not stated otherwise in this file or this component's LICENSE file the
 * following copyright and licenses apply:
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

#ifndef FIREBOLT_RIALTO_I_WEB_AUDIO_PLAYER_CLIENT_H_
#define FIREBOLT_RIALTO_I_WEB_AUDIO_PLAYER_CLIENT_H_

/**
 * @file IWebAudioPlayerClient.h
 *
 * The definition of the IWebAudioPlayerClient interface.
 *
 * This is the API by which a IWebAudioPlayer implementation will
 * pass notifications to its client.
 */

#include <MediaCommon.h>
#include <memory>
#include <stdint.h>

namespace firebolt::rialto
{
/**
 * @brief The Rialto web audio client interface.
 *
 * This is The Rialto web audio player client abstract base class. It should be
 * implemented by any object that wishes to be notified by changes in the
 * state of the web audio player.
 */
class IWebAudioPlayerClient
{
public:
    /**
     * @brief Virtual destructor
     */
    virtual ~IWebAudioPlayerClient() {}

    /**
     * @brief Notifies the client of the playback state.
     *
     * The player will start IDLE. Once play() has been called the player
     * will be PLAYING. When pause() is call3d the player will be PAUSED.
     * When no further frames available for playout and setEos() has been
     * called the player will be END_OF_STREAM.
     *
     * @param[in] state : The new playback state.
     */
    virtual void notifyState(WebAudioPlayerState state) = 0;
};

} // namespace firebolt::rialto

#endif // FIREBOLT_RIALTO_I_WEB_AUDIO_PLAYER_CLIENT_H_
