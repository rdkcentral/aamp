/*
 * If not stated otherwise in this file or this component's license file the
 * following copyright and licenses apply:
 *
 * Copyright 2023 RDK Management
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

#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include <string_view>
#include <vector>
#include <cstdint>

#include "MockIsoBmffBuffer.h"
#include "AampLogManager.h"
#include "isobmff/isobmffhelper.h"

using ::testing::_;
using ::testing::Return;
using namespace std::literals;

// ---------------------------------------------------------------------------
// Binary helpers for GetIframeByteCap tests
// ---------------------------------------------------------------------------
namespace {

static void write4cc(std::vector<uint8_t> &buf, const char *cc)
{
	buf.push_back(uint8_t(cc[0]));
	buf.push_back(uint8_t(cc[1]));
	buf.push_back(uint8_t(cc[2]));
	buf.push_back(uint8_t(cc[3]));
}
static void write32be(std::vector<uint8_t> &buf, uint32_t v)
{
	buf.push_back(uint8_t(v >> 24));
	buf.push_back(uint8_t(v >> 16));
	buf.push_back(uint8_t(v >>  8));
	buf.push_back(uint8_t(v));
}

/* Patch a previously-written big-endian uint32 at byte position `pos`. */
static void patch32be(std::vector<uint8_t> &buf, size_t pos, uint32_t v)
{
	buf[pos]   = uint8_t(v >> 24);
	buf[pos+1] = uint8_t(v >> 16);
	buf[pos+2] = uint8_t(v >>  8);
	buf[pos+3] = uint8_t(v);
}

/* Write a FullBox header: size(4) + type(4) + version(1) + flags(3).
 * The size field is left as a placeholder (0) to be patched later. */
static size_t beginFullBox(std::vector<uint8_t> &buf, const char *type, uint8_t version = 0, uint32_t flags = 0)
{
	size_t startPos = buf.size();
	write32be(buf, 0);       // size placeholder
	write4cc(buf, type);
	buf.push_back(version);
	buf.push_back(uint8_t(flags >> 16));
	buf.push_back(uint8_t(flags >> 8));
	buf.push_back(uint8_t(flags));
	return startPos;
}
static void endBox(std::vector<uint8_t> &buf, size_t startPos)
{
	uint32_t size = static_cast<uint32_t>(buf.size() - startPos);
	patch32be(buf, startPos, size);
}

/* Build a minimal MOOF containing one TRAF/TFHD/TRUN.
 *
 * @param sampleSize       Per-sample size written into TRUN (0 = omit the field)
 * @param dataOffsetValue  Value to write as trun data_offset (-1 = omit the field)
 * @param tfhdDefaultSize  Value to write into TFHD default_sample_size (0 = omit)
 * Returns the serialised MOOF bytes. */
static std::vector<uint8_t> buildMoof(
	uint32_t sampleSize,
	int32_t  dataOffsetValue,  // -1 means "don't include data_offset in TRUN"
	uint32_t tfhdDefaultSize = 0)
{
	std::vector<uint8_t> buf;

	size_t moofStart = buf.size();
	write32be(buf, 0); // MOOF size placeholder
	write4cc(buf, "moof");

	// MFHD
	{
		size_t s = buf.size();
		write32be(buf, 0); write4cc(buf, "mfhd");
		buf.push_back(0); buf.push_back(0); buf.push_back(0); buf.push_back(0); // v+flags
		write32be(buf, 1); // sequence_number
		endBox(buf, s);
	}

	// TRAF
	size_t trafStart = buf.size();
	write32be(buf, 0); write4cc(buf, "traf");

	// TFHD
	{
		uint32_t tfhdFlags = (tfhdDefaultSize > 0) ? 0x000010u : 0u; // default_sample_size_present
		size_t s = beginFullBox(buf, "tfhd", 0, tfhdFlags);
		write32be(buf, 1); // track_ID
		if (tfhdDefaultSize > 0)
			write32be(buf, tfhdDefaultSize);
		endBox(buf, s);
	}

	// TRUN
	{
		bool hasDataOffset  = (dataOffsetValue != -1);
		bool hasSampleSize  = (sampleSize > 0);
		uint32_t trunFlags  = (hasDataOffset ? 0x0001u : 0u)
		                    | (hasSampleSize  ? 0x0200u : 0u);
		size_t s = beginFullBox(buf, "trun", 0, trunFlags);
		write32be(buf, 1); // sample_count = 1
		if (hasDataOffset)
			write32be(buf, static_cast<uint32_t>(dataOffsetValue));
		if (hasSampleSize)
			write32be(buf, sampleSize);
		endBox(buf, s);
	}

	endBox(buf, trafStart);
	endBox(buf, moofStart);
	return buf;
}

} // anonymous namespace



