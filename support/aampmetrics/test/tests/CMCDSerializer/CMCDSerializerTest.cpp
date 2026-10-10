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
 * @file CMCDSerializerTest.cpp
 * @brief Direct unit tests for CMCDSerializer primitives (RoundToNearest100, QuoteString,
 *        CMCDGroupToHeaderKey, and SerializeToCMCDMap). Tests are free-function based
 *        (no fixture needed) since all serializer functions are pure free functions.
 */

#include <gtest/gtest.h>
#include "CMCDSerializer.h"

#include <string>
#include <unordered_map>
#include <vector>

// ---------------------------------------------------------------------------
// RoundToNearest100 tests
// ---------------------------------------------------------------------------

TEST(CMCDSerializer_RoundToNearest100, RoundsHalfUp)
{
    EXPECT_EQ(RoundToNearest100(3842), 3800);
    EXPECT_EQ(RoundToNearest100(6150), 6200);
    EXPECT_EQ(RoundToNearest100(50),   100);
    EXPECT_EQ(RoundToNearest100(100),  100);
    EXPECT_EQ(RoundToNearest100(150),  200);
}

TEST(CMCDSerializer_RoundToNearest100, SubFiftyRoundsToZero)
{
    EXPECT_EQ(RoundToNearest100(49), 0);
    EXPECT_EQ(RoundToNearest100(0),  0);
    EXPECT_EQ(RoundToNearest100(1),  0);
}

// ---------------------------------------------------------------------------
// QuoteString tests
// ---------------------------------------------------------------------------

TEST(CMCDSerializer_QuoteString, WrapsAndEscapes)
{
    // Plain string — just wrap in double-quotes
    EXPECT_EQ(QuoteString("abc"), "\"abc\"");

    // Interior double-quote must be backslash-escaped
    EXPECT_EQ(QuoteString("a\"b"), "\"a\\\"b\"");

    // Interior backslash must be backslash-escaped
    EXPECT_EQ(QuoteString("a\\b"), "\"a\\\\b\"");

    // Empty string
    EXPECT_EQ(QuoteString(""), "\"\"");
}

// ---------------------------------------------------------------------------
// CMCDGroupToHeaderKey tests
// ---------------------------------------------------------------------------

TEST(CMCDSerializer_CMCDGroupToHeaderKey, ReturnsCorrectHeaderKeys)
{
    EXPECT_EQ(CMCDGroupToHeaderKey(CMCDGroup::Object),  "CMCD-Object:");
    EXPECT_EQ(CMCDGroupToHeaderKey(CMCDGroup::Request), "CMCD-Request:");
    EXPECT_EQ(CMCDGroupToHeaderKey(CMCDGroup::Session), "CMCD-Session:");
    EXPECT_EQ(CMCDGroupToHeaderKey(CMCDGroup::Status),  "CMCD-Status:");
}

// ---------------------------------------------------------------------------
// SerializeToCMCDMap tests
// ---------------------------------------------------------------------------

TEST(CMCDSerializer_SerializeToCMCDMap, SortsAndRoundsObjectKeys)
{
    // Insert entries out of order: tb, ot, br — serializer must sort alphabetically
    // and round integer values. Expected: br=3800,ot=v,tb=6100
    std::vector<CMCDEntry> entries;

    CMCDEntry tb;
    tb.key = "tb";
    tb.value = "6100";
    tb.group = CMCDGroup::Object;
    tb.isInteger = true;
    entries.push_back(tb);

    CMCDEntry ot;
    ot.key = "ot";
    ot.value = "v";
    ot.group = CMCDGroup::Object;
    entries.push_back(ot);

    CMCDEntry br;
    br.key = "br";
    br.value = "3842";
    br.group = CMCDGroup::Object;
    br.isInteger = true;
    entries.push_back(br);

    std::unordered_map<std::string, std::vector<std::string>> out;
    SerializeToCMCDMap(entries, out);

    EXPECT_EQ(out.at("CMCD-Object:").at(0), "br=3800,ot=v,tb=6100");
}

TEST(CMCDSerializer_SerializeToCMCDMap, BoolTokenTrueEmitsBareKey)
{
    std::vector<CMCDEntry> entries;

    CMCDEntry bs;
    bs.key = "bs";
    bs.value = "1";
    bs.group = CMCDGroup::Status;
    bs.isBoolToken = true;
    entries.push_back(bs);

    std::unordered_map<std::string, std::vector<std::string>> out;
    SerializeToCMCDMap(entries, out);

    EXPECT_EQ(out.at("CMCD-Status:").at(0), "bs");
}

TEST(CMCDSerializer_SerializeToCMCDMap, BoolTokenFalseOmitsGroup)
{
    std::vector<CMCDEntry> entries;

    CMCDEntry bs;
    bs.key = "bs";
    bs.value = "0";
    bs.group = CMCDGroup::Status;
    bs.isBoolToken = true;
    entries.push_back(bs);

    std::unordered_map<std::string, std::vector<std::string>> out;
    SerializeToCMCDMap(entries, out);

    EXPECT_EQ(out.find("CMCD-Status:"), out.end());
}

TEST(CMCDSerializer_SerializeToCMCDMap, QuotesSid)
{
    std::vector<CMCDEntry> entries;

    CMCDEntry sid;
    sid.key = "sid";
    sid.value = "uuid-123";
    sid.group = CMCDGroup::Session;
    sid.isQuotedString = true;
    entries.push_back(sid);

    std::unordered_map<std::string, std::vector<std::string>> out;
    SerializeToCMCDMap(entries, out);

    EXPECT_EQ(out.at("CMCD-Session:").at(0), "sid=\"uuid-123\"");
}

TEST(CMCDSerializer_SerializeToCMCDMap, OmitsIntegerRoundingToZero)
{
    // Value "40" rounds to 0 — the entry must be omitted entirely.
    std::vector<CMCDEntry> entries;

    CMCDEntry br;
    br.key = "br";
    br.value = "40";
    br.group = CMCDGroup::Object;
    br.isInteger = true;
    entries.push_back(br);

    std::unordered_map<std::string, std::vector<std::string>> out;
    SerializeToCMCDMap(entries, out);

    // Group must not appear at all when every integer in it rounds to zero
    EXPECT_EQ(out.find("CMCD-Object:"), out.end());
}
