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

#include "SetImmediateOutput.h"
#include "RialtoServerLogging.h"
#include "TypeConverters.h"

namespace firebolt::rialto::server::tasks::generic
{
SetImmediateOutput::SetImmediateOutput(GenericPlayerContext &context, IGstGenericPlayerPrivate &player,
                                       const MediaSourceType &type, bool immediateOutput)
    : m_context{context}, m_player(player), m_type{type}, m_immediateOutput{immediateOutput}
{
    RIALTO_SERVER_LOG_DEBUG("Constructing SetImmediateOutput");
}

SetImmediateOutput::~SetImmediateOutput()
{
    RIALTO_SERVER_LOG_DEBUG("SetImmediateOutput finished");
}

void SetImmediateOutput::execute() const
{
    RIALTO_SERVER_LOG_DEBUG("Executing SetImmediateOutput for %s source", common::convertMediaSourceType(m_type));

    if (m_type == MediaSourceType::VIDEO)
    {
        m_context.pendingImmediateOutputForVideo = m_immediateOutput;
    }
    else
    {
        RIALTO_SERVER_LOG_ERROR("SetImmediateOutput not currently supported for non-video");
    }

    if (m_context.pipeline)
    {
        m_player.setImmediateOutput();
    }
}
} // namespace firebolt::rialto::server::tasks::generic
