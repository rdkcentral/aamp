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
    }

    void TearDown() override
    {
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
 * @brief prolonged buffering beyond the configured timeout must call
 *        SendStalledErrorEvent().
 */
TEST_F(AampUnderflowMonitorTest, ProlongedBuffering_InvokesStalledErrorEvent)
{
    constexpr int stallTimeoutMs = 100;
    std::atomic<bool> stalledEvent{false};

    EXPECT_CALL(*mMockConfig, GetConfigValue(eAAMPConfig_StallTimeoutMS))
        .WillRepeatedly(Return(stallTimeoutMs));
    EXPECT_CALL(*mMockAamp, SendStalledErrorEvent())
        .Times(AtLeast(1))
        .WillRepeatedly([&stalledEvent]() { stalledEvent.store(true); });

    mAamp->SetBufUnderFlowStatus(true);
    AampUnderflowMonitor monitor(mAamp);
    monitor.Start();

    const auto deadline = std::chrono::steady_clock::now()
        + std::chrono::milliseconds(500);
    while (!stalledEvent.load() && std::chrono::steady_clock::now() < deadline)
    {
        std::this_thread::yield();
    }

    EXPECT_TRUE(stalledEvent.load());
    monitor.Stop();
}
