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

#include "SetUseBuffering.h"
#include "RialtoServerLogging.h"
#include "TypeConverters.h"

namespace firebolt::rialto::server::tasks::generic
{
SetUseBuffering::SetUseBuffering(GenericPlayerContext &context, IGstGenericPlayerPrivate &player, bool useBuffering)
    : m_context{context}, m_player(player), m_useBuffering{useBuffering}
{
    RIALTO_SERVER_LOG_DEBUG("Constructing SetUseBuffering");
}

SetUseBuffering::~SetUseBuffering()
{
    RIALTO_SERVER_LOG_DEBUG("SetUseBuffering finished");
}

void SetUseBuffering::execute() const
{
    RIALTO_SERVER_LOG_DEBUG("Executing SetUseBuffering");

    m_context.pendingUseBuffering = m_useBuffering;
    if (m_context.pipeline)
    {
        m_player.setUseBuffering();
    }
}
} // namespace firebolt::rialto::server::tasks::generic
