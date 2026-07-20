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
 * @file AampCMCDSerializerTestCases.cpp
 * @brief Tests for the AampCMCD serialization primitives.
 *
 * Pins the CTA-5004 §3 serialization contract: token encoding per ValueKind
 * (plain, boolean, quoted String), alphabetical key ordering within each
 * header, nearest-100 rounding, and the fixed Object/Request/Session/Status
 * header emission order.
 */

#include <string>
#include <vector>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "AampCMCDSerializer.h"

using ::testing::ElementsAre;
using ::testing::IsEmpty;

using AampCMCD::Entry;
using AampCMCD::HeaderGroup;
using AampCMCD::HeaderName;
using AampCMCD::QuoteString;
using AampCMCD::RoundToNearest100;
using AampCMCD::SerializeHeaders;
using AampCMCD::ValueKind;

TEST(AampCMCDSerializerTests, HeaderName_MapsAllGroups)
{
	EXPECT_EQ(HeaderName(HeaderGroup::eOBJECT), "CMCD-Object:");
	EXPECT_EQ(HeaderName(HeaderGroup::eREQUEST), "CMCD-Request:");
	EXPECT_EQ(HeaderName(HeaderGroup::eSESSION), "CMCD-Session:");
	EXPECT_EQ(HeaderName(HeaderGroup::eSTATUS), "CMCD-Status:");
}

TEST(AampCMCDSerializerTests, NoEntries_ProducesNoHeaders)
{
	EXPECT_THAT(SerializeHeaders({}), IsEmpty());
}

TEST(AampCMCDSerializerTests, PlainEntry_EmitsKeyEqualsValue)
{
	std::vector<Entry> entries{
		Entry{"ot", "v", HeaderGroup::eOBJECT, ValueKind::ePLAIN}
	};

	EXPECT_THAT(SerializeHeaders(entries), ElementsAre("CMCD-Object: ot=v"));
}

TEST(AampCMCDSerializerTests, TokensSortAlphabeticallyWithinGroup)
{
	// Entries arrive unsorted; the serializer must order keys alphabetically
	// within the header (CTA-5004 §3.2 requirement 6).
	std::vector<Entry> entries{
		Entry{"tb", "5000", HeaderGroup::eOBJECT, ValueKind::ePLAIN},
		Entry{"br", "2500", HeaderGroup::eOBJECT, ValueKind::ePLAIN},
		Entry{"ot", "v", HeaderGroup::eOBJECT, ValueKind::ePLAIN}
	};

	EXPECT_THAT(SerializeHeaders(entries), ElementsAre("CMCD-Object: br=2500,ot=v,tb=5000"));
}

TEST(AampCMCDSerializerTests, QuotedValue_IsWrappedAndEscaped)
{
	std::vector<Entry> entries{
		Entry{"sid", "abc-123", HeaderGroup::eSESSION, ValueKind::eQUOTED},
		Entry{"nor", "seg\"2\\a.ts", HeaderGroup::eREQUEST, ValueKind::eQUOTED}
	};

	EXPECT_THAT(SerializeHeaders(entries),
	            ElementsAre("CMCD-Request: nor=\"seg\\\"2\\\\a.ts\"",
	                        "CMCD-Session: sid=\"abc-123\""));
}

TEST(AampCMCDSerializerTests, QuoteString_EscapesQuotesAndBackslashes)
{
	EXPECT_EQ(QuoteString("abc"), "\"abc\"");
	EXPECT_EQ(QuoteString(""), "\"\"");
	EXPECT_EQ(QuoteString("a\"b"), "\"a\\\"b\"");
	EXPECT_EQ(QuoteString("a\\b"), "\"a\\\\b\"");
}

TEST(AampCMCDSerializerTests, RoundToNearest100_HalfUpAndUnavailable)
{
	EXPECT_EQ(RoundToNearest100(0), 0);
	EXPECT_EQ(RoundToNearest100(-100), 0);
	EXPECT_EQ(RoundToNearest100(49), 0);    // rounds to 0 -> caller omits as unavailable
	EXPECT_EQ(RoundToNearest100(50), 100);  // half rounds up
	EXPECT_EQ(RoundToNearest100(149), 100);
	EXPECT_EQ(RoundToNearest100(150), 200);
	EXPECT_EQ(RoundToNearest100(3049), 3000);
	EXPECT_EQ(RoundToNearest100(3050), 3100);
}

TEST(AampCMCDSerializerTests, GroupsEmitSeparateHeadersInFixedOrder)
{
	// Entries arrive in scrambled group order; headers must come out in the
	// fixed Object, Request, Session, Status order.
	std::vector<Entry> entries{
		Entry{"sid", "abc", HeaderGroup::eSESSION, ValueKind::ePLAIN},
		Entry{"bs", "1", HeaderGroup::eSTATUS, ValueKind::eBOOLEAN},
		Entry{"br", "2500", HeaderGroup::eOBJECT, ValueKind::ePLAIN},
		Entry{"bl", "3000", HeaderGroup::eREQUEST, ValueKind::ePLAIN}
	};

	EXPECT_THAT(SerializeHeaders(entries),
	            ElementsAre("CMCD-Object: br=2500",
	                        "CMCD-Request: bl=3000",
	                        "CMCD-Session: sid=abc",
	                        "CMCD-Status: bs"));
}

TEST(AampCMCDSerializerTests, BooleanTrue_EmitsBareKey)
{
	std::vector<Entry> entries{
		Entry{"bs", "1", HeaderGroup::eSTATUS, ValueKind::eBOOLEAN}
	};

	EXPECT_THAT(SerializeHeaders(entries), ElementsAre("CMCD-Status: bs"));
}

TEST(AampCMCDSerializerTests, BooleanFalse_IsOmitted)
{
	std::vector<Entry> entries{
		Entry{"bs", "0", HeaderGroup::eSTATUS, ValueKind::eBOOLEAN}
	};

	EXPECT_THAT(SerializeHeaders(entries), IsEmpty());
}

TEST(AampCMCDSerializerTests, OmittedBooleanDoesNotSuppressOtherTokens)
{
	std::vector<Entry> entries{
		Entry{"bs", "0", HeaderGroup::eSTATUS, ValueKind::eBOOLEAN},
		Entry{"ot", "v", HeaderGroup::eOBJECT, ValueKind::ePLAIN}
	};

	EXPECT_THAT(SerializeHeaders(entries), ElementsAre("CMCD-Object: ot=v"));
}

TEST(AampCMCDSerializerTests, EmptyPlainValue_StillEmitsToken)
{
	// Legacy behaviour: an unset session id is still serialized as "sid=".
	std::vector<Entry> entries{
		Entry{"sid", "", HeaderGroup::eSESSION, ValueKind::ePLAIN}
	};

	EXPECT_THAT(SerializeHeaders(entries), ElementsAre("CMCD-Session: sid="));
}
