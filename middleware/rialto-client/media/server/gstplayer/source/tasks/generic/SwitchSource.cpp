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

#include "SwitchSource.h"
#include "RialtoServerLogging.h"
#include "TypeConverters.h"
#include "Utils.h"

namespace firebolt::rialto::server::tasks::generic
{
SwitchSource::SwitchSource(IGstGenericPlayerPrivate &player, const std::unique_ptr<IMediaPipeline::MediaSource> &source)
    : m_player{player}, m_source{source->copy()}
{
    RIALTO_SERVER_LOG_DEBUG("Constructing SwitchSource");
}

SwitchSource::~SwitchSource()
{
    RIALTO_SERVER_LOG_DEBUG("SwitchSource finished");
}

void SwitchSource::execute() const
{
    RIALTO_SERVER_LOG_DEBUG("Executing SwitchSource");
    if (!m_player.reattachSource(m_source))
    {
        RIALTO_SERVER_LOG_WARN("Switch audio source failed");
        return;
    }
    RIALTO_SERVER_LOG_MIL("%s source switched", common::convertMediaSourceType(m_source->getType()));
}
} // namespace firebolt::rialto::server::tasks::generic
