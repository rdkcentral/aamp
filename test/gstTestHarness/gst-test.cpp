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
#include "gst-port.h"
#include "gst-test.h"
#include <string.h>
#include "tsdemux.hpp"
#include "turbo_xml.hpp"
#include "downloader.hpp"
#include "stream_utils.hpp"
#include "dash_adapter.hpp"
#include "turbo_xml.hpp"

static double gPtsOffset = 0.0; // used with es injection, as alternative to gst_pad_set_offset

static enum
{
	eCONTENTFORMAT_MP4, // use (cmaf) .mp4 segments as input
	eCONTENTFORMAT_TS, // inject .ts segments
	eCONTENTFORMAT_ES // inject es extracted from ts segments
} mContentFormat;

/*
 todo: automated tests with pass/fail
 todo: audio codec test (stereo); video codec test (h.265)
 todo: support 1x to/from FF/REW transition (needs to suppress or drop audio track)
 todo: underflow/rebuffering detection
 */

#define DEFAULT_BASE_PATH "../../test/VideoTestStream"
#define MAX_BASE_PATH_SIZE 200
#define MAX_PATH_SIZE 256
#define SEGMENT_DURATION_SECONDS 2.0
#define SEGMENT_COUNT 30
#define AV_SEGMENT_LOAD_COUNT 8 // used for generic audio/video tests; inject 8 segment (~16s)
#define ARRAY_SIZE(A) (sizeof(A)/sizeof(A[0]))
#define BUFFER_SIZE 4096L
#define IFRAME_TRACK_FPS 4
#define IFRAME_TRACK_CADENCE_MS (1000/IFRAME_TRACK_FPS)

static int m_ff_delta = 1;
static int m_ff_delay = 250;

static const char *GetCapsFormat()
{
	if( mContentFormat == eCONTENTFORMAT_MP4 )
	{
		return "video/quicktime";
	}
	else
	{ // auto-detect
		return NULL;
	}
}

MyPipelineContext::MyPipelineContext( void ): numPendingEOS(0), nextPTS(0.0), nextTime(0.0), track(), pipeline(new Pipeline( this ))
	{
	}
	
	MyPipelineContext::~MyPipelineContext()
	{
		delete pipeline;
	}
	
void MyPipelineContext::ReachedEOS( void )
{
	printf( "app_ReachedEOS\n" );
	if( numPendingEOS>0 )
	{
		numPendingEOS--;
		if( numPendingEOS==0 )
		{ // both tracks reached their respective EOS
			printf( "nextPTS=%f\n", nextPTS );
			double rate = 1.0;
			double start = nextPTS;
			double stop = -1;
			double pts_offset = 0;
			pipeline->Flush( rate, start, stop, nextTime, pts_offset );
		}
	}
}
	
void MyPipelineContext::NeedData( MediaType mediaType )
{
	track[mediaType].needsData = true;
}
	
void MyPipelineContext::EnoughData( MediaType mediaType )
{
	track[mediaType].needsData = false;
}
	
void MyPipelineContext::PadProbeCallback( MediaType mediaType )
{
	track[mediaType].padProbeCount++;
}

static char base_path[MAX_BASE_PATH_SIZE] = DEFAULT_BASE_PATH;

void GetAudioHeaderPath( char path[MAX_PATH_SIZE], const char *language )
{
	(void)snprintf( path, MAX_PATH_SIZE, "%s/dash/%s_init.m4s", base_path, language );
}

void GetAudioSegmentPath( char path[MAX_PATH_SIZE], int segmentNumber, const char *language )
{
	assert( segmentNumber<SEGMENT_COUNT );
	if( mContentFormat == eCONTENTFORMAT_MP4 )
	{ // note: test content using one-based index
		(void)snprintf( path, MAX_PATH_SIZE, "%s/dash/%s_%03d.mp3", base_path, language, segmentNumber+1 );
	}
	else
	{ // note: test content using zero-based index
		(void)snprintf( path, MAX_PATH_SIZE, "%s/hls/%s_%d.ts", base_path, language, segmentNumber );
	}
}

void GetVideoIHeaderPath( char path[MAX_PATH_SIZE], VideoResolution resolution )
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
	if( mContentFormat == eCONTENTFORMAT_MP4 )
	{ // note: test content using one-based index
		if( resolution == eVIDEORESOLUTION_IFRAME )
		{
			(void)snprintf( path, MAX_PATH_SIZE, "%s/dash/iframe_%03d.m4s", base_path, segmentNumber+1 );
		}
		else
		{
			(void)snprintf( path, MAX_PATH_SIZE, "%s/dash/%dp_%03d.m4s", base_path, resolution, segmentNumber+1 );
		}
	}
	else
	{ // note: test content using zero-based index
		if( resolution == eVIDEORESOLUTION_IFRAME )
		{
			(void)snprintf( path, MAX_PATH_SIZE, "%s/hls/iframe_%03d.ts", base_path, segmentNumber );
		}
		else
		{
			(void)snprintf( path, MAX_PATH_SIZE, "%s/hls/%dp_%03d.ts", base_path, resolution, segmentNumber );
		}
	}
}

/**
 * @brief segment buffer supporting data loading and injection
 */
class TrackFragment: public TrackEvent
{
private:
	// for mp4 segment
	gsize len;
	gpointer ptr;
	
	// for demuxed ts segment
	TsDemux *tsDemux;
	double pts_offset;
	std::string url;
	
	void Load( void )
	{
		if( mContentFormat == eCONTENTFORMAT_ES )
		{
			tsDemux = new TsDemux( url.c_str() );
			assert( tsDemux );
			pts_offset = gPtsOffset;
		}
		else
		{
			ptr = LoadUrl(url,&len);
			//assert( this->ptr );
		}
	}
	
public:
	TrackFragment( const char *path ):len(), ptr(), tsDemux(), pts_offset()
	{
		url = path;
		//Load(); // preload all content up-front
	}
	
	~TrackFragment()
	{
		g_free(ptr);
		delete tsDemux;
	}
	
