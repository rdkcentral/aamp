/*
 * If not stated otherwise in this file or this component's LICENSE file the
 * following copyright and licenses apply:
 *
 * Copyright 2023 
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

#include "tasks/generic/RemoveSource.h"
#include "RialtoServerLogging.h"
#include "TypeConverters.h"

namespace firebolt::rialto::server::tasks::generic
{
RemoveSource::RemoveSource(GenericPlayerContext &context, IGstGenericPlayerPrivate &player,
                           IGstGenericPlayerClient *client,
                           std::shared_ptr<firebolt::rialto::wrappers::IGstWrapper> gstWrapper,
                           const MediaSourceType &type)
    : m_context{context}, m_player{player}, m_gstPlayerClient{client}, m_gstWrapper{gstWrapper}, m_type{type}
{
    RIALTO_SERVER_LOG_DEBUG("Constructing RemoveSource");
}

RemoveSource::~RemoveSource()
{
    RIALTO_SERVER_LOG_DEBUG("RemoveSource finished");
}

void RemoveSource::execute() const
{
    RIALTO_SERVER_LOG_DEBUG("Executing RemoveSource for %s source", common::convertMediaSourceType(m_type));
    if (MediaSourceType::AUDIO != m_type)
    {
        RIALTO_SERVER_LOG_DEBUG("RemoveSource not supported for type != AUDIO");
        return;
    }
    m_context.audioSourceRemoved = true;
    m_gstPlayerClient->invalidateActiveRequests(m_type);
    GstElement *source{nullptr};
    auto sourceElem = m_context.streamInfo.find(m_type);
    if (sourceElem != m_context.streamInfo.end())
    {
        source = sourceElem->second.appSrc;
    }
    if (!source)
    {
        RIALTO_SERVER_LOG_WARN("failed to flush - source is NULL");
        return;
    }
    sourceElem->second.buffers.clear();
    sourceElem->second.isDataNeeded = false;
    sourceElem->second.isNeedDataPending = false;
    m_player.clearNeedDataScheduled(GST_APP_SRC(sourceElem->second.appSrc));
    m_player.stopPositionReportingAndCheckAudioUnderflowTimer();
    GstEvent *flushStart = m_gstWrapper->gstEventNewFlushStart();
    if (!m_gstWrapper->gstElementSendEvent(source, flushStart))
    {
        RIALTO_SERVER_LOG_WARN("failed to send flush-start event");
    }
    GstEvent *flushStop = m_gstWrapper->gstEventNewFlushStop(FALSE);
    if (!m_gstWrapper->gstElementSendEvent(source, flushStop))
    {
        RIALTO_SERVER_LOG_WARN("failed to send flush-stop event");
    }

    // Turn audio off, removing audio sink from playsink
    m_player.setPlaybinFlags(false);

    RIALTO_SERVER_LOG_MIL("%s source removed", common::convertMediaSourceType(m_type));
}
} // namespace firebolt::rialto::server::tasks::generic
