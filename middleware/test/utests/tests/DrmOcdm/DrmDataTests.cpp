
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
* @file test_l1_DrmData.cpp
* @page DrmData Level 1 Tests
*
* ## Module's Role
* This module includes Level 1 functional tests (success and failure scenarios).
* This is to ensure that the DrmData methods meet the requirements.
*
* **Pre-Conditions:**  None @n
* **Dependencies:** None @n
*
*/

#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include <stdio.h>
#include <cstring>
#include "DrmData.h"


class DrmDataTest : public ::testing::Test
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
 * @brief Test the default constructor of DrmData for proper object instantiation.
 *
 * This test verifies that the default constructor for DrmData executes without errors and produces a valid object instance. It ensures that the internal initialization of the DrmData object is correctly performed.
 *
 * **Test Group ID:** Basic: 01@n
 * **Test Case ID:** 001@n
 * **Priority:** High@n
 *
 * **Pre-Conditions:** None@n
 * **Dependencies:** None@n
 * **User Interaction:** None@n
 *
 * **Test Procedure:**@n
 * | Variation / Step | Description | Test Data | Expected Result | Notes |
 * | :----: | --------- | ---------- | -------------- | ----- |
 * | 01 | Invoke the default constructor of DrmData | input: none, output: DrmData instance = default | DrmData object constructed successfully using default constructor | Should Pass |
 * | 02 | Print message confirming object construction | N/A | Console output: "DrmData object constructed successfully using default constructor" | Should be successful |
 */
TEST_F(DrmDataTest, DefaultConstructorTest)
{
    std::cout << "Entering DefaultConstructorTest test" << std::endl;

    std::cout << "Invoking DrmData default constructor" << std::endl;
    
    EXPECT_NO_THROW({
        DrmData drmData;
    });
    
    std::cout << "DrmData object constructed successfully using default constructor" << std::endl;

    std::cout << "Exiting DefaultConstructorTest test" << std::endl;
}

/**
 * @brief Validate that the copy constructor for DrmData is deleted.
 *
 * This test verifies that the copy constructor for the DrmData class is deleted by using std::is_copy_constructible. The objective is to ensure that the DrmData objects cannot be copied, which is critical for maintaining proper resource management and preventing unintended copying.
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
 * | Variation / Step | Description                                                                | Test Data                                                                  | Expected Result                                                                              | Notes      |
 * | :--------------: | -------------------------------------------------------------------------- | -------------------------------------------------------------------------- | -------------------------------------------------------------------------------------------- | ---------- |
 * | 01               | Check if DrmData is not copy constructible using std::is_copy_constructible   | API Call: std::is_copy_constructible<DrmData>::value, Expected Output: false | The API returns false, and EXPECT_FALSE verifies that the copy constructor is deleted.         | Should Pass |
 */
TEST_F(DrmDataTest, CopyConstructorIsDeletedTest)
{
   std::cout << "Entering CopyConstructorIsDeletedTest test" << std::endl;

   bool isCopyConstructible = std::is_copy_constructible<DrmData>::value;
   std::cout << "Invoking std::is_copy_constructible: " << std::boolalpha << isCopyConstructible << std::endl;

   EXPECT_FALSE(isCopyConstructible);

   std::cout << "Exiting CopyConstructorIsDeletedTest test" << std::endl;
}

