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
 * @file CLicenseStatistics.cpp
 * @brief File contains the stat about the License for encrypted fragment
 */

#include "IPLicnsStatistics.h"
#include "StatsDefine.h"



/**
 *   @brief  Converts class object data to Json object
 */
cJSON * CLicenseStatistics::ToJson() const
{
	cJSON *monitor = NULL;

	if(mTotalEncryptedToClear > 0 ||
	   mTotalClearToEncrypted > 0 	)
	{
		monitor = cJSON_CreateObject();
		if(monitor)
		{
			cJSON * jsonObj = NULL;// cJSON_CreateNumber(mTotalRotations);
			//  Commenting TAG_TOTAL_ROTATIONS License rotation is not reliable as it represent metadata change which may happen even if there is no change in key
			//cJSON_AddItemToObject(monitor, TAG_TOTAL_ROTATIONS, jsonObj);
			if(mTotalEncryptedToClear > 0)
			{
				jsonObj =  cJSON_CreateNumber(mTotalEncryptedToClear);
				cJSON_AddItemToObject(monitor, TAG_TOTAL_ENCRYPTED_TO_CLEAR, jsonObj);
			}

			if(mTotalClearToEncrypted > 0 )
			{
				jsonObj =  cJSON_CreateNumber(mTotalClearToEncrypted);
				cJSON_AddItemToObject(monitor, TAG_TOTAL_CLEAR_TO_ENCRYPTED, jsonObj);
			}

		}
	}
	return monitor;
}


/**
 *   @brief  Increments License stat count
 */
void CLicenseStatistics::IncrementCount(VideoStatCountType type)
{
    if( isInitialized )
    {
        switch (type) {
			case COUNT_LIC_TOTAL:
			{
				mTotalRotations++;
			}
				break;
			case COUNT_LIC_ENC_TO_CLR:
			{
				mTotalEncryptedToClear++;
			}
				break;
			case COUNT_LIC_CLR_TO_ENC:
			{
				mTotalClearToEncrypted++;
			}
				break;
			default:
				break;
		}
    }
}

/**
 *   @brief  Records license stat
 */
void CLicenseStatistics::Record_License_EncryptionStat(bool  isEncrypted, bool isKeyChanged)
{
    if(isInitialized)
    {
        // Encrypted to clear
        if(mbEncrypted   && !isEncrypted)
        {
            IncrementCount(VideoStatCountType::COUNT_LIC_ENC_TO_CLR);
        }

        // Clear  to Encrypted
        if(!mbEncrypted   && isEncrypted)
        {
            IncrementCount(VideoStatCountType::COUNT_LIC_CLR_TO_ENC);
        }

        if(isKeyChanged)
        {
            IncrementCount(VideoStatCountType::COUNT_LIC_TOTAL);
        }
    }

    if( !isInitialized )
    {
        isInitialized = true;
    }
    mbEncrypted = isEncrypted;
}
