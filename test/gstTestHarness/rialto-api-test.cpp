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

// TODO: move implementation to own class
// plug api gaps?
// explore suite of real rialto impl
// missing codec mappings?
#include <gst/gst.h>

// TBR
static int gUserPathLen;
static const char *gUserPathPtr;

using namespace firebolt::rialto;

static void found_video_source_cb( GObject * object, GObject * orig, GParamSpec * pspec, class GstMediaPipeline *gstMediaPipeline );
static void found_audio_source_cb( GObject * object, GObject * orig, GParamSpec * pspec, class GstMediaPipeline *gstMediaPipeline );

#define TRACK_VIDEO 0
#define TRACK_AUDIO 1

class GstMediaPipeline:IMediaPipeline
{
private:
	friend void found_video_source_cb( GObject *, GObject *, GParamSpec *, class GstMediaPipeline * );
	friend void found_audio_source_cb( GObject *, GObject *, GParamSpec *, class GstMediaPipeline * );
	
	void found_source( GObject *orig, GParamSpec *pspec, int sourceId )
	{
		g_object_get( orig, pspec->name, &track[sourceId].appsrc, nullptr );
	}
	
	GstElement *pipeline;
	
	struct
	{
		GstElement *playbin;
		GstElement *appsrc;
	} track[2];
	
protected:
	std::weak_ptr<IMediaPipelineClient> client;
	std::weak_ptr<IMediaPipelineClient> getClient(){ return client; }
	
	bool attachSource(const std::unique_ptr<MediaSource> &source){
		return true;
	}
	
