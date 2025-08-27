/*
* If not stated otherwise in this file or this component's license file the
* following copyright and licenses apply:
*
* Copyright 2022 RDK Management
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

#include "main_aamp.h"

#include "AampConfig.h"
#include "AampScheduler.h"
#include "AampLogManager.h"
#include "MockAampConfig.h"
#include "MockAampGstPlayer.h"
#include "MockAampScheduler.h"
#include "MockAampEventManager.h"
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


#define WAIT_FOR_SCHEDUE_TASK_POLL_PERIOD_MS    (50)

class PauseAtTests : public ::testing::Test
{
protected:

    PlayerInstanceAAMP *mPlayerInstanceAAMP{};

    AampScheduler mScheduler{};

    AsyncTask mScheduleAsyncTask{};
    void * mScheduleAsyncData{};
    int mScheduleAsyncId{};
    std::string mScheduleAsyncTaskName{};

    bool mScheduleTaskCalled{};
    std::mutex mScheduleTaskCalledMutex{};
    std::condition_variable mScheduleTaskCalledCV{};

    void SetUp() override
    {
        mScheduleTaskCalled = false;

        if(gpGlobalConfig == nullptr)
        {
            gpGlobalConfig =  new AampConfig();
        }

        mPlayerInstanceAAMP = new PlayerInstanceAAMP();

        g_mockAampConfig = new MockAampConfig();

        g_mockAampScheduler = new MockAampScheduler();
        g_mockAampGstPlayer = new MockAAMPGstPlayer( mPlayerInstanceAAMP);
        g_mockAampEventManager = new MockAampEventManager();
        g_mockStreamAbstractionAAMP = new MockStreamAbstractionAAMP( mPlayerInstanceAAMP);
		g_mockAampStreamSinkManager = new NiceMock<MockAampStreamSinkManager>();

        mPlayerInstanceAAMP->SetScheduler(&mScheduler);
        mPlayerInstanceAAMP->mpStreamAbstractionAAMP = g_mockStreamAbstractionAAMP;

        // Called in destructor of PlayerInstanceAAMP
        // Done here because setting up the EXPECT_CALL in TearDown, conflicted with the mock
        // being called in the PausePosition thread.
        EXPECT_CALL(*g_mockAampConfig, IsConfigSet(eAAMPConfig_EnableCurlStore)).WillRepeatedly(Return(false));

   		EXPECT_CALL(*g_mockAampStreamSinkManager, GetStreamSink(_)).WillRepeatedly(Return(g_mockAampGstPlayer));
    }

    void TearDown() override
    {
        mPlayerInstanceAAMP->mpStreamAbstractionAAMP = nullptr;
        delete g_mockStreamAbstractionAAMP;
        g_mockStreamAbstractionAAMP = nullptr;

        delete mPlayerInstanceAAMP;
        mPlayerInstanceAAMP = nullptr;

//        delete g_mockStreamAbstractionAAMP;
//        g_mockStreamAbstractionAAMP = nullptr;

        delete g_mockAampEventManager;
        g_mockAampEventManager = nullptr;

        delete g_mockAampGstPlayer;
        g_mockAampGstPlayer = nullptr;

        delete g_mockAampScheduler;
        g_mockAampScheduler = nullptr;

//        delete gpGlobalConfig;
//        gpGlobalConfig = nullptr;

        delete g_mockAampConfig;
        g_mockAampConfig = nullptr;

		delete g_mockAampStreamSinkManager;
		g_mockAampStreamSinkManager = nullptr;
    }

public:

    int ScheduleTask(AsyncTaskObj obj)
    {
        mScheduleAsyncTask = obj.mTask;
        mScheduleAsyncData = obj.mData;
        mScheduleAsyncId = obj.mId;
        mScheduleAsyncTaskName = obj.mTaskName;

        std::unique_lock<std::mutex> lock(mScheduleTaskCalledMutex);
        mScheduleTaskCalled = true;
        mScheduleTaskCalledCV.notify_one();

        return 1;
    }

    // Wait for either:
    //    - ScheduleTask to be called
    //    - mPausePositionMilliseconds to be set to -1 (i.e. canceled)
    //    - timeout to avoid lockup of test
    // Returns true if ScheduleTask was called, otherwise false
    bool WaitForScheduleTask(int timeoutMs)
    {
        std::unique_lock<std::mutex> lock(mScheduleTaskCalledMutex);
        std::chrono::time_point<std::chrono::steady_clock> startTime = std::chrono::steady_clock::now();
        std::chrono::time_point<std::chrono::steady_clock> currentTime = startTime;

        while ((false == mScheduleTaskCalled) &&
               (mPlayerInstanceAAMP->mPausePositionMilliseconds != AAMP_PAUSE_POSITION_INVALID_POSITION) &&
               (std::chrono::duration_cast<std::chrono::milliseconds>(currentTime - startTime).count() < timeoutMs))
        {
            mScheduleTaskCalledCV.wait_for(lock, std::chrono::milliseconds(WAIT_FOR_SCHEDUE_TASK_POLL_PERIOD_MS));
            currentTime = std::chrono::steady_clock::now();
        }

        return mScheduleTaskCalled;
    }
};

// Testing calling StartPausePositionMonitoring when pipeline paused
// Don't expect ScheduleTask to be called to execute pause
TEST_F(PauseAtTests, StartPausePositionMonitoring_PipelinePaused)
{
    long long pauseAtMilliseconds = 100.0 * 1000;

    mPlayerInstanceAAMP->rate = AAMP_NORMAL_PLAY_RATE;
    mPlayerInstanceAAMP->pipeline_paused = true;
    mPlayerInstanceAAMP->trickStartUTCMS = 0;
    mPlayerInstanceAAMP->mAudioOnlyPb = false;
    mPlayerInstanceAAMP->durationSeconds = 3600;

    EXPECT_CALL(*g_mockAampConfig, GetConfigValue(eAAMPConfig_VODTrickPlayFPS)).Times(0);

    // Calls from PlayerInstanceAAMP::GetPositionMilliseconds
    EXPECT_CALL(*g_mockAampConfig, IsConfigSet(eAAMPConfig_EnableGstPositionQuery)).WillOnce(Return(true));
    EXPECT_CALL(*g_mockAampConfig, IsConfigSet(eAAMPConfig_AudioOnlyPlayback)).WillOnce(Return(false));

    // Return a position that is already beyond pauseAtMilliseconds
    EXPECT_CALL(*g_mockAampGstPlayer, GetPositionMilliseconds()).WillRepeatedly(Return (pauseAtMilliseconds + 1));

    // Check that PlayerInstanceAAMP_PausePosition is not called
    EXPECT_CALL(*g_mockAampScheduler, ScheduleTask(_)).Times(0);

    mPlayerInstanceAAMP->StartPausePositionMonitoring(pauseAtMilliseconds);

    // Wait for scheduler to check it is not called
    ASSERT_FALSE(WaitForScheduleTask(1000));
    EXPECT_EQ(mPlayerInstanceAAMP->mPausePositionMilliseconds, -1);

    mPlayerInstanceAAMP->StopPausePositionMonitoring("stopped by test");
}

// Testing calling StartPausePositionMonitoring when rate is pause
// (which the code checks for but really pause is done by setting pipeline_paused)
// Don't expect ScheduleTask to be called to execute pause
TEST_F(PauseAtTests, StartPausePositionMonitoring_RatePaused)
{
    long long pauseAtMilliseconds = 100.0 * 1000;

    mPlayerInstanceAAMP->rate = AAMP_RATE_PAUSE;
    mPlayerInstanceAAMP->pipeline_paused = true;
    mPlayerInstanceAAMP->trickStartUTCMS = 0;
    mPlayerInstanceAAMP->mAudioOnlyPb = false;
    mPlayerInstanceAAMP->durationSeconds = 3600;

    EXPECT_CALL(*g_mockAampConfig, GetConfigValue(eAAMPConfig_VODTrickPlayFPS)).Times(0);

    // Calls from PlayerInstanceAAMP::GetPositionMilliseconds
    EXPECT_CALL(*g_mockAampConfig, IsConfigSet(eAAMPConfig_EnableGstPositionQuery)).WillOnce(Return(true));
    EXPECT_CALL(*g_mockAampConfig, IsConfigSet(eAAMPConfig_AudioOnlyPlayback)).WillOnce(Return(false));

    // Return a position that is already beyond pauseAtMilliseconds
    EXPECT_CALL(*g_mockAampGstPlayer, GetPositionMilliseconds()).WillRepeatedly(Return (pauseAtMilliseconds + 1));

    // Check that PlayerInstanceAAMP_PausePosition is not called
    EXPECT_CALL(*g_mockAampScheduler, ScheduleTask(_)).Times(0);

    mPlayerInstanceAAMP->StartPausePositionMonitoring(pauseAtMilliseconds);

    // Wait for scheduler to check it is not called
    ASSERT_FALSE(WaitForScheduleTask(1000));
    EXPECT_EQ(mPlayerInstanceAAMP->mPausePositionMilliseconds, -1);

    mPlayerInstanceAAMP->StopPausePositionMonitoring("stopped by test");
}

// Testing calling StartPausePositionMonitoring when already monitoring
// Expect the pause position to be updated with later position
TEST_F(PauseAtTests, StartPausePositionMonitoring_AlreadyStarted)
{
    long long pauseAtMilliseconds01 = 100.0 * 1000;
    long long pauseAtMilliseconds02 = 200.0 * 1000;

    mPlayerInstanceAAMP->rate = AAMP_NORMAL_PLAY_RATE;
    mPlayerInstanceAAMP->pipeline_paused = false;
    mPlayerInstanceAAMP->trickStartUTCMS = 0;
    mPlayerInstanceAAMP->mAudioOnlyPb = false;
    mPlayerInstanceAAMP->durationSeconds = 3600;

    // Calls from PlayerInstanceAAMP::GetPositionMilliseconds
    EXPECT_CALL(*g_mockAampConfig, IsConfigSet(eAAMPConfig_EnableGstPositionQuery)).WillRepeatedly(Return(true));
    EXPECT_CALL(*g_mockAampConfig, IsConfigSet(eAAMPConfig_AudioOnlyPlayback)).WillRepeatedly(Return(false));

    // Don't move position for this test
    EXPECT_CALL(*g_mockAampGstPlayer, GetPositionMilliseconds()).WillRepeatedly(Return (0));

    // Check that PlayerInstanceAAMP_PausePosition is not called
    EXPECT_CALL(*g_mockAampScheduler, ScheduleTask(_)).Times(0);

    mPlayerInstanceAAMP->StartPausePositionMonitoring(pauseAtMilliseconds01);
    ASSERT_EQ(mPlayerInstanceAAMP->mPausePositionMilliseconds, pauseAtMilliseconds01);

    mPlayerInstanceAAMP->StartPausePositionMonitoring(pauseAtMilliseconds02);
    EXPECT_EQ(mPlayerInstanceAAMP->mPausePositionMilliseconds, pauseAtMilliseconds02);

    mPlayerInstanceAAMP->StopPausePositionMonitoring("stopped by test");
}

// Testing calling StartPausePositionMonitoring with an invalid position (i.e. negative)
// Don't expect ScheduleTask to be called to execute pause
// Expect the pause position to be set to AAMP_PAUSE_POSITION_INVALID_POSITION
TEST_F(PauseAtTests, StartPausePositionMonitoring_InvalidPosition)
{
    long long pauseAtMilliseconds = -100.0 * 1000;

    mPlayerInstanceAAMP->rate = AAMP_NORMAL_PLAY_RATE;
    mPlayerInstanceAAMP->pipeline_paused = false;
    mPlayerInstanceAAMP->trickStartUTCMS = 0;
    mPlayerInstanceAAMP->mAudioOnlyPb = false;
    mPlayerInstanceAAMP->durationSeconds = 3600;

    // Calls from PlayerInstanceAAMP::GetPositionMilliseconds
    EXPECT_CALL(*g_mockAampConfig, IsConfigSet(eAAMPConfig_EnableGstPositionQuery)).Times(0);
    EXPECT_CALL(*g_mockAampConfig, IsConfigSet(eAAMPConfig_AudioOnlyPlayback)).Times(0);

    // Don't move position for this test
    EXPECT_CALL(*g_mockAampGstPlayer, GetPositionMilliseconds()).WillRepeatedly(Return (0));

    // Check that PlayerInstanceAAMP_PausePosition is not called
    EXPECT_CALL(*g_mockAampScheduler, ScheduleTask(_)).Times(0);

    mPlayerInstanceAAMP->StartPausePositionMonitoring(pauseAtMilliseconds);
    ASSERT_EQ(mPlayerInstanceAAMP->mPausePositionMilliseconds, AAMP_PAUSE_POSITION_INVALID_POSITION);
}

// Testing calling StopPausePositionMonitoring whilst monitoring
// Don't expect ScheduleTask to be called to execute pause
// Expect the pause position to be set to AAMP_PAUSE_POSITION_INVALID_POSITION
TEST_F(PauseAtTests, StopPausePositionMonitoring_WhenMonitoring)
{
    long long pauseAtMilliseconds = 100.0 * 1000;

    mPlayerInstanceAAMP->rate = AAMP_NORMAL_PLAY_RATE;
    mPlayerInstanceAAMP->pipeline_paused = false;
    mPlayerInstanceAAMP->trickStartUTCMS = 0;
    mPlayerInstanceAAMP->mAudioOnlyPb = false;
    mPlayerInstanceAAMP->durationSeconds = 3600;

    // Calls from PlayerInstanceAAMP::GetPositionMilliseconds
    EXPECT_CALL(*g_mockAampConfig, IsConfigSet(eAAMPConfig_EnableGstPositionQuery)).WillRepeatedly(Return(true));
    EXPECT_CALL(*g_mockAampConfig, IsConfigSet(eAAMPConfig_AudioOnlyPlayback)).WillRepeatedly(Return(false));

    // Don't move position for this test
    EXPECT_CALL(*g_mockAampGstPlayer, GetPositionMilliseconds()).WillRepeatedly(Return (0));

    // Check that PlayerInstanceAAMP_PausePosition is not called
    EXPECT_CALL(*g_mockAampScheduler, ScheduleTask(_)).Times(0);

    mPlayerInstanceAAMP->StartPausePositionMonitoring(pauseAtMilliseconds);
    ASSERT_EQ(mPlayerInstanceAAMP->mPausePositionMilliseconds, pauseAtMilliseconds);

    mPlayerInstanceAAMP->StopPausePositionMonitoring("stopped by test");
    EXPECT_EQ(mPlayerInstanceAAMP->mPausePositionMilliseconds, AAMP_PAUSE_POSITION_INVALID_POSITION);
}

// Testing calling StopPausePositionMonitoring whilst not monitoring
// Expect the pause position to be set to AAMP_PAUSE_POSITION_INVALID_POSITION
TEST_F(PauseAtTests, StopPausePositionMonitoring_WhenNotMonitoring)
{
    mPlayerInstanceAAMP->rate = AAMP_NORMAL_PLAY_RATE;
    mPlayerInstanceAAMP->pipeline_paused = false;
    mPlayerInstanceAAMP->trickStartUTCMS = 0;
    mPlayerInstanceAAMP->mAudioOnlyPb = false;
    mPlayerInstanceAAMP->durationSeconds = 3600;

    ASSERT_EQ(mPlayerInstanceAAMP->mPausePositionMilliseconds, AAMP_PAUSE_POSITION_INVALID_POSITION);

    mPlayerInstanceAAMP->StopPausePositionMonitoring("stopped by test");
    EXPECT_EQ(mPlayerInstanceAAMP->mPausePositionMilliseconds, AAMP_PAUSE_POSITION_INVALID_POSITION);
}

// Testing call of PlayerInstanceAAMP_PausePosition when at playback speed
// Current position returned is beyond the requested pause position, to trigger
// the call immediately to PlayerInstanceAAMP_PausePosition
// Expect:
//     gstreamer pause to be called,
//     notifications of AAMP_EVENT_STATE_CHANGED and AAMP_EVENT_SPEED_CHANGED
//     call to StreamAbstractionAAMP::NotifyPlaybackPaused
//     seek_pos_seconds should remain at initial value
//     trickStartUTCMS to be set to 0
TEST_F(PauseAtTests, PausePosition_Playback)
{
    long long pauseAtMilliseconds = 100.0 * 1000;
    int seek_pos_seconds = 123;

    mPlayerInstanceAAMP->rate = AAMP_NORMAL_PLAY_RATE;
    mPlayerInstanceAAMP->pipeline_paused = false;
    mPlayerInstanceAAMP->seek_pos_seconds = seek_pos_seconds;
    mPlayerInstanceAAMP->trickStartUTCMS = 0;
    mPlayerInstanceAAMP->mAudioOnlyPb = false;
    mPlayerInstanceAAMP->durationSeconds = 3600;

    mPlayerInstanceAAMP->SetState(eSTATE_PLAYING);

    ASSERT_FALSE(mPlayerInstanceAAMP->mbDownloadsBlocked);

    // Calls from PlayerInstanceAAMP::GetPositionMilliseconds
    EXPECT_CALL(*g_mockAampConfig, IsConfigSet(eAAMPConfig_EnableGstPositionQuery)).WillRepeatedly(Return(true));
    EXPECT_CALL(*g_mockAampConfig, IsConfigSet(eAAMPConfig_AudioOnlyPlayback)).WillRepeatedly(Return(false));

    // Already beyond position
    EXPECT_CALL(*g_mockAampGstPlayer, GetPositionMilliseconds())
        .WillRepeatedly(Return(pauseAtMilliseconds+ (mPlayerInstanceAAMP->rate * 1000)));

    // Check that PlayerInstanceAAMP_PausePosition is called
    EXPECT_CALL(*g_mockAampScheduler, ScheduleTask(_))
        .WillOnce(Invoke(this, &PauseAtTests::ScheduleTask));

    mPlayerInstanceAAMP->StartPausePositionMonitoring(pauseAtMilliseconds);
    ASSERT_EQ(mPlayerInstanceAAMP->mPausePositionMilliseconds, pauseAtMilliseconds);

    // Wait for scheduler to be called, and assert it didn't timeout
    ASSERT_TRUE(WaitForScheduleTask(5000));
    ASSERT_EQ(mScheduleAsyncTaskName, "PlayerInstanceAAMP_PausePosition");
    ASSERT_EQ(mScheduleAsyncData, mPlayerInstanceAAMP);

    EXPECT_CALL(*g_mockAampGstPlayer, Pause(true, false)).WillOnce(Return(true));

    // Expected calls from PlayerInstanceAAMP::NotifySpeedChanged
    EXPECT_CALL(*g_mockAampConfig, IsConfigSet(eAAMPConfig_NativeCCRendering)).WillRepeatedly(Return(false));
    EXPECT_CALL(*g_mockAampConfig, IsConfigSet(eAAMPConfig_RepairIframes)).WillRepeatedly(Return(false));
    EXPECT_CALL(*g_mockAampConfig, IsConfigSet(eAAMPConfig_UseSecManager)).WillRepeatedly(Return(false));
    EXPECT_CALL(*g_mockAampConfig, IsConfigSet(eAAMPConfig_UseFireboltSDK)).WillRepeatedly(Return(false));
    EXPECT_CALL(*g_mockAampScheduler, SetState(eSTATE_PAUSED)).Times(1);

    // Expected calls from PlayerInstanceAAMP::SetState
    EXPECT_CALL(*g_mockAampEventManager, IsEventListenerAvailable(AAMP_EVENT_STATE_CHANGED)).WillOnce(Return(true));
    EXPECT_CALL(*g_mockAampEventManager, SendEvent(AnEventOfType(AAMP_EVENT_STATE_CHANGED),_)).Times(1);

    EXPECT_CALL(*g_mockAampEventManager, SendEvent(AnEventOfType(AAMP_EVENT_SPEED_CHANGED),_)).Times(1);

    EXPECT_CALL(*g_mockStreamAbstractionAAMP, NotifyPlaybackPaused(true)).Times(1);

    // Execute PlayerInstanceAAMP_PausePosition
    mScheduleAsyncTask(mScheduleAsyncData);

    EXPECT_TRUE(mPlayerInstanceAAMP->pipeline_paused);
    EXPECT_TRUE(mPlayerInstanceAAMP->mbDownloadsBlocked);
    EXPECT_EQ(mPlayerInstanceAAMP->seek_pos_seconds, seek_pos_seconds);
    EXPECT_EQ(mPlayerInstanceAAMP->trickStartUTCMS, 0);
}

// Testing call of PlayerInstanceAAMP_PausePosition when at trickmode speed
// Current position returned is beyond the requested pause position, to trigger
// the call immediately to PlayerInstanceAAMP_PausePosition
// Expect:
//     gstreamer pause to be called,
//     notifications of AAMP_EVENT_STATE_CHANGED and AAMP_EVENT_SPEED_CHANGED
//     call to StreamAbstractionAAMP::NotifyPlaybackPaused
//     seek_pos_seconds to be set to the current position
//     trickStartUTCMS to be set to -1
TEST_F(PauseAtTests, PausePosition_Trickmode)
{
    long long pauseAtMilliseconds = 100.0 * 1000;
    // Current position is beyond the pause at position
    long long currentPosition = pauseAtMilliseconds + 2000;
    int trickplayFPS = 4;

    mPlayerInstanceAAMP->rate = 2;
    mPlayerInstanceAAMP->pipeline_paused = false;
    mPlayerInstanceAAMP->trickStartUTCMS = 0;
    mPlayerInstanceAAMP->mAudioOnlyPb = false;
    mPlayerInstanceAAMP->durationSeconds = 3600;

    mPlayerInstanceAAMP->SetState(eSTATE_PLAYING);

    ASSERT_FALSE(mPlayerInstanceAAMP->mbDownloadsBlocked);

    EXPECT_CALL(*g_mockAampConfig, GetConfigValue(eAAMPConfig_VODTrickPlayFPS))
      .WillRepeatedly(Return(trickplayFPS));
    EXPECT_CALL(*g_mockAampConfig, IsConfigSet(eAAMPConfig_UseAbsoluteTimeline)).WillRepeatedly(Return(false));

    // Calls from PlayerInstanceAAMP::GetPositionMilliseconds
    EXPECT_CALL(*g_mockAampConfig, IsConfigSet(eAAMPConfig_EnableGstPositionQuery)).WillRepeatedly(Return(true));
    EXPECT_CALL(*g_mockAampConfig, IsConfigSet(eAAMPConfig_AudioOnlyPlayback)).WillRepeatedly(Return(false));

    EXPECT_CALL(*g_mockAampGstPlayer, GetPositionMilliseconds())
        .WillRepeatedly(Return(currentPosition));

    // Check that PlayerInstanceAAMP_PausePosition is called
    EXPECT_CALL(*g_mockAampScheduler, ScheduleTask(_))
        .WillOnce(Invoke(this, &PauseAtTests::ScheduleTask));

    mPlayerInstanceAAMP->StartPausePositionMonitoring(pauseAtMilliseconds);
    ASSERT_EQ(mPlayerInstanceAAMP->mPausePositionMilliseconds, pauseAtMilliseconds);

    // Wait for scheduler to be called, and assert it didn't timeout
    ASSERT_TRUE(WaitForScheduleTask(5000));
    ASSERT_EQ(mScheduleAsyncTaskName, "PlayerInstanceAAMP_PausePosition");
    ASSERT_EQ(mScheduleAsyncData, mPlayerInstanceAAMP);

    EXPECT_CALL(*g_mockAampGstPlayer, Pause(true, false)).WillOnce(Return(true));

    // Expected calls from PlayerInstanceAAMP::NotifySpeedChanged
    EXPECT_CALL(*g_mockAampConfig, IsConfigSet(eAAMPConfig_NativeCCRendering)).WillRepeatedly(Return(false));
    EXPECT_CALL(*g_mockAampConfig, IsConfigSet(eAAMPConfig_RepairIframes)).WillRepeatedly(Return(false));
    EXPECT_CALL(*g_mockAampConfig, IsConfigSet(eAAMPConfig_UseSecManager)).WillRepeatedly(Return(false));
    EXPECT_CALL(*g_mockAampConfig, IsConfigSet(eAAMPConfig_UseFireboltSDK)).WillRepeatedly(Return(false));
    EXPECT_CALL(*g_mockAampScheduler, SetState(eSTATE_PAUSED)).Times(1);

    // Expected calls from PlayerInstanceAAMP::SetState
    EXPECT_CALL(*g_mockAampEventManager, IsEventListenerAvailable(AAMP_EVENT_STATE_CHANGED)).WillOnce(Return(true));
    EXPECT_CALL(*g_mockAampEventManager, SendEvent(AnEventOfType(AAMP_EVENT_STATE_CHANGED),_)).Times(1);

    EXPECT_CALL(*g_mockAampEventManager, SendEvent(AnEventOfType(AAMP_EVENT_SPEED_CHANGED),_)).Times(1);

    EXPECT_CALL(*g_mockStreamAbstractionAAMP, NotifyPlaybackPaused(true)).Times(1);

    // Execute PlayerInstanceAAMP_PausePosition
    mScheduleAsyncTask(mScheduleAsyncData);

    EXPECT_TRUE(mPlayerInstanceAAMP->pipeline_paused);
    EXPECT_TRUE(mPlayerInstanceAAMP->mbDownloadsBlocked);
    EXPECT_EQ(mPlayerInstanceAAMP->seek_pos_seconds, currentPosition / 1000);
    EXPECT_EQ(mPlayerInstanceAAMP->trickStartUTCMS, -1);
}

// Parameter test class, for running same tests with different rates
class PlaybackSpeedTests : public PauseAtTests,
                           public testing::WithParamInterface<float>
{

};

// Testing calling StartPausePositionMonitoring with a valid position
TEST_P(PlaybackSpeedTests, StartPausePositionMonitoring)
{
    long long pauseAtMilliseconds = 100.0 * 1000;
    int trickplayFPS = 4;

    mPlayerInstanceAAMP->rate = GetParam();
    mPlayerInstanceAAMP->pipeline_paused = false;
    mPlayerInstanceAAMP->trickStartUTCMS = 0;
    mPlayerInstanceAAMP->mAudioOnlyPb = false;
    mPlayerInstanceAAMP->durationSeconds = 3600;

    EXPECT_CALL(*g_mockAampConfig, GetConfigValue(eAAMPConfig_VODTrickPlayFPS))
     .WillRepeatedly(Return(trickplayFPS));

    // Calls from PlayerInstanceAAMP::GetPositionMilliseconds
    EXPECT_CALL(*g_mockAampConfig, IsConfigSet(eAAMPConfig_EnableGstPositionQuery)).WillRepeatedly(Return(true));
    EXPECT_CALL(*g_mockAampConfig, IsConfigSet(eAAMPConfig_AudioOnlyPlayback)).WillRepeatedly(Return(false));

    if ((mPlayerInstanceAAMP->rate == AAMP_NORMAL_PLAY_RATE) ||
        (mPlayerInstanceAAMP->rate == AAMP_SLOWMOTION_RATE))
    {
        // Simulate position just beyond AAMP_PAUSE_POSITION_POLL_PERIOD_MS of position,
        // then within AAMP_PAUSE_POSITION_POLL_PERIOD_MS of position,
        // then at position
        EXPECT_CALL(*g_mockAampGstPlayer, GetPositionMilliseconds())
            .WillOnce(Return(pauseAtMilliseconds - AAMP_PAUSE_POSITION_POLL_PERIOD_MS - 100))
            .WillOnce(Return(pauseAtMilliseconds - AAMP_PAUSE_POSITION_POLL_PERIOD_MS + 100))
            .WillRepeatedly(Return(pauseAtMilliseconds));
    }
    else if (mPlayerInstanceAAMP->rate > 0)
    {
        // Simulate position more than 2 frames prior to position,
        // then more than 1 frames prior to position,,
        // then within one frame of position,
        EXPECT_CALL(*g_mockAampGstPlayer, GetPositionMilliseconds())
            .WillOnce(Return(pauseAtMilliseconds - (((mPlayerInstanceAAMP->rate * 1000) / trickplayFPS) * 2) - 50))
            .WillOnce(Return(pauseAtMilliseconds - (((mPlayerInstanceAAMP->rate * 1000) / trickplayFPS) * 1) - 50))
            .WillRepeatedly(Return(pauseAtMilliseconds - (((mPlayerInstanceAAMP->rate * 1000) / trickplayFPS) * 1) + 50));
    }
    else
    {
        // Simulate position more than 2 frames prior to position,
        // then more than 1 frames prior to position,,
        // then within one frame of position,
        EXPECT_CALL(*g_mockAampGstPlayer, GetPositionMilliseconds())
            .WillOnce(Return(pauseAtMilliseconds - (((mPlayerInstanceAAMP->rate * 1000) / trickplayFPS) * 2) + 50))
            .WillOnce(Return(pauseAtMilliseconds - (((mPlayerInstanceAAMP->rate * 1000) / trickplayFPS) * 1) + 50))
            .WillRepeatedly(Return(pauseAtMilliseconds - (((mPlayerInstanceAAMP->rate * 1000) / trickplayFPS) * 1) - 50));
    }

    // Check that PlayerInstanceAAMP_PausePosition is called
    EXPECT_CALL(*g_mockAampScheduler, ScheduleTask(_))
        .WillOnce(Invoke(this, &PauseAtTests::ScheduleTask));

    mPlayerInstanceAAMP->StartPausePositionMonitoring(pauseAtMilliseconds);

    // Wait for scheduler to be called, and assert it didn't timeout
    ASSERT_TRUE(WaitForScheduleTask(5000));
    EXPECT_EQ(mScheduleAsyncTaskName, "PlayerInstanceAAMP_PausePosition");
}

// Testing calling StartPausePositionMonitoring with a position already in the past
TEST_P(PlaybackSpeedTests, StartPausePositionMonitoring_PositionAlreadyPassed)
{
    long long pauseAtMilliseconds = 100.0 * 1000;
    int trickplayFPS = 4;

    mPlayerInstanceAAMP->rate = GetParam();
    mPlayerInstanceAAMP->pipeline_paused = false;
    mPlayerInstanceAAMP->trickStartUTCMS = 0;
    mPlayerInstanceAAMP->mAudioOnlyPb = false;
    mPlayerInstanceAAMP->durationSeconds = 3600;

    EXPECT_CALL(*g_mockAampConfig, GetConfigValue(eAAMPConfig_VODTrickPlayFPS))
      .WillRepeatedly(Return(trickplayFPS));

    // Calls from PlayerInstanceAAMP::GetPositionMilliseconds
    EXPECT_CALL(*g_mockAampConfig, IsConfigSet(eAAMPConfig_EnableGstPositionQuery)).WillRepeatedly(Return(true));
    EXPECT_CALL(*g_mockAampConfig, IsConfigSet(eAAMPConfig_AudioOnlyPlayback)).WillRepeatedly(Return(false));

    EXPECT_CALL(*g_mockAampGstPlayer, GetPositionMilliseconds()).WillRepeatedly(Return(pauseAtMilliseconds + 100));

    // Check that PlayerInstanceAAMP_PausePosition is called
    EXPECT_CALL(*g_mockAampScheduler, ScheduleTask(_))
        .WillOnce(Invoke(this, &PauseAtTests::ScheduleTask));

    mPlayerInstanceAAMP->StartPausePositionMonitoring(pauseAtMilliseconds);

    // Wait for scheduler to be called, and assert it didn't timeout
    ASSERT_TRUE(WaitForScheduleTask(5000));
    EXPECT_EQ(mScheduleAsyncTaskName, "PlayerInstanceAAMP_PausePosition");
}

// Run PlaybackSpeedTests tests at various speeds
INSTANTIATE_TEST_SUITE_P(TestPlaybackSpeeds,
                         PlaybackSpeedTests,
                         testing::Values(AAMP_NORMAL_PLAY_RATE,AAMP_SLOWMOTION_RATE, -30, -2, 2, 30));


