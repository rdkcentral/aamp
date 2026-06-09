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
 * @test AampRialtoSubtitleSource_InvalidateGeneration_BumpsGeneration
 * @brief Verify invalidateGeneration works on the skeleton subclass.
 */
TEST_F(AampRialtoSubtitleSourceTest, AampRialtoSubtitleSource_InvalidateGeneration_BumpsGeneration)
{
	uint64_t gen1 = m_source.captureGeneration();
	m_source.invalidateGeneration();
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
		GST_FORMAT_SUBTITLE_TTML, mimeType, fmt);
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
		GST_FORMAT_SUBTITLE_WEBVTT, mimeType, fmt);
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
		GST_FORMAT_SUBTITLE_MP4, mimeType, fmt);
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
		GST_FORMAT_INVALID, mimeType, fmt);
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
	const bool ok = m_source.mapCodecToMime(GST_FORMAT_UNKNOWN, mimeType, fmt);

	EXPECT_TRUE(ok);
	EXPECT_TRUE(m_source.isInbandCC());
	EXPECT_EQ(mimeType, "text/cc");
	EXPECT_EQ(fmt, firebolt::rialto::StreamFormat::RAW);
}

/**
 * @test AampRialtoSubtitleSource_HandleNeedData_InbandCC_RespondsWithNoAvailableSamples
 * @brief handleNeedData() must immediately call
 *        haveData(NO_AVAILABLE_SAMPLES, requestId) for inband CC sources
 *        and must NOT set hasPending, because AAMP has no data to push —
 *        the Rialto server extracts CC from the video bitstream internally.
 */
TEST_F(AampRialtoSubtitleSourceTest,
	AampRialtoSubtitleSource_HandleNeedData_InbandCC_RespondsWithNoAvailableSamples)
{
	// Trigger inband-CC mode by passing GST_FORMAT_UNKNOWN to mapCodecToMime.
	std::string mime;
	firebolt::rialto::StreamFormat fmt{};
	m_source.mapCodecToMime(GST_FORMAT_UNKNOWN, mime, fmt);

	EXPECT_CALL(*m_pipelinePtr,
		haveData(firebolt::rialto::MediaSourceStatus::NO_AVAILABLE_SAMPLES,
			static_cast<uint32_t>(42)))
		.WillOnce(Return(true));

	m_source.handleNeedData(/*frameCount=*/1, /*requestId=*/42, m_pipelinePtr);

	// hasPending must NOT be set — no injection should ever be attempted.
	auto &st = m_source.state();
	std::lock_guard<std::mutex> lock(st.mu);
	EXPECT_FALSE(st.hasPending);
}
