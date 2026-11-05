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
 * @file AampRialtoVideoSourceFragmentTestCases.cpp
 * @brief AampRialtoVideoSource L1 tests: processInitFragment, processDataFragment and injectSingleSample with the demuxer.
 */

#include "AampRialtoVideoSourceTestFixture.h"

// ---------------------------------------------------------------------------
// processInitFragment — success
// ---------------------------------------------------------------------------

/**
 * @test AampRialtoVideoSource_ProcessInitFragment_Success
 * @brief Verify processInitFragment returns codec info when Parse succeeds.
 */
TEST_F(AampRialtoVideoSourceWithDemuxTest,
	AampRialtoVideoSource_ProcessInitFragment_Success)
{
	ON_CALL(*g_mockMp4Demux, Parse(_)).WillByDefault(Return(true));
	ON_CALL(*g_mockMp4Demux, GetCodecInfo())
		.WillByDefault([]() { return MakeH264CodecInfo(1280, 720); });

	auto buf = std::make_shared<std::vector<uint8_t>>(
		std::vector<uint8_t>{0x00, 0x00, 0x00, 0x01});

	auto result = m_source.processInitFragment(std::move(buf));

	ASSERT_TRUE(result.has_value());
	EXPECT_EQ(result->mCodecFormat, GST_FORMAT_VIDEO_ES_H264);
	EXPECT_EQ(result->mInfo.video.mWidth, uint16_t{1280});
	EXPECT_EQ(result->mInfo.video.mHeight, uint16_t{720});
}

// ---------------------------------------------------------------------------
// processInitFragment — parse fails
// ---------------------------------------------------------------------------

/**
 * @test AampRialtoVideoSource_ProcessInitFragment_ParseFails_ReturnsNullopt
 * @brief Verify processInitFragment returns nullopt when the demuxer rejects
 *        the buffer.
 */
TEST_F(AampRialtoVideoSourceWithDemuxTest,
	AampRialtoVideoSource_ProcessInitFragment_ParseFails_ReturnsNullopt)
{
	ON_CALL(*g_mockMp4Demux, Parse(_)).WillByDefault(Return(false));

	auto buf = std::make_shared<std::vector<uint8_t>>(
		std::vector<uint8_t>{0xFF});

	auto result = m_source.processInitFragment(std::move(buf));

	EXPECT_FALSE(result.has_value());
}

// ---------------------------------------------------------------------------
// processInitFragment — no demuxer
// ---------------------------------------------------------------------------

/**
 * @test AampRialtoVideoSource_ProcessInitFragment_NoDemuxer_CreatesDemuxerLazily
 * @brief Verify processInitFragment creates a demuxer lazily when none is
 *        pre-installed, so the caller never needs to call setDemuxer.
 */
TEST_F(AampRialtoVideoSourceTest,
	AampRialtoVideoSource_ProcessInitFragment_NoDemuxer_CreatesDemuxerLazily)
{
	// No demuxer pre-installed (base fixture has no mock demux).
	// g_mockMp4Demux is null, so the fake Mp4Demux returns true from Parse
	// and a default (INVALID-format) MediaCodecInfo from GetCodecInfo.
	ASSERT_FALSE(m_source.hasDemuxer());

	auto buf = std::make_shared<std::vector<uint8_t>>(
		std::vector<uint8_t>{0x00, 0x01});
	m_source.processInitFragment(std::move(buf));

	EXPECT_TRUE(m_source.hasDemuxer());
}

// ---------------------------------------------------------------------------
// processDataFragment — parse fails
// ---------------------------------------------------------------------------

/**
 * @test AampRialtoVideoSource_ProcessDataFragment_NoDemuxerNotAttached_ReturnsTrue
 * @brief Verify processDataFragment returns true and performs no injection
 *        in no-demuxer mode when the source is not attached.
 */
TEST_F(AampRialtoVideoSourceTest,
	AampRialtoVideoSource_ProcessDataFragment_NoDemuxerNotAttached_ReturnsTrue)
{
	ASSERT_FALSE(m_source.hasDemuxer());
	ASSERT_FALSE(m_source.isAttached());

	EXPECT_CALL(*m_pipelinePtr, addSegment(_, _)).Times(0);

	auto buf = std::make_shared<std::vector<uint8_t>>(
		std::vector<uint8_t>{0xAA, 0xBB, 0xCC});

	bool result = m_source.processDataFragment(*m_pipelinePtr, std::move(buf),
		1.25, 1.25, 0.04, 0.0);

	EXPECT_TRUE(result);
	EXPECT_EQ(m_source.firstPtsMs(), AampRialtoMediaSource::kFirstPtsNotSet);
}

