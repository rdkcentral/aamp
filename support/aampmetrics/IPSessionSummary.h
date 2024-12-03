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
 * @file IPSessionSummary.h
 * @brief This file handles information on session summary report
 */

#include <stdio.h>
#include <map>
#include <string>
#include <cjson/cJSON.h>
#include <string.h>

#ifndef __SESSION_SUMMARY_H__
#define __SESSION_SUMMARY_H__

typedef std::map<std::string, unsigned int>  MapSessionSummary; // Session summary stats for each profile

/**
 * @class CSessionSummary
 * @brief Holds the information about session summary report
 */
class CSessionSummary
{
protected:
	bool isInitialized;
	MapSessionSummary mSessionSummaryMap;
public:
	CSessionSummary() : isInitialized(false), mSessionSummaryMap()
	{
	}
	//Keeps track of all the error counts across profiles
	static size_t totalErrorCount;
	/**
	 *   @fn IncrementCount
	 *   @param[in] response HTTP/CURL response
	 *   @return None
	 */
	void IncrementCount(std::string response);

	/**
	 *   @fn ToJson
	 *   @return cJSON pointer
	 */
	cJSON * ToJson() const;

	/**
	 *   @fn UpdateSummary
	 *   @param[in] response, http/curl response
	 *   @param[in] connectivity - connection status flag
	 *   @return NONE.
	 */
	void UpdateSummary(int response, bool connectivity);
};




#endif /* __SESSION_SUMMARY_H__ */
