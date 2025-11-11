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

#include "tasks/webAudio/Eos.h"
#include "IGstWrapper.h"
#include "RialtoServerLogging.h"
#include "WebAudioPlayerContext.h"

namespace firebolt::rialto::server::tasks::webaudio
{
Eos::Eos(WebAudioPlayerContext &context, std::shared_ptr<firebolt::rialto::wrappers::IGstWrapper> gstWrapper)
    : m_context{context}, m_gstWrapper{gstWrapper}
{
    RIALTO_SERVER_LOG_DEBUG("Constructing Eos");
}

Eos::~Eos()
{
    RIALTO_SERVER_LOG_DEBUG("Eos finished");
}

void Eos::execute() const
{
    RIALTO_SERVER_LOG_DEBUG("Executing Eos");
    if (m_gstWrapper->gstAppSrcEndOfStream(GST_APP_SRC(m_context.source)) != GST_FLOW_OK)
    {
        RIALTO_SERVER_LOG_WARN("Set eos failed - Gstreamer error");
    }
    RIALTO_SERVER_LOG_MIL("EOS set for webaudio source");
}
} // namespace firebolt::rialto::server::tasks::webaudio