/**
 * @test AampRialtoVideoSource_ProcessDataFragment_NoDemuxerAttached_InjectsSingleSample
 * @brief Verify processDataFragment injects one ES sample directly in
 *        no-demuxer mode when attached and a needData slot is available.
 */
TEST_F(AampRialtoVideoSourceTest,
	AampRialtoVideoSource_ProcessDataFragment_NoDemuxerAttached_InjectsSingleSample)
{
	ASSERT_FALSE(m_source.hasDemuxer());
	auto codecInfo = MakeH264CodecInfo();
	m_source.attachOrUpdate(*m_pipelinePtr, codecInfo, nullptr, -1);

	{
		auto &st = m_source.state();
		std::lock_guard<std::mutex> lock(st.mu);
		st.hasPending            = true;
		st.pendingRequestId      = 66;
		st.pendingFrameCount     = 1;
		st.segmentsAddedInBatch  = 0;
		st.injectorActive        = true;
	}

	EXPECT_CALL(*m_pipelinePtr, addSegment(66, _))
		.WillOnce(Return(firebolt::rialto::AddSegmentStatus::OK));

	auto buf = std::make_shared<std::vector<uint8_t>>(
		std::vector<uint8_t>{0x10, 0x20, 0x30, 0x40});

	const double pts = 7.5;
	bool result = m_source.processDataFragment(*m_pipelinePtr, std::move(buf),
		pts, 7.4, 0.033, 0.0);

	EXPECT_TRUE(result);
	EXPECT_EQ(m_source.firstPtsMs(), static_cast<int64_t>(pts * 1000.0));
}

/**
 * @test AampRialtoVideoSource_InjectedSpanMs_NothingAccepted_IsZero
 * @brief Verify injectedSpanMs is zero before Rialto accepts any segment.
 */
TEST_F(AampRialtoVideoSourceTest,
	AampRialtoVideoSource_InjectedSpanMs_NothingAccepted_IsZero)
{
	EXPECT_EQ(m_source.injectedSpanMs(), 0);
}

/**
 * @test AampRialtoVideoSource_InjectedSpanMs_CoversAcceptedSegment
 * @brief Verify injectedSpanMs spans from the first accepted PTS to the end
 *        of the last accepted segment, and resets with unblockInjection().
 */
TEST_F(AampRialtoVideoSourceTest,
	AampRialtoVideoSource_InjectedSpanMs_CoversAcceptedSegment)
{
	auto codecInfo = MakeH264CodecInfo();
	m_source.attachOrUpdate(*m_pipelinePtr, codecInfo, nullptr, -1);
	{
		auto &st = m_source.state();
		std::lock_guard<std::mutex> lock(st.mu);
		st.hasPending        = true;
		st.pendingRequestId  = 67;
		st.pendingFrameCount = 1;
		st.injectorActive    = true;
	}
	ON_CALL(*m_pipelinePtr, addSegment(_, _))
		.WillByDefault(Return(firebolt::rialto::AddSegmentStatus::OK));

	auto buf = std::make_shared<std::vector<uint8_t>>(
		std::vector<uint8_t>{0x10, 0x20});
	m_source.processDataFragment(*m_pipelinePtr, std::move(buf),
		/*fpts=*/7.5, /*fdts=*/7.5, /*fDuration=*/0.040, 0.0);

	EXPECT_NEAR(m_source.injectedSpanMs(), 40, 1);

	m_source.unblockInjection(m_pipelinePtr, "test");
	EXPECT_EQ(m_source.injectedSpanMs(), 0);
}

/**
 * @test AampRialtoVideoSource_AcceptedFrames_CountsOnlyAcceptedSegments
 * @brief Verify acceptedFrames counts segments Rialto accepted, ignores
 *        rejected ones, and resets with unblockInjection().
 */
TEST_F(AampRialtoVideoSourceTest,
	AampRialtoVideoSource_AcceptedFrames_CountsOnlyAcceptedSegments)
{
	EXPECT_EQ(m_source.acceptedFrames(), 0u);

	auto codecInfo = MakeH264CodecInfo();
	m_source.attachOrUpdate(*m_pipelinePtr, codecInfo, nullptr, -1);
	EXPECT_CALL(*m_pipelinePtr, addSegment(_, _))
		.WillOnce(Return(firebolt::rialto::AddSegmentStatus::OK))
		.WillOnce(Return(firebolt::rialto::AddSegmentStatus::OK))
		.WillOnce(Return(firebolt::rialto::AddSegmentStatus::ERROR));

	for (uint32_t i = 0; i < 3; ++i)
	{
		{
			auto &st = m_source.state();
			std::lock_guard<std::mutex> lock(st.mu);
			st.hasPending        = true;
			st.pendingRequestId  = 68 + i;
			st.pendingFrameCount = 1;
			st.injectorActive    = true;
		}
		auto buf = std::make_shared<std::vector<uint8_t>>(
			std::vector<uint8_t>{0x10, 0x20});
		m_source.processDataFragment(*m_pipelinePtr, std::move(buf),
			7.5 + (0.040 * i), 7.5 + (0.040 * i), 0.040, 0.0);
	}

	EXPECT_EQ(m_source.acceptedFrames(), 2u);

	m_source.unblockInjection(m_pipelinePtr, "test");
	EXPECT_EQ(m_source.acceptedFrames(), 0u);
}

