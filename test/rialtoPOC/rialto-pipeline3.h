#ifndef GST_MEDIA_PIPELINE_H3
#define GST_MEDIA_PIPELINE_H3

#include "IMediaPipeline.h"
#include <memory>
#include <iostream>
#include <vector>

#include <mutex>
#include <condition_variable>
#include <queue>

struct NeedDataRequestEvent
{
    int32_t sourceId;
    uint32_t requestId;
};

extern std::mutex g_needDataMutex;
extern std::condition_variable g_needDataCv;
extern std::queue<NeedDataRequestEvent> g_needDataQueue;

using namespace firebolt::rialto;

class GstMediaPipeline : public IMediaPipeline, 
                         public IMediaPipelineClient,
                         public std::enable_shared_from_this<GstMediaPipeline>
{
private:
    std::shared_ptr<IMediaPipeline> m_pipeline;

public:
    GstMediaPipeline();
    ~GstMediaPipeline() override;

    bool init();

    std::weak_ptr<IMediaPipelineClient> getClient() override { return weak_from_this(); }

    // IMediaPipelineClient Implementation (All required pure virtuals)
    void notifyNetworkState(NetworkState state) override;
    void notifyPlaybackState(PlaybackState state) override;
    void notifyPosition(int64_t position) override;
    void notifyNeedMediaData(int32_t sourceId, size_t frameCount, 
                             uint32_t needDataRequestId, 
                             const std::shared_ptr<MediaPlayerShmInfo> &mediaPlayerShmInfo) override;
    void notifyQos(int32_t sourceId, const QosInfo &qosInfo) override;
    void notifyBufferUnderflow(int32_t sourceId) override;
    void notifyPlaybackError(int32_t sourceId, PlaybackError error) override;
    void notifySourceFlushed(int32_t sourceId) override;
    void notifyDuration(int64_t duration) override;
    void notifyNativeSize(uint32_t width, uint32_t height, double aspect = 1.0) override;
    void notifyVideoData(bool hasData) override;
    void notifyAudioData(bool hasData) override;
    void notifyCancelNeedMediaData(int32_t sourceId) override;

    // IMediaPipeline Implementation

    
    bool attachSource(const std::unique_ptr<MediaSource> &source) override; 
    bool attachSource(std::unique_ptr<MediaSource> &&source, int32_t &sourceId);

    bool removeSource(int32_t id) override;
    bool allSourcesAttached() override;
    bool load(MediaType type, const std::string &mimeType, const std::string &url) override;
    AddSegmentStatus addSegment(uint32_t needDataRequestId, const std::unique_ptr<MediaSegment> &mediaSegment) override;

    bool play() override;
    bool stop() override;
    bool setVideoWindow(uint32_t x, uint32_t y, uint32_t width, uint32_t height) override;
    bool pause() override;
    bool setPlaybackRate(double) override;
    bool setPosition(int64_t) override;
    bool getPosition(int64_t &p) override;
    bool getStats(int32_t, uint64_t &r, uint64_t &d) override;
    bool setImmediateOutput(int32_t, bool) override;
    bool getImmediateOutput(int32_t, bool &) override;
    bool haveData(MediaSourceStatus, uint32_t) override;
    bool renderFrame() override;
    bool setVolume(double, uint32_t, EaseType) override;
    bool getTextTrackIdentifier(std::string &) override;
    bool setLowLatency(bool) override;
    bool setSync(bool) override;
    bool getSync(bool &) override;
    bool setSyncOff(bool) override;
    bool setStreamSyncMode(int32_t, int32_t) override;
    bool getStreamSyncMode(int32_t &) override;
    bool flush(int32_t, bool, bool &) override;
    bool setSourcePosition(int32_t, int64_t, bool, double, uint64_t) override;
    bool setSubtitleOffset(int32_t, int64_t) override;
    bool processAudioGap(int64_t, uint32_t, int64_t, bool) override;
    bool setBufferingLimit(uint32_t) override;
    bool getBufferingLimit(uint32_t &) override;
    bool setUseBuffering(bool) override;
    bool getUseBuffering(bool &) override;
    bool switchSource(const std::unique_ptr<MediaSource> &) override;
    bool getVolume(double &) override;
    bool setMute(int32_t, bool) override;
    bool getMute(int32_t, bool &) override;
    bool setTextTrackIdentifier(const std::string &) override;
};

#endif // GST_MEDIA_PIPELINE_H3