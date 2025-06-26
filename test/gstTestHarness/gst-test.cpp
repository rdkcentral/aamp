/*
 * If not stated otherwise in this file or this component's license file the
 * following copyright and licenses apply:
 *
 * Copyright 2024 RDK Management
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
#include "gst-test.h"
#include <string.h>
#include "tsdemux.hpp"
#include "turbo_xml.hpp"
#include "downloader.hpp"
#include "stream_utils.hpp"
#include "dash_adapter.hpp"
#include "turbo_xml.hpp"
#include "mp4demux.hpp"
#include <mutex>
#include <thread>
#include <unistd.h>
#include <netdb.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/time.h>

#define STOP_NEVER 9999

static std::mutex mCommandMutex;

#define TIME_BASED_BUFFERING_THRESHOLD 4.0

static enum ContentFormat
{
	eCONTENTFORMAT_MP4_ES,
	eCONTENTFORMAT_QTDEMUX,
	eCONTENTFORMAT_TS_ES,
	eCONTENTFORMAT_TSDEMUX,
} mContentFormat = eCONTENTFORMAT_QTDEMUX;

static const char *mContentFormatDescription[] =
{
	"inject elementary stream frames extracted from mp4",
	"inject mp4 (demuxed by gstreamer qtdemux element)",
	"inject elementary stream frames extracted from ts",
	"inject ts (demuxed by gstreamer tsdemux element, if available)",
};

/*
 todo: automated tests with pass/fail
 todo: audio codec test (stereo); video codec test (h.265)
 todo: support 1x to/from FF/REW transition (needs to suppress or drop audio track)
 todo: underflow/rebuffering detection
 */

#define DEFAULT_BASE_PATH "../../test/VideoTestStream"
#define MAX_BASE_PATH_SIZE 200
#define MAX_PATH_SIZE 256
#define RATE_NORMAL 1.0
#define SEGMENT_DURATION_SECONDS 2.0
#define SEGMENT_COUNT 30
#define AV_SEGMENT_LOAD_COUNT 8 // used for generic audio/video tests; inject 8 segment (~16s)
#define ARRAY_SIZE(A) (sizeof(A)/sizeof(A[0]))
#define BUFFER_SIZE 4096L
#define IFRAME_TRACK_FPS 4
#define IFRAME_TRACK_CADENCE_MS (1000/IFRAME_TRACK_FPS)

long long GetCurrentTimeMS(void)
{
	struct timeval t;
	gettimeofday(&t, NULL);
	return (long long)(t.tv_sec*1e3 + t.tv_usec*1e-3);
}

MyPipelineContext::MyPipelineContext( void ): nextPTS(0.0), nextTime(0.0), track(), pipeline(new Pipeline( this ))
{
}

MyPipelineContext::~MyPipelineContext()
{
	delete pipeline;
}

void MyPipelineContext::NeedData( MediaType mediaType )
{
	track[mediaType].needsData = true;
}

void MyPipelineContext::EnoughData( MediaType mediaType )
{
	track[mediaType].needsData = false;
}

static char base_path[MAX_BASE_PATH_SIZE] = DEFAULT_BASE_PATH;

void GetAudioHeaderPath( char path[MAX_PATH_SIZE], const char *language )
{
	(void)snprintf( path, MAX_PATH_SIZE, "%s/dash/%s_init.m4s", base_path, language );
}

void GetAudioSegmentPath( char path[MAX_PATH_SIZE], int segmentNumber, const char *language )
{
	//assert( segmentNumber<SEGMENT_COUNT );
	switch( mContentFormat )
	{
		case eCONTENTFORMAT_MP4_ES:
		case eCONTENTFORMAT_QTDEMUX:
			// note: test content using one-based index
			(void)snprintf( path, MAX_PATH_SIZE, "%s/dash/%s_%03d.mp3", base_path, language, segmentNumber+1 );
			break;
		case eCONTENTFORMAT_TSDEMUX:
		case eCONTENTFORMAT_TS_ES:
			// note: test content using zero-based index
			(void)snprintf( path, MAX_PATH_SIZE, "%s/hls/%s_%d.ts", base_path, language, segmentNumber );
			break;
	}
}

void GetVideoHeaderPath( char path[MAX_PATH_SIZE], VideoResolution resolution )
{
	if( resolution == eVIDEORESOLUTION_IFRAME )
	{
		(void)snprintf( path, MAX_PATH_SIZE, "%s/dash/iframe_init.m4s", base_path );
	}
	else
	{
		(void)snprintf( path, MAX_PATH_SIZE, "%s/dash/%dp_init.m4s", base_path, resolution );
	}
}

void GetVideoSegmentPath( char path[MAX_PATH_SIZE], int segmentNumber, VideoResolution resolution )
{
	switch( mContentFormat )
	{
		case eCONTENTFORMAT_MP4_ES:
		case eCONTENTFORMAT_QTDEMUX:
			// note: test content using one-based index
			if( resolution == eVIDEORESOLUTION_IFRAME )
			{
				(void)snprintf( path, MAX_PATH_SIZE, "%s/dash/iframe_%03d.m4s", base_path, segmentNumber+1 );
			}
			else
			{
				(void)snprintf( path, MAX_PATH_SIZE, "%s/dash/%dp_%03d.m4s", base_path, resolution, segmentNumber+1 );
			}
			break;
		case eCONTENTFORMAT_TSDEMUX:
		case eCONTENTFORMAT_TS_ES:
			if( resolution == eVIDEORESOLUTION_IFRAME )
			{
				(void)snprintf( path, MAX_PATH_SIZE, "%s/hls/iframe_%03d.ts", base_path, segmentNumber );
			}
			else
			{
				(void)snprintf( path, MAX_PATH_SIZE, "%s/hls/%dp_%03d.ts", base_path, resolution, segmentNumber );
			}
			break;
	}
}

/**
 * @brief segment buffer supporting data loading and injection
 */
class TrackFragment: public TrackEvent
{
private:
	uint32_t timeScale;
	gsize len;
	gpointer ptr;
	double duration;
	double pts_offset;
	MediaType mediaType;
	
	// for demuxed ts segment
	TsDemux *tsDemux;
	Mp4Demux *mp4Demux;
	
	std::string url;
	
	void Load( void )
	{
		ptr = LoadUrl(url,&len);
		switch( mContentFormat )
		{
			case eCONTENTFORMAT_MP4_ES:
				mp4Demux = new Mp4Demux(ptr,len,timeScale);
				assert( mp4Demux );
				break;
				
			case eCONTENTFORMAT_QTDEMUX:
			case eCONTENTFORMAT_TSDEMUX:
				break;
				
			case eCONTENTFORMAT_TS_ES:
				tsDemux = new TsDemux( mediaType, ptr, len );
				assert( tsDemux );
				break;
		}
	}
	
public:
	TrackFragment( MediaType mediaType, uint32_t timeScale, const char *path, double duration, double pts_offset=0 ):len(), ptr(), tsDemux(), mp4Demux(),  pts_offset(pts_offset), duration(duration), timeScale(timeScale), url(path), mediaType(mediaType)
	{
	}
	
