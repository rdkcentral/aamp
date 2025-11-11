/*
 * If not stated otherwise in this file or this component's LICENSE file the
 * following copyright and licenses apply:
 *
 * Copyright 2025 
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

#include "SetSubtitleOffset.h"
#include "RialtoServerLogging.h"
#include <cinttypes>

namespace firebolt::rialto::server::tasks::generic
{
SetSubtitleOffset::SetSubtitleOffset(GenericPlayerContext &context,
                                     const std::shared_ptr<wrappers::IGlibWrapper> &glibWrapper, std::int64_t position)
    : m_context{context}, m_glibWrapper{glibWrapper}, m_position{position}
{
    RIALTO_SERVER_LOG_DEBUG("Constructing SetSubtitleOffset");
}

SetSubtitleOffset::~SetSubtitleOffset()
{
    RIALTO_SERVER_LOG_DEBUG("SetSubtitleOffset finished");
}

void SetSubtitleOffset::execute() const
{
    RIALTO_SERVER_LOG_DEBUG("Executing SetSubtitleOffset");

    if (m_context.subtitleSink)
    {
        RIALTO_SERVER_LOG_DEBUG("Setting subtitle offset to %" PRId64 " nanoseconds", m_position);
        m_glibWrapper->gObjectSet(m_context.subtitleSink, "offset", static_cast<gint64>(m_position), nullptr);
    }
    else
    {
        RIALTO_SERVER_LOG_WARN("Failed to set subtitle offset - subtitle sink is NULL");
    }
}

} // namespace firebolt::rialto::server::tasks::generic
