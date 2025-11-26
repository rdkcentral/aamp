#include "rialto-gst-pipeline.h"
#include <inttypes.h>
#include <gst/app/gstappsrc.h>
#include <iostream>

GstMediaPipeline::GstMediaPipeline()
{
    std::cout << "Constructing GstMediaPipeline (Rialto public API)\n";

    if (!gst_is_initialized())
    {
        int argc = 0; char **argv = nullptr;
        gst_init(&argc, &argv);
    }

    m_messageQueueFactory = IMessageQueueFactory::createFactory();
    m_rialtoClient = IClient::createClient(m_messageQueueFactory);
}

GstMediaPipeline::~GstMediaPipeline()
{
    std::cout << "Destructing GstMediaPipeline\n";
    if (m_rialtoClient)
    {
        m_rialtoClient->stop();
    }
    m_rialtoClient.reset();
    m_messageQueueFactory.reset();
}

bool GstMediaPipeline::play()
{
    if (!m_rialtoClient)
    {
        std::cerr << "play(): Rialto client not initialized\n";
        return false;
    }
    return m_rialtoClient->play();
}

bool GstMediaPipeline::removeSource(int32_t id)
{
    if (!m_rialtoClient) return false;
    return m_rialtoClient->removeSource(id);
}

bool GstMediaPipeline::allSourcesAttached()
{
    if (!m_rialtoClient) return false;
    return m_rialtoClient->allSourcesAttached();
}

bool GstMediaPipeline::load(MediaType type, const std::string &mimeType, const std::string &url)
{
    if (!m_rialtoClient) return false;
    return m_rialtoClient->load(type, mimeType, url);
}

AddSegmentStatus GstMediaPipeline::addSegment(uint32_t needDataRequestId, const std::unique_ptr<IMediaPipeline::MediaSegment> &mediaSegment)
{
    if (!m_rialtoClient) return AddSegmentStatus::ERROR;
    return m_rialtoClient->addSegment(needDataRequestId, mediaSegment);
}

bool GstMediaPipeline::attachSource(const std::unique_ptr<IMediaPipeline::MediaSource> &source)
{
    if (!m_rialtoClient || !source) return false;

    std::unique_ptr<IMediaPipeline::MediaSource> temp = source->copy();
    if (!temp) return false;

    bool ok = m_rialtoClient->attachSource(temp);
    if (!ok) return false;

    const int32_t assignedId = temp->getId();
    const_cast<IMediaPipeline::MediaSource*>(source.get())->setId(assignedId);

    return true;
}

// Remaining IMediaPipeline methods just return default false or 0
bool GstMediaPipeline::pause() { return m_rialtoClient ? m_rialtoClient->pause() : false; }
bool GstMediaPipeline::stop() { return m_rialtoClient ? m_rialtoClient->stop() : false; }
bool GstMediaPipeline::setPlaybackRate(double rate) { return false; }
bool GstMediaPipeline::setPosition(int64_t position) { return false; }
bool GstMediaPipeline::getPosition(int64_t &position) { position = 0; return false; }
bool GstMediaPipeline::getStats(int32_t, uint64_t &renderedFrames, uint64_t &droppedFrames)
{ renderedFrames = droppedFrames = 0; return false; }
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
