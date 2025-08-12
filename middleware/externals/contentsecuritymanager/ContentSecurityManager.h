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
 * @file ContentSecurityManager.h
 * @brief Class to communicate with SecManager Thunder plugin
 */

#ifndef __CONTENT_SECURITY_MANAGER_H__
#define __CONTENT_SECURITY_MANAGER_H__

#include <mutex>
#include <sys/time.h>
#include "ContentSecurityManagerSession.h"
#include "PlayerScheduler.h"
#include "PlayerMemoryUtils.h"
#include "PlayerExternalUtils.h"
#include <inttypes.h>
#include <memory>
#include <list>
#include <map>
#include <vector>

//Secmanager/Content Protection error class codes
#define CONTENT_SECURITY_MANAGER_DRM_FAILURE 200
#define CONTENT_SECURITY_MANAGER_WM_FAILURE 300 	/**< If secmanager couldn't initialize watermark service */

//Secmanager/Content Protection error reason codes
#define CONTENT_SECURITY_MANAGER_DRM_GEN_FAILURE 1	/**< General or internal failure */
#define CONTENT_SECURITY_MANAGER_SERVICE_TIMEOUT 3
#define CONTENT_SECURITY_MANAGER_SERVICE_CON_FAILURE 4
#define CONTENT_SECURITY_MANAGER_SERVICE_BUSY 5
#define CONTENT_SECURITY_MANAGER_ACCTOKEN_EXPIRED 8
#define CONTENT_SECURITY_MANAGER_ENTITLEMENT_FAILURE 102

#define MAX_LICENSE_REQUEST_ATTEMPTS 2

/**
 * @class ContentSecurityManager
 * @brief Class to get License from Sec Manager
 */
class ContentSecurityManager : public PlayerScheduler
{
public:
	//allow access to ContentSecurityManager::ReleaseSession()
	friend ContentSecurityManagerSession::SessionManager::~SessionManager();
	/**
	 * @fn GetInstance
	 *
	 * @return ContentSecurityManager instance
	 */
	static ContentSecurityManager* GetInstance();

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
	virtual bool AcquireLicense( std::string clientId, std::string appId, const char* licenseUrl, const char* moneyTraceMetadata[][2],
						const char* accessAttributes[][2], const char* contentMetadata, size_t contentMetadataLen,
						const char* licenseRequest, size_t licenseRequestLen, const char* keySystemId,
						const char* mediaUsage, const char* accessToken, size_t accessTokenLen,
						ContentSecurityManagerSession &session,
						char** licenseResponse, size_t* licenseResponseLength,
						int32_t* statusCode, int32_t* reasonCode, int32_t*  businessStatus, bool isVideoMuted, int sleepTime);


	/**
	 * @fn UpdateSessionState
	 *
	 * @param[in] sessionId - session id
	 * @param[in] active - true if session is active, false otherwise
	 */
	virtual bool UpdateSessionState(int64_t sessionId, bool active);

	/**
	 * @fn setVideoWindowSize
	 *
	 * @param[in] sessionId - session id
	 * @param[in] video_width - video width 
	 * @param[in] video_height - video height 
	 */
	virtual bool setVideoWindowSize(int64_t sessionId, int64_t video_width, int64_t video_height);
	/**
	 * @fn setPlaybackSpeedState
	 *
	 * @param[in] sessionId - session id
	 * @param[in] playback_speed - playback speed 
	 * @param[in] playback_position - playback position	 
	 */
	virtual bool setPlaybackSpeedState(int64_t sessionId, int64_t playback_speed, int64_t playback_position);
	
        /**
         * @fn ReleaseSession - this should only be used by ContentSecurityManagerSession::SessionManager::~SessionManager();
         *
         * @param[in] sessionId - session id
         */
        virtual void ReleaseSession(int64_t sessionId);

	/**
	 *   @fn SendWatermarkSessionEvent_CB
	 */
	static std::function<void(uint32_t, uint32_t, const std::string&)> SendWatermarkSessionEvent_CB;

	/**
	 * @fn UseFireboltSDK
	 * @brief To indicate whether application support firebolt capability
	 */
	static void UseFireboltSDK(bool status);

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

