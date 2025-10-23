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

typedef enum {
    // API Errors
    CONTENT_PROTECTION_SERVICE_INVALID_SESSION_CONFIG = 21001,
    CONTENT_PROTECTION_SERVICE_INVALID_ASPECT_DIMENSIONS = 21002,
    CONTENT_PROTECTION_SERVICE_INVALID_KEY_SYSTEM = 21003,
    CONTENT_PROTECTION_SERVICE_INVALID_LICENSE_REQUEST = 21004,
    CONTENT_PROTECTION_SERVICE_INVALID_CONTENT_METADATA = 21005,
    CONTENT_PROTECTION_SERVICE_INVALID_MEDIA_USAGE = 21006,
    CONTENT_PROTECTION_SERVICE_INVALID_ACCESS_TOKEN = 21007,
    CONTENT_PROTECTION_SERVICE_INVALID_ACCESS_ATTRIBUTES = 21008,
    CONTENT_PROTECTION_SERVICE_INVALID_SESSION_ID = 21009,
    CONTENT_PROTECTION_SERVICE_INVALID_APP_ID = 21010,
    CONTENT_PROTECTION_SERVICE_INVALID_EVENT_ID = 21011,
    CONTENT_PROTECTION_SERVICE_INVALID_CLIENT_ID = 21012,
    CONTENT_PROTECTION_SERVICE_INVALID_PERCEPTION_ID = 21013,
    CONTENT_PROTECTION_SERVICE_INVALID_WATERMARKING_PARAM = 21014,
    CONTENT_PROTECTION_SERVICE_INVALID_CONTENT_ATTRIBUTES = 21015,
    CONTENT_PROTECTION_SERVICE_INVALID_ACCOUNT_TOKEN = 21016,
    CONTENT_PROTECTION_SERVICE_INVALID_APP_CERT_CALLBACK = 21017,
    CONTENT_PROTECTION_SERVICE_INVALID_RSA_SIGNATURE_CALLBACK = 21018,
    CONTENT_PROTECTION_SERVICE_INVALID_ACCOUNT_SOURCE_CALLBACK = 21019,

    // DRM Errors
    CONTENT_PROTECTION_SERVICE_DRM_GENERAL_FAILURE = 22001,
    CONTENT_PROTECTION_SERVICE_DRM_SESSION_NOT_FOUND = 22002,
    CONTENT_PROTECTION_SERVICE_DRM_LICENSE_NW_TIMEOUT = 22003,
    CONTENT_PROTECTION_SERVICE_DRM_LICENSE_NW_CONNECTION_FAILURE = 22004,
    CONTENT_PROTECTION_SERVICE_DRM_LICENSE_SERVER_BUSY = 22005,
    CONTENT_PROTECTION_SERVICE_DRM_ACCESS_TOKEN_ACQUISITION_FAILED = 22006,
    CONTENT_PROTECTION_SERVICE_DRM_ACCESS_TOKEN_IP_MISMATCH = 22007,
    CONTENT_PROTECTION_SERVICE_DRM_ACCESS_TOKEN_EXPIRED = 22008,
    CONTENT_PROTECTION_SERVICE_DRM_DEVICE_TOKEN_EXPIRED = 22009,
    CONTENT_PROTECTION_SERVICE_DRM_MAC_TOKEN_MISSING_OR_EXPIRED = 22010,
    CONTENT_PROTECTION_SERVICE_DRM_MAC_KEY_NOT_PROVISIONED = 22011,
    CONTENT_PROTECTION_SERVICE_DRM_MEMORY_ALLOCATION_ERROR = 22012,
    CONTENT_PROTECTION_SERVICE_DRM_SECURITY_API_FAILURE = 22013,
    CONTENT_PROTECTION_SERVICE_DRM_CLIENT_TRANSACTION_ERROR = 22014,
    CONTENT_PROTECTION_SERVICE_DRM_LICENSE_AUTHORIZATION_FAILED = 22015,
    CONTENT_PROTECTION_SERVICE_DRM_ENTITLEMENT_FAILURE = 22016,
    CONTENT_PROTECTION_SERVICE_DRM_AUTHENTICATION_FAILURE = 22017,
    CONTENT_PROTECTION_SERVICE_DRM_MISSING_PARTNER_CONTEXT = 22018,

    // Watermarking Errors
    CONTENT_PROTECTION_SERVICE_WATERMARK_GENERAL_FAILURE = 23001,
    CONTENT_PROTECTION_SERVICE_WATERMARK_SESSION_DENIED = 23002,
    CONTENT_PROTECTION_SERVICE_WATERMARK_TIMEOUT = 23003,
    CONTENT_PROTECTION_SERVICE_WATERMARK_CONNECTION_FAILURE = 23004,
    CONTENT_PROTECTION_SERVICE_WATERMARK_SERVICE_BUSY = 23005,
    CONTENT_PROTECTION_SERVICE_WATERMARK_PROTOCOL_ERROR = 23006,
    CONTENT_PROTECTION_SERVICE_WATERMARK_IMAGE_TAMPERING = 23007,
    CONTENT_PROTECTION_SERVICE_WATERMARK_REQUEST_TAMPERING = 23008,
    CONTENT_PROTECTION_SERVICE_WATERMARK_RESPONSE_TAMPERING = 23009,
    CONTENT_PROTECTION_SERVICE_WATERMARK_INVALID_ACCOUNT = 23010,
    CONTENT_PROTECTION_SERVICE_WATERMARK_INVALID_ACCESS_TOKEN = 23011,
    CONTENT_PROTECTION_SERVICE_WATERMARK_MEMORY_ALLOCATION_ERROR = 23012,
    CONTENT_PROTECTION_SERVICE_WATERMARK_SECURITY_API_FAILURE = 23013,
    CONTENT_PROTECTION_SERVICE_WATERMARK_PERCEPTIBILITY_ERROR = 23014
}ContentProtectionSecurityPluginErrorCode;

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
	bool OpenDrmSession(std::string& clientId, std::string appId, std::string keySystem, std::string licenseRequest, std::string initData, int64_t &sessionId, int32_t &errorCode, std::string &response);
	/**
	 * @brief Sends update license challenge to existing session
	 * @param sessionId DRM session ID
	 * @param licenseRequest Challenge string
	 * @param initData Initialization data
	 * @param[out] response Response from Firebolt
	 * @return true on success
	 */	
	bool UpdateDrmSession(int64_t sessionId, int32_t &errorCode, std::string licenseRequest, std::string initData, std::string &response);
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
