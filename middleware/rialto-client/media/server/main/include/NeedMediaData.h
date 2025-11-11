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

#ifndef FIREBOLT_RIALTO_SERVER_NEED_MEDIA_DATA_H_
#define FIREBOLT_RIALTO_SERVER_NEED_MEDIA_DATA_H_

#include "IActiveRequests.h"
#include "IMediaPipelineClient.h"
#include "ISharedMemoryBuffer.h"
#include "MediaCommon.h"
#include <cstdint>
#include <memory>

namespace firebolt::rialto::server
{
class NeedMediaData
{
public:
    NeedMediaData(std::weak_ptr<IMediaPipelineClient> client, IActiveRequests &activeRequests,
                  const ISharedMemoryBuffer &shmBuffer, int sessionId, MediaSourceType mediaSourceType,
                  std::int32_t sourceId, PlaybackState currentPlaybackState);
    ~NeedMediaData() = default;

    bool send() const;

private:
    std::weak_ptr<IMediaPipelineClient> m_client;
    IActiveRequests &m_activeRequests;
    MediaSourceType m_mediaSourceType;
    std::uint32_t m_frameCount;
    std::int32_t m_sourceId;
    std::uint32_t m_maxMediaBytes;
    std::shared_ptr<MediaPlayerShmInfo> m_shmInfo;
    bool m_isValid;
};
} // namespace firebolt::rialto::server

#endif // FIREBOLT_RIALTO_SERVER_NEED_MEDIA_DATA_H_
