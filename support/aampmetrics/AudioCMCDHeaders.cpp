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
 * @file AudioCMCDHeaders.cpp
 * @brief AudioCMCDHeaders values formatting
 */

#include "AudioCMCDHeaders.h"
#include "CMCDSerializer.h"
using namespace std;

/**
 * @brief   BuildCMCDCustomHeaders
 * @param   map which collects formatted CMCD headers
 */
void AudioCMCDHeaders::BuildCMCDCustomHeaders(std::unordered_map<std::string, std::vector<std::string>> &mCMCDCustomHeaders)
{
	// Step 1: seed the CMCD-Session group with a quoted sid entry via the base class.
	CMCDHeaders::BuildCMCDCustomHeaders(mCMCDCustomHeaders);

	// Step 2: build structured entries for Object/Request/Status groups.
	std::vector<CMCDEntry> entries;

	// ot token: bare identifier (i for init, a otherwise).
	std::string otToken;
	if (mediaType == "INIT_AUDIO")
	{
		otToken = "i";
	}
	else
	{
		otToken = "a";
	}
	entries.push_back(CMCDEntry{"ot", otToken, CMCDGroup::Object});

	// br and tb: kbps integers — serializer applies RoundToNearest100 and omits zeros.
	entries.push_back(CMCDEntry{"br", std::to_string(bitrate), CMCDGroup::Object, true});
	entries.push_back(CMCDEntry{"tb", std::to_string(topBitrate), CMCDGroup::Object, true});

	// bl: buffer length in ms — serializer rounds and omits zero.
	entries.push_back(CMCDEntry{"bl", std::to_string(bufferLength), CMCDGroup::Request, true});

	// bs: boolean bare token — emit only when bufferStarvation is true (SER-06).
	if (bufferStarvation)
	{
		entries.push_back(CMCDEntry{"bs", "1", CMCDGroup::Status, false, false, true});
	}

	// nor / nrr / Comcast custom keys: preserve existing branch logic.
	// Note: nor/nrr are wrapped in double-quotes (isQuotedString=true).
	// AAMP segment paths are already URL-safe ASCII, so quoting without
	// percent-encoding is sufficient (RESEARCH Pitfall 4 / Assumption A1).
	// Comcast keys are bare integers (NOT isInteger) to preserve raw unrounded
	// values — they are outside the SER-01/SER-02 kbps/ms rounding scope.
	if (dnsLookUptime > 0)
	{
		entries.push_back(CMCDEntry{"nor", nextUrl, CMCDGroup::Request, false, true});
		entries.push_back(CMCDEntry{"com.comcast-dns", std::to_string(dnsLookUptime), CMCDGroup::Request});
		entries.push_back(CMCDEntry{"com.comcast-fb", std::to_string(firstByte), CMCDGroup::Request});
		entries.push_back(CMCDEntry{"com.comcast-lb", std::to_string(lastByte), CMCDGroup::Request});
	}
	else if (!mNextRange.empty())
	{
		entries.push_back(CMCDEntry{"nrr", mNextRange, CMCDGroup::Request, false, true});
		entries.push_back(CMCDEntry{"com.comcast-fb", std::to_string(firstByte), CMCDGroup::Request});
		entries.push_back(CMCDEntry{"com.comcast-lb", std::to_string(lastByte), CMCDGroup::Request});
	}
	else
	{
		entries.push_back(CMCDEntry{"nor", nextUrl, CMCDGroup::Request, false, true});
		entries.push_back(CMCDEntry{"com.comcast-fb", std::to_string(firstByte), CMCDGroup::Request});
		entries.push_back(CMCDEntry{"com.comcast-lb", std::to_string(lastByte), CMCDGroup::Request});
	}

	// Step 3: serialize Object/Request/Status entries into the map.
	// SerializeToCMCDMap merges into the existing map without clearing, so the
	// CMCD-Session: entry written by the base class is preserved.
	SerializeToCMCDMap(entries, mCMCDCustomHeaders);
}
