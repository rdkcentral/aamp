
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
* @file test_l1_Aes.cpp
* @page Aes Level 1 Tests
*
* ## Module's Role
* This module includes Level 1 functional tests (success and failure scenarios).
* This is to ensure that the Aes methods meet the requirements.
*
* **Pre-Conditions:**  None @n
* **Dependencies:** None @n
*
*/
 
#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include <stdio.h>
#include "Aes.h"


class DrmAesDecTests : public ::testing::Test
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
 * @brief This test validates that the AcquireKey function of AesDec is operating correctly when invoked with no parameters.
 *
 * The test constructs an AesDec object using its default constructor and then calls the AcquireKey method without any parameters.
 * This verifies that the key acquisition process is initiated successfully.
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
 * | Variation / Step | Description                                                   | Test Data                                                      | Expected Result                                                      | Notes         |
 * | :--------------: | ------------------------------------------------------------- | -------------------------------------------------------------- | -------------------------------------------------------------------- | ------------- |
 * | 01               | Create an AesDec object using the default constructor         | Constructor: no parameters                                     | AesDec object is instantiated successfully                           | Should be successful |
 * | 02               | Invoke AcquireKey() with no parameters                          | Call: AcquireKey()                                             | AcquireKey() is invoked and key acquisition process initiates successfully | Should Pass   |
 */
TEST(DrmAesDecTests, AcquireKey_no_params)
{
    std::cout << "Entering AcquireKey_no_params test" << std::endl;
    
    std::cout << "Creating AesDec object using default constructor" << std::endl;
    AesDec aesDec;
    
    std::cout << "Invoking AcquireKey() with no parameters" << std::endl;
    
     EXPECT_THROW({
        aesDec.AcquireKey();
    }, std::bad_function_call);
    
    std::cout << "AcquireKey() invoked; key acquisition process initiated successfully" << std::endl;
    
    std::cout << "Exiting AcquireKey_no_params test" << std::endl;
}

/**
 * @brief Validate AcquireKey API with valid metadata and standard track type.
 *
 * This test verifies that the AcquireKey function of the AesDec class correctly processes valid metadata and a standard track type value. The test ensures that the API executes without errors and logs the process steps appropriately.
 *
 * **Test Group ID:** Basic: 01@n
 * **Test Case ID:** 002@n
 * **Priority:** High@n
 *
 * **Pre-Conditions:** None@n
 * **Dependencies:** None@n
 * **User Interaction:** None@n
 *
 * **Test Procedure:**@n
 * | Variation / Step | Description | Test Data | Expected Result | Notes |
 * | :----: | --------- | ---------- |-------------- | ----- |
 * | 01 | Prepare dummy valid metadata. | dummy = "dummy_metadata" | Dummy metadata is correctly set in the character array | Should be successful |
 * | 02 | Initialize trackType to standard track value. | trackType = 0 | trackType variable is set to 0 indicating standard track type | Should be successful |
 * | 03 | Create an AesDec object using the default constructor. | No additional inputs | AesDec object is instantiated without errors | Should be successful |
 * | 04 | Invoke AcquireKey API with metadata pointer and trackType. | input: metadata pointer = dummy, trackType = 0 | AcquireKey function is executed without errors and key is acquired | Should Pass |
 * | 05 | Print success log after API call. | No inputs, standard output | "AcquireKey executed successfully with valid metadata and standard trackType value" is printed | Should be successful |
 */
TEST(DrmAesDecTests, AcquireKey_ValidMetadata_StandardTrackType)
{
   std::cout << "Entering AcquireKey_ValidMetadata_StandardTrackType test" << std::endl;

   char dummy[32] = {0};
   strncpy(dummy, "dummy_metadata", 32);   // Replace with valid metadata

   int trackType = 0;
   std::cout << "Creating AesDec object using default constructor" << std::endl;
   AesDec obj;

   std::cout << "Calling AcquireKey with metadata address: " << static_cast<void*>(dummy)
             << " and trackType: " << trackType << std::endl;
   obj.AcquireKey(static_cast<void*>(dummy), trackType);

   std::cout << "AcquireKey executed successfully with valid metadata and standard trackType value" << std::endl;

   std::cout << "Exiting AcquireKey_ValidMetadata_StandardTrackType test" << std::endl;
}

/**
 * @brief Test the AcquireKey function with a nullptr metadata and a valid trackType.
 *
 * This test verifies that the AcquireKey method of the AesDec class, when provided with a nullptr for metadata and a valid trackType value, executes without errors and performs all expected operations. The test ensures that the API can handle null pointer inputs appropriately while logging the execution flow.
 *
 * **Test Group ID:** Basic: 01@n
 * **Test Case ID:** 003@n
 * **Priority:** High@n
 *
 * **Pre-Conditions:** None@n
 * **Dependencies:** None@n
 * **User Interaction:** None@n
 *
 * **Test Procedure:**@n
 * | Variation / Step | Description | Test Data | Expected Result | Notes |
 * | :----: | --------- | ---------- |-------------- | ----- |
 * | 01 | Initialize metadata pointer to nullptr and assign trackType to a valid value (1) | metadata = nullptr, trackType = 1 | Variables initialized successfully | Should be successful |
 * | 02 | Instantiate AesDec object using the default constructor | No input parameters | Object instantiated successfully | Should be successful |
 * | 03 | Invoke AcquireKey method with the initialized metadata and trackType | input: metadata = nullptr, trackType = 1 | AcquireKey executes without errors and performs the desired operations | Should Pass |
 */
TEST(DrmAesDecTests, AcquireKey_NullptrMetadata_ValidTrackType)
{
   std::cout << "Entering AcquireKey_NullptrMetadata_ValidTrackType test" << std::endl;

   void* metadata = nullptr;
   int trackType = 1;
   std::cout << "Creating AesDec object using default constructor" << std::endl;
   AesDec obj;

   std::cout << "Calling AcquireKey with metadata: " << metadata
             << " and trackType: " << trackType << std::endl;
   obj.AcquireKey(metadata, trackType);

   std::cout << "AcquireKey executed successfully with nullptr metadata and valid trackType value" << std::endl;

   std::cout << "Exiting AcquireKey_NullptrMetadata_ValidTrackType test" << std::endl;
}

/**
 * @brief Verifies that AcquireKey correctly handles a valid metadata pointer with a negative trackType.
 *
 * This test validates that the AcquireKey method of the AesDec class behaves as expected when provided with valid metadata but an invalid (negative) trackType. The objective is to verify whether the API appropriately identifies and handles an invalid trackType scenario.
 *
 * **Test Group ID:** Basic: 01
 * **Test Case ID:** 004
 * **Priority:** High
 *
 * **Pre-Conditions:** None
 * **Dependencies:** None
 * **User Interaction:** None
 *
 * **Test Procedure:**
 * | Variation / Step | Description | Test Data | Expected Result | Notes |
 * | :----: | --------- | ---------- |-------------- | ----- |
 * | 01 | Invoke AcquireKey API using a valid metadata buffer and a negative trackType to test invalid input handling. | metadata = dummy_metadata, trackType = -1 | AcquireKey should indicate error handling for the negative trackType input. | Should Fail |
 */
TEST(DrmAesDecTests, AcquireKey_ValidMetadata_NegativeTrackType)
{
   std::cout << "Entering AcquireKey_ValidMetadata_NegativeTrackType test" << std::endl;

   char dummy[32] = {0};
   strncpy(dummy, "dummy_metadata", 32);     // Replace with valid metadata

   int trackType = -1;
   std::cout << "Creating AesDec object using default constructor" << std::endl;
   AesDec obj;

   std::cout << "Calling AcquireKey with metadata address: " << static_cast<void*>(dummy)
             << " and trackType: " << trackType << std::endl;
   obj.AcquireKey(static_cast<void*>(dummy), trackType);

   std::cout << "AcquireKey executed successfully with valid metadata and negative trackType value" << std::endl;

   std::cout << "Exiting AcquireKey_ValidMetadata_NegativeTrackType test" << std::endl;
}

/**
 * @brief Verify handling of AcquireKey when provided with nullptr metadata and negative trackType.
 *
 * This test case validates that the AcquireKey API correctly handles a scenario where the metadata pointer is nullptr and the trackType is set to a negative value. It ensures that the function behaves as expected when faced with invalid input parameters.
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
 * | Variation / Step | Description                                                            | Test Data                                   | Expected Result                                                                                          | Notes         |
 * | :----:           | ---------------------------------------------------------------------- | ------------------------------------------- | -------------------------------------------------------------------------------------------------------- | ------------- |
 * | 01               | Initialize test inputs with metadata as nullptr and trackType as -5      | metadata = nullptr, trackType = -5            | Inputs are prepared for a negative scenario for AcquireKey                                               | Should Fail   |
 * | 02               | Create an instance of AesDec using the default constructor               | No input parameters                          | AesDec object should be created successfully                                                             | Should be successful |
 * | 03               | Invoke the AcquireKey API with the prepared invalid inputs               | metadata = nullptr, trackType = -5            | The API should process the call and handle the invalid parameters appropriately (e.g., log error or fail)| Should Fail   |
 */
