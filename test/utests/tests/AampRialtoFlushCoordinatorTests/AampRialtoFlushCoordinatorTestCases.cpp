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
 * @file AampRialtoFlushCoordinatorTestCases.cpp
 * @brief L1 tests for AampRialtoFlushCoordinator, driven against the real
 *        PlayerStateMachine.
 */

#include <gtest/gtest.h>

#include <atomic>
#include <chrono>
#include <future>
#include <thread>

#include "AampPlayerStateMachine.h"
#include "AampRialtoFlushCoordinator.h"

using namespace std::chrono_literals;

class AampRialtoFlushCoordinatorTest : public ::testing::Test
{
protected:
	AampRialtoFlushCoordinatorTest() : m_flush(m_sm) {}

	/// PLAYING is a state Flush() may start from.
	void MoveToPlaying()
	{
		m_sm.onPipelineLoaded();
		m_sm.onSourceAttaching();
		m_sm.onAllSourcesAttached();
		m_sm.onPlaybackStarted();
	}

	PlayerStateMachine m_sm;
	AampRialtoFlushCoordinator m_flush;
};

/**
 * @test Without a flush in progress the wait returns at once, not abandoned.
 */
TEST_F(AampRialtoFlushCoordinatorTest, WaitForComplete_NotFlushing_ReturnsFalseAtOnce)
{
	MoveToPlaying();
	bool handlerRan = false;

	const auto start = std::chrono::steady_clock::now();
	const bool abandoned = m_flush.WaitForComplete([&]() { handlerRan = true; });

	EXPECT_FALSE(abandoned);
	EXPECT_FALSE(handlerRan);
	EXPECT_LT(std::chrono::steady_clock::now() - start, 1s);
	EXPECT_EQ(m_sm.currentState(), PlayerStateId::PLAYING);
}

/**
 * @test ClaimFlushing moves the state machine into FLUSHING.
 */
TEST_F(AampRialtoFlushCoordinatorTest, ClaimFlushing_FromPlaying_EntersFlushing)
{
	MoveToPlaying();

	m_flush.ClaimFlushing();

	EXPECT_EQ(m_sm.currentState(), PlayerStateId::FLUSHING);
}

/**
 * @test Complete() ends FLUSHING and releases a thread blocked in
 *       WaitForComplete() without abandoning the flush.
 */
TEST_F(AampRialtoFlushCoordinatorTest, Complete_WakesWaiter_NotAbandoned)
{
	MoveToPlaying();
	m_flush.SetCompleteTimeout(10s);
	m_flush.ClaimFlushing();

	std::atomic<bool> handlerRan{false};
	auto waiter = std::async(std::launch::async, [&]()
	{
		return m_flush.WaitForComplete([&]() { handlerRan = true; });
	});

	// The waiter must really be blocked: FLUSHING has not ended.
	EXPECT_EQ(waiter.wait_for(100ms), std::future_status::timeout);

	m_flush.Complete();

	ASSERT_EQ(waiter.wait_for(5s), std::future_status::ready);
	EXPECT_FALSE(waiter.get());
	EXPECT_FALSE(handlerRan.load());
	EXPECT_EQ(m_sm.currentState(), PlayerStateId::FLUSHED);
}

/**
 * @test When SEEK_DONE never arrives the wait gives up: the state machine
 *       leaves FLUSHING, the handler runs once after that, and true is
 *       returned.
 */
TEST_F(AampRialtoFlushCoordinatorTest, WaitForComplete_Timeout_AbandonsFlush)
{
	MoveToPlaying();
	m_flush.SetCompleteTimeout(50ms);
	m_flush.ClaimFlushing();

	int handlerCalls = 0;
	PlayerStateId stateInHandler = PlayerStateId::FLUSHING;
	const bool abandoned = m_flush.WaitForComplete([&]()
	{
		++handlerCalls;
		stateInHandler = m_sm.currentState();
	});

	EXPECT_TRUE(abandoned);
	EXPECT_EQ(handlerCalls, 1);
	EXPECT_NE(stateInHandler, PlayerStateId::FLUSHING);
	EXPECT_NE(m_sm.currentState(), PlayerStateId::FLUSHING);
}

/**
 * @test A timed-out wait with no handler is allowed.
 */
TEST_F(AampRialtoFlushCoordinatorTest, WaitForComplete_TimeoutWithoutHandler_StillAbandons)
{
	MoveToPlaying();
	m_flush.SetCompleteTimeout(20ms);
	m_flush.ClaimFlushing();

	EXPECT_TRUE(m_flush.WaitForComplete());
	EXPECT_NE(m_sm.currentState(), PlayerStateId::FLUSHING);
}

/**
 * @test A failure that arrives while FLUSHING wakes the waiter (no timeout)
 *       and reports the state it interrupted.
 */
TEST_F(AampRialtoFlushCoordinatorTest, SignalError_WhileFlushing_ReturnsPreviousAndWakesWaiter)
{
	MoveToPlaying();
	m_flush.SetCompleteTimeout(10s);
	m_flush.ClaimFlushing();

	auto waiter = std::async(std::launch::async,
		[&]() { return m_flush.WaitForComplete(); });
	EXPECT_EQ(waiter.wait_for(100ms), std::future_status::timeout);

	EXPECT_EQ(m_flush.SignalError(), PlayerStateId::FLUSHING);

	ASSERT_EQ(waiter.wait_for(5s), std::future_status::ready);
	EXPECT_FALSE(waiter.get());
	EXPECT_EQ(m_sm.currentState(), PlayerStateId::ERROR);
}

/**
 * @test SignalError() outside a flush reports the current state.
 */
TEST_F(AampRialtoFlushCoordinatorTest, SignalError_WhenPlaying_ReturnsPlaying)
{
	MoveToPlaying();

	EXPECT_EQ(m_flush.SignalError(), PlayerStateId::PLAYING);
	EXPECT_EQ(m_sm.currentState(), PlayerStateId::ERROR);
}

/**
 * @test The call lock is recursive: Flush() recovers through Stop(), which
 *       takes it again on the same thread.
 */
TEST_F(AampRialtoFlushCoordinatorTest, SerialiseCalls_SameThreadMayReenter)
{
	auto outer = m_flush.SerialiseCalls();
	auto inner = m_flush.SerialiseCalls();

	EXPECT_TRUE(outer.owns_lock());
	EXPECT_TRUE(inner.owns_lock());
}

/**
 * @test A second thread cannot enter while the first holds the call lock,
 *       and gets in once it is released.
 */
TEST_F(AampRialtoFlushCoordinatorTest, SerialiseCalls_ExcludesOtherThread)
{
	std::atomic<bool> entered{false};
	std::thread other;
	{
		auto held = m_flush.SerialiseCalls();
		other = std::thread([&]()
		{
			auto lock = m_flush.SerialiseCalls();
			entered = true;
		});
		std::this_thread::sleep_for(100ms);
		EXPECT_FALSE(entered.load());
	}
	other.join();
	EXPECT_TRUE(entered.load());
}
