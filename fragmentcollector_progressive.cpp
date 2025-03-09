/*
 * If not stated otherwise in this file or this component's license file the
 * following copyright and licenses apply:
 *
 * Copyright 2018 RDK Management
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

/**
 * @file fragmentcollector_progressive.cpp
 * @brief Streamer for progressive mp3/mp4 playback
 */

#include "fragmentcollector_progressive.h"
#include "priv_aamp.h"
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <assert.h>
#include "AampCurlStore.h"

/**
 * @struct StreamWriteCallbackContext
 * @brief Write call back functions for streamer
 */

struct StreamWriteCallbackContext
{
    bool sentTunedEvent;
    PrivateInstanceAAMP *aamp;
    StreamWriteCallbackContext() : aamp(NULL), sentTunedEvent(false)
    {
    }
};

/**
 TODO: consider harvesting fixed size chunks instead of immediately tiny ranges of bytes to gstreamer
 TODO: consider config for required bytes to collect before starting gstreamer pipeline
 TODO: if we can't keep up with required bandwidth we don't have luxury of ABR ramp down; need to inform app about buffering status
 TODO: consider config for required bytes to collect after buffer runs dry before updating state
 TODO: consider harvesting additional configurable buffer of bytes even while throttled so they are "on deck" and ready to inject

 TODO: pause/play testing
 
 TODO: we are not yet reporting a valid duration, either for entire stream or for each injected chunk
 is there a nice way to get real mp4 file rate from initial headers?
 if we can't determine mp4 logical duration, can/should we report -1 as length?

 TODO: progress reporting/testing
 TODO: what to return for available bitrates?
 TODO: FF/REW should be disabled, with out any available rates
 TODO: trickplay requests must return error

 TODO: errors that can occur at tune time or mid-stream
 TODO: extend StreamFile with applicable features from aamp->GetFile
 TODO: profiling - stream based, not fragment based
 */

/**
 * @fn StreamWriteCallback
 * @param ptr
 * @param size always 1, per curl documentation
 * @param nmemb number of bytes advertised in this callback
 * @param userdata app-specific context
 */
static size_t StreamWriteCallback( void *ptr, size_t size, size_t nmemb, void *userdata )
{
    StreamWriteCallbackContext *context = (StreamWriteCallbackContext *)userdata;
    struct PrivateInstanceAAMP *aamp = context->aamp;
    if( context->aamp->mDownloadsEnabled)
    {
       // TODO: info logging is normally only done up until first frame rendered, but even so is too noisy for below, since CURL write callback yields many small chunks
		AAMPLOG_INFO("StreamWriteCallback(%zu bytes)", nmemb);
        // throttle download speed if gstreamer isn't hungry
        aamp->BlockUntilGstreamerWantsData( NULL/*CB*/, 0.0/*periodMs*/, eMEDIATYPE_VIDEO );
        double fpts = 0.0;
        double fdts = 0.0;
        double fDuration = 2.0; // HACK!  //CID:113073 - Position variable initialized but not used
        if( nmemb>0 )
        {
           aamp->SendStreamCopy( eMEDIATYPE_VIDEO, ptr, nmemb, fpts, fdts, fDuration);
           if( !context->sentTunedEvent )
           { // send TunedEvent after first chunk injected - this is hint for XRE to hide the "tuning overcard"
               aamp->SendTunedEvent(false);
               context->sentTunedEvent = true;
           }
       }
   }
   else
   {
       AAMPLOG_WARN("write_callback - interrupted");
       nmemb = 0;
   }
   return nmemb;
}


void StreamAbstractionAAMP_PROGRESSIVE::StreamFile( const char *uri, int *http_error )
{ // TODO: move to main_aamp
	int http_code = -1;
	AAMPLOG_INFO("StreamFile: %s", uri );
	CURL *curl = curl_easy_init();
	if (curl)
	{
		StreamWriteCallbackContext context;
		context.aamp = aamp;
		context.sentTunedEvent = false;
		CURL_EASY_SETOPT_FUNC(curl, CURLOPT_WRITEFUNCTION, StreamWriteCallback );
		CURL_EASY_SETOPT_POINTER(curl, CURLOPT_WRITEDATA, (void *)&context );
		CURL_EASY_SETOPT_STRING(curl, CURLOPT_URL, uri );
		CURL_EASY_SETOPT_STRING(curl, CURLOPT_USERAGENT, "aamp-progressive/1.0"); // TODO: use same user agent string normally used by AAMP
		CURLcode res = curl_easy_perform(curl); // synchronous; callbacks allow interruption
		if( res == CURLE_OK)
		{ // all data collected
			http_code = GetCurlResponseCode(curl);
		}
		if (http_error)
		{
			*http_error = http_code;
		}
		curl_easy_cleanup(curl);
	}
}

/**
 * @brief harvest chunks from large mp3/mp4
 */