	bool Inject( MyPipelineContext *context, MediaType mediaType )
	{
		Load(); // lazily load segment data when needed
		if( tsDemux )
		{
			int count = tsDemux->count();
			for( int i=0; i<count; i++ )
			{
				size_t len = tsDemux->getLen(i);
				double pts = tsDemux->getPts(i);
				double dts = tsDemux->getDts(i);
				assert( len>0 );
				gpointer ptr = g_malloc(len);
				if( ptr )
				{
					double duration = 0; // logically, subset of SEGMENT_DURATION_SECONDS
					memcpy( ptr, tsDemux->getPtr(i), len );
					context->pipeline->SendBuffer( mediaType, ptr, len,
												  pts+pts_offset,
												  dts+pts_offset,
												  duration );
				}
			}
		}
		else if( ptr )
		{
			context->pipeline->SendBuffer( mediaType, ptr, len );
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
			context->pipeline->SendBuffer( mediaType, ptr, len );
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
public:
	TrackEOS()
	{
	}
	
	~TrackEOS()
	{
	}
	
	bool Inject( MyPipelineContext *context, MediaType mediaType)
	{
		context->pipeline->SendEOS(mediaType);
		return true;
	}
};

/**
 * @brief period seperator for discontinuity handling
 */
class TrackDiscontinuity: public TrackEvent
{
private:
	bool active;
	double nextPTS;
	double nextTime;
	
public:
	TrackDiscontinuity( double pts, double baseTime ) :active(false),nextPTS(pts),nextTime(baseTime)
	{
		printf( "TrackDiscontinuity constructor; nextPTS := %f\n", pts );
	}
	
	~TrackDiscontinuity()
	{
	}
	
	bool Inject(MyPipelineContext *context, MediaType mediaType)
	{
		if( !active )
		{
			context->nextPTS = nextPTS;
			context->nextTime = nextTime;
			context->numPendingEOS++;
			printf( "queued EOS; nextPTS=%f\n", nextPTS );
			context->pipeline->SendEOS(mediaType);
			active = true;
		}
		if( context->numPendingEOS>0 )
		{ // prevent queue from advancing until both tracks reached end of period
			return false;
		}
		return true;
	}
};

class TrackWaitState: public TrackEvent
{
private:
	PipelineState prevState;
	PipelineState desiredState;
public:
	TrackWaitState( PipelineState desiredState ) : prevState(ePIPELINESTATE_NULL),desiredState(desiredState)
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
				case ePIPELINESTATE_NULL:
					printf( "TrackWaitState(PIPELINESTATE_NULL)\n" );
					break;
				case ePIPELINESTATE_READY:
					printf( "TrackWaitState(PIPELINESTATE_READY)\n" );
					break;
				case ePIPELINESTATE_PAUSED:
					printf( "TrackWaitState(PIPELINESTATE_PAUSED)\n" );
					break;
				case ePIPELINESTATE_PLAYING:
					printf( "TrackWaitState(PIPELINESTATE_PLAYING)\n" );
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

class TrackNext: public TrackEvent
{
private:
	int injectCount;
	
public:
	TrackNext( int injectCount ) : injectCount(injectCount)
	{
		printf( "TrackNext; injectCount = %d\n", injectCount );
	}
	
	~TrackNext()
	{
	}
	
	bool Inject( MyPipelineContext *context, MediaType mediaType)
	{
		if( context->track[mediaType].padProbeCount>=injectCount )
		{
			printf( "UNBLOCKING %s\n", (mediaType==eMEDIATYPE_AUDIO)?"audio":"video" );
			return true;
		}
		return false;
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

class TrackFlush: public TrackEvent
{
private:
	double rate;
	double start;
	double stop;
	double position_adjust;
	
public:
	TrackFlush( double rate, double start, double stop, double position_adjust )
	: rate(rate),start(start),stop(stop),position_adjust(position_adjust)
	{
	}
	
	~TrackFlush()
	{
	}
	
	bool Inject( MyPipelineContext *context, MediaType mediaType)
	{
		printf( "processing TrackFlush\n" );
		double pts_offset = 0.0;
		context->pipeline->Flush( rate, start, stop, position_adjust, pts_offset );
		return true;
	}
};

class TrackTimestampOffset: public TrackEvent
{
private:
	double pts_offset;
	
public:
	TrackTimestampOffset( double pts_offset )
	: pts_offset(pts_offset)
	{
	}
	
	~TrackTimestampOffset()
	{
	}
	
	bool Inject( MyPipelineContext *context, MediaType mediaType)
	{
		printf( "processing TrackTimestampOffset(%f)\n", pts_offset );
		context->pipeline->SetTimestampOffset(mediaType,pts_offset);
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
		context->pipeline->SetPipelineState(ePIPELINESTATE_PLAYING);
		return true;
	}
};

Track::Track() : queue(new std::queue<class TrackEvent *>), injectCount(), needsData(), padProbeCount()
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
	injectCount = 0;
}

void Track::EnqueueSegment( TrackEvent *TrackEvent )
{
	injectCount++;
	queue->push( TrackEvent );
}
void Track::EnqueueControl( TrackEvent *TrackEvent )
{
	queue->push( TrackEvent );
}

void Track::QueueVideoHeader( VideoResolution resolution )
{
	if( mContentFormat == eCONTENTFORMAT_MP4 )
	{
		char path[MAX_PATH_SIZE];
		GetVideoIHeaderPath(path, resolution );
		EnqueueSegment( new TrackFragment( path ) );
	}
}

void Track::QueueVideoSegment( VideoResolution resolution, int startIndex, int count )
{
	char path[MAX_PATH_SIZE];
	double pts = startIndex*SEGMENT_DURATION_SECONDS;
	if( count>0 )
	{
		while( count>0 )
		{
			GetVideoSegmentPath(path, startIndex, resolution );
			EnqueueSegment( new TrackFragment( path ) );
			pts += SEGMENT_DURATION_SECONDS;
			startIndex++;
			count--;
		}
	}
	else
	{
		while( count<0 )
		{
			GetVideoSegmentPath(path, startIndex, resolution );
			EnqueueSegment( new TrackFragment( path ) );
			pts -= SEGMENT_DURATION_SECONDS;
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
	if( mContentFormat == eCONTENTFORMAT_MP4 )
	{
		char path[MAX_PATH_SIZE];
		GetAudioHeaderPath( path, language );
		EnqueueSegment( new TrackFragment( path ) );
	}
}

void Track::QueueAudioSegment( const char *language, int startIndex, int count )
{
	char path[MAX_PATH_SIZE];
	double pts = startIndex*SEGMENT_DURATION_SECONDS;
	for( int i=0; i<count; i++ )
	{
		int segmentNumber = startIndex + i;
		GetAudioSegmentPath( path, segmentNumber, language );
		EnqueueSegment( new TrackFragment( path ) );
		pts += SEGMENT_DURATION_SECONDS;
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
{
	{ 10, 5, eVIDEORESOLUTION_720P,  "fr" },
	{  0, 2, eVIDEORESOLUTION_360P,  "es" },
	{ 15, 4, eVIDEORESOLUTION_1080P, "en" },
	{  0, 2, eVIDEORESOLUTION_480P,  "fr" },
	{ 23, 4, eVIDEORESOLUTION_1080P, "en" },
	{  0, 2, eVIDEORESOLUTION_720P,  "es" },
};

class AppContext
{
public:
	MyPipelineContext pipelineContext;
	GMainLoop *main_loop;
	bool logPositionChanges;
	long long last_reported_position;
	int autoStepCount;
	gulong autoStepDelayMs;
	
	AppContext():last_reported_position(-1),logPositionChanges(false),autoStepCount(0),autoStepDelayMs(IFRAME_TRACK_CADENCE_MS), pipelineContext(),main_loop(g_main_loop_new(NULL, FALSE))
	{
		printf( "AppContext constructor.... type stuff here!!!\n" );
	}
	~AppContext()
	{
		printf( "AppContext destructor\n" );
	}
	
	void Flush( double rate = 1, double start = 0, double stop = -1, double baseTime = 0, double pts_offset = 0.0 )
	{
		pipelineContext.track[eMEDIATYPE_VIDEO].Flush();
		pipelineContext.track[eMEDIATYPE_AUDIO].Flush();
		pipelineContext.pipeline->Flush(rate,start,stop,baseTime,pts_offset);
		pipelineContext.track[eMEDIATYPE_VIDEO].padProbeCount = 0;
		pipelineContext.track[eMEDIATYPE_AUDIO].padProbeCount = 0;
	}
	
	void TestABR( void )
	{
		printf( "***TestABR\n" );
		Flush();
		const char *language = "en";
		int count = 0;
		
		//Flush(1,0,-1,0,-1.8);
		
		Track &video = pipelineContext.track[eMEDIATYPE_VIDEO];
		
		video.QueueVideoHeader( eVIDEORESOLUTION_360P );
		video.QueueVideoSegment(eVIDEORESOLUTION_360P, count++, 1 );
		
		video.QueueVideoHeader(eVIDEORESOLUTION_480P);
		video.QueueVideoSegment(eVIDEORESOLUTION_480P, count++, 1 );
		
		video.QueueVideoHeader(eVIDEORESOLUTION_720P );
		video.QueueVideoSegment(eVIDEORESOLUTION_720P, count++, 1 );
		
		video.QueueVideoHeader(eVIDEORESOLUTION_1080P );
		video.QueueVideoSegment(eVIDEORESOLUTION_1080P, count++, 1 );
		
		video.QueueVideoHeader(eVIDEORESOLUTION_720P );
		video.QueueVideoSegment(eVIDEORESOLUTION_720P, count++, 1 );
		
		video.QueueVideoHeader(eVIDEORESOLUTION_480P );
		video.QueueVideoSegment(eVIDEORESOLUTION_480P, count++, 1 );
		
		video.QueueVideoHeader( eVIDEORESOLUTION_360P );
		video.QueueVideoSegment(eVIDEORESOLUTION_360P, count++, 1 );
		
		video.EnqueueControl( new TrackEOS() );
		
		Track &audio = pipelineContext.track[eMEDIATYPE_AUDIO];
		audio.QueueAudioHeader(language );
		audio.QueueAudioSegment(language, 0, count );
		audio.EnqueueControl( new TrackEOS() );
		
		pipelineContext.pipeline->Configure( eMEDIATYPE_VIDEO, GetCapsFormat() );
		pipelineContext.pipeline->Configure( eMEDIATYPE_AUDIO, GetCapsFormat() );
		pipelineContext.pipeline->SetPipelineState(ePIPELINESTATE_PLAYING);
	}
	
	void TestDAI( void )
	{
		Track &video = pipelineContext.track[eMEDIATYPE_VIDEO];
		Track &audio = pipelineContext.track[eMEDIATYPE_AUDIO];
		double rate = 1.0;
		double start = mPeriodInfo[0].startIndex*SEGMENT_DURATION_SECONDS;
		double stop = -1;
		double baseTime = 0;
		
		// queue up content and discontinuities
		Flush(rate,start,stop,baseTime);
		for( int i=0; i<ARRAY_SIZE(mPeriodInfo); i++ )
		{
			const PeriodInfo *periodInfo = &mPeriodInfo[i];
			
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
			
			double periodDuration = periodInfo->segmentCount*SEGMENT_DURATION_SECONDS;
			baseTime += periodDuration;
			
			if( i < ARRAY_SIZE(mPeriodInfo)-1 )
			{
				periodInfo = &mPeriodInfo[i+1];
				double nextPTS = periodInfo->startIndex*SEGMENT_DURATION_SECONDS;
				video.EnqueueControl( new TrackDiscontinuity( nextPTS, baseTime ) );
				audio.EnqueueControl( new TrackDiscontinuity( nextPTS, baseTime ) );
			}
		}
		video.EnqueueControl( new TrackEOS() );
		audio.EnqueueControl( new TrackEOS() );
		
		// configure pipelines and begin streaming
		pipelineContext.pipeline->Configure( eMEDIATYPE_VIDEO, GetCapsFormat() );
		pipelineContext.pipeline->Configure( eMEDIATYPE_AUDIO, GetCapsFormat() );
		pipelineContext.pipeline->SetPipelineState(ePIPELINESTATE_PLAYING);
	}
	
	void TestDAI2( void )
	{
		Track &video = pipelineContext.track[eMEDIATYPE_VIDEO];
		Track &audio = pipelineContext.track[eMEDIATYPE_AUDIO];
		double first_pts = 0.0;
		double last_pts = 0.0;
		for( int i=0; i<ARRAY_SIZE(mPeriodInfo); i++ )
		{
			const PeriodInfo *periodInfo = &mPeriodInfo[i];
			double next_pts = periodInfo->startIndex*SEGMENT_DURATION_SECONDS;
			if( i==0 )
			{
				first_pts = next_pts;
				last_pts = first_pts;
				if( mContentFormat == eCONTENTFORMAT_ES )
				{
					gPtsOffset = -first_pts;
					Flush( 1.0/*rate*/, 0/*start*/, -1/*stop*/, 0/*baseTime*/, 0 );
				}
				else
				{
					Flush( 1.0/*rate*/, 0/*start*/, -1/*stop*/, 0/*baseTime*/, -first_pts );
				}
			}
			else
			{
				double pts_offset = (last_pts - next_pts) - first_pts;
				if( mContentFormat == eCONTENTFORMAT_ES )
				{ // adjust pts at injection time
					gPtsOffset = pts_offset;
				}
				else
				{ // use gst_pad_set_offset
					// wait for pad probe to ack prior segment processing
					video.EnqueueControl( new TrackNext(video.injectCount) );
					audio.EnqueueControl( new TrackNext(audio.injectCount) );
					
					// update pts offset
					video.EnqueueControl( new TrackTimestampOffset(pts_offset) );
					audio.EnqueueControl( new TrackTimestampOffset(pts_offset) );
				}
			}
			last_pts += periodInfo->segmentCount*SEGMENT_DURATION_SECONDS;
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
		}
		
		video.EnqueueControl( new TrackEOS() );
		audio.EnqueueControl( new TrackEOS() );
		
		// configure pipelines and begin streaming
		pipelineContext.pipeline->Configure( eMEDIATYPE_VIDEO, GetCapsFormat() );
		pipelineContext.pipeline->Configure( eMEDIATYPE_AUDIO, GetCapsFormat() );
		pipelineContext.pipeline->SetPipelineState(ePIPELINESTATE_PLAYING);
	}
	
	void TestStream( double start, double stop, const char *language )
	{
		printf( "***TestStream(%f..%f,%s)\n", start, stop, language );
		double rate = 1.0;
		Flush(rate,start,stop,start);
		
		Track &video = pipelineContext.track[eMEDIATYPE_VIDEO];
		Track &audio = pipelineContext.track[eMEDIATYPE_AUDIO];
		
		int startIndex = start / SEGMENT_DURATION_SECONDS;
		
		VideoResolution resolution = eVIDEORESOLUTION_360P;
		
		video.QueueVideoHeader( resolution ); // video initialization header
		video.EnqueueControl(new TrackStreamer(resolution, startIndex ) );
		
		audio.QueueAudioHeader( language ); // audio initialization header
		audio.EnqueueControl(new TrackStreamer(language, startIndex ) );
		
		pipelineContext.pipeline->Configure( eMEDIATYPE_VIDEO, GetCapsFormat() );
		pipelineContext.pipeline->Configure( eMEDIATYPE_AUDIO, GetCapsFormat() );
		pipelineContext.pipeline->SetPipelineState(ePIPELINESTATE_PLAYING);
	}
	
	void Test_Seek( double seek_pos )
	{
		printf("Test_Seek : seek_pos %lf",seek_pos);
		Track &video = pipelineContext.track[eMEDIATYPE_VIDEO];
		Track &audio = pipelineContext.track[eMEDIATYPE_AUDIO];
		
		Flush();
		
		int startIndex = (seek_pos / SEGMENT_DURATION_SECONDS);
		VideoResolution resolution = eVIDEORESOLUTION_360P;
		video.QueueVideoHeader( resolution ); //video initialization header
		video.EnqueueControl(new TrackStreamer(resolution, startIndex ) );
		audio.QueueAudioHeader( "en" ); // audio initialization header
		audio.EnqueueControl(new TrackStreamer("en", startIndex ) );
		
		pipelineContext.pipeline->Flush( eMEDIATYPE_VIDEO, seek_pos );
		pipelineContext.pipeline->Flush( eMEDIATYPE_AUDIO, seek_pos );
		
	}
	
	/**
	 * @brief fast forward through iframes with 250s wait and flush after each presentation
	 */
	void TestFF_Seek( void )
	{
		Track &video = pipelineContext.track[eMEDIATYPE_VIDEO];
		Flush();
		video.QueueVideoHeader( eVIDEORESOLUTION_IFRAME );
		pipelineContext.pipeline->Configure( eMEDIATYPE_VIDEO, GetCapsFormat() );
		pipelineContext.pipeline->SetPipelineState(ePIPELINESTATE_PAUSED);
		for( int frame=0; frame<SEGMENT_COUNT; frame ++ )
		{
			video.QueueVideoSegment( eVIDEORESOLUTION_IFRAME, frame, 1 );
			video.EnqueueControl(new TrackEOS() ); // needed for small segment to render
			video.EnqueueControl( new TrackWaitState(ePIPELINESTATE_PAUSED) ); // block until visible
			video.EnqueueControl( new TrackSleep(IFRAME_TRACK_CADENCE_MS) );
			double pts = frame*SEGMENT_DURATION_SECONDS;
			video.EnqueueControl( new TrackFlush( 1, pts, -1, pts ) );
		}
	}
	
	/**
	 * @brief stream iframe track at accelerated playback rate
	 */
	void TestFF_Rate( void )
	{
		Track &video = pipelineContext.track[eMEDIATYPE_VIDEO];
		Flush( 8/*rate*/ );
		video.QueueVideoHeader( eVIDEORESOLUTION_IFRAME );
		video.QueueVideoSegment( eVIDEORESOLUTION_IFRAME, 0, SEGMENT_COUNT );
		video.EnqueueControl( new TrackEOS() );
		pipelineContext.pipeline->Configure( eMEDIATYPE_VIDEO, GetCapsFormat() );
		pipelineContext.pipeline->SetPipelineState( ePIPELINESTATE_PLAYING );
	}
	
	/**
	 * @brief rewind through iframe track with 250s wait and flush after each presentation
	 */
	void TestREW_Seek( void )
	{
		Track &video = pipelineContext.track[eMEDIATYPE_VIDEO];
		double rate = 1;
		bool first = true;
		int frame = SEGMENT_COUNT;
		while( frame>0 )
		{
			double pts = --frame*SEGMENT_DURATION_SECONDS;
			if( first )
			{
				Flush( rate, pts, -1, pts );
				video.QueueVideoHeader(eVIDEORESOLUTION_IFRAME );
				pipelineContext.pipeline->Configure( eMEDIATYPE_VIDEO, GetCapsFormat() );
				pipelineContext.pipeline->SetPipelineState(ePIPELINESTATE_PAUSED);
				first = false;
			}
			else
			{
				video.EnqueueControl( new TrackFlush( 1, pts, -1, pts ) );
			}
			video.QueueVideoSegment(eVIDEORESOLUTION_IFRAME, frame, 1 );
			video.EnqueueControl( new TrackEOS() ); // needed for small segment to render
			video.EnqueueControl( new TrackWaitState(ePIPELINESTATE_PAUSED) );
			video.EnqueueControl( new TrackSleep(IFRAME_TRACK_CADENCE_MS) );
		}
	}
	
	/**
	 * brief stream iframes track using negative rate
	 *
	 * @note  working on ubuntu, but on OSX rewinds crazy fast, with bogus position reporting
	 */
	void TestREW_Rate( void )
	{
		Track &video = pipelineContext.track[eMEDIATYPE_VIDEO];
		int frame = SEGMENT_COUNT-1;
		double rate = -8;
		double pts = frame*SEGMENT_DURATION_SECONDS;
		Flush( rate, pts, 0, pts );
		video.QueueVideoHeader(eVIDEORESOLUTION_IFRAME );
		video.QueueVideoSegment(eVIDEORESOLUTION_IFRAME, frame, -SEGMENT_COUNT );
		video.EnqueueControl( new TrackEOS() );
		pipelineContext.pipeline->Configure( eMEDIATYPE_VIDEO, GetCapsFormat() );
		pipelineContext.pipeline->SetPipelineState(ePIPELINESTATE_PLAYING);
	} // TestREW
	
	/**
	 * @brief use gstreamer frame stepping feature to advance through iframe track
	 * @note this works well (at least in simulator), but we don't get progress updates*
	 */
	void TestREW_Step( void )
	{
		Track &video = pipelineContext.track[eMEDIATYPE_VIDEO];
		int frame = SEGMENT_COUNT-1;
		double rate = -1;
		double pts = frame*SEGMENT_DURATION_SECONDS;
		Flush( rate, pts, 0, pts );
		video.QueueVideoHeader( eVIDEORESOLUTION_IFRAME );
		video.QueueVideoSegment( eVIDEORESOLUTION_IFRAME, frame, -SEGMENT_COUNT );
		video.EnqueueControl( new TrackEOS() );
		pipelineContext.pipeline->Configure( eMEDIATYPE_VIDEO, GetCapsFormat() );
		pipelineContext.pipeline->SetPipelineState( ePIPELINESTATE_PAUSED );
		
		// periodically step through playback, with autoStepDelayMs (250ms) delay
		autoStepCount = SEGMENT_COUNT+1;
	}
	
	void TestSAP( void )
	{
		pipelineContext.pipeline->SetPipelineState(ePIPELINESTATE_PAUSED);
		auto position = pipelineContext.pipeline->GetPositionMilliseconds()/1000.0;
		Flush( 1, position, -1, position );
		LoadVideo( eVIDEORESOLUTION_360P );
		LoadAudio("fr");
		pipelineContext.pipeline->SetPipelineState(ePIPELINESTATE_PLAYING);
	}
	
	void TestGap( const char *videoGap, const char *audioGap )
	{
		VideoResolution resolution = eVIDEORESOLUTION_720P;
		const char *language = "en";
		Track &videoTrack = pipelineContext.track[eMEDIATYPE_VIDEO];
		Track &audioTrack = pipelineContext.track[eMEDIATYPE_AUDIO];
		int frame = 0;
		Flush();
		videoTrack.QueueVideoHeader(resolution );
		audioTrack.QueueAudioHeader(language );
		pipelineContext.pipeline->Configure( eMEDIATYPE_VIDEO, GetCapsFormat() );
		pipelineContext.pipeline->Configure( eMEDIATYPE_AUDIO, GetCapsFormat() );
		
		// start with 4s normal audio/video
		videoTrack.QueueVideoSegment(resolution, frame, 2 );
		audioTrack.QueueAudioSegment(language, frame, 2 );
		frame += 2;
		
		// 4s
		if( strcmp(videoGap,"content")==0 )
		{ // Play video during the gap
			videoTrack.QueueVideoSegment(resolution, frame, 2 );
		}
		else if( strcmp(videoGap,"event")==0 )
		{ // Send gap event
			videoTrack.QueueGap(frame, 2 );
		}
		else if( strcmp(videoGap,"skip")==0 )
		{ // skip - let SOC handle mising segment
		}
		else
		{
			printf( "unknown video gap directive: '%s'\n", videoGap );
		}
		
		if( strcmp(audioGap,"content")==0 )
		{ // Play audio during the gap
			audioTrack.QueueAudioSegment(language, frame, 2 );
		}
		else if( strcmp(audioGap,"event")==0 )
		{ // Send gap event
			audioTrack.QueueGap(frame, 2);
		}
		else if( strcmp(audioGap,"skip")==0 )
		{ // skip - let SOC handle mising segment
		}
		else
		{
			printf( "unknown audio gap directive: '%s'\n", audioGap );
		}
		
		frame += 2;
		
		// end with 4s normal audio/video
		videoTrack.QueueVideoSegment(resolution, frame, 2 );
		audioTrack.QueueAudioSegment(language, frame, 2 );
		
		videoTrack.EnqueueControl( new TrackEOS() );
		audioTrack.EnqueueControl( new TrackEOS() );
		
		pipelineContext.pipeline->SetPipelineState(ePIPELINESTATE_PLAYING);
	}
	
	void TestSeamlessAudioSwitch()
	{
		// Get current position
		double position = pipelineContext.pipeline->GetPositionMilliseconds() / 1000.0;
		
		Track &audio = pipelineContext.track[eMEDIATYPE_AUDIO];
		int startIndex = position / SEGMENT_DURATION_SECONDS;
		int count = AV_SEGMENT_LOAD_COUNT - startIndex;
		printf( "position=%lf startIndex=%d count:%d\n", position, startIndex, count );
		
		// Flush current audio buffers
		audio.Flush();
		// Queue new audio track
		audio.QueueAudioHeader( "en" );
		audio.QueueAudioSegment( "en", startIndex, count );
		
		double newPosition = pipelineContext.pipeline->GetPositionMilliseconds() / 1000.0;
		pipelineContext.pipeline->Flush( eMEDIATYPE_AUDIO, newPosition );
	}
	
	void LoadIframes( void )
	{
		Track &video = pipelineContext.track[eMEDIATYPE_VIDEO];
		int startIndex = 0;
		int count = 30;
		video.QueueVideoHeader( eVIDEORESOLUTION_IFRAME );
		video.QueueVideoSegment( eVIDEORESOLUTION_IFRAME, startIndex, count );
	}
	
	void LoadVideo( VideoResolution resolution )
	{
		Track &video = pipelineContext.track[eMEDIATYPE_VIDEO];
		int startIndex = 0;
		int count = AV_SEGMENT_LOAD_COUNT;
		video.QueueVideoHeader( resolution );
		video.QueueVideoSegment( resolution, startIndex, count );
	}
	
	void LoadAudio( const char *language )
	{
		Track &audio = pipelineContext.track[eMEDIATYPE_AUDIO];
		int startIndex = 0;
		int count = AV_SEGMENT_LOAD_COUNT;
		audio.QueueAudioHeader( language );
		audio.QueueAudioSegment( language, startIndex, count );
	}
	
	void FeedPipelineIfNeeded( MediaType mediaType )
	{
		Track &t = pipelineContext.track[mediaType];
		if( t.needsData )
		{
			auto queue = t.queue;
			if( queue->size()>0 )
			{
				auto buffer = queue->front();
				if( buffer->Inject(&pipelineContext,mediaType) )
				{
					queue->pop();
					delete buffer;
				}
			}
		}
	}
	
	void IdleFunc( void )
	{
		if( pipelineContext.pipeline )
		{
			if( logPositionChanges )
			{
				long long position = pipelineContext.pipeline->GetPositionMilliseconds();
				if( position>=0 && position != last_reported_position )
				{
					g_print( "position=%lld\n", position );
				}
				last_reported_position = position;
			}
			FeedPipelineIfNeeded( eMEDIATYPE_VIDEO );
			FeedPipelineIfNeeded( eMEDIATYPE_AUDIO );
			
			if(( autoStepCount>0 ) &&
			   ( ePIPELINESTATE_PAUSED == pipelineContext.pipeline->GetPipelineState() ))
			{
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
		printf( "load <vodurl> // play DASH manifest from start to end\n" );
		printf( "format mp4 // default; use/inject DASH .mp4 segments for tests\n" );
		printf( "format ts // use/inject whole HLS .ts segments for tests\n" );
		printf( "format es // demux HLS .ts segments, injecting elementary streams for tests\n" );
		printf( "position // toggle position reporting (default = off)\n" );
		
		printf( "abr // ramp from 360p up to 1080p then back down\n" );
		printf( "stream <from-position-seconds> <to-position-seconds>\n" );
		
		printf( "ff // ff (seek) iframe track\n" );
		printf( "ff2 // ff (rate) iframe track\n" );
		
		printf( "rew // rewind (seek) iframe track\n" );
		printf( "rew2 // rewind (rate) iframe track\n" );
		printf( "rew3 // rewind (step) iframe track\n" );
		
		printf( "ffads <step> <delay> // multi-period trick play test using real ad content\n");
		
		printf( "rate <newRate> // apply instantaneous rate change\n" );
		
		printf( "dai // multi-period test exercising discontinuity handling with EOS signaling and flush\n" );
		printf( "dai2 // optimized multi-period test using pts restamping\n" );
		
		printf( "gap <video> <audio> // play specified 4s gap bookended by 4s audio/video\n" );
		printf( "   // content - fill with normal video/audio\n" );
		printf( "   // event - use gstreamer gap event\n" );
		printf( "   // skip - skip injection; let decoder handle gap\n" );
		
		// manual content injection
		printf( "flush // flush av; prepare for playback from 0\n" );
		printf( "360 // load 360p video\n" );
		printf( "480 // load 480p video\n" );
		printf( "7200 // load 720p video\n" );
		printf( "1080 // load 1080p video\n" );
		printf( "en // load English audio\n" );
		printf( "fr // load French audio\n" );
		printf( "es // load Spanish audio\n" );
		
		// configure pipeline for audio and/or video playback
		printf( "video // configure video track\n" );
		printf( "audio // configure audio track\n" );
		
		// starting/pausing pipeline
		printf( "pause // pause playback\n" );
		printf( "play // resume/start playback\n" );
		printf( "null // reset pipeline\n" );
		printf( "stop // kill pipeline\n" );
		
		// misc post-tune commands
		printf( "step // step one frame at a time (while paused)\n" );
		printf( "sap // replace current audio with French\n" );
		printf( "sap2 // switch audio to English using seamless audio switching\n" );
		printf( "dump // generate gst-test.dot\n" );
		printf( "position // log position changes\n" );
		printf( "seek // Specify new position to start playback\n" );
		
		// misc commands
		printf( "path <base_path> // set the base path to the test stream data\n" );
		printf( "exit // exit test\n" );
	}
	
	void InjectSegments( const Timeline &timelineObj )
	{
		for( int iPeriod=0; iPeriod<timelineObj.period.size(); iPeriod++ )
		{
			const PeriodObj &period = timelineObj.period[iPeriod];
			auto timestampOffset = period.timestampOffset;
			if( iPeriod == 0 )
			{
				Flush( 1.0/*rate*/, 0/*start*/, -1/*stop*/, 0/*baseTime*/, timestampOffset );
			}
			else
			{
				for( int mediaType=0; mediaType<2; mediaType++ )
				{
					Track &t = pipelineContext.track[mediaType];
					t.EnqueueControl( new TrackNext(t.injectCount) );
					t.EnqueueControl( new TrackTimestampOffset(timestampOffset) );
				}
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
				else if( adaptationSet.contentType == "text" )
				{ // TODO
					continue;
				}
				else
				{ // unknown/unsupported track type
					assert(0);
				}
				auto representation = adaptationSet.representation[0];
				std::map<std::string,std::string> param;
				uint64_t number = representation.data.startNumber;
				auto len = representation.data.media.size();
				if( len==1 )
				{
					len = representation.data.duration.size();
					if( len==1 )
					{
						auto d = representation.data.duration[0]/representation.data.timescale;
						len = period.duration/d;
					}
				}
				param["RepresentationID"] = representation.id;
				
				std::string mediaUrl = representation.BaseURL + ExpandURL( representation.data.initialization, param );
				std::cout << mediaUrl << "\n";
				pipelineContext.track[mediaType].EnqueueSegment( new TrackFragment( mediaUrl.c_str() ) );
				
				for( unsigned idx=0; idx<len; idx++ )
				{
					int mediaIndex = idx;
					if( mediaIndex >= representation.data.media.size() )
					{
						mediaIndex = 0;
					}
					const std::string &media = representation.data.media[mediaIndex];
					param["Number"] = std::to_string( number++ );
					if( representation.data.time.size()>0 )
					{
						param["Time"] = std::to_string( representation.data.time[idx] );
					}
					mediaUrl = representation.BaseURL + ExpandURL( media, param );
					std::cout << mediaUrl << "\n";
					pipelineContext.track[mediaType].EnqueueSegment( new TrackFragment( mediaUrl.c_str() ) );
				}
			}
		}
		
		for( int mediaType=0; mediaType<2; mediaType++ )
		{
			Track &t = pipelineContext.track[mediaType];
			t.EnqueueControl( new TrackEOS() );
		}
		// configure pipelines and begin streaming
		pipelineContext.pipeline->Configure( eMEDIATYPE_VIDEO, GetCapsFormat() );
		pipelineContext.pipeline->Configure( eMEDIATYPE_AUDIO, GetCapsFormat() );
		pipelineContext.pipeline->SetPipelineState(ePIPELINESTATE_PLAYING);
	}

	void Load( const std::string &url )
	{
		size_t size = 0;
		auto ptr = LoadUrl( url, &size );
		if( ptr )
		{
			XmlNode *xml = new XmlNode( "document", (char *)ptr, size );
			auto MPD = xml->children[1];
			DumpXml(MPD,0);
			Timeline timeline = parseManifest( *MPD, url );
			timeline.Debug();
			ComputeTimestampOffsets( timeline );
			InjectSegments(timeline);
			delete xml;
			free( ptr );
		}
	}
	
	void TestMultiPeriodFF( void )
	{
		Flush();
		Track &video = pipelineContext.track[eMEDIATYPE_VIDEO];
		bool first = true;
		
		double fragmentDuration = 1.92;
		const char *representationID = "LE2.Trick";
		//const char *prefix = "file:///Users/pstrof200/Downloads/dai-test"; // use local file system
		const char *prefix = "https://"; // download content at runtime
		struct PeriodInfo
		{
			const char *baseUrl;
			int count;
		} mPeriodInfo[] =
		{ // https://cpetestutility.stb.r53.xcal.tv/VideoTestStream/public/aamptest/streams/ads/stitched/manifest.mpd
			{
				"hsar1022-soip-ads-prd.cdn01.skycdp.com/ads-gb-s8-prd-ak.cdn01.skycdp.com/v1/frag/bmff/t/ipvodad10/141865a9-d4cb-483f-a838-da28edd53ff2/1685615384261/AD/HD",
				5
			},
			{
				"ads-gb-s8-prd-ak.cdn01.skycdp.com/v1/frag/bmff/t/ipvodad17/dc004d50-30ea-4f46-add8-9a007fe7c8ec/1628085330949/AD/HD",
				16
			},
			{
				"ads-gb-s8-prd-ak.cdn01.skycdp.com/v1/frag/bmff/t/ipvodad18/a07dc735-36c2-4c0d-bf85-0d4f16bf7838/1687202233510/AD/HD",
				16
			},
			{
				"ads-gb-s8-prd-ak.cdn01.skycdp.com/v1/frag/bmff/t/ipvodad20/2887b05d-588b-4091-9bfe-4800c5acc957/1687192727494/AD/HD",
				5
			},
			{
				"ads-gb-s8-prd-ak.cdn01.skycdp.com/v1/frag/bmff/t/ipvodad13/5f7965ea-39cc-49db-945d-74a3c89ccd79/1687197133189/AD/HD",
				10
			},
			{
				"ads-gb-s8-prd-ak.cdn01.skycdp.com/v1/frag/bmff/t/ipvodad14/b9c4a503-b6e2-4994-a904-cabf5f463fb8/1687203165414/AD/HD",
				16
			},
			{
				"ads-gb-s8-prd-ak.cdn01.skycdp.com/v1/frag/bmff/t/ipvodad7/ed9e9eba-e818-413f-97ea-10cb3559ac31/1628085935274/AD/HD",
				21
			},
			{
				"ads-gb-s8-prd-ak.cdn01.skycdp.com/v1/frag/bmff/t/ipvodad17/dc004d50-30ea-4f46-add8-9a007fe7c8ec/1628085330949/AD/HD",
				16
			},
			{
				"ads-gb-s8-prd-ak.cdn01.skycdp.com/v1/frag/bmff/t/ipvodad1/7849033a-530a-43ce-ac01-fc4518674ed0/1628085609056/AD/HD",
				32
			},
			{
				"ads-gb-s8-prd-ak.cdn01.skycdp.com/v1/frag/bmff/t/ipvodad19/7b048ca3-6cf7-43c8-98a3-b91c09ed59bb/1628252309135/AD/HD",
				6
			},
			{
				"ads-gb-s8-prd-ak.cdn01.skycdp.com/v1/frag/bmff/t/ipvodad2/d14dff37-36d1-4850-aa9d-7d948cbf1fc6/1628318436178/AD/HD",
				11
			},
			{
				"ads-gb-s8-prd-ak.cdn01.skycdp.com/v1/frag/bmff/t/ipvodad20/ce5b8762-d14a-4f92-ba34-13d74e34d6ac/1628252375289/AD/HD",
				14
			},
			{
				"ads-gb-s8-prd-ll.cdn01.skycdp.com/v1/frag/bmff/t/ipvodad7/af35882d-c6fe-4244-8f83-c488cbe9cbcb/1648591567144/AD/HD",
				14
			}
		};
		for( int iPeriod=0; iPeriod<ARRAY_SIZE(mPeriodInfo); iPeriod++ )
		{
			const struct PeriodInfo *periodInfo = &mPeriodInfo[iPeriod];
			char path[256];
			snprintf( path, sizeof(path),
					 "%s/%s/manifest/track-iframe-repid-%s-tc--header.mp4",
					 prefix,
					 periodInfo->baseUrl,
					 representationID );
			video.EnqueueSegment( new TrackFragment( path ) );
			if( first )
			{ // configure pipeline
				pipelineContext.pipeline->Configure( eMEDIATYPE_VIDEO, GetCapsFormat() );
#ifdef REALTEK_HACK
				pipelineContext.pipeline->SetPipelineState(ePIPELINESTATE_PLAYING);
#else
				pipelineContext.pipeline->SetPipelineState(ePIPELINESTATE_PAUSED);
#endif
				first = false;
			}
			for( int fragmentNumber = 0; fragmentNumber<periodInfo->count; fragmentNumber+=m_ff_delta )
			{
				snprintf( path, sizeof(path),
						 "%s/%s/manifest/track-iframe-repid-%s-tc--frag-%d.mp4",
						 prefix,
						 periodInfo->baseUrl,
						 representationID,
						 fragmentNumber );
				double pts = fragmentNumber*fragmentDuration;
				video.EnqueueSegment( new TrackFragment( path ) ); // inject next iframe
#ifdef REALTEK_HACK
				video.EnqueueSegment( new TrackFragment( path ) ); // inject next iframe
#endif
				video.EnqueueControl( new TrackEOS() ); // inject EOS; needed for small segment to render
#ifdef REALTEK_HACK
				video.EnqueueControl( new TrackWaitState(ePIPELINESTATE_PLAYING) ); // wait for segment to be visible
#else
				video.EnqueueControl( new TrackWaitState(ePIPELINESTATE_PAUSED) ); // wait for segment to be visible
#endif
				video.EnqueueControl( new TrackSleep( m_ff_delay ) );
				video.EnqueueControl( new TrackFlush( 1, pts, -1, pts ) );
			}
		}
	}
	
	void ProcessCommand( const char *str )
	{
		double start = 0;
		double stop = -1;
		char videoGap[8] = "";
		char audioGap[8] = "";
		char format[8] = "";
		double newRate = 1.0;
		double seekPos = 0;
		
		if( strcmp(str,"help")==0 )
		{
			ShowHelp();
		}
		else if( starts_with(str,"load ") )
		{
			Load( &str[5] );
		}
		else if( sscanf(str,"format %7s",format)==1 )
		{
			if( strcmp(format,"mp4")==0 )
			{
				mContentFormat = eCONTENTFORMAT_MP4;
				printf( "format: mp4 (DASH segments)\n" );
			}
			else if( strcmp(format,"es")==0 )
			{
				mContentFormat = eCONTENTFORMAT_ES;
				printf( "format: es (HLS - demuxed elementary stream)\n" );
			}
			else if( strcmp(format,"ts")==0 )
			{
				mContentFormat = eCONTENTFORMAT_TS;
				printf( "format: ts (HLS - ts segments)\n" );
			}
			else
			{
				printf( "unk format: '%s' use one of: mp4 ts es\n", format );
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
		else if( sscanf(str,"ffads %d %d", &m_ff_delta, &m_ff_delay )==2 )
		{
			TestMultiPeriodFF();
		}
		else if( strcmp(str,"ff")==0 )
		{
			TestFF_Seek();
		}
		else if( strcmp(str,"ff2")==0 )
		{
			TestFF_Rate();
		}
		else if( strcmp(str,"rew")==0 )
		{
			TestREW_Seek();
		}
		else if( strcmp(str,"rew2")==0 )
		{
			TestREW_Rate();
		}
		else if( strcmp(str,"rew3")==0 )
		{
			TestREW_Step();
		}
		else if( sscanf(str,"rate %lf", &newRate )==1 )
		{
			pipelineContext.pipeline->InstantaneousRateChange(newRate);
		}
		else if( strcmp(str,"step")==0 )
		{
			pipelineContext.pipeline->Step();
		}
		else if( strcmp(str,"stream")==0 || sscanf(str,"stream %lf %lf", &start, &stop )>=1 )
		{
			TestStream( start, stop, "en" );
		}
		else if( strcmp(str,"sap")==0 )
		{
			TestSAP();
		}
		else if( strcmp(str,"en")==0 )
		{
			LoadAudio("en");
		}
		else if( strcmp(str,"fr")==0 )
		{
			LoadAudio("fr");
		}
		else if( strcmp(str,"es")==0 )
		{
			LoadAudio("es");
		}
		else if( strcmp(str,"iframe")==0 )
		{
			LoadIframes();
		}
		else if( strcmp(str,"dai")==0 )
		{
			TestDAI();
		}
		else if( strcmp(str,"dai2")==0 )
		{
			TestDAI2();
		}
		else if( strcmp(str,"abr")==0 )
		{
			TestABR();
		}
		else if( sscanf(str,"gap %7s %7s", videoGap, audioGap)==2 )
		{
			TestGap(videoGap, audioGap);
		}
		else if( strcmp(str,"flush")==0 )
		{
			Flush();
		}
		else if(
				strcmp(str,"360")==0 ||
				strcmp(str,"480")==0 ||
				strcmp(str,"720")==0 ||
				strcmp(str,"1080")==0 )
		{
			VideoResolution resolution = (VideoResolution)atoi(str);
			LoadVideo(resolution);
		}
		else if( strcmp(str,"ready")==0 )
		{
			pipelineContext.pipeline->SetPipelineState(ePIPELINESTATE_READY);
		}
		else if( strcmp(str,"pause")==0 )
		{
			pipelineContext.pipeline->SetPipelineState(ePIPELINESTATE_PAUSED);
		}
		else if( strcmp(str,"play")==0 )
		{
			pipelineContext.pipeline->SetPipelineState(ePIPELINESTATE_PLAYING);
		}
		else if( strcmp(str,"null")==0 )
		{
			pipelineContext.pipeline->SetPipelineState(ePIPELINESTATE_NULL);
		}
		else if( strcmp(str,"video")==0 )
		{
			pipelineContext.pipeline->Configure( eMEDIATYPE_VIDEO, GetCapsFormat() );
		}
		else if( strcmp(str,"audio")==0 )
		{
			pipelineContext.pipeline->Configure( eMEDIATYPE_AUDIO, GetCapsFormat() );
		}
		else if( sscanf(str, "path %199s", base_path ) == 1 )
		{
			assert(199 < sizeof(base_path));
		}
		else if( strcmp( str,"exit")==0 )
		{
			g_main_loop_quit(main_loop);
		}
		else if( strcmp(str,"sap2")==0 )
		{
			TestSeamlessAudioSwitch();
		}
		else if( sscanf(str,"seek %lf", &seekPos )==1 )
		{
			Test_Seek(seekPos);
		}
		else if( str[0] )
		{
			printf( "unk command: %s\n", str );
		}
	}
	
	//copy constructor
	AppContext(const AppContext&)=delete;
	//copy assignment operator
	AppContext& operator=(const AppContext&)=delete;
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

int trickplay_test_main(int argc, char **argv);

int main(int argc, char **argv)
{
	// programatically override gstreamer log level:
	// setenv( "GST_DEBUG", "*:8", 1 );
	// refer https://gstreamer.freedesktop.org/documentation/tutorials/basic/debugging-tools.html?gi-language=c
	
	gst_init(&argc, &argv);
	g_print( "gstreamer test harness\n" );
	
	struct AppContext appContext;
	
	GIOChannel *io_stdin = g_io_channel_unix_new (fileno (stdin));
	(void)g_io_add_watch (io_stdin, G_IO_IN, (GIOFunc) handle_keyboard, &appContext);
	(void)g_idle_add( myIdleFunc, (gpointer)&appContext );
	g_main_loop_run(appContext.main_loop);
	g_main_loop_unref(appContext.main_loop);
	
	return 0;
}
