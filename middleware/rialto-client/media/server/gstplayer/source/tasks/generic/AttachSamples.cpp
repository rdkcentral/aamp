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

#include "tasks/generic/AttachSamples.h"
#include "GenericPlayerContext.h"
#include "IGstGenericPlayerPrivate.h"
#include "RialtoServerLogging.h"
#include "TypeConverters.h"

namespace firebolt::rialto::server::tasks::generic
{
AttachSamples::AttachSamples(GenericPlayerContext &context,
                             const std::shared_ptr<firebolt::rialto::wrappers::IGstWrapper> &gstWrapper,
                             IGstGenericPlayerPrivate &player, const IMediaPipeline::MediaSegmentVector &mediaSegments)
    : m_context{context}, m_gstWrapper{gstWrapper}, m_player{player}
{
    RIALTO_SERVER_LOG_DEBUG("Constructing AttachSamples");
    for (const auto &mediaSegment : mediaSegments)
    {
        GstBuffer *gstBuffer = m_player.createBuffer(*mediaSegment);
        if (mediaSegment->getType() == firebolt::rialto::MediaSourceType::VIDEO)
        {
            try
            {
                IMediaPipeline::MediaSegmentVideo &videoSegment =
                    dynamic_cast<IMediaPipeline::MediaSegmentVideo &>(*mediaSegment);
                VideoData videoData = {gstBuffer, videoSegment.getWidth(), videoSegment.getHeight(),
                                       videoSegment.getFrameRate(), videoSegment.getCodecData()};
                m_videoData.push_back(videoData);
            }
            catch (const std::exception &e)
            {
                // Catching error, but continuing as best as we can
                RIALTO_SERVER_LOG_ERROR("Failed to get the video segment, reason: %s", e.what());
            }
        }
        else if (mediaSegment->getType() == firebolt::rialto::MediaSourceType::AUDIO)
        {
            try
            {
                IMediaPipeline::MediaSegmentAudio &audioSegment =
                    dynamic_cast<IMediaPipeline::MediaSegmentAudio &>(*mediaSegment);
                AudioData audioData = {gstBuffer,
                                       audioSegment.getSampleRate(),
                                       audioSegment.getNumberOfChannels(),
                                       audioSegment.getCodecData(),
                                       audioSegment.getClippingStart(),
                                       audioSegment.getClippingEnd()};
                m_audioData.push_back(audioData);
            }
            catch (const std::exception &e)
            {
                // Catching error, but continuing as best as we can
                RIALTO_SERVER_LOG_ERROR("Failed to get the audio segment, reason: %s", e.what());
            }
        }
        else if (mediaSegment->getType() == firebolt::rialto::MediaSourceType::SUBTITLE)
        {
            m_subtitleData.push_back(gstBuffer);
        }
    }
}

AttachSamples::~AttachSamples()
{
    RIALTO_SERVER_LOG_DEBUG("AttachSamples finished");
}

void AttachSamples::execute() const
{
    RIALTO_SERVER_LOG_DEBUG("Executing AttachSamples");
    for (AudioData audioData : m_audioData)
    {
        m_player.updateAudioCaps(audioData.rate, audioData.channels, audioData.codecData);
        m_player.addAudioClippingToBuffer(audioData.buffer, audioData.clippingStart, audioData.clippingEnd);

        attachData(firebolt::rialto::MediaSourceType::AUDIO, audioData.buffer);
    }
    for (VideoData videoData : m_videoData)
    {
        m_player.updateVideoCaps(videoData.width, videoData.height, videoData.frameRate, videoData.codecData);

        attachData(firebolt::rialto::MediaSourceType::VIDEO, videoData.buffer);
    }
    for (GstBuffer *buffer : m_subtitleData)
    {
        attachData(firebolt::rialto::MediaSourceType::SUBTITLE, buffer);
    }

    if (!m_audioData.empty())
    {
        m_player.notifyNeedMediaData(firebolt::rialto::MediaSourceType::AUDIO);
    }
    else if (!m_videoData.empty())
    {
        m_player.notifyNeedMediaData(firebolt::rialto::MediaSourceType::VIDEO);
    }
    else if (!m_subtitleData.empty())
    {
        m_player.notifyNeedMediaData(firebolt::rialto::MediaSourceType::SUBTITLE);
    }
}

void AttachSamples::attachData(const firebolt::rialto::MediaSourceType mediaType, GstBuffer *buffer) const
{
    auto elem = m_context.streamInfo.find(mediaType);
    if (elem != m_context.streamInfo.end())
    {
        elem->second.buffers.push_back(buffer);
        m_player.attachData(mediaType);
    }
    else
    {
        RIALTO_SERVER_LOG_WARN("Could not find stream info for %s", common::convertMediaSourceType(mediaType));
        m_gstWrapper->gstBufferUnref(buffer);
    }
}

} // namespace firebolt::rialto::server::tasks::generic
