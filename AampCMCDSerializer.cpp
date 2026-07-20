/*
 * If not stated otherwise in this file or this component's license file the
 * following copyright and licenses apply:
 *
 * Copyright 2026 RDK Management
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
 * @file AampCMCDSerializer.cpp
 * @brief Serialization of CMCD (CTA-5004) key-value entries into HTTP request header lines.
 */

#include "AampCMCDSerializer.h"

#include <algorithm>

namespace AampCMCD
{

std::string HeaderName(HeaderGroup group)
{
	switch (group)
	{
		case HeaderGroup::eOBJECT:
			return "CMCD-Object:";
		case HeaderGroup::eREQUEST:
			return "CMCD-Request:";
		case HeaderGroup::eSESSION:
			return "CMCD-Session:";
		case HeaderGroup::eSTATUS:
			return "CMCD-Status:";
	}
	return "CMCD-Object:";
}

int RoundToNearest100(int value)
{
	if (value <= 0)
	{
		// Negative/zero mean "unavailable"; returning 0 immediately also avoids
		// the INT_MIN + 50 signed overflow the formula below would produce.
		return 0;
	}
	return ((value + 50) / 100) * 100;
}

std::string QuoteString(std::string_view value)
{
	std::string result;
	result.reserve(value.size() + 2);
	result += '"';
	for (char c : value)
	{
		if (c == '"' || c == '\\')
		{
			result += '\\';
		}
		result += c;
	}
	result += '"';
	return result;
}

std::vector<std::string> SerializeHeaders(const std::vector<Entry> &entries)
{
	static constexpr HeaderGroup kGroupOrder[] =
	{
		HeaderGroup::eOBJECT, HeaderGroup::eREQUEST, HeaderGroup::eSESSION, HeaderGroup::eSTATUS
	};
	// Alphabetical key order within each group (CTA-5004 §3.2 requirement 6).
	std::vector<Entry> sorted{entries};
	std::stable_sort(sorted.begin(), sorted.end(),
	                 [](const Entry &a, const Entry &b) { return a.key < b.key; });
	std::vector<std::string> headers;
	for (HeaderGroup group : kGroupOrder)
	{
		std::string joined;
		for (const Entry &entry : sorted)
		{
			if (entry.group != group)
			{
				continue;
			}
			std::string token;
			switch (entry.kind)
			{
				case ValueKind::ePLAIN:
					token = entry.key + "=" + entry.value;
					break;
				case ValueKind::eBOOLEAN:
					if (entry.value == "1")
					{
						token = entry.key;
					}
					break;
				case ValueKind::eQUOTED:
					token = entry.key + "=" + QuoteString(entry.value);
					break;
			}
			if (token.empty())
			{
				continue;
			}
			if (!joined.empty())
			{
				joined += ',';
			}
			joined += token;
		}
		if (!joined.empty())
		{
			headers.push_back(HeaderName(group) + " " + joined);
		}
	}
	return headers;
}

} // namespace AampCMCD
