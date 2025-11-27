#include "rialto-gst-pipeline.h"
#include <iostream>
#include <vector>

class LocalGstPipeline : public firebolt::rialto::IMediaPipeline
{
public:
    LocalGstPipeline() : m_nextId(1) {}
    ~LocalGstPipeline() override { stop(); }

    std::weak_ptr<firebolt::rialto::IMediaPipelineClient> getClient() override
    {
        return std::weak_ptr<firebolt::rialto::IMediaPipelineClient>();
    }

    bool play() override { return true; }
    bool pause() override { return true; }
    bool stop() override { return true; }
    bool setPlaybackRate(double) override { return true; }
    bool setPosition(int64_t) override { return true; }
    bool getPosition(int64_t &position) override { position = 0; return true; }
    bool getStats(int32_t, uint64_t &renderedFrames, uint64_t &droppedFrames) override { renderedFrames = droppedFrames = 0; return true; }
    bool setImmediateOutput(int32_t, bool) override { return true; }
    bool getImmediateOutput(int32_t, bool &) override { return true; }
    bool setVideoWindow(uint32_t, uint32_t, uint32_t, uint32_t) override { return true; }
    bool haveData(firebolt::rialto::MediaSourceStatus, uint32_t) override { return true; }
    bool renderFrame() override { return true; }
    bool setVolume(double, uint32_t, firebolt::rialto::EaseType) override { return true; }
    bool getTextTrackIdentifier(std::string &) override { return true; }
    bool setLowLatency(bool) override { return true; }
    bool setSync(bool) override { return true; }
    bool getSync(bool &) override { return true; }
    bool setSyncOff(bool) override { return true; }
    bool setStreamSyncMode(int32_t, int32_t) override { return true; }
    bool getStreamSyncMode(int32_t &) override { return true; }
    bool flush(int32_t, bool, bool &async) override { async = false; return true; }
    bool setSourcePosition(int32_t, int64_t, bool, double, uint64_t) override { return true; }
    bool setSubtitleOffset(int32_t, int64_t) override { return true; }
    bool processAudioGap(int64_t, uint32_t, int64_t, bool) override { return true; }
    bool setBufferingLimit(uint32_t) override { return true; }
    bool getBufferingLimit(uint32_t &) override { return true; }
    bool setUseBuffering(bool) override { return true; }
    bool getUseBuffering(bool &) override { return true; }
    bool switchSource(const std::unique_ptr<MediaSource> &) override { return true; }
    bool getVolume(double &) override { return true; }
    bool setMute(int32_t, bool) override { return true; }
    bool getMute(int32_t, bool &) override { return true; }
    bool setTextTrackIdentifier(const std::string &) override { return true; }

    bool attachSource(const std::unique_ptr<MediaSource> &source) override
    {
        if (!source) return false;
        auto copy = source->copy();
        if (!copy) return false;
        int32_t id = m_nextId++;
        copy->setId(id);
        const_cast<MediaSource*>(source.get())->setId(id);
        m_sources.push_back(std::move(copy));
        return true;
    }

    bool removeSource(int32_t id) override
    {
        for (auto it = m_sources.begin(); it != m_sources.end(); ++it)
        {
            if ((*it)->getId() == id)
            {
                m_sources.erase(it);
                return true;
            }
        }
        return false;
    }

    bool allSourcesAttached() override { return true; }

    bool load(MediaType, const std::string &, const std::string &) override { return true; }

    AddSegmentStatus addSegment(uint32_t, const std::unique_ptr<MediaSegment> &mediaSegment) override
    {
        if (!mediaSegment) return AddSegmentStatus::ERROR;
        return AddSegmentStatus::OK;
    }

private:
    int32_t m_nextId;
    std::vector<std::unique_ptr<MediaSource>> m_sources;
};

GstMediaPipeline::GstMediaPipeline()
{
    std::cout << "Constructing GstMediaPipeline (Rialto-managed, public API)\n";
    // Fallback to a local minimal pipeline if Rialto pipeline isn't injected
    m_pipeline = std::make_shared<LocalGstPipeline>();
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