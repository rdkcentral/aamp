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

#include "AampCMCDCollector.h"
#include "AampCMCDSerializer.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <uuid/uuid.h>

namespace
{
	// Comcast vendor-specific CMCD keys, carried alongside the standard keys in
	// reverse-DNS custom-key form. Existing collectors consume these; keep emitting them.
	const std::string kKeyComcastDns{"com.comcast-dns"};
	const std::string kKeyComcastFirstByte{"com.comcast-fb"};
	const std::string kKeyComcastLastByte{"com.comcast-lb"};

	/**
	 * @brief Map the session MediaFormat to the CMCD sf token.
	 * @return "d" (DASH), "h" (HLS), "s" (Smooth); empty string omits the key.
	 */
	std::string MediaFormatToSf(MediaFormat fmt)
	{
		switch (fmt)
		{
			case eMEDIAFORMAT_DASH:
				return "d";
			case eMEDIAFORMAT_HLS:
			case eMEDIAFORMAT_HLS_MP4:
				return "h";
			case eMEDIAFORMAT_SMOOTHSTREAMINGMEDIA:
				return "s";
			default:
				return "";
		}
	}

	/**
	 * @brief Format a playback rate as a CMCD decimal token ("2", "0.5", "-2").
	 */
	std::string FormatPlaybackRate(float rate)
	{
		char buf[32];
		std::snprintf(buf, sizeof(buf), "%g", static_cast<double>(rate));
		return std::string(buf);
	}
}

/**
 * @brief AampCMCDCollector - Constructor
 *
 */
AampCMCDCollector::AampCMCDCollector() : bCMCDEnabled(false),mTraceId(""),
							mCMCDStreamData(),myMutex()
{

}

/**
 * @brief AampCMCDCollector - Destructor
 *
 */
AampCMCDCollector::~AampCMCDCollector()
{

}

/**
 * @brief Initialize the CMCDCollector , create storage for metrics
 *
 * @return None
 */
void AampCMCDCollector::Initialize(bool enableDisable , std::string &traceId)
{
	std::lock_guard<std::mutex> lock (myMutex);
	bCMCDEnabled = enableDisable;
	if(enableDisable)
	{
		if(traceId == "unknown")
		{
			uuid_t uuid;
			uuid_generate(uuid);
			char sid[MAX_SESSION_ID_LENGTH];
			uuid_unparse_lower(uuid, sid);
			traceId = sid;
		}
		mTraceId = traceId;
		AAMPLOG_MIL("CMCD Enabled. TraceId:%s", mTraceId.c_str());
		// Reset per-media-type reporting state. Labels mirror the media type names
		// the legacy CMCDHeaders subclasses were configured with.
		mCMCDStreamData[eMEDIATYPE_MANIFEST] = CMCDState{StreamCategory::eMANIFEST, "MANIFEST"};
		mCMCDStreamData[eMEDIATYPE_VIDEO] = CMCDState{StreamCategory::eVIDEO, "VIDEO"};
		mCMCDStreamData[eMEDIATYPE_INIT_VIDEO] = CMCDState{StreamCategory::eVIDEO, "INIT_VIDEO"};
		mCMCDStreamData[eMEDIATYPE_IFRAME] = CMCDState{StreamCategory::eVIDEO, "VIDEO"};
		mCMCDStreamData[eMEDIATYPE_AUDIO] = CMCDState{StreamCategory::eAUDIO, "AUDIO"};
		mCMCDStreamData[eMEDIATYPE_INIT_AUDIO] = CMCDState{StreamCategory::eAUDIO, "INIT_AUDIO"};
		mCMCDStreamData[eMEDIATYPE_SUBTITLE] = CMCDState{StreamCategory::eSUBTITLE, "SUBTITLE"};
		mCMCDStreamData[eMEDIATYPE_INIT_SUBTITLE] = CMCDState{StreamCategory::eSUBTITLE, "SUBTITLE"};
		mCMCDStreamData[eMEDIATYPE_PLAYLIST_VIDEO] = CMCDState{StreamCategory::eMANIFEST, "PLAYLIST_VIDEO"};
		mCMCDStreamData[eMEDIATYPE_PLAYLIST_AUDIO] = CMCDState{StreamCategory::eMANIFEST, "PLAYLIST_AUDIO"};
		mCMCDStreamData[eMEDIATYPE_PLAYLIST_SUBTITLE] = CMCDState{StreamCategory::eMANIFEST, "PLAYLIST_SUBTITLE"};
	}
}



