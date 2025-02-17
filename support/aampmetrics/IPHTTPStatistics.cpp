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
 * @file IPHTTPStatistics.cpp
 * @brief This file handles the operations to manage the HTTP statistics for all video types
 */

#include "IPHTTPStatistics.h"
#include "StatsDefine.h"

/**
 *   @brief  Increment stat count
 */
void CHTTPStatistics::IncrementCount(long downloadTimeMs, int responseCode, bool connectivity, ManifestData * manifestData)
{
	if(responseCode != 200 && responseCode != 204 && responseCode != 206)
	{
		GetSessionSummary()->UpdateSummary(responseCode, connectivity);
		CSessionSummary::totalErrorCount++;
	}
	else
	{
		GetLatencyReport()->RecordLatency(downloadTimeMs);
		GetSessionSummary()->UpdateSummary(200, true);
	}
	
	//update manifest details if present
	if(manifestData)
	{
		GetManGenStatsInstance()->UpdateManifestData(manifestData);
	}
}


/**
 *   @brief  Converts class object data to Json object
 */
cJSON * CHTTPStatistics::ToJson() const
{

	//CID:100288 - Removed bDataAdded variable which has been initialized but not used
	cJSON *monitor = cJSON_CreateObject();
	cJSON * jsonObj =  NULL;
	if(monitor)
	{

		if(mLatencyReport)
		{
			jsonObj = mLatencyReport->ToJson();
			if(jsonObj)
			{
				cJSON_AddItemToObject(monitor, TAG_LATENCY_REPORT, jsonObj);
			}
		}

		if(mSessionSummary)
		{
			jsonObj = mSessionSummary->ToJson();
			if(jsonObj)
			{
				cJSON_AddItemToObject(monitor, TAG_SESSION_SUMMARY, jsonObj);
			}
		}
		if(g_ForPartnerApps && mManifestGenericStats)
		{
			jsonObj = mManifestGenericStats->ToJson();
			if(jsonObj)
			{
				cJSON_AddItemToObject(monitor, TAG_MANIFEST_INFO, jsonObj);
			}
		}
	}

	if(jsonObj == NULL)
	{
		//None of the data got added so delete
		cJSON_Delete(monitor);
		monitor = NULL;
	}

	return monitor;
}



