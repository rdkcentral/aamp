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
#include "DrmMediaFormat.h"
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

	/**
	 * @brief CMCDSetSessionParams Push streaming format (sf) and content ID (cid) to all media types.
	 *        Strips the query string and fragment from rawUrl before using it as cid, so auth
	 *        tokens carried in the manifest URL are not leaked. Called once after Initialize().
	 *
	 * @param[in] mediaFormat - session media format, mapped to the CMCD sf token
	 * @param[in] rawUrl - manifest URL used as the content id
	 * @return None
	 */
	void CMCDSetSessionParams(MediaFormat mediaFormat, const std::string& rawUrl);

	/**
	 * @brief CMCDSetLiveStatus Push live/VOD stream type (st) to all media types.
	 *        Called from the fragment collectors after manifest parse.
	 *
	 * @param[in] isLive - true for live streams ("l"), false for VOD ("v")
	 * @return None
	 */
	void CMCDSetLiveStatus(bool isLive);

	/**
	 * @brief CMCDSetPlaybackRate Push the current playback rate (pr) to all media types.
	 *        pr is emitted only when the rate is not 1 (normal play); 0 means "not playing".
	 *
	 * @param[in] rate - current playback rate
	 * @return None
	 */
	void CMCDSetPlaybackRate(float rate);

	/**
	 * @brief CMCDSetFragmentDuration Set the object duration (d) in ms for one media type.
	 *
	 * @param[in] mediaType - media type of the request
	 * @param[in] durationMs - object duration in milliseconds; 0 omits the key
	 * @return None
	 */
	void CMCDSetFragmentDuration(AampMediaType mediaType, int durationMs);

	/**
	 * @brief CMCDSetMeasuredThroughput Set the measured throughput (mtp) in kbps for one media type.
	 *
	 * @param[in] mediaType - media type of the request
	 * @param[in] kbps - measured throughput in kbps; 0 omits the key
	 * @return None
	 */
	void CMCDSetMeasuredThroughput(AampMediaType mediaType, int kbps);

	/**
	 * @brief CMCDSetStartupUrgent Set the startup-urgent flag (su) for one media type.
	 *        Level-triggered: recomputed by the engine on every request.
	 *
	 * @param[in] mediaType - media type of the request
	 * @param[in] startupUrgent - true during tune, seek or rebuffer recovery
	 * @return None
	 */
	void CMCDSetStartupUrgent(AampMediaType mediaType, bool startupUrgent);
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
		int bitrate{0};                ///< Encoded bitrate of the requested object (kbps); 0 = unknown, br/rtp omitted
		int topBitrate{0};             ///< Highest bitrate available for this track (kbps); 0 = unknown, tb omitted
		int bufferLength{0};           ///< Buffered media ahead of the playhead (ms)
		bool bufferStarvation{false};  ///< CMCD bs: latched on starvation, sticky until reported once, then cleared
		std::string nextUrl{};         ///< URL of the next expected object request (nor); empty = omit
		std::string nextRange{};       ///< Byte range of the next request (nrr); SegmentList/SegmentBase MPDs
		std::string streamingFormat{}; ///< CMCD sf token: "d" (DASH), "h" (HLS), "s" (Smooth); empty = omit
		std::string streamType{};      ///< CMCD st token: "v" (VOD) or "l" (live); empty = omit until known
		std::string contentId{};       ///< CMCD cid value (String type); empty = omit
		float playbackRate{1.0f};      ///< CMCD pr value; 1.0f = normal play (pr omitted), 0 = not playing
		int fragmentDuration{0};       ///< CMCD d value: object duration in ms; 0 = omit
		int measuredThroughput{0};     ///< CMCD mtp value: measured throughput in kbps; 0 = omit
		bool startupUrgent{false};     ///< CMCD su flag: true when the request is startup/seek/rebuffer urgent
	};

	/**
	 * @brief Build the CMCD entries for one media type's current state.
	 *        Consumes the bs latch: a latched starvation is reported once, then cleared.
	 *
	 * @param[in,out] state Per-media-type CMCD state.
	 * @return Entries ready for serialization (the serializer sorts them).
	 */
	std::vector<AampCMCD::Entry> BuildEntries(CMCDState &state) const;

	bool bCMCDEnabled;			/**< CMCD enable/disable flag  */
	typedef std::map<int, CMCDState> StreamTypeCMCD;
	typedef StreamTypeCMCD::iterator StreamTypeCMCDIter;
	StreamTypeCMCD mCMCDStreamData;
	std::string mTraceId;
	std::mutex myMutex;
};




#endif
