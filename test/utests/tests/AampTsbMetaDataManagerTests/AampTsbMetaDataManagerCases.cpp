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
#include <gmock/gmock.h>
#include "AampTsbMetaDataManager.h"
#include "MockAampTsbMetaData.h"
#include <cstdint>
#include <memory>
#include <string>
#include <thread>
#include <chrono>
#include <list>
#include <algorithm>
#include <functional>
#include <atomic>

using ::testing::_;
using ::testing::Return;
using ::testing::AtLeast;
using ::testing::NiceMock;
using ::testing::StrictMock;

/**
 * @brief Base test fixture for AampTsbMetaDataManager tests
 */
class AampTsbMetaDataManagerTest : public ::testing::Test
{
protected:
	void SetUp() override
	{
		manager.reset(new AampTsbMetaDataManager());
		EXPECT_NO_THROW(manager->Initialize())
			<< "Failed to initialize manager";
	}

	void TearDown() override
	{
		if (manager)
		{
			EXPECT_NO_THROW(manager->Cleanup())
				<< "Cleanup should not throw";
			manager.reset();
		}
	}

	std::unique_ptr<AampTsbMetaDataManager> manager;
};

/**
 * @brief Test metadata type registration
 */
TEST_F(AampTsbMetaDataManagerTest, RegisterMetaDataTypeTest)
{
	bool isTransient = false;

	// Check unregistered type
	EXPECT_FALSE(manager->IsRegisteredType(AampTsbMetaData::Type::AD_METADATA_TYPE, isTransient));

	// Register a new type
	EXPECT_TRUE(manager->RegisterMetaDataType(AampTsbMetaData::Type::AD_METADATA_TYPE, false));

	// Check if types are registered
	EXPECT_TRUE(manager->IsRegisteredType(AampTsbMetaData::Type::AD_METADATA_TYPE, isTransient));
	EXPECT_FALSE(isTransient);

	// Try to register the same type again
	EXPECT_FALSE(manager->RegisterMetaDataType(AampTsbMetaData::Type::AD_METADATA_TYPE, true));

	// Check if types are registered
	EXPECT_TRUE(manager->IsRegisteredType(AampTsbMetaData::Type::AD_METADATA_TYPE, isTransient));
	EXPECT_FALSE(isTransient);
}

/**
 * @brief Test adding metadata
 */
TEST_F(AampTsbMetaDataManagerTest, AddMetaDataTest)
{
	// Register metadata type - must succeed for test to continue
	ASSERT_TRUE(manager->RegisterMetaDataType(AampTsbMetaData::Type::AD_METADATA_TYPE, false))
		<< "Failed to register metadata type";

	// Create mock metadata
	auto mockMetaData = std::make_shared<StrictMock<MockAampTsbMetaData>>();
	EXPECT_CALL(*mockMetaData, GetType()).WillRepeatedly(Return(AampTsbMetaData::Type::AD_METADATA_TYPE));
	EXPECT_CALL(*mockMetaData, GetPosition()).WillRepeatedly(Return(AampTime(10.0)));
	EXPECT_CALL(*mockMetaData, SetOrderAdded(1)).Times(1);
	EXPECT_CALL(*mockMetaData, GetOrderAdded()).WillRepeatedly(Return(1));
	EXPECT_CALL(*mockMetaData, Dump(std::string("Add "))).Times(1);

	// Add metadata - critical for test flow
	ASSERT_TRUE(manager->AddMetaData(mockMetaData))
		<< "Failed to add metadata";

	// Check size
	EXPECT_EQ(manager->GetSize(), 1);
}

/**
 * @brief Test GetMetaDataByType with range for non-transient metadata
 */
TEST_F(AampTsbMetaDataManagerTest, GetMetaDataByTypeRangeNonTransientTest)
{
	// Register metadata type as non-transient
	ASSERT_TRUE(manager->RegisterMetaDataType(AampTsbMetaData::Type::AD_METADATA_TYPE, false))
		<< "Failed to register non-transient metadata type";

	// Add mock metadata
	auto mockMetaData1 = std::make_shared<StrictMock<MockAampTsbMetaData>>();
	auto mockMetaData2 = std::make_shared<StrictMock<MockAampTsbMetaData>>();
	auto mockMetaData3 = std::make_shared<StrictMock<MockAampTsbMetaData>>();

	EXPECT_CALL(*mockMetaData1, GetType()).WillRepeatedly(Return(AampTsbMetaData::Type::AD_METADATA_TYPE));
	EXPECT_CALL(*mockMetaData1, GetPosition()).WillRepeatedly(Return(AampTime(10.0)));
	EXPECT_CALL(*mockMetaData1, SetOrderAdded(1)).Times(1);
	EXPECT_CALL(*mockMetaData1, GetOrderAdded()).WillRepeatedly(Return(1));
	EXPECT_CALL(*mockMetaData1, Dump(std::string("Add "))).Times(1);

	EXPECT_CALL(*mockMetaData2, GetType()).WillRepeatedly(Return(AampTsbMetaData::Type::AD_METADATA_TYPE));
	EXPECT_CALL(*mockMetaData2, GetPosition()).WillRepeatedly(Return(AampTime(15.0)));
	EXPECT_CALL(*mockMetaData2, SetOrderAdded(2)).Times(1);
	EXPECT_CALL(*mockMetaData2, GetOrderAdded()).WillRepeatedly(Return(2));
	EXPECT_CALL(*mockMetaData2, Dump(std::string("Add "))).Times(1);

	EXPECT_CALL(*mockMetaData3, GetType()).WillRepeatedly(Return(AampTsbMetaData::Type::AD_METADATA_TYPE));
	EXPECT_CALL(*mockMetaData3, GetPosition()).WillRepeatedly(Return(AampTime(20.0)));
	EXPECT_CALL(*mockMetaData3, SetOrderAdded(3)).Times(1);
	EXPECT_CALL(*mockMetaData3, GetOrderAdded()).WillRepeatedly(Return(3));
	EXPECT_CALL(*mockMetaData3, Dump(std::string("Add "))).Times(1);

	// Add operations must succeed for test to be valid
	ASSERT_TRUE(manager->AddMetaData(mockMetaData1))
		<< "Failed to add mockMetaData1";
	ASSERT_TRUE(manager->AddMetaData(mockMetaData2))
		<< "Failed to add mockMetaData2";
	ASSERT_TRUE(manager->AddMetaData(mockMetaData3))
		<< "Failed to add mockMetaData3";

	// Verify state - not critical for test flow
	EXPECT_EQ(manager->GetSize(), 3)
		<< "Expected 3 metadata items after adding";

	// Test 1: Range that covers mockMetaData2 and mockMetaData3 completely
	auto result = manager->GetMetaDataByType<MockAampTsbMetaData>(
		AampTsbMetaData::Type::AD_METADATA_TYPE, 15.0, 25.0);

	// Verify size and content of returned list
	ASSERT_EQ(result.size(), 2)
		<< "Expected 2 metadata items in range 15.0-25.0";

	// Check items are in correct order (by position) and match expected metadata
	auto it = result.begin();
	EXPECT_EQ(*it, mockMetaData2) << "First item should be mockMetaData2";
	EXPECT_EQ((*it)->GetPosition(), AampTime(15.0)) << "First item should be at position 15.0";
	EXPECT_EQ((*it)->GetOrderAdded(), 2) << "First item should have priority 2";

	++it;
	EXPECT_EQ(*it, mockMetaData3) << "Second item should be mockMetaData3";
	EXPECT_EQ((*it)->GetPosition(), AampTime(20.0)) << "Second item should be at position 20.0";
	EXPECT_EQ((*it)->GetOrderAdded(), 3) << "Second item should have priority 3";

	// Test 2: Range that covers only mockMetaData1
	auto result2 = manager->GetMetaDataByType<MockAampTsbMetaData>(
		AampTsbMetaData::Type::AD_METADATA_TYPE, 5.0, 12.0);

	ASSERT_EQ(result2.size(), 1) << "Expected 1 metadata item in range 5.0-12.0";
	EXPECT_EQ(result2.front(), mockMetaData1) << "Item should be mockMetaData1";
	EXPECT_EQ(result2.front()->GetPosition(), AampTime(10.0)) << "Item should be at position 10.0";
	EXPECT_EQ(result2.front()->GetOrderAdded(), 1) << "Item should have priority 1";

	// Test 2a: Range where end equals a metadata position - should exclude that metadata
	auto result2a = manager->GetMetaDataByType<MockAampTsbMetaData>(
		AampTsbMetaData::Type::AD_METADATA_TYPE, 5.0, 15.0);

	ASSERT_EQ(result2a.size(), 1)
		<< "Expected only 1 metadata item when rangeEnd equals a metadata position";
	EXPECT_EQ(result2a.front(), mockMetaData1)
		<< "Should only include mockMetaData1 and exclude mockMetaData2 at rangeEnd";

	// Test 3: Range that doesn't cover any items exactly but returns non-transient item before the range
	auto result3 = manager->GetMetaDataByType<MockAampTsbMetaData>(
		AampTsbMetaData::Type::AD_METADATA_TYPE, 11.0, 14.0);

	ASSERT_EQ(result3.size(), 1) << "Expected 1 metadata item for non-transient behavior";
	EXPECT_EQ(result3.front(), mockMetaData1) << "Should get mockMetaData1 as it's before range start";
	EXPECT_EQ(result3.front()->GetPosition(), AampTime(10.0));

	// Test 4: Position after all metadata - should return last item for non-transient
	auto result4 = manager->GetMetaDataByType<MockAampTsbMetaData>(
		AampTsbMetaData::Type::AD_METADATA_TYPE, 25.0, 30.0);

	ASSERT_EQ(result4.size(), 1) << "Expected 1 metadata item for position after all metadata";
	EXPECT_EQ(result4.front(), mockMetaData3) << "Should get mockMetaData3 as it's the last item";
}

