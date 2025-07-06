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
 * @file ContentSecurityManager.cpp
 * @brief Class impl for ContentSecurityManager
 */

#include "ContentSecurityManager.h"
#include <string.h>
#include "_base64.h"
#include <inttypes.h> // For PRId64
#include <uuid/uuid.h>

static ContentSecurityManager *Instance = nullptr; /**< singleton instance*/

/* mutex GetInstance() & DestroyInstance() to improve thread safety
 * There is still a race between using the pointer returned from GetInstance() and calling DestroyInstance()*/
static std::mutex InstanceMutex;

/**
 * @brief To get ContentSecurityManager instance
 */
ContentSecurityManager* ContentSecurityManager::GetInstance()
{
	return Instance;
}

/**
 * @brief To release ContentSecurityManager singelton instance
 */
void ContentSecurityManager::DestroyInstance()
{
}

static std::size_t getInputSummaryHash(const char* moneyTraceMetdata[][2], const char* contentMetdata,
					size_t contMetaLen, const char* licenseRequest, const char* keySystemId,
					const char* mediaUsage, const char* accessToken, bool isVideoMuted)
{
	return 0;
}

bool ContentSecurityManager::AcquireLicense( std::string clientId, std::string appId, const char* licenseUrl, const char* moneyTraceMetdata[][2],
					const char* accessAttributes[][2], const char* contentMetdata, size_t contMetaLen,
					const char* licenseRequest, size_t licReqLen, const char* keySystemId,
					const char* mediaUsage, const char* accessToken, size_t accTokenLen,
					SecureContentSession &session,
					char** licenseResponse, size_t* licenseResponseLength, int32_t* statusCode, int32_t* reasonCode, int32_t* businessStatus, bool isVideoMuted, int sleepTime)
{
	return false;
}

/**
 * @brief To update session state to SecManager
 */
bool ContentSecurityManager::UpdateSessionState(int64_t sessionId, bool active)
{
	return false;
}

/**
 * @brief To update session state to SecManager
 */
bool ContentSecurityManager::setVideoWindowSize(int64_t sessionId, int64_t video_width, int64_t video_height)
{
       return false;
}

/**
 * @brief To set Playback Speed State to SecManager
 */
bool ContentSecurityManager::setPlaybackSpeedState(int64_t sessionId, int64_t playback_speed, int64_t playback_position)
{
       return false;
}

/**
 * @brief To notify SecManager to release a session
 */
void ContentSecurityManager::ReleaseSession(int64_t sessionId)
{
	return;
}

/**
 * @brief To set Watermark Session callback
 */
void ContentSecurityManager::setWatermarkSessionEvent_CB(const std::function<void(uint32_t, uint32_t, const std::string&)>& callback)
{
	return;
}

/**
 * @brief To set Watermark Session callback
 */
std::function<void(uint32_t, uint32_t, const std::string&)>& ContentSecurityManager::getWatermarkSessionEvent_CB( )
{
	static std::function<void(uint32_t, uint32_t, const std::string&)> callback = nullptr;
	return callback;
}

void ContentSecurityManager::UseFireboltSDK(bool status)
{
	return;
}

/**
 * @brief To generate UUID
 */
void uuid_generate(uuid_t out)
{
	return;
}

/**
 * @brief To parse UUID
 */
void uuid_unparse(const uuid_t uu, char *out)
{
	return;
}
