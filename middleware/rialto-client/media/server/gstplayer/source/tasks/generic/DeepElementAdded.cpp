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

#include "tasks/generic/DeepElementAdded.h"
#include "RialtoServerLogging.h"

namespace
{
void onHaveType(GstElement *typefind, guint probability, const GstCaps *caps, gpointer data)
{
    firebolt::rialto::server::IGstGenericPlayerPrivate *player =
        static_cast<firebolt::rialto::server::IGstGenericPlayerPrivate *>(data);
    player->updatePlaybackGroup(typefind, caps);
}
} // namespace

namespace firebolt::rialto::server::tasks::generic
{
DeepElementAdded::DeepElementAdded(GenericPlayerContext &context, IGstGenericPlayerPrivate &player,
                                   const std::shared_ptr<firebolt::rialto::wrappers::IGstWrapper> &gstWrapper,
                                   const std::shared_ptr<firebolt::rialto::wrappers::IGlibWrapper> &glibWrapper,
                                   GstBin *pipeline, GstBin *bin, GstElement *element)
    : m_context{context}, m_player{player}, m_gstWrapper{gstWrapper}, m_glibWrapper{glibWrapper}, m_pipeline{pipeline},
      m_bin{bin}, m_element{element}, m_elementName{nullptr}, m_callbackRegistered{false}
{
    RIALTO_SERVER_LOG_DEBUG("Constructing DeepElementAdded");
    // Signal connection has to happen immediately (we cannot wait for thread switch)
    if (m_gstWrapper->gstObjectParent(m_element) == m_gstWrapper->gstObjectCast(m_bin))
    {
        m_elementName = m_gstWrapper->gstElementGetName(m_element);
        if (m_elementName)
        {
            RIALTO_SERVER_LOG_DEBUG("Element Name = %s", m_elementName);
            if (m_glibWrapper->gStrrstr(m_elementName, "typefind"))
            {
                RIALTO_SERVER_LOG_DEBUG("Registering onHaveType callback");
                m_glibWrapper->gSignalConnect(G_OBJECT(m_element), "have-type", G_CALLBACK(onHaveType), &m_player);
                m_callbackRegistered = true;
            }
        }
    }
}

DeepElementAdded::~DeepElementAdded()
{
    RIALTO_SERVER_LOG_DEBUG("DeepElementAdded finished");
    m_glibWrapper->gFree(m_elementName);
}

void DeepElementAdded::execute() const
{
    RIALTO_SERVER_LOG_DEBUG("Executing DeepElementAdded");
    m_context.playbackGroup.m_gstPipeline = GST_ELEMENT(m_pipeline);

    RIALTO_SERVER_LOG_DEBUG("Element = %p Bin = %p Pipeline = %p", m_element, m_bin, m_pipeline);
    if (m_callbackRegistered)
    {
        m_context.playbackGroup.m_curAudioTypefind = m_element;
    }
    if (m_elementName)
    {
        if (m_gstWrapper->gstObjectCast(m_bin) == m_gstWrapper->gstObjectCast(m_context.playbackGroup.m_curAudioDecodeBin))
        {
            if (m_glibWrapper->gStrrstr(m_elementName, "parse"))
            {
                RIALTO_SERVER_LOG_DEBUG("curAudioParse = %s", m_elementName);
                m_context.playbackGroup.m_curAudioParse = m_element;
            }
            else if (m_glibWrapper->gStrrstr(m_elementName, "dec"))
            {
                RIALTO_SERVER_LOG_DEBUG("curAudioDecoder = %s", m_elementName);
                m_context.playbackGroup.m_curAudioDecoder = m_element;
            }
        }
        else
        {
            if (m_glibWrapper->gStrrstr(m_elementName, "audiosink"))
            {
                GstElement *audioSinkParent =
                    reinterpret_cast<GstElement *>(m_gstWrapper->gstElementGetParent(m_element));
                if (audioSinkParent)
                {
                    gchar *audioSinkParentName = m_gstWrapper->gstElementGetName(audioSinkParent);
                    RIALTO_SERVER_LOG_DEBUG("audioSinkParentName = %s", audioSinkParentName);
                    if (audioSinkParentName && m_glibWrapper->gStrrstr(audioSinkParentName, "bin"))
                    {
                        RIALTO_SERVER_LOG_DEBUG("curAudioPlaysinkBin = %s", audioSinkParentName);
                        m_context.playbackGroup.m_curAudioPlaysinkBin = audioSinkParent;
                    }
                    m_glibWrapper->gFree(audioSinkParentName);
                }
            }
        }
    }
}
} // namespace firebolt::rialto::server::tasks::generic
