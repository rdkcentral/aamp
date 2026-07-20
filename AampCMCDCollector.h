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
 * @file AampCMCDCollector.h
 * @brief Class to collect the CMCD Data
 */


#ifndef __AAMP_CMCD_COLLECTOR_H__
#define __AAMP_CMCD_COLLECTOR_H__

#include <map>
#include <mutex>
#include <string>
#include <vector>

#include "AampDefine.h"
#include "AampLogManager.h"
#include "AampMediaType.h"
#include "abr.h"

namespace AampCMCD
{
	struct Entry;
}

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
private:
	/**
	 * @enum StreamCategory
	 * @brief Family of CMCD keys emitted for a media type (mirrors the legacy per-type CMCDHeaders subclasses).
	 */
	enum class StreamCategory
	{
		eMANIFEST,  ///< Manifest/playlist requests: sid and ot=m only
		eVIDEO,     ///< Video segment requests: full object/request/status key set
		eAUDIO,     ///< Audio segment requests: full object/request/status key set
		eSUBTITLE   ///< Subtitle requests: sid and ot=s only
	};

	/**
	 * @struct CMCDState
	 * @brief Per-media-type CMCD reporting state.
	 */
	struct CMCDState
	{
		StreamCategory category{StreamCategory::eMANIFEST}; ///< Key family emitted for this media type
		std::string mediaTypeLabel{};  ///< Legacy media type name ("VIDEO", "INIT_AUDIO", "MUXED", ...); selects the ot value
		int firstByte{0};              ///< Time to first byte of the last download (ms)
		int lastByte{0};               ///< Time to last byte of the last download (ms)
		int dnsLookUpTime{0};          ///< DNS lookup time of the last download (ms)
		int bitrate{0};                ///< Encoded bitrate of the requested object (kbps)
		int topBitrate{0};             ///< Highest bitrate available for this track (kbps)
		int bufferLength{0};           ///< Buffered media ahead of the playhead (ms)
		bool bufferStarvation{false};  ///< True when the track buffer has run dry
		std::string nextUrl{};         ///< URL of the next expected object request (nor)
		std::string nextRange{};       ///< Byte range of the next request (nrr); SegmentList/SegmentBase MPDs
	};

	/**
	 * @brief Build the CMCD entries for one media type's current state.
	 *
	 * @param[in] state Per-media-type CMCD state.
	 * @return Entries in emission order, ready for serialization.
	 */
	std::vector<AampCMCD::Entry> BuildEntries(const CMCDState &state) const;

	bool bCMCDEnabled;			/**< CMCD enable/disable flag  */
	typedef std::map<int, CMCDState> StreamTypeCMCD;
	typedef StreamTypeCMCD::iterator StreamTypeCMCDIter;
	StreamTypeCMCD mCMCDStreamData;
	std::string mTraceId;
	std::mutex myMutex;
};




#endif