/**
 * @brief Test GetMetaDataByType with range for transient metadata
 */
TEST_F(AampTsbMetaDataManagerTest, GetMetaDataByTypeRangeTransientTest)
{
	// Register metadata type as transient
	ASSERT_TRUE(manager->RegisterMetaDataType(AampTsbMetaData::Type::AD_METADATA_TYPE, true))
		<< "Failed to register transient metadata type";

	// Add mock metadata
	auto mockMetaData1 = std::make_shared<StrictMock<MockAampTsbMetaData>>();
	auto mockMetaData2 = std::make_shared<StrictMock<MockAampTsbMetaData>>();
	auto mockMetaData3 = std::make_shared<StrictMock<MockAampTsbMetaData>>();

	EXPECT_CALL(*mockMetaData1, GetType()).WillRepeatedly(Return(AampTsbMetaData::Type::AD_METADATA_TYPE));
	EXPECT_CALL(*mockMetaData1, GetPosition()).WillRepeatedly(Return(AampTime(10.0)));
	EXPECT_CALL(*mockMetaData1, SetOrderAdded(1)).Times(1);
	EXPECT_CALL(*mockMetaData1, GetOrderAdded()).WillRepeatedly(Return(1));
	EXPECT_CALL(*mockMetaData1, Dump(std::string("Add "))).Times(1);

	EXPECT_CALL(*mockMetaData2, GetType()).WillRepeatedly(Return(AampTsbMetaData::Type::AD_METADATA_TYPE));
	EXPECT_CALL(*mockMetaData2, GetPosition()).WillRepeatedly(Return(AampTime(15.0)));
	EXPECT_CALL(*mockMetaData2, SetOrderAdded(2)).Times(1);
	EXPECT_CALL(*mockMetaData2, GetOrderAdded()).WillRepeatedly(Return(2));
	EXPECT_CALL(*mockMetaData2, Dump(std::string("Add "))).Times(1);

	EXPECT_CALL(*mockMetaData3, GetType()).WillRepeatedly(Return(AampTsbMetaData::Type::AD_METADATA_TYPE));
	EXPECT_CALL(*mockMetaData3, GetPosition()).WillRepeatedly(Return(AampTime(20.0)));
	EXPECT_CALL(*mockMetaData3, SetOrderAdded(3)).Times(1);
	EXPECT_CALL(*mockMetaData3, GetOrderAdded()).WillRepeatedly(Return(3));
	EXPECT_CALL(*mockMetaData3, Dump(std::string("Add "))).Times(1);

	// Add operations must succeed for test to be valid
	ASSERT_TRUE(manager->AddMetaData(mockMetaData1))
		<< "Failed to add mockMetaData1";
	ASSERT_TRUE(manager->AddMetaData(mockMetaData2))
		<< "Failed to add mockMetaData2";
	ASSERT_TRUE(manager->AddMetaData(mockMetaData3))
		<< "Failed to add mockMetaData3";

	// Verify state - not critical for test flow
	EXPECT_EQ(manager->GetSize(), 3)
		<< "Expected 3 metadata items after adding";

	// Test 1: Range that includes position of mockMetaData2
	auto result = manager->GetMetaDataByType<MockAampTsbMetaData>(
		AampTsbMetaData::Type::AD_METADATA_TYPE, 15.0, 16.0);

	ASSERT_EQ(result.size(), 1)
		<< "Expected exactly 1 metadata item for exact match with transient type";
	EXPECT_EQ(result.front(), mockMetaData2)
		<< "Should be mockMetaData2 at exactly 15.0";

	// Test 2: Range that spans multiple positions
	auto result2 = manager->GetMetaDataByType<MockAampTsbMetaData>(
		AampTsbMetaData::Type::AD_METADATA_TYPE, 15.0, 25.0);

	ASSERT_EQ(result2.size(), 2)
		<< "Expected exactly 2 metadata items for range including two positions";
	auto it = result2.begin();
	EXPECT_EQ(*it++, mockMetaData2) << "First should be mockMetaData2";
	EXPECT_EQ(*it, mockMetaData3) << "Second should be mockMetaData3";

	// Test 3: Range where end equals a metadata position - should exclude that metadata
	auto result2a = manager->GetMetaDataByType<MockAampTsbMetaData>(
		AampTsbMetaData::Type::AD_METADATA_TYPE, 15.0, 20.0);

	ASSERT_EQ(result2a.size(), 1)
		<< "Expected only 1 metadata item when rangeEnd equals a metadata position";
	EXPECT_EQ(result2a.front(), mockMetaData2)
		<< "Should only include mockMetaData2 and exclude mockMetaData3 at rangeEnd";

	// Test 4: Range that doesn't include any exact positions - should return empty list for transient
	auto result3 = manager->GetMetaDataByType<MockAampTsbMetaData>(
		AampTsbMetaData::Type::AD_METADATA_TYPE, 11.0, 14.0);

	EXPECT_EQ(result3.size(), 0)
		<< "Expected 0 metadata items when no exact positions in range (transient behavior)";

	// Test 5: Position before any metadata - should return empty list for transient
	auto result4 = manager->GetMetaDataByType<MockAampTsbMetaData>(
		AampTsbMetaData::Type::AD_METADATA_TYPE, 5.0, 8.0);

	EXPECT_EQ(result4.size(), 0)
		<< "Expected 0 metadata items for range before any metadata (transient behavior)";

	// Test 6: Position after all metadata - should return empty list for transient
	auto result5 = manager->GetMetaDataByType<MockAampTsbMetaData>(
		AampTsbMetaData::Type::AD_METADATA_TYPE, 25.0, 30.0);

	EXPECT_EQ(result5.size(), 0)
		<< "Expected 0 metadata items for range after all metadata (transient behavior)";
}

