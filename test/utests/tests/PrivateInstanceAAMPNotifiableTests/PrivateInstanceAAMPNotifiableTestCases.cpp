/*
 * If not stated otherwise in this file or this component's license file the
 * following copyright and licenses apply:
 *
 * Copyright 2025 RDK Management
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
 * @file PrivateInstanceAAMPNotifiableTestCases.cpp
 * @brief L1 unit tests for PrivateInstanceAAMPNotifiable.
 *
 * Verifies that the adapter correctly forwards IStreamSinkNotifiable calls
 * to the underlying PrivateInstanceAAMP instance.  The fakes library
 * provides FakePrivateInstanceAAMP which delegates some methods to
 * g_mockPrivateInstanceAAMP.
 */

#include <gtest/gtest.h>
#include <gmock/gmock.h>

#include "PrivateInstanceAAMPNotifiable.h"
#include "MockAampConfig.h"
#include "MockPrivateInstanceAAMP.h"

using ::testing::Return;
using ::testing::NiceMock;
using ::testing::_;
using ::testing::NiceMock;

// ---------------------------------------------------------------------------
// Fixture
// ---------------------------------------------------------------------------

class PrivateInstanceAAMPNotifiableTest : public ::testing::Test
{
protected:
	void SetUp() override
	{
		g_mockAampConfig = std::make_shared<NiceMock<MockAampConfig>>();
		g_mockPrivateInstanceAAMP = std::make_shared<NiceMock<MockPrivateInstanceAAMP>>();
		m_notifiable = std::make_unique<PrivateInstanceAAMPNotifiable>(
			&m_aamp);
	}

	void TearDown() override
	{
		m_notifiable.reset();
		g_mockPrivateInstanceAAMP.reset();
		g_mockAampConfig.reset();
	}

	PrivateInstanceAAMP m_aamp{};
	std::unique_ptr<PrivateInstanceAAMPNotifiable> m_notifiable;
};

// ===========================================================================
// Methods that delegate through g_mockPrivateInstanceAAMP
// ===========================================================================

TEST_F(PrivateInstanceAAMPNotifiableTest,
	GetState_ForwardsToAamp_ReturnsMockedValue)
{
	EXPECT_CALL(*g_mockPrivateInstanceAAMP, GetState())
		.WillOnce(Return(eSTATE_PLAYING));

	EXPECT_EQ(m_notifiable->GetState(), eSTATE_PLAYING);
}

TEST_F(PrivateInstanceAAMPNotifiableTest,
	NotifySpeedChanged_SchedulesTaskThatForwardsToAamp)
{
	EXPECT_CALL(*g_mockPrivateInstanceAAMP,
		ScheduleAsyncTask(_, _, std::string("NotifySpeedChanged")))
		.WillOnce([](IdleTask task, void *arg, std::string) -> int {
			task(arg);
			return 1; // non-zero: must not equal AAMP_TASK_ID_INVALID (0)
		});
	EXPECT_CALL(*g_mockPrivateInstanceAAMP, NotifySpeedChanged(2.0f, true));

	m_notifiable->NotifySpeedChanged(2.0f, true);
}

// ===========================================================================
// Methods that call the fake stub (verify no crash / correct forwarding)
// ===========================================================================

TEST_F(PrivateInstanceAAMPNotifiableTest,
	NotifyFirstFrameReceived_SchedulesTaskThatCallsAamp)
{
	// The fake's NotifyFirstFrameReceived calls SetState(PLAYING) when state
	// is not IDLE and mFirstVideoFrameDisplayedEnabled is false (the default).
	EXPECT_CALL(*g_mockPrivateInstanceAAMP, GetState())
		.WillRepeatedly(Return(eSTATE_PLAYING));
	EXPECT_CALL(*g_mockPrivateInstanceAAMP, SetState(eSTATE_PLAYING, _));
	EXPECT_CALL(*g_mockPrivateInstanceAAMP,
		ScheduleAsyncTask(_, _, std::string("NotifyFirstFrameReceived")))
		.WillOnce([](IdleTask task, void *arg, std::string) -> int {
			task(arg);
			return 1;
		});

	m_notifiable->NotifyFirstFrameReceived(42);
}

