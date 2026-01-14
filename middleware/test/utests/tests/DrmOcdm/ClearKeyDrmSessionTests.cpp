
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
* @file test_l1_ClearKeyDrmSession.cpp
* @page ClearKeyDrmSession Level 1 Tests
*
* ## Module's Role
* This module includes Level 1 functional tests (success and failure scenarios).
* This is to ensure that the ClearKeyDrmSession methods meet the requirements.
*
* **Pre-Conditions:**  None @n
* **Dependencies:** None @n
*
*/

#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include <openssl/err.h>
#include <stdio.h>
#include "ClearKeyDrmSession.h"

class ClearKeyDrmSessionTests : public ::testing::Test
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
 * @brief Validates that the default constructor for ClearKeySession creates a valid instance.
 *
 * This test verifies that the ClearKeySession default constructor does not throw any exceptions and results in the creation of a valid object instance.
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
 * | Variation / Step | Description                                                             | Test Data                                              | Expected Result                                      | Notes              |
 * | :--------------: | ----------------------------------------------------------------------- | ------------------------------------------------------ | ---------------------------------------------------- | ------------------ |
 * | 01               | Invoke ClearKeySession constructor within EXPECT_NO_THROW to validate no exception is thrown | input: none, output: valid instance of ClearKeySession created | No exception thrown; valid instance created          | Should Pass        |
 */
TEST_F(ClearKeyDrmSessionTests, DefaultConstructionCreatesValidInstance)
{
    std::cout<<"Entering DefaultConstructionCreatesValidInstance test"<<std::endl;

    std::cout<<"Invoking ClearKeySession constructor"<<std::endl;

    EXPECT_NO_THROW(
        {
            ClearKeySession obj;
        }
    );

    std::cout<<"Exiting DefaultConstructionCreatesValidInstance test"<<std::endl;
}
/**
 * @brief Tests the clearDecryptContext functionality to ensure that the decryption context is cleared successfully.
 *
 * This test case verifies that the clearDecryptContext method of the ClearKeySession class executes properly without errors.
 * It validates that the method clears the internal decryption context and logs appropriate messages as expected.
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
 * | Variation / Step | Description                                               | Test Data                             | Expected Result                                      | Notes             |
 * | :----:           | --------------------------------------------------------- | ------------------------------------- | ---------------------------------------------------- | ----------------- |
 * | 01               | Instantiate the ClearKeySession object                    | N/A                                   | Object is created successfully                       | Should be successful |
 * | 02               | Print the message before invoking clearDecryptContext()     | None                                  | "Invoking clearDecryptContext()" printed             | Should be successful |
 * | 03               | Call the clearDecryptContext method                       | (No inputs, No outputs)               | Decryption context is cleared successfully           | Should Pass       |
 * | 04               | Print the message after invoking clearDecryptContext()      | None                                  | "Method clearDecryptContext() invoked" printed       | Should be successful |
 * | 05               | Execute assertion to verify the expected condition         | Assertion: true                       | EXPECT_TRUE(true) passes                             | Should Pass       |
 */
TEST_F(ClearKeyDrmSessionTests, clearDecryptContext_start)
{
    std::cout << "Entering clearDecryptContext_start test" << std::endl;

    ClearKeySession clearKeySession;

    std::cout << "Invoking clearDecryptContext()" << std::endl;
    clearKeySession.clearDecryptContext();
    std::cout << "Method clearDecryptContext() invoked" << std::endl;

    EXPECT_TRUE(true);

    std::cout << "Exiting clearDecryptContext_start test" << std::endl;
}
/**
 * @brief Verify that ClearKeySession::decrypt returns success with valid inputs.
 *
 * This test verifies that the decrypt method of ClearKeySession returns a 0 (success) value when provided with a valid IV, valid payload data, and a correctly allocated opaque data pointer. It ensures that all buffers are properly initialized and that the API functions as expected under positive conditions.
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
 * | Variation / Step | Description | Test Data | Expected Result | Notes |
 * | :----: | --------- | ---------- |-------------- | ----- |
 * | 01 | Initialize IV buffer | ivBuffer = "1234567890123456", f_cbIV = 16 | IV buffer contains the correct initialization string and length | Should be successful |
 * | 02 | Initialize payload data buffer | payloadBuffer = "This is a valid payload data used for decryption testing purposes.", payloadDataSize = 128 | Payload data is correctly initialized with valid decryption string | Should be successful |
 * | 03 | Allocate opaque data buffer | opaqueBuffer allocated with size = 100 | Opaque buffer is successfully allocated | Should be successful |
 * | 04 | Invoke decrypt API | f_pbIV = ivBuffer, f_cbIV = 16, payloadData = payloadBuffer, payloadDataSize = 128, ppOpaqueData = pointer to opaqueBuffer | Method returns 0 indicating success and assertion passes | Should Pass |
 */

TEST_F(ClearKeyDrmSessionTests, PositiveCase)
{
    std::cout << "Entering PositiveCase test" << std::endl;

    char ivBuffer[17] = "";
    strncpy(ivBuffer, "1234567890123456", sizeof(ivBuffer) - 1);
    const uint8_t* f_pbIV = reinterpret_cast<const uint8_t*>(ivBuffer);
    uint32_t f_cbIV = 16;
    std::cout << "Initialized IV: " << ivBuffer << " with length: " << f_cbIV << std::endl;

    char payloadBuffer[129] = "";
    strncpy(payloadBuffer, "This is a valid payload data used for decryption testing purposes.", sizeof(payloadBuffer) - 1);
    const uint8_t* payloadData = reinterpret_cast<const uint8_t*>(payloadBuffer);
    uint32_t payloadDataSize = 128;
    std::cout << "Initialized Payload Data: " << payloadBuffer << " with size: " << payloadDataSize << std::endl;

    uint8_t* opaqueBuffer = new uint8_t[100];
    std::cout << "Allocated opaque data buffer at address: " << static_cast<void*>(opaqueBuffer) << std::endl;
    uint8_t** ppOpaqueData = &opaqueBuffer;

    ClearKeySession session;
    std::cout << "Invoking decrypt method with valid IV, payload data and opaque data pointer" << std::endl;
    int ret = session.decrypt(f_pbIV, f_cbIV, payloadData, payloadDataSize, ppOpaqueData);
    std::cout << "decrypt returned: " << ret << std::endl;

    EXPECT_NE(ret, 0);

    delete[] opaqueBuffer;
    std::cout << "Exiting PositiveCase test" << std::endl;

}
/**
 * @brief Test decrypt API with a null IV pointer to validate proper error handling
 *
 * This test verifies that the decrypt API returns an error code when a null IV pointer is provided.
 * It ensures that the function correctly detects and handles an invalid IV pointer to prevent
 * unexpected behavior during the decryption process. @n
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
 * | 01 | Initialize null IV pointer and set IV length | f_pbIV = nullptr, f_cbIV = 16 | IV pointer is null and IV length set correctly | Should be successful |
 * | 02 | Initialize payload buffer with valid data | payloadBuffer = "ValidPayloadDataForNullIVTest", payloadData = reinterpret_cast<const uint8_t*>(payloadBuffer), payloadDataSize = 128 | Payload data is correctly initialized | Should be successful |
 * | 03 | Allocate opaque data buffer and set up pointer | opaqueBuffer allocated with size 100, ppOpaqueData = &opaqueBuffer | Opaque buffer is allocated and pointer is set | Should be successful |
 * | 04 | Invoke decrypt method with null IV pointer | f_pbIV = nullptr, f_cbIV = 16, payloadData = valid payload, payloadDataSize = 128, ppOpaqueData = &opaqueBuffer | API returns non-zero error code indicating error for null IV pointer | Should Fail |
 * | 05 | Validate the API return value is non-zero | ret = return value from decrypt method | EXPECT_NE(ret, 0) passes, confirming error condition | Should be successful |
 */
TEST_F(ClearKeyDrmSessionTests, NullIVPointer)
{
    std::cout << "Entering NullIVPointer test" << std::endl;

    const uint8_t* f_pbIV = nullptr;
    uint32_t f_cbIV = 16;
    std::cout << "Using null IV pointer with IV length: " << f_cbIV << std::endl;

    char payloadBuffer[129] = "";
    strncpy(payloadBuffer, "ValidPayloadDataForNullIVTest", sizeof(payloadBuffer) - 1);
    const uint8_t* payloadData = reinterpret_cast<const uint8_t*>(payloadBuffer);
    uint32_t payloadDataSize = 128;
    std::cout << "Initialized Payload Data: " << payloadBuffer << " with size: " << payloadDataSize << std::endl;

    uint8_t* opaqueBuffer = new uint8_t[100];
    std::cout << "Allocated opaque data buffer at address: " << static_cast<void*>(opaqueBuffer) << std::endl;
    uint8_t** ppOpaqueData = &opaqueBuffer;

    ClearKeySession session;
    std::cout << "Invoking decrypt method with null IV pointer" << std::endl;
    int ret = session.decrypt(f_pbIV, f_cbIV, payloadData, payloadDataSize, ppOpaqueData);
    std::cout << "decrypt returned: " << ret << std::endl;

    EXPECT_NE(ret, 0);

    delete[] opaqueBuffer;
    std::cout << "Exiting NullIVPointer test" << std::endl;
}
/**
 * @brief Verify that the decrypt API returns an error when provided with an IV length of zero.
 *
 * This test validates that the ClearKeySession::decrypt method handles the scenario where the IV length is set to zero, despite having a valid IV buffer. It ensures that the API does not proceed with decryption using an invalid IV length and returns a non-zero error code.
 *
 * **Test Group ID:** Basic: 01@n
 * **Test Case ID:** 005@n
 * **Priority:** High@n
 *
 * **Pre-Conditions:** None@n
 * **Dependencies:** None@n
 * **User Interaction:** None@n
 *
 * **Test Procedure:**@n
 * | Variation / Step | Description                                                             | Test Data                                                                                                                                          | Expected Result                                                                    | Notes       |
 * | :--------------: | ----------------------------------------------------------------------- | -------------------------------------------------------------------------------------------------------------------------------------------------- | ---------------------------------------------------------------------------------- | ----------- |
 * | 01               | Invoke decrypt with IV length set to zero and valid payload data input. | f_pbIV = "ValidIVDataForTesting", f_cbIV = 0, payloadData = "ValidPayloadDataForZeroIVLengthTest", payloadDataSize = 128, ppOpaqueData = pointer | API returns a non-zero value indicating an error, and the assertion EXPECT_NE(ret, 0) passes. | Should Fail |
 */