/**
 * @brief Test priority handling in GetMetaDataByType
 */
TEST_F(AampTsbMetaDataManagerTest, PriorityHandlingTest)
{
	manager->RegisterMetaDataType(AampTsbMetaData::Type::AD_METADATA_TYPE, false);

	// Create mocks with behavior setup (supporting methods)
	auto mockMetaDataHighPriority = std::make_shared<NiceMock<MockAampTsbMetaData>>();
	auto mockMetaDataLowPriority = std::make_shared<NiceMock<MockAampTsbMetaData>>();

	// Verify critical behaviors that must occur
	EXPECT_CALL(*mockMetaDataLowPriority, GetType()).WillRepeatedly(Return(AampTsbMetaData::Type::AD_METADATA_TYPE));
	EXPECT_CALL(*mockMetaDataLowPriority, GetPosition()).WillRepeatedly(Return(AampTime(10.0)));
	EXPECT_CALL(*mockMetaDataLowPriority, SetOrderAdded(1)).Times(1);
	EXPECT_CALL(*mockMetaDataLowPriority, GetOrderAdded()).WillRepeatedly(Return(1));
	EXPECT_CALL(*mockMetaDataLowPriority, Dump(std::string("Add "))).Times(1);

	EXPECT_CALL(*mockMetaDataHighPriority, GetType()).WillRepeatedly(Return(AampTsbMetaData::Type::AD_METADATA_TYPE));
	EXPECT_CALL(*mockMetaDataHighPriority, GetPosition()).WillRepeatedly(Return(AampTime(10.0)));
	EXPECT_CALL(*mockMetaDataHighPriority, SetOrderAdded(2)).Times(1);
	EXPECT_CALL(*mockMetaDataHighPriority, GetOrderAdded()).WillRepeatedly(Return(2));
	EXPECT_CALL(*mockMetaDataHighPriority, Dump(std::string("Add "))).Times(1);

	// Adding metadata is critical for test flow
	ASSERT_TRUE(manager->AddMetaData(mockMetaDataLowPriority))
		<< "Failed to add low priority metadata";
	ASSERT_TRUE(manager->AddMetaData(mockMetaDataHighPriority))
		<< "Failed to add high priority metadata";

	// Get metadata at exact position - both should be returned with low priority first
	auto result1 = manager->GetMetaDataByType<MockAampTsbMetaData>(
		AampTsbMetaData::Type::AD_METADATA_TYPE, 10.0, 10.0);

	EXPECT_EQ(result1.size(), 2);
	EXPECT_EQ(result1.front()->GetOrderAdded(), 1);
	EXPECT_EQ(result1.back()->GetOrderAdded(), 2);

	// Get metadata with position after - higher priority should be returned
	auto result2 = manager->GetMetaDataByType<MockAampTsbMetaData>(
		AampTsbMetaData::Type::AD_METADATA_TYPE, 11.0, 11.0);

	EXPECT_EQ(result2.size(), 1);
	EXPECT_EQ(result2.front()->GetOrderAdded(), 2);
}

/**
 * @brief Test GetMetaDataByType with lambda filter
 */
TEST_F(AampTsbMetaDataManagerTest, FilteredMetaDataTest)
{
	// Register metadata types
	ASSERT_TRUE(manager->RegisterMetaDataType(AampTsbMetaData::Type::AD_METADATA_TYPE, false))
		<< "Failed to register metadata type";

	// Add mock metadata
	auto mockMetaData1 = std::make_shared<NiceMock<MockAampTsbMetaData>>();
	auto mockMetaData2 = std::make_shared<NiceMock<MockAampTsbMetaData>>();
	auto mockMetaData3 = std::make_shared<NiceMock<MockAampTsbMetaData>>();

	EXPECT_CALL(*mockMetaData1, GetType()).WillRepeatedly(Return(AampTsbMetaData::Type::AD_METADATA_TYPE));
	EXPECT_CALL(*mockMetaData1, GetPosition()).WillRepeatedly(Return(AampTime(10.0)));
	EXPECT_CALL(*mockMetaData1, SetOrderAdded(1)).Times(1);
	EXPECT_CALL(*mockMetaData1, Dump(std::string("Add "))).Times(1);

	EXPECT_CALL(*mockMetaData2, GetType()).WillRepeatedly(Return(AampTsbMetaData::Type::AD_METADATA_TYPE));
	EXPECT_CALL(*mockMetaData2, GetPosition()).WillRepeatedly(Return(AampTime(15.0)));
	EXPECT_CALL(*mockMetaData2, SetOrderAdded(2)).Times(1);
	EXPECT_CALL(*mockMetaData2, Dump(std::string("Add "))).Times(1);

	EXPECT_CALL(*mockMetaData3, GetType()).WillRepeatedly(Return(AampTsbMetaData::Type::AD_METADATA_TYPE));
	EXPECT_CALL(*mockMetaData3, GetPosition()).WillRepeatedly(Return(AampTime(20.0)));
	EXPECT_CALL(*mockMetaData3, SetOrderAdded(3)).Times(1);
	EXPECT_CALL(*mockMetaData3, Dump(std::string("Add "))).Times(1);

	ASSERT_TRUE(manager->AddMetaData(mockMetaData1))
		<< "Failed to add mockMetaData1";
	ASSERT_TRUE(manager->AddMetaData(mockMetaData2))
		<< "Failed to add mockMetaData2";
	ASSERT_TRUE(manager->AddMetaData(mockMetaData3))
		<< "Failed to add mockMetaData3";

	// Filter for specific position
	auto filteredMetaData = manager->GetMetaDataByType<MockAampTsbMetaData>(
		AampTsbMetaData::Type::AD_METADATA_TYPE,
		[](const std::shared_ptr<MockAampTsbMetaData>& md) {
			return md->GetPosition() == AampTime(15.0);
		});

	EXPECT_EQ(filteredMetaData.size(), 1);
	EXPECT_EQ(filteredMetaData.front()->GetPosition(), AampTime(15.0));

	// Test with null filter (should return all)
	auto allMetaData = manager->GetMetaDataByType<MockAampTsbMetaData>(
		AampTsbMetaData::Type::AD_METADATA_TYPE, nullptr);
	EXPECT_EQ(allMetaData.size(), 3);

	// Test with a filter that returns false for all
	auto noMetaData = manager->GetMetaDataByType<MockAampTsbMetaData>(
		AampTsbMetaData::Type::AD_METADATA_TYPE,
		[](const std::shared_ptr<MockAampTsbMetaData>&) { return false; });
	EXPECT_EQ(noMetaData.size(), 0);
}

/**
 * @brief Test removing metadata
 */