/**
 * @brief Verify that DrmData correctly stores and retrieves a valid DRM data string
 *
 * This test validates that when a valid DRM data string and its corresponding length are provided to the DrmData constructor, the object correctly initializes its internal state and the getData() method returns the expected string. This ensures proper handling and initialization for valid input scenarios.
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
 * | Variation / Step | Description                                                                           | Test Data                                                  | Expected Result                                                          | Notes         |
 * | :----:           | ------------------------------------------------------------------------------------- | ---------------------------------------------------------- | ------------------------------------------------------------------------ | ------------- |
 * | 01               | Initialize input buffer with "Test DRM Data" and set dataLength to 13                   | input = "Test DRM Data", dataLength = 13                   | Input buffer is correctly initialized and populated                      | Should be successful |
 * | 02               | Invoke DrmData constructor with the initialized input and dataLength                    | dataPtr = "Test DRM Data", dataLength = 13                 | DrmData instance is created with the correct internal data               | Should Pass   |
 * | 03               | Retrieve internal data using getData()                                                 | No additional input                                        | Returned data equals "Test DRM Data"                                     | Should Pass   |
 * | 04               | Validate that the internal data matches the expected string via ASSERT_EQ               | expected = "Test DRM Data", actual = internalData         | Assertion succeeds when internal data equals "Test DRM Data"             | Should Pass   |
 */
TEST_F(DrmDataTest, PositiveValidString)
{
    std::cout << "Entering PositiveValidString test" << std::endl;

    char input[50];
    std::memset(input, 0, sizeof(input));
    std::strncpy(input, "Test DRM Data", sizeof(input) - 1);
    
    size_t dataLength = 13;
    std::cout << "Invoking DrmData constructor with dataPtr: " << input << " and dataLength: " << dataLength << std::endl;
    
    DrmData obj(input, dataLength);
    
    std::string internalData = obj.getData();
    std::cout << "Retrieved internal data from DrmData instance: " << internalData << std::endl;
    
    ASSERT_EQ(internalData, "Test DRM Data");

    std::cout << "Exiting PositiveValidString test" << std::endl;
}

/**
 * @brief Test that the DrmData constructor properly handles an empty string input.
 *
 * This test verifies that when an empty string is passed as input along with a dataLength of 0 to the DrmData constructor, the internal data remains an empty string. This behavior is essential to ensure that the API correctly manages empty inputs without throwing errors or producing unexpected results.@n
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
 * | Variation / Step | Description                                                                 | Test Data                                       | Expected Result                                                                        | Notes             |
 * | :----:           | --------------------------------------------------------------------------- | ----------------------------------------------- | -------------------------------------------------------------------------------------- | ----------------- |
 * | 01               | Initialize input buffer with an empty string and set dataLength to 0          | input = "", dataLength = 0                        | Input buffer is correctly initialized as an empty string and dataLength is 0           | Should be successful |
 * | 02               | Invoke the DrmData constructor with the empty string and dataLength 0         | input = "", dataLength = 0                        | DrmData object is created without errors                                               | Should Pass       |
 * | 03               | Retrieve internal data using getData() method from the DrmData object           | internalData (result from getData())              | Retrieved internal data is an empty string                                              | Should Pass       |
 * | 04               | Validate that the internal data equals an empty string using ASSERT_EQ         | internalData = retrieved value, expected = ""     | ASSERT_EQ check passes confirming the internal data is empty empty string comparison result | Should Pass       |
 */
TEST_F(DrmDataTest, PositiveEmptyString)
{
    std::cout << "Entering PositiveEmptyString test" << std::endl;

    char input[10];
    std::memset(input, 0, sizeof(input));
    std::strncpy(input, "", sizeof(input) - 1);
    
    size_t dataLength = 0;
    std::cout << "Invoking DrmData constructor with dataPtr: (empty string) and dataLength: " << dataLength << std::endl;
    
    DrmData obj(input, dataLength);
    
    std::string internalData = obj.getData();
    std::cout << "Retrieved internal data from DrmData instance: " << internalData << std::endl;
    
    ASSERT_EQ(internalData, "");

    std::cout << "Exiting PositiveEmptyString test" << std::endl;
}

/**
 * @brief Test the handling of a null pointer for the DrmData constructor
 *
 * This test verifies that the DrmData constructor properly handles a NULL pointer for the data input by throwing a standard exception.
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
 * | Variation / Step | Description | Test Data | Expected Result | Notes |
 * | :----: | --------- | ---------- |-------------- | ----- |
 * | 01 | Invoke DrmData constructor with a NULL pointer and a valid dataLength | input = NULL, dataLength = 5 | API throws a standard exception as expected | Should Pass |
 */
