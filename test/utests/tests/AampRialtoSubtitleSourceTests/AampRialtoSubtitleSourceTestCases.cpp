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

	AampRialtoSubtitleSource m_source;
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