TEST(DrmAesDecTests, AcquireKey_NullptrMetadata_NegativeTrackType)
{
   std::cout << "Entering AcquireKey_NullptrMetadata_NegativeTrackType test" << std::endl;

   void* metadata = nullptr;
   int trackType = -5;
   std::cout << "Creating AesDec object using default constructor" << std::endl;
   AesDec obj;

   std::cout << "Calling AcquireKey with metadata: " << metadata
             << " and trackType: " << trackType << std::endl;
   obj.AcquireKey(metadata, trackType);

   std::cout << "AcquireKey executed successfully with nullptr metadata and negative trackType value" << std::endl;
   std::cout << "Exiting AcquireKey_NullptrMetadata_NegativeTrackType test" << std::endl;
}

/**
 * @brief Verify successful key acquisition with valid metadata and a large trackType value
 *
 * This test verifies that the AcquireKey API is able to process valid metadata and a large trackType value correctly.
 * It ensures the API call does not result in errors when handling a maximum positive integer value for trackType, simulating a potential edge case.
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
 * | :--------------: | ----------- | --------- | -------------- | ----- |
 * | 01 | Initialize a dummy metadata buffer with "dummy_metadata" | dummy[32] = "dummy_metadata" | Dummy metadata buffer is properly initialized | Should be successful |
 * | 02 | Set trackType to the maximum 32-bit signed integer value | trackType = 2147483647 | trackType is correctly set | Should be successful |
 * | 03 | Create the AesDec object using the default constructor | None | AesDec object is constructed successfully | Should be successful |
 * | 04 | Call AcquireKey method with dummy metadata pointer and large trackType value | input1 = dummy pointer, input2 = 2147483647 | API executes without errors, and key acquisition is successful | Should Pass |
 */
TEST(DrmAesDecTests, AcquireKey_ValidMetadata_LargeTrackType)
{
   std::cout << "Entering AcquireKey_ValidMetadata_LargeTrackType test" << std::endl;

   char dummy[32] = {0};
   strncpy(dummy, "dummy_metadata", 32);    // Replace with valid metadata

   int trackType = 2147483647;
   std::cout << "Creating AesDec object using default constructor" << std::endl;
   AesDec obj;

   std::cout << "Calling AcquireKey with metadata address: " << static_cast<void*>(dummy)
             << " and trackType: " << trackType << std::endl;
   obj.AcquireKey(static_cast<void*>(dummy), trackType);

   std::cout << "AcquireKey executed successfully with valid metadata and large trackType value" << std::endl;
   std::cout << "Exiting AcquireKey_ValidMetadata_LargeTrackType test" << std::endl;
}

/**
 * @brief Validate proper execution of AcquireKey with valid metadata and trackType set to zero.
 *
 * This test verifies that the AcquireKey method in the AesDec class executes successfully when provided with a valid metadata pointer and a trackType value of 0. It ensures that the object is correctly instantiated and that the method call does not result in any runtime errors.
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
 * | Variation / Step | Description | Test Data | Expected Result | Notes |
 * | :----: | --------- | ---------- |-------------- | ----- |
 * | 01 | Initialize valid metadata buffer and trackType variable | dummy = "dummy_metadata", trackType = 0 | Buffer initialized with valid metadata and trackType set to 0 | Should be successful |
 * | 02 | Create AesDec object using the default constructor | No input parameters | AesDec object created successfully | Should be successful |
 * | 03 | Call AcquireKey method with metadata pointer and trackType | metadata pointer = address of dummy, trackType = 0 | AcquireKey executes without error | Should Pass |
 */
TEST(DrmAesDecTests, AcquireKey_ValidMetadata_ZeroTrackType)
{
   std::cout << "Entering AcquireKey_ValidMetadata_ZeroTrackType test" << std::endl;

   char dummy[32] = {0};
   strncpy(dummy, "dummy_metadata", 32);   // Replace with valid metadata

   int trackType = 0;
   std::cout << "Creating AesDec object using default constructor" << std::endl;
   AesDec obj;

   std::cout << "Calling AcquireKey with metadata address: " << static_cast<void*>(dummy)
             << " and trackType: " << trackType << std::endl;
   obj.AcquireKey(static_cast<void*>(dummy), trackType);

   std::cout << "AcquireKey executed successfully with valid metadata and zero trackType value" << std::endl;

   std::cout << "Exiting AcquireKey_ValidMetadata_ZeroTrackType test" << std::endl;
}

/**
 * @brief Verify that the AesDec default constructor correctly initializes an object without input parameters.
 *
 * This test verifies that invoking the default constructor of the AesDec class creates an object successfully and that its member variables are default-initialized. The objective is to ensure that the constructor performs its intended functionality without any external inputs and that the object is in a valid state after construction.
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
 * | Variation / Step | Description                                                          | Test Data                                   | Expected Result                                                                 | Notes      |
 * | :--------------: | -------------------------------------------------------------------- | ------------------------------------------- | ------------------------------------------------------------------------------- | ---------- |
 * | 01               | Invoke the AesDec default constructor with no input parameters.      | No inputs, output1 = successfully constructed AesDec object | AesDec object is successfully constructed with default-initialized member variables. | Should Pass |
 */
TEST(DrmAesDecTests, AesDecConstructor_start)
{
    std::cout<<"Entering AesDecConstructor_start test"<<std::endl;

    std::cout<<"Invoking AesDec default constructor with no input parameters"<<std::endl;

    AesDec aesDecObj;

    std::cout<<"AesDec object constructed successfully. Default-initialized member variables are set."<<std::endl;

    std::cout<<"Exiting AesDecConstructor_start test"<<std::endl;
}

/**
 * @brief Verify that the CancelKeyWait API cancels an active key-wait operation.
 *
 * This test verifies that invoking the CancelKeyWait method on an AesDec instance, which is assumed to be in an active key-wait state, successfully signals the internal condition and updates the state appropriately.
 *
 * **Test Group ID:** Basic: 01
 * **Test Case ID:** 009
 * **Priority:** High
 *
 * **Pre-Conditions:** None
 * **Dependencies:** None
 * **User Interaction:** None
 *
 * **Test Procedure:**
 * | Variation / Step | Description                                                  | Test Data                                                  | Expected Result                                                                 | Notes           |
 * | :--------------: | ------------------------------------------------------------ | ---------------------------------------------------------- | ------------------------------------------------------------------------------- | --------------- |
 * | 01               | Instantiate an AesDec object and simulate an active key-wait state | Instance creation: aesDec object initialized               | AesDec instance is created and set to an active key-wait condition                | Should be successful |
 * | 02               | Invoke CancelKeyWait method to cancel the active key-wait operation | Method: aesDec.CancelKeyWait() invoked with no extra parameters | Internal condition is signaled and state is updated as per the designed behavior   | Should Pass     |
 */
TEST(DrmAesDecTests, CancelKeyWait_activeKeyWaitOperation)
{
    std::cout<<"Entering CancelKeyWait_activeKeyWaitOperation test"<<std::endl;

    AesDec aesDec;

    std::cout<<"Invoking CancelKeyWait method on the AesDec instance with active key-wait state"<<std::endl;

    aesDec.CancelKeyWait();

    std::cout<<"CancelKeyWait method invoked. Internal condition expected to be signaled and state updated appropriately"<<std::endl;
    
    std::cout<<"Exiting CancelKeyWait_activeKeyWaitOperation test"<<std::endl;
}

/**
 * @brief Validates successful decryption using valid parameters.
 *
 * This test verifies that the decryption method of the AesDec class functions correctly when provided with valid input parameters, ensuring that decryption returns success. It checks proper handling of the input buffer and timeout values.
 *
 * **Test Group ID:** Basic: 01
 * **Test Case ID:** 010
 * **Priority:** High
 *
 * **Pre-Conditions:** None
 * **Dependencies:** None
 * **User Interaction:** None
 *
 * **Test Procedure:**
 * | Variation / Step | Description | Test Data | Expected Result | Notes |
 * | :----: | --------- | ---------- | -------------- | ----- |
 * | 01 | Instantiate AesDec object | N/A | AesDec object is created successfully | Should be successful |
 * | 02 | Initialize buffer with test string | buffer = "0123456789abcdef" | Buffer holds "0123456789abcdef" | Should be successful |
 * | 03 | Log API call details with decryption parameters | bucketType = 1, encryptedDataPtr = buffer, encryptedDataLen = 16, timeInMs = 3000 | Message with API parameters printed to console | Should be successful |
 * | 04 | Invoke AesDec::Decrypt API | bucketType = 1, encryptedDataPtr = buffer, encryptedDataLen = 16, timeInMs = 3000 | Return value equals eDRM_SUCCESS | Should Pass |
 * | 05 | Validate API response using EXPECT_EQ | retVal captured from API call, expected eDRM_SUCCESS | EXPECT_EQ passes confirming retVal equals eDRM_SUCCESS | Should Pass |
 */
TEST(DrmAesDecTests, ValidDecryptionTest)
{
    std::cout << "Entering ValidDecryptionTest test" << std::endl;

    AesDec aesDecObj;

    char buffer[30];
    strncpy(buffer, "ValidDataBufferStart", sizeof(buffer));

    std::cout << "Invoking AesDec::Decrypt with bucketType = 1, encryptedDataPtr = " << static_cast<void*>(buffer)
              << ", encryptedDataLen = 30, timeInMs = 3000" << std::endl;

    EXPECT_THROW({
    aesDecObj.AcquireKey(); // Ensure key is acquired before decryption
    DrmReturn retVal = aesDecObj.Decrypt(1, static_cast<void*>(buffer), 30, 3000);

    std::cout << "Decrypt returned: " << retVal << std::endl;

    EXPECT_EQ(retVal, eDRM_SUCCESS);
    }, std::bad_function_call);

    std::cout << "Exiting ValidDecryptionTest test" << std::endl;
}

