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
 *  @file AampRDKCCManager.cpp
 *
 *  @brief Impl of RDK ClosedCaption integration layer
 *
 */

#include "AampRDKCCManager.h"

#include "ccDataReader.h"
#include "vlCCConstants.h"
#include "cc_util.h"
#include "vlGfxScreen.h"

#define CHAR_CODE_1 49
#define CHAR_CODE_6 54

/**
 *  @brief Release CC resources
 */
void AampRDKCCManager::Release(int iID)
 {
	std::lock_guard<std::mutex> lock(mIdLock);
	if( mIdSet.erase(iID) > 0 )
	{
		int iSize = (int)mIdSet.size();
		AAMPLOG_WARN("AampRDKCCManager::users:%d",iSize);
		//No one using RDKCCMgr, stop/close it.
		if(0 == iSize)
		{
			Stop();
			if (mCCHandle != NULL)
			{
				media_closeCaptionStop();
				mCCHandle = NULL;
			}
			mTrickplayStarted = false;
			mParentalCtrlLocked = false;
		}
	}
	else
	{
		AAMPLOG_TRACE("AampRDKCCManager::ID:%d not found returning",iID);
	}	
 }

/**
 *  @brief To start CC rendering
 */
void AampRDKCCManager::StartRendering()
 {
	ccSetCCState(CCStatus_ON, 1);
 }

/**
 *  @brief To stop CC rendering
 */
void AampRDKCCManager::StopRendering()
 {
	ccSetCCState(CCStatus_OFF, 1);
 }

/**
 *  @brief Impl specific initialization code called once in Init() function
 */
int AampRDKCCManager::Initialize(void * handle)
 {
	static bool initStatus = false;
	
	int ret = -1;
	if (!initStatus)
	{
		vlGfxInit(0);
		ret = vlMpeosCCManagerInit();
		if (ret != 0)
		{
			return ret;
		}
		initStatus = true;
	}

	if (handle == NULL)
	{
		return -1;
	}

	mCCHandle = handle;
	media_closeCaptionStart((void *)mCCHandle);

	return 0;
 }

/**
 *  @brief set digital channel with specified id
 */
int AampRDKCCManager::SetDigitalChannel(unsigned int id)
 {
	return ccSetDigitalChannel(id);
 }

/**
 *  @brief set analog channel with specified id
 */
int AampRDKCCManager::SetAnalogChannel(unsigned int id)
 {
	return ccSetAnalogChannel(id);
 }

/**
 *  @brief Gets Handle or ID, Every client using rdkccmgr must call GetId  in the beginning , save id, which is required for Release function.
 */
int AampRDKCCManager::GetId()
 {
	std::lock_guard<std::mutex> lock(mIdLock);
	mId++;
	mIdSet.insert(mId);
	return mId;
 }

