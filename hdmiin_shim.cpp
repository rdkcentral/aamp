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
 * @file hdmiin_shim.cpp
 * @brief shim for dispatching UVE HDMI input playback
 */
#include "hdmiin_shim.h"
#include "priv_aamp.h"
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <assert.h>
#include "AampUtils.h"

/**
* AVInput thunder plugin reference: https://rdkcentral.github.io/rdkservices/#/api/AVInputPlugin
*/

StreamAbstractionAAMP_HDMIIN* StreamAbstractionAAMP_HDMIIN::mHdmiinInstance = NULL;

/**
 * @brief StreamAbstractionAAMP_HDMIIN Constructor
 */
StreamAbstractionAAMP_HDMIIN::StreamAbstractionAAMP_HDMIIN(class PrivateInstanceAAMP *aamp,double seek_pos, float rate)
                             : StreamAbstractionAAMP_VIDEOIN("HDMIIN", PlayerThunderAccessPlugin::AVINPUT,aamp,seek_pos,rate,"HDMI")
{
	aamp->SetContentType("HDMI_IN");
}
		   
/**
 * @brief StreamAbstractionAAMP_HDMIIN Destructor
 */
StreamAbstractionAAMP_HDMIIN::~StreamAbstractionAAMP_HDMIIN()
{
	AAMPLOG_WARN("destructor ");
	mHdmiinInstance = NULL;
}

/**
 *   @brief  Initialize a newly created object.
 */
AAMPStatusType StreamAbstractionAAMP_HDMIIN::Init(TuneType tuneType)
{
	AAMPStatusType retval = eAAMPSTATUS_OK;
	if(false == mIsInitialized)
	{
		retval = InitHelper(tuneType);

		std::function<void(PlayerVideoStreamInfoData)> videoInfoUpdatedMethodCb = std::bind(&StreamAbstractionAAMP_HDMIIN::OnVideoStreamInfoUpdate, this, std::placeholders::_1);
		thunderAccessObj.RegisterEventOnVideoStreamInfoUpdateHdmiin(videoInfoUpdatedMethodCb);
	}
	return retval;
}

/**
 *   @brief  Starts streaming.
 */
void StreamAbstractionAAMP_HDMIIN::Start(void)
{
	if(aamp)
	{
		const char *url = aamp->GetManifestUrl().c_str();
		int hdmiInputPort = -1;
		if( sscanf(url, "hdmiin://localhost/deviceid/%d", &hdmiInputPort ) == 1 )
		{
			StartHelper(hdmiInputPort);
		}
	}
}

/**
 *   @brief  Stops streaming.
 */
void StreamAbstractionAAMP_HDMIIN::Stop(bool clearChannelData)
{
	StopHelper();
}

/**
 *   @brief get StreamAbstractionAAMP_HDMIIN instance
 */

StreamAbstractionAAMP_HDMIIN * StreamAbstractionAAMP_HDMIIN::GetInstance(class PrivateInstanceAAMP *aamp,double seekpos, float rate)
{
	if( mHdmiinInstance == NULL)
	{
		mHdmiinInstance = new StreamAbstractionAAMP_HDMIIN(aamp,seekpos,rate);
	}
	else
	{
		// Reuse existing instance and set new aamp
		mHdmiinInstance->aamp = aamp;
		mHdmiinInstance->aamp->SetContentType("HDMI_IN");
	}
	return mHdmiinInstance;
}

/**
 *   @brief Clear aamp of HdmiinInstance
 */
void StreamAbstractionAAMP_HDMIIN::ResetInstance()
{
	std::lock_guard<std::mutex>lock(mEvtMutex);
	if(mHdmiinInstance != NULL)
	{
		if(mHdmiinInstance->aamp != NULL)
		{
			mHdmiinInstance->aamp->SetState(eSTATE_STOPPED);
		}
		//clear aamp
		mHdmiinInstance->aamp = NULL;
	}
}

/**
 * @brief  Gets videoStreamInfoUpdate event and translates into aamp events
 */
void StreamAbstractionAAMP_HDMIIN::OnVideoStreamInfoUpdate(PlayerVideoStreamInfoData data)
{
	if(aamp)
	{
		VideoScanType videoScanType = (data.progressive ? eVIDEOSCAN_PROGRESSIVE : eVIDEOSCAN_INTERLACED);
		double frameRate = 0.0;
		if((0 != data.frameRateN) && (0 != data.frameRateD))
		{
			frameRate = data.frameRateN / data.frameRateD;
		}
		aamp->NotifyBitRateChangeEvent(0, eAAMP_BITRATE_CHANGE_BY_HDMIIN, data.width, data.height, frameRate, 0, false, videoScanType, 0, 0);

	}
}
