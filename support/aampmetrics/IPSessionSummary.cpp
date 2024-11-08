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
 * @file IPSessionSummary.cpp
 * @brief This file handles operations on session summary report
 */

#include "IPSessionSummary.h"
#include "StatsDefine.h"

size_t CSessionSummary::totalErrorCount = 0;
/**
 *   @brief  Converts class object data to Json object
 */
cJSON * CSessionSummary::ToJson() const
{
	cJSON *monitor = NULL;

	if(isInitialized)
	{
		monitor = cJSON_CreateObject();
		if(monitor)
		{
			cJSON * jsonObj = NULL;
			for(auto it : mSessionSummaryMap)
			{
				jsonObj =  cJSON_CreateNumber(it.second);
				cJSON_AddItemToObject(monitor, it.first.c_str(), jsonObj);
			}

			if(jsonObj == NULL)
            {
				//nothing is added to monitor
				//delete monitor
				cJSON_Delete(monitor);
				monitor = NULL;
			}
		}
	}
	return monitor;
}


/**
 *   @brief  Increments Latency report count
 */
void CSessionSummary::IncrementCount(std::string response)
{
    if(isInitialized)
    {
	mSessionSummaryMap.at(response)++;
    }
}

/**
 *   @brief Update session summary report
 */
void CSessionSummary::UpdateSummary(int response, bool connectivity)
{
	if(response > 0)
	{
		// Initialize data
		if(!isInitialized)
		{
			isInitialized = true;
		}

		std::string responseStr;

		if(response == 28 || response == 18)
		{
			// breakdown of curl timeouts by type (based on connectivity)
			responseStr = std::to_string(response) + "(" + std::to_string(connectivity) + ")";
		}
		else
		{
			responseStr = std::to_string(response);
		}

		// Check whether window is already recorded
		if(mSessionSummaryMap.find(responseStr) == mSessionSummaryMap.end())
		{
			mSessionSummaryMap.insert(std::pair<std::string, unsigned int>(responseStr, DEFAULT_START_VALUE));
		}
		else
		{
			IncrementCount(responseStr);
		}
	}
}