/**
 * @test AampRialtoVideoSource_BufferFull_SetByNoSpace_ClearedByUnblock
 * @brief Verify bufferFull latches when addSegment returns NO_SPACE and
 *        resets with unblockInjection().
 */
TEST_F(AampRialtoVideoSourceTest,
	AampRialtoVideoSource_BufferFull_SetByNoSpace_ClearedByUnblock)
{
	EXPECT_FALSE(m_source.bufferFull());

	auto codecInfo = MakeH264CodecInfo();
	m_source.attachOrUpdate(*m_pipelinePtr, codecInfo, nullptr, -1);
	uint64_t gen = m_source.captureGeneration();
	{
		auto &st = m_source.state();
		std::lock_guard<std::mutex> lock(st.mu);
		st.hasPending        = true;
		st.pendingRequestId  = 82;
		st.pendingFrameCount = 1;
		st.injectorActive    = true;
	}
	ON_CALL(*m_pipelinePtr, haveData(_, _)).WillByDefault(Return(true));
	EXPECT_CALL(*m_pipelinePtr, addSegment(82, _))
		.WillOnce(Return(firebolt::rialto::AddSegmentStatus::NO_SPACE));

	AampMediaSample sample;
	uint8_t data[] = {0x10, 0x11};
	sample.mData = std::shared_ptr<const uint8_t>(data, [](const uint8_t *){});
	sample.mDataSize = 2;
	sample.mPts = 4.0;
	sample.mDuration = 0.033;

	// NO_SPACE leaves the injector waiting for the next needData.
	std::thread injector([&] {
		m_source.injectOneSample(*m_pipelinePtr, gen, std::move(sample), nullptr);
	});
	const auto deadline = std::chrono::steady_clock::now() +
		std::chrono::milliseconds(200);
	while (!m_source.bufferFull() &&
		std::chrono::steady_clock::now() < deadline)
	{
		std::this_thread::sleep_for(std::chrono::milliseconds(1));
	}
	EXPECT_TRUE(m_source.bufferFull());

	m_source.unblockInjection(m_pipelinePtr, "test");
	injector.join();

	EXPECT_FALSE(m_source.bufferFull());
}

/**
 * @test AampRialtoVideoSource_ProcessDataFragment_ParseFails_ReturnsFalse
 * @brief Verify processDataFragment returns false when Parse fails.
 */
TEST_F(AampRialtoVideoSourceWithDemuxTest,
	AampRialtoVideoSource_ProcessDataFragment_ParseFails_ReturnsFalse)
{
	ON_CALL(*g_mockMp4Demux, Parse(_)).WillByDefault(Return(false));

	auto buf = std::make_shared<std::vector<uint8_t>>(
		std::vector<uint8_t>{0xFF});

	bool result = m_source.processDataFragment(*m_pipelinePtr, std::move(buf),
		0.0, 0.0, 0.0, 0.0);

	EXPECT_FALSE(result);
}

// ---------------------------------------------------------------------------
// processDataFragment — empty sample list
// ---------------------------------------------------------------------------

/**
 * @test AampRialtoVideoSource_ProcessDataFragment_EmptySamples_ReturnsTrue
 * @brief Verify processDataFragment returns true and injects nothing when
 *        the demuxer produces an empty sample list.
 */
TEST_F(AampRialtoVideoSourceWithDemuxTest,
	AampRialtoVideoSource_ProcessDataFragment_EmptySamples_ReturnsTrue)
{
	ON_CALL(*g_mockMp4Demux, Parse(_)).WillByDefault(Return(true));
	ON_CALL(*g_mockMp4Demux, GetSamples())
		.WillByDefault([]() { return std::vector<AampMediaSample>{}; });

	EXPECT_CALL(*m_pipelinePtr, addSegment(_, _)).Times(0);

	auto buf = std::make_shared<std::vector<uint8_t>>(
		std::vector<uint8_t>{0x01, 0x02});

	bool result = m_source.processDataFragment(*m_pipelinePtr, std::move(buf),
		0.0, 0.0, 0.0, 0.0);

	EXPECT_TRUE(result);
}

