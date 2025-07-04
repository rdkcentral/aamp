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
 * @file PlayerSecManager.h
 * @brief Class to communicate with SecManager Thunder plugin
 */

#ifndef __PLAYER_SECMANAGER_H__
#define __PLAYER_SECMANAGER_H__

#include <mutex>
#include <sys/time.h>
#include "ThunderAccessPlayer.h"
#include "PlayerSecManagerSession.h"
#include "PlayerScheduler.h"
#include "PlayerMemoryUtils.h"
#include <inttypes.h>
#include <memory>
#include <list>
#include <map>
#include <vector>

//Secmanager error class codes
#define SECMANAGER_DRM_FAILURE 200
#define SECMANAGER_WM_FAILURE 300 	/**< If secmanager couldn't initialize watermark service */

//Secmanager error reason codes
#define SECMANAGER_DRM_GEN_FAILURE 1	/**< General or internal failure */
#define SECMANAGER_SERVICE_TIMEOUT 3
#define SECMANAGER_SERVICE_CON_FAILURE 4
#define SECMANAGER_SERVICE_BUSY 5
#define SECMANAGER_ACCTOKEN_EXPIRED 8
#define SECMANAGER_ENTITLEMENT_FAILURE 102

#define SECMANAGER_CALL_SIGN "org.rdk.SecManager.1"
#define WATERMARK_PLUGIN_CALLSIGN "org.rdk.Watermark.1"
//#define RDKSHELL_CALLSIGN "org.rdk.RDKShell.1"   //need to be used instead of WATERMARK_PLUGIN_CALLSIGN if RDK Shell is used for rendering watermark

/**
 * @class PlayerSecManager
 * @brief Class to get License from Sec Manager
 */
class PlayerSecManager : public PlayerScheduler
{
public:
	//allow access to PlayerSecManager::ReleaseSession()
	friend PlayerSecManagerSession::SessionManager::~SessionManager();
	/**
	 * @fn GetInstance
	 *
	 * @return PlayerSecManager instance
	 */
	static PlayerSecManager* GetInstance();

	/**
	 * @fn DestroyInstance
	 */
	static void DestroyInstance();

	/**
	 * @fn AcquireLicense
	 *
	 * @param[in] licenseUrl - url to fetch license from
	 * @param[in] moneyTraceMetadata - moneytrace info
	 * @param[in] accessAttributes - accessAttributes info
	 * @param[in] contentMetadata - content metadata info
	 * @param[in] licenseRequest - license challenge info
	 * @param[in] keySystemId - unique system ID of drm
	 * @param[in] mediaUsage - indicates whether its stream or download license request
	 * @param[in] accessToken - access token info
	 * @param[out] sessionId - session ID object of current session
	 * @param[out] licenseResponse - license response
	 * @param[out] licenseResponseLength - len of license response
	 * @param[out] statusCode - license fetch status code
	 * @param[out] reasonCode - license fetch reason code
	 * @return bool - true if license fetch successful, false otherwise
	 */
	bool AcquireLicense( const char* licenseUrl, const char* moneyTraceMetadata[][2],
						const char* accessAttributes[][2], const char* contentMetadata, size_t contentMetadataLen,
						const char* licenseRequest, size_t licenseRequestLen, const char* keySystemId,
						const char* mediaUsage, const char* accessToken, size_t accessTokenLen,
						PlayerSecManagerSession &session,
						char** licenseResponse, size_t* licenseResponseLength,
						int32_t* statusCode, int32_t* reasonCode, int32_t*  businessStatus, bool isVideoMuted, int sleepTime);


	/**
	 * @fn UpdateSessionState
	 *
	 * @param[in] sessionId - session id
	 * @param[in] active - true if session is active, false otherwise
	 */
	bool UpdateSessionState(int64_t sessionId, bool active);

	/**
	 * @fn setVideoWindowSize
	 *
	 * @param[in] sessionId - session id
	 * @param[in] video_width - video width
	 * @param[in] video_height - video height
	 */
	bool setVideoWindowSize(int64_t sessionId, int64_t video_width, int64_t video_height);
	/**
	 * @fn setPlaybackSpeedState
	 *
	 * @param[in] sessionId - session id
	 * @param[in] playback_speed - playback speed
	 * @param[in] playback_position - playback position
	 */
	bool setPlaybackSpeedState(int64_t sessionId, int64_t playback_speed, int64_t playback_position);
	/**
	 * @fn loadClutWatermark
	 * @param[in] sessionId - session id
	 *
	 */
	bool loadClutWatermark(int64_t sessionId, int64_t graphicId, int64_t watermarkClutBufferKey, int64_t watermarkImageBufferKey, int64_t clutPaletteSize, const char* clutPaletteFormat, int64_t watermarkWidth, int64_t watermarkHeight, float aspectRatio);
	/**
	 *   @fn SendWatermarkSessionEvent_CB
	 */
	static std::function<void(uint32_t, uint32_t, const std::string&)> SendWatermarkSessionEvent_CB;

