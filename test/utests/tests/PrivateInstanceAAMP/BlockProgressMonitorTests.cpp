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

#include <chrono>
#include <cstdlib>
#include <future>
#include <optional>
#include <thread>

#include "priv_aamp.h"
#include "AampConfig.h"
#include "MockAampEventManager.h"

using ::testing::_;
using ::testing::AnyNumber;
using ::testing::NiceMock;

namespace
{
// Generous upper bound for an operation that is expected to complete.
constexpr auto kCompletionTimeout = std::chrono::seconds(5);
// Window used to observe that an operation is (still) blocked.
constexpr auto kBlockedWindow = std::chrono::milliseconds(100);

// A hung worker cannot be joined safely, so abort rather than hang CI.
void AbortIfNotReady(std::future<void> &f, const char *what)
{
    if (f.wait_for(kCompletionTimeout) != std::future_status::ready)
    {
        ADD_FAILURE() << "Deadlock suspected: " << what;
        std::abort();
    }
}
}

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
    {
        PrivateInstanceAAMP::SetRateProtect setRateLock(mPrivateInstanceAAMP);

        EXPECT_CALL(*g_mockAampEventManager, SendEvent(_, _)).Times(0);
        mPrivateInstanceAAMP->MonitorProgress();
        ASSERT_TRUE(::testing::Mock::VerifyAndClearExpectations(
            g_mockAampEventManager.get()));
    }

    EXPECT_CALL(*g_mockAampEventManager, SendEvent(AnEventOfType(AAMP_EVENT_PROGRESS), _)).Times(1);
    mPrivateInstanceAAMP->MonitorProgress();
}

TEST_F(BlockProgressMonitorTests, EarlyClearOfSetRateActiveUnblocksMonitorProgress)
{
    PrivateInstanceAAMP::SetRateProtect setRateLock(mPrivateInstanceAAMP);

    // Simulate code (e.g. flush completion) clearing the flag before the guard is destroyed
    mPrivateInstanceAAMP->mSetRateActive = false;

    EXPECT_CALL(*g_mockAampEventManager, SendEvent(AnEventOfType(AAMP_EVENT_PROGRESS), _)).Times(1);
    mPrivateInstanceAAMP->MonitorProgress();
}

/*
 * While SetRate holds the stream lock, MonitorProgress() on another thread
 * must return immediately (no progress, no wait on the stream lock).
 */
TEST_F(BlockProgressMonitorTests, MonitorProgressOnOtherThreadSkippedWithoutBlocking)
{
    std::future<void> progress; // declared first so the guard is released before join
    PrivateInstanceAAMP::SetRateProtect setRateLock(mPrivateInstanceAAMP);

    EXPECT_CALL(*g_mockAampEventManager, SendEvent(_, _)).Times(0);
    progress = std::async(std::launch::async,
        [this] { mPrivateInstanceAAMP->MonitorProgress(); });

    EXPECT_EQ(progress.wait_for(kCompletionTimeout), std::future_status::ready)
        << "MonitorProgress() blocked behind SetRate instead of skipping";
}

/*
 * SetState(PLAYING) during SetRate (after flush) clears the block early:
 * MonitorProgress() on another thread is no longer skipped, serialises on the
 * stream lock behind SetRate, and reports progress once SetRate completes.
 */
TEST_F(BlockProgressMonitorTests, SetStateDuringSetRateEarlyUnblocksMonitorProgressOnOtherThread)
{
    std::future<void> progress;
    std::optional<PrivateInstanceAAMP::SetRateProtect> setRateLock;
    setRateLock.emplace(mPrivateInstanceAAMP);

    EXPECT_CALL(*g_mockAampEventManager, SendEvent(_, _)).Times(0);
    progress = std::async(std::launch::async,
        [this] { mPrivateInstanceAAMP->MonitorProgress(); });
    AbortIfNotReady(progress, "MonitorProgress() before early unblock");
    ASSERT_TRUE(::testing::Mock::VerifyAndClearExpectations(
        g_mockAampEventManager.get()));

    mPrivateInstanceAAMP->SetState(eSTATE_PLAYING);
    EXPECT_FALSE(mPrivateInstanceAAMP->mSetRateActive.load());

    EXPECT_CALL(*g_mockAampEventManager, SendEvent(AnEventOfType(AAMP_EVENT_PROGRESS), _)).Times(1);
    progress = std::async(std::launch::async,
        [this] { mPrivateInstanceAAMP->MonitorProgress(); });

    // Passed the SetRate gate, so it must now be waiting on the stream lock.
    EXPECT_EQ(progress.wait_for(kBlockedWindow), std::future_status::timeout);

    setRateLock.reset();
    AbortIfNotReady(progress, "MonitorProgress() after SetRate released");
}