/**
 * @brief CMCDSetNextObjectRequest Store the next segment uri for stream type
 *
 * @return None
 */
void AampCMCDCollector::CMCDSetNextObjectRequest(std::string url,BitsPerSecond CMCDBandwidth,AampMediaType mediaT)
{
	std::lock_guard<std::mutex> lock (myMutex);
	if(bCMCDEnabled)
	{
		StreamTypeCMCDIter it=mCMCDStreamData.find(mediaT);
		if(it != mCMCDStreamData.end())
		{
			CMCDState &state = it->second;
			state.bitrate = (int)(CMCDBandwidth/1000);
			state.nextUrl = std::move(url);
		}
	}
}

/**
 * @brief Build the CMCD entries for one media type's current state.
 *
 * Emits the CTA-5004 v1 key set this collector can source: the Session group
 * (sid, v, sf, st, pr, cid) for every media type, the object type token, and
 * — for media segment types — the segment metrics (br/tb/d, bl/dl/mtp/su,
 * bs/rtp, nor/nrr plus the vendor keys). Unavailable keys are omitted per the
 * CTA-5004 optional-key rule. Consumes the bs latch.
 */
std::vector<AampCMCD::Entry> AampCMCDCollector::BuildEntries(CMCDState &state) const
{
	using AampCMCD::Entry;
	using AampCMCD::HeaderGroup;
	using AampCMCD::RoundToNearest100;
	using AampCMCD::ValueKind;

	std::vector<Entry> entries;

	// --- CMCD-Session group (all media types) ---

	// sid - String type, always present
	entries.push_back(Entry{"sid", mTraceId, HeaderGroup::eSESSION, ValueKind::eQUOTED});
	// v=1 - constant version token, always present
	entries.push_back(Entry{"v", "1", HeaderGroup::eSESSION, ValueKind::ePLAIN});
	// sf - omit until the streaming format is known
	if(!state.streamingFormat.empty())
	{
		entries.push_back(Entry{"sf", state.streamingFormat, HeaderGroup::eSESSION, ValueKind::ePLAIN});
	}
	// st - omit before the first manifest parse
	if(!state.streamType.empty())
	{
		entries.push_back(Entry{"st", state.streamType, HeaderGroup::eSESSION, ValueKind::ePLAIN});
	}
	// pr - CTA-5004: sent whenever the rate is not 1. 0 is valid ("not playing") and
	// must be emitted; only 1x is omitted. Epsilon compare rather than float equality.
	static constexpr float kNormalPlayRate = 1.0f;
	static constexpr float kPlayRateEps = 1e-4f;
	if(std::fabs(state.playbackRate - kNormalPlayRate) > kPlayRateEps)
	{
		entries.push_back(Entry{"pr", FormatPlaybackRate(state.playbackRate), HeaderGroup::eSESSION, ValueKind::ePLAIN});
	}
	// cid - String type; omit when no content id is available
	if(!state.contentId.empty())
	{
		entries.push_back(Entry{"cid", state.contentId, HeaderGroup::eSESSION, ValueKind::eQUOTED});
	}

	// --- CMCD-Object: ot token ---
	switch(state.category)
	{
		case StreamCategory::eMANIFEST:
			entries.push_back(Entry{"ot", "m", HeaderGroup::eOBJECT, ValueKind::ePLAIN});
			break;
		case StreamCategory::eSUBTITLE:
			// "c" = caption/subtitle per CTA-5004 (the legacy "s" is not a defined token)
			entries.push_back(Entry{"ot", "c", HeaderGroup::eOBJECT, ValueKind::ePLAIN});
			break;
		case StreamCategory::eVIDEO:
		case StreamCategory::eAUDIO:
		{
			std::string objectType;
			if(state.category == StreamCategory::eVIDEO)
			{
				if(state.mediaTypeLabel == "INIT_VIDEO")
				{
					objectType = "i";
				}
				else if(state.mediaTypeLabel == "MUXED")
				{
					objectType = "av";
				}
				else
				{
					objectType = "v";
				}
			}
			else
			{
				objectType = (state.mediaTypeLabel == "INIT_AUDIO") ? "i" : "a";
			}
			entries.push_back(Entry{"ot", objectType, HeaderGroup::eOBJECT, ValueKind::ePLAIN});

			// --- Segment metrics (media segment types only) ---

			// br and tb - plain integer kbps. CTA-5004 defines no rounding for these keys;
			// 0 means unknown and is omitted.
			if(state.bitrate > 0)
			{
				entries.push_back(Entry{"br", std::to_string(state.bitrate), HeaderGroup::eOBJECT, ValueKind::ePLAIN});
			}
			if(state.topBitrate > 0)
			{
				entries.push_back(Entry{"tb", std::to_string(state.topBitrate), HeaderGroup::eOBJECT, ValueKind::ePLAIN});
			}
			// d - object duration, plain integer ms (no rounding clause)
			if(state.fragmentDuration > 0)
			{
				entries.push_back(Entry{"d", std::to_string(state.fragmentDuration), HeaderGroup::eOBJECT, ValueKind::ePLAIN});
			}
			// bl - buffer length in ms, rounded to the nearest 100 ms; omit when unavailable
			const int blRounded = RoundToNearest100(state.bufferLength);
			if(blRounded > 0)
			{
				entries.push_back(Entry{"bl", std::to_string(blRounded), HeaderGroup::eREQUEST, ValueKind::ePLAIN});
			}
			// dl - deadline in ms = buffered duration / |playback rate|, rounded to 100 ms.
			// When not playing (pr=0) the buffer is not draining, so no deadline exists and
			// the key is omitted. Trick/slow rates scale the drain: 0.25x -> dl = 4*bl.
			static constexpr float kNotPlayingEps = 1e-4f;
			if(state.bufferLength > 0 && std::fabs(state.playbackRate) > kNotPlayingEps)
			{
				const int dlMs = static_cast<int>(static_cast<float>(state.bufferLength) / std::fabs(state.playbackRate));
				const int dlRounded = RoundToNearest100(dlMs);
				if(dlRounded > 0)
				{
					entries.push_back(Entry{"dl", std::to_string(dlRounded), HeaderGroup::eREQUEST, ValueKind::ePLAIN});
				}
			}
			// mtp - measured throughput in kbps, rounded to the nearest 100 kbps
			const int mtpRounded = RoundToNearest100(state.measuredThroughput);
			if(mtpRounded > 0)
			{
				entries.push_back(Entry{"mtp", std::to_string(mtpRounded), HeaderGroup::eREQUEST, ValueKind::ePLAIN});
			}
			// su - startup-urgent bare token
			if(state.startupUrgent)
			{
				entries.push_back(Entry{"su", "1", HeaderGroup::eREQUEST, ValueKind::eBOOLEAN});
			}
			// bs - reported once when a starvation has been latched since the last request,
			// then cleared (CTA-5004: bs marks starvation at some point since the prior request)
			if(state.bufferStarvation)
			{
				entries.push_back(Entry{"bs", "1", HeaderGroup::eSTATUS, ValueKind::eBOOLEAN});
				state.bufferStarvation = false;
			}
			// rtp - requested max throughput = 2 x encoded bitrate, rounded to 100 kbps.
			// Factor of 2 per the CTA-5004 client-discretion clause; matches the ExoPlayer
			// community default.
			if(state.bitrate > 0)
			{
				const int rtpRounded = RoundToNearest100(state.bitrate * 2);
				if(rtpRounded > 0)
				{
					entries.push_back(Entry{"rtp", std::to_string(rtpRounded), HeaderGroup::eSTATUS, ValueKind::ePLAIN});
				}
			}
			// nor / nrr - String type keys; AAMP segment paths are URL-safe ASCII so
			// quoting without percent-encoding is sufficient. The selection precedence
			// (dns path uses nor; otherwise nrr wins over nor) matches the legacy code;
			// an unknown next URL is omitted per the CTA-5004 optional-key rule.
			if(state.dnsLookUpTime > 0 || state.nextRange.empty())
			{
				if(!state.nextUrl.empty())
				{
					entries.push_back(Entry{"nor", state.nextUrl, HeaderGroup::eREQUEST, ValueKind::eQUOTED});
				}
			}
			else
			{
				entries.push_back(Entry{"nrr", state.nextRange, HeaderGroup::eREQUEST, ValueKind::eQUOTED});
			}
			// Vendor keys - presence rules are deliberately unchanged from the deployed
			// behaviour, because downstream consumers may assume they always exist:
			// fb/lb are emitted on every media segment request (even when 0), dns only
			// when a lookup time is available. Values stay plain unrounded integers -
			// they are outside the CTA-5004 kbps/ms rounding scope.
			if(state.dnsLookUpTime > 0)
			{
				entries.push_back(Entry{kKeyComcastDns, std::to_string(state.dnsLookUpTime), HeaderGroup::eREQUEST, ValueKind::ePLAIN});
			}
			entries.push_back(Entry{kKeyComcastFirstByte, std::to_string(state.firstByte), HeaderGroup::eREQUEST, ValueKind::ePLAIN});
			entries.push_back(Entry{kKeyComcastLastByte, std::to_string(state.lastByte), HeaderGroup::eREQUEST, ValueKind::ePLAIN});
			break;
		}
	}
	return entries;
}

