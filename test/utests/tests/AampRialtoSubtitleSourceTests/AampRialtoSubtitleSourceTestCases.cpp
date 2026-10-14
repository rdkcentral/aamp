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
 * @file AampRialtoSubtitleSourceTestCases.cpp
 * @brief L1 unit tests for AampRialtoSubtitleSource.
 */

#include <gtest/gtest.h>
#include <gmock/gmock.h>

#include "AampRialtoSubtitleSource.h"
#include "MockIMediaPipeline.h"

using ::testing::_;
using ::testing::NiceMock;
using ::testing::Return;

// ---------------------------------------------------------------------------
// Testable subclass — exposes protected methods for white-box unit testing
// ---------------------------------------------------------------------------

class TestableAampRialtoSubtitleSource : public AampRialtoSubtitleSource
{
public:
	using AampRialtoSubtitleSource::mapCodecToMime;
	using AampRialtoSubtitleSource::createSegment;
	using AampRialtoSubtitleSource::updateCachedMetadata;
	using AampRialtoMediaSource::handleNeedData;
};

// ---------------------------------------------------------------------------
// Fixture
// ---------------------------------------------------------------------------

/**
 * @class AampRialtoSubtitleSourceTest
 * @brief Fixture for AampRialtoSubtitleSource unit tests.
 */
class AampRialtoSubtitleSourceTest : public ::testing::Test
{
protected:
	void SetUp() override
	{
		m_mockPipeline = std::make_unique<NiceMock<MockIMediaPipeline>>();
		m_pipelinePtr  = m_mockPipeline.get();
	}

	TestableAampRialtoSubtitleSource m_source;
	std::unique_ptr<MockIMediaPipeline> m_mockPipeline;
	MockIMediaPipeline *m_pipelinePtr{nullptr};
};

// ---------------------------------------------------------------------------
// mediaType
// ---------------------------------------------------------------------------

/**
 * @test AampRialtoSubtitleSource_mediaType_ReturnsSubtitle
 * @brief Verify mediaType() returns eMEDIATYPE_SUBTITLE.
 */
TEST_F(AampRialtoSubtitleSourceTest, AampRialtoSubtitleSource_mediaType_ReturnsSubtitle)
{
	EXPECT_EQ(m_source.mediaType(), eMEDIATYPE_SUBTITLE);
}

// ---------------------------------------------------------------------------
// Initial state
// ---------------------------------------------------------------------------

/**
 * @test AampRialtoSubtitleSource_InitialState_NotAttached
 * @brief Verify source starts unattached.
 */
TEST_F(AampRialtoSubtitleSourceTest, AampRialtoSubtitleSource_InitialState_NotAttached)
{
	EXPECT_FALSE(m_source.isAttached());
	EXPECT_EQ(m_source.sourceId(), -1);
}

// ---------------------------------------------------------------------------
// attachOrUpdate — not yet supported
// ---------------------------------------------------------------------------

/**
 * @test AampRialtoSubtitleSource_Attach_ReturnsFailed
 * @brief Verify attachment fails since subtitle codecs are not mapped yet.
 */
TEST_F(AampRialtoSubtitleSourceTest, AampRialtoSubtitleSource_Attach_ReturnsFailed)
{
	MediaCodecInfo ci{};
	ci.mCodecFormat = GST_FORMAT_INVALID;

	EXPECT_CALL(*m_pipelinePtr, attachSource(_)).Times(0);

	auto result = m_source.attachOrUpdate(
		*m_pipelinePtr, ci, nullptr, -1);

	EXPECT_EQ(result, AampRialtoMediaSource::AttachResult::FAILED);
	EXPECT_FALSE(m_source.isAttached());
}

// ---------------------------------------------------------------------------
// Base class operations still work
// ---------------------------------------------------------------------------

/**
 * @test AampRialtoSubtitleSource_Reset_WorksOnSkeleton
 * @brief Verify reset works even on the skeleton subclass.
 */
TEST_F(AampRialtoSubtitleSourceTest, AampRialtoSubtitleSource_Reset_WorksOnSkeleton)
{
	m_source.reset();
	EXPECT_FALSE(m_source.isAttached());
}

/**
 * @test AampRialtoSubtitleSource_UnblockInjection_BumpsGeneration
 * @brief Verify unblockInjection works on the skeleton subclass.
 */
TEST_F(AampRialtoSubtitleSourceTest, AampRialtoSubtitleSource_UnblockInjection_BumpsGeneration)
{
	uint64_t gen1 = m_source.captureGeneration();
	m_source.unblockInjection(nullptr);
	uint64_t gen2 = m_source.captureGeneration();

	EXPECT_GT(gen2, gen1);
}