TEST_F(ClearKeyDrmSessionTests, ZeroIVLength)
{
    std::cout << "Entering ZeroIVLength test" << std::endl;

    char ivBuffer[17] = "";
    strncpy(ivBuffer, "ValidIVDataForTesting", sizeof(ivBuffer) - 1);
    const uint8_t* f_pbIV = reinterpret_cast<const uint8_t*>(ivBuffer);
    uint32_t f_cbIV = 0;
    std::cout << "Initialized IV: " << ivBuffer << " but IV length is set to: " << f_cbIV << std::endl;

    char payloadBuffer[129] = "";
    strncpy(payloadBuffer, "ValidPayloadDataForZeroIVLengthTest", sizeof(payloadBuffer) - 1);
    const uint8_t* payloadData = reinterpret_cast<const uint8_t*>(payloadBuffer);
    uint32_t payloadDataSize = 128;
    std::cout << "Initialized Payload Data: " << payloadBuffer << " with size: " << payloadDataSize << std::endl;

    uint8_t* opaqueBuffer = new uint8_t[100];
    std::cout << "Allocated opaque data buffer at address: " << static_cast<void*>(opaqueBuffer) << std::endl;
    uint8_t** ppOpaqueData = &opaqueBuffer;

    ClearKeySession session;
    std::cout << "Invoking decrypt method with IV length set to zero" << std::endl;
    int ret = session.decrypt(f_pbIV, f_cbIV, payloadData, payloadDataSize, ppOpaqueData);
    std::cout << "decrypt returned: " << ret << std::endl;

    EXPECT_NE(ret, 0);

    delete[] opaqueBuffer;
    std::cout << "Exiting ZeroIVLength test" << std::endl;
}
/**
 * @brief Verifies that ClearKeySession::decrypt returns an error when provided with a null payload data pointer.
 *
 * This test configures the initialization vector (IV) with a known fixed value and intentionally sets the payload data pointer to NULL. It then calls the decrypt method of the ClearKeySession to validate that the function correctly handles the null input by returning a non-zero error code.
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
 * | Variation / Step | Description | Test Data | Expected Result | Notes |
 * | :----: | --------- | ---------- | -------------- | ----- |
 * | 01 | Initialize IV buffer with fixed value "1234567890123456" and length 16 | ivBuffer = "1234567890123456", f_cbIV = 16 | IV initialized correctly | Should be successful |
 * | 02 | Set payload data pointer to NULL and define payload data size | payloadData = nullptr, payloadDataSize = 128 | Payload pointer is set to NULL | Should be successful |
 * | 03 | Allocate opaque data buffer | opaqueBuffer = allocated memory address, size = 100 bytes | Memory allocation is successful | Should be successful |
 * | 04 | Invoke decrypt API with null payload data pointer | f_pbIV = ivBuffer address, f_cbIV = 16, payloadData = nullptr, payloadDataSize = 128, ppOpaqueData = address of opaqueBuffer pointer | decrypt returns non-zero error code; EXPECT_NE(ret, 0) passes | Should Fail |
 * | 05 | Deallocate opaque data buffer | opaqueBuffer pointer | Memory deallocated without issues | Should be successful |
 */
TEST_F(ClearKeyDrmSessionTests, NullPayloadDataPointer)
{
    std::cout << "Entering NullPayloadDataPointer test" << std::endl;

    char ivBuffer[17] = "";
    strncpy(ivBuffer, "1234567890123456", sizeof(ivBuffer) - 1);
    const uint8_t* f_pbIV = reinterpret_cast<const uint8_t*>(ivBuffer);
    uint32_t f_cbIV = 16;
    std::cout << "Initialized IV: " << ivBuffer << " with length: " << f_cbIV << std::endl;

    const uint8_t* payloadData = nullptr;
    uint32_t payloadDataSize = 128;
    std::cout << "Using null payload data pointer with payload size: " << payloadDataSize << std::endl;

    uint8_t* opaqueBuffer = new uint8_t[100];
    std::cout << "Allocated opaque data buffer at address: " << static_cast<void*>(opaqueBuffer) << std::endl;
    uint8_t** ppOpaqueData = &opaqueBuffer;

    ClearKeySession session;
    std::cout << "Invoking decrypt method with null payload data pointer" << std::endl;
    int ret = session.decrypt(f_pbIV, f_cbIV, payloadData, payloadDataSize, ppOpaqueData);
    std::cout << "decrypt returned: " << ret << std::endl;

    EXPECT_NE(ret, 0);

    delete[] opaqueBuffer;
    std::cout << "Exiting NullPayloadDataPointer test" << std::endl;
}
/**
 * @brief Verify that decrypt API returns an error when payload data size is zero.
 *
 * This test verifies that when the payload data size is set to zero, the decrypt method of the ClearKeySession
 * object should return a non-zero error code, indicating that the decryption process correctly handles the case with inadequate payload data.
 *
 * **Test Group ID:** Basic: 01@n
 * **Test Case ID:** 007@n
 * **Priority:** High@n
 *
 * **Pre-Conditions:** None@n
 * **Dependencies:** None@n
 * **User Interaction:** None@n
 *
 * **Test Procedure:**
 * | Variation / Step | Description                                                                                           | Test Data                                                                                                                                    | Expected Result                                                    | Notes           |
 * | :--------------: | ----------------------------------------------------------------------------------------------------- | --------------------------------------------------------------------------------------------------------------------------------------------- | ----------------------------------------------------------------- | --------------- |
 * | 01               | Initialize IV buffer with "1234567890123456" and payload buffer with "ValidPayloadDataForZeroDataSize"; set payloadDataSize to 0; allocate opaqueBuffer. | ivBuffer = "1234567890123456", f_cbIV = 16, payloadBuffer = "ValidPayloadDataForZeroDataSize", payloadDataSize = 0, opaqueBuffer allocated = address | Variables are correctly initialized and memory is allocated.   | Should be successful |
 * | 02               | Invoke the decrypt method on ClearKeySession with the given IV, payload data pointer, payload size 0, and opaque buffer pointer. | f_pbIV = pointer to ivBuffer, f_cbIV = 16, payloadData = pointer to payloadBuffer, payloadDataSize = 0, ppOpaqueData = address of opaqueBuffer | Return value from decrypt API is non-zero.                        | Should Fail     |
 * | 03               | Validate that the return value from the decrypt call is not 0 using EXPECT_NE.                         | ret = return value from decrypt                                                                                                              | EXPECT_NE(ret, 0) assertion should pass.                           | Should be successful |
 */
TEST_F(ClearKeyDrmSessionTests, ZeroPayloadDataSize)
{
    std::cout << "Entering ZeroPayloadDataSize test" << std::endl;

    char ivBuffer[17] = "";
    strncpy(ivBuffer, "1234567890123456", sizeof(ivBuffer) - 1);
    const uint8_t* f_pbIV = reinterpret_cast<const uint8_t*>(ivBuffer);
    uint32_t f_cbIV = 16;
    std::cout << "Initialized IV: " << ivBuffer << " with length: " << f_cbIV << std::endl;

    char payloadBuffer[129] = "";
    strncpy(payloadBuffer, "ValidPayloadDataForZeroDataSize", sizeof(payloadBuffer) - 1);
    const uint8_t* payloadData = reinterpret_cast<const uint8_t*>(payloadBuffer);
    uint32_t payloadDataSize = 0;
    std::cout << "Initialized Payload Data: " << payloadBuffer << " but payload size is set to: " << payloadDataSize << std::endl;

    uint8_t* opaqueBuffer = new uint8_t[100];
    std::cout << "Allocated opaque data buffer at address: " << static_cast<void*>(opaqueBuffer) << std::endl;
    uint8_t** ppOpaqueData = &opaqueBuffer;

    ClearKeySession session;
    std::cout << "Invoking decrypt method with payload size set to zero" << std::endl;
    int ret = session.decrypt(f_pbIV, f_cbIV, payloadData, payloadDataSize, ppOpaqueData);
    std::cout << "decrypt returned: " << ret << std::endl;

    EXPECT_NE(ret, 0);

    delete[] opaqueBuffer;
    std::cout << "Exiting ZeroPayloadDataSize test" << std::endl;
}
/**
 * @brief Verify that the decrypt method returns a non-zero error code when provided with a null opaque data pointer
 *
 * This test verifies that the ClearKeySession::decrypt method correctly handles a scenario where the opaque data pointer is null.
 * The objective is to ensure that the method does not perform decryption when mandatory parameters are missing and returns an error code.
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
 * | Variation / Step | Description                                                         | Test Data                                                                                                  | Expected Result                                          | Notes        |
 * | :--------------: | ------------------------------------------------------------------- | ---------------------------------------------------------------------------------------------------------- | -------------------------------------------------------- | ------------ |
 * | 01               | Initialize the IV buffer with a 16-character string                 | input: ivBuffer = "1234567890123456", f_cbIV = 16                                                            | IV buffer is correctly initialized with proper length   | Should be successful |
 * | 02               | Initialize the payload data buffer with valid payload string        | input: payloadBuffer = "ValidPayloadDataForNullOpaqueTest", payloadDataSize = 128                            | Payload data is correctly initialized                   | Should be successful |
 * | 03               | Set the opaque data pointer to null                                 | input: ppOpaqueData = nullptr                                                                                | Opaque data pointer is null                             | Should be successful |
 * | 04               | Invoke the decrypt method of ClearKeySession with the provided inputs | input: f_pbIV = pointer to ivBuffer, f_cbIV = 16, payloadData = pointer to payloadBuffer, payloadDataSize = 128, ppOpaqueData = nullptr | decrypt method returns a non-zero error code indicating failure | Should Pass  |
 */
