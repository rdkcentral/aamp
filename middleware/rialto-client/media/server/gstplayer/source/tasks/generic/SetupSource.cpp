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

#include "tasks/generic/SetupSource.h"
#include "GenericPlayerContext.h"
#include "IGstGenericPlayerPrivate.h"
#include "RialtoServerLogging.h"

namespace firebolt::rialto::server::tasks::generic
{
SetupSource::SetupSource(GenericPlayerContext &context, IGstGenericPlayerPrivate &player, GstElement *source)
    : m_context{context}, m_player{player}, m_source{source}
{
    RIALTO_SERVER_LOG_DEBUG("Constructing SetupSource");
}

SetupSource::~SetupSource()
{
    RIALTO_SERVER_LOG_DEBUG("SetupSource finished");
}

void SetupSource::execute() const
{
    RIALTO_SERVER_LOG_DEBUG("Executing SetupSource");
    m_context.source = m_source;

    if (m_context.wereAllSourcesAttached)
    {
        RIALTO_SERVER_LOG_DEBUG("Source ready. Schedulling allSourcesAttached");
        m_player.scheduleAllSourcesAttached();
    }
}
} // namespace firebolt::rialto::server::tasks::generic
