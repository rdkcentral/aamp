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

#ifndef __HTTP_STATISTICS_H__
#define __HTTP_STATISTICS_H__

#include "IPLatencyReport.h"
#include "IPSessionSummary.h"
#include "ManifestGenericStats.h"

#define COUNT_NONE	0
#define VIDEO_END_DATA_VERSION		"2.0"

/**
 * @enum VideoStatCountType
 * @brief Defines Video Stat count type
 */
typedef enum E_VideoStatCountType {
	COUNT_UNKNOWN,
	COUNT_LIC_TOTAL,
	COUNT_LIC_ENC_TO_CLR,
	COUNT_LIC_CLR_TO_ENC,
} VideoStatCountType;


/**
 * @class CHTTPStatistics
 * @brief Class to store all Video stats common to all download types
 */
class CHTTPStatistics
{
protected:
	CSessionSummary* mSessionSummary;
	CLatencyReport* mLatencyReport;
	ManifestGenericStats* mManifestGenericStats;
public:
	CHTTPStatistics() : mSessionSummary(NULL), mLatencyReport(NULL), mManifestGenericStats(NULL)
	{

	}

	~CHTTPStatistics()
	{
		if(mSessionSummary)
		{
			delete mSessionSummary;
			mSessionSummary = NULL;
		}
		if(mLatencyReport)
		{
			delete mLatencyReport;
			mLatencyReport = NULL;
		}
		if(mManifestGenericStats)
		{
			delete mManifestGenericStats;
			mManifestGenericStats = NULL;
		}
	}

	CHTTPStatistics(const CHTTPStatistics& newObj): CHTTPStatistics()
	{
		if(newObj.mSessionSummary)
		{
			mSessionSummary = new CSessionSummary(*newObj.mSessionSummary);
		}
		if(newObj.mLatencyReport)
		{
			mLatencyReport = new CLatencyReport(*newObj.mLatencyReport);
		}
		if(newObj.mManifestGenericStats)
		{
			mManifestGenericStats = new ManifestGenericStats(*newObj.mManifestGenericStats);
		}
	}

	CHTTPStatistics& operator=(const CHTTPStatistics& newObj)
	{
		if(newObj.mSessionSummary)
		{
			mSessionSummary = new CSessionSummary(*newObj.mSessionSummary);
		}
		else
		{
			if(mSessionSummary)
			{
				delete mSessionSummary;
				mSessionSummary = NULL;
			}
		}
		if(newObj.mLatencyReport)
		{
			mLatencyReport = new CLatencyReport(*newObj.mLatencyReport);
		}
		else
		{
			if(mLatencyReport)
			{
				delete mLatencyReport;
				mLatencyReport = NULL;
			}
		}
		
		if(newObj.mManifestGenericStats)
		{
			mManifestGenericStats = new ManifestGenericStats(*newObj.mManifestGenericStats);
		}
		else
		{
			if(mManifestGenericStats)
			{
				delete mManifestGenericStats;
				mManifestGenericStats = NULL;
			}
		}
		return *this;
	}

	/**
	 *   @brief Returns Latency report and allocates if not allocated.
	 *   @param[in]  NONE
	 *   @return CLatencyReport pointer
	 */
	CLatencyReport * GetLatencyReport()
	{
		if(!mLatencyReport)
		{
			mLatencyReport = new CLatencyReport();
		}
		return mLatencyReport;
	}

	/**
	 *   @brief Returns session summary and allocates if not allocated.
	 *   @param[in]  NONE
	 *   @return CSessionSummary pointer
	 */
	CSessionSummary * GetSessionSummary()
	{
		if(!mSessionSummary)
		{
			mSessionSummary = new CSessionSummary();
		}
		return mSessionSummary;
	}

	/**
	 *   @brief Returns ManifestGenericStats instance  and allocates if not allocated.
	 *   @param[in]  NONE
	 *   @return ManifestGenericStats pointer
	 */
	ManifestGenericStats * GetManGenStatsInstance()
	{
		if(!mManifestGenericStats)
		{
			mManifestGenericStats = new ManifestGenericStats();
		}
		return mManifestGenericStats;
	}
	
	/**
	 *   @fn IncrementCount
	 *
	 *   @param[in]  download time
	 *   @param[in]  HTTP/CURL response code
	 *   @param[in] bool - connection status flag
	 *   @param[in] manifestData - connection status flag
	 *   @return void
	 */
	void IncrementCount(long downloadTimeMs, int responseCode, bool connectivity, ManifestData * manifestData = nullptr);

	/**
	 *   @fn ToJson
	 *
	 *   @param[in]  NONE
	 *
	 *   @return cJSON pointer
	 */
	cJSON * ToJson() const;
};



#endif /* __HTTP_STATISTICS_H__ */