void StreamAbstractionAAMP_PROGRESSIVE::FetcherLoop()
{
    std::string contentUrl = aamp->GetManifestUrl();
    std::string effectiveUrl;
    int http_error;
    
    if(ISCONFIGSET(eAAMPConfig_UseAppSrcForProgressivePlayback))
    {
	    StreamFile( contentUrl.c_str(), &http_error );
    }
    else
    {
	    // send TunedEvent after first chunk injected - this is hint for XRE to hide the "tuning overcard"
	    aamp->SendTunedEvent(false);
    }

    while( aamp->DownloadsAreEnabled() )
    {
        aamp->interruptibleMsSleep( 1000 );
    }
}

/**
 * @brief Fragment collector thread
 * @param arg Pointer to StreamAbstractionAAMP_PROGRESSIVE object
 * @retval void
 */
void StreamAbstractionAAMP_PROGRESSIVE::FragmentCollector(void)
{
    if(aamp_pthread_setname(pthread_self(), "aampPSFetcher"))
    {
        AAMPLOG_WARN("aamp_pthread_setname failed");
    }
    FetcherLoop();
    return;
}


/**
 *  @brief  Initialize a newly created object.
 */
AAMPStatusType StreamAbstractionAAMP_PROGRESSIVE::Init(TuneType tuneType)
{
    AAMPStatusType retval = eAAMPSTATUS_OK;
    aamp->CurlInit(eCURLINSTANCE_VIDEO, AAMP_TRACK_COUNT,aamp->GetNetworkProxy());  //CID:110904 - newTune bool variable  initialized not used
    aamp->IsTuneTypeNew = false;
    std::set<std::string> mLangList; /**< empty language list */
    std::vector<BitsPerSecond> bitrates; /**< empty bitrates */
    for (int i = 0; i < AAMP_TRACK_COUNT; i++)
    {
        aamp->SetCurlTimeout(aamp->mNetworkTimeoutMs, (AampCurlInstance) i);
    }
    aamp->SendMediaMetadataEvent();
    return retval;
}


/*
AAMPStatusType StreamAbstractionAAMP_PROGRESSIVE::Init(TuneType tuneType)
{
    AAMPStatusType retval = eAAMPSTATUS_OK;
    bool newTune = aamp->IsNewTune();
    aamp->IsTuneTypeNew = false;
    return retval;
}
 */

/**
 * @brief StreamAbstractionAAMP_PROGRESSIVE Constructor
 */
StreamAbstractionAAMP_PROGRESSIVE::StreamAbstractionAAMP_PROGRESSIVE(class PrivateInstanceAAMP *aamp,double seek_pos, float rate): StreamAbstractionAAMP(aamp),
fragmentCollectorThreadStarted(false), fragmentCollectorThreadID(), seekPosition(seek_pos)
{
    trickplayMode = (rate != AAMP_NORMAL_PLAY_RATE);
}

/**
 * @brief StreamAbstractionAAMP_PROGRESSIVE Destructor
 */
StreamAbstractionAAMP_PROGRESSIVE::~StreamAbstractionAAMP_PROGRESSIVE()
{
}

/**
 * @brief  Starts streaming.
 */
void StreamAbstractionAAMP_PROGRESSIVE::Start(void)
{
    try
    {
        fragmentCollectorThreadID = std::thread(&StreamAbstractionAAMP_PROGRESSIVE::FragmentCollector, this);
        fragmentCollectorThreadStarted = true;
        AAMPLOG_INFO("Thread created for FragmentCollector [%zx]", GetPrintableThreadID(fragmentCollectorThreadID));
    }
    catch(const std::exception& e)
    {
        AAMPLOG_ERR("Failed to create FragmentCollector thread : %s", e.what());
    }
    
}

/**
 * @brief  Stops streaming.
 */
void StreamAbstractionAAMP_PROGRESSIVE::Stop(bool clearChannelData)
{
    if(fragmentCollectorThreadStarted)
    {
        aamp->DisableDownloads();
        fragmentCollectorThreadID.join();
        fragmentCollectorThreadStarted = false;
        aamp->EnableDownloads();
    }
 }

/**
 * @brief Get output format of stream.
 */
void StreamAbstractionAAMP_PROGRESSIVE::GetStreamFormat(StreamOutputFormat &primaryOutputFormat, StreamOutputFormat &audioOutputFormat, StreamOutputFormat &auxAudioOutputFormat, StreamOutputFormat &subtitleOutputFormat)
{
    primaryOutputFormat = FORMAT_ISO_BMFF;
    audioOutputFormat = FORMAT_INVALID;
    auxAudioOutputFormat = FORMAT_INVALID;
    subtitleOutputFormat = FORMAT_INVALID;
}

/**
 *  @brief Get current stream position.
 */
double StreamAbstractionAAMP_PROGRESSIVE::GetStreamPosition()
{
    return seekPosition;
}

/**
 *  @brief  Get PTS of first sample.
 */
double StreamAbstractionAAMP_PROGRESSIVE::GetFirstPTS()
{
    return 0.0;
}

/**
 *  @brief check whether initial caching data supported
 *
 */
bool StreamAbstractionAAMP_PROGRESSIVE::IsInitialCachingSupported()
{
	return false;
}

/**
 *  @brief Gets Max Bitrate available for current playback.
 */
BitsPerSecond StreamAbstractionAAMP_PROGRESSIVE::GetMaxBitrate()
{ // STUB
    return 0;
}

