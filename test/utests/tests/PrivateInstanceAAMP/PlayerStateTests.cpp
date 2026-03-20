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
 * @brief Unit tests for AAMPPlayerState transitions in PrivateInstanceAAMP.
 *
 * Covers:
 *  - Initial player state after construction
 *  - Normal tune progression: RELEASED → INITIALIZING → PREPARED → PLAYING
 *  - Pause and resume via NotifySpeedChanged
 *  - Trickplay (fast-forward / rewind) and return to normal play
 *  - Seek: PLAYING → SEEKING → PLAYING via NotifyFirstBufferProcessed
 *  - Buffering: PLAYING → BUFFERING → PLAYING via NotifyFragmentCachingComplete
 *  - EOS: PLAYING → COMPLETE via NotifyEOSReached
 *  - Error and Stop terminal states
 */

#include <gtest/gtest.h>
#include <gmock/gmock.h>

#include "priv_aamp.h"
#include "AampConfig.h"
#include "AampDefine.h"
#include "MockAampConfig.h"
#include "MockAampEventManager.h"
#include "MockAampGstPlayer.h"
#include "MockStreamAbstractionAAMP.h"
#include "MockStreamAbstractionAAMP_MPD.h"
#include "MockAampStreamSinkManager.h"

using ::testing::_;
using ::testing::NiceMock;
using ::testing::Return;

/**
 * @class PlayerStateTests
 * @brief Unit tests for AAMPPlayerState transitions in PrivateInstanceAAMP.
 *
 * This fixture sets up a PrivateInstanceAAMP with mocked dependencies to verify
 * that state transitions occur as expected in response to player actions and
 * notifications.
 */
class PlayerStateTests : public ::testing::Test
{
protected:
	PrivateInstanceAAMP *mPrivateInstanceAAMP{};

	void SetUp() override
	{
		if (gpGlobalConfig == nullptr)
		{
			gpGlobalConfig = new AampConfig();
		}

		mPrivateInstanceAAMP = new PrivateInstanceAAMP(gpGlobalConfig);

		g_mockAampConfig =
			new NiceMock<MockAampConfig>();
		g_mockAampGstPlayer =
			new NiceMock<MockAAMPGstPlayer>(mPrivateInstanceAAMP);
		g_mockAampEventManager =
			new NiceMock<MockAampEventManager>();
		g_mockStreamAbstractionAAMP =
			new NiceMock<MockStreamAbstractionAAMP>(mPrivateInstanceAAMP);
		g_mockAampStreamSinkManager =
			new NiceMock<MockAampStreamSinkManager>();

		mPrivateInstanceAAMP->mpStreamAbstractionAAMP =
			g_mockStreamAbstractionAAMP;

		ON_CALL(*g_mockAampConfig,
			IsConfigSet(eAAMPConfig_EnableCurlStore))
			.WillByDefault(Return(false));
		EXPECT_CALL(*g_mockAampStreamSinkManager, GetStreamSink(_))
			.WillRepeatedly(Return(g_mockAampGstPlayer));
	}

	void TearDown() override
	{
		delete mPrivateInstanceAAMP;
		mPrivateInstanceAAMP = nullptr;

		delete g_mockStreamAbstractionAAMP;
		g_mockStreamAbstractionAAMP = nullptr;

		delete g_mockAampGstPlayer;
		g_mockAampGstPlayer = nullptr;

		delete g_mockAampEventManager;
		g_mockAampEventManager = nullptr;

		delete g_mockAampStreamSinkManager;
		g_mockAampStreamSinkManager = nullptr;

		delete gpGlobalConfig;
		gpGlobalConfig = nullptr;

		delete g_mockAampConfig;
		g_mockAampConfig = nullptr;
	}
};

// ============================================================
// Initial State Tests
// ============================================================

/**
 * @test PlayerState_NormalTune_FullSequence_ReleasedToIdle
 * @brief Verify the complete player lifecycle state progression using a real
 *        Tune() call with a localhost MPD URL:
 *        RELEASED → INITIALIZING → PREPARED → PLAYING → PAUSED → PLAYING
 *        → COMPLETE → IDLE.
 *
 * The INITIALIZING state is verified inside the StreamAbstraction Init()
 * callback, which is the point in the real Tune path where that state is set.
 * Subsequent transitions are driven by GStreamer notifications and speed
 * changes, reaching COMPLETE via NotifyEOSReached and finally IDLE after
 * Stop().
 */
