/*
 * If not stated otherwise in this file or this component's license file the
 * following copyright and licenses apply:
 *
 * Copyright 2023 RDK Management
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
 * @file AampCMCDCollector.cpp
 * @brief Class to collect the CMCD Data
 */


#ifndef __AAMP_CMCD_COLLECTOR_H__
#define __AAMP_CMCD_COLLECTOR_H__

#include <iostream>
#include <memory>
#include <map>
#include <exception>

#include <CMCDHeaders.h>
#include <AudioCMCDHeaders.h>
#include <VideoCMCDHeaders.h>
#include <ManifestCMCDHeaders.h>
#include <SubtitleCMCDHeaders.h>
#include <uuid/uuid.h>
#include "AampDefine.h"
#include "AampLogManager.h"
#include <algorithm>
#include "ABRManager.h"

/**
 * @class AampCMCDCollector
 * @brief AAMP CMCD Data Collector
 */
class AampCMCDCollector
{
public:
	/**
	 * @fn AampCMCDCollector constructor
	 *
	 * @return None
	 */
	AampCMCDCollector();
	/**
	 * @brief AampCMCDCollector Destructor function
	 *
	 * @return None
	 */
	~AampCMCDCollector();
	/**
	 * @brief Copy constructor disabled
	 *
	 */
	AampCMCDCollector(const AampCMCDCollector&) = delete;
	/**
	 * @brief assignment operator disabled
	 *
	 */
	AampCMCDCollector& operator=(const AampCMCDCollector&) = delete;
	/**
	 * @brief CMCDSetNextObjectRequest Store the next segment uri for stream type
	 *
	 * @param[in] url - current segment url
	 * @param[in] CMCDBandwidth - Bandwidth of current segment
	 * @param[in] mediaT - media type
	 * @return None
	 */
	void CMCDSetNextObjectRequest(std::string url,long CMCDBandwidth,AampMediaType mediaT=eMEDIATYPE_VIDEO);
    
    	/**
	* @brief CMCDSetNextRangeRequest Store the next range relative to the current url
	*
	* @param[in] nextrange - the next byte range to be requested
	* @param[in] CMCDBandwidth - Bandwidth of current segment
	* @param[in] mediaT - media type
	* @return None
	*/
	void CMCDSetNextRangeRequest(std::string nextrange,long bandwidth,AampMediaType mediaType);

	/**
	 * @brief Initialize AampCMCD Collector instance
	 *
	 * @param[in] enableDisable - Enable CMCD functionality
	 * @param[in] traceId - TraceId for the CMCD
	 * @return None
	 */
	void Initialize(bool enableDisable , std::string &traceId);
	/**
	 * @brief CMCDSetNetworkMetrics Store Network Metrics for the mediaType
	 *
	 * @param[in] mediaType - File Type for storing the data
	 * @param[in] NetworkMetrics - Network Metrics to store 
	 * @return None
	 */
	void CMCDSetNetworkMetrics(AampMediaType mediaType, int startTransferTime, int totalTime, int dnsLookUpTime);
	/**
	 * @brief CMCDGetHeaders Get the CMCD headers to add in download request
	 *
	 * @return None
	 */
	void CMCDGetHeaders(AampMediaType mediaType ,  std::vector<std::string> &customHeader);
	void SetBitrates(AampMediaType mediaType,const std::vector<BitsPerSecond> bitrates);
	void SetTrackData(AampMediaType mediaType,bool bufferRedStatus,int bufferedDuration,int currentBitrate, bool IsMuxed=false);
private:
	bool bCMCDEnabled;			/**< CMCD enable/disable flag  */
	typedef std::map<int, CMCDHeaders *> StreamTypeCMCD;
	typedef std::map<int, CMCDHeaders *>::iterator StreamTypeCMCDIter;
	StreamTypeCMCD mCMCDStreamData;
	std::string mTraceId;
	std::mutex myMutex;
	/**
	 * @brief convertHexa Convert decimal to hexadecimal
	 *
	 * @param[in] number - decimal number
	 * @return hexadecimal number
	 */
	std::string convertHexa(long long number);
};




#endif
