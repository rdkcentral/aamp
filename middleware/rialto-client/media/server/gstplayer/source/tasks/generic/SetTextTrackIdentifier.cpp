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

#include "tasks/generic/SetTextTrackIdentifier.h"
#include "RialtoServerLogging.h"
#include <gst/gst.h>

namespace firebolt::rialto::server::tasks::generic
{
SetTextTrackIdentifier::SetTextTrackIdentifier(GenericPlayerContext &context,
                                               std::shared_ptr<firebolt::rialto::wrappers::IGlibWrapper> glibWrapper,
                                               const std::string &textTrackIdentifier)
    : m_context{context}, m_glibWrapper{glibWrapper}, m_textTrackIdentifier{textTrackIdentifier}
{
    RIALTO_SERVER_LOG_DEBUG("Constructing SetTextTrackIdentifier");
}

SetTextTrackIdentifier::~SetTextTrackIdentifier()
{
    RIALTO_SERVER_LOG_DEBUG("SetTextTrackIdentifier finished");
}

void SetTextTrackIdentifier::execute() const
{
    RIALTO_SERVER_LOG_DEBUG("Executing SetTextTrackIdentifier");

    if (!m_context.subtitleSink)
    {
        RIALTO_SERVER_LOG_ERROR("There is no subtitle sink");
        return;
    }

    m_glibWrapper->gObjectSet(m_context.subtitleSink, "text-track-identifier", m_textTrackIdentifier.c_str(), nullptr);
}
} // namespace firebolt::rialto::server::tasks::generic
