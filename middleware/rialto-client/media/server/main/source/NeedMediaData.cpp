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

#include "NeedMediaData.h"
#include "IActiveRequests.h"
#include "IMediaPipelineClient.h"
#include "ISharedMemoryBuffer.h"
#include "RialtoServerLogging.h"
#include "ShmUtils.h"
#include "TypeConverters.h"

namespace firebolt::rialto::server
{
NeedMediaData::NeedMediaData(std::weak_ptr<IMediaPipelineClient> client, IActiveRequests &activeRequests,
                             const ISharedMemoryBuffer &shmBuffer, int sessionId, MediaSourceType mediaSourceType,
                             std::int32_t sourceId, PlaybackState currentPlaybackState)
    : m_client{client}, m_activeRequests{activeRequests}, m_mediaSourceType{mediaSourceType}, m_frameCount{kMaxFrames},
      m_sourceId{sourceId}
{
    if (PlaybackState::PLAYING != currentPlaybackState)
    {
        RIALTO_SERVER_LOG_DEBUG("Pipeline in prerolling state. Sending smaller frame count for %s",
                                common::convertMediaSourceType(m_mediaSourceType));
        m_frameCount = kPrerollNumFrames;
    }
    if (MediaSourceType::AUDIO != mediaSourceType && MediaSourceType::VIDEO != mediaSourceType &&
        MediaSourceType::SUBTITLE != mediaSourceType)
    {
        RIALTO_SERVER_LOG_ERROR("Unable to initialize NeedMediaData - unknown mediaSourceType: %s",
                                common::convertMediaSourceType(m_mediaSourceType));
        m_isValid = false;
        return;
    }
    try
    {
        m_maxMediaBytes =
            shmBuffer.getMaxDataLen(ISharedMemoryBuffer::MediaPlaybackType::GENERIC, sessionId, mediaSourceType) -
            getMaxMetadataBytes();
        auto metadataOffset =
            shmBuffer.getDataOffset(ISharedMemoryBuffer::MediaPlaybackType::GENERIC, sessionId, mediaSourceType);
        auto mediadataOffset = metadataOffset + getMaxMetadataBytes();
        m_shmInfo = std::make_shared<MediaPlayerShmInfo>(
            MediaPlayerShmInfo{getMaxMetadataBytes(), metadataOffset, mediadataOffset, m_maxMediaBytes});
        m_isValid = true;
    }
    catch (const std::exception &e)
    {
        RIALTO_SERVER_LOG_ERROR("Unable to construct NeedMediaData message for %s - %s",
                                common::convertMediaSourceType(m_mediaSourceType), e.what());
        m_isValid = false;
    }
}

bool NeedMediaData::send() const
{
    auto client = m_client.lock();
    if (client && m_isValid)
    {
        client->notifyNeedMediaData(m_sourceId, m_frameCount,
                                    m_activeRequests.insert(m_mediaSourceType, m_maxMediaBytes), m_shmInfo);
        return true;
    }
    return false;
}
} // namespace firebolt::rialto::server