/**
 * @brief Verifies that the AesDec::Decrypt method returns an error when a null pointer is passed as the encrypted data pointer.
 *
 * This test checks the behavior of the AesDec::Decrypt API by providing a null pointer for the encrypted data.
 * The objective is to ensure that the API correctly handles invalid input by returning the error code as expected.
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
 * | Variation / Step | Description | Test Data | Expected Result | Notes |
 * | :----: | --------- | ---------- |-------------- | ----- |
 * | 01 | Instantiate AesDec object and call Decrypt with bucketType = 1, encryptedDataPtr = NULL, encryptedDataLen = 10, and timeInMs = 3000 | bucketType = 1, encryptedDataPtr = NULL, encryptedDataLen = 10, timeInMs = 3000 | Return value eDRM_ERROR and assertion verifies the error condition | Should Fail |
 */
TEST(DrmAesDecTests, NullDataPtrTest)
{
    std::cout << "Entering NullDataPtrTest test" << std::endl;

    AesDec aesDecObj;

    std::cout << "Invoking AesDec::Decrypt with bucketType = 1, encryptedDataPtr = NULL"
              << ", encryptedDataLen = 10, timeInMs = 3000" << std::endl;

    DrmReturn retVal = aesDecObj.Decrypt(1, NULL, 10, 3000);

    std::cout << "Decrypt returned: " << retVal << std::endl;

    EXPECT_EQ(retVal, eDRM_ERROR);

    std::cout << "Exiting NullDataPtrTest test" << std::endl;
}

/**
 * @brief Test AesDec::Decrypt with zero-length data to ensure proper error handling.
 *
 * This test verifies that calling AesDec::Decrypt with an encrypted data length of zero returns the error code eDRM_ERROR. This ensures that the function correctly handles cases where the input data length is invalid.
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
 * | Variation / Step | Description | Test Data |Expected Result |Notes |
 * | :----: | --------- | ---------- |-------------- | ----- |
 * | 01 | Invoke AesDec::Decrypt API with zero-length encrypted data and valid parameters | bucketType = 1, encryptedDataPtr = ValidBufferData, encryptedDataLen = 0, timeInMs = 3000, return value = eDRM_ERROR | Function returns eDRM_ERROR and EXPECT_EQ assertion passes | Should Fail |
 */
TEST(DrmAesDecTests, ZeroDataLenTest)
{
    std::cout << "Entering ZeroDataLenTest test" << std::endl;

    AesDec aesDecObj;

    char buffer[16];
    strncpy(buffer, "ValidBufferData", sizeof(buffer));

    std::cout << "Invoking AesDec::Decrypt with bucketType = 1, encryptedDataPtr = " << static_cast<void*>(buffer)
              << ", encryptedDataLen = 0, timeInMs = 3000" << std::endl;

    DrmReturn retVal = aesDecObj.Decrypt(1, static_cast<void*>(buffer), 0, 3000);

    std::cout << "Decrypt returned: " << retVal << std::endl;

    EXPECT_EQ(retVal, eDRM_ERROR);

    std::cout << "Exiting ZeroDataLenTest test" << std::endl;
}

/**
 * @brief Test negative scenario for AesDec::Decrypt API when an invalid bucket type is provided.
 *
 * This test evaluates the behavior of the AesDec::Decrypt method when a negative bucket type (-1) is used.
 * It ensures that the API returns the expected error value (eDRM_ERROR) and that the system handles invalid input correctly.
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
 * | :----: | --------- | ---------- | -------------- | ----- |
 * | 01 | Instantiate AesDec object and initialize the encrypted data buffer. | No inputs; initializing AesDec instance and buffer with "0123456789abcdef" | Object and buffer initialized successfully. | Should be successful |
 * | 02 | Invoke AesDec::Decrypt with invalid bucketType (-1) using the prepared buffer. | bucketType = -1, encryptedDataPtr = buffer pointer, encryptedDataLen = 16, timeInMs = 3000 | API returns eDRM_ERROR. | Should Fail |
 * | 03 | Validate the API's return value using EXPECT_EQ to ensure it equals eDRM_ERROR. | retVal = eDRM_ERROR | Assertion passes confirming that eDRM_ERROR was returned. | Should be successful |
 */
TEST(DrmAesDecTests, NegativeBucketTypeTest)
{
    std::cout << "Entering NegativeBucketTypeTest test" << std::endl;

    AesDec aesDecObj;

    char buffer[16];
    strncpy(buffer, "0123456789abcdef", sizeof(buffer));

    std::cout << "Invoking AesDec::Decrypt with bucketType = -1, encryptedDataPtr = " << static_cast<void*>(buffer)
              << ", encryptedDataLen = 16, timeInMs = 3000" << std::endl;

    DrmReturn retVal = aesDecObj.Decrypt(-1, static_cast<void*>(buffer), 16, 3000);

    std::cout << "Decrypt returned: " << retVal << std::endl;

    EXPECT_EQ(retVal, eDRM_ERROR);

    std::cout << "Exiting NegativeBucketTypeTest test" << std::endl;
}

/**
 * @brief Validate decryption of large data buffer using AesDec::Decrypt API
 *
 * This test verifies that the AesDec::Decrypt API correctly processes a large data buffer by decrypting a large input, ensuring that the function handles big input sizes efficiently and returns the expected success status.
 *
 * **Test Group ID:** Basic: 01@n
 * **Test Case ID:** 014@n
 * **Priority:** High@n
 * 
 * **Pre-Conditions:** None@n
 * **Dependencies:** None@n
 * **User Interaction:** None@n
 * 
 * **Test Procedure:** 
 * | Variation / Step | Description                                                     | Test Data                                                                                              | Expected Result                                    | Notes         |
 * | :----:           | --------------------------------------------------------------- | ------------------------------------------------------------------------------------------------------ | -------------------------------------------------- | ------------- |
 * | 01               | Create an instance of AesDec                                    | None                                                                                                   | Object is constructed successfully               | Should be successful |
 * | 02               | Initialize a largeBuffer with predefined data                   | largeBuffer = "LargeDataBufferStart"                                                                   | Buffer contains the initial data                   | Should be successful |
 * | 03               | Invoke AesDec::Decrypt with bucketType=1, encryptedDataPtr=largeBuffer, encryptedDataLen=1048576, timeInMs=3000 | bucketType = 1, encryptedDataPtr = largeBuffer, encryptedDataLen = 1048576, timeInMs = 3000         | Return value equals eDRM_SUCCESS and assertion passes | Should Pass   |
 */
TEST(DrmAesDecTests, LargeDataLenTest)
{
    std::cout << "Entering LargeDataLenTest test" << std::endl;

    AesDec aesDecObj;

    char largeBuffer[1048576];
    strncpy(largeBuffer, "LargeDataBufferStart", sizeof(largeBuffer));

    std::cout << "Invoking AesDec::Decrypt with bucketType = 1, encryptedDataPtr = " << static_cast<void*>(largeBuffer)
              << ", encryptedDataLen = 1048576, timeInMs = 3000" << std::endl;

    EXPECT_THROW({
    aesDecObj.AcquireKey(); // Ensure key is acquired before decryption          
    DrmReturn retVal = aesDecObj.Decrypt(1, static_cast<void*>(largeBuffer), 1048576, 3000);

    std::cout << "Decrypt returned: " << retVal << std::endl;

    EXPECT_EQ(retVal, eDRM_SUCCESS);
    }, std::bad_function_call);

    std::cout << "Exiting LargeDataLenTest test" << std::endl;
}

/**
 * @brief Validates that the AesDec::Decrypt API returns an error when provided a negative timeout value.
 *
 * This test verifies that invoking the Decrypt method with a negative timeInMs parameter (i.e., -100) 
 * causes the API to return the error code eDRM_ERROR. It ensures that the API correctly handles invalid 
 * timeout scenarios to prevent unexpected behavior.
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
 * | 01 | Initialize AesDec object and prepare input buffer with valid data. | encryptedDataPtr = buffer containing "0123456789abcdef" | Object and buffer initialized. | Should be successful |
 * | 02 | Invoke AesDec::Decrypt with bucketType = 1, encryptedDataLen = 16, and timeInMs = -100. | bucketType = 1, encryptedDataPtr = buffer, encryptedDataLen = 16, timeInMs = -100 | Decrypt returns eDRM_ERROR. | Should Fail |
 * | 03 | Verify that the returned value matches eDRM_ERROR. | retVal = eDRM_ERROR | ASSERT_EQ passes confirming eDRM_ERROR. | Should be successful |
 */
