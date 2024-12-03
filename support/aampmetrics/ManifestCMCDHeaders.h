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
 * @file ManifestCMCDHeaders.h
 * @brief ManifestCMCDHeaders Values
 */

#ifndef ManifestCMCDHeaders_h
#define ManifestCMCDHeaders_h

#include "CMCDHeaders.h"

/**
 * @class   ManifestCMCDHeaders
 * @brief   ManifestCMCDHeaders Context
 */
class ManifestCMCDHeaders: public CMCDHeaders
{

public:
	ManifestCMCDHeaders() : CMCDHeaders() {}
	void BuildCMCDCustomHeaders(std::unordered_map<std::string, std::vector<std::string>> &mCMCDCustomHeaders);
	~ManifestCMCDHeaders() {}
};

#endif