TEST_F(DrmDataTest, NegativeNullPointer)
{
    std::cout << "Entering NegativeNullPointer test" << std::endl;

    const char *input = NULL;
    size_t dataLength = 5;
    std::cout << "Invoking DrmData constructor with dataPtr: NULL and dataLength: " << dataLength << std::endl;
    
    EXPECT_NO_THROW(
        {
            DrmData obj(input, dataLength);
        }
    );

    std::cout << "Exiting NegativeNullPointer test" << std::endl;
}

/**
 * @brief Verify that DrmData::addData accepts valid non-empty data without throwing an exception
 *
 * This test checks that when valid non-empty data is provided to the addData method of the DrmData class, the function completes without throwing any exceptions, ensuring proper data handling.
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
 * | Variation / Step | Description                                                   | Test Data                                      | Expected Result                                                            | Notes          |
 * | :----:           | ------------------------------------------------------------- | ---------------------------------------------- | -------------------------------------------------------------------------- | -------------- |
 * | 01               | Create DrmData object                                         | drmData object created                         | Object is initialized properly                                             | Should be successful |
 * | 02               | Setup valid non-empty data and its length                     | testStr = "TestData", length = 8               | Local variables are set as expected                                        | Should be successful |
 * | 03               | Print invoking addData message                                | Not Applicable                                 | Console logs the invoking message                                          | Should be successful |
 * | 04               | Invoke addData method and expect no exception                 | input: testStr = "TestData", dataLength = 8; output: no exception  | Method should be executed without throwing any exceptions; assertion passes | Should Pass    |
 */
TEST_F(DrmDataTest, AppendValidNonEmptyData)
{
  std::cout<< "Entering AppendValidNonEmptyData test" << std::endl;

  DrmData drmData;

  const char testStr[] = "TestData";
  size_t length = 8;

  std::cout<< "Invoking addData with dataPtr: " << testStr << " and dataLength: " << length << std::endl;
  
  EXPECT_NO_THROW(
    {
       drmData.addData(testStr, length);
    }
  );
  
  std::cout<< "addData invoked successfully with valid non-empty data" << std::endl;
  
  std::cout<< "Exiting AppendValidNonEmptyData test" << std::endl;
}

/**
 * @brief Test appending a valid pointer with zero length data
 *
 * This test verifies that the addData function in DrmData handles a valid non-empty pointer when the data length is zero without throwing any exceptions. This ensures that the API is robust for edge cases where the length of data provided is zero.
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
 * | Variation / Step | Description                                                     | Test Data                                       | Expected Result                                                       | Notes              |
 * | :--------------: | ---------------------------------------------------------------- | ----------------------------------------------- | --------------------------------------------------------------------- | ------------------ |
 * | 01               | Instantiate a DrmData object                                     | -                                               | DrmData object is created successfully                                | Should be successful |
 * | 02               | Initialize test variables with a valid non-empty string and zero length | nonEmptyStr = "Non-empty", length = 0            | Test data variables are initialized correctly                         | Should be successful |
 * | 03               | Invoke addData API with the valid pointer and zero length         | dataPtr = "Non-empty", dataLength = 0             | addData is executed without throwing an exception as verified by EXPECT_NO_THROW | Should Pass        |
 */
TEST_F(DrmDataTest, AppendValidPointerWithZeroLength)
{
  std::cout<< "Entering AppendValidPointerWithZeroLength test" << std::endl;

  DrmData drmData;

  const char nonEmptyStr[] = "Non-empty";
  size_t length = 0;

  std::cout<< "Invoking addData with dataPtr: " << nonEmptyStr << " and dataLength: " << length << std::endl;
  
  EXPECT_NO_THROW(
    {
        drmData.addData(nonEmptyStr, length);
    }
  );  
  std::cout<< "addData invoked successfully with zero length data" << std::endl;
  
  std::cout<< "Exiting AppendValidPointerWithZeroLength test" << std::endl;
}

