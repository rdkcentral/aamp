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
#include <condition_variable>
#include <future>
#include <mutex>
#include <thread>

#include "priv_aamp.h"

#include "AampConfig.h"
#include "MockAampConfig.h"
#include "MockAampGstPlayer.h"
#include "MockAampScheduler.h"
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

		// Set up mock config to control eAAMPConfig_GstSubtecEnabled
		g_mockAampConfig = std::make_shared<NiceMock<MockAampConfig>>();

		// Return false for all config settings by default using ON_CALL
		ON_CALL(*g_mockAampConfig, IsConfigSet(_))
			.WillByDefault(Return(false));

		// Specifically return true for eAAMPConfig_GstSubtecEnabled using ON_CALL
		ON_CALL(*g_mockAampConfig, IsConfigSet(eAAMPConfig_GstSubtecEnabled))
			.WillByDefault(Return(true));

		mPrivateInstanceAAMP = new PrivateInstanceAAMP(gpGlobalConfig);
		// Test out-of-band subtitles
		mPrivateInstanceAAMP->mIsInbandCC = false;
		g_mockAampGstPlayer = std::make_shared<MockAAMPGstPlayer>( mPrivateInstanceAAMP);
		g_mockStreamAbstractionAAMP = std::make_shared<MockStreamAbstractionAAMP>(mPrivateInstanceAAMP);
		g_mockAampStreamSinkManager = std::make_shared<NiceMock<MockAampStreamSinkManager>>();

		mPrivateInstanceAAMP->mpStreamAbstractionAAMP = g_mockStreamAbstractionAAMP.get();

		EXPECT_CALL(*g_mockAampStreamSinkManager, GetStreamSink(_)).WillRepeatedly(Return(g_mockAampGstPlayer.get()));
	}

	void TearDown() override
	{
		delete mPrivateInstanceAAMP;
		mPrivateInstanceAAMP = nullptr;

		g_mockStreamAbstractionAAMP.reset();

		g_mockAampGstPlayer.reset();

		g_mockAampConfig.reset();

		delete gpGlobalConfig;
		gpGlobalConfig = nullptr;

		g_mockAampStreamSinkManager.reset();
	}

public:
	void CacheAndMuteSubtitles(bool currState, bool inputState)
	{
		AAMPLOG_INFO("currState=%d, inputState=%d", currState, inputState);
		mPrivateInstanceAAMP->subtitles_muted = currState;
		// Confirm operation works as expected
		// If input = unmute, subtitles should be set to currState (mute/un-mute)
		bool finalState = inputState ? inputState : currState;
		EXPECT_CALL(*g_mockStreamAbstractionAAMP, MuteSubtitles(finalState)).Times(1);
		EXPECT_CALL(*g_mockAampGstPlayer, SetSubtitleMute(finalState)).Times(1);

		mPrivateInstanceAAMP->SetVideoMute(inputState);

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

TEST_F(SubtitleMuteTests, SetCCStatus_StreamLockHeld_DefersLatestVisibility)
{
	mPrivateInstanceAAMP->SetState(eSTATE_INITIALIZING, false);
	mPrivateInstanceAAMP->mpStreamAbstractionAAMP = nullptr;

	std::mutex completionMutex;
	std::condition_variable completionCondition;
	bool requestStarted = false;
	bool requestCompleted = false;

	std::unique_lock<std::recursive_mutex> streamLock(
		mPrivateInstanceAAMP->GetStreamLock());
	std::thread requester([&]() {
		{
			std::lock_guard<std::mutex> lock(completionMutex);
			requestStarted = true;
		}
		completionCondition.notify_one();

		mPrivateInstanceAAMP->SetCCStatus(true);
		mPrivateInstanceAAMP->SetCCStatus(false);

		{
			std::lock_guard<std::mutex> lock(completionMutex);
			requestCompleted = true;
		}
		completionCondition.notify_one();
	});

	std::unique_lock<std::mutex> completionLock(completionMutex);
	const bool started = completionCondition.wait_for(
		completionLock, std::chrono::seconds(1), [&]() { return requestStarted; });
	const bool completedBeforeUnlock = started && completionCondition.wait_for(
		completionLock, std::chrono::seconds(1), [&]() { return requestCompleted; });
	completionLock.unlock();
	streamLock.unlock();
	requester.join();

	ASSERT_TRUE(started);
	EXPECT_TRUE(completedBeforeUnlock);
	EXPECT_FALSE(mPrivateInstanceAAMP->GetCCStatus());
	EXPECT_TRUE(mPrivateInstanceAAMP->mApplyCachedCCStatus.load());
}

/**
 * @test SetCCStatus_DuringTune_DeferredStatusAppliedByTune
 * @brief SetCCStatus() called while Tune() holds mStreamLock at INITIALIZING is
 *        cached, then applied to the new stream by the tune once it resumes.
 */
TEST_F(SubtitleMuteTests, SetCCStatus_DuringTune_DeferredStatusAppliedByTune)
{
	// Tune() creates its own stream abstraction and deletes any existing one.
	mPrivateInstanceAAMP->mpStreamAbstractionAAMP = nullptr;

	std::promise<void> tuneInitializing;
	std::promise<void> resumeTune;
	auto resumeTuneFuture = resumeTune.get_future().share();

	// TuneHelper() reports INITIALIZING to the scheduler with mStreamLock held.
	g_mockAampScheduler = std::make_shared<NiceMock<MockAampScheduler>>();
	ON_CALL(*g_mockAampScheduler, SetState(eSTATE_INITIALIZING))
		.WillByDefault([&](AAMPPlayerState)
		{
			tuneInitializing.set_value();
			resumeTuneFuture.wait_for(std::chrono::seconds(5));
		});

	// Subtitles start muted, so only the deferred request can unmute them.
	EXPECT_CALL(*g_mockStreamAbstractionAAMP, MuteSubtitles(false)).Times(1);
	EXPECT_CALL(*g_mockAampGstPlayer, SetSubtitleMute(false)).Times(1);

	std::thread tuneThread([this]()
	{
		mPrivateInstanceAAMP->Tune("http://localhost:80/manifest.mpd", false, "VOD");
	});

	const bool tuneReachedInitializing =
		tuneInitializing.get_future().wait_for(std::chrono::seconds(5)) == std::future_status::ready;
	bool cachedWhileTuning = false;
	if (tuneReachedInitializing)
	{
		mPrivateInstanceAAMP->SetCCStatus(true);
		cachedWhileTuning = mPrivateInstanceAAMP->mApplyCachedCCStatus.load();
	}
	resumeTune.set_value();
	tuneThread.join();

	SAFE_DELETE(mPrivateInstanceAAMP->mpStreamAbstractionAAMP);
	g_mockAampScheduler.reset();

	EXPECT_TRUE(tuneReachedInitializing);
	EXPECT_TRUE(cachedWhileTuning);
	EXPECT_FALSE(mPrivateInstanceAAMP->mApplyCachedCCStatus.load());
	EXPECT_TRUE(mPrivateInstanceAAMP->GetCCStatus());
}

INSTANTIATE_TEST_SUITE_P(TestSubtitleMute,
						 SubtitleMuteTests,
						 testing::Values(
							std::pair<bool, bool>(true, true), // subtitle muted, video mute
							std::pair<bool, bool>(true, false),// subtitle muted, video unmute
							std::pair<bool, bool>(false, true),// subtitle unmuted, video mute
							std::pair<bool, bool>(false, false)// subtitle unmuted, video unmute
							));


