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
	NotifySpeedChanged_ForwardsRateAndChangeState)
{
	EXPECT_CALL(*g_mockPrivateInstanceAAMP, NotifySpeedChanged(2.0f, true));

	m_notifiable->NotifySpeedChanged(2.0f, true);
}

// ===========================================================================
// Methods that call the fake stub (verify no crash / correct forwarding)
// ===========================================================================

TEST_F(PrivateInstanceAAMPNotifiableTest,
	NotifyFirstFrameReceived_ForwardsWithoutCrash)
{
	// NotifyFirstFrameReceived in the fake calls SetState internally;
	// set up the mock to handle GetState/SetState calls.
	EXPECT_CALL(*g_mockPrivateInstanceAAMP, GetState())
		.WillRepeatedly(Return(eSTATE_IDLE));
	EXPECT_CALL(*g_mockPrivateInstanceAAMP, SetState(_, _)).Times(testing::AnyNumber());

	m_notifiable->NotifyFirstFrameReceived(42);
}

TEST_F(PrivateInstanceAAMPNotifiableTest,
	NotifyFirstBufferProcessed_ForwardsWithoutCrash)
{
	m_notifiable->NotifyFirstBufferProcessed("0,0,1920,1080");
}

TEST_F(PrivateInstanceAAMPNotifiableTest,
	LogFirstFrame_ForwardsWithoutCrash)
{
	m_notifiable->LogFirstFrame();
}

TEST_F(PrivateInstanceAAMPNotifiableTest,
	LogTuneComplete_ForwardsWithoutCrash)
{
	m_notifiable->LogTuneComplete();
}

TEST_F(PrivateInstanceAAMPNotifiableTest,
	NotifyEOSReached_ForwardsWithoutCrash)
{
	m_notifiable->NotifyEOSReached();
}

TEST_F(PrivateInstanceAAMPNotifiableTest,
	MonitorProgress_ForwardsWithoutCrash)
{
	m_notifiable->MonitorProgress(true, false);
}

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
