/*
 * If not stated otherwise in this file or this component's license file the
 * following copyright and licenses apply:
 *
 * Copyright 2022 RDK Management
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

#include "ManifestGenericStats.h"

size_t ManifestGenericStats::totalGaps = 0;

/**
 *   @brief Updated mManifestData
 */
void ManifestGenericStats::UpdateManifestData(ManifestData *manifestData)
{
	// Initialize data
	if(!isInitialized)
	{
		isInitialized = true;
	}
	if(manifestData)
	{
		mManifestData["DownloadTimeMs"] = manifestData->mDownloadTimeMs;
		mManifestData["Size"] = manifestData->mSize;
		if(-1 != manifestData->mParseTimeMs)
			mManifestData["ParseTimeMs"] = manifestData->mParseTimeMs;
		if(0 != manifestData->mPeriodCount)
			mManifestData["PeriodCount"] = manifestData->mPeriodCount;
	}
}

/**
 *   @brief Convert to json
 */
cJSON * ManifestGenericStats::ToJson() const
{
	cJSON * jsonObj = cJSON_CreateObject();
	if(isInitialized && jsonObj)
	{
		cJSON * manifest;
		for(const auto& item : mManifestData)
		{
			manifest =  cJSON_CreateNumber(item.second);
			cJSON_AddItemToObject(jsonObj, item.first.c_str(), manifest);
		}
	}
	return jsonObj;
}
