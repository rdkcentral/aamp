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
 * @file AampRialtoPreRollMonitorTestCases.cpp
 * @brief Unit tests for AampRialtoPreRollMonitor.
 *
 * Oracle: InterfacePlayerRDK::buffering_timeout (middleware-player-interface)
 * releases the pre-roll when the video decoder's queued frame count reaches
 * the platform floor, when that count cannot be read, or when the buffering
 * timeout expires.  End of stream is an additional exit here because a short
 * asset can EOS before the floor is ever reached.
 */

#include <gtest/gtest.h>
#include <gmock/gmock.h>

#include "AampRialtoPreRollMonitor.h"
#include "MockGLib.h"

using ::testing::_;
using ::testing::Invoke;
using ::testing::NiceMock;
using ::testing::Return;

// Required by fake AAMP infrastructure linked via fakes library.
class AampConfig;
AampConfig *gpGlobalConfig{nullptr};

namespace {
	constexpr uint32_t kFloor          = 4u;
	constexpr guint    kPollIntervalMs = 10u;
	constexpr int64_t  kTimeoutMs      = 1000;
	constexpr guint    kTimerId        = 42u;
}

class AampRialtoPreRollMonitorTest : public ::testing::Test
{
protected:
	void SetUp() override
	{
		m_mockGLib = std::make_shared<NiceMock<MockGLib>>();
		g_mockGLib = m_mockGLib;

		ON_CALL(*m_mockGLib, g_timeout_add(_, _, _))
			.WillByDefault(Invoke(
				[this](guint, GSourceFunc fn, gpointer data) -> guint
				{
					m_timerFn   = fn;
					m_timerData = data;
					return kTimerId;
				}));
		ON_CALL(*m_mockGLib, g_source_remove(_)).WillByDefault(Return(TRUE));

		m_monitor = std::make_unique<AampRialtoPreRollMonitor>(
			AampRialtoPreRollMonitor::Config{kFloor, kPollIntervalMs, kTimeoutMs},
			[this]() { ++m_completions; },
			[this]() { return m_nowMs; });
	}

	void TearDown() override
	{
		m_monitor.reset();
		g_mockGLib.reset();
	}

	AampRialtoPreRollMonitor::Probe MakeProbe()
	{
		AampRialtoPreRollMonitor::Probe probe;
		probe.queuedFrames = [this](uint32_t &frames)
		{
			frames = m_queuedFrames;
			return m_framesAvailable;
		};
		probe.endOfStream = [this]() { return m_eos; };
		probe.injectedSpanMs = [this]() { return m_spanMs; };
		return probe;
	}

	std::shared_ptr<NiceMock<MockGLib>> m_mockGLib;
	std::unique_ptr<AampRialtoPreRollMonitor> m_monitor;
	GSourceFunc m_timerFn{nullptr};
	gpointer m_timerData{nullptr};

	int64_t m_nowMs{5000};
	uint32_t m_queuedFrames{0};
	bool m_framesAvailable{true};
	bool m_eos{false};
	int64_t m_spanMs{0};
	int m_completions{0};
};

TEST_F(AampRialtoPreRollMonitorTest, Start_SchedulesPollTimer)
{
	EXPECT_CALL(*m_mockGLib, g_timeout_add(kPollIntervalMs, _, _)).Times(1);

	m_monitor->Start(MakeProbe());

	EXPECT_TRUE(m_monitor->IsRunning());
	EXPECT_EQ(m_completions, 0);
}

TEST_F(AampRialtoPreRollMonitorTest, Poll_BelowFloor_KeepsPolling)
{
	m_monitor->Start(MakeProbe());
	m_queuedFrames = kFloor - 1;

	EXPECT_TRUE(m_monitor->Poll());

	EXPECT_TRUE(m_monitor->IsRunning());
	EXPECT_EQ(m_completions, 0);
}

TEST_F(AampRialtoPreRollMonitorTest, Poll_FloorReached_CompletesOnce)
{
	m_monitor->Start(MakeProbe());
	m_queuedFrames = kFloor;

	EXPECT_FALSE(m_monitor->Poll());
	EXPECT_FALSE(m_monitor->Poll());

	EXPECT_FALSE(m_monitor->IsRunning());
	EXPECT_EQ(m_completions, 1);
}

TEST_F(AampRialtoPreRollMonitorTest, Poll_QueuedFramesUnavailable_Completes)
{
	/**
	 * @brief The reference treats an unreadable decoder count as satisfied;
	 *        waiting out the timeout on every tune would be pure latency.
	 */
	m_monitor->Start(MakeProbe());
	m_framesAvailable = false;

	EXPECT_FALSE(m_monitor->Poll());

	EXPECT_EQ(m_completions, 1);
}

