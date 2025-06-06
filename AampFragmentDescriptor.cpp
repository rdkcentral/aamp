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
 * @file AampFragmentDescriptor.cpp
 * @brief FragmentDescriptor implementation
 */


#include "AampUtils.h"
#include "AampFragmentDescriptor.hpp"

FragmentDescriptor::FragmentDescriptor() : manifestUrl(""), Bandwidth(0), Number(0), Time(0), RepresentationID(""), matchingBaseURL(""), bUseMatchingBaseUrl(false), nextfragmentNum(-1), nextfragmentTime(0), TimeScale(1)
{
}

FragmentDescriptor::FragmentDescriptor(const FragmentDescriptor &p) : manifestUrl(p.manifestUrl), Bandwidth(p.Bandwidth), RepresentationID(p.RepresentationID), Number(p.Number), Time(p.Time), matchingBaseURL(p.matchingBaseURL), bUseMatchingBaseUrl(p.bUseMatchingBaseUrl), nextfragmentNum(p.nextfragmentNum), nextfragmentTime(p.nextfragmentTime), TimeScale(p.TimeScale)
{
}

FragmentDescriptor &FragmentDescriptor::operator=(const FragmentDescriptor &p)
{
	manifestUrl = p.manifestUrl;
	RepresentationID.assign(p.RepresentationID);
	Bandwidth = p.Bandwidth;
	Number = p.Number;
	Time = p.Time;
	matchingBaseURL = p.matchingBaseURL;
	nextfragmentNum = p.nextfragmentNum;
	nextfragmentTime = p.nextfragmentTime;
	TimeScale = p.TimeScale;
	return *this;
}

std::string FragmentDescriptor::GetMatchingBaseUrl() const
{
	return matchingBaseURL;
}

void FragmentDescriptor::ClearMatchingBaseUrl()
{
	matchingBaseURL.clear();
}

void FragmentDescriptor::AppendMatchingBaseUrl(const std::vector<IBaseUrl *> *baseUrls)
{
	if (baseUrls && baseUrls->size() > 0)
	{
		const std::string &url = baseUrls->at(0)->GetUrl();
		if (url.empty())
		{
		}
		else if (aamp_IsAbsoluteURL(url))
		{
			if (bUseMatchingBaseUrl)
			{
				std::string prefHost = aamp_getHostFromURL(manifestUrl);
				for (auto &item : *baseUrls)
				{
					const std::string itemUrl = item->GetUrl();
					std::string host = aamp_getHostFromURL(itemUrl);
					if (0 == prefHost.compare(host))
					{
						matchingBaseURL = item->GetUrl();
						return;
					}
				}
			}
			matchingBaseURL = url;
		}
		else if (url.rfind("/", 0) == 0)
		{
			matchingBaseURL = aamp_getHostFromURL(matchingBaseURL);
			matchingBaseURL += url;
			AAMPLOG_WARN("baseURL with leading /");
		}
		else
		{
			if (!matchingBaseURL.empty() && matchingBaseURL.back() != '/')
			{ // add '/' delimiter only if parent baseUrl doesn't already end with one
				matchingBaseURL += "/";
			}
			matchingBaseURL += url;
		}
	}
}

void FragmentDescriptor::AppendMatchingBaseUrl(const std::vector<std::string> &baseUrls)
{
	if (!baseUrls.empty())
	{
		const std::string &url = baseUrls.at(0);
		if (url.empty())
		{
			// Do nothing if the URL is empty
		}
		else if (aamp_IsAbsoluteURL(url))
		{
			if (bUseMatchingBaseUrl)
			{
				std::string prefHost = aamp_getHostFromURL(manifestUrl);
				for (const auto &itemUrl : baseUrls)
				{
					std::string host = aamp_getHostFromURL(itemUrl);
					if (0 == prefHost.compare(host))
					{
						matchingBaseURL = itemUrl;
						return;
					}
				}
			}
			matchingBaseURL = url;
		}
		else if (url.rfind("/", 0) == 0)
		{
			matchingBaseURL = aamp_getHostFromURL(matchingBaseURL);
			matchingBaseURL += url;
			AAMPLOG_WARN("baseURL with leading /");
		}
		else
		{
			if (!matchingBaseURL.empty() && matchingBaseURL.back() != '/')
			{
				// Add '/' delimiter only if parent baseUrl doesn't already end with one
				matchingBaseURL += "/";
			}
			matchingBaseURL += url;
		}
	}
}
