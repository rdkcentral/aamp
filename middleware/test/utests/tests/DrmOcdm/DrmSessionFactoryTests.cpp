
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
* @file test_l1_DrmSessionFactory.cpp
* @page DrmSessionFactory Level 1 Tests
*
* ## Module's Role
* This module includes Level 1 functional tests (success and failure scenarios).
* This is to ensure that the DrmSessionFactory methods meet the requirements.
*
* **Pre-Conditions:**  None @n
* **Dependencies:** None @n
*
*/

#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include <stdio.h>
#include "DrmSessionFactory.h"

class DummyDrmHelper : public DrmHelper {
public:
       const std::string& ocdmSystemId() const override {
        static std::string systemId = "dummy-system-id";
        return systemId;
    }

    void createInitData(std::vector<uint8_t>& initData) const override {
        initData = {0x01, 0x02, 0x03}; // some dummy data
    }

    bool parsePssh(const uint8_t* initData, uint32_t initDataLen) override {
        // Just pretend parsing is always successful
        return true;
    }

    bool isClearDecrypt() const override {
        return true; // Just a dummy implementation
    }
    
    void getKey(std::vector<uint8_t>& keyID) const override {
        keyID = {0x00, 0x01, 0x02, 0x03}; // some dummy key
    }

    bool isHdcp22Required() const override {
        return false; // Just a dummy implementation
    }

    const std::string& getDrmMetaData() const override {
        static std::string metaData = "dummy-metadata";
        return metaData;
    }

    void setDrmMetaData(const std::string& metaData) override {
        // do nothing for test
    }

    void generateLicenseRequest(const ChallengeInfo& challengeInfo, LicenseRequest& licenseRequest) const override {
        // Just a dummy implementation
        licenseRequest.url = "http://dummy-license-url";
        //licenseRequest.postData = "dummy-post-data";
    }
    static DrmInfo makeDummyDrmInfo() {
        DrmInfo info{};
        // Fill fields with dummy values
        // e.g., info.keySystem = "dummy";
        return info;
    }
    DummyDrmHelper()
        : DrmHelper(makeDummyDrmInfo()) // Call base constructor
    {}
};

class DrmSessionFactoryTests : public ::testing::Test
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
 * @brief Validate that a valid DrmSession is created when using a valid DrmHelper and valid DrmCallbacks.
 *
 * This test verifies that DrmSessionFactory::GetDrmSession returns a non-null DrmSession pointer when supplied
 * with a valid DrmHelper instance and a valid DrmCallbacks implementation. It checks the proper instantiation of
 * the session and the retrieval of the key system from the session.
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
 * | Variation / Step | Description | Test Data | Expected Result | Notes |
 * | :----: | --------- | ---------- |-------------- | ----- |
 * | 01 | Create a valid DrmHelper instance using default constructor | validDrmHelper = instance created using std::make_shared<DrmHelper>() | Instance created with a valid pointer | Should be successful |
 * | 02 | Create a valid DrmCallbacks instance using dummy implementation | drmCallbacksInstance = instance of DummyDrmCallbacks; address valid | Instance created with a valid address | Should be successful |
 * | 03 | Invoke DrmSessionFactory::GetDrmSession with valid DrmHelper and DrmCallbacks | validDrmHelper = valid instance pointer, drmCallbacksInstance = pointer to instance | Return value is a non-null DrmSession pointer | Should Pass |
 * | 04 | Retrieve the key system from the DrmSession if session is valid | session pointer from previous step | Key system successfully retrieved from session | Should be successful |
 */
TEST_F(DrmSessionFactoryTests, ValidDrmHelperValidDrmCallbacks) {
    std::cout << "Entering ValidDrmHelperValidDrmCallbacks test" << std::endl;
    
    // Create a valid DrmHelper instance (assuming default constructor is available)
    std::shared_ptr<DrmHelper> validDrmHelper = std::make_shared<DummyDrmHelper>();
    std::cout << "Created valid DrmHelper instance at address: " << validDrmHelper.get() << std::endl;
    
    // Create a valid DrmCallbacks instance using a local dummy implementation.
    class DummyDrmCallbacks : public DrmCallbacks {
    public:
        void Individualization(const std::string &payload) override {
            std::cout << "DummyDrmCallbacks::Individualization invoked with payload: " << payload << std::endl;
        }
        void LicenseRenewal(std::shared_ptr<DrmHelper> drmHelper, void* userData) override {
            std::cout << "DummyDrmCallbacks::LicenseRenewal invoked with drmHelper address: " << drmHelper.get() 
                      << " and userData: " << userData << std::endl;
        }
    } drmCallbacksInstance;
    std::cout << "Created valid DrmCallbacks instance at address: " << &drmCallbacksInstance << std::endl;
    
    std::cout << "Invoking DrmSessionFactory::GetDrmSession with valid DrmHelper and valid DrmCallbacks" << std::endl;
    DrmSession* session = DrmSessionFactory::GetDrmSession(validDrmHelper, &drmCallbacksInstance);
    std::cout << "DrmSessionFactory::GetDrmSession returned pointer: " << session << std::endl;
    
    EXPECT_NE(session, nullptr);
    if (session) {
        std::string keySystem = session->getKeySystem();
        std::cout << "Retrieved key system from DrmSession: " << keySystem << std::endl;
    }
    
    std::cout << "Exiting ValidDrmHelperValidDrmCallbacks test" << std::endl;
}
/**
 * @brief Validate GetDrmSession behavior with a null DrmHelper and a valid DrmCallbacks instance
 *
 * This test ensures that the DrmSessionFactory::GetDrmSession method correctly handles a scenario where the DrmHelper pointer is null,
 * even when provided with a valid DrmCallbacks instance. The expected behavior is that the function will return a null session pointer,
 * thereby preventing the creation of an invalid or unintended DRM session.
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
 * | Variation / Step | Description                                               | Test Data                                                                                           | Expected Result                                    | Notes             |
 * | :----:           | --------------------------------------------------------- | --------------------------------------------------------------------------------------------------- | -------------------------------------------------- | ----------------- |
 * | 01               | Create a nullptr for DrmHelper                            | DrmHelper = nullptr                                                                                 | DrmHelper pointer is null                          | Should be successful |
 * | 02               | Create a valid DrmCallbacks instance using a dummy implementation | drmCallbacksInstance = DummyDrmCallbacks instance, address provided by local scope                   | DrmCallbacks instance is successfully created      | Should be successful |
 * | 03               | Invoke DrmSessionFactory::GetDrmSession with the above instances | input: DrmHelper = nullptr, drmCallbacks = address of drmCallbacksInstance; output: session pointer      | Returns nullptr session pointer as per expectation | Should Pass       |
 */
