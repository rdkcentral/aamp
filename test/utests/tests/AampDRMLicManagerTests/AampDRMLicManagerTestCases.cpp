/*
 * If not stated otherwise in this file or this component's license file the
 * following copyright and licenses apply:
 *
 * Copyright 2026 RDK Management
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
#include <memory>
#include <vector>
#include <thread>
#include <chrono>
#include "AampConfig.h"
#include "priv_aamp.h"
#include "MockPrivateInstanceAAMP.h"
#include "MockStreamAbstractionAAMP.h"
#include "DrmHelper.h"
#include "DrmSession.h"
#include "AampDRMLicManager.h"
#include "DrmSessionManager.h"
#include "MockAampDRMSessionManager.h"
#include "MockDrmHelper.h"

// Forward-declare the mock license manager to avoid re-including
// AampDRMLicManager.h (that header lacks include-guards in this
// test environment and causes redefinition errors).
class MockAampLicenseManager;
extern std::shared_ptr<MockAampLicenseManager> g_mockAampLicenseManager;
#include "MockDrmMetaDataEvent.h"
#include "PlayerUtils.h"

// External mock pointer from FakeAampEvent.cpp
extern std::shared_ptr<MockDrmMetaDataEvent> g_mockDrmMetaDataEvent;

using ::testing::_;
using ::testing::DoAll;
using ::testing::Invoke;
using ::testing::NiceMock;
using ::testing::Return;
using ::testing::SetArgReferee;
using ::testing::StrictMock;

// ============================================================================
// CONSTANT KEY IDs FOR DRM TESTING
// ============================================================================
// Reusable key IDs for all test scenarios. These represent different
// adaptation/quality slots (UHD/HD/SD) or track types (VIDEO/AUDIO).

constexpr const char* TEST_KEY_ID_SLOT_0 = "11111111111111111111111111111111"; // UHD/Highest quality
constexpr const char* TEST_KEY_ID_SLOT_1 = "22222222222222222222222222222222"; // HD/Mid quality
constexpr const char* TEST_KEY_ID_SLOT_2 = "33333333333333333333333333333333"; // SD/Low quality
constexpr const char* TEST_KEY_ID_AUDIO   = "fedcba9876543210fedcba9876543210"; // AUDIO track

/**
 * @brief Minimal test implementation of DrmSession for testing
 */
class TestDrmSession : public DrmSession
{
private:
	KeyState mState;
	std::vector<std::vector<uint8_t>> mUsableKeys;

public:
	TestDrmSession() : DrmSession("test-key-system"), mState(KEY_READY) {}

	void generateDRMSession(const uint8_t *f_pbInitData, uint32_t f_cbInitData, std::string &customData) override
	{
		// Test implementation - no-op
		mState = KEY_PENDING;
	}

	DrmData *generateKeyRequest(string &destinationURL, uint32_t timeout) override
	{
		// Test implementation - return nullptr for now
		destinationURL = "http://test-license-server.com";
		return nullptr;
	}

	int processDRMKey(DrmData *key, uint32_t timeout) override
	{
		// Test implementation - return success
		mState = KEY_READY;
		return 0;
	}

	KeyState getState() override
	{
		return mState;
	}

	void clearDecryptContext() override
	{
		// Test implementation - reset state
		mState = KEY_INIT;
		mUsableKeys.clear();
	}

	std::vector<std::vector<uint8_t>> getUsableKeys() const override
	{
		return mUsableKeys;
	}

	void setUsableKeys(const std::vector<std::vector<uint8_t>> &keys)
	{
		mUsableKeys = keys;
	}

	void setState(KeyState state)
	{
		mState = state;
	}
};

/**
 * @brief Minimal test implementation of DrmHelper for testing
 */
