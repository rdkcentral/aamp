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
* @file test_l1_PlayerHlsDrmSessionInterface.cpp
* @page PlayerHlsDrmSessionInterface Level 1 Tests
*
* ## Module's Role
* This module includes Level 1 functional tests (success and failure scenarios).
* This is to ensure that the PlayerHlsDrmSessionInterface methods meet the requirements.
*
* **Pre-Conditions:**  None @n
* **Dependencies:** None @n
*
*/

#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include <stdio.h>
#include "PlayerHlsDrmSessionInterface.h"
#include "MockDrmSession.h"
#include "MockHlsDrmSession.h"


class FakeHlsDrmSessionManagerTests : public ::testing::Test
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

class PlayerHlsDrmSessionInterfaceTests : public ::testing::Test
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
 * @brief Positive test for createSession with a valid positive stream type.
 *
 * This test validates that invoking createSession with a valid drmInfo and a positive stream type returns the expected result as confirmed by the ASSERT_EQ check, ensuring the manager's behavior aligns with the expectations under normal conditions.
 *
 * **Test Group ID:** Basic: 01
 * **Test Case ID:** 001
 * **Priority:** High
 *
 * **Pre-Conditions:** None
 * **Dependencies:** None
 * **User Interaction:** None
 *
 * **Test Procedure:**
 * | Variation / Step | Description                                                          | Test Data                                 | Expected Result                                      | Notes       |
 * | :---------------:| -------------------------------------------------------------------- | ----------------------------------------- | ---------------------------------------------------- | ----------- |
 * | 01               | Invoke createSession with drmInfo (default) and streamType = 10        | drmInfo = default, streamType = 10          | API returns nullptr and ASSERT_EQ validates the result | Should Pass |
 */
TEST_F(FakeHlsDrmSessionManagerTests, PositiveStreamType) {
    std::cout << "Entering PositiveStreamType test" << std::endl;
    DrmInfo drmInfo;
    int streamType = 10;
    
    std::cout << "Invoking createSession with positive streamType = " << streamType << std::endl;
    FakeHlsDrmSessionManager manager;
    auto session = manager.createSession(drmInfo, streamType);
    std::cout << "createSession returned: " << (session == nullptr ? "nullptr" : "non-null") << std::endl;
    
    ASSERT_EQ(session, nullptr);
    std::cout << "Exiting PositiveStreamType test" << std::endl;
}

/**
 * @brief Verify that createSession returns nullptr when provided with a negative stream type
 *
 * This test case validates the behavior of FakeHlsDrmSessionManager's createSession method when an invalid, negative stream type is provided. The test confirms that the API correctly handles invalid input by returning a nullptr, ensuring robust error handling in the session creation process.
 *
 * **Test Group ID:** Basic: 01@n
 * **Test Case ID:** 002@n
 * **Priority:** High@n
 * 
 * **Pre-Conditions:** None@n
 * **Dependencies:** None@n
 * **User Interaction:** None@n
 * 
 * **Test Procedure:**
 * | Variation / Step | Description | Test Data | Expected Result | Notes |
 * | :----: | --------- | ---------- |-------------- | ----- |
 * | 01 | Initialize drmInfo and streamType with invalid negative value | drmInfo = default, streamType = -1 | Inputs initialized correctly | Should be successful |
 * | 02 | Invoke createSession API with invalid streamType | drmInfo = default, streamType = -1 | API returns nullptr indicating failure for invalid stream type input | Should Fail |
 * | 03 | Verify that the returned session is nullptr using ASSERT_EQ | session == nullptr | Assertion passes confirming the API's correct error handling | Should be successful |
 */
TEST_F(FakeHlsDrmSessionManagerTests, NegativeStreamType) {
    std::cout << "Entering NegativeStreamType test" << std::endl;   
    DrmInfo drmInfo;
    int streamType = -1;
    
    std::cout << "Invoking createSession with negative streamType = " << streamType << std::endl;
    FakeHlsDrmSessionManager manager;
    auto session = manager.createSession(drmInfo, streamType);
    std::cout << "createSession returned: " << (session == nullptr ? "nullptr" : "non-null") << std::endl;
    
    ASSERT_EQ(session, nullptr);
   
    std::cout << "Exiting NegativeStreamType test" << std::endl;
}

