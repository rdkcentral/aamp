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
#include <uuid/uuid.h>

namespace
{
	// Comcast vendor-specific CMCD keys, carried alongside the standard keys in
	// reverse-DNS custom-key form. Existing collectors consume these; keep emitting them.
	const std::string kKeyComcastDns{"com.comcast-dns"};
	const std::string kKeyComcastFirstByte{"com.comcast-fb"};
	const std::string kKeyComcastLastByte{"com.comcast-lb"};
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
 * Entry order matches the byte order the legacy CMCDHeaders subclasses
 * produced, so serialized header values are unchanged by the refactor.
 */
std::vector<AampCMCD::Entry> AampCMCDCollector::BuildEntries(const CMCDState &state) const
{
	using AampCMCD::Entry;
	using AampCMCD::HeaderGroup;
	using AampCMCD::ValueKind;

	std::vector<Entry> entries;
	// Every media type reports the session id; legacy output leaves it unquoted.
	entries.push_back(Entry{"sid", mTraceId, HeaderGroup::eSESSION, ValueKind::ePLAIN});
	switch(state.category)
	{
		case StreamCategory::eMANIFEST:
			entries.push_back(Entry{"ot", "m", HeaderGroup::eOBJECT, ValueKind::ePLAIN});
			break;
		case StreamCategory::eSUBTITLE:
			entries.push_back(Entry{"ot", "s", HeaderGroup::eOBJECT, ValueKind::ePLAIN});
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
			entries.push_back(Entry{"br", std::to_string(state.bitrate), HeaderGroup::eOBJECT, ValueKind::ePLAIN});
			entries.push_back(Entry{"ot", objectType, HeaderGroup::eOBJECT, ValueKind::ePLAIN});
			entries.push_back(Entry{"tb", std::to_string(state.topBitrate), HeaderGroup::eOBJECT, ValueKind::ePLAIN});
			entries.push_back(Entry{"bl", std::to_string(state.bufferLength), HeaderGroup::eREQUEST, ValueKind::ePLAIN});
			if(state.dnsLookUpTime > 0)
			{
				entries.push_back(Entry{"nor", state.nextUrl, HeaderGroup::eREQUEST, ValueKind::ePLAIN});
				entries.push_back(Entry{kKeyComcastDns, std::to_string(state.dnsLookUpTime), HeaderGroup::eREQUEST, ValueKind::ePLAIN});
			}
			else if(!state.nextRange.empty())
			{
				entries.push_back(Entry{"nrr", state.nextRange, HeaderGroup::eREQUEST, ValueKind::ePLAIN});
			}
			else
			{
				entries.push_back(Entry{"nor", state.nextUrl, HeaderGroup::eREQUEST, ValueKind::ePLAIN});
			}
			entries.push_back(Entry{kKeyComcastFirstByte, std::to_string(state.firstByte), HeaderGroup::eREQUEST, ValueKind::ePLAIN});
			entries.push_back(Entry{kKeyComcastLastByte, std::to_string(state.lastByte), HeaderGroup::eREQUEST, ValueKind::ePLAIN});
			entries.push_back(Entry{"bs", state.bufferStarvation ? "1" : "0", HeaderGroup::eSTATUS, ValueKind::eBOOLEAN});
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
			state.bufferStarvation = bufferRedStatus;
			state.bitrate = currentBitrate;
			state.bufferLength = bufferedDuration;
		}
		else if(mediaType == eMEDIATYPE_AUDIO || mediaType == eMEDIATYPE_INIT_AUDIO)
		{
			state.bufferStarvation = bufferRedStatus;
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
