#include <stdio.h>
#include <cassert>
#include <thread>
#include <chrono>
#include <cstring>
#include <memory>
#include "mp4demux.hpp"
#include "rialto-gst-pipeline.h"

// Nanosecond conversion for timestamps
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

    assert(trackAudio.tracks.size() == 1);
    assert(trackAudio.tracks[0].type == MediaSourceType::Audio);

    std::unique_ptr<MediaSource> source =
        std::make_unique<MediaSourceAudio>(
            trackAudio.tracks[0].type,
            trackAudio.tracks[0].codec,
            trackAudio.tracks[0].mimeType,
            trackAudio.tracks[0].timeScale,
            trackAudio.tracks[0].initializationData);

    assert(gstMediaPipeline->attachSource(source));
    sourceIdAudio = source->getId();
}

void ConfigureVideo()
{
    LoadAndDemuxSegment(trackVideo, "video/init-stream0.m4s");
    std::cout << "loading rialtotest /tmp/data/bipbop-gen/video/init-stream0.m4s" << std::endl;

    assert(trackVideo.tracks.size() == 1);
    assert(trackVideo.tracks[0].type == MediaSourceType::Video);

    std::unique_ptr<MediaSource> source =
        std::make_unique<MediaSourceVideo>(
            trackVideo.tracks[0].type,
            trackVideo.tracks[0].codec,
            trackVideo.tracks[0].mimeType,
            trackVideo.tracks[0].timeScale,
            trackVideo.tracks[0].initializationData,
            trackVideo.tracks[0].video.width,
            trackVideo.tracks[0].video.height);

    assert(gstMediaPipeline->attachSource(source));
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
    printf("adding %zu audio frames\n", trackAudio.getNbSegments());

    for (size_t i = 0; i < trackAudio.getNbSegments(); ++i)
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
    printf("adding %zu video frames\n", trackVideo.getNbSegments());

    for (size_t i = 0; i < trackVideo.getNbSegments(); ++i)
    {
        double pts = trackVideo.getPts(i);
        double dur = trackVideo.getDuration(i);

        std::unique_ptr<IMediaPipeline::MediaSegment> videoSegment =
            std::make_unique<IMediaPipeline::MediaSegmentVideo>(
                sourceIdVideo,
                (int64_t)(pts * NS_SECOND),
                (int64_t)(dur * NS_SECOND),
                trackVideo.tracks[0].video.width,
                trackVideo.tracks[0].video.height);

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