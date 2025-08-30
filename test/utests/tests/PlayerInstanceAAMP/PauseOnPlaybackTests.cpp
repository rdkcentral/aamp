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
#include "MockAampConfig.h"
#include "MockAampScheduler.h"
#include "MockPrivateInstanceAAMP.h"
#include "main_aamp.h"
#include "MockStreamAbstractionAAMP.h"

using ::testing::_;
using ::testing::Return;
using ::testing::SetArgReferee;
using ::testing::AtLeast;

class PauseOnPlaybackTests : public ::testing::Test
{
protected:
    AampConfig *mConfig;

    void SetUp() override
    {
        if(gpGlobalConfig == nullptr)
        {
            gpGlobalConfig =  new AampConfig();
        }

        g_mockAampConfig = new MockAampConfig();
        g_mockAampScheduler = new MockAampScheduler();
        g_mockPrivateInstanceAAMP = new MockPrivateInstanceAAMP();
        mConfig = new AampConfig();
        mplayer = new TestablePlayerInstanceAAMP();

		// FIXME: below violates aamp member being private
        g_mockStreamAbstractionAAMP = new MockStreamAbstractionAAMP(mplayer->GetPrivAamp());
        mplayer->GetPrivAamp()->mpStreamAbstractionAAMP = g_mockStreamAbstractionAAMP;
    }

    void TearDown() override
    {
        delete g_mockPrivateInstanceAAMP;
        g_mockPrivateInstanceAAMP = nullptr;

        delete g_mockAampScheduler;
        g_mockAampScheduler = nullptr;

        delete g_mockAampConfig;
        g_mockAampConfig = nullptr;

        delete g_mockStreamAbstractionAAMP;
        g_mockStreamAbstractionAAMP =nullptr;

        delete mConfig;
        mConfig = nullptr;

        delete mplayer;
        mplayer = nullptr;

    }

    class TestablePlayerInstanceAAMP : public PlayerInstanceAAMP {
public:
    TestablePlayerInstanceAAMP() : PlayerInstanceAAMP()
    {
    }

    PrivateInstanceAAMP* GetPrivAamp()
    {
	    return aamp;
    }
		
    void SetRate_Internal(float rate,int overshootcorrection)
    {
        SetRateInternal(rate,overshootcorrection);
    }
};


    TestablePlayerInstanceAAMP *mplayer;
};

// Test PauseOnPlayback functionality not enabled when speed is play rate
TEST_F(PauseOnPlaybackTests, NormalPlayRate)
{
    float rate = AAMP_NORMAL_PLAY_RATE;
    int overshootcorrection = 0;

	// FIXME: below violates aamp member being private
    mplayer->GetPrivAamp()->mbUsingExternalPlayer = false;
    mplayer->GetPrivAamp()->pipeline_paused = true;
    mplayer->GetPrivAamp()->mbPlayEnabled = false;
    mplayer->GetPrivAamp()->mbDetached = false;

	EXPECT_CALL(*g_mockPrivateInstanceAAMP, SetPauseOnStartPlayback(false)).Times(1);

    mplayer->SetRate_Internal(rate,overshootcorrection);
}

// Test PauseOnPlayback functionality not enabled when playback has already been initiated
TEST_F(PauseOnPlaybackTests, PlaybackAlreadyInitiated)
{
    float rate = AAMP_NORMAL_PLAY_RATE;
    int overshootcorrection = 0;

	// FIXME: below violates aamp member being private
    mplayer->GetPrivAamp()->mbUsingExternalPlayer = false;
    mplayer->GetPrivAamp()->pipeline_paused = true;
    mplayer->GetPrivAamp()->mbPlayEnabled = true;
    mplayer->GetPrivAamp()->mbDetached = false;
	
    EXPECT_CALL(*g_mockPrivateInstanceAAMP, SetPauseOnStartPlayback(false)).Times(1);

    mplayer->SetRate_Internal(rate,overshootcorrection);
}

// Test PauseOnPlayback functionality enabled
TEST_F(PauseOnPlaybackTests, Success)
{
    float rate = AAMP_RATE_PAUSE;
    int overshootcorrection = 0;

	// FIXME: below violates aamp member being private
    mplayer->GetPrivAamp()->mbUsingExternalPlayer = false;
    mplayer->GetPrivAamp()->pipeline_paused = true;
    mplayer->GetPrivAamp()->mbPlayEnabled = false;
    mplayer->GetPrivAamp()->mbDetached = false;
	
    EXPECT_CALL(*g_mockPrivateInstanceAAMP, SetPauseOnStartPlayback(true)).Times(1);

    mplayer->SetRate_Internal(rate,overshootcorrection);
}