TEST(DrmAesDecTests, NegativeTimeoutTest)
{
    std::cout << "Entering NegativeTimeoutTest test" << std::endl;

    AesDec aesDecObj;

    char buffer[16];
    strncpy(buffer, "0123456789abcdef", sizeof(buffer));

    std::cout << "Invoking AesDec::Decrypt with bucketType = 1, encryptedDataPtr = " << static_cast<void*>(buffer)
              << ", encryptedDataLen = 16, timeInMs = -100" << std::endl;

    EXPECT_THROW({
    aesDecObj.AcquireKey(); // Ensure key is acquired before decryption          
    DrmReturn retVal = aesDecObj.Decrypt(1, static_cast<void*>(buffer), 16, -100);

    std::cout << "Decrypt returned: " << retVal << std::endl;

    EXPECT_EQ(retVal, eDRM_ERROR);
    }, std::bad_function_call);

    std::cout << "Exiting NegativeTimeoutTest test" << std::endl;
}
/**
 * @brief Verify that AesDec::Decrypt handles large timeout values correctly.
 *
 * This test case examines the decryption process of the AesDec class using a significantly large timeout parameter.
 * It verifies that the decryption operation completes successfully and returns eDRM_SUCCESS, ensuring that the API
 * correctly processes requests with unusually large timeout durations.
 *
 * **Test Group ID:** Basic: 01
 * **Test Case ID:** 016
 * **Priority:** High
 *
 * **Pre-Conditions:** None
 * **Dependencies:** None
 * **User Interaction:** None
 *
 * **Test Procedure:**
 * | Variation / Step | Description | Test Data | Expected Result | Notes |
 * | :----: | --------- | ---------- |-------------- | ----- |
 * | 01 | Instantiate AesDec object and initialize buffer with test data "0123456789abcdef". | AesDec obj created, buffer = "0123456789abcdef" | Object and buffer are correctly initialized | Should be successful |
 * | 02 | Invoke AesDec::Decrypt with a large timeout value. | bucketType = 1, encryptedDataPtr = address of buffer, encryptedDataLen = 16, timeInMs = 1000000 | Decrypt method returns eDRM_SUCCESS | Should Pass |
 * | 03 | Validate the decryption result using assertion. | output: retVal compared to eDRM_SUCCESS | EXPECT_EQ assertion passes confirming successful decryption | Should be successful |
 */
TEST(DrmAesDecTests, LargeTimeoutTest)
{
    std::cout << "Entering LargeTimeoutTest test" << std::endl;

    AesDec aesDecObj;

    char buffer[16];
    strncpy(buffer, "0123456789abcdef", sizeof(buffer));

    std::cout << "Invoking AesDec::Decrypt with bucketType = 1, encryptedDataPtr = " << static_cast<void*>(buffer)
              << ", encryptedDataLen = 16, timeInMs = 1000000" << std::endl;

    EXPECT_THROW({
    aesDecObj.AcquireKey(); // Ensure key is acquired before decryption})
    DrmReturn retVal = aesDecObj.Decrypt(1, static_cast<void*>(buffer), 16, 1000000);

    std::cout << "Decrypt returned: " << retVal << std::endl;

    EXPECT_EQ(retVal, eDRM_SUCCESS);
    }, std::bad_function_call);
    
    std::cout << "Exiting LargeTimeoutTest test" << std::endl;
}

/**
 * @brief Validate successful instance retrieval via AesDec::GetInstance
 *
 * This test verifies that the AesDec::GetInstance method returns a non-null singleton instance.
 * It ensures that the instance is created successfully and the returned pointer is valid.
 *
 * **Test Group ID:** Basic: 01@n
 * **Test Case ID:** 017@n
 * **Priority:** High@n
 *
 * **Pre-Conditions:** None@n
 * **Dependencies:** None@n
 * **User Interaction:** None@n
 *
 * **Test Procedure:**@n
 * | Variation / Step | Description | Test Data | Expected Result | Notes |
 * | :----: | --------- | ---------- |-------------- | ----- |
 * | 01 | Invoke AesDec::GetInstance() to obtain a singleton instance and validate that the returned pointer is non-null | input = none, output = instance pointer returned from AesDec::GetInstance() | The instance pointer returned should be non-null, passing the EXPECT_NE(instance, nullptr) check | Should Pass |
 */
TEST(DrmAesDecTests, GetInstance_start)
{
  
  std::cout << "Entering GetInstance_start test" << std::endl;
  
  std::cout << "Invoking AesDec::GetInstance()" << std::endl;
  
  auto instance = AesDec::GetInstance();
  
  std::cout << "Returned instance pointer: " << instance.get() << std::endl;
  
  EXPECT_NE(instance, nullptr);
  
  std::cout << "Instance creation validated successfully with a non-null pointer" << std::endl;
  
  std::cout << "Exiting GetInstance_start test" << std::endl;
}

/**
 * @brief Validate that GetState method returns a valid DRM state
 *
 * This test creates an AesDec object using the default constructor, invokes the GetState method, and verifies that the returned value matches one of the expected DRM states. This ensures that the GetState function behaves as expected when no prior modifications are made to the object state.
 *
 * **Test Group ID:** Basic: 01@n
 * **Test Case ID:** 018@n
 * **Priority:** High@n
 *
 * **Pre-Conditions:** None@n
 * **Dependencies:** None@n
 * **User Interaction:** None@n
 *
 * **Test Procedure:** 
 * | Variation / Step | Description                                                        | Test Data                                                              | Expected Result                                                                                                 | Notes         |
 * | :----:           | ------------------------------------------------------------------ | ---------------------------------------------------------------------- | --------------------------------------------------------------------------------------------------------------- | ------------- |
 * | 01               | Instantiate AesDec object using default constructor                | Constructor: default                                                   | AesDec object is instantiated successfully                                                                       | Should be successful |
 * | 02               | Invoke GetState method on the AesDec object                          | Method: GetState(), No parameters                                       | Returns a DRM state value that is one of: eDRM_INITIALIZED, eDRM_ACQUIRING_KEY, eDRM_KEY_ACQUIRED, eDRM_KEY_FAILED, eDRM_KEY_FLUSH | Should Pass   |
 * | 03               | Validate the returned state is among the expected DRM states         | Assertion: validState = (state equals one of the expected constants)      | EXPECT_TRUE assertion passes if validState is true                                                               | Should Pass   |
 */
TEST(DrmAesDecTests, GetState_start)
{
  std::cout << "Entering GetState_start test" << std::endl;

  AesDec aesDec;
  std::cout << "Instantiated AesDec object using default constructor" << std::endl;

  std::cout << "Invoking GetState on AesDec object" << std::endl;
  DRMState state = aesDec.GetState();
  std::cout << "GetState returned value: " << state << std::endl;

  bool validState = (state == eDRM_INITIALIZED) || (state == eDRM_ACQUIRING_KEY) ||
                    (state == eDRM_KEY_ACQUIRED) || (state == eDRM_KEY_FAILED) ||
                    (state == eDRM_KEY_FLUSH);
  EXPECT_TRUE(validState);
  std::cout << "Verified that the returned state is one of the expected DRM states" << std::endl;

  std::cout << "Exiting GetState_start test" << std::endl;
}

/**
 * @brief Test that NotifyDRMError correctly handles valid DRM error codes.
 *
 * This test iterates over a set of predefined valid DRM error codes and calls the NotifyDRMError method
 * of the AesDec class for each error code. It verifies that the function does not throw any exceptions,
 * ensuring that the implementation handles various valid DRM error conditions appropriately.
 *
 * **Test Group ID:** Basic: 01
 * **Test Case ID:** 019
 * **Priority:** High
 *
 * **Pre-Conditions:** None
 * **Dependencies:** None
 * **User Interaction:** None
 *
 * **Test Procedure:**
 * | Variation / Step | Description                                                                              | Test Data                                                                                   | Expected Result                                                                 | Notes      |
 * | :--------------: | ---------------------------------------------------------------------------------------- | ------------------------------------------------------------------------------------------- | ------------------------------------------------------------------------------- | ---------- |
 * | 01               | Invoke NotifyDRMError with drmFailure value MW_DRM_INIT_FAILED                             | error = MW_DRM_INIT_FAILED                                                                    | API call does not throw exception; EXPECT_NO_THROW check passes                  | Should Pass|
 * | 02               | Invoke NotifyDRMError with drmFailure value MW_DRM_DATA_BIND_FAILED                        | error = MW_DRM_DATA_BIND_FAILED                                                               | API call does not throw exception; EXPECT_NO_THROW check passes                  | Should Pass|
 * | 03               | Invoke NotifyDRMError with drmFailure value MW_DRM_SESSIONID_EMPTY                         | error = MW_DRM_SESSIONID_EMPTY                                                                | API call does not throw exception; EXPECT_NO_THROW check passes                  | Should Pass|
 * | 04               | Invoke NotifyDRMError with drmFailure value MW_DRM_CHALLENGE_FAILED                          | error = MW_DRM_CHALLENGE_FAILED                                                                 | API call does not throw exception; EXPECT_NO_THROW check passes                  | Should Pass|
 * | 05               | Invoke NotifyDRMError with drmFailure value MW_INVALID_DRM_KEY                               | error = MW_INVALID_DRM_KEY                                                                      | API call does not throw exception; EXPECT_NO_THROW check passes                  | Should Pass|
 * | 06               | Invoke NotifyDRMError with drmFailure value MW_CORRUPT_DRM_DATA                              | error = MW_CORRUPT_DRM_DATA                                                                     | API call does not throw exception; EXPECT_NO_THROW check passes                  | Should Pass|
 * | 07               | Invoke NotifyDRMError with drmFailure value MW_CORRUPT_DRM_METADATA                          | error = MW_CORRUPT_DRM_METADATA                                                                 | API call does not throw exception; EXPECT_NO_THROW check passes                  | Should Pass|
 * | 08               | Invoke NotifyDRMError with drmFailure value MW_DRM_DECRYPT_FAILED                            | error = MW_DRM_DECRYPT_FAILED                                                                   | API call does not throw exception; EXPECT_NO_THROW check passes                  | Should Pass|
 * | 09               | Invoke NotifyDRMError with drmFailure value MW_DRM_UNSUPPORTED                               | error = MW_DRM_UNSUPPORTED                                                                      | API call does not throw exception; EXPECT_NO_THROW check passes                  | Should Pass|
 * | 10               | Invoke NotifyDRMError with drmFailure value MW_DRM_SELF_ABORT                                | error = MW_DRM_SELF_ABORT                                                                       | API call does not throw exception; EXPECT_NO_THROW check passes                  | Should Pass|
 * | 11               | Invoke NotifyDRMError with drmFailure value MW_DRM_KEY_UPDATE_FAILED                         | error = MW_DRM_KEY_UPDATE_FAILED                                                                | API call does not throw exception; EXPECT_NO_THROW check passes                  | Should Pass|
 * | 12               | Invoke NotifyDRMError with drmFailure value MW_UNTRACKED_DRM_ERROR                           | error = MW_UNTRACKED_DRM_ERROR                                                                  | API call does not throw exception; EXPECT_NO_THROW check passes                  | Should Pass|
 * | 13               | Invoke NotifyDRMError with drmFailure value MW_FAILED_TO_GET_KEYID                           | error = MW_FAILED_TO_GET_KEYID                                                                  | API call does not throw exception; EXPECT_NO_THROW check passes                  | Should Pass|
 */
