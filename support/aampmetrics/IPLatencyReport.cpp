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
 * @file IPLatencyReport.h
 * @brief This file holds information about Latency report
 */

#include "IPLatencyReport.h"
#include "StatsDefine.h"


/**
 *   @brief  Converts class object data to Json object
 */
cJSON * CLatencyReport::ToJson() const
{
	cJSON *monitor = NULL;

	if(isInitialized)
	{
		monitor = cJSON_CreateObject();
		if(monitor)
		{
			cJSON * jsonObj = NULL;
			for(auto it : mLatencyReportMap)
			{
				jsonObj =  cJSON_CreateNumber(it.second);
				cJSON_AddItemToObject(monitor, (TAG_TIME_WINDOW_PREFIX + it.first).c_str(), jsonObj);
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
void CLatencyReport::IncrementCount(std::string window)
{
    if(isInitialized)
    {
		mLatencyReportMap.at(window)++;
    }
}


/**
 *   @brief Calculates time window for latency report
 */
std::string CLatencyReport::GetTimeWindow(long timeMs)
{
	std::string window;
	if(isInitialized)
	{
		window = std::to_string(timeMs / LATENCY_WINDOW_BUCKET_DURATION);
	}
	return window;
}


/**
 *   @brief Record Latency Report
 */
void CLatencyReport::RecordLatency(long timeMs)
{
	// Initialize data
	if(!isInitialized)
	{
		isInitialized = true;
	}

	std::string window = GetTimeWindow(timeMs);
	if(!window.empty())
	{
		// Check whether window is already recorded
		if(mLatencyReportMap.find(window) == mLatencyReportMap.end())
		{
			mLatencyReportMap.insert(std::pair<std::string, unsigned int>(window, DEFAULT_START_VALUE));
		}
		else
		{
			IncrementCount(window);
		}
	}
}