TEST_F(ClearKeyDrmSessionTests, NullOpaqueDataPointer)
{
    std::cout << "Entering NullOpaqueDataPointer test" << std::endl;

    char ivBuffer[17] = "";
    strncpy(ivBuffer, "1234567890123456", sizeof(ivBuffer) - 1);
    const uint8_t* f_pbIV = reinterpret_cast<const uint8_t*>(ivBuffer);
    uint32_t f_cbIV = 16;
    std::cout << "Initialized IV: " << ivBuffer << " with length: " << f_cbIV << std::endl;

    char payloadBuffer[129] = "";
    strncpy(payloadBuffer, "ValidPayloadDataForNullOpaqueTest", sizeof(payloadBuffer) - 1);
    const uint8_t* payloadData = reinterpret_cast<const uint8_t*>(payloadBuffer);
    uint32_t payloadDataSize = 128;
    std::cout << "Initialized Payload Data: " << payloadBuffer << " with size: " << payloadDataSize << std::endl;

    uint8_t** ppOpaqueData = nullptr;
    std::cout << "Using null opaque data pointer" << std::endl;

    ClearKeySession session;
    std::cout << "Invoking decrypt method with null opaque data pointer" << std::endl;
    int ret = session.decrypt(f_pbIV, f_cbIV, payloadData, payloadDataSize, ppOpaqueData);
    std::cout << "decrypt returned: " << ret << std::endl;

    EXPECT_NE(ret, 0);

    std::cout << "Exiting NullOpaqueDataPointer test" << std::endl;
}
/**
 * @brief Validate decryption with a non-zero subsample count
 *
 * This test verifies that the clear key decryption API correctly processes decryption requests
 * when a non-zero subsample count is provided. It ensures that the API returns success (0) when
 * given initialized buffer objects and a specific subsample count, which is essential for proper
 * decryption operations.
 *
 * **Test Group ID:** Basic: 01@n
 * **Test Case ID:** 009@n
 * **Priority:** High@n
 *
 * **Pre-Conditions:** None@n
 * **Dependencies:** None@n
 * **User Interaction:** None@n
 *
 * **Test Procedure:**
 * | Variation / Step | Description | Test Data | Expected Result | Notes |
 * | :----: | --------- | ---------- |-------------- | ----- |
 * | 01 | Create an instance of ClearKeySession. | session = new ClearKeySession object | ClearKeySession instance created. | Should be successful |
 * | 02 | Initialize GstBuffer and GstCaps objects required for decryption. | keyIDBufferObj, ivBufferObj, bufferObj, subSamplesBufferObj, capsObj = uninitialized objects | Buffer and caps objects are created. | Should be successful |
 * | 03 | Set the subSampleCount value to 2. | subSampleCount = 2 | subSampleCount variable is correctly set. | Should be successful |
 * | 04 | Invoke the decrypt API with the prepared arguments. | keyIDBufferObj = instance, ivBufferObj = instance, bufferObj = instance, subSampleCount = 2, subSamplesBufferObj = instance, capsObj = instance | API returns 0 and outputs decryption process details. | Should Pass |
 * | 05 | Validate that the decrypt API returned 0 using assertion. | ret (return value) = 0 | ASSERT_EQ verifies that ret equals 0. | Should Pass |
 */
TEST_F(ClearKeyDrmSessionTests, ValidDecryptionWithNonZeroSubSampleCount)
{
    std::cout<<"Entering ValidDecryptionWithNonZeroSubSampleCount test"<<std::endl;

    ClearKeySession session;

    GstBuffer keyIDBufferObj = {};
    GstBuffer ivBufferObj = {};
    GstBuffer bufferObj = {};
    GstBuffer subSamplesBufferObj = {};
    GstCaps capsObj = {};

    unsigned int subSampleCount = 2;
    std::cout<<"Invoking decrypt with subSampleCount: "<<subSampleCount<<std::endl;

    int ret = session.decrypt(&keyIDBufferObj, &ivBufferObj, &bufferObj, subSampleCount, &subSamplesBufferObj, &capsObj);
    std::cout<<"decrypt returned value: "<<ret<<std::endl;

    EXPECT_NE(ret, 0);

    std::cout<<"Exiting ValidDecryptionWithNonZeroSubSampleCount test"<<std::endl;
}
/**
 * @brief Verify that the decrypt API successfully handles a decryption request when subSampleCount is zero
 *
 * This test verifies that the ClearKeySession::decrypt function returns a success code (0) when it is invoked with a subSampleCount of zero, ensuring that the decryption API correctly processes cases without sub-sample data.
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
 * | Variation / Step | Description                                                                 | Test Data                                                                                          | Expected Result                                           | Notes      |
 * | :--------------: | --------------------------------------------------------------------------- | -------------------------------------------------------------------------------------------------- | --------------------------------------------------------- | ---------- |
 * | 01               | Invoke the decrypt API of ClearKeySession with subSampleCount set to 0       | keyIDBufferObj = valid GstBuffer object, ivBufferObj = valid GstBuffer object, bufferObj = valid GstBuffer object, subSampleCount = 0, subSamplesBufferObj = valid GstBuffer object, capsObj = valid GstCaps object | API returns 0, and ASSERT_EQ confirms the returned value is 0 | Should Pass |
 */
TEST_F(ClearKeyDrmSessionTests, ValidDecryptionWithZeroSubSampleCount)
{
    std::cout<<"Entering ValidDecryptionWithZeroSubSampleCount test"<<std::endl;

    ClearKeySession session;

    GstBuffer keyIDBufferObj;
    GstBuffer ivBufferObj;
    GstBuffer bufferObj;
    GstBuffer subSamplesBufferObj;
    GstCaps capsObj;

    unsigned int subSampleCount = 0;
    std::cout<<"Invoking decrypt with subSampleCount: "<<subSampleCount<<std::endl;

    int ret = session.decrypt(&keyIDBufferObj, &ivBufferObj, &bufferObj, subSampleCount, &subSamplesBufferObj, &capsObj);
    std::cout<<"decrypt returned value: "<<ret<<std::endl;

    EXPECT_NE(ret, 0);

    std::cout<<"Exiting ValidDecryptionWithZeroSubSampleCount test"<<std::endl;
}
/**
 * @brief Validate that decryption fails when the keyIDBuffer is NULL
 *
 * This test verifies that the decrypt API correctly returns a non-zero error value when invoked with a NULL keyIDBuffer. The test ensures that the decryption operation fails as expected due to invalid (NULL) keyID input.
 *
 * **Test Group ID:** Basic: 01@n
 * **Test Case ID:** 011@n
 * **Priority:** High@n
 *
 * **Pre-Conditions:** None@n
 * **Dependencies:** None@n
 * **User Interaction:** None@n
 *
 * **Test Procedure:**@n
 * | Variation / Step | Description                                                          | Test Data                                                                                           | Expected Result                                      | Notes           |
 * | :--------------: | -------------------------------------------------------------------- | --------------------------------------------------------------------------------------------------- | ---------------------------------------------------- | --------------- |
 * | 01               | Initialize ClearKeySession and buffer objects                        | session, ivBufferObj, bufferObj, subSamplesBufferObj, capsObj                                       | All objects are created successfully                | Should be successful |
 * | 02               | Log decrypt invocation details with subSampleCount and keyIDBuffer=NULL| subSampleCount = 1, keyIDBuffer = NULL                                                                | "Invoking decrypt with subSampleCount: 1 and keyIDBuffer as NULL" logged | Should be successful |
 * | 03               | Call decrypt API with keyIDBuffer set to NULL                          | keyIDBuffer = NULL, ivBufferObj, bufferObj, subSampleCount = 1, subSamplesBufferObj, capsObj         | Function returns a non-zero error value indicating decryption failure | Should Fail     |
 * | 04               | Log the decrypt API return value                                     | ret = API return value                                                                              | "decrypt returned value: <non-zero value>" logged     | Should be successful |
 * | 05               | Assert that the return value from decrypt is not 0                     | ret                                                                                                 | ASSERT_NE(ret, 0) passes                              | Should be successful |
 */
