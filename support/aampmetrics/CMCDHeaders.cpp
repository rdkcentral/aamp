/*
 * If not stated otherwise in this file or this component's license file the
 * following copyright and licenses apply:
 *
 *   Copyright 2022 RDK Management
 *
 *   Licensed under the Apache License, Version 2.0 (the "License");
 *   you may not use this file except in compliance with the License.
 *   You may obtain a copy of the License at
 *
 *       http://www.apache.org/licenses/LICENSE-2.0
 *
 *   Unless required by applicable law or agreed to in writing, software
 *   distributed under the License is distributed on an "AS IS" BASIS,
 *   WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *   See the License for the specific language governing permissions and
 *   limitations under the License.
 */

/**
 * @file CMCDHeaders.cpp
 * @brief CMCDHeaders values
 */
#include "CMCDHeaders.h"
#include "CMCDSerializer.h"
using namespace std;

/**
 * @brief   SetNetworkMetrics
 * @param   startTransferTime time to first byte
 * @param   totalTime time to last byte
 * @param   dnsLookUpTime dns look up time
 */
void CMCDHeaders::SetNetworkMetrics(const int &startTransferTime,const int &totalTime,const int &dnsLookUpTime)
{
	firstByte = startTransferTime;
	lastByte = totalTime;
	dnsLookUptime = dnsLookUpTime;

}

/**
 * @brief   GetNetworkMetrics
 * @param   startTransferTime time to first byte
 * @param   totalTime time to last byte
 * @param   dnsLookUpTime dns look up time
 */
void CMCDHeaders::GetNetworkMetrics(int &startTransferTime, int &totalTime, int &dnsLookUpTime)
{
	startTransferTime = firstByte;
	totalTime = lastByte;
	dnsLookUpTime = dnsLookUptime;
}

/**
 * @brief   SetSessionId
 * @param   sid session id to be set
 */
void  CMCDHeaders::SetSessionId(const std::string &sid)
{
	sessionId = sid;
}

/**
 * @brief   GetSessionId
 * @param   sid session id
 */
std::string  CMCDHeaders::GetSessionId()
{
	return sessionId;
}

/**
 * @brief   SetMediaType
 * @param   mediaTypeName type of media
 */
void  CMCDHeaders::SetMediaType(const std::string &mediaTypeName )
{
	mediaType = mediaTypeName;
}

/**
 * @brief   SetNextUrl
 * @param   url
 */
void  CMCDHeaders::SetNextUrl(const std::string &url)
{
	nextUrl = url;
}

/**
 * @brief   SetBitrate
 * @param   Bandwidth
 */
void  CMCDHeaders::SetBitrate(const int &Bandwidth)
{
	bitrate = Bandwidth;
}

/**
 * @brief   SetTopBitrate
 * @param   Bandwidth
 */
void  CMCDHeaders::SetTopBitrate(const int &Bandwidth)
{
	topBitrate = Bandwidth;
}

/**
 * @brief   SetBufferLength
 * @param   bufferlength
 */
void  CMCDHeaders::SetBufferLength(const int &bufferlength)
{
	bufferLength = bufferlength;
}

/**
 * @brief   SetBufferStarvation
 * @param   bufferStarvation
 */
void  CMCDHeaders::SetBufferStarvation(const bool &bufferStarvation)
{
	this->bufferStarvation = bufferStarvation;
}

/**
 * @brief   GetMediaType
 */
std::string  CMCDHeaders::GetMediaType()
{
	return mediaType;
}


/**
 * @brief   BuildCMCDCustomHeaders
 * @param   map which collects formatted CMCD headers
 *
 * Seeds the CMCD-Session group with a quoted sid entry (SER-03) via the shared
 * SerializeToCMCDMap serializer. Subclasses must call this base method first to
 * populate the Session group, then build their own Object/Request/Status entries
 * and pass them to SerializeToCMCDMap using the same map. Subclasses must not
 * clobber the "CMCD-Session:" entry written here.
 */
void CMCDHeaders::BuildCMCDCustomHeaders(std::unordered_map<std::string, std::vector<std::string>> &mCMCDCustomHeaders)
{
	mCMCDCustomHeaders.clear();
	// Emit sid as a quoted-string token per CTA-5004 §3 (SER-03).
	std::vector<CMCDEntry> entries;
	entries.push_back(CMCDEntry{"sid", sessionId, CMCDGroup::Session, false, true, false});
	SerializeToCMCDMap(entries, mCMCDCustomHeaders);
}

/**
 * @brief   SetNextRange -> SegmentBase MPD
 * @param
 */
void  CMCDHeaders::SetNextRange(const std::string &nextrange)
{
	mNextRange = nextrange;
}

/**
 * @brief   SetStreamingFormat
 * @param   sf CMCD streaming format token: "d" (DASH), "h" (HLS/HLS-MP4), "s" (Smooth)
 */
void CMCDHeaders::SetStreamingFormat(const std::string &sf)
{
	mStreamingFormat = sf;
}

/**
 * @brief   SetStreamType
 * @param   st CMCD stream type token: "v" (VOD) or "l" (live)
 */
void CMCDHeaders::SetStreamType(const std::string &st)
{
	mStreamType = st;
}

/**
 * @brief   SetContentId
 * @param   cid CMCD content identifier; empty string causes the cid key to be omitted
 */
void CMCDHeaders::SetContentId(const std::string &cid)
{
	mContentId = cid;
}

/**
 * @brief   SetPlaybackRate
 * @param   rate playback rate; 1.0f is normal play (pr key omitted at 1.0f)
 */
void CMCDHeaders::SetPlaybackRate(const float &rate)
{
	mPlaybackRate = rate;
}