/**
 * @brief CMCDGetHeaders Get the CMCD headers to add in download request
 *
 * @return None
 */
void AampCMCDCollector::CMCDGetHeaders(AampMediaType mediaType , std::vector<std::string> &customHeader)
{
	std::lock_guard<std::mutex> lock (myMutex);
	if(bCMCDEnabled)
	{
		StreamTypeCMCDIter it=mCMCDStreamData.find(mediaType);
		if(it == mCMCDStreamData.end())
		{
			AAMPLOG_INFO("[CMCD][%d]Couldn't find the filetype to Get metrics",mediaType);
			return;
		}
		for(const std::string &headerValue : AampCMCD::SerializeHeaders(BuildEntries(it->second)))
		{
			customHeader.push_back(headerValue);
			AAMPLOG_TRACE("[CMCD][%d]Header :%s",mediaType,headerValue.c_str());
		}
	}
}


/**
 * @brief CMCDSetNetworkMetrics Set Network Metrics for CMCD
 *
 * @return None
 */
void AampCMCDCollector::CMCDSetNetworkMetrics(AampMediaType mediaType,  int startTransferTime, int totalTime, int dnsLookUpTime)
{
	std::lock_guard<std::mutex> lock (myMutex);
	if(bCMCDEnabled)
	{
		StreamTypeCMCDIter it=mCMCDStreamData.find(mediaType);
		if(it != mCMCDStreamData.end())
		{
			CMCDState &state = it->second;
			state.firstByte = startTransferTime;
			state.lastByte = totalTime;
			state.dnsLookUpTime = dnsLookUpTime;
		}
		else
		{
			AAMPLOG_INFO("[CMCD][%d]couldn't find the filetype to store metrics",mediaType);
		}
	}
}