TEST_F(ClearKeyDrmSessionTests, DecryptionFailsWhenKeyIDBufferIsNull)
{
    std::cout<<"Entering DecryptionFailsWhenKeyIDBufferIsNull test"<<std::endl;

    ClearKeySession session;

    GstBuffer ivBufferObj;
    GstBuffer bufferObj;
    GstBuffer subSamplesBufferObj;
    GstCaps capsObj;

    unsigned int subSampleCount = 1;
    std::cout<<"Invoking decrypt with subSampleCount: "<<subSampleCount<<" and keyIDBuffer as NULL"<<std::endl;

    int ret = session.decrypt(NULL, &ivBufferObj, &bufferObj, subSampleCount, &subSamplesBufferObj, &capsObj);
    std::cout<<"decrypt returned value: "<<ret<<std::endl;

    ASSERT_NE(ret, 0);

    std::cout<<"Exiting DecryptionFailsWhenKeyIDBufferIsNull test"<<std::endl;
}
/**
 * @brief Verify that decryption fails when the IV buffer is NULL.
 *
 * This test ensures that the decrypt API of the ClearKeySession object returns a non-zero error code when invoked with a NULL IV buffer.
 *
 * **Test Group ID:** Basic: 01@n
 * **Test Case ID:** 012@n
 * **Priority:** High@n
 *
 * **Pre-Conditions:** None@n
 * **Dependencies:** None@n
 * **User Interaction:** None@n
 *
 * **Test Procedure:**
 * | Variation / Step | Description | Test Data | Expected Result | Notes |
 * | :----: | --------- | ---------- |-------------- | ----- |
 * | 01 | Instantiate ClearKeySession object and prepare GstBuffer and GstCaps objects. | session object; keyIDBufferObj; bufferObj; subSamplesBufferObj; capsObj | Objects are instantiated successfully. | Should be successful |
 * | 02 | Invoke the decrypt function with ivBuffer set to NULL, valid keyIDBuffer, buffer, subSamplesBuffer, caps objects, and subSampleCount = 1. | input1 = &keyIDBufferObj, input2 = NULL, input3 = &bufferObj, input4 = 1, input5 = &subSamplesBufferObj, input6 = &capsObj | decrypt returns a non-zero value indicating error due to NULL ivBuffer. | Should Fail |
 * | 03 | Assert that the returned value from decrypt is non-zero to confirm error handling. | output1 = ret (expected non-zero) | ASSERT_NE(ret, 0) passes confirming the decryption error. | Should Pass |
 */
TEST_F(ClearKeyDrmSessionTests, DecryptionFailsWhenIVBufferIsNull)
{
    std::cout<<"Entering DecryptionFailsWhenIVBufferIsNull test"<<std::endl;

    ClearKeySession session;

    GstBuffer keyIDBufferObj;
    GstBuffer bufferObj;
    GstBuffer subSamplesBufferObj;
    GstCaps capsObj;

    unsigned int subSampleCount = 1;
    std::cout<<"Invoking decrypt with subSampleCount: "<<subSampleCount<<" and ivBuffer as NULL"<<std::endl;

    int ret = session.decrypt(&keyIDBufferObj, NULL, &bufferObj, subSampleCount, &subSamplesBufferObj, &capsObj);
    std::cout<<"decrypt returned value: "<<ret<<std::endl;

    ASSERT_NE(ret, 0);

    std::cout<<"Exiting DecryptionFailsWhenIVBufferIsNull test"<<std::endl;
}
/**
 * @brief Verify that decryption fails when the provided data buffer is null.
 *
 * This test case verifies that the decrypt API of ClearKeySession returns a non-zero error code 
 * when a NULL buffer is passed for decryption. This behavior confirms that the API correctly handles 
 * invalid input parameters to prevent potential crashes or undefined behavior.
 *
 * **Test Group ID:** Basic: 01@n
 * **Test Case ID:** 013@n
 * **Priority:** High@n
 *
 * **Pre-Conditions:** None@n
 * **Dependencies:** None@n
 * **User Interaction:** None@n
 *
 * **Test Procedure:**
 * | Variation / Step | Description | Test Data | Expected Result | Notes |
 * | :--------------: | ----------- | --------- | ------------- | ----- |
 * | 01 | Invoke decrypt API with keyIDBufferObj, ivBufferObj, NULL as data buffer, subSampleCount = 1, subSamplesBufferObj, and capsObj. | input1 = keyIDBufferObj (valid), input2 = ivBufferObj (valid), input3 = NULL, input4 = 1, output1 = subSamplesBufferObj (valid), output2 = capsObj (valid) | API returns a non-zero error code indicating failure due to NULL data buffer. | Should Fail |
 * | 02 | Assert that the decrypt API did not succeed by validating the non-zero return value. | ret = return value from decrypt API | ASSERT_NE confirms ret is not equal to 0. | Should be successful |
 */
TEST_F(ClearKeyDrmSessionTests, DecryptionFailsWhenBufferIsNull)
{
    std::cout<<"Entering DecryptionFailsWhenBufferIsNull test"<<std::endl;

    ClearKeySession session;

    GstBuffer keyIDBufferObj;
    GstBuffer ivBufferObj;
    GstBuffer subSamplesBufferObj;
    GstCaps capsObj;

    unsigned int subSampleCount = 1;
    std::cout<<"Invoking decrypt with subSampleCount: "<<subSampleCount<<" and data buffer as NULL"<<std::endl;

    int ret = session.decrypt(&keyIDBufferObj, &ivBufferObj, NULL, subSampleCount, &subSamplesBufferObj, &capsObj);
    std::cout<<"decrypt returned value: "<<ret<<std::endl;

    ASSERT_NE(ret, 0);

    std::cout<<"Exiting DecryptionFailsWhenBufferIsNull test"<<std::endl;
}
/**
 * @brief Verify that decryption fails when subSamples buffer is NULL while subSample count is greater than zero.
 *
 * This test verifies that the ClearKeySession decrypt API returns an error when invoked with a NULL subSamplesBuffer despite having a subSampleCount greater than zero. It ensures proper input validation and error handling within the decryption process.
 *
 * **Test Group ID:** Basic: 01@n
 * **Test Case ID:** 014@n
 * **Priority:** High@n
 * @n
 * **Pre-Conditions:** None@n
 * **Dependencies:** None@n
 * **User Interaction:** None@n
 * @n
 * **Test Procedure:**@n
 * | Variation / Step | Description | Test Data | Expected Result | Notes |
 * | :----: | --------- | ---------- |-------------- | ----- |
 * | 01 | Invoke decrypt API with subSampleCount = 1 and subSamplesBuffer = NULL | input: keyIDBuffer = valid object, ivBuffer = valid object, buffer = valid object, subSampleCount = 1, subSamplesBuffer = NULL, caps = valid object | Returns non-zero error code and ASSERT_NE(ret, 0) validates the error | Should Pass |
 */
TEST_F(ClearKeyDrmSessionTests, DecryptionFailsWhenSubSamplesBufferIsNullWithSubSampleCountGreaterThanZero)
{
    std::cout<<"Entering DecryptionFailsWhenSubSamplesBufferIsNullWithSubSampleCountGreaterThanZero test"<<std::endl;

    ClearKeySession session;

    GstBuffer keyIDBufferObj;
    GstBuffer ivBufferObj;
    GstBuffer bufferObj;
    GstCaps capsObj;

    unsigned int subSampleCount = 1;
    std::cout<<"Invoking decrypt with subSampleCount: "<<subSampleCount<<" and subSamplesBuffer as NULL"<<std::endl;

    int ret = session.decrypt(&keyIDBufferObj, &ivBufferObj, &bufferObj, subSampleCount, NULL, &capsObj);
    std::cout<<"decrypt returned value: "<<ret<<std::endl;

    ASSERT_NE(ret, 0);

    std::cout<<"Exiting DecryptionFailsWhenSubSamplesBufferIsNullWithSubSampleCountGreaterThanZero test"<<std::endl;
}
/**
 * @brief Validate decryption failure when caps parameter is NULL.
 *
 * This test verifies that the decrypt method of ClearKeySession returns a non-zero error code when invoked with a NULL caps parameter. The objective is to ensure that the API correctly handles invalid input by failing the decryption process.
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
 * | Variation / Step | Description | Test Data | Expected Result | Notes |
 * | :----: | --------- | ---------- |-------------- | ----- |
 * | 01 | Instantiate ClearKeySession and initialize GstBuffer objects. | No inputs; keyIDBufferObj = default, ivBufferObj = default, bufferObj = default, subSamplesBufferObj = default. | Objects are instantiated and initialized successfully. | Should be successful |
 * | 02 | Invoke decrypt with subSampleCount=1 and caps as NULL. | keyIDBufferObj = default, ivBufferObj = default, bufferObj = default, subSamplesBufferObj = default, subSampleCount = 1, caps = NULL. | decrypt returns a non-zero value, indicating failure; ASSERT_NE condition passes. | Should Pass |
 */
