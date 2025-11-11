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

#include "SetSourcePosition.h"
#include "RialtoServerLogging.h"
#include "TypeConverters.h"
#include "tasks/generic/NeedData.h"

namespace firebolt::rialto::server::tasks::generic
{
SetSourcePosition::SetSourcePosition(GenericPlayerContext &context, IGstGenericPlayerPrivate &player,
                                     IGstGenericPlayerClient *client,
                                     const std::shared_ptr<wrappers::IGlibWrapper> &glibWrapper,
                                     const MediaSourceType &type, std::int64_t position, bool resetTime,
                                     double appliedRate, uint64_t stopPosition)
    : m_context{context}, m_player(player), m_gstPlayerClient{client}, m_glibWrapper{glibWrapper}, m_type{type},
      m_position{position}, m_resetTime{resetTime}, m_appliedRate{appliedRate}, m_stopPosition{stopPosition}
{
    RIALTO_SERVER_LOG_DEBUG("Constructing SetSourcePosition");
}

SetSourcePosition::~SetSourcePosition()
{
    RIALTO_SERVER_LOG_DEBUG("SetSourcePosition finished");
}

void SetSourcePosition::execute() const
{
    RIALTO_SERVER_LOG_DEBUG("Executing SetSourcePosition for %s source", common::convertMediaSourceType(m_type));

    if (MediaSourceType::UNKNOWN == m_type)
    {
        RIALTO_SERVER_LOG_WARN("failed to set source position - source type is unknown");
        return;
    }

    // Get source first
    GstElement *source{nullptr};
    auto sourceElem = m_context.streamInfo.find(m_type);
    if (sourceElem != m_context.streamInfo.end())
    {
        source = sourceElem->second.appSrc;
    }
    if (!source)
    {
        RIALTO_SERVER_LOG_WARN("failed to set source position - %s source is NULL",
                               common::convertMediaSourceType(m_type));
        return;
    }

    if (MediaSourceType::VIDEO == m_type || MediaSourceType::AUDIO == m_type)
    {
        m_context.initialPositions[source].emplace_back(
            SegmentData{m_position, m_resetTime, m_appliedRate, m_stopPosition});
    }
    else if (MediaSourceType::SUBTITLE == m_type)
    {
        setSubtitlePosition(source);
    }

    if (m_context.setupSourceFinished)
    {
        // Reset Eos info
        m_context.endOfStreamInfo.erase(m_type);
        m_context.eosNotified = false;

        // Trigger NeedData for source
        NeedData task{m_context, m_player, m_gstPlayerClient, GST_APP_SRC(source)};
        task.execute();
    }
}

void SetSourcePosition::setSubtitlePosition(GstElement *source) const
{
    // in case of subtitles, all data might be already in the sink and we might not get any data anymore,
    // so set position here and to not depend on any following buffers
    if (m_context.setupSourceFinished)
    {
        m_glibWrapper->gObjectSet(m_context.subtitleSink, "position", static_cast<guint64>(m_position), nullptr);
    }
    else
    {
        m_context.initialPositions[source].emplace_back(
            SegmentData{m_position, m_resetTime, m_appliedRate, m_stopPosition});
    }
}

} // namespace firebolt::rialto::server::tasks::generic
