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
 * @file CMCDSerializer.cpp
 * @brief Implementation of CTA-5004 §3 CMCD serialization primitives.
 *
 * Provides pure, dependency-free implementations of RoundToNearest100,
 * QuoteString, CMCDGroupToHeaderKey, and SerializeToCMCDMap. These functions
 * are the single point of truth for CMCD key encoding, rounding, quoting,
 * and alphabetical ordering within each header group.
 */

#include "CMCDSerializer.h"

#include <algorithm>
#include <map>
#include <string>

int RoundToNearest100(int value)
{
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

std::string CMCDGroupToHeaderKey(CMCDGroup group)
{
    switch (group)
    {
        case CMCDGroup::Object:
            return "CMCD-Object:";
        case CMCDGroup::Request:
            return "CMCD-Request:";
        case CMCDGroup::Session:
            return "CMCD-Session:";
        case CMCDGroup::Status:
            return "CMCD-Status:";
    }
    return "CMCD-Object:";
}

void SerializeToCMCDMap(const std::vector<CMCDEntry>& entries,
                        std::unordered_map<std::string, std::vector<std::string>>& out)
{
    // Sort a local copy alphabetically by key (SER-04: CTA-5004 §3 alphabetical ordering).
    std::vector<CMCDEntry> sorted = entries;
    std::sort(sorted.begin(), sorted.end(),
              [](const CMCDEntry& a, const CMCDEntry& b) { return a.key < b.key; });

    // Use std::map to accumulate comma-joined value per group (deterministic key order).
    std::map<std::string, std::string> groups;

    for (const auto& e : sorted)
    {
        std::string headerKey = CMCDGroupToHeaderKey(e.group);
        std::string token;

        if (e.isBoolToken)
        {
            if (e.value == "1")
            {
                token = e.key; // bare key only — SER-06
            }
            else
            {
                continue; // omit when false — SER-06
            }
        }
        else if (e.isInteger)
        {
            if (e.value.empty())
            {
                continue; // treat missing integer as "unavailable" — omit entry
            }
            int parsed = 0;
            try
            {
                parsed = std::stoi(e.value);
            }
            catch (const std::exception&)
            {
                continue; // malformed value — omit rather than crash
            }
            int r = RoundToNearest100(parsed);
            if (r == 0)
            {
                continue; // omit zero — Pitfall 3 / CTA-5004 optional-key rule
            }
            token = e.key + "=" + std::to_string(r); // SER-01 / SER-02
        }
        else if (e.isQuotedString)
        {
            token = e.key + "=" + QuoteString(e.value); // SER-03
        }
        else
        {
            token = e.key + "=" + e.value; // bare token value (e.g. ot=v)
        }

        if (!groups[headerKey].empty())
        {
            groups[headerKey] += ",";
        }
        groups[headerKey] += token;
    }

    // Write each group's joined value into the output map as a single-element vector
    // at index [0], matching the AampCMCDCollector::CMCDGetHeaders assembly contract.
    for (const auto& kv : groups)
    {
        out[kv.first] = {kv.second};
    }
}
