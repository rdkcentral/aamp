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

#include "PlayerSecManager.h"
#include <string.h>
#include "_base64.h"
#include <inttypes.h> // For PRId64
#include <uuid/uuid.h>

static PlayerSecManager *Instance = nullptr; /**< singleton instance*/

/* mutex GetInstance() & DestroyInstance() to improve thread safety
 * There is still a race between using the pointer returned from GetInstance() and calling DestroyInstance()*/
static std::mutex InstanceMutex;

/**
 * @brief Sleep for given milliseconds
 */
void ms_sleep(int milliseconds)
{
}

std::shared_ptr<PlayerSecManagerSession::SessionManager> PlayerSecManagerSession::SessionManager::getInstance(int64_t sessionID, std::size_t inputSummaryHash)
{
	std::shared_ptr<SessionManager> returnValue = nullptr;
	return returnValue;
}

PlayerSecManagerSession::SessionManager::~SessionManager()
{
}
void PlayerSecManagerSession::SessionManager::setInputSummaryHash(std::size_t inputSummaryHash)
{
}


PlayerSecManagerSession::SessionManager::SessionManager(int64_t sessionID, std::size_t inputSummaryHash)
{};

PlayerSecManagerSession::PlayerSecManagerSession(int64_t sessionID, std::size_t inputSummaryHash)
{};

int64_t PlayerSecManagerSession::getSessionID(void) const
{
	int64_t ID = PLAYER_SECMGR_INVALID_SESSION_ID;
	return ID;
}

std::size_t PlayerSecManagerSession::getInputSummaryHash()
{
	std::size_t hash=0;
	return hash;
}

/**
 * @brief To get PlayerSecManager instance
 */
PlayerSecManager* PlayerSecManager::GetInstance()
{
	return Instance;
}

/**
 * @brief To release PlayerSecManager singelton instance
 */
void PlayerSecManager::DestroyInstance()
{
}

/**
 * @brief PlayerScheduler Constructor
 */
PlayerSecManager::PlayerSecManager() : mSecManagerObj(SECMANAGER_CALL_SIGN), mSecMutex(), mSchedulerStarted(false),
				   mRegisteredEvents(), mWatermarkPluginObj(WATERMARK_PLUGIN_CALLSIGN), mWatMutex(), mSpeedStateMutex()
{
}

/**
 * @brief PlayerScheduler Destructor
 */
PlayerSecManager::~PlayerSecManager()
{
}
static std::size_t getInputSummaryHash(const char* moneyTraceMetdata[][2], const char* contentMetdata,
					size_t contMetaLen, const char* licenseRequest, const char* keySystemId,
					const char* mediaUsage, const char* accessToken, bool isVideoMuted)
{
	return 0;
}

bool PlayerSecManager::AcquireLicense( const char* licenseUrl, const char* moneyTraceMetdata[][2],
					const char* accessAttributes[][2], const char* contentMetdata, size_t contMetaLen,
					const char* licenseRequest, size_t licReqLen, const char* keySystemId,
					const char* mediaUsage, const char* accessToken, size_t accTokenLen,
					PlayerSecManagerSession &session,
					char** licenseResponse, size_t* licenseResponseLength, int32_t* statusCode, int32_t* reasonCode, int32_t* businessStatus, bool isVideoMuted, int sleepTime)
{
	return false;
}

/**
 * @brief To update session state to SecManager
 */
bool PlayerSecManager::UpdateSessionState(int64_t sessionId, bool active)
{
	return false;
}

/**
 * @brief To update session state to SecManager
 */
bool PlayerSecManager::setVideoWindowSize(int64_t sessionId, int64_t video_width, int64_t video_height)
{
       return false;
}

/**
 * @brief To set Playback Speed State to SecManager
 */
bool PlayerSecManager::setPlaybackSpeedState(int64_t sessionId, int64_t playback_speed, int64_t playback_position)
{
       return false;
}


/**
 * @brief To Load ClutWatermark
 */
bool PlayerSecManager::loadClutWatermark(int64_t sessionId, int64_t graphicId, int64_t watermarkClutBufferKey, int64_t watermarkImageBufferKey, int64_t clutPaletteSize,
                                                                       const char* clutPaletteFormat, int64_t watermarkWidth, int64_t watermarkHeight, float aspectRatio)
{
       return false;
}

/**
 * @brief To set Watermark Session callback
 */
void PlayerSecManager::setWatermarkSessionEvent_CB(const std::function<void(uint32_t, uint32_t, const std::string&)>& callback)
{
	return;
}

/**
 * @brief To set Watermark Session callback
 */
std::function<void(uint32_t, uint32_t, const std::string&)>& PlayerSecManager::getWatermarkSessionEvent_CB( )
{
	static std::function<void(uint32_t, uint32_t, const std::string&)> callback = nullptr;
	return callback;
}

/**
 * @brief To generate UUID
 */
void uuid_generate(uuid_t out)
{
}

/**
 * @brief To parse UUID
 */
void uuid_unparse(const uuid_t uu, char *out)
{
}
