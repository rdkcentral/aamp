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
#include <signal.h>
#include <assert.h>
#include "AampUtils.h"

using namespace std;

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
		std::function<void(std::string)> OnInputStatusChangedCb = bind(&StreamAbstractionAAMP_VIDEOIN::OnInputStatusChanged, this, placeholders::_1);
		std::function<void(std::string)> OnSignalChangedCb = bind(&StreamAbstractionAAMP_VIDEOIN::OnSignalChanged, this, placeholders::_1);
		thunderAccessObj.RegisterAllEventsVideoin(OnInputStatusChangedCb, OnSignalChangedCb);
		mIsInitialized = true;
	}
	return retval;
}

/**
 * @brief StreamAbstractionAAMP_VIDEOIN Constructor
 */
StreamAbstractionAAMP_VIDEOIN::StreamAbstractionAAMP_VIDEOIN( const std::string name, PlayerThunderAccessPlugin callSign,  class PrivateInstanceAAMP *aamp,double seek_pos, float rate, const std::string type)
                               : mName(name),
                               StreamAbstractionAAMP(aamp),
                               mTuned(false),
                               videoInputType(type),
				mIsInitialized(false)
                                ,thunderAccessObj(callSign)
{
	AAMPLOG_WARN("%s Constructor",mName.c_str());
    thunderAccessObj.ActivatePlugin();
}

/**
 *  @brief StreamAbstractionAAMP_VIDEOIN Destructor
 */
StreamAbstractionAAMP_VIDEOIN::~StreamAbstractionAAMP_VIDEOIN()
{
	AAMPLOG_WARN("%s destructor",mName.c_str());
	thunderAccessObj.UnRegisterAllEventsVideoin();
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
	thunderAccessObj.StartHelperVideoin(port, videoInputType);
}

/**
 *  @brief  Stops streaming.
 */
void StreamAbstractionAAMP_VIDEOIN::StopHelper()
{
	thunderAccessObj.StopHelperVideoin(videoInputType);
}

/**
 *  @brief Stops streaming.
 */
void StreamAbstractionAAMP_VIDEOIN::Stop(bool clearChannelData)
{
	AAMPLOG_WARN("%s Function not implemented",mName.c_str());
}

/**
 *  @brief SetVideoRectangle sets the position coordinates (x,y) & size (w,h)
 */
void StreamAbstractionAAMP_VIDEOIN::SetVideoRectangle(int x, int y, int w, int h)
{
	thunderAccessObj.SetVideoRectangle(x, y, w, h, videoInputType, PlayerThunderAccessShim::VIDEOIN_SHIM);
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
 *  @brief Gets Max Bitrate available for current playback.
 */
BitsPerSecond StreamAbstractionAAMP_VIDEOIN::GetMaxBitrate()
{ // STUB
	AAMPLOG_WARN("%s ",mName.c_str());
    return 0;
}

/**
 *  @brief  Gets  onSignalChanged and translates into aamp events
 */
void StreamAbstractionAAMP_VIDEOIN::OnInputStatusChanged(std::string strStatus)
{
	std::lock_guard<std::mutex>lock(mEvtMutex);
	if(NULL != aamp)
	{
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
void StreamAbstractionAAMP_VIDEOIN::OnSignalChanged (std::string strStatus)
{
	std::lock_guard<std::mutex>lock(mEvtMutex);
	if(NULL != aamp)
	{
		std::string strReason;

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