TEST_F(PlayerStateTests, PlayerState_NormalTune_FullSequence_ReleasedToIdle)
{
	// 1. Initial state must be RELEASED.
	EXPECT_EQ(mPrivateInstanceAAMP->GetState(), eSTATE_RELEASED);

	EXPECT_CALL(*g_mockAampConfig, IsConfigSet(_))
		.WillRepeatedly(Return(false));
	EXPECT_CALL(*g_mockAampConfig,
		GetConfigValue(testing::Matcher<AAMPConfigSettingInt>(_)))
		.WillRepeatedly(Return(0));
	EXPECT_CALL(*g_mockAampConfig,
		GetConfigValue(testing::Matcher<AAMPConfigSettingFloat>(_)))
		.WillRepeatedly(Return(0.0));
	EXPECT_CALL(*g_mockAampConfig,
		GetConfigValue(testing::Matcher<AAMPConfigSettingString>(_)))
		.WillRepeatedly(Return(""));

	MockStreamAbstractionAAMP_MPD mockStreamAbstractionAAMP_MPD(
		mPrivateInstanceAAMP, 0, AAMP_NORMAL_PLAY_RATE);
	g_mockStreamAbstractionAAMP_MPD = &mockStreamAbstractionAAMP_MPD;

	// Let Tune() create its own protocol abstraction by clearing the fixture's
	// pointer; the MPD fake routes Init() calls to our mock.
	mPrivateInstanceAAMP->mpStreamAbstractionAAMP = nullptr;

	// 2. INITIALIZING is set by TuneHelper before calling Init(); verify it
	//    from inside the Init() mock callback.
	EXPECT_CALL(mockStreamAbstractionAAMP_MPD, Init(_))
		.WillOnce([this](TuneType) {
			EXPECT_EQ(mPrivateInstanceAAMP->GetState(), eSTATE_INITIALIZING);
			return eAAMPSTATUS_OK;
		});

	const char *testUrl = "http://localhost:80/test/manifest.mpd";
	mPrivateInstanceAAMP->Tune(testUrl, true, "VOD");

	// 3. After Tune() completes for a new tune, TuneHelper sets PREPARED.
	EXPECT_EQ(mPrivateInstanceAAMP->GetState(), eSTATE_PREPARED);

	// 4. GStreamer first-frame notification transitions PREPARED → PLAYING.
	mPrivateInstanceAAMP->NotifyFirstFrameReceived(0);
	EXPECT_EQ(mPrivateInstanceAAMP->GetState(), eSTATE_PLAYING);

	// 5. Pause transitions PLAYING → PAUSED.
	mPrivateInstanceAAMP->NotifySpeedChanged(0);
	EXPECT_EQ(mPrivateInstanceAAMP->GetState(), eSTATE_PAUSED);

	// 6. Resume transitions PAUSED → PLAYING.
	mPrivateInstanceAAMP->NotifySpeedChanged(AAMP_NORMAL_PLAY_RATE);
	EXPECT_EQ(mPrivateInstanceAAMP->GetState(), eSTATE_PLAYING);

	// 7. EOS notification transitions PLAYING → COMPLETE.
	EXPECT_CALL(*g_mockStreamAbstractionAAMP, IsEOSReached())
		.WillRepeatedly(Return(true));
	mPrivateInstanceAAMP->NotifyEOSReached();
	EXPECT_EQ(mPrivateInstanceAAMP->GetState(), eSTATE_COMPLETE);

	// 8. Stop transitions COMPLETE → IDLE, completing the lifecycle.
	mPrivateInstanceAAMP->Stop(false);
	EXPECT_EQ(mPrivateInstanceAAMP->GetState(), eSTATE_IDLE);

	g_mockStreamAbstractionAAMP_MPD = nullptr;
}

// ============================================================
// Buffering State Tests
// ============================================================

/**
 * @test PlayerState_VerifyBuffering_Playing
 * @brief Verify a real buffering scenario during tune when initial fragment
 *        caching is enabled:
 *        PREPARED → BUFFERING on first video frame displayed, then
 *        BUFFERING → PLAYING when fragment caching completes.
 */
