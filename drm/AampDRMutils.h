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
* @file AampDRMutils.h
* @brief Data structures to help with DRM sessions.
*/

#ifndef AampDRMutils_h
#define AampDRMutils_h

#include <stdio.h>
#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <sstream>

#include "AampDrmMediaFormat.h"
#include "AampDrmData.h"
#include "AampDrmInfo.h"
#include "AampDrmSystems.h"
#include "AampUtils.h"

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

#ifdef USE_SECMANAGER
#define AAMP_SECMGR_INVALID_SESSION_ID (-1)

/**
 * @brief To handle get/set/invalidate and enquiry about the state of a session ID from multiple threads
 */
class SessionId
{
private:
	int64_t 	mSessionId;
	std::mutex 	sessionIdMutex;

public:
	SessionId(): mSessionId(AAMP_SECMGR_INVALID_SESSION_ID), sessionIdMutex() {};

	void setSessionId(int64_t sessionId)
	{
		std::lock_guard<std::mutex>lock(sessionIdMutex);
		mSessionId=sessionId;
	}
	int64_t getSessionId(void)
	{
		std::lock_guard<std::mutex>lock(sessionIdMutex);
		return mSessionId;
	}
	bool isSessionValid(void)
	{
		std::lock_guard<std::mutex>lock(sessionIdMutex);
		return (mSessionId != AAMP_SECMGR_INVALID_SESSION_ID);
	}
	void setSessionInvalid(void)
	{
		std::lock_guard<std::mutex>lock(sessionIdMutex);
		mSessionId=AAMP_SECMGR_INVALID_SESSION_ID;
	}
};
#endif

/**
 *  @brief	Convert endianness of 16 byte block.
 *
 *  @param[in]	original - Pointer to source byte block.
 *  @param[out]	guidBytes - Pointer to destination byte block.
 *  @return	void.
 */
void aamp_ConvertEndianness(unsigned char *original, unsigned char *guidBytes);
/**
 *  @fn 	aamp_ExtractDataFromPssh
 *  @param[in]	psshData - Pointer to PSSH data.
 *  @param[in]	dataLength - Length of PSSH data.
 *  @param[in]	startStr, endStr - Pointer to delimiter strings.
 *  @param[in]  verStr - Pointer to version delimiter string.
 *  @param[out]	len - Gets updated with length of content meta data.
 *  @return	Extracted data between delimiters; NULL if not found.
 */
unsigned char *aamp_ExtractDataFromPssh(const char* psshData, int dataLength, const char* startStr, const char* endStr, int *len, const char* verStr);
/**
 *  @fn 	aamp_ExtractWVContentMetadataFromPssh
 *  @param[in]	psshData - Pointer to PSSH data.
 *  @param[in]  dataLength - pssh data length
 *  @return	Extracted ContentMetaData.
 */
std::string aamp_ExtractWVContentMetadataFromPssh(const char* psshData, int dataLength);

unsigned char * aamp_ExtractKeyIdFromPssh(const char* psshData, int dataLength, int *len, DRMSystems drmSystem);

#endif
