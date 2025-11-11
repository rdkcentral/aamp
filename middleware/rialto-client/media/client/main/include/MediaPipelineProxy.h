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

#ifndef FIREBOLT_RIALTO_CLIENT_MEDIA_PIPELINE_PROXY_H_
#define FIREBOLT_RIALTO_CLIENT_MEDIA_PIPELINE_PROXY_H_

#include <stdint.h>

#include <memory>
#include <string>

#include "MediaPipeline.h"

namespace firebolt::rialto::client
{
class MediaPipelineProxy : public IMediaPipelineAndIControlClient
{
public:
    MediaPipelineProxy(const std::shared_ptr<IMediaPipelineAndIControlClient> &mediaPipeline,
                       client::IClientController &clientController);
    virtual ~MediaPipelineProxy();

    std::weak_ptr<IMediaPipelineClient> getClient() override { return m_mediaPipeline->getClient(); }

    bool load(MediaType type, const std::string &mimeType, const std::string &url) override
    {
        return m_mediaPipeline->load(type, mimeType, url);
    }

    bool attachSource(const std::unique_ptr<MediaSource> &source) override
    {
        return m_mediaPipeline->attachSource(source);
    }

    bool removeSource(int32_t id) override { return m_mediaPipeline->removeSource(id); }

    bool allSourcesAttached() override { return m_mediaPipeline->allSourcesAttached(); }

    bool play() override { return m_mediaPipeline->play(); }

    bool pause() override { return m_mediaPipeline->pause(); }

    bool stop() override { return m_mediaPipeline->stop(); }

    bool setPlaybackRate(double rate) override { return m_mediaPipeline->setPlaybackRate(rate); }

    bool setPosition(int64_t position) override { return m_mediaPipeline->setPosition(position); }

    bool getPosition(int64_t &position) override { return m_mediaPipeline->getPosition(position); }

    bool setImmediateOutput(int32_t sourceId, bool immediateOutput)
    {
        return m_mediaPipeline->setImmediateOutput(sourceId, immediateOutput);
    }
    bool getImmediateOutput(int32_t sourceId, bool &immediateOutput)
    {
        return m_mediaPipeline->getImmediateOutput(sourceId, immediateOutput);
    }

    bool getStats(int32_t sourceId, uint64_t &renderedFrames, uint64_t &droppedFrames)
    {
        return m_mediaPipeline->getStats(sourceId, renderedFrames, droppedFrames);
    }

    bool setVideoWindow(uint32_t x, uint32_t y, uint32_t width, uint32_t height) override
    {
        return m_mediaPipeline->setVideoWindow(x, y, width, height);
    }

    bool haveData(MediaSourceStatus status, uint32_t needDataRequestId) override
    {
        return m_mediaPipeline->haveData(status, needDataRequestId);
    }

    AddSegmentStatus addSegment(uint32_t needDataRequestId, const std::unique_ptr<MediaSegment> &mediaSegment) override
    {
        return m_mediaPipeline->addSegment(needDataRequestId, mediaSegment);
    }

    bool renderFrame() override { return m_mediaPipeline->renderFrame(); }

    bool setVolume(double targetVolume, uint32_t volumeDuration, EaseType easeType) override
    {
        return m_mediaPipeline->setVolume(targetVolume, volumeDuration, easeType);
    }

    bool getVolume(double &currentVolume) override { return m_mediaPipeline->getVolume(currentVolume); }

    bool setMute(int32_t sourceId, bool mute) override { return m_mediaPipeline->setMute(sourceId, mute); }

    bool getMute(int32_t sourceId, bool &mute) override { return m_mediaPipeline->getMute(sourceId, mute); }

    bool setTextTrackIdentifier(const std::string &textTrackIdentifier) override
    {
        return m_mediaPipeline->setTextTrackIdentifier(textTrackIdentifier);
    }

    bool getTextTrackIdentifier(std::string &textTrackIdentifier) override
    {
        return m_mediaPipeline->getTextTrackIdentifier(textTrackIdentifier);
    }

    bool setLowLatency(bool lowLatency) override { return m_mediaPipeline->setLowLatency(lowLatency); }

    bool setSync(bool sync) override { return m_mediaPipeline->setSync(sync); }

    bool getSync(bool &sync) override { return m_mediaPipeline->getSync(sync); }

    bool setSyncOff(bool syncOff) override { return m_mediaPipeline->setSyncOff(syncOff); }

    bool setStreamSyncMode(int32_t sourceId, int32_t streamSyncMode) override
    {
        return m_mediaPipeline->setStreamSyncMode(sourceId, streamSyncMode);
    }

    bool getStreamSyncMode(int32_t &streamSyncMode) override
    {
        return m_mediaPipeline->getStreamSyncMode(streamSyncMode);
    }

    bool flush(int32_t sourceId, bool resetTime, bool &async) override
    {
        return m_mediaPipeline->flush(sourceId, resetTime, async);
    }

    bool setSourcePosition(int32_t sourceId, int64_t position, bool resetTime, double appliedRate,
                           uint64_t stopPosition) override
    {
        return m_mediaPipeline->setSourcePosition(sourceId, position, resetTime, appliedRate, stopPosition);
    }

    bool setSubtitleOffset(int32_t sourceId, int64_t position) override
    {
        return m_mediaPipeline->setSubtitleOffset(sourceId, position);
    }

    bool processAudioGap(int64_t position, uint32_t duration, int64_t discontinuityGap, bool audioAac) override
    {
        return m_mediaPipeline->processAudioGap(position, duration, discontinuityGap, audioAac);
    }

    bool setBufferingLimit(uint32_t limitBufferingMs) override
    {
        return m_mediaPipeline->setBufferingLimit(limitBufferingMs);
    }

    bool getBufferingLimit(uint32_t &limitBufferingMs) override
    {
        return m_mediaPipeline->getBufferingLimit(limitBufferingMs);
    }

    bool setUseBuffering(bool useBuffering) override { return m_mediaPipeline->setUseBuffering(useBuffering); }

    bool getUseBuffering(bool &useBuffering) override { return m_mediaPipeline->getUseBuffering(useBuffering); }

    bool switchSource(const std::unique_ptr<MediaSource> &source) override
    {
        return m_mediaPipeline->switchSource(source);
    }

    void notifyApplicationState(ApplicationState state) override { m_mediaPipeline->notifyApplicationState(state); }

private:
    std::shared_ptr<IMediaPipelineAndIControlClient> m_mediaPipeline;
    client::IClientController &m_clientController;
};

}; // namespace firebolt::rialto::client

#endif // FIREBOLT_RIALTO_CLIENT_MEDIA_PIPELINE_PROXY_H_
