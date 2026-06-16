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
 * @file AampRialtoAudioSourceTestCases.cpp
 * @brief L1 unit tests for AampRialtoAudioSource.
 */

#include <gtest/gtest.h>
#include <gmock/gmock.h>

#include "AampRialtoAudioSource.h"
#include "MockIMediaPipeline.h"
#include "MockDrmBridge.h"

using ::testing::_;
using ::testing::Invoke;
using ::testing::NiceMock;
using ::testing::Return;

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

static MediaCodecInfo MakeAacCodecInfo(
	uint16_t channels = 2, uint16_t sampleRate = 48000)
{
	MediaCodecInfo ci{};
	ci.mCodecFormat              = GST_FORMAT_AUDIO_ES_AAC_RAW;
	ci.mInfo.audio.mChannelCount = channels;
	ci.mInfo.audio.mSampleRate   = sampleRate;
	ci.mCodecData                = {0xAA, 0xBB};
	return ci;
}

static MediaCodecInfo MakeEc3CodecInfo(
	uint16_t channels = 6, uint16_t sampleRate = 48000)
{
	MediaCodecInfo ci{};
	ci.mCodecFormat              = GST_FORMAT_AUDIO_ES_EC3;
	ci.mInfo.audio.mChannelCount = channels;
	ci.mInfo.audio.mSampleRate   = sampleRate;
	ci.mCodecData                = {0xCC, 0xDD};
	return ci;
}

static MediaCodecInfo MakeAc4CodecInfo(
	uint16_t channels = 2, uint16_t sampleRate = 44100)
{
	MediaCodecInfo ci{};
	ci.mCodecFormat              = GST_FORMAT_AUDIO_ES_AC4;
	ci.mInfo.audio.mChannelCount = channels;
	ci.mInfo.audio.mSampleRate   = sampleRate;
	ci.mCodecData                = {0xEE};
	return ci;
}

// ---------------------------------------------------------------------------
// Fixture
// ---------------------------------------------------------------------------

/**
 * @class AampRialtoAudioSourceTest
 * @brief Fixture for AampRialtoAudioSource unit tests.
 */
class AampRialtoAudioSourceTest : public ::testing::Test
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

	AampRialtoAudioSource m_source;
	std::unique_ptr<MockIMediaPipeline> m_mockPipeline;
	MockIMediaPipeline *m_pipelinePtr{nullptr};
	int32_t m_nextSourceId{20};
};

// ---------------------------------------------------------------------------
// mediaType
// ---------------------------------------------------------------------------

/**
 * @test AampRialtoAudioSource_mediaType_ReturnsAudio
 * @brief Verify mediaType() returns eMEDIATYPE_AUDIO.
 */
TEST_F(AampRialtoAudioSourceTest, AampRialtoAudioSource_mediaType_ReturnsAudio)
{
	EXPECT_EQ(m_source.mediaType(), eMEDIATYPE_AUDIO);
}

// ---------------------------------------------------------------------------
// Initial state
// ---------------------------------------------------------------------------

/**
 * @test AampRialtoAudioSource_InitialState_NotAttached
 * @brief Verify source starts unattached.
 */
TEST_F(AampRialtoAudioSourceTest, AampRialtoAudioSource_InitialState_NotAttached)
{
	EXPECT_FALSE(m_source.isAttached());
	EXPECT_EQ(m_source.sourceId(), -1);
	EXPECT_EQ(m_source.mksId(), -1);
	EXPECT_EQ(m_source.sampleRate(), 0);
	EXPECT_EQ(m_source.channels(), 0);
}

// ---------------------------------------------------------------------------
// attachOrUpdate — AAC
// ---------------------------------------------------------------------------

/**
 * @test AampRialtoAudioSource_AttachAac_Success
 * @brief Verify first attach with AAC codec succeeds.
 */
TEST_F(AampRialtoAudioSourceTest, AampRialtoAudioSource_AttachAac_Success)
{
	auto codecInfo = MakeAacCodecInfo(2, 48000);

	EXPECT_CALL(*m_pipelinePtr, attachSource(_)).Times(1);

	auto result = m_source.attachOrUpdate(
		*m_pipelinePtr, codecInfo, nullptr, -1);

	EXPECT_EQ(result, AampRialtoMediaSource::AttachResult::NEWLY_ATTACHED);
	EXPECT_TRUE(m_source.isAttached());
	EXPECT_EQ(m_source.sourceId(), 20);
	EXPECT_EQ(m_source.sampleRate(), 48000);
	EXPECT_EQ(m_source.channels(), 2);
}

// ---------------------------------------------------------------------------
// attachOrUpdate — EAC3
// ---------------------------------------------------------------------------

/**
 * @test AampRialtoAudioSource_AttachEc3_Success
 * @brief Verify first attach with EAC3 codec succeeds.
 */
TEST_F(AampRialtoAudioSourceTest, AampRialtoAudioSource_AttachEc3_Success)
{
	auto codecInfo = MakeEc3CodecInfo(6, 48000);

	auto result = m_source.attachOrUpdate(
		*m_pipelinePtr, codecInfo, nullptr, -1);

	EXPECT_EQ(result, AampRialtoMediaSource::AttachResult::NEWLY_ATTACHED);
	EXPECT_TRUE(m_source.isAttached());
	EXPECT_EQ(m_source.sampleRate(), 48000);
	EXPECT_EQ(m_source.channels(), 6);
}

