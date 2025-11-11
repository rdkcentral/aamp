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

#include "tasks/generic/SetMute.h"
#include "RialtoServerLogging.h"
#include "TypeConverters.h"
#include <gst/gst.h>

namespace firebolt::rialto::server::tasks::generic
{
SetMute::SetMute(GenericPlayerContext &context, IGstGenericPlayerPrivate &player,
                 std::shared_ptr<firebolt::rialto::wrappers::IGstWrapper> gstWrapper,
                 std::shared_ptr<firebolt::rialto::wrappers::IGlibWrapper> glibWrapper,
                 const MediaSourceType &mediaSourceType, bool mute)
    : m_context{context}, m_player{player}, m_gstWrapper{gstWrapper}, m_glibWrapper{glibWrapper},
      m_mediaSourceType{mediaSourceType}, m_mute{mute}
{
    RIALTO_SERVER_LOG_DEBUG("Constructing SetMute");
}

SetMute::~SetMute()
{
    RIALTO_SERVER_LOG_DEBUG("SetMute finished");
}

void SetMute::execute() const
{
    RIALTO_SERVER_LOG_DEBUG("Executing SetMute");
    if (m_mediaSourceType == MediaSourceType::SUBTITLE)
    {
        if (!m_context.subtitleSink)
        {
            RIALTO_SERVER_LOG_ERROR("There is no subtitle sink");
            return;
        }
        m_glibWrapper->gObjectSet(m_context.subtitleSink, "mute", m_mute, nullptr);
    }
    else if (m_mediaSourceType == MediaSourceType::AUDIO)
    {
        if (!m_context.pipeline)
        {
            RIALTO_SERVER_LOG_ERROR("Setting mute failed. Pipeline is NULL");
            return;
        }
        m_gstWrapper->gstStreamVolumeSetMute(GST_STREAM_VOLUME(m_context.pipeline), m_mute);
    }
    else if (m_mediaSourceType == MediaSourceType::VIDEO)
    {
        m_context.pendingShowVideoWindow = !m_mute;
        m_player.setShowVideoWindow();
    }
    else
    {
        RIALTO_SERVER_LOG_ERROR("Setting mute for type %s unsupported",
                                common::convertMediaSourceType(m_mediaSourceType));
        return;
    }
    RIALTO_SERVER_LOG_MIL("%s source %s", common::convertMediaSourceType(m_mediaSourceType),
                          (m_mute ? "muted" : "unmuted"));
}
} // namespace firebolt::rialto::server::tasks::generic
