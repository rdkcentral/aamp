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
 * limitations under the License.m
 */

/**
 * @file PlayerSecManager.cpp
 * @brief Class impl for PlayerSecManager
 */

#include "ContentProtectionFirebolt.h"
#include "PlayerSecManager.h"
#include "PlayerLogManager.h"
#include <string.h>
#include "_base64.h"
#include <inttypes.h> // For PRId64
#include <fstream>
#include <sstream>
#include <string>
#if defined(USE_SECMANAGER)
#include "SecManagerThunder.h"
#endif

static PlayerSecManager *Instance = nullptr; /**< singleton instance*/

std::function<void(uint32_t, uint32_t, const std::string&)> PlayerSecManager::SendWatermarkSessionEvent_CB;

/* mutex GetInstance() & DestroyInstance() to improve thread safety
 * There is still a race between using the pointer returned from GetInstance() and calling DestroyInstance()*/
static std::mutex InstanceMutex;
static bool mUseFireboltSDK = false;

/**
 * @brief To get PlayerSecManager instance
 */
PlayerSecManager* PlayerSecManager::GetInstance()
{
	std::lock_guard<std::mutex> lock{InstanceMutex};
	if(Instance == nullptr)
	{
/* Firebolt is applicable to all builds which uses either secmanager or secclient */
#if defined(USE_SECCLIENT) || defined(USE_SECMANAGER)
		if(mUseFireboltSDK)
		{
			Instance = new ContentProtectionFirebolt();
		}
		else
		{
#if defined(USE_SECMANAGER)
			Instance = new SecManagerThunder();
#endif
		}
#else
		Instance = new FakeSecManager();
#endif
	}
	return Instance;
}

/**
 * @brief To release PlayerSecManager singelton instance
 */
void PlayerSecManager::DestroyInstance()
{
	std::lock_guard<std::mutex> lock{InstanceMutex};
	if (Instance)
	{
		delete Instance;
		Instance = nullptr;
	}
}

/**
 * @brief To indicate whether application support firebolt capability
 */
void PlayerSecManager::UseFireboltSDK(bool status)
{
	mUseFireboltSDK = status;
}

std::size_t PlayerSecManager::getInputSummaryHash(const char* moneyTraceMetadata[][2], const char* contentMetadata,
		size_t contMetaLen, const char* licenseRequest, const char* keySystemId,
		const char* mediaUsage, const char* accessToken, bool isVideoMuted)
{
	std::stringstream ss;
	ss<< moneyTraceMetadata[0][1]<<isVideoMuted<<//sessionConfiguration (only variables)
		//ignoring hard coded aspectDimensions 
		keySystemId<<mediaUsage<<accessToken<<contentMetadata<<licenseRequest;

	std::string InputSummary =  ss.str();

	auto returnHash = std::hash<std::string>{}(InputSummary);
	ss<<"SecManager input summary hash: "<<returnHash << "(" << InputSummary << ")";
	MW_LOG_MIL("%s", ss.str().c_str());

	return returnHash;
}

bool PlayerSecManager::AcquireLicense(std::string clientId, std::string appId, const char* licenseUrl, const char* moneyTraceMetadata[][2],
		const char* accessAttributes[][2], const char* contentMetadata, size_t contMetaLen,
		const char* licenseRequest, size_t licReqLen, const char* keySystemId,
		const char* mediaUsage, const char* accessToken, size_t accTokenLen,
		PlayerSecManagerSession &session,
		char** licenseResponse, size_t* licenseResponseLength, int32_t* statusCode, int32_t* reasonCode, int32_t* businessStatus, bool isVideoMuted, int sleepTime)
{
	bool success = false;
	auto inputSummaryHash = getInputSummaryHash(moneyTraceMetadata, contentMetadata,
			contMetaLen, licenseRequest, keySystemId,
			mediaUsage, accessToken, isVideoMuted);

	if(!session.isSessionValid())
	{
		MW_LOG_MIL("%s, open new session.", session.ToString().c_str());
	}
	else if(inputSummaryHash==session.getInputSummaryHash())
	{
		MW_LOG_MIL("%s, & input data matches, set session to active.", session.ToString().c_str());
		success = UpdateSessionState(session.getSessionID(), true);
	}
	else
	{
		MW_LOG_MIL("%s but the input data has changed, update session.", session.ToString().c_str());
	}

	if(!success)
	{
		/*old AcquireLicense() code used for open & update (expected operation) and
		 * if setPlaybackSessionState doesn't have the expected result*/
		success = AcquireLicenseOpenOrUpdate( clientId, appId, licenseUrl, moneyTraceMetadata,
				accessAttributes, contentMetadata, contMetaLen,
				licenseRequest, licReqLen, keySystemId,
				mediaUsage, accessToken, accTokenLen,
				session,
				licenseResponse, licenseResponseLength, statusCode, reasonCode,
				businessStatus, isVideoMuted, sleepTime);
	}
	return success;
}

/**
 * @brief To update session state to SecManager
 */
bool PlayerSecManager::UpdateSessionState(int64_t sessionId, bool active)
{
	bool success = false;
	success = SetDrmSessionState(sessionId, active);
	return success;
}

/**
 * @brief To notify SecManager to release a session
 */
void PlayerSecManager::ReleaseSession(int64_t sessionId)
{
	CloseDrmSession(sessionId);
}

/**
 * @brief To update session state to SecManager
 */
bool PlayerSecManager::setVideoWindowSize(int64_t sessionId, int64_t video_width, int64_t video_height)
{
	bool rpcResult = false;
	rpcResult = setWindowSize(sessionId, video_width, video_height);
	return rpcResult;
}

/**
 * @brief To set Playback Speed State to SecManager
 */
bool PlayerSecManager::setPlaybackSpeedState(int64_t sessionId, int64_t playback_speed, int64_t playback_position)
{
	bool rpcResult = false;
	rpcResult = SetPlaybackPosition(sessionId, playback_speed, playback_position);
	return rpcResult;
}

/**
 * @brief To set Watermark Session callback
 */
void PlayerSecManager::setWatermarkSessionEvent_CB(const std::function<void(uint32_t, uint32_t, const std::string&)>& callback)
{
	SendWatermarkSessionEvent_CB = callback;
	return;
}

/**
 * @brief To set Watermark Session callback
 */
std::function<void(uint32_t, uint32_t, const std::string&)>& PlayerSecManager::getWatermarkSessionEvent_CB( )
{
	return SendWatermarkSessionEvent_CB;
}

