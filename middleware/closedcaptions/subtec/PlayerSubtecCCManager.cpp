/*
 * If not stated otherwise in this file or this component's license file the
 * following copyright and licenses apply:
 *
 * Copyright 2025 RDK Management
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
 *  @file PlayerSubtecCCManager.cpp
 *
 *  @brief Impl of Subtec ClosedCaption integration layer
 *
 */

#include "PlayerSubtecCCManager.h"

#include "PlayerLogManager.h" // Included for MW_LOG
//TODO: Fix cyclic dependency btw GlobalConfig and PlayerLogManager

#include "SubtecConnector.h"

/**
 *  @brief Impl specific initialization code called before each public interface call
 */
void PlayerSubtecCCManager::EnsureInitialized()
{
	EnsureHALInitialized();
	EnsureRendererCommsInitialized();
}

/**
 *  @brief Impl specific initialization code for HAL
 */
void PlayerSubtecCCManager::EnsureHALInitialized()
{
	if(not mHALInitialized || mHandleUpdated)
	{
		if(subtecConnector::initHal((void *)mCCHandle) == CC_VL_OS_API_RESULT_SUCCESS)
		{
			MW_LOG_WARN("PlayerSubtecCCManager::calling subtecConnector::initHal() - success");
			mHALInitialized = true;
			mHandleUpdated = false;
		}
		else
		{
			MW_LOG_WARN("PlayerSubtecCCManager::calling subtecConnector::initHal() - failure");
		}
	}
};

/**
 *  @brief Impl specific initialization code for Communication with rendered
 */
void PlayerSubtecCCManager::EnsureRendererCommsInitialized()
{
	if(not mRendererInitialized)
	{
		if(subtecConnector::initPacketSender() == CC_VL_OS_API_RESULT_SUCCESS)
		{
			MW_LOG_WARN("PlayerSubtecCCManager::calling subtecConnector::initPacketSender() - success");
			mRendererInitialized = true;
		}
		else
		{
			MW_LOG_WARN("PlayerSubtecCCManager::calling subtecConnector::initPacketSender() - failure");
		}
	}
};

/**
 * @brief stores Handle
 */
int PlayerSubtecCCManager::Initialize(void * handle)
{
	if (mCCHandle != handle)
		mHandleUpdated = true;
	mCCHandle = handle;

	return 0;
}


/**
 *  @brief Gets Handle or ID, Every client using subtec must call GetId  in the beginning , save id, which is required for Release function.
 */
int PlayerSubtecCCManager::GetId()
{
    std::lock_guard<std::mutex> lock(mIdLock);
    mId++;
    mIdSet.insert(mId);
    return mId;
}

/**
 *  @brief Release CC resources
 */
void PlayerSubtecCCManager::Release(int id)
{
    std::lock_guard<std::mutex> lock(mIdLock);
    if( mIdSet.erase(id) > 0 )
    {
		int iSize = mIdSet.size();
		MW_LOG_WARN("PlayerSubtecCCManager::users:%d",iSize);
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
		MW_LOG_TRACE("PlayerSubtecCCManager::ID:%d not found returning",id);
	}
}

/**
 *  @brief To start CC rendering
 */
void PlayerSubtecCCManager::StartRendering()
{
	subtecConnector::ccMgrAPI::ccShow();
}

/**
 *  @brief To stop CC rendering
 */
void PlayerSubtecCCManager::StopRendering()
{
	subtecConnector::ccMgrAPI::ccHide();
}

/**
 *  @brief set digital channel with specified id
 */
int PlayerSubtecCCManager::SetDigitalChannel(unsigned int id)
{
	const auto ret =  subtecConnector::ccMgrAPI::ccSetDigitalChannel(id);
	EnsureRendererStateConsistency();
	return ret;
}

/**
 *  @brief set analog channel with specified id
 */
int PlayerSubtecCCManager::SetAnalogChannel(unsigned int id)
{
	const auto ret =  subtecConnector::ccMgrAPI::ccSetAnalogChannel(id);
	EnsureRendererStateConsistency();
	return ret;
}

/**
 *  @brief ensure mRendering is consistent with renderer state
 */
void PlayerSubtecCCManager::EnsureRendererStateConsistency()
{
	MW_LOG_WARN("PlayerSubtecCCManager::");
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
PlayerSubtecCCManager::PlayerSubtecCCManager()
{
 	// Some of the apps don’t call set track  and as default CC is not set, CC doesn’t work. 
	// In this case app expect to render default cc as CC1.
	// Hence Set default CC track to CC1
	MW_LOG_WARN("PlayerSubtecCCManager::PlayerSubtecCCManager setting default to cc1");
	SetTrack("CC1");
}
