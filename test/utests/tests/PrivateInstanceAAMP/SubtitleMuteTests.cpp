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
#include <chrono>

#include "priv_aamp.h"

#include "AampConfig.h"
#include "MockAampGstPlayer.h"
#include "MockStreamAbstractionAAMP.h"
#include "MockAampStreamSinkManager.h"

using ::testing::_;
using ::testing::WithParamInterface;
using ::testing::An;
using ::testing::DoAll;
using ::testing::SetArgReferee;
using ::testing::Invoke;
using ::testing::Return;
using ::testing::NiceMock;
using ::testing::AnyNumber;

class SubtitleMuteTests : public testing::TestWithParam< std::pair<bool, bool> >
{
protected:

	PrivateInstanceAAMP *mPrivateInstanceAAMP{};

	void SetUp() override
	{

		if(gpGlobalConfig == nullptr)
		{
			gpGlobalConfig =  new AampConfig();
		}

		mPrivateInstanceAAMP = new PrivateInstanceAAMP(gpGlobalConfig);
		g_mockAampGstPlayer = new MockAAMPGstPlayer( mPrivateInstanceAAMP);
		g_mockStreamAbstractionAAMP = new MockStreamAbstractionAAMP(mPrivateInstanceAAMP);
		g_mockAampStreamSinkManager = new NiceMock<MockAampStreamSinkManager>();

		mPrivateInstanceAAMP->mpStreamAbstractionAAMP = g_mockStreamAbstractionAAMP;

		EXPECT_CALL(*g_mockAampStreamSinkManager, GetStreamSink(_)).WillRepeatedly(Return(g_mockAampGstPlayer));
	}

	void TearDown() override
	{
		delete mPrivateInstanceAAMP;
		mPrivateInstanceAAMP = nullptr;

		delete g_mockStreamAbstractionAAMP;
		g_mockStreamAbstractionAAMP = nullptr;

		delete g_mockAampGstPlayer;
		g_mockAampGstPlayer = nullptr;

		delete gpGlobalConfig;
		gpGlobalConfig = nullptr;

		delete g_mockAampStreamSinkManager;
		g_mockAampStreamSinkManager = nullptr;
	}

public:
	void CacheAndMuteSubtitles(bool currState, bool inputState)
	{
		mPrivateInstanceAAMP->subtitles_muted = currState;
		// Confirm operation works as expected
		// If input = unmute, subtitles should be set to currState (mute/un-mute)
		bool finalState = inputState ? inputState : currState;
		EXPECT_CALL(*g_mockStreamAbstractionAAMP, MuteSubtitles(finalState)).Times(1);
		EXPECT_CALL(*g_mockAampGstPlayer, SetSubtitleMute(finalState)).Times(1);

		mPrivateInstanceAAMP->AcquireStreamLock();
		mPrivateInstanceAAMP->CacheAndApplySubtitleMute(inputState);
		mPrivateInstanceAAMP->ReleaseStreamLock();

		// Confirm original state is preserved
		EXPECT_EQ(mPrivateInstanceAAMP->subtitles_muted, currState);
	}

	void TestSetSubtitleMute(bool videoMuted, bool subtitleMuteParam)
	{
		// Set the video_muted state
		mPrivateInstanceAAMP->video_muted = videoMuted;
		
		// Expected result should be logical OR of video_muted and subtitleMuteParam
		// This tests the core logic: sink->SetSubtitleMute(video_muted | muted);
		bool expectedMuteState = videoMuted | subtitleMuteParam;
		
		// Expect SetSubtitleMute to be called on the sink with the ORed result
		EXPECT_CALL(*g_mockAampGstPlayer, SetSubtitleMute(expectedMuteState)).Times(1);
		
		// Call the method under test
		mPrivateInstanceAAMP->SetSubtitleMute(subtitleMuteParam);
	}

};

TEST_P(SubtitleMuteTests, CacheSubtitleMuteTests)
{
	auto param {GetParam()};
	bool currState {param.first};
	bool inputState {param.second};
	CacheAndMuteSubtitles(currState, inputState);
}

TEST_P(SubtitleMuteTests, SetSubtitleMuteTests)
{
	auto param {GetParam()};
	bool subtitleMuted {param.first};
	bool videoMuted {param.second};
	TestSetSubtitleMute(videoMuted, subtitleMuted);
}

INSTANTIATE_TEST_SUITE_P(TestSubtitleMute,
						 SubtitleMuteTests,
						 testing::Values(
							std::pair<bool, bool>(true, true), // subtitle muted, video mute
							std::pair<bool, bool>(true, false),// subtitle muted, video unmute
							std::pair<bool, bool>(false, true),// subtitle unmuted, video mute
							std::pair<bool, bool>(false, false)// subtitle unmuted, video unmute
							));