class TestDrmHelper : public DrmHelper
{
private:
	std::vector<uint8_t> mKeyId;
	std::map<int, std::vector<uint8_t>> mKeyIds; // Multiple keys for multi-key scenarios
	std::string mSystemId;

public:
	TestDrmHelper(const DrmInfo &drmInfo, const std::vector<uint8_t> &keyId)
		: DrmHelper(drmInfo), mKeyId(keyId), mSystemId("test-system-id") {}

	// Constructor for multi-key scenarios
	TestDrmHelper(const DrmInfo &drmInfo, const std::vector<uint8_t> &primaryKeyId,
				  const std::map<int, std::vector<uint8_t>> &allKeyIds)
		: DrmHelper(drmInfo), mKeyId(primaryKeyId), mKeyIds(allKeyIds), mSystemId("test-system-id") {}

	const std::string &ocdmSystemId() const override { return mSystemId; }

	void createInitData(std::vector<uint8_t> &initData) const override
	{
		initData = mKeyId;
	}

	bool parsePssh(const uint8_t *initData, uint32_t initDataLen) override
	{
		if (initData && initDataLen > 0)
		{
			mKeyId.assign(initData, initData + initDataLen);
			return true;
		}
		return false;
	}

	bool isClearDecrypt() const override { return true; }

	void getKey(std::vector<uint8_t> &keyID) const override
	{
		keyID = mKeyId;
	}

	void getKeys(std::map<int, std::vector<uint8_t>> &keyIDs) const override
	{
		if (!mKeyIds.empty())
		{
			keyIDs = mKeyIds;
		}
	}

	void generateLicenseRequest(const ChallengeInfo &challengeInfo, LicenseRequest &licenseRequest) const override
	{
		licenseRequest.method = LicenseRequest::POST;
		licenseRequest.url = "http://test-license-server.com";
	}
};
/**
 * @brief Test fixture for AampDRMLicManagerTests integration tests
 */
class AampDRMLicManagerTests : public ::testing::Test
{
protected:
	std::unique_ptr<PrivateInstanceAAMP> mPrivateInstanceAAMP;
	std::unique_ptr<AampDRMLicenseManager> mTestableDRMLicenseManager;

	void SetUp() override
	{
		// Initialize mocks
		g_mockPrivateInstanceAAMP = std::make_shared<NiceMock<MockPrivateInstanceAAMP>>();
		g_mockDRMSessionManager = std::make_shared<NiceMock<MockDRMSessionManager>>();
		
		mPrivateInstanceAAMP.reset(new PrivateInstanceAAMP());
		mTestableDRMLicenseManager.reset(new AampDRMLicenseManager(5, mPrivateInstanceAAMP.get()));
		// Capture the original DrmSessionManager instance so we can restore
		// it in TearDown after tests replace it with the mock.
		mPrivateInstanceAAMP->mDRMLicenseManager = mTestableDRMLicenseManager.get();
	}

	void TearDown() override
	{
		mTestableDRMLicenseManager.reset();
		mPrivateInstanceAAMP.reset();

	// Delete the test-created mock DRMSessionManager now that we've
	// restored the original pointer in the license manager to avoid
	// leaks. This must be done after restoring mDrmSessionManager so
	// the license manager destructor doesn't attempt to delete the
	// mock as well.
	g_mockDRMSessionManager.reset();

	g_mockPrivateInstanceAAMP.reset();
	}
};
/**
 * @brief Test OCDM session construction failure error mapping
 * 
 * Validates that MW_DRM_SESSION_CREATE_FAILED from middleware layer
 * is properly mapped to AAMP_TUNE_DRM_SESSION_CREATE_FAILED at player layer,
 * which corresponds to the error message "OCDM session construction failed".
 * 
 * Test Flow:
 * 1. Mock g_mockAampLicenseManager->createDrmSession to invoke a custom action
 * 2. In that action, call g_mockDRMSessionManager->initializeDrmSession 
 * 3. initializeDrmSession sets err = MW_DRM_SESSION_CREATE_FAILED
 * 4. The fake createDrmSession should then call the mapping logic
 * 5. Verify eventHandle contains AAMP_TUNE_DRM_SESSION_CREATE_FAILED
 */
