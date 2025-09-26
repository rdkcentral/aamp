//
//  main.cpp
//  RialtoSim
//
//  Created by Stroffolino, Philip on 9/19/25.
//
#include <unistd.h>
#include <assert.h>
#include <stdio.h>
#include "mp4demux.hpp"
#include "IMediaPipeline.h"
//#include "MediaPipelineIpc.h"

using namespace firebolt::rialto;

class MyMediaPipeline:IMediaPipeline
{
protected:
	std::weak_ptr<IMediaPipelineClient> client;
	std::weak_ptr<IMediaPipelineClient> getClient(){ return client; }
	
	bool attachSource(const std::unique_ptr<MediaSource> &source){
		printf( "attachSource\n" );
		return true;
	}
	bool attachSource(const std::unique_ptr<MediaSource> &source, int32_t &sourceId ){
		static int32_t id;
		sourceId = ++id;
		printf( "attachSource() -> sourceId=%" PRId32 "\n", sourceId );
		return true;
	};
	bool removeSource(int32_t id){
		printf( "removeSource(sourceId=%" PRId32 ")\n", id );
		return false;
	};
	bool allSourcesAttached(){
		return true;
	};
	bool load(MediaType type, const std::string &mimeType, const std::string &url){ return true; }
	bool play(){
		printf( "play\n" );
		return true;
	};
	bool pause(){ return false; };
	bool stop(){ return false; };
	bool setPlaybackRate(double rate){ return false; };
	bool setPosition(int64_t position){ return false; };
	bool getPosition(int64_t &position){ return false; };
	bool getStats(int32_t sourceId, uint64_t &renderedFrames, uint64_t &droppedFrames){ return false; };
	bool setImmediateOutput(int32_t sourceId, bool immediateOutput){ return false; };
	bool getImmediateOutput(int32_t sourceId, bool &immediateOutput){ return false; };
	bool setVideoWindow(uint32_t x, uint32_t y, uint32_t width, uint32_t height){ return false; };
	bool haveData(MediaSourceStatus status, uint32_t needDataRequestId){ return false; };
	AddSegmentStatus addSegment(uint32_t needDataRequestId, const std::unique_ptr<MediaSegment> &mediaSegment){
		//printf( "addSegment(%" PRIu32 ")\n", needDataRequestId );
		return AddSegmentStatus::OK;
	}
	bool renderFrame(){ return false; };
	bool setVolume(double targetVolume, uint32_t volumeDuration = 0, EaseType easeType = EaseType::EASE_LINEAR){ return false; };
	bool getVolume(double &currentVolume){ return false; };
	bool setMute(int32_t sourceId, bool mute){ return false; };
	bool getMute(int32_t sourceId, bool &mute){ return false; };
	bool setTextTrackIdentifier(const std::string &textTrackIdentifier){ return false; };
	bool getTextTrackIdentifier(std::string &textTrackIdentifier){ return false; };
	bool setLowLatency(bool lowLatency){ return false; };
	bool setSync(bool sync){ return false; };
	bool getSync(bool &sync){ return false; };
	bool setSyncOff(bool syncOff){ return false; };
	bool setStreamSyncMode(int32_t sourceId, int32_t streamSyncMode){ return false; };
	bool getStreamSyncMode(int32_t &streamSyncMode){ return false; };
	bool flush(int32_t sourceId, bool resetTime, bool &async){ return false; };
	bool setSourcePosition(int32_t sourceId, int64_t position, bool resetTime = false, double appliedRate = 1.0, uint64_t stopPosition = kUndefinedPosition){ return false; };
	bool processAudioGap(int64_t position, uint32_t duration, int64_t discontinuityGap, bool audioAac){ return false; };
	bool setBufferingLimit(uint32_t limitBufferingMs){ return false; };
	bool getBufferingLimit(uint32_t &limitBufferingMs){ return false; };
	bool setUseBuffering(bool useBuffering){ return false; };
	bool getUseBuffering(bool &useBuffering){ return false; };
	bool switchSource(const std::unique_ptr<MediaSource> &source){ return false; };

public:
	int userPathLen;
	const char *userPathPtr;
	
private:
	const int64_t nanoseconds_per_second = 1e+9;
	uint32_t needDataRequestId;
	
	Mp4Demux trackAudio;
	Mp4Demux trackVideo;
	int32_t sourceIdVideo;
	int32_t sourceIdAudio;
	
	void LoadAndDemuxSegment( Mp4Demux &mp4Demux, const char *path )
	{ // using generated bipbop content for initial test
		char fullpath[256];
		snprintf( fullpath, sizeof(fullpath), "%.*s/Documents/r" "d" "k" "e/aamp_test_internal/test/VideoTestStream/bipbop-gen/%s",
				 userPathLen, userPathPtr, path );
		FILE *f = fopen(fullpath,"rb");
		assert( f );
		if( f )
		{
			fseek( f,0,SEEK_END );
			size_t len = ftell(f);
			unsigned char *ptr = (unsigned char *)malloc(len);
			assert( ptr );
			if( ptr )
			{
				fseek( f, 0, SEEK_SET );
				size_t n = fread( ptr, 1, len, f );
				assert( n == len );
				if( n==len )
				{
					mp4Demux.Parse( ptr, len );
				}
			}
			fclose( f );
		}
	}
	
public:
	MyMediaPipeline() : needDataRequestId(0), sourceIdVideo(-1), sourceIdAudio(-1)
	{
		printf( "constructing MyMediaPipeline\n" );
	}
	
