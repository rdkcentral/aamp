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
#include "AampTsbAdPlacementMetaData.h"

/**
 * @brief Test fixture for AampTsbAdPlacementMetaData tests.
 */
class AampTsbAdPlacementMetaDataTest : public ::testing::Test
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
 * @brief Test case to verify AampTsbAdPlacementMetaData functionality.
 */
TEST_F(AampTsbAdPlacementMetaDataTest, VerifyAampTsbAdPlacementMetaData)
{
	AampTime position(5.0); // 5 seconds
	uint32_t duration = 10;   // 10 seconds
	std::string adId = "ad1";
	uint32_t relPosition = 100;
	uint32_t offset = 50;

	AampTsbAdPlacementMetaData metaData(
		AampTsbAdMetaData::EventType::START, position, duration, adId, relPosition, offset);

	EXPECT_EQ(metaData.GetPosition(), position)
		<< "GetPosition did not return the expected position.";
}

/**
 * @brief Test sending ad placement events through the mock AAMP instance
 *
 * Tests the following:
 * - START event with expected parameters
 * - END event with expected parameters
 * - ERROR event with expected parameters
 * - Verifies each event is sent exactly once
 */
TEST_F(AampTsbAdPlacementMetaDataTest, PlacementEventTest)
{
	// Create placement metadata with different event types
	std::string adId = "testAd123";
	uint32_t position = 100;
	AampTime positionTime(5.0); // 5 seconds
	uint32_t duration = 10;
	uint32_t offset = 50;
	bool immediate = false;

	// Test START event
	{
		AampTsbAdPlacementMetaData startMetadata(
			AampTsbAdMetaData::EventType::START,
			positionTime, duration, adId, position, offset);

		EXPECT_CALL(*g_mockPrivateInstanceAAMP, SendAdPlacementEvent(
			AAMP_EVENT_AD_PLACEMENT_START, adId, position, positionTime.milliseconds(),
			offset, duration, immediate, 0)).Times(1);

		startMetadata.SendEvent(mPrivateInstanceAAMP);
	}

	// Test END event
	{
		AampTsbAdPlacementMetaData endMetadata(
			AampTsbAdMetaData::EventType::END,
			positionTime, duration, adId, position, offset);

		EXPECT_CALL(*g_mockPrivateInstanceAAMP, SendAdPlacementEvent(
			AAMP_EVENT_AD_PLACEMENT_END, adId, position, positionTime.milliseconds(),
			offset, duration, immediate, 0)).Times(1);

		endMetadata.SendEvent(mPrivateInstanceAAMP);
	}

	// Test ERROR event
	{
		AampTsbAdPlacementMetaData errorMetadata(
			AampTsbAdMetaData::EventType::ERROR,
			positionTime, duration, adId, position, offset);

		EXPECT_CALL(*g_mockPrivateInstanceAAMP, SendAdPlacementEvent(
			AAMP_EVENT_AD_PLACEMENT_ERROR, adId, position, positionTime.milliseconds(),
			offset, duration, immediate, 0)).Times(1);

		errorMetadata.SendEvent(mPrivateInstanceAAMP);
	}
}

/**
 * @brief Test error handling for invalid AAMP instance
 */
TEST_F(AampTsbAdPlacementMetaDataTest, NullAampTest)
{
	std::string adId = "testAd123";
	uint32_t position = 100;
	AampTime positionTime(5.0); // 5 seconds
	uint32_t duration = 10;
	uint32_t offset = 50;

	// Create metadata objects
	AampTsbAdPlacementMetaData placementMetadata(
		AampTsbAdMetaData::EventType::START,
		positionTime, duration, adId, position, offset);

	// Verify no crashes when passing null AAMP instance
	EXPECT_NO_THROW(placementMetadata.SendEvent(nullptr));
}

/**
 * @brief Test case to verify edge cases
 */
TEST_F(AampTsbAdPlacementMetaDataTest, EdgeCasesTest)
{
	// Create placement metadata with different event types
	std::string adId = "testAd123";
	uint32_t position = 100;
	AampTime positionTime(5.0); // 5 seconds
	uint32_t duration = 10000;
	uint32_t offset = 50;
	bool immediate = false;

	// Test zero duration
	{
		uint32_t zeroDurationValue = 0;

		AampTsbAdPlacementMetaData zeroDuration(
			AampTsbAdMetaData::EventType::START,
			positionTime, zeroDurationValue, adId, position, offset);

		EXPECT_CALL(*g_mockPrivateInstanceAAMP, SendAdPlacementEvent(
			AAMP_EVENT_AD_PLACEMENT_START, adId, position, positionTime.milliseconds(),
			offset, zeroDurationValue, immediate, 0)).Times(1);

		zeroDuration.SendEvent(mPrivateInstanceAAMP);
	}

	// Test large values
	{
		uint32_t maxRelPos = std::numeric_limits<uint32_t>::max();
		uint32_t maxOffset = std::numeric_limits<uint32_t>::max();

		AampTsbAdPlacementMetaData largeValues(
			AampTsbAdMetaData::EventType::START, positionTime, duration, adId, maxRelPos, maxOffset);

		EXPECT_CALL(*g_mockPrivateInstanceAAMP, SendAdPlacementEvent(
			AAMP_EVENT_AD_PLACEMENT_START, adId, maxRelPos, positionTime.milliseconds(),
			maxOffset, duration, immediate, 0)).Times(1);

			largeValues.SendEvent(mPrivateInstanceAAMP);
	}

	// Test empty ad ID
	{
		AampTsbAdPlacementMetaData emptyId(
			AampTsbAdMetaData::EventType::START, positionTime, duration, "", position, offset);

		EXPECT_CALL(*g_mockPrivateInstanceAAMP, SendAdPlacementEvent(
				AAMP_EVENT_AD_PLACEMENT_START, "", position, positionTime.milliseconds(),
				offset, duration, immediate, 0)).Times(1);

		emptyId.SendEvent(mPrivateInstanceAAMP);

	}
}

/**
 * @brief Test case to verify dump functionality
 */
TEST_F(AampTsbAdPlacementMetaDataTest, DumpTest)
{
	AampTsbAdPlacementMetaData metadata(
		AampTsbAdMetaData::EventType::START, AampTime(5.0), 10, "ad1", 100, 50);

	// No expectations to check

	// Test dump with custom message
	std::string message = "Test Dump";
	metadata.Dump(message);
}

/**
 * @brief Test position setter and getter
 */
TEST_F(AampTsbAdPlacementMetaDataTest, PositionSetGet)
{
	AampTime initialPosition(10.0);  // 10 seconds
	AampTime newPosition(20.0);  // 20 seconds

	AampTsbAdPlacementMetaData metadata(
		AampTsbAdMetaData::EventType::START, initialPosition, 10, "ad1", 100, 50);
	ASSERT_EQ(metadata.GetPosition(), initialPosition);

	metadata.SetPosition(newPosition);
	EXPECT_EQ(metadata.GetPosition(), newPosition);
}
