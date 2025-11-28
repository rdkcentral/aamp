#include <stdio.h>
#include <cassert>
#include <thread>
#include <chrono>
#include <cstring>
#include <memory>
#include "mp4demux.hpp"
#include "rialto-gst-pipeline.h"

static const int64_t NS_SECOND = 1000000000LL;

static Mp4Demux trackAudio;
static Mp4Demux trackVideo;

static std::shared_ptr<GstMediaPipeline> gstMediaPipeline; 
static int gUserPathLen;
static const char *gUserPathPtr;
static int32_t sourceIdAudio;
static int32_t sourceIdVideo;

using namespace firebolt::rialto; 

void LoadAndDemuxSegment(Mp4Demux &mp4Demux, const char *path)
{
    char fullpath[512];
    snprintf(fullpath, sizeof(fullpath), "/tmp/data/bipbop-gen/%s", path);
    printf("loading rialtotest %s\n", fullpath);

    FILE *f = fopen(fullpath, "rb");
    assert(f);
    if (f)
    {
        fseek(f, 0, SEEK_END);
        long len = ftell(f);
        if (len > 0)
        {
            unsigned char *ptr = (unsigned char *)malloc(len);
            assert(ptr);
            if (ptr)
            {
                fseek(f, 0, SEEK_SET);
                size_t n = fread(ptr, 1, len, f);
                assert(n == len);
                if (n == len)
                {
                    
                    mp4Demux.Parse(ptr, (uint32_t)len);
                }
                free(ptr);
            }
        }
        fclose(f);
    }
}

void ConfigureAudio()
{
    LoadAndDemuxSegment(trackAudio, "audio/init-stream0.m4s");
    std::cout << "loading rialtotest /tmp/data/bipbop-gen/audio/init-stream0.m4s" << std::endl;

    // The 'tracks' array is likely missing. Assuming direct member access:
    std::unique_ptr<MediaSource> source =
        std::make_unique<MediaSourceAudio>(
            MediaSourceType::Audio,
            trackAudio.codec,
            trackAudio.mimeType,
            trackAudio.timeScale,
            trackAudio.initializationData);

    
    assert(gstMediaPipeline->attachSource(source));
    
    sourceIdAudio = source->getId(); 
}

void ConfigureVideo()
{
    LoadAndDemuxSegment(trackVideo, "video/init-stream0.m4s");
    std::cout << "loading rialtotest /tmp/data/bipbop-gen/video/init-stream0.m4s" << std::endl;


    std::unique_ptr<MediaSource> source =
        std::make_unique<MediaSourceVideo>(
            MediaSourceType::Video,
            trackVideo.codec,  
            trackVideo.mimeType,
            trackVideo.timeScale,
            trackVideo.initializationData,
            trackVideo.width,     
            trackVideo.height);   

    // Using assert to attachSource returns a bool
    assert(gstMediaPipeline->attachSource(source));
    // Must move the unique_ptr out to get the ID, then detach it from 'source'
    sourceIdVideo = source->getId();
}

void ConfigureComplete()
{
    gstMediaPipeline->allSourcesAttached();
}

void InjectAudio()
{
    LoadAndDemuxSegment(trackAudio, "audio/chunk-stream0-00001.m4s");
    std::cout << "loading rialtotest /tmp/data/bipbop-gen/audio/chunk-stream0-00001.m4s" << std::endl;
    
    // Using getSegmentCount() instead of getNbSegments() or similar
    size_t segmentCount = trackAudio.getSegmentCount();
    printf("adding %zu audio frames\n", segmentCount); 

    for (size_t i = 0; i < segmentCount; ++i)
    {
        double pts = trackAudio.getPts(i);
        double dur = trackAudio.getDuration(i);

        std::unique_ptr<IMediaPipeline::MediaSegment> audioSegment =
            std::make_unique<IMediaPipeline::MediaSegmentAudio>(
                sourceIdAudio,
                (int64_t)(pts * NS_SECOND),
                (int64_t)(dur * NS_SECOND));

        size_t len = trackAudio.getLen(i);
        uint8_t *data = new uint8_t[len];
        std::memcpy(data, trackAudio.getPtr(i), len);
        audioSegment->setData((uint32_t)len, data);

        AddSegmentStatus status = gstMediaPipeline->addSegment(0, audioSegment);
        assert(status == AddSegmentStatus::OK);
    }
}

void InjectVideo()
{
    LoadAndDemuxSegment(trackVideo, "video/chunk-stream0-00001.m4s");
    std::cout << "loading rialtotest /tmp/data/bipbop-gen/video/chunk-stream0-00001.m4s" << std::endl;
    
    // Using getSegmentCount() instead of getNbSegments() or similar
    size_t segmentCount = trackVideo.getSegmentCount();
    printf("adding %zu video frames\n", segmentCount);

    for (size_t i = 0; i < segmentCount; ++i)
    {
        double pts = trackVideo.getPts(i);
        double dur = trackVideo.getDuration(i);

        std::unique_ptr<IMediaPipeline::MediaSegment> videoSegment =
            std::make_unique<IMediaPipeline::MediaSegmentVideo>(
                sourceIdVideo,
                (int64_t)(pts * NS_SECOND),
                (int64_t)(dur * NS_SECOND),
                trackVideo.width,
                trackVideo.height); 

        size_t len = trackVideo.getLen(i);
        uint8_t *data = new uint8_t[len];
        std::memcpy(data, trackVideo.getPtr(i), len);
        videoSegment->setData((uint32_t)len, data);

        AddSegmentStatus status = gstMediaPipeline->addSegment(0, videoSegment);
        assert(status == AddSegmentStatus::OK);
    }
}

int my_main(int argc, char **argv)
{
    const char *executablePath = argv[0];
    const char *prefix = "/Users/";
    size_t prefixLen = strlen(prefix);
    gUserPathPtr = strstr(executablePath, prefix);
    const char *delim = strchr(&gUserPathPtr[prefixLen], '/');
    gUserPathLen = (int)(delim - gUserPathPtr);

    gstMediaPipeline = std::make_shared<GstMediaPipeline>();
    if (!gstMediaPipeline->init())
    {
        fprintf(stderr, "FATAL: Failed to initialize Rialto pipeline. Check logs/server status.\n");
        return -1;
    }
    
    if (!gstMediaPipeline->setVideoWindow(0, 0, 1920, 1080))
    {
        fprintf(stderr, "Warning: Failed to set video window. Video may not appear.\n");
    }

    gstMediaPipeline->play();

    ConfigureAudio();
    ConfigureVideo();
    ConfigureComplete();

    InjectAudio();
    InjectVideo();

    gstMediaPipeline->stop();

    return 0;
}

int main(int argc, char **argv)
{
#if defined(__APPLE__) && defined(__GST_MACOS_H__)
    return gst_macos_main((GstMainFunc)my_main, argc, argv, nullptr);
#else
    return my_main(argc, argv);
#endif
}