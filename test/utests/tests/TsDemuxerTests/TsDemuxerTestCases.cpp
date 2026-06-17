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
 * @file TsDemuxerTestCases.cpp
 * @brief Unit tests for the Demuxer class covering the PR changes:
 *
 *  1. UpdateSegmentInfo() restamp path (new early-return block) uses raw
 *     encoder ticks + ptsOffset instead of stale base_pts-relative arithmetic.
 *
 *  2. The removed end-of-function ptsOffset addition means ptsOffset is
 *     NOT added to output timestamps in non-restamp mode.
 *
 *  3. setPtsOffset() clears rollover_pts and sets suppress_rollover_detection
 *     so that the large PTS backward-jump at an SSAI boundary is not
 *     mistakenly treated as a 33-bit counter wrap.
 *
 *  4. init() resets suppress_rollover_detection back to false so the flag
 *     does not persist across segment boundaries.
 *
 *  5. Legitimate 33-bit PTS rollover (same encoder epoch) is still
 *     corrected in restamp mode.
 */

#include <gtest/gtest.h>
#include <gmock/gmock.h>

#include <array>
#include <cstdint>
#include <optional>

#include "tsDemuxer.hpp"
#include "priv_aamp.h"
#include "AampConfig.h"
#include "AampLogManager.h"
#include "MockAampConfig.h"
#include "MockPrivateInstanceAAMP.h"

using ::testing::_;
using ::testing::AnyNumber;
using ::testing::NiceMock;
using ::testing::Return;

// Named tolerances for floating-point comparisons used in tests.
static constexpr double EPS_SMALL = 1e-6; // for ~1s-scale values

// Global test config pointer used by many test fixtures.
AampConfig *gpGlobalConfig{nullptr};

// ---------------------------------------------------------------------------
// TS-packet construction helpers
// ---------------------------------------------------------------------------

/**
 * @brief Encode a 33-bit PTS/DTS value into the 5-byte MPEG-TS wire format.
 *
 * @param pts_val  33-bit timestamp value (encoder ticks at 90 kHz)
 * @param prefix   Prefix nibble in the high 4 bits of byte 0.
 *                 Use 0x30 for PTS when PTS+DTS present,
 *                 0x20 for PTS-only, 0x10 for DTS.
 * @param out      Pointer to at least 5 bytes to write into.
 */
static void EncodePts(uint64_t pts_val, uint8_t prefix, uint8_t* out)
{
	out[0] = prefix | static_cast<uint8_t>((pts_val >> 29) & 0x0E) | 0x01;
	out[1] = static_cast<uint8_t>((pts_val >> 22) & 0xFF);
	out[2] = static_cast<uint8_t>((pts_val >> 14) & 0xFE) | 0x01;
	out[3] = static_cast<uint8_t>((pts_val >>  7) & 0xFF);
	out[4] = static_cast<uint8_t>((pts_val <<  1) & 0xFE) | 0x01;
}

/**
 * @brief Build a minimal 188-byte TS packet carrying PES with PTS and DTS.
 *
 * Layout chosen to be parsed cleanly by processPacket():
 *  - TS header       (bytes 0–3)  : sync=0x47, PAYLOAD_UNIT_START=1,
 *                                   PID=0, payload-only (no adaptation).
 *  - PES header      (bytes 4–12) : start code, stream_id=0xE0 (video),
 *                                   packet length=0 (unbounded), flags
 *                                   requesting both PTS and DTS,
 *                                   header_data_length=10.
 *  - PTS encoding    (bytes 13–17): prefix 0x30.
 *  - DTS encoding    (bytes 18–22): prefix 0x10.
 *  - ES payload      (bytes 23–187): zero-filled.
 *
 * @param pts  33-bit PTS value in 90-kHz ticks.
 * @param dts  33-bit DTS value in 90-kHz ticks.
 */
