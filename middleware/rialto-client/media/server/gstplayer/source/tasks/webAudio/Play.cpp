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

#include "tasks/webAudio/Play.h"
#include "IGstWebAudioPlayerPrivate.h"
#include "RialtoServerLogging.h"

namespace firebolt::rialto::server::tasks::webaudio
{
Play::Play(IGstWebAudioPlayerPrivate &player, IGstWebAudioPlayerClient *client)
    : m_player{player}, m_gstPlayerClient{client}
{
    RIALTO_SERVER_LOG_DEBUG("Constructing Play");
}

Play::~Play()
{
    RIALTO_SERVER_LOG_DEBUG("Play finished");
}

void Play::execute() const
{
    RIALTO_SERVER_LOG_DEBUG("Executing Play");
    if (!m_player.changePipelineState(GST_STATE_PLAYING))
    {
        RIALTO_SERVER_LOG_ERROR("Failed to play the web audio player");
    }
    RIALTO_SERVER_LOG_MIL("State change to PLAYING requested for webaudio pipeline");
}
} // namespace firebolt::rialto::server::tasks::webaudio