class IsoBmffHelperTests : public ::testing::Test
{
	protected:
		std::shared_ptr<IsoBmffHelper> helper;

		void SetUp() override
		{
			g_mockIsoBmffBuffer = std::make_shared<MockIsoBmffBuffer>();
			helper = std::make_shared<IsoBmffHelper>();
		}

		void TearDown() override
		{
			g_mockIsoBmffBuffer.reset();
			helper.reset();
		}
};


/**
 * @brief Test the PTS restamp function (positive case)
 *        Verify that the expected IsoBmffBuffer methods are called when
 *        RestampPts() function is called.
 */
TEST_F(IsoBmffHelperTests, restampPtsTest)
{
	static constexpr auto BUFFER = "IsoBmff buffer content"sv;
	std::vector<uint8_t> buffer(BUFFER.begin(), BUFFER.end());
	int64_t ptsOffset{123};
    std::string url("Dummy");
	const char* trackName = "video";
	uint32_t timeScale = 48000;
	// Check that setBuffer receives the actual buffer pointer
	auto expectedPtr = buffer.data();
	EXPECT_CALL(*g_mockIsoBmffBuffer, setBuffer(expectedPtr, buffer.size()));
	EXPECT_CALL(*g_mockIsoBmffBuffer, parseBuffer(false, -1)).WillOnce(Return(true));
	EXPECT_CALL(*g_mockIsoBmffBuffer, restampPts(ptsOffset));
	EXPECT_CALL(*g_mockIsoBmffBuffer, getSegmentDuration());
	EXPECT_TRUE(helper->RestampPts(buffer, ptsOffset,url, trackName, timeScale));
}

/**
 * @brief Test the PTS restamp function (negative case)
 *        Verify that IsoBmffBuffer::restampPts() is not called if
 *        IsoBmffBuffer::parseBuffer() fails, when RestampPts() function
 *        is called.
 */
TEST_F(IsoBmffHelperTests, restampPtsNegativeTest)
{
	static constexpr auto BUFFER = "IsoBmff buffer content"sv;
	std::vector<uint8_t> buffer(BUFFER.begin(), BUFFER.end());
	int64_t ptsOffset{123};
    std::string url("Dummy");
	const char* trackName = "video";
	uint32_t timeScale = 48000;
	auto expectedPtr = buffer.data();
	EXPECT_CALL(*g_mockIsoBmffBuffer, setBuffer(expectedPtr, buffer.size()));
	EXPECT_CALL(*g_mockIsoBmffBuffer, parseBuffer(false, -1)).WillOnce(Return(false));
	EXPECT_CALL(*g_mockIsoBmffBuffer, restampPts(_)).Times(0);
	EXPECT_CALL(*g_mockIsoBmffBuffer, getSegmentDuration()).Times(0);
	EXPECT_FALSE(helper->RestampPts(buffer, ptsOffset, url, trackName, timeScale));
}

/**
 * @brief Test the set PTS and duration function (positive case)
 *        Verify that the expected IsoBmffBuffer methods are called when
 *        SetPtsAndDuration() function is called.
 */
TEST_F(IsoBmffHelperTests, setPtsAndDurationTest)
{
	static constexpr auto BUFFER = "IsoBmff buffer content"sv;
	std::vector<uint8_t> buffer(BUFFER.begin(), BUFFER.end());
	uint64_t pts{123};
	uint64_t duration{1};
	auto expectedPtr = buffer.data();
	EXPECT_CALL(*g_mockIsoBmffBuffer, setBuffer(expectedPtr, buffer.size()));
	EXPECT_CALL(*g_mockIsoBmffBuffer, parseBuffer(false, -1)).WillOnce(Return(true));
	EXPECT_CALL(*g_mockIsoBmffBuffer, setPtsAndDuration(pts, duration));
	EXPECT_TRUE(helper->SetPtsAndDuration(buffer, pts, duration));
}