/**
 * @brief Verify that default constructed DRM objects return expected support status
 *
 * This test verifies that a default constructed FakeHlsDrmSessionManager and a default constructed DrmInfo object behave as expected when passed to isDrmSupported. This ensures that the default initialization state provides the correct support status.
 *
 * **Test Group ID:** Basic: 01@n
 * **Test Case ID:** 003@n
 * **Priority:** High@n
 *
 * **Pre-Conditions:** None@n
 * **Dependencies:** None@n
 * **User Interaction:** None@n
 *
 * **Test Procedure:**
 * | Variation / Step | Description                                                             | Test Data                                                   | Expected Result                                                               | Notes           |
 * | :--------------: | ----------------------------------------------------------------------- | ----------------------------------------------------------- | ----------------------------------------------------------------------------- | --------------- |
 * | 01               | Create default constructed FakeHlsDrmSessionManager and DrmInfo objects   | manager = FakeHlsDrmSessionManager(), drmInfo = DrmInfo()     | Objects instantiated with default values                                    | Should be successful |
 * | 02               | Invoke isDrmSupported with default DrmInfo                              | input: drmInfo.method = default, drmInfo.mediaFormat = default| Returns boolean value indicating whether DRM is supported (expected to pass)    | Should Pass     |
 */
TEST_F(FakeHlsDrmSessionManagerTests, DefaultConstructedDrmInfo) {
    std::cout << "Entering DefaultConstructedDrmInfo test" << std::endl;
    // Create a default instance of FakeHlsDrmSessionManager using default constructor
    FakeHlsDrmSessionManager manager;
    // Create default constructed DrmInfo
    DrmInfo drmInfo;
    
    std::cout << "Created default DrmInfo with method = " << drmInfo.method
              << " and mediaFormat = " << drmInfo.mediaFormat << std::endl;
    // Invoke the method isDrmSupported
    std::cout << "Invoking isDrmSupported with default DrmInfo" << std::endl;
    bool support = manager.isDrmSupported(drmInfo);
    
    std::cout << "Returned value from isDrmSupported: " << support << std::endl;
    
    std::cout << "Exiting DefaultConstructedDrmInfo test" << std::endl;
}

/**
 * @brief Verify that registering a valid callback correctly saves the callback for later use within FakeHlsDrmSessionManager
 *
 * This test ensures that when a valid callback lambda is registered to the FakeHlsDrmSessionManager, it is properly stored.
 * The test verifies the registration mechanism by checking the internal state update through a callback mechanism.
 *
 * **Test Group ID:** Basic: 01@n
 * **Test Case ID:** 004@n
 * **Priority:** High@n
 * 
 * **Pre-Conditions:** None@n
 * **Dependencies:** None@n
 * **User Interaction:** None@n
 * 
 * **Test Procedure:**@n
 * | Variation / Step | Description | Test Data | Expected Result | Notes |
 * | :----: | --------- | ---------- |-------------- | ----- |
 * | 01 | Log the entry into RegisterValidCallbackAndVerifyStorage test | No inputs, output: "Entering RegisterValidCallbackAndVerifyStorage test" printed | Log message printed indicating start of test | Should be successful |
 * | 02 | Create an instance of FakeHlsDrmSessionManager | No inputs, output: instance of FakeHlsDrmSessionManager created | Instance successfully created | Should be successful |
 * | 03 | Define and initialize callbackInvoked variable and a valid callback lambda | callbackInvoked = 0; validCallback lambda defined with input: bridge, drmHelper, session, streamType | Lambda is defined and callbackInvoked set to 0, expected to be updated when invoked | Should be successful |
 * | 04 | Register the valid callback lambda via RegisterGetHlsDrmSessionCb | Input: validCallback; output: callback registration within fakeManager | Callback is registered correctly within FakeHlsDrmSessionManager | Should Pass |
 * | 05 | Log the exit from the test case | No inputs, output: "Exiting RegisterValidCallbackAndVerifyStorage test" printed | Log message printed indicating end of test | Should be successful |
 */