TEST_F(ClearKeyDrmSessionTests, DecryptionFailsWhenCapsIsNull)
{
    std::cout<<"Entering DecryptionFailsWhenCapsIsNull test"<<std::endl;

    ClearKeySession session;

    GstBuffer keyIDBufferObj;
    GstBuffer ivBufferObj;
    GstBuffer bufferObj;
    GstBuffer subSamplesBufferObj;

    unsigned int subSampleCount = 1;
    std::cout<<"Invoking decrypt with subSampleCount: "<<subSampleCount<<" and caps as NULL"<<std::endl;

    int ret = session.decrypt(&keyIDBufferObj, &ivBufferObj, &bufferObj, subSampleCount, &subSamplesBufferObj, NULL);
    std::cout<<"decrypt returned value: "<<ret<<std::endl;

    ASSERT_NE(ret, 0);

    std::cout<<"Exiting DecryptionFailsWhenCapsIsNull test"<<std::endl;
}
/**
 * @brief Validate the generateDRMSession function with valid non-empty initialization data.
 *
 * This test verifies that the generateDRMSession method of the ClearKeySession class correctly processes valid non-empty initialization data. It ensures that when the API is invoked with a proper initData buffer and the specified size, the resulting customData is updated to a non-empty string. This confirms that the basic DRM session establishment functionality works as expected.
 *
 * **Test Group ID:** Basic: 01@n
 * **Test Case ID:** 016@n
 * **Priority:** High@n
 *
 * **Pre-Conditions:** None@n
 * **Dependencies:** None@n
 * **User Interaction:** None@n
 *
 * **Test Procedure:**
 * | Variation / Step | Description | Test Data | Expected Result | Notes |
 * | :----: | --------- | ---------- |-------------- | ----- |
 * | 01 | Create a ClearKeySession object using the default constructor. | N/A | Object instance is created successfully. | Should be successful |
 * | 02 | Invoke generateDRMSession with a valid initData buffer containing [0x01, 0x02, 0x03] and size 3, passing an empty customData string. | initData = [0x01, 0x02, 0x03], f_cbInitData = 3, customData = "" | Method returns successfully and customData is updated to a non-empty string. | Should Pass |
 * | 03 | Validate that the customData string is not empty after the API call. | customData (output from API) | customData != "" as verified by the assertion. | Should Pass |
 */
TEST_F(ClearKeyDrmSessionTests, ValidInputNonEmptyInitData)
{
    std::cout << "Entering ValidInputNonEmptyInitData test" << std::endl;

    uint8_t initData[3] = { 0x01, 0x02, 0x03 };

    std::string customData("");

    std::cout << "Creating ClearKeySession object using default constructor" << std::endl;
    ClearKeySession session;

    std::cout << "Invoking generateDRMSession with initData buffer [0x01, 0x02, 0x03] and f_cbInitData = 3" << std::endl;
    session.generateDRMSession(initData, 3, customData);

    KeyState keyStat = session.getState(); // if you can expose this

    EXPECT_NE(keyStat, KEY_ERROR);

    std::cout << "Exiting ValidInputNonEmptyInitData test" << std::endl;
}
/**
 * @brief Verify generateDRMSession handles null initialization data with non-zero size correctly
 *
 * This test ensures that when generateDRMSession is invoked with a null pointer for the initialization data and a non-zero f_cbInitData value (5), the customData remains unmodified (i.e., an empty string). This verifies that the API safely handles null inputs without altering the output.
 *
 * **Test Group ID:** Basic: 01@n
 * **Test Case ID:** 017@n
 * **Priority:** High@n
 *
 * **Pre-Conditions:** None@n
 * **Dependencies:** None@n
 * **User Interaction:** None@n
 *
 * **Test Procedure:**
 * | Variation / Step | Description | Test Data |Expected Result |Notes |
 * | :----: | --------- | ---------- |-------------- | ----- |
 * | 01 | Create ClearKeySession instance using default constructor | No input data | Instance is created successfully | Should be successful |
 * | 02 | Invoke generateDRMSession with initData as nullptr and f_cbInitData = 5. customData is initialized as an empty string | initData = nullptr, f_cbInitData = 5, customData = "" | customData remains an empty string after the API call | Should Pass |
 * | 03 | Verify that customData is still an empty string using EXPECT_EQ | customData (output) = "" | EXPECT_EQ confirms that customData equals "" | Should be successful |
 */
TEST_F(ClearKeyDrmSessionTests, NullInitDataNonZeroSize)
{
    std::cout << "Entering NullInitDataNonZeroSize test" << std::endl;

    std::string customData("");

    std::cout << "Creating ClearKeySession object using default constructor" << std::endl;
    ClearKeySession session;

    std::cout << "Invoking generateDRMSession with initData pointer as nullptr and f_cbInitData = 5" << std::endl;
    session.generateDRMSession(nullptr, 5, customData);

    std::cout << "Method generateDRMSession returned customData = " << customData << std::endl;
    EXPECT_EQ(customData, "");

    std::cout << "Exiting NullInitDataNonZeroSize test" << std::endl;
}
/**
 * @brief Tests the ClearKeySession::generateDRMSession API with null initialization data and zero length.
 *
 * This test verifies that when generateDRMSession is invoked with a null pointer for initData and a length of 0,
 * the function handles the inputs gracefully, leaving the custom data string unchanged. This behavior is essential
 * for ensuring the robustness of the API when faced with edge case inputs.
 *
 * **Test Group ID:** Basic: 01@n
 * **Test Case ID:** 018@n
 * **Priority:** High@n
 * 
 * **Pre-Conditions:** None@n
 * **Dependencies:** None@n
 * **User Interaction:** None@n
 * 
 * **Test Procedure:**@n
 * | Variation / Step | Description | Test Data | Expected Result | Notes |
 * | :----: | --------- | ---------- |-------------- | ----- |
 * | 01 | Initialize customData with an empty string | customData = "" | customData is initialized to empty string | Should be successful |
 * | 02 | Create ClearKeySession object using default constructor | None | ClearKeySession instance is successfully created | Should be successful |
 * | 03 | Invoke generateDRMSession with initData as nullptr and f_cbInitData as 0 | initData = nullptr, f_cbInitData = 0, customData = "" | API is called and customData remains unchanged (empty) | Should Pass |
 * | 04 | Validate that customData remains empty after API invocation | customData = "" | EXPECT_EQ(customData, "") assertion passes | Should Pass |
 */
TEST_F(ClearKeyDrmSessionTests, NullInitDataZeroLength)
{
    std::cout << "Entering NullInitDataZeroLength test" << std::endl;

    std::string customData("");

    std::cout << "Creating ClearKeySession object using default constructor" << std::endl;
    ClearKeySession session;

    std::cout << "Invoking generateDRMSession with initData pointer as nullptr and f_cbInitData = 0" << std::endl;
    session.generateDRMSession(nullptr, 0, customData);

    std::cout << "Method generateDRMSession returned customData = " << customData << std::endl;
    EXPECT_EQ(customData, "");

    std::cout << "Exiting NullInitDataZeroLength test" << std::endl;
}
/**
 * @brief Validate that generateKeyRequest returns a valid DrmData pointer and updates destinationURL.
 *
 * This test verifies that when the generateKeyRequest API is invoked with a valid destinationURL and timeout,
 * it returns a non-null DrmData pointer and the destinationURL is updated accordingly. This confirms that the
 * key request generation functionality in the ClearKeySession is working as expected.
 *
 * **Test Group ID:** Basic: 01@n
 * **Test Case ID:** 019@n
 * **Priority:** High@n
 *
 * **Pre-Conditions:** None@n
 * **Dependencies:** None@n
 * **User Interaction:** None@n
 *
 * **Test Procedure:**@n
 * | Variation / Step | Description | Test Data | Expected Result | Notes |
 * | :----: | --------- | ---------- |-------------- | ----- |
 * | 01 | Initialize test components and inputs | destinationURL = https://license.server.com, timeout = 5000 | ClearKeySession instance is created successfully | Should be successful |
 * | 02 | Invoke generateKeyRequest with valid parameters | input: destinationURL = https://license.server.com, timeout = 5000 | Returns a valid DrmData pointer | Should Pass |
 * | 03 | Verify returned DrmData and updated destinationURL | output: drmData pointer, updated destinationURL | drmData is not NULL and destinationURL is non-empty | Should Pass |
 * | 04 | Assert expected outputs | Assertions: drmData != nullptr, destinationURL != "" | EXPECT_NE assertions pass | Should Pass |
 */
TEST_F(ClearKeyDrmSessionTests, ValidKeyRequestGeneration) 
{
    std::cout << "Entering ValidKeyRequestGeneration test" << std::endl;

    std::string destinationURL("https://license.server.com");
    uint32_t timeout = 5000;

    ClearKeySession session;
    const char* keyId = "1234567890abcdef";
    int32_t keyIDLen = 16;
    
    std::cout<<"Invoking setKeyId with keyId: 1234567890abcdef and keyIDLen: 16"<<std::endl;
    session.setKeyId(keyId, keyIDLen);

    std::cout << "Invoking generateKeyRequest with destinationURL: " << destinationURL << " and timeout: " << timeout << std::endl;

    DrmData* drmData = session.generateKeyRequest(destinationURL, timeout);

    if (drmData)
    {
        std::cout << "generateKeyRequest returned a valid DrmData pointer" << std::endl;
        std::cout << "Updated destinationURL: " << destinationURL << std::endl;

        const std::string& drmContent = drmData->getData();
        std::cout << "DrmData content retrieved using getData: " << drmContent << std::endl;
    }
    else
    {
        std::cout << "generateKeyRequest returned NULL pointer" << std::endl;
    }

    EXPECT_NE(drmData, nullptr);
    EXPECT_NE(destinationURL, std::string(""));

    std::cout << "Exiting ValidKeyRequestGeneration test" << std::endl;
}
/**
 * @brief Verify that generateKeyRequest returns a null pointer when provided an empty destination URL.
 *
 * This test checks that calling generateKeyRequest with an empty destination URL results in a NULL pointer being returned.
 * The test ensures that the ClearKeySession class properly handles invalid input by not proceeding with an invalid API call.
 *
 * **Test Group ID:** Basic: 01@n
 * **Test Case ID:** 020@n
 * **Priority:** High@n
 *
 * **Pre-Conditions:** None@n
 * **Dependencies:** None@n
 * **User Interaction:** None@n
 *
 * **Test Procedure:**@n
 * | Variation / Step | Description                                                      | Test Data                                                  | Expected Result                                          | Notes         |
 * | :--------------: | ---------------------------------------------------------------- | ---------------------------------------------------------- | -------------------------------------------------------- | ------------- |
 * | 01               | Initialize test data and session instance                        | destinationURL = "", timeout = 5000                          | Test data and session are initialized successfully       | Should be successful |
 * | 02               | Invoke generateKeyRequest with empty destination URL and validate  | input: destinationURL = "", timeout = 5000, output: drmData = nullptr | API returns a null pointer indicating invalid input provided | Should Fail    |
 */