static std::array<uint8_t, 188> MakeTsPacket(uint64_t pts, uint64_t dts)
{
	std::array<uint8_t, 188> pkt{};

	// TS header
	pkt[0] = 0x47;             // sync byte
	pkt[1] = 0x40;             // PAYLOAD_UNIT_START | PID high byte = 0
	pkt[2] = 0x00;             // PID low byte
	pkt[3] = 0x10;             // payload present, no adaptation field

	// PES header
	pkt[4] = 0x00;             // PES start code byte 0
	pkt[5] = 0x00;             // PES start code byte 1
	pkt[6] = 0x01;             // PES start code byte 2
	pkt[7] = 0xE0;             // stream_id: video (0xE0)
	pkt[8] = 0x00;             // PES_packet_length high (0 = unbounded)
	pkt[9] = 0x00;             // PES_packet_length low
	pkt[10] = 0x80;            // optional header flags 1: marker bits
	pkt[11] = 0xC0;            // optional header flags 2: PTS+DTS present
	pkt[12] = 0x0A;            // PES_header_data_length = 10 (5+5)

	EncodePts(pts, 0x30, &pkt[13]);   // PTS (prefix 0x30 = PTS+DTS context)
	EncodePts(dts, 0x10, &pkt[18]);   // DTS (prefix 0x10)

	// pkt[23..187]: ES data (zero-initialised by array{} above)
	return pkt;
}

// ---------------------------------------------------------------------------
// Test fixture
// ---------------------------------------------------------------------------

class DemuxerTests : public ::testing::Test
{
protected:
	PrivateInstanceAAMP  *mAamp{nullptr};

	void SetUp() override
	{
		if (gpGlobalConfig == nullptr)
		{
			gpGlobalConfig = new AampConfig();
		}
		g_mockAampConfig = new NiceMock<MockAampConfig>();
		g_mockPrivateInstanceAAMP = new NiceMock<MockPrivateInstanceAAMP>();
		mAamp = new PrivateInstanceAAMP(gpGlobalConfig);
	}

	void TearDown() override
	{
		delete mAamp;
		mAamp = nullptr;
		delete g_mockPrivateInstanceAAMP;
		g_mockPrivateInstanceAAMP = nullptr;
		delete g_mockAampConfig;
		g_mockAampConfig = nullptr;
		delete gpGlobalConfig;
		gpGlobalConfig = nullptr;
	}

	/**
	 * @brief Feed one TS packet through the demuxer without a processor
	 *        callback.  Any ES flushed internally goes to the fake's
	 *        SendStreamCopy (no-op).
	 */
	static void ProcessPacket(
		Demuxer& demux,
		const std::array<uint8_t, 188>& pkt)
	{
		bool basePtsUpdated = false;
		bool ptsError       = false;
		bool isPacketIgnored = false;
		demux.processPacket(
			pkt.data(),
			basePtsUpdated, ptsError, isPacketIgnored,
			/*applyOffset=*/false,
			/*processor=*/nullptr);
	}

	/**
	 * @brief Call the public send(processor) interface and return the
	 *        SegmentInfo_t that UpdateSegmentInfo() produced, or
	 *        std::nullopt if CheckForSteadyState() discarded the call.
	 */
	static std::optional<SegmentInfo_t> Capture(Demuxer& demux)
	{
		std::optional<SegmentInfo_t> result;
		demux.send([&result](AampMediaType, SegmentInfo_t info,
		                     std::vector<uint8_t>)
		{
			result = info;
		});
		return result;
	}
};

// ---------------------------------------------------------------------------
// Test cases
// ---------------------------------------------------------------------------

/**
 * @test RestampMode_OutputPtsIsPtsOffsetPlusRawPts
 *
 * In HLS-TS restamp mode, UpdateSegmentInfo() must return
 *   pts_s = ptsOffset + raw_encoder_ticks / 90000
 * instead of the old base_pts-relative computation.
 *
 * This validates the new early-return block added to UpdateSegmentInfo()
 * which fixes the "stale base_pts causes wrong output PTS after SSAI" bug.
 */
