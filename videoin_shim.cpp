/*
 * If not stated otherwise in this file or this component's license file the
 * following copyright and licenses apply:
 *
 * Copyright 2021 RDK Management
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
 * @file videoin_shim.cpp
 * @brief shim for dispatching UVE HDMI input playback
 */
#include "videoin_shim.h"
#include "priv_aamp.h"
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <signal.h>
#include <assert.h>
#include "AampUtils.h"


#ifdef USE_CPP_THUNDER_PLUGIN_ACCESS

#include <core/core.h>
#include <websocket/websocket.h>

using namespace std;
using namespace WPEFramework;

#define RDKSHELL_CALLSIGN "org.rdk.RDKShell.1"
#define DS_CALLSIGN "org.rdk.DisplaySettings.1"


#endif

std::mutex StreamAbstractionAAMP_VIDEOIN::mEvtMutex;
/**
 *  @brief  Initialize a newly created object.
 */
AAMPStatusType StreamAbstractionAAMP_VIDEOIN::Init(TuneType tuneType)
{
	AAMPLOG_WARN("%s Function not implemented",mName.c_str());
	return eAAMPSTATUS_OK;
}


AAMPStatusType StreamAbstractionAAMP_VIDEOIN::InitHelper(TuneType tuneType)
{
	AAMPStatusType retval = eAAMPSTATUS_OK;
	if(false == mIsInitialized)
	{
#ifdef USE_CPP_THUNDER_PLUGIN_ACCESS
        RegisterAllEvents();
#endif
		mIsInitialized = true;
	}
	return retval;
}

/**
 * @brief StreamAbstractionAAMP_VIDEOIN Constructor
 */
StreamAbstractionAAMP_VIDEOIN::StreamAbstractionAAMP_VIDEOIN( const std::string name, const std::string callSign,  class PrivateInstanceAAMP *aamp,double seek_pos, float rate, const std::string type)
                               : mName(name),
                               mRegisteredEvents(),
                               StreamAbstractionAAMP(aamp),
                               mTuned(false),
                               videoInputPort(-1),
                               videoInputType(type),
				mIsInitialized(false)
#ifdef USE_CPP_THUNDER_PLUGIN_ACCESS
                                ,thunderAccessObj(callSign),
				thunderRDKShellObj(RDKSHELL_CALLSIGN),
				thunderDSAccessObj(DS_CALLSIGN)
#endif
{
#ifdef USE_CPP_THUNDER_PLUGIN_ACCESS
	AAMPLOG_WARN("%s Constructor",mName.c_str());
    thunderAccessObj.ActivatePlugin();
#endif
}

/**
 *  @brief StreamAbstractionAAMP_VIDEOIN Destructor
 */
StreamAbstractionAAMP_VIDEOIN::~StreamAbstractionAAMP_VIDEOIN()
{
	AAMPLOG_WARN("%s destructor",mName.c_str());
#ifdef USE_CPP_THUNDER_PLUGIN_ACCESS
	for (auto const& evtName : mRegisteredEvents) {
		thunderAccessObj.UnSubscribeEvent(_T(evtName));
	}
#endif
	mRegisteredEvents.clear();
}

/**
 *  @brief  Starts streaming.
 */
void StreamAbstractionAAMP_VIDEOIN::Start(void)
{
	AAMPLOG_WARN("%s Function not implemented",mName.c_str());
}

/**
 *  @brief  calls start on video in specified by port and method name
 */
void StreamAbstractionAAMP_VIDEOIN::StartHelper(int port)
{
	AAMPLOG_WARN("%s port:%d",mName.c_str(),port);

	videoInputPort = port;
#ifdef USE_CPP_THUNDER_PLUGIN_ACCESS
		JsonObject param;
		JsonObject result;
		const std::string & methodName = "startInput";
		param["portId"] = videoInputPort;
		param["typeOfInput"] = videoInputType;
		thunderAccessObj.InvokeJSONRPC(methodName, param, result);
#endif
}

/**
 *  @brief  Stops streaming.
 */
void StreamAbstractionAAMP_VIDEOIN::StopHelper()
{
	if( videoInputPort>=0 )
	{
		AAMPLOG_WARN("%s ",mName.c_str());

#ifdef USE_CPP_THUNDER_PLUGIN_ACCESS
    		JsonObject param;
    		JsonObject result;
			const std::string methodName = "stopInput";
			param["typeOfInput"] = videoInputType;
    		thunderAccessObj.InvokeJSONRPC(methodName, param, result);
#endif

		videoInputPort = -1;
	}
}

