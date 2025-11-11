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

#include "SetStreamSyncMode.h"
#include "RialtoServerLogging.h"

namespace firebolt::rialto::server::tasks::generic
{
SetStreamSyncMode::SetStreamSyncMode(GenericPlayerContext &context, IGstGenericPlayerPrivate &player,
                                     const MediaSourceType &type, int32_t streamSyncMode)
    : m_context(context), m_player(player), m_type{type}, m_streamSyncMode{streamSyncMode}
{
    RIALTO_SERVER_LOG_DEBUG("Constructing SetStreamSyncMode");
}

SetStreamSyncMode::~SetStreamSyncMode()
{
    RIALTO_SERVER_LOG_DEBUG("SetStreamSyncMode finished");
}

void SetStreamSyncMode::execute() const
{
    RIALTO_SERVER_LOG_DEBUG("Executing SetStreamSyncMode");

    m_context.pendingStreamSyncMode.emplace(m_type, m_streamSyncMode);
    if (m_context.pipeline)
    {
        m_player.setStreamSyncMode(m_type);
    }
}
} // namespace firebolt::rialto::server::tasks::generic
