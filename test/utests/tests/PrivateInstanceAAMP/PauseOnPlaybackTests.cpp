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
#include "AampScheduler.h"
#include "AampLogManager.h"
#include "MockAampConfig.h"
#include "MockAampGstPlayer.h"
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


class TestablePrivateInstanceAAMP : public PrivateInstanceAAMP
{
public:

    TestablePrivateInstanceAAMP(AampConfig *config) : PrivateInstanceAAMP(config)
    {
    }

    void SetLowLatencyMode(void)
    {
        mAampLLDashServiceData.lowLatencyMode = true;
    }

    void NotifyPauseOnStartPlayback(void)
    {
        PrivateInstanceAAMP::NotifyPauseOnStartPlayback();
    }

    bool Test_PauseOnStartPlayback(void)
    {
        return mbPauseOnStartPlayback;
    }
};

class PauseOnPlaybackTests : public ::testing::Test
{
protected:

    TestablePrivateInstanceAAMP *mPrivateInstanceAAMP{};
    AampLLDashServiceData aampLLDashServiceData;

    void SetUp() override
    {

        if(gpGlobalConfig == nullptr)
        {
            gpGlobalConfig =  new AampConfig();
        }

        mPrivateInstanceAAMP = new TestablePrivateInstanceAAMP(gpGlobalConfig);

        g_mockAampGstPlayer = new MockAAMPGstPlayer( mPrivateInstanceAAMP);
        g_mockAampStreamSinkManager = new NiceMock<MockAampStreamSinkManager>();
        g_mockAampEventManager = new MockAampEventManager();
        g_mockStreamAbstractionAAMP = new MockStreamAbstractionAAMP(mPrivateInstanceAAMP);


        mPrivateInstanceAAMP->mpStreamAbstractionAAMP = g_mockStreamAbstractionAAMP;

   		EXPECT_CALL(*g_mockAampStreamSinkManager, GetStreamSink(_)).WillRepeatedly(Return(g_mockAampGstPlayer));
        EXPECT_CALL(*g_mockAampEventManager, IsEventListenerAvailable(_)).WillRepeatedly(Return(true));
    }

    void TearDown() override
    {
        delete mPrivateInstanceAAMP;
        mPrivateInstanceAAMP = nullptr;

        delete g_mockStreamAbstractionAAMP;
        g_mockStreamAbstractionAAMP = nullptr;

        delete g_mockAampGstPlayer;
        g_mockAampGstPlayer = nullptr;

        delete g_mockAampStreamSinkManager;
        g_mockAampStreamSinkManager = nullptr;

        delete g_mockAampEventManager;
        g_mockAampEventManager = nullptr;
    }

public:
};

// ensure default zoom mod initialized as expected
TEST_F(PauseOnPlaybackTests, DefaultZoomMode )
{
	EXPECT_EQ(mPrivateInstanceAAMP->zoom_mode,VIDEO_ZOOM_NONE);
}

// Testing calling SetPauseOnStartPlayback_Enable with enabled
TEST_F(PauseOnPlaybackTests, SetPauseOnStartPlayback_Enable)
{
    EXPECT_CALL(*g_mockAampGstPlayer, SetPauseOnStartPlayback(true)).Times(1);
    mPrivateInstanceAAMP->SetPauseOnStartPlayback(true);
    EXPECT_TRUE(mPrivateInstanceAAMP->Test_PauseOnStartPlayback());
}

// Testing calling SetPauseOnStartPlayback_Enable with not enabled
TEST_F(PauseOnPlaybackTests, SetPauseOnStartPlayback_NotEnable)
{
    EXPECT_CALL(*g_mockAampGstPlayer, SetPauseOnStartPlayback(false)).Times(1);
    mPrivateInstanceAAMP->SetPauseOnStartPlayback(false);
    EXPECT_FALSE(mPrivateInstanceAAMP->Test_PauseOnStartPlayback());
}

