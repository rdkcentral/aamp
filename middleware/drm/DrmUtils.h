/*
 * If not stated otherwise in this file or this component's license file the
 * following copyright and licenses apply:
 *
 * Copyright 2018 RDK Management
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
* @file DrmUtils.h
* @brief Data structures to help with DRM sessions.
*/

#ifndef DrmUtils_h
#define DrmUtils_h

#include <atomic>
#include <stdio.h>
#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <sstream>
#include <mutex>
#include <memory>

#include "DrmMediaFormat.h"
#include "DrmData.h"
#include "DrmInfo.h"
#include "DrmSystems.h"

/**
 * @brief Macros to track the value of API success or failure
 */
#define DRM_API_SUCCESS (0)
#define DRM_API_FAILED  (-1)

/**
 * @brief start and end tags for DRM policy
 */
#define DRM_METADATA_TAG_START "<ckm:policy xmlns:ckm=\"urn:ccp:ckm\">"
#define DRM_METADATA_TAG_END "</ckm:policy>"

typedef enum
{
	MW_DRM_INIT_FAILED,			   /**< DRM initialization failure */
	MW_DRM_DATA_BIND_FAILED,			   /**< InitData binding with DRM failed */
	MW_DRM_SESSIONID_EMPTY,			   /**< DRM session ID empty */
	MW_DRM_CHALLENGE_FAILED,			   /**< DRM key request challenge generation failed */
	MW_INVALID_DRM_KEY,			   /**< DRM reporting invalid license key */
	MW_CORRUPT_DRM_DATA,			   /**< DRM failure due to corrupt drm data, self heal might clear further errors*/
	MW_CORRUPT_DRM_METADATA,			   /**< DRM failure due to corrupt drm metadata in the stream*/
	MW_DRM_DECRYPT_FAILED,			   /**< DRM Decryption Failed for Fragments */
	MW_DRM_UNSUPPORTED,			   /**< DRM Format Unsupported */
	MW_DRM_SELF_ABORT,			   /**< Download activity is aborted by player */
	MW_DRM_KEY_UPDATE_FAILED,		   /**< Failed to process DRM key, see the error code returned from Update() for more info */
	MW_UNTRACKED_DRM_ERROR,
	MW_FAILED_TO_GET_KEYID			   /**< Failed to parse key id from init data*/
} DrmTuneFailure;
namespace DrmUtils
{
	/**
	 *  @brief	Convert endianness of 16 byte block.
	 *
	 *  @param[in]	original - Pointer to source byte block.
	 *  @param[out]	guidBytes - Pointer to destination byte block.
	 *  @return	void.
	 */
	void convertEndianness(unsigned char *original, unsigned char *guidBytes);
	/**
	 *  @fn 	extractDataFromPssh
	 *  @param[in]	psshData - Pointer to PSSH data.
	 *  @param[in]	dataLength - Length of PSSH data.
	 *  @param[in]	startStr, endStr - Pointer to delimiter strings.
	 *  @param[in]  verStr - Pointer to version delimiter string.
	 *  @param[out]	len - Gets updated with length of content meta data.
	 *  @return	Extracted data between delimiters; NULL if not found.
	 */
	unsigned char *extractDataFromPssh(const char* psshData, int dataLength, const char* startStr, const char* endStr, int *len, const char* verStr);
	/**
	 *  @fn 	extractWVContentMetadataFromPssh
	 *  @param[in]	psshData - Pointer to PSSH data.
	 *  @param[in]  dataLength - pssh data length
	 *  @return	Extracted ContentMetaData.
	 */
	std::string extractWVContentMetadataFromPssh(const char* psshData, int dataLength);

	unsigned char * extractKeyIdFromPssh(const char* psshData, int dataLength, int *len, DRMSystems drmSystem);
}
#endif