/**
 * @brief Collect and send all key-value pairs for CMCD headers.
 */
void AampCMCDCollector::SetBitrates(AampMediaType mediaType,const std::vector<BitsPerSecond> bitrateList)
{
	std::lock_guard<std::mutex> lock (myMutex);
	if(bCMCDEnabled && bitrateList.size())
	{
		StreamTypeCMCDIter it=mCMCDStreamData.find(mediaType);
		if(it != mCMCDStreamData.end())
		{
			BitsPerSecond maxBitrate = *max_element(bitrateList.begin(), bitrateList.end());
			AAMPLOG_INFO("[CMCD][%d]Top Bitrate %" BITSPERSECOND_FORMAT, mediaType,maxBitrate);
			if(mediaType == eMEDIATYPE_VIDEO || mediaType == eMEDIATYPE_AUDIO)
			{
				it->second.topBitrate = (int)(maxBitrate/1000);
			}
		}
		else
		{
			AAMPLOG_INFO("[CMCD][%d]couldn't find the filetype to store metrics",mediaType);
		}
	}
}



/**
 * @brief Collect and send all key-value pairs for CMCD headers.
 */
void AampCMCDCollector::SetTrackData(AampMediaType mediaType,bool bufferRedStatus,int bufferedDuration,int currentBitrate, bool IsMuxed)
{
	if(bCMCDEnabled)
	{
		// This is internal function called from GetHeaders. No Mutex lock needed here
		StreamTypeCMCDIter it=mCMCDStreamData.find(mediaType);
		if(it == mCMCDStreamData.end())
		{
			return;
		}
		CMCDState &state = it->second;
		if(mediaType == eMEDIATYPE_VIDEO || mediaType == eMEDIATYPE_INIT_VIDEO)
		{
			if(IsMuxed)
			{
				// One-way latch, matching legacy SetMediaType("MUXED") behaviour
				state.mediaTypeLabel = "MUXED";
			}
			// bs latch: a starvation is sticky until reported (BuildEntries clears it),
			// so one seen between two requests is still reported on the resumption request
			if(bufferRedStatus)
			{
				state.bufferStarvation = true;
			}
			state.bitrate = currentBitrate;
			state.bufferLength = bufferedDuration;
		}
		else if(mediaType == eMEDIATYPE_AUDIO || mediaType == eMEDIATYPE_INIT_AUDIO)
		{
			if(bufferRedStatus)
			{
				state.bufferStarvation = true;
			}
			state.bufferLength = bufferedDuration;
		}
	}
}