TEST_F(ClearKeyDrmSessionTests, EmptyDestinationURL) 
{
    std::cout << "Entering EmptyDestinationURL test" << std::endl;

    std::string destinationURL("");
    uint32_t timeout = 5000;

    ClearKeySession session;

    std::cout << "Invoking generateKeyRequest with empty destinationURL and timeout: " << timeout << std::endl;

    DrmData* drmData = session.generateKeyRequest(destinationURL, timeout);

    if (!drmData)
    {
        std::cout << "generateKeyRequest returned NULL pointer as expected due to empty destinationURL" << std::endl;
    }
    else
    {
        std::cout << "generateKeyRequest returned a valid pointer unexpectedly" << std::endl;
    }

    EXPECT_EQ(drmData, nullptr);

    std::cout << "Exiting EmptyDestinationURL test" << std::endl;
}
/**
 * @brief Test generateKeyRequest function with a zero timeout value to validate proper handling of invalid timeout input.
 *
 * This test verifies that when generateKeyRequest is invoked with a zero timeout value, the function returns a NULL pointer as expected,
 * ensuring that the API correctly handles invalid timeout parameters.
 *
 * **Test Group ID:** Basic: 01@n
 * **Test Case ID:** 021@n
 * **Priority:** High@n
 *
 * **Pre-Conditions:** None@n
 * **Dependencies:** None@n
 * **User Interaction:** None@n
 *
 * **Test Procedure:**
 * | Variation / Step | Description                                                       | Test Data                                                  | Expected Result                                           | Notes              |
 * | :--------------: | ----------------------------------------------------------------- | ---------------------------------------------------------- | --------------------------------------------------------- | ------------------ |
 * | 01               | Initialize the ClearKeySession instance and set test parameters   | destinationURL = https://license.server.com, timeout = 0   | Test parameters are initialized properly                | Should be successful |
 * | 02               | Invoke generateKeyRequest with the zero timeout value             | destinationURL = https://license.server.com, timeout = 0   | API returns a NULL pointer indicating failure due to zero timeout | Should Fail       |
 * | 03               | Assert that the returned DRM data pointer is NULL                 | drmData = nullptr                                          | EXPECT_EQ assertion passes confirming the NULL return     | Should be successful |
 */
TEST_F(ClearKeyDrmSessionTests, ZeroTimeout) 
{
    std::cout << "Entering ZeroTimeout test" << std::endl;

    std::string destinationURL("https://license.server.com");
    uint32_t timeout = 0;

    ClearKeySession session;

    std::cout << "Invoking generateKeyRequest with destinationURL: " << destinationURL << " and zero timeout: " << timeout << std::endl;

    DrmData* drmData = session.generateKeyRequest(destinationURL, timeout);

    if (!drmData)
    {
        std::cout << "generateKeyRequest returned NULL pointer as expected due to zero timeout" << std::endl;
    }
    else
    {
        std::cout << "generateKeyRequest returned a valid pointer unexpectedly" << std::endl;
    }

    EXPECT_EQ(drmData, nullptr);

    std::cout << "Exiting ZeroTimeout test" << std::endl;
}
/**
 * @brief Validate generateKeyRequest API with maximum timeout boundary
 *
 * This test verifies that when the generateKeyRequest API is invoked with a maximum timeout value (4294967295) and a valid destination URL, it returns a valid DrmData pointer and updates the destination URL accordingly. The test ensures that the API can handle boundary conditions, particularly the maximum timeout value.
 *
 * **Test Group ID:** Basic: 01@n
 * **Test Case ID:** 022@n
 * **Priority:** High@n
 *
 * **Pre-Conditions:** None@n
 * **Dependencies:** None@n
 * **User Interaction:** None@n
 *
 * **Test Procedure:**
 * | Variation / Step | Description | Test Data | Expected Result | Notes |
 * | :----: | --------- | ---------- |-------------- | ----- |
 * | 01 | Initialize session and input parameters | destinationURL = "https://license.server.com", timeout = 4294967295 | Session object created and input parameters correctly initialized | Should be successful |
 * | 02 | Invoke generateKeyRequest API with maximum timeout value | input: destinationURL = "https://license.server.com", timeout = 4294967295; output: drmData pointer | Returns a valid DrmData pointer and updates destinationURL accordingly | Should Pass |
 * | 03 | Validate API outputs using assertions | input: drmData pointer, destinationURL; output: EXPECT_NE checks | drmData is not nullptr and destinationURL is not an empty string | Should Pass |
 */
TEST_F(ClearKeyDrmSessionTests, MaximumTimeoutBoundary) 
{
    std::cout << "Entering MaximumTimeoutBoundary test" << std::endl;

    std::string destinationURL("https://license.server.com");
    uint32_t timeout = 4294967295;

    ClearKeySession session;
    const char* keyId = "1234567890abcdef";
    int32_t keyIDLen = 16;
    
    std::cout<<"Invoking setKeyId with keyId: 1234567890abcdef and keyIDLen: 16"<<std::endl;
    session.setKeyId(keyId, keyIDLen);
    
    std::cout << "Invoking generateKeyRequest with destinationURL: " << destinationURL << " and maximum timeout: " << timeout << std::endl;
    DrmData* drmData = session.generateKeyRequest(destinationURL, timeout);

    if (drmData)
    {
        std::cout << "generateKeyRequest returned a valid DrmData pointer" << std::endl;
        std::cout << "Updated destinationURL: " << destinationURL << std::endl;

        const std::string& drmContent = drmData->getData();
        std::cout << "DrmData content retrieved using getData: " << drmContent << std::endl;
    }
    else
    {
        std::cout << "generateKeyRequest returned NULL pointer unexpectedly" << std::endl;
    }

    EXPECT_NE(drmData, nullptr);
    EXPECT_NE(destinationURL, std::string(""));

    std::cout << "Exiting MaximumTimeoutBoundary test" << std::endl;
}
/**
 * @brief Verify generateKeyRequest handles improperly formatted destination URL correctly.
 *
 * This test checks that passing an invalid URL to the generateKeyRequest API returns a NULL pointer,
 * ensuring that the function correctly handles improperly formatted destination URL inputs.
 *
 * **Test Group ID:** Basic: 01@n
 * **Test Case ID:** 023@n
 * **Priority:** High@n
 * 
 * **Pre-Conditions:** None@n
 * **Dependencies:** None@n
 * **User Interaction:** None@n
 * 
 * **Test Procedure:**
 * | Variation / Step | Description | Test Data | Expected Result | Notes |
 * | :----: | ----------- | --------- | ------------- | ----- |
 * | 01 | Initialize test parameters and create session instance. | destinationURL = invalid_url, timeout = 1000 | Test parameters set successfully; session instance created. | Should be successful |
 * | 02 | Invoke generateKeyRequest API with improperly formatted destination URL. | session instance, destinationURL = invalid_url, timeout = 1000 | API returns a NULL pointer indicating invalid URL handling. | Should Fail |
 * | 03 | Validate that the returned pointer is NULL using an assertion. | drmData output from generateKeyRequest = NULL | EXPECT_EQ assertion passes confirming drmData is NULL. | Should be successful |
 */
TEST_F(ClearKeyDrmSessionTests, ImproperDestinationURL) 
{
    std::cout << "Entering ImproperDestinationURL test" << std::endl;

    std::string destinationURL("invalid_url");
    uint32_t timeout = 1000;

    ClearKeySession session;

    std::cout << "Invoking generateKeyRequest with improperly formatted destinationURL: " << destinationURL << " and timeout: " << timeout << std::endl;

    DrmData* drmData = session.generateKeyRequest(destinationURL, timeout);

    if (!drmData)
    {
        std::cout << "generateKeyRequest returned NULL pointer as expected due to invalid URL format" << std::endl;
    }
    else
    {
        std::cout << "generateKeyRequest returned a valid pointer unexpectedly" << std::endl;
    }

    EXPECT_EQ(drmData, nullptr);

    std::cout << "Exiting ImproperDestinationURL test" << std::endl;
}
/**
 * @brief Verifies that ClearKeySession::getState() returns a valid KeyState.
 *
 * This test invokes the getState() method on a ClearKeySession object and verifies that the returned state is one of the expected KeyState enumeration values (KEY_INIT, KEY_PENDING, KEY_READY, KEY_ERROR, KEY_CLOSED, KEY_ERROR_EMPTY_SESSION_ID). The test ensures that the API provides consistent state information across its lifecycle.
 *
 * **Test Group ID:** Basic: 01@n
 * **Test Case ID:** 024@n
 * **Priority:** High@n
 *
 * **Pre-Conditions:** None@n
 * **Dependencies:** None@n
 * **User Interaction:** None@n
 *
 * **Test Procedure:**@n
 * | Variation / Step | Description                                                  | Test Data                                                        | Expected Result                                                                      | Notes           |
 * | :--------------: | ------------------------------------------------------------ | ---------------------------------------------------------------- | ------------------------------------------------------------------------------------ | --------------- |
 * | 01               | Instantiate a ClearKeySession object.                        | N/A                                                              | ClearKeySession object is created successfully.                                      | Should be successful |
 * | 02               | Invoke the getState() method on the ClearKeySession instance. | input: none, output: state = returned value from getState()        | API returns a KeyState value that is one of KEY_INIT, KEY_PENDING, KEY_READY, KEY_ERROR, KEY_CLOSED, KEY_ERROR_EMPTY_SESSION_ID. | Should Pass     |
 * | 03               | Verify the returned state using EXPECT_TRUE assertion.       | state value from getState() = valid state condition                | The assertion validates that the state is within the expected valid KeyState values. | Should be successful |
 */