/**
 * @test AampRialtoSubtitleSource_SignalEos_SetsFlag
 * @brief Verify EOS signaling works on the skeleton subclass.
 */
TEST_F(AampRialtoSubtitleSourceTest, AampRialtoSubtitleSource_SignalEos_SetsFlag)
{
	m_source.signalEos(nullptr);

	auto &st = m_source.state();
	std::lock_guard<std::mutex> lock(st.mu);
	EXPECT_TRUE(st.eos);
}

/**
 * @test AampRialtoSubtitleSource_Demuxer_SetAndGet
 * @brief Verify demuxer can be set and queried.
 */
TEST_F(AampRialtoSubtitleSourceTest, AampRialtoSubtitleSource_Demuxer_SetAndGet)
{
	EXPECT_FALSE(m_source.hasDemuxer());
	EXPECT_EQ(m_source.demuxer(), nullptr);
}

// ---------------------------------------------------------------------------
// mapCodecToMime
// ---------------------------------------------------------------------------

/**
 * @test AampRialtoSubtitleSource_MapCodecToMime_TTML_ReturnsTtmlMime
 * @brief Verify GST_FORMAT_SUBTITLE_TTML maps to "text/ttml".
 */
TEST_F(AampRialtoSubtitleSourceTest,
	AampRialtoSubtitleSource_MapCodecToMime_TTML_ReturnsTtmlMime)
{
	std::string mimeType;
	firebolt::rialto::StreamFormat fmt{};
	const bool ok = m_source.mapCodecToMime(
		GST_FORMAT_SUBTITLE_TTML, false, mimeType, fmt);
	EXPECT_TRUE(ok);
	EXPECT_EQ(mimeType, "text/ttml");
}

/**
 * @test AampRialtoSubtitleSource_MapCodecToMime_WebVTT_ReturnsVttMime
 * @brief Verify GST_FORMAT_SUBTITLE_WEBVTT maps to "text/vtt".
 */
TEST_F(AampRialtoSubtitleSourceTest,
	AampRialtoSubtitleSource_MapCodecToMime_WebVTT_ReturnsVttMime)
{
	std::string mimeType;
	firebolt::rialto::StreamFormat fmt{};
	const bool ok = m_source.mapCodecToMime(
		GST_FORMAT_SUBTITLE_WEBVTT, false, mimeType, fmt);
	EXPECT_TRUE(ok);
	EXPECT_EQ(mimeType, "text/vtt");
}

/**
 * @test AampRialtoSubtitleSource_MapCodecToMime_MP4_ReturnsTtmlMime
 * @brief Verify GST_FORMAT_SUBTITLE_MP4 (stpp/wvtt) maps to "text/ttml".
 */
TEST_F(AampRialtoSubtitleSourceTest,
	AampRialtoSubtitleSource_MapCodecToMime_MP4_ReturnsTtmlMime)
{
	std::string mimeType;
	firebolt::rialto::StreamFormat fmt{};
	const bool ok = m_source.mapCodecToMime(
		GST_FORMAT_SUBTITLE_MP4, false, mimeType, fmt);
	EXPECT_TRUE(ok);
	EXPECT_EQ(mimeType, "text/ttml");
}

/**
 * @test AampRialtoSubtitleSource_MapCodecToMime_UnknownFormat_ReturnsFalse
 * @brief When both codec format and subtitleFormat are unrecognised,
 *        mapCodecToMime returns false.
 */
TEST_F(AampRialtoSubtitleSourceTest,
	AampRialtoSubtitleSource_MapCodecToMime_UnknownFormat_ReturnsFalse)
{
	std::string mimeType;
	firebolt::rialto::StreamFormat fmt{};
	const bool ok = m_source.mapCodecToMime(
		GST_FORMAT_INVALID, false, mimeType, fmt);
	EXPECT_FALSE(ok);
}

// ---------------------------------------------------------------------------
// createSegment
// ---------------------------------------------------------------------------

/**
 * @test AampRialtoSubtitleSource_CreateSegment_CorrectPtsAndDuration
 * @brief Verify createSegment converts pts/duration to nanoseconds and
 *        returns a MediaSegment with SUBTITLE type.
 */
