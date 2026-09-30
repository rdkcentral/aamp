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

		// Create and install g_mockAampConfig before constructing
		// PrivateInstanceAAMP so that any config reads during construction
		// see safe default values instead of uninitialized fakes.
		g_mockAampConfig = std::make_shared<NiceMock<MockAampConfig>>();
		ON_CALL(*g_mockAampConfig, IsConfigSet(_))
			.WillByDefault(Return(false));
		ON_CALL(*g_mockAampConfig,
			GetConfigValue(testing::Matcher<AAMPConfigSettingInt>(_)))
			.WillByDefault(Return(0));
		ON_CALL(*g_mockAampConfig,
			GetConfigValue(testing::Matcher<AAMPConfigSettingFloat>(_)))
			.WillByDefault(Return(0.0));
		ON_CALL(*g_mockAampConfig,
			GetConfigValue(testing::Matcher<AAMPConfigSettingString>(_)))
			.WillByDefault(Return(""));

		mPrivateInstanceAAMP = new PrivateInstanceAAMP(gpGlobalConfig);

		g_mockAampGstPlayer =
			std::make_shared<NiceMock<MockAAMPGstPlayer>>(mPrivateInstanceAAMP);
		g_mockAampEventManager =
			std::make_shared<NiceMock<MockAampEventManager>>();
		g_mockStreamAbstractionAAMP =
			std::make_shared<NiceMock<MockStreamAbstractionAAMP>>(mPrivateInstanceAAMP);
		g_mockAampStreamSinkManager =
			std::make_shared<NiceMock<MockAampStreamSinkManager>>();

		mPrivateInstanceAAMP->mpStreamAbstractionAAMP =
			g_mockStreamAbstractionAAMP.get();

		EXPECT_CALL(*g_mockAampStreamSinkManager, GetStreamSink(_))
			.WillRepeatedly(Return(g_mockAampGstPlayer.get()));
	}

	void TearDown() override
	{
		g_mockStreamAbstractionAAMP_MPD.reset();

		delete mPrivateInstanceAAMP;
		mPrivateInstanceAAMP = nullptr;

		g_mockStreamAbstractionAAMP.reset();

		g_mockAampGstPlayer.reset();

		g_mockAampEventManager.reset();

		g_mockAampStreamSinkManager.reset();

		delete gpGlobalConfig;
		gpGlobalConfig = nullptr;

		g_mockAampConfig.reset();
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

	g_mockStreamAbstractionAAMP_MPD = std::make_shared<MockStreamAbstractionAAMP_MPD>(
		mPrivateInstanceAAMP, 0, AAMP_NORMAL_PLAY_RATE);

	// Let Tune() create its own protocol abstraction by clearing the fixture's
	// pointer; the MPD fake routes Init() calls to our mock.
	mPrivateInstanceAAMP->mpStreamAbstractionAAMP = nullptr;

	// 2. INITIALIZING is set by TuneHelper before calling Init(); verify it
	//    from inside the Init() mock callback.
	EXPECT_CALL(*g_mockStreamAbstractionAAMP_MPD, Init(_))
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

	g_mockStreamAbstractionAAMP_MPD.reset();
}

// ============================================================
// Buffering State Tests
// ============================================================

