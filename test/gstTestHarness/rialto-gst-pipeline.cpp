
#include "rialto-gst-pipeline.h"
#include <inttypes.h>
#include <gst/app/gstappsrc.h>

GstMediaPipeline::GstMediaPipeline()
{
    printf("constructing GstMediaPipeline (Rialto-backed)\n");
    if (!gst_is_initialized())
    {
        int argc = 0; char **argv = nullptr; gst_init(&argc, &argv);
    }
    m_messageQueueFactory = IMessageQueueFactory::createFactory();
    m_clientBackend = std::make_shared<firebolt::rialto::client::MediaPlayerClientBackend>();
    const uint32_t maxW = DEFAULT_MAX_VIDEO_WIDTH;
    const uint32_t maxH = DEFAULT_MAX_VIDEO_HEIGHT;
    m_mseClient = std::make_shared<GStreamerMSEMediaPlayerClient>(m_messageQueueFactory, m_clientBackend, maxW, maxH);
    if (!m_mseClient->createBackend())
    {
        fprintf(stderr, "Failed to create Rialto media pipeline backend\n");
    }
}

GstMediaPipeline::~GstMediaPipeline()
{
    printf("destructing GstMediaPipeline (Rialto-backed)\n");
    if (m_clientBackend)
    {
        m_clientBackend->stop();
    }
    m_mseClient.reset();
    m_clientBackend.reset();
    m_messageQueueFactory.reset();
}

bool GstMediaPipeline::play()
{
    printf("play (Rialto backend)\n");
    if (!m_clientBackend)
    {
        fprintf(stderr, "play(): backend not created\n");
        return false;
    }
    return m_clientBackend->play();
}

bool GstMediaPipeline::removeSource(int32_t id)
{
    if (!m_clientBackend) return false;
    return m_clientBackend->removeSource(id);
}

bool GstMediaPipeline::allSourcesAttached()
{
    if (!m_clientBackend) return false;
    return m_clientBackend->allSourcesAttached();
}

bool GstMediaPipeline::load(MediaType type, const std::string &mimeType, const std::string &url)
{ 
    if (!m_clientBackend) return false; 
    return m_clientBackend->load(type, mimeType, url); 
}

AddSegmentStatus GstMediaPipeline::addSegment(uint32_t needDataRequestId, const std::unique_ptr<IMediaPipeline::MediaSegment> &mediaSegment)
{
    if (!m_clientBackend)
    {
        return AddSegmentStatus::ERROR;
    }
    return m_clientBackend->addSegment(needDataRequestId, mediaSegment);
}

bool GstMediaPipeline::attachSource(const std::unique_ptr<IMediaPipeline::MediaSource> &source)
{
    if (!m_clientBackend)
    {
        fprintf(stderr, "attachSource(): backend not ready\n");
        return false;
    }
    if (!source)
    {
        fprintf(stderr, "attachSource(): null source\n");
        return false;
    }
    // Create a copy we can hand to backend (it may mutate id inside copy)
    std::unique_ptr<IMediaPipeline::MediaSource> temp = source->copy();
    if (!temp)
    {
        fprintf(stderr, "attachSource(): copy failed\n");
        return false;
    }
    bool ok = m_clientBackend->attachSource(temp);
    if (!ok)
    {
        fprintf(stderr, "attachSource(): backend attach failed\n");
        return false;
    }
    // Propagate assigned id back to original
    const int32_t assignedId = temp->getId();
    const_cast<IMediaPipeline::MediaSource*>(source.get())->setId(assignedId);
    printf("attachSource() -> sourceId=%d (Rialto backend)\n", assignedId);
    return true;
}
