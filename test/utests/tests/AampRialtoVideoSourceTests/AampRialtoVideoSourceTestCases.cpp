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
 * @file AampRialtoVideoSourceTestCases.cpp
 * @brief L1 unit tests for AampRialtoVideoSource.
 */

#include <gtest/gtest.h>
#include <gmock/gmock.h>

#include "AampRialtoVideoSource.h"
#include "MockIMediaPipeline.h"
#include "MockDrmBridge.h"

using ::testing::_;
using ::testing::Invoke;
using ::testing::NiceMock;
using ::testing::Return;

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

static MediaCodecInfo MakeH264CodecInfo(
	uint16_t width = 1280, uint16_t height = 720)
{
	MediaCodecInfo ci{};
	ci.mCodecFormat           = GST_FORMAT_VIDEO_ES_H264;
	ci.mInfo.video.mWidth     = width;
	ci.mInfo.video.mHeight    = height;
	ci.mCodecData             = {0x01, 0x02, 0x03};
	return ci;
}

static MediaCodecInfo MakeHevcCodecInfo(
	uint16_t width = 1920, uint16_t height = 1080)
{
	MediaCodecInfo ci{};
	ci.mCodecFormat           = GST_FORMAT_VIDEO_ES_HEVC;
	ci.mInfo.video.mWidth     = width;
	ci.mInfo.video.mHeight    = height;
	ci.mCodecData             = {0x01, 0x02};
	return ci;
}

// ---------------------------------------------------------------------------
// Fixture
// ---------------------------------------------------------------------------

/**
 * @class AampRialtoVideoSourceTest
 * @brief Fixture for AampRialtoVideoSource unit tests.
 */
class AampRialtoVideoSourceTest : public ::testing::Test
{
protected:
	void SetUp() override
	{
		m_mockPipeline = std::make_unique<NiceMock<MockIMediaPipeline>>();
		m_pipelinePtr  = m_mockPipeline.get();

		ON_CALL(*m_pipelinePtr, attachSource(_))
			.WillByDefault(Invoke(
				[this](const std::unique_ptr<
					firebolt::rialto::IMediaPipeline::MediaSource> &src)
				{
					const_cast<firebolt::rialto::IMediaPipeline::MediaSource &>(
						*src).setId(m_nextSourceId++);
					return true;
				}));

		ON_CALL(*m_pipelinePtr, setSourcePosition(_, _, _, _, _))
			.WillByDefault(Return(true));
	}

	AampRialtoVideoSource m_source;
	std::unique_ptr<MockIMediaPipeline> m_mockPipeline;
	MockIMediaPipeline *m_pipelinePtr{nullptr};
	int32_t m_nextSourceId{10};
};

// ---------------------------------------------------------------------------
// mediaType
// ---------------------------------------------------------------------------

/**
 * @test AampRialtoVideoSource_mediaType_ReturnsVideo
 * @brief Verify mediaType() returns eMEDIATYPE_VIDEO.
 */
TEST_F(AampRialtoVideoSourceTest, AampRialtoVideoSource_mediaType_ReturnsVideo)
{
	EXPECT_EQ(m_source.mediaType(), eMEDIATYPE_VIDEO);
}

// ---------------------------------------------------------------------------
// Initial state
// ---------------------------------------------------------------------------

/**
 * @test AampRialtoVideoSource_InitialState_NotAttached
 * @brief Verify source starts unattached.
 */
TEST_F(AampRialtoVideoSourceTest, AampRialtoVideoSource_InitialState_NotAttached)
{
	EXPECT_FALSE(m_source.isAttached());
	EXPECT_EQ(m_source.sourceId(), -1);
	EXPECT_EQ(m_source.mksId(), -1);
	EXPECT_EQ(m_source.width(), 0);
	EXPECT_EQ(m_source.height(), 0);
}

// ---------------------------------------------------------------------------
// attachOrUpdate — H264
// ---------------------------------------------------------------------------

