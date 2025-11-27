#include "rialto-gst-pipeline.h"
#include <iostream>

GstMediaPipeline::GstMediaPipeline()
{
    std::cout << "Constructing GstMediaPipeline (Rialto-managed, public API)\n";

    // In Option 2, the pipeline should be obtained via the public API.
    // This is a placeholder. In real usage, obtain the pipeline from a factory or inject it.
    m_pipeline = nullptr;
}

GstMediaPipeline::~GstMediaPipeline()
{
    std::cout << "Destructing GstMediaPipeline\n";
    if (m_pipeline)
    {
        m_pipeline->stop();
    }
    m_pipeline.reset();
}

// Rialto-managed methods delegate to IMediaPipeline if available
bool GstMediaPipeline::play() { return m_pipeline ? m_pipeline->play() : false; }
bool GstMediaPipeline::removeSource(int32_t id) { return m_pipeline ? m_pipeline->removeSource(id) : false; }
bool GstMediaPipeline::allSourcesAttached() { return m_pipeline ? m_pipeline->allSourcesAttached() : false; }
bool GstMediaPipeline::load(MediaType type, const std::string &mimeType, const std::string &url)
{
    return m_pipeline ? m_pipeline->load(type, mimeType, url) : false;
}

AddSegmentStatus GstMediaPipeline::addSegment(uint32_t needDataRequestId,
        const std::unique_ptr<IMediaPipeline::MediaSegment> &mediaSegment)
{
    return m_pipeline ? m_pipeline->addSegment(needDataRequestId, mediaSegment) : AddSegmentStatus::ERROR;
}

bool GstMediaPipeline::attachSource(const std::unique_ptr<IMediaPipeline::MediaSource> &source)
{
    if (!m_pipeline || !source) return false;

    std::unique_ptr<IMediaPipeline::MediaSource> temp = source->copy();
    if (!temp) return false;

    bool ok = m_pipeline->attachSource(temp);
    if (!ok) return false;

    const int32_t assignedId = temp->getId();
    const_cast<IMediaPipeline::MediaSource*>(source.get())->setId(assignedId);
    return true;
}

// All other IMediaPipeline methods: return false or default values
bool GstMediaPipeline::pause() { return false; }
bool GstMediaPipeline::stop() { return false; }
bool GstMediaPipeline::setPlaybackRate(double) { return false; }
bool GstMediaPipeline::setPosition(int64_t) { return false; }
bool GstMediaPipeline::getPosition(int64_t &position) { position = 0; return false; }
bool GstMediaPipeline::getStats(int32_t, uint64_t &renderedFrames, uint64_t &droppedFrames) { renderedFrames = droppedFrames = 0; return false; }
bool GstMediaPipeline::setImmediateOutput(int32_t, bool) { return false; }
bool GstMediaPipeline::getImmediateOutput(int32_t, bool &) { return false; }
bool GstMediaPipeline::setVideoWindow(uint32_t, uint32_t, uint32_t, uint32_t) { return false; }
bool GstMediaPipeline::haveData(MediaSourceStatus, uint32_t) { return false; }
bool GstMediaPipeline::renderFrame() { return false; }
bool GstMediaPipeline::setVolume(double, uint32_t, EaseType) { return false; }
bool GstMediaPipeline::getTextTrackIdentifier(std::string &) { return false; }
bool GstMediaPipeline::setLowLatency(bool) { return false; }
bool GstMediaPipeline::setSync(bool) { return false; }
bool GstMediaPipeline::getSync(bool &) { return false; }
bool GstMediaPipeline::setSyncOff(bool) { return false; }
bool GstMediaPipeline::setStreamSyncMode(int32_t, int32_t) { return false; }
bool GstMediaPipeline::getStreamSyncMode(int32_t &) { return false; }
bool GstMediaPipeline::flush(int32_t, bool, bool &) { return false; }
bool GstMediaPipeline::setSourcePosition(int32_t, int64_t, bool, double, uint64_t) { return false; }
bool GstMediaPipeline::setSubtitleOffset(int32_t, int64_t) { return false; }
bool GstMediaPipeline::processAudioGap(int64_t, uint32_t, int64_t, bool) { return false; }
bool GstMediaPipeline::setBufferingLimit(uint32_t) { return false; }
bool GstMediaPipeline::getBufferingLimit(uint32_t &) { return false; }
bool GstMediaPipeline::setUseBuffering(bool) { return false; }
bool GstMediaPipeline::getUseBuffering(bool &) { return false; }
bool GstMediaPipeline::switchSource(const std::unique_ptr<MediaSource> &) { return false; }
bool GstMediaPipeline::getVolume(double &) { return false; }
bool GstMediaPipeline::setMute(int32_t, bool) { return false; }
bool GstMediaPipeline::getMute(int32_t, bool &) { return false; }
bool GstMediaPipeline::setTextTrackIdentifier(const std::string &) { return false; }