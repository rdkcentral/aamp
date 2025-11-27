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
static GstMediaPipeline *gstMediaPipeline;
static int gUserPathLen;
static const char *gUserPathPtr;
static int32_t sourceIdAudio;
static int32_t sourceIdVideo;

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
                    mp4Demux.Parse(ptr, len);
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

    bool hasDrm = false;
    std::string mimeType;
    StreamFormat streamFormat;
    AudioConfig audioConfig;
    audioConfig.numberOfChannels = trackAudio.audio.channel_count;
    audioConfig.sampleRate = trackAudio.audio.samplerate;

    switch (trackAudio.codec_type)
    {
    case MultiChar_Constant("esds"):
        mimeType = "audio/mpeg";
        streamFormat = StreamFormat::RAW;
        break;
    case MultiChar_Constant("dec3"):
        mimeType = "audio/x-eac3";
        streamFormat = StreamFormat::UNDEFINED;
        break;
    default:
        assert(0);
    }

    std::unique_ptr<IMediaPipeline::MediaSource> sourceAudio =
        std::make_unique<IMediaPipeline::MediaSourceAudio>(
            mimeType, hasDrm, audioConfig, SegmentAlignment::UNDEFINED, streamFormat, nullptr);

    bool okA = gstMediaPipeline->attachSource(sourceAudio);
    // assert(okA);

    sourceIdAudio = sourceAudio->getId();
}

void ConfigureVideo()
{
    LoadAndDemuxSegment(trackVideo, "video/init-stream0.m4s");

    bool hasDrm = false;
    std::string mimeType;
    StreamFormat streamFormat;
    int32_t width = trackVideo.video.width;
    int32_t height = trackVideo.video.height;
    SegmentAlignment alignment = SegmentAlignment::AU;

    switch (trackVideo.codec_type)
    {
    case MultiChar_Constant("hvcC"):
        mimeType = "video/x-h265";
        streamFormat = StreamFormat::HVC1;
        break;
    case MultiChar_Constant("avcC"):
        mimeType = "video/x-h264";
        streamFormat = StreamFormat::AVC;
        break;
    default:
        assert(0);
    }

    CodecData codecData;
    const char *codec_ptr = trackVideo.codec_data.c_str();
    codecData.data = std::vector<uint8_t>(codec_ptr, &codec_ptr[trackVideo.codec_data.size()]);

    std::unique_ptr<IMediaPipeline::MediaSource> sourceVideo =
        std::make_unique<IMediaPipeline::MediaSourceVideo>(
            mimeType, hasDrm, width, height, alignment, streamFormat, std::make_shared<CodecData>(codecData));

    bool okV = gstMediaPipeline->attachSource(sourceVideo);
    // assert(okV);

    sourceIdVideo = sourceVideo->getId();
}

void ConfigureComplete()
{
    gstMediaPipeline->allSourcesAttached();
}

void InjectAudio()
{
    LoadAndDemuxSegment(trackAudio, "audio/chunk-stream0-00001.m4s");
    int count = trackAudio.count();
    printf("adding %d audio frames\n", count);

    for (int i = 0; i < count; i++)
    {
        double pts = trackAudio.getPts(i);
        double dur = trackAudio.getDuration(i);

        std::unique_ptr<IMediaPipeline::MediaSegment> audioSegment =
            std::make_unique<IMediaPipeline::MediaSegmentAudio>(
                sourceIdAudio,
                (int64_t)(pts * NS_SECOND),
                (int64_t)(dur * NS_SECOND),
                trackAudio.audio.samplerate,
                trackAudio.audio.channel_count);

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
    int count = trackVideo.count();
    printf("adding %d video frames\n", count);

    for (int i = 0; i < count; i++)
    {
        double pts = trackVideo.getPts(i);
        double dur = trackVideo.getDuration(i);

        std::unique_ptr<IMediaPipeline::MediaSegment> videoSegment =
            std::make_unique<IMediaPipeline::MediaSegmentVideo>(
                sourceIdVideo,
                (int64_t)(pts * NS_SECOND),
                (int64_t)(dur * NS_SECOND),
                trackVideo.video.width,
                trackVideo.video.height);

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

    gstMediaPipeline = new GstMediaPipeline();
    gstMediaPipeline->play();

    ConfigureAudio();
    ConfigureVideo();
    ConfigureComplete();

    InjectAudio();
    InjectVideo();

    std::this_thread::sleep_for(std::chrono::seconds(2));
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
