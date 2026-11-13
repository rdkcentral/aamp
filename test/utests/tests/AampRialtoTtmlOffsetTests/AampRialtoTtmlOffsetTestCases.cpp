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
 * @file AampRialtoTtmlOffsetTestCases.cpp
 * @brief L1 tests for AampRialtoTtmlOffset.
 *
 * Oracle: the gstvipertransform rules the class mirrors.  The first document
 * with a begin= time picks the mode: |(begin - pts) - aampOffset| > duration
 * means absolute cue times (offset = begin - pts); otherwise a non-zero AAMP
 * offset is used as is; otherwise cue times already match the PTS
 * (PASSTHROUGH, offset 0 from then on).
 */

#include <gtest/gtest.h>

#include <string>

#include "AampRialtoTtmlOffset.h"

namespace
{
constexpr int64_t kPtsMs = 10'000;
constexpr int64_t kDurationMs = 2'000;

std::string Doc(const std::string &begin)
{
	return "<?xml version=\"1.0\"?><tt><body><p begin=\"" + begin +
		"\" end=\"99:00:00.000\">x</p></body></tt>";
}

int64_t Refine(AampRialtoTtmlOffset &offset, const std::string &doc,
	int64_t ptsMs = kPtsMs, int64_t aampOffsetMs = 0)
{
	return offset.Refine(doc.data(), doc.size(), ptsMs, kDurationMs,
		aampOffsetMs);
}
}

// ---------------------------------------------------------------------------
// Parsing
// ---------------------------------------------------------------------------

/**
 * @test ParseTimeToMs_ClockValue
 */
TEST(AampRialtoTtmlOffsetTest, ParseTimeToMs_ClockValue)
{
	EXPECT_EQ(AampRialtoTtmlOffset::ParseTimeToMs("01:02:03.500"), 3'723'500);
	EXPECT_EQ(AampRialtoTtmlOffset::ParseTimeToMs("00:00:00"), 0);
	EXPECT_EQ(AampRialtoTtmlOffset::ParseTimeToMs("00:00:00.251"), 251);
	EXPECT_EQ(AampRialtoTtmlOffset::ParseTimeToMs("00:00:00.2505"), 251);
}

/**
 * @test ParseTimeToMs_AbsoluteHoursAndFrames_AreAccepted
 */