TEST_F(DemuxerTests, RestampMode_OutputPtsIsPtsOffsetPlusRawPts)
{
	EXPECT_CALL(*g_mockAampConfig,
	            IsConfigSet(eAAMPConfig_HlsTsEnablePTSReStamp))
		.WillRepeatedly(Return(true));

	Demuxer demux(mAamp, eMEDIATYPE_VIDEO, /*optimizeMuxed=*/true);
	demux.init(0.0, 2.0, false, /*resetBasePTS=*/false, /*optimizeMuxed=*/true);
	demux.setPtsOffset(100.0);

	// PTS = 90000 ticks = 1 second, DTS = 90000 ticks = 1 second
	constexpr uint64_t kPts = 90000ULL;
	constexpr uint64_t kDts = 90000ULL;
	ProcessPacket(demux, MakeTsPacket(kPts, kDts));

	const auto result = Capture(demux);
	ASSERT_TRUE(result.has_value());

	const double kExpectedPts = 100.0 + static_cast<double>(kPts) / 90000.0;
	const double kExpectedDts = 100.0 + static_cast<double>(kDts) / 90000.0;
	EXPECT_NEAR(result->pts_s, kExpectedPts, EPS_SMALL);
	EXPECT_NEAR(result->dts_s, kExpectedDts, EPS_SMALL);
}

/**
 * @test NonRestampMode_PtsOffsetNotAdded
 *
 * In non-restamp mode, ptsOffset must NOT be added to the output timestamps.
 *
 * The code removed the unconditional end-of-UpdateSegmentInfo block:
 *   if (aamp && ISCONFIGSET(eAAMPConfig_HlsTsEnablePTSReStamp)) {
 *       ret.pts_s += ptsOffset;
 *   }
 * This test confirms that block is gone: setting a non-zero ptsOffset while
 * restamp mode is disabled must not contaminate the output.
 */
TEST_F(DemuxerTests, NonRestampMode_PtsOffsetNotAdded)
{
	EXPECT_CALL(*g_mockAampConfig,
	            IsConfigSet(eAAMPConfig_HlsTsEnablePTSReStamp))
		.WillRepeatedly(Return(false));

	Demuxer demux(mAamp, eMEDIATYPE_VIDEO, /*optimizeMuxed=*/true);
	// optimizeMuxed=true sets base_pts=0 and finalized_base_pts=true.
	demux.init(0.0, 2.0, false, false, true);
	// Non-zero offset that should NOT influence output in non-restamp mode.
	demux.setPtsOffset(50.0);

	// PTS = 90000 ticks = 1 second, DTS = 90000 ticks = 1 second
	constexpr uint64_t kPts = 90000ULL;
	constexpr uint64_t kDts = 90000ULL;
	ProcessPacket(demux, MakeTsPacket(kPts, kDts));

	const auto result = Capture(demux);
	ASSERT_TRUE(result.has_value());

	// base_pts = 0, position = 0 → pts_s = (90000 - 0) / 90000 = 1.0
	// ptsOffset (50.0) must NOT be added → expected is 1.0, not 51.0.
	EXPECT_NEAR(result->pts_s, 1.0, EPS_SMALL);
}

/**
 * @test SetPtsOffset_SuppressesRolloverDetectionOnNextPts
 *
 * Key regression test for the SSAI video-freeze bug:
 *
 * At an SSAI boundary (discontinuity) the main-content encoder (the content before the discontinuity)
 * may have been running near the top of the 33-bit PTS range (e.g. 8 billion ticks) while the ad
 * encoder (after the discontinuity) starts fresh near 0 (e.g. 90000 ticks). Without the fix,
 * processPacket() sees prev_pts (≈ 8B) >> current_pts (≈ 90K) and the difference (≈ 8B)
 * exceeds half_max (≈ 4.3B), wrongly setting rollover_pts = true. UpdateSegmentInfo() then
 * adds max_pts_s (≈ 95443 s) to every subsequent output PTS, freezing video.
 *
 * setPtsOffset() must set suppress_rollover_detection = true so that the
 * first PTS comparison after the boundary is skipped, preventing the false
 * rollover detection.
 */