/**
 * @brief Test the set PTS and duration function (positive case)
 *        Verify that IsoBmffBuffer::setPtsAndDuration() is not called if
 *        IsoBmffBuffer::parseBuffer() fails, when SetPtsAndDuration() function
 *        is called.
 */
TEST_F(IsoBmffHelperTests, setPtsAndDurationNegativeTest)
{
	static constexpr auto BUFFER = "IsoBmff buffer content"sv;
	std::vector<uint8_t> buffer(BUFFER.begin(), BUFFER.end());
	uint64_t pts{123};
	uint64_t duration{1};
	auto expectedPtr = buffer.data();
	EXPECT_CALL(*g_mockIsoBmffBuffer, setBuffer(expectedPtr, buffer.size()));
	EXPECT_CALL(*g_mockIsoBmffBuffer, parseBuffer(false, -1)).WillOnce(Return(false));
	EXPECT_CALL(*g_mockIsoBmffBuffer, setPtsAndDuration(_, _)).Times(0);
	EXPECT_FALSE(helper->SetPtsAndDuration(buffer, pts, duration));
}

/**
 * @brief Test the set timescale function
 *        Verify that IsoBmffBuffer::SetTimescale() is called
 */
TEST_F(IsoBmffHelperTests, setTimescaleTest)
{
	static constexpr auto BUFFER = "IsoBmff buffer content"sv;
	std::vector<uint8_t> buffer(BUFFER.begin(), BUFFER.end());
	auto expectedPtr = buffer.data();
	EXPECT_CALL(*g_mockIsoBmffBuffer, setBuffer(expectedPtr, buffer.size()));
	EXPECT_CALL(*g_mockIsoBmffBuffer, parseBuffer(false, -1)).WillOnce(Return(true));
	EXPECT_CALL(*g_mockIsoBmffBuffer, setTrickmodeTimescale(1000)).WillOnce(Return(true));
	EXPECT_TRUE(helper->SetTimescale(buffer, 1000));
}

/**
 * @brief Test the set timescale function (negative case)
 *        Verify that SetTimescale returns false if
 *        IsoBmffBuffer::setTrickmodeTimescale() fails
 */

TEST_F(IsoBmffHelperTests, setTimescaleTestNegativeTest)
{
	static constexpr auto BUFFER = "IsoBmff buffer content"sv;
	std::vector<uint8_t> buffer(BUFFER.begin(), BUFFER.end());
	auto expectedPtr = buffer.data();
	EXPECT_CALL(*g_mockIsoBmffBuffer, setBuffer(expectedPtr, buffer.size()));
	EXPECT_CALL(*g_mockIsoBmffBuffer, parseBuffer(false, -1)).WillOnce(Return(true));
	EXPECT_CALL(*g_mockIsoBmffBuffer, setTrickmodeTimescale(1000)).WillOnce(Return(false));
	EXPECT_FALSE(helper->SetTimescale(buffer, 1000));
}

/**
 * @brief Test the ClearMediaHeaderDuration function
 *        Verify that IsoBmffBuffer::clearMediaHeaderDuration() is called
 */
TEST_F(IsoBmffHelperTests, clearMediaHeaderDurationTest)
{
	static constexpr auto BUFFER = "IsoBmff buffer content"sv;
	std::vector<uint8_t> buffer(BUFFER.begin(), BUFFER.end());
	auto expectedPtr = buffer.data();
	EXPECT_CALL(*g_mockIsoBmffBuffer, setBuffer(expectedPtr, buffer.size()));
	EXPECT_CALL(*g_mockIsoBmffBuffer, parseBuffer(false, -1)).WillOnce(Return(true));
	EXPECT_CALL(*g_mockIsoBmffBuffer, isInitSegment()).WillOnce(Return(true));
	EXPECT_CALL(*g_mockIsoBmffBuffer, setMediaHeaderDuration(0)).WillOnce(Return(true));
	EXPECT_TRUE(helper->ClearMediaHeaderDuration(buffer));
}

/**
 * @brief Test the ClearMediaHeaderDuration function (negative case)
 *        Verify that ClearMediaHeaderDuration returns false if
 *        IsoBmffBuffer::isInitSegment() fails
 */