TEST(AampRialtoTtmlOffsetTest, ParseTimeToMs_AbsoluteHoursAndFrames_AreAccepted)
{
	// Wall-clock TTML can count hours since 1970.
	EXPECT_EQ(AampRialtoTtmlOffset::ParseTimeToMs("495000:00:01"),
		495'000LL * 3'600'000 + 1'000);
	// The frames part of HH:MM:SS:FF is ignored, as before.
	EXPECT_EQ(AampRialtoTtmlOffset::ParseTimeToMs("00:00:02:12"), 2'000);
	// TTML allows second 60 for a leap second.
	EXPECT_EQ(AampRialtoTtmlOffset::ParseTimeToMs("00:00:60"), 60'000);
}

/**
 * @test ParseTimeToMs_NotAClockValue_IsRejected
 */
TEST(AampRialtoTtmlOffsetTest, ParseTimeToMs_NotAClockValue_IsRejected)
{
	for (const char *bad : {"12.5s", "", "00:00", "a0:00:00",
		"-01:00:00", "00:-1:00", "00:00:nan", "00:00:inf",
		"00:60:00", "00:00:61", "00:00:01.x", "00:00:01junk",
		"99999999999999999999:00:00"})
	{
		EXPECT_FALSE(AampRialtoTtmlOffset::ParseTimeToMs(bad)) << bad;
	}
}

/**
 * @test FindFirstBeginMs_ReturnsTheFirstOnly
 */
TEST(AampRialtoTtmlOffsetTest, FindFirstBeginMs_ReturnsTheFirstOnly)
{
	int64_t ms = -1;
	ASSERT_TRUE(AampRialtoTtmlOffset::FindFirstBeginMs(
		"<p begin=\"00:00:05.000\"/><p begin=\"00:00:09.000\"/>", ms));
	EXPECT_EQ(ms, 5'000);
}

/**
 * @test FindFirstBeginMs_MissingOrUnterminated_ReturnsFalse
 */
TEST(AampRialtoTtmlOffsetTest, FindFirstBeginMs_MissingOrUnterminated_ReturnsFalse)
{
	int64_t ms = -1;
	EXPECT_FALSE(AampRialtoTtmlOffset::FindFirstBeginMs("<tt/>", ms));
	EXPECT_FALSE(AampRialtoTtmlOffset::FindFirstBeginMs(
		"<p begin=\"00:00:05.000", ms));
	EXPECT_EQ(ms, -1);
}

/**
 * @test FindFirstBeginMs_Unparsable_ReturnsFalse
 */
TEST(AampRialtoTtmlOffsetTest, FindFirstBeginMs_Unparsable_ReturnsFalse)
{
	int64_t ms = -1;
	EXPECT_FALSE(AampRialtoTtmlOffset::FindFirstBeginMs(
		"<p begin=\"-00:00:05.000\"/>", ms));
	EXPECT_EQ(ms, -1);
}

/**
 * @test UnparsableBegin_DoesNotInventAnOffset
 */
TEST(AampRialtoTtmlOffsetTest, UnparsableBegin_DoesNotInventAnOffset)
{
	AampRialtoTtmlOffset offset;
	// Read as 0 ms, "12.5s" made the offset -PTS and wrongly picked
	// LINEAR_OFFSET; an unreadable begin= must leave the mode undecided.
	EXPECT_EQ(Refine(offset, Doc("12.5s")), 0);
	EXPECT_EQ(Refine(offset, Doc("00:00:10.000")), 0);
	EXPECT_EQ(Refine(offset, Doc("01:00:10.000")), 0);
}

// ---------------------------------------------------------------------------
// Mode detection
// ---------------------------------------------------------------------------

/**
 * @test CueTimesMatchPts_IsPassthroughForGood
 */
TEST(AampRialtoTtmlOffsetTest, CueTimesMatchPts_IsPassthroughForGood)
{
	AampRialtoTtmlOffset offset;
	EXPECT_EQ(Refine(offset, Doc("00:00:10.000")), 0);
	// Once PASSTHROUGH, even absolute cue times are not corrected.
	EXPECT_EQ(Refine(offset, Doc("01:00:10.000")), 0);
}

/**
 * @test AbsoluteCueTimes_OffsetIsBeginMinusPts
 */
TEST(AampRialtoTtmlOffsetTest, AbsoluteCueTimes_OffsetIsBeginMinusPts)
{
	AampRialtoTtmlOffset offset;
	EXPECT_EQ(Refine(offset, Doc("01:00:10.000")), 3'600'000);
}

/**
 * @test AampOffsetWithMatchingCues_UsesAampOffset
 */
TEST(AampRialtoTtmlOffsetTest, AampOffsetWithMatchingCues_UsesAampOffset)
{
	AampRialtoTtmlOffset offset;
	// begin - pts == aampOffset, so the cue times agree with AAMP.
	EXPECT_EQ(Refine(offset, Doc("00:00:15.000"), kPtsMs, 5'000), 5'000);
}

/**
 * @test LinearOffset_IsRefinedEachDocument
 */
TEST(AampRialtoTtmlOffsetTest, LinearOffset_IsRefinedEachDocument)
{
	AampRialtoTtmlOffset offset;
	ASSERT_EQ(Refine(offset, Doc("01:00:10.000")), 3'600'000);
	// The wall-clock base moved by 1 s.
	EXPECT_EQ(Refine(offset, Doc("01:00:13.000"), 12'000), 3'601'000);
	// Within one duration of the AAMP offset: the AAMP offset wins.
	EXPECT_EQ(Refine(offset, Doc("00:00:16.500"), 12'000, 4'000), 4'000);
}

/**
 * @test DocumentWithoutBegin_ReusesStoredOffset
 */
TEST(AampRialtoTtmlOffsetTest, DocumentWithoutBegin_ReusesStoredOffset)
{
	AampRialtoTtmlOffset offset;
	EXPECT_EQ(Refine(offset, "<?xml version=\"1.0\"?><tt/>"), 0);
	ASSERT_EQ(Refine(offset, Doc("01:00:10.000")), 3'600'000);
	EXPECT_EQ(Refine(offset, "<?xml version=\"1.0\"?><tt/>"), 3'600'000);
}

/**
 * @test ConcatenatedDocuments_OnlyTheFirstIsRead
 */
TEST(AampRialtoTtmlOffsetTest, ConcatenatedDocuments_OnlyTheFirstIsRead)
{
	AampRialtoTtmlOffset offset;
	// The first document has no begin=; the second must be ignored.
	const std::string doc = "<?xml version=\"1.0\"?><tt/>" +
		Doc("01:00:10.000");
	EXPECT_EQ(Refine(offset, doc), 0);
}

/**
 * @test Reset_RedetectsTheMode
 */
TEST(AampRialtoTtmlOffsetTest, Reset_RedetectsTheMode)
{
	AampRialtoTtmlOffset offset;
	ASSERT_EQ(Refine(offset, Doc("00:00:10.000")), 0);
	offset.Reset();
	EXPECT_EQ(Refine(offset, Doc("01:00:10.000")), 3'600'000);
}