TEST_F(DemuxerTests, SetPtsOffset_SuppressesRolloverDetectionOnNextPts)
{
	EXPECT_CALL(*g_mockAampConfig,
	            IsConfigSet(eAAMPConfig_HlsTsEnablePTSReStamp))
		.WillRepeatedly(Return(true));

	Demuxer demux(mAamp, eMEDIATYPE_VIDEO, /*optimizeMuxed=*/true);
	demux.init(0.0, 2.0, false, false, true);

	// Step 1: simulate main-content encoder (content before the discontinuity) near top of 33-bit range.
	// First packet — sets current_pts = 8B (8 Billion), fills es.
	constexpr uint64_t kHighPts = 8000000000ULL;
	ProcessPacket(demux, MakeTsPacket(kHighPts, kHighPts));

	// Step 2: SSAI boundary (discontinuity) — a new encoder epoch begins.
	// setPtsOffset clears rollover_pts and sets suppress_rollover_detection.
	constexpr double kAdOffset = 100.0;
	demux.setPtsOffset(kAdOffset);

	// Step 3: process first ad packet (after discontinuity) (PTS = 90000 = 1 s).
	// Internally, the demuxer sends the main-content ES (preceding the discontinuity) (via SendStreamCopy)
	// then performs the rollover check: prev_pts = kHighPts (8B) > 90000, diff ≈ 8B >
	// half_max ≈ 4.3B. WITH suppress, rollover_pts stays false; without it
	// the flag would be set, corrupting the output.
	constexpr uint64_t kAdPts = 90000ULL;
	ProcessPacket(demux, MakeTsPacket(kAdPts, kAdPts));

	// Step 4: capture the SegmentInfo for the ad packet (after discontinuity) (current_pts = 90000,
	// rollover_pts = false due to suppression).
	const auto result = Capture(demux);
	ASSERT_TRUE(result.has_value());

	// Expected: ptsOffset + raw_pts / 90000 = 100.0 + 1.0 = 101.0.
	// Incorrect (if rollover were set): 100.0 + 1.0 + 95443.71768889 ≈ 95544.7.
	const double kExpected = kAdOffset + static_cast<double>(kAdPts) / 90000.0;
	EXPECT_NEAR(result->pts_s, kExpected, EPS_SMALL)
		<< "pts_s should be ~" << kExpected << " (no false rollover). "
		<< "A value near 95544 indicates rollover was NOT suppressed.";
	EXPECT_NEAR(result->dts_s, kExpected, EPS_SMALL)
		<< "dts_s should be ~" << kExpected << " (no false rollover). "
		<< "A value near 95544 indicates rollover was NOT suppressed.";

}

/**
 * @test RolloverCorrection_AppliesToRawPtsBelowHalfMax
 *
 * Verifies that genuine 33-bit PTS rollover (the encoder counter actually
 * wraps around) is still corrected in restamp mode. When rollover_pts = true
 * and raw_pts_s < max_pts_s / 2, UpdateSegmentInfo() must add max_pts_s to
 * restore a monotonic timeline.
 *
 * This test does NOT call setPtsOffset() so suppress_rollover_detection
 * remains false and the rollover IS correctly detected.
 */
