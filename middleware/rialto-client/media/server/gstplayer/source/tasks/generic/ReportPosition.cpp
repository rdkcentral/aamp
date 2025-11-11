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

#include "tasks/generic/ReportPosition.h"
#include "GenericPlayerContext.h"
#include "IGstGenericPlayerClient.h"
#include "IGstWrapper.h"
#include "RialtoServerLogging.h"
#include <gst/gst.h>

namespace firebolt::rialto::server::tasks::generic
{
ReportPosition::ReportPosition(GenericPlayerContext &context, IGstGenericPlayerClient *client,
                               const std::shared_ptr<firebolt::rialto::wrappers::IGstWrapper> &gstWrapper,
                               IGstGenericPlayerPrivate &player)
    : m_context{context}, m_gstPlayerClient{client}, m_gstWrapper{gstWrapper}, m_player{player}
{
}

void ReportPosition::execute() const
{
    gint64 position = m_player.getPosition(m_context.pipeline);
    if (position == -1)
    {
        RIALTO_SERVER_LOG_WARN("Getting the position failed");
        return;
    }

    if (m_gstPlayerClient)
    {
        m_gstPlayerClient->notifyPosition(position);
    }
}
} // namespace firebolt::rialto::server::tasks::generic
