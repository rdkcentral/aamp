/*
 * If not stated otherwise in this file or this component's license file the
 * following copyright and licenses apply:
 *
 * Copyright 2026 RDK Management
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
*/

#include <stdio.h>
#include <thread>
#include <chrono>
#include <cstring>
#include <memory>
#include <vector>
#include <mutex>
#include <queue>
#include <condition_variable>
#include "mp4demux.hpp"
#include "rialto-pipeline3.h"

static const int64_t NS_SECOND = 1000000000LL;

static Mp4Demux trackAudio;
static Mp4Demux trackVideo;
static std::shared_ptr<GstMediaPipeline> gstMediaPipeline; 
static int gUserPathLen;
static const char *gUserPathPtr;
static int32_t sourceIdAudio;
static int32_t sourceIdVideo;

using namespace firebolt::rialto; 

/**
 * @brief Waits for a need-data request for a specific source.
 *
 * This helper blocks the caller until a matching NeedDataRequestEvent
 * for the given source ID is present in the global need-data queue, or
 * until the specified timeout elapses.
 *
 * On success, the corresponding request is removed from the queue and
 * its requestId is returned. If no matching request is received within
 * the timeout, an error message is printed to stderr and UINT32_MAX is
 * returned.
 *
 * @param sourceId   The media source identifier to match against
 *                   queued NeedDataRequestEvent objects.
 * @param timeoutMs  Maximum time to wait, in milliseconds. Defaults to
 *                   5000 ms.
 *
 * @return The requestId of the first matching need-data request found,
 *         or UINT32_MAX if the wait timed out or no matching event was
 *         available.
 */
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

void LoadAndDemuxSegment(Mp4DemuxAdapter &mp4Demux, const char *path)
{
    char fullpath[512];
    snprintf(fullpath, sizeof(fullpath), "/tmp/data/bipbop-gen/%s", path);
    printf("loading rialtotest %s\n", fullpath);

    FILE *f = fopen(fullpath, "rb");
	if( !f )
	{
		printf( "fopen failure\n" );
		exit(1);
	}
    if (f)
    {
        fseek(f, 0, SEEK_END);
        long len = ftell(f);
        if (len > 0)
        {
            unsigned char *ptr = (unsigned char *)malloc(len);
			if( !ptr )
			{
				printf( "malloc failure\n" );
				exit(1);
			}
			if (ptr)
            {
                fseek(f, 0, SEEK_SET);
                size_t n = fread(ptr, 1, len, f);
				if( n!=len )
				{
					printf( "fread failure\n" );
					exit(1);
				}
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
			printf( "unknown trackAudio.codec_type\n" );
			exit(1);
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
			printf( "unknown trackVideo.codec_type\n" );
			exit(1);
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
        if( status != AddSegmentStatus::OK )
		{
			printf( "gstMediaPipeline->addSegment failure\n" );
			exit(1);
		}
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
		if( status != AddSegmentStatus::OK )
		{
			printf( "gstMediaPipeline->addSegment failure\n" );
			exit(1);
		}
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
