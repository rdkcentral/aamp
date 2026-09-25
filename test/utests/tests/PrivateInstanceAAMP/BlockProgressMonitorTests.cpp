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

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "priv_aamp.h"
#include "AampConfig.h"
#include "MockAampEventManager.h"

using ::testing::_;
using ::testing::NiceMock;

class TestablePrivateInstanceAAMP : public PrivateInstanceAAMP
{
public:
    TestablePrivateInstanceAAMP(AampConfig *config) : PrivateInstanceAAMP(config)
    {
    }

    void SetProgressState()
    {
        mState = eSTATE_PLAYING;
        mFirstProgress = true;
    }
};

class BlockProgressMonitorTests : public ::testing::Test
{
protected:
    TestablePrivateInstanceAAMP *mPrivateInstanceAAMP{};

    void SetUp() override
    {
        gpGlobalConfig = new AampConfig();
        mPrivateInstanceAAMP = new TestablePrivateInstanceAAMP(gpGlobalConfig);
        g_mockAampEventManager =
            std::make_shared<NiceMock<MockAampEventManager>>();

        mPrivateInstanceAAMP->SetProgressState();
        mPrivateInstanceAAMP->trickStartUTCMS = 0;
    }

    void TearDown() override
    {
        delete mPrivateInstanceAAMP;
        mPrivateInstanceAAMP = nullptr;

        g_mockAampEventManager.reset();

        delete gpGlobalConfig;
        gpGlobalConfig = nullptr;
    }
};

TEST_F(BlockProgressMonitorTests, BlocksMonitorProgressUntilUnblocked)
{
    mPrivateInstanceAAMP->BlockProgressMonitor();

    EXPECT_CALL(*g_mockAampEventManager, SendEvent(_, _)).Times(0);
    mPrivateInstanceAAMP->MonitorProgress();
    ASSERT_TRUE(::testing::Mock::VerifyAndClearExpectations(
        g_mockAampEventManager.get()));

    mPrivateInstanceAAMP->BlockProgressMonitor(false);

    EXPECT_CALL(*g_mockAampEventManager, SendEvent(AnEventOfType(AAMP_EVENT_PROGRESS), _)).Times(1);
    mPrivateInstanceAAMP->MonitorProgress();
}