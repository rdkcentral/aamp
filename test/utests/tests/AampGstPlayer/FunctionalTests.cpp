/*
 * If not stated otherwise in this file or this component's license file the
 * following copyright and licenses apply:
 *
 * Copyright 2024 RDK Management
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

#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include "InterfacePlayerRDK.h"
#include "aampgstplayer.h"
#include "MockGStreamer.h"
#include "MockGLib.h"
#include "MockAampConfig.h"
#include "MockPrivateInstanceAAMP.h"
#include "MockAampUtils.h"
#include "MockInterfacePlayerRDK.h"

using ::testing::DoAll;
using ::testing::NiceMock;
using ::testing::Return;
using ::testing::SetArgReferee;
using ::testing::StrEq;
using ::testing::Eq;
using ::testing::_;
using ::testing::NotNull;
using ::testing::Matcher;

AampConfig *gpGlobalConfig{nullptr};

class AAMPGstPlayerTests : public ::testing::Test
{

protected:
	AAMPGstPlayer *mAAMPGstPlayer;
	PrivateInstanceAAMP *mPrivateInstanceAAMP;

	void SetUp() override
	{
		g_mockAampUtils = std::make_shared<NiceMock<MockAampUtils>>();
		g_mockGStreamer = new NiceMock<MockGStreamer>();
		g_mockGLib = std::make_shared<NiceMock<MockGLib>>();
		g_mockAampConfig = std::make_shared<NiceMock<MockAampConfig>>();
		g_mockPrivateInstanceAAMP = std::make_shared<NiceMock<MockPrivateInstanceAAMP>>();
		g_mockInterfacePlayerRDK = std::make_shared<NiceMock<MockInterfacePlayerRDK>>();
		mPrivateInstanceAAMP = new PrivateInstanceAAMP{};
	}

	void TearDown() override
	{
		g_mockInterfacePlayerRDK.reset();

		g_mockPrivateInstanceAAMP.reset();

		g_mockAampConfig.reset();

		g_mockGLib.reset();

		delete g_mockGStreamer;
		g_mockGStreamer = nullptr;

		g_mockAampUtils.reset();

		delete mPrivateInstanceAAMP;
		mPrivateInstanceAAMP = nullptr;
	}

public:
	void ConstructAMPGstPlayer()
	{
		std::string debug_level{"test_level"};

		EXPECT_CALL(*g_mockAampConfig, GetConfigValue(eAAMPConfig_GstDebugLevel))
					.WillOnce(Return(debug_level));

		mAAMPGstPlayer = new AAMPGstPlayer{mPrivateInstanceAAMP, nullptr};
	}

	void DestroyAMPGstPlayer()
	{
		delete mAAMPGstPlayer;
		mAAMPGstPlayer = nullptr;
	}
};

TEST_F(AAMPGstPlayerTests, Constructor)
{
	ConstructAMPGstPlayer();
	DestroyAMPGstPlayer();
}

TEST_F(AAMPGstPlayerTests, SetAudioVolume_NoSink)
{
	// Setup
	ConstructAMPGstPlayer();

	// Code under test

	// No sink, so no call to set volume or mute expected
	int volume = 0;
	EXPECT_CALL(*g_mockGLib, g_object_set(NotNull(), StrEq("mute"), Matcher<int>(_))).Times(0);
	EXPECT_CALL(*g_mockGLib, g_object_set(NotNull(), StrEq("volume"), Matcher<double>(_))).Times(0);
	mAAMPGstPlayer->SetAudioVolume(volume);

	volume = 100;
	mAAMPGstPlayer->SetAudioVolume(volume);

	volume = 50;
	mAAMPGstPlayer->SetAudioVolume(volume);

	//Tidy Up
	DestroyAMPGstPlayer();
}

TEST_F(AAMPGstPlayerTests, SetVideoMute_NoSink)
{
	// Setup
	ConstructAMPGstPlayer();

	bool mute = true;
	EXPECT_CALL(*g_mockGLib, g_object_set(NotNull(), StrEq("show-video-window"), Matcher<int>(_))).Times(0);
	mAAMPGstPlayer->SetVideoMute(mute);

	//Tidy Up
	DestroyAMPGstPlayer();
}

// ---------------------------------------------------------------------------
// Discontinuity — VPAAMP-1050: PTS-restamp path with mp4demux
// ---------------------------------------------------------------------------

/**
 * @test VPAAMP-1050: When CheckDiscontinuity() sets unblockDiscProcess=true
 *       (CompleteDiscontinuityDataDeliverForPTSRestamp path) and useMp4Demux
 *       is enabled, Discontinuity() must:
 *         (a) call aamp->CompleteDiscontinuityDataDeliverForPTSRestamp(type), and
 *         (b) return false so the inject-loop keeps running.
 *
 *       With mp4demux the PTS offset is absorbed by AampMp4Demuxer's restamp
 *       layer; stopping injection here would stall the pipeline permanently.
 */
