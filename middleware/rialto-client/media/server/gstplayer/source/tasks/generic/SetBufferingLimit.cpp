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

#include "SetBufferingLimit.h"
#include "RialtoServerLogging.h"
#include "TypeConverters.h"

namespace firebolt::rialto::server::tasks::generic
{
SetBufferingLimit::SetBufferingLimit(GenericPlayerContext &context, IGstGenericPlayerPrivate &player, std::uint32_t limit)
    : m_context{context}, m_player(player), m_limit{limit}
{
    RIALTO_SERVER_LOG_DEBUG("Constructing SetBufferingLimit");
}

SetBufferingLimit::~SetBufferingLimit()
{
    RIALTO_SERVER_LOG_DEBUG("SetBufferingLimit finished");
}

void SetBufferingLimit::execute() const
{
    RIALTO_SERVER_LOG_DEBUG("Executing SetBufferingLimit");

    m_context.pendingBufferingLimit = m_limit;
    if (m_context.pipeline)
    {
        m_player.setBufferingLimit();
    }
}
} // namespace firebolt::rialto::server::tasks::generic
