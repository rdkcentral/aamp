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
 * @brief Player-facing state tests for PlayerInstanceAAMP.
 *
 * This test target is backed by the unit-test fake implementation of
 * PrivateInstanceAAMP. The suite therefore drives the public
 * PlayerInstanceAAMP APIs and verifies state transitions through the
 * player's GetState() API while tracking underlying private-state updates
 * via MockPrivateInstanceAAMP.
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
 * @class PlayerInstanceAAMPStateTests
 * @brief Fixture for player-facing state transition tests.
 */
class PlayerInstanceAAMPStateTests : public ::testing::Test
{
protected:
	PlayerInstanceAAMP *mPlayerInstanceAAMP{};
	PrivateInstanceAAMP *mPrivateInstanceAAMP{};
	AAMPPlayerState mCurrentState{eSTATE_RELEASED};

	void SetUp() override
	{
		if (gpGlobalConfig == nullptr)
		{
			gpGlobalConfig = new AampConfig();
		}

		mPrivateInstanceAAMP = new PrivateInstanceAAMP(gpGlobalConfig);
		mPlayerInstanceAAMP = new PlayerInstanceAAMP();
		mPlayerInstanceAAMP->aamp = mPrivateInstanceAAMP;

		g_mockAampConfig = new NiceMock<MockAampConfig>();
		g_mockPrivateInstanceAAMP = new NiceMock<MockPrivateInstanceAAMP>();
		g_mockAampGstPlayer =
			new NiceMock<MockAAMPGstPlayer>(mPrivateInstanceAAMP);
		g_mockAampStreamSinkManager =
			new NiceMock<MockAampStreamSinkManager>();
		g_mockStreamAbstractionAAMP =
			new NiceMock<MockStreamAbstractionAAMP>(mPrivateInstanceAAMP);

		mPrivateInstanceAAMP->mpStreamAbstractionAAMP =
			g_mockStreamAbstractionAAMP;

		EXPECT_CALL(*g_mockAampStreamSinkManager, GetStreamSink(_))
			.WillRepeatedly(Return(g_mockAampGstPlayer));

		ON_CALL(*g_mockPrivateInstanceAAMP, GetState())
			.WillByDefault([this]() { return mCurrentState; });
		ON_CALL(*g_mockPrivateInstanceAAMP, SetState(_, _))
			.WillByDefault([this](AAMPPlayerState state, bool) {
				mCurrentState = state;
			});
	}

	void TearDown() override
	{
		delete mPlayerInstanceAAMP;
		mPlayerInstanceAAMP = nullptr;

		delete mPrivateInstanceAAMP;
		mPrivateInstanceAAMP = nullptr;

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
	const char *mainManifestUrl = "https://example.com";
	bool autoPlay = true;
	const char *contentType = "video";
	bool bFirstAttempt = true;
	bool bFinalAttempt = false;
	const char *traceUUID = "12345";
	bool audioDecoderStreamSync = true;
	const char *refreshManifestUrl = "https://example.comm";
	int mpdStitchingMode = 10;
	static const char* manifestData = R"(<?xml version="1.0" encoding="UTF-8"?><MPD xmlns:xsi="http://www.w3.org/2001/XMLSchema-instance" xmlns="urn:mpeg:dash:schema:mpd:2011" xmlns:scte35="http://www.scte.org/schemas/35/2014SCTE35.xsd" xsi:schemaLocation="urn:mpeg:dash:schema:mpd:2011 DASH-MPD.xsd" profiles="urn:mpeg:dash:profile:isoff-live:2011" type="static" minBufferTime="PT5.000S" maxSegmentDuration="PT2.005S" availabilityStartTime="2016-01-20T21:10:02Z" mediaPresentationDuration="PT193.680S"><Period id="period0"><AdaptationSet mimeType="video/mp4" segmentAlignment="true" startWithSAP="1" maxWidth="1920" maxHeight="1080" maxFrameRate="30000/1001" par="1:1"><SegmentTemplate timescale="90000" initialization="$RepresentationID$-Header.m4s" media="$RepresentationID$-270146-i-$Number$.m4s" startNumber="1" duration="179704" presentationTimeOffset="0"/><Representation id="v1_257" bandwidth="1200000" codecs="avc1.4D401E" width="768" height="432" frameRate="30000/1001" sar="1:1" scanType="progressive"/><Representation id="v2_257" bandwidth="1850000" codecs="avc1.4D401E" width="1024" height="576" frameRate="30000/1001" sar="1:1" scanType="progressive"/><Representation id="v3_257" bandwidth="2850000" codecs="avc1.4D401E" width="1280" height="720" frameRate="30000/1001" sar="1:1" scanType="progressive"/><Representation id="v4_257" bandwidth="200000" codecs="avc1.4D401E" width="320" height="180" frameRate="30000/1001" sar="1:1" scanType="progressive"/><Representation id="v5_257" bandwidth="300000" codecs="avc1.4D401E" width="320" height="180" frameRate="30000/1001" sar="1:1" scanType="progressive"/><Representation id="v6_257" bandwidth="4300000" codecs="avc1.4D401E" width="1280" height="720" frameRate="30000/1001" sar="1:1" scanType="progressive"/><Representation id="v7_257" bandwidth="5300000" codecs="avc1.4D401E" width="1920" height="1080" frameRate="30000/1001" sar="1:1" scanType="progressive"/><Representation id="v8_257" bandwidth="480000" codecs="avc1.4D401E" width="512" height="288" frameRate="30000/1001" sar="1:1" scanType="progressive"/><Representation id="v9_257" bandwidth="750000" codecs="avc1.4D401E" width="640" height="360" frameRate="30000/1001" sar="1:1" scanType="progressive"/></AdaptationSet><AdaptationSet mimeType="audio/mp4" segmentAlignment="true" startWithSAP="1" lang="qaa"><SegmentTemplate timescale="90000" initialization="$RepresentationID$-Header.m4s" media="$RepresentationID$-270146-i-$Number$.m4s" startNumber="1" duration="179704" presentationTimeOffset="0"/><Representation id="v4_258" bandwidth="130800" codecs="mp4a.40.2" audioSamplingRate="48000"><AudioChannelConfiguration schemeIdUri="urn:mpeg:dash:23003:3:audio_channel_configuration:2011" value="2"/></Representation></AdaptationSet></Period></MPD>)";

	std::string session_id {"0259343c-cffc-4659-bcd8-97f9dd36f6b1"};

	mCurrentState = eSTATE_PLAYING;

	mPlayerInstanceAAMP->Tune(mainManifestUrl,autoPlay,contentType,bFirstAttempt,bFinalAttempt,traceUUID,audioDecoderStreamSync,refreshManifestUrl,mpdStitchingMode,session_id,manifestData);

	ASSERT_EQ(mPlayerInstanceAAMP->GetState(), eSTATE_PLAYING);

	mPlayerInstanceAAMP->Seek(10.0);
	EXPECT_EQ(mPlayerInstanceAAMP->GetState(), eSTATE_SEEKING);

	mPrivateInstanceAAMP->SetState(eSTATE_PLAYING, false);
	EXPECT_EQ(mPlayerInstanceAAMP->GetState(), eSTATE_PLAYING);
}