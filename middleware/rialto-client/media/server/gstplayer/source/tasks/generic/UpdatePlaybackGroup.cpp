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

#include "tasks/generic/UpdatePlaybackGroup.h"
#include "RialtoServerLogging.h"

namespace firebolt::rialto::server::tasks::generic
{
UpdatePlaybackGroup::UpdatePlaybackGroup(GenericPlayerContext &context, IGstGenericPlayerPrivate &player,
                                         std::shared_ptr<firebolt::rialto::wrappers::IGstWrapper> gstWrapper,
                                         std::shared_ptr<firebolt::rialto::wrappers::IGlibWrapper> glibWrapper,
                                         GstElement *typefind, const GstCaps *caps)
    : m_context{context}, m_player{player}, m_gstWrapper{gstWrapper}, m_glibWrapper{glibWrapper}, m_typefind{typefind},
      m_caps{caps}
{
    RIALTO_SERVER_LOG_DEBUG("Constructing UpdatePlaybackGroup");
}

UpdatePlaybackGroup::~UpdatePlaybackGroup()
{
    RIALTO_SERVER_LOG_DEBUG("UpdatePlaybackGroup finished");
}

void UpdatePlaybackGroup::execute() const
{
    RIALTO_SERVER_LOG_DEBUG("Executing UpdatePlaybackGroup");
    if (nullptr == m_caps)
    {
        RIALTO_SERVER_LOG_DEBUG("Typefind SRC Pad Caps NULL");
        return;
    }
    gchar *typefindCaps = m_gstWrapper->gstCapsToString(m_caps);
    if (typefindCaps)
    {
        RIALTO_SERVER_LOG_DEBUG("Typefind SRC Pad Strm Parsed Caps %s", typefindCaps);
        if (m_glibWrapper->gStrrstr(typefindCaps, "audio/"))
        {
            GstElement *typeFindParent = reinterpret_cast<GstElement *>(m_gstWrapper->gstElementGetParent(m_typefind));
            if (typeFindParent)
            {
                gchar *elementName = m_gstWrapper->gstElementGetName(typeFindParent);
                RIALTO_SERVER_LOG_DEBUG("elementName %s", elementName);
                if (elementName && m_glibWrapper->gStrrstr(elementName, "decodebin"))
                {
                    RIALTO_SERVER_LOG_DEBUG("m_context.playbackGroup.curAudioDecodeBin %s", elementName);
                    m_context.playbackGroup.m_curAudioDecodeBin = typeFindParent;
                    gchar *typefindName = m_gstWrapper->gstElementGetName(m_typefind);
                    RIALTO_SERVER_LOG_DEBUG("onTypeFound(): m_context.playbackGroup.curAudioTypefind %s", typefindName);
                    m_glibWrapper->gFree(typefindName);
                    m_context.playbackGroup.m_curAudioTypefind = m_typefind;
                    if (m_context.pendingUseBuffering.has_value())
                    {
                        m_player.setUseBuffering();
                    }
                }
                m_glibWrapper->gFree(elementName);
                m_gstWrapper->gstObjectUnref(typeFindParent);
            }
        }
        m_glibWrapper->gFree(typefindCaps);
    }
}
} // namespace firebolt::rialto::server::tasks::generic
