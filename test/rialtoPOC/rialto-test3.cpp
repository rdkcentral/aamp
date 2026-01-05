#include <stdio.h>
#include <cassert>
#include <thread>
#include <chrono>
#include <cstring>
#include <memory>
#include <vector>
#include <mutex>
#include <queue>
#include <condition_variable>
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

uint32_t WaitForNeedDataRequest(int32_t sourceId, int timeoutMs = 5000)
{
    std::unique_lock<std::mutex> lock(g_needDataMutex);

    // Wait until a matching event is in the queue
    bool ok = g_needDataCv.wait_for(lock, std::chrono::milliseconds(timeoutMs), [&]{
        std::queue<NeedDataRequestEvent> tmp = g_needDataQueue;
        while (!tmp.empty()) {
            if (tmp.front().sourceId == sourceId)
                return true;
            tmp.pop();
        }
        return false;
    });

    if (!ok)
    {
        fprintf(stderr, "ERROR: Timeout waiting for need-data for source %d\n", sourceId);
        return UINT32_MAX;
    }

    uint32_t requestId = UINT32_MAX;
    size_t qSize = g_needDataQueue.size();
    for (size_t i = 0; i < qSize; ++i) {
        NeedDataRequestEvent ev = g_needDataQueue.front();
        g_needDataQueue.pop();

        if (ev.sourceId == sourceId && requestId == UINT32_MAX) {
            requestId = ev.requestId; 
            
        } else {
            g_needDataQueue.push(ev); 
        }
    }

    return requestId;
}

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

    bool hasDrm = false;
    std::string mimeType;
    StreamFormat streamFormat;
	AudioConfig audioConfig;
    audioConfig.numberOfChannels = trackAudio.audio.channel_count;
	audioConfig.sampleRate = trackAudio.audio.samplerate;

    // StreamFormat streamFormat = StreamFormat::AAC; 
    // SegmentAlignment alignment = SegmentAlignment::AU;

    switch( trackAudio.codec_type )
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
			break;
	}
    
    // CodecData codecData;
    // const char *codec_ptr = trackAudio.codec_data.c_str();
    // codecData.data = std::vector<uint8_t>( codec_ptr, &codec_ptr[trackAudio.codec_data.size()] );

   std::unique_ptr<IMediaPipeline::MediaSourceAudio> sourceAudio = std::make_unique<IMediaPipeline::MediaSourceAudio>(
																					   mimeType,
																					   hasDrm,
																					   audioConfig,
																					   SegmentAlignment::UNDEFINED,
																					   streamFormat,
																					   nullptr /* codecData*/ );
                        
    gstMediaPipeline->attachSource( std::move(sourceAudio), sourceIdAudio );
}

void ConfigureVideo()
{
	LoadAndDemuxSegment( trackVideo, "video/init-stream0.m4s" );

	bool hasDrm = false;
	std::string mimeType;
	StreamFormat streamFormat;
	int32_t width = trackVideo.video.width; 
	int32_t height = trackVideo.video.height;
	SegmentAlignment alignment = SegmentAlignment::AU;

	switch( trackVideo.codec_type )
	{
		case MultiChar_Constant("hvcC"):
			mimeType = "video/h265";
			streamFormat = StreamFormat::HVC1;
			break;
		case MultiChar_Constant("avcC"):
			mimeType = "video/h264";
			streamFormat = StreamFormat::AVC;
			break;
		default:
			assert(0);
			break;
	}
	CodecData codecData;
	const char *codec_ptr = trackVideo.codec_data.c_str();
    codecData.data = std::vector<uint8_t>( codec_ptr, codec_ptr + trackVideo.codec_data.size() );
    
	std::unique_ptr<IMediaPipeline::MediaSourceVideo> sourceVideo =
	std::make_unique<IMediaPipeline::MediaSourceVideo>(
									 mimeType,
									 hasDrm,
									 width,
									 height,
									 alignment,
									 streamFormat,
									 std::make_shared<CodecData>(codecData) );
    
	gstMediaPipeline->attachSource( std::move(sourceVideo), sourceIdVideo );
}

void ConfigureComplete()
{
    gstMediaPipeline->allSourcesAttached();
}

void InjectAudio(int32_t needDataId)
{
    LoadAndDemuxSegment(trackAudio, "audio/chunk-stream0-00001.m4s");
    std::cout << "loading rialtotest /tmp/data/bipbop-gen/audio/chunk-stream0-00001.m4s" << std::endl;
    
    size_t segmentCount = trackAudio.count();
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

        AddSegmentStatus status = gstMediaPipeline->addSegment(needDataId, audioSegment);
        assert(status == AddSegmentStatus::OK);
    }

    gstMediaPipeline->haveData(MediaSourceStatus::OK, needDataId);
}

void InjectVideo(int32_t needDataId)
{
    LoadAndDemuxSegment(trackVideo, "video/chunk-stream0-00001.m4s");
    std::cout << "loading rialtotest /tmp/data/bipbop-gen/video/chunk-stream0-00001.m4s" << std::endl;
    
    // size_t segmentCount = trackVideo.getNbSegments();
    size_t segmentCount = trackVideo.count();
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
                trackVideo.video.width,
                trackVideo.video.height);

        size_t len = trackVideo.getLen(i);
        uint8_t *data = new uint8_t[len];
        std::memcpy(data, trackVideo.getPtr(i), len);
        videoSegment->setData((uint32_t)len, data);

        AddSegmentStatus status = gstMediaPipeline->addSegment(needDataId, videoSegment);
        assert(status == AddSegmentStatus::OK);
    }

    gstMediaPipeline->haveData(MediaSourceStatus::OK, needDataId);
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
    

    // MUST happen before any attachSource() to create a Rialto Gstreamer player
    gstMediaPipeline->load(MediaType::MSE, "video/mp4", "file:///tmp/data/bipbop-gen/video/chunk-stream0-00001.m4s"); // Temp

    if (!gstMediaPipeline->setVideoWindow(0, 0, 1920, 1080))
    {
        fprintf(stderr, "Warning: Failed to set video window. Video may not appear.\n");
    }

    ConfigureAudio();
    ConfigureVideo();
    ConfigureComplete();

    gstMediaPipeline->play();

    uint32_t audioReqId = WaitForNeedDataRequest(sourceIdAudio);
    uint32_t videoReqId = WaitForNeedDataRequest(sourceIdVideo);

    if (audioReqId != UINT32_MAX)
        InjectAudio(audioReqId);

    if (videoReqId != UINT32_MAX)
        InjectVideo(videoReqId);


    std::this_thread::sleep_for(std::chrono::seconds(7)); 
    
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