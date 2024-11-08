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

#include <stdio.h>
#include <map>
#include <string>
#include <cjson/cJSON.h>
#include <string.h>
#include <cmath>
#ifndef __LATENCY_REPORT_H__
#define __LATENCY_REPORT_H__


typedef std::map<std::string, unsigned int>  MapLatencyReport; // Latency Report stats for each profile

/**
 * @class CLatencyReport
 * @brief class to hold information about Latency report
 */
class CLatencyReport
{
protected:
	bool isInitialized;
	MapLatencyReport mLatencyReportMap;
public:
	CLatencyReport() : isInitialized(false), mLatencyReportMap()
	{
	}

	/**
	 *   @fn IncrementCount
	 *   @param[in] window time window
	 *   @return None
	 */
	void IncrementCount(std::string window);

	/**
	 *   @fn GetTimeWindow
	 *   @param[in] timeMs time value
	 *   @return string time window tag.
	 */
	std::string GetTimeWindow(long timeMs);

	/**
	 *   @fn ToJson
	 *   @param[in]  NONE
	 *   @return cJSON pointer
	 */
	cJSON * ToJson() const;

	/**
	 *   @fn RecordLatency
	 *   @param[in] timeMs time in milli seconds
	 *
	 *   @return NONE.
	 */
	void RecordLatency(long timeMs);
};




#endif /* __LATENCY_REPORT_H__ */
