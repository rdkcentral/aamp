/*
 * If not stated otherwise in this file or this component's license file the
 * following copyright and licenses apply:
 *
 * Copyright 2020 RDK Management
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

#include <cstdlib>
#include <cstdint>
#include <iostream>
#include <string>
#include <string.h>
#include <vector>
#include <utility>

//include the google test dependencies
#include <gtest/gtest.h>
#include <gmock/gmock.h>

#include "AampLogManager.h"
#include "MockAampConfig.h"
#include "MockAampUtils.h"
#include "AampMPDUtils.h"

AampConfig *gpGlobalConfig{nullptr};

class AampMPDUtils : public ::testing::Test
{
protected:
	void SetUp() override
	{
	}

	void TearDown() override
	{
	}
};


TEST(AampMPDUtils, IsCompatibleMimeTypeTest1)
{
    AampMediaType mediaType[21] = {
    eMEDIATYPE_DEFAULT,
    eMEDIATYPE_VIDEO,
    eMEDIATYPE_AUDIO,
    eMEDIATYPE_SUBTITLE,
    eMEDIATYPE_RESERVED,
    eMEDIATYPE_MANIFEST,
    eMEDIATYPE_LICENCE,
    eMEDIATYPE_IFRAME,
    eMEDIATYPE_INIT_VIDEO,
    eMEDIATYPE_INIT_AUDIO,
    eMEDIATYPE_INIT_SUBTITLE,
    eMEDIATYPE_INIT_RESERVED,
    eMEDIATYPE_PLAYLIST_VIDEO,
    eMEDIATYPE_PLAYLIST_AUDIO,
    eMEDIATYPE_PLAYLIST_SUBTITLE,
    eMEDIATYPE_PLAYLIST_RESERVED,
    eMEDIATYPE_PLAYLIST_IFRAME,
    eMEDIATYPE_INIT_IFRAME,
    eMEDIATYPE_DSM_CC,
    eMEDIATYPE_IMAGE,
    eMEDIATYPE_DEFAULT
    };
    std::string mimeType = "test";
    for(int i=0; i<21; i++){
    bool minetype = IsCompatibleMimeType(mimeType,mediaType[i]);
	EXPECT_FALSE(minetype);
    }
}

TEST(AampMPDUtils, IsCompatibleMimeTypeTest2)
{
    AampMediaType mediaType = eMEDIATYPE_AUDIO;
    std::string mimeType = "audio/webm";
    bool result = IsCompatibleMimeType(mimeType,mediaType);
	EXPECT_TRUE(result);
}
TEST(AampMPDUtils, IsCompatibleMimeTypeTest3)
{
    AampMediaType mediaType = eMEDIATYPE_SUBTITLE;
    std::string mimeType = "application/ttml+xml";
    bool minetype = IsCompatibleMimeType(mimeType,mediaType);
	EXPECT_TRUE(minetype);
}
TEST(AampMPDUtils, ComputeFragmentDurationTest1)
{
	uint32_t duration = 0;
	uint32_t timeScale = 50;

	double result = ComputeFragmentDuration(duration,timeScale);
	EXPECT_DOUBLE_EQ(result,2.0);
}

// ---------------------------------------------------------------------------
// ParseSegmentIndexBox test helpers
// ---------------------------------------------------------------------------

/**
 * @brief Write a big-endian 32-bit value into a byte buffer.
 * @param[out] dest  Destination pointer (must have >= 4 bytes available).
 * @param[in]  val   Value to write.
 */
static void WriteBE32(uint8_t *dest, uint32_t val)
{
	dest[0] = static_cast<uint8_t>((val >> 24) & 0xFF);
	dest[1] = static_cast<uint8_t>((val >> 16) & 0xFF);
	dest[2] = static_cast<uint8_t>((val >> 8)  & 0xFF);
	dest[3] = static_cast<uint8_t>( val        & 0xFF);
}

/**
 * @brief Write a big-endian 16-bit value into a byte buffer.
 * @param[out] dest  Destination pointer (must have >= 2 bytes available).
 * @param[in]  val   Value to write.
 */
static void WriteBE16(uint8_t *dest, uint16_t val)
{
	dest[0] = static_cast<uint8_t>((val >> 8) & 0xFF);
	dest[1] = static_cast<uint8_t>( val       & 0xFF);
}

/**
 * @brief Represents a single SIDX reference entry for test construction.
 */
struct SidxEntry
{
	uint32_t referencedSize;
	uint32_t subsegmentDuration;
};

