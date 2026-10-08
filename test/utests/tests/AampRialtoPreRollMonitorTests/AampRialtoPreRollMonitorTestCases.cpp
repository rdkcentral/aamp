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
 * releases the pre-roll at a 4-frame floor or when the buffering timeout
 * expires.  Here the floor counts frames Rialto has accepted rather than the
 * server's decoder queue, so the gate does not depend on how a Rialto server
 * is implemented.  End of stream is an additional exit because a short asset
 * can EOS before the floor is ever reached, and NO_SPACE from Rialto because
 * a buffer that is already full can never reach it either.
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
		probe.acceptedFrames = [this]() { return m_acceptedFrames; };
		probe.endOfStream = [this]() { return m_eos; };
		probe.bufferFull = [this]() { return m_bufferFull; };
		probe.injectedSpanMs = [this]() { return m_spanMs; };
		return probe;
	}

	std::shared_ptr<NiceMock<MockGLib>> m_mockGLib;
	std::unique_ptr<AampRialtoPreRollMonitor> m_monitor;
	GSourceFunc m_timerFn{nullptr};
	gpointer m_timerData{nullptr};

	int64_t m_nowMs{5000};
	uint32_t m_acceptedFrames{0};
	bool m_eos{false};
	bool m_bufferFull{false};
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
	m_acceptedFrames = kFloor - 1;

	EXPECT_TRUE(m_monitor->Poll());

	EXPECT_TRUE(m_monitor->IsRunning());
	EXPECT_EQ(m_completions, 0);
}

TEST_F(AampRialtoPreRollMonitorTest, Poll_FloorReached_CompletesOnce)
{
	m_monitor->Start(MakeProbe());
	m_acceptedFrames = kFloor;

	EXPECT_FALSE(m_monitor->Poll());
	EXPECT_FALSE(m_monitor->Poll());

	EXPECT_FALSE(m_monitor->IsRunning());
	EXPECT_EQ(m_completions, 1);
}

TEST_F(AampRialtoPreRollMonitorTest, Poll_NothingAcceptedYet_KeepsPolling)
{
	/**
	 * @brief Regression: pre-roll used to complete on its first poll, before
	 *        any data had flowed, because the server had no decoder yet.
	 */
	m_monitor->Start(MakeProbe());

	EXPECT_TRUE(m_monitor->Poll());

	EXPECT_EQ(m_completions, 0);
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

TEST_F(AampRialtoPreRollMonitorTest, Poll_BufferFull_Completes)
{
	/**
	 * @brief Once Rialto has no space for more data, waiting for the floor
	 *        can only end in the timeout.
	 */
	m_monitor->Start(MakeProbe());
	m_acceptedFrames = kFloor - 1;
	m_bufferFull = true;

	EXPECT_FALSE(m_monitor->Poll());

	EXPECT_FALSE(m_monitor->IsRunning());
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
	m_acceptedFrames = kFloor;

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

	m_acceptedFrames = kFloor;
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
	m_acceptedFrames = kFloor;
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
	m_acceptedFrames = kFloor;
	EXPECT_FALSE(monitor.Poll());

	EXPECT_EQ(restarts, 1);
	EXPECT_TRUE(monitor.IsRunning());
	monitor.Stop();
}

class AampRialtoPreRollMonitorProbeLifetimeTest : public AampRialtoPreRollMonitorTest
{
protected:
	/// A probe whose captured state we can watch: the weak_ptr expires once
	/// the monitor no longer holds a copy of the probe.
	AampRialtoPreRollMonitor::Probe MakeObservedProbe(std::weak_ptr<int> &observer)
	{
		auto token = std::make_shared<int>(0);
		observer = token;
		AampRialtoPreRollMonitor::Probe probe = MakeProbe();
		probe.endOfStream = [token, this]() { return m_eos; };
		return probe;
	}
};

TEST_F(AampRialtoPreRollMonitorProbeLifetimeTest, Completion_ReleasesProbe)
{
	/**
	 * @brief A probe may capture owning references (the player's once did,
	 *        to the Rialto pipeline).  Once pre-roll completes nothing may
	 *        keep them alive, or the pipeline's server session leaks.
	 */
	std::weak_ptr<int> observer;
	m_monitor->Start(MakeObservedProbe(observer));
	ASSERT_FALSE(observer.expired());

	m_acceptedFrames = kFloor;
	m_monitor->Poll();

	EXPECT_TRUE(observer.expired());
}

TEST_F(AampRialtoPreRollMonitorProbeLifetimeTest, Stop_ReleasesProbe)
{
	std::weak_ptr<int> observer;
	m_monitor->Start(MakeObservedProbe(observer));
	ASSERT_FALSE(observer.expired());

	m_monitor->Stop();

	EXPECT_TRUE(observer.expired());
}

TEST_F(AampRialtoPreRollMonitorProbeLifetimeTest, Restart_ReleasesPreviousProbe)
{
	std::weak_ptr<int> first;
	m_monitor->Start(MakeObservedProbe(first));

	std::weak_ptr<int> second;
	m_monitor->Start(MakeObservedProbe(second));

	EXPECT_TRUE(first.expired());
	EXPECT_FALSE(second.expired());
}

TEST_F(AampRialtoPreRollMonitorProbeLifetimeTest, StillRunning_KeepsProbe)
{
	std::weak_ptr<int> observer;
	m_monitor->Start(MakeObservedProbe(observer));

	m_acceptedFrames = kFloor - 1;
	m_monitor->Poll();

	EXPECT_FALSE(observer.expired());
}

TEST_F(AampRialtoPreRollMonitorProbeLifetimeTest, Destruction_ReleasesProbe)
{
	std::weak_ptr<int> observer;
	m_monitor->Start(MakeObservedProbe(observer));

	m_monitor.reset();

	EXPECT_TRUE(observer.expired());
}