TEST_F(FakeHlsDrmSessionManagerTests, RegisterValidCallbackAndVerifyStorage) {
    std::cout << "Entering RegisterValidCallbackAndVerifyStorage test" << std::endl;
    FakeHlsDrmSessionManager fakeManager;
    int callbackInvoked = 0;
    auto validCallback = [&callbackInvoked](std::shared_ptr<HlsDrmBase>& bridge,
                                             std::shared_ptr<DrmHelper>& drmHelper,
                                             DrmSession*& session,
                                             int streamType) {
        std::cout << "Invoking validCallback lambda with streamType: " << streamType << std::endl;
        callbackInvoked = 1;
    };
    std::cout << "Invoking RegisterGetHlsDrmSessionCb with valid callback lambda" << std::endl;
    fakeManager.RegisterGetHlsDrmSessionCb(validCallback);
    std::cout << "Exiting RegisterValidCallbackAndVerifyStorage test" << std::endl;
}

/**
 * @brief Verify that registering an empty callback does not alter internal storage behavior.
 *
 * This test verifies the behavior of FakeHlsDrmSessionManager when an empty (default constructed) callback is registered.
 * It ensures that the API accepts an empty callback without errors and that the storage for callbacks remains consistent.
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
 * | Variation / Step | Description                                                                 | Test Data                                                                                      | Expected Result                                                                                 | Notes         |
 * | :--------------: | --------------------------------------------------------------------------- | ---------------------------------------------------------------------------------------------- | ----------------------------------------------------------------------------------------------- | ------------- |
 * | 01               | Print the entering test log message.                                        | Message = "Entering RegisterEmptyCallbackAndVerifyStorage test"                                | The entering log is printed.                                                                    | Should be successful |
 * | 02               | Invoke RegisterGetHlsDrmSessionCb with an empty callback on fakeManager.     | input: emptyCallback = default constructed, fakeManager instance registered via API call         | The empty callback is registered with fakeManager without errors (API returns void).            | Should Pass   |
 * | 03               | Print the exiting test log message.                                         | Message = "Exiting RegisterEmptyCallbackAndVerifyStorage test"                                 | The exiting log is printed.                                                                     | Should be successful |
 */
TEST_F(FakeHlsDrmSessionManagerTests, RegisterEmptyCallbackAndVerifyStorage) {
    std::cout << "Entering RegisterEmptyCallbackAndVerifyStorage test" << std::endl;
    FakeHlsDrmSessionManager fakeManager;
    std::function<void(std::shared_ptr<HlsDrmBase>&, std::shared_ptr<DrmHelper>&, DrmSession*&, int)> emptyCallback;
    std::cout << "Invoking RegisterGetHlsDrmSessionCb with empty callback (default constructed)" << std::endl;
    fakeManager.RegisterGetHlsDrmSessionCb(emptyCallback);
    std::cout << "Exiting RegisterEmptyCallbackAndVerifyStorage test" << std::endl;
}

/**
 * @brief Verifies that registering a valid callback successfully registers it in PlayerHlsDrmSessionInterface.
 *
 * This test ensures that a valid callback lambda can be registered with the PlayerHlsDrmSessionInterface. It validates that the registration method accepts the callback and that the corresponding console outputs are produced, indicating the correct functioning of the callback registration mechanism.
 *
 * **Test Group ID:** Basic: 01@n
 * **Test Case ID:** 006@n
 * **Priority:** High@n
 *
 * **Pre-Conditions:** None@n
 * **Dependencies:** None@n
 * **User Interaction:** None@n
 *
 * **Test Procedure:**@n
 * | Variation / Step | Description                                                        | Test Data                                                             | Expected Result                                                         | Notes       |
 * | :----:           | ------------------------------------------------------------------ | --------------------------------------------------------------------- | ----------------------------------------------------------------------  | ----------- |
 * | 01               | Invoke RegisterGetHlsDrmSessionCb with a valid callback lambda       | playerHlsDrmSessionInterface = instance, validCallback = lambda       | Callback registered successfully; appropriate output messages displayed | Should Pass |
 */
TEST_F(PlayerHlsDrmSessionInterfaceTests, RegisterValidCallback) {
    std::cout << "Entering RegisterValidCallback test" << std::endl;
    PlayerHlsDrmSessionInterface* playerHlsDrmSessionInterface = PlayerHlsDrmSessionInterface::getInstance();
    ASSERT_NE(playerHlsDrmSessionInterface, nullptr);

    auto validCallback = [] (std::shared_ptr<HlsDrmBase>& bridge,
                             std::shared_ptr<DrmHelper>& drmHelper,
                             DrmSession*& session,
                             int streamType) {
        std::cout << "Invoking RegisterGetHlsDrmSessionCb with valid callback lambda" << std::endl;
    };

    playerHlsDrmSessionInterface->RegisterGetHlsDrmSessionCb(validCallback);
    std::cout << "Exiting RegisterValidCallback test" << std::endl;
}