/**
 * @brief Build a version-0 SIDX box in memory for testing.
 *
 * Layout (version 0, all big-endian):
 *   size(4) | 'sidx'(4) | version+flags(4) | reference_ID(4) |
 *   timescale(4) | earliest_presentation_time(4) | first_offset(4) |
 *   reserved(2) | reference_count(2) |
 *   [ referenced_size(4) | subsegment_duration(4) | SAP_flags(4) ] * N
 *
 * The @p flags parameter populates the lower 24 bits of the version+flags
 * field (version byte is always 0). Defaults to 0 for the common case.
 *
 * @param[in] timescale    Timescale for duration calculation.
 * @param[in] entries      Vector of reference entries.
 * @param[in] firstOffset  Value for the first_offset field (default 0).
 * @param[in] flags        24-bit flags value in the lower bits of the
 *                         version+flags field (default 0). The version
 *                         byte (top 8 bits) is always 0.
 * @return                 Byte vector containing the complete SIDX box.
 */
static std::vector<uint8_t> BuildSidxBoxV0(
	uint32_t timescale,
	const std::vector<SidxEntry> &entries,
	uint32_t firstOffset = 0,
	uint32_t flags = 0)
{
	constexpr uint32_t HEADER_SIZE = 32;
	constexpr uint32_t ENTRY_SIZE  = 12;
	const auto entryCount = static_cast<uint32_t>(entries.size());
	const uint32_t boxSize = HEADER_SIZE + entryCount * ENTRY_SIZE;

	std::vector<uint8_t> buf(boxSize, 0);
	auto *p = buf.data();

	WriteBE32(p,      boxSize);
	p[4] = 's'; p[5] = 'i'; p[6] = 'd'; p[7] = 'x';
	WriteBE32(p + 8,  flags & 0x00FFFFFFu); // version=0, optional flags
	WriteBE32(p + 12, 1);                   // reference_ID
	WriteBE32(p + 16, timescale);
	WriteBE32(p + 20, 0);                   // earliest_presentation_time
	WriteBE32(p + 24, firstOffset);
	WriteBE16(p + 28, 0);                   // reserved
	WriteBE16(p + 30, static_cast<uint16_t>(entryCount));

	uint8_t *ep = p + HEADER_SIZE;
	for (const auto &entry : entries)
	{
		WriteBE32(ep,     entry.referencedSize & 0x7FFFFFFFu);
		WriteBE32(ep + 4, entry.subsegmentDuration);
		WriteBE32(ep + 8, 0);  // SAP flags
		ep += ENTRY_SIZE;
	}

	return buf;
}

/**
 * @brief Build a version-1 SIDX box in memory for testing.
 *
 * Version 1 uses 64-bit earliest_presentation_time and first_offset fields,
 * making the header 40 bytes instead of 32.
 *
 * Layout (version 1, all big-endian):
 *   size(4) | 'sidx'(4) | version+flags(4) | reference_ID(4) |
 *   timescale(4) | earliest_presentation_time(8) | first_offset(8) |
 *   reserved(2) | reference_count(2) |
 *   [ referenced_size(4) | subsegment_duration(4) | SAP_flags(4) ] * N
 *
 * @param[in] timescale    Timescale for duration calculation.
 * @param[in] entries      Vector of reference entries.
 * @param[in] firstOffset  Value for the first_offset field.
 * @return                 Byte vector containing the complete SIDX box.
 */
static std::vector<uint8_t> BuildSidxBoxV1(
	uint32_t timescale,
	const std::vector<SidxEntry> &entries,
	uint64_t firstOffset = 0)
{
	constexpr uint32_t HEADER_SIZE = 40; // 4+4+4+4+4+8+8+2+2
	constexpr uint32_t ENTRY_SIZE  = 12;
	const auto entryCount = static_cast<uint32_t>(entries.size());
	const uint32_t boxSize = HEADER_SIZE + entryCount * ENTRY_SIZE;

	std::vector<uint8_t> buf(boxSize, 0);
	auto *p = buf.data();

	WriteBE32(p,      boxSize);
	p[4] = 's'; p[5] = 'i'; p[6] = 'd'; p[7] = 'x';
	WriteBE32(p + 8,  0x01000000u); // version=1, flags=0
	WriteBE32(p + 12, 1);           // reference_ID
	WriteBE32(p + 16, timescale);
	// earliest_presentation_time (64-bit, offset 20)
	WriteBE32(p + 20, 0);
	WriteBE32(p + 24, 0);
	// first_offset (64-bit, offset 28)
	WriteBE32(p + 28, static_cast<uint32_t>(firstOffset >> 32));
	WriteBE32(p + 32, static_cast<uint32_t>(firstOffset & 0xFFFFFFFFu));
	WriteBE16(p + 36, 0); // reserved
	WriteBE16(p + 38, static_cast<uint16_t>(entryCount));

	uint8_t *ep = p + HEADER_SIZE;
	for (const auto &entry : entries)
	{
		WriteBE32(ep,     entry.referencedSize & 0x7FFFFFFFu);
		WriteBE32(ep + 4, entry.subsegmentDuration);
		WriteBE32(ep + 8, 0);  // SAP flags
		ep += ENTRY_SIZE;
	}

	return buf;
}