TEST_F(DemuxerTests, RolloverCorrection_AppliesToRawPtsBelowHalfMax)
{
	EXPECT_CALL(*g_mockAampConfig,
	            IsConfigSet(eAAMPConfig_HlsTsEnablePTSReStamp))
		.WillRepeatedly(Return(true));

	Demuxer demux(mAamp, eMEDIATYPE_VIDEO, /*optimizeMuxed=*/true);
	demux.init(0.0, 2.0, false, false, true);

	// Packet A: PTS near top of 33-bit range.
	constexpr uint64_t kHighPts = 8000000000ULL;
	ProcessPacket(demux, MakeTsPacket(kHighPts, kHighPts));

	// Packet B: PTS wraps to near zero (90000 ticks = 1 s).
	// Processing B sends A internally (no-op via SendStreamCopy) and performs
	// rollover detection: prev_pts = kHighPts (8B) > 90000 AND diff ≈ 8B > half_max
	// → rollover_pts = true. suppress is NOT set (no setPtsOffset call).
	constexpr uint64_t kLowPts = 90000ULL;
	ProcessPacket(demux, MakeTsPacket(kLowPts, kLowPts));

	// Capture: current_pts = 90000, rollover_pts = true.
	// raw_pts_s = 1.0 < max_pts_s/2 ≈ 47721 → correction applied.
	const auto result = Capture(demux);
	ASSERT_TRUE(result.has_value());

	constexpr double kMaxPtsS = 95443.71768889; // 2^33 / 90000
	const double kExpected =
		0.0 + static_cast<double>(kLowPts) / 90000.0 + kMaxPtsS;
	EXPECT_NEAR(result->pts_s, kExpected, EPS_SMALL);
	EXPECT_NEAR(result->dts_s, kExpected, EPS_SMALL);
}

/**
 * @test Init_ResetsSuppressRolloverDetectionFlag
 *
 * init() must clear suppress_rollover_detection so that a flag armed by a
 * previous setPtsOffset() call does not silently persist across the segment
 * boundary that init() represents.
 *
 * The test arms the flag via setPtsOffset(), calls init() to reset it, then
 * triggers a genuine high-to-low PTS transition that SHOULD set rollover_pts.
 * If init() correctly cleared suppress, rollover_pts is set and the output
 * PTS gets the max_pts_s correction; if the flag survived init(), rollover
 * detection is skipped and the output PTS is wrong.
 */
TEST_F(DemuxerTests, Init_ResetsSuppressRolloverDetectionFlag)
{
	EXPECT_CALL(*g_mockAampConfig,
	            IsConfigSet(eAAMPConfig_HlsTsEnablePTSReStamp))
		.WillRepeatedly(Return(true));

	Demuxer demux(mAamp, eMEDIATYPE_VIDEO, /*optimizeMuxed=*/true);

	// Arm suppress_rollover_detection.
	demux.setPtsOffset(10.0);

	// init() must clear the flag; position = 0.0 so ptsOffset-based
	// expected value is 0 + kLowPts/90000 + max_pts_s.
	demux.init(0.0, 2.0, false, false, true);

	// Trigger a rollover exactly as in the RolloverCorrection test above.
	constexpr uint64_t kHighPts = 8000000000ULL;
	constexpr uint64_t kLowPts  = 90000ULL;
	ProcessPacket(demux, MakeTsPacket(kHighPts, kHighPts));
	ProcessPacket(demux, MakeTsPacket(kLowPts, kLowPts));

	const auto result = Capture(demux);
	ASSERT_TRUE(result.has_value());

	// init() does NOT clear ptsOffset; it remains 10.0 from setPtsOffset().
	// If suppress was cleared by init(), rollover IS detected:
	//   pts_s = ptsOffset(10.0) + raw_pts(1.0) + max_pts_s(≈95443.7) ≈ 95454.7
	// If the bug existed (suppress survived init()), rollover NOT detected:
	//   pts_s = ptsOffset(10.0) + raw_pts(1.0) = 11.0
	constexpr double kPtsOffset = 10.0;
	constexpr double kMaxPtsS   = 95443.71768889;
	const double kExpected =
		kPtsOffset + static_cast<double>(kLowPts) / 90000.0 + kMaxPtsS;
	EXPECT_NEAR(result->pts_s, kExpected, EPS_SMALL)
		<< "init() should have reset suppress_rollover_detection. "
		<< "pts_s near 11.0 means the flag survived init() — bug not fixed.";
	EXPECT_NEAR(result->dts_s, kExpected, EPS_SMALL)
		<< "init() should have reset suppress_rollover_detection. "
		<< "pts_s near 11.0 means the flag survived init() — bug not fixed.";
}