	/**
	 * @fn getSchedulerStatus
	 *
	 * @return bool - true if scheduler is running, false otherwise
	 */
	bool getSchedulerStatus();

	/**
	 *   @fn CreateWatermark
	 */
	void CreateWatermark(int graphicId, int zIndex);
	/**
	 *   @fn UpdateWatermark
	 */
	void UpdateWatermark(int graphicId, int smKey, int smSize);
	/**
	 *   @fn GetWaterMarkPalette
	 */
	void GetWaterMarkPalette(int sessionId, int graphicId);
	/**
	 *   @fn ModifyWatermarkPalette
	 */
	void ModifyWatermarkPalette(int graphicId, int clutKey, int imageKey);
	/**
	 *   @fn DeleteWatermark
	 */
	void DeleteWatermark(int graphicId);
	/**
	 *   @fn AlwaysShowWatermarkOnTop
	 */
	void AlwaysShowWatermarkOnTop(bool show);
	/**
	 *   @fn ShowWatermark
	 */
	void ShowWatermark(bool show);

	/**
	 * @fn setWatermarkSessionEvent_CB
	 * @param[in] callback - callback function
	 * @return void
	 * @brief Set callback function for watermark session
	 */
	static void setWatermarkSessionEvent_CB(const std::function<void(uint32_t, uint32_t, const std::string&)>& callback);

	/**
	 * @fn getWatermarkSessionEvent_CB
	 * @return std::function<void(uint32_t, uint32_t, const std::string&)>&
	 * @brief Get callback function for watermark session
	 */
	static std::function<void(uint32_t, uint32_t, const std::string&)>& getWatermarkSessionEvent_CB( );
private:

	/* Run AcquireLicenseOpenOrUpdate is the old AcquireLicense code
	 * It is used by AcquireLicense() to for opening sessions & for calling update when this is required*/
	bool AcquireLicenseOpenOrUpdate( const char* licenseUrl, const char* moneyTraceMetadata[][2],
						const char* accessAttributes[][2], const char* contentMetadata, size_t contentMetadataLen,
						const char* licenseRequest, size_t licenseRequestLen, const char* keySystemId,
						const char* mediaUsage, const char* accessToken, size_t accessTokenLen,
						PlayerSecManagerSession &session,
						char** licenseResponse, size_t* licenseResponseLength,
						int32_t* statusCode, int32_t* reasonCode, int32_t*  businessStatus, bool isVideoMuted, int sleepTime);

	/**
	 * @fn ReleaseSession - this should only be used by PlayerSecManagerSession::SessionManager::~SessionManager();
	 *
	 * @param[in] sessionId - session id
	 */
	void ReleaseSession(int64_t sessionId);

	/**
	 * @fn PlayerSecManager
	 */
	PlayerSecManager();

	/**
	 * @fn ~PlayerSecManager
	 */
	~PlayerSecManager();
	/**
	 * @brief Copy constructor disabled
	 *
	 */
	PlayerSecManager(const PlayerSecManager&) = delete;
	/**
	 * @brief assignment operator disabled
         *
         */
	PlayerSecManager* operator=(const PlayerSecManager&) = delete;
	/**
	 *   @fn RegisterAllEvents
	 */
	void RegisterAllEvents ();
	/**
	 *   @fn UnRegisterAllEvents
	 */
	void UnRegisterAllEvents ();

	ThunderAccessPlayer mSecManagerObj;       /**< ThunderAccessPlayer object for communicating with SecManager*/
	ThunderAccessPlayer mWatermarkPluginObj;  /**< ThunderAccessPlayer object for communicating with Watermark Plugin Obj*/
	std::mutex mSecMutex;    	        /**< Lock for accessing mSecManagerObj*/
	std::mutex mWatMutex;		        /**< Lock for accessing mWatermarkPluginObj*/
	std::mutex mSpeedStateMutex;		/**< mutex for setPlaybackSpeedState()*/
	std::list<std::string> mRegisteredEvents;
	bool mSchedulerStarted;
};

#endif /* __PLAYER_SECMANAGER_H__ */