// ---------------------------------------------------------------------------
// ParseSegmentIndexBox tests
// ---------------------------------------------------------------------------

/**
 * @brief Null start pointer returns false.
 */
TEST(AampMPDUtils, ParseSegmentIndexBox_NullStart_ReturnsFalse)
{
	unsigned int refSize = 0;
	float refDuration = 0.0f;
	EXPECT_FALSE(ParseSegmentIndexBox(nullptr, 0, 0, &refSize, &refDuration, nullptr));
}

/**
 * @brief Version-0 box with non-zero flags is parsed correctly.
 *
 * The version+flags field has version=0 in the top byte and non-zero
 * values in the lower 24 bits (flags). The parser must isolate the top
 * 8-bit version and treat this as a v0 box (32-bit time fields), not v1.
 */
TEST(AampMPDUtils, ParseSegmentIndexBox_V0WithNonZeroFlags_ParsedCorrectly)
{
	constexpr uint32_t TIMESCALE = 1000;
	constexpr uint32_t FLAGS     = 0x000001u; // non-zero flags, version still 0
	constexpr uint32_t REF_SIZE  = 5000;
	constexpr uint32_t DURATION  = 2000;

	auto sidx = BuildSidxBoxV0(TIMESCALE, {{REF_SIZE, DURATION}}, 0, FLAGS);
	unsigned int refSize = 0;
	float refDuration = 0.0f;
	EXPECT_TRUE(ParseSegmentIndexBox(sidx.data(), sidx.size(), 0,
									 &refSize, &refDuration, nullptr));
	EXPECT_EQ(refSize, REF_SIZE);
	EXPECT_FLOAT_EQ(refDuration,
					 static_cast<float>(DURATION) / static_cast<float>(TIMESCALE));
}

/**
 * @brief Version-1 SIDX box (64-bit time fields) is parsed correctly.
 *
 * Version 1 uses 8-byte earliest_presentation_time and first_offset.
 * Verifies that the parser selects the correct (64-bit) read path and
 * returns the right size/duration for the requested entry.
 */
TEST(AampMPDUtils, ParseSegmentIndexBox_V1_ParsedCorrectly)
{
	constexpr uint32_t TIMESCALE = 90000;
	constexpr uint32_t REF_SIZE  = 12000;
	constexpr uint32_t DURATION  = 90000;  // 1 second

	auto sidx = BuildSidxBoxV1(TIMESCALE, {{REF_SIZE, DURATION}});
	unsigned int refSize = 0;
	float refDuration = 0.0f;
	EXPECT_TRUE(ParseSegmentIndexBox(sidx.data(), sidx.size(), 0,
									 &refSize, &refDuration, nullptr));
	EXPECT_EQ(refSize, REF_SIZE);
	EXPECT_FLOAT_EQ(refDuration,
					 static_cast<float>(DURATION) / static_cast<float>(TIMESCALE));
}

/**
 * @brief Negative segment index returns false without UB.
 *
 * A negative segmentIndex must be rejected before the skip arithmetic,
 * which would otherwise convert a negative product to a huge size_t.
 */
TEST(AampMPDUtils, ParseSegmentIndexBox_NegativeIndex_ReturnsFalse)
{
	constexpr uint32_t TIMESCALE = 1000;
	constexpr uint32_t REF_SIZE  = 100;
	constexpr uint32_t DURATION  = 1000;
	auto sidx = BuildSidxBoxV0(TIMESCALE, {{REF_SIZE, DURATION}});
	unsigned int refSize = 0;
	float refDuration = 0.0f;
	EXPECT_FALSE(ParseSegmentIndexBox(sidx.data(), sidx.size(), -1,
									  &refSize, &refDuration, nullptr));
}

/**
 * @brief Mismatched size field in box header returns false.
 */
