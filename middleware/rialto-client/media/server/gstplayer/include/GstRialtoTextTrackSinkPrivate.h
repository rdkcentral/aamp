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

#ifndef FIREBOLT_RIALTO_SERVER_GST_RIALTO_TEXT_TRACK_SINK_PRIVATE_H_
#define FIREBOLT_RIALTO_SERVER_GST_RIALTO_TEXT_TRACK_SINK_PRIVATE_H_

#include "ITextTrackSession.h"
#include <atomic>
#include <memory>
#include <mutex>
#include <optional>
#include <string>

namespace firebolt::rialto::server
{
struct GstRialtoTextTrackSinkPrivate
{
    std::unique_ptr<ITextTrackSession> m_textTrackSession;
    std::atomic<bool> m_isMuted{false};
    std::string m_textTrackIdentifier;
    uint64_t m_videoDecoderIdentifier{0};
    bool m_capsSet{false};
    std::optional<uint64_t> m_position;
    std::optional<uint64_t> m_queuedPosition;
    std::optional<uint64_t> m_offset;
    std::optional<uint64_t> m_queuedOffset;
    std::mutex m_mutex;
};
}; // namespace firebolt::rialto::server

#endif // FIREBOLT_RIALTO_SERVER_GST_RIALTO_TEXT_TRACK_SINK_PRIVATE_H_