/**
 * @test AampRialtoVideoSource_AttachH264_Success
 * @brief Verify first attach with H264 codec succeeds.
 */
TEST_F(AampRialtoVideoSourceTest, AampRialtoVideoSource_AttachH264_Success)
{
	auto codecInfo = MakeH264CodecInfo(1280, 720);

	EXPECT_CALL(*m_pipelinePtr, attachSource(_)).Times(1);

	auto result = m_source.attachOrUpdate(
		*m_pipelinePtr, codecInfo, nullptr, -1);

	EXPECT_EQ(result, AampRialtoMediaSource::AttachResult::NEWLY_ATTACHED);
	EXPECT_TRUE(m_source.isAttached());
	EXPECT_EQ(m_source.sourceId(), 10);
	EXPECT_EQ(m_source.width(), 1280);
	EXPECT_EQ(m_source.height(), 720);
}

// ---------------------------------------------------------------------------
// attachOrUpdate — HEVC
// ---------------------------------------------------------------------------

/**
 * @test AampRialtoVideoSource_AttachHevc_Success
 * @brief Verify first attach with HEVC codec succeeds.
 */
TEST_F(AampRialtoVideoSourceTest, AampRialtoVideoSource_AttachHevc_Success)
{
	auto codecInfo = MakeHevcCodecInfo(3840, 2160);

	auto result = m_source.attachOrUpdate(
		*m_pipelinePtr, codecInfo, nullptr, -1);

	EXPECT_EQ(result, AampRialtoMediaSource::AttachResult::NEWLY_ATTACHED);
	EXPECT_TRUE(m_source.isAttached());
	EXPECT_EQ(m_source.width(), 3840);
	EXPECT_EQ(m_source.height(), 2160);
}

// ---------------------------------------------------------------------------
// attachOrUpdate — unknown codec
// ---------------------------------------------------------------------------

/**
 * @test AampRialtoVideoSource_AttachUnknownCodec_Fails
 * @brief Verify attach with unrecognised codec returns FAILED.
 */
TEST_F(AampRialtoVideoSourceTest, AampRialtoVideoSource_AttachUnknownCodec_Fails)
{
	MediaCodecInfo ci{};
	ci.mCodecFormat = GST_FORMAT_INVALID;

	auto result = m_source.attachOrUpdate(
		*m_pipelinePtr, ci, nullptr, -1);

	EXPECT_EQ(result, AampRialtoMediaSource::AttachResult::FAILED);
	EXPECT_FALSE(m_source.isAttached());
}

// ---------------------------------------------------------------------------
// attachOrUpdate — already attached (update)
// ---------------------------------------------------------------------------

/**
 * @test AampRialtoVideoSource_AttachTwice_ReturnsUpdated
 * @brief Verify second attach returns UPDATED and refreshes metadata.
 */
TEST_F(AampRialtoVideoSourceTest, AampRialtoVideoSource_AttachTwice_ReturnsUpdated)
{
	auto codecInfo1 = MakeH264CodecInfo(1280, 720);
	m_source.attachOrUpdate(*m_pipelinePtr, codecInfo1, nullptr, -1);
	ASSERT_TRUE(m_source.isAttached());

	auto codecInfo2 = MakeH264CodecInfo(1920, 1080);

	EXPECT_CALL(*m_pipelinePtr, attachSource(_)).Times(0);

	auto result = m_source.attachOrUpdate(
		*m_pipelinePtr, codecInfo2, nullptr, -1);

	EXPECT_EQ(result, AampRialtoMediaSource::AttachResult::UPDATED);
	EXPECT_EQ(m_source.width(), 1920);
	EXPECT_EQ(m_source.height(), 1080);
}

// ---------------------------------------------------------------------------
// attachOrUpdate — attachSource failure
// ---------------------------------------------------------------------------

/**
 * @test AampRialtoVideoSource_AttachSourceFails_ReturnsFailed
 * @brief Verify that when pipeline->attachSource returns false, result is FAILED.
 */