TEST_F(AampRialtoSubtitleSourceTest,
	AampRialtoSubtitleSource_CreateSegment_CorrectPtsAndDuration)
{
	AampMediaSample sample{};
	sample.mPts      = 1.5;
	sample.mDuration = 0.5;

	auto seg = m_source.createSegment(sample);

	ASSERT_NE(seg, nullptr);
	EXPECT_EQ(seg->getType(), firebolt::rialto::MediaSourceType::SUBTITLE);
	EXPECT_EQ(seg->getTimeStamp(),
		static_cast<int64_t>(1.5 * 1'000'000'000LL));
	EXPECT_EQ(seg->getDuration(),
		static_cast<int64_t>(0.5 * 1'000'000'000LL));
}

/**
 * @test AampRialtoSubtitleSource_CreateSegment_NegativeDisplayOffset_IsApplied
 * @brief Regression (L3): processDataFragment() deliberately passes a
 *        negative display offset for WebVTT, but createSegment() only applied
 *        offsets > 0.  Rialto's API takes a uint64; the server stores it in
 *        GST_BUFFER_OFFSET and the text-track sink reads it back as signed, so
 *        a negative value must be passed as its two's-complement form.
 */
TEST_F(AampRialtoSubtitleSourceTest,
	AampRialtoSubtitleSource_CreateSegment_NegativeDisplayOffset_IsApplied)
{
	AampMediaSample sample{};
	sample.mPts             = 1.5;
	sample.mDuration        = 0.5;
	sample.mDisplayOffsetMs = -2000;

	auto seg = m_source.createSegment(sample);

	ASSERT_NE(seg, nullptr);
	ASSERT_TRUE(seg->getDisplayOffset().has_value());
	EXPECT_EQ(static_cast<int64_t>(*seg->getDisplayOffset()), -2000);
}

/**
 * @test AampRialtoSubtitleSource_CreateSegment_PositiveDisplayOffset_IsApplied
 * @brief A positive display offset is still passed through unchanged.
 */
TEST_F(AampRialtoSubtitleSourceTest,
	AampRialtoSubtitleSource_CreateSegment_PositiveDisplayOffset_IsApplied)
{
	AampMediaSample sample{};
	sample.mDisplayOffsetMs = 1500;

	auto seg = m_source.createSegment(sample);

	ASSERT_NE(seg, nullptr);
	ASSERT_TRUE(seg->getDisplayOffset().has_value());
	EXPECT_EQ(*seg->getDisplayOffset(), 1500u);
}

/**
 * @test AampRialtoSubtitleSource_CreateSegment_ZeroDisplayOffset_IsNotSet
 * @brief No offset is set on the segment when there is nothing to correct.
 */
TEST_F(AampRialtoSubtitleSourceTest,
	AampRialtoSubtitleSource_CreateSegment_ZeroDisplayOffset_IsNotSet)
{
	AampMediaSample sample{};
	sample.mDisplayOffsetMs = 0;

	auto seg = m_source.createSegment(sample);

	ASSERT_NE(seg, nullptr);
	EXPECT_FALSE(seg->getDisplayOffset().has_value());
}

/**
 * @test AampRialtoSubtitleSource_CreateSegment_PtsNotExactInBinary_IsRounded
 * @brief 1.001 s and 1.003 s are not exact doubles; truncating
 *        seconds * 1e9 gave 1000999999 ns and 1002999999 ns.
 */
TEST_F(AampRialtoSubtitleSourceTest,
	AampRialtoSubtitleSource_CreateSegment_PtsNotExactInBinary_IsRounded)
{
	AampMediaSample sample{};
	sample.mPts      = 1.001;
	sample.mDuration = 1.003;

	auto seg = m_source.createSegment(sample);

	ASSERT_NE(seg, nullptr);
	EXPECT_EQ(seg->getTimeStamp(), 1'001'000'000LL);
	EXPECT_EQ(seg->getDuration(), 1'003'000'000LL);
}

/**
 * @test AampRialtoSubtitleSource_CreateSegment_TtmlBeginTime_IsNotTruncated
 * @brief The TTML clock value 00:00:00.251 was parsed through a float, which
 *        gives 250.99998 ms and truncated to 250.  The first begin= minus the
 *        container PTS is the display offset applied to the segment.
 */
TEST_F(AampRialtoSubtitleSourceTest,
	AampRialtoSubtitleSource_CreateSegment_TtmlBeginTime_IsNotTruncated)
{
	MediaCodecInfo ci{};
	ci.mCodecFormat = GST_FORMAT_SUBTITLE_TTML;
	m_source.updateCachedMetadata(ci);

	const std::string ttml =
		"<tt><body><div><p begin=\"00:00:00.251\" end=\"00:00:02.000\"/></div></body></tt>";
	auto buffer = std::make_shared<std::vector<uint8_t>>(ttml.begin(), ttml.end());

	AampMediaSample sample{};
	sample.mData     = std::shared_ptr<const uint8_t>(buffer, buffer->data());
	sample.mDataSize = buffer->size();
	sample.mPts      = 0.0;
	sample.mDuration = 0.002;

	auto seg = m_source.createSegment(sample);

	ASSERT_NE(seg, nullptr);
	ASSERT_TRUE(seg->getDisplayOffset().has_value());
	EXPECT_EQ(*seg->getDisplayOffset(), 251u);
}

// ---------------------------------------------------------------------------
// Inband CC
// ---------------------------------------------------------------------------

/**
 * @test AampRialtoSubtitleSource_MapCodecToMime_InbandCC_ReturnsCCMime
 * @brief When GST_FORMAT_UNKNOWN is passed to mapCodecToMime(), the source
 *        enters inband-CC mode and must return "text/cc" with RAW format.
 *        The Rialto server uses this MIME type for sources whose
 *        closed-caption data is embedded in the video bitstream.
 */
TEST_F(AampRialtoSubtitleSourceTest,
	AampRialtoSubtitleSource_MapCodecToMime_InbandCC_ReturnsCCMime)
{
	std::string mimeType;
	firebolt::rialto::StreamFormat fmt{};
	const bool ok = m_source.mapCodecToMime(GST_FORMAT_UNKNOWN, false, mimeType, fmt);

	EXPECT_TRUE(ok);
	EXPECT_TRUE(m_source.isInbandCC());
	EXPECT_EQ(mimeType, "text/cc");
	EXPECT_EQ(fmt, firebolt::rialto::StreamFormat::RAW);
}

// ---------------------------------------------------------------------------
// handleNeedData logging
// ---------------------------------------------------------------------------

static size_t CountNeedDataInfoLines(const std::string &log)
{
	size_t count = 0;
	size_t pos = 0;
	while ((pos = log.find("needData #", pos)) != std::string::npos)
	{
		const size_t lineStart = log.rfind('\n', pos);
		const size_t from = (lineStart == std::string::npos) ? 0 : lineStart;
		const std::string prefix = log.substr(from, pos - from);
		if (prefix.find("[INFO]") != std::string::npos)
		{
			++count;
		}
		pos += 1;
	}
	return count;
}

/**
 * @test AampRialtoSubtitleSource_HandleNeedData_LogsFirstAndPeriodicSummaryAtInfo
 * @brief needData arrives many times a second, so only the first request and
 *        every kNeedDataLogInterval-th one are logged at INFO (with the
 *        accepted-frame and injected-span counters); the others use TRACE.
 */
TEST_F(AampRialtoSubtitleSourceTest,
	AampRialtoSubtitleSource_HandleNeedData_LogsFirstAndPeriodicSummaryAtInfo)
{
	constexpr uint32_t kRequests =
		2 * AampRialtoMediaSource::kNeedDataLogInterval + 5;

	testing::internal::CaptureStdout();
	for (uint32_t id = 1; id <= kRequests; ++id)
	{
		m_source.handleNeedData(1, id, m_pipelinePtr);
	}
	const std::string log = testing::internal::GetCapturedStdout();

	// Requests #1, #100 and #200.
	EXPECT_EQ(CountNeedDataInfoLines(log), 3u);
	EXPECT_NE(log.find("needData #100 "), std::string::npos);
	EXPECT_NE(log.find("acceptedFrames="), std::string::npos);
}

/**
 * @test AampRialtoSubtitleSource_HandleNeedData_AfterReset_LogsFirstRequestAgain
 * @brief A new session or flush starts counting again, so the first request
 *        after it is visible at INFO.
 */
TEST_F(AampRialtoSubtitleSourceTest,
	AampRialtoSubtitleSource_HandleNeedData_AfterReset_LogsFirstRequestAgain)
{
	m_source.handleNeedData(1, 1, m_pipelinePtr);
	m_source.handleNeedData(1, 2, m_pipelinePtr);
	m_source.reset();

	testing::internal::CaptureStdout();
	m_source.handleNeedData(1, 3, m_pipelinePtr);
	const std::string log = testing::internal::GetCapturedStdout();

	EXPECT_EQ(CountNeedDataInfoLines(log), 1u);
	EXPECT_NE(log.find("needData #1 "), std::string::npos) << log;
}
