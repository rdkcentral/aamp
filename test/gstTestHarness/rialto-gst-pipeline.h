#ifndef GST_MEDIA_PIPELINE_H
#define GST_MEDIA_PIPELINE_H

#include "IMediaPipeline.h"

using namespace firebolt::rialto;

class GstMediaPipeline : public IMediaPipeline
{
private:
    std::shared_ptr<IMediaPipeline> m_pipeline;  // Rialto-managed pipeline

protected:
    std::weak_ptr<IMediaPipelineClient> client;
    std::weak_ptr<IMediaPipelineClient> getClient() { return client; }

public:
    GstMediaPipeline();
    ~GstMediaPipeline();

    // IMediaPipeline interface
    bool pause() override;
    bool stop() override;
    bool setPlaybackRate(double rate) override;
    bool setPosition(int64_t position) override;
    bool getPosition(int64_t &position) override;
    bool getStats(int32_t sourceId, uint64_t &renderedFrames, uint64_t &droppedFrames) override;
    bool setImmediateOutput(int32_t sourceId, bool immediateOutput) override;
    bool getImmediateOutput(int32_t sourceId, bool &immediateOutput) override;
    bool setVideoWindow(uint32_t x, uint32_t y, uint32_t width, uint32_t height) override;
    bool haveData(MediaSourceStatus status, uint32_t needDataRequestId) override;
    bool renderFrame() override;
    bool setVolume(double targetVolume, uint32_t volumeDuration = 0, EaseType easeType = EaseType::EASE_LINEAR) override;
    bool getTextTrackIdentifier(std::string &textTrackIdentifier) override;
    bool setLowLatency(bool lowLatency) override;
    bool setSync(bool sync) override;
    bool getSync(bool &sync) override;
    bool setSyncOff(bool syncOff) override;
    bool setStreamSyncMode(int32_t sourceId, int32_t streamSyncMode) override;
    bool getStreamSyncMode(int32_t &streamSyncMode) override;
    bool flush(int32_t sourceId, bool resetTime, bool &async) override;
    bool setSourcePosition(int32_t sourceId, int64_t position, bool resetTime = false, double appliedRate = 1.0, uint64_t stopPosition = kUndefinedPosition) override;
    bool setSubtitleOffset(int32_t sourceId, int64_t position) override;
    bool processAudioGap(int64_t position, uint32_t duration, int64_t discontinuityGap, bool audioAac) override;
    bool setBufferingLimit(uint32_t limitBufferingMs) override;
    bool getBufferingLimit(uint32_t &limitBufferingMs) override;
    bool setUseBuffering(bool useBuffering) override;
    bool getUseBuffering(bool &useBuffering) override;
    bool switchSource(const std::unique_ptr<MediaSource> &source) override;
    bool getVolume(double &currentVolume) override;
    bool setMute(int32_t sourceId, bool mute) override;
    bool getMute(int32_t sourceId, bool &mute) override;
    bool setTextTrackIdentifier(const std::string &textTrackIdentifier) override;
    bool attachSource(const std::unique_ptr<MediaSource> &source) override;
    bool removeSource(int32_t id) override;
    bool allSourcesAttached() override;
    bool load(MediaType type, const std::string &mimeType, const std::string &url) override;
    bool play() override;
    AddSegmentStatus addSegment(uint32_t needDataRequestId, const std::unique_ptr<MediaSegment> &mediaSegment) override;
};

#endif // GST_MEDIA_PIPELINE_H
