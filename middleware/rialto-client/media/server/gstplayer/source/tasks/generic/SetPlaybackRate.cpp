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

#include "tasks/generic/SetPlaybackRate.h"
#include "IGlibWrapper.h"
#include "IGstWrapper.h"
#include "RialtoServerLogging.h"
#include <gst/base/gstbasesink.h>

namespace
{
const char kCustomInstantRateChangeEventName[] = "custom-instant-rate-change";
} // namespace

namespace firebolt::rialto::server::tasks::generic
{
SetPlaybackRate::SetPlaybackRate(GenericPlayerContext &context,
                                 std::shared_ptr<firebolt::rialto::wrappers::IGstWrapper> gstWrapper,
                                 std::shared_ptr<firebolt::rialto::wrappers::IGlibWrapper> glibWrapper, double rate)
    : m_context{context}, m_gstWrapper{gstWrapper}, m_glibWrapper{glibWrapper}, m_rate{rate}
{
    RIALTO_SERVER_LOG_DEBUG("Constructing SetPlaybackRate");
}

SetPlaybackRate::~SetPlaybackRate()
{
    RIALTO_SERVER_LOG_DEBUG("SetPlaybackRate finished");
}

void SetPlaybackRate::execute() const
{
    RIALTO_SERVER_LOG_DEBUG("Executing SetPlaybackRate");
    if (m_context.playbackRate == m_rate)
    {
        RIALTO_SERVER_LOG_DEBUG("No need to change playback rate - it is already %lf", m_rate);
        return;
    }

    if (!m_context.pipeline)
    {
        RIALTO_SERVER_LOG_INFO("Postponing set playback rate to %lf. Pipeline is NULL", m_rate);
        m_context.pendingPlaybackRate = m_rate;
        return;
    }

    if (GST_STATE(m_context.pipeline) < GST_STATE_PLAYING)
    {
        RIALTO_SERVER_LOG_INFO("Postponing set playback rate to %lf. Pipeline state is below PLAYING", m_rate);
        m_context.pendingPlaybackRate = m_rate;
        return;
    }
    m_context.pendingPlaybackRate = kNoPendingPlaybackRate;

    GstElement *audioSink{nullptr};
    gboolean success{FALSE};
    m_glibWrapper->gObjectGet(m_context.pipeline, "audio-sink", &audioSink, nullptr);
    if (audioSink && m_glibWrapper->gStrHasPrefix(GST_ELEMENT_NAME(audioSink), "amlhalasink"))
    {
        GstSegment *segment{m_gstWrapper->gstSegmentNew()};
        m_gstWrapper->gstSegmentInit(segment, GST_FORMAT_TIME);
        segment->rate = m_rate;
        segment->start = GST_CLOCK_TIME_NONE;
        segment->position = GST_CLOCK_TIME_NONE;
        success = m_gstWrapper->gstPadSendEvent(GST_BASE_SINK_PAD(audioSink), m_gstWrapper->gstEventNewSegment(segment));
        RIALTO_SERVER_LOG_DEBUG("Sent new segment, success = %s", success ? "true" : "false");
        m_gstWrapper->gstSegmentFree(segment);
    }
    else
    {
        GstStructure *structure{
            m_gstWrapper->gstStructureNew(kCustomInstantRateChangeEventName, "rate", G_TYPE_DOUBLE, m_rate, NULL)};
        success = m_gstWrapper->gstElementSendEvent(m_context.pipeline,
                                                    m_gstWrapper->gstEventNewCustom(GST_EVENT_CUSTOM_DOWNSTREAM_OOB,
                                                                                    structure));
        RIALTO_SERVER_LOG_DEBUG("Sent new event, success = %s", success ? "true" : "false");
    }

    if (success)
    {
        RIALTO_SERVER_LOG_MIL("Playback rate set to: %lf", m_rate);
        m_context.playbackRate = m_rate;
    }

    if (audioSink)
    {
        m_glibWrapper->gObjectUnref(audioSink);
    }
}
} // namespace firebolt::rialto::server::tasks::generic
