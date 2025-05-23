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
 * @file ManifestCMCDHeaders.cpp
 * @brief ManifestCMCDHeaders values formatting
 */

#include "ManifestCMCDHeaders.h"
using namespace std;


/**
 * @brief   BuildCMCDCustomHeaders
 * @param   map which collects formatted CMCD headers
 */
void ManifestCMCDHeaders::BuildCMCDCustomHeaders(std::unordered_map<std::string, std::vector<std::string>> &mCMCDCustomHeaders)
{
	//For manifest sessionid and object type is passed as a part of CMCD Headers
	CMCDHeaders::BuildCMCDCustomHeaders(mCMCDCustomHeaders);
	std::string headerName;
	std::vector<std::string> headerValue;
	std::string delimiter = ",";
	headerName="m";
	headerValue.clear();
	headerValue.push_back((CMCDObject+headerName));
	mCMCDCustomHeaders["CMCD-Object:"] = headerValue;
}