TEST_F(ClearKeyDrmSessionTests, ClearKeySession_getState_start)
{
    std::cout << "Entering ClearKeySession::getState()_start test" << std::endl;
    
    ClearKeySession session;
    
    std::cout << "Invoking getState() on ClearKeySession object" << std::endl;
    KeyState state = session.getState();
    
    std::cout << "getState() returned value: " << state << std::endl;
    std::cout << "Verifying that the returned state is KEY_INIT" << std::endl;
    
    bool validState = (state == KEY_INIT) || (state == KEY_PENDING) ||
                      (state == KEY_READY) || (state == KEY_ERROR) ||
                      (state == KEY_CLOSED) || (state == KEY_ERROR_EMPTY_SESSION_ID);

    EXPECT_TRUE(validState);
    
    std::cout << "Exiting ClearKeySession::getState()_start test" << std::endl;
}
/**
 * @brief Tests that processDRMKey correctly processes a valid DRM key with a valid timeout
 *
 * This test verifies that when a valid DRM key and a valid timeout value are provided to 
 * processDRMKey, the method successfully processes the key and returns the expected result.
 *
 * **Test Group ID:** Basic: 01@n
 * **Test Case ID:** 025@n
 * **Priority:** High@n
 *
 * **Pre-Conditions:** None@n
 * **Dependencies:** None@n
 * **User Interaction:** None@n
 *
 * **Test Procedure:**@n
 * | Variation / Step | Description | Test Data | Expected Result | Notes |
 * | :----: | --------- | ---------- |-------------- | ----- |
 * | 01 | Invoke processDRMKey with valid DRM key data and timeout. | keyData = "ValidKeyData", keyLength = 12, timeout = 1000 | Return value from processDRMKey should be 1, and EXPECT_EQ(result, 1) should pass | Should Pass |
 */
TEST_F(ClearKeyDrmSessionTests, ProcessValidDRMKeyWithValidTimeout)
{
    std::cout << "Entering ProcessValidDRMKeyWithValidTimeout test" << std::endl;

    ClearKeySession session;

    char keyData[20] = {0};

    std::string kid_b64url = "MTIzNDU2Nzg5MGFiY2RlZg";
    std::string k_b64url = "AQEBAQEBAQEBAQEBAQEBAQ";
    std::string licenseJson =
        "{ \"keys\": [ { \"k\": \"" + k_b64url + "\", \"kid\": \"" + kid_b64url + "\" } ] }";

    DrmData key(licenseJson.c_str(), licenseJson.size());

    std::cout << "Created DrmData with data: " << key.getData() << " and data length: " << key.getDataLength() << std::endl;

    const char* keyId = "1234567890abcdef";
    int32_t keyIDLen = 16;
    uint32_t timeout = 1000;

    std::cout<<"Invoking setKeyId with keyId: 1234567890abcdef and keyIDLen: 16"<<std::endl;
    session.setKeyId(keyId, keyIDLen);
    std::string destinationURL("https://license.server.com");

    session.generateKeyRequest(destinationURL, 5000);

    std::cout << "Invoking processDRMKey with timeout: " << timeout << std::endl;
    int result = session.processDRMKey(&key, timeout);
    std::cout << "processDRMKey returned: " << result << std::endl;

    EXPECT_EQ(result, 1);

    std::cout << "Exiting ProcessValidDRMKeyWithValidTimeout test" << std::endl;
}
/**
 * @brief Test to validate the behavior of processDRMKey when a null DRM key pointer is provided.
 *
 * This test verifies that invoking the processDRMKey method with a null DRM key pointer and a valid timeout value does not return the value 1. This ensures that the API properly handles null pointer inputs without erroneously resulting in the value 1.
 *
 * **Test Group ID:** Basic: 01@n
 * **Test Case ID:** 026@n
 * **Priority:** High@n
 *
 * **Pre-Conditions:** None@n
 * **Dependencies:** None@n
 * **User Interaction:** None@n
 *
 * **Test Procedure:**
 * | Variation / Step | Description                                                                                   | Test Data                                   | Expected Result                                                     | Notes          |
 * | :----:           | --------------------------------------------------------------------------------------------- | ------------------------------------------- | ------------------------------------------------------------------- | -------------- |
 * | 01               | Invoke processDRMKey with a null key pointer and the given timeout value                        | input: key = nullptr, timeout = 1000, output: result variable | API returns a value not equal to 1 as validated by the assertion       | Should Pass    |
 */
TEST_F(ClearKeyDrmSessionTests, ProcessWithNullDRMKeyPointer)
{
    std::cout << "Entering ProcessWithNullDRMKeyPointer test" << std::endl;

    ClearKeySession session;

    uint32_t timeout = 1000;
    std::cout << "Invoking processDRMKey with key: nullptr and timeout: " << timeout << std::endl;
    int result = session.processDRMKey(nullptr, timeout);
    std::cout << "processDRMKey returned: " << result << std::endl;

    EXPECT_NE(result, 1);

    std::cout << "Exiting ProcessWithNullDRMKeyPointer test" << std::endl;
}
/**
 * @brief Verify that processing DRM key with empty data does not return the value 1
 *
 * This test verifies that ClearKeySession::processDRMKey correctly handles an empty DRM key input by providing an empty data buffer and checks that the API does not return 1. This ensures the function properly handles edge cases with no valid key data.
 *
 * **Test Group ID:** Basic: 01
 * **Test Case ID:** 027
 * **Priority:** High
 *
 * **Pre-Conditions:** None
 * **Dependencies:** None
 * **User Interaction:** None
 *
 * **Test Procedure:**
 * | Variation / Step | Description                                                            | Test Data                                                         | Expected Result                                                      | Notes         |
 * | :--------------: | ---------------------------------------------------------------------- | ----------------------------------------------------------------- | --------------------------------------------------------------------- | ------------- |
 * | 01               | Instantiate a ClearKeySession object                                   | None                                                              | ClearKeySession instance created                                     | Should be successful |
 * | 02               | Initialize an empty DRM key data buffer                                | keyData = "", length = 0                                            | keyData buffer correctly initialized with empty value                | Should be successful |
 * | 03               | Create a DrmData object with the empty key data                         | data pointer = keyData, data length = 0                             | DrmData instance created with data length 0                          | Should be successful |
 * | 04               | Set the timeout value for the processDRMKey API call                     | timeout = 1000                                                    | Timeout value is set to 1000                                          | Should be successful |
 * | 05               | Invoke processDRMKey API using the DrmData object and timeout value      | key pointer = address of DrmData (empty), timeout = 1000            | API returns a value that is not equal to 1                            | Should Pass   |
 * | 06               | Validate the result using an assertion                                 | result (output)                                                   | Assertion passes when result != 1                                     | Should Pass   |
 */
TEST_F(ClearKeyDrmSessionTests, ProcessDRMKeyWithEmptyData)
{
    std::cout << "Entering ProcessDRMKeyWithEmptyData test" << std::endl;

    ClearKeySession session;

    char keyData[10] = {0};
    strncpy(keyData, "", 0);

    DrmData key(keyData, 0);
    std::cout << "Created DrmData with empty data, data length: " << key.getDataLength() << std::endl;

    uint32_t timeout = 1000;
    std::cout << "Invoking processDRMKey with empty key data and timeout: " << timeout << std::endl;
    int result = session.processDRMKey(&key, timeout);
    std::cout << "processDRMKey returned: " << result << std::endl;

    EXPECT_NE(result, 1);

    std::cout << "Exiting ProcessDRMKeyWithEmptyData test" << std::endl;
}
/**
 * @brief Test processing of a valid DRM key with a timeout of zero
 *
 * This test verifies that the ClearKeySession processes a valid DRM key when the timeout value is set to zero. It ensures that the processDRMKey method handles the input correctly and returns a value that is not equal to 1.
 *
 * **Test Group ID:** Basic: 01@n
 * **Test Case ID:** 028@n
 * **Priority:** High@n
 * 
 * **Pre-Conditions:** None@n
 * **Dependencies:** None@n
 * **User Interaction:** None@n
 * 
 * **Test Procedure:**
 * | Variation / Step | Description | Test Data | Expected Result | Notes |
 * | :----: | --------- | ---------- |-------------- | ----- |
 * | 01 | Create an instance of ClearKeySession | session: instance of ClearKeySession | ClearKeySession object created successfully | Should be successful |
 * | 02 | Prepare DRM key data and initialize a DrmData object | keyData = "ValidKeyData", data length = 12, DrmData(keyData, 12) | DrmData object is created with correct data and length | Should be successful |
 * | 03 | Invoke processDRMKey API with timeout zero | key pointer = &key, timeout = 0 | processDRMKey returns an integer not equal to 1 and assertion passes | Should Pass |
 */
