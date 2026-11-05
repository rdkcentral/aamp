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
 * @file AampRialtoGlibTimerTestCases.cpp
 * @brief L1 tests for AampRialtoGlibTimer.
 *
 * Oracle: the class contract.  A tick runs while the timer is running and
 * keeps it running only while it returns true; once Stop() or Restart() has
 * run, a dispatch from the earlier run does nothing; a tick may stop or
 * restart its own timer; Stop() does not return while another thread is in a
 * tick.  GLib is the MockGLib fake: the test fires the captured source.
 */

#include <gtest/gtest.h>
#include <gmock/gmock.h>

#include <atomic>
#include <chrono>
#include <future>
#include <memory>
#include <vector>

#include "AampRialtoGlibTimer.h"
#include "MockGLib.h"

using ::testing::_;
using ::testing::Invoke;
using ::testing::NiceMock;
using ::testing::Return;

class AampRialtoGlibTimerTest : public ::testing::Test
{
protected:
	struct Source
	{
		guint id;
		guint interval;
		GSourceFunc function;
		gpointer data;
	};

	void SetUp() override
	{
		g_mockGLib = std::make_shared<NiceMock<MockGLib>>();
		ON_CALL(*g_mockGLib, g_timeout_add(_, _, _))
			.WillByDefault(Invoke([this](guint interval, GSourceFunc function,
				gpointer data) {
				m_sources.push_back({m_nextId, interval, function, data});
				return m_nextId++;
			}));
		ON_CALL(*g_mockGLib, g_source_remove(_))
			.WillByDefault(Invoke([this](guint id) {
				m_removed.push_back(id);
				return TRUE;
			}));
	}

	void TearDown() override
	{
		g_mockGLib.reset();
	}

	/// Fire source @p index as the GLib main loop would.
	gboolean Fire(size_t index)
	{
		return m_sources.at(index).function(m_sources.at(index).data);
	}

	// The fake does not pass GLib's destroy notify on, so each started run
	// leaks one small handle for the duration of the test.
	std::vector<Source> m_sources;
	std::vector<guint> m_removed;
	guint m_nextId{100};
};

/**
 * @test AampRialtoGlibTimer_Start_SchedulesAtInterval
 * @brief Start() schedules one source at the interval and reports running.
 */
TEST_F(AampRialtoGlibTimerTest, AampRialtoGlibTimer_Start_SchedulesAtInterval)
{
	AampRialtoGlibTimer timer;

	EXPECT_TRUE(timer.Start(250, [] { return true; }));

	ASSERT_EQ(m_sources.size(), 1u);
	EXPECT_EQ(m_sources[0].interval, 250u);
	EXPECT_TRUE(timer.IsRunning());
}

/**
 * @test AampRialtoGlibTimer_Start_WhenRunning_ReturnsFalse
 * @brief A second Start() neither schedules another source nor replaces the
 *        tick.
 */
TEST_F(AampRialtoGlibTimerTest, AampRialtoGlibTimer_Start_WhenRunning_ReturnsFalse)
{
	AampRialtoGlibTimer timer;
	int first = 0;
	int second = 0;
	ASSERT_TRUE(timer.Start(250, [&] { ++first; return true; }));

	EXPECT_FALSE(timer.Start(100, [&] { ++second; return true; }));

	EXPECT_EQ(m_sources.size(), 1u);
	Fire(0);
	EXPECT_EQ(first, 1);
	EXPECT_EQ(second, 0);
}

/**
 * @test AampRialtoGlibTimer_Start_GlibFails_ReturnsFalse
 * @brief If GLib cannot schedule the source the timer is not running.
 */
TEST_F(AampRialtoGlibTimerTest, AampRialtoGlibTimer_Start_GlibFails_ReturnsFalse)
{
	EXPECT_CALL(*g_mockGLib, g_timeout_add(_, _, _)).WillOnce(Return(0));
	AampRialtoGlibTimer timer;

	EXPECT_FALSE(timer.Start(250, [] { return true; }));
	EXPECT_FALSE(timer.IsRunning());
}

/**
 * @test AampRialtoGlibTimer_Tick_KeepsRunningOnlyWhileItReturnsTrue
 * @brief A tick returning true keeps the source; false removes it and the
 *        timer stops.
 */
TEST_F(AampRialtoGlibTimerTest,
	AampRialtoGlibTimer_Tick_KeepsRunningOnlyWhileItReturnsTrue)
{
	AampRialtoGlibTimer timer;
	int ticks = 0;
	ASSERT_TRUE(timer.Start(250, [&] { return ++ticks < 2; }));

	EXPECT_EQ(Fire(0), G_SOURCE_CONTINUE);
	EXPECT_TRUE(timer.IsRunning());
	EXPECT_EQ(Fire(0), G_SOURCE_REMOVE);
	EXPECT_EQ(ticks, 2);
	EXPECT_FALSE(timer.IsRunning());

	// The removed source must not be removed again on stop.
	timer.Stop();
	EXPECT_TRUE(m_removed.empty());
}

/**
 * @test AampRialtoGlibTimer_Stop_RemovesSourceAndIgnoresLateDispatch
 * @brief Stop() removes the source; a dispatch GLib had already picked up
 *        does not run the tick.
 */