TEST_F(DrmSessionFactoryTests, NullDrmHelperValidDrmCallbacks) {
    std::cout << "Entering NullDrmHelperValidDrmCallbacks test" << std::endl;
    
    // Create a nullptr for DrmHelper
    std::shared_ptr<DrmHelper> nullDrmHelper = nullptr;
    std::cout << "Created null DrmHelper instance." << std::endl;
    
    // Create a valid DrmCallbacks instance using a local dummy implementation.
    class DummyDrmCallbacks : public DrmCallbacks {
    public:
        void Individualization(const std::string &payload) override {
            std::cout << "DummyDrmCallbacks::Individualization invoked with payload: " << payload << std::endl;
        }
        void LicenseRenewal(std::shared_ptr<DrmHelper> drmHelper, void* userData) override {
            std::cout << "DummyDrmCallbacks::LicenseRenewal invoked with drmHelper address: " 
                      << (drmHelper ? drmHelper.get() : nullptr) 
                      << " and userData: " << userData << std::endl;
        }
    } drmCallbacksInstance;
    std::cout << "Created valid DrmCallbacks instance at address: " << &drmCallbacksInstance << std::endl;
    
    std::cout << "Invoking DrmSessionFactory::GetDrmSession with null DrmHelper and valid DrmCallbacks" << std::endl;
    DrmSession* session = DrmSessionFactory::GetDrmSession(nullDrmHelper, &drmCallbacksInstance);
    std::cout << "DrmSessionFactory::GetDrmSession returned pointer: " << session << std::endl;
        
    EXPECT_EQ(session, nullptr);
    
    std::cout << "Exiting NullDrmHelperValidDrmCallbacks test" << std::endl;
}
/**
 * @brief Test that GetDrmSession returns nullptr when provided with a valid DrmHelper and null DrmCallbacks
 *
 * This test verifies that the DrmSessionFactory::GetDrmSession method correctly handles a scenario where a valid DrmHelper instance is provided but the DrmCallbacks pointer is null, ensuring that the API returns a null pointer as expected.
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
 * | :----: | --------- | ---------- | -------------- | ----- |
 * | 01 | Create a valid DrmHelper instance using its default constructor | validDrmHelper = std::make_shared<DrmHelper>() | A valid DrmHelper instance is created | Should be successful |
 * | 02 | Set DrmCallbacks pointer to nullptr | nullDrmCallbacks = nullptr | DrmCallbacks pointer is set to null | Should be successful |
 * | 03 | Invoke DrmSessionFactory::GetDrmSession with the valid DrmHelper and null DrmCallbacks | validDrmHelper, nullDrmCallbacks = nullptr | DrmSession pointer returned is nullptr | Should Pass |
 * | 04 | Verify the returned DrmSession pointer is nullptr using EXPECT_EQ | session = GetDrmSession(validDrmHelper, nullDrmCallbacks) | Assertion passes confirming session is nullptr | Should Pass |
 */
TEST_F(DrmSessionFactoryTests, ValidDrmHelperNullDrmCallbacks) {
    std::cout << "Entering ValidDrmHelperNullDrmCallbacks test" << std::endl;
    
    // Create a valid DrmHelper instance (assuming default constructor is available)
    std::shared_ptr<DrmHelper> validDrmHelper = std::make_shared<DummyDrmHelper>();
    std::cout << "Created valid DrmHelper instance at address: " << validDrmHelper.get() << std::endl;
    
    // Use a nullptr for DrmCallbacks
    DrmCallbacks* nullDrmCallbacks = nullptr;
    std::cout << "Using null DrmCallbacks instance." << std::endl;
    
    std::cout << "Invoking DrmSessionFactory::GetDrmSession with valid DrmHelper and null DrmCallbacks" << std::endl;
    DrmSession* session = DrmSessionFactory::GetDrmSession(validDrmHelper, nullDrmCallbacks);
    std::cout << "DrmSessionFactory::GetDrmSession returned pointer: " << session << std::endl;
    
    EXPECT_EQ(session, nullptr);
    
    std::cout << "Exiting ValidDrmHelperNullDrmCallbacks test" << std::endl;
}