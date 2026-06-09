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
 * @file SubtitleCMCDHeaders.cpp
 * @brief SubtitleCMCDHeaders values formatting
 */

#include "SubtitleCMCDHeaders.h"
#include "CMCDSerializer.h"
using namespace std;

/**
 * @brief   BuildCMCDCustomHeaders
 * @param   map which collects formatted CMCD headers
 */
void SubtitleCMCDHeaders::BuildCMCDCustomHeaders(std::unordered_map<std::string, std::vector<std::string>> &mCMCDCustomHeaders)
{
	// Seed the CMCD-Session group with a quoted sid entry via the base class.
	CMCDHeaders::BuildCMCDCustomHeaders(mCMCDCustomHeaders);
	// Emit ot=s as a bare token in CMCD-Object (subtitle type).
	// SerializeToCMCDMap merges into the existing map without clearing, so the
	// CMCD-Session: entry written by the base class is preserved.
	std::vector<CMCDEntry> entries;
	entries.push_back(CMCDEntry{"ot", "s", CMCDGroup::Object});
	SerializeToCMCDMap(entries, mCMCDCustomHeaders);
}