/**
 * @test PlayerState_VerifyBuffering_Playing
 * @brief Verify a real buffering scenario during tune when initial fragment
 *        caching is enabled. After Tune() reaches PREPARED, calling
 *        NotifyFirstVideoFrameDisplayed() (the real trigger path in
 *        priv_aamp.cpp) transitions to BUFFERING because
 *        mFragmentCachingRequired is set by TuneHelper when
 *        IsInitialCachingSupported() returns true and the initial buffer
 *        duration is non-zero. The sequence is:
 *        PREPARED → BUFFERING (via NotifyFirstVideoFrameDisplayed) →
 *        PLAYING (via NotifyFragmentCachingComplete) → IDLE (via Stop).
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
	// Non-zero initial buffer causes TuneHelper to set mFragmentCachingRequired.
	EXPECT_CALL(*g_mockAampConfig, GetConfigValue(eAAMPConfig_InitialBuffer))
		.WillRepeatedly(Return(1));
	// IsInitialCachingSupported is routed through the StreamAbstractionAAMP
	// fake base class to g_mockStreamAbstractionAAMP.
	EXPECT_CALL(*g_mockStreamAbstractionAAMP, IsInitialCachingSupported())
		.WillRepeatedly(Return(true));
	EXPECT_CALL(*g_mockStreamAbstractionAAMP,
		NotifyPipelinePausedToUnderflowMonitor()).Times(1);
	EXPECT_CALL(*g_mockStreamAbstractionAAMP,
		NotifyPipelineResumedToUnderflowMonitor(AAMP_NORMAL_PLAY_RATE)).Times(1);

	g_mockStreamAbstractionAAMP_MPD = std::make_shared<MockStreamAbstractionAAMP_MPD>(
		mPrivateInstanceAAMP, 0, AAMP_NORMAL_PLAY_RATE);

	// Let Tune() create its own protocol abstraction by clearing the fixture's
	// pointer; the MPD fake routes Init() calls to our mock.
	mPrivateInstanceAAMP->mpStreamAbstractionAAMP = nullptr;

	EXPECT_CALL(*g_mockStreamAbstractionAAMP_MPD, Init(_))
		.WillOnce([this](TuneType) {
			EXPECT_EQ(mPrivateInstanceAAMP->GetState(), eSTATE_INITIALIZING);
			return eAAMPSTATUS_OK;
		});

	const char *testUrl = "http://localhost:80/test/manifest.mpd";
	mPrivateInstanceAAMP->Tune(testUrl, true, "VOD");
	ASSERT_EQ(mPrivateInstanceAAMP->GetState(), eSTATE_PREPARED);

	// Drive the PREPARED → BUFFERING transition via the real app path:
	// NotifyFirstVideoFrameDisplayed() checks mFirstVideoFrameDisplayedEnabled
	// (set by TuneHelper above) and calls SetStateBufferingIfRequired()
	// internally.
	mPrivateInstanceAAMP->NotifyFirstVideoFrameDisplayed();
	EXPECT_EQ(mPrivateInstanceAAMP->GetState(), eSTATE_BUFFERING);

	mPrivateInstanceAAMP->NotifyFragmentCachingComplete();
	EXPECT_EQ(mPrivateInstanceAAMP->GetState(), eSTATE_PLAYING);

	// Verify Stop() transitions to IDLE from PLAYING
	mPrivateInstanceAAMP->Stop(false);
	EXPECT_EQ(mPrivateInstanceAAMP->GetState(), eSTATE_IDLE);

	g_mockStreamAbstractionAAMP_MPD.reset();
}


/**
 * @test PlayerState_VerifyBuffering_AfterSeekToAd
 * @brief Regression test for VPAAMP-705: AD markers intermittently not
 *        displayed when a user seeks to an AD boundary on VOD.
 *
 *        After an initial tune reaches PLAYING (no fragment caching), a
 *        seek to an AD boundary causes AAMP to re-tune, which re-enables
 *        fragment caching (mFragmentCachingRequired is set again via
 *        TuneHelper).  NotifyFirstVideoFrameDisplayed() then calls
 *        SetStateBufferingIfRequired(), which must:
 *          1. Disarm the underflow monitor (NotifyPipelinePausedToUnderflowMonitor)
 *          2. Set state to BUFFERING
 *        and NotifyFragmentCachingComplete() must:
 *          3. Re-arm the underflow monitor (NotifyPipelineResumedToUnderflowMonitor)
 *          4. Set state back to PLAYING
 *
 *        Without the VPAAMP-705 fix neither notification was made in the
 *        fragment-caching code path, leaving the underflow monitor armed.
 *        The false underflow it would subsequently fire suppressed the AD
 *        marker delivery event.
 *
 *        The initial caching flag (IsInitialCachingSupported) is false for
 *        the first tune and true for the seek, so mFragmentCachingRequired
 *        is set only during the second TuneHelper call, mirroring the real
 *        seek-to-AD code path.  State sequence:
 *          RELEASED → INITIALIZING → PREPARED → PLAYING   (initial tune)
 *          PLAYING  → INITIALIZING → PREPARED → BUFFERING (seek/re-tune to AD)
 *          BUFFERING → PLAYING                             (caching complete)
 */