TEST_F(IsoBmffHelperTests, clearMediaHeaderDurationNegativeTest_1)
{
	static constexpr auto BUFFER = "IsoBmff buffer content"sv;
	std::vector<uint8_t> buffer(BUFFER.begin(), BUFFER.end());
	auto expectedPtr = buffer.data();
	EXPECT_CALL(*g_mockIsoBmffBuffer, setBuffer(expectedPtr, buffer.size()));
	EXPECT_CALL(*g_mockIsoBmffBuffer, parseBuffer(false, -1)).WillOnce(Return(true));
	EXPECT_CALL(*g_mockIsoBmffBuffer, isInitSegment()).WillOnce(Return(false));
	EXPECT_FALSE(helper->ClearMediaHeaderDuration(buffer));
}

/**
 * @brief Test the ClearMediaHeaderDuration function (negative case)
 *        Verify that ClearMediaHeaderDuration returns false if
 *        IsoBmffBuffer::clearMediaHeaderDuration() fails
 */
TEST_F(IsoBmffHelperTests, clearMediaHeaderDurationNegativeTest_2)
{
	static constexpr auto BUFFER = "IsoBmff buffer content"sv;
	std::vector<uint8_t> buffer(BUFFER.begin(), BUFFER.end());
	auto expectedPtr = buffer.data();
	EXPECT_CALL(*g_mockIsoBmffBuffer, setBuffer(expectedPtr, buffer.size()));
	EXPECT_CALL(*g_mockIsoBmffBuffer, parseBuffer(false, -1)).WillOnce(Return(true));
	EXPECT_CALL(*g_mockIsoBmffBuffer, isInitSegment()).WillOnce(Return(true));
	EXPECT_CALL(*g_mockIsoBmffBuffer, setMediaHeaderDuration(0)).WillOnce(Return(false));
	EXPECT_FALSE(helper->ClearMediaHeaderDuration(buffer));

}
// ---------------------------------------------------------------------------
// GetIframeByteCap tests
// These tests call the static method directly with hand-crafted binary data,
// so no mock or test fixture is required.
// ---------------------------------------------------------------------------

/**
 * @brief Returns 0 when the buffer is too small to contain a MOOF header.
 */
TEST(GetIframeByteCap, BufferTooSmall_ReturnsZero)
{
	uint8_t tiny[] = {0x00, 0x00, 0x00, 0x10}; // only 4 bytes
	EXPECT_EQ(0u, IsoBmffHelper::GetIframeByteCap(tiny, 4));
}

/**
 * @brief Returns 0 when the first box type is not 'moof'.
 */
TEST(GetIframeByteCap, WrongBoxType_ReturnsZero)
{
	std::vector<uint8_t> buf;
	write32be(buf, 16);      // size = 16
	write4cc(buf, "mdat");   // wrong type
	buf.resize(16, 0);
	EXPECT_EQ(0u, IsoBmffHelper::GetIframeByteCap(buf.data(), buf.size()));
}

/**
 * @brief Returns 0 when only part of the MOOF has been received (incremental
 *        download simulation).  The caller should keep buffering and retry.
 */
TEST(GetIframeByteCap, IncompleteMoof_ReturnsZero)
{
	// Build a complete MOOF, then present only the first half.
	auto moof = buildMoof(/*sampleSize=*/1024, /*dataOffset=*/-1);
	size_t half = moof.size() / 2;
	EXPECT_EQ(0u, IsoBmffHelper::GetIframeByteCap(moof.data(), half));
}

/**
 * @brief Returns 0 when the MOOF has no TRAF child, so the first-sample
 *        size cannot be determined.
 */
TEST(GetIframeByteCap, NoTrafChild_ReturnsZero)
{
	// Minimal MOOF containing only MFHD (no TRAF).
	std::vector<uint8_t> buf;
	size_t moofStart = buf.size();
	write32be(buf, 0); write4cc(buf, "moof");
	// MFHD
	{
		size_t s = buf.size();
		write32be(buf, 0); write4cc(buf, "mfhd");
		buf.push_back(0); buf.push_back(0); buf.push_back(0); buf.push_back(0);
		write32be(buf, 1);
		patch32be(buf, s, uint32_t(buf.size() - s));
	}
	patch32be(buf, moofStart, uint32_t(buf.size() - moofStart));
	EXPECT_EQ(0u, IsoBmffHelper::GetIframeByteCap(buf.data(), buf.size()));
}

/**
 * @brief Returns 0 when TRUN is present but neither per-sample size
 *        (kTrunSampleSizePresent) nor TFHD default_sample_size is set.
 */