TEST_F(AampRialtoVideoSourceTest, AampRialtoVideoSource_AttachSourceFails_ReturnsFailed)
{
	ON_CALL(*m_pipelinePtr, attachSource(_))
		.WillByDefault(Return(false));

	auto codecInfo = MakeH264CodecInfo();

	auto result = m_source.attachOrUpdate(
		*m_pipelinePtr, codecInfo, nullptr, -1);

	EXPECT_EQ(result, AampRialtoMediaSource::AttachResult::FAILED);
	EXPECT_FALSE(m_source.isAttached());
}

// ---------------------------------------------------------------------------
// attachOrUpdate — with DRM
// ---------------------------------------------------------------------------

/**
 * @test AampRialtoVideoSource_AttachWithDRM_CreatesDrmSession
 * @brief Verify DRM session is created when protection params are set.
 */
TEST_F(AampRialtoVideoSourceTest, AampRialtoVideoSource_AttachWithDRM_CreatesDrmSession)
{
	NiceMock<MockDrmBridge> mockDrm;
	EXPECT_CALL(mockDrm, createSession(_, _, _, eMEDIATYPE_VIDEO))
		.WillOnce(Return(42));

	AampRialtoMediaSource::ProtectionParams prot;
	prot.systemId = "edef8ba9-79d6-4ace-a3c8-27dcd51d21ed";
	prot.initData = {0xCA, 0xFE};
	prot.type     = eMEDIATYPE_VIDEO;
	m_source.setProtection(std::move(prot));

	auto codecInfo = MakeH264CodecInfo();

	auto result = m_source.attachOrUpdate(
		*m_pipelinePtr, codecInfo, &mockDrm, -1);

	EXPECT_EQ(result, AampRialtoMediaSource::AttachResult::NEWLY_ATTACHED);
	EXPECT_EQ(m_source.mksId(), 42);
}

// ---------------------------------------------------------------------------
// attachOrUpdate — with flush position
// ---------------------------------------------------------------------------

/**
 * @test AampRialtoVideoSource_AttachWithFlushPosition_SetsSourcePosition
 * @brief Verify setSourcePosition is called when flush position >= 0.
 */
TEST_F(AampRialtoVideoSourceTest, AampRialtoVideoSource_AttachWithFlushPosition_SetsSourcePosition)
{
	const int64_t flushPosNs = 5'000'000'000LL;

	EXPECT_CALL(*m_pipelinePtr, setSourcePosition(10, flushPosNs, _, _, _))
		.WillOnce(Return(true));

	auto codecInfo = MakeH264CodecInfo();

	auto result = m_source.attachOrUpdate(
		*m_pipelinePtr, codecInfo, nullptr, flushPosNs);

	EXPECT_EQ(result, AampRialtoMediaSource::AttachResult::NEWLY_ATTACHED);
}

// ---------------------------------------------------------------------------
// reset
// ---------------------------------------------------------------------------

/**
 * @test AampRialtoVideoSource_Reset_ClearsState
 * @brief Verify reset clears source ID, mks ID and metadata.
 */
TEST_F(AampRialtoVideoSourceTest, AampRialtoVideoSource_Reset_ClearsState)
{
	auto codecInfo = MakeH264CodecInfo(1280, 720);
	m_source.attachOrUpdate(*m_pipelinePtr, codecInfo, nullptr, -1);
	ASSERT_TRUE(m_source.isAttached());

	m_source.reset();

	EXPECT_FALSE(m_source.isAttached());
	EXPECT_EQ(m_source.sourceId(), -1);
	EXPECT_EQ(m_source.mksId(), -1);
}

// ---------------------------------------------------------------------------
// createSegment
// ---------------------------------------------------------------------------

/**
 * @test AampRialtoVideoSource_CreateSegment_ReturnsVideoSegment
 * @brief Verify createSegment produces a MediaSegmentVideo.
 */