	~MyMediaPipeline()
	{
		printf( "destructing MyMediaPipeline\n" );
	}
	
	void ConfigureAudio( void )
	{
		LoadAndDemuxSegment( trackAudio,"audio/init-stream0.m4s" );
		
		bool hasDrm = false;
		std::string mimeType;
		StreamFormat streamFormat;
		AudioConfig audioConfig;
		audioConfig.numberOfChannels = trackAudio.audio.channel_count;
		audioConfig.sampleRate = trackAudio.audio.samplerate;
		
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
		std::unique_ptr<MediaSourceAudio> sourceAudio = std::make_unique<MediaSourceAudio>(
																						   mimeType,
																						   hasDrm,
																						   audioConfig,
																						   SegmentAlignment::UNDEFINED,
																						   streamFormat,
																						   nullptr /* codecData*/ );
		attachSource( std::move(sourceAudio), sourceIdAudio );
	}
	
	void ConfigureVideo( void )
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
				mimeType = "video/x-h265";
				streamFormat = StreamFormat::HVC1;
				break;
			case MultiChar_Constant("avcC"):
				mimeType = "video/x-h264";
				streamFormat = StreamFormat::AVC;
				break;
			default:
				assert(0);
				break;
		}
		CodecData codecData;
		const char *codec_ptr = trackVideo.codec_data.c_str();
		codecData.data = std::vector<uint8_t>( codec_ptr, &codec_ptr[trackVideo.codec_data.size()] );
		std::unique_ptr<MediaSourceVideo> sourceVideo = std::make_unique<MediaSourceVideo>(
																						   mimeType,
																						   hasDrm,
																						   width,
																						   height,
																						   alignment,
																						   streamFormat,
																						   std::make_shared<CodecData>(codecData) );
		attachSource( std::move(sourceVideo), sourceIdVideo );
	}
	
	void ConfigurenComplete()
	{
		allSourcesAttached();
	}
	
	void InjectAudio( void )
	{
		LoadAndDemuxSegment( trackAudio, "audio/chunk-stream0-00001.m4s");
		int count = trackAudio.count();
		printf( "adding %d audio frames\n", count );
		for( int i=0; i<count; i++ )
		{
			double pts = trackAudio.getPts(i);
			double dur = trackAudio.getDuration(i);
			std::unique_ptr<MediaSegmentAudio> audioSegment =
			std::make_unique<MediaSegmentAudio>(
												sourceIdAudio,
												(int64_t)(pts*nanoseconds_per_second), // timestamp
												(int64_t)(dur*nanoseconds_per_second),
												trackAudio.audio.samplerate,
												trackAudio.audio.channel_count );
			
			audioSegment->setData( (uint32_t)trackAudio.getLen(i), trackAudio.getPtr(i) );
			
			std::unique_ptr<MediaSegment> segment = std::move(audioSegment);
			AddSegmentStatus status = addSegment(++needDataRequestId,segment);
			assert( status == AddSegmentStatus::OK );
		}
	}
	
	void InjectVideo( void )
	{
		LoadAndDemuxSegment( trackVideo, "video/chunk-stream0-00001.m4s" );
		int count = trackVideo.count();
		printf( "adding %d video frames\n", count );
		for( int i=0; i<count; i++ )
		{
			double pts = trackVideo.getPts(i);
			double dur = trackVideo.getDuration(i);
			std::unique_ptr<MediaSegmentVideo> videoSegment =
			std::make_unique<MediaSegmentVideo>(
												sourceIdVideo,
												(int64_t)(pts*nanoseconds_per_second),
												(int64_t)(dur*nanoseconds_per_second),
												trackVideo.video.width,
												trackVideo.video.height );
			
			videoSegment->setData( (uint32_t)trackVideo.getLen(i), trackVideo.getPtr(i) );
			
			std::unique_ptr<MediaSegment> segment = std::move(videoSegment);
			AddSegmentStatus status = addSegment(++needDataRequestId,segment);
			assert( status == AddSegmentStatus::OK );
		}
	}
	
	void Play( void )
	{
		play();
	}
};

int main(int argc, const char * argv[])
{
	auto pipeline = new MyMediaPipeline();

	{
		const char *executablePath = argv[0];
		const char *prefix = "/Users/";
		size_t prefixLen = strlen(prefix);
		pipeline->userPathPtr = strstr(executablePath,prefix);
		assert( pipeline->userPathPtr );
		const char *delim = strchr( &pipeline->userPathPtr[prefixLen],'/' );
		assert( delim );
		pipeline->userPathLen = (int)(delim - pipeline->userPathPtr);
		printf( "'%.*s'\n", pipeline->userPathLen, pipeline->userPathPtr );
	}
	
	pipeline->ConfigureAudio();
	pipeline->ConfigureVideo();
	pipeline->ConfigurenComplete();
	pipeline->InjectAudio();
	pipeline->InjectVideo();
	pipeline->Play();
	sleep(2);
	delete pipeline;
	return EXIT_SUCCESS;
}
