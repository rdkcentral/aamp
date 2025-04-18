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

#include <gtest/gtest.h>
#include "MockPrivateInstanceAAMP.h"
#include "AampTsbAdReservationMetaData.h"
#include <limits>

/**
 * @brief Test fixture for AampTsbAdReservationMetaData tests.
 */
class AampTsbAdReservationMetaDataTest : public ::testing::Test
{
public:
	PrivateInstanceAAMP *mPrivateInstanceAAMP;
protected:
	void SetUp() override
	{
		g_mockPrivateInstanceAAMP = new MockPrivateInstanceAAMP();
		mPrivateInstanceAAMP = new PrivateInstanceAAMP{};
	}

	void TearDown() override
	{
		delete g_mockPrivateInstanceAAMP;
		g_mockPrivateInstanceAAMP = nullptr;

		delete mPrivateInstanceAAMP;
		mPrivateInstanceAAMP = nullptr;
	}
};

/**
 * @brief Test case to verify AampTsbAdReservationMetaData functionality.
 */
TEST_F(AampTsbAdReservationMetaDataTest, VerifyAampTsbAdReservationMetaData)
{
	AampTime position(10.0);  // 10 seconds
	std::string adBreakId = "break1";
	uint64_t periodPosition = 5000;

	AampTsbAdReservationMetaData metaData(
		AampTsbAdMetaData::EventType::START, position, adBreakId, periodPosition);

	EXPECT_EQ(metaData.GetPosition(), position)
		<< "GetPosition did not return the expected position.";
}

/**
 * @brief Test sending ad reservation events through the mock AAMP instance
 *
 * Tests the following:
 * - START event with expected parameters
 * - END event with expected parameters
 * - Verifies each event is sent exactly once
 */
TEST_F(AampTsbAdReservationMetaDataTest, ReservationEventTest)
{
	std::string adBreakId = "break123";
	uint64_t periodPosition = 1000;
	AampTime position(5.0);  // 5 seconds
	bool immediate = false;

	// Test START event
	{
		AampTsbAdReservationMetaData startMetadata(
			AampTsbAdMetaData::EventType::START,
			position, adBreakId, periodPosition);

		EXPECT_CALL(*g_mockPrivateInstanceAAMP, SendAdReservationEvent(
			AAMP_EVENT_AD_RESERVATION_START, adBreakId, periodPosition, position.milliseconds(), immediate)).Times(1);

		startMetadata.SendEvent(mPrivateInstanceAAMP);
	}

	// Test END event
	{
		AampTsbAdReservationMetaData endMetadata(
			AampTsbAdMetaData::EventType::END,
			position, adBreakId, periodPosition);

		EXPECT_CALL(*g_mockPrivateInstanceAAMP, SendAdReservationEvent(
			AAMP_EVENT_AD_RESERVATION_END, adBreakId, periodPosition, position.milliseconds(), immediate)).Times(1);

		endMetadata.SendEvent(mPrivateInstanceAAMP);
	}
}

/**
 * @brief Test error handling for invalid AAMP instance
 */
TEST_F(AampTsbAdReservationMetaDataTest, NullAampTest)
{
	AampTime position(5.0);  // 5 seconds
	AampTsbAdReservationMetaData reservationMetadata(
		AampTsbAdMetaData::EventType::START,
		position, "break123", 1000);

	EXPECT_NO_THROW(reservationMetadata.SendEvent(nullptr));
}

/**
 * @brief Test case to verify edge cases.
 */
TEST_F(AampTsbAdReservationMetaDataTest, EdgeCasesTest)
{
	// Test empty ad break ID
	{
		AampTime position(1.0);  // 1 second
		AampTsbAdReservationMetaData emptyId(
			AampTsbAdMetaData::EventType::START, position, "", 500);
	}

	// Test large values
	{
		AampTime maxPosition(std::numeric_limits<double>::max());
		uint64_t maxPeriodPosition = std::numeric_limits<uint64_t>::max();

		AampTsbAdReservationMetaData largeValues(
			AampTsbAdMetaData::EventType::START, maxPosition, "break1", maxPeriodPosition);

		EXPECT_EQ(largeValues.GetPosition(), maxPosition);
	}

	// Test zero values
	{
		AampTime zeroPosition(0.0);
		AampTsbAdReservationMetaData zeroValues(
			AampTsbAdMetaData::EventType::START, zeroPosition, "break1", 0);

		EXPECT_EQ(zeroValues.GetPosition(), zeroPosition);
	}
}

/**
 * @brief Test case to verify dump functionality.
 */
TEST_F(AampTsbAdReservationMetaDataTest, DumpTest)
{
	AampTsbAdReservationMetaData metadata(
		AampTsbAdMetaData::EventType::START, AampTime(10.0), "break1", 5000);

	// No expectations to check

	// Test dump with custom message
	std::string message = "Test Dump";
	metadata.Dump(message);
}