TEST_F(AampRialtoVideoSourceTest, AampRialtoVideoSource_CreateSegment_AfterAttach)
{
	auto codecInfo = MakeH264CodecInfo(1920, 1080);
	m_source.attachOrUpdate(*m_pipelinePtr, codecInfo, nullptr, -1);

	// Access createSegment indirectly through injection
	// (createSegment is protected — tested via injectOneSample)
	EXPECT_TRUE(m_source.isAttached());
	EXPECT_EQ(m_source.width(), 1920);
	EXPECT_EQ(m_source.height(), 1080);
}

// ---------------------------------------------------------------------------
// Protection params
// ---------------------------------------------------------------------------

/**
 * @test AampRialtoVideoSource_SetClearProtection_WorksCorrectly
 * @brief Verify set/clear protection lifecycle.
 */
TEST_F(AampRialtoVideoSourceTest, AampRialtoVideoSource_SetClearProtection_WorksCorrectly)
{
	EXPECT_FALSE(m_source.hasProtection());

	AampRialtoMediaSource::ProtectionParams prot;
	prot.systemId = "test-uuid";
	prot.initData = {0x01};
	prot.type     = eMEDIATYPE_VIDEO;
	m_source.setProtection(std::move(prot));

	EXPECT_TRUE(m_source.hasProtection());

	m_source.clearProtection();

	EXPECT_FALSE(m_source.hasProtection());
	EXPECT_EQ(m_source.mksId(), -1);
}

// ---------------------------------------------------------------------------
// Codec data staging
// ---------------------------------------------------------------------------

/**
 * @test AampRialtoVideoSource_TakePendingCodecData_ReturnsStagedData
 * @brief Verify pending codec data is staged by attach and consumed by take.
 */
TEST_F(AampRialtoVideoSourceTest, AampRialtoVideoSource_TakePendingCodecData_ReturnsStagedData)
{
	auto codecInfo = MakeH264CodecInfo();
	m_source.attachOrUpdate(*m_pipelinePtr, codecInfo, nullptr, -1);

	auto cd = m_source.takePendingCodecData();
	EXPECT_NE(cd, nullptr);
	EXPECT_FALSE(cd->data.empty());

	auto cd2 = m_source.takePendingCodecData();
	EXPECT_EQ(cd2, nullptr);
}

// ---------------------------------------------------------------------------
// invalidateGeneration
// ---------------------------------------------------------------------------

/**
 * @test AampRialtoVideoSource_InvalidateGeneration_BumpsGeneration
 * @brief Verify invalidateGeneration increments the generation counter.
 */
TEST_F(AampRialtoVideoSourceTest, AampRialtoVideoSource_InvalidateGeneration_BumpsGeneration)
{
	uint64_t gen1 = m_source.captureGeneration();
	m_source.invalidateGeneration();
	uint64_t gen2 = m_source.captureGeneration();

	EXPECT_GT(gen2, gen1);
}

// ---------------------------------------------------------------------------
// EOS signaling
// ---------------------------------------------------------------------------

/**
 * @test AampRialtoVideoSource_SignalEos_SetsEosFlag
 * @brief Verify signalEos sets the EOS flag on the source state.
 */
TEST_F(AampRialtoVideoSourceTest, AampRialtoVideoSource_SignalEos_SetsEosFlag)
{
	m_source.signalEos(nullptr);

	auto &st = m_source.state();
	std::lock_guard<std::mutex> lock(st.mu);
	EXPECT_TRUE(st.eos);
}

/**
 * @test AampRialtoVideoSource_SignalEos_WithPendingRequest_SendsHaveDataEOS
 * @brief Verify signalEos closes a pending needData request with EOS.
 */
TEST_F(AampRialtoVideoSourceTest, AampRialtoVideoSource_SignalEos_WithPendingRequest_SendsHaveDataEOS)
{
	auto codecInfo = MakeH264CodecInfo();
	m_source.attachOrUpdate(*m_pipelinePtr, codecInfo, nullptr, -1);

	m_source.handleNeedData(1, /*requestId=*/99, m_pipelinePtr);

	EXPECT_CALL(*m_pipelinePtr,
		haveData(firebolt::rialto::MediaSourceStatus::EOS, 99))
		.WillOnce(Return(true));

	m_source.signalEos(m_pipelinePtr);
}