TEST_F(AampRialtoPreRollMonitorTest, Poll_EndOfStream_Completes)
{
	/**
	 * @brief An asset shorter than the target can never reach the floor.
	 */
	m_monitor->Start(MakeProbe());
	m_eos = true;

	EXPECT_FALSE(m_monitor->Poll());

	EXPECT_EQ(m_completions, 1);
}

TEST_F(AampRialtoPreRollMonitorTest, Poll_TimeoutElapsed_Completes)
{
	m_monitor->Start(MakeProbe());

	m_nowMs += kTimeoutMs - 1;
	EXPECT_TRUE(m_monitor->Poll());
	EXPECT_EQ(m_completions, 0);

	m_nowMs += 1;
	EXPECT_FALSE(m_monitor->Poll());
	EXPECT_EQ(m_completions, 1);
}

TEST_F(AampRialtoPreRollMonitorTest, Stop_RemovesTimerAndSuppressesCompletion)
{
	m_monitor->Start(MakeProbe());
	EXPECT_CALL(*m_mockGLib, g_source_remove(kTimerId)).Times(1);

	m_monitor->Stop();
	m_queuedFrames = kFloor;

	EXPECT_FALSE(m_monitor->Poll());
	EXPECT_FALSE(m_monitor->IsRunning());
	EXPECT_EQ(m_completions, 0);
}

TEST_F(AampRialtoPreRollMonitorTest, Start_WhileRunning_RestartsTimeout)
{
	/**
	 * @brief Restarting after a flush must measure the timeout from the new
	 *        start, not from the abandoned one.
	 */
	m_monitor->Start(MakeProbe());
	m_nowMs += kTimeoutMs - 100;

	EXPECT_CALL(*m_mockGLib, g_source_remove(kTimerId)).Times(1);
	m_monitor->Start(MakeProbe());
	// Teardown legitimately removes the restarted timer too.
	::testing::Mock::VerifyAndClearExpectations(m_mockGLib.get());
	m_nowMs += 200;

	EXPECT_TRUE(m_monitor->Poll());
	EXPECT_EQ(m_completions, 0);
}

TEST_F(AampRialtoPreRollMonitorTest, TimerCallback_DrivesPoll)
{
	m_monitor->Start(MakeProbe());
	ASSERT_NE(m_timerFn, nullptr);

	EXPECT_EQ(m_timerFn(m_timerData), G_SOURCE_CONTINUE);

	m_queuedFrames = kFloor;
	EXPECT_EQ(m_timerFn(m_timerData), G_SOURCE_REMOVE);
	EXPECT_EQ(m_completions, 1);
}

TEST_F(AampRialtoPreRollMonitorTest, Completed_StopDoesNotRemoveSelfRemovedTimer)
{
	/**
	 * @brief A timer that returned G_SOURCE_REMOVE is already gone; removing
	 *        it again would log a GLib critical.
	 */
	m_monitor->Start(MakeProbe());
	m_queuedFrames = kFloor;
	m_timerFn(m_timerData);

	EXPECT_CALL(*m_mockGLib, g_source_remove(_)).Times(0);
	m_monitor->Stop();
}

TEST_F(AampRialtoPreRollMonitorTest, TimerUnavailable_CompletesImmediately)
{
	/**
	 * @brief Without a timer nothing would ever poll, so play() would be
	 *        held forever; complete at once instead.
	 */
	ON_CALL(*m_mockGLib, g_timeout_add(_, _, _)).WillByDefault(Return(0u));

	m_monitor->Start(MakeProbe());

	EXPECT_EQ(m_completions, 1);
	EXPECT_FALSE(m_monitor->IsRunning());
}

TEST_F(AampRialtoPreRollMonitorTest, Completion_MayRestartMonitor)
{
	/**
	 * @brief The completion releases a hold, which can synchronously lead
	 *        back into Start(); the mutex must not be held across it.  A
	 *        deadlock fails this test by hanging.
	 */
	int restarts = 0;
	AampRialtoPreRollMonitor *self = nullptr;
	AampRialtoPreRollMonitor monitor(
		AampRialtoPreRollMonitor::Config{kFloor, kPollIntervalMs, kTimeoutMs},
		[&]()
		{
			if (restarts++ == 0)
			{
				self->Start(MakeProbe());
			}
		},
		[this]() { return m_nowMs; });
	self = &monitor;

	monitor.Start(MakeProbe());
	m_queuedFrames = kFloor;
	EXPECT_FALSE(monitor.Poll());

	EXPECT_EQ(restarts, 1);
	EXPECT_TRUE(monitor.IsRunning());
	monitor.Stop();
}