TEST_F(AampDRMLicManagerTests, ValidateOCDMSessionConstructFailure)
{
	// Create a MockDrmHelper
	std::shared_ptr<MockDrmHelper> drmHelper = std::make_shared<MockDrmHelper>();

	// Set up mock expectations for DrmHelper
	std::string systemId = "com.widevine.alpha";
	EXPECT_CALL(*drmHelper, ocdmSystemId())
		.WillRepeatedly(testing::ReturnRef(systemId));

	// Create the global mock event directly
	g_mockDrmMetaDataEvent = std::make_shared<MockDrmMetaDataEvent>(
		AAMP_TUNE_FAILURE_UNKNOWN,  // Initial failure state
		"",                          // Access status
		0,                           // Status value
		0,                           // Response code
		false,                       // Secclient error
		""                           // Session ID
	);

	// CRITICAL: Expect that setFailure() IS called exactly once with AAMP_TUNE_DRM_SESSION_CREATE_FAILED
	EXPECT_CALL(*g_mockDrmMetaDataEvent, setFailure(AAMP_TUNE_DRM_SESSION_CREATE_FAILED))
		.Times(1);

	// Set up getFailure() to return the error after it's been set
	EXPECT_CALL(*g_mockDrmMetaDataEvent, getFailure())
		.WillRepeatedly(Return(AAMP_TUNE_DRM_SESSION_CREATE_FAILED));

	// Mock DrmSessionManager::createDrmSession to set MW_DRM_SESSION_CREATE_FAILED
	EXPECT_CALL(*g_mockDRMSessionManager, createDrmSession(_, _, _, _, _, _))
		.Times(1)
		.WillOnce(DoAll(
			SetArgReferee<1>(MW_DRM_SESSION_CREATE_FAILED),  // Set err (2nd param) to MW_DRM_SESSION_CREATE_FAILED
			Return(nullptr)  // Return null session
		));

	// Call createDrmSession which routes through fake
	// The call g_mockDRMSessionManager->createDrmSession
	// which sets err = MW_DRM_SESSION_CREATE_FAILED
	// Then the fake maps it using MapDrmToPlayerTuneFailure
	DrmSession* result = mTestableDRMLicenseManager->createDrmSession(
		drmHelper,
		mPrivateInstanceAAMP.get(),
		g_mockDrmMetaDataEvent,
		(int)eMEDIATYPE_VIDEO
	);
	// Verify session creation failed
	EXPECT_EQ(result, nullptr);

	// Clear the global mock pointer
	g_mockDrmMetaDataEvent.reset();
}

/**
 * @brief Test case to verify successful DRM session creation
 * 
 * This test verifies that when DrmSessionManager::createDrmSession succeeds:
 * 1. A valid DrmSession pointer is returned
 * 2. setFailure() is NOT called on the eventHandle
 * 3. The error code remains unchanged
 */
