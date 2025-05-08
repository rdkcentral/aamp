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
 * @file compositein_shim.cpp
 * @brief shim for dispatching UVE Composite input playback
 */
#include "compositein_shim.h"
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <assert.h>

StreamAbstractionAAMP_COMPOSITEIN* StreamAbstractionAAMP_COMPOSITEIN::mCompositeinInstance = NULL;

/**
 * @brief StreamAbstractionAAMP_COMPOSITEIN Constructor
 */
StreamAbstractionAAMP_COMPOSITEIN::StreamAbstractionAAMP_COMPOSITEIN(class PrivateInstanceAAMP *aamp,double seek_pos, float rate)
                             : StreamAbstractionAAMP_VIDEOIN("COMPOSITEIN", PlayerThunderAccessPlugin::AVINPUT, aamp,seek_pos,rate,"COMPOSITE")
{
	aamp->SetContentType("COMPOSITE_IN");
}

/**
 * @brief StreamAbstractionAAMP_COMPOSITEIN Destructor
 */
StreamAbstractionAAMP_COMPOSITEIN::~StreamAbstractionAAMP_COMPOSITEIN()
{
	AAMPLOG_WARN("destructor ");
	mCompositeinInstance = NULL;
}

/**
 * @brief  Initialize a newly created object.
 */
AAMPStatusType StreamAbstractionAAMP_COMPOSITEIN::Init(TuneType tuneType)
{
        AAMPStatusType retval = eAAMPSTATUS_OK;
        retval = InitHelper(tuneType);
        return retval;
}

/**
 *   @brief  Starts streaming.
 */
void StreamAbstractionAAMP_COMPOSITEIN::Start(void)
{
	if(aamp)
	{
		const char *url = aamp->GetManifestUrl().c_str();
		int compositeInputPort = -1;
		if( sscanf(url, "cvbsin://localhost/deviceid/%d", &compositeInputPort ) == 1 )
		{
			StartHelper(compositeInputPort);
		}
	}
}

/**
 * @brief  Stops streaming.
 */
void StreamAbstractionAAMP_COMPOSITEIN::Stop(bool clearChannelData)
{
	StopHelper();
}

/**
 * @brief get StreamAbstractionAAMP_COMPOSITEIN instance
 */
StreamAbstractionAAMP_COMPOSITEIN * StreamAbstractionAAMP_COMPOSITEIN::GetInstance(class PrivateInstanceAAMP *aamp,double seekpos, float rate)
{
	if(mCompositeinInstance == NULL)
	{
		mCompositeinInstance = new StreamAbstractionAAMP_COMPOSITEIN(aamp,seekpos,rate);
	}
	else
	{
		// Reuse existing instance and set new aamp
		mCompositeinInstance->aamp = aamp;
		mCompositeinInstance->aamp->SetContentType("COMPOSITE_IN");
	}

	return mCompositeinInstance;
}

/**
*  @brief Clear aamp of CompositeInInstance
*/
void StreamAbstractionAAMP_COMPOSITEIN::ResetInstance()
{
	std::lock_guard<std::mutex>lock(mEvtMutex);
	if(mCompositeinInstance != NULL)
	{
		if(mCompositeinInstance->aamp != NULL)
		{
			mCompositeinInstance->aamp->SetState(eSTATE_STOPPED);
		}
		//clear aamp
		mCompositeinInstance->aamp = NULL;
	}
}