	~TrackFragment()
	{
		delete mp4Demux;
		delete tsDemux;
		g_free(ptr);
	}
	
	bool Inject( MyPipelineContext *context, MediaType mediaType )
	{
		Load(); // lazily load segment data
		// TODO: use common baseclass for tsDemux and mp4Demux
		if( tsDemux )
		{
			int count = tsDemux->count();
			for( int i=0; i<count; i++ )
			{
				size_t len = tsDemux->getLen(i);
				double pts = tsDemux->getPts(i);
				double dts = tsDemux->getDts(i);
				double dur = tsDemux->getDuration(i);
				assert( len>0 );
				gpointer ptr = g_malloc(len);
				if( ptr )
				{
					memcpy( ptr, tsDemux->getPtr(i), len );
					context->pipeline->SendBufferES( mediaType, ptr, len,
													dur,
													pts+pts_offset,
													dts+pts_offset );
				}
			}
		}
		else if( mp4Demux )
		{
			int count = mp4Demux->count();
			if( count>0 )
			{ // media segment
				for( int i=0; i<count; i++ )
				{
					size_t len = mp4Demux->getLen(i);
					double pts = mp4Demux->getPts(i);
					double dts = mp4Demux->getDts(i);
					double dur = mp4Demux->getDuration(i);
					gpointer ptr = g_malloc(len);
					if( ptr )
					{
						memcpy( ptr, mp4Demux->getPtr(i), len );
						context->pipeline->SendBufferES( mediaType, ptr, len,
														dur,
														pts+pts_offset,
														dts+pts_offset );
					}
				}
			}
			else
			{ // initialization header
				context->pipeline->SetCaps(mediaType, mp4Demux );
			}
		}
		else if( ptr )
		{
			if( mContentFormat == eCONTENTFORMAT_QTDEMUX )
			{
				if( duration>0 )
				{ // audio or video segment (not an initialization header)
					mp4_AdjustMediaDecodeTime( (uint8_t *)ptr, len, (int64_t)(pts_offset*timeScale) );
				}
			}
			context->pipeline->SendBufferMP4( mediaType, ptr, len, duration, url.c_str() );
			ptr = NULL;
		}
		return true;
	}
	
	//copy constructor
	TrackFragment(const TrackFragment&)=delete;
	//copy assignment operator
	TrackFragment& operator=(const TrackFragment&)=delete;
};

/**
 * @brief gap event where no source content is to be presented
 */
class TrackGap: public TrackEvent
{
private:
	double pts;
	double duration;
	
public:
	TrackGap( double pts, double duration ):pts(pts),duration(duration)
	{
	}
	
	~TrackGap()
	{
	}
	
	bool Inject( MyPipelineContext *context, MediaType mediaType )
	{
		context->pipeline->SendGap( mediaType, pts, duration );
		return true;
	}
};

/**
 * @brief streaming source that dynamically collects and feed new fragments to pipeline, similar to an HLS/DASH player
 * @todo this is currently being invoked late on demand, giving bursts of download activity and sputters of underflow
 */
class TrackStreamer: public TrackEvent
{
private:
	int segmentIndex;
	double pts;
	VideoResolution resolution;
	const char *language;
	
public:
	TrackStreamer( VideoResolution resolution, int startIndex ) :segmentIndex(startIndex)
	,pts(startIndex*SEGMENT_DURATION_SECONDS)
	,resolution(resolution)
	,language("?")
	
	{ // video
	}
	
	TrackStreamer( const char *language, int startIndex ) :segmentIndex(startIndex)
	,pts(startIndex*SEGMENT_DURATION_SECONDS)
	,resolution((VideoResolution)0)
	,language(language)
	{ // audio
	}
	
	~TrackStreamer()
	{
	}
	
	bool Inject( MyPipelineContext *context, MediaType mediaType )
	{
		char path[MAX_PATH_SIZE];
		switch( mediaType )
		{
			case eMEDIATYPE_VIDEO:
				GetVideoSegmentPath( path, segmentIndex, resolution );
				break;
			case eMEDIATYPE_AUDIO:
				GetAudioSegmentPath( path, segmentIndex, language );
				break;
		}
		
		gsize len;
		gpointer ptr = LoadUrl(path,&len);
		if( ptr )
		{
			context->pipeline->SendBufferMP4( mediaType, ptr, len, SEGMENT_DURATION_SECONDS, path );
			segmentIndex++;
			pts += SEGMENT_DURATION_SECONDS;
			return false; // more
		}
		else
		{
			context->pipeline->SendEOS( mediaType );
			return true;
		}
	}
	
	//copy constructor
	TrackStreamer(const TrackStreamer&)=delete;
	//copy assignment operator
	TrackStreamer& operator=(const TrackStreamer&)=delete;
};

/**
 * @brief end-of-stream event to be injected to pipeline after final segment has been published
 */
class TrackEOS: public TrackEvent
{
private:
	size_t period;
	bool sentEOS;
	
public:
	TrackEOS() : sentEOS(false), period(0)
	{
	}
	
	~TrackEOS()
	{
	}
	
	bool Inject( MyPipelineContext *context, MediaType mediaType)
	{
		size_t count = context->pipeline->GetNumPendingSeek();
		if( !sentEOS )
		{
			period = count;
			context->pipeline->SendEOS(mediaType);
			sentEOS = true;
		}
		return count != period;
	}
};

class TrackWaitState: public TrackEvent
{
private:
	PipelineState prevState;
	PipelineState desiredState;
public:
	TrackWaitState( PipelineState desiredState ) : prevState(ePIPELINE_STATE_NULL),desiredState(desiredState)
	{
	}
	
	~TrackWaitState()
	{
	}
	
	bool Inject( MyPipelineContext *context, MediaType mediaType)
	{
		PipelineState state = context->pipeline->GetPipelineState();
		if( state != prevState )
		{
			switch( state )
			{
				case ePIPELINE_STATE_NULL:
					printf( "TrackWaitState(PIPELINeSTATE_NULL)\n" );
					break;
				case ePIPELINE_STATE_READY:
					printf( "TrackWaitState(PIPELINeSTATE_READY)\n" );
					break;
				case ePIPELINE_STATE_PAUSED:
					printf( "TrackWaitState(PIPELINeSTATE_PAUSED)\n" );
					break;
				case ePIPELINE_STATE_PLAYING:
					printf( "TrackWaitState(PIPELINeSTATE_PLAYING)\n" );
					break;
				default:
					printf( "TrackWaitState(%d)\n", state );
					break;
			}
			prevState = state;
		}
		if( state != desiredState )
		{
			return false;
		}
		return true;
	}
};

