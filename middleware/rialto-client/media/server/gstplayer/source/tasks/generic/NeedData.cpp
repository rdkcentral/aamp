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

#include "tasks/generic/NeedData.h"
#include "GenericPlayerContext.h"
#include "IGstGenericPlayerClient.h"
#include "RialtoServerLogging.h"
#include "TypeConverters.h"
#include <gst/gst.h>

namespace firebolt::rialto::server::tasks::generic
{
NeedData::NeedData(GenericPlayerContext &context, IGstGenericPlayerPrivate &player, IGstGenericPlayerClient *client,
                   GstAppSrc *src)
    : m_context{context}, m_player{player}, m_gstPlayerClient{client}, m_src{src}
{
    RIALTO_SERVER_LOG_DEBUG("Constructing NeedData");
}

NeedData::~NeedData()
{
    RIALTO_SERVER_LOG_DEBUG("NeedData finished");
}

void NeedData::execute() const
{
    RIALTO_SERVER_LOG_DEBUG("Executing NeedData");

    for (auto &elem : m_context.streamInfo)
    {
        firebolt::rialto::MediaSourceType sourceType = elem.first;
        if (elem.second.appSrc == GST_ELEMENT(m_src))
        {
            RIALTO_SERVER_LOG_DEBUG("%s source needs data", common::convertMediaSourceType(sourceType));

            elem.second.isDataNeeded = true;

            if (!elem.second.buffers.empty())
            {
                RIALTO_SERVER_LOG_INFO("Attaching cached data for %s", common::convertMediaSourceType(sourceType));
                m_player.attachData(sourceType);
            }

            if (m_gstPlayerClient && !elem.second.isNeedDataPending)
            {
                if (sourceType == MediaSourceType::AUDIO && m_context.audioSourceRemoved)
                {
                    RIALTO_SERVER_LOG_DEBUG("Audio source is removed, no need to request data");
                    break;
                }
                elem.second.isNeedDataPending = m_gstPlayerClient->notifyNeedMediaData(sourceType);
            }
            break;
        }
    }
}
} // namespace firebolt::rialto::server::tasks::generic