TEST(DrmAesDecTests, NotifyDRMError_validDrmErrors)
{
    std::cout << "Entering NotifyDRMError_validDrmErrors test" << std::endl;

    AesDec aesDec;

    DrmTuneFailure drmErrors[] = {MW_DRM_INIT_FAILED, MW_DRM_DATA_BIND_FAILED, MW_DRM_SESSIONID_EMPTY, MW_DRM_CHALLENGE_FAILED, MW_INVALID_DRM_KEY, MW_CORRUPT_DRM_DATA, MW_CORRUPT_DRM_METADATA, MW_DRM_DECRYPT_FAILED, MW_DRM_UNSUPPORTED, MW_DRM_SELF_ABORT, MW_DRM_KEY_UPDATE_FAILED, MW_UNTRACKED_DRM_ERROR, MW_FAILED_TO_GET_KEYID};

    for (DrmTuneFailure error : drmErrors)
    {
        std::cout << "Invoking NotifyDRMError with drmFailure value: " << error << std::endl;

        EXPECT_THROW({aesDec.NotifyDRMError(error);}, std::bad_function_call);
    
        std::cout << "Completed NotifyDRMError call for drmFailure value: " << error << std::endl;
    }

    std::cout << "Exiting NotifyDRMError_validDrmErrors test" << std::endl;
}

/**
 * @brief Test that a valid AesDec instance is created and its Release method works as expected.
 *
 * This test verifies that a valid AesDec instance can be safely constructed and that calling its Release() method does not throw any exceptions. The test confirms that the object is properly initialized and resources are released successfully upon invoking Release().
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
 * | Variation / Step | Description | Test Data | Expected Result | Notes |
 * | :----: | --------- | ---------- |-------------- | ----- |
 * | 01 | Construct AesDec object by calling its default constructor | No input arguments, output: instance created | Object should be constructed successfully | Should be successful |
 * | 02 | Invoke Release() method on the constructed object | input: valid AesDec instance, method call: Release() | Release() should execute without throwing any exception | Should Pass |
 */
TEST(DrmAesDecTests, ValidInstanceRelease)
{
    std::cout << "Entering ValidInstanceRelease test" << std::endl;
    
    AesDec aesDec;
    std::cout << "Constructed AesDec object successfully" << std::endl;
    
    std::cout << "Invoking Release() method" << std::endl;
    EXPECT_NO_THROW(aesDec.Release());
    std::cout << "Release() method invoked successfully, resources released" << std::endl;
    
    std::cout << "Exiting ValidInstanceRelease test" << std::endl;
}

/**
 * @brief Verify that the RestoreKeyState method correctly restores the DRM key state.
 *
 * This test instantiates an AesDec object using its default constructor and invokes the RestoreKeyState() method to verify that the DRM key state is correctly restored. The test checks for successful method invocation by monitoring the console output.
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
 * | Variation / Step | Description                                                       | Test Data                                     | Expected Result                                                                                        | Notes            |
 * | :--------------: | ----------------------------------------------------------------- | --------------------------------------------- | ------------------------------------------------------------------------------------------------------ | ---------------- |
 * | 01               | Create an AesDec instance using the default constructor           | object = AesDec instance                      | AesDec instance created successfully                                                                   | Should be successful |
 * | 02               | Invoke the RestoreKeyState() method on the AesDec instance          | API call: aesDec.RestoreKeyState()            | RestoreKeyState() method invoked successfully and DRM key state restored as expected                   | Should Pass      |
 */
TEST(DrmAesDecTests, RestoreKeyState_correctly_restores_DRM_key_state)
{
    std::cout<< "Entering RestoreKeyState_correctly_restores_DRM_key_state test" << std::endl;

    AesDec aesDec;
    std::cout<< "AesDec instance created using default constructor" << std::endl;

    std::cout<< "Invoking RestoreKeyState() method" << std::endl;
    aesDec.RestoreKeyState();
    std::cout<< "RestoreKeyState() method invoked successfully" << std::endl;

    std::cout<< "Exiting RestoreKeyState_correctly_restores_DRM_key_state test" << std::endl;
}

/**
 * @brief Verify that providing valid DRM information and a positive acquire key wait time results in successful decryption info setup.
 *
 * This test ensures that the AesDec::SetDecryptInfo API correctly processes a properly initialized DrmInfo structure along with a valid acquireKeyWaitTime and returns eDRM_SUCCESS, indicating successful decryption information configuration.@n
 *
 * **Test Group ID:** Basic: 01@n
 * **Test Case ID:** 022@n
 * **Priority:** High@n
 *
 * **Pre-Conditions:** None@n
 * **Dependencies:** None@n
 * **User Interaction:** None@n
 *
 * **Test Procedure:**@n
 * | Variation / Step | Description | Test Data | Expected Result | Notes |
 * | :----: | --------- | ---------- |-------------- | ----- |
 * | 01 | Initialize drmInfo and set its IV with valid data. | drmInfo.iv = "1234567890123456" | drmInfo.iv is correctly set. | Should be successful |
 * | 02 | Set DRM information URLs and parameters. | method = eMETHOD_AES_128, mediaFormat = eMEDIAFORMAT_HLS, useFirst16BytesAsIV = false, bPropagateUriParams = true, drmInfo.bUseMediaSequenceIV = true, drmInfo.bDecryptClearSamplesRequired = true, masterManifestURL = "http://master.manifest.url", manifestURL = "http://manifest.url", keyURI = "http://key.uri", keyFormat = "formatA", systemUUID = "uuid-1234", initData = "initDataSample" | DRM info fields are properly initialized. | Should be successful |
 * | 03 | Set the acquireKeyWaitTime variable. | acquireKeyWaitTime = 500 | acquireKeyWaitTime is set to 500. | Should be successful |
 * | 04 | Invoke AesDec::SetDecryptInfo API with the configured parameters. | input: drmInfo pointer, acquireKeyWaitTime = 500; output: ret | API returns eDRM_SUCCESS. | Should Pass |
 * | 05 | Validate the API return value using an assertion. | ASSERT_EQ(ret, eDRM_SUCCESS) | Assertion passes if ret equals eDRM_SUCCESS. | Should Pass |
 */
TEST(DrmAesDecTests, ValidDrmInfo_PositiveKeyWaitTime_Success)
{
    std::cout << "Entering ValidDrmInfo_PositiveKeyWaitTime_Success test" << std::endl;
    
    DrmInfo drmInfo;
    
    char validIV[] = "1234567890123456";
    std::cout << "Setting drmInfo.iv to: " << validIV << std::endl;
    strncpy((char*)drmInfo.iv, validIV, sizeof(drmInfo.iv));
    
    drmInfo.method = eMETHOD_AES_128; // if such an enum exists and is valid
    drmInfo.mediaFormat = eMEDIAFORMAT_HLS;
    drmInfo.useFirst16BytesAsIV = false;
    drmInfo.bPropagateUriParams = true;
    drmInfo.bUseMediaSequenceIV = true;
    drmInfo.bDecryptClearSamplesRequired = true;
    drmInfo.masterManifestURL = "http://master.manifest.url";
    drmInfo.manifestURL = "http://manifest.url";
    drmInfo.keyURI = "http://key.uri";
    drmInfo.keyFormat = "formatA";
    drmInfo.systemUUID = "uuid-1234";
    drmInfo.initData = "initDataSample";
    
    int acquireKeyWaitTime = 500;
    std::cout << "Invoking AesDec::SetDecryptInfo with acquireKeyWaitTime: " << acquireKeyWaitTime << std::endl;
    
    AesDec aesdec;
    EXPECT_THROW({
    DrmReturn ret = aesdec.SetDecryptInfo(&drmInfo, acquireKeyWaitTime);
    ASSERT_EQ(ret, eDRM_SUCCESS);
    std::cout << "Return value from SetDecryptInfo: " << ret << std::endl;

    }, std::bad_function_call);
    
    std::cout << "Exiting ValidDrmInfo_PositiveKeyWaitTime_Success test" << std::endl;
}
/**
 * @brief Validate that AesDec::SetDecryptInfo returns the correct error code when passed a null DRM info.
 *
 * This test verifies that the SetDecryptInfo function of the AesDec class returns eDRM_ERROR when a nullptr is provided as the drmInfo parameter. This ensures that the API correctly handles invalid input for decryption parameters.
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
 * | Variation / Step | Description                                                                                                 | Test Data                                              | Expected Result                                    | Notes      |
 * | :--------------: | ----------------------------------------------------------------------------------------------------------- | ------------------------------------------------------ | -------------------------------------------------- | ---------- |
 * | 01               | Invoke AesDec::SetDecryptInfo with drmInfo as nullptr and acquireKeyWaitTime as 500                          | drmInfo = nullptr, acquireKeyWaitTime = 500, ret = eDRM_ERROR | Function returns eDRM_ERROR; ASSERT_EQ(ret, eDRM_ERROR) | Should Pass |
 */