class TrackStep: public TrackEvent
{
private:
	int count;
	gulong delayMs;
public:
	TrackStep( int count, int delayMs ) :count(count),delayMs((gulong)delayMs)
	{
	}
	~TrackStep()
	{
	}
	
	bool Inject( MyPipelineContext *context, MediaType mediaType)
	{
		context->pipeline->Step();
		count--;
		if( count>0 )
		{
			g_usleep(delayMs*1000);
			return false;
		}
		return true;
	}
};

class TrackSleep: public TrackEvent
{
private:
	double milliseconds;
	
public:
	
	TrackSleep( double milliseconds ) : milliseconds(milliseconds)
	{
	}
	
	~TrackSleep()
	{
	}
	
	bool Inject( MyPipelineContext *context, MediaType mediaType)
	{ // todo: make non-blocking
		if( this->milliseconds>0 )
		{
			g_usleep(this->milliseconds*1000);
		}
		return true;
	}
};

class TrackPlay: public TrackEvent
{
public:
	TrackPlay( void )
	{
	}
	
	~TrackPlay()
	{
	}
	
	bool Inject( MyPipelineContext *context, MediaType mediaType)
	{
		context->pipeline->SetPipelineState(ePIPELINE_STATE_PLAYING);
		return true;
	}
};

Track::Track() : queue(new std::queue<class TrackEvent *>), needsData(), gstreamerReadyForInjection()
{
}

Track::~Track()
{
	delete queue;
}

void Track::Flush( void )
{
	while( !queue->empty() )
	{
		auto trackEvent = queue->front();
		queue->pop();
		delete trackEvent;
	}
}

void Track::EnqueueSegment( TrackEvent *TrackEvent )
{
	queue->push( TrackEvent );
}
void Track::EnqueueControl( TrackEvent *TrackEvent )
{
	queue->push( TrackEvent );
}

void Track::QueueVideoHeader( VideoResolution resolution )
{
	if( mContentFormat == eCONTENTFORMAT_QTDEMUX || mContentFormat == eCONTENTFORMAT_MP4_ES )
	{
		char path[MAX_PATH_SIZE];
		GetVideoHeaderPath(path, resolution );
		EnqueueSegment( new TrackFragment( eMEDIATYPE_VIDEO, 0, path, 0 ) );
	}
}

void Track::QueueVideoSegment( VideoResolution resolution, int startIndex, int count, double pts_offset )
{
	uint32_t timescale = 12800;
	char path[MAX_PATH_SIZE];
	if( count>0 )
	{
		while( count>0 )
		{
			GetVideoSegmentPath(path, startIndex, resolution );
			EnqueueSegment( new TrackFragment( eMEDIATYPE_VIDEO, timescale, path, SEGMENT_DURATION_SECONDS, pts_offset ) );
			startIndex++;
			count--;
		}
	}
	else
	{
		while( count<0 )
		{
			GetVideoSegmentPath(path, startIndex, resolution );
			EnqueueSegment( new TrackFragment( eMEDIATYPE_VIDEO, timescale, path, SEGMENT_DURATION_SECONDS, pts_offset ) );
			startIndex--;
			count++;
		}
	}
}

/**
 * @brief convenience method to collect init header and subsequent segments for an audio/video track
 */
void Track::QueueAudioHeader( const char *language )
{
	if( mContentFormat == eCONTENTFORMAT_QTDEMUX || mContentFormat == eCONTENTFORMAT_MP4_ES )
	{
		char path[MAX_PATH_SIZE];
		GetAudioHeaderPath( path, language );
		EnqueueSegment( new TrackFragment( eMEDIATYPE_AUDIO, 0, path, 0 ) );
	}
}

void Track::QueueAudioSegment( const char *language, int startIndex, int count, double pts_offset )
{
	uint32_t timescale = 48000;
	char path[MAX_PATH_SIZE];
	for( int i=0; i<count; i++ )
	{
		int segmentNumber = startIndex + i;
		GetAudioSegmentPath( path, segmentNumber, language );
		EnqueueSegment( new TrackFragment( eMEDIATYPE_AUDIO, timescale, path, SEGMENT_DURATION_SECONDS, pts_offset ) );
	}
}

/**
 * @brief convenience method to queue multiple gap events
 */
void Track::QueueGap( int startIndex, int count )
{
	double pts = startIndex*SEGMENT_DURATION_SECONDS;
	for( int i=0; i<count; i++ )
	{
		EnqueueControl( new TrackGap( pts, SEGMENT_DURATION_SECONDS ) );
		pts += SEGMENT_DURATION_SECONDS;
	}
}

typedef struct
{
	int startIndex;
	int segmentCount;
	VideoResolution resolution;
	const char *language;
} PeriodInfo;

static const PeriodInfo mPeriodInfo[] =
{ // startIndex, segmentCount
	{ 0, 2, eVIDEORESOLUTION_720P,  "en" },
	{ 0, 2, eVIDEORESOLUTION_720P,  "es" },
	{ 0, 2, eVIDEORESOLUTION_720P,  "fr" },
	{ 0, 2, eVIDEORESOLUTION_720P,  "en" },
	{ 0, 2, eVIDEORESOLUTION_720P,  "es" },
	{ 0, 2, eVIDEORESOLUTION_720P,  "fr" },
	
	{ 10, 5, eVIDEORESOLUTION_720P,  "fr" },
	
	{  0, 2, eVIDEORESOLUTION_360P,  "es" }, // Español, uno, dos, tres
	{ 15, 4, eVIDEORESOLUTION_1080P, "en" }, // 30..37
	{  0, 2, eVIDEORESOLUTION_480P,  "fr" }, // Français, un, deux, trois
	{ 23, 4, eVIDEORESOLUTION_1080P, "en" }, // 46..53
	{  0, 2, eVIDEORESOLUTION_720P,  "es" }, // Español, uno, dos, tres
};

typedef enum
{
	eSTATE_IDLE,
	eSTATE_LOAD
} AppState;

class AppContext
{
public:
	AppState appState;
	MyPipelineContext pipelineContext;
	Timeline timeline;
	GMainLoop *main_loop;
	bool logPositionChanges;
	int autoStepCount;
	gulong autoStepDelayMs;
	
	AppContext():logPositionChanges(false),autoStepCount(0),autoStepDelayMs(IFRAME_TRACK_CADENCE_MS), pipelineContext(),main_loop(g_main_loop_new(NULL, FALSE))
	{
		printf( "AppContext constructor.... type stuff here!!!\n" );
	}
	~AppContext()
	{
		printf( "AppContext destructor\n" );
	}
	