/**
 *  @brief Stops streaming.
 */
void StreamAbstractionAAMP_VIDEOIN::Stop(bool clearChannelData)
{
	AAMPLOG_WARN("%s Function not implemented",mName.c_str());
}

bool StreamAbstractionAAMP_VIDEOIN::GetResolutionFromDS(int & widthFromDS, int & heightFromDS)
{
#ifndef USE_CPP_THUNDER_PLUGIN_ACCESS
	return false;
#else
	JsonObject param;
	JsonObject result;
	bool bRetVal = false;

	if( thunderDSAccessObj.InvokeJSONRPC("getCurrentResolution", param, result) )
	{
		widthFromDS = result["w"].Number();
		heightFromDS = result["h"].Number();
		AAMPLOG_INFO("%s widthFromDS:%d heightFromDS:%d ",mName.c_str(),widthFromDS, heightFromDS);
		bRetVal = true;
	}
	return bRetVal;
#endif
}

bool StreamAbstractionAAMP_VIDEOIN::GetScreenResolution(int & screenWidth, int & screenHeight)
{
#ifndef USE_CPP_THUNDER_PLUGIN_ACCESS
	return false;
#else
	JsonObject param;
	JsonObject result;
	bool bRetVal = false;

	if( thunderRDKShellObj.InvokeJSONRPC("getScreenResolution", param, result) )
	{
		screenWidth = result["w"].Number();
		screenHeight = result["h"].Number();
		AAMPLOG_INFO("%s screenWidth:%d screenHeight:%d ",mName.c_str(),screenWidth, screenHeight);
		bRetVal = true;
	}
	return bRetVal;
#endif
}

/**
 *  @brief SetVideoRectangle sets the position coordinates (x,y) & size (w,h)
 */
void StreamAbstractionAAMP_VIDEOIN::SetVideoRectangle(int x, int y, int w, int h)
{
#ifdef USE_CPP_THUNDER_PLUGIN_ACCESS
	int screenWidth = 0;
	int screenHeight = 0;
	int widthFromDS = 0;
	int heightFromDS = 0;
	float width_ratio = 1.0, height_ratio = 1.0;
	if(GetScreenResolution(screenWidth,screenHeight) && GetResolutionFromDS(widthFromDS,heightFromDS))
    {
		if((0 != screenWidth) && (0 != screenHeight))
		{
			width_ratio = (float)widthFromDS /(float) screenWidth;
			height_ratio =(float) heightFromDS / (float) screenHeight;
			AAMPLOG_INFO("screenWidth:%d screenHeight:%d widthFromDS:%d heightFromDS:%d width_ratio:%f height_ratio:%f",screenWidth,screenHeight,widthFromDS,heightFromDS,width_ratio,height_ratio);
		}
	}

	JsonObject param;
	JsonObject result;
	param["x"] = (int) (x * width_ratio);
	param["y"] = (int) (y * height_ratio);
	param["w"] = (int) (w * width_ratio);
	param["h"] = (int) (h * height_ratio);
	param["typeOfInput"] = videoInputType;
	AAMPLOG_WARN("%s type:%s x:%d y:%d w:%d h:%d w-ratio:%f h-ratio:%f",mName.c_str(),videoInputType.c_str(),x,y,w,h,width_ratio,height_ratio);
	thunderAccessObj.InvokeJSONRPC("setVideoRectangle", param, result);
#endif
}

/**
 * @brief Get output format of stream.
 *
 */
void StreamAbstractionAAMP_VIDEOIN::GetStreamFormat(StreamOutputFormat &primaryOutputFormat, StreamOutputFormat &audioOutputFormat, StreamOutputFormat &auxAudioOutputFormat, StreamOutputFormat &subtitleOutputFormat)
{ // STUB
	AAMPLOG_WARN("%s ",mName.c_str());
    primaryOutputFormat = FORMAT_INVALID;
    audioOutputFormat = FORMAT_INVALID;
    //auxAudioOutputFormat = FORMAT_INVALID;
}

/**
 *  @brief  Get PTS of first sample.
 */