	static std::size_t getInputSummaryHash(const char* moneyTraceMetadata[][2], const char* contentMetadata,
                                        size_t contMetaLen, const char* licenseRequest, const char* keySystemId,
                                        const char* mediaUsage, const char* accessToken, bool isVideoMuted);
protected:

	/* Run AcquireLicenseOpenOrUpdate is the old AcquireLicense code
	 * It is used by AcquireLicense() to for opening sessions & for calling update when this is required*/
	virtual bool AcquireLicenseOpenOrUpdate( std::string clientId, std::string appId, const char* licenseUrl, const char* moneyTraceMetadata[][2],
						const char* accessAttributes[][2], const char* contentMetadata, size_t contentMetadataLen,
						const char* licenseRequest, size_t licenseRequestLen, const char* keySystemId,
						const char* mediaUsage, const char* accessToken, size_t accessTokenLen,
						ContentSecurityManagerSession &session,
						char** licenseResponse, size_t* licenseResponseLength,
						int32_t* statusCode, int32_t* reasonCode, int32_t*  businessStatus, bool isVideoMuted, int sleepTime) { return false; }
        /**
         * @brief Sets DRM session state (e.g., active/inactive)
         * @param sessionId DRM session ID
         * @param active Whether the session should be marked active
         * @return true on success, false otherwise
         */
    	virtual bool SetDrmSessionState(int64_t sessionId, bool active) { return false; }
        /**
         * @brief Closes an existing DRM session
         * @param sessionId DRM session ID
         * @return true if closed successfully
         */
	virtual void CloseDrmSession(int64_t sessionId) {}
        /**
         * @brief Sets playback state for watermark alignment
         * @param sessionId Session ID
         * @param speed Playback rate (1.0 = normal)
         * @param position Current position in seconds
         * @return true if command succeeded
         */
	virtual bool SetPlaybackPosition(int64_t sessionId, float speed, int32_t position) { return false; }
	/**
	 * @fn setWindowSize
	 *
	 * @param[in] sessionId - session id
	 * @param[in] video_width - video width 
	 * @param[in] video_height - video height 
         */
        virtual bool setWindowSize(int64_t sessionId, int64_t video_width, int64_t video_height) { return false; };

	/**
	 * @fn ContentSecurityManager
	 */
	ContentSecurityManager(){};

	/**
	 * @fn ~ContentSecurityManager
	 */
	~ContentSecurityManager(){};
	/**     
     	 * @brief Copy constructor disabled
    	 *
     	 */
	ContentSecurityManager(const ContentSecurityManager&) = delete;
	/**
 	 * @brief assignment operator disabled
         *
         */
	ContentSecurityManager* operator=(const ContentSecurityManager&) = delete;
};

/**
 * @class FakeSecManager
 * @brief Dummy no-op fallback implementation for unsupported platforms
 */
class FakeSecManager : public ContentSecurityManager 
{
public:
	/**
	 * @fn FakeSecManager
	 */
	FakeSecManager() = default;

	/**
	 * @brief Destructor
	 */
	~FakeSecManager() = default;

	FakeSecManager(const FakeSecManager&) = delete;
	FakeSecManager& operator=(const FakeSecManager&) = delete;
        bool AcquireLicense( std::string clientId, std::string appId, const char* licenseUrl, const char* moneyTraceMetadata[][2],
                                                const char* accessAttributes[][2], const char* contentMetadata, size_t contentMetadataLen,
                                                const char* licenseRequest, size_t licenseRequestLen, const char* keySystemId,
                                                const char* mediaUsage, const char* accessToken, size_t accessTokenLen,
                                                ContentSecurityManagerSession &session,
                                                char** licenseResponse, size_t* licenseResponseLength,
                                                int32_t* statusCode, int32_t* reasonCode, int32_t*  businessStatus, bool isVideoMuted, int sleepTime) override
	{
		return false;
	}

	bool UpdateSessionState(int64_t sessionId, bool active) override
	{
		return false;
	}

	bool setPlaybackSpeedState(int64_t sessionId, int64_t speed, int64_t position) override
	{
		return false;
	}
        bool setVideoWindowSize(int64_t sessionId, int64_t video_width, int64_t video_height) override
	{
		return false;
	}
        void ReleaseSession(int64_t sessionId) override
	{
		
	}
};
#endif /* __CONTENT_SECURITY_MANAGER_H__ */