TEST_F(AampTsbMetaDataManagerTest, RemoveMetaDataTest)
{
	ASSERT_TRUE(manager->RegisterMetaDataType(AampTsbMetaData::Type::AD_METADATA_TYPE, false))
		<< "Failed to register metadata type";

	auto mockMetaData = std::make_shared<StrictMock<MockAampTsbMetaData>>();
	EXPECT_CALL(*mockMetaData, GetType()).WillRepeatedly(Return(AampTsbMetaData::Type::AD_METADATA_TYPE));
	EXPECT_CALL(*mockMetaData, GetPosition()).WillRepeatedly(Return(AampTime(1.0)));
	EXPECT_CALL(*mockMetaData, GetOrderAdded()).WillRepeatedly(Return(1));
	EXPECT_CALL(*mockMetaData, SetOrderAdded(1)).Times(1);
	EXPECT_CALL(*mockMetaData, Dump(std::string("Add "))).Times(1);
	EXPECT_CALL(*mockMetaData, Dump("Erase ")).Times(1);

	ASSERT_TRUE(manager->AddMetaData(mockMetaData))
		<< "Failed to add metadata";
	EXPECT_EQ(manager->GetSize(), 1)
		<< "Expected size 1 after adding metadata";

	ASSERT_TRUE(manager->RemoveMetaData(mockMetaData))
		<< "Failed to remove metadata";
	EXPECT_EQ(manager->GetSize(), 0)
		<< "Expected size 0 after removing metadata";
}

/**
 * @brief Test removing metadata up to a position
 */
TEST_F(AampTsbMetaDataManagerTest, RemoveNonTransientMetaDataByPositionTest)
{
	ASSERT_TRUE(manager->RegisterMetaDataType(AampTsbMetaData::Type::AD_METADATA_TYPE, false))
		<< "Failed to register metadata type";

	// Add mock metadata
	auto mockMetaData1 = std::make_shared<NiceMock<MockAampTsbMetaData>>();
	auto mockMetaData2 = std::make_shared<NiceMock<MockAampTsbMetaData>>();
	auto mockMetaData3 = std::make_shared<NiceMock<MockAampTsbMetaData>>();

	EXPECT_CALL(*mockMetaData1, GetType()).WillRepeatedly(Return(AampTsbMetaData::Type::AD_METADATA_TYPE));
	EXPECT_CALL(*mockMetaData1, GetPosition()).WillRepeatedly(Return(AampTime(10.0)));
	EXPECT_CALL(*mockMetaData1, SetOrderAdded(1)).Times(1);
	EXPECT_CALL(*mockMetaData1, Dump(std::string("Add "))).Times(1);
	EXPECT_CALL(*mockMetaData1, Dump("Erase (non-transient) 15000ms ")).Times(1);  // Fixed position

	EXPECT_CALL(*mockMetaData2, GetType()).WillRepeatedly(Return(AampTsbMetaData::Type::AD_METADATA_TYPE));
	EXPECT_CALL(*mockMetaData2, GetPosition()).WillRepeatedly(Return(AampTime(15.0)));
	EXPECT_CALL(*mockMetaData2, SetOrderAdded(2)).Times(1);
	EXPECT_CALL(*mockMetaData2, Dump(std::string("Add "))).Times(1);
	EXPECT_CALL(*mockMetaData2, Dump("Erase (non-transient) 30000ms ")).Times(1);  // Fixed position

	EXPECT_CALL(*mockMetaData3, GetType()).WillRepeatedly(Return(AampTsbMetaData::Type::AD_METADATA_TYPE));
	EXPECT_CALL(*mockMetaData3, GetPosition()).WillRepeatedly(Return(AampTime(20.0)));
	EXPECT_CALL(*mockMetaData3, SetOrderAdded(3)).Times(1);
	EXPECT_CALL(*mockMetaData3, Dump(_)).Times(0);
	EXPECT_CALL(*mockMetaData3, Dump(std::string("Add "))).Times(1);

	ASSERT_TRUE(manager->AddMetaData(mockMetaData1))
		<< "Failed to add mockMetaData1";
	ASSERT_TRUE(manager->AddMetaData(mockMetaData2))
		<< "Failed to add mockMetaData2";
	ASSERT_TRUE(manager->AddMetaData(mockMetaData3))
		<< "Failed to add mockMetaData3";
	EXPECT_EQ(manager->GetSize(), 3)
		<< "Expected 3 metadata items after adding";

	// Remove metadata up to and including position 15.0
	EXPECT_EQ(1, manager->RemoveMetaData(15.0))
		<< "Failed to remove metadata up to position 15.0";
	EXPECT_EQ(manager->GetSize(), 2)
		<< "Expected 2 metadata items remaining after removal";

	// Verify mockMetaData2 and mockMetaData3 remain
	// mockMetaData2 remains because it is the ative metadata from 15-20
	auto result = manager->GetMetaDataByType<MockAampTsbMetaData>(AampTsbMetaData::Type::AD_METADATA_TYPE, 0, 30.0);
	ASSERT_EQ(result.size(), 2);
	auto it = result.begin();
	EXPECT_EQ((*it++)->GetPosition(), AampTime(15.0));  // mockMetaData2
	EXPECT_EQ((*it)->GetPosition(), AampTime(20.0));    // mockMetaData3

	// Remove at position with no metadata
	EXPECT_EQ(0, manager->RemoveMetaData(5.0))
		<< "Failed to remove metadata at position 5.0";
	EXPECT_EQ(manager->GetSize(), 2); // Size unchanged

	// Remove metadata at a position beyond mockMetaData3
	EXPECT_EQ(1, manager->RemoveMetaData(30.0))
		<< "Failed to remove metadata up to position 30.0";
	EXPECT_EQ(manager->GetSize(), 1) 
		<< "Expected mockMetaData3 to remain after removal";

	// Verify mockMetaData3 remains (highest priority at or before 30.0)
	auto finalResult = manager->GetMetaDataByType<MockAampTsbMetaData>(
		AampTsbMetaData::Type::AD_METADATA_TYPE, 0, 30.0);
	ASSERT_EQ(finalResult.size(), 1);
	EXPECT_EQ(finalResult.front()->GetPosition(), AampTime(20.0)); // mockMetaData3
}

/**
 * @brief Test removing transient metadata up to a position
 */