TEST(AampMPDUtils, ParseSegmentIndexBox_WrongSize_ReturnsFalse)
{
	auto sidx = BuildSidxBoxV0(1000, {{100, 1000}});
	unsigned int refSize = 0;
	float refDuration = 0.0f;
	// Pass a size that does not match the box header
	EXPECT_FALSE(ParseSegmentIndexBox(sidx.data(), sidx.size() + 1, 0,
									  &refSize, &refDuration, nullptr));
}

/**
 * @brief Mismatched size field zeroes firstOffset and returns false.
 */
TEST(AampMPDUtils, ParseSegmentIndexBox_WrongSize_ZeroesFirstOffset)
{
	auto sidx = BuildSidxBoxV0(1000, {{100, 1000}});
	unsigned int firstOff = 99;
	EXPECT_FALSE(ParseSegmentIndexBox(sidx.data(), sidx.size() + 1, 0,
									  nullptr, nullptr, &firstOff));
	EXPECT_EQ(firstOff, 0u);
}

/**
 * @brief Wrong box type returns false.
 */
TEST(AampMPDUtils, ParseSegmentIndexBox_WrongType_ReturnsFalse)
{
	auto sidx = BuildSidxBoxV0(1000, {{100, 1000}});
	// Corrupt the type field ('sidx' at bytes 4-7)
	sidx[4] = 'f'; sidx[5] = 'r'; sidx[6] = 'e'; sidx[7] = 'e';
	unsigned int refSize = 0;
	float refDuration = 0.0f;
	EXPECT_FALSE(ParseSegmentIndexBox(sidx.data(), sidx.size(), 0,
									  &refSize, &refDuration, nullptr));
}

/**
 * @brief Request firstOffset returns the correct value and exits early.
 */
TEST(AampMPDUtils, ParseSegmentIndexBox_FirstOffset_ReturnsCorrectValue)
{
	constexpr uint32_t TIMESCALE      = 1000;
	constexpr uint32_t REF_SIZE       = 100;
	constexpr uint32_t DURATION       = 1000;
	constexpr uint32_t EXPECTED_OFFSET = 42;
	auto sidx = BuildSidxBoxV0(TIMESCALE, {{REF_SIZE, DURATION}}, EXPECTED_OFFSET);
	unsigned int firstOff = 0;
	EXPECT_TRUE(ParseSegmentIndexBox(sidx.data(), sidx.size(), 0,
									 nullptr, nullptr, &firstOff));
	EXPECT_EQ(firstOff, EXPECTED_OFFSET);
}

/**
 * @brief Parse the first segment entry (index 0).
 */
TEST(AampMPDUtils, ParseSegmentIndexBox_Index0_ReturnsCorrectValues)
{
	constexpr uint32_t TIMESCALE = 1000;
	constexpr uint32_t REF_SIZE = 5000;
	constexpr uint32_t DURATION = 2000;
	auto sidx = BuildSidxBoxV0(TIMESCALE, {{REF_SIZE, DURATION}});

	unsigned int refSize = 0;
	float refDuration = 0.0f;
	EXPECT_TRUE(ParseSegmentIndexBox(sidx.data(), sidx.size(), 0,
									 &refSize, &refDuration, nullptr));
	EXPECT_EQ(refSize, REF_SIZE);
	EXPECT_FLOAT_EQ(refDuration,
					 static_cast<float>(DURATION) / static_cast<float>(TIMESCALE));
}

/**
 * @brief Parse multiple SIDX entries and verify correct indexing.
 *
 * Ensures that ParseSegmentIndexBox correctly handles multiple entries
 * in the SIDX box and, for each requested segment index, returns the
 * expected referenced_size and referenced_duration values.
 */
TEST(AampMPDUtils, ParseSegmentIndexBox_MultipleEntries_CorrectIndexing)
{
	constexpr uint32_t TIMESCALE = 48000;
	const std::vector<SidxEntry> entries = {
		{10000, 48000},
		{20000, 96000},
		{30000, 144000}
	};
	auto sidx = BuildSidxBoxV0(TIMESCALE, entries);

	for (int i = 0; i < static_cast<int>(entries.size()); ++i)
	{
		unsigned int refSize = 0;
		float refDuration = 0.0f;
		EXPECT_TRUE(ParseSegmentIndexBox(sidx.data(), sidx.size(), i,
										 &refSize, &refDuration, nullptr))
			<< "Failed for segment index " << i;
		EXPECT_EQ(refSize, entries[i].referencedSize)
			<< "Wrong referenced_size for segment index " << i;
		EXPECT_FLOAT_EQ(refDuration,
						 static_cast<float>(entries[i].subsegmentDuration) /
						 static_cast<float>(TIMESCALE))
			<< "Wrong referenced_duration for segment index " << i;
	}
}