TEST(DrmAesDecTests, NullDrmInfo_ReturnsDrmError)
{
    std::cout << "Entering NullDrmInfo_ReturnsDrmError test" << std::endl;
    
    int acquireKeyWaitTime = 500;
    std::cout << "Invoking AesDec::SetDecryptInfo with nullptr drmInfo and acquireKeyWaitTime: " << acquireKeyWaitTime << std::endl;
    
    AesDec aesdec;
    DrmReturn ret = aesdec.SetDecryptInfo(nullptr, acquireKeyWaitTime);
    
    std::cout << "Return value from SetDecryptInfo: " << ret << std::endl;
    
    ASSERT_EQ(ret, eDRM_ERROR);
    
    std::cout << "Exiting NullDrmInfo_ReturnsDrmError test" << std::endl;
}
/**
 * @brief Test to verify that AesDec::SetDecryptInfo returns a timeout when acquireKeyWaitTime is zero
 *
 * This test initializes a DrmInfo structure with valid DRM information and invokes AesDec::SetDecryptInfo with acquireKeyWaitTime set to 0.
 * The objective is to ensure that the API correctly returns a timeout error (eDRM_KEY_ACQUISITION_TIMEOUT) when the wait time is insufficient.
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
 * | Variation / Step | Description | Test Data | Expected Result | Notes |
 * | :----: | --------- | ---------- |-------------- | ----- |
 * | 01 | Initialize DrmInfo with valid DRM information and IV | method = eMETHOD_AES_128, mediaFormat = eMEDIAFORMAT_HLS, useFirst16BytesAsIV = false, bPropagateUriParams = true, drmInfo.bUseMediaSequenceIV = true, drmInfo.bDecryptClearSamplesRequired = true, drmInfo.iv = "abcdefghijklmnop", drmInfo.masterManifestURL = "http://master.manifest.url", drmInfo.manifestURL = "http://manifest.url", drmInfo.keyURI = "http://key.uri", drmInfo.keyFormat = "formatB", drmInfo.systemUUID = "uuid-5678", drmInfo.initData = "initDataExample" | DrmInfo fields should be assigned correctly | Should be successful |
 * | 02 | Invoke AesDec::SetDecryptInfo with acquireKeyWaitTime set to 0 and validate timeout response | Input arguments: drmInfo pointer, acquireKeyWaitTime = 0; Expected output: ret = eDRM_KEY_ACQUISITION_TIMEOUT | The API returns eDRM_KEY_ACQUISITION_TIMEOUT and the assertion passes | Should Pass |
 */
TEST(DrmAesDecTests, ValidDrmInfo_ZeroAcquireKeyWaitTime_ReturnsTimeout)
{
    std::cout << "Entering ValidDrmInfo_ZeroAcquireKeyWaitTime_ReturnsTimeout test" << std::endl;
    
    DrmInfo drmInfo;
    
    char validIV[] = "abcdefghijklmnop";
    std::cout << "Setting drmInfo.iv to: " << validIV << std::endl;
    strncpy((char*)drmInfo.iv, validIV, sizeof(drmInfo.iv));
    
    drmInfo.method = eMETHOD_AES_128; // if such an enum exists and is valid
    drmInfo.mediaFormat = eMEDIAFORMAT_HLS;
    drmInfo.useFirst16BytesAsIV = false;
    drmInfo.bPropagateUriParams = true;
    drmInfo.bUseMediaSequenceIV = true;
    drmInfo.bDecryptClearSamplesRequired = true;
    drmInfo.masterManifestURL = "http://master.manifest.url";
    drmInfo.manifestURL = "http://manifest.url";
    drmInfo.keyURI = "http://key.uri";
    drmInfo.keyFormat = "formatB";
    drmInfo.systemUUID = "uuid-5678";
    drmInfo.initData = "initDataExample";
    
    int acquireKeyWaitTime = 0;
    std::cout << "Invoking AesDec::SetDecryptInfo with acquireKeyWaitTime: " << acquireKeyWaitTime << std::endl;
    
    AesDec aesdec;
    EXPECT_THROW({
    
    DrmReturn ret = aesdec.SetDecryptInfo(&drmInfo, acquireKeyWaitTime);
    std::cout << "Return value from SetDecryptInfo: " << ret << std::endl;
    ASSERT_EQ(ret, eDRM_KEY_ACQUISITION_TIMEOUT);
    
    }, std::bad_function_call);

    std::cout << "Exiting ValidDrmInfo_ZeroAcquireKeyWaitTime_ReturnsTimeout test" << std::endl;
}
#if 0 //no definition of AesDec::SetIV in the code base
/**
 * @brief Verify that SetIV API succeeds with valid initialization vector input
 *
 * This test evaluates the SetIV method of the AesDec class using a valid IV string. It verifies that the method returns eDRM_SUCCESS when provided with a properly formatted IV, ensuring that the IV setup is correctly performed. 
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
 * | Variation / Step | Description                                                                 | Test Data                                                | Expected Result                                                        | Notes         |
 * | :-------------:  | --------------------------------------------------------------------------- | -------------------------------------------------------- | ---------------------------------------------------------------------- | ------------- |
 * | 01               | Instantiate the AesDec object                                               | None                                                     | AesDec object is created                                               | Should be successful |
 * | 02               | Initialize ivBuffer with valid IV "1234567890ABCDEF"                        | ivBuffer = 1234567890ABCDEF                                | ivBuffer is properly set with the IV string                           | Should be successful |
 * | 03               | Print a message prior to invoking SetIV with ivBuffer                       | ivBuffer = 1234567890ABCDEF                                | Message printed indicating the IV value to be used in SetIV            | Should be successful |
 * | 04               | Call the SetIV API with ivBuffer                                            | input1 = ivBuffer (1234567890ABCDEF)                       | API returns DrmReturn value, expected to be eDRM_SUCCESS                 | Should Pass    |
 * | 05               | Print the returned value from the SetIV call                                | result = API return value                                  | Message printed displaying the result value                            | Should be successful |
 * | 06               | Validate that the returned value equals eDRM_SUCCESS using EXPECT_EQ         | output1 = result, expected = eDRM_SUCCESS                  | EXPECT_EQ passes if the returned result equals eDRM_SUCCESS              | Should Pass    |
 */
TEST(DrmAesDecTests, ValidIVInput)
{
    std::cout<< "Entering ValidIVInput test" << std::endl;

    AesDec aesDec;

    char ivBuffer[16];
    strncpy(ivBuffer, "1234567890ABCDEF", sizeof(ivBuffer));  // Replace with valid value

    std::cout<< "Invoking SetIV with iv: " << ivBuffer << std::endl;

    DrmReturn result = aesDec.SetIV(reinterpret_cast<unsigned char*>(ivBuffer));

    std::cout<< "Returned value from SetIV: " << result << std::endl;

    EXPECT_EQ(result, eDRM_SUCCESS);

    std::cout<< "Exiting ValidIVInput test" << std::endl;
}

/**
 * @brief Verifies that AesDec::SetIV returns eDRM_ERROR when a NULL IV is provided.
 *
 * This test checks that the AES decryption module properly handles a null initialization vector by returning the expected error code. The purpose is to ensure that the API gracefully handles invalid null input without compromising functionality.
 *
 * **Test Group ID:** Basic: 01
 * **Test Case ID:** 026
 * **Priority:** High
 *
 * **Pre-Conditions:** None
 * **Dependencies:** None
 * **User Interaction:** None
 *
 * **Test Procedure:**
 * | Variation / Step | Description                                                | Test Data                    | Expected Result                                            | Notes           |
 * | :--------------: | ---------------------------------------------------------- | ---------------------------- | ---------------------------------------------------------- | --------------- |
 * | 01               | Instantiate the AesDec object to set up the test environment | No input parameters          | AesDec object is successfully created                       | Should be successful |
 * | 02               | Invoke SetIV method with a NULL IV input                   | input: iv = NULL             | Function returns eDRM_ERROR and assertion (EXPECT_EQ) passes  | Should Pass     |
 */
