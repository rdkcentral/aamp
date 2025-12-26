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
#ifndef GST_PORT_H
#define GST_PORT_H

#include "gst-utils.h"
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <assert.h>
#include <gst/gst.h>
#include <queue>
#include "mp4demux.hpp"

typedef enum
{ // 1-to-1 map to GstState
	ePIPELINE_STATE_NULL	= 1, // GST_STATE_NULL
	ePIPELINE_STATE_READY	= 2, // GST_STATE_READY
	ePIPELINE_STATE_PAUSED	= 3, // GST_STATE_PAUSED
	ePIPELINE_STATE_PLAYING	= 4, // GST_STATE_PLAYING
} PipelineState;

struct SeekParam {
	bool flush = false;
	bool segment = false;
	double start_seconds = 0.0;
	double stop_seconds = 0.0; // pass stop_seconds == start_seconds for open-ended segment
	double playback_rate = 1.0;
};

class PipelineContext
{
public:
	PipelineContext() : found_count() {}
	virtual ~PipelineContext(){};
	virtual void NeedData( MediaType mediaType ) = 0;
	virtual void EnoughData( MediaType mediaType ) = 0;
	
	// discovered via deep-notify::source
	virtual void OnAppsrcReady(const SeekParam&) = 0;
	
	/**
	 * This could/should be better abstracted, but the way it works is:
	 * 1. belated lazy seek done as each appsrc is first connected (MediaStream::found_source)
	 * 2. when Pipeline::ReachedEOS signaled, new seek done on pipeline to prepare for next segment
	 */
	mutable std::mutex segment_seek_mutex;
	std::queue<SeekParam> mSegmentEndSeekQueue;
	int found_count;
};

class Pipeline
{
public:
	Pipeline( class PipelineContext *context );
	~Pipeline();
	Pipeline(const Pipeline&)=delete; //copy constructor
	Pipeline& operator=(const Pipeline&)=delete; //copy assignment operator
	
	double GetInjectedSeconds( MediaType mediaType ) const;
	long long GetPositionMilliseconds( MediaType mediaType ) const;
	void SetPipelineState( PipelineState );
	PipelineState GetPipelineState( void ) const;
	void Configure( MediaType mediaType );
	void SetCaps( MediaType mediaType, const Mp4Demux *mp4Demux );
	void InstantaneousRateChange( double newRate );
	void DumpDOT( void ) const;
	void SendBufferMP4( MediaType mediaType, gpointer ptr, gsize len, double duration );
	void SendBufferES( MediaType mediaType, gpointer ptr, gsize len, double duration, double pts, double dts, GstStructure *metadata = NULL );
	void SendGap( MediaType mediaType, double pts, double base_time );
	void SendEOS( MediaType mediaType );
	void Step( void );
	void ScheduleSeek( const SeekParam & );
	size_t GetNumPendingSeek(void) const;
	bool DoSeekNow(const SeekParam & );
	void Reset( void );
	
private:
	void Seek( MediaType mediaType, const SeekParam &param );
	void ReachedEOS( void );
	class PipelineContext *context;
	class MediaStream *mediaStream[NUM_MEDIA_TYPES];
	GstElement *pipeline;
	GstBus *bus;
	gboolean bus_message( GstBus * bus, GstMessage * msg );
	friend gboolean bus_message_cb(GstBus * bus, GstMessage * msg, class Pipeline *pipeline );
	
	void HandleGstMessageError( GstMessage *msg, const char *messageName );
	void HandleGstMessageWarning( GstMessage *msg, const char *messageName );
	void HandleGstMessageEOS( GstMessage *msg, const char *messageName );
	void HandleGstMessageSegmentDone( GstMessage *msg, const char *messageName );
	void HandleGstMessageStateChanged( GstMessage *msg, const char *messageName );
	void HandleGstMessageStreamStatus( GstMessage *msg, const char *messageName );
	void HandleGstMessageTag( GstMessage *msg, const char *messageName );
	void HandleGstMessageQOS( GstMessage *msg, const char *messageName );
};

#endif // GST_PORT_H
