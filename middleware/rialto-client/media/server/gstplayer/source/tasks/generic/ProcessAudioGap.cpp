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

#include "ProcessAudioGap.h"
#include "RialtoServerLogging.h"

namespace firebolt::rialto::server::tasks::generic
{
ProcessAudioGap::ProcessAudioGap(
    GenericPlayerContext &context, const std::shared_ptr<firebolt::rialto::wrappers::IGstWrapper> &gstWrapper,
    const std::shared_ptr<firebolt::rialto::wrappers::IGlibWrapper> &glibWrapper,
    const std::shared_ptr<firebolt::rialto::wrappers::IRdkGstreamerUtilsWrapper> rdkGstreamerUtilsWrapper,
    std::int64_t position, std::uint32_t duration, std::int64_t discontinuityGap, bool audioAac)
    : m_context{context}, m_gstWrapper{gstWrapper}, m_glibWrapper{glibWrapper},
      m_rdkGstreamerUtilsWrapper{rdkGstreamerUtilsWrapper}, m_position{position}, m_duration{duration},
      m_discontinuityGap{discontinuityGap}, m_audioAac{audioAac}
{
    RIALTO_SERVER_LOG_DEBUG("Constructing ProcessAudioGap");
}

ProcessAudioGap::~ProcessAudioGap()
{
    RIALTO_SERVER_LOG_DEBUG("ProcessAudioGap finished");
}

void ProcessAudioGap::execute() const
{
    RIALTO_SERVER_LOG_DEBUG("Executing ProcessAudioGap");
    if (!m_context.pipeline)
    {
        RIALTO_SERVER_LOG_ERROR("Process audio gap failed - pipeline is null");
        return;
    }
    m_rdkGstreamerUtilsWrapper->processAudioGap(m_context.pipeline, m_position, m_duration, m_discontinuityGap,
                                                m_audioAac);
}
} // namespace firebolt::rialto::server::tasks::generic