TEST_F(AampDRMLicManagerTests, ValidateSuccessfulSessionCreation)
{
	// Create a MockDrmHelper
	std::shared_ptr<MockDrmHelper> drmHelper = std::make_shared<MockDrmHelper>();

	// Set up mock expectations for DrmHelper
	std::string systemId = "com.widevine.alpha";
	EXPECT_CALL(*drmHelper, ocdmSystemId())
		.WillRepeatedly(testing::ReturnRef(systemId));

	// Create the global mock event directly
	g_mockDrmMetaDataEvent = std::make_shared<MockDrmMetaDataEvent>(
		AAMP_TUNE_FAILURE_UNKNOWN,  // Initial failure state
		"",                          // Access status
		0,                           // Status value
		0,                           // Response code
		false,                       // Secclient error
		""                           // Session ID
	);

	// CRITICAL: Expect that setFailure() is NOT called (0 times) on success
	EXPECT_CALL(*g_mockDrmMetaDataEvent, setFailure(_))
		.Times(0);

	// Optional: Set up getFailure() to return the initial state
	EXPECT_CALL(*g_mockDrmMetaDataEvent, getFailure())
		.WillRepeatedly(Return(AAMP_TUNE_FAILURE_UNKNOWN));

	// Create a mock DrmSession to return on success
	// NOTE: This pointer is used only for identity/comparison and non-null checks
	// in this test. It must NEVER be dereferenced or deleted.
	DrmSession* mockSession = reinterpret_cast<DrmSession*>(0xDEADBEEF); // Non-null pointer for test

	// Mock DrmSessionManager::createDrmSession to return a valid session (err = -1 means success)
	EXPECT_CALL(*g_mockDRMSessionManager, createDrmSession(_, _, _, _, _, _))
		.Times(1)
		.WillOnce(DoAll(
			SetArgReferee<1>(-1),  // Set err (2nd param) to -1 (success)
			Return(mockSession)    // Return valid session pointer
		));

	// Call createDrmSession which should succeed
	DrmSession* result = mTestableDRMLicenseManager->createDrmSession(
		drmHelper,
		mPrivateInstanceAAMP.get(),
		g_mockDrmMetaDataEvent,
		(int)eMEDIATYPE_VIDEO
	);

	// Verify session creation succeeded
	EXPECT_NE(result, nullptr);
	EXPECT_EQ(result, mockSession);

	// Clear the global mock pointer
	g_mockDrmMetaDataEvent.reset();
}

/**
 * @brief Test DRM session ID empty error mapping
 * 
 * Validates that MW_DRM_SESSIONID_EMPTY from middleware layer
 * is properly mapped to AAMP_TUNE_DRM_SESSIONID_EMPTY at player layer
 * 
 * Test Flow:
 * 1. Mock g_mockDRMSessionManager->createDrmSession to set err = MW_DRM_SESSIONID_EMPTY
 * 2. The fake createDrmSession calls the real error mapping logic
 * 3. Verify eventHandle contains AAMP_TUNE_DRM_SESSIONID_EMPTY
 */
TEST_F(AampDRMLicManagerTests, ValidateDRMSessionIdEmpty)
{
	// Create a MockDrmHelper
	std::shared_ptr<MockDrmHelper> drmHelper = std::make_shared<MockDrmHelper>();

	// Set up mock expectations for DrmHelper
	std::string systemId = "com.widevine.alpha";
	
	EXPECT_CALL(*drmHelper, ocdmSystemId())
		.WillRepeatedly(testing::ReturnRef(systemId));


	// Create the global mock event directly
	g_mockDrmMetaDataEvent = std::make_shared<MockDrmMetaDataEvent>(
		AAMP_TUNE_FAILURE_UNKNOWN,  // Initial failure state
		"",                          // Access status
		0,                           // Status value
		0,                           // Response code
		false,                       // Secclient error
		""                           // Session ID
	);

	// CRITICAL: Expect that setFailure() IS called exactly once with AAMP_TUNE_DRM_SESSIONID_EMPTY
	EXPECT_CALL(*g_mockDrmMetaDataEvent, setFailure(AAMP_TUNE_DRM_SESSIONID_EMPTY))
		.Times(1);

	// Set up getFailure() to return the error after it's been set
	EXPECT_CALL(*g_mockDrmMetaDataEvent, getFailure())
		.WillRepeatedly(Return(AAMP_TUNE_DRM_SESSIONID_EMPTY));

	// Mock DrmSessionManager::createDrmSession to set MW_DRM_SESSIONID_EMPTY
	EXPECT_CALL(*g_mockDRMSessionManager, createDrmSession(_, _, _, _, _, _))
		.Times(1)
		.WillOnce(DoAll(
			SetArgReferee<1>(MW_DRM_SESSIONID_EMPTY),  // Set err (2nd param) to MW_DRM_SESSIONID_EMPTY
			Return(nullptr)  // Return null session
		));

	// Call createDrmSession which routes through fake
	// The fake will call g_mockDRMSessionManager->createDrmSession
	// which sets err = MW_DRM_SESSIONID_EMPTY
	// Then the fake maps it using MapDrmToPlayerTuneFailure
	DrmSession* result = mTestableDRMLicenseManager->createDrmSession(
		drmHelper,
		mPrivateInstanceAAMP.get(),
		g_mockDrmMetaDataEvent,
		(int)eMEDIATYPE_VIDEO
	);

	// Verify session creation failed
	EXPECT_EQ(result, nullptr);

	// Clear the global mock pointer
	g_mockDrmMetaDataEvent.reset();
}