TEST(GetIframeByteCap, NoSampleSizeInfo_ReturnsZero)
{
	// sampleSize=0 means "omit the field", tfhdDefaultSize=0 too.
	auto moof = buildMoof(/*sampleSize=*/0, /*dataOffset=*/-1, /*tfhdDefaultSize=*/0);
	EXPECT_EQ(0u, IsoBmffHelper::GetIframeByteCap(moof.data(), moof.size()));
}

/**
 * @brief Plain unencrypted layout: data_offset field absent in TRUN.
 *        The cap falls back to moofSize + 8 + sampleSize.
 */
TEST(GetIframeByteCap, PlainLayout_NoDataOffset_CorrectCap)
{
	constexpr uint32_t kSampleSize = 512;
	// dataOffset = -1 → field omitted from TRUN
	auto moof = buildMoof(kSampleSize, /*dataOffset=*/-1);
	size_t moofSize = moof.size();

	size_t expected = moofSize + 8u + kSampleSize;
	EXPECT_EQ(expected, IsoBmffHelper::GetIframeByteCap(moof.data(), moof.size()));
}

/**
 * @brief Plain unencrypted layout: data_offset equals moofSize+8 exactly
 *        (sample starts right after the MDAT header).  The validation
 *        rejects this value (not strictly greater than moofSize+8) and
 *        falls back to moofSize + 8 + sampleSize, producing the same result.
 */
TEST(GetIframeByteCap, PlainLayout_DataOffsetEqualsThreshold_FallbackCap)
{
	constexpr uint32_t kSampleSize = 256;
	auto moof = buildMoof(kSampleSize, /*dataOffset=*/0); // placeholder; we'll patch it
	size_t moofSize = moof.size();

	// Reconstruct with the exact threshold value.
	int32_t dataOffsetExact = static_cast<int32_t>(moofSize + 8u);
	auto moof2 = buildMoof(kSampleSize, dataOffsetExact);
	// moofSize of moof2 is the same (data_offset is embedded in the box, not after it)
	size_t moofSize2 = moof2.size();

	// Expected: falls back because data_offset == moofSize+8 (not strictly greater)
	size_t expected = moofSize2 + 8u + kSampleSize;
	EXPECT_EQ(expected, IsoBmffHelper::GetIframeByteCap(moof2.data(), moof2.size()));
}

/**
 * @brief Encrypted CMAF layout: data_offset > moofSize+8 because auxiliary
 *        CENC data (subsample encryption info) precedes the actual sample
 *        payload.  The cap must be data_offset + sampleSize, not the
 *        (smaller) moofSize + 8 + sampleSize.
 */
TEST(GetIframeByteCap, EncryptedLayout_DataOffsetWithAuxInfo_CorrectCap)
{
	constexpr uint32_t kSampleSize  = 1024;
	constexpr uint32_t kAuxInfoSize = 32; // 2 × (8-byte IV + 2-byte count + 6-byte entry)

	// First pass to learn moofSize.
	auto probe = buildMoof(kSampleSize, 0 /* placeholder */);
	size_t moofSize = probe.size();

	int32_t dataOffset = static_cast<int32_t>(moofSize + 8u + kAuxInfoSize);
	auto moof = buildMoof(kSampleSize, dataOffset);
	ASSERT_EQ(moofSize, moof.size()) << "MOOF size must not change between passes";

	size_t expected = static_cast<size_t>(dataOffset) + kSampleSize;
	EXPECT_EQ(expected, IsoBmffHelper::GetIframeByteCap(moof.data(), moof.size()));
	// Also verify it is strictly larger than the plain-layout cap would be.
	EXPECT_GT(expected, moofSize + 8u + kSampleSize);
}

/**
 * @brief TRUN per-sample size absent, but TFHD default_sample_size is set.
 *        The cap is derived from the TFHD fallback value.
 */
TEST(GetIframeByteCap, TfhdDefaultSampleSize_UsedWhenTrunSizeAbsent)
{
	constexpr uint32_t kDefaultSize = 768;
	// sampleSize=0 → TRUN has no per-sample size field; tfhdDefaultSize provides it.
	auto moof = buildMoof(/*sampleSize=*/0, /*dataOffset=*/-1, kDefaultSize);
	size_t moofSize = moof.size();

	size_t expected = moofSize + 8u + kDefaultSize;
	EXPECT_EQ(expected, IsoBmffHelper::GetIframeByteCap(moof.data(), moof.size()));
}