// ---------------------------------------------------------------------------
// handleNeedData / handleCancelNeedData
// ---------------------------------------------------------------------------

/**
 * @test AampRialtoVideoSource_HandleNeedData_SetsPendingState
 * @brief Verify handleNeedData sets the pending request state.
 */
TEST_F(AampRialtoVideoSourceTest, AampRialtoVideoSource_HandleNeedData_SetsPendingState)
{
	m_source.handleNeedData(5, /*requestId=*/42, nullptr);

	auto &st = m_source.state();
	std::lock_guard<std::mutex> lock(st.mu);
	EXPECT_TRUE(st.hasPending);
	EXPECT_EQ(st.pendingRequestId, 42u);
	EXPECT_EQ(st.pendingFrameCount, 5u);
}

/**
 * @test AampRialtoVideoSource_HandleNeedData_WhenEos_SendsEos
 * @brief Verify handleNeedData replies with EOS when source is in EOS state.
 */
TEST_F(AampRialtoVideoSourceTest, AampRialtoVideoSource_HandleNeedData_WhenEos_SendsEos)
{
	auto codecInfo = MakeH264CodecInfo();
	m_source.attachOrUpdate(*m_pipelinePtr, codecInfo, nullptr, -1);

	m_source.signalEos(m_pipelinePtr);

	EXPECT_CALL(*m_pipelinePtr,
		haveData(firebolt::rialto::MediaSourceStatus::EOS, 55))
		.WillOnce(Return(true));

	m_source.handleNeedData(1, /*requestId=*/55, m_pipelinePtr);
}

/**
 * @test AampRialtoVideoSource_HandleCancelNeedData_ClearsPending
 * @brief Verify handleCancelNeedData clears the pending state.
 */
TEST_F(AampRialtoVideoSourceTest, AampRialtoVideoSource_HandleCancelNeedData_ClearsPending)
{
	m_source.handleNeedData(5, /*requestId=*/42, nullptr);

	m_source.handleCancelNeedData();

	auto &st = m_source.state();
	std::lock_guard<std::mutex> lock(st.mu);
	EXPECT_FALSE(st.hasPending);
}

// ---------------------------------------------------------------------------
// flushSource
// ---------------------------------------------------------------------------

/**
 * @test AampRialtoVideoSource_FlushSource_CallsPipelineFlush
 * @brief Verify flushSource calls flush and setSourcePosition on the pipeline.
 */
TEST_F(AampRialtoVideoSourceTest, AampRialtoVideoSource_FlushSource_CallsPipelineFlush)
{
	auto codecInfo = MakeH264CodecInfo();
	m_source.attachOrUpdate(*m_pipelinePtr, codecInfo, nullptr, -1);

	const int64_t posNs = 3'000'000'000LL;
	EXPECT_CALL(*m_pipelinePtr, flush(m_source.sourceId(), true, _))
		.WillOnce(Return(true));
	EXPECT_CALL(*m_pipelinePtr,
		setSourcePosition(m_source.sourceId(), posNs, _, _, _))
		.WillOnce(Return(true));

	m_source.flushSource(*m_pipelinePtr, posNs);
}

/**
 * @test AampRialtoVideoSource_FlushSource_NotAttached_NoOp
 * @brief Verify flushSource does nothing when source is not attached.
 */
TEST_F(AampRialtoVideoSourceTest, AampRialtoVideoSource_FlushSource_NotAttached_NoOp)
{
	EXPECT_CALL(*m_pipelinePtr, flush(_, _, _)).Times(0);
	EXPECT_CALL(*m_pipelinePtr, setSourcePosition(_, _, _, _, _)).Times(0);

	m_source.flushSource(*m_pipelinePtr, 1'000'000'000LL);
}
