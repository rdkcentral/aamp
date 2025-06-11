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
 * @file ContentProtectionInterface.h
 * @brief Class to communicate with Content Protection Firebolt Interface
 */

#ifndef CONTENT_PROTECTION_PRIV_H
#define CONTENT_PROTECTION_PRIV_H

#include <mutex>
#include <sys/time.h>
#include "ContentProtectionSession.h"
#include "PlayerScheduler.h"
#include "AampLogManager.h"
#include <inttypes.h>
#include <memory>
#include <list>
#include <map>
#include <vector>

//ContentProtection error class codes
#define CONTENT_PROTECTION_DRM_FAILURE 200
#define CONTENT_PROTECTION_WM_FAILURE 300 	/**< If secmanager couldn't initialize watermark service */

//ContentProtection error reason codes
#define CONTENT_PROTECTION_DRM_GEN_FAILURE 1	/**< General or internal failure */
#define CONTENT_PROTECTION_SERVICE_TIMEOUT 3
#define CONTENT_PROTECTION_SERVICE_CON_FAILURE 4
#define CONTENT_PROTECTION_SERVICE_BUSY 5
#define CONTENT_PROTECTION_ACCTOKEN_EXPIRED 8
#define CONTENT_PROTECTION_ENTITLEMENT_FAILURE 102

/**
 * @class ContentProtectionBase
 * @brief Private class to get License from Sec Manager via ContentProtection Firebolt Plugin
 */
class ContentProtectionBase : public PlayerScheduler
{
public:
	//allow access to ContentProtection::ReleaseSession()
	friend ContentProtectionSession::SessionManager::~SessionManager();

	/**
	 * @fn AcquireLicense
	 *
	 * @param[in] licenseUrl - url to fetch license from
	 * @param[in] moneyTraceMetdata - moneytrace info
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
	virtual bool AcquireLicense(std::string clientId, std::string appId, const char* licenseUrl, const char* moneyTraceMetdata[][2],
						const char* accessAttributes[][2], const char* contentMetadata, size_t contentMetadataLen,
						const char* licenseRequest, size_t licenseRequestLen, const char* keySystemId,
						const char* mediaUsage, const char* accessToken, size_t accessTokenLen,
						ContentProtectionSession &session,
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
	 * @fn setPlaybackSpeedState
	 *
	 * @param[in] sessionId - session id
	 * @param[in] playback_speed - playback speed 
	 * @param[in] playback_position - playback position	 
	 */
	virtual bool setPlaybackSpeedState(int64_t sessionId, float speed, int32_t position);

	/**
	 * @fn getSchedulerStatus
	 *
	 * @return bool - true if scheduler is running, false otherwise
	 */
	virtual bool getSchedulerStatus();

	/**
	 * @fn setWatermarkSessionEvent_CB
	 * @param[in] callback - callback function
	 * @return void
	 * @brief Set callback function for watermark session
	 */
	static void setWatermarkSessionEvent_CB(const std::function<void(uint32_t, uint32_t, const std::string&)>& callback);
	
	/**
	 * @fn ContentProtectionBase
	 */
	ContentProtectionBase();
	/**
	 * @fn ~ContentProtectionBase
	 */
	virtual ~ContentProtectionBase();
protected:
	bool AcquireLicenseOpenOrUpdate(std::string clientId, std::string appId, const char* licenseUrl, const char* moneyTraceMetdata[][2],
						const char* accessAttributes[][2], const char* contentMetadata, size_t contentMetadataLen,
						const char* licenseRequest, size_t licenseRequestLen, const char* keySystemId,
						const char* mediaUsage, const char* accessToken, size_t accessTokenLen,
						ContentProtectionSession &session,
						char** licenseResponse, size_t* licenseResponseLength,
						int32_t* statusCode, int32_t* reasonCode, int32_t*  businessStatus, bool isVideoMuted, int sleepTime);