TEST_F(PrivateInstanceAAMPNotifiableTest,
	NotifyFirstBufferProcessed_SchedulesTask)
{
	EXPECT_CALL(*g_mockPrivateInstanceAAMP,
		ScheduleAsyncTask(_, _, std::string("NotifyFirstBufferProcessed")))
		.WillOnce([](IdleTask task, void *arg, std::string) -> int {
			task(arg);
			return 1;
		});

	m_notifiable->NotifyFirstBufferProcessed("0,0,1920,1080");
}

TEST_F(PrivateInstanceAAMPNotifiableTest,
	LogFirstFrame_SchedulesTask)
{
	EXPECT_CALL(*g_mockPrivateInstanceAAMP,
		ScheduleAsyncTask(_, _, std::string("LogFirstFrame")))
		.WillOnce([](IdleTask task, void *arg, std::string) -> int {
			task(arg);
			return 1;
		});

	m_notifiable->LogFirstFrame();
}

TEST_F(PrivateInstanceAAMPNotifiableTest,
	LogTuneComplete_SchedulesTask)
{
	EXPECT_CALL(*g_mockPrivateInstanceAAMP,
		ScheduleAsyncTask(_, _, std::string("LogTuneComplete")))
		.WillOnce([](IdleTask task, void *arg, std::string) -> int {
			task(arg);
			return 1;
		});

	m_notifiable->LogTuneComplete();
}

TEST_F(PrivateInstanceAAMPNotifiableTest,
	NotifyEOSReached_SchedulesTask)
{
	EXPECT_CALL(*g_mockPrivateInstanceAAMP,
		ScheduleAsyncTask(_, _, std::string("NotifyEOSReached")))
		.WillOnce([](IdleTask task, void *arg, std::string) -> int {
			task(arg);
			return 1;
		});

	m_notifiable->NotifyEOSReached();
}

TEST_F(PrivateInstanceAAMPNotifiableTest,
	MonitorProgress_SchedulesTask)
{
	EXPECT_CALL(*g_mockPrivateInstanceAAMP,
		ScheduleAsyncTask(_, _, std::string("MonitorProgress")))
		.WillOnce([](IdleTask task, void *arg, std::string) -> int {
			task(arg);
			return 1;
		});

	m_notifiable->MonitorProgress(true, false);
}

// ===========================================================================
// ChangeAamp
// ===========================================================================

TEST_F(PrivateInstanceAAMPNotifiableTest,
	ChangeAamp_UpdatesTargetInstance)
{
	// The fixture's m_aamp is constructed without config (mConfig == nullptr),
	// while newAamp has a non-null config. This lets us verify ChangeAamp
	// switched the wrapped instance without relying on g_mockPrivateInstanceAAMP.
	AampConfig config;
	PrivateInstanceAAMP newAamp(&config);

	// Force FakeAampConfig fallback behavior for non-null config access.
	g_mockAampConfig.reset();

	// Sanity: before change, adapter points to fixture m_aamp with null config.
	EXPECT_DOUBLE_EQ(m_notifiable->GetProgressReportIntervalSeconds(), 0.0);

	// Act.
	m_notifiable->ChangeAamp(&newAamp);

	// Assert: non-null config path is now used on the new wrapped instance.
	EXPECT_DOUBLE_EQ(m_notifiable->GetProgressReportIntervalSeconds(), -1.0);
}

TEST_F(PrivateInstanceAAMPNotifiableTest,
	ChangeAamp_AfterChange_OldInstanceNotNotified)
{
	// Use an oracle that depends on the wrapped instance pointer itself:
	// fixture m_aamp has null config, while newAamp has non-null config.
	AampConfig config;
	PrivateInstanceAAMP newAamp(&config);

	EXPECT_DOUBLE_EQ(m_notifiable->GetProgressReportIntervalSeconds(), 0.0);

	m_notifiable->ChangeAamp(&newAamp);

	EXPECT_CALL(*g_mockAampConfig,
		GetConfigValue(eAAMPConfig_ReportProgressInterval))
		.WillOnce(Return(1.5));

	EXPECT_DOUBLE_EQ(m_notifiable->GetProgressReportIntervalSeconds(), 1.5);
}