TEST_F(AAMPGstPlayerTests, Discontinuity_Mp4Demux_PtsRestamp_ReturnsFalseAndCompletesRestamp)
{
	ConstructAMPGstPlayer();

	// Arrange: CheckDiscontinuity signals the PTS-restamp path.
	EXPECT_CALL(*g_mockInterfacePlayerRDK, CheckDiscontinuity(_, _, _, _, _))
		.WillOnce(DoAll(SetArgReferee<3>(true),   // unblockDiscProcess = true
		                SetArgReferee<4>(false),  // shouldHaltBuffering = false
		                Return(true)));

	// useMp4Demux enabled → the new early-return branch must fire.
	EXPECT_CALL(*g_mockAampConfig, IsConfigSet(eAAMPConfig_UseMp4Demux))
		.WillRepeatedly(Return(true));

	// The restamp latch must be released exactly once.
	EXPECT_CALL(*g_mockPrivateInstanceAAMP,
	            CompleteDiscontinuityDataDeliverForPTSRestamp(eMEDIATYPE_VIDEO))
		.Times(1);

	// UnblockWaitForDiscontinuityProcessToComplete must NOT be called on
	// this path — only CompleteDiscontinuityDataDeliverForPTSRestamp is.
	EXPECT_CALL(*g_mockPrivateInstanceAAMP,
	            UnblockWaitForDiscontinuityProcessToComplete())
		.Times(0);

	// Act
	bool result = mAAMPGstPlayer->Discontinuity(eMEDIATYPE_VIDEO);

	// Assert: injection must continue — false means "do not stop".
	EXPECT_FALSE(result);

	DestroyAMPGstPlayer();
}

// ---------------------------------------------------------------------------
// Discontinuity — VPAAMP-1051: no-flag (period-boundary) path with mp4demux
// ---------------------------------------------------------------------------

/**
 * @test VPAAMP-1051: When CheckDiscontinuity() returns true but sets neither
 *       output flag (elementary-stream period boundary treated as a no-op by
 *       the player library) and useMp4Demux is enabled, Discontinuity() must:
 *         (a) call aamp->UnblockWaitForDiscontinuityProcessToComplete() so the
 *             FetcherLoop is not left waiting on a latch nobody will release, and
 *         (b) return false so the inject-loop keeps running.
 *
 *       Without this fix downloads stop, the buffer starves, and position freezes.
 */
TEST_F(AAMPGstPlayerTests, Discontinuity_Mp4Demux_NeitherFlag_UnblocksAndReturnsFalse)
{
	ConstructAMPGstPlayer();

	// Arrange: CheckDiscontinuity returns true but sets neither output flag —
	// the period-boundary no-op case for elementary streams with mp4demux.
	EXPECT_CALL(*g_mockInterfacePlayerRDK, CheckDiscontinuity(_, _, _, _, _))
		.WillOnce(DoAll(SetArgReferee<3>(false),  // unblockDiscProcess = false
		                SetArgReferee<4>(false),  // shouldHaltBuffering = false
		                Return(true)));

	// useMp4Demux enabled → the new else-if branch must fire.
	EXPECT_CALL(*g_mockAampConfig, IsConfigSet(eAAMPConfig_UseMp4Demux))
		.WillRepeatedly(Return(true));

	// The FetcherLoop latch must be released exactly once.
	EXPECT_CALL(*g_mockPrivateInstanceAAMP,
	            UnblockWaitForDiscontinuityProcessToComplete())
		.Times(1);

	// CompleteDiscontinuityDataDeliverForPTSRestamp must NOT be called —
	// neither flag was set.
	EXPECT_CALL(*g_mockPrivateInstanceAAMP,
	            CompleteDiscontinuityDataDeliverForPTSRestamp(_))
		.Times(0);

	// Act
	bool result = mAAMPGstPlayer->Discontinuity(eMEDIATYPE_VIDEO);

	// Assert: injection must continue — false means "do not stop".
	EXPECT_FALSE(result);

	DestroyAMPGstPlayer();
}