/**
 * @brief Verify that an empty callback is registered safely without errors.
 *
 * This test verifies whether the RegisterGetHlsDrmSessionCb method of PlayerHlsDrmSessionInterface can accept an empty callback and execute without any errors.
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
 * | 01 | Log entry message indicating the start of the test | None | "Entering RegisterEmptyCallback test" message printed | Should be successful |
 * | 02 | Create an instance of PlayerHlsDrmSessionInterface and declare an empty callback | PlayerHlsDrmSessionInterface instance, emptyCallback = uninitialized | Instance created and empty callback declared successfully | Should be successful |
 * | 03 | Log message before API invocation | None | "Invoking RegisterGetHlsDrmSessionCb with empty callback" message printed | Should be successful |
 * | 04 | Invoke RegisterGetHlsDrmSessionCb with an empty callback | input: emptyCallback = empty | API accepts empty callback without error | Should Pass |
 * | 05 | Log exit message indicating the end of the test | None | "Exiting RegisterEmptyCallback test" message printed | Should be successful |
 */
TEST_F(PlayerHlsDrmSessionInterfaceTests, RegisterEmptyCallback) {
    std::cout << "Entering RegisterEmptyCallback test" << std::endl;
    PlayerHlsDrmSessionInterface* drmSessionInterface = PlayerHlsDrmSessionInterface::getInstance();
    ASSERT_NE(drmSessionInterface, nullptr);

    std::function<void(std::shared_ptr<HlsDrmBase>&, std::shared_ptr<DrmHelper>&, DrmSession*&, int)> emptyCallback;
    std::cout << "Invoking RegisterGetHlsDrmSessionCb with empty callback" << std::endl;
    drmSessionInterface->RegisterGetHlsDrmSessionCb(emptyCallback);
    std::cout << "Exiting RegisterEmptyCallback test" << std::endl;
}

/**
 * @brief Verify that a valid DRM session is created when a positive stream type is provided.
 *
 * This test validates that the createSession() method of the PlayerHlsDrmSessionInterface class returns a non-null session pointer when invoked with a default initialized DrmInfo and a positive stream type value (15). It ensures that the DRM session creation functionality behaves correctly in a positive scenario.
 *
 * **Test Group ID:** Basic: 01@n
 * **Test Case ID:** 008@n
 * **Priority:** High@n
 *
 * **Pre-Conditions:** None@n
 * **Dependencies:** None@n
 * **User Interaction:** None@n
 *
 * **Test Procedure:** 
 * | Variation / Step | Description                                                                                     | Test Data                                                 | Expected Result                                                                  | Notes         |
 * | :--------------: | ----------------------------------------------------------------------------------------------- | --------------------------------------------------------- | -------------------------------------------------------------------------------- | ------------- |
 * | 01               | Create an instance of PlayerHlsDrmSessionInterface using its default constructor.               | drmSessionInterface: default constructed instance         | Instance is created successfully.                                                | Should be successful |
 * | 02               | Prepare input parameters including a default DrmInfo and setting streamType to 15.                | drmInfo: default, streamType = 15                           | Input parameters are ready for API invocation.                                   | Should be successful |
 * | 03               | Invoke the createSession method with the prepared drmInfo and positive streamType, and capture output. | drmInfo: default, streamType = 15                           | A session pointer is returned which is not null.                                 | Should Pass   |
 * | 04               | Validate that the returned session pointer is not null using the EXPECT_NE assertion check.      | session pointer from createSession invocation               | Assertion confirms that the session pointer is non-null.                         | Should Pass   |
 */
class DummyHlsDrmSessionManager : public PlayerHlsDrmSessionInterfaceBase {
public:
    std::shared_ptr<HlsDrmBase> createSession(const DrmInfo&, int) override {
        return std::make_shared<MockHlsDrmSession>();
    }    
};