double StreamAbstractionAAMP_VIDEOIN::GetFirstPTS()
{ // STUB
	AAMPLOG_WARN("%s ",mName.c_str());
    return 0.0;
}

/**
 * @brief Check if Initial caching is supported
 */
bool StreamAbstractionAAMP_VIDEOIN::IsInitialCachingSupported()
{
	AAMPLOG_WARN("%s ",mName.c_str());
	return false;
}

/**
 *  @brief Gets Max Bitrate avialable for current playback.
 */
BitsPerSecond StreamAbstractionAAMP_VIDEOIN::GetMaxBitrate()
{ // STUB
	AAMPLOG_WARN("%s ",mName.c_str());
    return 0;
}


#ifdef USE_CPP_THUNDER_PLUGIN_ACCESS

/**
 *  @brief  Registers  Event to input plugin and to mRegisteredEvents list for later use.
 */
void StreamAbstractionAAMP_VIDEOIN::RegisterEvent (string eventName, std::function<void(const WPEFramework::Core::JSON::VariantContainer&)> functionHandler)
{
	bool bSubscribed;
	bSubscribed = thunderAccessObj.SubscribeEvent(_T(eventName), functionHandler);
	if(bSubscribed)
	{
		mRegisteredEvents.push_back(eventName);
	}
}

/**
 *  @brief  Registers all Events to input plugin
 */
void StreamAbstractionAAMP_VIDEOIN::RegisterAllEvents ()
{
	std::function<void(const WPEFramework::Core::JSON::VariantContainer&)> inputStatusChangedMethod = std::bind(&StreamAbstractionAAMP_VIDEOIN::OnInputStatusChanged, this, std::placeholders::_1);

	RegisterEvent("onInputStatusChanged",inputStatusChangedMethod);

	std::function<void(const WPEFramework::Core::JSON::VariantContainer&)> signalChangedMethod = std::bind(&StreamAbstractionAAMP_VIDEOIN::OnSignalChanged, this, std::placeholders::_1);

	RegisterEvent("onSignalChanged",signalChangedMethod);
}


/**
 *  @brief  Gets  onSignalChanged and translates into aamp events
 */
void StreamAbstractionAAMP_VIDEOIN::OnInputStatusChanged(const JsonObject& parameters)
{
	std::lock_guard<std::mutex>lock(mEvtMutex);
	if(NULL != aamp)
	{
		std::string message;
		parameters.ToString(message);
		AAMPLOG_WARN("%s",message.c_str());

		std::string strStatus = parameters["status"].String();

		if(0 == strStatus.compare("started"))
		{
			if(!mTuned){
				aamp->SendTunedEvent(false);
				mTuned = true;
				aamp->LogFirstFrame();
				aamp->LogTuneComplete();
			}
			aamp->SetState(eSTATE_PLAYING);
		}
		else if(0 == strStatus.compare("stopped"))
		{
			aamp->SetState(eSTATE_STOPPED);
		}
	}
}

/** 
 *  @brief  Gets  onSignalChanged and translates into aamp events
 */
void StreamAbstractionAAMP_VIDEOIN::OnSignalChanged (const JsonObject& parameters)
{
	std::lock_guard<std::mutex>lock(mEvtMutex);
	if(NULL != aamp)
	{
		std::string message;
		parameters.ToString(message);
		AAMPLOG_WARN("%s",message.c_str());

		std::string strReason;
		std::string strStatus = parameters["signalStatus"].String();

		if(0 == strStatus.compare("noSignal"))
		{
			strReason = "NO_SIGNAL";
		}
		else if (0 == strStatus.compare("unstableSignal"))
		{
			strReason = "UNSTABLE_SIGNAL";
		}
		else if (0 == strStatus.compare("notSupportedSignal"))
		{
			strReason = "NOT_SUPPORTED_SIGNAL";
		}
		else if (0 == strStatus.compare("stableSignal"))
		{
			// Only Generate after started event, this can come after temp loss of signal.
			if(mTuned){
				aamp->SetState(eSTATE_PLAYING);
			}
		}

		if(!strReason.empty())
		{
			AAMPLOG_WARN("GENERATING BLOCKED EVNET :%s",strReason.c_str());
			aamp->SendBlockedEvent(strReason);
		}
	}
}
#endif