TEST_F(AampTsbMetaDataManagerTest, RemoveTransientMetaDataByPositionTest)
{
	ASSERT_TRUE(manager->RegisterMetaDataType(AampTsbMetaData::Type::AD_METADATA_TYPE, true))
		<< "Failed to register transient metadata type";

	// Add mock metadata with varied positions
	auto mockMetaData1 = std::make_shared<NiceMock<MockAampTsbMetaData>>();
	auto mockMetaData2 = std::make_shared<NiceMock<MockAampTsbMetaData>>();
	auto mockMetaData3 = std::make_shared<NiceMock<MockAampTsbMetaData>>();
	auto mockMetaData4 = std::make_shared<NiceMock<MockAampTsbMetaData>>();

	// First metadata at position 10.0
	EXPECT_CALL(*mockMetaData1, GetType()).WillRepeatedly(Return(AampTsbMetaData::Type::AD_METADATA_TYPE));
	EXPECT_CALL(*mockMetaData1, GetPosition()).WillRepeatedly(Return(AampTime(10.0)));
	EXPECT_CALL(*mockMetaData1, SetOrderAdded(1)).Times(1);
	EXPECT_CALL(*mockMetaData1, Dump(std::string("Add "))).Times(1);

	// Second metadata at same position as first
	EXPECT_CALL(*mockMetaData2, GetType()).WillRepeatedly(Return(AampTsbMetaData::Type::AD_METADATA_TYPE));
	EXPECT_CALL(*mockMetaData2, GetPosition()).WillRepeatedly(Return(AampTime(10.0)));
	EXPECT_CALL(*mockMetaData2, SetOrderAdded(2)).Times(1);
	EXPECT_CALL(*mockMetaData2, Dump(std::string("Add "))).Times(1);

	// Third metadata at position 15.0
	EXPECT_CALL(*mockMetaData3, GetType()).WillRepeatedly(Return(AampTsbMetaData::Type::AD_METADATA_TYPE));
	EXPECT_CALL(*mockMetaData3, GetPosition()).WillRepeatedly(Return(AampTime(15.0)));
	EXPECT_CALL(*mockMetaData3, SetOrderAdded(3)).Times(1);
	EXPECT_CALL(*mockMetaData3, Dump(std::string("Add "))).Times(1);

	// Fourth metadata at position 20.0
	EXPECT_CALL(*mockMetaData4, GetType()).WillRepeatedly(Return(AampTsbMetaData::Type::AD_METADATA_TYPE));
	EXPECT_CALL(*mockMetaData4, GetPosition()).WillRepeatedly(Return(AampTime(20.0)));
	EXPECT_CALL(*mockMetaData4, SetOrderAdded(4)).Times(1);
	EXPECT_CALL(*mockMetaData4, Dump(std::string("Add "))).Times(1);

	ASSERT_TRUE(manager->AddMetaData(mockMetaData1));
	ASSERT_TRUE(manager->AddMetaData(mockMetaData2));
	ASSERT_TRUE(manager->AddMetaData(mockMetaData3));
	ASSERT_TRUE(manager->AddMetaData(mockMetaData4));
	EXPECT_EQ(manager->GetSize(), 4)
		<< "Expected 4 metadata items after adding";

	// Edge case 1: Remove at position before any metadata
	EXPECT_CALL(*mockMetaData1, Dump(_)).Times(0);
	EXPECT_CALL(*mockMetaData2, Dump(_)).Times(0);
	EXPECT_CALL(*mockMetaData3, Dump(_)).Times(0);
	EXPECT_CALL(*mockMetaData4, Dump(_)).Times(0);

	EXPECT_EQ(0, manager->RemoveMetaData(5.0))
		<< "Failed to remove metadata up to position 5.0";
	EXPECT_EQ(manager->GetSize(), 4)
		<< "Expected all metadata to remain when removal position is before all items";

	// Edge case 2: Remove at exactly first metadata position
	EXPECT_CALL(*mockMetaData1, Dump("Erase (transient) 10000ms ")).Times(1);
	EXPECT_CALL(*mockMetaData2, Dump("Erase (transient) 10000ms ")).Times(1);
	EXPECT_CALL(*mockMetaData3, Dump(_)).Times(0);
	EXPECT_CALL(*mockMetaData4, Dump(_)).Times(0);

	EXPECT_EQ(2, manager->RemoveMetaData(10.0))
		<< "Failed to remove metadata up to position 10.0";
	EXPECT_EQ(manager->GetSize(), 2)
		<< "Expected metadata at position 10000 to be removed";

	// Edge case 3: Remove at position between metadata
	EXPECT_CALL(*mockMetaData3, Dump("Erase (transient) 17500ms ")).Times(1);
	EXPECT_CALL(*mockMetaData4, Dump(_)).Times(0);

	EXPECT_EQ(1, manager->RemoveMetaData(17.5))
		<< "Failed to remove metadata up to position 17500";
	EXPECT_EQ(manager->GetSize(), 1)
		<< "Expected only metadata after position 17500 to remain";

	// Verify only mockMetaData4 remains
	auto result = manager->GetMetaDataByType<MockAampTsbMetaData>(
		AampTsbMetaData::Type::AD_METADATA_TYPE, 0, 30.0);
	ASSERT_EQ(result.size(), 1)
		<< "Expected exactly one metadata item to remain";
	EXPECT_EQ(result.front()->GetPosition(), AampTime(20.0))
		<< "Expected remaining metadata to be at position 20.0";

	// Edge case 4: Remove at position after all metadata
	EXPECT_CALL(*mockMetaData4, Dump("Erase (transient) 25000ms ")).Times(1);

	EXPECT_EQ(1, manager->RemoveMetaData(25.0))
		<< "Failed to remove metadata up to position 25.0";
	EXPECT_EQ(manager->GetSize(), 0)
		<< "Expected no metadata to remain after removing past all positions";
}

/**
 * @brief Test removing metadata with custom filter
 */
TEST_F(AampTsbMetaDataManagerTest, RemoveMetaDataIfTest)
{
	ASSERT_TRUE(manager->RegisterMetaDataType(AampTsbMetaData::Type::AD_METADATA_TYPE, false))
		<< "Failed to register metadata type";

	// Add mock metadata
	auto mockMetaData1 = std::make_shared<NiceMock<MockAampTsbMetaData>>();
	auto mockMetaData2 = std::make_shared<NiceMock<MockAampTsbMetaData>>();
	auto mockMetaData3 = std::make_shared<NiceMock<MockAampTsbMetaData>>();

	EXPECT_CALL(*mockMetaData1, GetType()).WillRepeatedly(Return(AampTsbMetaData::Type::AD_METADATA_TYPE));
	EXPECT_CALL(*mockMetaData1, GetPosition()).WillRepeatedly(Return(AampTime(10.0)));
	EXPECT_CALL(*mockMetaData1, SetOrderAdded(1)).Times(1);
	EXPECT_CALL(*mockMetaData1, Dump(std::string("Add "))).Times(1);
	EXPECT_CALL(*mockMetaData1, Dump("Erase ")).Times(1);  // Fixed format

	EXPECT_CALL(*mockMetaData2, GetType()).WillRepeatedly(Return(AampTsbMetaData::Type::AD_METADATA_TYPE));
	EXPECT_CALL(*mockMetaData2, GetPosition()).WillRepeatedly(Return(AampTime(15.0)));
	EXPECT_CALL(*mockMetaData2, SetOrderAdded(2)).Times(1);
	EXPECT_CALL(*mockMetaData2, Dump(std::string("Add "))).Times(1);
	EXPECT_CALL(*mockMetaData2, Dump("Erase ")).Times(1);  // Fixed format

	EXPECT_CALL(*mockMetaData3, GetType()).WillRepeatedly(Return(AampTsbMetaData::Type::AD_METADATA_TYPE));
	EXPECT_CALL(*mockMetaData3, GetPosition()).WillRepeatedly(Return(AampTime(20.0)));
	EXPECT_CALL(*mockMetaData3, SetOrderAdded(3)).Times(1);
	EXPECT_CALL(*mockMetaData3, Dump(std::string("Add "))).Times(1);
	EXPECT_CALL(*mockMetaData3, Dump("Erase ")).Times(1);  // Fixed format

	ASSERT_TRUE(manager->AddMetaData(mockMetaData1))
		<< "Failed to add mockMetaData1";
	ASSERT_TRUE(manager->AddMetaData(mockMetaData2))
		<< "Failed to add mockMetaData2";
	ASSERT_TRUE(manager->AddMetaData(mockMetaData3))
		<< "Failed to add mockMetaData3";
	EXPECT_EQ(manager->GetSize(), 3)
		<< "Expected 3 metadata items after adding";

	// Remove metadata with position > 10.0
	size_t removed = manager->RemoveMetaDataIf(
		[](const std::shared_ptr<AampTsbMetaData>& md) {
			return md->GetPosition() > AampTime(10.0);
		});

	EXPECT_EQ(removed, 2)
		<< "Expected 2 metadata items to be removed";

	auto result = manager->GetMetaDataByType<MockAampTsbMetaData>(
		AampTsbMetaData::Type::AD_METADATA_TYPE, 0, 30.0);
	ASSERT_EQ(result.size(), 1)
		<< "Expected exactly one metadata item to remain";
	EXPECT_EQ(result.front()->GetPosition(), AampTime(10.0))
		<< "Expected remaining metadata to be at position 10.0";

	// Remove with a filter that matches nothing
	removed = manager->RemoveMetaDataIf(
		[](const std::shared_ptr<AampTsbMetaData>&) { return false; });

	EXPECT_EQ(removed, 0)
		<< "Expected no metadata items to be removed";
	EXPECT_EQ(manager->GetSize(), 1)
		<< "Expected 1 metadata item remaining";

	// Remove remaining metadata
	removed = manager->RemoveMetaDataIf(
		[](const std::shared_ptr<AampTsbMetaData>&) { return true; });

	EXPECT_EQ(removed, 1)
		<< "Expected 1 metadata item to be removed";
	EXPECT_EQ(manager->GetSize(), 0)
		<< "Expected no metadata items remaining";
}

