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
 * @file AampRialtoProgressTimerTestCases.cpp
 * @brief L1 tests for AampRialtoProgressTimer.
 *
 * Oracle: the class contract.  start() reports once immediately and then at
 * the interval; kick() reports at once and restarts the interval; no callback
 * runs after stop().  GLib is the MockGLib fake: the test fires the captured
 * source.
 */

#include <gtest/gtest.h>
#include <gmock/gmock.h>

#include <memory>
#include <vector>

#include "AampRialtoProgressTimer.h"
#include "MockGLib.h"

using ::testing::_;
using ::testing::Invoke;
using ::testing::NiceMock;
using ::testing::Return;

class AampRialtoProgressTimerTest : public ::testing::Test
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

	gboolean Fire(size_t index)
	{
		return m_sources.at(index).function(m_sources.at(index).data);
	}

	std::vector<Source> m_sources;
	std::vector<guint> m_removed;
	guint m_nextId{200};
	int m_reports{0};
};

/**
 * @test AampRialtoProgressTimer_start_ReportsAtOnceThenAtInterval
 * @brief The first report does not wait a full interval.
 */
TEST_F(AampRialtoProgressTimerTest,
	AampRialtoProgressTimer_start_ReportsAtOnceThenAtInterval)
{
	AampRialtoProgressTimer timer;

	timer.start(1000, [this] { ++m_reports; });

	EXPECT_EQ(m_reports, 1);
	EXPECT_TRUE(timer.isRunning());
	ASSERT_EQ(m_sources.size(), 1u);
	EXPECT_EQ(m_sources[0].interval, 1000u);
	EXPECT_EQ(Fire(0), G_SOURCE_CONTINUE);
	EXPECT_EQ(m_reports, 2);
}

/**
 * @test AampRialtoProgressTimer_start_ZeroInterval_DoesNothing
 * @brief A zero interval is rejected without reporting.
 */
TEST_F(AampRialtoProgressTimerTest, AampRialtoProgressTimer_start_ZeroInterval_DoesNothing)
{
	AampRialtoProgressTimer timer;

	timer.start(0, [this] { ++m_reports; });

	EXPECT_EQ(m_reports, 0);
	EXPECT_FALSE(timer.isRunning());
	EXPECT_TRUE(m_sources.empty());
}

/**
 * @test AampRialtoProgressTimer_start_WhenRunning_DoesNothing
 * @brief A second start() neither reports nor reschedules.
 */
TEST_F(AampRialtoProgressTimerTest, AampRialtoProgressTimer_start_WhenRunning_DoesNothing)
{
	AampRialtoProgressTimer timer;
	timer.start(1000, [this] { ++m_reports; });

	timer.start(500, [this] { m_reports += 10; });

	EXPECT_EQ(m_reports, 1);
	EXPECT_EQ(m_sources.size(), 1u);
}

/**
 * @test AampRialtoProgressTimer_start_GlibFails_StillReportsOnce
 * @brief If GLib cannot schedule the timer the first report is still sent,
 *        but the timer is not running.
 */
TEST_F(AampRialtoProgressTimerTest, AampRialtoProgressTimer_start_GlibFails_StillReportsOnce)
{
	EXPECT_CALL(*g_mockGLib, g_timeout_add(_, _, _)).WillOnce(Return(0));
	AampRialtoProgressTimer timer;

	timer.start(1000, [this] { ++m_reports; });

	EXPECT_EQ(m_reports, 1);
	EXPECT_FALSE(timer.isRunning());
}

/**
 * @test AampRialtoProgressTimer_kick_ReportsAtOnceAndRestartsInterval
 * @brief kick() reports immediately and replaces the scheduled source, so the
 *        next report is a full interval later; the old source is ignored.
 */
TEST_F(AampRialtoProgressTimerTest,
	AampRialtoProgressTimer_kick_ReportsAtOnceAndRestartsInterval)
{
	AampRialtoProgressTimer timer;
	timer.start(1000, [this] { ++m_reports; });

	timer.kick();

	EXPECT_EQ(m_reports, 2);
	ASSERT_EQ(m_sources.size(), 2u);
	EXPECT_EQ(m_sources[1].interval, 1000u);
	EXPECT_EQ(m_removed, std::vector<guint>{m_sources[0].id});
	EXPECT_EQ(Fire(0), G_SOURCE_REMOVE);
	EXPECT_EQ(m_reports, 2);
	EXPECT_EQ(Fire(1), G_SOURCE_CONTINUE);
	EXPECT_EQ(m_reports, 3);
}

/**
 * @test AampRialtoProgressTimer_kick_WhenStopped_DoesNothing
 * @brief kick() on a stopped timer neither reports nor schedules.
 */
TEST_F(AampRialtoProgressTimerTest, AampRialtoProgressTimer_kick_WhenStopped_DoesNothing)
{
	AampRialtoProgressTimer timer;

	timer.kick();

	EXPECT_EQ(m_reports, 0);
	EXPECT_TRUE(m_sources.empty());
}

/**
 * @test AampRialtoProgressTimer_stop_NoReportAfterStop
 * @brief A dispatch GLib had already picked up when stop() ran does not
 *        report.
 */
TEST_F(AampRialtoProgressTimerTest, AampRialtoProgressTimer_stop_NoReportAfterStop)
{
	AampRialtoProgressTimer timer;
	timer.start(1000, [this] { ++m_reports; });

	timer.stop();

	EXPECT_FALSE(timer.isRunning());
	EXPECT_EQ(m_removed, std::vector<guint>{m_sources[0].id});
	EXPECT_EQ(Fire(0), G_SOURCE_REMOVE);
	EXPECT_EQ(m_reports, 1);
}