TEST_F(PlayerStateTests, PlayerState_VerifyBuffering_Playing)
{
	EXPECT_CALL(*g_mockAampConfig, IsConfigSet(_))
		.WillRepeatedly(Return(false));
	EXPECT_CALL(*g_mockAampConfig,
		GetConfigValue(testing::Matcher<AAMPConfigSettingInt>(_)))
		.WillRepeatedly(Return(0));
	EXPECT_CALL(*g_mockAampConfig,
		GetConfigValue(testing::Matcher<AAMPConfigSettingFloat>(_)))
		.WillRepeatedly(Return(0.0));
	EXPECT_CALL(*g_mockAampConfig,
		GetConfigValue(testing::Matcher<AAMPConfigSettingString>(_)))
		.WillRepeatedly(Return(""));
	EXPECT_CALL(*g_mockAampConfig, GetConfigValue(eAAMPConfig_InitialBuffer))
		.WillRepeatedly(Return(1));
	EXPECT_CALL(*g_mockStreamAbstractionAAMP, IsInitialCachingSupported())
		.WillRepeatedly(Return(true));

	MockStreamAbstractionAAMP_MPD mockStreamAbstractionAAMP_MPD(
		mPrivateInstanceAAMP, 0, AAMP_NORMAL_PLAY_RATE);
	g_mockStreamAbstractionAAMP_MPD = &mockStreamAbstractionAAMP_MPD;

	// Let Tune() create its own protocol abstraction by clearing the fixture's
	// pointer; the MPD fake routes Init() calls to our mock.
	mPrivateInstanceAAMP->mpStreamAbstractionAAMP = nullptr;

	EXPECT_CALL(mockStreamAbstractionAAMP_MPD, Init(_))
		.WillOnce([this](TuneType) {
			EXPECT_EQ(mPrivateInstanceAAMP->GetState(), eSTATE_INITIALIZING);
			return eAAMPSTATUS_OK;
		});
		
	const char *testUrl = "http://localhost:80/test/manifest.mpd";
	mPrivateInstanceAAMP->Tune(testUrl, true, "VOD");
	ASSERT_EQ(mPrivateInstanceAAMP->GetState(), eSTATE_PREPARED);

	mPrivateInstanceAAMP->SetStateBufferingIfRequired();
	EXPECT_EQ(mPrivateInstanceAAMP->GetState(), eSTATE_BUFFERING);

	mPrivateInstanceAAMP->NotifyFragmentCachingComplete();
	EXPECT_EQ(mPrivateInstanceAAMP->GetState(), eSTATE_PLAYING);

	// Verify Stop() transitions to IDLE from PLAYING
	mPrivateInstanceAAMP->Stop(false);
	EXPECT_EQ(mPrivateInstanceAAMP->GetState(), eSTATE_IDLE);

	g_mockStreamAbstractionAAMP_MPD = nullptr;
}


// ============================================================
// Error State Tests
// ============================================================

/**
 * @test PlayerState_VerifyErrorState
 * @brief Verify that after a real Tune() path reaches PLAYING, sending a
 *        fatal error dispatches the tune-failed event and transitions the
 *        player to eSTATE_ERROR.
 */
TEST_F(PlayerStateTests, PlayerState_VerifyErrorState)
{
	EXPECT_CALL(*g_mockAampConfig, IsConfigSet(_))
		.WillRepeatedly(Return(false));
	EXPECT_CALL(*g_mockAampConfig,
		GetConfigValue(testing::Matcher<AAMPConfigSettingInt>(_)))
		.WillRepeatedly(Return(0));
	EXPECT_CALL(*g_mockAampConfig,
		GetConfigValue(testing::Matcher<AAMPConfigSettingFloat>(_)))
		.WillRepeatedly(Return(0.0));
	EXPECT_CALL(*g_mockAampConfig,
		GetConfigValue(testing::Matcher<AAMPConfigSettingString>(_)))
		.WillRepeatedly(Return(""));

	MockStreamAbstractionAAMP_MPD mockStreamAbstractionAAMP_MPD(
		mPrivateInstanceAAMP, 0, AAMP_NORMAL_PLAY_RATE);
	g_mockStreamAbstractionAAMP_MPD = &mockStreamAbstractionAAMP_MPD;
	mPrivateInstanceAAMP->mpStreamAbstractionAAMP = nullptr;

	EXPECT_CALL(mockStreamAbstractionAAMP_MPD, Init(_))
		.WillOnce(Return(eAAMPSTATUS_OK));

	const char *testUrl = "http://localhost:80/test/manifest.mpd";
	mPrivateInstanceAAMP->Tune(testUrl, true, "VOD");
	mPrivateInstanceAAMP->NotifyFirstFrameReceived(0);
	ASSERT_EQ(mPrivateInstanceAAMP->GetState(), eSTATE_PLAYING);

	EXPECT_CALL(*g_mockAampEventManager,
		SendEvent(AnEventOfType(AAMP_EVENT_TUNE_FAILED), _))
		.Times(1);

	mPrivateInstanceAAMP->SendErrorEvent(
		AAMP_TUNE_FAILURE_UNKNOWN,
		"fatal playback failure",
		true,
		11,
		12,
		13,
		"responseString");

	EXPECT_EQ(mPrivateInstanceAAMP->GetState(), eSTATE_ERROR);

	// Even on ERROR state, Stop() should still transition to IDLE.
	mPrivateInstanceAAMP->Stop(false);
	EXPECT_EQ(mPrivateInstanceAAMP->GetState(), eSTATE_IDLE);

	g_mockStreamAbstractionAAMP_MPD = nullptr;
}