/**
 * @brief Test thread safety with concurrent access
 */
TEST_F(AampTsbMetaDataManagerTest, ThreadSafetyTest)
{
	ASSERT_TRUE(manager->RegisterMetaDataType(AampTsbMetaData::Type::AD_METADATA_TYPE, false));

	std::vector<std::thread> threads;
	std::atomic<bool> running{true};

	// Pre-create a single mock to minimize thread contention
	auto mockMetaData = std::make_shared<NiceMock<MockAampTsbMetaData>>();
	EXPECT_CALL(*mockMetaData, GetType()).WillRepeatedly(Return(AampTsbMetaData::Type::AD_METADATA_TYPE));
	EXPECT_CALL(*mockMetaData, GetPosition()).WillRepeatedly(Return(AampTime(1.0)));

	// Thread 1: Add metadata repeatedly
	threads.emplace_back([this, mockMetaData, &running]()
	{
		while (running)
		{
			(void)manager->AddMetaData(mockMetaData);
			std::this_thread::sleep_for(std::chrono::milliseconds(1));
		}
	});

	// Thread 2: Read metadata repeatedly
	threads.emplace_back([this, &running]()
	{
		while (running)
		{
			(void)manager->GetMetaDataByType<MockAampTsbMetaData>(AampTsbMetaData::Type::AD_METADATA_TYPE, 1.0, 3.0);
			std::this_thread::sleep_for(std::chrono::milliseconds(1));
		}
	});

	// Thread 3: Remove metadata repeatedly
	threads.emplace_back([this, &running]()
	{
		while (running)
		{
			(void)manager->RemoveMetaData(1.0);
			std::this_thread::sleep_for(std::chrono::milliseconds(1));
		}
	});

	// Let threads run for a short time
	std::this_thread::sleep_for(std::chrono::milliseconds(50));
	running = false;

	// Join threads
	for (auto& thread : threads)
	{
		thread.join();
	}

	// Verify the manager is still in a valid state
	(void)manager->RemoveMetaData(UINT64_MAX);
	EXPECT_EQ(manager->GetSize(), 1);
}

/**
 * @brief Test that adding duplicate metadata objects is prevented
 */
TEST_F(AampTsbMetaDataManagerTest, DuplicateMetaDataTest)
{
	ASSERT_TRUE(manager->RegisterMetaDataType(AampTsbMetaData::Type::AD_METADATA_TYPE, false));

	// Create mock metadata
	auto mockMetaData = std::make_shared<StrictMock<MockAampTsbMetaData>>();
	EXPECT_CALL(*mockMetaData, GetType()).WillRepeatedly(Return(AampTsbMetaData::Type::AD_METADATA_TYPE));
	EXPECT_CALL(*mockMetaData, GetPosition()).WillRepeatedly(Return(AampTime(1.0)));
	EXPECT_CALL(*mockMetaData, SetOrderAdded(1)).Times(1);
	EXPECT_CALL(*mockMetaData, GetOrderAdded()).WillRepeatedly(Return(1));
	EXPECT_CALL(*mockMetaData, Dump(std::string("Add "))).Times(1);

	// Add metadata - should succeed
	ASSERT_TRUE(manager->AddMetaData(mockMetaData))
		<< "Failed to add metadata first time";

	// Try to add the same metadata object again - should fail
	EXPECT_FALSE(manager->AddMetaData(mockMetaData))
		<< "Should not be able to add same metadata object twice";

	// Verify size is still 1
	EXPECT_EQ(manager->GetSize(), 1)
		<< "Size should still be 1 after failed duplicate add";
}

/**
 * @brief Test changing metadata positions
 */