	/**
	 * @brief multi-period playback using flushing seek in-between periods
	 */
	void TestDAI1( void )
	{
		pipelineContext.pipeline->Reset();
		Track &video = pipelineContext.track[eMEDIATYPE_VIDEO];
		Track &audio = pipelineContext.track[eMEDIATYPE_AUDIO];
		for( int i=0; i<ARRAY_SIZE(mPeriodInfo); i++ )
		{
			const PeriodInfo *periodInfo = &mPeriodInfo[i];
			double firstPts = periodInfo->startIndex*SEGMENT_DURATION_SECONDS;
			double duration_s = periodInfo->segmentCount*SEGMENT_DURATION_SECONDS;
			SeekParam seekParam;
			seekParam.flags = GST_SEEK_FLAG_FLUSH;
			seekParam.start_s = firstPts;
			seekParam.stop_s = seekParam.start_s + duration_s;
			pipelineContext.pipeline->ScheduleSeek( seekParam );
			
			video.QueueVideoHeader( periodInfo->resolution );
			video.QueueVideoSegment(
									periodInfo->resolution,
									periodInfo->startIndex,
									periodInfo->segmentCount );
			
			audio.QueueAudioHeader( periodInfo->language );
			audio.QueueAudioSegment(
									periodInfo->language,
									periodInfo->startIndex,
									periodInfo->segmentCount );
			
			video.EnqueueControl( new TrackEOS() );
			audio.EnqueueControl( new TrackEOS() );
		}
		
		pipelineContext.pipeline->Configure( eMEDIATYPE_VIDEO );
		pipelineContext.pipeline->Configure( eMEDIATYPE_AUDIO );
		pipelineContext.pipeline->SetPipelineState(ePIPELINE_STATE_PLAYING);
	}
	
	/**
	 * @brief multi-period playback using pts restamping
	 */
	void TestDAI2( void )
	{
		pipelineContext.pipeline->Reset();
		Track &video = pipelineContext.track[eMEDIATYPE_VIDEO];
		Track &audio = pipelineContext.track[eMEDIATYPE_AUDIO];
		double total_duration = 0.0;
		for( int i=0; i<ARRAY_SIZE(mPeriodInfo); i++ )
		{
			double duration_s = mPeriodInfo[i].segmentCount*SEGMENT_DURATION_SECONDS;
			total_duration += duration_s;
		}
		SeekParam seekParam;
		seekParam.flags = GST_SEEK_FLAG_FLUSH;
		seekParam.start_s = 0;
		seekParam.stop_s = total_duration;
		pipelineContext.pipeline->ScheduleSeek(seekParam);
		
		total_duration = 0;
		for( int i=0; i<ARRAY_SIZE(mPeriodInfo); i++ )
		{
			const PeriodInfo *periodInfo = &mPeriodInfo[i];
			// TODO: estimated firstPts is not quite right for hls ts
			// leads to delays/stuttering
			double firstPts = periodInfo->startIndex*SEGMENT_DURATION_SECONDS;
			double duration = periodInfo->segmentCount*SEGMENT_DURATION_SECONDS;
			double pts_offset = total_duration - firstPts;
			total_duration += duration;
			video.QueueVideoHeader( periodInfo->resolution );
			video.QueueVideoSegment(
									periodInfo->resolution,
									periodInfo->startIndex,
									periodInfo->segmentCount,
									pts_offset ); // timescale adjusted
			
			audio.QueueAudioHeader( periodInfo->language );
			audio.QueueAudioSegment(
									periodInfo->language,
									periodInfo->startIndex,
									periodInfo->segmentCount,
									pts_offset ); // timescale adjusted
		}
		
		video.EnqueueControl( new TrackEOS() );
		audio.EnqueueControl( new TrackEOS() );
		
		pipelineContext.pipeline->Configure( eMEDIATYPE_VIDEO );
		pipelineContext.pipeline->Configure( eMEDIATYPE_AUDIO );
		pipelineContext.pipeline->SetPipelineState(ePIPELINE_STATE_PLAYING);
	}
	
	/**
	 * @brief multi-period playback using GST_SEEK_FLAG_SEGMENT and non-flushing seek
	 */
	void TestDAI3( void )
	{
		pipelineContext.pipeline->Reset();
		Track &video = pipelineContext.track[eMEDIATYPE_VIDEO];
		Track &audio = pipelineContext.track[eMEDIATYPE_AUDIO];
		double total_duration = 0.0;
		for( int i=0; i<ARRAY_SIZE(mPeriodInfo); i++ )
		{
			const PeriodInfo *periodInfo = &mPeriodInfo[i];
			double firstPts = periodInfo->startIndex*SEGMENT_DURATION_SECONDS;
			double duration = periodInfo->segmentCount*SEGMENT_DURATION_SECONDS;
			SeekParam seekParam;
			seekParam.flags = GST_SEEK_FLAG_SEGMENT;
			seekParam.start_s = total_duration;
			seekParam.stop_s = total_duration + duration;
			double pts_offset = total_duration-firstPts;
			printf( "period %d: start=%f stop=%f firstPts=%f\n", i, seekParam.start_s,seekParam.stop_s, firstPts );
			total_duration += duration;
			pipelineContext.pipeline->ScheduleSeek(seekParam);
			video.QueueVideoHeader( periodInfo->resolution );
			video.QueueVideoSegment(
									periodInfo->resolution,
									periodInfo->startIndex,
									periodInfo->segmentCount,
									pts_offset );
			video.EnqueueControl( new TrackEOS() );
			audio.QueueAudioHeader( periodInfo->language );
			audio.QueueAudioSegment(
									periodInfo->language,
									periodInfo->startIndex,
									periodInfo->segmentCount,
									pts_offset );
			audio.EnqueueControl( new TrackEOS() );
		}
		// configure pipelines and begin streaming
		pipelineContext.pipeline->Configure( eMEDIATYPE_VIDEO );
		pipelineContext.pipeline->Configure( eMEDIATYPE_AUDIO );
		pipelineContext.pipeline->SetPipelineState(ePIPELINE_STATE_PLAYING);
	}
	
