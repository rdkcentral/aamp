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

#include "tasks/generic/SetVolume.h"
#include "IRdkGstreamerUtilsWrapper.h"
#include "RialtoServerLogging.h"
#include <cmath>
#include <gst/gst.h>
#include <stdexcept>

namespace firebolt::rialto::server::tasks::generic
{
SetVolume::SetVolume(GenericPlayerContext &context, IGstGenericPlayerPrivate &player,
                     std::shared_ptr<firebolt::rialto::wrappers::IGstWrapper> gstWrapper,
                     std::shared_ptr<firebolt::rialto::wrappers::IGlibWrapper> glibWrapper,
                     std::shared_ptr<firebolt::rialto::wrappers::IRdkGstreamerUtilsWrapper> rdkGstreamerUtilsWrapper,
                     double targetVolume, uint32_t volumeDuration, firebolt::rialto::EaseType easeType)
    : m_context{context}, m_player{player}, m_gstWrapper{gstWrapper}, m_glibWrapper{glibWrapper},
      m_rdkGstreamerUtilsWrapper{rdkGstreamerUtilsWrapper}, m_targetVolume{targetVolume},
      m_volumeDuration{volumeDuration}, m_easeType{easeType}
{
    RIALTO_SERVER_LOG_DEBUG("Constructing SetVolume");
}

SetVolume::~SetVolume()
{
    RIALTO_SERVER_LOG_DEBUG("SetVolume finished");
}

firebolt::rialto::wrappers::rgu_Ease convertEaseTypeToRguEase(EaseType easeType)
{
    switch (easeType)
    {
    case EaseType::EASE_LINEAR:
        return firebolt::rialto::wrappers::rgu_Ease::EaseLinear;
    case EaseType::EASE_IN_CUBIC:
        return firebolt::rialto::wrappers::rgu_Ease::EaseInCubic;
    case EaseType::EASE_OUT_CUBIC:
        return firebolt::rialto::wrappers::rgu_Ease::EaseOutCubic;
    default:
        RIALTO_SERVER_LOG_ERROR("Unknown EaseType, defaulting to EaseLinear");
        return firebolt::rialto::wrappers::rgu_Ease::EaseLinear;
    }
}

void SetVolume::execute() const
{
    RIALTO_SERVER_LOG_DEBUG("Executing SetVolume");

    if (!m_context.pipeline)
    {
        RIALTO_SERVER_LOG_ERROR("Setting volume failed. Pipeline is NULL");
        return;
    }
    bool isImmediateVolumeChange = (m_volumeDuration == 0);
    GstElement *audioSink = m_player.getSink(firebolt::rialto::MediaSourceType::AUDIO);

    if (isImmediateVolumeChange)
    {
        RIALTO_SERVER_LOG_DEBUG("Immediate volume change, setting volume directly");
        m_gstWrapper->gstStreamVolumeSetVolume(GST_STREAM_VOLUME(m_context.pipeline), GST_STREAM_VOLUME_FORMAT_LINEAR,
                                               m_targetVolume);
    }
    else if (audioSink && m_glibWrapper->gObjectClassFindProperty(G_OBJECT_GET_CLASS(audioSink), "audio-fade"))
    {
        gchar fadeStr[32];
        uint32_t scaledTarget = trunc(100 * m_targetVolume);
        std::string easeString = "L";

        switch (m_easeType)
        {
        default:
        case firebolt::rialto::EaseType::EASE_LINEAR:
            easeString = "L";
            break;
        case firebolt::rialto::EaseType::EASE_IN_CUBIC:
            easeString = "I";
            break;
        case firebolt::rialto::EaseType::EASE_OUT_CUBIC:
            easeString = "O";
            break;
        }

        snprintf(reinterpret_cast<gchar *>(fadeStr), sizeof(fadeStr), "%u,%u,%s", scaledTarget, m_volumeDuration,
                 easeString.c_str());
        RIALTO_SERVER_LOG_DEBUG("Fade String: %s", fadeStr);

        m_glibWrapper->gObjectSet(audioSink, "audio-fade", fadeStr, nullptr);
        m_context.audioFadeEnabled = true;
    }
    else if (m_rdkGstreamerUtilsWrapper->isSocAudioFadeSupported())
    {
        RIALTO_SERVER_LOG_DEBUG("SOC audio fading is supported, applying SOC audio fade");
        auto rguEaseType = convertEaseTypeToRguEase(m_easeType);
        m_rdkGstreamerUtilsWrapper->doAudioEasingonSoc(m_targetVolume, m_volumeDuration, rguEaseType);
        m_context.audioFadeEnabled = true;
    }
    else
    {
        RIALTO_SERVER_LOG_WARN("No audio-fade property found in audio sink or SOC");
    }

    if (audioSink)
        m_gstWrapper->gstObjectUnref(GST_OBJECT(audioSink));
}
} // namespace firebolt::rialto::server::tasks::generic