// ===========================================================================
// NotifyBufferUnderflow
// ===========================================================================

TEST_F(PrivateInstanceAAMPNotifiableTest,
	NotifyBufferUnderflow_SchedulesTaskThatCallsScheduleRetune)
{
	// Use a PrivateInstanceAAMP with a non-null config so the task lambda can
	// call mConfig->IsConfigSet without crashing.
	AampConfig config;
	PrivateInstanceAAMP aampWithConfig(&config);
	PrivateInstanceAAMPNotifiable notifiable(&aampWithConfig);

	// Underflow monitor disabled: ScheduleRetune should be invoked (no-op in fake).
	EXPECT_CALL(*g_mockAampConfig,
		IsConfigSet(eAAMPConfig_EnableAampUnderflowMonitor))
		.WillOnce(Return(false));
	EXPECT_CALL(*g_mockPrivateInstanceAAMP,
		ScheduleAsyncTask(_, _, std::string("NotifyBufferUnderflow")))
		.WillOnce([](IdleTask task, void *arg, std::string) -> int {
			task(arg);
			return 1;
		});

	notifiable.NotifyBufferUnderflow(eMEDIATYPE_VIDEO);
}

TEST_F(PrivateInstanceAAMPNotifiableTest,
	NotifyBufferUnderflow_WhenMonitorEnabled_SchedulesTaskThatChecksConfig)
{
	AampConfig config;
	PrivateInstanceAAMP aampWithConfig(&config);
	PrivateInstanceAAMPNotifiable notifiable(&aampWithConfig);

	// ScheduleRetune is a no-op fake and not mockable, so this only verifies
	// the task is dispatched and takes the "monitor enabled" config branch,
	// not that ScheduleRetune itself was skipped.
	EXPECT_CALL(*g_mockAampConfig,
		IsConfigSet(eAAMPConfig_EnableAampUnderflowMonitor))
		.WillOnce(Return(true));
	EXPECT_CALL(*g_mockPrivateInstanceAAMP,
		ScheduleAsyncTask(_, _, std::string("NotifyBufferUnderflow")))
		.WillOnce([](IdleTask task, void *arg, std::string) -> int {
			task(arg);
			return 1;
		});

	notifiable.NotifyBufferUnderflow(eMEDIATYPE_AUDIO);
}

// ===========================================================================
// SendMonitorAvEvent
// ===========================================================================

TEST_F(PrivateInstanceAAMPNotifiableTest,
	SendMonitorAvEvent_SchedulesTask)
{
	EXPECT_CALL(*g_mockPrivateInstanceAAMP,
		ScheduleAsyncTask(_, _, std::string("SendMonitorAvEvent")))
		.WillOnce([](IdleTask task, void *arg, std::string) -> int {
			task(arg);
			return 1;
		});

	// The fake's SendMonitorAvEvent is a no-op; verify no crash and correct dispatch.
	m_notifiable->SendMonitorAvEvent("ok", 1000, 999, 5000, 0);
}

// ===========================================================================
// GetProgressReportIntervalSeconds
// ===========================================================================

TEST_F(PrivateInstanceAAMPNotifiableTest,
	GetProgressReportIntervalSeconds_WithNullConfig_ReturnsZero)
{
	AampConfig *config = nullptr;
	PrivateInstanceAAMP aampWithNullConfig(config);
	PrivateInstanceAAMPNotifiable notifiable(&aampWithNullConfig);

	EXPECT_DOUBLE_EQ(notifiable.GetProgressReportIntervalSeconds(), 0.0);
}

TEST_F(PrivateInstanceAAMPNotifiableTest,
	GetProgressReportIntervalSeconds_WithConfig_ForwardsToAampConfig)
{
	AampConfig config;
	PrivateInstanceAAMP aampWithConfig(&config);
	PrivateInstanceAAMPNotifiable notifiable(&aampWithConfig);

	EXPECT_CALL(*g_mockAampConfig,
		GetConfigValue(eAAMPConfig_ReportProgressInterval))
		.WillOnce(Return(0.75));

	EXPECT_DOUBLE_EQ(notifiable.GetProgressReportIntervalSeconds(), 0.75);
}
