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
 * @file SecManagerThunder.h
 * @brief Class to communicate with SecManager Thunder plugin
 */

#ifndef __SECMANAGER_THUNDER_H__
#define __SECMANAGER_THUNDER_H__

#include <mutex>
#include <sys/time.h>
#include "ContentSecurityManagerSession.h"
#include "PlayerScheduler.h"
#include "PlayerMemoryUtils.h"
#include "ThunderAccessPlayer.h"
#include <inttypes.h>
#include <memory>
#include <list>
#include <map>
#include <vector>

#define SECMANAGER_CALL_SIGN "org.rdk.SecManager.1"
#define WATERMARK_PLUGIN_CALLSIGN "org.rdk.Watermark.1"
//#define RDKSHELL_CALLSIGN "org.rdk.RDKShell.1"   //need to be used instead of WATERMARK_PLUGIN_CALLSIGN if RDK Shell is used for rendering watermark

/**
 * @class SecManagerThunder
 * @brief Class to get License from Sec Manager Thunder
 */
class SecManagerThunder : public ContentSecurityManager
{
public:
        /**
         * @brief Sets DRM session state (e.g., active/inactive)
         * @param sessionId DRM session ID
         * @param active Whether the session should be marked active
         * @return true on success, false otherwise
         */
        bool SetDrmSessionState(int64_t sessionId, bool active) override;
        /**
         * @brief Closes an existing DRM session
         * @param sessionId DRM session ID
         * @return true if closed successfully
         */
        void CloseDrmSession(int64_t sessionId) override;
        /* Run AcquireLicenseOpenOrUpdate is the old AcquireLicense code
         * It is used by AcquireLicense() to for opening sessions & for calling update when this is required*/
        bool AcquireLicenseOpenOrUpdate( std::string clientId, std::string appId, const char* licenseUrl, const char* moneyTraceMetadata[][2],
                                                const char* accessAttributes[][2], const char* contentMetadata, size_t contentMetadataLen,
                                                const char* licenseRequest, size_t licenseRequestLen, const char* keySystemId,
                                                const char* mediaUsage, const char* accessToken, size_t accessTokenLen,
                                                ContentSecurityManagerSession &session,
                                                char** licenseResponse, size_t* licenseResponseLength,
                                                int32_t* statusCode, int32_t* reasonCode, int32_t*  businessStatus, bool isVideoMuted, int sleepTime) override;
        /**
         * @brief Sends update license challenge to existing session
         * @param sessionId DRM session ID
         * @param licenseRequest Challenge string
         * @param initData Initialization data
         * @param[out] response Response from Firebolt
         * @return true on success
         */
        /**
         * @brief Sets playback state for watermark alignment
         * @param sessionId Session ID
         * @param speed Playback rate (1.0 = normal)
         * @param position Current position in seconds
         * @return true if command succeeded
         */
        bool SetPlaybackPosition(int64_t sessionId, float speed, int32_t position) override;
        /**
         * @brief Enables or disables visual watermark
         * @param show Show/hide flag
         * @param sessionId Session context (optional)
         */
        void ShowWatermark(bool show);
	/**
	 * @fn setWindowSize
	 *
	 * @param[in] sessionId - session id
	 * @param[in] video_width - video width 
	 * @param[in] video_height - video height 
	 */
	bool setWindowSize(int64_t sessionId, int64_t video_width, int64_t video_height);

	/**
	 * @fn getSchedulerStatus
	 *
	 * @return bool - true if scheduler is running, false otherwise
	 */
	bool getSchedulerStatus();
        /**
         * @fn SecManagerThunder
         */
        SecManagerThunder();
        /**
         * @fn ~SecManagerThunder
         */
        ~SecManagerThunder();
        /**
         * @brief Copy constructor disabled
         */
        SecManagerThunder(const SecManagerThunder&) = delete;
        /**
         * @brief assignment operator disabled
         */
        SecManagerThunder* operator=(const SecManagerThunder&) = delete;
protected:
	/*Event Handlers*/
	/**
	 *   @fn watermarkSessionHandler
	 *   @param  parameters - i/p JsonObject params
	 */
	void watermarkSessionHandler(const JsonObject& parameters);
	/**
	 *   @fn addWatermarkHandler
	 *   @param  parameters - i/p JsonObject params
	 */
	void addWatermarkHandler(const JsonObject& parameters);
	/**
	 *   @fn updateWatermarkHandler
	 *   @param  parameters - i/p JsonObject params
	 */
	void updateWatermarkHandler(const JsonObject& parameters);
	/**
	 *   @fn removeWatermarkHandler
	 *   @param  parameters - i/p JsonObject params
	 */
	void removeWatermarkHandler(const JsonObject& parameters);
	/**
	 *   @fn showWatermarkHandler
	 *   @param parameters - i/p JsonObject params	 
	 */
	void showWatermarkHandler(const JsonObject& parameters);
        /**
         * @fn loadClutWatermark
         * @param[in] sessionId - session id
         *  
         */
        bool loadClutWatermark(int64_t sessionId, int64_t graphicId, int64_t watermarkClutBufferKey, int64_t watermarkImageBufferKey, int64_t clutPaletteSize, const char* clutPaletteFormat, int64_t watermarkWidth, int64_t watermarkHeight, float aspectRatio);
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

#endif /* __SECMANAGER_THUNDER_H__ */