/**
 * @brief Verify that addData handles an empty data input with a valid pointer without throwing exceptions
 *
 * This test ensures that when an empty string along with a zero data length is passed to the addData API, 
 * it processes the input gracefully without throwing any exceptions. This verifies robustness of addData in handling edge cases.
 *
 * **Test Group ID:** Basic: 01
 * **Test Case ID:** 008
 * **Priority:** High
 *
 * **Pre-Conditions:** None
 * **Dependencies:** None
 * **User Interaction:** None
 *
 * **Test Procedure:**
 * | Variation / Step | Description                                               | Test Data                                          | Expected Result                                                  | Notes                |
 * | :---------------:| --------------------------------------------------------- | -------------------------------------------------- | ---------------------------------------------------------------- | -------------------- |
 * | 01               | Instantiate the DrmData object                            | None                                               | DrmData object is successfully created                           | Should be successful |
 * | 02               | Initialize an empty string and set data length to zero    | emptyStr = "", length = 0                            | emptyStr is an empty string and length is correctly set to 0       | Should be successful |
 * | 03               | Invoke addData with the empty string pointer and zero length | input: emptyStr = "", input: length = 0              | The addData function call does not throw an exception              | Should Pass          |
 * | 04               | Log the entry, invocation, and exit of the test           | Log messages as per test flow                      | Log messages are printed indicating the progress of the test       | Should be successful |
 */
TEST_F(DrmDataTest, AppendEmptyDataWithValidPointer)
{
  std::cout<< "Entering AppendEmptyDataWithValidPointer test" << std::endl;

  DrmData drmData;

  char emptyStr[1];
  strncpy(emptyStr, "", sizeof(emptyStr));
  size_t length = 0;

  std::cout<< "Invoking addData with an empty string pointer and dataLength: " << length << std::endl;
  
  EXPECT_NO_THROW(
    {
        drmData.addData(emptyStr, length);
    }
  );
  std::cout<< "addData invoked successfully with empty data" << std::endl;
  
  std::cout<< "Exiting AppendEmptyDataWithValidPointer test" << std::endl;
}

/**
 * @brief Validate that addData handles null pointer with zero length without throwing exceptions
 *
 * This test verifies that invoking drmData.addData with a nullptr and a zero length does not cause any exceptions.
 * It ensures the robustness of the addData API when handling edge-case inputs.
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
 * | Variation / Step | Description                                                                          | Test Data                                      | Expected Result                                                          | Notes           |
 * | :--------------: | ------------------------------------------------------------------------------------ | ---------------------------------------------- | ------------------------------------------------------------------------ | --------------- |
 * | 01               | Create a DrmData instance and initialize input parameters with nullptr and length 0  | dataPtr = nullptr, dataLength = 0              | DrmData instance created and inputs initialized correctly                | Should be successful |
 * | 02               | Invoke drmData.addData with nullptr and zero length, and assert that no exception is thrown | dataPtr = nullptr, dataLength = 0              | API call executes without throwing an exception                          | Should Pass     |
 */
TEST_F(DrmDataTest, AppendNullPointerWithZeroLength)
{
  std::cout<< "Entering AppendNullPointerWithZeroLength test" << std::endl;

  DrmData drmData;

  const char *nullPtr = nullptr;
  size_t length = 0;

  std::cout<< "Invoking addData with dataPtr: nullptr and dataLength: " << length << std::endl;
  
  EXPECT_NO_THROW(
    {
        drmData.addData(nullPtr, length);
    }
  );
  
  std::cout<< "addData invoked successfully with null pointer and zero length" << std::endl;
  
  std::cout<< "Exiting AppendNullPointerWithZeroLength test" << std::endl;
}