	void FeedPipelineIfNeeded( MediaType mediaType )
	{
		Track &t = pipelineContext.track[mediaType];
		bool needsData = false;
#ifdef TIME_BASED_BUFFERING_THRESHOLD
		if( t.needsData )
		{
			t.gstreamerReadyForInjection = true;
		}
		if( t.gstreamerReadyForInjection )
		{ // don't start injection until gstreamer has signaled ready
			double pos = pipelineContext.pipeline->GetPositionMilliseconds(mediaType)/1000.0;
			if( pos < pipelineContext.seekPos )
			{
				pos = pipelineContext.seekPos;
			}
			double endPos = pipelineContext.seekPos + pipelineContext.pipeline->GetInjectedSeconds(mediaType);
			needsData = (endPos - pos < TIME_BASED_BUFFERING_THRESHOLD);
		}
#else
		needsData = t.needsData;
#endif
		if( needsData )
		{
			auto queue = t.queue;
			if( queue->size()>0 )
			{
				auto buffer = queue->front();
				if( buffer->Inject(&pipelineContext,mediaType) )
				{
					queue->pop();
					delete buffer;
					//std::this_thread::sleep_for(std::chrono::milliseconds(50));
				}
			}
		}
	}
	
	void IdleFunc( void )
	{
		static long long tPrev;
		const std::lock_guard<std::mutex> lock(mCommandMutex);
		
		if( pipelineContext.pipeline )
		{
			if( logPositionChanges )
			{ // log any change in position
				long long tNow = GetCurrentTimeMS();
				if( abs(tNow-tPrev)>100 )
				{ // throttle to 10hz, to avoid spiking CPU
					tPrev = tNow;
					static long long last_reported_video_position;
					static long long last_reported_audio_position;
					
					long long video_position = pipelineContext.pipeline->GetPositionMilliseconds(eMEDIATYPE_VIDEO);
					long long audio_position = pipelineContext.pipeline->GetPositionMilliseconds(eMEDIATYPE_AUDIO);
					
					if( video_position != last_reported_video_position ||
					   audio_position != last_reported_audio_position )
					{
						g_print( "time=%lld position: audio=%lld video=%lld\n", tNow, audio_position, video_position );
						last_reported_audio_position = audio_position;
						last_reported_video_position = video_position;
					}
				}
			}
			FeedPipelineIfNeeded( eMEDIATYPE_VIDEO );
			FeedPipelineIfNeeded( eMEDIATYPE_AUDIO );
			
			if(( autoStepCount>0 ) &&
			   ( ePIPELINE_STATE_PAUSED == pipelineContext.pipeline->GetPipelineState() ))
			{ // delay between iframe presentation
				g_usleep(autoStepDelayMs*1000);
				pipelineContext.pipeline->Step();
				autoStepCount--;
			}
		}
	}
	
	void DumpXml( const XmlNode *node, int indent )
	{
		if( node )
		{
			for( int i=0; i<indent; i++ )
			{
				printf( " " );
			}
			printf( "%s", node->tagName.c_str() );
			for( auto it : node->attributes )
			{
				printf( " %s=%s", it.first.c_str(), it.second.c_str() );
			}
			printf("\n");
			for( int i=0; i<node->children.size(); i++ )
			{
				DumpXml( node->children[i], indent+1 );
			}
		}
	}
	
	static void ShowHelp( void )
	{
		printf( "help // show this list of available commands\n" );
		printf( "load <url> // play DASH or HLS manifest\n" );
		printf( "inventory <url> // load specified (DASH) manifest and generate inventory.sh with sequence of calls to generate-video-segment.sh and genenerate-audio-segment.sh\n" );
		printf( "format // show current format & demuxing options\n");
		printf( "position // toggle position reporting (default = off)\n" );
		printf( "flush // reset pipeline\n" );
		printf( "seek <offset_s> // specify target media position for load; call before or during playback\n" );
		
		printf( "dai1 // multi-period playback using flushing seek\n" );
		printf( "dai2 // multi-period playback using pts restamping\n" );
		printf( "dai3 // multi-period using GST_SEEK_FLAG_SEGMENT and non-flushing seek\n" );
		
		printf( "pause // pause playback\n" );
		printf( "play // resume/start playback\n" );
		printf( "null // reset pipeline\n" );
		printf( "stop // kill pipeline\n" );
		
		// misc post-tune commands
		printf( "rate <newRate> // apply instantaneous rate change\n" );
		printf( "step // step one frame at a time (while paused)\n" );

		printf( "dump // generate gst-test.dot\n" );
		
		// misc commands
		printf( "path <base_path> // set the base path for test stream data\n" );
		printf( "exit // exit test\n" );
	}
	
	static const char *MapCodec( const std::string & codec )
	{
		// audio
		if( codec.rfind("mp4a.40.",0)==0 ) return "aac";
		if( codec == "ec-3" ) return "eac3";
		if( codec == "ac-3" ) return "ac3";
		
		// video
		if( codec.rfind("avc1.")==0 ) return "h264";
		if( codec.rfind("hvc1.")==0 ) return "hevc";
		if( codec=="hev1" ) return "hevc";
		
		printf( "unmapped codec: %s\n", codec.c_str() );
		assert(0);
	}
	
	std::string localUrl( const std::string url )
	{
		if( url.rfind("file://",0)==0 )
		{
			return url.substr(7);
		}
		if( url.rfind("http://",0)==0 )
		{
			return url.substr(7);
		}
		if( url.rfind("https://",0)==0 )
		{
			return url.substr(8);
		}
		return url;
	}
	
