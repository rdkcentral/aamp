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

#include <gst/gst.h>

#include "RialtoServerLogging.h"
#include "tasks/webAudio/SetVolume.h"

namespace firebolt::rialto::server::tasks::webaudio
{
SetVolume::SetVolume(WebAudioPlayerContext &context,
                     std::shared_ptr<firebolt::rialto::wrappers::IGstWrapper> gstWrapper, double volume)
    : m_context{context}, m_gstWrapper{gstWrapper}, m_volume{volume}
{
    RIALTO_SERVER_LOG_DEBUG("Constructing SetVolume");
}

SetVolume::~SetVolume()
{
    RIALTO_SERVER_LOG_DEBUG("SetVolume finished");
}

void SetVolume::execute() const
{
    RIALTO_SERVER_LOG_DEBUG("WebAudio Executing SetVolume %f", m_volume);
    m_gstWrapper->gstStreamVolumeSetVolume(m_context.gstVolumeElement, GST_STREAM_VOLUME_FORMAT_LINEAR, m_volume);
}
} // namespace firebolt::rialto::server::tasks::webaudio