/**
 * @brief CMCD populate the nrr fields - next range request to fetch in case of Segment List MPD
 *
 * @return None
 */
void AampCMCDCollector::CMCDSetNextRangeRequest(std::string nextrange,BitsPerSecond bandwidth,AampMediaType mediaType)
{
	std::lock_guard<std::mutex> lock (myMutex);
	if(bCMCDEnabled && (!nextrange.empty()))
	{
		StreamTypeCMCDIter it=mCMCDStreamData.find(mediaType);
		if(it != mCMCDStreamData.end())
		{
			CMCDState &state = it->second;
			state.bitrate = (int)(bandwidth/1000);
			state.nextRange = std::move(nextrange);
		}
	}
}

/**
 * @brief CMCDSetSessionParams Push streaming format (sf) and content ID (cid) to all media types
 *
 * @return None
 */
void AampCMCDCollector::CMCDSetSessionParams(MediaFormat mediaFormat, const std::string& rawUrl)
{
	std::lock_guard<std::mutex> lock (myMutex);
	if(bCMCDEnabled)
	{
		// Strip query string and fragment from cid (auth-token leakage prevention):
		// take the substring up to the first '?' or '#', whichever comes first.
		const auto stripPos = std::min(rawUrl.find('?'), rawUrl.find('#'));
		const std::string contentId = (stripPos != std::string::npos) ? rawUrl.substr(0, stripPos) : rawUrl;
		const std::string sf = MediaFormatToSf(mediaFormat);
		for(auto &kv : mCMCDStreamData)
		{
			kv.second.streamingFormat = sf;
			kv.second.contentId = contentId;
		}
		AAMPLOG_INFO("[CMCD] CMCDSetSessionParams sf=%s cid=%s", sf.c_str(), contentId.c_str());
	}
}

/**
 * @brief CMCDSetLiveStatus Push live/VOD stream type (st) to all media types
 *
 * @return None
 */
void AampCMCDCollector::CMCDSetLiveStatus(bool isLive)
{
	std::lock_guard<std::mutex> lock (myMutex);
	if(bCMCDEnabled)
	{
		const std::string st = isLive ? "l" : "v";
		for(auto &kv : mCMCDStreamData)
		{
			kv.second.streamType = st;
		}
		AAMPLOG_INFO("[CMCD] CMCDSetLiveStatus st=%s", st.c_str());
	}
}

/**
 * @brief CMCDSetPlaybackRate Push the current playback rate (pr) to all media types
 *
 * @return None
 */
void AampCMCDCollector::CMCDSetPlaybackRate(float rate)
{
	std::lock_guard<std::mutex> lock (myMutex);
	if(bCMCDEnabled)
	{
		for(auto &kv : mCMCDStreamData)
		{
			kv.second.playbackRate = rate;
		}
		AAMPLOG_TRACE("[CMCD] CMCDSetPlaybackRate rate=%g", static_cast<double>(rate));
	}
}

/**
 * @brief CMCDSetFragmentDuration Set the object duration (d) in ms for one media type
 *
 * @return None
 */
void AampCMCDCollector::CMCDSetFragmentDuration(AampMediaType mediaType, int durationMs)
{
	std::lock_guard<std::mutex> lock (myMutex);
	if(bCMCDEnabled)
	{
		StreamTypeCMCDIter it=mCMCDStreamData.find(mediaType);
		if(it != mCMCDStreamData.end())
		{
			it->second.fragmentDuration = durationMs;
		}
	}
}

/**
 * @brief CMCDSetMeasuredThroughput Set the measured throughput (mtp) in kbps for one media type
 *
 * @return None
 */
void AampCMCDCollector::CMCDSetMeasuredThroughput(AampMediaType mediaType, int kbps)
{
	std::lock_guard<std::mutex> lock (myMutex);
	if(bCMCDEnabled)
	{
		StreamTypeCMCDIter it=mCMCDStreamData.find(mediaType);
		if(it != mCMCDStreamData.end())
		{
			it->second.measuredThroughput = kbps;
		}
	}
}

/**
 * @brief CMCDSetStartupUrgent Set the startup-urgent flag (su) for one media type
 *
 * @return None
 */
void AampCMCDCollector::CMCDSetStartupUrgent(AampMediaType mediaType, bool startupUrgent)
{
	std::lock_guard<std::mutex> lock (myMutex);
	if(bCMCDEnabled)
	{
		StreamTypeCMCDIter it=mCMCDStreamData.find(mediaType);
		if(it != mCMCDStreamData.end())
		{
			it->second.startupUrgent = startupUrgent;
		}
	}
}