TEST_F(AampRialtoGlibTimerTest,
	AampRialtoGlibTimer_Stop_RemovesSourceAndIgnoresLateDispatch)
{
	AampRialtoGlibTimer timer;
	int ticks = 0;
	ASSERT_TRUE(timer.Start(250, [&] { ++ticks; return true; }));

	timer.Stop();

	EXPECT_EQ(m_removed, std::vector<guint>{m_sources[0].id});
	EXPECT_FALSE(timer.IsRunning());
	EXPECT_EQ(Fire(0), G_SOURCE_REMOVE);
	EXPECT_EQ(ticks, 0);
}

/**
 * @test AampRialtoGlibTimer_Restart_IgnoresDispatchFromTheEarlierRun
 * @brief After Restart() only the new source runs the (new) tick.
 */
TEST_F(AampRialtoGlibTimerTest,
	AampRialtoGlibTimer_Restart_IgnoresDispatchFromTheEarlierRun)
{
	AampRialtoGlibTimer timer;
	int oldTicks = 0;
	int newTicks = 0;
	ASSERT_TRUE(timer.Start(250, [&] { ++oldTicks; return true; }));

	ASSERT_TRUE(timer.Restart(100, [&] { ++newTicks; return true; }));

	ASSERT_EQ(m_sources.size(), 2u);
	EXPECT_EQ(m_removed, std::vector<guint>{m_sources[0].id});
	EXPECT_EQ(Fire(0), G_SOURCE_REMOVE);
	EXPECT_EQ(Fire(1), G_SOURCE_CONTINUE);
	EXPECT_EQ(oldTicks, 0);
	EXPECT_EQ(newTicks, 1);
}

/**
 * @test AampRialtoGlibTimer_Tick_MayStopItsOwnTimer
 * @brief The lock is recursive: Stop() from inside the tick neither
 *        deadlocks nor leaves the source scheduled.
 */
TEST_F(AampRialtoGlibTimerTest, AampRialtoGlibTimer_Tick_MayStopItsOwnTimer)
{
	AampRialtoGlibTimer timer;
	ASSERT_TRUE(timer.Start(250, [&] { timer.Stop(); return true; }));

	EXPECT_EQ(Fire(0), G_SOURCE_REMOVE);
	EXPECT_FALSE(timer.IsRunning());
}

/**
 * @test AampRialtoGlibTimer_Tick_MayRestartItsOwnTimer
 * @brief A tick that restarts its timer ends the old source and leaves the
 *        new one running.
 */
TEST_F(AampRialtoGlibTimerTest, AampRialtoGlibTimer_Tick_MayRestartItsOwnTimer)
{
	AampRialtoGlibTimer timer;
	int newTicks = 0;
	ASSERT_TRUE(timer.Start(250, [&] {
		timer.Restart(100, [&] { ++newTicks; return true; });
		return true;
	}));

	EXPECT_EQ(Fire(0), G_SOURCE_REMOVE);
	ASSERT_EQ(m_sources.size(), 2u);
	EXPECT_TRUE(timer.IsRunning());
	EXPECT_EQ(Fire(1), G_SOURCE_CONTINUE);
	EXPECT_EQ(newTicks, 1);
}

/**
 * @test AampRialtoGlibTimer_DispatchAfterOwnerDestroyed_IsIgnored
 * @brief A dispatch arriving after the timer object is gone is safe and does
 *        not run the tick.
 */
TEST_F(AampRialtoGlibTimerTest, AampRialtoGlibTimer_DispatchAfterOwnerDestroyed_IsIgnored)
{
	int ticks = 0;
	{
		AampRialtoGlibTimer timer;
		ASSERT_TRUE(timer.Start(250, [&] { ++ticks; return true; }));
	}

	EXPECT_EQ(Fire(0), G_SOURCE_REMOVE);
	EXPECT_EQ(ticks, 0);
}

/**
 * @test AampRialtoGlibTimer_Stop_WaitsForTickOnAnotherThread
 * @brief Stop() does not return while a tick runs on another thread, and the
 *        tick is not run again afterwards.
 */
TEST_F(AampRialtoGlibTimerTest, AampRialtoGlibTimer_Stop_WaitsForTickOnAnotherThread)
{
	AampRialtoGlibTimer timer;
	std::promise<void> tickEntered;
	std::promise<void> releaseTick;
	auto release = releaseTick.get_future().share();
	std::atomic<int> ticks{0};
	ASSERT_TRUE(timer.Start(250, [&] {
		if (ticks++ == 0)
		{
			tickEntered.set_value();
			release.wait();
		}
		return true;
	}));

	auto dispatcher = std::async(std::launch::async, [this] { return Fire(0); });
	tickEntered.get_future().wait();
	auto stopper = std::async(std::launch::async, [&timer] { timer.Stop(); });

	// Negative check: Stop() must still be blocked behind the running tick.
	EXPECT_EQ(stopper.wait_for(std::chrono::milliseconds(50)),
		std::future_status::timeout);
	releaseTick.set_value();
	ASSERT_EQ(stopper.wait_for(std::chrono::seconds(5)), std::future_status::ready);
	ASSERT_EQ(dispatcher.wait_for(std::chrono::seconds(5)), std::future_status::ready);
	EXPECT_EQ(m_removed, std::vector<guint>{m_sources[0].id});
	EXPECT_EQ(Fire(0), G_SOURCE_REMOVE);
	EXPECT_EQ(ticks.load(), 1);
}
