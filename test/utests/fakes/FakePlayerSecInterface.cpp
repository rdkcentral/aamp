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
 * @file PlayerSecInterface.cpp
 * @brief Interface for PlayerSec client
 */


#include "PlayerSecInterface.h"

/**
 *	@brief Check if sec feature is enabled
 */
bool isSecFeatureEnabled() {
    return false;
}

/**
 *	@brief Check if sec manager is enabled
 */
bool isSecManagerEnabled() {
    return false;
}

/**
 * @brief Convert the secclient DRM error code into secmanager error code to have a unified verbose error reported
 */
bool getAsVerboseErrorCode(int32_t httpCode, int32_t &secManagerClass, int32_t &secManagerReasonCode )
{
	return false;
}

/**
 *	@brief Acquire license via sec client
 */
int32_t PlayerSecInterface::PlayerSec_AcquireLicense(const char *serviceHostUrl, uint8_t numberOfRequestMetadataKeys,
									const char *requestMetadata[][2], uint8_t numberOfAccessAttributes,
									const char *accessAttributes[][2], const char *contentMetadata,
									size_t contentMetadataLength, const char *licenseRequest,
									size_t licenseRequestLength, const char *keySystemId,
									const char *mediaUsage, const char *accessToken,
									char **licenseResponse, size_t *licenseResponseLength,
									uint32_t *refreshDurationSeconds,
									PlayerSecExtendedStatus *statusInfo)
{
	return 0;
}

/**
 *	@brief Free resource
 */
int32_t PlayerSecInterface::PlayerSec_FreeResource(const char *resource)
{
	return 0;
}

/**
 *	@brief Check if sec request failed
 */
bool PlayerSecInterface::isSecRequestFailed(int32_t requestResult)
{
	return 0;
}

/**
 *	@brief Check if sec request result is in range
 */
bool PlayerSecInterface::isSecResultInRange(int32_t requestResult)
{
	return 0;
}

