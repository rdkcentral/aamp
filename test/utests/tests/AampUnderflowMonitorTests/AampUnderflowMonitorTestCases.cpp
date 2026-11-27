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
 * @file AampUnderflowMonitorTestCases.cpp
 * @brief Unit tests for AampUnderflowMonitor related functionalities.
 */

#include <atomic>
#include <chrono>
#include <future>
#include <memory>
#include <thread>

#include <gtest/gtest.h>
#include <gmock/gmock.h>

#include "AampConfig.h"
#include "AampUnderflowMonitor.h"
#include "MockAampConfig.h"
#include "MockPrivateInstanceAAMP.h"
#include "priv_aamp.h"

using ::testing::AtLeast;
using ::testing::NiceMock;
using ::testing::Return;

AampConfig *gpGlobalConfig{nullptr};

/**
 * @class AampUnderflowMonitorTest
 * @brief Test fixture for AampUnderflowMonitor tests.
 */

class AampUnderflowMonitorTest : public ::testing::Test
{
protected:
    AampConfig *mConfig{nullptr};
    PrivateInstanceAAMP *mAamp{nullptr};
    AampUnderflowMonitor *mMonitor{nullptr};
    std::shared_ptr<NiceMock<MockAampConfig>> mMockConfig;
    std::shared_ptr<NiceMock<MockPrivateInstanceAAMP>> mMockAamp;

    void SetUp() override
    {
        mConfig = new AampConfig();
        mAamp = new PrivateInstanceAAMP(mConfig);
        mMockConfig = std::make_shared<NiceMock<MockAampConfig>>();
        mMockAamp = std::make_shared<NiceMock<MockPrivateInstanceAAMP>>();
        g_mockAampConfig = mMockConfig;
        g_mockPrivateInstanceAAMP = mMockAamp;
        mMonitor = new AampUnderflowMonitor(mAamp);
    }

    void TearDown() override
    {
        if (mMonitor)
        {
            mMonitor->Stop();
            delete mMonitor;
            mMonitor = nullptr;
        }
        g_mockPrivateInstanceAAMP.reset();
        g_mockAampConfig.reset();

        delete mAamp;
        mAamp = nullptr;

        delete mConfig;
        mConfig = nullptr;
    }

};

/**
 * @test ProlongedBuffering_InvokesStalledErrorEvent
 * @brief prolonged buffering beyond the configured timeout must call SendStalledErrorEvent().
 */
TEST_F(AampUnderflowMonitorTest, ProlongedBuffering_InvokesStalledErrorEvent)
{
    constexpr int stallTimeoutMs = 50;
    std::atomic<bool> stalledEvent{false};

    EXPECT_CALL(*mMockConfig, GetConfigValue(eAAMPConfig_StallTimeoutMS))
        .WillRepeatedly(Return(stallTimeoutMs));
    EXPECT_CALL(*mMockAamp, SendStalledErrorEvent())
        .Times(AtLeast(1))
        .WillRepeatedly([&stalledEvent]() { stalledEvent.store(true); });

    mAamp->SetBufUnderFlowStatus(true);
    mMonitor->Start();

    const auto deadline = std::chrono::steady_clock::now()
        + std::chrono::milliseconds(500);
    while (!stalledEvent.load() && std::chrono::steady_clock::now() < deadline)
    {
        std::this_thread::yield();
    }

    EXPECT_TRUE(stalledEvent.load());
}

/**
 * @class AampUnderflowMonitorDeadlineTest
 * @brief Fixture for the deadline-expiry decision in AampUnderflowMonitor::Run().
 *
 * A single video fragment is reported with a small amount of buffer so that the
 * drain deadline expires almost immediately. IsDiscontinuityProcessPending() is
 * queried exactly once per expiry, before the underflow decision is made, so it
 * is used as the synchronisation point for the suppression cases. Stop() joins
 * the monitor thread, after which the SetBufferingState expectations are final.
 */
class AampUnderflowMonitorDeadlineTest : public AampUnderflowMonitorTest
{
protected:
    static constexpr double kEndPositionSec = 0.05;
    static constexpr auto kWaitTimeout = std::chrono::seconds(2);

    std::promise<void> mDecisionPoint;
    std::atomic<bool> mDecisionSignalled{false};

    void ExpectDecision(AAMPPlayerState state, bool discontinuityPending)
    {
        EXPECT_CALL(*mMockAamp, GetPositionMs()).WillRepeatedly(Return(0));
        EXPECT_CALL(*mMockAamp, GetState()).WillRepeatedly(Return(state));
        EXPECT_CALL(*mMockAamp, IsDiscontinuityProcessPending())
            .WillRepeatedly([this, discontinuityPending]() {
                if (!mDecisionSignalled.exchange(true))
                {
                    mDecisionPoint.set_value();
                }
                return discontinuityPending;
            });
    }

    void ExpireDeadline(float playRate)
    {
        std::future<void> decision = mDecisionPoint.get_future();
        mMonitor->Start();
        mMonitor->NotifyVideoFragment(kEndPositionSec, playRate);
        ASSERT_EQ(decision.wait_for(kWaitTimeout), std::future_status::ready);
        mMonitor->Stop();
    }
};

/**
 * @test DeadlineExpired_NormalPlayback_DeclaresUnderflow
 * @brief Positive control: an expired deadline at normal rate, in a playing state,
 *        with no pending discontinuity, must enter buffering.
 */
TEST_F(AampUnderflowMonitorDeadlineTest, DeadlineExpired_NormalPlayback_DeclaresUnderflow)
{
    ExpectDecision(eSTATE_PLAYING, false);
    EXPECT_CALL(*mMockAamp, SetBufferingState(true)).Times(1);

    ExpireDeadline(AAMP_NORMAL_PLAY_RATE);
}

/**
 * @test DeadlineExpired_DiscontinuityPending_SuppressesUnderflow
 * @brief An expired deadline while a discontinuity is being processed must not
 *        enter buffering.
 */
TEST_F(AampUnderflowMonitorDeadlineTest, DeadlineExpired_DiscontinuityPending_SuppressesUnderflow)
{
    ExpectDecision(eSTATE_PLAYING, true);
    EXPECT_CALL(*mMockAamp, SetBufferingState(true)).Times(0);

    ExpireDeadline(AAMP_NORMAL_PLAY_RATE);
}

/**
 * @test DeadlineExpired_Trickplay_SuppressesUnderflow
 * @brief An expired deadline during trickplay must not enter buffering.
 */
TEST_F(AampUnderflowMonitorDeadlineTest, DeadlineExpired_Trickplay_SuppressesUnderflow)
{
    ExpectDecision(eSTATE_PLAYING, false);
    EXPECT_CALL(*mMockAamp, SetBufferingState(true)).Times(0);

    ExpireDeadline(4.0f);
}

/**
 * @test DeadlineExpired_Seeking_SuppressesUnderflow
 * @brief An expired deadline while seeking must not enter buffering.
 */
TEST_F(AampUnderflowMonitorDeadlineTest, DeadlineExpired_Seeking_SuppressesUnderflow)
{
    ExpectDecision(eSTATE_SEEKING, false);
    EXPECT_CALL(*mMockAamp, SetBufferingState(true)).Times(0);

    ExpireDeadline(AAMP_NORMAL_PLAY_RATE);
}