// ---------------------------------------------------------------------------
// processDataFragment — success (one sample injected)
// ---------------------------------------------------------------------------

/**
 * @test AampRialtoVideoSource_ProcessDataFragment_Success_InjectsSample
 * @brief Verify processDataFragment parses a data buffer and injects the
 *        contained sample into the pipeline.
 */
TEST_F(AampRialtoVideoSourceWithDemuxTest,
	AampRialtoVideoSource_ProcessDataFragment_Success_InjectsSample)
{
	auto codecInfo = MakeH264CodecInfo();
	m_source.attachOrUpdate(*m_pipelinePtr, codecInfo, nullptr, -1);

	// Provide one sample from the demuxer.
	ON_CALL(*g_mockMp4Demux, Parse(_)).WillByDefault(Return(true));
	ON_CALL(*g_mockMp4Demux, GetSamples())
		.WillByDefault([]() {
			AampMediaSample s;
			static uint8_t rawData[] = {0x00, 0x01, 0x02};
			s.mData = std::shared_ptr<const uint8_t>(
				rawData, [](const uint8_t *){});
			s.mDataSize = 3;
			s.mPts      = 1.0;
			s.mDuration = 0.033;
			std::vector<AampMediaSample> v;
			v.push_back(std::move(s));
			return v;
		});

	// Set up a pending needData slot so injectOneSample can proceed.
	{
		auto &st = m_source.state();
		std::lock_guard<std::mutex> lock(st.mu);
		st.hasPending            = true;
		st.pendingRequestId      = 55;
		st.pendingFrameCount     = 10;
		st.segmentsAddedInBatch  = 0;
		st.injectorActive        = true;
	}

	EXPECT_CALL(*m_pipelinePtr, addSegment(_, _))
		.WillOnce(Return(firebolt::rialto::AddSegmentStatus::OK));

	auto buf = std::make_shared<std::vector<uint8_t>>(
		std::vector<uint8_t>{0xAB, 0xCD});

	bool result = m_source.processDataFragment(*m_pipelinePtr, std::move(buf),
		0.0, 0.0, 0.0, 0.0);

	EXPECT_TRUE(result);
}

// ---------------------------------------------------------------------------
// injectSingleSample — source not attached
// ---------------------------------------------------------------------------

/**
 * @test AampRialtoVideoSource_InjectSingleSample_NotAttached_ReturnsFalse
 * @brief Verify injectSingleSample returns false when the source is not
 *        attached (waitForAttach returns false immediately).
 */
TEST_F(AampRialtoVideoSourceTest,
	AampRialtoVideoSource_InjectSingleSample_NotAttached_ReturnsFalse)
{
	// Source is unattached by default; attachPending is false.
	AampMediaSample sample;
	uint8_t data[] = {0xDE, 0xAD};
	sample.mData = std::shared_ptr<const uint8_t>(data, [](const uint8_t *){});
	sample.mDataSize = 2;
	sample.mPts      = 0.0;
	sample.mDuration = 0.033;

	bool result = m_source.injectSingleSample(
		*m_pipelinePtr, std::move(sample));

	EXPECT_FALSE(result);
}

// ---------------------------------------------------------------------------
// injectSingleSample — success
// ---------------------------------------------------------------------------

/**
 * @test AampRialtoVideoSource_InjectSingleSample_Success
 * @brief Verify injectSingleSample delivers a sample to the pipeline when
 *        the source is attached and a needData slot is available.
 */
TEST_F(AampRialtoVideoSourceTest,
	AampRialtoVideoSource_InjectSingleSample_Success)
{
	auto codecInfo = MakeH264CodecInfo();
	m_source.attachOrUpdate(*m_pipelinePtr, codecInfo, nullptr, -1);

	// Provide a pending needData slot.
	{
		auto &st = m_source.state();
		std::lock_guard<std::mutex> lock(st.mu);
		st.hasPending            = true;
		st.pendingRequestId      = 99;
		st.pendingFrameCount     = 10;
		st.segmentsAddedInBatch  = 0;
		st.injectorActive        = true;
	}

	EXPECT_CALL(*m_pipelinePtr, addSegment(_, _))
		.WillOnce(Return(firebolt::rialto::AddSegmentStatus::OK));

	AampMediaSample sample;
	uint8_t data[] = {0xBE, 0xEF};
	sample.mData = std::shared_ptr<const uint8_t>(data, [](const uint8_t *){});
	sample.mDataSize = 2;
	sample.mPts      = 5.0;
	sample.mDuration = 0.033;

	bool result = m_source.injectSingleSample(
		*m_pipelinePtr, std::move(sample));

	EXPECT_TRUE(result);
}
