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

#include "tasks/generic/AttachSource.h"
#include "GstMimeMapping.h"
#include "IGlibWrapper.h"
#include "IGstWrapper.h"
#include "IMediaPipeline.h"
#include "RialtoServerLogging.h"
#include "TypeConverters.h"
#include "Utils.h"
#include <unordered_map>

namespace firebolt::rialto::server::tasks::generic
{
AttachSource::AttachSource(GenericPlayerContext &context,
                           const std::shared_ptr<firebolt::rialto::wrappers::IGstWrapper> &gstWrapper,
                           const std::shared_ptr<firebolt::rialto::wrappers::IGlibWrapper> &glibWrapper,
                           const std::shared_ptr<IGstTextTrackSinkFactory> &gstTextTrackSinkFactory,
                           IGstGenericPlayerPrivate &player, const std::unique_ptr<IMediaPipeline::MediaSource> &source)
    : m_context{context}, m_gstWrapper{gstWrapper}, m_glibWrapper{glibWrapper},
      m_gstTextTrackSinkFactory{gstTextTrackSinkFactory}, m_player{player}, m_attachedSource{source->copy()}
{
    RIALTO_SERVER_LOG_DEBUG("Constructing AttachSource");
}

AttachSource::~AttachSource()
{
    RIALTO_SERVER_LOG_DEBUG("AttachSource finished");
}

void AttachSource::execute() const
{
    RIALTO_SERVER_LOG_DEBUG("Executing AttachSource %u", static_cast<uint32_t>(m_attachedSource->getType()));

    if (m_attachedSource->getType() == MediaSourceType::UNKNOWN)
    {
        RIALTO_SERVER_LOG_ERROR("Unknown media source type");
        return;
    }

    if (m_context.streamInfo.find(m_attachedSource->getType()) == m_context.streamInfo.end())
    {
        addSource();
    }
    else if (m_attachedSource->getType() == MediaSourceType::AUDIO && m_context.audioSourceRemoved)
    {
        reattachAudioSource();
    }
    else
    {
        RIALTO_SERVER_LOG_ERROR("cannot update caps");
    }
}

void AttachSource::addSource() const
{
    GstCaps *caps = createCapsFromMediaSource(m_gstWrapper, m_glibWrapper, m_attachedSource);
    if (!caps)
    {
        RIALTO_SERVER_LOG_ERROR("Failed to create caps from media source");
        return;
    }
    gchar *capsStr = m_gstWrapper->gstCapsToString(caps);
    GstElement *appSrc = nullptr;
    if (m_attachedSource->getType() == MediaSourceType::AUDIO)
    {
        RIALTO_SERVER_LOG_MIL("Adding Audio appsrc with caps %s", capsStr);
        appSrc = m_gstWrapper->gstElementFactoryMake("appsrc", "audsrc");
    }
    else if (m_attachedSource->getType() == MediaSourceType::VIDEO)
    {
        RIALTO_SERVER_LOG_MIL("Adding Video appsrc with caps %s", capsStr);
        appSrc = m_gstWrapper->gstElementFactoryMake("appsrc", "vidsrc");
    }
    else if (m_attachedSource->getType() == MediaSourceType::SUBTITLE)
    {
        RIALTO_SERVER_LOG_MIL("Adding Subtitle appsrc with caps %s", capsStr);
        appSrc = m_gstWrapper->gstElementFactoryMake("appsrc", "subsrc");

        if (m_glibWrapper->gObjectClassFindProperty(G_OBJECT_GET_CLASS(m_context.pipeline), "text-sink"))
        {
            GstElement *elem = m_gstTextTrackSinkFactory->createGstTextTrackSink();
            m_context.subtitleSink = elem;

            m_glibWrapper->gObjectSet(m_context.pipeline, "text-sink", elem, nullptr);
        }
    }
    m_glibWrapper->gFree(capsStr);

    m_gstWrapper->gstAppSrcSetCaps(GST_APP_SRC(appSrc), caps);
    m_context.streamInfo.emplace(m_attachedSource->getType(), StreamInfo{appSrc, m_attachedSource->getHasDrm()});

    if (caps)
        m_gstWrapper->gstCapsUnref(caps);
}

void AttachSource::reattachAudioSource() const
{
    if (!m_player.reattachSource(m_attachedSource))
    {
        RIALTO_SERVER_LOG_ERROR("Reattaching source failed!");
        return;
    }

    // Restart audio sink
    m_player.setPlaybinFlags(true);

    m_context.streamInfo[m_attachedSource->getType()].isDataNeeded = true;
    m_context.audioSourceRemoved = false;
    m_player.notifyNeedMediaData(MediaSourceType::AUDIO);

    RIALTO_SERVER_LOG_MIL("Audio source reattached");
}
} // namespace firebolt::rialto::server::tasks::generic
