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
 * @file AampRialtoPlayerTestCases.cpp
 * @brief L1 unit tests for AampRialtoPlayer.
 *
 * Tests are structured per the TDD implementation plan in
 * docs/rialto-integration/aamp-rialto-player-analysis.md.
 * Populate each phase before implementing the corresponding feature.
 */

#include <gtest/gtest.h>
#include <gmock/gmock.h>

#include "AampRialtoPlayer.h"
#include "MockIMediaPipeline.h"
#include "MockIMediaPipelineFactory.h"
#include "MockPrivateInstanceAAMP.h"

using ::testing::_;
using ::testing::NiceMock;
using ::testing::Return;
using ::testing::StrictMock;

// ---------------------------------------------------------------------------
// Test fixture
// ---------------------------------------------------------------------------

/**
 * @class AampRialtoPlayerTest
 * @brief Base fixture that wires a MockIMediaPipelineFactory into
 *        AampRialtoPlayer before each test.
 *
 * The mock factory returns a MockIMediaPipeline by default; individual tests
 * can override the ON_CALL expectations as needed.
 */
class AampRialtoPlayerTest : public ::testing::Test
{
protected:
	void SetUp() override
	{
		g_mockPrivateInstanceAAMP = new NiceMock<MockPrivateInstanceAAMP>();

		m_mockFactory = std::make_shared<NiceMock<MockIMediaPipelineFactory>>();
		m_mockPipeline = std::make_unique<NiceMock<MockIMediaPipeline>>();
		m_mockPipelinePtr = m_mockPipeline.get();

		// By default, factory returns the mock pipeline.
		ON_CALL(*m_mockFactory, createMediaPipeline(_, _))
			.WillByDefault([this](auto /*client*/, auto /*reqs*/) {
				return std::move(m_mockPipeline);
			});

		// By default, load() succeeds.
		ON_CALL(*m_mockPipelinePtr, load(_, _, _))
			.WillByDefault(Return(true));

		m_player = std::make_unique<AampRialtoPlayer>(
			reinterpret_cast<PrivateInstanceAAMP *>(g_mockPrivateInstanceAAMP),
			/*id3HandlerCallback=*/nullptr);
		m_player->SetPipelineFactoryForTesting(m_mockFactory);
	}

	void TearDown() override
	{
		m_player.reset();
		delete g_mockPrivateInstanceAAMP;
		g_mockPrivateInstanceAAMP = nullptr;
	}

	/// Call Configure() with sensible defaults for video+audio.
	void Configure(
		StreamOutputFormat video = FORMAT_ISO_BMFF,
		StreamOutputFormat audio = FORMAT_ISO_BMFF)
	{
		m_player->Configure(video, audio, FORMAT_UNKNOWN,
			/*bESChangeStatus=*/false,
			/*setReadyAfterPipelineCreation=*/false);
	}

	std::shared_ptr<NiceMock<MockIMediaPipelineFactory>> m_mockFactory;
	std::unique_ptr<NiceMock<MockIMediaPipeline>>        m_mockPipeline;
	NiceMock<MockIMediaPipeline> *                       m_mockPipelinePtr{nullptr};
	std::unique_ptr<AampRialtoPlayer>                    m_player;
};

// ---------------------------------------------------------------------------
// Phase 2 — Configure / pipeline creation
// (Uncomment and fill in as Phase 2 implementation begins)
// ---------------------------------------------------------------------------

// TEST_F(AampRialtoPlayerTest, Configure_ValidFormats_CreatesPipeline)
// {
//     EXPECT_CALL(*m_mockFactory, createMediaPipeline(_, _)).Times(1);
//     EXPECT_CALL(*m_mockPipelinePtr, load(_, _, _)).WillOnce(Return(true));
//     Configure();
//     // Pipeline was handed to the player — mock pointer still accessible via m_mockPipelinePtr.
// }

// TEST_F(AampRialtoPlayerTest, Configure_NullFactory_DoesNotCrash)
// {
//     m_player->SetPipelineFactoryForTesting(nullptr);
//     // Should not crash; pipeline will be null.
//     EXPECT_NO_THROW(Configure());
// }
