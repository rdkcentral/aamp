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

#include "tasks/generic/SetPosition.h"
#include "GenericPlayerContext.h"
#include "IGstGenericPlayerClient.h"
#include "IGstGenericPlayerPrivate.h"
#include "IGstWrapper.h"
#include "RialtoServerLogging.h"
#include "tasks/generic/NeedData.h"

namespace firebolt::rialto::server::tasks::generic
{
SetPosition::SetPosition(GenericPlayerContext &context, IGstGenericPlayerPrivate &player, IGstGenericPlayerClient *client,
                         std::shared_ptr<firebolt::rialto::wrappers::IGstWrapper> gstWrapper, std::int64_t position)
    : m_context{context}, m_player{player}, m_gstPlayerClient{client}, m_gstWrapper{gstWrapper}, m_position{position}
{
    RIALTO_SERVER_LOG_DEBUG("Constructing SetPosition");
}

SetPosition::~SetPosition()
{
    RIALTO_SERVER_LOG_DEBUG("SetPosition finished");
}

void SetPosition::execute() const
{
    RIALTO_SERVER_LOG_DEBUG("Executing SetPosition");
    if (!m_gstPlayerClient)
    {
        RIALTO_SERVER_LOG_ERROR("Seek failed - GstPlayerClient is NULL");
        return;
    }
    m_gstPlayerClient->notifyPlaybackState(PlaybackState::SEEKING);

    // Stop sending new NeedMediaData requests
    for (auto &elem : m_context.streamInfo)
    {
        StreamInfo &streamInfo = elem.second;
        streamInfo.isDataNeeded = false;
        streamInfo.isNeedDataPending = false;
        m_player.clearNeedDataScheduled(GST_APP_SRC(streamInfo.appSrc));

        // Clear buffered samples for player session
        for (auto &buffer : streamInfo.buffers)
        {
            m_gstWrapper->gstBufferUnref(buffer);
        }

        streamInfo.buffers.clear();
    }

    // Clear local cache of any active data requests for player session
    m_gstPlayerClient->clearActiveRequestsCache();
    m_context.lastAudioSampleTimestamps = m_position;

    if (!m_context.pipeline)
    {
        RIALTO_SERVER_LOG_ERROR("Seek failed - pipeline is null");
        m_gstPlayerClient->notifyPlaybackState(PlaybackState::FAILURE);
        return;
    }
    if (!m_gstWrapper->gstElementSeek(m_context.pipeline, m_context.playbackRate, GST_FORMAT_TIME,
                                      static_cast<GstSeekFlags>(GST_SEEK_FLAG_FLUSH), GST_SEEK_TYPE_SET, m_position,
                                      GST_SEEK_TYPE_NONE, GST_CLOCK_TIME_NONE))
    {
        RIALTO_SERVER_LOG_ERROR("Seek failed - gstreamer error");
        m_gstPlayerClient->notifyPlaybackState(PlaybackState::FAILURE);
        return;
    }

    RIALTO_SERVER_LOG_MIL("Pipeline seek to: %" GST_TIME_FORMAT, GST_TIME_ARGS(m_position));

    // Reset Eos info
    m_context.endOfStreamInfo.clear();
    m_context.eosNotified = false;

    m_gstPlayerClient->notifyPlaybackState(PlaybackState::SEEK_DONE);

    // Trigger NeedMediaData for all attached sources
    for (const auto &streamInfo : m_context.streamInfo)
    {
        if (streamInfo.second.appSrc)
        {
            NeedData task{m_context, m_player, m_gstPlayerClient, GST_APP_SRC(streamInfo.second.appSrc)};
            task.execute();
        }
    }
}
} // namespace firebolt::rialto::server::tasks::generic
