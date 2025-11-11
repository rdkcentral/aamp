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

#include "SetSyncOff.h"
#include "RialtoServerLogging.h"

namespace firebolt::rialto::server::tasks::generic
{
SetSyncOff::SetSyncOff(GenericPlayerContext &context, IGstGenericPlayerPrivate &player, bool syncOff)
    : m_context(context), m_player(player), m_syncOff{syncOff}
{
    RIALTO_SERVER_LOG_DEBUG("Constructing SetSyncOff");
}

SetSyncOff::~SetSyncOff()
{
    RIALTO_SERVER_LOG_DEBUG("SetSyncOff finished");
}

void SetSyncOff::execute() const
{
    RIALTO_SERVER_LOG_DEBUG("Executing SetSyncOff");

    m_context.pendingSyncOff = m_syncOff;
    if (m_context.pipeline)
    {
        m_player.setSyncOff();
    }
}
} // namespace firebolt::rialto::server::tasks::generic