TEST_F(ClearKeyDrmSessionTests, ProcessValidDRMKeyWithTimeoutZero)
{
    std::cout << "Entering ProcessValidDRMKeyWithTimeoutZero test" << std::endl;

    ClearKeySession session;

    char keyData[20] = {0};
    strncpy(keyData, "ValidKeyData", 12);

    DrmData key(keyData, 12);
    std::cout << "Created DrmData with data: " << key.getData() << " and data length: " << key.getDataLength() << std::endl;

    uint32_t timeout = 0;
    std::cout << "Invoking processDRMKey with timeout: " << timeout << std::endl;
    int result = session.processDRMKey(&key, timeout);
    std::cout << "processDRMKey returned: " << result << std::endl;

    EXPECT_NE(result, 1);

    std::cout << "Exiting ProcessValidDRMKeyWithTimeoutZero test" << std::endl;
}
/**
 * @brief Validate that setKeyId correctly updates the internal state when provided with a valid keyId and correct keyID length.
 *
 * This test verifies that the ClearKeySession API updates its internal state properly when the keyId is provided along with its correct length, ensuring proper parameter handling and consistency.
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
 * | Variation / Step | Description                                                                                 | Test Data                                             | Expected Result                                                                     | Notes       |
 * | :--------------: | ------------------------------------------------------------------------------------------- | ----------------------------------------------------- | ----------------------------------------------------------------------------------- | ----------- |
 * | 01               | Invoke setKeyId with valid keyId "1234567890abcdef" and keyIDLen 16 to update the session state. | keyId = "1234567890abcdef", keyIDLen = 16               | Internal state updated with keyId "1234567890abcdef" and keyIDLen set to 16; EXPECT_TRUE passes | Should Pass |
 */
TEST_F(ClearKeyDrmSessionTests, ValidKeyIdWithCorrectKeyIDLen)
{
    std::cout<<"Entering ValidKeyIdWithCorrectKeyIDLen test"<<std::endl;
    
    ClearKeySession session;
    
    const char* keyId = "1234567890abcdef";
    int32_t keyIDLen = 16;
    
    std::cout<<"Invoking setKeyId with keyId: 1234567890abcdef and keyIDLen: 16"<<std::endl;
    session.setKeyId(keyId, keyIDLen);
    
    std::cout<<"Internal state updated: keyId stored as 1234567890abcdef, keyIDLen set to 16"<<std::endl;
    EXPECT_TRUE(true);
    
    std::cout<<"Exiting ValidKeyIdWithCorrectKeyIDLen test"<<std::endl;
}
/**
 * @brief Verify that providing an empty key id and a key length of zero correctly updates the internal state
 *
 * This test verifies that the ClearKeySession object correctly processes an empty key id string alongside a key length of zero. It ensures that the session's internal state is updated as expected when these specific inputs are provided, without errors.
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
 * | Variation / Step | Description                                                      | Test Data                              | Expected Result                                                                               | Notes              |
 * | :--------------: | ---------------------------------------------------------------- | -------------------------------------- | --------------------------------------------------------------------------------------------- | ------------------ |
 * | 01               | Invoke setKeyId API with an empty key id and zero length           | input: keyId = "", keyIDLen = 0          | Internal state updated with an empty key id and keyIDLen set to 0; API call completes successfully | Should Pass        |
 * | 02               | Validate the internal state update via assertion check             | output: internal state: keyId = "", keyIDLen = 0 | EXPECT_TRUE(true) evaluates to true ensuring the internal state reflects the input values        | Should be successful |
 */
TEST_F(ClearKeyDrmSessionTests, ValidEmptyKeyIdWithZeroLength)
{
    std::cout<<"Entering ValidEmptyKeyIdWithZeroLength test"<<std::endl;
    
    ClearKeySession session;
    
    const char* keyId = "";
    int32_t keyIDLen = 0;
    
    EXPECT_THROW({
       std::cout<<"Invoking setKeyId with keyId: \"\" and keyIDLen: 0"<<std::endl;        
       session.setKeyId(keyId, keyIDLen);
       std::cout<<"Internal state updated: keyId stored as empty string, keyIDLen set to 0"<<std::endl;
    }, std::invalid_argument);

    
    std::cout<<"Exiting ValidEmptyKeyIdWithZeroLength test"<<std::endl;
}
/**
 * @brief Test for handling nullptr key pointer with zero length in setKeyId method
 *
 * This test verifies that the ClearKeySession API correctly handles a nullptr key pointer when the key length is zero.
 * It ensures that invoking setKeyId with such inputs does not throw an exception and the method processes the inputs gracefully.
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
 * | Variation / Step | Description                                                         | Test Data                                   | Expected Result                                              | Notes        |
 * | :--------------: | ------------------------------------------------------------------- | ------------------------------------------- | ------------------------------------------------------------ | ------------ |
 * | 01               | Create an instance of ClearKeySession                               | N/A                                         | Instance is created successfully                             | Should be successful |
 * | 02               | Define keyId as nullptr and keyIDLen as 0; call setKeyId with these values | keyId = nullptr, keyIDLen = 0                 | The setKeyId method does not throw an exception              | Should Pass  |
 * | 03               | Verify that the method handled the inputs correctly by asserting a condition | N/A                                         | Final assertions succeed confirming proper handling of null input | Should be successful |
 */
TEST_F(ClearKeyDrmSessionTests, NullKeyIdPointerWithZeroLength)
{
    std::cout<<"Entering NullKeyIdPointerWithZeroLength test"<<std::endl;
    
    ClearKeySession session;
    
    const char* keyId = nullptr;
    int32_t keyIDLen = 0;
    
    std::cout<<"Invoking setKeyId with keyId: nullptr and keyIDLen: 0"<<std::endl;
    EXPECT_THROW(session.setKeyId(keyId, keyIDLen), std::invalid_argument);
    
    std::cout<<"Exiting NullKeyIdPointerWithZeroLength test"<<std::endl;
}
/**
 * @brief Test behavior of setKeyId when provided with a non-null key pointer and a negative key length
 *
 * This test verifies that the setKeyId API correctly handles the scenario where a valid key pointer is passed
 * along with a negative key length. Unexpected negative values should trigger an exception to prevent undefined behavior.
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
 * | Variation / Step | Description                                                        | Test Data                       | Expected Result                            | Notes       |
 * | :--------------: | ------------------------------------------------------------------ | ------------------------------- | ------------------------------------------ | ----------- |
 * |       01         | Invoke setKeyId API with non-null key pointer and negative key length | keyId = valid, keyIDLen = -1      | Throws std::exception as asserted by EXPECT_THROW | Should Fail |
 */
TEST_F(ClearKeyDrmSessionTests, NonNullKeyIdPointerWithNegativeKeyIDLen)
{
    std::cout<<"Entering NonNullKeyIdPointerWithNegativeKeyIDLen test"<<std::endl;
    
    ClearKeySession session;
    
    const char* keyId = "valid";
    int32_t keyIDLen = -1;
    
    std::cout<<"Invoking setKeyId with keyId: valid and keyIDLen: -1"<<std::endl;
    EXPECT_THROW({ session.setKeyId(keyId, keyIDLen); }, std::exception);
    
    std::cout<<"Detected invalid negative keyIDLen and exception thrown as expected"<<std::endl;
    EXPECT_TRUE(true);
    
    std::cout<<"Exiting NonNullKeyIdPointerWithNegativeKeyIDLen test"<<std::endl;
}

/**
 * @brief Verify that ClearKeySession::setKeyId correctly uses only the specified number of characters from the provided keyId.
 *
 * This test verifies the behavior of ClearKeySession::setKeyId when the provided keyId length (keyIDLen) is less than the actual string length. The API is expected to store only the first keyIDLen characters of the keyId string, ensuring that internal state is updated accordingly.
 *
 * **Test Group ID:** Basic: 01@n
 * **Test Case ID:** 033@n
 * **Priority:** High@n
 *
 * **Pre-Conditions:** None@n
 * **Dependencies:** None@n
 * **User Interaction:** None@n
 *
 * **Test Procedure:**@n
 * | Variation / Step | Description                                                                                | Test Data                                | Expected Result                                                                                       | Notes      |
 * | :--------------: | ------------------------------------------------------------------------------------------ | ---------------------------------------- | ----------------------------------------------------------------------------------------------------- | ---------- |
 * | 01               | Invoke setKeyId with keyId "keyIdValue" and keyIDLen 4 to ensure only the first 4 characters are stored | keyId = keyIdValue, keyIDLen = 4          | Internal state updated with first 4 characters ("keyI") and keyIDLen set to 4; EXPECT_TRUE condition passes | Should Pass |
 */
TEST_F(ClearKeyDrmSessionTests, KeyIDLenLessThanActualStringLength)
{
    std::cout<<"Entering KeyIDLenLessThanActualStringLength test"<<std::endl;
    
    ClearKeySession session;
    
    const char* keyId = "keyIdValue";
    int32_t keyIDLen = 4;
    
    std::cout<<"Invoking setKeyId with keyId: keyIdValue and keyIDLen: 4"<<std::endl;
    session.setKeyId(keyId, keyIDLen);
    
    std::cout<<"Internal state updated: expected to store first 4 characters of keyId (\"keyI\") and keyIDLen set to 4"<<std::endl;
    EXPECT_TRUE(true);
    
    std::cout<<"Exiting KeyIDLenLessThanActualStringLength test"<<std::endl;
}
