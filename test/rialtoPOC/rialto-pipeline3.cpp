#include "rialto-gst-pipeline.h"
#include <iostream>
#include <vector>
#include <cassert>
#include <cstring>
#include <mutex>
#include <condition_variable>
#include <queue>

std::mutex g_needDataMutex;
std::condition_variable g_needDataCv;
std::queue<NeedDataRequestEvent> g_needDataQueue;

using namespace firebolt::rialto;

GstMediaPipeline::GstMediaPipeline()
{
    std::cout << "Constructing GstMediaPipeline (Rialto-managed, public API)\n";
}

GstMediaPipeline::~GstMediaPipeline()
{
    if (m_pipeline)
    {
        m_pipeline->stop();
    }
}

bool GstMediaPipeline::init()
{
    // FIX: Calling createFactory() on IMediaPipelineFactory class, as it's not a member of IMediaPipeline.
    std::shared_ptr<IMediaPipelineFactory> factory = IMediaPipelineFactory::createFactory(); 
    if (!factory) return false;

    constexpr std::uint32_t kWidth{3840};  
    constexpr std::uint32_t kHeight{2160}; 

    VideoRequirements kRequirements{kWidth, kHeight};
    
    m_pipeline = factory->createMediaPipeline(weak_from_this(), kRequirements);
    
    return m_pipeline != nullptr;
}

bool GstMediaPipeline::attachSource(const std::unique_ptr<MediaSource> &source)
{
    return m_pipeline ? m_pipeline->attachSource(source) : false;
}

bool GstMediaPipeline::attachSource(std::unique_ptr<MediaSource> &&source, int32_t &sourceId)
{
    if (!source)
    {
        sourceId = 0;
        return false;
    }
    
    bool ok = attachSource(std::move(source));  

    if (ok)
    {
        sourceId = source->getId();
    }
    else
    {
        sourceId = 0;
    }
    
    return ok;
}