	void InjectSegments( const Timeline &timelineObj, bool inventory )
	{
		FILE *fInventory = NULL;
		if( inventory )
		{
			fInventory = fopen( "inventory.sh", "wb" );
		}
		double secondsToSkip = pipelineContext.seekPos;
		bool processingFirstPeriod = true;
		double pts_offset = 0.0;
		double next_pts = 0.0;
		for( int iPeriod=0; iPeriod<timelineObj.period.size(); iPeriod++ )
		{
			const PeriodObj &period = timelineObj.period[iPeriod];
			pts_offset += next_pts - period.firstPts;
			next_pts = period.firstPts + period.duration;
			if( processingFirstPeriod )
			{
				if( secondsToSkip >= period.duration )
				{ // skip this period
					secondsToSkip -= period.duration;
					continue;
				}
				SeekParam seekParam;
				seekParam.flags = GST_SEEK_FLAG_FLUSH;
				seekParam.start_s = pipelineContext.seekPos;
				seekParam.stop_s = STOP_NEVER;
				if( !inventory )
				{
					pipelineContext.pipeline->ScheduleSeek(seekParam);
				}
				processingFirstPeriod = false;
			}
			
			for( auto it : period.adaptationSet )
			{
				const AdaptationSet &adaptationSet = it.second;
				MediaType mediaType;
				if( adaptationSet.contentType == "video" )
				{
					mediaType = eMEDIATYPE_VIDEO;
				}
				else if( adaptationSet.contentType == "audio" )
				{
					mediaType = eMEDIATYPE_AUDIO;
				}
				else if( adaptationSet.contentType == "image" )
				{ // not yet supported
					continue;
				}
				else if( adaptationSet.contentType == "text" )
				{ // not yet supported
					continue;
				}
				else
				{ // likely an empty early availability period - nothing to transcode
					continue;
				}
				
				std::map<std::string,std::string> segment_template_param;
				int iRep = 0; // select lowest resolution profile
				auto representation = adaptationSet.representation[iRep];
				segment_template_param["RepresentationID"] = representation.id;
				
				auto segmentCount = representation.data.media.size();
				if( segmentCount==1 )
				{
					segmentCount = representation.data.duration.size();
					if( segmentCount==1 )
					{
						auto d = representation.data.duration[0]/representation.data.timescale;
						if( d>0 )
						{ // avoids division by zero and ambiguity with SegmentTimeline having 1 segment
							segmentCount = period.duration/d;
						}
					}
				}
				
				std::string initHeaderUrl = representation.BaseURL + ExpandURL( representation.data.initialization, segment_template_param );
				std::cout << initHeaderUrl << "\n";
				if( !inventory )
				{
					pipelineContext.track[mediaType].EnqueueSegment(new TrackFragment( mediaType, representation.data.timescale, initHeaderUrl.c_str(), 0 ) );
				}
				
				double skip = secondsToSkip;
				for( unsigned idx=0; idx<segmentCount; idx++ )
				{
					int durationIndex = idx;
					if( durationIndex >= representation.data.duration.size() )
					{
						durationIndex = 0;
					}
					double segmentDurationS = representation.data.duration[durationIndex]/(double)representation.data.timescale;
					if( skip>0 )
					{
						skip -= segmentDurationS;
						if( skip>0 )
						{
							continue;
						}
						segmentDurationS += skip;
					}
					
					int mediaIndex = idx;
					if( mediaIndex >= representation.data.media.size() )
					{
						mediaIndex = 0;
					}
					
					uint64_t number = representation.data.startNumber+idx;
					segment_template_param["Number"] = std::to_string( number );
					if( representation.data.time.size()>0 )
					{
						segment_template_param["Time"] = std::to_string( representation.data.time[idx] );
					}
					const std::string &media = representation.data.media[mediaIndex];
					std::string mediaUrl = representation.BaseURL + ExpandURL( media, segment_template_param );
					std::cout << mediaUrl << "\n";
					if( inventory )
					{
						if( fInventory )
						{
							gsize len = 0;
							uint64_t baseMediaDecodeTime =
							representation.data.time.size()>0? representation.data.time[durationIndex] : 0;
							gpointer ptr = LoadUrl( mediaUrl, &len );
							if( ptr )
							{ // here we peek inside original segment (if available) to extract media decode time, expected to match time from manifest
								uint64_t extractedTime = mp4_AdjustMediaDecodeTime( (uint8_t *)ptr, (size_t)len, 0 );
								if( extractedTime != baseMediaDecodeTime )
								{
									printf( "WARNING! extractedTime(%" PRIu64 ") !=baseMediaDecodeTime(%" PRIu64 ")\n",
										   extractedTime, baseMediaDecodeTime );
									baseMediaDecodeTime = extractedTime;
								}
								g_free( ptr );
							}
							switch( mediaType )
							{
								case eMEDIATYPE_AUDIO:
									fprintf( fInventory, "bash generate-audio-segment.sh %ld %s %" PRIu64 " %" PRIu64 " %" PRIu32 " %" PRIu64 " \"%s\" \"%s\" silence.wav\n",
											representation.audioSamplingRate,
											MapCodec(adaptationSet.codecs),
											baseMediaDecodeTime,
											representation.data.duration[durationIndex],
											representation.data.timescale,
											number,
											localUrl(mediaUrl).c_str(),
											localUrl(initHeaderUrl).c_str() );
									break;
								case eMEDIATYPE_VIDEO:
									fprintf( fInventory, "bash generate-video-segment.sh %ld %ld %ld %s %" PRIu64 " %" PRIu64 " %" PRIu32 " %" PRIu64 " \"%s\" \"%s\" testpat.jpg\n",
											representation.width,
											representation.height,
											representation.frameRate,
											MapCodec(adaptationSet.codecs),
											baseMediaDecodeTime,
											representation.data.duration[durationIndex],
											representation.data.timescale,
											number,
											localUrl(mediaUrl).c_str(),
											localUrl(initHeaderUrl).c_str() );
									break;
							}
						}
						continue;
					}
					pipelineContext.track[mediaType].EnqueueSegment( new TrackFragment(
																					   mediaType,
																					   representation.data.timescale,
																					   mediaUrl.c_str(), segmentDurationS, pts_offset ) );
				}
			} // next adaptationSet
			secondsToSkip = 0.0;
		}
		
		if( inventory )
		{
			if( fInventory )
			{
				fclose( fInventory );
			}
		}
		else
		{
			for( int mediaType=0; mediaType<2; mediaType++ )
			{
				Track &t = pipelineContext.track[mediaType];
				t.EnqueueControl( new TrackEOS() );
			}
			
			// configure pipelines and begin streaming
			pipelineContext.pipeline->Configure( eMEDIATYPE_VIDEO );
			pipelineContext.pipeline->Configure( eMEDIATYPE_AUDIO );
			
			// begin playing immediately
			//pipelineContext.pipeline->SetPipelineState(ePIPELINE_STATE_PLAYING);
			
			// useful for seek-while-paused
			pipelineContext.pipeline->SetPipelineState(ePIPELINE_STATE_PAUSED);
		}
	}
	