	/**
	 * @fn ReleaseSession - this should only be used by ContentProtectionSession::SessionManager::~SessionManager();
	 *
	 * @param[in] sessionId - session id
	 */
	void ReleaseSession(int64_t sessionId);
        /**
         * @brief Sets DRM session state (e.g., active/inactive)
         * @param sessionId DRM session ID
         * @param active Whether the session should be marked active
         * @return true on success, false otherwise
         */
    	virtual bool SetDrmSessionState(const std::string& sessionId, bool active) { return false; }
        /**
         * @brief Closes an existing DRM session
         * @param sessionId DRM session ID
         * @return true if closed successfully
         */
	virtual bool CloseDrmSession(const std::string& sessionId) { return false; }
        /**
         * @brief Opens a new DRM session
         * @param[in,out] clientId Client identifier (may be modified)
         * @param appId Application ID
         * @param keySystem DRM system string (e.g., widevine)
         * @param licenseRequest License challenge
         * @param initData Initialization data
         * @param[out] sessionId Output DRM session ID
         * @param[out] response License server response
         * @return true on success
         */
	virtual bool OpenDrmSession(std::string& clientId, std::string appId, std::string keySystem, std::string licenseRequest, std::string initData, std::string& sessionId, std::string &response) { return false;}
        /**
         * @brief Sends update license challenge to existing session
         * @param sessionId DRM session ID
         * @param licenseRequest Challenge string
         * @param initData Initialization data
         * @param[out] response Response from Firebolt
         * @return true on success
         */
	virtual bool UpdateDrmSession(const std::string& sessionId, std::string licenseRequest, std::string initData, std::string &response) { return false; }
        /**
         * @brief Sets playback state for watermark alignment
         * @param sessionId Session ID
         * @param speed Playback rate (1.0 = normal)
         * @param position Current position in seconds
         * @return true if command succeeded
         */
	virtual bool SetPlaybackPosition(const std::string& sessionId, float speed, int32_t position) { return false; }

	/**
	 *   @fn SendWatermarkSessionEvent_CB
	 */
	static std::function<void(uint32_t, uint32_t, const std::string&)> SendWatermarkSessionEvent_CB;

	/**
	 * @fn getWatermarkSessionEvent_CB
	 * @return std::function<void(uint32_t, uint32_t, const std::string&)>&
	 * @brief Get callback function for watermark session
	 */
	static std::function<void(uint32_t, uint32_t, const std::string&)>& getWatermarkSessionEvent_CB( );

	std::mutex mCPMutex;    	        /**< Lock for accessing mCPManagerObj*/
	std::list<std::string> mRegisteredEvents;
	bool mSchedulerStarted;
    ContentProtectionBase* mCallbackClient = nullptr;
    std::mutex mCallbackMutex;

};

/**
 * @class ContentProtection
 * @brief Singleton wrapper class to access ContentProtectionBase implementation
 */
 class ContentProtection
 {
 public:
	 /**
	  * @fn GetInstance
	  *
	  * @return ContentProtection - singleton instance
	  */
	 static ContentProtectionBase * GetInstance();
 
	 /**
	  * @fn DestroyInstance
	  *
	  * @return void
	  */
	 static void DestroyInstance();
	/**
	 * @brief Indicates if content protection feature is enabled at build time
	 */	 
	 static bool ContentProtectionEnabled(); 
 private:
	 static ContentProtectionBase *mInstance; /**< Singleton instance */
 };

/**
 * @class FakeContentProtection
 * @brief Dummy no-op fallback implementation for unsupported platforms
 */
class FakeContentProtection : public ContentProtectionBase
{
public:
	/**
	 * @fn FakeContentProtection
	 */
	FakeContentProtection() = default;

	/**
	 * @brief Destructor
	 */
	~FakeContentProtection() = default;

	FakeContentProtection(const FakeContentProtection&) = delete;
	FakeContentProtection& operator=(const FakeContentProtection&) = delete;
	bool AcquireLicense(std::string clientId, std::string appId, const char* licenseUrl, const char* moneyTraceMetdata[][2],
						const char* accessAttributes[][2], const char* contentMetadata, size_t contentMetadataLen,
						const char* licenseRequest, size_t licenseRequestLen, const char* keySystemId,
						const char* mediaUsage, const char* accessToken, size_t accessTokenLen,
						ContentProtectionSession &session,
						char** licenseResponse, size_t* licenseResponseLength,
						int32_t* statusCode, int32_t* reasonCode, int32_t* businessStatus, bool isVideoMuted, int sleepTime) override
	{
		return false;
	}

	bool UpdateSessionState(int64_t sessionId, bool active) override
	{
		return false;
	}

	bool setPlaybackSpeedState(int64_t sessionId, float speed, int32_t position) override
	{
		return false;
	}

	bool getSchedulerStatus() override
	{
		return false;
	}

};

#endif /* CONTENT_PROTECTION_PRIV_H */