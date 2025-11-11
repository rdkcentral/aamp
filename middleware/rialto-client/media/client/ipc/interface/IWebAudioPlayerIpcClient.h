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

#ifndef FIREBOLT_RIALTO_CLIENT_I_WEB_AUDIO_PLAYER_IPC_CLIENT_H_
#define FIREBOLT_RIALTO_CLIENT_I_WEB_AUDIO_PLAYER_IPC_CLIENT_H_

#include <MediaCommon.h>

/**
 * @file IWebAudioPlayerIpcClient.h
 *
 * The definition of the IWebAudioPlayerIpcClient interface.
 */

namespace firebolt::rialto::client
{
/**
 * @brief The Rialto web audio player client ipc interface.
 */
class IWebAudioPlayerIpcClient
{
public:
    IWebAudioPlayerIpcClient() = default;
    virtual ~IWebAudioPlayerIpcClient() = default;

    IWebAudioPlayerIpcClient(const IWebAudioPlayerIpcClient &) = delete;
    IWebAudioPlayerIpcClient &operator=(const IWebAudioPlayerIpcClient &) = delete;
    IWebAudioPlayerIpcClient(IWebAudioPlayerIpcClient &&) = delete;
    IWebAudioPlayerIpcClient &operator=(IWebAudioPlayerIpcClient &&) = delete;

    /**
     * @brief Notifies the client of the playback state.
     * @param[in] state : The new playback state.
     *
     * @retval true on success.
     */
    virtual void notifyState(WebAudioPlayerState state) = 0;
};

}; // namespace firebolt::rialto::client

#endif // FIREBOLT_RIALTO_CLIENT_I_WEB_AUDIO_PLAYER_IPC_CLIENT_H_
