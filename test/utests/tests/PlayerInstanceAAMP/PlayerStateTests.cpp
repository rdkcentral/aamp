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
 * @file PlayerStateTests.cpp
 * @brief Unit tests for PlayerInstanceAAMP state transitions.
 */

#include <gtest/gtest.h>
#include <gmock/gmock.h>

#include "main_aamp.h"
#include "priv_aamp.h"
#include "AampConfig.h"
#include "AampDefine.h"
#include "MockAampConfig.h"
#include "MockAampGstPlayer.h"
#include "MockAampStreamSinkManager.h"
#include "MockPrivateInstanceAAMP.h"
#include "MockStreamAbstractionAAMP.h"

using ::testing::_;
using ::testing::NiceMock;
using ::testing::Return;

/**
 * @class TestablePlayerInstanceAAMP
 * @brief Subclass of PlayerInstanceAAMP that exposes the internal
 *        PrivateInstanceAAMP pointer for unit testing purposes.
 *
 * This follows the same pattern used by PauseOnPlaybackTests and avoids
 * the ownership problem that arises from replacing the `aamp` raw pointer
 * with a separately allocated instance.
 */
class TestablePlayerInstanceAAMP : public PlayerInstanceAAMP
{
public:
	TestablePlayerInstanceAAMP() : PlayerInstanceAAMP() {}

	/**
	 * @brief Return the PrivateInstanceAAMP created and owned by this player.
	 * @return Non-owning pointer to the internal PrivateInstanceAAMP.
	 */
	PrivateInstanceAAMP *GetPrivAamp()
	{
		return aamp;
	}
};

/**
 * @class PlayerInstanceAAMPStateTests
 * @brief Fixture for player-facing state transition tests.
 */
class PlayerInstanceAAMPStateTests : public ::testing::Test
{
protected:
	TestablePlayerInstanceAAMP *mPlayerInstanceAAMP{};
	PrivateInstanceAAMP *mPrivateInstanceAAMP{};  ///< Non-owning; owned by mPlayerInstanceAAMP via sp_aamp.
	AAMPPlayerState mCurrentState{eSTATE_RELEASED};

	void SetUp() override
	{
		if (gpGlobalConfig == nullptr)
		{
			gpGlobalConfig = new AampConfig();
		}

		g_mockAampConfig = new NiceMock<MockAampConfig>();
		g_mockPrivateInstanceAAMP = new NiceMock<MockPrivateInstanceAAMP>();
		g_mockAampStreamSinkManager =
			new NiceMock<MockAampStreamSinkManager>();

		mPlayerInstanceAAMP = new TestablePlayerInstanceAAMP();
		mPrivateInstanceAAMP = mPlayerInstanceAAMP->GetPrivAamp();

		g_mockAampGstPlayer =
			new NiceMock<MockAAMPGstPlayer>(mPrivateInstanceAAMP);
		g_mockStreamAbstractionAAMP =
			new NiceMock<MockStreamAbstractionAAMP>(mPrivateInstanceAAMP);

		mPrivateInstanceAAMP->mpStreamAbstractionAAMP =
			g_mockStreamAbstractionAAMP;

		ON_CALL(*g_mockAampStreamSinkManager, GetStreamSink(_))
			.WillByDefault(Return(g_mockAampGstPlayer));

		ON_CALL(*g_mockPrivateInstanceAAMP, GetState())
			.WillByDefault([this]() { return mCurrentState; });
		ON_CALL(*g_mockPrivateInstanceAAMP, SetState(_, _))
			.WillByDefault([this](AAMPPlayerState state, bool) {
				mCurrentState = state;
			});
	}

	void TearDown() override
	{
		delete mPlayerInstanceAAMP;  ///< Also deletes PrivateInstanceAAMP via sp_aamp.
		mPlayerInstanceAAMP = nullptr;
		mPrivateInstanceAAMP = nullptr;  ///< Non-owning; already deleted above.

		delete g_mockStreamAbstractionAAMP;
		g_mockStreamAbstractionAAMP = nullptr;

		delete g_mockAampGstPlayer;
		g_mockAampGstPlayer = nullptr;

		delete g_mockAampStreamSinkManager;
		g_mockAampStreamSinkManager = nullptr;

		delete g_mockPrivateInstanceAAMP;
		g_mockPrivateInstanceAAMP = nullptr;

		delete g_mockAampConfig;
		g_mockAampConfig = nullptr;

		delete gpGlobalConfig;
		gpGlobalConfig = nullptr;
	}
};

/**
 * @test PlayerState_Seek_FullSequence_PlayingToSeekingToPlaying
 * @brief Verify that PlayerInstanceAAMP::Seek() drives the player into
 *        SEEKING and the state returns to PLAYING when the underlying
 *        private instance reports seek completion.
 */
TEST_F(PlayerInstanceAAMPStateTests,
	PlayerState_Seek_FullSequence_PlayingToSeekingToPlaying)
{
	mPrivateInstanceAAMP->SetState(eSTATE_PLAYING, false);
	ASSERT_EQ(mPlayerInstanceAAMP->GetState(), eSTATE_PLAYING);

	mPlayerInstanceAAMP->Seek(10.0);
	EXPECT_EQ(mPlayerInstanceAAMP->GetState(), eSTATE_SEEKING);

	EXPECT_EQ(mPrivateInstanceAAMP->IsFirstVideoFrameDisplayedRequired(), false);
	mPrivateInstanceAAMP->NotifyFirstFrameReceived(0);
	EXPECT_EQ(mPlayerInstanceAAMP->GetState(), eSTATE_PLAYING);
}

/**
 * @test PlayerState_SeekWhilePaused
 * @brief Verify that SeekWhilePaused flag drives the
 * player into SEEKING and PAUSED after seek completion.
 */
TEST_F(PlayerInstanceAAMPStateTests,
	PlayerState_SeekWhilePaused)
{
	mPrivateInstanceAAMP->SetState(eSTATE_PLAYING, false);
	ASSERT_EQ(mPlayerInstanceAAMP->GetState(), eSTATE_PLAYING);

	mPrivateInstanceAAMP->SetState(eSTATE_PAUSED, false);
	ASSERT_EQ(mPlayerInstanceAAMP->GetState(), eSTATE_PAUSED);

	mPrivateInstanceAAMP->mSinkPaused = true;
	mPlayerInstanceAAMP->Seek(10.0, true);
	EXPECT_EQ(mPlayerInstanceAAMP->GetState(), eSTATE_SEEKING);

	EXPECT_EQ(mPrivateInstanceAAMP->IsFirstVideoFrameDisplayedRequired(), true);
	mPrivateInstanceAAMP->NotifyFirstVideoFrameDisplayed();
	EXPECT_EQ(mPlayerInstanceAAMP->GetState(), eSTATE_PAUSED);
}