TEST_F(PlayerHlsDrmSessionInterfaceTests, ValidDRMSession_PositiveType) {
    std::cout << "Entering ValidDRMSession_PositiveType test" << std::endl;
    auto drmSessionInterface = PlayerHlsDrmSessionInterface::getInstance();
    ASSERT_NE(drmSessionInterface, nullptr);

    drmSessionInterface->setSessionManager(new DummyHlsDrmSessionManager());

    DrmInfo drmInfo;
    int streamType = 15;

    auto session = drmSessionInterface->createSession(drmInfo, streamType);
    EXPECT_NE(session, nullptr);

    std::cout << "Exiting ValidDRMSession_PositiveType test" << std::endl;
}


/**
 * @brief Test to ensure DRM session creation fails for negative stream type.
 *
 * This test verifies that invoking the createSession method of PlayerHlsDrmSessionInterface with a valid DrmInfo and an invalid negative streamType (-1) results in a null session pointer. It ensures that the API correctly handles negative stream types as an error scenario.
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
 * | Variation / Step | Description | Test Data | Expected Result | Notes |
 * | :----: | --------- | ---------- |-------------- | ----- |
 * | 01 | Print entering test message | N/A | "Entering DRMSession_NegativeStreamType test" printed to console | Should be successful |
 * | 02 | Instantiate PlayerHlsDrmSessionInterface and DrmInfo objects | drmSessionInterface = instance, drmInfo = instance | Objects are created successfully | Should be successful |
 * | 03 | Invoke createSession with a valid drmInfo and negative streamType (-1) | drmInfo, streamType = -1 | API returns nullptr | Should Fail |
 * | 04 | Assert that the return value is nullptr and print exiting test message | EXPECT_EQ(session, nullptr) check, exit message printed | EXPECT_EQ passes confirming no session is created | Should be successful |
 */
TEST_F(PlayerHlsDrmSessionInterfaceTests, DRMSession_NegativeStreamType)
{
    std::cout << "Entering DRMSession_NegativeStreamType test" << std::endl;    
    PlayerHlsDrmSessionInterface* drmSessionInterface = PlayerHlsDrmSessionInterface::getInstance();
    ASSERT_NE(drmSessionInterface, nullptr);


// Register a dummy callback to avoid bad_function_call
    auto dummyCallback = [](std::shared_ptr<HlsDrmBase>& bridge,
                            std::shared_ptr<DrmHelper>& drmHelper,
                            DrmSession*& session,
                            int streamType) {
        // For negative stream type, no session is created
        session = nullptr;
    };
    drmSessionInterface->RegisterGetHlsDrmSessionCb(dummyCallback);

    DrmInfo drmInfo;
    int streamType = -1;
    std::cout << "Invoking createSession with with valid drminfo and negative streamType: " << streamType << std::endl;
    std::shared_ptr<HlsDrmBase> session = drmSessionInterface->createSession(drmInfo, streamType);
    EXPECT_EQ(session, nullptr);
    std::cout << "Exiting DRMSession_NegativeStreamType test" << std::endl;
}

/**
 * @brief Verify valid instance creation of PlayerHlsDrmSessionInterface.
 *
 * This test verifies that calling PlayerHlsDrmSessionInterface::getInstance() returns a valid, non-null pointer, ensuring the proper creation and access to the singleton instance.
 *
 * **Test Group ID:** Basic: 01@n
 * **Test Case ID:** 010@n
 * **Priority:** High@n
 * 
 * **Pre-Conditions:** None@n
 * **Dependencies:** None@n
 * **User Interaction:** None@n
 * 
 * **Test Procedure:**@n
 * | Variation / Step | Description | Test Data | Expected Result | Notes |
 * | :----: | --------- | ---------- |-------------- | ----- |
 * | 01 | Invoke PlayerHlsDrmSessionInterface::getInstance() to create the instance | input: None, output: instance pointer | The returned instance pointer should not be nullptr and the assertion check passes | Should Pass |
 */
TEST_F(PlayerHlsDrmSessionInterfaceTests, ValidInstanceCreation) {
    std::cout << "Entering ValidInstanceCreation test" << std::endl;
    std::cout << "Invoking PlayerHlsDrmSessionInterface::getInstance()" << std::endl;
    PlayerHlsDrmSessionInterface* instance = PlayerHlsDrmSessionInterface::getInstance();
    std::cout << "Returned instance pointer: " << instance << std::endl;
    ASSERT_NE(instance, nullptr);
    std::cout << "Exiting ValidInstanceCreation test" << std::endl;
}
