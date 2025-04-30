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
#ifndef PLAYER_SEC_INTERFACE_H
#define PLAYER_SEC_INTERFACE_H

/**
 * @file PlayerSecInterface.h
 * @brief Log manager for Player Interface
 */

#include <stdio.h>
#include <iostream>
#include <map>
#include <cstdint>

typedef enum {
	SECMANAGER_CLASS_RESULT_SUCCESS = 0,
	SECMANAGER_CLASS_RESULT_API_FAIL = 100,
	SECMANAGER_CLASS_RESULT_DRM_FAIL = 200,
	SECMANAGER_CLASS_RESULT_WATERMARK_FAIL = 300,
	SECMANAGER_CLASS_RESULT_SECCLIENT_FAIL = 400,
	SECMANAGER_CLASS_RESULT_UNDEFINED = 9999
} SecManagerResultClassStatusCode;

typedef enum {
	SECMANAGER_SUCCESS = 0,
	SECMANAGER_SUCCESS_WATERMARK_SESSION_ENGAGED = 100,
	SECMANAGER_SUCCESS_WATERMARK_NOT_REQUIRED = 101
} SecManagerResultSuccessCode;

typedef enum {
	SECMANAGER_REASON_DRM_GENERAL_FAILURE = 1,
	SECMANAGER_REASON_DRM_NO_PLAYBACK_SESSION = 2,
	SECMANAGER_REASON_DRM_LICENSE_TIMEOUT = 3,
	SECMANAGER_REASON_DRM_LICENSE_NETWORK_FAIL = 4,
	SECMANAGER_REASON_DRM_LICENSE_BUSY = 5,
	SECMANAGER_REASON_DRM_ACCESS_TOKEN_ERROR = 6,
	SECMANAGER_REASON_DRM_ACCESS_TOKEN_IP_DIFF = 7,
	SECMANAGER_REASON_DRM_ACCESS_TOKEN_EXPIRED = 8,
	SECMANAGER_REASON_DRM_DEVICE_TOKEN_EXPIRED = 9,
	SECMANAGER_REASON_DRM_MAC_TOKEN_MISSING = 10,
	SECMANAGER_REASON_DRM_MAC_TOKEN_NO_PROV = 11,
	SECMANAGER_REASON_DRM_MEMORY_ALLOCATION_ERROR = 12,
	SECMANAGER_REASON_DRM_SECAPI_USAGE_FAILURE = 13,
	SECMANAGER_REASON_DRM_PERMISSION_DENIED = 100,
	SECMANAGER_REASON_DRM_RULE_ERROR = 101,
	SECMANAGER_REASON_DRM_ENTITLEMENT_ERROR = 102,
	SECMANAGER_REASON_DRM_AUTHENTICATION_FAIL = 103
} SecManagerResultDRMCode;

typedef enum {
	SECMANAGER_REASON_API_INVALID_SESSION_CONFIG = 1,
	SECMANAGER_REASON_API_INVALID_ASPECT_DIMENSION = 2,
	SECMANAGER_REASON_API_INVALID_KEY_SYSTEM_PARAM = 3,
	SECMANAGER_REASON_API_INVALID_DRM_LICENSE_PARAM = 4,
	SECMANAGER_REASON_API_INVALID_CONTENT_METADATA = 5,
	SECMANAGER_REASON_API_INVALID_MEDIA_USAGE = 6,
	SECMANAGER_REASON_API_INVALID_ACCESS_TOKEN = 7,
	SECMANAGER_REASON_API_INVALID_ACCESS_ATTRIBUTE = 8,
	SECMANAGER_REASON_API_INVALID_SESSION_ID = 9,
	SECMANAGER_REASON_API_INVALID_APPLICATION_ID = 10,
	SECMANAGER_REASON_API_INVALID_EVENT_ID = 11,
	SECMANAGER_REASON_API_INVALID_CLIENT_ID = 12,
	SECMANAGER_REASON_API_INVALID_PERCEPTION_ID = 13,
	SECMANAGER_REASON_API_INVALID_WATERMARK_PARAMETER = 14,
	SECMANAGER_REASON_API_INVALID_CONTENT_PARAMETER = 15,
	SECMANAGER_REASON_API_UNDEFINED_ERROR = 9999
} SecManagerResultApiCode;

/**
 *  @struct	PlayerSecExtendedStatus
 *  @brief	Structure to hold status info
 */
struct PlayerSecExtendedStatus
{
	int accessAttributeStatus;
	int statusCode;
};

/**
 * @fn isSecFeatureEnabled
 * @brief check if sec feature is enabled
 * 
 * @return bool
 */
bool isSecFeatureEnabled();

/**
 * @fn DrmMetaDataEvent::getAsVerboseErrorCode
 * @brief get as verbose error code
 * 
 * @param httpCode - http code
 * @param secManagerClass - sec manager class
 * @param secManagerReasonCode - sec manager reason code
 * 
 * @return bool
 */
bool getAsVerboseErrorCode(int32_t httpCode, int32_t &secManagerClass, int32_t &secManagerReasonCode );

/**
 * @class PlayerSecInterface 
 * @brief PlayerSecInterface Class
 */
class PlayerSecInterface 
{
public :
	/**
	 * @fn PlayerSecInterface
	 * 
	 * @brief acquire license via sec client
	 * @param[in] serviceHostUrl - service host url
	 * @param[in] numberOfRequestMetadataKeys - number of request metadata keys
	 * @param[in] requestMetadata - request metadata
	 * @param[in] numberOfAccessAttributes - number of access attributes
	 * @param[in] accessAttributes - access attributes
	 * @param[in] contentMetadata - content metadata
	 * @param[in] contentMetadataLength - content metadata length
	 * @param[in] licenseRequest - license request
	 * @param[in] licenseRequestLength - license request length
	 * @param[in] keySystemId - key system id
	 * @param[in] mediaUsage - media usage
	 * @param[in] accessToken - access token
	 * @param[out] licenseResponse - license response
	 * @param[out] licenseResponseLength - license response length
	 * @param[out] refreshDurationSeconds - refresh duration seconds
	 * @param[out] statusInfo - status info
	 * 
	 * @return int32_t
	 */
	 
	int32_t PlayerSec_AcquireLicense( const char *serviceHostUrl, uint8_t numberOfRequestMetadataKeys,
									 const char *requestMetadata[][2], uint8_t numberOfAccessAttributes,
									 const char *accessAttributes[][2], const char *contentMetadata,
									 size_t contentMetadataLength, const char *licenseRequest,
									 size_t licenseRequestLength, const char *keySystemId,
									 const char *mediaUsage, const char *accessToken,
									 char **licenseResponse, size_t *licenseResponseLength,
									 uint32_t *refreshDurationSeconds,
									 PlayerSecExtendedStatus *statusInfo );
	/**
	 * @fn PlayerSec_FreeResource
	 *
	 * @param[in] resource - resource
	 * @return int32_t
	 */
	int32_t PlayerSec_FreeResource( const char *resource );
	
	/**
	 * @fn isSecRequestFailed
	 * @brief check if sec request failed
	 * 
	 * @param requestResult - request result
	 * 
	 * @return bool
	 */
	bool isSecRequestFailed( int32_t requestResult );

	/**
	 * @fn isSecResultInRange
	 * 
	 * @brief check if sec result is in range
	 * @param requestResult - request result
	 *
	 * @return bool
	 */
	bool isSecResultInRange( int32_t requestResult );
};

#endif /* PLAYER_SEC_INTERFACE_H */