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

#include "tasks/generic/Underflow.h"
#include "RialtoServerLogging.h"
#include "TypeConverters.h"

namespace firebolt::rialto::server::tasks::generic
{
Underflow::Underflow(GenericPlayerContext &context, IGstGenericPlayerPrivate &player, IGstGenericPlayerClient *client,
                     bool underflowEnabled, MediaSourceType sourceType)
    : m_context{context}, m_player{player}, m_gstPlayerClient{client}, m_underflowEnabled{underflowEnabled},
      m_sourceType{sourceType}
{
    RIALTO_SERVER_LOG_DEBUG("Constructing Underflow");
}

Underflow::~Underflow()
{
    RIALTO_SERVER_LOG_DEBUG("Underflow finished");
}

void Underflow::execute() const
{
    RIALTO_SERVER_LOG_WARN("Executing Underflow for %s source", common::convertMediaSourceType(m_sourceType));
    if (!m_underflowEnabled)
    {
        return;
    }

    auto elem = m_context.streamInfo.find(m_sourceType);
    if (elem != m_context.streamInfo.end())
    {
        StreamInfo &streamInfo = elem->second;
        if (streamInfo.underflowOccured)
        {
            return;
        }

        streamInfo.underflowOccured = true;

        if (m_gstPlayerClient)
        {
            m_gstPlayerClient->notifyBufferUnderflow(m_sourceType);
        }
    }
}

} // namespace firebolt::rialto::server::tasks::generic
