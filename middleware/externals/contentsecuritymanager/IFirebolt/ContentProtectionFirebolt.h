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
 * @file ContentProtectionFirebolt.h
 * @brief Class to communicate with Content Protection Firebolt SDK
 */
#ifndef CONTENT_PROTECTION_FIREBOLT_H
#define CONTENT_PROTECTION_FIREBOLT_H

#include "ContentSecurityManager.h"
#include "ContentSecurityManagerSession.h"

#include <iostream>
#include <list>
#include <mutex>
#include <optional>
#include <cassert>
#include <string>
#include <memory>

class FireboltInterface; //forward declaration

/**
 * @class ContentProtectionFirebolt
 * @brief Provides integration with Firebolt DRM SDK for content protection
 */
class ContentProtectionFirebolt : public ContentSecurityManager
{
public:
	/**
	 * @brief Default constructor
	 */
	ContentProtectionFirebolt();
	/**
	 * @brief Destructor
	 */
	~ContentProtectionFirebolt() ;
	ContentProtectionFirebolt(const ContentProtectionFirebolt&) = delete;
	ContentProtectionFirebolt& operator=(const ContentProtectionFirebolt&) = delete;
	/**
	 * @brief Initializes the Firebolt client
	 */
	void Initialize();
	/**
	 * @brief De-initializes and tears down connection
	 */
	void DeInitialize();
	/**
	 * @brief Checks if Firebolt is active
	 * @param force
	 * @return true if initialized
	 */
	bool IsActive(bool force=false);
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
	bool OpenDrmSession(std::string& clientId, std::string appId, std::string keySystem, std::string licenseRequest, std::string initData, int64_t &sessionId, std::string &response);
	/**
	 * @brief Sends update license challenge to existing session
	 * @param sessionId DRM session ID
	 * @param licenseRequest Challenge string
	 * @param initData Initialization data
	 * @param[out] response Response from Firebolt
	 * @return true on success
	 */	
	bool UpdateDrmSession(int64_t sessionId, std::string licenseRequest, std::string initData, std::string &response);
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
	void ShowWatermark(bool show, int64_t sessionId);

	void HandleWatermarkEvent(const std::string& sessionId, const std::string& statusStr, const std::string& appId);
private:
	/**
	 * @brief Subscribes to Firebolt events (currently stub)
	 * @return true if stub accepted
	 */
	void SubscribeEvents();
	/**
	 * @brief Unsubscribe from Firebolt events (currently stub)
	 * @return true if stub accepted
	 */
	void UnSubscribeEvents();
	std::mutex mFireboltInitMutex;
	std::mutex mContentProtectionMutex;
	std::mutex mSpeedStateMutex;
	bool mInitialized;
	static uint64_t mSubscriptionId;
	std::shared_ptr<FireboltInterface> m_pFireboltInterface;
};
#endif /* CONTENT_PROTECTION_FIREBOLT_H */
