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

#include <stdio.h>
#include <stdlib.h>
#include "unistd.h"
#include <assert.h>
#include <gst/gst.h>
#include "initializationheaderinfo.hpp"

// refer gstreamer pipeline unexpected behaviors when attempting to render single iframe
// #define REALTEK_HACK

typedef enum
{
	eMEDIATYPE_AUDIO,
	eMEDIATYPE_VIDEO,
	// aux audio
	// subtitle
	// ...
} MediaType;
#define NUM_MEDIA_TYPES 2

typedef enum
{ // 1-to-1 map to GstState
	ePIPELINE_STATE_NULL		= 1, // GST_STATE_NULL
	ePIPELINE_STATE_READY	= 2, // GST_STATE_READY
	ePIPELINE_STATE_PAUSED	= 3, // GST_STATE_PAUSED
	ePIPELINE_STATE_PLAYING	= 4, // GST_STATE_PLAYING
} PipelineState;

struct _GstElement;
struct _GstBus;
struct _GstMessage;

class PipelineContext
{
public:
	virtual ~PipelineContext(){};
	
	virtual void NeedData( MediaType mediaType ) = 0;
	virtual void EnoughData( MediaType mediaType ) = 0;
	virtual void ReachedEOS( void ) = 0;
	//virtual void PadProbeCallback( MediaType mediaType ) = 0;
};

class Pipeline
{
public:
	Pipeline( class PipelineContext *context );
	~Pipeline();
	
	double GetInjectedSeconds( MediaType mediaType );
	long long GetPositionMilliseconds( MediaType mediaType );
	void SetPipelineState( PipelineState );
	PipelineState GetPipelineState( void );
	void Configure( MediaType mediaType );
	void SetCaps( MediaType mediaType, InitializationHeaderInfo * );
	void Flush( double rate, double start, double stop, double baseTime );
	void Flush( MediaType type, double position );
	void InstantaneousRateChange( double newRate );
	void DumpDOT( void );
	void SendBufferMP4( MediaType mediaType, gpointer ptr, gsize len, double duration );
	void SendBufferES( MediaType mediaType, gpointer ptr, gsize len, double duration, double pts, double dts );
	void SendGap( MediaType mediaType, double pts, double base_time );
	void SendEOS( MediaType mediaType );
	void Step( void );

	Pipeline(const Pipeline&)=delete; //copy constructor
	Pipeline& operator=(const Pipeline&)=delete; //copy assignment operator
private:
	class PipelineContext *context;
	
	class MediaStream *mediaStream[NUM_MEDIA_TYPES];
	_GstElement *pipeline;

	double rate;
	gint64 start;
	gint64 stop;
	gint64 position_adjust;

	_GstBus *bus;
	gboolean bus_message( _GstBus * bus, _GstMessage * msg );
	GstBusSyncReply bus_sync_handler( _GstBus * bus, _GstMessage * msg );
	friend gboolean bus_message_cb(GstBus * bus, GstMessage * msg, class Pipeline *pipeline );
	friend GstBusSyncReply bus_sync_handler_cb(GstBus * bus, GstMessage * msg, Pipeline * pipeline );

	void HandleGstMsg( GstMessage *msg );
	void HandleGstMessageError( GstMessage *msg, const char *messageName );
	void HandleGstMessageWarning( GstMessage *msg, const char *messageName );
	void HandleGstMessageEOS( GstMessage *msg, const char *messageName );
	void HandleGstMessageStateChanged( GstMessage *msg, const char *messageName );
	void HandleGstMessageTag( GstMessage *msg, const char *messageName );
	void HandleGstMessageQOS( GstMessage *msg, const char *messageName );
};

#endif // GST_PORT_H