bool GstMediaPipeline::play() { return m_pipeline ? m_pipeline->play() : false; }
bool GstMediaPipeline::stop() { return m_pipeline ? m_pipeline->stop() : false; }
bool GstMediaPipeline::removeSource(int32_t id) { return m_pipeline ? m_pipeline->removeSource(id) : false; }
bool GstMediaPipeline::allSourcesAttached() { return m_pipeline ? m_pipeline->allSourcesAttached() : false; }
bool GstMediaPipeline::load(MediaType type, const std::string &mimeType, const std::string &url) { return m_pipeline ? m_pipeline->load(type, mimeType, url) : false; }
AddSegmentStatus GstMediaPipeline::addSegment(uint32_t needDataRequestId, const std::unique_ptr<MediaSegment> &mediaSegment) { return m_pipeline ? m_pipeline->addSegment(needDataRequestId, mediaSegment) : AddSegmentStatus::ERROR; }
bool GstMediaPipeline::pause() { return m_pipeline ? m_pipeline->pause() : false; }
bool GstMediaPipeline::setPlaybackRate(double rate) { return m_pipeline ? m_pipeline->setPlaybackRate(rate) : false; }
bool GstMediaPipeline::setPosition(int64_t position) { return m_pipeline ? m_pipeline->setPosition(position) : false; }
bool GstMediaPipeline::getPosition(int64_t &position) { return m_pipeline ? m_pipeline->getPosition(position) : false; }
bool GstMediaPipeline::getStats(int32_t sourceId, uint64_t &renderedFrames, uint64_t &droppedFrames) { return m_pipeline ? m_pipeline->getStats(sourceId, renderedFrames, droppedFrames) : false; }
bool GstMediaPipeline::setImmediateOutput(int32_t sourceId, bool immediateOutput) { return m_pipeline ? m_pipeline->setImmediateOutput(sourceId, immediateOutput) : false; }
bool GstMediaPipeline::getImmediateOutput(int32_t sourceId, bool &immediateOutput) { return m_pipeline ? m_pipeline->getImmediateOutput(sourceId, immediateOutput) : false; }
bool GstMediaPipeline::setVideoWindow(uint32_t x, uint32_t y, uint32_t width, uint32_t height) { return m_pipeline ? m_pipeline->setVideoWindow(x, y, width, height) : false; }
bool GstMediaPipeline::haveData(MediaSourceStatus status, uint32_t needDataRequestId) { return m_pipeline ? m_pipeline->haveData(status, needDataRequestId) : false; }
bool GstMediaPipeline::renderFrame() { return m_pipeline ? m_pipeline->renderFrame() : false; }
bool GstMediaPipeline::setVolume(double targetVolume, uint32_t volumeDuration, EaseType easeType) { return m_pipeline ? m_pipeline->setVolume(targetVolume, volumeDuration, easeType) : false; }
bool GstMediaPipeline::getTextTrackIdentifier(std::string &textTrackIdentifier) { return m_pipeline ? m_pipeline->getTextTrackIdentifier(textTrackIdentifier) : false; }
bool GstMediaPipeline::setLowLatency(bool lowLatency) { return m_pipeline ? m_pipeline->setLowLatency(lowLatency) : false; }
bool GstMediaPipeline::setSync(bool sync) { return m_pipeline ? m_pipeline->setSync(sync) : false; }
bool GstMediaPipeline::getSync(bool &sync) { return m_pipeline ? m_pipeline->getSync(sync) : false; }
bool GstMediaPipeline::setSyncOff(bool syncOff) { return m_pipeline ? m_pipeline->setSyncOff(syncOff) : false; }
bool GstMediaPipeline::setStreamSyncMode(int32_t sourceId, int32_t streamSyncMode) { return m_pipeline ? m_pipeline->setStreamSyncMode(sourceId, streamSyncMode) : false; }
bool GstMediaPipeline::getStreamSyncMode(int32_t &streamSyncMode) { return m_pipeline ? m_pipeline->getStreamSyncMode(streamSyncMode) : false; }
bool GstMediaPipeline::flush(int32_t sourceId, bool resetTime, bool &async) { return m_pipeline ? m_pipeline->flush(sourceId, resetTime, async) : false; }
bool GstMediaPipeline::setSourcePosition(int32_t sourceId, int64_t position, bool resetTime, double appliedRate, uint64_t stopPosition) { return m_pipeline ? m_pipeline->setSourcePosition(sourceId, position, resetTime, appliedRate, stopPosition) : false; }
bool GstMediaPipeline::setSubtitleOffset(int32_t sourceId, int64_t position) { return m_pipeline ? m_pipeline->setSubtitleOffset(sourceId, position) : false; }
bool GstMediaPipeline::processAudioGap(int64_t position, uint32_t duration, int64_t discontinuityGap, bool audioAac) { return m_pipeline ? m_pipeline->processAudioGap(position, duration, discontinuityGap, audioAac) : false; }
bool GstMediaPipeline::setBufferingLimit(uint32_t limitBufferingMs) { return m_pipeline ? m_pipeline->setBufferingLimit(limitBufferingMs) : false; }
bool GstMediaPipeline::getBufferingLimit(uint32_t &limitBufferingMs) { return m_pipeline ? m_pipeline->getBufferingLimit(limitBufferingMs) : false; }
bool GstMediaPipeline::setUseBuffering(bool useBuffering) { return m_pipeline ? m_pipeline->setUseBuffering(useBuffering) : false; }
bool GstMediaPipeline::getUseBuffering(bool &useBuffering) { return m_pipeline ? m_pipeline->getUseBuffering(useBuffering) : false; }
bool GstMediaPipeline::switchSource(const std::unique_ptr<MediaSource> &source) { return m_pipeline ? m_pipeline->switchSource(source->copy()) : false; }
bool GstMediaPipeline::getVolume(double &currentVolume) { return m_pipeline ? m_pipeline->getVolume(currentVolume) : false; }
bool GstMediaPipeline::setMute(int32_t sourceId, bool mute) { return m_pipeline ? m_pipeline->setMute(sourceId, mute) : false; }
bool GstMediaPipeline::getMute(int32_t sourceId, bool &mute) { return m_pipeline ? m_pipeline->getMute(sourceId, mute) : false; }
bool GstMediaPipeline::setTextTrackIdentifier(const std::string &textTrackIdentifier) { return m_pipeline ? m_pipeline->setTextTrackIdentifier(textTrackIdentifier) : false; }

// --- IMediaPipelineClient stubs ---

void GstMediaPipeline::notifyPlaybackState(PlaybackState state) { }
void GstMediaPipeline::notifyPlaybackError(int32_t sourceId, PlaybackError error) { }
void GstMediaPipeline::notifyPosition(int64_t position) {}
void GstMediaPipeline::notifyNetworkState(NetworkState state) {}
void GstMediaPipeline::notifyQos(int32_t sourceId, const QosInfo &qosInfo) {}
void GstMediaPipeline::notifyBufferUnderflow(int32_t sourceId) {}
void GstMediaPipeline::notifySourceFlushed(int32_t sourceId) {}

void GstMediaPipeline::notifyNeedMediaData(int32_t sourceId, size_t frameCount, 
                                           uint32_t needDataRequestId, 
                                           const std::shared_ptr<MediaPlayerShmInfo> &mediaPlayerShmInfo)
{
    std::lock_guard<std::mutex> lock(g_needDataMutex);
    g_needDataQueue.push({sourceId, needDataRequestId});
    g_needDataCv.notify_one();
}

void GstMediaPipeline::notifyDuration(int64_t duration) {}
void GstMediaPipeline::notifyNativeSize(uint32_t width, uint32_t height, double aspect) {}
void GstMediaPipeline::notifyVideoData(bool hasData) {}
void GstMediaPipeline::notifyAudioData(bool hasData) {}
void GstMediaPipeline::notifyCancelNeedMediaData(int32_t sourceId) {}