	/**
	 * @brief basic support for playing a muxed hls/ts playlist, with pts restamping for seamless playback across discontinuities
	 */
	void LoadHLS( const char *ptr, size_t size, const std::string &url )
	{
		struct SegmentInfo
		{
			float duration;
			std::string path;
			bool discontinuity;
			double firstPts;
		};
		std::vector<SegmentInfo> segmentList;
		
		std::string text = std::string(ptr,size);
		std::istringstream iss(text);
		std::string line;
		SegmentInfo info;
		memset( &info, 0, sizeof(info) );
		while (std::getline(iss, line)) {
			if( starts_with(line,"#EXT-X-DISCONTINUITY") )
			{
				info.discontinuity = true;
			}
			if( starts_with(line, "#EXTINF:") )
			{
				info.duration = std::stof(line.substr(8));
				if( std::getline(iss, line) )
				{
					info.path = line;
					info.firstPts = 0.0;
					segmentList.push_back(info);
					memset( &info, 0, sizeof(info) );
				}
			}
		}
		
		Track &video = pipelineContext.track[eMEDIATYPE_VIDEO];
		Track &audio = pipelineContext.track[eMEDIATYPE_AUDIO];
		double pts_offset = 0.0;
		double total_duration = 0.0;
		uint32_t timescale = 0; // n/a
		
		mContentFormat = eCONTENTFORMAT_TS_ES; // use tsdemux.hpp
		
		//Seek( 1.0/*rate*/, 0/*start*/, -1/*stop*/, 0/*baseTime*/ );
		
		for( auto segmentInfo : segmentList )
		{
			std::string fullpath = url;
			auto delim = fullpath.find_last_of("/");
			assert( delim!=std::string::npos );
			fullpath = fullpath.substr(0,delim+1);
			fullpath += segmentInfo.path;
			if( segmentInfo.discontinuity )
			{ // below used to compute firstPts for future fragment
				size_t segmentBytes = 0;
				void *segmentPtr = LoadUrl(fullpath.c_str(),&segmentBytes);
				assert( segmentPtr );
				auto tsDemux = new TsDemux( eMEDIATYPE_VIDEO, segmentPtr, segmentBytes );
				assert( tsDemux );
				assert( tsDemux->count()>0 );
				double firstPts = tsDemux->getPts(0);
				pts_offset = total_duration - firstPts;
				delete tsDemux;
				g_free( segmentPtr);
			}
			total_duration += segmentInfo.duration;
			
			video.EnqueueSegment( new TrackFragment( eMEDIATYPE_VIDEO, timescale, fullpath.c_str(), segmentInfo.duration, pts_offset ) );
			audio.EnqueueSegment( new TrackFragment( eMEDIATYPE_AUDIO, timescale, fullpath.c_str(), segmentInfo.duration, pts_offset ) );
		}
		
		video.EnqueueControl( new TrackEOS() );
		audio.EnqueueControl( new TrackEOS() );
		
		// configure pipelines and begin streaming
		pipelineContext.pipeline->Configure( eMEDIATYPE_VIDEO );
		pipelineContext.pipeline->Configure( eMEDIATYPE_AUDIO );
		//pipelineContext.pipeline->SetPipelineState(ePIPELINE_STATE_PLAYING);
		pipelineContext.pipeline->SetPipelineState(ePIPELINE_STATE_PAUSED);
	}
	
	void LoadDASH( const char *ptr, size_t size, const std::string &url, bool inventory )
	{
		XmlNode *xml = new XmlNode( "document", ptr, size );
		auto numChildren = xml->children.size();
		auto MPD = xml->children[numChildren-1];
		DumpXml(MPD,0);
		timeline = parseManifest( *MPD, url );
		timeline.Debug();
		ComputeTimestampOffsets( timeline );
		InjectSegments( timeline, inventory );
		delete xml;
	}
	
	void Load( const std::string &url, bool inventory )
	{
		size_t size = 0;
		auto ptr = LoadUrl( url, &size );
		if( ptr )
		{
			if( size>=7 && memcmp(ptr,"#EXTM3U",7)==0 )
			{
				LoadHLS( (const char *)ptr, size, url );
			}
			else
			{
				LoadDASH( (const char *)ptr, size, url, inventory );
			}
			free( ptr );
		}
	}
	
	void Flush( double position_s = 0.0 )
	{
		pipelineContext.pipeline->Reset();
		pipelineContext.track[eMEDIATYPE_VIDEO].Flush();
		pipelineContext.track[eMEDIATYPE_AUDIO].Flush();
		SeekParam param;
		param.flags = GST_SEEK_FLAG_FLUSH;
		param.start_s = position_s;
		param.stop_s = STOP_NEVER;
		pipelineContext.pipeline->Seek( param );
	}
	
	void ProcessCommand( const char *str )
	{
		const std::lock_guard<std::mutex> lock(mCommandMutex);
		double newRate = 1.0;
		int format;
		
		if( strcmp(str,"help")==0 )
		{
			ShowHelp();
		}
		else if( strcmp(str,"status")==0 )
		{ // stub - used with html front end to send NOP request and collect updated state for display
		}
		else if( starts_with(str,"inventory") )
		{
			Load( &str[10], true );
		}
		else if( starts_with(str,"load ") )
		{
			appState=eSTATE_LOAD;
			Load( &str[5], false );
		}
		else if( sscanf(str,"format %d",&format)==1 && format>=0 && format<ARRAY_SIZE(mContentFormatDescription) )
		{
			mContentFormat = (ContentFormat)format;
			printf( "format:=%d // %s\n",
				   format, mContentFormatDescription[format] );
		}
		else if( starts_with(str,"format") )
		{
			printf( "current format=%d\n", mContentFormat );
			printf( "usage: format <format>\n" );
			for( int i=0; i<ARRAY_SIZE(mContentFormatDescription); i++ )
			{
				printf( "\tformat %d // %s\n", i, mContentFormatDescription[i] );
			}
		}
		else if( strcmp(str,"dump")==0 )
		{
			pipelineContext.pipeline->DumpDOT();
		}
		else if( strcmp(str,"position")==0 )
		{
			logPositionChanges = !logPositionChanges;
			if( logPositionChanges )
			{
				printf( "position reporting enabled\n" );
			}
			else
			{
				printf( "position reporting disabled\n" );
			}
		}
		else if( strcmp(str,"flush")==0 )
		{
			Flush();
		}
		else if( sscanf(str,"rate %lf", &newRate )==1 )
		{
			pipelineContext.pipeline->InstantaneousRateChange(newRate);
		}
		else if( strcmp(str,"step")==0 )
		{
			pipelineContext.pipeline->Step();
		}
		else if( strcmp(str,"dai1")==0 )
		{
			TestDAI1();
		}
		else if( strcmp(str,"dai2")==0 )
		{
			TestDAI2();
		}
		else if( strcmp(str,"dai3")==0 )
		{
			TestDAI3();
		}
		else if( strcmp(str,"ready")==0 )
		{
			pipelineContext.pipeline->SetPipelineState(ePIPELINE_STATE_READY);
		}
		else if( strcmp(str,"pause")==0 )
		{
			pipelineContext.pipeline->SetPipelineState(ePIPELINE_STATE_PAUSED);
		}
		else if( strcmp(str,"play")==0 )
		{
			pipelineContext.pipeline->SetPipelineState(ePIPELINE_STATE_PLAYING);
		}
		else if( strcmp(str,"null")==0 )
		{
			pipelineContext.pipeline->SetPipelineState(ePIPELINE_STATE_NULL);
		}
		else if( strcmp(str,"stop")==0 )
		{
			pipelineContext.pipeline->SetPipelineState(ePIPELINE_STATE_NULL);
			delete pipelineContext.pipeline;
			pipelineContext.pipeline = new Pipeline( (class PipelineContext *)&pipelineContext );
		}
		else if( sscanf(str, "path %199s", base_path ) == 1 )
		{
			assert(199 < sizeof(base_path));
			printf( "new base path: '%s'\n", base_path );
		}
		else if( strcmp( str,"exit")==0 )
		{
			g_main_loop_quit(main_loop);
		}
		else if( sscanf(str,"seek %lf", &pipelineContext.seekPos )==1 )
		{
			switch( appState )
			{
				case eSTATE_IDLE:
					break;
				case eSTATE_LOAD:
					Flush(pipelineContext.seekPos);
					InjectSegments( timeline, false );
					break;
			}
		}
		else if( str[0] )
		{
			printf( "unknown command: %s\n", str );
		}
	}
	
