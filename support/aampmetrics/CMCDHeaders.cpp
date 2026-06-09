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
#include <cstdio>
#include <cmath>
using namespace std;

/**
 * @brief Format a playback rate as a plain decimal string with no trailing zeros.
 *
 * Uses snprintf with the %g format specifier to strip unnecessary trailing zeros:
 * 2.0f -> "2", 1.5f -> "1.5", 0.5f -> "0.5". Avoids std::to_string which
 * produces "2.000000" for floating-point values.
 *
 * @param rate Playback rate value to format.
 * @return Formatted decimal string suitable for CMCD pr token emission.
 */
static std::string FormatPlaybackRate(float rate)
{
	char buf[32];
	std::snprintf(buf, sizeof(buf), "%g", static_cast<double>(rate));
	return std::string(buf);
}

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
 * Seeds the CMCD-Session group with a quoted sid entry via the shared
 * SerializeToCMCDMap serializer. Subclasses must call this base method first to
 * populate the Session group, then build their own Object/Request/Status entries
 * and pass them to SerializeToCMCDMap using the same map. Subclasses must not
 * clobber the "CMCD-Session:" entry written here.
 */
void CMCDHeaders::BuildCMCDCustomHeaders(std::unordered_map<std::string, std::vector<std::string>> &mCMCDCustomHeaders)
{
	mCMCDCustomHeaders.clear();
	// Emit all CMCD-Session entries via a single SerializeToCMCDMap call.
	// SerializeToCMCDMap alpha-sorts keys within the group: cid<pr<sf<sid<st<v.
	std::vector<CMCDEntry> entries;

	// sid — quoted-string token, always present
	entries.push_back(CMCDEntry{"sid", sessionId, CMCDGroup::Session, false, true, false});

	// v=1 — constant bare token, always present
	entries.push_back(CMCDEntry{"v", "1", CMCDGroup::Session, false, false, false});

	// sf — bare token; omit when streaming format is not yet determined
	if (!mStreamingFormat.empty())
	{
		entries.push_back(CMCDEntry{"sf", mStreamingFormat, CMCDGroup::Session, false, false, false});
	}

	// st — bare token; omit before first manifest parse (mStreamType empty)
	if (!mStreamType.empty())
	{
		entries.push_back(CMCDEntry{"st", mStreamType, CMCDGroup::Session, false, false, false});
	}

	// pr — bare decimal token. Per CTA-5004 pr is the actual playback rate (1 = real-time,
	// 2 = double speed, 0 = not playing) and SHOULD be sent whenever it is not 1. So 0 is a
	// valid value that MUST be emitted (it signals "not playing"); only 1x is omitted.
	// Use epsilon comparison rather than exact float equality: 1.0f has an exact
	// IEEE-754 representation and AAMP_NORMAL_PLAY_RATE is integer 1 (promoted to
	// 1.0f), so equality holds in practice — but a future double-to-float conversion
	// could produce a value indistinguishable from 1 that fails the strict check.
	static constexpr float kNormalPlayRate = 1.0f;
	static constexpr float kPlayRateEps    = 1e-4f;
	if (std::fabs(mPlaybackRate - kNormalPlayRate) > kPlayRateEps)
	{
		entries.push_back(CMCDEntry{"pr", FormatPlaybackRate(mPlaybackRate), CMCDGroup::Session, false, false, false});
	}

	// cid — quoted-string token; omit when no content id is available
	if (!mContentId.empty())
	{
		entries.push_back(CMCDEntry{"cid", mContentId, CMCDGroup::Session, false, true, false});
	}

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

/**
 * @brief   SetFragmentDuration
 * @param   durationMs object duration in milliseconds; 0 omits the d key
 */
void CMCDHeaders::SetFragmentDuration(const int &durationMs)
{
	mFragmentDuration = durationMs;
}

/**
 * @brief   SetMeasuredThroughput
 * @param   kbps measured network throughput in kbps; 0 omits the mtp key
 */
void CMCDHeaders::SetMeasuredThroughput(const int &kbps)
{
	mMeasuredThroughput = kbps;
}

/**
 * @brief   SetStartupUrgent
 * @param   startupUrgent true emits bare su token in CMCD-Request; false omits it
 */
void CMCDHeaders::SetStartupUrgent(const bool &startupUrgent)
{
	mStartupUrgent = startupUrgent;
}