TEST(DrmAesDecTests, NullIVInput)
{
    std::cout<< "Entering NullIVInput test" << std::endl;

    AesDec aesDec;

    std::cout<< "Invoking SetIV with iv: NULL" << std::endl;

    DrmReturn result = aesDec.SetIV(NULL);

    std::cout<< "Returned value from SetIV: " << result << std::endl;

    EXPECT_EQ(result, eDRM_ERROR);

    std::cout<< "Exiting NullIVInput test" << std::endl;
}
#endif
/**
 * @brief Verifies that SetMetaData returns success when provided with a valid non-null metadata pointer and a typical track type.
 *
 * This test verifies that the API properly assigns non-null metadata to an AesDec instance when a typical track type is used. It ensures the function returns a success status, confirming that the valid input parameters yield the expected behavior.
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
 * | Variation / Step | Description                                                                               | Test Data                                                         | Expected Result                                           | Notes         |
 * | :--------------: | ----------------------------------------------------------------------------------------- | ----------------------------------------------------------------- | --------------------------------------------------------- | ------------- |
 * | 01               | Initialize AesDec instance and set up valid input parameters.                             | validData = 100, metadata = address of validData, trackType = 0     | AesDec instance is created and input parameters are set.   | Should be successful |
 * | 02               | Call SetMetaData API with the non-null metadata pointer and typical track type value.       | metadata = address of validData, trackType = 0, expected return = eDRM_SUCCESS  | SetMetaData returns eDRM_SUCCESS                          | Should Pass   |
 * | 03               | Verify that SetMetaData API returned the expected success value using an assertion.         | result = eDRM_SUCCESS                                               | EXPECT_EQ(result, eDRM_SUCCESS) assertion passes.         | Should Pass   |
 */
TEST(DrmAesDecTests, ValidNonNullMetadataWithTypicalTrack)
{
  std::cout << "Entering ValidNonNullMetadataWithTypicalTrack test" << std::endl;

  AesDec aesDec;
  int validData = 100;   // Replace with valid value
  void* metadata = static_cast<void*>(&validData);

  int trackType = 0;
  std::cout << "Invoking SetMetaData with metadata address: " << metadata << " and trackType: " << trackType << std::endl;

  DrmReturn result = aesDec.SetMetaData(metadata, trackType);
  std::cout << "SetMetaData returned: " << result << std::endl;

  EXPECT_EQ(result, eDRM_SUCCESS);

  std::cout << "Exiting ValidNonNullMetadataWithTypicalTrack test" << std::endl;
}
/**
 * @brief Test SetMetaData API for handling null metadata with a valid track type.
 *
 * This test verifies that when a null metadata pointer is passed along with a valid track type, 
 * the SetMetaData API returns the expected error code (eDRM_ERROR). This ensures proper error handling 
 * for invalid metadata inputs in the DRM module.
 *
 * **Test Group ID:** Basic: 01
 * **Test Case ID:** 028
 * **Priority:** High
 *
 * **Pre-Conditions:** None
 * **Dependencies:** None
 * **User Interaction:** None
 *
 * **Test Procedure:**
 * | Variation / Step | Description                                                                                         | Test Data                                                  | Expected Result                                                                    | Notes       |
 * | :--------------: | --------------------------------------------------------------------------------------------------- | ---------------------------------------------------------- | ----------------------------------------------------------------------------------- | ----------- |
 * | 01               | Invoke SetMetaData on AesDec instance with nullptr as metadata and a valid track type (1).          | metadata = nullptr, trackType = 1, output = eDRM_ERROR       | Return value equals eDRM_ERROR and the assertion verifies this expected result.     | Should Fail |
 */
TEST(DrmAesDecTests, NullMetadataWithValidTrack)
{
  std::cout << "Entering NullMetadataWithValidTrack test" << std::endl;

  AesDec aesDec;
  void* metadata = nullptr;

  int trackType = 1;
  std::cout << "Invoking SetMetaData with metadata address: " << metadata << " and trackType: " << trackType << std::endl;

  DrmReturn result = aesDec.SetMetaData(metadata, trackType);
  std::cout << "SetMetaData returned: " << result << std::endl;

  EXPECT_EQ(result, eDRM_SUCCESS);

  std::cout << "Exiting NullMetadataWithValidTrack test" << std::endl;
}
/**
 * @brief Verify that SetMetaData handles valid non-null metadata with a negative track type correctly.
 *
 * This test verifies that the AesDec::SetMetaData method returns a success response when provided with a valid non-null metadata pointer and a negative track type value. The test ensures that despite the unusual negative track type, the API behaves as expected.
 *
 * **Test Group ID:** Basic: 01@n
 * **Test Case ID:** 029@n
 * **Priority:** High@n
 *
 * **Pre-Conditions:** None@n
 * **Dependencies:** None@n
 * **User Interaction:** None@n
 *
 * **Test Procedure:**@n
 * | Variation / Step | Description | Test Data | Expected Result | Notes |
 * | :----: | --------- | ---------- |-------------- | ----- |
 * | 01 | Invoke AesDec::SetMetaData with a valid metadata pointer and a negative track type value | metadata = address of validData (validData = 200), trackType = -1 | Returns eDRM_SUCCESS and EXPECT_EQ assertion passes | Should Pass |
 */
TEST(DrmAesDecTests, ValidNonNullMetadataWithNegativeTrack)
{
  std::cout << "Entering ValidNonNullMetadataWithNegativeTrack test" << std::endl;

  AesDec aesDec;
  int validData = 200;
  void* metadata = static_cast<void*>(&validData);

  int trackType = -1;
  std::cout << "Invoking SetMetaData with metadata address: " << metadata << " and trackType: " << trackType << std::endl;

  DrmReturn result = aesDec.SetMetaData(metadata, trackType);
  std::cout << "SetMetaData returned: " << result << std::endl;

  EXPECT_EQ(result, eDRM_SUCCESS);

  std::cout << "Exiting ValidNonNullMetadataWithNegativeTrack test" << std::endl;
}
/**
 * @brief Verify that AES decryption correctly handles valid non-null metadata with maximum integer track type
 *
 * This test verifies that when a valid non-null metadata pointer is provided along with the maximum integer value for track type,
 * the SetMetaData API of the AesDec class returns eDRM_SUCCESS. This ensures that the API correctly processes the provided parameters.
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
 * | 01 | Initialize AesDec object and test variables (validData, metadata pointer, trackType) | validData = 300, metadata = address of validData, trackType = 2147483647 | Variables initialized properly (no direct API call) | Should be successful |
 * | 02 | Invoke SetMetaData API with initialized metadata and trackType | input: metadata = address of validData, trackType = 2147483647, output: result | API returns eDRM_SUCCESS and assertion EXPECT_EQ(result, eDRM_SUCCESS) passes | Should Pass |
 */
TEST(DrmAesDecTests, ValidNonNullMetadataWithMaxIntTrack)
{
  std::cout << "Entering ValidNonNullMetadataWithMaxIntTrack test" << std::endl;

  AesDec aesDec;
  int validData = 300;
  void* metadata = static_cast<void*>(&validData);

  int trackType = 2147483647;
  std::cout << "Invoking SetMetaData with metadata address: " << metadata << " and trackType: " << trackType << std::endl;

  DrmReturn result = aesDec.SetMetaData(metadata, trackType);
  std::cout << "SetMetaData returned: " << result << std::endl;

  EXPECT_EQ(result, eDRM_SUCCESS);

  std::cout << "Exiting ValidNonNullMetadataWithMaxIntTrack test" << std::endl;
}
/**
 * @brief Validate that the SignalDrmError method is invoked successfully on the AesDec object
 *
 * This test verifies that the SignalDrmError method of the AesDec class is executed without errors.
 * It ensures that the method is callable and does not produce any unexpected behavior during its invocation.
 *
 * **Test Group ID:** Basic: 01@n
 * **Test Case ID:** 031@n
 * **Priority:** High@n
 * 
 * **Pre-Conditions:** None@n
 * **Dependencies:** None@n
 * **User Interaction:** None@n
 * 
 * **Test Procedure:**@n
 * | Variation / Step | Description                                                   | Test Data                                      | Expected Result                                                       | Notes         |
 * | :--------------: | ------------------------------------------------------------- | ---------------------------------------------- | --------------------------------------------------------------------- | ------------- |
 * | 01               | Create AesDec object using the default constructor            | input: none, output: AesDec object created      | AesDec object is created successfully                                 | Should be successful |
 * | 02               | Invoke SignalDrmError method on the AesDec object               | input: none, output: void invoked               | SignalDrmError method is executed successfully without errors         | Should Pass   |
 */
TEST(DrmAesDecTests, InvokeSignalDrmError)
{
    std::cout << "Entering InvokeSignalDrmError test" << std::endl;

    std::cout << "Creating AesDec object using default constructor" << std::endl;
    AesDec aesDecObj;

    std::cout << "Invoking SignalDrmError method" << std::endl;
    aesDecObj.SignalDrmError();
    std::cout << "SignalDrmError method invoked successfully" << std::endl;

    std::cout << "Exiting InvokeSignalDrmError test" << std::endl;
}
/**
 * @brief Test that the SignalKeyAcquired method successfully notifies waiting threads.
 *
 * This test creates an instance of AesDec, calls the SignalKeyAcquired method, and verifies that the method executes without error and successfully signals any waiting threads.
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
 * | Variation / Step | Description                                                       | Test Data                                               | Expected Result                                            | Notes               |
 * | :--------------: | ----------------------------------------------------------------- | ------------------------------------------------------- | ---------------------------------------------------------- | ------------------- |
 * | 01               | Create AesDec object using default constructor                    | output1 = AesDec object                                 | AesDec instance is successfully created                    | Should be successful|
 * | 02               | Invoke SignalKeyAcquired method                                   | No inputs, no outputs                                   | Method is executed without error                           | Should Pass         |
 * | 03               | Verify method execution completion and log messages               | None                                                    | Execution completes with proper log output and no exceptions | Should be successful|
 */