	bool attachSource(const std::unique_ptr<MediaSource> &source, int32_t &sourceId ){
		MediaSourceType sourceType = source->getType();
		GstCaps * caps = gst_caps_new_empty_simple( source->getMimeType().c_str() );
		
		if( sourceType == MediaSourceType::VIDEO || sourceType == MediaSourceType::AUDIO )
		{
			const IMediaPipeline::MediaSourceAV *mediaSourceAV = dynamic_cast<IMediaPipeline::MediaSourceAV *>(source.get());
			const char *streamFormat = nullptr;
			switch( mediaSourceAV->getStreamFormat() )
			{
				case StreamFormat::HVC1:
					streamFormat = "hvc1";
					break;
				case StreamFormat::AVC:
					streamFormat = "avc";
					break;
				case StreamFormat::RAW:
					streamFormat = "raw";
					break;
				default:
					break;
			}
			if( streamFormat )
			{
				gst_caps_set_simple( caps, "stream-format", G_TYPE_STRING, streamFormat, nullptr );
			}
			const std::shared_ptr<CodecData> codecData = mediaSourceAV->getCodecData();
			GstBuffer *buf = nullptr;
			if( codecData )
			{
				buf = gst_buffer_new_and_alloc(codecData->data.size());
				gst_buffer_fill(buf, 0, codecData->data.data(), codecData->data.size() );
				gst_caps_set_simple( caps, "codec_data", GST_TYPE_BUFFER, buf, nullptr );
			}
			
			if( sourceType == MediaSourceType::VIDEO )
			{
				sourceId = TRACK_VIDEO;
				const IMediaPipeline::MediaSourceVideo *mediaSourceVideo = dynamic_cast<IMediaPipeline::MediaSourceVideo *>(source.get());
				gst_caps_set_simple( caps,
									"alignment", G_TYPE_STRING, "au",
									"width", G_TYPE_INT, mediaSourceVideo->getWidth(),
									"height", G_TYPE_INT, mediaSourceVideo->getHeight(),
									"pixel-aspect-ratio", GST_TYPE_FRACTION, 1, 1,
									nullptr);
			}
			else if( sourceType == MediaSourceType::AUDIO )
			{
				sourceId = TRACK_AUDIO;
				const IMediaPipeline::MediaSourceAudio *mediaSourceAudio = dynamic_cast<IMediaPipeline::MediaSourceAudio *>(source.get());
				auto audioConfig = mediaSourceAudio->getAudioConfig();//.numberOfChannels );
				gst_caps_set_simple( caps,
									"framed", G_TYPE_BOOLEAN, TRUE,
									"rate", G_TYPE_INT, audioConfig.sampleRate,
									"channels", G_TYPE_INT, audioConfig.numberOfChannels,
									nullptr );
			}
			else
			{
				assert(0);
			}
			
			gchar *caps_string = gst_caps_to_string(caps);
			g_print("Negotiated caps: %s\n", caps_string);
			g_free(caps_string);
			
			gst_app_src_set_caps(GST_APP_SRC(track[sourceId].appsrc), caps);
			gst_caps_unref(caps);
			if( buf )
			{
				gst_buffer_unref(buf);
			}
		}
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
		gst_element_set_state( pipeline, GST_STATE_PLAYING );
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
	AddSegmentStatus addSegment(uint32_t needDataRequestId, const std::unique_ptr<MediaSegment> &mediaSegment)
	{
		GstBuffer *gstBuffer = gst_buffer_new_wrapped(
													  (gpointer)mediaSegment->getData(),
													  (gsize)mediaSegment->getDataLength() );
		GST_BUFFER_PTS(gstBuffer) = (GstClockTime)(mediaSegment->getTimeStamp());
		GST_BUFFER_DURATION(gstBuffer) = (GstClockTime)(mediaSegment->getDuration());
		gst_app_src_push_buffer(GST_APP_SRC(track[mediaSegment->getId()].appsrc), gstBuffer );
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
	
private:
	uint32_t needDataRequestId;
	
	Mp4Demux trackAudio;
	Mp4Demux trackVideo;
	int32_t sourceIdVideo;
	int32_t sourceIdAudio;
	
	void LoadAndDemuxSegment( Mp4Demux &mp4Demux, const char *path )
	{ // using generated bipbop content for initial test
		char fullpath[256];
		snprintf( fullpath, sizeof(fullpath), "%.*s/Documents/r" "d" "k" "e/aamp_test_internal/test/VideoTestStream/bipbop-gen/%s",
				 gUserPathLen, gUserPathPtr, path );
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
	GstMediaPipeline()
	{
		printf( "constructing GstMediaPipeline\n" );
		pipeline = gst_pipeline_new( "rialtoTest" );
		for( int i=0; i<2; i++ )
		{
			printf( "creating playbin for track#%d\n", i );
			GstElement *playbin = gst_element_factory_make("playbin", nullptr);
			track[i].playbin = playbin;
			track[i].appsrc = nullptr;
			gboolean rc = gst_bin_add(GST_BIN(pipeline), playbin );
			assert( rc );
			g_object_set( playbin, "uri", "appsrc://", nullptr );
			switch( i )
			{
				case TRACK_VIDEO:
					g_signal_connect( playbin, "deep-notify::source", G_CALLBACK(found_video_source_cb), this );
					break;
				case TRACK_AUDIO:
					g_signal_connect( playbin, "deep-notify::source", G_CALLBACK(found_audio_source_cb), this );
					break;
				default:
					break;
			}
		}
	}
	
	~GstMediaPipeline()
	{
		printf( "destructing GstMediaPipeline\n" );
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
		std::unique_ptr<MediaSourceVideo> sourceVideo =
		std::make_unique<MediaSourceVideo>(
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
												(int64_t)(pts*GST_SECOND), // timestamp
												(int64_t)(dur*GST_SECOND),
												trackAudio.audio.samplerate,
												trackAudio.audio.channel_count );
			size_t len = trackAudio.getLen(i);
			uint8_t *data = new uint8_t[len];
			std::memcpy(data, trackAudio.getPtr(i),len);
			audioSegment->setData( (uint32_t)len, data );
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
												(int64_t)(pts*GST_SECOND),
												(int64_t)(dur*GST_SECOND),
												trackVideo.video.width,
												trackVideo.video.height );
			size_t len = trackVideo.getLen(i);
			uint8_t *data = new uint8_t[len];
			std::memcpy(data, trackVideo.getPtr(i),len);
			videoSegment->setData( (uint32_t)len, data );
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

static void found_video_source_cb( GObject * object, GObject * orig, GParamSpec * pspec, class GstMediaPipeline *gstMediaPipeline )
{
	printf( "found_video_source_cb\n" );
	gstMediaPipeline->found_source(orig,pspec,TRACK_VIDEO);
}

static void found_audio_source_cb( GObject * object, GObject * orig, GParamSpec * pspec, class GstMediaPipeline *gstMediaPipeline )
{
	printf( "found_audio_source_cb\n" );
	gstMediaPipeline->found_source(orig,pspec,TRACK_AUDIO);
}

int my_main(int argc, char **argv)
{
	const char *executablePath = argv[0];
	const char *prefix = "/Users/";
	size_t prefixLen = strlen(prefix);
	gUserPathPtr = strstr(executablePath,prefix);
	assert( gUserPathPtr );
	const char *delim = strchr( &gUserPathPtr[prefixLen],'/' );
	assert( delim );
	gUserPathLen = (int)(delim - gUserPathPtr);
	
	gst_init(&argc, &argv);
	
	GstMediaPipeline *gstMediaPipeline = new GstMediaPipeline();
	gstMediaPipeline->Play();
	gstMediaPipeline->ConfigureAudio();
	gstMediaPipeline->ConfigureVideo();
	gstMediaPipeline->ConfigurenComplete();
	gstMediaPipeline->InjectAudio();
	gstMediaPipeline->InjectVideo();
	
	GMainLoop *main_loop = g_main_loop_new(nullptr, FALSE);
	g_main_loop_run(main_loop);
	g_main_loop_unref(main_loop);
	return 0;
}

int main(int argc, char **argv)
{
#if defined(__APPLE__) && defined (__GST_MACOS_H__)
	return gst_macos_main((GstMainFunc)my_main, argc, argv, nullptr);
#else
	return my_main(argc,argv);
#endif
}
