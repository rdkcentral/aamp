/*
 * If not stated otherwise in this file or this component's license file the
 * following copyright and licenses apply:
 *
 * Copyright 2025 RDK Management
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
 * @file AampFragmentDescriptor.hpp
 * @brief FragmentDescriptor class definition.
 */

#ifndef AAMP_FRAGMENTDESCRIPTOR_HPP
#define AAMP_FRAGMENTDESCRIPTOR_HPP

#include <string>
#include <map>
#include <vector>
#include "libdash/IBaseUrl.h"
#include "libdash/IMPD.h"

using namespace dash;
using namespace dash::mpd;

/**
 * @class FragmentDescriptor
 * @brief Stores information of dash fragment
 */
class FragmentDescriptor
{
private:
	std::string matchingBaseURL;

public:
	std::string manifestUrl;
	uint32_t Bandwidth;
	std::string RepresentationID;
	uint64_t Number;
	double Time; // In units of timescale
	bool bUseMatchingBaseUrl;
	int64_t nextfragmentNum;
	double nextfragmentTime;
	uint32_t TimeScale;
	FragmentDescriptor();
	FragmentDescriptor(const FragmentDescriptor &p);
	FragmentDescriptor &operator=(const FragmentDescriptor &p);
	std::string GetMatchingBaseUrl() const;
	void ClearMatchingBaseUrl();
	void AppendMatchingBaseUrl(const std::vector<IBaseUrl *> *baseUrls);
	void AppendMatchingBaseUrl(const std::vector<std::string> &baseUrls);
};

#endif // AAMP_FRAGMENTDESCRIPTOR_HPP