/**
 * @brief Validates all networkMetrics JSON keys produced by UpdateLicenseMetrics
 *
 * Verifies that all expected keys (req, res, tot, url, con, str, dns, acn, ptr, rdt, dls, rqs)
 * are present in the JSON output with correct values.
 */
TEST_F(AampDRMLicManagerTests, UpdateLicenseMetrics_ValidateAllNetworkMetricKeys)
{
	// Assign a unique value to every metric field so mismatches are detectable
	auto respData = std::make_shared<DownloadResponse>();
	respData->downloadCompleteMetrics.connect       = 10.0;   // "con"
	respData->downloadCompleteMetrics.startTransfer = 20.0;   // "str"
	respData->downloadCompleteMetrics.resolve       = 42.5;   // "dns"
	respData->downloadCompleteMetrics.appConnect    = 30.0;   // "acn"
	respData->downloadCompleteMetrics.preTransfer   = 50.0;   // "ptr"
	respData->downloadCompleteMetrics.redirect      = 60.0;   // "rdt"
	respData->downloadCompleteMetrics.dlSize        = 70.0;   // "dls"
	respData->downloadCompleteMetrics.reqSize       = 80;     // "rqs"

	// Create the global mock event to capture the setNetworkMetricData call
	g_mockDrmMetaDataEvent = std::make_shared<MockDrmMetaDataEvent>(
		AAMP_TUNE_FAILURE_UNKNOWN,
		"",
		0,
		0,
		false,
		""
	);

	// setFailure must not be called in this path
	EXPECT_CALL(*g_mockDrmMetaDataEvent, setFailure(_)).Times(0);

	// Capture the JSON string produced by UpdateLicenseMetrics
	std::string capturedJson;
	EXPECT_CALL(*g_mockDrmMetaDataEvent, setNetworkMetricData(_))
		.Times(1)
		.WillOnce(::testing::SaveArg<0>(&capturedJson));

	// Invoke UpdateLicenseMetrics directly — this is the function under test
	mTestableDRMLicenseManager->UpdateLicenseMetrics(
		DRM_GET_LICENSE,           // requestType    — stored under key "req" (== 0)
		200,                       // statusCode     — stored under key "res"
		"http://test-license.com", // licenseRequestUrl — stored under key "url"
		1000,                      // downloadTimeMS — stored under key "tot"
		g_mockDrmMetaDataEvent,    // eventHandle    — receives setNetworkMetricData call
		respData                   // respData       — source of per-metric timing values (con/str/dns/acn/ptr/rdt/dls/rqs)
	);

	// Parse the captured JSON string
	cJSON *root = cJSON_Parse(capturedJson.c_str());
	ASSERT_NE(root, nullptr) << "JSON parsing failed for: " << capturedJson;

	// "req" – DRM license request type (DRM_GET_LICENSE == 0)
	cJSON *reqItem = cJSON_GetObjectItem(root, "req");
	ASSERT_NE(reqItem, nullptr) << "Key \"req\" missing from networkMetrics JSON: " << capturedJson;
	EXPECT_DOUBLE_EQ(reqItem->valuedouble, static_cast<double>(DRM_GET_LICENSE));

	// "res" – HTTP response code returned by the license server
	cJSON *resItem = cJSON_GetObjectItem(root, "res");
	ASSERT_NE(resItem, nullptr) << "Key \"res\" missing from networkMetrics JSON: " << capturedJson;
	EXPECT_DOUBLE_EQ(resItem->valuedouble, 200.0);

	// "tot" – total wall-clock time for DRM license acquisition
	cJSON *totItem = cJSON_GetObjectItem(root, "tot");
	ASSERT_NE(totItem, nullptr) << "Key \"tot\" missing from networkMetrics JSON: " << capturedJson;
	EXPECT_DOUBLE_EQ(totItem->valuedouble, 1000.0);

	// "url" – license server URL used for the DRM request
	cJSON *urlItem = cJSON_GetObjectItem(root, "url");
	ASSERT_NE(urlItem, nullptr) << "Key \"url\" missing from networkMetrics JSON: " << capturedJson;
	ASSERT_NE(urlItem->valuestring, nullptr);
	EXPECT_STREQ(urlItem->valuestring, "http://test-license.com");

	// "con" – TCP connection establishment time
	cJSON *conItem = cJSON_GetObjectItem(root, "con");
	ASSERT_NE(conItem, nullptr) << "Key \"con\" missing from networkMetrics JSON: " << capturedJson;
	EXPECT_DOUBLE_EQ(conItem->valuedouble, 10.0);

	// "str" – time from request start to first byte received
	cJSON *strItem = cJSON_GetObjectItem(root, "str");
	ASSERT_NE(strItem, nullptr) << "Key \"str\" missing from networkMetrics JSON: " << capturedJson;
	EXPECT_DOUBLE_EQ(strItem->valuedouble, 20.0);

	// "dns" – DNS resolution time
	cJSON *dnsItem = cJSON_GetObjectItem(root, "dns");
	ASSERT_NE(dnsItem, nullptr) << "Key \"dns\" missing from networkMetrics JSON: " << capturedJson;
	EXPECT_DOUBLE_EQ(dnsItem->valuedouble, 42.5);

	// "acn" – TLS/SSL handshake completion time
	cJSON *acnItem = cJSON_GetObjectItem(root, "acn");
	ASSERT_NE(acnItem, nullptr) << "Key \"acn\" missing from networkMetrics JSON: " << capturedJson;
	EXPECT_DOUBLE_EQ(acnItem->valuedouble, 30.0);

	// "ptr" – time from start to just before the transfer begins
	cJSON *ptrItem = cJSON_GetObjectItem(root, "ptr");
	ASSERT_NE(ptrItem, nullptr) << "Key \"ptr\" missing from networkMetrics JSON: " << capturedJson;
	EXPECT_DOUBLE_EQ(ptrItem->valuedouble, 50.0);

	// "rdt" – time spent following HTTP redirects
	cJSON *rdtItem = cJSON_GetObjectItem(root, "rdt");
	ASSERT_NE(rdtItem, nullptr) << "Key \"rdt\" missing from networkMetrics JSON: " << capturedJson;
	EXPECT_DOUBLE_EQ(rdtItem->valuedouble, 60.0);

	// "dls" – number of bytes downloaded in the response body
	cJSON *dlsItem = cJSON_GetObjectItem(root, "dls");
	ASSERT_NE(dlsItem, nullptr) << "Key \"dls\" missing from networkMetrics JSON: " << capturedJson;
	EXPECT_DOUBLE_EQ(dlsItem->valuedouble, 70.0);

	// "rqs" – number of bytes sent in the license request
	cJSON *rqsItem = cJSON_GetObjectItem(root, "rqs");
	ASSERT_NE(rqsItem, nullptr) << "Key \"rqs\" missing from networkMetrics JSON: " << capturedJson;
	EXPECT_DOUBLE_EQ(rqsItem->valuedouble, 80.0);

	cJSON_Delete(root);

	// Clear the global mock pointer
	g_mockDrmMetaDataEvent.reset();
}
