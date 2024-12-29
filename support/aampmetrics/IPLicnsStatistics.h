/*
 * If not stated otherwise in this file or this component's license file the
 * following copyright and licenses apply:
 *
 * Copyright 2020 RDK Management
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
 * @file CLicenseStatistics.h
 * @brief File contains the stat about the License for encrypted fragment
 */

// NOTE - protex_scan is failing hence changed file name from LicenseStatistics.h LicnStatistics.h

#ifndef __LICENSE_STATISTICS_H__
#define __LICENSE_STATISTICS_H__

#include "IPHTTPStatistics.h"

/**
 * @class CLicenseStatistics
 * @brief Contain the License information for encrypted contents
 */
class CLicenseStatistics
{
protected:
	int mTotalRotations; 		/**< total licence rotation/switch */
	int mTotalEncryptedToClear; 	/**< Encrypted to clear licence switch */
	int mTotalClearToEncrypted; 	/**< Clear to encrypted licence switch */
	bool mbEncrypted;
	// First call to reporting data will set this variable to true,
	// this is used to avoid recording license data of stream starts with encrypted content and never transition to clear.
	bool isInitialized;
public:
	CLicenseStatistics() :mTotalRotations(COUNT_NONE), mTotalEncryptedToClear(COUNT_NONE) , mTotalClearToEncrypted(COUNT_NONE),
	mbEncrypted(false) , isInitialized (false)
	{

	}

	/**
	 *   @fn Record_License_EncryptionStat
	 *   @param[in]  isEncrypted indicates track or fragment is encrypted, based on this info clear to enc or enc to clear stats are incremented
	 *   @param[in]  isKeyChanged indicates if keychanged for encrypted fragment
	 *   @return None
	 */
	void Record_License_EncryptionStat(bool isEncrypted, bool isKeyChanged);

	/**
	 *   @fn IncrementCount
	 *   @param[in] type  VideoStatCountType
	 *   @return None
	 */
	void IncrementCount(VideoStatCountType type);

	/**
	 *   @fn ToJson
	 *
	 *   @return cJSON pointer
	 */
	cJSON * ToJson() const;
};




#endif /* __LICENSE_STATISTICS_H__ */
