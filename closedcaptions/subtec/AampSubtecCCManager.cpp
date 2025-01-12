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
 *  @file AampSubtecCCManager.cpp
 *
 *  @brief Impl of Subtec ClosedCaption integration layer
 *
 */

#include "AampSubtecCCManager.h"

#include "priv_aamp.h" // Included for AAMPLOG
//TODO: Fix cyclic dependency btw GlobalConfig and AampLogManager

#include <closedcaptions/subtec/SubtecConnector.h>

/**
 *  @brief Impl specific initialization code called before each public interface call
 */
void AampSubtecCCManager::EnsureInitialized()
{
	EnsureHALInitialized();
	EnsureRendererCommsInitialized();
}

/**
 *  @brief Impl specific initialization code for HAL
 */
void AampSubtecCCManager::EnsureHALInitialized()
{
	if(not mHALInitialized || mHandleUpdated)
	{
		if(subtecConnector::initHal((void *)mCCHandle) == CC_VL_OS_API_RESULT_SUCCESS)
		{
			AAMPLOG_WARN("AampSubtecCCManager::calling subtecConnector::initHal() - success");
			mHALInitialized = true;
			mHandleUpdated = false;
		}
		else
		{
			AAMPLOG_WARN("AampSubtecCCManager::calling subtecConnector::initHal() - failure");
		}
	}
};

/**
 *  @brief Impl specific initialization code for Communication with rendered
 */
void AampSubtecCCManager::EnsureRendererCommsInitialized()
{
	if(not mRendererInitialized)
	{
		if(subtecConnector::initPacketSender() == CC_VL_OS_API_RESULT_SUCCESS)
		{
			AAMPLOG_WARN("AampSubtecCCManager::calling subtecConnector::initPacketSender() - success");
			mRendererInitialized = true;
		}
		else
		{
			AAMPLOG_WARN("AampSubtecCCManager::calling subtecConnector::initPacketSender() - failure");
		}
	}
};

/**
 * @brief stores Handle
 */
int AampSubtecCCManager::Initialize(void * handle)
{
	if (mCCHandle != handle)
		mHandleUpdated = true;
	mCCHandle = handle;

	return 0;
}


/**
 *  @brief Gets Handle or ID, Every client using subtec must call GetId  in the begining , save id, which is required for Release funciton.
 */
int AampSubtecCCManager::GetId()
{
    std::lock_guard<std::mutex> lock(mIdLock);
    mId++;
    mIdSet.insert(mId);
    return mId;
}

/**
 *  @brief Release CC resources
 */
void AampSubtecCCManager::Release(int id)
{
    std::lock_guard<std::mutex> lock(mIdLock);
    if( mIdSet.erase(id) > 0 )
    {
		int iSize = mIdSet.size();
		AAMPLOG_WARN("AampSubtecCCManager::users:%d",iSize);
		//No one using subtec, stop/close it.
		if(0 == iSize)
		{
			subtecConnector::resetChannel();
			if(mHALInitialized)
			{
				subtecConnector::close();
				mHALInitialized = false;
				if (mCCHandle != NULL)
				{
					mCCHandle = NULL;
				}
			}
			mTrickplayStarted = false;
			mParentalCtrlLocked = false;
		}
	}
	else
	{
		AAMPLOG_TRACE("AampSubtecCCManager::ID:%d not found returning",id);
	}
}

/**
 *  @brief To start CC rendering
 */
void AampSubtecCCManager::StartRendering()
{
	subtecConnector::ccMgrAPI::ccShow();
}

/**
 *  @brief To stop CC rendering
 */
void AampSubtecCCManager::StopRendering()
{
	subtecConnector::ccMgrAPI::ccHide();
}

/**
 *  @brief set digital channel with specified id
 */
int AampSubtecCCManager::SetDigitalChannel(unsigned int id)
{
	const auto ret =  subtecConnector::ccMgrAPI::ccSetDigitalChannel(id);
	EnsureRendererStateConsistency();
	return ret;
}

/**
 *  @brief set analog channel with specified id
 */
int AampSubtecCCManager::SetAnalogChannel(unsigned int id)
{
	const auto ret =  subtecConnector::ccMgrAPI::ccSetAnalogChannel(id);
	EnsureRendererStateConsistency();
	return ret;
}

/**
 *  @brief ensure mRendering is consistent with renderer state
 */
void AampSubtecCCManager::EnsureRendererStateConsistency()
{
	AAMPLOG_WARN("AampSubtecCCManager::");
	if(mEnabled)
	{
		Start();
	}
	else
	{
		Stop();
	}
	SetStyle(mOptions);
}

/**
 *  @brief Constructor
 */
AampSubtecCCManager::AampSubtecCCManager()
{
 	// Some of the apps don’t call set track  and as default CC is not set, CC doesn’t work. 
	// In this case app expect to render default cc as CC1.
	// Hence Set default CC track to CC1
	AAMPLOG_WARN("AampSubtecCCManager::AampSubtecCCManager setting default to cc1");
	SetTrack("CC1");
}