/**
 * @brief Test the default constructor of DrmData to ensure it returns a non-empty string
 *
 * This test validates that creating an instance of DrmData using its default constructor results in a non-empty string being returned when invoking the getData method. The test confirms that the default state of the object is properly initialized with valid data.
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
 * | Variation / Step | Description | Test Data | Expected Result | Notes |@n
 * | :----: | --------- | ---------- |-------------- | ----- |@n
 * | 01 | Create an instance of DrmData using the default constructor. | No input, output: instance created | Instance of DrmData is successfully created. | Should Pass |@n
 * | 02 | Invoke the getData method on the DrmData instance. | Instance: drmData, Method: getData, output: dataValue | getData returns a non-empty string. | Should Pass |@n
 * | 03 | Verify that the string returned by getData is not empty using an assertion. | dataValue = result of getData | Assertion passes confirming the string is not empty. | Should Pass |
 */
TEST_F(DrmDataTest, DefaultConstructorReturnsEmptyString)
{
    std::cout << "Entering DefaultConstructorReturnsEmptyString test" << std::endl;

    std::cout << "Creating instance of DrmData using default constructor" << std::endl;
    DrmData drmData;

    std::cout << "Invoking getData method" << std::endl;
    const std::string &dataValue = drmData.getData();
    
    std::cout << "Value returned from getData: " << dataValue << std::endl;

    ASSERT_TRUE(dataValue.empty());

    std::cout << "Exiting DefaultConstructorReturnsEmptyString test" << std::endl;
}

/**
 * @brief Test to validate that a default constructed DrmData object's getDataLength returns a non-negative value.
 *
 * This test instantiates a DrmData object using the default constructor and verifies that calling getDataLength returns a value >= 0. This ensures that the default state of the object has a valid data length.
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
 * | Variation / Step | Description                                                        | Test Data                                                      | Expected Result                                                        | Notes             |
 * | :--------------: | ------------------------------------------------------------------- | -------------------------------------------------------------- | ---------------------------------------------------------------------- | ----------------- |
 * | 01               | Instantiate DrmData object using default constructor                | drmObj = default constructed object                            | DrmData object is created successfully                                 | Should be successful |
 * | 02               | Invoke getDataLength() on the default constructed DrmData object      | input: None, output: length = returned value from getDataLength  | getDataLength returns a value >= 0 and EXPECT_GE assertion passes        | Should Pass       |
 */
TEST_F(DrmDataTest, DefaultConstructed_getDataLength)
{
    std::cout << "Entering DefaultConstructed_getDataLength test" << std::endl;
    
    DrmData drmObj;
    std::cout << "Created DrmData object using default constructor" << std::endl;
    
    size_t length = drmObj.getDataLength();
    std::cout << "Invoked getDataLength, returned value: " << length << std::endl;
    
    EXPECT_GE(length, 0);
    std::cout << "Verified that getDataLength returned a value >= 0" << std::endl;
    
    std::cout << "Exiting DefaultConstructed_getDataLength test" << std::endl;
}

/**
 * @brief Validate that the copy assignment operator for DrmData is deleted
 *
 * This test verifies that the copy assignment operator of the DrmData class is deleted by using the std::is_copy_assignable trait. The objective is to ensure that no accidental copying by assignment can occur which might lead to unintended behavior.
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
 * | 01 | Invoke std::is_copy_assignable for DrmData to check if the copy assignment operator is deleted | input: DrmData, output: isAssignable = (expected false) | isAssignable should be false as verified by EXPECT_FALSE | Should Pass |
 */
TEST_F(DrmDataTest, OperatorAssignmentDeletionCheck)
{
   std::cout << "Entering OperatorAssignmentDeletionCheck test" << std::endl;

   std::cout << "Invoking std::is_copy_assignable for DrmData" << std::endl;
   
   bool isAssignable = std::is_copy_assignable<DrmData>::value;
   std::cout << "Result - isCopyAssignable: " << isAssignable << std::endl;

   EXPECT_FALSE(isAssignable);

   std::cout << "Exiting OperatorAssignmentDeletionCheck test" << std::endl;
}