/*
 * While SetRate is starting (holding mSetRateActiveMutex before the flag and
 * stream lock are both in place) MonitorProgress() on another thread must skip.
 */
TEST_F(BlockProgressMonitorTests, MonitorProgressSkippedWhileSetRateStarting)
{
    std::future<void> progress;
    EXPECT_CALL(*g_mockAampEventManager, SendEvent(_, _)).Times(0);
    {
        std::lock_guard<std::mutex> starting(mPrivateInstanceAAMP->mSetRateActiveMutex);
        ASSERT_FALSE(mPrivateInstanceAAMP->mSetRateActive.load());

        progress = std::async(std::launch::async,
            [this] { mPrivateInstanceAAMP->MonitorProgress(); });
        AbortIfNotReady(progress, "MonitorProgress() while SetRate starting");
    }
}

/*
 * SetRate must not start while MonitorProgress() is gathering progress data
 * (SetRateMonitor held); once started, subsequent MonitorProgress() calls skip.
 */
TEST_F(BlockProgressMonitorTests, SetRateWaitsForInFlightMonitorProgress)
{
    std::promise<void> acquired;
    std::promise<void> release;
    std::future<void> acquiredFuture = acquired.get_future();
    std::shared_future<void> releaseFuture = release.get_future().share();

    std::optional<PrivateInstanceAAMP::SetRateMonitor> monitor;
    monitor.emplace(mPrivateInstanceAAMP);
    EXPECT_FALSE(monitor->active());

    std::thread setRateThread([this, &acquired, releaseFuture] {
        PrivateInstanceAAMP::SetRateProtect setRateLock(mPrivateInstanceAAMP);
        acquired.set_value();
        releaseFuture.wait();
    });

    EXPECT_EQ(acquiredFuture.wait_for(kBlockedWindow), std::future_status::timeout)
        << "SetRate started while MonitorProgress() was in flight";

    monitor.reset();
    if (acquiredFuture.wait_for(kCompletionTimeout) != std::future_status::ready)
    {
        ADD_FAILURE() << "Deadlock suspected: SetRate did not start";
        std::abort();
    }
    EXPECT_TRUE(mPrivateInstanceAAMP->mSetRateActive.load());

    EXPECT_CALL(*g_mockAampEventManager, SendEvent(_, _)).Times(0);
	auto progress = std::async(std::launch::async,
		[this] { mPrivateInstanceAAMP->MonitorProgress(); });
	AbortIfNotReady(progress, "MonitorProgress() while SetRate active");

    release.set_value();
    setRateThread.join();
    EXPECT_FALSE(mPrivateInstanceAAMP->mSetRateActive.load());
}

/*
 * Repeated concurrent SetRate (with early unblock via SetState) and
 * MonitorProgress() must not deadlock and must leave progress unblocked.
 */
TEST_F(BlockProgressMonitorTests, ConcurrentSetRateAndMonitorProgressDoNotDeadlock)
{
    constexpr int kIterations = 500;
    EXPECT_CALL(*g_mockAampEventManager, SendEvent(_, _)).Times(AnyNumber());

    std::future<void> setRate = std::async(std::launch::async, [this] {
        for (int i = 0; i < kIterations; ++i)
        {
            PrivateInstanceAAMP::SetRateProtect setRateLock(mPrivateInstanceAAMP);
            if (i % 2)
            {
                mPrivateInstanceAAMP->SetState(eSTATE_PLAYING);
            }
            std::this_thread::yield();
        }
    });
    std::future<void> progress = std::async(std::launch::async, [this] {
        for (int i = 0; i < kIterations; ++i)
        {
            mPrivateInstanceAAMP->MonitorProgress();
            std::this_thread::yield();
        }
    });

    AbortIfNotReady(setRate, "SetRate loop");
    AbortIfNotReady(progress, "MonitorProgress loop");
    EXPECT_FALSE(mPrivateInstanceAAMP->mSetRateActive.load());
}