// ---------------------------------------------------------------------------
// attachOrUpdate — AC4
// ---------------------------------------------------------------------------

/**
 * @test AampRialtoAudioSource_AttachAc4_Success
 * @brief Verify first attach with AC4 codec succeeds.
 */
TEST_F(AampRialtoAudioSourceTest, AampRialtoAudioSource_AttachAc4_Success)
{
	auto codecInfo = MakeAc4CodecInfo(2, 44100);

	auto result = m_source.attachOrUpdate(
		*m_pipelinePtr, codecInfo, nullptr, -1);

	EXPECT_EQ(result, AampRialtoMediaSource::AttachResult::NEWLY_ATTACHED);
	EXPECT_TRUE(m_source.isAttached());
	EXPECT_EQ(m_source.sampleRate(), 44100);
	EXPECT_EQ(m_source.channels(), 2);
}

// ---------------------------------------------------------------------------
// attachOrUpdate — unknown codec
// ---------------------------------------------------------------------------

/**
 * @test AampRialtoAudioSource_AttachUnknownCodec_Fails
 * @brief Verify attach with unrecognised codec returns FAILED.
 */
TEST_F(AampRialtoAudioSourceTest, AampRialtoAudioSource_AttachUnknownCodec_Fails)
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
 * @test AampRialtoAudioSource_AttachTwice_ReturnsUpdated
 * @brief Verify second attach returns UPDATED and refreshes metadata.
 */
TEST_F(AampRialtoAudioSourceTest, AampRialtoAudioSource_AttachTwice_ReturnsUpdated)
{
	auto codecInfo1 = MakeAacCodecInfo(2, 48000);
	m_source.attachOrUpdate(*m_pipelinePtr, codecInfo1, nullptr, -1);
	ASSERT_TRUE(m_source.isAttached());

	auto codecInfo2 = MakeAacCodecInfo(6, 44100);

	EXPECT_CALL(*m_pipelinePtr, attachSource(_)).Times(0);

	auto result = m_source.attachOrUpdate(
		*m_pipelinePtr, codecInfo2, nullptr, -1);

	EXPECT_EQ(result, AampRialtoMediaSource::AttachResult::UPDATED);
	EXPECT_EQ(m_source.sampleRate(), 44100);
	EXPECT_EQ(m_source.channels(), 6);
}

// ---------------------------------------------------------------------------
// attachOrUpdate — attachSource failure
// ---------------------------------------------------------------------------

/**
 * @test AampRialtoAudioSource_AttachSourceFails_ReturnsFailed
 * @brief Verify that when pipeline->attachSource returns false, result is FAILED.
 */
TEST_F(AampRialtoAudioSourceTest, AampRialtoAudioSource_AttachSourceFails_ReturnsFailed)
{
	ON_CALL(*m_pipelinePtr, attachSource(_))
		.WillByDefault(Return(false));

	auto codecInfo = MakeAacCodecInfo();

	auto result = m_source.attachOrUpdate(
		*m_pipelinePtr, codecInfo, nullptr, -1);

	EXPECT_EQ(result, AampRialtoMediaSource::AttachResult::FAILED);
	EXPECT_FALSE(m_source.isAttached());
}

// ---------------------------------------------------------------------------
// attachOrUpdate — with DRM
// ---------------------------------------------------------------------------

/**
 * @test AampRialtoAudioSource_AttachWithDRM_CreatesDrmSession
 * @brief Verify DRM session is created when protection params are set.
 */
TEST_F(AampRialtoAudioSourceTest, AampRialtoAudioSource_AttachWithDRM_CreatesDrmSession)
{
	NiceMock<MockDrmBridge> mockDrm;
	EXPECT_CALL(mockDrm, createSession(_, _, _, eMEDIATYPE_AUDIO))
		.WillOnce(Return(77));

	AampRialtoMediaSource::ProtectionParams prot;
	prot.systemId = "edef8ba9-79d6-4ace-a3c8-27dcd51d21ed";
	prot.initData = {0xCA, 0xFE};
	prot.type     = eMEDIATYPE_AUDIO;

	auto codecInfo = MakeAacCodecInfo();

	auto result = m_source.attachOrUpdate(
		*m_pipelinePtr, codecInfo, &mockDrm, -1, prot);

	EXPECT_EQ(result, AampRialtoMediaSource::AttachResult::NEWLY_ATTACHED);
	EXPECT_EQ(m_source.mksId(), 77);
}

// ---------------------------------------------------------------------------
// attachOrUpdate — with flush position
// ---------------------------------------------------------------------------

/**
 * @test AampRialtoAudioSource_AttachWithFlushPosition_SetsSourcePosition
 * @brief Verify setSourcePosition is called when flush position >= 0.
 */
