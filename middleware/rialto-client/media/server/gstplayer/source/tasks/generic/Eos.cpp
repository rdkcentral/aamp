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

#include "tasks/generic/Eos.h"
#include "GenericPlayerContext.h"
#include "IGstWrapper.h"
#include "RialtoServerLogging.h"
#include "TypeConverters.h"

namespace firebolt::rialto::server::tasks::generic
{
Eos::Eos(GenericPlayerContext &context, IGstGenericPlayerPrivate &player,
         std::shared_ptr<firebolt::rialto::wrappers::IGstWrapper> gstWrapper,
         const firebolt::rialto::MediaSourceType &type)
    : m_context{context}, m_player{player}, m_gstWrapper{gstWrapper}, m_type{type}
{
    RIALTO_SERVER_LOG_DEBUG("Constructing Eos");
}

Eos::~Eos()
{
    RIALTO_SERVER_LOG_DEBUG("Eos finished");
}

void Eos::execute() const
{
    RIALTO_SERVER_LOG_DEBUG("Executing Eos for %s", common::convertMediaSourceType(m_type));

    m_player.cancelUnderflow(m_type);

    auto elem = m_context.streamInfo.find(m_type);
    if (elem == m_context.streamInfo.end())
    {
        RIALTO_SERVER_LOG_WARN("Set eos failed - Stream not found");
        return;
    }

    const auto eosInfoIt = m_context.endOfStreamInfo.find(m_type);
    if (eosInfoIt != m_context.endOfStreamInfo.end() && eosInfoIt->second == EosState::SET)
    {
        RIALTO_SERVER_LOG_DEBUG("Eos already set for source %s", common::convertMediaSourceType(m_type));
        return;
    }

    StreamInfo &streamInfo = elem->second;
    if (!streamInfo.buffers.empty())
    {
        RIALTO_SERVER_LOG_INFO("There are pending buffers; delaying sending EOS");
        m_context.endOfStreamInfo[m_type] = EosState::PENDING;
    }
    else if (m_gstWrapper->gstAppSrcEndOfStream(GST_APP_SRC(elem->second.appSrc)) != GST_FLOW_OK)
    {
        RIALTO_SERVER_LOG_WARN("Set eos failed - Gstreamer error");
    }
    else
    {
        RIALTO_SERVER_LOG_MIL("Successfully set EOS for source %s", common::convertMediaSourceType(m_type));
        m_context.endOfStreamInfo[m_type] = EosState::SET;
    }
}
} // namespace firebolt::rialto::server::tasks::generic