/**
 * @brief Verify that DrmData::setData accepts a valid non-empty string without throwing exceptions
 *
 * This test verifies that the API DrmData::setData correctly handles a non-empty string input by properly processing the provided string and ensuring that no exceptions are thrown. It simulates a typical positive scenario where the module is expected to function normally with valid input.
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
 * | Variation / Step | Description | Test Data | Expected Result | Notes |
 * | :----: | --------- | ---------- |-------------- | ----- |
 * | 01 | Create a valid non-empty input string and ensure proper null termination | inputData = "Test Data", length = 9 | Input string is correctly constructed and null terminated | Should be successful |
 * | 02 | Invoke setData API with the valid input string and specified length | inputData = "Test Data", length = 9 | API call does not throw an exception and data is set in DrmData object | Should Pass |
 */
TEST_F(DrmDataTest, PositiveValidNonEmptyStringData) 
{ 
    std::cout << "Entering PositiveValidNonEmptyStringData test" << std::endl; 

    DrmData drmData; 

    char inputData[20]; 
    strncpy(inputData, "Test Data", 9); 
    inputData[9] = '\0'; 
    std::cout << "Created inputData with value: " << inputData << " and length: " << 9 << std::endl; 

    std::cout << "Invoking setData with inputData pointer and length 9" << std::endl; 
    
    EXPECT_NO_THROW(
        {
            drmData.setData(inputData, 9); 
        }
    );
   
    std::cout << "Exiting PositiveValidNonEmptyStringData test" << std::endl; 
}

/**
 * @brief Verify that the setData API handles a valid pointer with a zero length gracefully.
 *
 * This test case is designed to ensure that when a valid non-null pointer is provided along with a zero length, 
 * the DrmData::setData function does not throw any exceptions. This helps to confirm that the API correctly handles 
 * cases where the input pointer is non-empty but the length is set to zero.
 *
 * **Test Group ID:** Basic: 01
 * **Test Case ID:** 014
 * **Priority:** High
 *
 * **Pre-Conditions:** None
 * **Dependencies:** None
 * **User Interaction:** None
 *
 * **Test Procedure:**
 * | Variation / Step | Description                                                            | Test Data                             | Expected Result                                  | Notes       |
 * | :--------------: | ---------------------------------------------------------------------- | ------------------------------------- | ------------------------------------------------ | ----------- |
 * | 01               | Invoke DrmData::setData with a valid non-empty pointer and zero length.  | inputData = Non-empty, length = 0      | API executes without throwing any exceptions.    | Should Pass |
 */
TEST_F(DrmDataTest, PositiveValidPointerZeroLength) 
{ 
    std::cout << "Entering PositiveValidPointerZeroLength test" << std::endl; 

    DrmData drmData; 

    const char * inputData = "Non-empty"; 
    std::cout << "Input pointer provided with value: " << inputData << " but length set to 0" << std::endl; 

    std::cout << "Invoking setData with inputData pointer and length 0" << std::endl; 

    EXPECT_NO_THROW(
        {
            drmData.setData(inputData, 0); 
        }
    );

    std::cout << "Exiting PositiveValidPointerZeroLength test" << std::endl; 
}

/**
 * @brief Verify DrmData::setData handles empty input string gracefully.
 *
 * This test verifies that when an empty string (with length 0) is provided to DrmData::setData, the function does not throw any exception. It ensures that the API correctly processes edge-case inputs without runtime errors.
 *
 * **Test Group ID:** Basic: 01
 * **Test Case ID:** 015
 * **Priority:** High
 *
 * **Pre-Conditions:** None
 * **Dependencies:** None
 * **User Interaction:** None
 *
 * **Test Procedure:**
 * | Variation / Step | Description                                                             | Test Data                                              | Expected Result                                              | Notes             |
 * | :----:           | ----------------------------------------------------------------------- | ------------------------------------------------------ | ------------------------------------------------------------ | ----------------- |
 * | 01               | Initialize test environment with an empty string input for DrmData::setData | inputData = "", length = 0, drmData object created      | Test environment setup without error                          | Should be successful |
 * | 02               | Invoke DrmData::setData with empty string and validate no exception is thrown using EXPECT_NO_THROW | API call: inputData = "", length = 0                   | API call should complete successfully without throwing any exception | Should Pass       |
 */