TEST_F(AampTsbMetaDataManagerTest, ChangeMetaDataPositionTest)
{
	ASSERT_TRUE(manager->RegisterMetaDataType(AampTsbMetaData::Type::AD_METADATA_TYPE, false))
		<< "Failed to register metadata type";

	// Create mock metadata with different priorities
	auto mockMetaData1 = std::make_shared<StrictMock<MockAampTsbMetaData>>();
	auto mockMetaData2 = std::make_shared<StrictMock<MockAampTsbMetaData>>();
	auto mockMetaData3 = std::make_shared<StrictMock<MockAampTsbMetaData>>();

	// First metadata with lower priority
	EXPECT_CALL(*mockMetaData1, GetType()).WillRepeatedly(Return(AampTsbMetaData::Type::AD_METADATA_TYPE));
	EXPECT_CALL(*mockMetaData1, GetPosition())
		.WillOnce(Return(10.0))   // Initial add
		.WillOnce(Return(10.0))   // During position change
		.WillRepeatedly(Return(20.0));  // After position change
		EXPECT_CALL(*mockMetaData1, SetOrderAdded(1)).Times(1);
		EXPECT_CALL(*mockMetaData1, GetOrderAdded()).WillRepeatedly(Return(1));
	EXPECT_CALL(*mockMetaData1, SetPosition(AampTime(20.0))).Times(1);
	EXPECT_CALL(*mockMetaData1, Dump(std::string("Add "))).Times(1);
	EXPECT_CALL(*mockMetaData1, Dump("Change Pos ")).Times(1);

	// Second metadata with medium priority
	EXPECT_CALL(*mockMetaData2, GetType()).WillRepeatedly(Return(AampTsbMetaData::Type::AD_METADATA_TYPE));
	EXPECT_CALL(*mockMetaData2, GetPosition())
		.WillOnce(Return(15.0))   // Initial add
		.WillOnce(Return(15.0))   // During position change
		.WillRepeatedly(Return(20.0));  // After position change
	EXPECT_CALL(*mockMetaData2, SetOrderAdded(2)).Times(1);
	EXPECT_CALL(*mockMetaData2, GetOrderAdded()).WillRepeatedly(Return(2));
	EXPECT_CALL(*mockMetaData2, SetPosition(AampTime(20.0))).Times(1);
	EXPECT_CALL(*mockMetaData2, Dump(std::string("Add "))).Times(1);
	EXPECT_CALL(*mockMetaData2, Dump("Change Pos ")).Times(1);

	// Third metadata with highest priority at target position - not being moved
	EXPECT_CALL(*mockMetaData3, GetType()).WillRepeatedly(Return(AampTsbMetaData::Type::AD_METADATA_TYPE));
	EXPECT_CALL(*mockMetaData3, GetPosition())
		.WillRepeatedly(Return(20.0));  // Already at target position
	EXPECT_CALL(*mockMetaData3, SetOrderAdded(3)).Times(1);
	EXPECT_CALL(*mockMetaData3, GetOrderAdded()).WillRepeatedly(Return(3));
	EXPECT_CALL(*mockMetaData3, SetPosition(AampTime(20.0))).Times(1);
	EXPECT_CALL(*mockMetaData3, Dump(std::string("Add "))).Times(1);
	EXPECT_CALL(*mockMetaData3, Dump("Change Pos ")).Times(1);

	// Add metadata in order of priority
	ASSERT_TRUE(manager->AddMetaData(mockMetaData1))
		<< "Failed to add mockMetaData1";
	ASSERT_TRUE(manager->AddMetaData(mockMetaData2))
		<< "Failed to add mockMetaData2";
	ASSERT_TRUE(manager->AddMetaData(mockMetaData3))
		<< "Failed to add mockMetaData3";

	// Change positions of first two metadata to same position as third
	std::list<std::shared_ptr<AampTsbMetaData>> updateList = {mockMetaData1, mockMetaData2, mockMetaData3};

	EXPECT_TRUE(manager->ChangeMetaDataPosition(updateList, 20.0))
		<< "Failed to change metadata positions";

	// Verify ordering by priority at same position
	auto result = manager->GetMetaDataByType<MockAampTsbMetaData>(
		AampTsbMetaData::Type::AD_METADATA_TYPE, 20.0, 20.0);
	ASSERT_EQ(result.size(), 3)
		<< "Expected all three metadata items at position";

	auto it = result.begin();
	EXPECT_EQ((*it++)->GetOrderAdded(), 1) << "Expected lowest priority first";
	EXPECT_EQ((*it++)->GetOrderAdded(), 2) << "Expected medium priority second";
	EXPECT_EQ((*it)->GetOrderAdded(), 3) << "Expected highest priority last";

	// Test edge cases
	EXPECT_FALSE(manager->ChangeMetaDataPosition({}, 30.0))
		<< "Should fail with empty list";

	auto notInManagerMetaData = std::make_shared<StrictMock<MockAampTsbMetaData>>();
	EXPECT_CALL(*notInManagerMetaData, GetType()).WillOnce(Return(AampTsbMetaData::Type::AD_METADATA_TYPE));
	EXPECT_FALSE(manager->ChangeMetaDataPosition({notInManagerMetaData}, 30.0))
		<< "Should fail with metadata not in manager";
}

/**
 * @brief Test manager's Dump functionality
 */
TEST_F(AampTsbMetaDataManagerTest, DumpTest)
{
	// Verify Dump call with no registered metadata, no expectations to check
	manager->Dump();

	// Register different types
	ASSERT_TRUE(manager->RegisterMetaDataType(AampTsbMetaData::Type::AD_METADATA_TYPE, false));

	// Verify Dump call with no metadata, no expectations to check
	manager->Dump();

	// Create metadata with different types and positions
	auto mockMetaData1 = std::make_shared<NiceMock<MockAampTsbMetaData>>();
	auto mockMetaData2 = std::make_shared<NiceMock<MockAampTsbMetaData>>();

	EXPECT_CALL(*mockMetaData1, GetType()).WillRepeatedly(Return(AampTsbMetaData::Type::AD_METADATA_TYPE));
	EXPECT_CALL(*mockMetaData1, GetPosition()).WillRepeatedly(Return(AampTime(1.0)));
	EXPECT_CALL(*mockMetaData1, Dump(_)).Times(AtLeast(1));

	EXPECT_CALL(*mockMetaData2, GetType()).WillRepeatedly(Return(AampTsbMetaData::Type::AD_METADATA_TYPE));
	EXPECT_CALL(*mockMetaData2, GetPosition()).WillRepeatedly(Return(AampTime(2.0)));
	EXPECT_CALL(*mockMetaData2, Dump(_)).Times(AtLeast(1));

	// Add metadata
	ASSERT_TRUE(manager->AddMetaData(mockMetaData1));
	ASSERT_TRUE(manager->AddMetaData(mockMetaData2));

	// Verify Dump call
	manager->Dump();
}

/**
 * @brief Test handling of null metadata
 */
TEST_F(AampTsbMetaDataManagerTest, NullMetadataTest)
{
	// Attempt to add null metadata
	EXPECT_FALSE(manager->AddMetaData(nullptr))
		<< "Adding null metadata should return false";

	// Attempt to remove null metadata
	EXPECT_FALSE(manager->RemoveMetaData(nullptr))
		<< "Removing null metadata should return false";

	// Size should remain 0
	EXPECT_EQ(manager->GetSize(), 0)
		<< "Size should be 0 after failed null operations";
}

/**
 * @brief Test edge cases for GetMetaDataByType range functionality
 */
TEST_F(AampTsbMetaDataManagerTest, GetNonTransientMetaDataByTypeRangeEdgeCases)
{
	// Define a practical max for testing
	// AampTime cannot take the max of a double without overflowing
	double practicalMax = 1000000.0;

	// Register as non-transient first
	ASSERT_TRUE(manager->RegisterMetaDataType(AampTsbMetaData::Type::AD_METADATA_TYPE, false));

	// Create test metadata
	auto mockMetaData = std::make_shared<NiceMock<MockAampTsbMetaData>>();
	EXPECT_CALL(*mockMetaData, GetType()).WillRepeatedly(Return(AampTsbMetaData::Type::AD_METADATA_TYPE));
	EXPECT_CALL(*mockMetaData, GetPosition()).WillRepeatedly(Return(AampTime(10.0)));
	EXPECT_CALL(*mockMetaData, SetOrderAdded(1)).Times(1);
	EXPECT_CALL(*mockMetaData, Dump("Add ")).Times(1);

	ASSERT_TRUE(manager->AddMetaData(mockMetaData));

	// Test with equal rangeStart and rangeEnd
	auto result1 = manager->GetMetaDataByType<MockAampTsbMetaData>(AampTsbMetaData::Type::AD_METADATA_TYPE, 10.0, 10.0);
	EXPECT_EQ(result1.size(), 1)
		<< "Zero duration should still return matches";

	// Test with very large rangeEnd
	auto result2 = manager->GetMetaDataByType<MockAampTsbMetaData>(
		AampTsbMetaData::Type::AD_METADATA_TYPE, 10.0, practicalMax);
	EXPECT_EQ(result2.size(), 1)
		<< "Large duration should return matches";

	// Test with rangeEnd < rangeStart
	auto result3 = manager->GetMetaDataByType<MockAampTsbMetaData>(AampTsbMetaData::Type::AD_METADATA_TYPE, 10.0, 9.0);
	EXPECT_EQ(result3.size(), 0)
		<< "rangeEnd < rangeStart should be treated as error";

	// Test position at max double
	auto result4 = manager->GetMetaDataByType<MockAampTsbMetaData>(
		AampTsbMetaData::Type::AD_METADATA_TYPE, practicalMax, practicalMax);
	EXPECT_EQ(result4.size(), 1)
		<< "Position at practicalMax should still return matches for non-transient metadata";
}

/**
 * @brief Test edge cases for GetMetaDataByType range functionality
 */