// Testing calling SetPauseOnStartPlayback_Enable with not enabled, when already been enabled
TEST_F(PauseOnPlaybackTests, SetPauseOnStartPlayback_AlreadyEnabled)
{
    EXPECT_CALL(*g_mockAampGstPlayer, SetPauseOnStartPlayback(true)).Times(1);
    mPrivateInstanceAAMP->SetPauseOnStartPlayback(true);
    ASSERT_TRUE(mPrivateInstanceAAMP->Test_PauseOnStartPlayback());

    EXPECT_CALL(*g_mockAampGstPlayer, SetPauseOnStartPlayback(false)).Times(1);
    mPrivateInstanceAAMP->SetPauseOnStartPlayback(false);
    EXPECT_FALSE(mPrivateInstanceAAMP->Test_PauseOnStartPlayback());
}

// Testing calling SetPauseOnStartPlayback_Enable with enabled, but no StreamSink
TEST_F(PauseOnPlaybackTests, SetPauseOnStartPlayback_NoSink)
{
    EXPECT_CALL(*g_mockAampStreamSinkManager, GetStreamSink(_)).WillRepeatedly(Return(nullptr));
    EXPECT_CALL(*g_mockAampGstPlayer, SetPauseOnStartPlayback(_)).Times(0);
    mPrivateInstanceAAMP->SetPauseOnStartPlayback(true);
    EXPECT_FALSE(mPrivateInstanceAAMP->Test_PauseOnStartPlayback());
}

// Testing calling NotifyPauseOnStartPlayback when Pause On Playback not active
// mbPauseOnStartPlayback has not been set
TEST_F(PauseOnPlaybackTests, NotifyPauseOnStartPlayback_NotActive)
{
    mPrivateInstanceAAMP->mbDownloadsBlocked = false;
    mPrivateInstanceAAMP->mDisableRateCorrection = false;

    mPrivateInstanceAAMP->SetLowLatencyMode();
    mPrivateInstanceAAMP->SetLLDashAdjustSpeed(true);
    ASSERT_TRUE(mPrivateInstanceAAMP->GetLLDashAdjustSpeed());

    EXPECT_CALL(*g_mockAampGstPlayer, Pause(_,_)).Times(0);

    mPrivateInstanceAAMP->NotifyPauseOnStartPlayback();

    EXPECT_FALSE(mPrivateInstanceAAMP->Test_PauseOnStartPlayback());
    EXPECT_FALSE(mPrivateInstanceAAMP->mbDownloadsBlocked);
    EXPECT_FALSE(mPrivateInstanceAAMP->mDisableRateCorrection);
    EXPECT_TRUE(mPrivateInstanceAAMP->GetLLDashAdjustSpeed());
}

// Testing calling NotifyPauseOnStartPlayback when Pause On Playback active
// Good case
TEST_F(PauseOnPlaybackTests, NotifyFirstFrameReceived_Success)
{
    mPrivateInstanceAAMP->mbDownloadsBlocked = false;
    mPrivateInstanceAAMP->mDisableRateCorrection = false;

    mPrivateInstanceAAMP->SetPauseOnStartPlayback(true);

    mPrivateInstanceAAMP->SetLowLatencyMode();
    mPrivateInstanceAAMP->SetLLDashAdjustSpeed(true);
    ASSERT_TRUE(mPrivateInstanceAAMP->GetLLDashAdjustSpeed());

    EXPECT_CALL(*g_mockAampEventManager, SendEvent(StateChanged(eSTATE_PAUSED),_)).Times(1);
    EXPECT_CALL(*g_mockAampEventManager, SendEvent(SpeedChanged(0.0),_)).Times(1);
    EXPECT_CALL(*g_mockStreamAbstractionAAMP, NotifyPlaybackPaused(true)).Times(1);

    mPrivateInstanceAAMP->NotifyPauseOnStartPlayback();

    EXPECT_FALSE(mPrivateInstanceAAMP->Test_PauseOnStartPlayback());
    EXPECT_TRUE(mPrivateInstanceAAMP->mbDownloadsBlocked);
    EXPECT_TRUE(mPrivateInstanceAAMP->mDisableRateCorrection);
    EXPECT_FALSE(mPrivateInstanceAAMP->GetLLDashAdjustSpeed());
}