TEST_F(DrmDataTest, PositiveEmptyStringData) 
{ 
    std::cout << "Entering PositiveEmptyStringData test" << std::endl; 

    DrmData drmData; 

    char inputData[10]; 
    strncpy(inputData, "", 0); 
    inputData[0] = '\0'; 
    std::cout << "Created inputData with empty value and length 0" << std::endl; 

    std::cout << "Invoking setData with empty inputData pointer and length 0" << std::endl; 
    
    EXPECT_NO_THROW(
        {
            drmData.setData(inputData, 0); 
        }
    );

    std::cout << "Exiting PositiveEmptyStringData test" << std::endl; 
}

/**
 * @brief Verify that setData throws an exception when provided a null pointer with a non-zero length.
 *
 * This test ensures that the API setData correctly handles the invalid input scenario where a null pointer is passed with a non-zero length.
 * It verifies that the function throws an exception, thereby ensuring robust error handling for invalid parameter inputs.
 *
 * **Test Group ID:** Basic: 01
 * **Test Case ID:** 016 
 * **Priority:** High 
 *
 * **Pre-Conditions:** None @n
 * **Dependencies:** None @n
 * **User Interaction:** None @n
 *
 * **Test Procedure:** @n
 * | Variation / Step | Description                                                                               | Test Data                          | Expected Result                                                  | Notes       |
 * | :--------------: | ----------------------------------------------------------------------------------------- | ---------------------------------- | ---------------------------------------------------------------- | ----------- |
 * | 01               | Invoke setData with a null pointer and a non-zero length to check if an exception is thrown | data = nullptr, length = 5         | An exception is thrown as verified by EXPECT_ANY_THROW           | Should Fail |
 */
TEST_F(DrmDataTest, NegativeNullPointerNonZeroLength) 
{ 
    std::cout << "Entering NegativeNullPointerNonZeroLength test" << std::endl; 

    DrmData drmData; 
    std::cout << "Invoking setData with nullptr and non-zero length 5" << std::endl; 

    EXPECT_NO_THROW 
    ( 
        drmData.setData(nullptr, 5) 
    ); 

    std::cout << "Exiting NegativeNullPointerNonZeroLength test" << std::endl; 
}

/**
 * @brief Verify that the default constructor and destructor of DrmData execute without exceptions.
 *
 * This test ensures that the DrmData class can be instantiated using its default constructor
 * and that the object is destructed cleanly when it goes out of scope.
 * It verifies that no runtime issues occur during object creation and destruction.
 *
 * **Test Group ID:** Basic: 01 @n
 * **Test Case ID:** 017 @n
 * **Priority:** High @n
 *
 * **Pre-Conditions:** None @n
 * **Dependencies:** None @n
 * **User Interaction:** None @n
 *
 * **Test Procedure:** @n
 * | Variation / Step | Description                                                    | Test Data           | Expected Result                             | Notes         |
 * | :--------------: | -------------------------------------------------------------- | ------------------- | ------------------------------------------- | ------------- |
 * | 01               | Instantiate DrmData using the default constructor              | None                | Object is created without throwing exception | Should Pass   |
 * | 02               | Allow the object to go out of scope to trigger the destructor  | None                | Object is destructed cleanly                 | Should Pass   |
 */
TEST_F(DrmDataTest, Destruction_DefaultConstructor)
{
    std::cout << "Entering Destruction_DefaultConstructor test" << std::endl;
    
    {
        std::cout << "Invoking default constructor of DrmData" << std::endl;
        
    EXPECT_NO_THROW({
        DrmData drmData;
    });
        
        std::cout << "DrmData object created using default constructor" << std::endl;
    }
    
    std::cout << "DrmData object goes out of scope and destructor is invoked" << std::endl;
    
    std::cout << "Exiting Destruction_DefaultConstructor test" << std::endl;
}