TEST_F(AampTsbMetaDataManagerTest, GetTransientMetaDataByTypeRangeEdgeCases)
{
	// Define a practical max for testing
	// AampTime cannot take the max of a double without overflowing
	double practicalMax = 1000000.0;

	// Now register the same type as transient
	ASSERT_TRUE(manager->RegisterMetaDataType(AampTsbMetaData::Type::AD_METADATA_TYPE, true));

	// Add metadata at same position
	auto mockMetaData = std::make_shared<NiceMock<MockAampTsbMetaData>>();
	EXPECT_CALL(*mockMetaData, GetType()).WillRepeatedly(Return(AampTsbMetaData::Type::AD_METADATA_TYPE));
	EXPECT_CALL(*mockMetaData, GetPosition()).WillRepeatedly(Return(AampTime(10.0)));
	EXPECT_CALL(*mockMetaData, SetOrderAdded(1)).Times(1);
	EXPECT_CALL(*mockMetaData, Dump("Add ")).Times(1);

	ASSERT_TRUE(manager->AddMetaData(mockMetaData));

	// Test range query with transient metadata - should only match exact positions
	auto result5 = manager->GetMetaDataByType<MockAampTsbMetaData>(AampTsbMetaData::Type::AD_METADATA_TYPE, 10.0, 10.0);
	EXPECT_EQ(result5.size(), 1)
		<< "Exact position match should return metadata for transient type";

	auto result6 = manager->GetMetaDataByType<MockAampTsbMetaData>(AampTsbMetaData::Type::AD_METADATA_TYPE, 11.0, 12.0);
	EXPECT_EQ(result6.size(), 0)
		<< "Range not containing position should return nothing for transient metadata";

	auto result7 = manager->GetMetaDataByType<MockAampTsbMetaData>(AampTsbMetaData::Type::AD_METADATA_TYPE, practicalMax, practicalMax);
	EXPECT_EQ(result7.size(), 0)
		<< "Position at practicalMax should return nothing for transient metadata";
}

/**
 * @brief Test IsRegisteredType output parameters
 */
TEST_F(AampTsbMetaDataManagerTest, IsRegisteredTypeOutputTest)
{
	std::list<std::shared_ptr<AampTsbMetaData>>* metadataList = nullptr;
	bool isTransient = true;  // Initialize to opposite of expected

	// Test unregistered type
	EXPECT_FALSE(manager->IsRegisteredType(AampTsbMetaData::Type::AD_METADATA_TYPE, isTransient, &metadataList))
		<< "Unregistered type should return false";
	EXPECT_TRUE(metadataList == nullptr)
		<< "Metadata list should be null for unregistered type";

	// Register as non-transient
	ASSERT_TRUE(manager->RegisterMetaDataType(AampTsbMetaData::Type::AD_METADATA_TYPE, false));

	// Test registered non-transient type
	EXPECT_TRUE(manager->IsRegisteredType(AampTsbMetaData::Type::AD_METADATA_TYPE, isTransient, &metadataList))
		<< "Registered type should return true";
	EXPECT_FALSE(isTransient)
		<< "isTransient should be false for non-transient type";
	EXPECT_TRUE(metadataList != nullptr)
		<< "Metadata list should not be null for registered type";
	EXPECT_TRUE(metadataList->empty())
		<< "Metadata list should be empty initially";
}

/**
 * @brief Test RemoveMetaDataIf with nullptr filter
 */
TEST_F(AampTsbMetaDataManagerTest, RemoveMetaDataIfNullptrTest)
{
	ASSERT_TRUE(manager->RegisterMetaDataType(AampTsbMetaData::Type::AD_METADATA_TYPE, false));

	// Add some test metadata
	auto mockMetaData = std::make_shared<NiceMock<MockAampTsbMetaData>>();
	EXPECT_CALL(*mockMetaData, GetType()).WillRepeatedly(Return(AampTsbMetaData::Type::AD_METADATA_TYPE));
	EXPECT_CALL(*mockMetaData, GetPosition()).WillRepeatedly(Return(AampTime(1.0)));
	EXPECT_CALL(*mockMetaData, SetOrderAdded(1)).Times(1);
	EXPECT_CALL(*mockMetaData, Dump(std::string("Add "))).Times(1);
	// Should not be removed by nullptr filter, but should be removed by valid filter
	EXPECT_CALL(*mockMetaData, Dump("Erase ")).Times(1);

	ASSERT_TRUE(manager->AddMetaData(mockMetaData));
	EXPECT_EQ(manager->GetSize(), 1)
		<< "Should have one item before removal";

	// Test RemoveMetaDataIf with nullptr - should remove nothing
	size_t removed = manager->RemoveMetaDataIf(nullptr);

	EXPECT_EQ(removed, 0)
		<< "nullptr filter should remove nothing";
	EXPECT_EQ(manager->GetSize(), 1)
		<< "All items should remain with nullptr filter";

	// Verify we can still remove with a valid filter
	removed = manager->RemoveMetaDataIf(
		[](const std::shared_ptr<AampTsbMetaData>&) { return true; });

	EXPECT_EQ(removed, 1)
		<< "Should remove item with valid filter";
	EXPECT_EQ(manager->GetSize(), 0)
		<< "No items should remain after removal with valid filter";
}

/**
 * @brief Test ChangeMetaDataPosition with edge cases
 */
TEST_F(AampTsbMetaDataManagerTest, ChangeMetaDataPositionEdgeCasesTest)
{
	ASSERT_TRUE(manager->RegisterMetaDataType(AampTsbMetaData::Type::AD_METADATA_TYPE, false));

	// Add some test metadata
	auto mockMetaData1 = std::make_shared<NiceMock<MockAampTsbMetaData>>();
	auto mockMetaData2 = std::make_shared<NiceMock<MockAampTsbMetaData>>();

	EXPECT_CALL(*mockMetaData1, GetType()).WillRepeatedly(Return(AampTsbMetaData::Type::AD_METADATA_TYPE));
	EXPECT_CALL(*mockMetaData1, GetPosition()).WillRepeatedly(Return(AampTime(1.0)));
	EXPECT_CALL(*mockMetaData1, SetOrderAdded(1)).Times(1);
	EXPECT_CALL(*mockMetaData1, Dump(::testing::AnyOf(std::string("Add "), std::string("Change Pos ")))).Times(AtLeast(1));

	EXPECT_CALL(*mockMetaData2, GetType()).WillRepeatedly(Return(AampTsbMetaData::Type::AD_METADATA_TYPE));
	EXPECT_CALL(*mockMetaData2, GetPosition()).WillRepeatedly(Return(AampTime(1.0)));
	EXPECT_CALL(*mockMetaData2, SetOrderAdded(2)).Times(1);
	EXPECT_CALL(*mockMetaData2, Dump(::testing::AnyOf(std::string("Add "), std::string("Change Pos ")))).Times(AtLeast(1));

	ASSERT_TRUE(manager->AddMetaData(mockMetaData1));
	ASSERT_TRUE(manager->AddMetaData(mockMetaData2));
	EXPECT_EQ(manager->GetSize(), 2);

	// Create a list containing a nullptr
	std::list<std::shared_ptr<AampTsbMetaData>> listWithNullptr = {mockMetaData1, nullptr, mockMetaData2};

	// Attempt to change positions with a list containing a nullptr
	EXPECT_FALSE(manager->ChangeMetaDataPosition(listWithNullptr, 2.0))
		<< "ChangeMetaDataPosition should return false when the list contains a nullptr";
}
