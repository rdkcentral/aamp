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

#include "tasks/generic/CheckAudioUnderflow.h"
#include "GenericPlayerContext.h"
#include "IGstGenericPlayerClient.h"
#include "IGstWrapper.h"
#include "RialtoServerLogging.h"
#include "tasks/generic/Underflow.h"
#include <gst/gst.h>

#include <cinttypes>

namespace firebolt::rialto::server::tasks::generic
{
CheckAudioUnderflow::CheckAudioUnderflow(GenericPlayerContext &context, IGstGenericPlayerPrivate &player,
                                         IGstGenericPlayerClient *client,
                                         std::shared_ptr<firebolt::rialto::wrappers::IGstWrapper> gstWrapper)
    : m_context{context}, m_player(player), m_gstPlayerClient{client}, m_gstWrapper{gstWrapper}
{
}

void CheckAudioUnderflow::execute() const
{
    // TODO(LLDEV-31012) Check if the audio stream is in underflow state.
    if (m_context.streamInfo.find(firebolt::rialto::MediaSourceType::AUDIO) != m_context.streamInfo.end())
    {
        gint64 position = m_player.getPosition(m_context.pipeline);
        if (position == -1)
        {
            RIALTO_SERVER_LOG_WARN("Getting the position failed");
            return;
        }

        constexpr int64_t kAudioUnderflowMarginNs = 350 * 1000000;
        if ((position > m_context.lastAudioSampleTimestamps + kAudioUnderflowMarginNs) &&
            m_gstWrapper->gstElementGetState(m_context.pipeline) == GST_STATE_PLAYING &&
            m_gstWrapper->gstElementGetPendingState(m_context.pipeline) != GST_STATE_PAUSED)
        {
            RIALTO_SERVER_LOG_WARN("Audio stream underflow! Position %" PRIu64 ", lastAudioSampleTimestamps: %" PRIu64,
                                   position, m_context.lastAudioSampleTimestamps);
            bool underflowEnabled = m_context.isPlaying && !m_context.audioSourceRemoved;
            Underflow task(m_context, m_player, m_gstPlayerClient, underflowEnabled, MediaSourceType::AUDIO);
            task.execute();
        }
    }
}

} // namespace firebolt::rialto::server::tasks::generic
