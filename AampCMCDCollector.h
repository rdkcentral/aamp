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
#include "middleware/drm/DrmMediaFormat.h"
#include <algorithm>
#include "abr.h"

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
	void CMCDSetNextObjectRequest(std::string url,BitsPerSecond CMCDBandwidth,AampMediaType mediaT=eMEDIATYPE_VIDEO);
    
    	/**
	* @brief CMCDSetNextRangeRequest Store the next range relative to the current url
	*
	* @param[in] nextrange - the next byte range to be requested
	* @param[in] CMCDBandwidth - Bandwidth of current segment
	* @param[in] mediaT - media type
	* @return None
	*/
	void CMCDSetNextRangeRequest(std::string nextrange,BitsPerSecond bandwidth,AampMediaType mediaType);

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

	/**
	 * @brief CMCDSetSessionParams Push streaming format and content ID to all CMCDHeaders instances.
	 *        Called once after Initialize(), propagating sf and cid session keys.
	 *
	 * @param[in] mediaFormat - streaming format enum (DASH/HLS/HLS_MP4/Smooth/etc.)
	 * @param[in] contentId   - content identifier (manifest URL with query+fragment stripped)
	 * @return None
	 */
	void CMCDSetSessionParams(MediaFormat mediaFormat, std::string contentId);

	/**
	 * @brief CMCDSetLiveStatus Push live/VOD stream type to all CMCDHeaders instances.
	 *        Called from fragment collectors after manifest parse (st session key).
	 *
	 * @param[in] isLive - true for live stream, false for VOD
	 * @return None
	 */
	void CMCDSetLiveStatus(bool isLive);

	/**
	 * @brief CMCDSetPlaybackRate Push current playback rate to all CMCDHeaders instances.
	 *        Called from NotifySpeedChanged on every rate change (pr session key).
	 *
	 * @param[in] rate - current playback rate (pr omitted when rate == 1.0f)
	 * @return None
	 */
	void CMCDSetPlaybackRate(float rate);

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