/**
 * @brief Segment index equal to reference_count returns false.
 */
TEST(AampMPDUtils, ParseSegmentIndexBox_IndexEqualToCount_ReturnsFalse)
{
	constexpr uint32_t TIMESCALE = 1000;
	const std::vector<SidxEntry> entries = {{100, 1000}, {200, 2000}};
	auto sidx = BuildSidxBoxV0(TIMESCALE, entries);
	unsigned int refSize = 0;
	float refDuration = 0.0f;
	EXPECT_FALSE(ParseSegmentIndexBox(sidx.data(), sidx.size(),
									  static_cast<int>(entries.size()),
									  &refSize, &refDuration, nullptr));
}

/**
 * @brief Segment index far out of range returns false.
 */
TEST(AampMPDUtils, ParseSegmentIndexBox_IndexFarOutOfRange_ReturnsFalse)
{
	constexpr uint32_t TIMESCALE      = 1000;
	constexpr uint32_t REF_SIZE       = 100;
	constexpr uint32_t DURATION       = 1000;
	constexpr int      OUT_OF_RANGE   = 99;
	auto sidx = BuildSidxBoxV0(TIMESCALE, {{REF_SIZE, DURATION}});
	unsigned int refSize = 0;
	float refDuration = 0.0f;
	EXPECT_FALSE(ParseSegmentIndexBox(sidx.data(), sidx.size(), OUT_OF_RANGE,
									  &refSize, &refDuration, nullptr));
}

/**
 * @brief Verify every entry in a large SIDX box is correctly indexed.
 *
 * This catches cumulative offset errors or off-by-one issues that only
 * manifest when many entries must be skipped.
 */
TEST(AampMPDUtils, ParseSegmentIndexBox_LargeEntryCount_AllCorrect)
{
	constexpr uint32_t TIMESCALE = 90000;
	constexpr int ENTRY_COUNT = 50;
	std::vector<SidxEntry> entries;
	entries.reserve(ENTRY_COUNT);
	for (int i = 0; i < ENTRY_COUNT; ++i)
	{
		entries.push_back({static_cast<uint32_t>(1000 * (i + 1)),
						   static_cast<uint32_t>(90000 * (i + 1))});
	}
	auto sidx = BuildSidxBoxV0(TIMESCALE, entries);

	for (int i = 0; i < ENTRY_COUNT; ++i)
	{
		unsigned int refSize = 0;
		float refDuration = 0.0f;
		EXPECT_TRUE(ParseSegmentIndexBox(sidx.data(), sidx.size(), i,
										 &refSize, &refDuration, nullptr))
			<< "Failed for segment index " << i;
		EXPECT_EQ(refSize, entries[i].referencedSize)
			<< "Wrong referenced_size for segment index " << i;
		EXPECT_FLOAT_EQ(refDuration,
						 static_cast<float>(entries[i].subsegmentDuration) /
						 static_cast<float>(TIMESCALE))
			<< "Wrong referenced_duration for segment index " << i;
	}
}

/**
 * @brief Zero reference_count SIDX box with index 0 returns false.
 */
TEST(AampMPDUtils, ParseSegmentIndexBox_ZeroEntries_ReturnsFalse)
{
	constexpr uint32_t TIMESCALE = 1000;
	auto sidx = BuildSidxBoxV0(TIMESCALE, {});
	unsigned int refSize = 0;
	float refDuration = 0.0f;
	EXPECT_FALSE(ParseSegmentIndexBox(sidx.data(), sidx.size(), 0,
									  &refSize, &refDuration, nullptr));
}

/**
 * @brief Last valid entry in a multi-entry box is returned correctly.
 */
TEST(AampMPDUtils, ParseSegmentIndexBox_LastEntry_ReturnsCorrectValues)
{
	constexpr uint32_t TIMESCALE = 44100;
	const std::vector<SidxEntry> entries = {
		{1000, 44100},
		{2000, 88200},
		{3000, 132300}
	};
	auto sidx = BuildSidxBoxV0(TIMESCALE, entries);

	const int lastIdx = static_cast<int>(entries.size()) - 1;
	unsigned int refSize = 0;
	float refDuration = 0.0f;
	EXPECT_TRUE(ParseSegmentIndexBox(sidx.data(), sidx.size(), lastIdx,
									 &refSize, &refDuration, nullptr));
	EXPECT_EQ(refSize, entries[lastIdx].referencedSize);
	EXPECT_FLOAT_EQ(refDuration,
					 static_cast<float>(entries[lastIdx].subsegmentDuration) /
					 static_cast<float>(TIMESCALE));
}