TEST_F(AampRialtoAudioSourceTest, AampRialtoAudioSource_AttachWithFlushPosition_SetsSourcePosition)
{
	const int64_t flushPosNs = 2'000'000'000LL;

	EXPECT_CALL(*m_pipelinePtr, setSourcePosition(20, flushPosNs, _, _, _))
		.WillOnce(Return(true));

	auto codecInfo = MakeAacCodecInfo();

	auto result = m_source.attachOrUpdate(
		*m_pipelinePtr, codecInfo, nullptr, flushPosNs);

	EXPECT_EQ(result, AampRialtoMediaSource::AttachResult::NEWLY_ATTACHED);
}

// ---------------------------------------------------------------------------
// reset
// ---------------------------------------------------------------------------

/**
 * @test AampRialtoAudioSource_Reset_ClearsState
 * @brief Verify reset clears source ID, mks ID.
 */
TEST_F(AampRialtoAudioSourceTest, AampRialtoAudioSource_Reset_ClearsState)
{
	auto codecInfo = MakeAacCodecInfo();
	m_source.attachOrUpdate(*m_pipelinePtr, codecInfo, nullptr, -1);
	ASSERT_TRUE(m_source.isAttached());

	m_source.reset();

	EXPECT_FALSE(m_source.isAttached());
	EXPECT_EQ(m_source.sourceId(), -1);
	EXPECT_EQ(m_source.mksId(), -1);
}

// ---------------------------------------------------------------------------
// Empty codec data warning
// ---------------------------------------------------------------------------

/**
 * @test AampRialtoAudioSource_AttachEmptyCodecData_StillAttaches
 * @brief Verify attachment succeeds with empty codec data (logs warning).
 */
TEST_F(AampRialtoAudioSourceTest, AampRialtoAudioSource_AttachEmptyCodecData_StillAttaches)
{
	MediaCodecInfo ci{};
	ci.mCodecFormat              = GST_FORMAT_AUDIO_ES_AAC_RAW;
	ci.mInfo.audio.mChannelCount = 2;
	ci.mInfo.audio.mSampleRate   = 48000;
	// mCodecData intentionally left empty

	auto result = m_source.attachOrUpdate(
		*m_pipelinePtr, ci, nullptr, -1);

	EXPECT_EQ(result, AampRialtoMediaSource::AttachResult::NEWLY_ATTACHED);
	EXPECT_TRUE(m_source.isAttached());
}

// ---------------------------------------------------------------------------
// EOS signaling
// ---------------------------------------------------------------------------

/**
 * @test AampRialtoAudioSource_SignalEos_SetsEosFlag
 * @brief Verify signalEos sets the EOS flag.
 */
TEST_F(AampRialtoAudioSourceTest, AampRialtoAudioSource_SignalEos_SetsEosFlag)
{
	m_source.signalEos(nullptr);

	auto &st = m_source.state();
	std::lock_guard<std::mutex> lock(st.mu);
	EXPECT_TRUE(st.eos);
}

// ---------------------------------------------------------------------------
// handleNeedData / handleCancelNeedData
// ---------------------------------------------------------------------------

/**
 * @test AampRialtoAudioSource_HandleNeedData_SetsPendingState
 * @brief Verify handleNeedData sets the pending request state.
 */
TEST_F(AampRialtoAudioSourceTest, AampRialtoAudioSource_HandleNeedData_SetsPendingState)
{
	m_source.handleNeedData(3, /*requestId=*/88, nullptr);

	auto &st = m_source.state();
	std::lock_guard<std::mutex> lock(st.mu);
	EXPECT_TRUE(st.hasPending);
	EXPECT_EQ(st.pendingRequestId, 88u);
	EXPECT_EQ(st.pendingFrameCount, 3u);
}

/**
 * @test AampRialtoAudioSource_HandleCancelNeedData_ClearsPending
 * @brief Verify handleCancelNeedData clears the pending state.
 */
TEST_F(AampRialtoAudioSourceTest, AampRialtoAudioSource_HandleCancelNeedData_ClearsPending)
{
	m_source.handleNeedData(3, /*requestId=*/88, nullptr);
	m_source.handleCancelNeedData();

	auto &st = m_source.state();
	std::lock_guard<std::mutex> lock(st.mu);
	EXPECT_FALSE(st.hasPending);
}

// ---------------------------------------------------------------------------
// flushSource
// ---------------------------------------------------------------------------

/**
 * @test AampRialtoAudioSource_FlushSource_CallsPipelineFlush
 * @brief Verify flushSource calls flush and setSourcePosition.
 */
TEST_F(AampRialtoAudioSourceTest, AampRialtoAudioSource_FlushSource_CallsPipelineFlush)
{
	auto codecInfo = MakeAacCodecInfo();
	m_source.attachOrUpdate(*m_pipelinePtr, codecInfo, nullptr, -1);

	const int64_t posNs = 1'500'000'000LL;
	EXPECT_CALL(*m_pipelinePtr, flush(m_source.sourceId(), true, _))
		.WillOnce(Return(true));
	EXPECT_CALL(*m_pipelinePtr,
		setSourcePosition(m_source.sourceId(), posNs, _, _, _))
		.WillOnce(Return(true));

	m_source.flushSource(*m_pipelinePtr, posNs);
}