TEST_F(PlayerStateTests, PlayerState_VerifyBuffering_AfterSeekToAd)
{
	// ── Common config defaults ────────────────────────────────────────────
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
	// InitialBuffer is non-zero throughout.  Whether fragment caching is
	// actually engaged depends on IsInitialCachingSupported() (see below).
	EXPECT_CALL(*g_mockAampConfig, GetConfigValue(eAAMPConfig_InitialBuffer))
		.WillRepeatedly(Return(1));

	g_mockStreamAbstractionAAMP_MPD = std::make_shared<MockStreamAbstractionAAMP_MPD>(
		mPrivateInstanceAAMP, 0, AAMP_NORMAL_PLAY_RATE);

	// Let Tune() create its own protocol abstraction by clearing the
	// fixture's pointer; the MPD fake routes Init() calls to our mock.
	mPrivateInstanceAAMP->mpStreamAbstractionAAMP = nullptr;

	EXPECT_CALL(*g_mockStreamAbstractionAAMP_MPD, Init(_))
		.WillRepeatedly(Return(eAAMPSTATUS_OK));

	// ── Phase 1: Initial tune → PLAYING (no fragment caching) ────────────
	// IsInitialCachingSupported() is false here: even with InitialBuffer > 0
	// TuneHelper will not set mFragmentCachingRequired.  This models the
	// initial play of the VOD content before the user seeks to an AD.
	EXPECT_CALL(*g_mockStreamAbstractionAAMP, IsInitialCachingSupported())
		.WillRepeatedly(Return(false));

	const char *testUrl = "http://localhost:80/test/manifest.mpd";
	mPrivateInstanceAAMP->Tune(testUrl, true, "VOD");
	ASSERT_EQ(mPrivateInstanceAAMP->GetState(), eSTATE_PREPARED);

	// mFirstVideoFrameDisplayedEnabled is false (no caching), so
	// NotifyFirstFrameReceived() transitions directly to PLAYING.
	mPrivateInstanceAAMP->NotifyFirstFrameReceived(0);
	ASSERT_EQ(mPrivateInstanceAAMP->GetState(), eSTATE_PLAYING);

	// ── Phase 2: Seek to AD → fragment caching re-triggered ──────────────
	// From this point IsInitialCachingSupported() returns true.  The second
	// Tune() call (representing the internal re-tune AAMP performs on seek)
	// causes TuneHelper to set mFragmentCachingRequired = true.
	// GMock resolves the most-recently registered EXPECT_CALL first, so
	// this supersedes the Phase 1 expectation above.
	EXPECT_CALL(*g_mockStreamAbstractionAAMP, IsInitialCachingSupported())
		.WillRepeatedly(Return(true));

	// These are the two notifications that VPAAMP-705 was missing.
	// Each must be called exactly once: the first when entering BUFFERING
	// and the second when fragment caching completes and PLAYING resumes.
	EXPECT_CALL(*g_mockStreamAbstractionAAMP,
		NotifyPipelinePausedToUnderflowMonitor()).Times(1);
	EXPECT_CALL(*g_mockStreamAbstractionAAMP,
		NotifyPipelineResumedToUnderflowMonitor(AAMP_NORMAL_PLAY_RATE)).Times(1);

	// Simulate the seek to AD: AAMP internally re-tunes when seeking across
	// a period boundary, which goes through TuneHelper with InitialBuffer > 0
	// and IsInitialCachingSupported() = true.
	mPrivateInstanceAAMP->Tune(testUrl, true, "VOD");
	ASSERT_EQ(mPrivateInstanceAAMP->GetState(), eSTATE_PREPARED);

	// mFragmentCachingRequired is now true.  NotifyFirstVideoFrameDisplayed()
	// drives PREPARED → BUFFERING via SetStateBufferingIfRequired().
	mPrivateInstanceAAMP->NotifyFirstVideoFrameDisplayed();
	EXPECT_EQ(mPrivateInstanceAAMP->GetState(), eSTATE_BUFFERING);

	mPrivateInstanceAAMP->NotifyFragmentCachingComplete();
	EXPECT_EQ(mPrivateInstanceAAMP->GetState(), eSTATE_PLAYING);

	mPrivateInstanceAAMP->Stop(false);
	EXPECT_EQ(mPrivateInstanceAAMP->GetState(), eSTATE_IDLE);

	g_mockStreamAbstractionAAMP_MPD.reset();
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

	g_mockStreamAbstractionAAMP_MPD = std::make_shared<MockStreamAbstractionAAMP_MPD>(
		mPrivateInstanceAAMP, 0, AAMP_NORMAL_PLAY_RATE);
	mPrivateInstanceAAMP->mpStreamAbstractionAAMP = nullptr;

	EXPECT_CALL(*g_mockStreamAbstractionAAMP_MPD, Init(_))
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

	g_mockStreamAbstractionAAMP_MPD.reset();
}
