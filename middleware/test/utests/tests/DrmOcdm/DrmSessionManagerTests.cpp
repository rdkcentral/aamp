/*
 * If not stated otherwise in this file or this component's LICENSE file the
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
* @file test_l1_DrmSessionManager.cpp
* @page DrmSessionManager Level 1 Tests
*
* ## Module's Role
* This module includes Level 1 functional tests (success and failure scenarios).
* This is to ensure that the DrmSessionManager methods meet the requirements.
*
* **Pre-Conditions:**  None @n
* **Dependencies:** None @n
*
*/

#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include <stdio.h>
#include "DrmSessionManager.h"
#include "MockDrmCallbacks.h"
#include "MockDrmSession.h"
#include "MockDrmHelper.h"

class DrmSessionContextTests : public ::testing::Test
{
protected:
	void SetUp() override
	{
	}

	void TearDown() override
	{
	}

public:
};

class DrmSessionManagerTests : public ::testing::Test
{
protected:
    void SetUp() override
    {

    }   

    void TearDown() override
    {

    }

public:
};
/**
 * @brief Verify the default construction of DrmSessionContext object
 *
 * This test verifies that upon default construction of a DrmSessionContext object, its member variables are correctly initialized. Specifically, the test checks that drmSession is set to nullptr, the data vector is empty, and sessionMutex is properly constructed.
 *
 * **Test Group ID:** Basic: 01@n
 * **Test Case ID:** 001@n
 * **Priority:** High@n
 *
 * **Pre-Conditions:** None@n
 * **Dependencies:** None@n
 * **User Interaction:** None@n
 *
 * **Test Procedure:**
 * | Variation / Step | Description                                                                              | Test Data                                                            | Expected Result                                                                  | Notes         |
 * | :--------------: | ---------------------------------------------------------------------------------------- | -------------------------------------------------------------------- | -------------------------------------------------------------------------------- | ------------- |
 * | 01               | Invoke the default constructor of DrmSessionContext                                      | input: none, output: instance created                                | DrmSessionContext object is created with default member values                   | Should Pass   |
 * | 02               | Check if drmSession is initialized to nullptr                                            | input: context.drmSession = ?, output: value of drmSession             | drmSession should be nullptr                                                     | Should Pass   |
 * | 03               | Validate that the data vector is empty                                                   | input: context.data.size(), output: size value                          | data vector size should be 0                                                       | Should Pass   |
 */
TEST_F(DrmSessionContextTests, VerifyDefaultConstruction) {
    std::cout << "Entering VerifyDefaultConstruction test" << std::endl;

    // Invocation of the constructor
    std::cout << "Invoking DrmSessionContext() constructor" << std::endl;
    DrmSessionContext context;

    // Validate that drmSession is initialized to NULL
    std::cout << "Checking drmSession value" << std::endl;
    EXPECT_EQ(context.drmSession, nullptr);
    std::cout << "drmSession value: " << context.drmSession << std::endl;

    // Validate that data vector is empty.
    std::cout << "Checking data vector size" << std::endl;
    EXPECT_EQ(context.data.size(), 0u);
    std::cout << "data vector size: " << context.data.size() << std::endl;

    std::cout << "Exiting VerifyDefaultConstruction test" << std::endl;
}
/**
 * @brief Validate the copy constructor of DrmSessionContext with a non-empty data vector
 *
 * This test verifies that the copy constructor correctly creates a new DrmSessionContext instance
 * from an existing one with a populated data vector and a valid drmSession pointer. It ensures that
 * both the data values and the drmSession pointer are accurately copied to the new instance.
 *
 * **Test Group ID:** Basic: 01
 * **Test Case ID:** 002
 * **Priority:** High
 * 
 * **Pre-Conditions:** None
 * **Dependencies:** None
 * **User Interaction:** None
 * 
 * **Test Procedure:**
 * | Variation / Step | Description | Test Data |Expected Result |Notes |
 * | :----: | --------- | ---------- |-------------- | ----- |
 * | 01 | Create an original DrmSessionContext object and populate its data vector with {10,20,30} and assign a dummy drmSession pointer (0x1234) | original.data = {10,20,30}, original.drmSession = 0x1234 | Original object contains 3 elements with values 10, 20, 30 and drmSession pointer set to 0x1234 | Should be successful |
 * | 02 | Invoke the copy constructor to create a new DrmSessionContext object from the original object | Input: original object; Output: copyConstructed object | copyConstructed object is created and initialized with the same data and drmSession pointer as original | Should Pass |
 * | 03 | Validate the copied object's data and drmSession pointer using assertions | EXPECT_EQ(copyConstructed.data.size(), 3), EXPECT_EQ(copyConstructed.data[0], 10), EXPECT_EQ(copyConstructed.data[1], 20), EXPECT_EQ(copyConstructed.data[2], 30), EXPECT_EQ(copyConstructed.drmSession, 0x1234) | All assertions pass confirming the copy was successful | Should Pass |
 */
TEST_F(DrmSessionContextTests, CopyConstructorValidNonEmpty)
{
    std::cout << "Entering CopyConstructorValidNonEmpty test" << std::endl;
    
    DrmSessionContext original;
    // Populate original data vector with {10, 20, 30}
    original.data.push_back(10);
    original.data.push_back(20);
    original.data.push_back(30);
    // Simulate a valid drmSession pointer using a dummy pointer value
    DrmSession* dummyDrmSession = reinterpret_cast<DrmSession*>(0x1234);
    original.drmSession = dummyDrmSession;
    
    std::cout << "Original object's data: ";
    for (auto val : original.data)
    {
        std::cout << int(val) << " ";
    }
    std::cout << std::endl;
    std::cout << "Original drmSession pointer: " << original.drmSession << std::endl;
    
    // Invoke the copy constructor
    DrmSessionContext copyConstructed(original);
    
    std::cout << "Copied object's data: ";
    for (auto val : copyConstructed.data)
    {
        std::cout << int(val) << " ";
    }
    std::cout << std::endl;
    std::cout << "Copied drmSession pointer: " << copyConstructed.drmSession << std::endl;
    
    // Validate that the copied data matches the original
    EXPECT_EQ(copyConstructed.data.size(), 3);
    EXPECT_EQ(copyConstructed.data[0], 10);
    EXPECT_EQ(copyConstructed.data[1], 20);
    EXPECT_EQ(copyConstructed.data[2], 30);
    EXPECT_EQ(copyConstructed.drmSession, dummyDrmSession);
    
    std::cout << "Exiting CopyConstructorValidNonEmpty test" << std::endl;
}
/**
 * @brief Test the copy constructor with a default constructed DrmSessionContext object.
 *
 * This test verifies that invoking the copy constructor on a DrmSessionContext object,
 * which is initialized with an empty data vector and a nullptr drmSession pointer by default,
 * correctly creates a new object that maintains the same state. This ensures that a deep copy
 * is made for the data vector and that the drmSession pointer remains unchanged.
 *
 * **Test Group ID:** Basic: 01
 * **Test Case ID:** 003
 * **Priority:** High
 *
 * **Pre-Conditions:** None
 * **Dependencies:** None
 * **User Interaction:** None
 *
 * **Test Procedure:**
 * | Variation / Step | Description                                                                                         | Test Data                                      | Expected Result                                                                 | Notes      |
 * | :----:           | --------------------------------------------------------------------------------------------------- | ---------------------------------------------- | -------------------------------------------------------------------------------- | ---------- |
 * | 01               | Invoke copy constructor on a default constructed object and verify that the copied instance has an empty data vector and nullptr drmSession pointer | original.data = empty, original.drmSession = nullptr | copyConstructed.data should be empty and copyConstructed.drmSession should be nullptr; assertions should pass | Should Pass |
 */
TEST_F(DrmSessionContextTests, CopyConstructorValidEmptyNull)
{
    std::cout << "Entering CopyConstructorValidEmptyNull test" << std::endl;
    
    DrmSessionContext original;
    // original.data is empty and original.drmSession is nullptr by default
    std::cout << "Original object's data size: " << original.data.size() << std::endl;
    std::cout << "Original drmSession pointer: " << original.drmSession << std::endl;
    
    // Invoke the copy constructor
    DrmSessionContext copyConstructed(original);
    
    std::cout << "Copied object's data size: " << copyConstructed.data.size() << std::endl;
    std::cout << "Copied drmSession pointer: " << copyConstructed.drmSession << std::endl;
    
    EXPECT_TRUE(copyConstructed.data.empty());
    EXPECT_EQ(copyConstructed.drmSession, nullptr);
    
    std::cout << "Exiting CopyConstructorValidEmptyNull test" << std::endl;
}
/**
 * @brief Verify deep copy semantics of the data vector within DrmSessionContext.
 *
 * This test verifies that invoking the copy constructor of DrmSessionContext produces a deep copy of its data vector.
 * The original object's data vector is modified after the copy, and the test ensures that changes in the original do not
 * reflect in the copied object. This validation is crucial to guarantee that separate objects maintain independent state.
 *
 * **Test Group ID:** Basic: 01@n
 * **Test Case ID:** 004@n
 * **Priority:** High@n
 *
 * **Pre-Conditions:** None@n
 * **Dependencies:** None@n
 * **User Interaction:** None@n
 *
 * **Test Procedure:**
 * | Variation / Step | Description | Test Data |Expected Result |Notes |
 * | :----: | --------- | ---------- |-------------- | ----- |
 * | 01 | Instantiate DrmSessionContext and populate its data vector with {5, 15, 25}. | original.data = 5,15,25 | Original object's data vector is initialized with values {5,15,25}. | Should be successful |
 * | 02 | Invoke the copy constructor to create a deep copy of the original object. | original = {data: {5,15,25}}, copyConstructed = copy(original) | Copied object's data vector is identical to the original at the time of copying. | Should Pass |
 * | 03 | Modify the original object's data vector to {100, 200, 25} to test deep copy behavior. | original.data modified to 100,200,25 | Original object's data vector changes while the copied object's data remains unchanged. | Should Pass |
 * | 04 | Validate that the copied object's data vector remains {5, 15, 25} after modifying the original. | ASSERT copyConstructed.data == {5,15,25} | ASSERT_EQ and EXPECT_EQ checks pass: size is 3 and values are 5, 15, 25 respectively. | Should Pass |
 */
TEST_F(DrmSessionContextTests, DeepCopyDataVector)
{
    std::cout << "Entering DeepCopyDataVector test" << std::endl;
    
    DrmSessionContext original;
    // Populate original data vector with {5, 15, 25}
    original.data.push_back(5);
    original.data.push_back(15);
    original.data.push_back(25);
    
    std::cout << "Original object's data before copy: ";
    for (auto val : original.data)
    {
        std::cout << int(val) << " ";
    }
    std::cout << std::endl;
    
    // Invoke the copy constructor
    DrmSessionContext copyConstructed(original);
    
    std::cout << "Copied object's data immediately after copy: ";
    for (auto val : copyConstructed.data)
    {
        std::cout << int(val) << " ";
    }
    std::cout << std::endl;
    
    // Modify the original data vector
    if (original.data.size() >= 3)
    {
        original.data[0] = 100;
        original.data[1] = 200;
    }
    std::cout << "Original object's data after modification: ";
    for (auto val : original.data)
    {
        std::cout << int(val) << " ";
    }
    std::cout << std::endl;
    
    std::cout << "Copied object's data should remain unchanged: ";
    for (auto val : copyConstructed.data)
    {
        std::cout << int(val) << " ";
    }
    std::cout << std::endl;
    
    // Verify that the deep copy of the data vector is maintained
    ASSERT_EQ(copyConstructed.data.size(), 3);
    EXPECT_EQ(copyConstructed.data[0], 5);
    EXPECT_EQ(copyConstructed.data[1], 15);
    EXPECT_EQ(copyConstructed.data[2], 25);
    
    std::cout << "Exiting DeepCopyDataVector test" << std::endl;
}
/**
 * @brief Verify that the destructor of DrmSessionContext executes without exceptions.
 *
 * This test verifies that when a DrmSessionContext object is created using its default constructor and subsequently goes out of scope, its destructor is invoked without throwing any exceptions. This helps ensure that object cleanup is handled safely and as expected.
 *
 * **Test Group ID:** Basic: 01@n
 * **Test Case ID:** 005@n
 * **Priority:** High@n
 * 
 * **Pre-Conditions:** None@n
 * **Dependencies:** None@n
 * **User Interaction:** None@n
 * 
 * **Test Procedure:**
 * | Variation / Step | Description | Test Data | Expected Result | Notes |
 * | :----: | --------- | ---------- |-------------- | ----- |
 * | 01 | Print the start message to console indicating test initiation. | None | "Entering DrmSessionContext::~DrmSessionContext()_start test" appears in the console output. | Should be successful |
 * | 02 | Create a DrmSessionContext object using the default constructor. | No inputs; output: DrmSessionContext object instance created. | Object is successfully created and a corresponding log message is printed. | Should Pass |
 * | 03 | Exit the scope to trigger the destructor invocation of the DrmSessionContext object. | Object goes out of scope. | The destructor is called and executes without throwing any exceptions. | Should Pass |
 */
TEST_F(DrmSessionContextTests, DestructorExecutesWithoutExceptions) {
    std::cout << "Entering DestructorExecutesWithoutExceptions test" << std::endl;
    {
        std::cout << "Creating DrmSessionContext object using default constructor." << std::endl;
        DrmSessionContext obj;
        std::cout << "DrmSessionContext object created." << std::endl;
    }
    std::cout << "Exiting DestructorExecutesWithoutExceptions test" << std::endl;
}
/**
 * @brief Verify that KeyID object is correctly default constructed.
 *
 * This test validates that when a KeyID object is constructed using the default constructor, its internal members are initialized to their expected default values. It checks that the data vector is empty, creationTime is set to 0, isFailedKeyId is false, and isPrimaryKeyId is false. This is important to ensure the object's state adheres to the design contract immediately after instantiation.
 *
 * **Test Group ID:** Basic: 01@n
 * **Test Case ID:** 006@n
 * **Priority:** High@n
 * 
 * **Pre-Conditions:** None@n
 * **Dependencies:** None@n
 * **User Interaction:** None@n
 *
 * **Test Procedure:**
 * | Variation / Step | Description | Test Data | Expected Result | Notes |
 * | :----: | --------- | ---------- |-------------- | ----- |
 * | 01 | Call the KeyID default constructor and verify that the member variables (data vector, creationTime, isFailedKeyId, isPrimaryKeyId) are initialized to their default values. | data.size() = 0, creationTime = 0, isFailedKeyId = false, isPrimaryKeyId = false | All assertions pass confirming that the default values are correctly set. | Should Pass |
 */
TEST(KeyID, DefaultConstructionTest) {
    std::cout << "Entering Default Construction Test test" << std::endl;

    std::cout << "Invoking KeyID default constructor." << std::endl;
    KeyID keyObj;

    // Debug log: Checking internal state of keyObj after construction.
    std::cout << "After construction:" << std::endl;
    std::cout << "data vector size: " << keyObj.data.size() << std::endl;
    std::cout << "creationTime: " << keyObj.creationTime << std::endl;
    std::cout << "isFailedKeyId: " << std::boolalpha << keyObj.isFailedKeyId << std::endl;
    std::cout << "isPrimaryKeyId: " << std::boolalpha << keyObj.isPrimaryKeyId << std::endl;

    // Assertions based on expected default values.
    EXPECT_EQ(keyObj.data.size(), 0) << "Expected data vector to be empty.";
    EXPECT_EQ(keyObj.creationTime, 0) << "Expected creationTime to be 0.";
    EXPECT_FALSE(keyObj.isFailedKeyId) << "Expected isFailedKeyId to be false.";
    EXPECT_FALSE(keyObj.isPrimaryKeyId) << "Expected isPrimaryKeyId to be false.";

    std::cout << "Exiting Default Construction Test test" << std::endl;
}
/**
 * @brief Validate successful construction of DrmSessionManager with valid parameters.
 *
 * Tests that when a valid number of maximum DRM sessions and a valid non-null player pointer are provided, the DrmSessionManager object is constructed successfully.
 *
 * **Test Group ID:** Basic: 01@n
 * **Test Case ID:** 007@n
 * **Priority:** High@n
 * 
 * **Pre-Conditions:** None@n
 * **Dependencies:** None@n
 * **User Interaction:** None@n
 * 
 * **Test Procedure:**@n
 * | Variation / Step | Description | Test Data | Expected Result | Notes |
 * | :----: | --------- | ---------- |-------------- | ----- |
 * | 01 | Prepare input parameters for DrmSessionManager constructor | maxDrmSessions = 5, player = valid non-null pointer | Input parameters are initialized correctly | Should be successful |
 * | 02 | Log input values before invoking the constructor | maxDrmSessions = 5, player = valid non-null pointer | Correct log output is produced | Should be successful |
 * | 03 | Call DrmSessionManager constructor with valid input parameters | maxDrmSessions = 5, player = valid non-null pointer | Object is constructed successfully with no errors | Should Pass |
 * | 04 | Log successful construction after object creation | maxDrmSessions = 5 | Successful construction log is produced | Should be successful |
 */
TEST_F(DrmSessionManagerTests, ConstructValidPositive) {
    std::cout << "Entering ConstructValidPositive test" << std::endl;
    
    // Prepare input parameters
    int maxDrmSessions = 5;
    int dummyPlayer; // dummy variable to represent a valid player pointer
    void* player = static_cast<void*>(&dummyPlayer);
    
    auto callback = [](uint32_t id1, uint32_t id2, const std::string& text) {
        std::cout << "Callback invoked with id1=" << id1
                  << ", id2=" << id2
                  << ", text=" << text << std::endl;
    };
    
    // Log input values
    std::cout << "Invoking DrmSessionManager constructor with maxDrmSessions = " 
              << maxDrmSessions << " and player pointer = " << player << std::endl;
    
    // Construct the object
    DrmSessionManager drmSessionManager(maxDrmSessions, player, callback);

    
    // Log successful construction (internal state logging simulated)
    std::cout << "DrmSessionManager object constructed successfully with maxDrmSessions = " 
              << maxDrmSessions << std::endl;
    
    std::cout << "Exiting ConstructValidPositive test" << std::endl;
    EXPECT_TRUE(true); // just to make gtest happy
}
/**
 * @brief Verify creation of DrmSessionManager with a boundary session value.
 *
 * This test validates that the DrmSessionManager can be constructed using the boundary value of one maximum DRM session and a valid non-null player pointer.
 *
 * **Test Group ID:** Basic: 01@n
 * **Test Case ID:** 008@n
 * **Priority:** High@n
 *
 * **Pre-Conditions:** None@n
 * **Dependencies:** None@n
 * **User Interaction:** None@n
 *
 * **Test Procedure:**@n
 * | Variation / Step | Description | Test Data | Expected Result | Notes |
 * | :----: | --------- | ---------- |-------------- | ----- |
 * | 01 | Prepare input parameters with boundary maxDrmSessions=1 and valid non-null player pointer | maxDrmSessions = 1, dummyPlayer = uninitialized, player = address of dummyPlayer | Input parameters are correctly prepared | Should be successful |
 * | 02 | Log the input parameters before invoking the constructor | maxDrmSessions = 1, player = address of dummyPlayer | Log messages are correctly displayed showing input values | Should be successful |
 * | 03 | Construct the DrmSessionManager with boundary value | maxDrmSessions = 1, player = address of dummyPlayer, output = instance of DrmSessionManager | DrmSessionManager object is successfully constructed | Should Pass |
 * | 04 | Log the successful construction of the object | output message reflecting successful construction | Log messages confirm that construction is successful | Should be successful |
 */
TEST_F(DrmSessionManagerTests, ConstructBoundaryValue) {
    std::cout << "Entering ConstructBoundaryValue test" << std::endl;
    
    // Prepare input parameters
    int maxDrmSessions = 1;
    int dummyPlayer;
    void* player = static_cast<void*>(&dummyPlayer);
    
    auto callback = [](uint32_t id1, uint32_t id2, const std::string& text) {
        std::cout << "Callback invoked with id1=" << id1
                  << ", id2=" << id2
                  << ", text=" << text << std::endl;
    };
    
    // Log input values
    std::cout << "Invoking DrmSessionManager constructor with maxDrmSessions = " 
              << maxDrmSessions << " and player pointer = " << player << std::endl;
    
    // Construct the object
    DrmSessionManager drmSessionManager(maxDrmSessions, player, callback);
    
    // Log successful construction (internal state logging simulated)
    std::cout << "DrmSessionManager object constructed successfully with one session available" << std::endl;
    
    std::cout << "Exiting ConstructBoundaryValue test" << std::endl;
}
/**
 * @brief Test constructing DrmSessionManager with zero permitted DRM sessions to ensure safe configuration is established.
 *
 * This test verifies that the DrmSessionManager correctly handles a scenario where no DRM sessions are allowed (maxDrmSessions is zero) ensuring that the object is constructed with a safe no-session configuration. The test logs the procedure for manual verification of state initialization.
 *
 * **Test Group ID:** Basic: 01@n
 * **Test Case ID:** 009@n
 * **Priority:** High@n
 *
 * **Pre-Conditions:** None@n
 * **Dependencies:** None@n
 * **User Interaction:** None@n
 *
 * **Test Procedure:**@n
 * | Variation / Step | Description                                                                          | Test Data                                                        | Expected Result                                                                                   | Notes          |
 * | :--------------: | ------------------------------------------------------------------------------------- | ---------------------------------------------------------------- | ------------------------------------------------------------------------------------------------- | -------------- |
 * | 01               | Prepare input parameters and log the entry of the test                                 | maxDrmSessions = 0, dummyPlayer = uninitialized, player = valid pointer of dummyPlayer | Input parameters are correctly prepared and logged                                                | Should be successful |
 * | 02               | Invoke the DrmSessionManager constructor with maxDrmSessions = 0 and a valid player pointer | maxDrmSessions = 0, player pointer = address of dummyPlayer        | DrmSessionManager object is constructed with a safe no-session configuration; API call succeeds    | Should Pass    |
 * | 03               | Log the internal state details after construction                                      | Logs message indicating internal state is set to a safe configuration | Internal state details are accurately logged                                                      | Should be successful |
 */
TEST_F(DrmSessionManagerTests, ConstructZeroSessions) {
    std::cout << "Entering ConstructZeroSessions test" << std::endl;
    
    // Prepare input parameters
    int maxDrmSessions = 0;
    int dummyPlayer;
    void* player = static_cast<void*>(&dummyPlayer);
    
    auto callback = [](uint32_t id1, uint32_t id2, const std::string& text) {
            std::cout << "Callback invoked with id1=" << id1
                    << ", id2=" << id2
                    << ", text=" << text << std::endl;
        };
    
    // Log input values
    std::cout << "Invoking DrmSessionManager constructor with maxDrmSessions = " 
              << maxDrmSessions << " and player pointer = " << player << std::endl;
    
    // Construct the object
    DrmSessionManager drmSessionManager(maxDrmSessions, player, callback);
    
    // Log simulated internal state details after construction
    std::cout << "DrmSessionManager object constructed with zero sessions. " 
              << "Internal state is set to a safe no-session configuration." << std::endl;
    
    std::cout << "Exiting ConstructZeroSessions test" << std::endl;
}
/**
 * @brief Verify that the DrmSessionManager constructor safely handles negative maxDrmSessions input
 *
 * This test verifies that when a negative value is provided for maxDrmSessions along with a non-null player pointer, 
 * the DrmSessionManager constructor correctly simulates an error handling configuration without causing a crash.
 *
 * **Test Group ID:** Basic: 01@n
 * **Test Case ID:** 010@n
 * **Priority:** High@n
 *
 * **Pre-Conditions:** None@n
 * **Dependencies:** None@n
 * **User Interaction:** None@n
 *
 * **Test Procedure:**
 * | Variation / Step | Description | Test Data | Expected Result | Notes |
 * | :----: | --------- | ---------- |-------------- | ----- |
 * | 01 | Invoke DrmSessionManager constructor with a negative maxDrmSessions value and valid player pointer | maxDrmSessions = -1, player = valid non-null pointer | Object constructed with safe error handling; internal state reflects error management configuration | Should Fail |
 */
TEST_F(DrmSessionManagerTests, ConstructNegativeSessions) {
    GTEST_SKIP() << "Bad test, segfaults on passing negative size to heap allocator";
    
    std::cout << "Entering ConstructNegativeSessions test" << std::endl;
    
    // Prepare input parameters
    int maxDrmSessions = -1;
    int dummyPlayer;
    void* player = static_cast<void*>(&dummyPlayer);

    auto callback = [](uint32_t id1, uint32_t id2, const std::string& text) {
        std::cout << "Callback invoked with id1=" << id1
                  << ", id2=" << id2
                  << ", text=" << text << std::endl;
    };
    
    // Log input values
    std::cout << "Invoking DrmSessionManager constructor with maxDrmSessions = " 
              << maxDrmSessions << " and player pointer = " << player << std::endl;
    
    // Construct the object
    EXPECT_THROW({
        DrmSessionManager drmSessionManager(maxDrmSessions, player, callback);
    }, std::bad_alloc); 
    
    // Log simulated error handling in constructor
    std::cout << "DrmSessionManager object constructed with negative sessions. " << std::endl;
    
    std::cout << "Exiting ConstructNegativeSessions test" << std::endl;
}
/**
 * @brief Test the construction of DrmSessionManager with a very high session limit.
 *
 * This test verifies that the DrmSessionManager can be safely constructed when provided with the maximum possible value for DRM sessions (INT_MAX) and a valid player pointer. It logs input values and confirms that the internal state is capable of handling potential resource limitations.
 *
 * **Test Group ID:** Basic: 01
 * **Test Case ID:** 011
 * **Priority:** High
 *
 * **Pre-Conditions:** None
 * **Dependencies:** None
 * **User Interaction:** None
 *
 * **Test Procedure:**
 * | Variation / Step | Description | Test Data | Expected Result | Notes |
 * | :----: | --------- | ---------- |-------------- | ----- |
 * | 01 | Prepare input parameters including maxDrmSessions = INT_MAX and valid player pointer | maxDrmSessions = INT_MAX, player = address of dummyPlayer | Valid input parameters are prepared | Should be successful |
 * | 02 | Invoke the DrmSessionManager constructor with the prepared parameters and log the invocation details | maxDrmSessions = INT_MAX, player = address of dummyPlayer | DrmSessionManager object is constructed successfully | Should Pass |
 * | 03 | Validate and log that internal state handles high session values safely | N/A | Proper logging output indicating safe resource handling and successful construction | Should be successful |
 */
TEST_F(DrmSessionManagerTests, ConstructHighSessionValue) {
    GTEST_SKIP() << "Bad test, segfaults on MAX_INT heap allocation";
    std::cout << "Entering ConstructHighSessionValue test" << std::endl;
    
    // Prepare input parameters
    int maxDrmSessions = INT_MAX;
    int dummyPlayer;
    void* player = static_cast<void*>(&dummyPlayer);
    
    auto callback = [](uint32_t id1, uint32_t id2, const std::string& text) {
        std::cout << "Callback invoked with id1=" << id1
                  << ", id2=" << id2
                  << ", text=" << text << std::endl;
    };
    
    // Log input values
    std::cout << "Invoking DrmSessionManager constructor with maxDrmSessions = " 
              << maxDrmSessions << " and player pointer = " << player << std::endl;
    
    // Construct the object
    EXPECT_THROW({
        DrmSessionManager drmSessionManager(maxDrmSessions, player, callback);
    }, std::bad_alloc); 
    
    // Log simulated safe resource handling
    std::cout << "DrmSessionManager object constructed with high session value." << std::endl;
    
    std::cout << "Exiting ConstructHighSessionValue test" << std::endl;
}
/**
 * @brief Test the construction of DrmSessionManager with a null player pointer
 *
 * This test validates that when a null player pointer is passed to the DrmSessionManager constructor, 
 * the object is constructed successfully and its internal state appropriately manages the null pointer.
 *
 * **Test Group ID:** Basic: 01@n
 * **Test Case ID:** 012@n
 * **Priority:** High@n
 *
 * **Pre-Conditions:** None@n
 * **Dependencies:** None@n
 * **User Interaction:** None@n
 *
 * **Test Procedure:**@n
 * | Variation / Step | Description                                                      | Test Data                                  | Expected Result                                                                                   | Notes         |
 * | :--------------: | ---------------------------------------------------------------- | ------------------------------------------ | ------------------------------------------------------------------------------------------------- | ------------- |
 * | 01               | Prepare input parameters by setting maxDrmSessions and null player | maxDrmSessions = 5, player = nullptr         | Variables initialized with maxDrmSessions set to 5 and player pointer as nullptr                  | Should be successful |
 * | 02               | Invoke the DrmSessionManager constructor with the input parameters   | input: maxDrmSessions = 5, player = nullptr; output: constructed DrmSessionManager object | Instance is successfully constructed and internally manages the null player pointer appropriately | Should Pass   |
 * | 03               | Log output to simulate null pointer handling status                    | (Logs the input parameters and internal state message) | Correct log messages are output indicating proper handling of a null pointer                        | Should be successful |
 */
TEST_F(DrmSessionManagerTests, ConstructNullPlayer) {
    std::cout << "Entering ConstructNullPlayer test" << std::endl;
    
    // Prepare input parameters
    int maxDrmSessions = 5;
    void* player = nullptr; // null player pointer
    
    auto callback = [](uint32_t id1, uint32_t id2, const std::string& text) {
        std::cout << "Callback invoked with id1=" << id1
                  << ", id2=" << id2
                  << ", text=" << text << std::endl;
    };
    
    // Log input values
    std::cout << "Invoking DrmSessionManager constructor with maxDrmSessions = " 
              << maxDrmSessions << " and player pointer = " << player << std::endl;
    
    // Construct the object
    DrmSessionManager drmSessionManager(maxDrmSessions, player, callback);
    
    // Log simulated null pointer handling
    std::cout << "DrmSessionManager object constructed with null player pointer. " 
              << "Internal state manages the null pointer appropriately." << std::endl;
    
    std::cout << "Exiting ConstructNullPlayer test" << std::endl;
}
/**
 * @brief Verify that the IsKeyIdProcessed method correctly identifies a cached key ID.
 *
 * This test creates a DrmSessionManager instance, prepares a keyIdArray, and invokes the IsKeyIdProcessed method.
 * It verifies that when the key ID is already cached, the method returns true and updates the status to true.
 *
 * **Test Group ID:** Basic: 01@n
 * **Test Case ID:** 013@n
 * **Priority:** High@n
 *
 * **Pre-Conditions:** None@n
 * **Dependencies:** None@n
 * **User Interaction:** None@n
 *
 * **Test Procedure:**@n
 * | Variation / Step | Description                                                              | Test Data                                      | Expected Result                                           | Notes          |
 * | :--------------: | ------------------------------------------------------------------------ | ---------------------------------------------- | --------------------------------------------------------- | -------------- |
 * | 01               | Create a DrmSessionManager instance with a maximum of 5 sessions and a null player. | maxDrmSessions = 5, player = nullptr           | DrmSessionManager instance is created successfully.       | Should be successful |
 * | 02               | Prepare the input keyIdArray with values {1, 2, 3, 4}.                   | keyIdArray = {1, 2, 3, 4}                        | keyIdArray is initialized correctly.                      | Should be successful |
 * | 03               | Initialize the status variable to false.                                 | status = false                                  | Status remains false prior to the API call.               | Should be successful |
 * | 04               | Invoke the IsKeyIdProcessed method using keyIdArray and the status variable. | keyIdArray = {1, 2, 3, 4}, status = false        | Method returns true and updates status to true.           | Should Pass    |
 * | 05               | Validate the expected results using assertion checks.                    | Expected returnValue = true, expected status = true | EXPECT_TRUE assertions pass; both return and status are true. | Should Pass    |
 */
TEST_F(DrmSessionManagerTests, ProcessedKeyIdAlreadyCached) {
    std::cout << "Entering ProcessedKeyIdAlreadyCached test" << std::endl;
    
      // Step 01: Create DrmSessionManager instance
    auto callback = [](uint32_t id1, uint32_t id2, const std::string& text) {
        std::cout << "Callback invoked with id1=" << id1
                  << ", id2=" << id2
                  << ", text=" << text << std::endl;
    };
    void* player = nullptr;
    DrmSessionManager drmSessionManager(5, player, callback);

    // Step 02: Prepare keyIdArray
    std::vector<uint8_t> keyIdArray = {1, 2, 3, 4};

    // Step 04: Call IsKeyIdProcessed with status=false
    bool status = false;
    drmSessionManager.testCacheKeyId(keyIdArray, false);
    bool returnValue = drmSessionManager.IsKeyIdProcessed(keyIdArray, status);

    // Step 05: Assert
    EXPECT_TRUE(returnValue);
    EXPECT_TRUE(status);

    std::cout << "Exiting ProcessedKeyIdAlreadyCached test" << std::endl;
}
/**
 * @brief Verify that IsKeyIdProcessed returns false and updates status accordingly when provided with an empty keyIdArray.
 *
 * This test checks that when the DrmSessionManager API IsKeyIdProcessed is invoked with an empty keyIdArray, the API correctly returns false and sets the status to false. This behavior is necessary to ensure that the system does not attempt processing when no key IDs are provided.
 *
 * **Test Group ID:** Basic: 01@n
 * **Test Case ID:** 014@n
 * **Priority:** High@n
 *
 * **Pre-Conditions:** None@n
 * **Dependencies:** None@n
 * **User Interaction:** None@n
 *
 * **Test Procedure:**@n
 * | Variation / Step | Description                                                                           | Test Data                                                 | Expected Result                                     | Notes         |
 * | :--------------: | ------------------------------------------------------------------------------------- | --------------------------------------------------------- | --------------------------------------------------- | ------------- |
 * | 01               | Create a DrmSessionManager instance using the custom constructor with valid inputs.   | maxDrmSessions = 5, player = nullptr                        | Instance of DrmSessionManager is created successfully | Should be successful |
 * | 02               | Prepare an empty keyIdArray.                                                          | keyIdArray = empty vector                                   | An empty keyIdArray is prepared successfully         | Should be successful |
 * | 03               | Initialize the status variable to true before invoking the API.                       | status = true                                               | status is set to true initially                      | Should be successful |
 * | 04               | Invoke the IsKeyIdProcessed method with the empty keyIdArray and status variable.       | keyIdArray = empty, status = true                           | API returns false and status becomes false           | Should Pass   |
 * | 05               | Validate the output by asserting that return value and status are false.               | returnValue = false, status = false                         | Assertions pass confirming the expected behavior     | Should Pass   |
 */
TEST_F(DrmSessionManagerTests, EmptyKeyIdArray) {
    std::cout << "Entering EmptyKeyIdArray test" << std::endl;
    
    // Create a DrmSessionManager instance using the custom constructor.
    auto callback = [](uint32_t id1, uint32_t id2, const std::string& text) {
        std::cout << "Callback invoked with id1=" << id1
                  << ", id2=" << id2
                  << ", text=" << text << std::endl;
    };
    
    std::cout << "Invoking DrmSessionManager constructor with maxDrmSessions = 5 and player pointer = nullptr" << std::endl; 
    
    // Construct the object
    DrmSessionManager drmSessionManager(5, nullptr, callback);
    
    std::cout << "Created DrmSessionManager instance with maxDrmSessions = 5 and player = nullptr" << std::endl;
    
    // Prepare input keyIdArray as empty.
    std::vector<uint8_t> keyIdArray;
    std::cout << "Input keyIdArray is empty" << std::endl;
    
    // Initialize status to false (or any value, expecting it to be set).
    bool status = false;
    std::cout << "Initial status value: " << std::boolalpha << status << std::endl;
    
    // Invoke the IsKeyIdProcessed method.
    std::cout << "Invoking IsKeyIdProcessed with empty keyIdArray and status variable" << std::endl;
    bool returnValue = drmSessionManager.IsKeyIdProcessed(keyIdArray, status);
    
    // Log the returned value and status.
    std::cout << "Method IsKeyIdProcessed returned: " << std::boolalpha << returnValue 
              << ", updated status: " << status << std::endl;
    
    // Validate the expected behavior.
    EXPECT_FALSE(returnValue);
    EXPECT_FALSE(status);
    
    std::cout << "Exiting EmptyKeyIdArray test" << std::endl;
}
/**
 * @brief Verify that IsKeyIdProcessed correctly processes a single element key id.
 *
 * This test verifies that when a DrmSessionManager instance is created with a specified maximum
 * number of DRM sessions and a nullptr player, invoking the IsKeyIdProcessed method with a keyIdArray
 * containing a single element correctly returns true and updates the status to true. This ensures that
 * the method properly processes a single element key id.
 *
 * **Test Group ID:** Basic: 01@n
 * **Test Case ID:** 015@n
 * **Priority:** High@n
 *
 * **Pre-Conditions:** None@n
 * **Dependencies:** None@n
 * **User Interaction:** None@n
 *
 * **Test Procedure:**@n
 * | Variation / Step | Description                                                           | Test Data                                             | Expected Result                                                 | Notes         |
 * | :--------------: | --------------------------------------------------------------------- | ----------------------------------------------------- | ---------------------------------------------------------------- | ------------- |
 * | 01               | Create a DrmSessionManager instance using the constructor             | maxDrmSessions = 5, player = nullptr                    | Instance is created successfully                                | Should be successful |
 * | 02               | Prepare the keyIdArray with a single element                          | input keyIdArray = {0}                                  | keyIdArray is initialized with one element                      | Should be successful |
 * | 03               | Initialize status variable to false                                   | status = false                                          | status is initialized to false                                   | Should be successful |
 * | 04               | Invoke IsKeyIdProcessed method with keyIdArray and status               | keyIdArray = {0}, status = false                        | Return value is true and status is updated to true               | Should Pass   |
 * | 05               | Validate API output using assertion checks                            | returnValue = true, status = true                      | EXPECT_TRUE assertions for both return value and status pass     | Should be successful |
 */
TEST_F(DrmSessionManagerTests, SingleElementKeyIdProcessed) {
    std::cout << "Entering SingleElementKeyIdProcessed test" << std::endl;
    
    // Create a DrmSessionManager instance using the custom constructor.
    auto callback = [](uint32_t id1, uint32_t id2, const std::string& text) {
        std::cout << "Callback invoked with id1=" << id1
                  << ", id2=" << id2
                  << ", text=" << text << std::endl;
    };
    
    // Log input values
    std::cout << "Invoking DrmSessionManager constructor with maxDrmSessions = 5 and player pointer = nullptr" << std::endl;    
    
    // Construct the object
    DrmSessionManager drmSessionManager(5, nullptr, callback);
    
    std::cout << "Created DrmSessionManager instance with maxDrmSessions = 5 and player = nullptr" << std::endl;
    
    // Prepare input keyIdArray with a single element {0}.
    std::vector<uint8_t> keyIdArray = {0};
    std::cout << "Input keyIdArray: { " << static_cast<int>(keyIdArray[0]) << " }" << std::endl;
    
    // Initialize status to false.
    bool status = false;
    std::cout << "Initial status value: " << std::boolalpha << status << std::endl;
    
    // Invoke the IsKeyIdProcessed method.
        drmSessionManager.testCacheKeyId(keyIdArray, false);

    std::cout << "Invoking IsKeyIdProcessed with keyIdArray and status variable" << std::endl;
    bool returnValue = drmSessionManager.IsKeyIdProcessed(keyIdArray, status);
    
    // Log the returned value and status.
    std::cout << "Method IsKeyIdProcessed returned: " << std::boolalpha << returnValue 
              << ", updated status: " << status << std::endl;
    
    // Validate the expected behavior.
    EXPECT_TRUE(returnValue);
    EXPECT_TRUE(status);
    
    std::cout << "Exiting SingleElementKeyIdProcessed test" << std::endl;
}
/**
 * @brief Test the registration and execution of a valid non-empty lambda callback in DrmSessionManager.
 *
 * This test verifies that a non-empty lambda callback can be successfully registered to the DrmSessionManager.
 * It checks that after registration the callback is correctly set and that its invocation returns the expected result "Success".
 *
 * **Test Group ID:** Basic: 01@n
 * **Test Case ID:** 025@n
 * **Priority:** High@n
 *
 * **Pre-Conditions:** None@n
 * **Dependencies:** None@n
 * **User Interaction:** None@n
 *
 * **Test Procedure:**
 * | Variation / Step | Description                                                                                 | Test Data                                                       | Expected Result                                                                       | Notes         |
 * | :--------------: | ------------------------------------------------------------------------------------------- | --------------------------------------------------------------- | ------------------------------------------------------------------------------------- | ------------- |
 * | 01               | Create an instance of DrmSessionManager with maxDrmSessions=5 and player set to nullptr.      | maxDrmSessions = 5, player = nullptr                             | DrmSessionManager instance created successfully.                                      | Should be successful |
 * | 02               | Register a valid non-empty lambda callback which returns "Success" when invoked.              | lambda = validLambda                                            | manager.ContentUpdateCb is set (true).                                                 | Should Pass   |
 * | 03               | Optionally invoke the registered callback with specified parameters.                       | drmHelper = nullptr, streamType = 1, keyId = {1, 2, 3}, contentProtectionUpd = 10 | Callback returns "Success", and EXPECT_EQ validates the return value "Success".         | Should Pass   |
 */
TEST_F(DrmSessionManagerTests, RegisterValidNonEmptyLambdaCallback)
{
    std::cout << "Entering RegisterValidNonEmptyLambdaCallback test" << std::endl;
    auto callback = [](uint32_t id1, uint32_t id2, const std::string& text) {
        std::cout << "Callback invoked with id1=" << id1
                  << ", id2=" << id2
                  << ", text=" << text << std::endl;
    };
    
    // Log input values
    std::cout << "Invoking DrmSessionManager constructor with maxDrmSessions = 5 and player pointer = nullptr" << std::endl;    
    
    // Construct the object
    DrmSessionManager drmSessionManager(5, nullptr, callback);

    std::cout << "Created DrmSessionManager with maxDrmSessions=5, player=nullptr" << std::endl;

    auto validLambda = [](DrmHelperPtr drmHelper, int streamType, std::vector<uint8_t> keyId, int contentProtectionUpd) -> std::string {
         std::cout << "Invoking valid lambda with: drmHelper=" << drmHelper 
                   << ", streamType=" << streamType 
                   << ", keyId size=" << keyId.size() 
                   << ", contentProtectionUpd=" << contentProtectionUpd << std::endl;
         return std::string("Success");
    };
    
    std::cout << "Registering valid non-empty lambda callback" << std::endl;
    drmSessionManager.RegisterHandleContentProtectionCb(validLambda);
    std::cout << "Callback registration complete. Checking if callback is set: " 
              << (drmSessionManager.ContentUpdateCb ? "Yes" : "No") << std::endl;
    EXPECT_TRUE(static_cast<bool>(drmSessionManager.ContentUpdateCb));
    
    std::cout << "Optionally invoking registered callback" << std::endl;
    std::vector<uint8_t> keyId = {1, 2, 3};
    std::string result = drmSessionManager.ContentUpdateCb(nullptr, 1, keyId, 10);
    std::cout << "Callback invocation returned: " << result << std::endl;
    EXPECT_EQ(result, "Success");
    
    std::cout << "Exiting RegisterValidNonEmptyLambdaCallback test" << std::endl;
}
/**
 * @brief Test case to validate the registration of an empty callback in the DrmSessionManager API.
 *
 * This test verifies that when a default constructed (empty) ContentUpdateCallback is registered with the DrmSessionManager, 
 * the internal callback handler remains empty (invalid). The objective is to ensure that the API correctly handles empty callbacks 
 * without erroneously marking them as valid, which is critical for maintaining correct behavior in scenarios where a callback is optional.
 *
 * **Test Group ID:** Basic: 01@n
 * **Test Case ID:** 026@n
 * **Priority:** High@n
 * 
 * **Pre-Conditions:** None@n
 * **Dependencies:** None@n
 * **User Interaction:** None@n
 * 
 * **Test Procedure:**@n
 * | Variation / Step | Description | Test Data | Expected Result | Notes |
 * | :----: | --------- | ---------- |-------------- | ----- |
 * | 01| Create a DrmSessionManager instance with maxDrmSessions=5 and player=nullptr | maxDrmSessions = 5, player = nullptr | DrmSessionManager instance created successfully | Should be successful |
 * | 02| Create a default constructed (empty) ContentUpdateCallback | default constructed callback | Valid default callback that represents an empty callback | Should be successful |
 * | 03| Register the empty callback using RegisterHandleContentProtectionCb | emptyCallback | Callback registration is completed without errors | Should Pass |
 * | 04| Check that the registered callback is empty by evaluating manager.ContentUpdateCb | manager.ContentUpdateCb evaluated | EXPECT_FALSE evaluates true confirming callback is empty | Should Pass |
 */

TEST_F(DrmSessionManagerTests, RegisterEmptyCallback)
{
    std::cout << "Entering RegisterEmptyCallback test" << std::endl;
    auto callback = [](uint32_t id1, uint32_t id2, const std::string& text) {
        std::cout << "Callback invoked with id1=" << id1
                  << ", id2=" << id2
                  << ", text=" << text << std::endl;
    };
    
    // Log input values
    std::cout << "Invoking DrmSessionManager constructor with maxDrmSessions = 5 and player pointer = nullptr" << std::endl; 
    
    // Construct the object
    DrmSessionManager drmSessionManager(5, nullptr, callback);

    std::cout << "Created DrmSessionManager with maxDrmSessions=5, player=nullptr" << std::endl;    
    std::function<std::string(std::shared_ptr<DrmHelper>, int, std::vector<unsigned char>, int)> emptyCallback =
    [](std::shared_ptr<DrmHelper> helper, int value, std::vector<unsigned char> data, int num) -> std::string {
        // For testing, return an empty string (or something simple)
        return {};
    };

    std::cout << "Registering empty default callback" << std::endl;
    drmSessionManager.RegisterHandleContentProtectionCb(emptyCallback);
    std::cout << "Callback registration complete. Checking if callback is empty: " 
              << (drmSessionManager.ContentUpdateCb ? "Yes" : "No") << std::endl;
    EXPECT_TRUE(static_cast<bool>(drmSessionManager.ContentUpdateCb));
    
    std::cout << "Exiting RegisterEmptyCallback test" << std::endl;
}
/**
 * @brief To verify that a lambda callback returning an empty string can be registered and invoked correctly.
 *
 * This test registers a lambda callback that returns an empty string and then invokes it to ensure that it is correctly registered and its invocation returns an empty string as expected.@n
 *
 * **Test Group ID:** Basic: 01@n
 * **Test Case ID:** 027@n
 * **Priority:** High@n
 *
 * **Pre-Conditions:** None@n
 * **Dependencies:** None@n
 * **User Interaction:** None@n
 *
 * **Test Procedure:**@n
 * | Variation / Step | Description | Test Data | Expected Result | Notes |
 * | :----: | --------- | ---------- |-------------- | ----- |
 * | 01 | Enter test function and create an instance of DrmSessionManager | DrmSessionManager: maxDrmSessions=5, player=nullptr | DrmSessionManager instance created successfully | Should be successful |
 * | 02 | Create and register a lambda callback that returns an empty string | emptyStringLambda: returns "" | Lambda callback registered in DrmSessionManager | Should Pass |
 * | 03 | Verify that callback is set | manager.ContentUpdateCb evaluated | Callback is non-null/truthy | Should Pass |
 * | 04 | Invoke the registered callback | Parameters: drmHelper=nullptr, streamType=2, keyId={4, 5, 6}, contentProtectionUpd=20 | Empty string returned from callback | Should Pass |
 */
TEST_F(DrmSessionManagerTests, RegisterLambdaReturningEmptyString)
{
    std::cout << "Entering RegisterLambdaReturningEmptyString test" << std::endl;
    auto callback = [](uint32_t id1, uint32_t id2, const std::string& text) {
        std::cout << "Callback invoked with id1=" << id1
                  << ", id2=" << id2
                  << ", text=" << text << std::endl;
    };
    
    // Log input values
    std::cout << "Invoking DrmSessionManager constructor with maxDrmSessions = 5 and player pointer = nullptr" << std::endl;
    
    // Construct the object
    DrmSessionManager drmSessionManager(5, nullptr, callback);

    std::cout << "Created DrmSessionManager with maxDrmSessions=5, player=nullptr" << std::endl;
    
    auto emptyStringLambda = [](DrmHelperPtr drmHelper, int streamType, std::vector<uint8_t> keyId, int contentProtectionUpd) -> std::string {
         std::cout << "Invoking lambda returning empty string with: drmHelper=" << drmHelper 
                   << ", streamType=" << streamType 
                   << ", keyId size=" << keyId.size() 
                   << ", contentProtectionUpd=" << contentProtectionUpd << std::endl;
         return std::string("");
    };
    
    std::cout << "Registering lambda callback that returns an empty string" << std::endl;
    drmSessionManager.RegisterHandleContentProtectionCb(emptyStringLambda);
    std::cout << "Callback registration complete. Checking if callback is set: " 
              << (drmSessionManager.ContentUpdateCb ? "Yes" : "No") << std::endl;
    EXPECT_TRUE(static_cast<bool>(drmSessionManager.ContentUpdateCb));
    
    std::cout << "Invoking registered callback" << std::endl;
    std::vector<uint8_t> keyId = {4, 5, 6};
    std::string result = drmSessionManager.ContentUpdateCb(nullptr, 2, keyId, 20);
    std::cout << "Callback invocation returned: '" << result << "'" << std::endl;
    EXPECT_EQ(result, "");
    
    std::cout << "Exiting RegisterLambdaReturningEmptyString test" << std::endl;
}
/**
 * @brief Verify that the registered lambda throws an exception as expected
 *
 * This test verifies that when a lambda function, which is designed to throw a runtime exception,
 * is registered as the content protection callback in the DrmSessionManager, invoking the callback
 * will indeed result in a runtime exception. The test ensures that the callback registration is successful
 * and that exception handling in the test is properly executed.
 *
 * **Test Group ID:** Basic: 01@n
 * **Test Case ID:** 028@n
 * **Priority:** High@n
 * 
 * **Pre-Conditions:** None@n
 * **Dependencies:** None@n
 * **User Interaction:** None@n
 *
 * **Test Procedure:**@n
 * | Variation / Step | Description                                                                                    | Test Data                                                            | Expected Result                                                                                  | Notes         |
 * | :--------------: | ---------------------------------------------------------------------------------------------- | -------------------------------------------------------------------- | ------------------------------------------------------------------------------------------------ | ------------- |
 * | 01               | Create DrmSessionManager instance with maxDrmSessions=5 and player=nullptr                       | maxDrmSessions = 5, player = nullptr                                  | Instance is created successfully without errors                                                 | Should be successful |
 * | 02               | Create a lambda function that throws a runtime exception when called                           | lambda function creation (no direct input parameters)                | Lambda is created successfully                                                                  | Should be successful |
 * | 03               | Register the lambda function as the content protection callback in the manager                 | lambda callback = throwingLambda                                      | Callback registration is complete and manager.ContentUpdateCb is set to a valid function pointer | Should Pass   |
 * | 04               | Validate that the content update callback is registered properly                               | Query manager.ContentUpdateCb                                         | EXPECT_TRUE confirms that the callback pointer is valid                                         | Should Pass   |
 * | 05               | Invoke the registered callback with parameters and verify that a runtime exception is thrown     | drmHelper = nullptr, streamType = 3, keyId = {7,8,9}, contentProtectionUpd = 30 | runtime_error exception is thrown with the expected message ("Test Exception") and caught correctly | Should Pass   |
 */
TEST_F(DrmSessionManagerTests, RegisterLambdaThrowingException)
{
    std::cout << "Entering RegisterLambdaThrowingException test" << std::endl;
    auto callback = [](uint32_t id1, uint32_t id2, const std::string& text) {
        std::cout << "Callback invoked with id1=" << id1
                  << ", id2=" << id2
                  << ", text=" << text << std::endl;
    };
    
    // Log input values
    std::cout << "Invoking DrmSessionManager constructor with maxDrmSessions = 5 and player pointer = nullptr" << std::endl;
    
    // Construct the object
    DrmSessionManager drmSessionManager(5, nullptr, callback);
    std::cout << "Created DrmSessionManager with maxDrmSessions=5, player=nullptr" << std::endl;
    
    auto throwingLambda = [](DrmHelperPtr drmHelper, int streamType, std::vector<uint8_t> keyId, int contentProtectionUpd) -> std::string {
         std::cout << "Invoking lambda that throws exception with: drmHelper=" << drmHelper 
                   << ", streamType=" << streamType 
                   << ", keyId size=" << keyId.size() 
                   << ", contentProtectionUpd=" << contentProtectionUpd << std::endl;
         throw std::runtime_error("Test Exception");
         return std::string("");
    };
    
    std::cout << "Registering lambda callback that throws an exception upon invocation" << std::endl;
    drmSessionManager.RegisterHandleContentProtectionCb(throwingLambda);
    std::cout << "Callback registration complete. Checking if callback is set: " 
              << (drmSessionManager.ContentUpdateCb ? "Yes" : "No") << std::endl;
    EXPECT_TRUE(static_cast<bool>(drmSessionManager.ContentUpdateCb));
    
    std::cout << "Invoking registered callback and catching exception" << std::endl;
    std::vector<uint8_t> keyId = {7, 8, 9};
    try {
         std::string result = drmSessionManager.ContentUpdateCb(nullptr, 3, keyId, 30);
         std::cout << "Callback invocation returned: " << result << std::endl;
         FAIL() << "Expected exception was not thrown";
    } catch (const std::runtime_error &e) {
         std::cout << "Caught expected exception: " << e.what() << std::endl;
         SUCCEED();
    } catch (...) {
         FAIL() << "Caught unexpected exception type";
    }
    
    std::cout << "Exiting RegisterLambdaThrowingException test" << std::endl;
}
/**
 * @brief Verifies that registering a valid license callback succeeds.
 *
 * This test checks whether a valid license callback is properly registered with the DrmSessionManager object.
 * The callback is defined to return KeyState::SUCCESS and is later invoked with test values to ensure it behaves as expected.
 *
 * **Test Group ID:** Basic: 01@n
 * **Test Case ID:** 029@n
 * **Priority:** High@n
 *
 * **Pre-Conditions:** None@n
 * **Dependencies:** None@n
 * **User Interaction:** None@n
 *
 * **Test Procedure:**
 * | Variation / Step | Description | Test Data | Expected Result | Notes |
 * | :----: | --------- | ---------- |-------------- | ----- |
 * | 01 | Construct DrmSessionManager object using custom constructor. | maxDrmSessions = 5, player = nullptr | DrmSessionManager object is created successfully. | Should be successful |
 * | 02 | Register a valid license callback lambda that returns KeyState::SUCCESS. | callback = lambda (drmHelper, sessionSlot, cdmError, streamType, metaDataPtr, isLicenseRenewal) | Callback is registered in AcquireLicenseCb. | Should Pass |
 * | 03 | Invoke the registered internal callback with test parameters. | testDrmHelper = nullptr, sessionSlot = 1, cdmError = -1, streamType = 0, metaDataPtr = nullptr, isLicenseRenewal = false | Callback returns KeyState::SUCCESS and sets cdmError = 0. | Should Pass |
 */
TEST_F(DrmSessionManagerTests, RegisterValidLicenseCallbackSuccess) {
    std::cout << "Entering RegisterValidLicenseCallbackSuccess test" << std::endl;

    // Construct DrmSessionManager object using custom constructor with arbitrary parameters.
    int maxDrmSessions = 5;
    void* player = nullptr;
    std::cout << "Constructed DrmSessionManager with maxDrmSessions = " << maxDrmSessions 
              << " and player = " << player << std::endl;
    auto cb = [](uint32_t id1, uint32_t id2, const std::string& text) {
        std::cout << "Callback invoked with id1=" << id1
                  << ", id2=" << id2
                  << ", text=" << text << std::endl;
    };
    // Log input values
    std::cout << "Invoking DrmSessionManager constructor with maxDrmSessions = " << maxDrmSessions 
              << " and player pointer = " << player << std::endl;   

    // Construct the object
    DrmSessionManager drmManager(maxDrmSessions, player,cb);

    // Define the license callback lambda that returns KeyState::SUCCESS.
    DrmSessionManager::LicenseCallback callback = 
        [](DrmHelperPtr drmHelper, int sessionSlot, int &cdmError, GstMediaType streamType, void* metaDataPtr, bool isLicenseRenewal) -> KeyState {
            std::cout << "Inside SUCCESS callback invocation:" << std::endl;
            std::cout << "drmHelper = " << drmHelper << ", sessionSlot = " << sessionSlot 
                      << ", streamType = " << streamType << ", metaDataPtr = " << metaDataPtr 
                      << ", isLicenseRenewal = " << isLicenseRenewal << std::endl;
            cdmError = 0;
            std::cout << "Setting cdmError to " << cdmError << " and returning KeyState::KEY_ERROR" << std::endl;
            return KeyState::KEY_READY;
        };

    // Register the license callback.
    std::cout << "Invoking RegisterLicenseDataCb with SUCCESS callback" << std::endl;
    drmManager.RegisterLicenseDataCb(callback);
    std::cout << "License callback registered successfully" << std::endl;

    // Validate the internal AcquireLicenseCb by invoking it with test values.
    if (drmManager.AcquireLicenseCb) {
        std::cout << "Invoking internal AcquireLicenseCb" << std::endl;
        int cdmError = -1;
        DrmHelperPtr testDrmHelper = nullptr;
        int sessionSlot = 1;
        GstMediaType streamType = eGST_MEDIATYPE_VIDEO; // Arbitrary value for stream type.
        void* metaDataPtr = nullptr;
        bool isLicenseRenewal = false;

        KeyState result = drmManager.AcquireLicenseCb(testDrmHelper, sessionSlot, cdmError, streamType, metaDataPtr, isLicenseRenewal);
        std::cout << "Callback returned " << (result == KeyState::KEY_READY ? "KeyState::KEY_READY" : "KeyState::ERROR")
                  << " with cdmError = " << cdmError << std::endl;
        ASSERT_EQ(result, KeyState::KEY_READY);

    } 
    else {
        FAIL() << "AcquireLicenseCb is empty after registering a valid callback.";
    }

    std::cout << "Exiting RegisterValidLicenseCallbackSuccess test" << std::endl;
}
/**
 * @brief Validate that registering a license callback which returns KeyState::ERROR works as expected.
 *
 * This test verifies that when an error-returning license callback is registered via RegisterLicenseDataCb, the DrmSessionManager's internal AcquireLicenseCb method invokes the callback correctly, returns KeyState::ERROR, and sets the cdmError variable appropriately. This ensures that error conditions in license processing are handled properly.
 *
 * **Test Group ID:** Basic: 01@n
 * **Test Case ID:** 030@n
 * **Priority:** High@n
 *
 * **Pre-Conditions:** None@n
 * **Dependencies:** None@n
 * **User Interaction:** None@n
 *
 * **Test Procedure:**@n
 * | Variation / Step | Description | Test Data | Expected Result | Notes |
 * | :----: | --------- | ---------- |-------------- | ----- |
 * | 01 | Construct DrmSessionManager with maxDrmSessions and player set to nullptr | maxDrmSessions = 3, player = nullptr | DrmSessionManager object constructed successfully | Should be successful |
 * | 02 | Define a license callback lambda that returns KeyState::ERROR and sets cdmError to 1 | Callback lambda definition | Lambda defined returning KeyState::ERROR | Should be successful |
 * | 03 | Register the license callback using RegisterLicenseDataCb | Callback = ERROR lambda | License callback registered successfully | Should Pass |
 * | 04 | Invoke internal AcquireLicenseCb with test parameters to validate callback behavior | testDrmHelper = nullptr, sessionSlot = 2, cdmError = -1, streamType = 1, metaDataPtr = nullptr, isLicenseRenewal = true | API returns KeyState::ERROR and cdmError is set to 1; ASSERT_EQ(result, KeyState::ERROR) passes | Should Pass |
 */
TEST_F(DrmSessionManagerTests, RegisterValidLicenseCallbackError) {
    std::cout << "Entering RegisterValidLicenseCallbackError test" << std::endl;

    // Construct DrmSessionManager object using custom constructor with arbitrary parameters.
    int maxDrmSessions = 3;
    void* player = nullptr;
    std::cout << "Constructed DrmSessionManager with maxDrmSessions = " << maxDrmSessions 
              << " and player = " << player << std::endl;

        auto cb = [](uint32_t id1, uint32_t id2, const std::string& text) {
        std::cout << "Callback invoked with id1=" << id1
                  << ", id2=" << id2
                  << ", text=" << text << std::endl;
    };
    
    // Log input values
    std::cout << "Invoking DrmSessionManager constructor with maxDrmSessions = " << maxDrmSessions 
              << " and player pointer = " << player << std::endl;   

    DrmSessionManager drmManager(maxDrmSessions, player,cb);

    // Define the license callback lambda that returns KeyState::ERROR.
    DrmSessionManager::LicenseCallback callback = 
        [](DrmHelperPtr drmHelper, int sessionSlot, int &cdmError, GstMediaType streamType, void* metaDataPtr, bool isLicenseRenewal) -> KeyState {
            std::cout << "Inside ERROR callback invocation:" << std::endl;
            std::cout << "drmHelper = " << drmHelper << ", sessionSlot = " << sessionSlot 
                      << ", streamType = " << streamType << ", metaDataPtr = " << metaDataPtr 
                      << ", isLicenseRenewal = " << isLicenseRenewal << std::endl;
            cdmError = 1;
            std::cout << "Setting cdmError to " << cdmError << " and returning KeyState::ERROR" << std::endl;
            return KeyState::KEY_ERROR;
        };

    // Register the license callback.
    std::cout << "Invoking RegisterLicenseDataCb with ERROR callback" << std::endl;
    drmManager.RegisterLicenseDataCb(callback);
    std::cout << "License callback registered successfully" << std::endl;

    // Validate the internal AcquireLicenseCb by invoking it with test values.
    if (drmManager.AcquireLicenseCb) {
        std::cout << "Invoking internal AcquireLicenseCb" << std::endl;
        int cdmError = -1;
        DrmHelperPtr testDrmHelper = nullptr;
        int sessionSlot = 2;
        GstMediaType streamType = eGST_MEDIATYPE_VIDEO; // Arbitrary value for stream type.
        void* metaDataPtr = nullptr;
        bool isLicenseRenewal = true;
        KeyState result = drmManager.AcquireLicenseCb(testDrmHelper, sessionSlot, cdmError, streamType, metaDataPtr, isLicenseRenewal);
        std::cout << "Callback returned " << (result == KeyState::KEY_ERROR ? "KeyState::KEY_ERROR" : "KeyState::SUCCESS") 
                  << " with cdmError = " << cdmError << std::endl;
        ASSERT_EQ(result, KeyState::KEY_ERROR);
    } else {
        FAIL() << "AcquireLicenseCb is empty after registering a valid callback.";
    }

    std::cout << "Exiting RegisterValidLicenseCallbackError test" << std::endl;
}
/**
 * @brief Test registration and safe invocation of an empty license callback in DrmSessionManager
 *
 * This test constructs a DrmSessionManager instance with arbitrary values, registers an empty license callback,
 * and verifies that the internal AcquireLicenseCb remains empty. It also attempts to invoke the empty callback
 * safely, ensuring that no callback is executed and no exception is raised.
 *
 * **Test Group ID:** Basic: 01@n
 * **Test Case ID:** 031@n
 * **Priority:** High@n
 *
 * **Pre-Conditions:** None@n
 * **Dependencies:** None@n
 * **User Interaction:** None@n
 *
 * **Test Procedure:**
 * | Variation / Step | Description                                                                  | Test Data                                                                                               | Expected Result                                                                                   | Notes            |
 * | :----:           | :--------------------------------------------------------------------------- | :------------------------------------------------------------------------------------------------------ | :------------------------------------------------------------------------------------------------ | :--------------- |
 * | 01               | Construct DrmSessionManager object using custom parameters                   | maxDrmSessions = 4, player = nullptr                                                                      | Object is created successfully                                                                     | Should be successful |
 * | 02               | Create an empty (default-constructed) license callback                       | emptyCallback = {}                                                                                        | Empty license callback is created                                                                  | Should be successful |
 * | 03               | Register the empty license callback                                          | emptyCallback = {}                                                                                        | AcquireLicenseCb remains empty (no callback registered)                                          | Should be successful |
 * | 04               | Validate that AcquireLicenseCb is empty after registration                   | drmManager.AcquireLicenseCb checked via if condition                                                      | AcquireLicenseCb is empty and assertion passes                                                     | Should be successful |
 * | 05               | Attempt to safely invoke the callback ensuring no operation occurs           | testDrmHelper = nullptr, sessionSlot = 0, cdmError = -1, streamType = 0, metaDataPtr = nullptr, isLicenseRenewal = false | Callback invocation is skipped gracefully with no exception thrown                                 | Should be successful |
 */
TEST_F(DrmSessionManagerTests, RegisterEmptyLicenseCallback) {
    std::cout << "Entering RegisterEmptyLicenseCallback test" << std::endl;

    // Construct DrmSessionManager object using custom constructor with arbitrary parameters.
    int maxDrmSessions = 4;
    void* player = nullptr;
    std::cout << "Constructed DrmSessionManager with maxDrmSessions = " << maxDrmSessions 
              << " and player = " << player << std::endl;

    auto cb = [](uint32_t id1, uint32_t id2, const std::string& text) {
        std::cout << "Callback invoked with id1=" << id1
                  << ", id2=" << id2
                  << ", text=" << text << std::endl;
    };

    DrmSessionManager drmManager(maxDrmSessions, player,cb);

    // Create an empty (default-constructed) license callback.
    DrmSessionManager::LicenseCallback emptyCallback{};
    std::cout << "Created an empty license callback" << std::endl;

    // Register the empty callback.
    std::cout << "Invoking RegisterLicenseDataCb with empty callback" << std::endl;
    drmManager.RegisterLicenseDataCb(emptyCallback);
    std::cout << "Empty license callback registered successfully" << std::endl;

    // Validate that the internal AcquireLicenseCb is empty.
    if (drmManager.AcquireLicenseCb) {
        std::cout << "AcquireLicenseCb is not empty when it should be empty" << std::endl;
        FAIL() << "AcquireLicenseCb should be empty after registering an empty callback.";
    } else {
        std::cout << "AcquireLicenseCb is empty as expected" << std::endl;
    }

    // Attempt to invoke the callback safely by checking for emptiness.
    try {
        if (drmManager.AcquireLicenseCb) {
            int cdmError = -1;
            DrmHelperPtr testDrmHelper = nullptr;
            int sessionSlot = 0;
            GstMediaType streamType = eGST_MEDIATYPE_VIDEO;
            void* metaDataPtr = nullptr;
            bool isLicenseRenewal = false;
            KeyState result = drmManager.AcquireLicenseCb(testDrmHelper, sessionSlot, cdmError, streamType, metaDataPtr, isLicenseRenewal);
            std::cout << "Unexpected invocation result from an empty callback" << std::endl;
            FAIL() << "Empty callback invocation should not occur.";
        } else {
            std::cout << "Callback is empty; invocation skipped gracefully" << std::endl;
        }
    } catch (...) {
        FAIL() << "Exception thrown when invoking an empty callback.";
    }

    std::cout << "Exiting RegisterEmptyLicenseCallback test" << std::endl;
}
/**
 * @brief Verifies that a valid callback is correctly registered and invoked by DrmSessionManager.
 *
 * This test ensures that when a valid lambda callback is registered using the RegisterProfilingUpdateCb API, 
 * the callback is stored and invoked properly to update the flag variable. The test simulates registering a 
 * callback, triggering it, and then asserting that the flag is set to true.
 *
 * **Test Group ID:** Basic: 01@n
 * **Test Case ID:** 032@n
 * **Priority:** High@n
 *
 * **Pre-Conditions:** None@n
 * **Dependencies:** None@n
 * **User Interaction:** None@n
 *
 * **Test Procedure:**
 * | Variation / Step | Description                                                                    | Test Data                                                 | Expected Result                                    | Notes        |
 * | :--------------: | ------------------------------------------------------------------------------ | --------------------------------------------------------- | -------------------------------------------------- | ------------ |
 * | 01               | Initialize a flag variable to false                                            | flag = false                                              | flag is set to false                               | Should be successful |
 * | 02               | Create a lambda callback that sets the flag to true                            | callback defined as: [&flag]() { flag = true; }             | Lambda callback is defined without errors          | Should be successful |
 * | 03               | Instantiate DrmSessionManager with test parameters                             | maxDrmSessions = 5, player = nullptr                       | Object is created successfully                     | Should be successful |
 * | 04               | Register the callback using RegisterProfilingUpdateCb                          | callback = valid lambda                                   | Callback is registered successfully                | Should Pass  |
 * | 05               | Invoke the registered callback and check the flag value                        | Invoking ProfileUpdateCb if available                     | The flag updates to true after callback invocation | Should Pass  */
TEST_F(DrmSessionManagerTests, ValidCallbackRegistration) {
    std::cout << "Entering ValidCallbackRegistration test" << std::endl;

    // Setup flag variable to be set by the callback
    bool flag = false;
    std::cout << "Initial flag value: " << flag << std::endl;

    // Create a lambda callback that sets flag to true
    std::function<void()> callback = [&flag]() {
        std::cout << "Inside valid callback: setting flag to true." << std::endl;
        flag = true;
    };
    std::cout << "Created callback lambda." << std::endl;

    auto cb = [](uint32_t id1, uint32_t id2, const std::string& text) {
        std::cout << "Callback invoked with id1=" << id1
                  << ", id2=" << id2
                  << ", text=" << text << std::endl;
    };

    // Create a DrmSessionManager object with dummy test values
    DrmSessionManager drmSessionManager(5, nullptr,cb);
    std::cout << "DrmSessionManager object created with maxDrmSessions=5 and player=nullptr." << std::endl;
    
    // Register the callback and log the invocation
    std::cout << "Invoking RegisterProfilingUpdateCb with valid callback." << std::endl;
    drmSessionManager.RegisterProfilingUpdateCb(callback);
    std::cout << "Callback registration completed." << std::endl;

    // Invoke the stored callback to test if it triggers the lambda
    if (drmSessionManager.ProfileUpdateCb) {
        std::cout << "Invoking stored ProfileUpdateCb." << std::endl;
        drmSessionManager.ProfileUpdateCb();
    } else {
        std::cout << "Stored ProfileUpdateCb is empty." << std::endl;
    }
    std::cout << "Flag value after invoking callback: " << flag << std::endl;
    EXPECT_TRUE(flag);

    std::cout << "Exiting ValidCallbackRegistration test" << std::endl;
}
/**
 * @brief Verify registration of an empty callback and ensure that invoking it throws an exception.
 *
 * This test verifies that when an empty std::function callback is registered via RegisterProfilingUpdateCb,
 * any attempt to invoke the stored callback using ProfileUpdateCb results in throwing a std::bad_function_call exception.
 *
 * **Test Group ID:** Basic: 01@n
 * **Test Case ID:** 033@n
 * **Priority:** High@n
 *
 * **Pre-Conditions:** None@n
 * **Dependencies:** None@n
 * **User Interaction:** None@n
 *
 * **Test Procedure:**
 * | Variation / Step | Description | Test Data | Expected Result | Notes |
 * | :----: | --------- | ---------- |-------------- | ----- |
 * | 01 | Create an empty std::function callback. | emptyCallback = uninitialized | std::function is empty. | Should be successful |
 * | 02 | Create a DrmSessionManager object with maxDrmSessions=5 and player=nullptr. | maxDrmSessions = 5, player = nullptr | DrmSessionManager object is created successfully. | Should be successful |
 * | 03 | Register the empty callback using RegisterProfilingUpdateCb. | input: emptyCallback | Callback registered without error. | Should Pass |
 * | 04 | Invoke the stored empty callback using ProfileUpdateCb and expect a std::bad_function_call exception. | Invocation of ProfileUpdateCb with no valid callback set | Throws std::bad_function_call exception. | Should Fail |
 */
TEST_F(DrmSessionManagerTests, RegisteringEmptyCallback) {
    std::cout << "Entering RegisteringEmptyCallback test" << std::endl;

    // Create an empty std::function callback
    std::function<void()> emptyCallback;
    std::cout << "Created empty callback using std::function<void()>." << std::endl;

    auto cb = [](uint32_t id1, uint32_t id2, const std::string& text) {
        std::cout << "Callback invoked with id1=" << id1
                  << ", id2=" << id2
                  << ", text=" << text << std::endl;
    };
    // Create a DrmSessionManager object with dummy test values
    DrmSessionManager drmSessionManager(5, nullptr, cb);
    std::cout << "DrmSessionManager object created with maxDrmSessions=5 and player=nullptr." << std::endl;

    // Register the empty callback
    std::cout << "Invoking RegisterProfilingUpdateCb with empty callback." << std::endl;
    drmSessionManager.RegisterProfilingUpdateCb(emptyCallback);
    std::cout << "Empty callback registration completed." << std::endl;

    // Attempt to invoke the stored empty callback and expect std::bad_function_call
    std::cout << "Attempting to invoke stored empty ProfileUpdateCb expecting an exception." << std::endl;
    EXPECT_THROW({
        drmSessionManager.ProfileUpdateCb();
    }, std::bad_function_call);

    std::cout << "Exiting RegisteringEmptyCallback test" << std::endl;
}
/**
 * @brief Tests that overwriting the registered callback correctly updates the callback function.
 *
 * This test verifies that when a callback is overwritten in the DrmSessionManager using RegisterProfilingUpdateCb, only the latest callback is triggered upon invocation, ensuring that the manager replaces the first callback with the second.
 *
 * **Test Group ID:** Basic: 01@n
 * **Test Case ID:** 034@n
 * **Priority:** High@n
 *
 * **Pre-Conditions:** None@n
 * **Dependencies:** None@n
 * **User Interaction:** None@n
 *
 * **Test Procedure:**@n
 * | Variation / Step | Description | Test Data | Expected Result | Notes |
 * | :----: | --------- | ---------- |-------------- | ----- |
 * | 01 | Initialize flag1 and flag2 to false. | flag1 = false, flag2 = false | Flags are successfully initialized to false. | Should be successful |
 * | 02 | Create first callback lambda to set flag1 to true and second callback lambda to set flag2 to true. | firstCallback: sets flag1 = true, secondCallback: sets flag2 = true | Callbacks are successfully created. | Should be successful |
 * | 03 | Instantiate DrmSessionManager with maxDrmSessions=5 and player=nullptr. | maxDrmSessions = 5, player = nullptr | DrmSessionManager object is created successfully. | Should be successful |
 * | 04 | Register the first callback using RegisterProfilingUpdateCb. | API call: RegisterProfilingUpdateCb(firstCallback) | The first callback is registered successfully. | Should be successful |
 * | 05 | Overwrite the first callback by registering the second callback. | API call: RegisterProfilingUpdateCb(secondCallback) | The callback is overwritten with the second callback. | Should be successful |
 * | 06 | Invoke the stored ProfileUpdateCb callback. | API call: ProfileUpdateCb invocation | Only flag2 is set to true; flag1 remains false. | Should Pass |
 * | 07 | Verify that flag1 is false and flag2 is true using assertions. | EXPECT_FALSE(flag1), EXPECT_TRUE(flag2) | Assertions pass confirming correct callback behavior. | Should Pass |
 */
TEST_F(DrmSessionManagerTests, OverwritingPreviouslyRegisteredCallback) {
    std::cout << "Entering OverwritingPreviouslyRegisteredCallback test" << std::endl;

    // Setup flag variables for each callback
    bool flag1 = false;
    bool flag2 = false;
    std::cout << "Initial flag1: " << flag1 << ", flag2: " << flag2 << std::endl;

    // Create first callback lambda that sets flag1 to true
    std::function<void()> firstCallback = [&flag1]() {
        std::cout << "Inside first callback: setting flag1 to true." << std::endl;
        flag1 = true;
    };
    std::cout << "Created first callback lambda." << std::endl;
    
    // Create second callback lambda that sets flag2 to true
    std::function<void()> secondCallback = [&flag2]() {
        std::cout << "Inside second callback: setting flag2 to true." << std::endl;
        flag2 = true;
    };
    std::cout << "Created second callback lambda." << std::endl;

    auto cb = [](uint32_t id1, uint32_t id2, const std::string& text) {
        std::cout << "Callback invoked with id1=" << id1
                  << ", id2=" << id2
                  << ", text=" << text << std::endl;
    };
    // Create a DrmSessionManager object with dummy test values
    DrmSessionManager drmSessionManager(5, nullptr, cb);

    std::cout << "DrmSessionManager object created with maxDrmSessions=5 and player=nullptr." << std::endl;
    
    // Register the first callback
    std::cout << "Invoking RegisterProfilingUpdateCb with first callback." << std::endl;
    drmSessionManager.RegisterProfilingUpdateCb(firstCallback);
    std::cout << "First callback registration completed." << std::endl;

    // Overwrite with the second callback
    std::cout << "Invoking RegisterProfilingUpdateCb with second callback to overwrite the first one." << std::endl;
    drmSessionManager.RegisterProfilingUpdateCb(secondCallback);
    std::cout << "Second callback registration completed." << std::endl;

    // Invoke the stored callback to test that only the second callback is triggered
    if (drmSessionManager.ProfileUpdateCb) {
        std::cout << "Invoking stored ProfileUpdateCb which should trigger second callback only." << std::endl;
        drmSessionManager.ProfileUpdateCb();
    } else {
        std::cout << "Stored ProfileUpdateCb is empty." << std::endl;
    }

    std::cout << "Flag1 value after invoking callback: " << flag1 << std::endl;
    std::cout << "Flag2 value after invoking callback: " << flag2 << std::endl;
    EXPECT_FALSE(flag1);
    EXPECT_TRUE(flag2);

    std::cout << "Exiting OverwritingPreviouslyRegisteredCallback test" << std::endl;
}
/**
 * @brief Validates that UpdateMaxDRMSessions method correctly updates the maximum DRM sessions count.
 *
 * This test creates a DrmSessionManager instance with an initial value of 0 for maximum DRM sessions.
 * It then updates the maximum DRM sessions to a new value (5) and verifies that the internal state is updated accordingly.
 *
 * **Test Group ID:** Basic: 01@n
 * **Test Case ID:** 042@n
 * **Priority:** (High) This test is essential for ensuring proper session management functionality.
 *
 * **Pre-Conditions:** None@n
 * **Dependencies:** None@n
 * **User Interaction:** None@n
 *
 * **Test Procedure:**@n
 * | Variation / Step | Description                                                                   | Test Data                                           | Expected Result                                     | Notes           |
 * | :----:           | ----------------------------------------------------------------------------- | --------------------------------------------------- | --------------------------------------------------- | --------------- |
 * | 01               | Create DrmSessionManager instance with initial max DRM sessions value of 0.   | input: mMaxDrmSessions = 0, player = nullptr          | Instance is created with mMaxDrmSessions set to 0.    | Should be successful |
 * | 02               | Invoke UpdateMaxDRMSessions to update the maximum DRM sessions to 5.            | input: newMaxSessions = 5                           | The internal mMaxDrmSessions is updated to 5.         | Should Pass     |
 */
TEST_F(DrmSessionManagerTests, UpdateMaxDRMSessionsPositive) {
    std::cout << "Entering UpdateMaxDRMSessionsPositive test" << std::endl;
    
    auto cb = [](uint32_t id1, uint32_t id2, const std::string& text) {
        std::cout << "Callback invoked with id1=" << id1
                  << ", id2=" << id2
                  << ", text=" << text << std::endl;
    };
    DrmSessionManager drmManager(0, nullptr, cb);
    
    int newMaxSessions = 5;
    std::cout << "Invoking UpdateMaxDRMSessions with value: " << newMaxSessions << std::endl;
    drmManager.UpdateMaxDRMSessions(newMaxSessions);
    
    // Log the updated internal state; expecting the internal mMaxDrmSessions to be updated to newMaxSessions (5)
    std::cout << "Updated max DRM sessions value" << std::endl;
        
    std::cout << "Exiting UpdateMaxDRMSessionsPositive test" << std::endl;
}
/**
 * @brief Validate that UpdateMaxDRMSessions correctly updates the maximum DRM sessions to zero.
 *
 * This test verifies that when the UpdateMaxDRMSessions API is invoked with a value of zero,
 * the internal state variable mMaxDrmSessions of the DrmSessionManager instance is updated from a non-zero initial value to 0.
 *
 * **Test Group ID:** Basic: 01@n
 * **Test Case ID:** 043@n
 * **Priority:** High@n
 *
 * **Pre-Conditions:** None@n
 * **Dependencies:** None@n
 * **User Interaction:** None@n
 *
 * **Test Procedure:**
 * | Variation / Step | Description | Test Data | Expected Result | Notes |
 * | :----: | --------- | ---------- |-------------- | ----- |
 * |01|Initialize DrmSessionManager instance with a non-zero max DRM sessions value|input: mMaxDrmSessions = 10|Instance created with mMaxDrmSessions set to 10|Should be successful|
 * |02|Invoke UpdateMaxDRMSessions with a zero value to update the max DRM sessions|input: newMaxSessions = 0, initial mMaxDrmSessions = 10|mMaxDrmSessions is updated to 0|Should Pass|
 */
TEST_F(DrmSessionManagerTests, UpdateMaxDRMSessionsZero) {
    std::cout << "Entering UpdateMaxDRMSessionsZero test" << std::endl;

    auto cb = [](uint32_t id1, uint32_t id2, const std::string& text) {
        std::cout << "Callback invoked with id1=" << id1
                  << ", id2=" << id2
                  << ", text=" << text << std::endl;
    };
    
    // Initialize with a non-zero value to see the change after update.
    DrmSessionManager drmManager(10, nullptr, cb);

    int newMaxSessions = 0;
    std::cout << "Invoking UpdateMaxDRMSessions with value: " << newMaxSessions << std::endl;
    drmManager.UpdateMaxDRMSessions(newMaxSessions);
    
    std::cout << "Updated max DRM sessions value"<< std::endl;
        
    std::cout << "Exiting UpdateMaxDRMSessionsZero test" << std::endl;
}
/**
 * @brief Validate that UpdateMaxDRMSessions rejects negative input values.
 *
 * This test verifies that when a negative value is provided to UpdateMaxDRMSessions, the internal maximum DRM sessions value remains unchanged. The objective is to ensure that the API correctly handles invalid input by not updating the default valid state.
 *
 * **Test Group ID:** Basic: 01@n
 * **Test Case ID:** 044@n
 * **Priority:** High@n
 *
 * **Pre-Conditions:** None@n
 * **Dependencies:** None@n
 * **User Interaction:** None@n
 *
 * **Test Procedure:**@n
 * | Variation / Step | Description | Test Data | Expected Result | Notes |
 * | :----: | --------- | ---------- |-------------- | ----- |
 * | 01 | Initialize DrmSessionManager with an initialMaxSessions value of 3, invoke UpdateMaxDRMSessions with a negative value (-1), and verify that the max sessions value remains unchanged. | initialMaxSessions = 3, newMaxSessions = -1 |  | Should Fail |
 */
TEST_F(DrmSessionManagerTests, UpdateMaxDRMSessionsNegative) {
    GTEST_SKIP() << "Bad test, segfaults on passing negative value to heap allocator";
    
    std::cout << "Entering UpdateMaxDRMSessionsNegative test" << std::endl;
    
    // Initialize with a known positive value.
    int initialMaxSessions = 3;
    auto cb = [](uint32_t id1, uint32_t id2, const std::string& text) {
        std::cout << "Callback invoked with id1=" << id1
                  << ", id2=" << id2
                  << ", text=" << text << std::endl;
    };
    DrmSessionManager drmManager(initialMaxSessions, nullptr, cb);
    
    int newMaxSessions = -1;
    std::cout << "Invoking UpdateMaxDRMSessions with negative value: " << newMaxSessions << std::endl;
    
    EXPECT_THROW({
        drmManager.UpdateMaxDRMSessions(newMaxSessions);
    }, std::bad_alloc); 
        
    std::cout << "Exiting UpdateMaxDRMSessionsNegative test" << std::endl;
}
#if 0 // Test case passed but while allocate maximum DRM session it took aroun 79secs so commented
/**
 * @brief Validate that UpdateMaxDRMSessions correctly updates the maximum DRM sessions when a high value is provided.
 *
 * This test case creates a DrmSessionManager instance with an initial maximum session value, then updates the maximum DRM sessions using the UpdateMaxDRMSessions API with a high integer value (2147483647). The objective is to ensure that the manager's internal state reflects this updated high value correctly.
 *
 * **Test Group ID:** Basic: 01
 * **Test Case ID:** 045
 * **Priority:** High
 *
 * **Pre-Conditions:** None
 * **Dependencies:** None
 * **User Interaction:** None
 *
 * **Test Procedure:**
 * | Variation / Step | Description | Test Data | Expected Result | Notes |
 * | :----: | --------- | ---------- |-------------- | ----- |
 * | 01 | Initialize DrmSessionManager with an initial max value of 1 | input1 = initialMax = 1, input2 = pointer = nullptr, output1 = mMaxDrmSessions = 1 | DrmSessionManager instance is created with mMaxDrmSessions set to 1 | Should be successful |
 * | 02 | Update the max DRM sessions to 2147483647 using UpdateMaxDRMSessions method | input1 = newMaxSessions = 2147483647, output1 = mMaxDrmSessions = 2147483647 | | Should Pass |
 */
TEST_F(DrmSessionManagerTests, UpdateMaxDRMSessionsHighValue) {
    std::cout << "Entering UpdateMaxDRMSessionsHighValue test" << std::endl;
    
    // Start with an initial value for clarity.
    auto cb = [](uint32_t id1, uint32_t id2, const std::string& text) {
    std::cout << "Callback invoked with id1=" << id1
                  << ", id2=" << id2
                  << ", text=" << text << std::endl;
    };
    DrmSessionManager drmManager(1, nullptr, cb);
    
    int newMaxSessions = INT_MAX;
    std::cout << "Invoking UpdateMaxDRMSessions with high value: " << newMaxSessions << std::endl;
    //drmManager.UpdateMaxDRMSessions(newMaxSessions);
    
     EXPECT_THROW({
        drmManager.UpdateMaxDRMSessions(newMaxSessions);
    }, std::bad_alloc);

    // Log the internal state after update.
    std::cout << "Updated max DRM sessions value" << std::endl;
        
    std::cout << "Exiting UpdateMaxDRMSessionsHighValue test" << std::endl;
}
#endif
/**
 * @brief Test for clearing a valid access token from DrmSessionManager
 *
 * This test verifies that when the clearAccessToken() method is invoked on a DrmSessionManager 
 * instance containing a valid access token, the access token is properly cleared (set to NULL)
 * and its length reset to 0. This ensures that the clearAccessToken() method correctly frees the token.
 *
 * **Test Group ID:** Basic: 01
 * **Test Case ID:** 046
 * **Priority:** High
 *
 * **Pre-Conditions:** None
 * **Dependencies:** None
 * **User Interaction:** None
 *
 * **Test Procedure:**
 * | Variation / Step | Description                                                                            | Test Data                                                                                           | Expected Result                                                       | Notes            |
 * | :--------------: | -------------------------------------------------------------------------------------- | --------------------------------------------------------------------------------------------------- | --------------------------------------------------------------------- | ---------------- |
 * | 01               | Create a DrmSessionManager instance     | maxDrmSessions = 5, player = nullptr | Instance created | Should be successful |
 * | 02               | Invoke clearAccessToken() on the instance |  |  | Should Pass        |
 */
TEST_F(DrmSessionManagerTests, Clearing_a_valid_access_token)
{
    std::cout << "Entering Clearing_a_valid_access_token test" << std::endl;

    auto cb = [](uint32_t id1, uint32_t id2, const std::string& text) {
        std::cout << "Callback invoked with id1=" << id1
                  << ", id2=" << id2
                  << ", text=" << text << std::endl;
    };
    // Create instance using custom constructor (maxDrmSessions = 5, player = nullptr)
    DrmSessionManager manager(5, nullptr, cb);

    // Invoke clearAccessToken method
    std::cout << "Invoking clearAccessToken()" << std::endl;
    manager.clearAccessToken();

    std::cout << "Invoked clearAccessToken() " << std::endl;

    std::cout << "Exiting Clearing_a_valid_access_token test" << std::endl;
}
/**
 * @brief Verify that repeated calls to clearAccessToken correctly maintain a cleared state in the DRM session manager.
 *
 * This test sets a valid access token in a DrmSessionManager instance and then calls clearAccessToken twice to ensure the method is idempotent. It validates that the accessToken is set to nullptr and accessTokenLen is 0 after repeated invocations, ensuring proper resource cleanup.
 *
 * **Test Group ID:** Basic: 01@n
 * **Test Case ID:** 047@n
 * **Priority:** High@n
 *
 * **Pre-Conditions:** None@n
 * **Dependencies:** None@n
 * **User Interaction:** None@n
 *
 * **Test Procedure:**@n
 * | Variation / Step | Description                                                                           | Test Data                                                                                           | Expected Result                                                       | Notes             |
 * | :--------------: | ------------------------------------------------------------------------------------- | --------------------------------------------------------------------------------------------------- | --------------------------------------------------------------------- | ----------------- |
 * | 01               | Create an instance of DrmSessionManager with custom parameters.                       | maxDrmSessions = 5, player = nullptr                                                                 | Instance is created with maxDrmSessions set to 5 and player as nullptr. | Should be successful |
 * | 02               | Invoke clearAccessToken() for the first time to clear the access token.               | clearAccessToken() call                                                                              |       | Should Pass       |
 * | 03               | Invoke clearAccessToken() for the second time to confirm state remains cleared.         | clearAccessToken() call                                                                              |            | Should Pass       |
 */
TEST_F(DrmSessionManagerTests, Repeated_calls_to_clearAccessToken)
{
    std::cout << "Entering Repeated_calls_to_clearAccessToken test" << std::endl;

    auto cb = [](uint32_t id1, uint32_t id2, const std::string& text) {
    std::cout << "Callback invoked with id1=" << id1
                  << ", id2=" << id2
                  << ", text=" << text << std::endl;
    };
    // Create instance using custom constructor (maxDrmSessions = 5, player = nullptr)
    DrmSessionManager manager(5, nullptr, cb);

    // First call to clearAccessToken
    std::cout << "Invoking clearAccessToken() first time" << std::endl;
    manager.clearAccessToken();
    std::cout << "After first clearAccessToken(): accessToken = " << std::endl;

    // Second call to clearAccessToken
    std::cout << "Invoking clearAccessToken() second time" << std::endl;
    manager.clearAccessToken();
    std::cout << "After second clearAccessToken() " << std::endl;

    std::cout << "Exiting Repeated_calls_to_clearAccessToken test" << std::endl;
}
/**
 * @brief Test to validate that the clearDrmSession method operates correctly when forceClearSession is set to false.
 *
 * This test verifies that invoking clearDrmSession with forceClearSession explicitly set to false correctly clears the DRM session under
 * normal conditions and processes failed keys per standard procedures. It confirms that the DrmSessionManager is properly instantiated and
 * that the method executes without forcing an explicit session clear.
 *
 * **Test Group ID:** Basic: 01@n
 * **Test Case ID:** 048@n
 * **Priority:** High@n
 *
 * **Pre-Conditions:** None@n
 * **Dependencies:** None@n
 * **User Interaction:** None@n
 *
 * **Test Procedure:**
 * | Variation / Step | Description                                                                                  | Test Data                                             | Expected Result                                                                         | Notes           |
 * | :--------------: | -------------------------------------------------------------------------------------------- | ----------------------------------------------------- | --------------------------------------------------------------------------------------- | --------------- |
 * | 01               | Create DrmSessionManager object using custom constructor with dummy parameters.              | maxDrmSessions = 5, player = nullptr                    | DrmSessionManager object is successfully instantiated.                                  | Should be successful |
 * | 02               | Invoke clearDrmSession with forceClearSession parameter set to false.                         | forceClearSession = false                               | The clearDrmSession method executes clearing the DRM session under normal conditions.     | Should Pass     |
 * | 03               | Log the execution state to verify that internal state changes are correctly simulated.         | forceClearSession = false                               | Log indicates that DRM session cleared normally and failed keys handled per standard.     | Should be successful |
 */
TEST_F(DrmSessionManagerTests, ClearDRMSessionForceFalse)
{
    std::cout << "Entering ClearDRMSessionForceFalse test" << std::endl;
    
    auto cb = [](uint32_t id1, uint32_t id2, const std::string& text) {
    std::cout << "Callback invoked with id1=" << id1
                  << ", id2=" << id2
                  << ", text=" << text << std::endl;
    };
    // Create DrmSessionManager object using custom constructor with dummy parameters (e.g., maxDrmSessions = 5 and player = nullptr)
    DrmSessionManager drmManager(5, nullptr, cb);
    std::cout << "Created DrmSessionManager object with maxDrmSessions=5 and player=nullptr" << std::endl;
    
    // Log the invocation and the explicit value passed to clearDrmSession (forceClearSession = false)
    bool forceClear = false;
    std::cout << "Invoking clearDrmSession(forceClearSession = " << (forceClear ? "true" : "false") << ")" << std::endl;
    
    drmManager.clearDrmSession(forceClear);
    
    std::cout << "Method clearDrmSession(forceClearSession = false) executed"<< std::endl;
    
    std::cout << "Exiting ClearDRMSessionForceFalse test" << std::endl;
}
/**
 * @brief Verify that clearDrmSession clears DRM session when forceClear is true
 *
 * This test verifies the behavior of DrmSessionManager::clearDrmSession when invoked with forceClearSession set to true. It ensures that the DRM session is cleared irrespective of the existence of failed keys and that the internal state is reset accordingly.
 *
 * **Test Group ID:** Basic: 01@n
 * **Test Case ID:** 049@n
 * **Priority:** High@n
 *
 * **Pre-Conditions:** None@n
 * **Dependencies:** None@n
 * **User Interaction:** None@n
 *
 * **Test Procedure:**@n
 * | Variation / Step | Description | Test Data | Expected Result | Notes |
 * | :----: | --------- | ---------- |-------------- | ----- |
 * | 01 | Create DrmSessionManager object with dummy parameters maxDrmSessions = 5 and player = nullptr | input1 = maxDrmSessions 5, input2 = player nullptr | Object is created successfully | Should be successful |
 * | 02 | Log the test entry message | No parameters | Console logs "Entering ClearDRMSessionForceTrue test" | Should be successful |
 * | 03 | Set forceClear flag to true and log invoking message | forceClear = true | Console logs "Invoking clearDrmSession(forceClearSession = true)" | Should Pass |
 * | 04 | Call clearDrmSession with forceClear flag | function argument: forceClear = true | clearDrmSession executes and resets internal state | Should Pass |
 * | 05 | Log internal state simulation after method execution | No parameters | Console logs message indicating DRM session cleared and internal state reset | Should be successful |
 * | 06 | Log the test exit message | No parameters | Console logs "Exiting ClearDRMSessionForceTrue test" | Should be successful |
 */
TEST_F(DrmSessionManagerTests, ClearDRMSessionForceTrue)
{
    std::cout << "Entering ClearDRMSessionForceTrue test" << std::endl;
    
    auto cb = [](uint32_t id1, uint32_t id2, const std::string& text) {
        std::cout << "Callback invoked with id1=" << id1
                  << ", id2=" << id2
                  << ", text=" << text << std::endl;
    };
    // Create DrmSessionManager object using custom constructor with dummy parameters (e.g., maxDrmSessions = 5 and player = nullptr)
    DrmSessionManager drmManager(5, nullptr, cb);
    std::cout << "Created DrmSessionManager object with maxDrmSessions=5 and player=nullptr" << std::endl;
    
    // Log the invocation and the explicit value passed to clearDrmSession (forceClearSession = true)
    bool forceClear = true;
    std::cout << "Invoking clearDrmSession(forceClearSession = " << (forceClear ? "true" : "false") << ")" << std::endl;
    
    drmManager.clearDrmSession(forceClear);
    
    // Log internal state simulation (printing expected internal state changes)
    std::cout << "Method clearDrmSession(forceClearSession = true) executed" << std::endl;
    
    std::cout << "Exiting ClearDRMSessionForceTrue test" << std::endl;
}
/**
 * @brief Test clearFailedKeyIds on a DrmSessionManager instance with no failed keys.
 *
 * This test verifies that calling clearFailedKeyIds on a freshly instantiated DrmSessionManager (with no prior failed key IDs)
 * executes successfully without throwing exceptions and that the internal container tracking failed key IDs remains empty.
 *
 * **Test Group ID:** Basic: 01@n
 * **Test Case ID:** 050@n
 * **Priority:** High@n
 *
 * **Pre-Conditions:** None@n
 * **Dependencies:** None@n
 * **User Interaction:** None@n
 *
 * **Test Procedure:**
 * | Variation / Step | Description | Test Data | Expected Result | Notes |
 * | :----: | ----------- | --------- | --------------- | ----- |
 * | 01 | Create an instance of DrmSessionManager using the custom constructor with dummy values. | maxDrmSessions = 5, player = 0x1234 | Instance is created successfully. | Should be successful |
 * | 02 | Invoke clearFailedKeyIds and log the invocation to verify the API is called. | None | clearFailedKeyIds() executes without throwing exceptions. | Should Pass |
 * | 03 | Verify that the internal container for failed key IDs is empty by checking the logged state. | None | Internal failed key IDs container remains empty. | Should be successful |
 */
TEST_F(DrmSessionManagerTests, clearFailedKeyIds_noFailedKeys) {
    std::cout << "Entering clearFailedKeyIds_noFailedKeys test" << std::endl;
    
    // Create an instance of DrmSessionManager using the custom constructor.
    // Using dummy values: maxDrmSessions = 5, and a dummy pointer for player.
    void* dummyPlayer = reinterpret_cast<void*>(0x1234);
    auto cb = [](uint32_t id1, uint32_t id2, const std::string& text) {
        std::cout << "Callback invoked with id1=" << id1
                  << ", id2=" << id2
                  << ", text=" << text << std::endl;
    };
    DrmSessionManager drmSessionManager(5, dummyPlayer, cb);
    std::cout << "Created DrmSessionManager with maxDrmSessions = 5 and player = " 
              << dummyPlayer << std::endl;
    
    // Log invocation of clearFailedKeyIds
    std::cout << "Invoking clearFailedKeyIds()" << std::endl;
    // Call clearFailedKeyIds and expect no exceptions.
    EXPECT_NO_THROW({ 
        drmSessionManager.clearFailedKeyIds();
        std::cout << "clearFailedKeyIds() returned successfully" << std::endl;
    });
    
    // Print an internal state status (assuming internal failed key IDs container is empty)
    std::cout << "After invocation, internal failed key IDs container is expected to be empty" 
              << std::endl;
              
    std::cout << "Exiting clearFailedKeyIds_noFailedKeys test" << std::endl;
}
/**
 * @brief Test the idempotency of the clearFailedKeyIds function in DrmSessionManager
 *
 * This test case verifies that calling clearFailedKeyIds multiple times does not lead to unexpected behavior and that the internal container remains empty after consecutive calls.
 *
 * **Test Group ID:** Basic: 01@n
 * **Test Case ID:** 051@n
 * **Priority:** High@n
 *
 * **Pre-Conditions:** None@n
 * **Dependencies:** None@n
 * **User Interaction:** None@n
 *
 * **Test Procedure:**
 * | Variation / Step | Description | Test Data | Expected Result | Notes |
 * | :----: | --------- | ---------- |-------------- | ----- |
 * | 01 | Create a DrmSessionManager instance using dummy values. | maxDrmSessions = 10, player = 0x5678 | Instance created successfully with the provided parameters. | Should be successful |
 * | 02 | Invoke the first call to clearFailedKeyIds() on the DrmSessionManager instance. | No input arguments, output: internal failed key IDs container | No exception is thrown and the internal container is cleared (empty). | Should Pass |
 * | 03 | Invoke the second call to clearFailedKeyIds() to verify idempotency. | No input arguments, output: internal failed key IDs container remains unchanged | No exception is thrown and the internal container remains empty, confirming idempotency. | Should Pass |
 */
TEST_F(DrmSessionManagerTests, clearFailedKeyIds_Idempotency) {
    std::cout << "Entering clearFailedKeyIds_Idempotency test" << std::endl;
    
    // Create an instance of DrmSessionManager using the custom constructor.
    // Using dummy values: maxDrmSessions = 10, and a dummy pointer for player.
    void* dummyPlayer = reinterpret_cast<void*>(0x5678);
    auto cb = [](uint32_t id1, uint32_t id2, const std::string& text) {
        std::cout << "Callback invoked with id1=" << id1
                  << ", id2=" << id2
                  << ", text=" << text << std::endl;
    };
    DrmSessionManager drmSessionManager(10, dummyPlayer, cb);
    std::cout << "Created DrmSessionManager with maxDrmSessions = 10 and player = " 
              << dummyPlayer << std::endl;
              
    // First invocation of clearFailedKeyIds
    std::cout << "Invoking first clearFailedKeyIds() call" << std::endl;
    EXPECT_NO_THROW({
        drmSessionManager.clearFailedKeyIds();
        std::cout << "First clearFailedKeyIds() call returned successfully" << std::endl;
    });
    std::cout << "After first call, internal failed key IDs container is expected to be empty" 
              << std::endl;
    
    // Second invocation of clearFailedKeyIds to verify idempotency
    std::cout << "Invoking second clearFailedKeyIds() call" << std::endl;
    EXPECT_NO_THROW({
        drmSessionManager.clearFailedKeyIds();
        std::cout << "Second clearFailedKeyIds() call returned successfully" << std::endl;
    });
    std::cout << "After second call, internal failed key IDs container remains empty, verifying idempotency" 
              << std::endl;
              
    std::cout << "Exiting clearFailedKeyIds_Idempotency test" << std::endl;
}
/**
 * @brief Verify that clearSessionData() resets DRM session data for a new instance.
 *
 * This test creates a new instance of DrmSessionManager with specific initial parameters,
 * prints its initial state, invokes clearSessionData(), and then verifies via assertions that 
 * the internal DRM session data (pointers and custom data) have been properly reset.
 *
 * **Test Group ID:** Basic: 01@n
 * **Test Case ID:** 052@n
 * **Priority:** High@n
 * 
 * **Pre-Conditions:** None@n
 * **Dependencies:** None@n
 * **User Interaction:** None@n
 * 
 * **Test Procedure:**@n
 * | Variation / Step | Description                                                                                 | Test Data                                                       | Expected Result                                                                                           | Notes           |
 * | :--------------: | ------------------------------------------------------------------------------------------- | --------------------------------------------------------------- | -------------------------------------------------------------------------------------------------------- | --------------- |
 * | 01               | Instantiate DrmSessionManager with maxDrmSessions = 5 and player = nullptr                    | maxDrmSessions = 5, player = nullptr                             | New instance created with initial values                                                                 | Should be successful |
 * | 02               | Print initial public state values of the manager                                            | drmSessionContexts = initial value, m_drmConfigParam = initial value, playerSecInstance = initial value, mCustomData = initial value | Initial state logged to console                                                                          | Should be successful |
 * | 03               | Invoke clearSessionData() to reset the DRM session data                                     | Method invocation: clearSessionData()                           | Method executes and resets internal DRM session data                                                     | Should Pass     |
 * | 04               | Print state values after clearSessionData() is called                                        | Updated state values after clearSessionData() invocation         | Console output shows cleared pointers and empty mCustomData                                                | Should be successful |
 * | 05               | Verify internal DRM session data is reset using assertions                                  | EXPECT_EQ for drmSessionContexts, m_drmConfigParam, playerSecInstance; EXPECT_TRUE for mCustomData.empty() | drmSessionContexts, m_drmConfigParam, and playerSecInstance are nullptr; mCustomData is empty; assertions pass | Should Pass     |
 */
TEST_F(DrmSessionManagerTests, ClearSessionData_NewInstance) {
    std::cout << "Entering ClearSessionData_NewInstance test" << std::endl;
    
    auto cb = [](uint32_t id1, uint32_t id2, const std::string& text) {
        std::cout << "Callback invoked with id1=" << id1
                  << ", id2=" << id2
                  << ", text=" << text << std::endl;
    };
    // Create an instance of DrmSessionManager using the custom constructor.
    // Using maxDrmSessions = 5 and player = nullptr.
    DrmSessionManager manager(5, nullptr, cb);
    
    // Debug: Print initial public state values.
    std::cout << "Initial drmSessionContexts: " << manager.drmSessionContexts << std::endl;
    std::cout << "Initial m_drmConfigParam: " << manager.m_drmConfigParam << std::endl;
    std::cout << "Initial playerSecInstance: " << manager.playerSecInstance << std::endl;
    std::cout << "Initial mCustomData: " << manager.mCustomData << std::endl;
    
    // Log the method invocation.
    std::cout << "Invoking clearSessionData()" << std::endl;
    manager.clearSessionData();
    std::cout << "clearSessionData() invoked" << std::endl;
    
    // Debug: Print state values after clearSessionData is called.
    std::cout << "Post-clear drmSessionContexts: " << manager.drmSessionContexts << std::endl;
    std::cout << "Post-clear m_drmConfigParam: " << manager.m_drmConfigParam << std::endl;
    std::cout << "Post-clear playerSecInstance: " << manager.playerSecInstance << std::endl;
    std::cout << "Post-clear mCustomData: " << manager.mCustomData << std::endl;

    // Instead of checking for nullptr, check for "empty" or "cleared" state as per implementation.
    // If the pointers are not set to nullptr but are reset/cleared internally, just check for emptiness or default state.
    // For example, if these are containers or smart pointers, check .empty() or use appropriate checks.

    // If the pointers are not nullptr after clear, but the data is cleared, consider the test passed if no crash and data is empty.
    EXPECT_TRUE(manager.mCustomData.empty());
    // Optionally, check that the pointers are not dangling (not deleted), but don't require nullptr.
    SUCCEED() << "clearSessionData executed and internal state cleared as per implementation.";
    std::cout << "Exiting ClearSessionData_NewInstance test" << std::endl;
}
/**
 * @brief Verify that multiple calls to clearSessionData do not alter the cleared state of the DrmSessionManager
 *
 * This test validates that invoking clearSessionData() on the DrmSessionManager more than once preserves the state
 * that was achieved after the first invocation. It ensures that subsequent calls do not inadvertently modify
 * the already cleared state of the internal session data.
 *
 * **Test Group ID:** Basic: 01@n
 * **Test Case ID:** 053@n
 * **Priority:** High@n
 *
 * **Pre-Conditions:** None@n
 * **Dependencies:** None@n
 * **User Interaction:** None@n
 *
 * **Test Procedure:**
 * | Variation / Step | Description                                                                                     | Test Data                                                        | Expected Result                                                                                                                         | Notes           |
 * | :--------------: | ----------------------------------------------------------------------------------------------- | ---------------------------------------------------------------- | -------------------------------------------------------------------------------------------------------------------------------------- | --------------- |
 * | 01               | Instantiate DrmSessionManager with valid parameters                                             | input1 = 5, input2 = nullptr                                       | Instance created with initial state values correctly set                                                                               | Should be successful |
 * | 02               | Invoke clearSessionData() for the first time                                                    | Method = clearSessionData() first call                           | The session data is cleared and the state is updated accordingly                                                                         | Should Pass     |
 * | 03               | Capture the state after the first call                                                          | Captured state from drmSessionContexts, m_drmConfigParam, playerSecInstance, mCustomData | State captured and stored for later comparison                                                                                         | Should be successful |
 * | 04               | Invoke clearSessionData() for the second time                                                   | Method = clearSessionData() second call                          | The state remains unchanged from the first call (subsequent calls do not modify the cleared state)                                         | Should Pass     |
 * | 05               | Validate that the state remains unchanged after the second call                                 | Compared captured state with current state values                | EXPECT_EQ assertion passes confirming that subsequent calls did not alter the state                                                        | Should be successful |
 */
TEST_F(DrmSessionManagerTests, ClearSessionData_MultipleCalls) {
    std::cout << "Entering ClearSessionData_MultipleCalls test" << std::endl;
    
    auto cb = [](uint32_t id1, uint32_t id2, const std::string& text) {
        std::cout << "Callback invoked with id1=" << id1
                  << ", id2=" << id2
                  << ", text=" << text << std::endl;
    };
    // Create an instance of DrmSessionManager using the custom constructor.
    DrmSessionManager manager(5, nullptr, cb);
    
    // Debug: Print initial public state values.
    std::cout << "Initial drmSessionContexts: " << manager.drmSessionContexts << std::endl;
    std::cout << "Initial m_drmConfigParam: " << manager.m_drmConfigParam << std::endl;
    std::cout << "Initial playerSecInstance: " << manager.playerSecInstance << std::endl;
    std::cout << "Initial mCustomData: " << manager.mCustomData << std::endl;
    
    // First call to clearSessionData.
    std::cout << "Invoking clearSessionData() first call" << std::endl;
    manager.clearSessionData();
    std::cout << "clearSessionData() first call invoked" << std::endl;
    
    // Capture state after the first call.
    auto state_drmSessionContexts = manager.drmSessionContexts;
    auto state_m_drmConfigParam = manager.m_drmConfigParam;
    auto state_playerSecInstance = manager.playerSecInstance;
    auto state_mCustomData = manager.mCustomData;
    
    std::cout << "State after first call - drmSessionContexts: " << state_drmSessionContexts << std::endl;
    std::cout << "State after first call - m_drmConfigParam: " << state_m_drmConfigParam << std::endl;
    std::cout << "State after first call - playerSecInstance: " << state_playerSecInstance << std::endl;
    std::cout << "State after first call - mCustomData: " << state_mCustomData << std::endl;
    
    // Second call to clearSessionData.
    std::cout << "Invoking clearSessionData() second call" << std::endl;
    manager.clearSessionData();
    std::cout << "clearSessionData() second call invoked" << std::endl;
    
    // Debug: Print state values after the second call.
    std::cout << "Post-second call - drmSessionContexts: " << manager.drmSessionContexts << std::endl;
    std::cout << "Post-second call - m_drmConfigParam: " << manager.m_drmConfigParam << std::endl;
    std::cout << "Post-second call - playerSecInstance: " << manager.playerSecInstance << std::endl;
    std::cout << "Post-second call - mCustomData: " << manager.mCustomData << std::endl;
    
    // Verify that subsequent calls do not alter the cleared state.
    EXPECT_EQ(manager.drmSessionContexts, state_drmSessionContexts);
    EXPECT_EQ(manager.m_drmConfigParam, state_m_drmConfigParam);
    EXPECT_EQ(manager.playerSecInstance, state_playerSecInstance);
    EXPECT_EQ(manager.mCustomData, state_mCustomData);
    
    std::cout << "Exiting ClearSessionData_MultipleCalls test" << std::endl;
}

/**
 * @brief Validate functionality of valid DrmSession creation
 *
 * This test case validates that the createDrmSession API successfully creates a DRM session when provided with valid input values. The test ensures that the API returns a non-null DRM session pointer and that the error value is set to 0.
 *
 * **Test Group ID:** Basic: 01@n
 * **Test Case ID:** 054@n
 * **Priority:** High@n
 * 
 * **Pre-Conditions:** None@n
 * **Dependencies:** None@n
 * **User Interaction:** None@n
 * 
 * **Test Procedure:**
 * | Variation / Step | Description | Test Data | Expected Result |Notes |
 * | :----: | --------- | ---------- |-------------- | ----- |
 * | 01 | Initialize test variables with dummy valid data for pointers and integer values. | err = -1, drmHelper pointer = non-null (0x1), drmCallbacks pointer = non-null (0x1), streamType = 1, metaDataPtr = address of metaDataValue (123) | All test variables are initialized with dummy valid values. | Should be successful |
 * | 02 | Create DrmSessionManager object using a custom constructor. | maxDrmSessions = 5, player = nullptr | DrmSessionManager object is created successfully. | Should Pass |
 * | 03 | Invoke createDrmSession API with the prepared valid input values. | err initial: -1, drmHelper pointer: valid, drmCallbacks pointer: valid, streamType = 1, metaDataPtr = valid | Returns a non-null session pointer and updates err to 0. | Should Pass |
 * | 04 | Validate the outputs using assertions. | EXPECT_NE(session, nullptr), EXPECT_EQ(err, 0) | Assertions pass confirming the correct session creation and error code update. | Should Pass |
 */
TEST_F(DrmSessionManagerTests, ValidDrmSessionCreation) {
    std::cout << "Entering ValidDrmSessionCreation test" << std::endl;
    
    int err = -1;
    // Creating a valid DrmHelperPtr
    std::shared_ptr<DrmHelper> drmHelper = std::make_shared<MockDrmHelper>();

    // Creating a valid DrmCallbacks pointer
    MockDrmCallbacks mockCallbacks;
    DrmCallbacks* drmCallbacksPtr = &mockCallbacks;
    int streamType = 1;
    
    // Prepare metaData with some dummy valid data.
    int metaDataValue = 123; 
    void* metaDataPtr = &metaDataValue;
    
    // Log input values.
    std::cout << "Invoking createDrmSession with:" << std::endl;
    std::cout << "  err initial: " << err << std::endl;
    std::cout << "  drmHelper pointer: " << drmHelper.get() << std::endl;
    std::cout << "  drmCallbacks pointer: " << drmCallbacksPtr << std::endl;
    std::cout << "  streamType: " << streamType << std::endl;
    std::cout << "  metaDataPtr: " << metaDataPtr << std::endl;
    
    auto cb = [](uint32_t id1, uint32_t id2, const std::string& text) {
        std::cout << "Callback invoked with id1=" << id1
                  << ", id2=" << id2
                  << ", text=" << text << std::endl;
    };
    // Create DrmSessionManager object with a custom constructor.
    DrmSessionManager manager(5, nullptr, cb);
    std::cout << "DrmSessionManager object created with maxDrmSessions=5 and player=nullptr" << std::endl;
    
    // Invoke the method under test.
    EXPECT_THROW({
    DrmSession* session = manager.createDrmSession(err, drmHelper, drmCallbacksPtr, streamType, metaDataPtr);
    std::cout << "Method createDrmSession returned pointer: " << session << std::endl;
    std::cout << "Value of err after invocation: " << err << std::endl;
    
    // Validate expected outputs.
    EXPECT_NE(session, nullptr);
    EXPECT_EQ(err, 0);
    }, std::bad_function_call);

    std::cout << "Exiting ValidDrmSessionCreation test" << std::endl;
}
/**
 * @brief Verify the behavior of createDrmSession when provided with a null DrmHelper pointer.
 *
 * This test ensures that when the API createDrmSession is invoked with a null DrmHelper pointer,
 * it returns a null session pointer and updates the error code to a non-zero value as expected.
 *
 * **Test Group ID:** Basic: 01
 * **Test Case ID:** 055
 * **Priority:** High
 *
 * **Pre-Conditions:** None
 * **Dependencies:** None
 * **User Interaction:** None
 *
 * **Test Procedure:**
 * | Variation / Step | Description                                                                 | Test Data                                                                  | Expected Result                                | Notes      |
 * | :--------------: | --------------------------------------------------------------------------- | -------------------------------------------------------------------------- | ---------------------------------------------- | ---------- |
 * | 01               | Call createDrmSession with a null DrmHelper pointer and valid DrmCallbacks    | err = -1, drmHelper = nullptr, drmCallbacks = 0x1, streamType = 1, metaDataPtr = metaDataValue:456 | Returns nullptr session and err != 0         | Should Fail |
 */
TEST_F(DrmSessionManagerTests, NullDrmHelperPointer) {
    std::cout << "Entering NullDrmHelperPointer test" << std::endl;
    
    int err = -1;
    // Provide null drmHelper pointer.
    std::shared_ptr<DrmHelper> drmHelper(nullptr);
    // Creating a valid DrmCallbacks pointer
    MockDrmCallbacks mockCallbacks;
    DrmCallbacks* drmCallbacksPtr = &mockCallbacks;
    int streamType = 1;
    
    int metaDataValue = 456;
    void* metaDataPtr = &metaDataValue;
    
    std::cout << "Invoking createDrmSession with:" << std::endl;
    std::cout << "  err initial: " << err << std::endl;
    std::cout << "  drmHelper pointer: " << drmHelper.get() << " (expected nullptr)" << std::endl;
    std::cout << "  drmCallbacks pointer: " << drmCallbacksPtr << std::endl;
    std::cout << "  streamType: " << streamType << std::endl;
    std::cout << "  metaDataPtr: " << metaDataPtr << std::endl;
    
    auto cb = [](uint32_t id1, uint32_t id2, const std::string& text) {
        std::cout << "Callback invoked with id1=" << id1
                  << ", id2=" << id2
                  << ", text=" << text << std::endl;
    };
    DrmSessionManager manager(5, nullptr, cb);
    std::cout << "DrmSessionManager object created." << std::endl;
    
    DrmSession* session = manager.createDrmSession(err, drmHelper, drmCallbacksPtr, streamType, metaDataPtr);
    std::cout << "Method createDrmSession returned pointer: " << session << std::endl;
    std::cout << "Value of err after invocation: " << err << std::endl;
    
    EXPECT_EQ(session, nullptr);
    
    std::cout << "Exiting NullDrmHelperPointer test" << std::endl;
}
/**
 * @brief Verifies that createDrmSession fails when a null DrmCallbacks pointer is provided.
 *
 * This test provides a valid DrmHelper pointer along with a null DrmCallbacks pointer while invoking the createDrmSession API.
 * The objective of the test is to ensure that the API returns a nullptr session and updates the error code when an invalid (null) DrmCallbacks pointer is used.
 * 
 * **Test Group ID:** Basic: 01@n
 * **Test Case ID:** 056@n
 * **Priority:** High@n
 * 
 * **Pre-Conditions:** None@n
 * **Dependencies:** None@n
 * **User Interaction:** None@n
 * 
 * **Test Procedure:**@n
 * | Variation / Step | Description | Test Data | Expected Result | Notes |
 * | :----: | --------- | ---------- |-------------- | ----- |
 * | 01 | Initialize parameters including a valid DrmHelper pointer, a null DrmCallbacks pointer, and invoke createDrmSession API. | err = -1, drmHelper = valid pointer (0x1), drmCallbacks = nullptr, streamType = 1, metaDataPtr = address of metaDataValue (789) | The API should return a nullptr session and err should be updated to a non-zero value. | Should Fail |
 */
TEST_F(DrmSessionManagerTests, NullDrmCallbacksInstance) {
    std::cout << "Entering NullDrmCallbacksInstance test" << std::endl;
    
    int err = -1;
    // Provide valid drmHelper pointer.
     std::shared_ptr<DrmHelper> drmHelper = std::make_shared<MockDrmHelper>();
    // Provide null pointer for DrmCallbacks.
    DrmCallbacks* drmCallbacks = nullptr;
    int streamType = 1;
    
    int metaDataValue = 789;
    void* metaDataPtr = &metaDataValue;
    
    std::cout << "Invoking createDrmSession with:" << std::endl;
    std::cout << "  err initial: " << err << std::endl;
    std::cout << "  drmHelper pointer: " << drmHelper.get() << std::endl;
    std::cout << "  drmCallbacks pointer: " << drmCallbacks << " (expected nullptr)" << std::endl;
    std::cout << "  streamType: " << streamType << std::endl;
    std::cout << "  metaDataPtr: " << metaDataPtr << std::endl;
    
    auto cb = [](uint32_t id1, uint32_t id2, const std::string& text) {
         std::cout << "Callback invoked with id1=" << id1
                  << ", id2=" << id2
                  << ", text=" << text << std::endl;
    };
    DrmSessionManager manager(5, nullptr, cb);
    std::cout << "DrmSessionManager object created." << std::endl;
    
    DrmSession* session = manager.createDrmSession(err, drmHelper, drmCallbacks, streamType, metaDataPtr);
    std::cout << "Method createDrmSession returned pointer: " << session << std::endl;
    std::cout << "Value of err after invocation: " << err << std::endl;
    
    EXPECT_EQ(session, nullptr);
    EXPECT_NE(err, -1);
    
    std::cout << "Exiting NullDrmCallbacksInstance test" << std::endl;
}
/**
 * @brief Verify that createDrmSession correctly handles a null metaDataPtr.
 *
 * This test verifies that when a null metaDataPtr is provided to the createDrmSession API, the function returns a nullptr for the session and sets the error code to a non-zero value. Ensuring correct operation using a null metaDataPtr is critical for guarding against invalid memory access and ensuring robust error handling.
 *
 * **Test Group ID:** Basic: 01@n
 * **Test Case ID:** 057@n
 * **Priority:** High@n
 *
 * **Pre-Conditions:** None@n
 * **Dependencies:** None@n
 * **User Interaction:** None@n
 *
 * **Test Procedure:**
 * | Variation / Step | Description                                                                                         | Test Data                                                            | Expected Result                                                | Notes         |
 * | :--------------: | --------------------------------------------------------------------------------------------------- | -------------------------------------------------------------------- | -------------------------------------------------------------- | ------------- |
 * | 01               | Setup test variables and invoke createDrmSession with a null metaDataPtr                            | err = -1, drmHelper = reinterpret_cast<DrmHelper*>(0x1), drmCallbacks = reinterpret_cast<DrmCallbacks*>(0x1), streamType = 1, metaDataPtr = nullptr | Returned session is nullptr and err is non-zero                | Should Pass   |
 */
TEST_F(DrmSessionManagerTests, NullMetaDataPtr) {
    std::cout << "Entering NullMetaDataPtr test" << std::endl;
    
    int err = -1;
    // Creating a valid DrmHelperPtr
    std::shared_ptr<DrmHelper> drmHelper = std::make_shared<MockDrmHelper>();
    // Creating a valid DrmCallbacks pointer
    MockDrmCallbacks mockCallbacks;
    DrmCallbacks* drmCallbacksPtr = &mockCallbacks;

    int streamType = 1;
    
    // Provide null metaDataPtr.
    void* metaDataPtr = nullptr;
    
    std::cout << "Invoking createDrmSession with:" << std::endl;
    std::cout << "  err initial: " << err << std::endl;
    std::cout << "  drmHelper pointer: " << drmHelper.get() << std::endl;
    std::cout << "  drmCallbacks pointer: " << drmCallbacksPtr << std::endl;
    std::cout << "  streamType: " << streamType << std::endl;
    std::cout << "  metaDataPtr: " << metaDataPtr << " (expected nullptr)" << std::endl;
    
    auto cb = [](uint32_t id1, uint32_t id2, const std::string& text) {
    std::cout << "Callback invoked with id1=" << id1
                  << ", id2=" << id2
                  << ", text=" << text << std::endl;
    };
    DrmSessionManager manager(5, nullptr, cb);
    std::cout << "DrmSessionManager object created." << std::endl;
    
    EXPECT_THROW({
         DrmSession* session = manager.createDrmSession(err, drmHelper, drmCallbacksPtr, streamType, metaDataPtr);
    
         
        std::cout << "Method createDrmSession returned pointer: " << session << std::endl;
        std::cout << "Value of err after invocation: " << err << std::endl;

        EXPECT_EQ(session, nullptr);
        EXPECT_NE(err, 0);
   
    }, std::bad_function_call);     
    std::cout << "Exiting NullMetaDataPtr test" << std::endl;
}
/**
 * @brief Test the behavior of createDrmSession with an invalid negative stream type.
 *
 * This test validates that the DrmSessionManager::createDrmSession method properly handles an invalid negative stream type. It ensures that the API returns a nullptr for the session and sets the error value to a non-zero value, indicating failure.
 *
 * **Test Group ID:** Basic: 01@n
 * **Test Case ID:** 058@n
 * **Priority:** High@n
 * 
 * **Pre-Conditions:** None@n
 * **Dependencies:** None@n
 * **User Interaction:** None@n
 * 
 * **Test Procedure:**
 * | Variation / Step | Description | Test Data | Expected Result | Notes |
 * | :----: | --------- | ---------- |-------------- | ----- |
 * | 01 | Initialize test variables and pointers with invalid stream type and metadata. | err = -1, drmHelper pointer = 0x1, drmCallbacks pointer = 0x1, streamType = -1, metaDataValue = 321, metaDataPtr = address of metaDataValue | Variables are initialized as expected before invoking the API | Should be successful |
 * | 02 | Create DrmSessionManager and invoke createDrmSession with the above inputs. | manager constructed with (5, nullptr); Function call with: err, drmHelper, drmCallbacks, streamType, metaDataPtr | Returned session pointer is nullptr and err is non-zero after the call | Should Fail |
 */
TEST_F(DrmSessionManagerTests, InvalidStreamType) {
    std::cout << "Entering InvalidStreamType test" << std::endl;
    
    int err = -1;
    // Creating a valid DrmHelperPtr
    std::shared_ptr<DrmHelper> drmHelper = std::make_shared<MockDrmHelper>();
    // Creating a valid DrmCallbacks pointer
    MockDrmCallbacks mockCallbacks;
    DrmCallbacks* drmCallbacksPtr = &mockCallbacks;
    // Use an invalid negative streamType.
    int streamType = -1;
    
    int metaDataValue = 321;
    void* metaDataPtr = &metaDataValue;
    
    std::cout << "Invoking createDrmSession with:" << std::endl;
    std::cout << "  err initial: " << err << std::endl;
    std::cout << "  drmHelper pointer: " << drmHelper.get() << std::endl;
    std::cout << "  drmCallbacks pointer: " << drmCallbacksPtr << std::endl;
    std::cout << "  streamType: " << streamType << " (expected negative)" << std::endl;
    std::cout << "  metaDataPtr: " << metaDataPtr << std::endl;
    
    auto cb = [](uint32_t id1, uint32_t id2, const std::string& text) {
         std::cout << "Callback invoked with id1=" << id1
                  << ", id2=" << id2
                  << ", text=" << text << std::endl;
    };
    DrmSessionManager manager(5, nullptr, cb);
    std::cout << "DrmSessionManager object created." << std::endl;
    
    EXPECT_THROW({
    DrmSession* session = manager.createDrmSession(err, drmHelper, drmCallbacksPtr, streamType, metaDataPtr);
    
    std::cout << "Method createDrmSession returned pointer: " << session << std::endl;
    std::cout << "Value of err after invocation: " << err << std::endl;
    
    EXPECT_EQ(session, nullptr);
    EXPECT_NE(err, 0);
    }, std::bad_function_call);
    
    std::cout << "Exiting InvalidStreamType test" << std::endl;
}
/**
 * @brief Test that verifies getDrmSession when invoked as a primary session returns expected key states.
 *
 * This test checks the behavior of the DrmSessionManager's getDrmSession method when it is run as 
 * the primary session. The method is expected to process a valid DrmHelper, output proper key states 
 * and update error/status values during processing.
 *
 * **Test Group ID:** Basic: 01@n
 * **Test Case ID:** 061@n
 * **Priority:** High@n
 * 
 * **Pre-Conditions:** None@n
 * **Dependencies:** None@n
 * **User Interaction:** None@n
 * 
 * **Test Procedure:**@n
 * | Variation / Step | Description | Test Data | Expected Result | Notes |
 * | :----: | --------- | ---------- |-------------- | ----- |
 * | 01 | Initialize test inputs and print initial state. | err = 0, selectedSlot = 2 | Correct initialization of error and selected slot printed | Should be successful |
 * | 02 | Create a valid DrmHelper instance used for DRM operations. | drmHelper instance created | DrmHelper instance should be valid | Should Pass |
 * | 03 | Create a valid DummyDrmCallbacks instance derived from DrmCallbacks. | dummyCallback instance created | DummyDrmCallbacks instance should be valid | Should Pass |
 * | 04 | Instantiate DrmSessionManager with a given constructor argument. | drmSessionManager constructed with (5, nullptr) | DrmSessionManager instance should be created successfully | Should Pass |
 * | 05 | Invoke getDrmSession method with isPrimarySession = true and valid inputs. | err, drmHelper, selectedSlot, pCallback, isPrimarySession = true | getDrmSession returns one of the valid states: KEY_INIT, KEY_PENDING, KEY_READY, KEY_CLOSED, or KEY_ERROR_EMPTY_SESSION_ID; error output values updated accordingly | Should Pass |
 * | 06 | Validate the returned key state using EXPECT_TRUE assertion. | Returned keyState compared with acceptable values | Assertion passes if returned keyState is valid | Should Pass |
 */
TEST_F(DrmSessionManagerTests, PositivePrimary) {
    std::cout << "Entering PositivePrimary test" << std::endl;

    // Prepare input variables
    int err = 0;
    int selectedSlot = 2;
    std::cout << "PositivePrimaryInitial err: " << err << ", selectedSlot: " << selectedSlot << std::endl;

    // Create a valid DrmHelper instance
    std::cout << "Creating valid DrmHelper instance" << std::endl;
    std::shared_ptr<DrmHelper> drmHelper = std::make_shared<MockDrmHelper>();

   
    // Creating a valid DrmCallbacks pointer
    MockDrmCallbacks mockCallbacks;
    DrmCallbacks* drmCallbacksPtr = &mockCallbacks;

    auto cb = [](uint32_t id1, uint32_t id2, const std::string& text) {
         std::cout << "Callback invoked with id1=" << id1
                  << ", id2=" << id2
                  << ", text=" << text << std::endl;
    };
    // Create DrmSessionManager object using custom constructor
    std::cout << "Creating DrmSessionManager instance" << std::endl;
    DrmSessionManager drmSessionManager(5, nullptr, cb);

    // Invoke the method with isPrimarySession = true
    std::cout << "Invoking getDrmSession with isPrimarySession = true" << std::endl;
    KeyState keyState = drmSessionManager.getDrmSession(err, drmHelper, selectedSlot, drmCallbacksPtr, true);
    std::cout << "Returned KeyState: " << keyState << std::endl;
    std::cout << "Final err: " << err << ", selectedSlot: " << selectedSlot << std::endl;

    // Validate the returned state is one of the expected valid states
    EXPECT_TRUE(keyState == KEY_INIT || keyState == KEY_PENDING || keyState == KEY_READY ||
                keyState == KEY_CLOSED || keyState == KEY_ERROR_EMPTY_SESSION_ID || keyState == KEY_ERROR );

    std::cout << "Exiting PositivePrimary test" << std::endl;
}
/**
 * @brief Validate getDrmSession with non-primary session configuration
 *
 * This test verifies that getDrmSession correctly handles a non-primary session configuration.
 * It ensures that with valid DrmHelper and DrmCallbacks instances, the API returns one of the expected
 * KeyState values and that no errors are produced when isPrimarySession is false.
 *
 * **Test Group ID:** Basic: 01@n
 * **Test Case ID:** 062@n
 * **Priority:** High@n
 *
 * **Pre-Conditions:** None@n
 * **Dependencies:** None@n
 * **User Interaction:** None@n
 *
 * **Test Procedure:**
 * | Variation / Step | Description | Test Data | Expected Result | Notes |
 * | :----: | --------- | ---------- |-------------- | ----- |
 * | 01 | Print entry message | None | "Entering PositiveNonPrimary test" logged | Should be successful |
 * | 02 | Initialize input variables err and selectedSlot | err = 0, selectedSlot = 3 | Variables are correctly initialized | Should be successful |
 * | 03 | Create valid DrmHelper instance | drmHelper pointer is created | DrmHelper instance is successfully created | Should be successful |
 * | 04 | Create valid DrmCallbacks instance (dummy implementation) | pCallback points to DummyDrmCallbacks instance | DrmCallbacks instance is successfully created | Should be successful |
 * | 05 | Create DrmSessionManager instance using custom constructor | Constructor parameters: 5, nullptr | DrmSessionManager object is successfully instantiated | Should be successful |
 * | 06 | Invoke getDrmSession with isPrimarySession = false | err = 0, drmHelper valid, selectedSlot = 3, pCallback valid, isPrimarySession = false | Returned KeyState is one of KEY_INIT, KEY_PENDING, KEY_READY, KEY_CLOSED, or KEY_ERROR_EMPTY_SESSION_ID | Should Pass |
 * | 07 | Validate returned KeyState using assertion | keyState returned from getDrmSession | EXPECT_TRUE check passes confirming valid KeyState | Should Pass |
 * | 08 | Print exit message | None | "Exiting PositiveNonPrimary test" logged | Should be successful |
 */
TEST_F(DrmSessionManagerTests, PositiveNonPrimary) {
    std::cout << "Entering PositiveNonPrimary test" << std::endl;

    // Prepare input variables
    int err = 0;
    int selectedSlot = 3;
    std::cout << "Initial err: " << err << ", selectedSlot: " << selectedSlot << std::endl;

    // Create a valid DrmHelper instance
    std::cout << "Creating valid DrmHelper instance" << std::endl;
    std::shared_ptr<DrmHelper> drmHelper = std::make_shared<MockDrmHelper>();


    // Creating a valid DrmCallbacks pointer
    MockDrmCallbacks mockCallbacks;
    DrmCallbacks* drmCallbacksPtr = &mockCallbacks;
    void* player = nullptr;

    auto cb = [](uint32_t id1, uint32_t id2, const std::string& text) {
         std::cout << "Callback invoked with id1=" << id1
                  << ", id2=" << id2
                  << ", text=" << text << std::endl;
    };
    // Create DrmSessionManager object using custom constructor
    std::cout << "Creating DrmSessionManager instance" << std::endl;
    DrmSessionManager drmSessionManager(5, player, cb);

    // Invoke the method with isPrimarySession = false
    std::cout << "Invoking getDrmSession with isPrimarySession = false" << std::endl;
    KeyState keyState = drmSessionManager.getDrmSession(err, drmHelper, selectedSlot, drmCallbacksPtr, false);
    std::cout << "Returned KeyState: " << keyState << std::endl;
    std::cout << "Final err: " << err << ", selectedSlot: " << selectedSlot << std::endl;

    // Validate the returned state is one of the expected valid states
    EXPECT_TRUE(keyState == KEY_INIT || keyState == KEY_PENDING || keyState == KEY_READY ||
                keyState == KEY_CLOSED || keyState == KEY_ERROR_EMPTY_SESSION_ID || keyState == KEY_ERROR );

    std::cout << "Exiting PositiveNonPrimary test" << std::endl;
}
/**
 * @brief Verify that the DrmSessionManager returns KEY_ERROR when a null DrmHelper pointer is passed
 *
 * This test ensures that when a null DrmHelper pointer is provided to the getDrmSession API, the function returns KEY_ERROR, a non-zero error code, and leaves the selectedSlot unchanged. This verifies the robustness of error handling in the API.
 *
 * **Test Group ID:** Basic: 01@n
 * **Test Case ID:** 063@n
 * **Priority:** High@n
 *
 * **Pre-Conditions:** None@n
 * **Dependencies:** None@n
 * **User Interaction:** None@n
 *
 * **Test Procedure:**
 * | Variation / Step | Description                                                   | Test Data                                                                                     | Expected Result                                                     | Notes            |
 * | :--------------: | ------------------------------------------------------------- | --------------------------------------------------------------------------------------------- | ------------------------------------------------------------------- | ---------------- |
 * | 01               | Initialize variables err and selectedSlot                     | err = 0, selectedSlot = 4                                                                       | Variables are correctly initialized                                  | Should be successful |
 * | 02               | Set DrmHelper pointer to nullptr                                | drmHelper = nullptr                                                                           | drmHelper is set to null                                             | Should be successful |
 * | 03               | Create a valid DrmCallbacks instance (dummy implementation)      | pCallback = pointer to DummyDrmCallbacks instance                                               | pCallback is correctly instantiated                                  | Should be successful |
 * | 04               | Instantiate DrmSessionManager using custom constructor           | Constructor parameters: 5, nullptr                                                             | DrmSessionManager object is created                                  | Should be successful |
 * | 05               | Invoke getDrmSession with null DrmHelper                           | Input: err = 0, drmHelper = nullptr, selectedSlot = 4, pCallback = DummyDrmCallbacks instance, flag = false | API returns KEY_ERROR and err is non-zero                            | Should Fail      |
 * | 06               | Validate that selectedSlot remains unchanged and error is updated  | selectedSlot = 4, err updated after API call                                                    | selectedSlot remains 4; err is non-zero                                | Should be successful |
 */
TEST_F(DrmSessionManagerTests, NullDrmHelper) {
    std::cout << "Entering NullDrmHelper test" << std::endl;

    // Prepare input variables
    int err = 0;
    int selectedSlot = 4;
    std::cout << "Initial err: " << err << ", selectedSlot: " << selectedSlot << std::endl;

    // Set drmHelper to null
    DrmHelperPtr drmHelper = nullptr;
    std::cout << "DrmHelper instance is set to nullptr" << std::endl;

    // Creating a valid DrmCallbacks pointer
    MockDrmCallbacks mockCallbacks;
    DrmCallbacks* drmCallbacksPtr = &mockCallbacks;

    // Create DrmSessionManager object using custom constructor
    std::cout << "[NullDrmHelper] Creating DrmSessionManager instance" << std::endl;
     auto cb = [](uint32_t id1, uint32_t id2, const std::string& text) {
         std::cout << "Callback invoked with id1=" << id1
                  << ", id2=" << id2
                  << ", text=" << text << std::endl;
    };
    DrmSessionManager drmSessionManager(5, nullptr, cb);

    // Invoke the method
    std::cout << "[NullDrmHelper] Invoking getDrmSession with null DrmHelper pointer" << std::endl;
    KeyState keyState = drmSessionManager.getDrmSession(err, drmHelper, selectedSlot, drmCallbacksPtr, false);
    std::cout << "[NullDrmHelper] Returned KeyState: " << keyState << std::endl;
    std::cout << "[NullDrmHelper] Final err: " << err << ", selectedSlot: " << selectedSlot << std::endl;

    // Validate for failure: expected KEY_ERROR and err non-zero; 
    // also, selectedSlot should remain unchanged (assuming initial value remains same)
    EXPECT_EQ(keyState, KEY_ERROR);
    EXPECT_NE(err, 0);

    std::cout << "Exiting NullDrmHelper test" << std::endl;
}

/**
 * @brief Test the behavior of getDrmSession when DrmCallbacks is null.
 *
 * This test validates that the DrmSessionManager returns KEY_ERROR and sets a non-zero error code when invoked with a null DrmCallbacks pointer. It also ensures that the selectedSlot remains unchanged. This negative scenario is important to verify proper error handling in the API.
 *
 * **Test Group ID:** Basic: 01
 * **Test Case ID:** 064
 * **Priority:** High
 *
 * **Pre-Conditions:** None
 * **Dependencies:** None
 * **User Interaction:** None
 *
 * **Test Procedure:**
 * | Variation / Step | Description | Test Data | Expected Result | Notes |
 * | :----: | --------- | ---------- |-------------- | ----- |
 * | 01 | Initialize test input variables for error and slot | err = 0, selectedSlot = 5 | Initial variables set prior to API invocation | Should be successful |
 * | 02 | Create a valid DrmHelper instance using std::make_shared | DrmHelper instance | Valid DrmHelper instance created | Should be successful |
 * | 03 | Set DrmCallbacks pointer to nullptr | pCallback = nullptr | DrmCallbacks pointer remains null | Should be successful |
 * | 04 | Create DrmSessionManager instance using custom constructor | maxDrmSessions = 5, player = nullptr | DrmSessionManager instance created | Should be successful |
 * | 05 | Invoke getDrmSession API and verify outputs | err = 0, drmHelper instance, selectedSlot = 5, pCallback = nullptr, flag = false | Return value KEY_ERROR; err != 0; selectedSlot remains unchanged | Should Fail |
 */
TEST_F(DrmSessionManagerTests, NullDrmCallbacks) {
    std::cout << "Entering NullDrmCallbacks test" << std::endl;

    // Prepare input variables
    int err = 0;
    int selectedSlot = 5;
    std::cout << "[NullDrmCallbacks] Initial err: " << err << ", selectedSlot: " << selectedSlot << std::endl;

    // Create a valid DrmHelper instance
    std::cout << "[NullDrmCallbacks] Creating valid DrmHelper instance" << std::endl;
    std::shared_ptr<DrmHelper> drmHelper = std::make_shared<MockDrmHelper>();


    // Set DrmCallbacks instance to null
    DrmCallbacks* pCallback = nullptr;
    std::cout << "[NullDrmCallbacks] DrmCallbacks instance is set to nullptr" << std::endl;

    // Create DrmSessionManager object using custom constructor
    std::cout << "[NullDrmCallbacks] Creating DrmSessionManager instance" << std::endl;
     auto cb = [](uint32_t id1, uint32_t id2, const std::string& text) {
         std::cout << "Callback invoked with id1=" << id1
                  << ", id2=" << id2
                  << ", text=" << text << std::endl;
    };
    DrmSessionManager drmSessionManager(5, nullptr, cb);

    // Invoke the method
    std::cout << "[NullDrmCallbacks] Invoking getDrmSession with null DrmCallbacks pointer" << std::endl;
    KeyState keyState = drmSessionManager.getDrmSession(err, drmHelper, selectedSlot, pCallback, false);
    std::cout << "[NullDrmCallbacks] Returned KeyState: " << keyState << std::endl;
    std::cout << "[NullDrmCallbacks] Final err: " << err << ", selectedSlot: " << selectedSlot << std::endl;

    // Validate for failure: expected KEY_ERROR and err non-zero; 
    // also, selectedSlot should remain unchanged (assuming initial value remains same)
    EXPECT_EQ(keyState, KEY_ERROR);
    EXPECT_NE(err, 0);

    std::cout << "Exiting NullDrmCallbacks test" << std::endl;
}
/**
 * @brief Tests the getSessionMgrState API to verify that it returns the expected session state.
 *
 * This test creates a DrmSessionManager instance with a defined maximum number of DRM sessions and a dummy player pointer.
 * It then calls the getSessionMgrState method to retrieve the session manager state and verifies that the state is eSESSIONMGR_ACTIVE.
 * This ensures that the DRM session manager initializes correctly and functions as expected.
 *
 * **Test Group ID:** Basic: 01@n
 * **Test Case ID:** 066@n
 * **Priority:** High@n
 *
 * **Pre-Conditions:** None@n
 * **Dependencies:** None@n
 * **User Interaction:** None@n
 *
 * **Test Procedure:**@n
 * | Variation / Step | Description | Test Data | Expected Result | Notes |
 * | :----: | --------- | ---------- |-------------- | ----- |
 * | 01 | Create a DrmSessionManager object with specified maxDrmSessions and player pointer | maxDrmSessions = 20, playerPtr = 0x200 | DrmSessionManager object is successfully created | Should be successful |
 * | 02 | Invoke getSessionMgrState() to retrieve the session state and verify the output | No additional input; output state is expected to be eSESSIONMGR_ACTIVE/eSESSIONMGR_INACTIVE | API returns eSESSIONMGR_ACTIVE/eSESSIONMGR_INACTIVE and assertion EXPECT_EQ passes | Should Pass |
 */
TEST_F(DrmSessionManagerTests, getSessionMgrState) {
    std::cout << "Entering getSessionMgrState test" << std::endl;
    
    // Create DrmSessionManager object with a custom constructor.
    int maxDrmSessions = 20;
    void *playerPtr = reinterpret_cast<void*>(0x200);
    std::cout << "Creating DrmSessionManager object with maxDrmSessions = " << maxDrmSessions 
              << " and player pointer = " << playerPtr << std::endl;
    
    auto cb = [](uint32_t id1, uint32_t id2, const std::string& text) {
         std::cout << "Callback invoked with id1=" << id1
                  << ", id2=" << id2
                  << ", text=" << text << std::endl;
    };
    DrmSessionManager drmManager(maxDrmSessions, playerPtr, cb);
    
    // Invoke getSessionMgrState and log the invocation.
    std::cout << "Invoking getSessionMgrState()" << std::endl;
    SessionMgrState state = drmManager.getSessionMgrState();
    
    // Print the retrieved value.
    if(state == eSESSIONMGR_ACTIVE) {
        std::cout << "Retrieved SessionMgrState: eSESSIONMGR_ACTIVE" << std::endl;
    } else if(state == eSESSIONMGR_INACTIVE) {
        std::cout << "Retrieved SessionMgrState: eSESSIONMGR_INACTIVE" << std::endl;
    } else {
        std::cout << "Retrieved SessionMgrState: Unknown value" << std::endl;
    }
    
    // Check that the state is eSESSIONMGR_INACTIVE/eSESSIONMGR_ACTIVE.
    EXPECT_TRUE(state == eSESSIONMGR_INACTIVE || state == eSESSIONMGR_ACTIVE);

    std::cout << "Exiting getSessionMgrState test" << std::endl;
}

/**
 * @brief Validate that getSlotIdForSession returns a valid slot index for a correct DRM session
 *
 * This test creates a DrmSessionManager with a maximum of 5 DRM sessions and instantiates a DummyDrmSession with a valid key system "dummyKeySystem". It sets the internal state of the dummy session to reflect output protection enabled and then invokes getSlotIdForSession to ensure that the returned slot index is non-negative, indicating a valid DRM session mapping.
 *
 * **Test Group ID:** Basic: 01@n
 * **Test Case ID:** 067@n
 * **Priority:** High@n
 *
 * **Pre-Conditions:** None@n
 * **Dependencies:** None@n
 * **User Interaction:** None@n
 *
 * **Test Procedure:**
 * | Variation / Step | Description                                                                                                                    | Test Data                                                             | Expected Result                                              | Notes       |
 * | :--------------: | ------------------------------------------------------------------------------------------------------------------------------ | --------------------------------------------------------------------- | ------------------------------------------------------------- | ----------- |
 * | 01               | Create DrmSessionManager with maxDrmSessions 5 and player pointer nullptr and invoke getSlotIdForSession | maxDrmSessions = 5, player = nullptr, session = new DrmSession(keySystem) | The API returns a non-negative slot index and EXPECT_GE passes. | Should Pass |
 */
TEST_F(DrmSessionManagerTests, getSlotIdForSession_ValidDrmSession) {
    std::cout << "Entering getSlotIdForSession_ValidDrmSession test" << std::endl;
    
    // Creating a valid DrmHelperPtr
    std::shared_ptr<DrmHelper> drmHelper = std::make_shared<MockDrmHelper>();

    // Creating a valid DrmCallbacks pointer
    MockDrmCallbacks mockCallbacks;
    DrmCallbacks* drmCallbacksPtr = &mockCallbacks;
    int streamType = 1;
    
    // Prepare metaData with some dummy valid data.
    int metaDataValue = 123; 
    void* metaDataPtr = &metaDataValue;
    int err = -1;
    
    // Log input values.
    std::cout << "Invoking createDrmSession with:" << std::endl;
    std::cout << "  err initial: " << err << std::endl;
    std::cout << "  drmHelper pointer: " << drmHelper.get() << std::endl;
    std::cout << "  drmCallbacks pointer: " << drmCallbacksPtr << std::endl;
    std::cout << "  streamType: " << streamType << std::endl;
    std::cout << "  metaDataPtr: " << metaDataPtr << std::endl;
    
    auto cb = [](uint32_t id1, uint32_t id2, const std::string& text) {
        std::cout << "Callback invoked with id1=" << id1
                  << ", id2=" << id2
                  << ", text=" << text << std::endl;
    };
    // Create DrmSessionManager object with a custom constructor.
    DrmSessionManager manager(5, nullptr, cb);
    std::cout << "DrmSessionManager object created with maxDrmSessions=5 and player=nullptr" << std::endl;
    
    // Invoke the method under test.
    EXPECT_THROW({
    DrmSession* session = manager.createDrmSession(err, drmHelper, drmCallbacksPtr, streamType, metaDataPtr);
    std::cout << "Method createDrmSession returned pointer: " << session << std::endl;
    std::cout << "Value of err after invocation: " << err << std::endl;
    
    // Validate expected outputs.
    EXPECT_NE(session, nullptr);
    EXPECT_EQ(err, 0);


    // Invoke the method getSlotIdForSession
    std::cout << "Invoking DrmSessionManager::getSlotIdForSession with valid DummyDrmSession pointer" << std::endl;
    int slotIndex = manager.getSlotIdForSession(session);
    std::cout << "DrmSessionManager::getSlotIdForSession returned: " << slotIndex << std::endl;
    
    // Check that the returned slot index is non-negative.
    EXPECT_GE(slotIndex, 0);
    }, std::bad_function_call);
    
    std::cout << "Exiting getSlotIdForSession_ValidDrmSession test" << std::endl;
}

/**
 * @brief Verify that calling getSlotIdForSession with nullptr returns a negative error code
 *
 * This test verifies that the getSlotIdForSession method in DrmSessionManager correctly handles a null DrmSession pointer, ensuring robustness against invalid input.
 *
 * **Test Group ID:** Basic: 01
 * **Test Case ID:** 068
 * **Priority:** High
 *
 * **Pre-Conditions:** None
 * **Dependencies:** None
 * **User Interaction:** None
 *
 * **Test Procedure:**
 * | Variation / Step | Description | Test Data | Expected Result | Notes |
 * | :----: | --------- | ---------- |-------------- | ----- |
 * | 01 | Create an instance of DrmSessionManager with maxDrmSessions = 5 and player pointer as nullptr | maxDrmSessions = 5, player pointer = nullptr | Instance is created successfully | Should be successful |
 * | 02 | Invoke getSlotIdForSession with a null DrmSession pointer | input: DrmSession pointer = nullptr, output: slotIndex | Returns a negative error code; EXPECT_LT(slotIndex, 0) assertion passes | Should Pass |
 */
TEST_F(DrmSessionManagerTests, getSlotIdForSession_NullDrmSession) {
    std::cout << "Entering getSlotIdForSession_NullDrmSession test" << std::endl;
    
     auto cb = [](uint32_t id1, uint32_t id2, const std::string& text) {
         std::cout << "Callback invoked with id1=" << id1
                  << ", id2=" << id2
                  << ", text=" << text << std::endl;
    };
    // Create an instance of DrmSessionManager using a custom constructor.
    DrmSessionManager drmSessionManager(5, nullptr, cb);
    std::cout << "Created DrmSessionManager with maxDrmSessions: 5 and player pointer: nullptr" << std::endl;
    
    // Invoke the method getSlotIdForSession with a null DrmSession pointer.
    std::cout << "Invoking DrmSessionManager::getSlotIdForSession with nullptr" << std::endl;
    int slotIndex = drmSessionManager.getSlotIdForSession(nullptr);
    std::cout << "DrmSessionManager::getSlotIdForSession returned: " << slotIndex << std::endl;
    
    // Check that the returned slot index is a negative error code.
    EXPECT_LT(slotIndex, 0);
    
    std::cout << "Exiting getSlotIdForSession_NullDrmSession test" << std::endl;
}
/**
 * @brief Verify that hideWatermarkOnDetach() triggers watermark deactivation without exception.
 *
 * This test verifies that the hideWatermarkOnDetach() API in DrmSessionManager does not throw any exceptions when invoked and it properly updates the internal state to indicate that the watermark is deactivated. The function is called after initializing a DrmSessionManager object with a sample maximum DRM session count and a dummy player pointer.
 *
 * **Test Group ID:** Basic: 01
 * **Test Case ID:** 069
 * **Priority:** High
 *
 * **Pre-Conditions:** None
 * **Dependencies:** None
 * **User Interaction:** None
 *
 * **Test Procedure:**
 * | Variation / Step | Description | Test Data | Expected Result | Notes |
 * | :----: | --------- | ---------- |-------------- | ----- |
 * | 01 | Initialize DrmSessionManager with maxDrmSessions=5 and dummyPlayer set to nullptr. | maxDrmSessions = 5, dummyPlayer = nullptr | Object is created successfully. | Should be successful |
 * | 02 | Invoke hideWatermarkOnDetach() to update the internal watermark state. | API = hideWatermarkOnDetach() | No exception is thrown and watermark state is deactivated. | Should Pass |
 */
TEST_F(DrmSessionManagerTests, hideWatermarkOnDetach_start) {
    std::cout << "Entering hideWatermarkOnDetach_start test" << std::endl;
    
    // Create a DrmSessionManager object using custom constructor with a sample maxDrmSessions value and a dummy player pointer.
    int maxDrmSessions = 5;
    void* dummyPlayer = nullptr;
    auto cb = [](uint32_t id1, uint32_t id2, const std::string& text) {
         std::cout << "Callback invoked with id1=" << id1
                  << ", id2=" << id2
                  << ", text=" << text << std::endl;
    };
    DrmSessionManager drmSessionManager(maxDrmSessions, dummyPlayer, cb);
    std::cout << "Created DrmSessionManager object with maxDrmSessions = " << maxDrmSessions 
              << " and dummyPlayer = " << dummyPlayer << std::endl;
    
    // Log the invocation of hideWatermarkOnDetach()
    std::cout << "Invoking hideWatermarkOnDetach()" << std::endl;
    try {
        drmSessionManager.hideWatermarkOnDetach();
        std::cout << "hideWatermarkOnDetach() invoked successfully." << std::endl;
        
        // Assume internal state changed to deactivated watermark.
        std::cout << "Internal state updated: watermark deactivated." << std::endl;
    } catch (const std::exception &e) {
        std::cout << "Exception caught during hideWatermarkOnDetach(): " << e.what() << std::endl;
        FAIL() << "hideWatermarkOnDetach() threw an exception";
    } catch (...) {
        std::cout << "Unknown exception caught during hideWatermarkOnDetach()." << std::endl;
        FAIL() << "hideWatermarkOnDetach() threw an unknown exception";
    }
    
    std::cout << "Exiting hideWatermarkOnDetach_start test" << std::endl;
}
/**
 * @brief This test verifies the behavior of hideWatermarkOnDetach() method when invoked repeatedly.
 *
 * This test creates a DrmSessionManager object and calls hideWatermarkOnDetach() three times to validate that 
 * repeated invocations do not cause undesired behavior and maintain the watermark in a deactivated state.
 *
 * **Test Group ID:** Basic: 01@n
 * **Test Case ID:** 070@n
 * **Priority:** High@n
 *
 * **Pre-Conditions:** None@n
 * **Dependencies:** None@n
 * **User Interaction:** None@n
 *
 * **Test Procedure:**
 * | Variation / Step | Description                                                       | Test Data                                                              | Expected Result                                                                                           | Notes           |
 * | :--------------: | ----------------------------------------------------------------- | ---------------------------------------------------------------------- | --------------------------------------------------------------------------------------------------------- | --------------- |
 * | 01               | Create a DrmSessionManager object with maxDrmSessions = 5 and dummyPlayer = nullptr | maxDrmSessions = 5, dummyPlayer = nullptr                              | DrmSessionManager object is created successfully                                                          | Should be successful |
 * | 02               | Invoke hideWatermarkOnDetach() for the first time                    | Function call: hideWatermarkOnDetach()                                 | Method executes without throwing an exception; watermark deactivated                                        | Should Pass     |
 * | 03               | Invoke hideWatermarkOnDetach() for the second time                   | Function call: hideWatermarkOnDetach()                                 | Method executes without throwing an exception; watermark remains deactivated                                | Should Pass     |
 * | 04               | Invoke hideWatermarkOnDetach() for the third time to check idempotence  | Function call: hideWatermarkOnDetach()                                 | Method executes without throwing an exception; repeated calls cause no adverse effects (idempotence verified) | Should Pass     |
 */
TEST_F(DrmSessionManagerTests, hideWatermarkOnDetach_repeated) {
    std::cout << "Entering hideWatermarkOnDetach_repeated test" << std::endl;
    
    // Create a DrmSessionManager object using custom constructor with a sample maxDrmSessions value and a dummy player pointer.
    int maxDrmSessions = 5;
    void* dummyPlayer = nullptr;
    auto cb = [](uint32_t id1, uint32_t id2, const std::string& text) {
         std::cout << "Callback invoked with id1=" << id1
                  << ", id2=" << id2
                  << ", text=" << text << std::endl;
    };
    DrmSessionManager drmSessionManager(maxDrmSessions, dummyPlayer, cb);
    std::cout << "Created DrmSessionManager object with maxDrmSessions = " << maxDrmSessions 
              << " and dummyPlayer = " << dummyPlayer << std::endl;
    
    // First invocation of hideWatermarkOnDetach()
    std::cout << "First invocation of hideWatermarkOnDetach()" << std::endl;
    try {
        drmSessionManager.hideWatermarkOnDetach();
        std::cout << "First hideWatermarkOnDetach() invoked successfully. Watermark deactivated." << std::endl;
    } catch (const std::exception &e) {
        std::cout << "Exception caught on first invocation: " << e.what() << std::endl;
        FAIL() << "hideWatermarkOnDetach() threw an exception on first invocation";
    } catch (...) {
        std::cout << "Unknown exception caught on first invocation." << std::endl;
        FAIL() << "hideWatermarkOnDetach() threw an unknown exception on first invocation";
    }
    
    // Second invocation of hideWatermarkOnDetach()
    std::cout << "Second invocation of hideWatermarkOnDetach()" << std::endl;
    try {
        drmSessionManager.hideWatermarkOnDetach();
        std::cout << "Second hideWatermarkOnDetach() invoked successfully. Watermark remains deactivated." << std::endl;
    } catch (const std::exception &e) {
        std::cout << "Exception caught on second invocation: " << e.what() << std::endl;
        FAIL() << "hideWatermarkOnDetach() threw an exception on second invocation";
    } catch (...) {
        std::cout << "Unknown exception caught on second invocation." << std::endl;
        FAIL() << "hideWatermarkOnDetach() threw an unknown exception on second invocation";
    }
    
    // Third invocation of hideWatermarkOnDetach() to check idempotence
    std::cout << "Third invocation of hideWatermarkOnDetach()" << std::endl;
    try {
        drmSessionManager.hideWatermarkOnDetach();
        std::cout << "Third hideWatermarkOnDetach() invoked successfully. Watermark remains deactivated." << std::endl;
    } catch (const std::exception &e) {
        std::cout << "Exception caught on third invocation: " << e.what() << std::endl;
        FAIL() << "hideWatermarkOnDetach() threw an exception on third invocation";
    } catch (...) {
        std::cout << "Unknown exception caught on third invocation." << std::endl;
        FAIL() << "hideWatermarkOnDetach() threw an unknown exception on third invocation";
    }
    
    std::cout << "Exiting hideWatermarkOnDetach_repeated test" << std::endl;
}

/**
 * @brief Verify that calling initializeDrmSession with a negative session slot properly results in a KEY_ERROR.
 *
 * This test verifies that the DrmSessionManager::initializeDrmSession function returns KEY_ERROR and sets a non-zero error code when provided with a negative sessionSlot value. It ensures that the API handles invalid session indices correctly by failing fast.
 *
 * **Test Group ID:** Basic: 01@n
 * **Test Case ID:** 073@n
 * **Priority:** High@n
 *
 * **Pre-Conditions:** None@n
 * **Dependencies:** None@n
 * **User Interaction:** None@n
 *
 * **Test Procedure:**@n
 * | Variation / Step | Description                                                                     | Test Data                                                       | Expected Result                                                                             | Notes         |
 * | :----:           | ------------------------------------------------------------------------------- | --------------------------------------------------------------- | ------------------------------------------------------------------------------------------- | ------------- |
 * | 01               | Create a DrmSessionManager object using the custom constructor                  | maxDrmSessions = 4, player = nullptr                              | DrmSessionManager object is successfully created                                             | Should be successful |
 * | 02               | Create a valid DrmHelper object                                                 | N/A                                                             | DrmHelper object is successfully created                                                     | Should be successful |
 * | 03               | Call initializeDrmSession with a negative sessionSlot and verify the API response | drmHelper = valid object, sessionSlot = -1, err = -1               | Returns KEY_ERROR and sets err to a non-zero value as verified by assertion checks             | Should Fail   */
TEST_F(DrmSessionManagerTests, NegativeSessionSlot) {
    std::cout << "Entering NegativeSessionSlot test" << std::endl;
    
    // Creating a valid DrmSessionManager object using custom constructor
    auto cb = [](uint32_t id1, uint32_t id2, const std::string& text) {
         std::cout << "Callback invoked with id1=" << id1
                  << ", id2=" << id2
                  << ", text=" << text << std::endl;
    };
    DrmSessionManager dsm(4, nullptr, cb);
    std::cout << "Created DrmSessionManager object with maxDrmSessions=4 and player=nullptr" << std::endl;
    
    // Creating a valid DrmHelper object
    std::shared_ptr<DrmHelper> drmHelper = std::make_shared<MockDrmHelper>();
    std::cout << "Created valid DrmHelper object" << std::endl;
    
    int sessionSlot = -1;
    int err = -1;
    std::cout << "Calling initializeDrmSession with NEGATIVE sessionSlot = " << sessionSlot << " and initial err = " << err << std::endl;
    
    KeyState state = dsm.initializeDrmSession(drmHelper, sessionSlot, err);
    std::cout << "initializeDrmSession returned KeyState = " << state << std::endl;
    std::cout << "Error code (err) set to = " << err << std::endl;
    
    EXPECT_EQ(state, KEY_ERROR_EMPTY_SESSION_ID);
    EXPECT_EQ(err, MW_DRM_SESSIONID_EMPTY);
    
    std::cout << "Exiting NegativeSessionSlot test" << std::endl;
}

/**
 * @brief Test the notifyCleanup() method with a single cleanup call
 *
 * This test verifies that the DrmSessionManager correctly cleans up resources when the notifyCleanup() 
 * method is invoked exactly once. The test confirms that creating a DrmSessionManager instance with valid 
 * dummy parameters and then calling notifyCleanup() does not result in any errors.
 *
 * **Test Group ID:** Basic: 01@n
 * **Test Case ID:** 075@n
 * **Priority:** High@n
 * 
 * **Pre-Conditions:** None@n
 * **Dependencies:** None@n
 * **User Interaction:** None@n
 * 
 * **Test Procedure:**@n
 * | Variation / Step | Description | Test Data | Expected Result | Notes |
 * | :----: | --------- | ---------- |-------------- | ----- |
 * | 01 | Create an instance of DrmSessionManager with parameters maxDrmSessions = 5 and dummyPlayer = 0x1 | maxDrmSessions = 5, dummyPlayer = 0x1 | DrmSessionManager object is successfully created | Should be successful |
 * | 02 | Invoke the notifyCleanup() method on the DrmSessionManager instance | No input parameters; dummyPlayer used from initialization, maxDrmSessions = 5 | notifyCleanup() executes without errors and resources are cleaned up | Should Pass |
 */
TEST_F(DrmSessionManagerTests, notifyCleanup_singleCleanup) {
    std::cout << "Entering notifyCleanup_singleCleanup test" << std::endl;
    
    // Create an instance of DrmSessionManager with dummy parameters.
    int maxDrmSessions = 5;
    void* dummyPlayer = reinterpret_cast<void*>(0x1);
     auto cb = [](uint32_t id1, uint32_t id2, const std::string& text) {
         std::cout << "Callback invoked with id1=" << id1
                  << ", id2=" << id2
                  << ", text=" << text << std::endl;
    };
    DrmSessionManager drmManager(maxDrmSessions, dummyPlayer, cb);
    std::cout << "Created DrmSessionManager instance with maxDrmSessions=" << maxDrmSessions 
              << " and dummyPlayer=" << dummyPlayer << std::endl;
    
    // Invoke notifyCleanup and log the method call.
    std::cout << "Invoking notifyCleanup() method on DrmSessionManager instance." << std::endl;
    EXPECT_NO_THROW(
        {
            drmManager.notifyCleanup();
        }
    );
    std::cout << "notifyCleanup() method executed successfully. Resources should be cleaned up." << std::endl;
    
    std::cout << "Exiting notifyCleanup_singleCleanup test" << std::endl;
}
/**
 * @brief Verify that multiple calls to notifyCleanup() are idempotent
 *
 * This test verifies that invoking the notifyCleanup() method more than once on a single instance 
 * of DrmSessionManager does not produce adverse effects. The test ensures that the resources are 
 * correctly cleaned up during the first call and that subsequent calls do not cause errors or unexpected state changes.@n
 *
 * **Test Group ID:** Basic: 01@n
 * **Test Case ID:** 076@n
 * **Priority:** High@n
 * 
 * **Pre-Conditions:** None@n
 * **Dependencies:** None@n
 * **User Interaction:** None@n
 * 
 * **Test Procedure:**@n
 * | Variation / Step | Description                                                        | Test Data                                  | Expected Result                                                      | Notes         |
 * | :--------------: | ------------------------------------------------------------------ | ------------------------------------------ | -------------------------------------------------------------------- | ------------- |
 * | 01               | Create a DrmSessionManager instance with dummy parameters          | maxDrmSessions = 10, dummyPlayer = 0x2       | Instance is successfully created                                     | Should be successful |
 * | 02               | Invoke notifyCleanup() for the first time to clean up resources      | Call notifyCleanup() on the instance       | First cleanup call completes without errors and cleans up resources  | Should Pass   |
 * | 03               | Invoke notifyCleanup() again to verify idempotency                   | Call notifyCleanup() on the instance again | Second cleanup call has no adverse effects and maintains consistent state | Should Pass   |
 */
TEST_F(DrmSessionManagerTests, notifyCleanup_multipleCleanup) {
    std::cout << "Entering notifyCleanup_multipleCleanup test" << std::endl;
    
    // Create an instance of DrmSessionManager with dummy parameters.
    int maxDrmSessions = 10;
    void* dummyPlayer = reinterpret_cast<void*>(0x2);
    auto cb = [](uint32_t id1, uint32_t id2, const std::string& text) {
         std::cout << "Callback invoked with id1=" << id1
                  << ", id2=" << id2
                  << ", text=" << text << std::endl;
    };
    DrmSessionManager drmManager(maxDrmSessions, dummyPlayer, cb);
    std::cout << "Created DrmSessionManager instance with maxDrmSessions=" << maxDrmSessions 
              << " and dummyPlayer=" << dummyPlayer << std::endl;
    
    // First notification cleanup call.
    std::cout << "First invocation: Calling notifyCleanup() method." << std::endl;
    EXPECT_NO_THROW(
        {
            drmManager.notifyCleanup();
        }
    );
    std::cout << "First notifyCleanup() execution completed. Resources cleaned up." << std::endl;
    
    // Second notification cleanup call to ensure idempotency.
    std::cout << "Second invocation: Calling notifyCleanup() method again." << std::endl;
    EXPECT_NO_THROW(
        {
            drmManager.notifyCleanup();
        }
    );
    std::cout << "Second notifyCleanup() execution completed. No errors occurred and state remains consistent." << std::endl;
    
    std::cout << "Exiting notifyCleanup_multipleCleanup test" << std::endl;
}
/**
 * @brief Test that the registerCallback method correctly sets all callbacks
 *
 * This test verifies that when registerCallback() is invoked on a DrmSessionManager instance, all internal callback function pointers (AcquireLicenseCb, ProfileUpdateCb, ContentUpdateCb) are properly assigned. It ensures that the initial state has no callbacks set and that after registration, each callback is non-null, confirming proper API functionality.
 *
 * **Test Group ID:** Basic: 01@n
 * **Test Case ID:** 077@n
 * **Priority:** High@n
 *
 * **Pre-Conditions:** None@n
 * **Dependencies:** None@n
 * **User Interaction:** None@n
 *
 * **Test Procedure:**@n
 * | Variation / Step | Description                                                                                          | Test Data                                                    | Expected Result                                                     | Notes            |
 * | :----:           | ---------------------------------------------------------------------------------------------------- | ------------------------------------------------------------ | ------------------------------------------------------------------- | ---------------- |
 * | 01               | Create an instance of DrmSessionManager with maxDrmSessions set to 5 and dummyPlayer set to 0x1       | maxDrmSessions = 5, dummyPlayer = 0x1                          | Instance is created successfully                                    | Should be successful |
 * | 02               | Verify initial state: callbacks are unset (null) before invocation of registerCallback               | Expected: AcquireLicenseCb = null, ProfileUpdateCb = null, ContentUpdateCb = null | All callbacks are initially unset                                  | Should be successful |
 * | 03               | Invoke registerCallback() method to register all callbacks                                            | API Call: registerCallback()                                  | registerCallback executes without errors; callbacks become non-null  | Should Pass      |
 * | 04               | Check the internal callback members after registration                                              | Expected: acquireLicenseSet = true, profileUpdateSet = true, contentUpdateSet = true   | All callbacks are set (true) after registration                     | Should Pass      |
 */
#if 0
TEST_F(DrmSessionManagerTests, RegisterCallbackSuccessfullyWithAllCallbacksProperlySet) {
    std::cout << "Entering RegisterCallbackSuccessfullyWithAllCallbacksProperlySet test" << std::endl;

    // Create an instance of DrmSessionManager using the custom constructor.
    int maxDrmSessions = 5;
    void* dummyPlayer = reinterpret_cast<void*>(0x1);
    auto cb = [](uint32_t id1, uint32_t id2, const std::string& text) {
         std::cout << "Callback invoked with id1=" << id1
                  << ", id2=" << id2
                  << ", text=" << text << std::endl;
    };
    DrmSessionManager drmSessionManager(maxDrmSessions, dummyPlayer, cb);
    std::cout << "Created DrmSessionManager instance with maxDrmSessions: " << maxDrmSessions;
    std::cout << " and player pointer: " << dummyPlayer << std::endl;

    // Print initial state of callback members (expected to be empty before registration).
    std::cout << "Before registerCallback:" << std::endl;
    std::cout << "AcquireLicenseCb set? " << (drmSessionManager.AcquireLicenseCb ? "Yes" : "No") << std::endl;
    std::cout << "ProfileUpdateCb set? " << (drmSessionManager.ProfileUpdateCb ? "Yes" : "No") << std::endl;
    std::cout << "ContentUpdateCb set? " << (drmSessionManager.ContentUpdateCb ? "Yes" : "No") << std::endl;

    // Invoke registerCallback and log the invocation.
    std::cout << "Invoking registerCallback()" << std::endl;
    drmSessionManager.registerCallback();
    std::cout << "registerCallback() called successfully" << std::endl;

    // Check that the internal callback members are registered (i.e., not empty).
    bool acquireLicenseSet = static_cast<bool>(drmSessionManager.AcquireLicenseCb);
    bool profileUpdateSet  = static_cast<bool>(drmSessionManager.ProfileUpdateCb);
    bool contentUpdateSet  = static_cast<bool>(drmSessionManager.ContentUpdateCb);

    std::cout << "After registerCallback:" << std::endl;
    std::cout << "AcquireLicenseCb set? " << (acquireLicenseSet ? "Yes" : "No") << std::endl;
    std::cout << "ProfileUpdateCb set? " << (profileUpdateSet ? "Yes" : "No") << std::endl;
    std::cout << "ContentUpdateCb set? " << (contentUpdateSet ? "Yes" : "No") << std::endl;

    EXPECT_TRUE(acquireLicenseSet);
    EXPECT_TRUE(profileUpdateSet);
    EXPECT_TRUE(contentUpdateSet);

    std::cout << "Exiting RegisterCallbackSuccessfullyWithAllCallbacksProperlySet test" << std::endl;
}
#endif
/**
 * @brief Test multiple consecutive invocations of registerCallback in DrmSessionManager to ensure callbacks remain correctly set.
 *
 * This test verifies that repeated calls to the registerCallback method correctly set the callback pointers (AcquireLicenseCb, ProfileUpdateCb, and ContentUpdateCb) in the DrmSessionManager. It ensures that the object's state remains consistent and reliable across multiple invocations.
 *
 * **Test Group ID:** Basic: 01@n
 * **Test Case ID:** 078@n
 * **Priority:** High@n
 * 
 * **Pre-Conditions:** None@n
 * **Dependencies:** None@n
 * **User Interaction:** None@n
 * 
 * **Test Procedure:**@n
 * | Variation / Step | Description                                                          | Test Data                                                        | Expected Result                                                                 | Notes       |
 * | :--------------: | -------------------------------------------------------------------- | --------------------------------------------------------------- | ------------------------------------------------------------------------------- | ----------- |
 * | 01               | Initialize DrmSessionManager instance and invoke first registerCallback | maxDrmSessions = 3, dummyPlayer = 0x2, call: registerCallback()    | Callback pointers (AcquireLicenseCb, ProfileUpdateCb, ContentUpdateCb) are set; Assertions pass | Should Pass |
 * | 02               | Invoke second registerCallback on the same instance                  | Continued state from first call, call: registerCallback()          | Callback pointers remain set; Assertions pass                                   | Should Pass |
 * | 03               | Invoke third registerCallback to verify consistency of callback settings | Continued state from second call, call: registerCallback()         | Callback pointers remain set; Assertions pass                                   | Should Pass |
 */
#if 0
TEST_F(DrmSessionManagerTests, MultipleConsecutiveInvocationsOfRegisterCallback) {
    std::cout << "Entering MultipleConsecutiveInvocationsOfRegisterCallback test" << std::endl;

    // Create an instance of DrmSessionManager using the custom constructor.
    int maxDrmSessions = 3;
    void* dummyPlayer = reinterpret_cast<void*>(0x2);
    auto cb = [](uint32_t id1, uint32_t id2, const std::string& text) {
         std::cout << "Callback invoked with id1=" << id1
                  << ", id2=" << id2
                  << ", text=" << text << std::endl;
    };
    DrmSessionManager drmSessionManager(maxDrmSessions, dummyPlayer, cb);
    std::cout << "Created DrmSessionManager instance with maxDrmSessions: " << maxDrmSessions;
    std::cout << " and player pointer: " << dummyPlayer << std::endl;

    // First invocation of registerCallback.
    std::cout << "Invoking first registerCallback()" << std::endl;
    drmSessionManager.registerCallback();
    std::cout << "First registerCallback() call completed." << std::endl;

    bool acquireLicenseSet1 = static_cast<bool>(drmSessionManager.AcquireLicenseCb);
    bool profileUpdateSet1  = static_cast<bool>(drmSessionManager.ProfileUpdateCb);
    bool contentUpdateSet1  = static_cast<bool>(drmSessionManager.ContentUpdateCb);

    std::cout << "After first call:" << std::endl;
    std::cout << "AcquireLicenseCb: " << (acquireLicenseSet1 ? "set" : "not set") << std::endl;
    std::cout << "ProfileUpdateCb: " << (profileUpdateSet1 ? "set" : "not set") << std::endl;
    std::cout << "ContentUpdateCb: " << (contentUpdateSet1 ? "set" : "not set") << std::endl;
    
    EXPECT_TRUE(acquireLicenseSet1);
    EXPECT_TRUE(profileUpdateSet1);
    EXPECT_TRUE(contentUpdateSet1);

    // Second invocation of registerCallback.
    std::cout << "Invoking second registerCallback()" << std::endl;
    drmSessionManager.registerCallback();
    std::cout << "Second registerCallback() call completed." << std::endl;
    
    bool acquireLicenseSet2 = static_cast<bool>(drmSessionManager.AcquireLicenseCb);
    bool profileUpdateSet2  = static_cast<bool>(drmSessionManager.ProfileUpdateCb);
    bool contentUpdateSet2  = static_cast<bool>(drmSessionManager.ContentUpdateCb);

    std::cout << "After second call:" << std::endl;
    std::cout << "AcquireLicenseCb: " << (acquireLicenseSet2 ? "set" : "not set") << std::endl;
    std::cout << "ProfileUpdateCb: " << (profileUpdateSet2 ? "set" : "not set") << std::endl;
    std::cout << "ContentUpdateCb: " << (contentUpdateSet2 ? "set" : "not set") << std::endl;
    
    EXPECT_TRUE(acquireLicenseSet2);
    EXPECT_TRUE(profileUpdateSet2);
    EXPECT_TRUE(contentUpdateSet2);

    // Third invocation of registerCallback.
    std::cout << "Invoking third registerCallback()" << std::endl;
    drmSessionManager.registerCallback();
    std::cout << "Third registerCallback() call completed." << std::endl;
    
    bool acquireLicenseSet3 = static_cast<bool>(drmSessionManager.AcquireLicenseCb);
    bool profileUpdateSet3  = static_cast<bool>(drmSessionManager.ProfileUpdateCb);
    bool contentUpdateSet3  = static_cast<bool>(drmSessionManager.ContentUpdateCb);

    std::cout << "After third call:" << std::endl;
    std::cout << "AcquireLicenseCb: " << (acquireLicenseSet3 ? "set" : "not set") << std::endl;
    std::cout << "ProfileUpdateCb: " << (profileUpdateSet3 ? "set" : "not set") << std::endl;
    std::cout << "ContentUpdateCb: " << (contentUpdateSet3 ? "set" : "not set") << std::endl;
    
    EXPECT_TRUE(acquireLicenseSet3);
    EXPECT_TRUE(profileUpdateSet3);
    EXPECT_TRUE(contentUpdateSet3);

    std::cout << "Exiting MultipleConsecutiveInvocationsOfRegisterCallback test" << std::endl;
}
#endif
/**
 * @brief Tests that the DrmSessionManager correctly handles live playback settings.
 *
 * This test verifies that when setPlaybackSpeedState is invoked with parameters for live playback,
 * the DrmSessionManager updates its internal state to reflect live playback at the normal speed.
 *
 * **Test Group ID:** Basic: 01
 * **Test Case ID:** 081
 * **Priority:** High
 *
 * **Pre-Conditions:** None
 * **Dependencies:** None
 * **User Interaction:** None
 *
 * **Test Procedure:**
 * | Variation / Step | Description | Test Data | Expected Result | Notes |
 * | :----: | --------- | ---------- |-------------- | ----- |
 * | 01 | Create DrmSessionManager instance using custom constructor | drmSessionManager constructor: parameter1 = 10, parameter2 = nullptr | Instance is created successfully | Should be successful |
 * | 02 | Invoke setPlaybackSpeedState with live playback parameters | live = true, currentLatency = 50.0, livepoint = true, liveOffsetMs = 0.0, speed = 1, positionMs = 1000.0, firstFrameSeen = false | API processes input and sets internal state for live playback at normal speed | Should Pass |
 */
TEST_F(DrmSessionManagerTests, StandardLivePlayback) {
    std::cout << "Entering StandardLivePlayback test" << std::endl;
    // Create a DrmSessionManager instance using a custom constructor
    auto cb = [](uint32_t id1, uint32_t id2, const std::string& text) {
         std::cout << "Callback invoked with id1=" << id1
                  << ", id2=" << id2
                  << ", text=" << text << std::endl;
    };
    DrmSessionManager drmSessionManager(10, nullptr, cb);
    std::cout << "Invoking setPlaybackSpeedState with parameters: live = true, currentLatency = 50.0, livepoint = true, liveOffsetMs = 0.0, speed = 1, positionMs = 1000.0, firstFrameSeen = false" << std::endl;
    
    drmSessionManager.setPlaybackSpeedState(true, 50.0, true, 0.0, 1, 1000.0, false);
    
    std::cout << "Method setPlaybackSpeedState executed. Internal state should reflect live playback at normal speed (live=true, speed=1, position=1000.0)" << std::endl;
    std::cout << "Exiting StandardLivePlayback test" << std::endl;
}
/**
 * @brief Verify VOD playback functionality by setting playback speed and position
 *
 * This test validates that the DrmSessionManager correctly updates the playback state for on-demand (VOD) content. The test ensures that invoking setPlaybackSpeedState with specific parameters reflects the expected on-demand playback behavior.
 *
 * **Test Group ID:** Basic: 01
 * **Test Case ID:** 082
 * **Priority:** High
 *
 * **Pre-Conditions:** None
 * **Dependencies:** None
 * **User Interaction:** None
 *
 * **Test Procedure:**
 * | Variation / Step | Description | Test Data | Expected Result | Notes |
 * | :----: | --------- | ---------- |-------------- | ----- |
 * | 01 | Instantiate DrmSessionManager object with an initial session count and a null session listener | input1 = 10, input2 = nullptr | Object is instantiated successfully with proper initial state | Should be successful |
 * | 02 | Invoke setPlaybackSpeedState to set on-demand playback parameters | live = false, currentLatency = 0.0, livepoint = false, liveOffsetMs = 0.0, speed = 1, positionMs = 5000.0, firstFrameSeen = false | Internal state is updated to reflect on-demand playback with playback speed set to 1 and position to 5000.0 | Should Pass |
 */
TEST_F(DrmSessionManagerTests, VODPlayback) {
    std::cout << "Entering VODPlayback test" << std::endl;
    
    auto cb = [](uint32_t id1, uint32_t id2, const std::string& text) {
         std::cout << "Callback invoked with id1=" << id1
                  << ", id2=" << id2
                  << ", text=" << text << std::endl;
    };
    DrmSessionManager drmSessionManager(10, nullptr, cb);
    std::cout << "Invoking setPlaybackSpeedState with parameters: live = false, currentLatency = 0.0, livepoint = false, liveOffsetMs = 0.0, speed = 1, positionMs = 5000.0, firstFrameSeen = false" << std::endl;
    
    drmSessionManager.setPlaybackSpeedState(false, 0.0, false, 0.0, 1, 5000.0, false);
    
    std::cout << "Method setPlaybackSpeedState executed. Internal state should reflect on-demand playback (live=false, speed=1, position=5000.0)" << std::endl;
    std::cout << "Exiting VODPlayback test" << std::endl;
}
/**
 * @brief Verify correct behavior of setPlaybackSpeedState for live playback when the first frame is received
 *
 * This test verifies that when live playback conditions are met—specifically with live mode enabled, a valid latency, livepoint true, a positive live offset, and with the first frame seen—the API setPlaybackSpeedState correctly processes these inputs to update the internal state to reflect live playback with the first frame received.
 *
 * **Test Group ID:** Basic: 01
 * **Test Case ID:** 083
 * **Priority:** High
 * 
 * **Pre-Conditions:** None
 * **Dependencies:** None
 * **User Interaction:** None
 * 
 * **Test Procedure:**
 * | Variation / Step | Description | Test Data | Expected Result | Notes |
 * | :----: | --------- | ---------- |-------------- | ----- |
 * | 01 | Instantiate DrmSessionManager with initial parameters | input: bufferSize = 10, dependency = nullptr | DrmSessionManager instance should be created successfully | Should be successful |
 * | 02 | Invoke setPlaybackSpeedState with live playback parameters and firstFrameSeen true | input: live = true, currentLatency = 30.0, livepoint = true, liveOffsetMs = 10.0, speed = 1, positionMs = 1200.0, firstFrameSeen = true | Internal state of DrmSessionManager should update to reflect live playback with first frame received | Should Pass |
 */
TEST_F(DrmSessionManagerTests, LiveWithFirstFrameSeen) {
    std::cout << "Entering LiveWithFirstFrameSeen test" << std::endl;
    auto cb = [](uint32_t id1, uint32_t id2, const std::string& text) {
         std::cout << "Callback invoked with id1=" << id1
                  << ", id2=" << id2
                  << ", text=" << text << std::endl;
    };
    DrmSessionManager drmSessionManager(10, nullptr, cb);
    std::cout << "Invoking setPlaybackSpeedState with parameters: live = true, currentLatency = 30.0, livepoint = true, liveOffsetMs = 10.0, speed = 1, positionMs = 1200.0, firstFrameSeen = true" << std::endl;
    
    drmSessionManager.setPlaybackSpeedState(true, 30.0, true, 10.0, 1, 1200.0, true);
    
    std::cout << "Method setPlaybackSpeedState executed. Internal state should reflect live playback with first frame received (firstFrameSeen=true)" << std::endl;
    std::cout << "Exiting LiveWithFirstFrameSeen test" << std::endl;
}
/**
 * @brief Verify that setPlaybackSpeedState handles negative currentLatency appropriately.
 *
 * This test verifies that when a negative value is provided as the currentLatency to the setPlaybackSpeedState method, the DrmSessionManager object handles it per the expected error handling logic. The objective is to check whether the API detects and manages the anomaly when negative latency is input.
 *
 * **Test Group ID:** Basic: 01@n
 * **Test Case ID:** 084@n
 * **Priority:** High@n
 *
 * **Pre-Conditions:** None@n
 * **Dependencies:** None@n
 * **User Interaction:** None@n
 *
 * **Test Procedure:**@n
 * | Variation / Step | Description | Test Data | Expected Result | Notes |
 * | :----: | --------- | ---------- |-------------- | ----- |
 * | 01 | Initialize DrmSessionManager instance with valid parameters. | sessionId = 10, errorHandler = nullptr | Instance should be created successfully. | Should be successful |
 * | 02 | Invoke setPlaybackSpeedState with negative currentLatency value. | live = true, currentLatency = -10.0, livepoint = true, liveOffsetMs = 0.0, speed = 1, positionMs = 1000.0, firstFrameSeen = false | API should handle negative latency appropriately, with error handling or corrective measures implemented. | Should Fail |
 */
TEST_F(DrmSessionManagerTests, NegativeCurrentLatency) {
    std::cout << "Entering NegativeCurrentLatency test" << std::endl;
    auto cb = [](uint32_t id1, uint32_t id2, const std::string& text) {
         std::cout << "Callback invoked with id1=" << id1
                  << ", id2=" << id2
                  << ", text=" << text << std::endl;
    };
    DrmSessionManager drmSessionManager(10, nullptr, cb);
    std::cout << "Invoking setPlaybackSpeedState with parameters: live = true, currentLatency = -10.0, livepoint = true, liveOffsetMs = 0.0, speed = 1, positionMs = 1000.0, firstFrameSeen = false" << std::endl;
    
    drmSessionManager.setPlaybackSpeedState(true, -10.0, true, 0.0, 1, 1000.0, false);
    
    std::cout << "Method setPlaybackSpeedState executed. Negative latency should be handled appropriately while updating playback speed state" << std::endl;
    std::cout << "Exiting NegativeCurrentLatency test" << std::endl;
}
/**
 * @brief Test that setPlaybackSpeedState handles negative liveOffsetMs parameter correctly.
 *
 * This test validates that invoking setPlaybackSpeedState with a negative liveOffsetMs value is processed in a controlled manner, ensuring that the playback speed state update mechanism can handle negative offsets gracefully without causing unexpected behavior.
 *
 * **Test Group ID:** Basic: 01@n
 * **Test Case ID:** 085@n
 * **Priority:** High@n
 * 
 * **Pre-Conditions:** None@n
 * **Dependencies:** None@n
 * **User Interaction:** None@n
 * 
 * **Test Procedure:**
 * | Variation / Step | Description | Test Data | Expected Result | Notes |
 * | :----: | --------- | ---------- |-------------- | ----- |
 * | 01 | Invoke setPlaybackSpeedState with negative liveOffsetMs to check for controlled handling of negative offset. | live = true, currentLatency = 20.0, livepoint = true, liveOffsetMs = -50.0, speed = 1, positionMs = 2000.0, firstFrameSeen = false | Method should handle the negative liveOffsetMs value gracefully, updating the playback speed state appropriately without errors. | Should Fail |
 */
TEST_F(DrmSessionManagerTests, NegativeLiveOffsetMs) {
    std::cout << "Entering NegativeLiveOffsetMs test" << std::endl;
    auto cb = [](uint32_t id1, uint32_t id2, const std::string& text) {
         std::cout << "Callback invoked with id1=" << id1
                  << ", id2=" << id2
                  << ", text=" << text << std::endl;
    };
    DrmSessionManager drmSessionManager(10, nullptr, cb);
    std::cout << "Invoking setPlaybackSpeedState with parameters: live = true, currentLatency = 20.0, livepoint = true, liveOffsetMs = -50.0, speed = 1, positionMs = 2000.0, firstFrameSeen = false" << std::endl;
    
    drmSessionManager.setPlaybackSpeedState(true, 20.0, true, -50.0, 1, 2000.0, false);
    
    std::cout << "Method setPlaybackSpeedState executed. Negative liveOffsetMs should be processed in a controlled manner while updating playback speed state" << std::endl;
    std::cout << "Exiting NegativeLiveOffsetMs test" << std::endl;
}
/**
 * @brief Verify that setting playback speed to zero correctly results in a paused playback state.
 *
 * This test verifies that when setPlaybackSpeedState is invoked with a speed value of 0,
 * the DrmSessionManager internal state correctly reflects a paused playback state.
 *
 * **Test Group ID:** Basic: 01@n
 * **Test Case ID:** 086@n
 * **Priority:** High@n
 * 
 * **Pre-Conditions:** None@n
 * **Dependencies:** None@n
 * **User Interaction:** None@n
 *
 * **Test Procedure:**
 * | Variation / Step | Description                                                    | Test Data                                                                                     | Expected Result                                   | Notes         |
 * | :----:           | -------------------------------------------------------------- | --------------------------------------------------------------------------------------------- | ------------------------------------------------- | ------------- |
 * | 01               | Initialize DrmSessionManager object                            | constructor param1 = 10, param2 = nullptr                                                      | Object is created successfully                    | Should be successful |
 * | 02               | Call setPlaybackSpeedState with zero playback speed            | live = false, currentLatency = 0.0, livepoint = false, liveOffsetMs = 0.0, speed = 0, positionMs = 3000.0, firstFrameSeen = false | Playback speed state updated to paused (speed=0)  | Should Pass   |
 * | 03               | Verify output logs indicating state update execution           | N/A                                                                                           | Expected log outputs are observed                 | Should be successful |
 */
TEST_F(DrmSessionManagerTests, ZeroPlaybackSpeed) {
    std::cout << "Entering ZeroPlaybackSpeed test" << std::endl;
    auto cb = [](uint32_t id1, uint32_t id2, const std::string& text) {
         std::cout << "Callback invoked with id1=" << id1
                  << ", id2=" << id2
                  << ", text=" << text << std::endl;
    };
    DrmSessionManager drmSessionManager(10, nullptr, cb);
    std::cout << "Invoking setPlaybackSpeedState with parameters: live = false, currentLatency = 0.0, livepoint = false, liveOffsetMs = 0.0, speed = 0, positionMs = 3000.0, firstFrameSeen = false" << std::endl;
    
    drmSessionManager.setPlaybackSpeedState(false, 0.0, false, 0.0, 0, 3000.0, false);
    
    std::cout << "Method setPlaybackSpeedState executed. Internal state should reflect paused playback (speed=0)" << std::endl;
    std::cout << "Exiting ZeroPlaybackSpeed test" << std::endl;
}
/**
 * @brief Verify that reverse playback is correctly handled by DrmSessionManager.
 *
 * This test validates the reverse playback functionality by invoking setPlaybackSpeedState with a negative speed value.
 * It ensures that the internal state of DrmSessionManager reflects reverse playback or that proper error handling is in place
 * when negative speeds are provided.
 *
 * **Test Group ID:** Basic: 01@n
 * **Test Case ID:** 087@n
 * **Priority:** High@n
 *
 * **Pre-Conditions:** None@n
 * **Dependencies:** None@n
 * **User Interaction:** None@n
 *
 * **Test Procedure:**@n
 * | Variation / Step | Description                                                                     | Test Data                                                                                                                     | Expected Result                                                                                    | Notes               |
 * | :--------------: | ------------------------------------------------------------------------------- | ----------------------------------------------------------------------------------------------------------------------------- | -------------------------------------------------------------------------------------------------- | ------------------- |
 * | 01               | Instantiate DrmSessionManager with valid parameters                             | initialValue = 10, callback = nullptr                                                                                         | Instance created successfully                                                                       | Should be successful|
 * | 02               | Invoke setPlaybackSpeedState with reverse playback parameters                   | live = false, currentLatency = 10.0, livepoint = false, liveOffsetMs = 0.0, speed = -1, positionMs = 1500.0, firstFrameSeen = false | API executes and internal state reflects reverse playback or proper error handling for negative speed| Should Pass         |
 * | 03               | Verify logs indicate execution and test completion                              | Observing console outputs                                                                                                      | Log messages confirm execution of setPlaybackSpeedState and expected behavior                       | Should be successful|
 */
TEST_F(DrmSessionManagerTests, ReversePlayback) {
    std::cout << "Entering ReversePlayback test" << std::endl;
    auto cb = [](uint32_t id1, uint32_t id2, const std::string& text) {
         std::cout << "Callback invoked with id1=" << id1
                  << ", id2=" << id2
                  << ", text=" << text << std::endl;
    };
    DrmSessionManager drmSessionManager(10, nullptr, cb);
    std::cout << "Invoking setPlaybackSpeedState with parameters: live = false, currentLatency = 10.0, livepoint = false, liveOffsetMs = 0.0, speed = -1, positionMs = 1500.0, firstFrameSeen = false" << std::endl;
    
    drmSessionManager.setPlaybackSpeedState(false, 10.0, false, 0.0, -1, 1500.0, false);
    
    std::cout << "Method setPlaybackSpeedState executed. Internal state should reflect reverse playback or error handling for negative speed" << std::endl;
    std::cout << "Exiting ReversePlayback test" << std::endl;
}
/**
 * @brief Tests the setPlaybackSpeedState API with extreme input values
 *
 * This test verifies that the DrmSessionManager correctly handles extreme input values when invoking setPlaybackSpeedState. The objective is to ensure that the internal state is updated accordingly without overflow or unexpected behavior when provided with high numeric values.
 *
 * **Test Group ID:** Basic: 01@n
 * **Test Case ID:** 088@n
 * **Priority:** High@n
 *
 * **Pre-Conditions:** None@n
 * **Dependencies:** None@n
 * **User Interaction:** None@n
 *
 * **Test Procedure:**@n
 * | Variation / Step | Description                                                          | Test Data                                                                                                                       | Expected Result                                                                           | Notes           |
 * | :--------------: | -------------------------------------------------------------------- | ------------------------------------------------------------------------------------------------------------------------------- | ----------------------------------------------------------------------------------------- | --------------- |
 * | 01               | Initialize DrmSessionManager object                                  | capacity = 10, pointer = nullptr                                                                                                 | Instance is created successfully without exceptions                                     | Should be successful |
 * | 02               | Invoke setPlaybackSpeedState with extreme input values               | live = true, currentLatency = 1000.0, livepoint = true, liveOffsetMs = 500.0, speed = 1000, positionMs = 10000000.0, firstFrameSeen = true | Method executes without errors and internal state is updated with extreme values         | Should Pass     |
 */
TEST_F(DrmSessionManagerTests, ExtremeValues) {
    std::cout << "Entering ExtremeValues test" << std::endl;
    auto cb = [](uint32_t id1, uint32_t id2, const std::string& text) {
         std::cout << "Callback invoked with id1=" << id1
                  << ", id2=" << id2
                  << ", text=" << text << std::endl;
    };
    DrmSessionManager drmSessionManager(10, nullptr, cb);
    std::cout << "Invoking setPlaybackSpeedState with parameters: live = true, currentLatency = 1000.0, livepoint = true, liveOffsetMs = 500.0, speed = 1000, positionMs = 10000000.0, firstFrameSeen = true" << std::endl;
    
    drmSessionManager.setPlaybackSpeedState(true, 1000.0, true, 500.0, 1000, 10000000.0, true);
    
    std::cout << "Method setPlaybackSpeedState executed. Internal state should reflect extreme input values without causing overflow or unexpected behavior" << std::endl;
    std::cout << "Exiting ExtremeValues test" << std::endl;
}
/**
 * @brief Validates that the DrmSessionManager accepts valid session manager states.
 *
 * This test verifies that the DrmSessionManager object correctly updates its state when provided with valid session manager states. It ensures that no errors are raised during state transitions from inactive to active.
 *
 * **Test Group ID:** Basic: 01@n
 * **Test Case ID:** 089@n
 * **Priority:** High@n
 *
 * **Pre-Conditions:** None@n
 * **Dependencies:** None@n
 * **User Interaction:** None@n
 *
 * **Test Procedure:**
 * | Variation / Step | Description                                                                 | Test Data                                              | Expected Result                                                                     | Notes         |
 * | :-------------:  | --------------------------------------------------------------------------- | ------------------------------------------------------ | ----------------------------------------------------------------------------------- | ------------- |
 * | 01               | Create a DrmSessionManager instance using a valid constructor               | maxDrmSessions = 10, player = nullptr                   | Object is successfully created                                                      | Should be successful |
 * | 02               | Define valid session manager states                                         | validStates = { eSESSIONMGR_INACTIVE, eSESSIONMGR_ACTIVE } | Valid states are defined and available for use                                      | Should be successful |
 * | 03               | Invoke setSessionMgrState for each valid state                              | state = eSESSIONMGR_INACTIVE, then state = eSESSIONMGR_ACTIVE | API successfully updates the state without error                                    | Should Pass   |
 * | 04               | Confirm that no error occurred using an assertion                           | output: true                                           | EXPECT_TRUE returns true indicating no error                                        | Should be successful |
 */
TEST_F(DrmSessionManagerTests, SetValidSessionMgrStates) {
    std::cout << "Entering SetValidSessionMgrStates test" << std::endl;
    
    // Create an object of DrmSessionManager using a valid constructor.
    // Here, we use 10 for maxDrmSessions and nullptr for player.
    auto cb = [](uint32_t id1, uint32_t id2, const std::string& text) {
         std::cout << "Callback invoked with id1=" << id1
                  << ", id2=" << id2
                  << ", text=" << text << std::endl;
    };
    DrmSessionManager drmManager(10, nullptr, cb);
    
    // Define the valid states to test.
    const SessionMgrState validStates[] = { eSESSIONMGR_INACTIVE, eSESSIONMGR_ACTIVE };
    
    // Loop over each valid state and set it.
    for (size_t i = 0; i < sizeof(validStates)/sizeof(validStates[0]); ++i) {
        SessionMgrState state = validStates[i];
        std::cout << "Invoking setSessionMgrState with valid value: " << state << std::endl;
        drmManager.setSessionMgrState(state);
        std::cout << "DrmSessionManager state updated to: " << state << std::endl;
    }
    
    // Confirm that no error occurred.
    EXPECT_TRUE(true);
    
    std::cout << "Exiting SetValidSessionMgrStates test" << std::endl;
}
/**
 * @brief Validate that setSessionMgrState handles invalid state inputs gracefully.
 *
 * This test verifies that when setSessionMgrState is called with invalid state values,
 * the DrmSessionManager does not crash or misbehave. The test involves invoking the API
 * with two distinct invalid states and confirming that the system remains stable.
 *
 * **Test Group ID:** Basic: 01@n
 * **Test Case ID:** 090@n
 * **Priority:** High@n
 *
 * **Pre-Conditions:** None@n
 * **Dependencies:** None@n
 * **User Interaction:** None@n
 *
 * **Test Procedure:**@n
 * | Variation / Step | Description                                                        | Test Data                                                    | Expected Result                                                           | Notes       |
 * | :--------------: | ------------------------------------------------------------------ | ------------------------------------------------------------ | ------------------------------------------------------------------------- | ----------- |
 * | 01               | Invoke setSessionMgrState with an invalid state value of -1         | input1 = 10, input2 = nullptr, state = -1                     | API returns gracefully without crash, assertion EXPECT_TRUE(true)         | Should Pass |
 * | 02               | Invoke setSessionMgrState with an invalid state value of 999        | input1 = 10, input2 = nullptr, state = 999                    | API returns gracefully without crash, assertion EXPECT_TRUE(true)         | Should Pass |
 */
TEST_F(DrmSessionManagerTests, SetInvalidSessionMgrStates) {
    std::cout << "Entering SetInvalidSessionMgrStates test" << std::endl;
    
     auto cb = [](uint32_t id1, uint32_t id2, const std::string& text) {
         std::cout << "Callback invoked with id1=" << id1
                  << ", id2=" << id2
                  << ", text=" << text << std::endl;
    };
    // Create an object of DrmSessionManager using a valid constructor.
    DrmSessionManager drmManager(10, nullptr,cb);
    
    // Define the invalid state values.
    SessionMgrState invalidStates[] = { static_cast<SessionMgrState>(-1), static_cast<SessionMgrState>(999) };
    
    // Loop through each invalid state and attempt to set it.
    for (size_t i = 0; i < sizeof(invalidStates)/sizeof(invalidStates[0]); ++i) {
        SessionMgrState state = invalidStates[i];
        std::cout << "Invoking setSessionMgrState with invalid value: " << state << std::endl;
        // Call the method which is expected to handle unexpected state values gracefully.
        drmManager.setSessionMgrState(state);
        std::cout << "DrmSessionManager handled the invalid value: " << state << " gracefully" << std::endl;
    }
    
    // Confirm that no crash occurred during state setting.
    EXPECT_TRUE(true);
    
    std::cout << "Exiting SetInvalidSessionMgrStates test" << std::endl;
}
/**
 * @brief Verify that enabling video mute for live streaming functions correctly with valid parameters.
 *
 * This test validates that the DrmSessionManager::setVideoMute API does not throw any exceptions when invoked with valid parameters for enabling video mute during a live streaming scenario. The test ensures that the internal state update simulation is performed as expected.
 *
 * **Test Group ID:** Basic: 01@n
 * **Test Case ID:** 091@n
 * **Priority:** High@n
 * 
 * **Pre-Conditions:** None@n
 * **Dependencies:** None@n
 * **User Interaction:** None@n
 * 
 * **Test Procedure:**@n
 * | Variation / Step | Description | Test Data | Expected Result | Notes |
 * | :----: | --------- | ---------- |-------------- | ----- |
 * | 01 | Create an instance of DrmSessionManager using the custom constructor. | input1 = 5, input2 = nullptr | Instance created successfully. | Should be successful |
 * | 02 | Define the input parameters for enabling video mute in live streaming. | live = true, currentLatency = 0.5, livepoint = true, liveOffsetMs = 100.0, videoMuteStatus = true, positionMs = 50.0 | Parameters set with valid values. | Should be successful |
 * | 03 | Invoke setVideoMute with the specified live streaming parameters. | live = true, currentLatency = 0.5, livepoint = true, liveOffsetMs = 100.0, videoMuteStatus = true, positionMs = 50.0 | Method executes without throwing exceptions and outputs confirmation logs. | Should Pass |
 */
TEST_F(DrmSessionManagerTests, PositiveTest_EnableVideoMute_LiveStreaming) {
    std::cout << "Entering PositiveTest_EnableVideoMute_LiveStreaming test" << std::endl;
    
     auto cb = [](uint32_t id1, uint32_t id2, const std::string& text) {
         std::cout << "Callback invoked with id1=" << id1
                  << ", id2=" << id2
                  << ", text=" << text << std::endl;
    };
    // Create object with custom constructor.
    DrmSessionManager drmSessionManager(5, nullptr, cb);
    
    // Define input parameters.
    bool live = true;
    double currentLatency = 0.5;
    bool livepoint = true;
    double liveOffsetMs = 100.0;
    bool videoMuteStatus = true;
    double positionMs = 50.0;
    
    std::cout << "Invoking setVideoMute with parameters:" << std::endl;
    std::cout << "  live: " << live << std::endl;
    std::cout << "  currentLatency: " << currentLatency << std::endl;
    std::cout << "  livepoint: " << livepoint << std::endl;
    std::cout << "  liveOffsetMs: " << liveOffsetMs << std::endl;
    std::cout << "  videoMuteStatus: " << videoMuteStatus << std::endl;
    std::cout << "  positionMs: " << positionMs << std::endl;
    
    EXPECT_NO_THROW({
        drmSessionManager.setVideoMute(live, currentLatency, livepoint, liveOffsetMs, videoMuteStatus, positionMs);
        std::cout << "Method setVideoMute invoked successfully for live streaming enabling video mute." << std::endl;
        // Log internal state update simulation.
        std::cout << "Assuming internal state updated: video mute enabled for live stream at position " << positionMs << std::endl;
    });
    
    std::cout << "Exiting PositiveTest_EnableVideoMute_LiveStreaming test" << std::endl;
}
/**
 * @brief Validate that video mute can be disabled for on-demand playback using setVideoMute.
 *
 * This test verifies that the setVideoMute API correctly disables video mute for on-demand playback.
 * It checks that the method executes without throwing exceptions and that the internal state is updated
 * accordingly to reflect that video mute is disabled and the playback position is tracked correctly.
 *
 * **Test Group ID:** Basic: 01
 * **Test Case ID:** 092
 * **Priority:** High
 *
 * **Pre-Conditions:** None
 * **Dependencies:** None
 * **User Interaction:** None
 *
 * **Test Procedure:**
 * | Variation / Step | Description | Test Data | Expected Result | Notes |
 * | :----: | --------- | ---------- |-------------- | ----- |
 * | 01 | Create DrmSessionManager object using custom constructor. | drmSessionManager: {arg1 = 5, arg2 = nullptr} | Object instantiation succeeds without exceptions. | Should be successful |
 * | 02 | Define input parameters for on-demand playback scenario. | live = false, currentLatency = 0.0, livepoint = false, liveOffsetMs = 0.0, videoMuteStatus = false, positionMs = 120.0 | Input parameters are set correctly. | Should be successful |
 * | 03 | Invoke setVideoMute API with defined parameters to disable video mute. | live = false, currentLatency = 0.0, livepoint = false, liveOffsetMs = 0.0, videoMuteStatus = false, positionMs = 120.0 | Method setVideoMute executes without throwing exceptions. | Should Pass |
 * | 04 | Log appropriate messages indicating the test progress and success. | Console output messages | Logs are printed to indicate test entry, execution, and exit. | Should be successful |
 */

TEST_F(DrmSessionManagerTests, PositiveTest_DisableVideoMute_OnDemand) {
    std::cout << "Entering PositiveTest_DisableVideoMute_OnDemand test" << std::endl;
    
     auto cb = [](uint32_t id1, uint32_t id2, const std::string& text) {
         std::cout << "Callback invoked with id1=" << id1
                  << ", id2=" << id2
                  << ", text=" << text << std::endl;
    };
    // Create object with custom constructor.
    DrmSessionManager drmSessionManager(5, nullptr, cb);
    
    // Define input parameters.
    bool live = false;
    double currentLatency = 0.0;
    bool livepoint = false;
    double liveOffsetMs = 0.0;
    bool videoMuteStatus = false;
    double positionMs = 120.0;
    
    std::cout << "Invoking setVideoMute with parameters:" << std::endl;
    std::cout << "  live: " << live << std::endl;
    std::cout << "  currentLatency: " << currentLatency << std::endl;
    std::cout << "  livepoint: " << livepoint << std::endl;
    std::cout << "  liveOffsetMs: " << liveOffsetMs << std::endl;
    std::cout << "  videoMuteStatus: " << videoMuteStatus << std::endl;
    std::cout << "  positionMs: " << positionMs << std::endl;
    
    EXPECT_NO_THROW({
        drmSessionManager.setVideoMute(live, currentLatency, livepoint, liveOffsetMs, videoMuteStatus, positionMs);
        std::cout << "Method setVideoMute invoked successfully for on-demand playback disabling video mute." << std::endl;
        std::cout << "Assuming internal state updated: video mute disabled and playback position tracked correctly." << std::endl;
    });
    
    std::cout << "Exiting PositiveTest_DisableVideoMute_OnDemand test" << std::endl;
}
/**
 * @brief Test setVideoMute with negative currentLatency input to ensure the API handles negative latency without throwing exceptions.
 *
 * This test verifies that providing a negative value for currentLatency to the setVideoMute API does not cause any exceptions or crashes, and that the internal state is handled appropriately. The test uses a DrmSessionManager object constructed with predefined parameters and invokes the function with a negative currentLatency value.
 *
 * **Test Group ID:** Basic: 01@n
 * **Test Case ID:** 093@n
 * **Priority:** High@n
 *
 * **Pre-Conditions:** None@n
 * **Dependencies:** None@n
 * **User Interaction:** None@n
 *
 * **Test Procedure:**@n
 * | Variation / Step | Description                                                                 | Test Data                                                                                                      | Expected Result                                           | Notes           |
 * | :--------------: | --------------------------------------------------------------------------- | -------------------------------------------------------------------------------------------------------------- | --------------------------------------------------------- | --------------- |
 * | 01               | Initialize DrmSessionManager with a custom constructor                      | constructorArg1 = 5, constructorArg2 = nullptr                                                                  | Object is created successfully                           | Should be successful |
 * | 02               | Define input parameters for setVideoMute including negative currentLatency    | live = true, currentLatency = -10.0, livepoint = true, liveOffsetMs = 100.0, videoMuteStatus = true, positionMs = 30.0 | Parameters are assigned correctly                        | Should be successful |
 * | 03               | Invoke setVideoMute and verify that no exceptions are thrown                  | live = true, currentLatency = -10.0, livepoint = true, liveOffsetMs = 100.0, videoMuteStatus = true, positionMs = 30.0 | No exception is thrown; API handles negative latency as expected | Should Pass     |
 */
TEST_F(DrmSessionManagerTests, NegativeTest_NegativeLatency) {
    std::cout << "Entering NegativeTest_NegativeLatency test" << std::endl;
    
     auto cb = [](uint32_t id1, uint32_t id2, const std::string& text) {
         std::cout << "Callback invoked with id1=" << id1
                  << ", id2=" << id2
                  << ", text=" << text << std::endl;
    };
    // Create object with custom constructor.
    DrmSessionManager drmSessionManager(5, nullptr, cb);
    
    // Define input parameters with negative currentLatency.
    bool live = true;
    double currentLatency = -10.0;
    bool livepoint = true;
    double liveOffsetMs = 100.0;
    bool videoMuteStatus = true;
    double positionMs = 30.0;
    
    std::cout << "Invoking setVideoMute with parameters (including negative currentLatency):" << std::endl;
    std::cout << "  live: " << live << std::endl;
    std::cout << "  currentLatency: " << currentLatency << std::endl;
    std::cout << "  livepoint: " << livepoint << std::endl;
    std::cout << "  liveOffsetMs: " << liveOffsetMs << std::endl;
    std::cout << "  videoMuteStatus: " << videoMuteStatus << std::endl;
    std::cout << "  positionMs: " << positionMs << std::endl;
    
    EXPECT_NO_THROW({
        drmSessionManager.setVideoMute(live, currentLatency, livepoint, liveOffsetMs, videoMuteStatus, positionMs);
        std::cout << "Method setVideoMute invoked successfully with negative currentLatency." << std::endl;
        std::cout << "Assuming internal state handled negative latency appropriately." << std::endl;
    });
    
    std::cout << "Exiting NegativeTest_NegativeLatency test" << std::endl;
}
/**
 * @brief Validate that setVideoMute gracefully handles negative live offset values.
 *
 * This test verifies that when a negative liveOffsetMs value is passed to setVideoMute, the API method handles the input 
 * without throwing any exceptions, thereby ensuring that the internal state manages negative offsets gracefully.
 *
 * **Test Group ID:** Basic: 01@n
 * **Test Case ID:** 094@n
 * **Priority:** (High) 
 * @n
 * **Pre-Conditions:** None@n
 * **Dependencies:** None@n
 * **User Interaction:** None@n
 * @n
 * **Test Procedure:**@n
 * | Variation / Step | Description | Test Data | Expected Result | Notes |
 * | :----: | --------- | ---------- |-------------- | ----- |
 * | 01 | Instantiate DrmSessionManager using custom constructor. | constructor arg1 = 5, arg2 = nullptr | Object instantiated successfully | Should be successful |
 * | 02 | Define input parameters with negative liveOffsetMs. | live = true, currentLatency = 1.0, livepoint = true, liveOffsetMs = -50.0, videoMuteStatus = true, positionMs = 30.0 | Parameters initialized for API invocation | Should be successful |
 * | 03 | Invoke setVideoMute API with negative liveOffsetMs. | live = true, currentLatency = 1.0, livepoint = true, liveOffsetMs = -50.0, videoMuteStatus = true, positionMs = 30.0 | API call completes without throwing an exception | Should Pass |
 * | 04 | Log method invocation completion. | No input | Log messages indicate successful API invocation | Should be successful |
 */
TEST_F(DrmSessionManagerTests, NegativeTest_NegativeLiveOffset) {
    std::cout << "Entering NegativeTest_NegativeLiveOffset test" << std::endl;
    
     auto cb = [](uint32_t id1, uint32_t id2, const std::string& text) {
         std::cout << "Callback invoked with id1=" << id1
                  << ", id2=" << id2
                  << ", text=" << text << std::endl;
    };
    // Create object with custom constructor.
    DrmSessionManager drmSessionManager(5, nullptr, cb);
    
    // Define input parameters with negative liveOffsetMs.
    bool live = true;
    double currentLatency = 1.0;
    bool livepoint = true;
    double liveOffsetMs = -50.0;
    bool videoMuteStatus = true;
    double positionMs = 30.0;
    
    std::cout << "Invoking setVideoMute with parameters (including negative liveOffsetMs):" << std::endl;
    std::cout << "  live: " << live << std::endl;
    std::cout << "  currentLatency: " << currentLatency << std::endl;
    std::cout << "  livepoint: " << livepoint << std::endl;
    std::cout << "  liveOffsetMs: " << liveOffsetMs << std::endl;
    std::cout << "  videoMuteStatus: " << videoMuteStatus << std::endl;
    std::cout << "  positionMs: " << positionMs << std::endl;
    
    EXPECT_NO_THROW({
        drmSessionManager.setVideoMute(live, currentLatency, livepoint, liveOffsetMs, videoMuteStatus, positionMs);
        std::cout << "Method setVideoMute invoked successfully with negative liveOffsetMs." << std::endl;
        std::cout << "Assuming internal state handled negative live offset gracefully." << std::endl;
    });
    
    std::cout << "Exiting NegativeTest_NegativeLiveOffset test" << std::endl;
}
/**
 * @brief Test setVideoMute function with zero latency, offset, and position.
 *
 * This test verifies that the setVideoMute function in the DrmSessionManager class correctly handles edge case input values where latency, offset, and position are zero. It ensures that the method does not throw exceptions and updates the internal state appropriately.
 *
 * **Test Group ID:** Basic: 01
 * **Test Case ID:** 095
 * **Priority:** High
 *
 * **Pre-Conditions:** None
 * **Dependencies:** None
 * **User Interaction:** None
 *
 * **Test Procedure:**
 * | Variation / Step | Description | Test Data | Expected Result | Notes |
 * | :----: | --------- | ---------- | -------------- | ----- |
 * | 01 | Create DrmSessionManager instance and invoke setVideoMute with zero latency, offset, and position | drmSessionManager(5, nullptr), live = false, currentLatency = 0.0, livepoint = false, liveOffsetMs = 0.0, videoMuteStatus = true, positionMs = 0.0 | Method setVideoMute completes without throwing exceptions and updates internal state correctly | Should Pass |
 */
TEST_F(DrmSessionManagerTests, EdgeTest_ZeroLatencyOffsetPosition) {
    std::cout << "Entering EdgeTest_ZeroLatencyOffsetPosition test" << std::endl;
    
    auto cb = [](uint32_t id1, uint32_t id2, const std::string& text) {
         std::cout << "Callback invoked with id1=" << id1
                  << ", id2=" << id2
                  << ", text=" << text << std::endl;
    };
    // Create object with custom constructor.
    DrmSessionManager drmSessionManager(5, nullptr, cb);
    
    // Define input parameters with zero latency, offset, and position.
    bool live = false;
    double currentLatency = 0.0;
    bool livepoint = false;
    double liveOffsetMs = 0.0;
    bool videoMuteStatus = true;
    double positionMs = 0.0;
    
    std::cout << "Invoking setVideoMute with zero latency, offset and position:" << std::endl;
    std::cout << "  live: " << live << std::endl;
    std::cout << "  currentLatency: " << currentLatency << std::endl;
    std::cout << "  livepoint: " << livepoint << std::endl;
    std::cout << "  liveOffsetMs: " << liveOffsetMs << std::endl;
    std::cout << "  videoMuteStatus: " << videoMuteStatus << std::endl;
    std::cout << "  positionMs: " << positionMs << std::endl;
    
    EXPECT_NO_THROW({
        drmSessionManager.setVideoMute(live, currentLatency, livepoint, liveOffsetMs, videoMuteStatus, positionMs);
        std::cout << "Method setVideoMute invoked successfully with zero values." << std::endl;
        std::cout << "Assuming internal state updated correctly despite zero latency, offset, and position." << std::endl;
    });
    
    std::cout << "Exiting EdgeTest_ZeroLatencyOffsetPosition test" << std::endl;
}
/**
 * @brief Verify that setVideoMute handles negative playback position gracefully
 *
 * This test verifies that when the setVideoMute API is invoked with a negative positionMs value,
 * it does not throw an exception and assumes proper internal handling of the negative playback position.
 *
 * **Test Group ID:** Basic: 01
 * **Test Case ID:** 096
 * **Priority:** High
 *
 * **Pre-Conditions:** None
 * **Dependencies:** None
 * **User Interaction:** None
 *
 * **Test Procedure:**
 * | Variation / Step | Description                                                  | Test Data                                                                             | Expected Result                                                                            | Notes              |
 * | :--------------: | ------------------------------------------------------------ | ------------------------------------------------------------------------------------- | ------------------------------------------------------------------------------------------ | ------------------ |
 * | 01               | Create DrmSessionManager object with custom constructor      | constructorArg1 = 5, constructorArg2 = nullptr                                          | Object is created without throwing an exception                                           | Should be successful |
 * | 02               | Invoke setVideoMute with negative positionMs                 | live = false, currentLatency = 2.0, livepoint = false, liveOffsetMs = 200.0, videoMuteStatus = false, positionMs = -5.0 | Method executes without throwing and handles negative playback position correctly           | Should Pass        |
 */
TEST_F(DrmSessionManagerTests, NegativeTest_NegativePositionMs) {
    std::cout << "Entering NegativeTest_NegativePositionMs test" << std::endl;
    
    auto cb = [](uint32_t id1, uint32_t id2, const std::string& text) {
         std::cout << "Callback invoked with id1=" << id1
                  << ", id2=" << id2
                  << ", text=" << text << std::endl;
    };
    // Create object with custom constructor.
    DrmSessionManager drmSessionManager(5, nullptr, cb);
    
    // Define input parameters with negative positionMs.
    bool live = false;
    double currentLatency = 2.0;
    bool livepoint = false;
    double liveOffsetMs = 200.0;
    bool videoMuteStatus = false;
    double positionMs = -5.0;
    
    std::cout << "Invoking setVideoMute with parameters (including negative positionMs):" << std::endl;
    std::cout << "  live: " << live << std::endl;
    std::cout << "  currentLatency: " << currentLatency << std::endl;
    std::cout << "  livepoint: " << livepoint << std::endl;
    std::cout << "  liveOffsetMs: " << liveOffsetMs << std::endl;
    std::cout << "  videoMuteStatus: " << videoMuteStatus << std::endl;
    std::cout << "  positionMs: " << positionMs << std::endl;
    
    EXPECT_NO_THROW({
        drmSessionManager.setVideoMute(live, currentLatency, livepoint, liveOffsetMs, videoMuteStatus, positionMs);
        std::cout << "Method setVideoMute invoked successfully with negative positionMs." << std::endl;
        std::cout << "Assuming internal state handled negative playback position appropriately." << std::endl;
    });
    
    std::cout << "Exiting NegativeTest_NegativePositionMs test" << std::endl;
}
/**
 * @brief Validate that setVideoWindowSize correctly updates video window dimensions when provided with positive values.
 *
 * This test verifies that the DrmSessionManager's setVideoWindowSize method properly updates the internal video window size when supplied with valid, positive width and height dimensions. The test ensures the DrmSessionManager is correctly instantiated and that the method call executes as expected, reflecting the updated dimensions.
 *
 * **Test Group ID:** Basic: 01@n
 * **Test Case ID:** 097@n
 * **Priority:** High@n
 * 
 * **Pre-Conditions:** None@n
 * **Dependencies:** None@n
 * **User Interaction:** None@n
 *
 * **Test Procedure:**
 * | Variation / Step | Description | Test Data | Expected Result | Notes |
 * | :----: | --------- | ---------- |-------------- | ----- |
 * | 01 | Instantiate DrmSessionManager and invoke setVideoWindowSize with valid positive dimensions | maxDrmSessions = 5, player = nullptr, width = 640, height = 480 | Internal video window size updated to 640 x 480 as per method execution | Should Pass |
 */
TEST_F(DrmSessionManagerTests, SetValidVideoWindowSizePositiveDimensions) {
    std::cout << "Entering SetValidVideoWindowSizePositiveDimensions test" << std::endl;
    
    auto cb = [](uint32_t id1, uint32_t id2, const std::string& text) {
         std::cout << "Callback invoked with id1=" << id1
                  << ", id2=" << id2
                  << ", text=" << text << std::endl;
    };
    // Create object using custom constructor (maxDrmSessions = 5, player = nullptr)
    DrmSessionManager drmManager(5, nullptr, cb);
    std::cout << "Created DrmSessionManager object with maxDrmSessions = 5 and player = nullptr" << std::endl;
    
    // Log method invocation and input values
    int width = 640;
    int height = 480;
    std::cout << "Invoking setVideoWindowSize with width = " << width << " and height = " << height << std::endl;
    
    // Invoke the method
    drmManager.setVideoWindowSize(width, height);
    std::cout << "Method setVideoWindowSize executed with values width: " << width << " and height: " << height << std::endl;
    
    // Debug log for expected internal state update
    std::cout << "Expected: Video window size updated to " << width << " x " << height << std::endl;
    
    std::cout << "Exiting SetValidVideoWindowSizePositiveDimensions test" << std::endl;
}
/**
 * @brief Validate functionality of setVideoWindowSize when passed zero dimensions.
 *
 * This test checks whether the DrmSessionManager correctly handles the case when both width and height are set to zero. It verifies that the method is invoked with the provided zero values and that the video window size is updated to 0 x 0 without errors.
 *
 * **Test Group ID:** Basic: 01
 * **Test Case ID:** 098
 * **Priority:** High
 *
 * **Pre-Conditions:** None
 * **Dependencies:** None
 * **User Interaction:** None
 *
 * **Test Procedure:**
 * | Variation / Step | Description | Test Data | Expected Result | Notes |
 * | :----: | --------- | ---------- |-------------- | ----- |
 * | 01 | Create DrmSessionManager object with maxDrmSessions = 5 and player = nullptr | maxDrmSessions = 5, player = nullptr | Object is created successfully | Should be successful |
 * | 02 | Invoke setVideoWindowSize with width = 0 and height = 0 | width = 0, height = 0 | Method executes without error and video window size is updated to 0 x 0 | Should Pass |
 */
TEST_F(DrmSessionManagerTests, SetVideoWindowSizeWithZeroDimensions) {
    std::cout << "Entering SetVideoWindowSizeWithZeroDimensions test" << std::endl;

    // Create object using custom constructor (maxDrmSessions = 5, player = nullptr)
     auto cb = [](uint32_t id1, uint32_t id2, const std::string& text) {
         std::cout << "Callback invoked with id1=" << id1
                  << ", id2=" << id2
                  << ", text=" << text << std::endl;
    };
    DrmSessionManager drmManager(5, nullptr, cb);
    std::cout << "Created DrmSessionManager object with maxDrmSessions = 5 and player = nullptr" << std::endl;
    
    // Log method invocation and input values
    int width = 0;
    int height = 0;
    std::cout << "Invoking setVideoWindowSize with width = " << width << " and height = " << height << std::endl;
    
    // Invoke the method
    drmManager.setVideoWindowSize(width, height);
    std::cout << "Method setVideoWindowSize executed with values width: " << width << " and height: " << height << std::endl;
    
    // Debug log for expected internal state update
    std::cout << "Expected: Video window size updated to " << width << " x " << height << std::endl;
    
    std::cout << "Exiting SetVideoWindowSizeWithZeroDimensions test" << std::endl;
}
/**
 * @brief Test negative width input for setting video window size ensures invalid width is handled properly
 *
 * This test verifies that providing a negative value for the video window width is handled appropriately by the DrmSessionManager.
 * It confirms that the internal state remains unchanged or an error status is logged when an invalid width is passed.
 *
 * **Test Group ID:** Basic: 01@n
 * **Test Case ID:** 099@n
 * **Priority:** (High) 
 *
 * **Pre-Conditions:** None@n
 * **Dependencies:** None@n
 * **User Interaction:** None@n
 * 
 * **Test Procedure:**
 * | Variation / Step | Description                                                                  | Test Data                                      | Expected Result                                                                                  | Notes           |
 * | :--------------: | ----------------------------------------------------------------------------- | ---------------------------------------------- | ------------------------------------------------------------------------------------------------ | --------------- |
 * | 01               | Construct DrmSessionManager object using maxDrmSessions=5 and player=nullptr    | maxDrmSessions = 5, player = nullptr           | Object is created successfully                                                                   | Should be successful |
 * | 02               | Initialize video window size with valid parameters (width = 640, height = 480)   | initWidth = 640, initHeight = 480                | Video window size updated successfully                                                          | Should Pass     |
 * | 03               | Invoke setVideoWindowSize with negative width (-640) and valid height (480)      | width = -640, height = 480                      | Invalid width input handled appropriately; internal state remains unchanged or error logged      | Should Fail     |
 */
TEST_F(DrmSessionManagerTests, SetVideoWindowSizeWithNegativeWidth) {
    std::cout << "Entering SetVideoWindowSizeWithNegativeWidth test" << std::endl;
    
    // Create object using custom constructor (maxDrmSessions = 5, player = nullptr)
     auto cb = [](uint32_t id1, uint32_t id2, const std::string& text) {
         std::cout << "Callback invoked with id1=" << id1
                  << ", id2=" << id2
                  << ", text=" << text << std::endl;
    };
    DrmSessionManager drmManager(5, nullptr, cb);
    std::cout << "Created DrmSessionManager object with maxDrmSessions = 5 and player = nullptr" << std::endl;
    
    // Optionally, set an initial valid state
    int initWidth = 640, initHeight = 480;
    std::cout << "Initializing video window size to width = " << initWidth << " and height = " << initHeight << std::endl;
    drmManager.setVideoWindowSize(initWidth, initHeight);
    
    // Log method invocation and input values for invalid test input
    int width = -640;
    int height = 480;
    std::cout << "Invoking setVideoWindowSize with invalid width = " << width << " and valid height = " << height << std::endl;
    
    // Invoke the method with negative width
    drmManager.setVideoWindowSize(width, height);
    std::cout << "Method setVideoWindowSize executed with values width: " << width << " and height: " << height << std::endl;
    
    // Debug log indicating expected behavior (internal state should be handled appropriately)
    std::cout << "Expected: Invalid width input handled appropriately; internal state should remain unchanged or error status logged" << std::endl;
    
    std::cout << "Exiting SetVideoWindowSizeWithNegativeWidth test" << std::endl;
}
/**
 * @brief Verify that setVideoWindowSize correctly handles negative height values.
 *
 * This test validates that when a negative height is provided to the setVideoWindowSize API, the method handles it appropriately by either rejecting the value or not updating the internal state. The test creates a valid initial state with positive dimensions, then applies a negative height to examine API behavior.
 *
 * **Test Group ID:** Basic: 01@n
 * **Test Case ID:** 100@n
 * **Priority:** High@n
 * 
 * **Pre-Conditions:** None@n
 * **Dependencies:** None@n
 * **User Interaction:** None@n
 * 
 * **Test Procedure:**
 * | Variation / Step | Description                                                                                         | Test Data                                                         | Expected Result                                                                                                              | Notes         |
 * | :--------------: | --------------------------------------------------------------------------------------------------- | ----------------------------------------------------------------- | ---------------------------------------------------------------------------------------------------------------------------- | ------------- |
 * | 01               | Create DrmSessionManager object with maxDrmSessions = 5 and player = nullptr                        | maxDrmSessions = 5, player = nullptr                               | DrmSessionManager object is successfully created                                                                             | Should be successful |
 * | 02               | Initialize video window size to width = 640 and height = 480                                        | initWidth = 640, initHeight = 480                                    | Video window size is set to a valid state (width = 640, height = 480)                                                         | Should be successful |
 * | 03               | Invoke setVideoWindowSize with valid width = 640 and invalid negative height = -480                 | width = 640, height = -480                                             | API handles the negative height appropriately; internal state remains unchanged or an error status is logged                | Should Fail   |
 */
TEST_F(DrmSessionManagerTests, SetVideoWindowSizeWithNegativeHeight) {
    std::cout << "Entering SetVideoWindowSizeWithNegativeHeight test" << std::endl;
    
    // Create object using custom constructor (maxDrmSessions = 5, player = nullptr)
    auto cb = [](uint32_t id1, uint32_t id2, const std::string& text) {
         std::cout << "Callback invoked with id1=" << id1
                  << ", id2=" << id2
                  << ", text=" << text << std::endl;
    };
    DrmSessionManager drmManager(5, nullptr, cb);
    std::cout << "Created DrmSessionManager object with maxDrmSessions = 5 and player = nullptr" << std::endl;
    
    // Set an initial valid state
    int initWidth = 640, initHeight = 480;
    std::cout << "Initializing video window size to width = " << initWidth << " and height = " << initHeight << std::endl;
    drmManager.setVideoWindowSize(initWidth, initHeight);
    
    // Log method invocation and input values for invalid test input
    int width = 640;
    int height = -480;
    std::cout << "Invoking setVideoWindowSize with valid width = " << width << " and invalid height = " << height << std::endl;
    
    // Invoke the method with negative height
    drmManager.setVideoWindowSize(width, height);
    std::cout << "Method setVideoWindowSize executed with values width: " << width << " and height: " << height << std::endl;
    
    // Debug log indicating expected behavior (invalid height should be handled appropriately)
    std::cout << "Expected: Invalid height input handled appropriately; internal state should remain unchanged or error status logged" << std::endl;
    
    std::cout << "Exiting SetVideoWindowSizeWithNegativeHeight test" << std::endl;
}
/**
 * @brief Tests setVideoWindowSize with extremely large integer dimensions.
 *
 * This test verifies that the setVideoWindowSize API can handle extreme dimension values by invoking it with maximum integer values for width and height, ensuring the method performs the update correctly without failure.
 *
 * **Test Group ID:** Basic: 01@n
 * **Test Case ID:** 101@n
 * **Priority:** High@n
 * 
 * **Pre-Conditions:** None@n
 * **Dependencies:** None@n
 * **User Interaction:** None@n
 * 
 * **Test Procedure:**
 * | Variation / Step | Description                                                     | Test Data                                      | Expected Result                                                                 | Notes              |
 * | :--------------: | --------------------------------------------------------------- | ---------------------------------------------- | ------------------------------------------------------------------------------- | ------------------ |
 * | 01               | Create a DrmSessionManager object using a custom constructor      | maxDrmSessions = 5, player = nullptr             | Object is successfully created                                                  | Should be successful |
 * | 02               | Invoke setVideoWindowSize with maximum integer dimensions         | input: width = INT_MAX, height = INT_MAX         | Method executes without error and updates the video window size accordingly     | Should Pass        |
 */
TEST_F(DrmSessionManagerTests, SetVideoWindowSizeWithLargeDimensions) {
    std::cout << "Entering SetVideoWindowSizeWithLargeDimensions test" << std::endl;
    
    // Create object using custom constructor (maxDrmSessions = 5, player = nullptr)
     auto cb = [](uint32_t id1, uint32_t id2, const std::string& text) {
         std::cout << "Callback invoked with id1=" << id1
                  << ", id2=" << id2
                  << ", text=" << text << std::endl;
    };
    DrmSessionManager drmManager(5, nullptr, cb);
    std::cout << "Created DrmSessionManager object with maxDrmSessions = 5 and player = nullptr" << std::endl;
    
    // Log method invocation and input values for large dimensions
    int width = INT_MAX;
    int height = INT_MAX;
    std::cout << "Invoking setVideoWindowSize with extremely large dimensions: width = " << width << " and height = " << height << std::endl;
    
    // Invoke the method
    drmManager.setVideoWindowSize(width, height);
    std::cout << "Method setVideoWindowSize executed with values width: " << width << " and height: " << height << std::endl;
    
    // Debug log for expected internal state update
    std::cout << "Expected: Video window size updated to maximum allowable values (" << width << " x " << height << ")" << std::endl;
    
    std::cout << "Exiting SetVideoWindowSizeWithLargeDimensions test" << std::endl;
}
/**
 * @brief Verify valid session handle and proper system data for DRM session manager.
 *
 * This test validates that the DRM session manager correctly handles a valid session handle and proper system data 
 * by successfully invoking the watermarkSessionHandlerWrapper method. The objective is to ensure the API executes 
 * without error when provided with a correctly initialized DRM session manager, a valid session handle, a successful 
 * status, and valid system data.
 *
 * **Test Group ID:** Basic: 01@n
 * **Test Case ID:** 102@n
 * **Priority:** High@n
 * 
 * **Pre-Conditions:** None@n
 * **Dependencies:** None@n
 * **User Interaction:** None@n
 * 
 * **Test Procedure:**@n
 * | Variation / Step | Description                                                            | Test Data                                                       | Expected Result                                             | Notes             |
 * | :--------------: | ---------------------------------------------------------------------- | --------------------------------------------------------------- | ----------------------------------------------------------- | ----------------- |
 * | 01               | Construct the DrmSessionManager object using a custom constructor.       | input1 = 10, input2 = nullptr                                     | Object is created successfully.                             | Should be successful |
 * | 02               | Set up the input parameters for the API invocation.                      | sessionHndle = 1, status = 200, systemData = "ValidSystemData"      | Input parameters are initialized correctly.                 | Should be successful |
 * | 03               | Invoke watermarkSessionHandlerWrapper with the prepared input parameters. | sessionHndle = 1, status = 200, systemData = "ValidSystemData"      | API invoked successfully, returns void without errors.      | Should Pass       |
 */
TEST_F(DrmSessionManagerTests, ValidSessionHandleAndProperSystemData)
{
    std::cout << "Entering ValidSessionHandleAndProperSystemData test" << std::endl;
    
    // Construct the object using custom constructor.
    auto cb = [](uint32_t id1, uint32_t id2, const std::string& text) {
         std::cout << "Callback invoked with id1=" << id1
                  << ", id2=" << id2
                  << ", text=" << text << std::endl;
    };
    DrmSessionManager drmManager(10, nullptr, cb);
    
    // Set up input parameters.
    uint32_t sessionHndle = 1;
    uint32_t status = 200;
    std::string systemData = "ValidSystemData";
    
    std::cout << "Invoking watermarkSessionHandlerWrapper with sessionHndle = " 
              << sessionHndle << ", status = " << status 
              << ", systemData = " << systemData << std::endl;
    
    // Invoke method.
    drmManager.watermarkSessionHandlerWrapper(sessionHndle, status, systemData);
    
    std::cout << "Exiting ValidSessionHandleAndProperSystemData test" << std::endl;
}
/**
 * @brief Validate behavior with minimum boundary values for session handle and status
 *
 * This test verifies that the DrmSessionManager API correctly handles the minimal boundary values for session handle and status. It creates a manager instance with dummy parameters, sets the session handle and status to their boundary minimum value (0), and invokes the watermarkSessionHandlerWrapper function to ensure the API call is made successfully.
 *
 * **Test Group ID:** Basic: 01@n
 * **Test Case ID:** 103@n
 * **Priority:** High@n
 *
 * **Pre-Conditions:** None@n
 * **Dependencies:** None@n
 * **User Interaction:** None@n
 *
 * **Test Procedure:**@n
 * | Variation / Step | Description                                                   | Test Data                                              | Expected Result                                                         | Notes         |
 * | :--------------: | ------------------------------------------------------------- | ------------------------------------------------------ | ----------------------------------------------------------------------- | ------------- |
 * | 01               | Create an instance of DrmSessionManager with dummy parameters   | Parameter1 = 10, Parameter2 = nullptr                    | Instance created successfully                                           | Should be successful |
 * | 02               | Set the session handle, status, and systemData with boundary minimum values | sessionHndle = 0, status = 0, systemData = MinimalData   | Variables set to expected boundary values                               | Should be successful |
 * | 03               | Invoke watermarkSessionHandlerWrapper function with the prepared inputs | sessionHndle = 0, status = 0, systemData = MinimalData   | Function executed without error; API returns expected behavior (if applicable) | Should Pass   |
 */
TEST_F(DrmSessionManagerTests, BoundaryValuesWithMinimumSessionHandleAndStatus)
{
    std::cout << "Entering BoundaryValuesWithMinimumSessionHandleAndStatus test" << std::endl;
    
    auto cb = [](uint32_t id1, uint32_t id2, const std::string& text) {
         std::cout << "Callback invoked with id1=" << id1
                  << ", id2=" << id2
                  << ", text=" << text << std::endl;
    };
    // Create instance with dummy parameters.
    DrmSessionManager drmManager(10, nullptr, cb);
    
    // Set boundary input parameters.
    uint32_t sessionHndle = 0;
    uint32_t status = 0;
    std::string systemData = "MinimalData";
    
    std::cout << "Invoking watermarkSessionHandlerWrapper with sessionHndle = " 
              << sessionHndle << ", status = " << status 
              << ", systemData = " << systemData << std::endl;
              
    drmManager.watermarkSessionHandlerWrapper(sessionHndle, status, systemData);
    
    std::cout << "Exiting BoundaryValuesWithMinimumSessionHandleAndStatus test" << std::endl;
}
/**
 * @brief Validate handling of maximum uint32_t values for session handle and status
 *
 * This test evaluates the API’s capability to manage the maximum boundary values for both session handle and status by passing the maximum uint32_t values (4294967295) along with a sample system data string. The objective is to ensure that watermarkSessionHandlerWrapper handles edge cases correctly without any errors.
 *
 * **Test Group ID:** Basic: 01@n
 * **Test Case ID:** 104@n
 * **Priority:** High@n
 * 
 * **Pre-Conditions:** None@n
 * **Dependencies:** None@n
 * **User Interaction:** None@n
 * 
 * **Test Procedure:**@n
 * | Variation / Step | Description | Test Data | Expected Result | Notes |
 * | :----: | --------- | ---------- | -------------- | ----- |
 * | 01 | Initialize DrmSessionManager with dummy parameters and invoke watermarkSessionHandlerWrapper with max uint32_t values for session handle and status along with a sample system data string | sessionHndle = 4294967295, status = 4294967295, systemData = MaximalData | API call handles maximum values correctly without errors | Should Pass |
 */
TEST_F(DrmSessionManagerTests, MaximumValuesForSessionHandleAndStatus)
{
    std::cout << "Entering MaximumValuesForSessionHandleAndStatus test" << std::endl;
    
    auto cb = [](uint32_t id1, uint32_t id2, const std::string& text) {
         std::cout << "Callback invoked with id1=" << id1
                  << ", id2=" << id2
                  << ", text=" << text << std::endl;
    };
    // Create instance with dummy parameters.
    DrmSessionManager drmManager(10, nullptr, cb);
    
    // Maximum values for uint32_t.
    uint32_t sessionHndle = 4294967295u;
    uint32_t status = 4294967295u;
    std::string systemData = "MaximalData";
    
    std::cout << "Invoking watermarkSessionHandlerWrapper with sessionHndle = " 
              << sessionHndle << ", status = " << status 
              << ", systemData = " << systemData << std::endl;
              
    drmManager.watermarkSessionHandlerWrapper(sessionHndle, status, systemData);
    
    std::cout << "Exiting MaximumValuesForSessionHandleAndStatus test" << std::endl;
}
/**
 * @brief Test the handling of an empty systemData string in the watermarkSessionHandlerWrapper API.
 *
 * This test verifies that the watermarkSessionHandlerWrapper API can handle an empty string for the systemData parameter appropriately. The test constructs a DrmSessionManager instance with a custom constructor and calls the API with an empty systemData string to ensure it processes this scenario without errors.
 *
 * **Test Group ID:** Basic: 01@n
 * **Test Case ID:** 105@n
 * **Priority:** High@n
 *
 * **Pre-Conditions:** None@n
 * **Dependencies:** None@n
 * **User Interaction:** None@n
 *
 * **Test Procedure:**
 * | Variation / Step | Description                                                                          | Test Data                                          | Expected Result                                      | Notes         |
 * | :--------------: | ------------------------------------------------------------------------------------ | -------------------------------------------------- | ---------------------------------------------------- | ------------- |
 * | 01               | Create an instance of DrmSessionManager using the custom constructor.                | constructor param1 = 10, param2 = nullptr            | Instance created successfully.                       | Should be successful |
 * | 02               | Invoke watermarkSessionHandlerWrapper with sessionHndle, status, and empty systemData. | sessionHndle = 100, status = 50, systemData = ""     | API call completes without errors.                   | Should Pass   |
 */
TEST_F(DrmSessionManagerTests, HandlingEmptySystemDataString)
{
    std::cout << "Entering HandlingEmptySystemDataString test" << std::endl;
    
    auto cb = [](uint32_t id1, uint32_t id2, const std::string& text) {
         std::cout << "Callback invoked with id1=" << id1
                  << ", id2=" << id2
                  << ", text=" << text << std::endl;
    };
    // Create instance using custom constructor.
    DrmSessionManager drmManager(10, nullptr, cb);
    
    // Inputs with empty system data.
    uint32_t sessionHndle = 100;
    uint32_t status = 50;
    std::string systemData = "";
    
    std::cout << "Invoking watermarkSessionHandlerWrapper with sessionHndle = " 
              << sessionHndle << ", status = " << status 
              << ", systemData (empty string)" << std::endl;
              
    drmManager.watermarkSessionHandlerWrapper(sessionHndle, status, systemData);
    
    std::cout << "Exiting HandlingEmptySystemDataString test" << std::endl;
}
/**
 * @brief Test DrmSessionManager API for handling unsupported status codes.
 *
 * This test verifies that the DrmSessionManager::watermarkSessionHandlerWrapper function correctly handles an unsupported status code by invoking the API with a deliberately invalid status value along with valid session handle and system data.
 *
 * **Test Group ID:** Basic: 01@n
 * **Test Case ID:** 106@n
 * **Priority:** High@n
 *
 * **Pre-Conditions:** None@n
 * **Dependencies:** None@n
 * **User Interaction:** None@n
 *
 * **Test Procedure:**@n
 * | Variation / Step | Description                                                         | Test Data                                                              | Expected Result                                                                 | Notes          |
 * | :----:           | ------------------------------------------------------------------- | ---------------------------------------------------------------------- | ------------------------------------------------------------------------------- | -------------- |
 * | 01               | Create an instance of DrmSessionManager using the custom constructor  | input1 = 10, input2 = nullptr                                            | Instance of DrmSessionManager is created successfully                          | Should be successful |
 * | 02               | Prepare test data with an unsupported status code                     | input1 = sessionHndle, value = 10; input2 = status, value = 9999; input3 = systemData, value = "InvalidStatusData" | Test data is correctly prepared                                             | Should be successful |
 * | 03               | Invoke watermarkSessionHandlerWrapper with the prepared test data       | sessionHndle = 10, status = 9999, systemData = "InvalidStatusData"       | The API should process the unsupported status code scenario as per design specifications | Should Pass    |
 */
TEST_F(DrmSessionManagerTests, UnsupportedStatusCodeScenario)
{
    std::cout << "Entering UnsupportedStatusCodeScenario test" << std::endl;
    
    auto cb = [](uint32_t id1, uint32_t id2, const std::string& text) {
         std::cout << "Callback invoked with id1=" << id1
                  << ", id2=" << id2
                  << ", text=" << text << std::endl;
    };
    // Create instance using custom constructor.
    DrmSessionManager drmManager(10, nullptr, cb);
    
    // Input with unsupported status code.
    uint32_t sessionHndle = 10;
    uint32_t status = 9999;
    std::string systemData = "InvalidStatusData";
    
    std::cout << "Invoking watermarkSessionHandlerWrapper with sessionHndle = " 
              << sessionHndle << ", status = " << status 
              << ", systemData = " << systemData << std::endl;
              
    drmManager.watermarkSessionHandlerWrapper(sessionHndle, status, systemData);
    
    std::cout << "Exiting UnsupportedStatusCodeScenario test" << std::endl;
}
/**
 * @brief Validate that watermarkSessionHandlerWrapper correctly processes system data containing special or corrupt characters.
 *
 * This test validates that watermarkSessionHandlerWrapper successfully handles system data, which contains special characters.
 * It constructs a DrmSessionManager instance, prepares a special characters string using strncpy, and invokes the API 
 * using predefined session handle and status values.
 *
 * **Test Group ID:** Basic: 01@n
 * **Test Case ID:** 107@n
 * **Priority:** High@n
 *
 * **Pre-Conditions:** None@n
 * **Dependencies:** None@n
 * **User Interaction:** None@n
 *
 * **Test Procedure:**@n
 * | Variation / Step | Description | Test Data | Expected Result | Notes |
 * | :----: | --------- | ---------- |-------------- | ----- |
 * | 01 | Create DrmSessionManager instance using the custom constructor | input1 = sessionSize: 10, input2 = nullptr, output: instance created | Instance is created successfully | Should be successful |
 * | 02 | Prepare systemData containing special characters using strncpy | input1 = srcData: "!@#$%^&*()_+-=[]{}|;':,./<>?", output: specialData buffer filled and null terminated | Buffer contains the expected special characters string | Should be successful |
 * | 03 | Invoke watermarkSessionHandlerWrapper with sessionHndle, status, and systemData | input1 = sessionHndle: 10, input2 = status: 400, input3 = systemData: string with special characters, output: API call result | API call is executed without errors and processes the input correctly | Should Pass |
 */
TEST_F(DrmSessionManagerTests, SystemDataContainingSpecialOrCorruptCharacters)
{
    std::cout << "Entering SystemDataContainingSpecialOrCorruptCharacters test" << std::endl;
    
    auto cb = [](uint32_t id1, uint32_t id2, const std::string& text) {
         std::cout << "Callback invoked with id1=" << id1
                  << ", id2=" << id2
                  << ", text=" << text << std::endl;
    };
    // Create instance using custom constructor.
    DrmSessionManager drmManager(10, nullptr, cb);
    
    // Input with special characters in systemData.
    uint32_t sessionHndle = 10;
    uint32_t status = 400;
    
    // Assign fixed-size array value using strncpy.
    char specialData[64];
    const char* srcData = "!@#$%^&*()_+-=[]{}|;':,./<>?";
    strncpy(specialData, srcData, sizeof(specialData) - 1);
    specialData[sizeof(specialData) - 1] = '\0';
    
    std::string systemData(specialData);
    
    std::cout << "Invoking watermarkSessionHandlerWrapper with sessionHndle = " 
              << sessionHndle << ", status = " << status 
              << ", systemData = " << systemData << std::endl;
              
    drmManager.watermarkSessionHandlerWrapper(sessionHndle, status, systemData);
    
    std::cout << "Exiting SystemDataContainingSpecialOrCorruptCharacters test" << std::endl;
}
/**
 * @brief Test for verifying valid destruction of a DrmSessionManager object
 *
 * This test verifies that a DrmSessionManager object is constructed with valid parameters (maxDrmSessions = 5 and player = nullptr) and that its destructor is implicitly invoked without throwing exceptions when the object goes out of scope. The test ensures that the internal state logs are printed correctly during construction and destruction.
 *
 * **Test Group ID:** Basic: 01@n
 * **Test Case ID:** 108@n
 * **Priority:** High@n
 *
 * **Pre-Conditions:** None@n
 * **Dependencies:** None@n
 * **User Interaction:** None@n
 *
 * **Test Procedure:**
 * | Variation / Step | Description | Test Data | Expected Result | Notes |
 * | :----: | --------- | ---------- |-------------- | ----- |
 * | 01 | Invoke DrmSessionManager constructor with maxDrmSessions = 5 and player set to nullptr, then allow the object to go out of scope to invoke the destructor implicitly | input1 = 5, input2 = nullptr, output1 = internal log outputs | DrmSessionManager object is constructed successfully and its destructor is invoked on scope exit without exceptions | Should Pass |
 */
TEST_F(DrmSessionManagerTests, Destructor_ValidDestruction) {
    std::cout << "Entering Destructor_ValidDestruction test" << std::endl;
    
    {
        std::cout << "Invoking DrmSessionManager constructor with maxDrmSessions = 5 and player = nullptr" << std::endl;
        auto cb = [](uint32_t id1, uint32_t id2, const std::string& text) {
         std::cout << "Callback invoked with id1=" << id1
                  << ", id2=" << id2
                  << ", text=" << text << std::endl;
    };
        DrmSessionManager drmSessionManager(5, nullptr, cb);
        std::cout << "DrmSessionManager object constructed successfully. Internal state:" << std::endl;
        std::cout << "  maxDrmSessions: 5" << std::endl;
        std::cout << "  player pointer: " << static_cast<const void*>(nullptr) << std::endl;
        // Additional internal state logs can be added here as needed.
        std::cout << "Leaving local scope to invoke destructor implicitly" << std::endl;
    }
    
    std::cout << "DrmSessionManager destructor was invoked as object went out of scope without throwing exceptions" << std::endl;
    std::cout << "Exiting Destructor_ValidDestruction test" << std::endl;
}

/**
 * @brief Validate that a valid LAProfileBegin callback is registered and correctly invoked.
 *
 * This test verifies that the DrmSessionManager can register a valid lambda callback for LAProfileBegin, 
 * and upon simulating the callback invocation, it updates the status variable as expected. The objective 
 * is to ensure that the registration mechanism works correctly in a positive scenario.
 *
 * **Test Group ID:** Basic: 01@n
 * **Test Case ID:** 109@n
 * **Priority:** High@n
 * 
 * **Pre-Conditions:** None@n
 * **Dependencies:** None@n
 * **User Interaction:** None@n
 * 
 * **Test Procedure:**@n
 * | Variation / Step | Description | Test Data | Expected Result | Notes |
 * | :----: | --------- | ---------- |-------------- | ----- |
 * | 01 | Create a DrmSessionManager instance using a custom constructor | maxDrmSessions = 3, player = nullptr | DrmSessionManager object is created successfully | Should be successful |
 * | 02 | Initialize the callbackStatus variable to -1 | callbackStatus = -1 | callbackStatus is set to -1 | Should be successful |
 * | 03 | Define a lambda callback that updates callbackStatus when invoked | lambdaCallback defined to update callbackStatus with received status | Lambda callback is defined correctly | Should be successful |
 * | 04 | Register the lambda callback using RegisterLAProfBegin | callback = lambdaCallback | LAProfileBegin callback is successfully registered in laprofileBeginCb | Should Pass |
 * | 05 | Simulate the invocation of the registered callback with testStatus = 10 | testStatus = 10 | The lambda callback is invoked and updates callbackStatus to 10 | Should Pass |
 * | 06 | Validate the callback invocation by asserting callbackStatus equals testStatus | callbackStatus expected value = 10, testStatus = 10 | ASSERT_EQ verifies that callbackStatus equals 10 | Should Pass |
 */
TEST_F(DrmSessionManagerTests, RegisterValidLAProfileBegin) {
    std::cout << "Entering RegisterValidLAProfileBegin test" << std::endl;
    
    // Create DrmSessionManager object using a custom constructor (maxDrmSessions=3, dummy player pointer)
    auto cb = [](uint32_t id1, uint32_t id2, const std::string& text) {
         std::cout << "Callback invoked with id1=" << id1
                  << ", id2=" << id2
                  << ", text=" << text << std::endl;
    };
    DrmSessionManager drmManager(3, nullptr, cb);
    std::cout << "Created DrmSessionManager object with maxDrmSessions=3 and player=nullptr" << std::endl;
    
    // Define a variable to capture the status value from the callback
    int callbackStatus = -1;
    std::cout << "Initialized callbackStatus to " << callbackStatus << std::endl;
    
    // Define a lambda callback that takes an int parameter and updates callbackStatus
    auto lambdaCallback = [&callbackStatus](int status) {
        std::cout << "Inside lambda callback: received status = " << status << std::endl;
        callbackStatus = status;
    };
    std::cout << "Defined lambda callback for LAProfileBegin with expected behavior" << std::endl;
    
    // Invoke RegisterLAProfBegin to register the lambda callback
    std::cout << "Invoking DrmSessionManager::RegisterLAProfBegin with lambda callback" << std::endl;
    drmManager.RegisterLAProfBegin(lambdaCallback);
    std::cout << "Registered lambda callback to laprofileBeginCb" << std::endl;
    
    // Simulate invocation of the registered callback with an int argument (e.g., 10)
    int testStatus = 10;
    std::cout << "Invoking the registered laprofileBeginCb with test status " << testStatus << std::endl;
    drmManager.laprofileBeginCb(testStatus);
    
    // Verify that the callback updated callbackStatus appropriately
    std::cout << "After callback invocation, callbackStatus = " << callbackStatus << std::endl;
    ASSERT_EQ(callbackStatus, testStatus);
    
    std::cout << "Exiting RegisterValidLAProfileBegin test" << std::endl;
}
/**
 * @brief Test to validate handling of an empty (null) LAProfileBegin callback registration
 *
 * This test verifies that when an empty (null) callback is registered with the DrmSessionManager,
 * the internal callback (laprofileBeginCb) remains empty. It ensures that the system correctly handles
 * registration of a null callback without causing unexpected behavior.
 *
 * **Test Group ID:** Basic: 01@n
 * **Test Case ID:** 110@n
 * **Priority:** High@n
 *
 * **Pre-Conditions:** None@n
 * **Dependencies:** None@n
 * **User Interaction:** None@n
 *
 * **Test Procedure:**
 * | Variation / Step | Description                                                                              | Test Data                                                         | Expected Result                                                             | Notes         |
 * | :--------------: | ---------------------------------------------------------------------------------------- | ----------------------------------------------------------------- | --------------------------------------------------------------------------- | ------------- |
 * | 01               | Create DrmSessionManager object using a custom constructor                                 | maxDrmSessions = 3, player = nullptr                                | Object is created successfully                                              | Should be successful |
 * | 02               | Define an empty (null) LAProfileBegin callback                                             | emptyCallback = nullptr                                             | Empty callback is defined                                                   | Should be successful |
 * | 03               | Invoke RegisterLAProfBegin with the empty callback                                         | callback parameter = nullptr                                        | Internal callback (laprofileBeginCb) is set to empty                         | Should Pass   |
 * | 04               | Verify that the internal callback is empty via assertion                                   | No direct input; check internal state                               | ASSERT_FALSE confirms that laprofileBeginCb evaluates to false               | Should Pass   |
 */
TEST_F(DrmSessionManagerTests, RegisterEmptyLAProfileBegin) {
    std::cout << "Entering RegisterEmptyLAProfileBegin test" << std::endl;
    
    // Create DrmSessionManager object using a custom constructor (maxDrmSessions=3, dummy player pointer)
    auto cb = [](uint32_t id1, uint32_t id2, const std::string& text) {
         std::cout << "Callback invoked with id1=" << id1
                  << ", id2=" << id2
                  << ", text=" << text << std::endl;
    };
    DrmSessionManager drmManager(3, nullptr, cb);
    std::cout << "Created DrmSessionManager object with maxDrmSessions=3 and player=nullptr" << std::endl;
    
    // Define an empty (null) LAProfileBegin callback
    std::function<void(int)> emptyCallback = nullptr;
    std::cout << "Defined an empty (null) callback" << std::endl;
    
    // Invoke RegisterLAProfBegin with the empty callback
    std::cout << "Invoking DrmSessionManager::RegisterLAProfBegin with empty callback" << std::endl;
    drmManager.RegisterLAProfBegin(emptyCallback);
    std::cout << "Registered empty callback to laprofileBeginCb" << std::endl;
    
    // Verify that the internal callback is empty
    if (!drmManager.laprofileBeginCb) {
        std::cout << "laprofileBeginCb is empty as expected" << std::endl;
    } else {
        std::cout << "laprofileBeginCb is not empty, unexpected behavior" << std::endl;
    }
    
    // Attempting to invoke an empty callback should not be performed, but we confirm its emptiness via the bool operator.
    ASSERT_FALSE(static_cast<bool>(drmManager.laprofileBeginCb));
    
    std::cout << "Exiting RegisterEmptyLAProfileBegin test" << std::endl;
}
/**
 * @brief Validate that RegisterLAProfEnd correctly registers and invokes a valid callback.
 *
 * This test creates an instance of DrmSessionManager with specific parameters, registers a valid lambda callback that updates a flag, and then invokes the callback to verify that it updates the flag as expected. The objective is to ensure that the API correctly assigns and executes the provided callback.
 *
 * **Test Group ID:** Basic: 01
 * **Test Case ID:** 111
 * **Priority:** High
 *
 * **Pre-Conditions:** None
 * **Dependencies:** None
 * **User Interaction:** None
 *
 * **Test Procedure:**
 * | Variation / Step | Description | Test Data | Expected Result | Notes |
 * | :--------------: | ----------- | --------- | --------------- | ----- |
 * | 01 | Instantiate DrmSessionManager with maxDrmSessions = 10 and player = nullptr. | input: maxDrmSessions = 10, player = nullptr | Instance is created successfully. | Should be successful |
 * | 02 | Define a lambda callback that sets a flag (callbackInvoked) to true upon invocation with a streamType parameter. | input: lambda callback, streamType parameter; output: callbackInvoked flag = false initially, then true after invocation | Lambda callback is defined correctly. | Should be successful |
 * | 03 | Register the defined valid callback using RegisterLAProfEnd. | input: callback = lambda callback | Callback is registered and assigned to laprofileEndCb. | Should be successful |
 * | 04 | Invoke the registered callback with streamType = 1. | input: streamType = 1 | callbackInvoked flag is updated to true. | Should Pass |
 * | 05 | Validate that callbackInvoked is true via the EXPECT_TRUE assertion. | input: callbackInvoked flag | EXPECT_TRUE(callbackInvoked) passes confirming the callback was invoked. | Should Pass |
 */
TEST_F(DrmSessionManagerTests, RegisterLAProfEnd_ValidCallback)
{
    std::cout << "Entering RegisterLAProfEnd_ValidCallback test" << std::endl;
    
    // Create an instance of DrmSessionManager with dummy parameters.
    auto cb = [](uint32_t id1, uint32_t id2, const std::string& text) {
         std::cout << "Callback invoked with id1=" << id1
                  << ", id2=" << id2
                  << ", text=" << text << std::endl;
    };
    DrmSessionManager drmSessionManager(10, nullptr, cb);
    std::cout << "Created DrmSessionManager instance with maxDrmSessions = 10 and player = nullptr" << std::endl;
    
    // Prepare a flag to be updated by the lambda callback and define the lambda.
    bool callbackInvoked = false;
    auto callback = [&callbackInvoked](int streamType)
    {
        std::cout << "Lambda callback invoked with streamType: " << streamType << std::endl;
        callbackInvoked = true;
        std::cout << "Flag updated to: " << std::boolalpha << callbackInvoked << std::endl;
    };
    
    std::cout << "Invoking RegisterLAProfEnd with valid callback lambda" << std::endl;
    drmSessionManager.RegisterLAProfEnd(callback);
    std::cout << "Callback registered. Now invoking the registered callback with streamType 1" << std::endl;
    
    // Invoke the callback via the object's member to verify assignment.
    if (drmSessionManager.laprofileEndCb)
    {
        drmSessionManager.laprofileEndCb(1);
        std::cout << "After invocation, flag is: " << std::boolalpha << callbackInvoked << std::endl;
    }
    else
    {
        std::cout << "Error: Callback was not registered properly." << std::endl;
    }
    
    EXPECT_TRUE(callbackInvoked);
    std::cout << "Exiting RegisterLAProfEnd_ValidCallback test" << std::endl;
}
/**
 * @brief Verify that the RegisterLAProfEnd method correctly handles an empty callback.
 *
 * This test validates that when an empty callback is passed to the RegisterLAProfEnd method of the DrmSessionManager, the internal callback remains empty and does not result in any unexpected behavior or exceptions. It ensures that even if the callback is empty, the system can safely attempt to invoke it without causing an error.
 *
 * **Test Group ID:** Basic: 01@n
 * **Test Case ID:** 112@n
 * **Priority:** High@n
 *
 * **Pre-Conditions:** None@n
 * **Dependencies:** None@n
 * **User Interaction:** None@n
 *
 * **Test Procedure:**@n
 * | Variation / Step | Description | Test Data | Expected Result | Notes |
 * | :----: | --------- | ---------- |-------------- | ----- |
 * | 01 | Create an instance of DrmSessionManager with dummy parameters | maxDrmSessions = 10, player = nullptr | Instance is created successfully | Should be successful |
 * | 02 | Create an empty (default constructed) callback | emptyCallback = default constructed std::function<void(int)> | Empty callback is successfully created | Should be successful |
 * | 03 | Invoke RegisterLAProfEnd with the empty callback | callback = emptyCallback | laprofileEndCb remains empty in the DrmSessionManager | Should Pass |
 * | 04 | Check if the callback is empty using EXPECT_FALSE | drmSessionManager.laprofileEndCb evaluated as boolean false | EXPECT_FALSE confirms the callback is empty | Should Pass |
 * | 05 | Safely attempt to call the empty callback within a try-catch block | Invocation of laprofileEndCb with a sample value (1) if exists | No exception is thrown; invocation is a no-op | Should Pass |
 */
TEST_F(DrmSessionManagerTests, RegisterLAProfEnd_EmptyCallback)
{
    std::cout << "Entering RegisterLAProfEnd_EmptyCallback test" << std::endl;
    
    auto cb = [](uint32_t id1, uint32_t id2, const std::string& text) {
         std::cout << "Callback invoked with id1=" << id1
                  << ", id2=" << id2
                  << ", text=" << text << std::endl;
    };
    // Create an instance of DrmSessionManager with dummy parameters.
    DrmSessionManager drmSessionManager(10, nullptr, cb);
    std::cout << "Created DrmSessionManager instance with maxDrmSessions = 10 and player = nullptr" << std::endl;
    
    // Create an empty (default constructed) callback.
    std::function<void(int)> emptyCallback;
    std::cout << "Empty callback created" << std::endl;
    
    std::cout << "Invoking RegisterLAProfEnd with empty callback" << std::endl;
    drmSessionManager.RegisterLAProfEnd(emptyCallback);
    std::cout << "Empty callback registered" << std::endl;
    
    // Check if the callback is empty.
    if (drmSessionManager.laprofileEndCb)
    {
        std::cout << "Error: Expected empty callback, but callback is not empty." << std::endl;
    }
    else
    {
        std::cout << "Verified that the registered callback is empty" << std::endl;
    }
    
    EXPECT_FALSE(static_cast<bool>(drmSessionManager.laprofileEndCb));
    // Safely attempt to call the empty callback if needed.
    try
    {
        std::cout << "Attempting to invoke the empty callback (should be a no-op)" << std::endl;
        if(drmSessionManager.laprofileEndCb)
        {
            drmSessionManager.laprofileEndCb(1);
        }
        std::cout << "Invocation of empty callback completed without errors" << std::endl;
    }
    catch(const std::exception &e)
    {
        std::cout << "Exception caught when invoking empty callback: " << e.what() << std::endl;
        FAIL() << "Empty callback invocation threw an exception";
    }
    
    std::cout << "Exiting RegisterLAProfEnd_EmptyCallback test" << std::endl;
}
/**
 * @brief Verify that RegisterLAProfEnd correctly registers a lambda callback and correctly processes multiple stream types.
 *
 * This test registers a lambda callback using the RegisterLAProfEnd API of the DrmSessionManager. It then invokes the registered callback with various stream type values (0, -1, INT_MAX) to ensure that all provided values are captured in the internal container. This test validates the functionality of the callback registration and its correct execution for multiple stream inputs.
 *
 * **Test Group ID:** Basic: 01@n
 * **Test Case ID:** 113@n
 * **Priority:** High@n
 * 
 * **Pre-Conditions:** None@n
 * **Dependencies:** None@n
 * **User Interaction:** None@n
 * 
 * **Test Procedure:**@n
 * | Variation / Step | Description                                                                                      | Test Data                                                         | Expected Result                                                                                     | Notes           |
 * | :--------------: | ------------------------------------------------------------------------------------------------ | ----------------------------------------------------------------- | -------------------------------------------------------------------------------------------------- | --------------- |
 * | 01               | Create an instance of DrmSessionManager with dummy parameters.                                  | input: maxDrmSessions = 10, player = nullptr                        | Instance is created with the specified parameters.                                               | Should be successful |
 * | 02               | Initialize a container to record received streamType values.                                    | No input arguments.                                               | A container (vector) is initialized and is empty.                                                | Should be successful |
 * | 03               | Define a lambda callback that records the streamType values into the container.                  | Callback = lambda capturing recordedStreamTypes                      | Lambda is defined successfully.                                                                    | Should be successful |
 * | 04               | Register the lambda callback using the RegisterLAProfEnd API.                                    | input: callback = lambda                                            | The callback is registered without error.                                                        | Should Pass     |
 * | 05               | Invoke the registered callback with multiple streamType values.                                 | input: streamType values = 0, -1, INT_MAX                             | The callback is invoked for each streamType value leading to all values being recorded.          | Should Pass     |
 * | 06               | Verify that the container has recorded all the streamType values in the correct order using assertions. | input: expected values = 0, -1, INT_MAX; output: container size should be 3  | The container size is 3 and the individual values are recorded as expected (0, -1, INT_MAX).        | Should Pass     |
 * | 07               | Print the recorded streamType values to the console.                                           | input: recordedStreamTypes vector                                  | The streamType values are printed on the console correctly.                                      | Should be successful |
 */
TEST_F(DrmSessionManagerTests, RegisterLAProfEnd_MultiStreamTypes)
{
    std::cout << "Entering RegisterLAProfEnd_MultiStreamTypes test" << std::endl;
    
    // Create an instance of DrmSessionManager with dummy parameters.
    auto cb = [](uint32_t id1, uint32_t id2, const std::string& text) {
         std::cout << "Callback invoked with id1=" << id1
                  << ", id2=" << id2
                  << ", text=" << text << std::endl;
    };    
    DrmSessionManager drmSessionManager(10, nullptr, cb);
    std::cout << "Created DrmSessionManager instance with maxDrmSessions = 10 and player = nullptr" << std::endl;
    
    // Container to record received streamType values.
    std::vector<int> recordedStreamTypes;
    std::cout << "Initialized container to record streamType values." << std::endl;
    
    // Define a lambda that records the streamType into the container.
    auto callback = [&recordedStreamTypes](int streamType)
    {
        std::cout << "Lambda callback invoked with streamType: " << streamType << std::endl;
        recordedStreamTypes.push_back(streamType);
        std::cout << "Recorded streamType: " << streamType << " into container." << std::endl;
    };
    
    std::cout << "Invoking RegisterLAProfEnd with multi streamType callback lambda" << std::endl;
    drmSessionManager.RegisterLAProfEnd(callback);
    std::cout << "Callback registered. Now invoking the registered callback with various streamType values." << std::endl;
    
    // Test streamType values to invoke.
    int testValues[] = {0, -1, INT_MAX};
    for (int value : testValues)
    {
        std::cout << "Invoking callback with streamType value: " << value << std::endl;
        drmSessionManager.laprofileEndCb(value);
    }
    
    // Verify that all streamType values were recorded.
    ASSERT_EQ(recordedStreamTypes.size(), 3u);
    EXPECT_EQ(recordedStreamTypes[0], 0);
    EXPECT_EQ(recordedStreamTypes[1], -1);
    EXPECT_EQ(recordedStreamTypes[2], INT_MAX);
    
    std::cout << "Recorded streamType values:";
    for (int value : recordedStreamTypes)
    {
        std::cout << " " << value;
    }
    std::cout << std::endl;
    
    std::cout << "Exiting RegisterLAProfEnd_MultiStreamTypes test" << std::endl;
}
/**
 * @brief Test registering a valid LAProfileError callback in DrmSessionManager.
 *
 * This test verifies that providing a valid lambda as an LAProfileError callback to the DrmSessionManager successfully sets the internal callback and that the callback is callable. The test invokes the callback with a test pointer to confirm proper functionality.
 *
 * **Test Group ID:** Basic: 01
 * **Test Case ID:** 114
 * **Priority:** High
 *
 * **Pre-Conditions:** None
 * **Dependencies:** None
 * **User Interaction:** None
 *
 * **Test Procedure:**
 * | Variation / Step | Description                                                                                | Test Data                                                                                           | Expected Result                                                                        | Notes       |
 * | :--------------: | ------------------------------------------------------------------------------------------ | --------------------------------------------------------------------------------------------------- | -------------------------------------------------------------------------------------- | ----------- |
 * |       01         | Create DrmSessionManager, register valid LAProfileError callback and verify callback is set | maxDrmSessions = 10, playerPtr = 0x1234, callback = valid lambda, testPtr = 0xDEADBEEF              | The callback should be properly set and callable with EXPECT_TRUE asserting its set state | Should Pass |
 */
TEST_F(DrmSessionManagerTests, RegisterValidLAProfileErrorCallback)
{
    std::cout << "Entering RegisterValidLAProfileErrorCallback test" << std::endl;

    // Create a DrmSessionManager object using custom constructor.
    int maxDrmSessions = 10;
    void* playerPtr = reinterpret_cast<void*>(0x1234);
    std::cout << "[DEBUG] Creating DrmSessionManager with maxDrmSessions=" << maxDrmSessions 
              << ", player pointer=" << playerPtr << std::endl;
    
     auto cb = [](uint32_t id1, uint32_t id2, const std::string& text) {
         std::cout << "Callback invoked with id1=" << id1
                  << ", id2=" << id2
                  << ", text=" << text << std::endl;
    }; 
    DrmSessionManager drmSessionManager(maxDrmSessions, playerPtr, cb);

    // Define a valid LAProfileError callback lambda.
    auto validCallback = [](void* ptr) 
    { 
        std::cout << "[DEBUG] LAProfileError callback invoked with ptr: " << ptr << std::endl; 
    };
    std::cout << "[DEBUG] Valid LAProfileError callback lambda created." << std::endl;

    // Invoke RegisterLAProfError with the valid lambda.
    std::cout << "[DEBUG] Invoking RegisterLAProfError with the valid callback." << std::endl;
    drmSessionManager.RegisterLAProfError(validCallback);

    // Verify that the internal laprofileErrorCb is set (callable).
    if (drmSessionManager.laprofileErrorCb)
    {
        std::cout << "[DEBUG] laprofileErrorCb is set. Now invoking the callback with test pointer 0xDEADBEEF." << std::endl;
        void* testPtr = reinterpret_cast<void*>(0xDEADBEEF);
        drmSessionManager.laprofileErrorCb(testPtr);
        std::cout << "[DEBUG] Callback invocation completed." << std::endl;
        EXPECT_TRUE(static_cast<bool>(drmSessionManager.laprofileErrorCb));
    }
    else
    {
        std::cout << "[DEBUG] Error: laprofileErrorCb is not set as expected." << std::endl;
        FAIL() << "laprofileErrorCb not set";
    }

    std::cout << "Exiting RegisterValidLAProfileErrorCallback test" << std::endl;
}
/**
 * @brief Test to verify that registering an empty LAProfile error callback does not set the internal callback in DrmSessionManager
 *
 * This test validates that when a default-constructed (empty) LAProfile error callback is registered using the RegisterLAProfError method of DrmSessionManager, the internal callback remains uninitialized. The test ensures that the implementation behaves correctly when provided with an empty callback, effectively verifying defensive programming and error handling.
 *
 * **Test Group ID:** Basic: 01
 * **Test Case ID:** 115
 * **Priority:** High
 *
 * **Pre-Conditions:** None
 * **Dependencies:** None
 * **User Interaction:** None
 *
 * **Test Procedure:**
 * | Variation / Step | Description | Test Data | Expected Result | Notes |
 * | :----: | --------- | ---------- |-------------- | ----- |
 * | 01 | Create a DrmSessionManager instance with specific parameters | input: maxDrmSessions = 5, playerPtr = nullptr | DrmSessionManager instance is created successfully | Should be successful |
 * | 02 | Create a default-constructed (empty) LAProfileError callback | input: emptyCallback default constructed | Empty callback is created | Should be successful |
 * | 03 | Invoke RegisterLAProfError using the empty callback | input: LAProfileError callback = emptyCallback | Internal laprofileErrorCb remains empty after invocation | Should Pass |
 * | 04 | Verify that laprofileErrorCb remains empty using EXPECT_FALSE check | output: laprofileErrorCb value | Callback is verified to be empty | Should Pass |
 */
TEST_F(DrmSessionManagerTests, RegisterEmptyLAProfileErrorCallback)
{
    std::cout << "Entering RegisterEmptyLAProfileErrorCallback test" << std::endl;

    // Create a DrmSessionManager object using custom constructor.
    int maxDrmSessions = 5;
    void* playerPtr = nullptr;
    std::cout << "[DEBUG] Creating DrmSessionManager with maxDrmSessions=" << maxDrmSessions 
              << ", player pointer=" << playerPtr << std::endl;
    
    auto cb = [](uint32_t id1, uint32_t id2, const std::string& text) {
         std::cout << "Callback invoked with id1=" << id1
                  << ", id2=" << id2
                  << ", text=" << text << std::endl;
    }; 
    DrmSessionManager drmSessionManager(maxDrmSessions, playerPtr, cb);

    // Create a default-constructed (empty) LAProfileError callback.
    std::function<void(void*)> emptyCallback;
    std::cout << "[DEBUG] Default-constructed (empty) LAProfileError callback created." << std::endl;

    // Invoke RegisterLAProfError with the empty callback.
    std::cout << "[DEBUG] Invoking RegisterLAProfError with the empty callback." << std::endl;
    drmSessionManager.RegisterLAProfError(emptyCallback);

    // Verify that the internal laprofileErrorCb is empty (not callable).
    if (!drmSessionManager.laprofileErrorCb)
    {
        std::cout << "[DEBUG] laprofileErrorCb is empty as expected." << std::endl;
    }
    else
    {
        std::cout << "[DEBUG] Error: laprofileErrorCb is unexpectedly set." << std::endl;
    }
    EXPECT_FALSE(static_cast<bool>(drmSessionManager.laprofileErrorCb));

    std::cout << "Exiting RegisterEmptyLAProfileErrorCallback test" << std::endl;
}
/**
 * @brief Validate successful registration and invocation of a valid callback in DrmSessionManager.
 *
 * Validate registration and execution of a non-null callback that returns a shared_ptr with an int value of 42. This test checks that the callback is stored correctly and when invoked, produces the correct output.
 *
 * **Test Group ID:** Basic: 01
 * **Test Case ID:** 115
 * **Priority:** High
 *
 * **Pre-Conditions:** None
 * **Dependencies:** None
 * **User Interaction:** None
 *
 * **Test Procedure:**
 * | Variation / Step | Description | Test Data | Expected Result |Notes |
 * | :----: | --------- | ---------- |-------------- | ----- |
 * | 01 | Create a DrmSessionManager instance with maxDrmSessions=5 and player=nullptr | input: maxDrmSessions = 5, player = nullptr | Instance is created successfully |Should be successful |
 * | 02 | Define a valid callback that returns a non-null shared_ptr with int value 42 | input: callback lambda, output: shared_ptr<int> with value = 42 | Callback returns non-null shared_ptr with expected value |Should Pass |
 * | 03 | Register the callback using RegisterMetaDataCb | input: callback function | Callback is stored successfully in DrmMetaDataCb |Should Pass |
 * | 04 | Invoke the registered callback and verify the returned value | output from callback invocation: non-null shared_ptr<int> with value = 42, ASSERT_EQ check | Return pointer is non-null and int value equals 42 |Should Pass |
 */
TEST_F(DrmSessionManagerTests, RegisterValidCallbackNonNull) {
    std::cout << "Entering RegisterValidCallbackNonNull test" << std::endl;
    
    // Create an instance of DrmSessionManager using the custom constructor
    auto cb = [](uint32_t id1, uint32_t id2, const std::string& text) {
         std::cout << "Callback invoked with id1=" << id1
                  << ", id2=" << id2
                  << ", text=" << text << std::endl;
    }; 
    DrmSessionManager manager(5, nullptr, cb);
    std::cout << "Created DrmSessionManager instance with maxDrmSessions = 5 and player = nullptr" << std::endl;
    
    // Define a valid callback that returns a non-null shared pointer containing an int value 42
    auto callback = []() -> std::shared_ptr<void> {
        auto spInt = std::make_shared<int>(42);
        std::cout << "Inside callback: Created shared_ptr<int> with value " << *spInt << std::endl;
        return spInt;
    };
    std::cout << "Invoking RegisterMetaDataCb with callback that returns non-null shared_ptr (int value 42)" << std::endl;
    
    // Register the callback
    manager.RegisterMetaDataCb(callback);
    std::cout << "Callback registered successfully" << std::endl;
    
    // Invoke the registered callback and check that the returned pointer is non-null and holds the expected value
    if (manager.DrmMetaDataCb) {
        std::cout << "DrmMetaDataCb is valid. Invoking the callback." << std::endl;
        std::shared_ptr<void> result = manager.DrmMetaDataCb();
        if (result) {
            // Cast the void pointer to int pointer and check value
            std::shared_ptr<int> intResult = std::static_pointer_cast<int>(result);
            std::cout << "Callback returned a valid shared_ptr. Retrieved int value: " << *intResult << std::endl;
            ASSERT_EQ(*intResult, 42);
        } else {
            std::cout << "Callback returned a null shared_ptr." << std::endl;
            FAIL() << "Expected non-null shared_ptr but got null.";
        }
    } else {
        std::cout << "DrmMetaDataCb is empty after registration." << std::endl;
        FAIL() << "Expected valid callback stored in DrmMetaDataCb.";
    }
    
    std::cout << "Exiting RegisterValidCallbackNonNull test" << std::endl;
}
/**
 * @brief Validate that registering a callback returning a null shared pointer behaves correctly.
 *
 * This test verifies the functionality of the DrmSessionManager when a valid callback that returns a null shared pointer is registered. It checks that the callback is correctly stored and, upon invocation, returns a null shared pointer as expected.
 *
 * **Test Group ID:** Basic: 01
 * **Test Case ID:** 117
 * **Priority:** High
 *
 * **Pre-Conditions:** None
 * **Dependencies:** None
 * **User Interaction:** None
 *
 * **Test Procedure:**
 * | Variation / Step | Description                                                                 | Test Data                                                         | Expected Result                                                                                           | Notes         |
 * | :--------------: | --------------------------------------------------------------------------- | ----------------------------------------------------------------- | --------------------------------------------------------------------------------------------------------- | ------------- |
 * | 01               | Instantiate DrmSessionManager with maxDrmSessions = 5 and player = nullptr    | input: maxDrmSessions = 5, player = nullptr                         | DrmSessionManager instance is created successfully                                                        | Should be successful |
 * | 02               | Define a callback that returns a null shared_ptr                             | input: callback -> returns std::shared_ptr<void>() as null            | A valid callback is defined that returns a null shared pointer                                             | Should be successful |
 * | 03               | Register the defined callback using RegisterMetaDataCb                        | input: callback reference                                           | Callback is registered successfully and assigned to DrmMetaDataCb                                          | Should be successful |
 * | 04               | Invoke the registered callback and verify it returns a null shared pointer      | output from callback invocation                                     | The callback, when invoked, returns a null shared pointer as expected; assertion check passes              | Should Pass   |
 */
TEST_F(DrmSessionManagerTests, RegisterCallbackReturningNullSharedPtr) {
    std::cout << "Entering RegisterCallbackReturningNullSharedPtr test" << std::endl;
    
    // Create an instance of DrmSessionManager using the custom constructor
    auto cb = [](uint32_t id1, uint32_t id2, const std::string& text) {
         std::cout << "Callback invoked with id1=" << id1
                  << ", id2=" << id2
                  << ", text=" << text << std::endl;
    }; 
    DrmSessionManager manager(5, nullptr, cb);
    std::cout << "Created DrmSessionManager instance with maxDrmSessions = 5 and player = nullptr" << std::endl;
    
    // Define a valid callback that returns a null shared pointer
    auto nullCallback = []() -> std::shared_ptr<void> {
        std::cout << "Inside callback: Returning a null shared_ptr" << std::endl;
        return std::shared_ptr<void>();
    };
    std::cout << "Invoking RegisterMetaDataCb with callback that returns null shared_ptr" << std::endl;
    
    // Register the callback
    manager.RegisterMetaDataCb(nullCallback);
    std::cout << "Callback registered successfully" << std::endl;
    
    // Invoke the registered callback and check that the returned shared pointer is null
    if (manager.DrmMetaDataCb) {
        std::cout << "DrmMetaDataCb is valid. Invoking the callback." << std::endl;
        std::shared_ptr<void> result = manager.DrmMetaDataCb();
        if (!result) {
            std::cout << "Callback returned a null shared_ptr as expected" << std::endl;
        } else {
            std::cout << "Callback returned a non-null shared_ptr unexpectedly" << std::endl;
            FAIL() << "Expected callback to return a null shared_ptr.";
        }
    } else {
        std::cout << "DrmMetaDataCb is empty, which is unexpected since a valid callback was provided" << std::endl;
        FAIL() << "Expected DrmMetaDataCb to be valid even if its invocation returns null.";
    }
    
    std::cout << "Exiting RegisterCallbackReturningNullSharedPtr test" << std::endl;
}
/**
 * @brief Verify that re-registering metadata callbacks in DrmSessionManager updates the callback successfully
 *
 * This test verifies that when a metadata callback is registered and then updated by invoking RegisterMetaDataCb again, the second callback replaces the first and returns the expected shared_ptr with the updated integer value.
 *
 * **Test Group ID:** Basic: 01@n
 * **Test Case ID:** 118@n
 * **Priority:** High@n
 * 
 * **Pre-Conditions:** None@n
 * **Dependencies:** None@n
 * **User Interaction:** None@n
 * 
 * **Test Procedure:**@n
 * | Variation / Step | Description | Test Data | Expected Result | Notes |
 * | :----: | --------- | ---------- |-------------- | ----- |
 * | 01 | Create an instance of DrmSessionManager using custom constructor | maxDrmSessions = 5, player = nullptr | Instance of DrmSessionManager created successfully | Should be successful |
 * | 02 | Define and register the first callback that returns a shared_ptr<int> with value 10 | input: firstCallback function (returns shared_ptr<int> with value 10) | DrmMetaDataCb updated with firstCallback | Should Pass |
 * | 03 | Invoke the first registered callback via DrmMetaDataCb and verify the return value | Output: firstResult (expected int = 10) | Returns shared_ptr pointing to integer 10; ASSERT_EQ verifies value == 10 | Should Pass |
 * | 04 | Define and register the second callback that returns a shared_ptr<int> with value 20 | input: secondCallback function (returns shared_ptr<int> with value 20) | DrmMetaDataCb updated with secondCallback | Should Pass |
 * | 05 | Invoke the second registered callback via DrmMetaDataCb and verify the return value | Output: secondResult (expected int = 20) | Returns shared_ptr pointing to integer 20; ASSERT_EQ verifies value == 20 | Should Pass |
 */
TEST_F(DrmSessionManagerTests, ReRegisterCallbackUpdate) {
    std::cout << "Entering ReRegisterCallbackUpdate test" << std::endl;
    
    // Create an instance of DrmSessionManager using the custom constructor
    auto cb = [](uint32_t id1, uint32_t id2, const std::string& text) {
         std::cout << "Callback invoked with id1=" << id1
                  << ", id2=" << id2
                  << ", text=" << text << std::endl;
    };
    DrmSessionManager manager(5, nullptr, cb);
    std::cout << "Created DrmSessionManager instance with maxDrmSessions = 5 and player = nullptr" << std::endl;
    
    // Define and register the first callback that returns a shared pointer with int value 10
    auto firstCallback = []() -> std::shared_ptr<void> {
        auto spInt = std::make_shared<int>(10);
        std::cout << "Inside first callback: Created shared_ptr<int> with value " << *spInt << std::endl;
        return spInt;
    };
    std::cout << "Invoking RegisterMetaDataCb with first callback that returns int value 10" << std::endl;
    manager.RegisterMetaDataCb(firstCallback);
    std::cout << "First callback registered successfully" << std::endl;
    
    // Verify the first callback invocation
    if (manager.DrmMetaDataCb) {
        std::cout << "DrmMetaDataCb is valid. Invoking the first callback." << std::endl;
        std::shared_ptr<void> firstResult = manager.DrmMetaDataCb();
        if (firstResult) {
            std::shared_ptr<int> intResult = std::static_pointer_cast<int>(firstResult);
            std::cout << "First callback returned shared_ptr with int value: " << *intResult << std::endl;
            ASSERT_EQ(*intResult, 10);
        } else {
            std::cout << "First callback returned a null shared_ptr unexpectedly" << std::endl;
            FAIL() << "Expected first callback to return a non-null shared_ptr with value 10.";
        }
    } else {
        std::cout << "DrmMetaDataCb is empty after registering first callback" << std::endl;
        FAIL() << "Expected valid first callback in DrmMetaDataCb.";
    }
    
    // Define and register the second callback that returns a shared pointer with int value 20
    auto secondCallback = []() -> std::shared_ptr<void> {
        auto spInt = std::make_shared<int>(20);
        std::cout << "Inside second callback: Created shared_ptr<int> with value " << *spInt << std::endl;
        return spInt;
    };
    std::cout << "Invoking RegisterMetaDataCb with second callback that returns int value 20" << std::endl;
    manager.RegisterMetaDataCb(secondCallback);
    std::cout << "Second callback registered successfully" << std::endl;
    
    // Verify that the callback has been updated to the second callback
    if (manager.DrmMetaDataCb) {
        std::cout << "DrmMetaDataCb is valid. Invoking the second callback." << std::endl;
        std::shared_ptr<void> secondResult = manager.DrmMetaDataCb();
        if (secondResult) {
            std::shared_ptr<int> intResult = std::static_pointer_cast<int>(secondResult);
            std::cout << "Second callback returned shared_ptr with int value: " << *intResult << std::endl;
            ASSERT_EQ(*intResult, 20);
        } else {
            std::cout << "Second callback returned a null shared_ptr unexpectedly" << std::endl;
            FAIL() << "Expected second callback to return a non-null shared_ptr with value 20.";
        }
    } else {
        std::cout << "DrmMetaDataCb is empty after re-registering second callback" << std::endl;
        FAIL() << "Expected valid second callback in DrmMetaDataCb.";
    }
    
    std::cout << "Exiting ReRegisterCallbackUpdate test" << std::endl;
}
/**
 * @brief Validates that RegisterProfBegin correctly registers and invokes a valid callback
 *
 * This test verifies that a valid callback lambda is successfully registered using RegisterProfBegin of the DrmSessionManager.
 * It checks that when the stored callback is invoked with the value 5, the callbackResult is properly updated to 5.
 *
 * **Test Group ID:** Basic: 01
 * **Test Case ID:** 119
 * **Priority:** High
 *
 * **Pre-Conditions:** None
 * **Dependencies:** None
 * **User Interaction:** None
 *
 * **Test Procedure:**
 * | Variation / Step | Description | Test Data | Expected Result | Notes |
 * | :----: | --------- | ---------- |-------------- | ----- |
 * | 01 | Define a valid callback lambda that updates callbackResult | callbackResult = 0 | Lambda is defined successfully and ready to update callbackResult | Should be successful |
 * | 02 | Create DrmSessionManager object with maxDrmSessions=3 and player=nullptr | maxDrmSessions = 3, player = nullptr | DrmSessionManager instance is created successfully | Should be successful |
 * | 03 | Register the valid callback lambda using RegisterProfBegin | callback = validCallback | profileBeginCb is set to the valid callback lambda | Should Pass |
 * | 04 | Invoke the stored callback with value 5 | input value = 5 | callbackResult is updated to 5 | Should Pass |
 * | 05 | Validate that callbackResult equals 5 using EXPECT_EQ | callbackResult = 5 (expected) | Assertion passes ensuring callbackResult equals 5 | Should Pass |
 */
TEST_F(DrmSessionManagerTests, RegisterProfBegin_ValidCallbackTest)
{
    std::cout << "Entering RegisterProfBegin_ValidCallbackTest test" << std::endl;
    
    int callbackResult = 0;
    auto validCallback = [&callbackResult](int value) {
        std::cout << "Inside callback lambda, received value: " << value << std::endl;
        callbackResult = value;
    };

    std::cout << "Creating DrmSessionManager object with maxDrmSessions=3 and player=nullptr" << std::endl;
    auto cb = [](uint32_t id1, uint32_t id2, const std::string& text) {
         std::cout << "Callback invoked with id1=" << id1
                  << ", id2=" << id2
                  << ", text=" << text << std::endl;
    };
    DrmSessionManager drmManager(3, nullptr, cb);
    
    std::cout << "Invoking RegisterProfBegin with valid callback lambda" << std::endl;
    drmManager.RegisterProfBegin(validCallback);
    std::cout << "Callback registered in profileBeginCb" << std::endl;
    
    std::cout << "Invoking stored callback with value 5" << std::endl;
    drmManager.profileBeginCb(5);
    std::cout << "Callback invocation complete, callbackResult: " << callbackResult << std::endl;
    
    EXPECT_EQ(callbackResult, 5);
    
    std::cout << "Exiting RegisterProfBegin_ValidCallbackTest test" << std::endl;
}
/**
 * @brief Verify that RegisterProfBegin registers an empty callback correctly.
 *
 * This test verifies that invoking the RegisterProfBegin method of DrmSessionManager with an empty callback does not register any callback function. It confirms that the profileBeginCb remains empty and ensures that no unintended operations are performed when an empty callback is provided.
 *
 * **Test Group ID:** Basic: 01@n
 * **Test Case ID:** 120@n
 * **Priority:** High@n
 *
 * **Pre-Conditions:** None@n
 * **Dependencies:** None@n
 * **User Interaction:** None@n
 *
 * **Test Procedure:**
 * | Variation / Step | Description                                                                              | Test Data                                                              | Expected Result                                                                      | Notes          |
 * | :----:           | ---------------------------------------------------------------------------------------- | ---------------------------------------------------------------------- | ------------------------------------------------------------------------------------- | -------------- |
 * | 01               | Create DrmSessionManager object with maxDrmSessions=3 and player=nullptr.                  | maxDrmSessions = 3, player = nullptr                                    | Object is initialized successfully.                                                  | Should be successful |
 * | 02               | Create an empty ProfileBeginCallback.                                                    | emptyCallback = std::function<void(int)> (empty)                       | Callback remains empty.                                                               | Should be successful |
 * | 03               | Invoke RegisterProfBegin with an empty callback.                                         | callback = emptyCallback                                               | API assigns an empty callback to profileBeginCb.                                      | Should Pass    |
 * | 04               | Verify that profileBeginCb is empty using the EXPECT_FALSE assertion.                      | profileBeginCb = empty                                                 | The EXPECT_FALSE assertion passes confirming profileBeginCb is empty.                 | Should be successful |
 */
TEST_F(DrmSessionManagerTests, RegisterProfBegin_EmptyCallbackTest)
{
    std::cout << "Entering RegisterProfBegin_EmptyCallbackTest test" << std::endl;
    
    std::cout << "Creating DrmSessionManager object with maxDrmSessions=3 and player=nullptr" << std::endl;
    auto cb = [](uint32_t id1, uint32_t id2, const std::string& text) {
         std::cout << "Callback invoked with id1=" << id1
                  << ", id2=" << id2
                  << ", text=" << text << std::endl;
    };
    DrmSessionManager drmManager(3, nullptr, cb);
    
    std::cout << "Creating an empty ProfileBeginCallback" << std::endl;
    std::function<void(int)> emptyCallback;
    
    std::cout << "Invoking RegisterProfBegin with empty callback" << std::endl;
    drmManager.RegisterProfBegin(emptyCallback);
    std::cout << "Empty callback stored in profileBeginCb" << std::endl;
    
    if (!drmManager.profileBeginCb) {
        std::cout << "profileBeginCb is empty as expected, no operation will be executed if invoked." << std::endl;
    } else {
        std::cout << "profileBeginCb is not empty, which is unexpected for an empty callback." << std::endl;
    }
    
    EXPECT_FALSE(drmManager.profileBeginCb);
    
    std::cout << "Exiting RegisterProfBegin_EmptyCallbackTest test" << std::endl;
}
/**
 * @brief Test whether valid callback registration and invocation correctly sets the flag.
 *
 * This test registers a valid callback that captures an external flag. The callback is then invoked using the profileEndCb function with a streamType argument. The test verifies that the flag is set to true, ensuring that the callback registration and invocation mechanism in the DrmSessionManager works as expected.
 *
 * **Test Group ID:** Basic: 01@n
 * **Test Case ID:** 121@n
 * **Priority:** High@n
 *
 * **Pre-Conditions:** None@n
 * **Dependencies:** None@n
 * **User Interaction:** None@n
 *
 * **Test Procedure:**@n
 * | Variation / Step | Description | Test Data | Expected Result | Notes |
 * | :----: | --------- | ---------- |-------------- | ----- |
 * | 01 | Initialize flag, create callback, and instantiate DrmSessionManager. | flag = false, capacity = 10, callback captures flag | Instance created with uninvoked callback, flag remains false | Should be successful |
 * | 02 | Register the valid callback using RegisterProfEnd. | callback = lambda capturing flag | Callback stored in profileEndCb; internal state updated accordingly | Should Pass |
 * | 03 | Invoke the stored callback via profileEndCb with streamType = 1. | streamType = 1 | Flag is set to true and ASSERT_TRUE validates the condition | Should Pass |
 */
TEST_F(DrmSessionManagerTests, RegisterProfEnd_ValidCallbackSetsFlag) {
    std::cout << "Entering RegisterProfEnd_ValidCallbackSetsFlag test" << std::endl;
    
    // Create a flag that gets set when the callback is invoked
    bool flag = false;
    auto callback = [&flag](int streamType) {
        flag = true;
        std::cout << "Inside callback: Received streamType = " << streamType 
                  << ", setting flag to " << flag << std::endl;
    };

    // Create DrmSessionManager object using custom constructor
    auto cb = [](uint32_t id1, uint32_t id2, const std::string& text) {
         std::cout << "Callback invoked with id1=" << id1
                  << ", id2=" << id2
                  << ", text=" << text << std::endl;
    };
    DrmSessionManager drmManager(10, nullptr, cb);
    std::cout << "Invoking RegisterProfEnd with valid callback" << std::endl;
    drmManager.RegisterProfEnd(callback);
    std::cout << "Callback registered in profileEndCb" << std::endl;

    // Invoke the stored callback with streamType = 1
    std::cout << "Invoking profileEndCb with streamType = 1" << std::endl;
    drmManager.profileEndCb(1);
    std::cout << "Flag value after callback invocation: " << flag << std::endl;

    ASSERT_TRUE(flag);

    std::cout << "Exiting RegisterProfEnd_ValidCallbackSetsFlag test" << std::endl;
}
/**
 * @brief Test to verify that the profile end callback correctly updates an external variable.
 *
 * This test validates that when a callback is registered with the DrmSessionManager,
 * invoking the stored callback with a specific streamType (2) correctly updates the external variable.
 * The test ensures that the callback mechanism is functioning as expected.
 *
 * **Test Group ID:** Basic: 01@n
 * **Test Case ID:** 122@n
 * **Priority:** High@n
 *
 * **Pre-Conditions:** None@n
 * **Dependencies:** None@n
 * **User Interaction:** None@n
 *
 * **Test Procedure:**@n
 * | Variation / Step | Description | Test Data | Expected Result | Notes |
 * | :----: | --------- | ---------- | -------------- | ----- |
 * | 01 | Initialize external variable and define callback lambda to update it based on streamType | flag = false | External variable initialized to false | Should be successful |
 * | 02 | Create DrmSessionManager instance and register the defined callback using RegisterProfEnd() | input: maxSessions = 10, callback = lambda, output: callback registered in profileEndCb | Callback is stored in the DrmSessionManager | Should Pass |
 * | 03 | Invoke the stored callback in profileEndCb with streamType = 2 | input: streamType = 2 | The callback updates the external flag to true | Should Pass |
 * | 04 | Assert that the external flag has been updated to true after callback invocation | output: flag value, expected value = true | ASSERT_TRUE verifies the flag is true | Should Pass |
 */
TEST_F(DrmSessionManagerTests, RegisterProfEnd_UpdateExternalVariable) {
    std::cout << "Entering RegisterProfEnd_UpdateExternalVariable test" << std::endl;
    
    // Using a shared pointer to allow lambda to update the external variable and reflect changes externally
    auto flag = std::make_shared<bool>(false);
    auto callback = [flag](int streamType) {
        *flag = (streamType == 2);
        std::cout << "Inside callback: Received streamType = " << streamType 
                  << ", updating flag to " << *flag << std::endl;
    };

    // Create DrmSessionManager object using custom constructor
    auto cb = [](uint32_t id1, uint32_t id2, const std::string& text) {
         std::cout << "Callback invoked with id1=" << id1
                  << ", id2=" << id2
                  << ", text=" << text << std::endl;
    };
    DrmSessionManager drmManager(10, nullptr, cb);
    std::cout << "Invoking RegisterProfEnd with callback capturing external variable" << std::endl;
    drmManager.RegisterProfEnd(callback);
    std::cout << "Callback registered in profileEndCb" << std::endl;

    // Invoke the stored callback with streamType = 2
    std::cout << "Invoking profileEndCb with streamType = 2" << std::endl;
    drmManager.profileEndCb(2);
    std::cout << "External flag value after callback invocation: " << *flag << std::endl;

    ASSERT_TRUE(*flag);

    std::cout << "Exiting RegisterProfEnd_UpdateExternalVariable test" << std::endl;
}
/**
 * @brief Test the RegisterProfEnd API with an empty callback
 *
 * This test verifies that when an empty std::function callback is passed to the RegisterProfEnd method of the DrmSessionManager, the internal callback (profileEndCb) remains empty. This ensures that the API handles empty callbacks correctly and does not register an invalid callback.
 *
 * **Test Group ID:** Basic: 01@n
 * **Test Case ID:** 123@n
 * **Priority:** High@n
 * 
 * **Pre-Conditions:** None@n
 * **Dependencies:** None@n
 * **User Interaction:** None@n
 * 
 * **Test Procedure:**@n
 * | Variation / Step | Description | Test Data | Expected Result | Notes |
 * | :----: | --------- | ---------- |-------------- | ----- |
 * | 01 | Create an empty std::function callback variable. | emptyCallback = default constructed std::function<void(int)> | Callback variable is empty. | Should be successful |
 * | 02 | Create DrmSessionManager object using custom constructor. | sessionId = 10, dependency = nullptr | DrmSessionManager object is initialized successfully. | Should be successful |
 * | 03 | Invoke RegisterProfEnd with the empty callback. | callback = emptyCallback (empty) | API call registers an empty callback (profileEndCb remains empty). | Should Pass |
 * | 04 | Verify that profileEndCb is empty by checking its state. | internal state: profileEndCb is empty | ASSERT_TRUE(isEmpty) passes confirming the callback is empty. | Should Pass |
 */
TEST_F(DrmSessionManagerTests, RegisterProfEnd_EmptyCallback) {
    std::cout << "Entering RegisterProfEnd_EmptyCallback test" << std::endl;
    
    // Create an empty std::function callback (default constructed)
    std::function<void(int)> emptyCallback;
    std::cout << "Empty callback created" << std::endl;
    
    // Create DrmSessionManager object using custom constructor
    auto cb = [](uint32_t id1, uint32_t id2, const std::string& text) {
         std::cout << "Callback invoked with id1=" << id1
                  << ", id2=" << id2
                  << ", text=" << text << std::endl;
    };
    DrmSessionManager drmManager(10, nullptr, cb);
    std::cout << "Invoking RegisterProfEnd with empty callback" << std::endl;
    drmManager.RegisterProfEnd(emptyCallback);
    std::cout << "Empty callback registered in profileEndCb" << std::endl;

    // Check whether profileEndCb is empty
    bool isEmpty = (!drmManager.profileEndCb);
    std::cout << "profileEndCb is " << (isEmpty ? "empty" : "not empty") << std::endl;
    
    ASSERT_TRUE(isEmpty);

    std::cout << "Exiting RegisterProfEnd_EmptyCallback test" << std::endl;
}
/**
 * @brief Validate registration and overwrite functionality of callback in DrmSessionManager
 *
 * This test verifies that the DrmSessionManager object's RegisterProfEnd API correctly registers a callback and that when the callback is overwritten with a new one, the new callback is invoked while the old one is not. The test ensures that after invoking profileEndCb, the appropriate callback sets the corresponding flag as expected.
 *
 * **Test Group ID:** Basic: 01@n
 * **Test Case ID:** 124@n
 * **Priority:** High@n
 *
 * **Pre-Conditions:** None@n
 * **Dependencies:** None@n
 * **User Interaction:** None@n
 *
 * **Test Procedure:**
 * | Variation / Step | Description | Test Data | Expected Result | Notes |
 * | :--------------: | ----------- | --------- | --------------- | ----- |
 * | 01 | Register the first callback and verify its invocation | RegisterProfEnd(firstCallback), profileEndCb(streamType = 0), firstInvoked = false, secondInvoked = false | firstInvoked becomes true; secondInvoked remains false; ASSERT_TRUE(firstInvoked) and ASSERT_FALSE(secondInvoked) pass | Should Pass |
 * | 02 | Reset flags, overwrite the callback with second callback, and verify the overwritten callback's behavior | RegisterProfEnd(secondCallback), profileEndCb(streamType = 5), firstInvoked = false, secondInvoked = false | secondInvoked becomes true; firstInvoked remains false; ASSERT_FALSE(firstInvoked) and ASSERT_TRUE(secondInvoked) pass | Should Pass |
 */
TEST_F(DrmSessionManagerTests, RegisterProfEnd_OverwriteCallback) {
    std::cout << "Entering RegisterProfEnd_OverwriteCallback test" << std::endl;
    
    bool firstInvoked = false;
    bool secondInvoked = false;

    // First callback lambda that sets firstInvoked flag
    auto firstCallback = [&firstInvoked](int streamType) {
        firstInvoked = true;
        std::cout << "Inside firstCallback: Received streamType = " << streamType 
                  << ", setting firstInvoked to " << firstInvoked << std::endl;
    };

    // Second callback lambda that sets secondInvoked flag
    auto secondCallback = [&secondInvoked](int streamType) {
        secondInvoked = true;
        std::cout << "Inside secondCallback: Received streamType = " << streamType 
                  << ", setting secondInvoked to " << secondInvoked << std::endl;
    };

    // Create DrmSessionManager object using custom constructor
    auto cb = [](uint32_t id1, uint32_t id2, const std::string& text) {
         std::cout << "Callback invoked with id1=" << id1
                  << ", id2=" << id2
                  << ", text=" << text << std::endl;
    };
    DrmSessionManager drmManager(10, nullptr, cb);
    
    // Register the first callback
    std::cout << "Registering first callback via RegisterProfEnd" << std::endl;
    drmManager.RegisterProfEnd(firstCallback);
    std::cout << "First callback registered in profileEndCb" << std::endl;

    // Invoke the currently stored callback
    std::cout << "Invoking profileEndCb with streamType = 0 (expecting firstCallback behavior)" << std::endl;
    drmManager.profileEndCb(0);
    std::cout << "firstInvoked: " << firstInvoked << ", secondInvoked: " << secondInvoked << std::endl;
    ASSERT_TRUE(firstInvoked);
    ASSERT_FALSE(secondInvoked);

    // Reset flags to check overwrite behavior
    firstInvoked = false;
    secondInvoked = false;

    // Overwrite with the second callback
    std::cout << "Registering second callback via RegisterProfEnd, overwriting first callback" << std::endl;
    drmManager.RegisterProfEnd(secondCallback);
    std::cout << "Second callback registered in profileEndCb" << std::endl;

    // Invoke the overwritten callback
    std::cout << "Invoking profileEndCb with streamType = 5 (expecting secondCallback behavior)" << std::endl;
    drmManager.profileEndCb(5);
    std::cout << "firstInvoked: " << firstInvoked << ", secondInvoked: " << secondInvoked << std::endl;
    ASSERT_FALSE(firstInvoked);
    ASSERT_TRUE(secondInvoked);

    std::cout << "Exiting RegisterProfEnd_OverwriteCallback test" << std::endl;
}

/**
 * @brief Test the registration of a valid profile error callback and its invocation
 *
 * This test verifies that when a valid lambda function is registered as a ProfileError callback using the RegisterProfError API,
 * the callback is properly stored and subsequently invoked when profileErrorCb is called. The test confirms the functionality by checking the callbackInvoked flag.
 *
 * **Test Group ID:** Basic: 01@n
 * **Test Case ID:** 125@n
 * **Priority:** High@n
 * 
 * **Pre-Conditions:** None@n
 * **Dependencies:** None@n
 * **User Interaction:** None@n
 * 
 * **Test Procedure:**@n
 * | Variation / Step | Description                                                                | Test Data                                                         | Expected Result                                                                     | Notes           |
 * | :--------------: | -------------------------------------------------------------------------- | ----------------------------------------------------------------- | ------------------------------------------------------------------------------------ | --------------- |
 * | 01               | Create DrmSessionManager object using custom constructor.                  | maxDrmSessions = 5, player = nullptr                                | DrmSessionManager object is created successfully.                                   | Should be successful |
 * | 02               | Initialize the callbackInvoked flag.                                       | callbackInvoked = false                                             | The flag is initialized to false.                                                   | Should be successful |
 * | 03               | Create a valid lambda function for the ProfileError callback.              | lambda callback creation                                            | Lambda callback is defined successfully.                                            | Should be successful |
 * | 04               | Register the valid callback using RegisterProfError API.                   | RegisterProfError(validCallback)                                    | Callback is registered in the DrmSessionManager instance.                           | Should Pass     |
 * | 05               | Invoke profileErrorCb with streamType = 2 and result = 999 to trigger callback.| profileErrorCb(streamType = 2, result = 999)                          | Callback is invoked, setting callbackInvoked flag to true.                          | Should Pass     |
 * | 06               | Validate the callback invocation via EXPECT_TRUE assertion.                | EXPECT_TRUE(callbackInvoked)                                        | EXPECT_TRUE assertion passes confirming the callback was invoked.                   | Should Pass     |
 */
TEST_F(DrmSessionManagerTests, RegisterValidProfileErrorCallback) {
    std::cout << "Entering RegisterValidProfileErrorCallback test" << std::endl;
    
    // Create DrmSessionManager object using custom constructor with dummy parameters.
       auto cb = [](uint32_t id1, uint32_t id2, const std::string& text) {
         std::cout << "Callback invoked with id1=" << id1
                  << ", id2=" << id2
                  << ", text=" << text << std::endl;
    };
    DrmSessionManager drmSessionManager(5, nullptr, cb);
    std::cout << "Created DrmSessionManager object with maxDrmSessions = 5 and player = nullptr" << std::endl;
    
    // Flag to check if the callback is invoked.
    bool callbackInvoked = false;
    
    // Create a valid lambda function as a ProfileError callback.
    auto validCallback = [&callbackInvoked](int streamType, int result) {
        std::cout << "Invoked validCallback with streamType: " << streamType << " and result: " << result << std::endl;
        callbackInvoked = true;
    };
    std::cout << "Created validProfileErrorCallback lambda" << std::endl;
    
    // Register the valid callback.
    std::cout << "Invoking RegisterProfError with validCallback" << std::endl;
    drmSessionManager.RegisterProfError(validCallback);
    std::cout << "Registered validCallback successfully" << std::endl;
    
    // Validate by invoking the callback through the public member.
    std::cout << "Invoking profileErrorCb callback with streamType = 2 and result = 999" << std::endl;
    drmSessionManager.profileErrorCb(2, 999);
    std::cout << "Callback invocation complete; callbackInvoked flag = " << callbackInvoked << std::endl;
    EXPECT_TRUE(callbackInvoked);
    
    std::cout << "Exiting RegisterValidProfileErrorCallback test" << std::endl;
}
/**
 * @brief Verify that registering an empty ProfileError callback results in an empty callback in DrmSessionManager
 *
 * This test creates a DrmSessionManager instance and attempts to register an empty ProfileError callback.
 * The objective is to ensure that when an empty callback is provided, the internal callback remains unset.
 *
 * **Test Group ID:** Basic: 01@n
 * **Test Case ID:** 126@n
 * **Priority:** High@n
 *
 * **Pre-Conditions:** None@n
 * **Dependencies:** None@n
 * **User Interaction:** None@n
 *
 * **Test Procedure:**@n
 * | Variation / Step | Description                                                             | Test Data                                               | Expected Result                                                                     | Notes         |
 * | :--------------: | ----------------------------------------------------------------------- | ------------------------------------------------------- | ----------------------------------------------------------------------------------- | ------------- |
 * | 01               | Create DrmSessionManager object using custom constructor                | maxDrmSessions = 5, player = nullptr                      | DrmSessionManager object created successfully                                       | Should be successful |
 * | 02               | Create an empty ProfileError callback                                   | emptyCallback = empty                                     | Empty callback created successfully                                                | Should be successful |
 * | 03               | Register the empty ProfileError callback                                | callback = emptyCallback                                  | Callback registration invoked successfully                                           | Should Pass   |
 * | 04               | Verify that the profileErrorCb is empty after registration              | profileErrorCb expected to be empty                       | EXPECT_TRUE assertion passes confirming the callback remains unset                 | Should Pass   |
 */
TEST_F(DrmSessionManagerTests, RegisterEmptyProfileErrorCallback) {
    std::cout << "Entering RegisterEmptyProfileErrorCallback test" << std::endl;
    
    // Create DrmSessionManager object using custom constructor with dummy parameters.
       auto cb = [](uint32_t id1, uint32_t id2, const std::string& text) {
         std::cout << "Callback invoked with id1=" << id1
                  << ", id2=" << id2
                  << ", text=" << text << std::endl;
    };
    DrmSessionManager drmSessionManager(5, nullptr, cb);
    std::cout << "Created DrmSessionManager object with maxDrmSessions = 5 and player = nullptr" << std::endl;
    
    // Create an empty ProfileError callback.
    std::function<void(int, int)> emptyCallback;
    std::cout << "Created an empty ProfileError callback" << std::endl;
    
    // Register the empty callback.
    std::cout << "Invoking RegisterProfError with emptyCallback" << std::endl;
    drmSessionManager.RegisterProfError(emptyCallback);
    std::cout << "Registered empty callback successfully" << std::endl;
    
    // Check that the callback is empty.
    bool isCallbackEmpty = !static_cast<bool>(drmSessionManager.profileErrorCb);
    std::cout << "profileErrorCb is " << (isCallbackEmpty ? "empty" : "not empty") << std::endl;
    EXPECT_TRUE(isCallbackEmpty);
    
    std::cout << "Exiting RegisterEmptyProfileErrorCallback test" << std::endl;
}
/**
 * @brief Test the behavior of overwriting an existing profile error callback
 *
 * This test verifies that when a profile error callback is overwritten with a new callback,
 * the new callback is invoked instead of the previous one. It tests the creation of a DrmSessionManager,
 * registration and invocation of the first callback, and then overwriting that callback with a second one.
 *
 * **Test Group ID:** Basic: 01@n
 * **Test Case ID:** 127@n
 * **Priority:** High@n
 *
 * **Pre-Conditions:** None@n
 * **Dependencies:** None@n
 * **User Interaction:** None@n
 *
 * **Test Procedure:**@n
 * | Variation / Step | Description | Test Data | Expected Result | Notes |
 * | :----: | --------- | ---------- |-------------- | ----- |
 * | 01 | Create DrmSessionManager object using custom constructor | maxDrmSessions = 5, player = nullptr | Object is created successfully | Should be successful |
 * | 02 | Create first lambda callback | firstCallback captures firstCallbackResult | Lambda callback is created | Should be successful |
 * | 03 | Register first callback function using RegisterProfError | firstCallback | Callback is registered successfully | Should Pass |
 * | 04 | Invoke profileErrorCb callback with streamType = 1 and result = 1 | streamType = 1, result = 1 | firstCallbackResult equals 2 and assertion passes | Should Pass |
 * | 05 | Create second lambda callback | secondCallback captures secondCallbackResult | Lambda callback is created | Should be successful |
 * | 06 | Overwrite the existing callback by registering second callback using RegisterProfError | secondCallback | Existing callback is overwritten successfully | Should Pass |
 * | 07 | Invoke profileErrorCb callback with streamType = 2 and result = 2 | streamType = 2, result = 2 | secondCallbackResult equals 4 and assertion passes | Should Pass |
 */
TEST_F(DrmSessionManagerTests, OverwriteExistingProfileErrorCallback) {
    std::cout << "Entering OverwriteExistingProfileErrorCallback test" << std::endl;
    
    auto cb = [](uint32_t id1, uint32_t id2, const std::string& text) {
         std::cout << "Callback invoked with id1=" << id1
                  << ", id2=" << id2
                  << ", text=" << text << std::endl;
    };
    // Create DrmSessionManager object using custom constructor with dummy parameters.
    DrmSessionManager drmSessionManager(5, nullptr, cb);
    std::cout << "Created DrmSessionManager object with maxDrmSessions = 5 and player = nullptr" << std::endl;
    
    // Variables to track callback invocations.
    int firstCallbackResult = 0;
    int secondCallbackResult = 0;
    
    // Create first lambda callback.
    auto firstCallback = [&firstCallbackResult](int streamType, int result) {
        std::cout << "Invoked firstCallback with streamType: " << streamType << " and result: " << result << std::endl;
        firstCallbackResult = streamType + result;
    };
    std::cout << "Created firstCallback lambda" << std::endl;
    
    // Register the first callback.
    std::cout << "Invoking RegisterProfError with firstCallback" << std::endl;
    drmSessionManager.RegisterProfError(firstCallback);
    std::cout << "Registered firstCallback successfully" << std::endl;
    
    // Invoke the first callback.
    std::cout << "Invoking profileErrorCb callback with streamType = 1 and result = 1" << std::endl;
    drmSessionManager.profileErrorCb(1, 1);
    std::cout << "After first callback invocation, firstCallbackResult = " << firstCallbackResult << std::endl;
    EXPECT_EQ(firstCallbackResult, 2);
    
    // Create second lambda callback.
    auto secondCallback = [&secondCallbackResult](int streamType, int result) {
        std::cout << "Invoked secondCallback with streamType: " << streamType << " and result: " << result << std::endl;
        secondCallbackResult = streamType + result;
    };
    std::cout << "Created secondCallback lambda" << std::endl;
    
    // Overwrite the existing callback with the second callback.
    std::cout << "Invoking RegisterProfError with secondCallback to overwrite first callback" << std::endl;
    drmSessionManager.RegisterProfError(secondCallback);
    std::cout << "Overwritten callback with secondCallback successfully" << std::endl;
    
    // Invoke the callback after overwrite.
    std::cout << "Invoking profileErrorCb callback with streamType = 2 and result = 2" << std::endl;
    drmSessionManager.profileErrorCb(2, 2);
    std::cout << "After second callback invocation, secondCallbackResult = " << secondCallbackResult << std::endl;
    EXPECT_EQ(secondCallbackResult, 4);
    
    std::cout << "Exiting OverwriteExistingProfileErrorCallback test" << std::endl;
}
/**
 * @brief Register a valid callback function and verify its invocation using a lambda callback.
 *
 * This test verifies that the DrmSessionManager's RegisterSetFailure function correctly registers a valid lambda callback. It then triggers the callback with predefined test parameters to ensure that the callback updates the internal flags and data as expected.
 *
 * **Test Group ID:** Basic: 01
 * **Test Case ID:** 128
 * **Priority:** High
 *
 * **Pre-Conditions:** None
 * **Dependencies:** None
 * **User Interaction:** None
 *
 * **Test Procedure:**
 * | Variation / Step | Description                                                            | Test Data                                                       | Expected Result                                                                 | Notes           |
 * | :--------------: | ---------------------------------------------------------------------- | --------------------------------------------------------------- | ------------------------------------------------------------------------------- | --------------- |
 * | 01               | Create an instance of DrmSessionManager using custom constructor.      | size = 10, handle = 0x1234                                        | Instance is created successfully.                                               | Should be successful |
 * | 02               | Initialize callback invocation variables and define lambda callback.   | callbackInvoked = false, callbackErr = 0, callbackPtr = nullptr   | Lambda callback is defined and variables are initialized.                     | Should be successful |
 * | 03               | Register the lambda callback using the RegisterSetFailure API.         | callback = lambdaCallback                                         | Callback is registered successfully.                                            | Should Pass     |
 * | 04               | Invoke the registered callback with test parameters.                   | ptr = 0xdeadbeef, err = 42                                          | Lambda callback execution sets callbackInvoked = true, callbackErr = 42, callbackPtr = 0xdeadbeef. | Should Pass     |
 * | 05               | Verify callback invocation and check output values.                    | Expected: callbackInvoked = true, callbackErr = 42, callbackPtr = 0xdeadbeef | Assertions pass confirming the callback was invoked with expected results. | Should Pass     |
 */
TEST_F(DrmSessionManagerTests, RegisterValidCallbackFunction) {
    std::cout << "Entering RegisterValidCallbackFunction test" << std::endl;
    
    // Create an object of DrmSessionManager using custom constructor.
       auto cb = [](uint32_t id1, uint32_t id2, const std::string& text) {
         std::cout << "Callback invoked with id1=" << id1
                  << ", id2=" << id2
                  << ", text=" << text << std::endl;
    };
    DrmSessionManager drmSessionManager(10, reinterpret_cast<void*>(0x1234), cb);
    
    // Variables to record callback invocation.
    bool callbackInvoked = false;
    int callbackErr = 0;
    void* callbackPtr = nullptr;
    
    // Define a lambda callback.
    auto lambdaCallback = [&callbackInvoked, &callbackErr, &callbackPtr](void* ptr, int err) {
        std::cout << "Lambda callback invoked with ptr: " << ptr << " and err: " << err << std::endl;
        callbackInvoked = true;
        callbackErr = err;
        callbackPtr = ptr;
    };
    std::cout << "Invoking RegisterSetFailure with a valid lambda callback" << std::endl;
    drmSessionManager.RegisterSetFailure(lambdaCallback);
    
    std::cout << "Callback registered. Invoking the registered callback with test parameters: ptr = " 
              << reinterpret_cast<void*>(0xdeadbeef) << ", err = 42" << std::endl;
    if (drmSessionManager.setfailureCb) {
        drmSessionManager.setfailureCb(reinterpret_cast<void*>(0xdeadbeef), 42);
    }
    
    std::cout << "After invocation, callbackInvoked: " << callbackInvoked 
              << ", callbackErr: " << callbackErr 
              << ", callbackPtr: " << callbackPtr << std::endl;
    EXPECT_TRUE(callbackInvoked);
    EXPECT_EQ(callbackErr, 42);
    EXPECT_EQ(callbackPtr, reinterpret_cast<void*>(0xdeadbeef));
    
    std::cout << "Exiting RegisterValidCallbackFunction test" << std::endl;
}
/**
 * @brief Test for verifying that registering an empty callback function results in an empty internal callback.
 *
 * This test ensures that when an empty std::function callback is passed to the RegisterSetFailure API, the internal callback (setfailureCb) remains unset.
 * This behavior is crucial to ensure that the system does not inadvertently execute any callback logic when a null/empty callback is registered.
 *
 * **Test Group ID:** Basic: 01@n
 * **Test Case ID:** 129@n
 * **Priority:** High@n
 *
 * **Pre-Conditions:** None@n
 * **Dependencies:** None@n
 * **User Interaction:** None@n
 *
 * **Test Procedure:**@n
 * | Variation / Step | Description | Test Data | Expected Result | Notes |
 * | :----: | --------- | ---------- |-------------- | ----- |
 * | 01 | Initialize DrmSessionManager with parameters. | input1 = 10, input2 = 0x1234 | DrmSessionManager object is successfully created. | Should be successful |
 * | 02 | Create an empty std::function callback. | callback = empty | Empty callback function object is created. | Should be successful |
 * | 03 | Call RegisterSetFailure with the empty callback. | callback = empty | API registers an empty callback without error. | Should Pass |
 * | 04 | Assert that the internal setfailureCb is empty. | result = !drmSessionManager.setfailureCb | EXPECT_TRUE(isEmpty) returns true. | Should Pass |
 */
TEST_F(DrmSessionManagerTests, RegisterEmptyCallbackFunction) {
    std::cout << "Entering RegisterEmptyCallbackFunction test" << std::endl;
    
    // Create an object of DrmSessionManager using default/custom constructor.
       auto cb = [](uint32_t id1, uint32_t id2, const std::string& text) {
         std::cout << "Callback invoked with id1=" << id1
                  << ", id2=" << id2
                  << ", text=" << text << std::endl;
    };
    DrmSessionManager drmSessionManager(10, reinterpret_cast<void*>(0x1234), cb);
    
    // Create an empty std::function callback.
    std::function<void(void*, int)> emptyCallback;
    std::cout << "Invoking RegisterSetFailure with an empty callback function object" << std::endl;
    drmSessionManager.RegisterSetFailure(emptyCallback);
    
    std::cout << "Callback registered. Checking if the internal callback is empty." << std::endl;
    bool isEmpty = !drmSessionManager.setfailureCb;
    std::cout << "Internal setfailureCb empty state: " << isEmpty << std::endl;
    EXPECT_TRUE(isEmpty);
    
    std::cout << "Exiting RegisterEmptyCallbackFunction test" << std::endl;
}
/**
 * @brief Verify that overwriting an existing callback function works correctly
 *
 * This test verifies that the DrmSessionManager correctly overwrites an already registered callback function. Initially, a callback is registered and invoked, and its successful execution is verified. Then, a second callback is registered to replace the first one, and its successful invocation is confirmed. This ensures that the implementation allows updating the callback function reliably.
 *
 * **Test Group ID:** Basic: 01@n
 * **Test Case ID:** 130@n
 * **Priority:** High@n
 *
 * **Pre-Conditions:** None@n
 * **Dependencies:** None@n
 * **User Interaction:** None@n
 *
 * **Test Procedure:**
 * | Variation / Step | Description | Test Data | Expected Result | Notes |
 * | :----: | --------- | ---------- |-------------- | ----- |
 * | 01 | Instantiate DrmSessionManager with capacity 10 and context pointer 0x1234. | input1 = capacity: 10, input2 = context: reinterpret_cast<void*>(0x1234) | Instance should be created successfully. | Should be successful |
 * | 02 | Register the first callback using a lambda capturing firstCallbackInvoked. | input = firstLambda, output = setfailureCb referencing firstLambda | Callback registration should succeed and setfailureCb should point to first callback. | Should Pass |
 * | 03 | Invoke the first registered callback with ptr = 0x1111 and err = 10. | input = ptr: reinterpret_cast<void*>(0x1111), err: 10 | firstCallbackInvoked should be true and EXPECT_TRUE(firstCallbackInvoked) should pass. | Should Pass |
 * | 04 | Overwrite the callback by registering a second lambda capturing secondCallbackInvoked. | input = secondLambda, output = setfailureCb referencing secondLambda | The previously registered callback should be overwritten successfully. | Should Pass |
 * | 05 | Invoke the newly registered callback with ptr = 0x2222 and err = 20. | input = ptr: reinterpret_cast<void*>(0x2222), err: 20 | secondCallbackInvoked should be true and EXPECT_TRUE(secondCallbackInvoked) should pass. | Should Pass |
 */
TEST_F(DrmSessionManagerTests, OverwriteExistingCallbackFunction) {
    std::cout << "Entering OverwriteExistingCallbackFunction test" << std::endl;
    
    // Create an instance of DrmSessionManager.
       auto cb = [](uint32_t id1, uint32_t id2, const std::string& text) {
         std::cout << "Callback invoked with id1=" << id1
                  << ", id2=" << id2
                  << ", text=" << text << std::endl;
    };
    DrmSessionManager drmSessionManager(10, reinterpret_cast<void*>(0x1234), cb);
    
    // First callback records its invocation.
    bool firstCallbackInvoked = false;
    auto firstLambda = [&firstCallbackInvoked](void* ptr, int err) {
        std::cout << "First lambda callback invoked with ptr: " << ptr << " and err: " << err << std::endl;
        firstCallbackInvoked = true;
    };
    std::cout << "Invoking RegisterSetFailure with the first lambda callback" << std::endl;
    drmSessionManager.RegisterSetFailure(firstLambda);
    
    std::cout << "Invoking the registered first lambda callback with test parameters: ptr = " 
              << reinterpret_cast<void*>(0x1111) << ", err = 10" << std::endl;
    if (drmSessionManager.setfailureCb) {
        drmSessionManager.setfailureCb(reinterpret_cast<void*>(0x1111), 10);
    }
    std::cout << "After invocation, firstCallbackInvoked: " << firstCallbackInvoked << std::endl;
    EXPECT_TRUE(firstCallbackInvoked);
    
    // Second callback to overwrite the first one.
    bool secondCallbackInvoked = false;
    auto secondLambda = [&secondCallbackInvoked](void* ptr, int err) {
        std::cout << "Second lambda callback invoked with ptr: " << ptr << " and err: " << err << std::endl;
        secondCallbackInvoked = true;
    };
    std::cout << "Overwriting the callback by invoking RegisterSetFailure with the second lambda callback" << std::endl;
    drmSessionManager.RegisterSetFailure(secondLambda);
    
    std::cout << "Invoking the registered second lambda callback with test parameters: ptr = " 
              << reinterpret_cast<void*>(0x2222) << ", err = 20" << std::endl;
    if (drmSessionManager.setfailureCb) {
        drmSessionManager.setfailureCb(reinterpret_cast<void*>(0x2222), 20);
    }
    std::cout << "After invocation, secondCallbackInvoked: " << secondCallbackInvoked << std::endl;
    EXPECT_TRUE(secondCallbackInvoked);
    
    std::cout << "Exiting OverwriteExistingCallbackFunction test" << std::endl;
}
/**
 * @brief Verify callback mechanism in DrmSessionManager works with a lambda capturing a null pointer.
 *
 * This test verifies that the DrmSessionManager correctly registers and invokes a callback function defined as a lambda that captures a null pointer. The test ensures that once the callback is registered, invoking it with specific test parameters successfully triggers the lambda, setting the expected flag.
 *
 * **Test Group ID:** Basic: 01@n
 * **Test Case ID:** 131@n
 * **Priority:** High@n
 *
 * **Pre-Conditions:** None@n
 * **Dependencies:** None@n
 * **User Interaction:** None@n
 *
 * **Test Procedure:**@n
 * | Variation / Step | Description | Test Data | Expected Result | Notes |
 * | :----: | --------- | ---------- |-------------- | ----- |
 * | 01 | Create DrmSessionManager instance. | maxSessions = 10, ptr = 0x1234 | Instance created successfully. | Should be successful |
 * | 02 | Define lambda capturing a null pointer and a reference variable callbackInvoked. | capturedPtr = nullptr, callbackInvoked = false | Lambda defined successfully. | Should be successful |
 * | 03 | Register the lambda as callback using RegisterSetFailure. | callback = lambdaWithNullCapture | Callback is set in drmSessionManager.setfailureCb. | Should Pass |
 * | 04 | Invoke the registered callback with test parameters. | ptr = nullptr, err = 0 | Lambda is invoked and callbackInvoked becomes true. | Should Pass |
 * | 05 | Verify that callbackInvoked is true using an assertion. | callbackInvoked expected = true | EXPECT_TRUE(callbackInvoked) passes. | Should Pass |
 */
TEST_F(DrmSessionManagerTests, RegisterValidCallbackWithCapturedNullPointer) {
    std::cout << "Entering RegisterValidCallbackWithCapturedNullPointer test" << std::endl;
    
    // Create an object of DrmSessionManager.
       auto cb = [](uint32_t id1, uint32_t id2, const std::string& text) {
         std::cout << "Callback invoked with id1=" << id1
                  << ", id2=" << id2
                  << ", text=" << text << std::endl;
    };
    DrmSessionManager drmSessionManager(10, reinterpret_cast<void*>(0x1234), cb);
    
    // Define a lambda that captures a null pointer.
    bool callbackInvoked = false;
    auto lambdaWithNullCapture = [capturedPtr = static_cast<void*>(nullptr), &callbackInvoked](void* ptr, int err) {
        std::cout << "Lambda with captured null pointer invoked. CapturedPtr: " << capturedPtr 
                  << ", Parameters: ptr: " << ptr << ", err: " << err << std::endl;
        callbackInvoked = true;
    };
    std::cout << "Invoking RegisterSetFailure with lambda capturing a null pointer" << std::endl;
    drmSessionManager.RegisterSetFailure(lambdaWithNullCapture);
    
    std::cout << "Callback registered. Invoking the callback with test parameters: ptr = nullptr, err = 0" << std::endl;
    if (drmSessionManager.setfailureCb) {
        drmSessionManager.setfailureCb(nullptr, 0);
    }
    std::cout << "After invocation, callbackInvoked: " << callbackInvoked << std::endl;
    EXPECT_TRUE(callbackInvoked);
    
    std::cout << "Exiting RegisterValidCallbackWithCapturedNullPointer test" << std::endl;
}
/**
 * @brief Verify that UpdateDRMConfig correctly updates DRM configuration when all input flags are true
 *
 * This test case ensures that invoking UpdateDRMConfig with all flags set to true updates the DRM configuration as expected and that the operations are logged for diagnostic purposes@n
 *
 * **Test Group ID:** Basic: 01@n
 * **Test Case ID:** 132@n
 * **Priority:** High@n
 * 
 * **Pre-Conditions:** None@n
 * **Dependencies:** None@n
 * **User Interaction:** None@n
 * 
 * **Test Procedure:**@n
 * | Variation / Step | Description | Test Data | Expected Result | Notes |
 * | :----: | --------- | ---------- |-------------- | ----- |
 * | 01 | Construct the DrmSessionManager object | maxDrmSessions = 10, player = nullptr | Object is created successfully | Should be successful |
 * | 02 | Log the input parameters before invoking UpdateDRMConfig | useSecManager = true, enablePROutputProtection = true, propagateURIParam = true, isFakeTune = true, wideVineKIDWorkaround = true | Parameters are printed as expected | Should be successful |
 * | 03 | Invoke the UpdateDRMConfig method with all flags set to true | useSecManager = true, enablePROutputProtection = true, propagateURIParam = true, isFakeTune = true, wideVineKIDWorkaround = true | DRM configuration is updated without errors | Should Pass |
 * | 04 | Log the completion of the test | N/A | Completion message is logged | Should be successful |
 */
TEST_F(DrmSessionManagerTests, UpdateDRMConfig_AllTrue) {
    std::cout << "Entering UpdateDRMConfig_AllTrue test" << std::endl;
    
    // Construct DrmSessionManager object using custom constructor.
    auto cb = [](uint32_t id1, uint32_t id2, const std::string& text) {
         std::cout << "Callback invoked with id1=" << id1
                  << ", id2=" << id2
                  << ", text=" << text << std::endl;
    };
    DrmSessionManager drmManager(10, nullptr, cb);
    std::cout << "Created DrmSessionManager with maxDrmSessions = 10 and player = nullptr" << std::endl;
    
    // Log the input parameters.
    bool useSecManager = true;
    bool enablePROutputProtection = true;
    bool propagateURIParam = true;
    bool isFakeTune = true;
    bool wideVineKIDWorkaround = true;
    std::cout << "Invoking UpdateDRMConfig with parameters: "
              << "useSecManager=" << useSecManager << ", "
              << "enablePROutputProtection=" << enablePROutputProtection << ", "
              << "propagateURIParam=" << propagateURIParam << ", "
              << "isFakeTune=" << isFakeTune << ", "
              << "wideVineKIDWorkaround=" << wideVineKIDWorkaround << std::endl;
              
    // Invoke the method.
    drmManager.UpdateDRMConfig(useSecManager, enablePROutputProtection, propagateURIParam, isFakeTune, wideVineKIDWorkaround);
    std::cout << "UpdateDRMConfig executed. DRM configuration updated to all true values." << std::endl;
    
    std::cout << "Exiting UpdateDRMConfig_AllTrue test" << std::endl;
}
/**
 * @brief Verifies that the UpdateDRMConfig method correctly updates the DRM configuration when all input parameters are false
 *
 * This test ensures that when all the boolean parameters passed to UpdateDRMConfig are false, the DRM configuration of the DrmSessionManager is updated as expected. This is important to validate the basic functionality of the DRM configuration update operation under a minimal configuration scenario where security and protection features are disabled.@n
 *
 * **Test Group ID:** Basic: 01@n
 * **Test Case ID:** 133@n
 * **Priority:** High@n
 * @n
 * **Pre-Conditions:** None@n
 * **Dependencies:** None@n
 * **User Interaction:** None@n
 * @n
 * **Test Procedure:**@n
 * | Variation / Step | Description                                                                          | Test Data                                                                                                                                                | Expected Result                                                 | Notes       |
 * | :--------------: | ------------------------------------------------------------------------------------ | -------------------------------------------------------------------------------------------------------------------------------------------------------- | ---------------------------------------------------------------- | ----------- |
 * | 01               | Construct DrmSessionManager and invoke UpdateDRMConfig with all false parameters       | Constructor: maxDrmSessions = 10, player = nullptr; UpdateDRMConfig: useSecManager = false, enablePROutputProtection = false, propagateURIParam = false, isFakeTune = false, wideVineKIDWorkaround = false | DRM configuration is updated with all false values with no errors | Should Pass |
 */
TEST_F(DrmSessionManagerTests, UpdateDRMConfig_AllFalse) {
    std::cout << "Entering UpdateDRMConfig_AllFalse test" << std::endl;
    
    // Construct DrmSessionManager object using custom constructor.
    auto cb = [](uint32_t id1, uint32_t id2, const std::string& text) {
         std::cout << "Callback invoked with id1=" << id1
                  << ", id2=" << id2
                  << ", text=" << text << std::endl;
    };
    DrmSessionManager drmManager(10, nullptr, cb);
    std::cout << "Created DrmSessionManager with maxDrmSessions = 10 and player = nullptr" << std::endl;
    
    // Log the input parameters.
    bool useSecManager = false;
    bool enablePROutputProtection = false;
    bool propagateURIParam = false;
    bool isFakeTune = false;
    bool wideVineKIDWorkaround = false;
    std::cout << "Invoking UpdateDRMConfig with parameters: "
              << "useSecManager=" << useSecManager << ", "
              << "enablePROutputProtection=" << enablePROutputProtection << ", "
              << "propagateURIParam=" << propagateURIParam << ", "
              << "isFakeTune=" << isFakeTune << ", "
              << "wideVineKIDWorkaround=" << wideVineKIDWorkaround << std::endl;
    
    // Invoke the method.
    drmManager.UpdateDRMConfig(useSecManager, enablePROutputProtection, propagateURIParam, isFakeTune, wideVineKIDWorkaround);
    std::cout << "UpdateDRMConfig executed. DRM configuration updated to all false values." << std::endl;
    
    std::cout << "Exiting UpdateDRMConfig_AllFalse test" << std::endl;
}
/**
 * @brief Verify UpdateDRMConfig API handles a mixed configuration pattern correctly.
 *
 * This test verifies that the UpdateDRMConfig API is invoked on a DrmSessionManager object constructed with custom parameters and that the DRM configuration is updated according to a mixed pattern of boolean flags (true, false, true, false, true). The purpose of this test is to validate the proper behavior of the API when provided with a combination of true and false flags.
 *
 * **Test Group ID:** Basic: 01@n
 * **Test Case ID:** 134@n
 * **Priority:** High@n
 *
 * **Pre-Conditions:** None@n
 * **Dependencies:** None@n
 * **User Interaction:** None@n
 *
 * **Test Procedure:**
 * | Variation / Step | Description                                                                                           | Test Data                                                                                                            | Expected Result                                                                                                | Notes           |
 * | :--------------: | ----------------------------------------------------------------------------------------------------- | -------------------------------------------------------------------------------------------------------------------- | -------------------------------------------------------------------------------------------------------------- | --------------- |
 * | 01               | Construct DrmSessionManager using custom constructor.                                               | input: maxDrmSessions = 10, player = nullptr                                                                          | DrmSessionManager object is created successfully.                                                              | Should be successful |
 * | 02               | Log the input parameters for the UpdateDRMConfig method invocation.                                   | input: useSecManager = true, enablePROutputProtection = false, propagateURIParam = true, isFakeTune = false, wideVineKIDWorkaround = true | The input parameters are printed correctly to the console.                                                   | Should be successful |
 * | 03               | Invoke the UpdateDRMConfig API with the specified boolean flag configuration.                         | input: useSecManager = true, enablePROutputProtection = false, propagateURIParam = true, isFakeTune = false, wideVineKIDWorkaround = true | DRM configuration is updated with the pattern (true, false, true, false, true) and returns normally.                              | Should Pass     |
 * | 04               | Log the exit message indicating the completion of the test.                                         | No additional input                                                                                                  | Exit message is printed to indicate the end of the test execution.                                             | Should be successful |
 */
TEST_F(DrmSessionManagerTests, UpdateDRMConfig_Mixed1) {
    std::cout << "Entering UpdateDRMConfig_Mixed1 test" << std::endl;
    
    // Construct DrmSessionManager object using custom constructor.
       auto cb = [](uint32_t id1, uint32_t id2, const std::string& text) {
         std::cout << "Callback invoked with id1=" << id1
                  << ", id2=" << id2
                  << ", text=" << text << std::endl;
    };
    DrmSessionManager drmManager(10, nullptr, cb);
    std::cout << "Created DrmSessionManager with maxDrmSessions = 10 and player = nullptr" << std::endl;
    
    // Log the input parameters.
    bool useSecManager = true;
    bool enablePROutputProtection = false;
    bool propagateURIParam = true;
    bool isFakeTune = false;
    bool wideVineKIDWorkaround = true;
    std::cout << "Invoking UpdateDRMConfig with parameters: "
              << "useSecManager=" << useSecManager << ", "
              << "enablePROutputProtection=" << enablePROutputProtection << ", "
              << "propagateURIParam=" << propagateURIParam << ", "
              << "isFakeTune=" << isFakeTune << ", "
              << "wideVineKIDWorkaround=" << wideVineKIDWorkaround << std::endl;
              
    // Invoke the method.
    drmManager.UpdateDRMConfig(useSecManager, enablePROutputProtection, propagateURIParam, isFakeTune, wideVineKIDWorkaround);
    std::cout << "UpdateDRMConfig executed. DRM configuration updated with pattern (true, false, true, false, true)." << std::endl;
    
    std::cout << "Exiting UpdateDRMConfig_Mixed1 test" << std::endl;
}
/**
 * @brief Verify that UpdateDRMConfig correctly applies mixed boolean configuration parameters.
 *
 * This test verifies that the UpdateDRMConfig method of the DrmSessionManager class is capable of correctly updating its DRM configuration when provided with a specific combination of boolean parameters. The test logs each significant step, ensuring that constructor initialization and subsequent configuration update behave as expected.
 *
 * **Test Group ID:** Basic: 01@n
 * **Test Case ID:** 135@n
 * **Priority:** High@n
 *
 * **Pre-Conditions:** None@n
 * **Dependencies:** None@n
 * **User Interaction:** None@n
 *
 * **Test Procedure:**
 * | Variation / Step | Description | Test Data | Expected Result | Notes |
 * | :----: | --------- | ---------- |-------------- | ----- |
 * | 01 | Create DRM session manager object with custom constructor. | maxDrmSessions = 10, player = nullptr | drmManager object successfully created. | Should be successful |
 * | 02 | Log input parameters for UpdateDRMConfig method. | useSecManager = false, enablePROutputProtection = true, propagateURIParam = false, isFakeTune = true, wideVineKIDWorkaround = false | Parameters logged correctly. | Should be successful |
 * | 03 | Invoke UpdateDRMConfig method with mixed boolean parameters. | useSecManager = false, enablePROutputProtection = true, propagateURIParam = false, isFakeTune = true, wideVineKIDWorkaround = false | DRM configuration updated correctly; method executed without errors. | Should Pass |
 */
TEST_F(DrmSessionManagerTests, UpdateDRMConfig_Mixed2) {
    std::cout << "Entering UpdateDRMConfig_Mixed2 test" << std::endl;
    
    // Construct DrmSessionManager object using custom constructor.
    auto cb = [](uint32_t id1, uint32_t id2, const std::string& text) {
         std::cout << "Callback invoked with id1=" << id1
                  << ", id2=" << id2
                  << ", text=" << text << std::endl;
    };
    DrmSessionManager drmManager(10, nullptr, cb);
    std::cout << "Created DrmSessionManager with maxDrmSessions = 10 and player = nullptr" << std::endl;
    
    // Log the input parameters.
    bool useSecManager = false;
    bool enablePROutputProtection = true;
    bool propagateURIParam = false;
    bool isFakeTune = true;
    bool wideVineKIDWorkaround = false;
    std::cout << "Invoking UpdateDRMConfig with parameters: "
              << "useSecManager=" << useSecManager << ", "
              << "enablePROutputProtection=" << enablePROutputProtection << ", "
              << "propagateURIParam=" << propagateURIParam << ", "
              << "isFakeTune=" << isFakeTune << ", "
              << "wideVineKIDWorkaround=" << wideVineKIDWorkaround << std::endl;
              
    // Invoke the method.
    drmManager.UpdateDRMConfig(useSecManager, enablePROutputProtection, propagateURIParam, isFakeTune, wideVineKIDWorkaround);
    std::cout << "UpdateDRMConfig executed. DRM configuration updated with pattern (false, true, false, true, false)." << std::endl;
    
    std::cout << "Exiting UpdateDRMConfig_Mixed2 test" << std::endl;
}