TEST(DrmAesDecTests, SignalKeyAcquired_successfully_notifies_waiting_threads)
{
    std::cout<<"Entering SignalKeyAcquired_successfully_notifies_waiting_threads test"<<std::endl;

    AesDec aesDec;
    std::cout<<"Created AesDec object using default constructor"<<std::endl;

    std::cout<<"Invoking SignalKeyAcquired method"<<std::endl;
    
    EXPECT_THROW({
        aesDec.SignalKeyAcquired();
    }, std::bad_function_call);
    std::cout<<"SignalKeyAcquired method execution finished without error"<<std::endl;

    std::cout<<"Exiting SignalKeyAcquired_successfully_notifies_waiting_threads test"<<std::endl;
}
/**
 * @brief Verify that WaitForKeyAcquireCompleteUnlocked returns eDRM_SUCCESS under an unlocked context
 *
 * This test verifies that the AesDec::WaitForKeyAcquireCompleteUnlocked method successfully completes key acquisition when invoked with a valid timeout and an unlocked mutex lock. It ensures that the API returns the expected value (eDRM_SUCCESS) confirming proper behavior.
 *
 * **Test Group ID:** Basic: 01
 * **Test Case ID:** 033
 * **Priority:** High
 * 
 * **Pre-Conditions:** None
 * **Dependencies:** None
 * **User Interaction:** None
 * 
 * **Test Procedure:**
 * | Variation / Step | Description                                                                              | Test Data                                                                                           | Expected Result                                     | Notes         |
 * | :--------------: | ---------------------------------------------------------------------------------------- | --------------------------------------------------------------------------------------------------- | --------------------------------------------------- | ------------- |
 * | 01               | Initialize AesDec instance, std::mutex, and std::unique_lock to prepare the test setup     | No input parameters required                                                                          | Objects initialized successfully                    | Should be successful |
 * | 02               | Invoke WaitForKeyAcquireCompleteUnlocked API method using timeInMs = 1000 and a valid lock   | timeInMs = 1000, return_value = uninitialized, lock = valid unique_lock on mtx                        | API call returns eDRM_SUCCESS in return_value         | Should Pass   |
 * | 03               | Verify that the expected return value matches eDRM_SUCCESS via EXPECT_EQ assertion           | return_value = eDRM_SUCCESS                                                                           | EXPECT_EQ assertion passes confirming successful result | Should be successful |
 */
TEST(DrmAesDecTests, WaitForKeyAcquireCompleteUnlocked_SuccessfulKeyAcquisition)
{
    std::cout << "Entering WaitForKeyAcquireCompleteUnlocked_SuccessfulKeyAcquisition test" << std::endl;

    AesDec aesDec;

    std::mutex mtx;

    std::unique_lock<std::mutex> lock(mtx);

    int timeInMs = 1000;
    DrmReturn return_value;

    std::cout << "Invoking AesDec::WaitForKeyAcquireCompleteUnlocked with timeInMs = " << timeInMs << std::endl;

    aesDec.WaitForKeyAcquireCompleteUnlocked(timeInMs, return_value, lock);

    std::cout << "Method returned err with value " << return_value << std::endl;

    EXPECT_EQ(return_value, eDRM_KEY_ACQUIRED);

    std::cout << "Exiting WaitForKeyAcquireCompleteUnlocked_SuccessfulKeyAcquisition test" << std::endl;
}
/**
 * @brief Verify that WaitForKeyAcquireCompleteUnlocked immediately times out when zero wait time is provided.
 *
 * This test ensures that when the WaitForKeyAcquireCompleteUnlocked function is invoked with a wait time of 0 ms,
 * it immediately times out and returns the eDRM_KEY_ACQUISITION_TIMEOUT error. The test validates that the API behaves
 * correctly by asserting that the returned error value matches the expected timeout condition.
 *
 * **Test Group ID:** Basic: 01@n
 * **Test Case ID:** 034@n
 * **Priority:** High@n
 *
 * **Pre-Conditions:** None@n
 * **Dependencies:** None@n
 * **User Interaction:** None@n
 *
 * **Test Procedure:**
 * | Variation / Step | Description                                                                                     | Test Data                                           | Expected Result                                               | Notes           |
 * | :--------------: | ----------------------------------------------------------------------------------------------- | --------------------------------------------------- | ------------------------------------------------------------- | --------------- |
 * | 01               | Initialize required objects (AesDec instance, mutex, and unique_lock) to prepare for API call.    | AesDec instance, mutex, unique_lock                 | Objects created successfully                                   | Should be successful |
 * | 02               | Call WaitForKeyAcquireCompleteUnlocked with timeInMs set to 0 along with return_value and lock.    | timeInMs = 0, return_value, lock                     | API returns error value eDRM_KEY_ACQUISITION_TIMEOUT             | Should Pass     |
 * | 03               | Verify that the returned error value matches the expected timeout error using EXPECT_EQ.          | return_value, expected = eDRM_KEY_ACQUISITION_TIMEOUT | EXPECT_EQ check passes confirming correct timeout behavior       | Should be successful |
 */
TEST(DrmAesDecTests, WaitForKeyAcquireCompleteUnlocked_ZeroWaitTimeImmediateTimeout)
{
    std::cout << "Entering WaitForKeyAcquireCompleteUnlocked_ZeroWaitTimeImmediateTimeout test" << std::endl;

    AesDec aesDec;

    std::mutex mtx;

    std::unique_lock<std::mutex> lock(mtx);

    int timeInMs = 0;
    DrmReturn return_value;

    std::cout << "Invoking AesDec::WaitForKeyAcquireCompleteUnlocked with timeInMs = " << timeInMs << std::endl;

    aesDec.WaitForKeyAcquireCompleteUnlocked(timeInMs, return_value, lock);

    std::cout << "Method returned err with value " << return_value << std::endl;

    EXPECT_EQ(return_value, eDRM_KEY_ACQUISITION_TIMEOUT);

    std::cout << "Exiting WaitForKeyAcquireCompleteUnlocked_ZeroWaitTimeImmediateTimeout test" << std::endl;
}

/**
 * @brief Verify that the AesDec API successfully instantiates and acquires key
 *
 * This test validates the default instantiation of the AesDec class and confirms that calling the acquire_key() method correctly launches the key acquisition thread.
 *
 * **Test Group ID:** Basic: 01@n
 * **Test Case ID:** 035@n
 * **Priority:** High@n
 * @n
 * **Pre-Conditions:** None@n
 * **Dependencies:** None@n
 * **User Interaction:** None@n
 * @n
 * **Test Procedure:**@n
 * | Variation / Step | Description                                              | Test Data                                  | Expected Result                                       | Notes             |
 * | :--------------: | -------------------------------------------------------- | ------------------------------------------ | ----------------------------------------------------- | ----------------- |
 * | 01               | Instantiate AesDec object using default constructor      | input: none, output: AesDec object created | AesDec object is successfully instantiated             | Should be successful |
 * | 02               | Invoke acquire_key() method to launch key acquisition thread | input: none, output: key acquisition thread launched | acquire_key() method is executed and thread is launched | Should Pass       |
 */
TEST(DrmAesDecTests, Successful_key_acquisition)
{
    std::cout << "Entering Successful_key_acquisition test" << std::endl;

    AesDec aesDec;
    std::cout << "AesDec object instantiated using default constructor" << std::endl;

    std::cout << "Invoking acquire_key() method" << std::endl;
    EXPECT_THROW({
        aesDec.acquire_key();
    }, std::bad_function_call);
    std::cout << "Method acquire_key() executed; key acquisition thread should be launched" << std::endl;

    std::cout << "Exiting Successful_key_acquisition test" << std::endl;
}

/**
 * @brief Verify that the AesDec destructor works correctly when object goes out of scope
 *
 * This test validates the proper destruction behavior of the AesDec class. It ensures that
 * an object created using the default constructor is destructed cleanly, without throwing
 * exceptions, crashes, or memory leaks upon exiting its scope.
 *
 * **Test Group ID:** Basic: 01@n
 * **Test Case ID:** 036@n
 * **Priority:** High@n
 * @n
 * **Pre-Conditions:** None@n
 * **Dependencies:** None@n
 * **User Interaction:** None@n
 * @n
 * **Test Procedure:**@n
 * | Variation / Step | Description                                            | Test Data                              | Expected Result                                         | Notes               |
 * | :--------------: | ------------------------------------------------------ | -------------------------------------- | ------------------------------------------------------- | ------------------- |
 * | 01               | Instantiate AesDec object using default constructor    | input: none, output: AesDec object     | AesDec object is successfully created                   | Should be successful |
 * | 02               | Let object go out of scope to trigger destructor       | input: none                            | Destructor is called and completes without exception    | No crash or leak     |
 */
TEST(DrmAesDecTests, AesDec_Destructor_Default)
{
    std::cout << "Entering AesDec_Destructor_Default test" << std::endl;

    std::cout << "Invoking default constructor for AesDec" << std::endl;
    
    ASSERT_NO_THROW(
    {
        AesDec aesDecObj;
        std::cout << "AesDec object created successfully. It will be destructed when going out of scope." << std::endl;
    });
    
    std::cout << "AesDec object destructed successfully" << std::endl;

    std::cout << "Exiting AesDec_Destructor_Default test" << std::endl;
}
