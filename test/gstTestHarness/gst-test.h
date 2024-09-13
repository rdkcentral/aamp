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
#include <queue>

typedef enum
{
	eVIDEORESOLUTION_IFRAME = 0,
	eVIDEORESOLUTION_360P = 360,
	eVIDEORESOLUTION_480P = 480,
	eVIDEORESOLUTION_720P = 720,
	eVIDEORESOLUTION_1080P = 1080
} VideoResolution;

/**
 * @brief application managed track abstraction
 * @todo this could be pushed lower into gst_port.cpp, but for not it's a layer above gstreamer abstraction
 */
class Track
{
public:
	uint32_t timeScale;
	int injectCount;
	bool needsData;
	bool gstreamerReadyForInjection;
	std::queue<class TrackEvent *> *queue; // sequential segments/commands, not yet injected
	
	Track();
	~Track();
	void Flush( void );
	void EnqueueSegment( TrackEvent *TrackEvent );
	void EnqueueControl( TrackEvent *TrackEvent );
	void QueueVideoHeader( VideoResolution resolution );
	void QueueVideoSegment( VideoResolution resolution, int startIndex, int count, double pts_offset=0.0 );
	void QueueAudioHeader( const char *language );
	void QueueAudioSegment( const char *language, int startIndex, int count, double pts_offset=0.0 );
	void QueueGap( int startIndex, int count );
	Track(const Track&)=delete;
	Track& operator=(const Track&)=delete;
};

class MyPipelineContext : PipelineContext
{
public:
	class Pipeline *pipeline;
	double seekPos;
	int numPendingEOS;
	double nextPTS;
	double nextTime;
	Track track[NUM_MEDIA_TYPES];
	MyPipelineContext( void );
	~MyPipelineContext();
	void ReachedEOS( void );
	void NeedData( MediaType mediaType );
	void EnoughData( MediaType mediaType );
	void PadProbeCallback( MediaType mediaType );
	
	MyPipelineContext(const MyPipelineContext&)=delete;
	MyPipelineContext& operator=(const MyPipelineContext&)=delete;
};

class TrackEvent
{
public:
	TrackEvent(){}
	virtual ~TrackEvent(){};
	virtual bool Inject( MyPipelineContext *context, MediaType mediaType ) = 0;
};