	AppContext(const AppContext&)=delete; // copy constructor
	AppContext& operator=(const AppContext&)=delete; // copy assignment operator
};

static gboolean myIdleFunc( gpointer arg )
{
	AppContext *appContext = (AppContext *)arg;
	appContext->IdleFunc();
	return TRUE;
}

static gboolean handle_keyboard( GIOChannel * source, GIOCondition cond, AppContext * appContext )
{
	gchar *str = NULL;
	gsize terminator_pos;
	GError *error = NULL;
	// CID:337049 - Untrusted loop bound
	gsize length = 0;
	if( g_io_channel_read_line(source, &str, &length, &terminator_pos, &error) == G_IO_STATUS_NORMAL )
	{
		// replace newline terminator with 0x00
		gchar *fin = str;
		gsize counter = 1;
		while( (counter < length) && (*fin>=' ') )
		{
			fin++;
			counter++;
		}
		*fin = 0x00;
		
		appContext->ProcessCommand( str );
	}
	
	g_free( str );
	
	if (error)
	{
		g_error_free(error);
	}
	
	return TRUE;
}

static void NetworkCommandServer( struct AppContext *appContext )
{ // simply http server, dispatching incoming commands and returning playback state
	char buf[1024];
	int parentfd = socket(AF_INET, SOCK_STREAM, 0);
	assert( parentfd>=0 );
	int optval = 1;
	setsockopt(parentfd, SOL_SOCKET, SO_REUSEADDR, (const void *)&optval , sizeof(int));
	struct sockaddr_in serveraddr; /* server's addr */
	bzero((char *) &serveraddr, sizeof(serveraddr));
	serveraddr.sin_family = AF_INET;
	serveraddr.sin_addr.s_addr = htonl(INADDR_ANY);
	unsigned short port = 8080;
	serveraddr.sin_port = htons(port);
	int rc;
	for(;;)
	{
		rc = bind(parentfd, (struct sockaddr *) &serveraddr, sizeof(serveraddr));
		if( rc>=0 ) break;
		//printf( "bind failed - retry in 5s\n" );
		g_usleep(5000*1000);
	}
	rc = listen(parentfd, 5);
	assert( rc>=0 );
	struct sockaddr_in clientaddr;
	socklen_t clientlen = sizeof(clientaddr);
	for(;;)
	{
		int childfd = accept(parentfd, (struct sockaddr *) &clientaddr, &clientlen);
		assert( childfd>=0 );
		struct hostent *hostp = gethostbyaddr((const char *)&clientaddr.sin_addr.s_addr,
											  sizeof(clientaddr.sin_addr.s_addr), AF_INET);
		assert( hostp != NULL );
		char *hostaddrp = inet_ntoa(clientaddr.sin_addr);
		assert( hostaddrp != NULL );
		static bool firstConnect = true;
		if( firstConnect )
		{
			printf("established connection with %s (%s)\n", hostp->h_name, hostaddrp);
			firstConnect = false;
		}
		bzero( buf, sizeof(buf) );
		auto numBytesRead = read( childfd, buf, sizeof(buf) );
		assert( numBytesRead>=0 );
		const char *delim = strstr(buf,"\r\n\r\n");
		if( delim )
		{
			delim+=4;
			if( *delim )
			{
				appContext->ProcessCommand( delim );
			}
		}
		char json[256];
		double seekPos = appContext->pipelineContext.seekPos;
		long long vpos = appContext->pipelineContext.pipeline->GetPositionMilliseconds(eMEDIATYPE_VIDEO);
		long long apos = appContext->pipelineContext.pipeline->GetPositionMilliseconds(eMEDIATYPE_AUDIO);
		int contentLength = snprintf(
									 json, sizeof(json),
									 "{\"state\":%d,"
									 "\n\"start\":%.3f,"
									 "\n\"video\":{\"pos\":%.3f,\"buf\":%.3f},"
									 "\n\"audio\":{\"pos\":%.3f,\"buf\":%.3f}}",
									 appContext->pipelineContext.pipeline->GetPipelineState(),
									 seekPos,
									 (vpos<0)?-1:vpos/1000.0,
									 appContext->pipelineContext.pipeline->GetInjectedSeconds(eMEDIATYPE_VIDEO),
									 (apos<0)?-1:apos/1000.0,
									 appContext->pipelineContext.pipeline->GetInjectedSeconds(eMEDIATYPE_AUDIO) );
		snprintf( buf, sizeof(buf),
				 "HTTP/1.1 200 OK\r\n"
				 "Access-Control-Allow-Origin: *\r\n"
				 "Access-Control-Allow-Methods: POST\r\n"
				 "Access-Control-Allow-Headers: Content-Type, Authorization\r\n"
				 "Content-Type: application/json\r\n"
				 "Content-Length: %d\r\n"
				 "\r\n%s",
				 contentLength,json
				 );
		auto numBytesWritten = write(childfd, buf, strlen(buf));
		assert( numBytesWritten>=0 );
		close(childfd);
	}
}

int my_main(int argc, char **argv)
{
	//setenv( "GST_DEBUG", "*:4", 1 ); // programatically override gstreamer log level:
	// refer https://gstreamer.freedesktop.org/documentation/tutorials/basic/debugging-tools.html?gi-language=c
	gst_init(&argc, &argv);
	g_print( "gstreamer test harness\n" );
	struct AppContext appContext;
	GIOChannel *io_stdin = g_io_channel_unix_new (fileno (stdin));
	(void)g_io_add_watch (io_stdin, G_IO_IN, (GIOFunc) handle_keyboard, &appContext);
	(void)g_idle_add( myIdleFunc, (gpointer)&appContext );
	std::thread myNetworkCommandServer( NetworkCommandServer, &appContext );
	g_main_loop_run(appContext.main_loop);
	g_main_loop_unref(appContext.main_loop);
	return 0;
}

int main(int argc, char **argv)
{
#if defined(__APPLE__) && defined (__GST_MACOS_H__)
	return gst_macos_main((GstMainFunc)my_main, argc, argv, NULL);
#else
	return my_main(argc,argv);
#endif
}
