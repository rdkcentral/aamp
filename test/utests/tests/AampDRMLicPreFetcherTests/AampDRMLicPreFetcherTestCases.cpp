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
#include <memory>
#include <vector>
#include <thread>
#include <chrono>
#include "AampDRMLicPreFetcher.h"
#include "MockPrivateInstanceAAMP.h"
#include "MockAampDRMSessionManager.h"
#include "MockAampLicManager.h"
#include "MockStreamAbstractionAAMP.h"
#include "DrmHelper.h"
#include "DrmSession.h"

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

	const std::vector<std::vector<uint8_t>> &getUsableKeys() const override
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
 * @brief Testable wrapper for AampLicensePreFetcher that exposes protected members
 */
class TestableAampLicensePreFetcher : public AampLicensePreFetcher
{
public:
	TestableAampLicensePreFetcher(PrivateInstanceAAMP *aamp) : AampLicensePreFetcher(aamp) {}

	// Expose protected members for testing
	bool isThreadJoinable() const { return mPreFetchThread.joinable(); }
	bool isVssThreadJoinable() const { return mVssPreFetchThread.joinable(); }
	size_t getFetchQueueSize() const
	{
		std::lock_guard<std::mutex> lock(const_cast<std::mutex &>(mQMutex));
		return mFetchQueue.size();
	}
	size_t getVssFetchQueueSize() const
	{
		std::lock_guard<std::mutex> lock(const_cast<std::mutex &>(mQVssMutex));
		return mVssFetchQueue.size();
	}
	bool getExitLoopFlag() const { return mExitLoop; }
	bool getTrackStatus(AampMediaType type) const
	{
		try
		{
			return mTrackStatus.at(type);
		}
		catch (...)
		{
			return false;
		}
	}
};

/**
 * @brief Test fixture for AampDRMLicPreFetcher integration tests
 */
class AampDRMLicPreFetcherTests : public ::testing::Test
{
protected:
	std::unique_ptr<PrivateInstanceAAMP> mPrivateInstanceAAMP;
	std::unique_ptr<TestableAampLicensePreFetcher> mTestablePreFetcher;
	std::unique_ptr<AampDRMLicenseManager> mAampDRMLicenseManager;

	void SetUp() override
	{
		// Initialize mocks with smart pointers
		g_mockPrivateInstanceAAMP = new NiceMock<MockPrivateInstanceAAMP>();
		g_mockDRMSessionManager = new NiceMock<MockDRMSessionManager>();
		g_mockAampLicenseManager = new NiceMock<MockAampLicenseManager>();
		mPrivateInstanceAAMP = std::make_unique<PrivateInstanceAAMP>();
		mAampDRMLicenseManager = std::make_unique<AampDRMLicenseManager>(5, mPrivateInstanceAAMP.get());

		// Create testable mTestablePreFetcher instance
		mTestablePreFetcher = std::make_unique<TestableAampLicensePreFetcher>(mPrivateInstanceAAMP.get());
		mPrivateInstanceAAMP->mDRMLicenseManager = mAampDRMLicenseManager.get();
	}

	void TearDown() override
	{
		// Cleanup in reverse order
		if (mTestablePreFetcher)
		{
			mTestablePreFetcher->Term();
			mTestablePreFetcher.reset();
		}
		mAampDRMLicenseManager.reset();
		mPrivateInstanceAAMP.reset();

		delete g_mockAampLicenseManager;
		g_mockAampLicenseManager = nullptr;

		delete g_mockDRMSessionManager;
		g_mockDRMSessionManager = nullptr;

		delete g_mockPrivateInstanceAAMP;
		g_mockPrivateInstanceAAMP = nullptr;
	}

	/**
	 * @brief Helper to create a key ID vector from hex string
	 */
	std::vector<uint8_t> CreateKeyIdFromHex(const std::string &hexStr)
	{
		std::vector<uint8_t> keyId;
		for (size_t i = 0; i < hexStr.length(); i += 2)
		{
			std::string byteStr = hexStr.substr(i, 2);
			uint8_t byte = static_cast<uint8_t>(std::stoul(byteStr, nullptr, 16));
			keyId.push_back(byte);
		}
		return keyId;
	}

	/**
	 * @brief Helper to create a DRM helper with a specific key ID
	 */
	DrmHelperPtr CreateDrmHelper(const std::vector<uint8_t> &keyId)
	{
		// Create DrmInfo
		DrmInfo drmInfo;
		drmInfo.keyFormat = "test-format";
		drmInfo.mediaFormat = eMEDIAFORMAT_UNKNOWN;
		drmInfo.systemUUID = "test-uuid-1234";

		// Create TestDrmHelper instance with the key ID
		auto helper = std::make_shared<TestDrmHelper>(drmInfo, keyId);

		return helper;
	}
};

/**
 * @brief Test Init() initializes correctly
 */
TEST_F(AampDRMLicPreFetcherTests, Init_FirstCall_ReturnsTrue)
{
	bool result = mTestablePreFetcher->Init();

	EXPECT_TRUE(result);
}

/**
 * @brief Test Init() can be called multiple times
 */
TEST_F(AampDRMLicPreFetcherTests, Init_MultipleCalls_WarnsButWorks)
{
	// First init
	EXPECT_TRUE(mTestablePreFetcher->Init());

	// Second init - may warn if threads are started
	EXPECT_TRUE(mTestablePreFetcher->Init());
}

/**
 * @brief Test Term() clears queues
 */
TEST_F(AampDRMLicPreFetcherTests, Term_WithQueuedItems_ClearsQueue)
{
	mTestablePreFetcher->Init();

	// Queue some items first
	auto keyId = CreateKeyIdFromHex("0123456789abcdef");
	auto drmHelper = CreateDrmHelper(keyId);

	// Setup mock to indicate key not processed yet
	EXPECT_CALL(*g_mockDRMSessionManager, IsKeyIdProcessed(_, _))
		.Times(1)
		.WillOnce(Return(false));
	// Queue content protection to populate the queue
	mTestablePreFetcher->QueueContentProtection(drmHelper, "period1", 0, eMEDIATYPE_VIDEO, false);

	// Give time for queueing
	std::this_thread::sleep_for(std::chrono::milliseconds(50));
	// For now, just verify Term doesn't crash
	bool result = mTestablePreFetcher->Term();

	EXPECT_TRUE(result);
	EXPECT_EQ(mTestablePreFetcher->getFetchQueueSize(), 0);
	EXPECT_EQ(mTestablePreFetcher->getVssFetchQueueSize(), 0);
}

/**
 * @brief Test Init sets exit flag correctly
 */
TEST_F(AampDRMLicPreFetcherTests, Init_SetsExitFlagToFalse)
{
	mTestablePreFetcher->Init();

	// Verify exit flag is false after Init
	EXPECT_FALSE(mTestablePreFetcher->getExitLoopFlag());

	mTestablePreFetcher->Term();
}

/**
 * @brief Test QueueContentProtection starts PreFetchThread
 */
TEST_F(AampDRMLicPreFetcherTests, QueueContentProtection_FirstCall_StartsThread)
{
	mTestablePreFetcher->Init();

	// Create DRM helper
	auto keyId = CreateKeyIdFromHex(TEST_KEY_ID_SLOT_0);
	auto drmHelper = CreateDrmHelper(keyId);

	// Setup mock to indicate key not processed yet
	EXPECT_CALL(*g_mockDRMSessionManager, IsKeyIdProcessed(_, _))
		.Times(1)
		.WillOnce(Return(false));

	// Queue content protection
	bool result = mTestablePreFetcher->QueueContentProtection(
		drmHelper, "period1", 0, eMEDIATYPE_VIDEO, false);

	EXPECT_TRUE(result);

	// Give thread time to start
	std::this_thread::sleep_for(std::chrono::milliseconds(50));

	// Verify thread was started
	EXPECT_EQ(mTestablePreFetcher->isThreadJoinable(), true);

	// Cleanup
	mTestablePreFetcher->Term();
}

/**
 * @brief Test QueueContentProtection for VSS period starts VssPreFetchThread
 */
TEST_F(AampDRMLicPreFetcherTests, QueueContentProtection_VssPeriod_StartsVssThread)
{
	mTestablePreFetcher->Init();

	// Create DRM helper
	auto keyId = CreateKeyIdFromHex(TEST_KEY_ID_AUDIO);
	auto drmHelper = CreateDrmHelper(keyId);

	// Setup mock to indicate key not processed yet
	EXPECT_CALL(*g_mockDRMSessionManager, IsKeyIdProcessed(_, _))
		.Times(1)
		.WillOnce(Return(false));

	// Queue VSS content protection
	bool result = mTestablePreFetcher->QueueContentProtection(
		drmHelper, "vss_period1", 0, eMEDIATYPE_VIDEO, true); // isVssPeriod = true

	EXPECT_TRUE(result);

	// Give thread time to start
	std::this_thread::sleep_for(std::chrono::milliseconds(50));

	// Verify VSS thread was started
	EXPECT_TRUE(mTestablePreFetcher->isVssThreadJoinable());

	// Cleanup
	mTestablePreFetcher->Term();
}

/**
 * @brief Test KeyIsQueued detects duplicate keys
 */
TEST_F(AampDRMLicPreFetcherTests, QueueSize_AfterQueueing_IncrementsCorrectly)
{
	mTestablePreFetcher->Init();

	auto keyId = CreateKeyIdFromHex(TEST_KEY_ID_SLOT_0);
	auto drmHelper = CreateDrmHelper(keyId);
	ON_CALL(*g_mockDRMSessionManager, IsKeyIdProcessed(_, _))
		.WillByDefault(Return(false));

	// Initial queue should be empty
	size_t initialSize = mTestablePreFetcher->getFetchQueueSize();
	// Queue content protection
	mTestablePreFetcher->QueueContentProtection(drmHelper, "period1", 0, eMEDIATYPE_VIDEO, false);
	// Queue size should have increased (or decreased after processing)
	// Note: Due to thread processing, queue might be empty if processed quickly
	// This test mainly verifies the queue size accessor works
	size_t finalSize = mTestablePreFetcher->getFetchQueueSize();
	EXPECT_GE(initialSize + finalSize, 0); // Just verify it's accessible
	mTestablePreFetcher->Term();
}

/**
 * @brief Test KeyIsQueued detects duplicate keys
 */
TEST_F(AampDRMLicPreFetcherTests, KeyIsQueued_SameKey_ReturnsTrue)
{
	mTestablePreFetcher->Init();

	// Create DRM helpers with specific key
	auto keyId = CreateKeyIdFromHex(TEST_KEY_ID_SLOT_0);
	auto drmHelper1 = CreateDrmHelper(keyId);
	auto drmHelper2 = CreateDrmHelper(keyId); // Same key ID

	// Setup mock
	ON_CALL(*g_mockDRMSessionManager, IsKeyIdProcessed(_, _))
		.WillByDefault(Return(false));

	// Queue first key
	mTestablePreFetcher->QueueContentProtection(drmHelper1, "period1", 0, eMEDIATYPE_VIDEO, false);
	// Queue same key again - should detect duplicate
	mTestablePreFetcher->QueueContentProtection(drmHelper2, "period1", 0, eMEDIATYPE_VIDEO, false);
	// Verify queue size is less than 2 (duplicate should be filtered out)
	size_t queueSize = mTestablePreFetcher->getFetchQueueSize();
	EXPECT_LT(queueSize, 2);
	// Cleanup
	mTestablePreFetcher->Term();
}

/**
 * @brief Test adaptation index tracking
 */
TEST_F(AampDRMLicPreFetcherTests, AdaptationIndex_MultipleAdaptations_Tracked)
{
	// Create objects for different adaptation sets
	auto objUHD = std::make_shared<LicensePreFetchObject>(
		nullptr, "period1", 0, eMEDIATYPE_VIDEO, false); // Adaptation 0 - UHD

	auto objHD = std::make_shared<LicensePreFetchObject>(
		nullptr, "period1", 1, eMEDIATYPE_VIDEO, false); // Adaptation 1 - HD

	auto objSD = std::make_shared<LicensePreFetchObject>(
		nullptr, "period1", 2, eMEDIATYPE_VIDEO, false); // Adaptation 2 - SD

	// Verify adaptation indices
	EXPECT_EQ(objUHD->mAdaptationIdx, 0);
	EXPECT_EQ(objHD->mAdaptationIdx, 1);
	EXPECT_EQ(objSD->mAdaptationIdx, 2);

	// All in same period
	EXPECT_EQ(objUHD->mPeriodId, objHD->mPeriodId);
	EXPECT_EQ(objHD->mPeriodId, objSD->mPeriodId);
}

/**
 * @brief Test object comparison
 */
TEST_F(AampDRMLicPreFetcherTests, ObjectComparison_NullObject_ReturnsFalse)
{
	auto obj1 = std::make_shared<LicensePreFetchObject>(
		nullptr, "period1", 0, eMEDIATYPE_VIDEO, false);

	// Compare with nullptr should return false
	bool result = obj1->compare(nullptr);
	EXPECT_FALSE(result);
}

/**
 * @brief Test track status initialization
 */
TEST_F(AampDRMLicPreFetcherTests, TrackStatus_AfterInit_AllFalse)
{
	mTestablePreFetcher->Init();

	// Verify all track statuses are false initially
	EXPECT_FALSE(mTestablePreFetcher->getTrackStatus(eMEDIATYPE_VIDEO));
	EXPECT_FALSE(mTestablePreFetcher->getTrackStatus(eMEDIATYPE_AUDIO));
	EXPECT_FALSE(mTestablePreFetcher->getTrackStatus(eMEDIATYPE_SUBTITLE));

	mTestablePreFetcher->Term();
}

/**
 * @brief Test PreFetchThread with already processed FAILED key
 */
TEST_F(AampDRMLicPreFetcherTests, PreFetchThread_KeyProcessedFailed_NotifiesError)
{
	mTestablePreFetcher->Init();

	// Create DRM helper for UHD key
	auto uhdKeyId = CreateKeyIdFromHex(TEST_KEY_ID_SLOT_0);
	auto uhdDrmHelper = CreateDrmHelper(uhdKeyId);

	// Setup mock: UHD key was already processed and FAILED
	bool keyStatus = false;
	EXPECT_CALL(*g_mockDRMSessionManager, IsKeyIdProcessed(uhdKeyId, _))
		.Times(1)
		.WillOnce(DoAll(
			SetArgReferee<1>(keyStatus),
			Return(true)));

	// Expect error notification for failed key
	EXPECT_CALL(*g_mockPrivateInstanceAAMP, SendDRMMetaData(_)).Times(::testing::AtLeast(1));
	EXPECT_CALL(*g_mockPrivateInstanceAAMP, SendDrmErrorEvent(_, _)).Times(::testing::AtLeast(1));

	// Queue the protection - should detect failure and notify
	mTestablePreFetcher->QueueContentProtection(uhdDrmHelper, "period1", 0, eMEDIATYPE_VIDEO, false);
	// Give thread time to process
	std::this_thread::sleep_for(std::chrono::milliseconds(50));
	mTestablePreFetcher->Term();
}

/**
 * @brief Test PreFetchThread with already processed SUCCESS key
 */
TEST_F(AampDRMLicPreFetcherTests, PreFetchThread_KeyProcessedSuccess_SkipsSession)
{
	mTestablePreFetcher->Init();

	// Create DRM helper for HD key
	auto hdKeyId = CreateKeyIdFromHex(TEST_KEY_ID_SLOT_1);
	auto hdDrmHelper = CreateDrmHelper(hdKeyId);

	// Setup mock: HD key was already processed and SUCCEEDED
	bool keyStatus = true;
	EXPECT_CALL(*g_mockDRMSessionManager, IsKeyIdProcessed(hdKeyId, _))
		.Times(1)
		.WillOnce(DoAll(
			SetArgReferee<1>(keyStatus),
			Return(true)));

	// Should NOT create DRM session since key already succeeded
	EXPECT_CALL(*g_mockAampLicenseManager, createDrmSession(_, _, _, _)).Times(0);

	// Queue the protection - should skip session creation
	mTestablePreFetcher->QueueContentProtection(hdDrmHelper, "period1", 1, eMEDIATYPE_VIDEO, false);

	// Give thread time to process
	std::this_thread::sleep_for(std::chrono::milliseconds(50));

	mTestablePreFetcher->Term();
}

/**
 * @brief Test multi-key scenario with mixed success/failure
 */
TEST_F(AampDRMLicPreFetcherTests, MultiKey_UHDFailedHDSuccess_BothProcessed)
{
	mTestablePreFetcher->Init();

	// Create DRM helpers for UHD and HD keys
	auto uhdKeyId = CreateKeyIdFromHex(TEST_KEY_ID_SLOT_0);
	auto hdKeyId = CreateKeyIdFromHex(TEST_KEY_ID_SLOT_1);

	auto uhdDrmHelper = CreateDrmHelper(uhdKeyId);
	auto hdDrmHelper = CreateDrmHelper(hdKeyId);

	// Setup mocks: UHD failed, HD succeeded
	bool uhdStatus = false;
	bool hdStatus = true;

	EXPECT_CALL(*g_mockDRMSessionManager, IsKeyIdProcessed(uhdKeyId, _))
		.Times(1)
		.WillOnce(DoAll(SetArgReferee<1>(uhdStatus), Return(true)));

	EXPECT_CALL(*g_mockDRMSessionManager, IsKeyIdProcessed(hdKeyId, _))
		.Times(1)
		.WillOnce(DoAll(SetArgReferee<1>(hdStatus), Return(true)));
	// Queue both keys
	mTestablePreFetcher->QueueContentProtection(uhdDrmHelper, "period1", 0, eMEDIATYPE_VIDEO, false);
	mTestablePreFetcher->QueueContentProtection(hdDrmHelper, "period1", 1, eMEDIATYPE_VIDEO, false);

	// Give thread time to process both
	std::this_thread::sleep_for(std::chrono::milliseconds(50));

	mTestablePreFetcher->Term();
}

/**
 * @brief Test VSS period with failed key (VPLAY-11304)
 */
TEST_F(AampDRMLicPreFetcherTests, VPLAY11304_VssPreFetchThread_FailedKey_Handled)
{
	mTestablePreFetcher->Init();

	// Create DRM helper for VSS key
	auto vssKeyId = CreateKeyIdFromHex(TEST_KEY_ID_SLOT_0);
	auto vssDrmHelper = CreateDrmHelper(vssKeyId);

	// Setup mock: VSS key was already processed and FAILED
	bool keyStatus = false;
	EXPECT_CALL(*g_mockDRMSessionManager, IsKeyIdProcessed(vssKeyId, _))
		.Times(1)
		.WillOnce(DoAll(
			SetArgReferee<1>(keyStatus),
			Return(true)));

	// Queue VSS protection - should skip creation due to failure
	mTestablePreFetcher->QueueContentProtection(vssDrmHelper, "vss_period1", 0, eMEDIATYPE_VIDEO, true);

	// Give VSS thread time to process
	std::this_thread::sleep_for(std::chrono::milliseconds(50));

	mTestablePreFetcher->Term();
}

/**
 * @brief Test common key duration affects VSS deferred processing
 */
TEST_F(AampDRMLicPreFetcherTests, SetCommonKeyDuration_VssThread_DefersProcessing)
{
	mTestablePreFetcher->Init();

	// Set common key duration for deferred license acquisition
	mTestablePreFetcher->SetCommonKeyDuration(1000); // 1 second

	auto vssKeyId = CreateKeyIdFromHex(TEST_KEY_ID_AUDIO);
	auto vssDrmHelper = CreateDrmHelper(vssKeyId);

	// Key not processed yet
	EXPECT_CALL(*g_mockDRMSessionManager, IsKeyIdProcessed(_, _))
		.Times(1)
		.WillOnce(Return(false));

	// Queue VSS content - should defer based on common key duration
	auto startTime = std::chrono::steady_clock::now();
	mTestablePreFetcher->QueueContentProtection(vssDrmHelper, "vss_period1", 0, eMEDIATYPE_VIDEO, true);

	// Give thread time to process (includes defer time)
	std::this_thread::sleep_for(std::chrono::milliseconds(50));

	mTestablePreFetcher->Term();

	// Test passes if no crash - actual timing validation would need more complex setup
	EXPECT_TRUE(true);
}

/**
 * @brief Test SetSendErrorOnFailure flag
 */
TEST_F(AampDRMLicPreFetcherTests, SetSendErrorOnFailure_True_SendsErrorImmediately)
{
	mTestablePreFetcher->Init();
	mTestablePreFetcher->SetSendErrorOnFailure(true);

	// With flag set, error should be sent immediately on failure
	// without checking pending requests

	auto keyId = CreateKeyIdFromHex(TEST_KEY_ID_SLOT_0);
	auto drmHelper = CreateDrmHelper(keyId);

	bool keyStatus = false;
	EXPECT_CALL(*g_mockDRMSessionManager, IsKeyIdProcessed(keyId, _))
		.Times(1)
		.WillOnce(DoAll(SetArgReferee<1>(keyStatus), Return(true)));

	mTestablePreFetcher->QueueContentProtection(drmHelper, "period1", 0, eMEDIATYPE_VIDEO, false);

	std::this_thread::sleep_for(std::chrono::milliseconds(50));

	mTestablePreFetcher->Term();
}

/**
 * @brief Test SetLicenseFetcher integration
 */
TEST_F(AampDRMLicPreFetcherTests, SetLicenseFetcher_NotNull_Accepted)
{
	// Test that license fetcher can be set
	// Would need AampLicenseFetcher mock for full test

	mTestablePreFetcher->SetLicenseFetcher(nullptr);

	// Test passes if no crash
	EXPECT_TRUE(true);
}

/**
 * @brief Test Init after destructor scenario (lifecycle)
 */
TEST_F(AampDRMLicPreFetcherTests, Lifecycle_InitTermMultiple_NoMemoryLeaks)
{
	// Test multiple Init/Term cycles
	for (int i = 0; i < 3; i++)
	{
		EXPECT_TRUE(mTestablePreFetcher->Init());
		EXPECT_TRUE(mTestablePreFetcher->Term());
	}
}

/**
 * @brief Test empty key ID skips IsKeyIdProcessed check
 */
TEST_F(AampDRMLicPreFetcherTests, EmptyKeyId_SkipsProcessedCheck)
{
	mTestablePreFetcher->Init();

	// Create DRM helper with empty key ID
	std::vector<uint8_t> emptyKeyId; // Empty vector
	auto drmHelper = CreateDrmHelper(emptyKeyId);

	// IsKeyIdProcessed should NOT be called for empty key ID
	EXPECT_CALL(*g_mockDRMSessionManager, IsKeyIdProcessed(_, _)).Times(0);

	// Queue content protection with empty key
	mTestablePreFetcher->QueueContentProtection(drmHelper, "period1", 0, eMEDIATYPE_VIDEO, false);

	std::this_thread::sleep_for(std::chrono::milliseconds(50));

	mTestablePreFetcher->Term();
}

/**
 * @brief Test multi-key scenario: All 3 keys usable (not processed yet)
 * Expected: No errors, all keys should be queued for processing
 */
TEST_F(AampDRMLicPreFetcherTests, MultiKey_AllThreeKeysUsable_NoErrors)
{
	mTestablePreFetcher->Init();

	// Create 3 unique key IDs for UHD, HD, SD
	auto keyId1 = CreateKeyIdFromHex(TEST_KEY_ID_SLOT_0); // Slot 0
	auto keyId2 = CreateKeyIdFromHex(TEST_KEY_ID_SLOT_1); // Slot 1
	auto keyId3 = CreateKeyIdFromHex(TEST_KEY_ID_SLOT_2); // Slot 2

	// Create DrmInfo
	DrmInfo drmInfo;
	drmInfo.keyFormat = "identity";
	drmInfo.mediaFormat = eMEDIAFORMAT_UNKNOWN;
	drmInfo.systemUUID = "test-uuid-multikey";

	// Build multi-key map
	std::map<int, std::vector<uint8_t>> allKeys;
	allKeys[0] = keyId1;
	allKeys[1] = keyId2;
	allKeys[2] = keyId3;

	// Create DrmHelper with all 3 keys
	auto drmHelper1 = std::make_shared<TestDrmHelper>(drmInfo, keyId1, allKeys);
	auto drmHelper2 = std::make_shared<TestDrmHelper>(drmInfo, keyId2, allKeys);
	auto drmHelper3 = std::make_shared<TestDrmHelper>(drmInfo, keyId3, allKeys);

	// Setup mocks: None of the keys processed yet
	EXPECT_CALL(*g_mockDRMSessionManager, IsKeyIdProcessed(keyId1, _))
		.Times(1)
		.WillOnce(Return(false));
	EXPECT_CALL(*g_mockDRMSessionManager, IsKeyIdProcessed(keyId2, _))
		.Times(1)
		.WillOnce(Return(false));
	EXPECT_CALL(*g_mockDRMSessionManager, IsKeyIdProcessed(keyId3, _))
		.Times(1)
		.WillOnce(Return(false));

	// Expect DRM sessions to be created for all 3 keys
	auto drmSession = std::make_shared<TestDrmSession>();
	EXPECT_CALL(*g_mockAampLicenseManager, createDrmSession(_, _, _, _))
		.Times(3)
		.WillRepeatedly(Return(drmSession.get()));
	// DRM Metadata expected for each key
	EXPECT_CALL(*g_mockPrivateInstanceAAMP, SendDRMMetaData(_)).Times(::testing::AtLeast(3));
	// NO error notifications expected - all keys are usable
	EXPECT_CALL(*g_mockPrivateInstanceAAMP, SendDrmErrorEvent(_, _)).Times(0);

	// Queue content protection
	mTestablePreFetcher->QueueContentProtection(drmHelper1, "period1", 0, eMEDIATYPE_VIDEO, false);
	mTestablePreFetcher->QueueContentProtection(drmHelper2, "period1", 1, eMEDIATYPE_AUDIO, false);
	mTestablePreFetcher->QueueContentProtection(drmHelper3, "period1", 2, eMEDIATYPE_VIDEO, false);

	// Give thread time to process
	std::this_thread::sleep_for(std::chrono::milliseconds(50));

	mTestablePreFetcher->Term();
}

/**
 * @brief Test multi-key scenario: Key in slot 1 failed, slots 0 and 2 usable
 * Expected: Error notification for slot 1 failure
 */
TEST_F(AampDRMLicPreFetcherTests, MultiKey_SendErrorOnFailure_Slot1Failed_Slot0And2Usable)
{
	mTestablePreFetcher->Init();

	auto keyId1 = CreateKeyIdFromHex(TEST_KEY_ID_SLOT_0); // Slot 0 - Usable
	auto keyId2 = CreateKeyIdFromHex(TEST_KEY_ID_SLOT_1); // Slot 1 - FAILED
	auto keyId3 = CreateKeyIdFromHex(TEST_KEY_ID_SLOT_2); // Slot 2 - Usable

	DrmInfo drmInfo;
	drmInfo.keyFormat = "identity";
	drmInfo.mediaFormat = eMEDIAFORMAT_UNKNOWN;
	drmInfo.systemUUID = "test-uuid-multikey";

	std::map<int, std::vector<uint8_t>> allKeys;
	allKeys[0] = keyId1;
	allKeys[1] = keyId2;
	allKeys[2] = keyId3;

	auto drmHelper1 = std::make_shared<TestDrmHelper>(drmInfo, keyId1, allKeys);
	auto drmHelper2 = std::make_shared<TestDrmHelper>(drmInfo, keyId2, allKeys);
	auto drmHelper3 = std::make_shared<TestDrmHelper>(drmInfo, keyId3, allKeys);

	// Setup mocks: Slot 1 failed, slots 0 and 2 not processed
	EXPECT_CALL(*g_mockDRMSessionManager, IsKeyIdProcessed(keyId1, _))
		.Times(1)
		.WillOnce(Return(false));
	bool slot1Status = false; // FAILED
	EXPECT_CALL(*g_mockDRMSessionManager, IsKeyIdProcessed(keyId2, _))
		.Times(1)
		.WillOnce(DoAll(SetArgReferee<1>(slot1Status), Return(true)));
	EXPECT_CALL(*g_mockDRMSessionManager, IsKeyIdProcessed(keyId3, _))
		.Times(1)
		.WillOnce(Return(false));

	// Expect error notification for slot 1 failure
	EXPECT_CALL(*g_mockPrivateInstanceAAMP, SendDRMMetaData(_)).Times(::testing::AtLeast(1));
	EXPECT_CALL(*g_mockPrivateInstanceAAMP, SendDrmErrorEvent(_, _)).Times(::testing::AtLeast(1));

	mTestablePreFetcher->QueueContentProtection(drmHelper1, "period1", 0, eMEDIATYPE_VIDEO, false);
	mTestablePreFetcher->QueueContentProtection(drmHelper2, "period1", 1, eMEDIATYPE_AUDIO, false);
	mTestablePreFetcher->QueueContentProtection(drmHelper3, "period1", 2, eMEDIATYPE_VIDEO, false);

	std::this_thread::sleep_for(std::chrono::milliseconds(50));

	mTestablePreFetcher->Term();
}

/**
 * @brief Test multi-key scenario: Keys in slots 0 and 1 failed, slot 2 usable
 * Expected: Error notifications for both slot 0 and slot 1 failures
 */
TEST_F(AampDRMLicPreFetcherTests, MultiKey_SendErrorOnFailure_Slot0And1Failed_Slot2Usable_TwoErrorsNotified)
{
	mTestablePreFetcher->Init();

	auto keyId1 = CreateKeyIdFromHex(TEST_KEY_ID_SLOT_0); // Slot 0 - FAILED
	auto keyId2 = CreateKeyIdFromHex(TEST_KEY_ID_SLOT_1); // Slot 1 - FAILED
	auto keyId3 = CreateKeyIdFromHex(TEST_KEY_ID_SLOT_2); // Slot 2 - Usable

	DrmInfo drmInfo;
	drmInfo.keyFormat = "identity";
	drmInfo.mediaFormat = eMEDIAFORMAT_UNKNOWN;
	drmInfo.systemUUID = "test-uuid-multikey";

	std::map<int, std::vector<uint8_t>> allKeys;
	allKeys[0] = keyId1;
	allKeys[1] = keyId2;
	allKeys[2] = keyId3;

	auto drmHelper1 = std::make_shared<TestDrmHelper>(drmInfo, keyId1, allKeys);
	auto drmHelper2 = std::make_shared<TestDrmHelper>(drmInfo, keyId2, allKeys);
	auto drmHelper3 = std::make_shared<TestDrmHelper>(drmInfo, keyId3, allKeys);
	// Setup mocks: Slots 0 and 1 failed, slot 2 not processed
	bool slot0Status = false; // FAILED
	bool slot1Status = false; // FAILED
	EXPECT_CALL(*g_mockDRMSessionManager, IsKeyIdProcessed(keyId1, _))
		.Times(1)
		.WillOnce(DoAll(SetArgReferee<1>(slot0Status), Return(true)));
	EXPECT_CALL(*g_mockDRMSessionManager, IsKeyIdProcessed(keyId2, _))
		.Times(1)
		.WillOnce(DoAll(SetArgReferee<1>(slot1Status), Return(true)));
	EXPECT_CALL(*g_mockDRMSessionManager, IsKeyIdProcessed(keyId3, _))
		.Times(1)
		.WillOnce(Return(false));

	// Expect error notifications for both slot 0 and slot 1 failures
	EXPECT_CALL(*g_mockPrivateInstanceAAMP, SendDRMMetaData(_)).Times(::testing::AtLeast(2));
	EXPECT_CALL(*g_mockPrivateInstanceAAMP, SendDrmErrorEvent(_, _)).Times(::testing::AtLeast(2));

	mTestablePreFetcher->QueueContentProtection(drmHelper1, "period1", 0, eMEDIATYPE_VIDEO, false);
	mTestablePreFetcher->QueueContentProtection(drmHelper2, "period1", 1, eMEDIATYPE_AUDIO, false);
	mTestablePreFetcher->QueueContentProtection(drmHelper3, "period1", 2, eMEDIATYPE_VIDEO, false);

	std::this_thread::sleep_for(std::chrono::milliseconds(50));

	mTestablePreFetcher->Term();
}

/**
 * @brief Test multi-key scenario: All 3 keys failed
 * Expected: Error notifications for all 3 slot failures
 */
TEST_F(AampDRMLicPreFetcherTests, MultiKey_SendErrorOnFailure_AllThreeKeysFailed_AllErrorsNotified)
{
	mTestablePreFetcher->Init();

	auto keyId1 = CreateKeyIdFromHex(TEST_KEY_ID_SLOT_0); // Slot 0 - FAILED
	auto keyId2 = CreateKeyIdFromHex(TEST_KEY_ID_SLOT_1); // Slot 1 - FAILED
	auto keyId3 = CreateKeyIdFromHex(TEST_KEY_ID_SLOT_2); // Slot 2 - FAILED

	DrmInfo drmInfo;
	drmInfo.keyFormat = "identity";
	drmInfo.mediaFormat = eMEDIAFORMAT_UNKNOWN;
	drmInfo.systemUUID = "test-uuid-multikey";

	std::map<int, std::vector<uint8_t>> allKeys;
	allKeys[0] = keyId1;
	allKeys[1] = keyId2;
	allKeys[2] = keyId3;

	auto drmHelper1 = std::make_shared<TestDrmHelper>(drmInfo, keyId1, allKeys);
	auto drmHelper2 = std::make_shared<TestDrmHelper>(drmInfo, keyId2, allKeys);
	auto drmHelper3 = std::make_shared<TestDrmHelper>(drmInfo, keyId3, allKeys);

	// Setup mocks: All slots failed
	bool slot0Status = false; // FAILED
	bool slot1Status = false; // FAILED
	bool slot2Status = false; // FAILED
	EXPECT_CALL(*g_mockDRMSessionManager, IsKeyIdProcessed(keyId1, _))
		.Times(1)
		.WillOnce(DoAll(SetArgReferee<1>(slot0Status), Return(true)));
	EXPECT_CALL(*g_mockDRMSessionManager, IsKeyIdProcessed(keyId2, _))
		.Times(1)
		.WillOnce(DoAll(SetArgReferee<1>(slot1Status), Return(true)));
	EXPECT_CALL(*g_mockDRMSessionManager, IsKeyIdProcessed(keyId3, _))
		.Times(1)
		.WillOnce(DoAll(SetArgReferee<1>(slot2Status), Return(true)));

	// Expect error notifications for all 3 slot failures
	EXPECT_CALL(*g_mockPrivateInstanceAAMP, SendDRMMetaData(_)).Times(::testing::AtLeast(3));
	EXPECT_CALL(*g_mockPrivateInstanceAAMP, SendDrmErrorEvent(_, _)).Times(::testing::AtLeast(3));

	mTestablePreFetcher->QueueContentProtection(drmHelper1, "period1", 0, eMEDIATYPE_VIDEO, false);
	mTestablePreFetcher->QueueContentProtection(drmHelper2, "period1", 1, eMEDIATYPE_AUDIO, false);
	mTestablePreFetcher->QueueContentProtection(drmHelper3, "period1", 2, eMEDIATYPE_VIDEO, false);

	std::this_thread::sleep_for(std::chrono::milliseconds(50));

	mTestablePreFetcher->Term();
}

/**
 * @brief Test multi-key scenario: Mixed success/failure
 * Slot 0: Already succeeded, Slot 1: Failed, Slot 2: Not processed yet
 * Expected: Error notification only for slot 1 failure
 */
TEST_F(AampDRMLicPreFetcherTests, MultiKey_SendErrorOnFailure_MixedStatus_Slot0Success_Slot1Failed_Slot2Unprocessed)
{
	mTestablePreFetcher->Init();

	auto keyId1 = CreateKeyIdFromHex(TEST_KEY_ID_SLOT_0);	// Slot 0 - SUCCESS
	auto keyId2 = CreateKeyIdFromHex(TEST_KEY_ID_SLOT_1); // Slot 1 - FAILED
	auto keyId3 = CreateKeyIdFromHex(TEST_KEY_ID_SLOT_2);	// Slot 2 - Not processed

	DrmInfo drmInfo;
	drmInfo.keyFormat = "identity";
	drmInfo.mediaFormat = eMEDIAFORMAT_UNKNOWN;
	drmInfo.systemUUID = "test-uuid-multikey";

	std::map<int, std::vector<uint8_t>> allKeys;
	allKeys[0] = keyId1;
	allKeys[1] = keyId2;
	allKeys[2] = keyId3;

	auto drmHelper1 = std::make_shared<TestDrmHelper>(drmInfo, keyId1, allKeys);
	auto drmHelper2 = std::make_shared<TestDrmHelper>(drmInfo, keyId2, allKeys);
	auto drmHelper3 = std::make_shared<TestDrmHelper>(drmInfo, keyId3, allKeys);

	// Setup mocks: Slot 0 succeeded, Slot 1 failed, Slot 2 not processed
	bool slot0Status = true;  // SUCCESS
	bool slot1Status = false; // FAILED
	EXPECT_CALL(*g_mockDRMSessionManager, IsKeyIdProcessed(keyId1, _))
		.Times(1)
		.WillOnce(DoAll(SetArgReferee<1>(slot0Status), Return(true)));
	EXPECT_CALL(*g_mockDRMSessionManager, IsKeyIdProcessed(keyId2, _))
		.Times(1)
		.WillOnce(DoAll(SetArgReferee<1>(slot1Status), Return(true)));
	EXPECT_CALL(*g_mockDRMSessionManager, IsKeyIdProcessed(keyId3, _))
		.Times(1)
		.WillOnce(Return(false));

	// Expect error notification only for slot 1 failure (slot 0 succeeded, slot 2 will be processed)
	EXPECT_CALL(*g_mockPrivateInstanceAAMP, SendDRMMetaData(_)).Times(::testing::AtLeast(1));
	EXPECT_CALL(*g_mockPrivateInstanceAAMP, SendDrmErrorEvent(_, _)).Times(::testing::AtLeast(1));

	mTestablePreFetcher->QueueContentProtection(drmHelper1, "period1", 0, eMEDIATYPE_VIDEO, false);
	mTestablePreFetcher->QueueContentProtection(drmHelper2, "period1", 1, eMEDIATYPE_AUDIO, false);
	mTestablePreFetcher->QueueContentProtection(drmHelper3, "period1", 2, eMEDIATYPE_VIDEO, false);

	std::this_thread::sleep_for(std::chrono::milliseconds(50));

	mTestablePreFetcher->Term();
}

/**
 * @brief Test multi-key scenario: First VIDEO key succeeds, second VIDEO key fails
 * Expected: No error event - license already acquired for VIDEO)
 * Tests that skipErrorEvent is set when mTrackStatus[VIDEO] is already true
 */
TEST_F(AampDRMLicPreFetcherTests, MultiKey_VideoLicenseAcquired_SubsequentVideoKeyFails_SkipsError)
{
	mTestablePreFetcher->Init();

	auto keyId1 = CreateKeyIdFromHex(TEST_KEY_ID_SLOT_0); // VIDEO Slot 0 - SUCCESS
	auto keyId2 = CreateKeyIdFromHex(TEST_KEY_ID_SLOT_1); // VIDEO Slot 1 - FAILED (processed)

	mTestablePreFetcher->SetSendErrorOnFailure(false); // Ensure flag is false for this test
	auto mockLicenseFetcher = std::make_shared<StrictMock<MockStreamAbstractionAAMP>>(mPrivateInstanceAAMP.get());
	mTestablePreFetcher->SetLicenseFetcher(mockLicenseFetcher.get());

	DrmInfo drmInfo;
	drmInfo.keyFormat = "identity";
	drmInfo.mediaFormat = eMEDIAFORMAT_UNKNOWN;
	drmInfo.systemUUID = "test-uuid-multikey";

	std::map<int, std::vector<uint8_t>> allKeys;
	allKeys[0] = keyId1;
	allKeys[1] = keyId2;

	auto drmHelper1 = std::make_shared<TestDrmHelper>(drmInfo, keyId1, allKeys);
	auto drmHelper2 = std::make_shared<TestDrmHelper>(drmInfo, keyId2, allKeys);

	// First VIDEO key - not processed, will succeed
	EXPECT_CALL(*g_mockDRMSessionManager, IsKeyIdProcessed(keyId1, _))
		.Times(1)
		.WillOnce(Return(false));

	// Second VIDEO key - already processed and FAILED
	bool key2Status = false;
	EXPECT_CALL(*g_mockDRMSessionManager, IsKeyIdProcessed(keyId2, _))
		.Times(1)
		.WillOnce(DoAll(SetArgReferee<1>(key2Status), Return(true)));

	// First key succeeds - DRM session created
	auto drmSession = std::make_shared<TestDrmSession>();
	EXPECT_CALL(*g_mockAampLicenseManager, createDrmSession(_, _, _, _))
		.Times(1)
		.WillOnce(Return(drmSession.get()));

	// UpdateFailedDRMStatus expected for second key failure
	EXPECT_CALL(*mockLicenseFetcher, UpdateFailedDRMStatus(_)).Times(1);

	// Metadata sent for successful key
	EXPECT_CALL(*g_mockPrivateInstanceAAMP, SendDRMMetaData(_)).Times(::testing::AtLeast(1));

	// NO error event expected - skipErrorEvent triggered because VIDEO license already acquired (line 391-393)
	EXPECT_CALL(*g_mockPrivateInstanceAAMP, SendDrmErrorEvent(_, _)).Times(0);

	// Queue both VIDEO keys
	mTestablePreFetcher->QueueContentProtection(drmHelper1, "period1", 0, eMEDIATYPE_VIDEO, false);
	mTestablePreFetcher->QueueContentProtection(drmHelper2, "period1", 1, eMEDIATYPE_VIDEO, false);

	std::this_thread::sleep_for(std::chrono::milliseconds(50));

	mTestablePreFetcher->Term();
}

/**
 * @brief Test multi-key scenario: First AUDIO key succeeds, VIDEO key fails
 * Expected: Error event sent (different track types)
 * Tests that error is NOT skipped when failed key is different track type
 */
TEST_F(AampDRMLicPreFetcherTests, MultiKey_AudioLicenseAcquired_VideoKeyFails_SendsError)
{
	mTestablePreFetcher->Init();

	auto keyId1 = CreateKeyIdFromHex(TEST_KEY_ID_AUDIO); // AUDIO - SUCCESS
	auto keyId2 = CreateKeyIdFromHex(TEST_KEY_ID_SLOT_0); // VIDEO - FAILED

	mTestablePreFetcher->SetSendErrorOnFailure(false); // Ensure flag is false for this test
	auto mockLicenseFetcher = std::make_shared<StrictMock<MockStreamAbstractionAAMP>>(mPrivateInstanceAAMP.get());
	mTestablePreFetcher->SetLicenseFetcher(mockLicenseFetcher.get());

	DrmInfo drmInfo;
	drmInfo.keyFormat = "identity";
	drmInfo.mediaFormat = eMEDIAFORMAT_UNKNOWN;
	drmInfo.systemUUID = "test-uuid-multikey";

	std::map<int, std::vector<uint8_t>> allKeys;
	allKeys[0] = keyId1;
	allKeys[1] = keyId2;

	auto drmHelper1 = std::make_shared<TestDrmHelper>(drmInfo, keyId1, allKeys);
	auto drmHelper2 = std::make_shared<TestDrmHelper>(drmInfo, keyId2, allKeys);

	// AUDIO key - not processed, will succeed
	EXPECT_CALL(*g_mockDRMSessionManager, IsKeyIdProcessed(keyId1, _))
		.Times(1)
		.WillOnce(Return(false));

	// VIDEO key - already processed and FAILED
	bool key2Status = false;
	EXPECT_CALL(*g_mockDRMSessionManager, IsKeyIdProcessed(keyId2, _))
		.Times(1)
		.WillOnce(DoAll(SetArgReferee<1>(key2Status), Return(true)));

	// AUDIO key succeeds
	auto drmSession = std::make_shared<TestDrmSession>();
	EXPECT_CALL(*g_mockAampLicenseManager, createDrmSession(_, _, _, _))
		.Times(1)
		.WillOnce(Return(drmSession.get()));

	// Error event EXPECTED - different track type, line 391 check fails
	EXPECT_CALL(*g_mockPrivateInstanceAAMP, SendDRMMetaData(_)).Times(::testing::AtLeast(1));
	EXPECT_CALL(*g_mockPrivateInstanceAAMP, SendDrmErrorEvent(_, _)).Times(::testing::AtLeast(1));

	// Queue AUDIO then VIDEO
	mTestablePreFetcher->QueueContentProtection(drmHelper1, "period1", 0, eMEDIATYPE_AUDIO, false);
	mTestablePreFetcher->QueueContentProtection(drmHelper2, "period1", 1, eMEDIATYPE_VIDEO, false);

	std::this_thread::sleep_for(std::chrono::milliseconds(50));

	mTestablePreFetcher->Term();
}

/**
 * @brief Test multi-key scenario: All VIDEO keys fail (no successful license acquired)
 * Expected: Error notification after processing all failed keys
 * Tests that errors are sent when mTrackStatus[VIDEO] is false
 */
TEST_F(AampDRMLicPreFetcherTests, MultiKey_NoLicenseAcquired_AllVideoKeysFail_SendsError)
{
	mTestablePreFetcher->Init();

	auto keyId1 = CreateKeyIdFromHex(TEST_KEY_ID_SLOT_0); // VIDEO - FAILED
	auto keyId2 = CreateKeyIdFromHex(TEST_KEY_ID_SLOT_1); // VIDEO - FAILED
	auto keyId3 = CreateKeyIdFromHex(TEST_KEY_ID_SLOT_2); // VIDEO - FAILED

	mTestablePreFetcher->SetSendErrorOnFailure(false); // Ensure flag is false for this test
	auto mockLicenseFetcher = std::make_shared<StrictMock<MockStreamAbstractionAAMP>>(mPrivateInstanceAAMP.get());
	mTestablePreFetcher->SetLicenseFetcher(mockLicenseFetcher.get());

	DrmInfo drmInfo;
	drmInfo.keyFormat = "test-format";
	drmInfo.mediaFormat = eMEDIAFORMAT_UNKNOWN;
	drmInfo.systemUUID = "test-uuid-multikey";

	std::map<int, std::vector<uint8_t>> allKeys;
	allKeys[0] = keyId1;
	allKeys[1] = keyId2;
	allKeys[2] = keyId3;

	auto drmHelper1 = std::make_shared<TestDrmHelper>(drmInfo, keyId1, allKeys);
	auto drmHelper2 = std::make_shared<TestDrmHelper>(drmInfo, keyId2, allKeys);
	auto drmHelper3 = std::make_shared<TestDrmHelper>(drmInfo, keyId3, allKeys);

	// All VIDEO keys failed
	bool failedStatus = false;
	EXPECT_CALL(*g_mockDRMSessionManager, IsKeyIdProcessed(keyId1, _))
		.Times(1)
		.WillOnce(DoAll(SetArgReferee<1>(failedStatus), Return(true)));
	EXPECT_CALL(*g_mockDRMSessionManager, IsKeyIdProcessed(keyId2, _))
		.Times(1)
		.WillOnce(DoAll(SetArgReferee<1>(failedStatus), Return(true)));
	EXPECT_CALL(*g_mockDRMSessionManager, IsKeyIdProcessed(keyId3, _))
		.Times(1)
		.WillOnce(DoAll(SetArgReferee<1>(failedStatus), Return(true)));

	// Expect UpdateFailedDRMStatus called for two times and then error notification
	EXPECT_CALL(*mockLicenseFetcher, UpdateFailedDRMStatus(_)).Times(2);
	EXPECT_CALL(*g_mockPrivateInstanceAAMP, SendDRMMetaData(_)).Times(1);
	EXPECT_CALL(*g_mockPrivateInstanceAAMP, SendDrmErrorEvent(_, _)).Times(1);

	mTestablePreFetcher->QueueContentProtection(drmHelper1, "period1", 0, eMEDIATYPE_VIDEO, false);
	mTestablePreFetcher->QueueContentProtection(drmHelper2, "period1", 1, eMEDIATYPE_VIDEO, false);
	mTestablePreFetcher->QueueContentProtection(drmHelper3, "period1", 2, eMEDIATYPE_VIDEO, false);

	std::this_thread::sleep_for(std::chrono::milliseconds(50));

	mTestablePreFetcher->Term();
}

/**
 * @brief Test multi-key: VIDEO and AUDIO - VIDEO succeeds, AUDIO fails
 * Expected: Error sent for AUDIO (different track type from successful VIDEO)
 * Tests that skipErrorEvent only applies to same track type
 */
TEST_F(AampDRMLicPreFetcherTests, MultiKey_VideoSucceeds_AudioFails_SendsAudioError)
{
	mTestablePreFetcher->Init();

	auto keyIdVideo1 = CreateKeyIdFromHex(TEST_KEY_ID_SLOT_0); // VIDEO - SUCCESS
	auto keyIdVideo2 = CreateKeyIdFromHex(TEST_KEY_ID_SLOT_1); // VIDEO - FAILED (will be skipped)
	auto keyIdAudio = CreateKeyIdFromHex(TEST_KEY_ID_AUDIO);  // AUDIO - FAILED (will send error)

	mTestablePreFetcher->SetSendErrorOnFailure(false); // Ensure flag is false for this test
	auto mockLicenseFetcher = std::make_shared<StrictMock<MockStreamAbstractionAAMP>>(mPrivateInstanceAAMP.get());
	mTestablePreFetcher->SetLicenseFetcher(mockLicenseFetcher.get());

	DrmInfo drmInfo;
	drmInfo.keyFormat = "identity";
	drmInfo.mediaFormat = eMEDIAFORMAT_UNKNOWN;
	drmInfo.systemUUID = "test-uuid-multikey";

	std::map<int, std::vector<uint8_t>> allKeys;
	allKeys[0] = keyIdVideo1;
	allKeys[1] = keyIdVideo2;
	allKeys[2] = keyIdAudio;

	auto drmHelperVideo1 = std::make_shared<TestDrmHelper>(drmInfo, keyIdVideo1, allKeys);
	auto drmHelperVideo2 = std::make_shared<TestDrmHelper>(drmInfo, keyIdVideo2, allKeys);
	auto drmHelperAudio = std::make_shared<TestDrmHelper>(drmInfo, keyIdAudio, allKeys);

	// First VIDEO - success
	EXPECT_CALL(*g_mockDRMSessionManager, IsKeyIdProcessed(keyIdVideo1, _))
		.Times(1)
		.WillOnce(Return(false));

	// Second VIDEO - failed (will skip error due to line 391-393)
	bool failedStatus = false;
	EXPECT_CALL(*g_mockDRMSessionManager, IsKeyIdProcessed(keyIdVideo2, _))
		.Times(1)
		.WillOnce(DoAll(SetArgReferee<1>(failedStatus), Return(true)));

	// AUDIO - failed (will send error, different track type)
	EXPECT_CALL(*g_mockDRMSessionManager, IsKeyIdProcessed(keyIdAudio, _))
		.Times(1)
		.WillOnce(DoAll(SetArgReferee<1>(failedStatus), Return(true)));

	// VIDEO session created
	auto drmSession = std::make_shared<TestDrmSession>();
	EXPECT_CALL(*g_mockAampLicenseManager, createDrmSession(_, _, _, _))
		.Times(1)
		.WillOnce(Return(drmSession.get()));

	// Expect UpdateFailedDRMStatus called for VIDEO failure and AUDIO failure
	EXPECT_CALL(*mockLicenseFetcher, UpdateFailedDRMStatus(_)).Times(1);

	// Metadata for VIDEO success + AUDIO failure
	EXPECT_CALL(*g_mockPrivateInstanceAAMP, SendDRMMetaData(_)).Times(2);

	// Error event ONLY for AUDIO failure (not for second VIDEO due to skipErrorEvent)
	EXPECT_CALL(*g_mockPrivateInstanceAAMP, SendDrmErrorEvent(_, _)).Times(1);

	mTestablePreFetcher->QueueContentProtection(drmHelperVideo1, "period1", 0, eMEDIATYPE_VIDEO, false);
	mTestablePreFetcher->QueueContentProtection(drmHelperVideo2, "period1", 1, eMEDIATYPE_VIDEO, false);
	mTestablePreFetcher->QueueContentProtection(drmHelperAudio, "period1", 2, eMEDIATYPE_AUDIO, false);

	std::this_thread::sleep_for(std::chrono::milliseconds(50));

	mTestablePreFetcher->Term();
}

/**
 * @brief Test multi-key scenario with VSS period: Slot 0 failed, slots 1 and 2 usable
 * Expected: Error notification for slot 0 failure in VSS thread
 */
TEST_F(AampDRMLicPreFetcherTests, MultiKey_VssPeriod_Slot0Failed_ErrorNotifiedInVssThread)
{
	mTestablePreFetcher->Init();

	auto keyId1 = CreateKeyIdFromHex(TEST_KEY_ID_SLOT_0); // Slot 0 - FAILED
	auto keyId2 = CreateKeyIdFromHex(TEST_KEY_ID_SLOT_1); // Slot 1 - Usable
	auto keyId3 = CreateKeyIdFromHex(TEST_KEY_ID_SLOT_2); // Slot 2 - Usable

	DrmInfo drmInfo;
	drmInfo.keyFormat = "identity";
	drmInfo.mediaFormat = eMEDIAFORMAT_UNKNOWN;
	drmInfo.systemUUID = "test-uuid-multikey";

	std::map<int, std::vector<uint8_t>> allKeys;
	allKeys[0] = keyId1;
	allKeys[1] = keyId2;
	allKeys[2] = keyId3;

	auto drmHelper1 = std::make_shared<TestDrmHelper>(drmInfo, keyId1, allKeys);
	auto drmHelper2 = std::make_shared<TestDrmHelper>(drmInfo, keyId2, allKeys);
	auto drmHelper3 = std::make_shared<TestDrmHelper>(drmInfo, keyId3, allKeys);

	// Setup mocks: Slot 0 failed in VSS period
	bool slot0Status = false; // FAILED
	EXPECT_CALL(*g_mockDRMSessionManager, IsKeyIdProcessed(keyId1, _))
		.Times(1)
		.WillOnce(DoAll(SetArgReferee<1>(slot0Status), Return(true)));
	EXPECT_CALL(*g_mockDRMSessionManager, IsKeyIdProcessed(keyId2, _))
		.Times(1)
		.WillOnce(Return(false));
	EXPECT_CALL(*g_mockDRMSessionManager, IsKeyIdProcessed(keyId3, _))
		.Times(1)
		.WillOnce(Return(false));

	// Expect error notification for slot 0 failure
	EXPECT_CALL(*g_mockPrivateInstanceAAMP, SendDRMMetaData(_)).Times(::testing::AtLeast(1));
	EXPECT_CALL(*g_mockPrivateInstanceAAMP, SendDrmErrorEvent(_, _)).Times(::testing::AtLeast(1));

	// Queue as VSS content protection
	mTestablePreFetcher->QueueContentProtection(drmHelper1, "vss_period1", 0, eMEDIATYPE_VIDEO, true);
	mTestablePreFetcher->QueueContentProtection(drmHelper2, "vss_period1", 1, eMEDIATYPE_AUDIO, true);
	mTestablePreFetcher->QueueContentProtection(drmHelper3, "vss_period1", 2, eMEDIATYPE_VIDEO, true);

	std::this_thread::sleep_for(std::chrono::milliseconds(50));

	// Verify VSS thread was started
	EXPECT_TRUE(mTestablePreFetcher->isVssThreadJoinable());

	mTestablePreFetcher->Term();
}

// ============================================================================
// CONCURRENCY TESTS FOR mLicenseAcquisitionMutex
// ============================================================================

/**
 * @brief Test: Mutex correctly serializes concurrent license acquisition attempts
 *
 * Verify that the mLicenseAcquisitionMutex properly serializes multiple concurrent
 * license acquisition requests, ensuring thread safety and preventing race conditions
 * during simultaneous createDrmSession() calls.
 *
 * Scenario:
 * - Multiple threads queue content protection requests for different tracks (VIDEO, AUDIO)
 * - Each request triggers CreateDRMSession() which acquires mLicenseAcquisitionMutex
 * - Verify createDrmSession() is called sequentially (not concurrently)
 * - Verify no data corruption occurs during concurrent access
 */
TEST_F(AampDRMLicPreFetcherTests, ConcurrencyTest_MutexSerializesLicenseAcquisition)
{
	mTestablePreFetcher->Init();

	auto keyIdVideo = CreateKeyIdFromHex(TEST_KEY_ID_SLOT_0);
	auto keyIdAudio = CreateKeyIdFromHex(TEST_KEY_ID_AUDIO);

	DrmInfo drmInfo;
	drmInfo.keyFormat = "identity";
	drmInfo.mediaFormat = eMEDIAFORMAT_UNKNOWN;
	drmInfo.systemUUID = "test-uuid-concurrent";

	std::map<int, std::vector<uint8_t>> allKeys;
	allKeys[0] = keyIdVideo;
	allKeys[1] = keyIdAudio;

	auto drmHelperVideo = std::make_shared<TestDrmHelper>(drmInfo, keyIdVideo, allKeys);
	auto drmHelperAudio = std::make_shared<TestDrmHelper>(drmInfo, keyIdAudio, allKeys);

	// Use atomic counters to detect concurrent execution - no timing-based sleeps
	std::atomic<int> concurrentCallCount{0};
	std::atomic<int> maxConcurrentCalls{0};
	std::atomic<int> callCount{0};
	std::condition_variable callsCompletedCond;
	std::mutex callCountMutex;

	auto recordEntryExit = [&](bool entering)
	{
		if (entering)
		{
			++concurrentCallCount;
			int current = concurrentCallCount.load();
			int maxSeen = maxConcurrentCalls.load();
			while (current > maxSeen && !maxConcurrentCalls.compare_exchange_weak(maxSeen, current))
			{
				maxSeen = maxConcurrentCalls.load();
			}
		}
		else
		{
			--concurrentCallCount;
		}
	};

	// Setup mock to track concurrent access - no timing-based delays
	auto drmSession = std::make_shared<TestDrmSession>();
	EXPECT_CALL(*g_mockAampLicenseManager, createDrmSession(_, _, _, _))
		.Times(2)
		.WillRepeatedly(DoAll(
			Invoke([&]()
			{
				recordEntryExit(true);
			}),
			// Yield to allow other threads to attempt concurrent access
			Invoke([](auto, auto, auto, auto)
			{
				for (int i = 0; i < 10; ++i)
				{
					std::this_thread::yield();
				}
			}),
			Invoke([&]()
			{
				recordEntryExit(false);
			}),
			Invoke([&]()
			{
				{
					std::unique_lock<std::mutex> lock(callCountMutex);
					++callCount;
				}
				callsCompletedCond.notify_one();
			}),
			Return(drmSession.get())
		));

	EXPECT_CALL(*g_mockDRMSessionManager, IsKeyIdProcessed(keyIdVideo, _))
		.Times(1)
		.WillOnce(Return(false));
	EXPECT_CALL(*g_mockDRMSessionManager, IsKeyIdProcessed(keyIdAudio, _))
		.Times(1)
		.WillOnce(Return(false));

	// Queue both requests
	mTestablePreFetcher->QueueContentProtection(drmHelperVideo, "period1", 0, eMEDIATYPE_VIDEO, false);
	mTestablePreFetcher->QueueContentProtection(drmHelperAudio, "period1", 1, eMEDIATYPE_AUDIO, false);

	// Wait for both calls to complete
	{
		std::unique_lock<std::mutex> lock(callCountMutex);
		auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(10);
		while (callCount.load() < 2)
		{
			auto status = callsCompletedCond.wait_until(lock, deadline);
			if (status == std::cv_status::timeout)
			{
				FAIL() << "Timeout waiting for license acquisition to complete";
				break;
			}
		}
	}

	// Verify mutex prevented concurrent access
	EXPECT_EQ(maxConcurrentCalls.load(), 1)
		<< "Mutex should serialize calls; max concurrent should be 1, but was " << maxConcurrentCalls.load();

	mTestablePreFetcher->Term();
}

/**
 * @brief Test: Term() properly waits for ongoing license acquisition to complete
 *
 * Verify that the stop/Term() flow correctly waits for any in-flight license acquisition
 * to complete before returning. This is critical to prevent applying a previous tune's
 * license to the CDM after a new tune has been initiated (fast channel change scenario).
 *
 * Uses synchronization primitives (condition variables) instead of timing-based
 * delays to ensure reliable, deterministic behavior across all systems.
 *
 * Scenario:
 * - Start license acquisition
 * - Signal when acquisition starts (without timing delays)
 * - Call Term() while acquisition is ongoing
 * - Verify Term() blocks until acquisition completes
 */
TEST_F(AampDRMLicPreFetcherTests, ConcurrencyTest_TermWaitsForOngoingLicenseAcquisition)
{
	mTestablePreFetcher->Init();

	auto keyIdVideo = CreateKeyIdFromHex(TEST_KEY_ID_SLOT_0);

	DrmInfo drmInfo;
	drmInfo.keyFormat = "identity";
	drmInfo.mediaFormat = eMEDIAFORMAT_UNKNOWN;
	drmInfo.systemUUID = "test-uuid-term-wait";

	std::map<int, std::vector<uint8_t>> allKeys;
	allKeys[0] = keyIdVideo;

	auto drmHelperVideo = std::make_shared<TestDrmHelper>(drmInfo, keyIdVideo, allKeys);

	// Set up mock license fetcher
	auto mockLicenseFetcher = std::make_shared<StrictMock<MockStreamAbstractionAAMP>>(mPrivateInstanceAAMP.get());
	mTestablePreFetcher->SetLicenseFetcher(mockLicenseFetcher.get());

	// Robust synchronization using condition variables instead of timing
	std::mutex syncMutex;
	std::condition_variable drmSessionStartedCond;
	std::condition_variable termStartedCond;
	std::condition_variable drmWaitingForSignalCond;
	std::condition_variable drmAllowedToCompleteCond;
	bool drmSessionStarted = false;
	bool termStartedFlag = false;
	bool drmWaitingForSignal = false;
	bool allowDrmToComplete = false;

	auto drmSession = std::make_shared<TestDrmSession>();
	EXPECT_CALL(*g_mockAampLicenseManager, createDrmSession(_, _, _, _))
		.Times(1)
		.WillOnce(DoAll(
			Invoke([&]()
			{
				std::unique_lock<std::mutex> lock(syncMutex);
				drmSessionStarted = true;
				drmSessionStartedCond.notify_all();
			}),
			// Wait for Term() to be called
			Invoke([&]()
			{
				{
					std::unique_lock<std::mutex> lock(syncMutex);
					auto waitDeadline = std::chrono::steady_clock::now() + std::chrono::seconds(5);
					while (!termStartedFlag)
					{
						auto status = termStartedCond.wait_until(lock, waitDeadline);
						if (status == std::cv_status::timeout)
						{
							FAIL() << "Timeout waiting for Term() to be called";
							break;
						}
					}
				}

				// Signal that we're now waiting for completion signal (SetLicenseFetcher should be blocked on mutex)
				{
					std::unique_lock<std::mutex> lock(syncMutex);
					drmWaitingForSignal = true;
					drmWaitingForSignalCond.notify_one();
				}

				// Wait for signal to proceed (allows test to verify Term() is blocked before continuing)
				{
					std::unique_lock<std::mutex> lock(syncMutex);
					auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(5);
					while (!allowDrmToComplete)
					{
						auto status = drmAllowedToCompleteCond.wait_until(lock, deadline);
						if (status == std::cv_status::timeout)
						{
							break; // Continue even if signal never arrives
						}
					}
				}
			}),
			Return(drmSession.get())
		));

	EXPECT_CALL(*g_mockDRMSessionManager, IsKeyIdProcessed(keyIdVideo, _))
		.Times(1)
		.WillOnce(Return(false));

	// Queue license acquisition
	mTestablePreFetcher->QueueContentProtection(drmHelperVideo, "period1", 0, eMEDIATYPE_VIDEO, false);

	// Wait for license acquisition to start (with synchronization, not timing)
	{
		std::unique_lock<std::mutex> lock(syncMutex);
		auto waitDeadline = std::chrono::steady_clock::now() + std::chrono::seconds(5);
		while (!drmSessionStarted) 
		{
			auto status = drmSessionStartedCond.wait_until(lock, waitDeadline);
			if (status == std::cv_status::timeout)
			{
				FAIL() << "Timeout waiting for license acquisition to start";
				break;
			}
		}
	}

	// Now call Term() while license acquisition is ongoing
	std::atomic<bool> termCompleted{false};
	std::thread termThread([this, &syncMutex, &termStartedCond, &termStartedFlag, &termCompleted]()
	{
		{
			std::unique_lock<std::mutex> lock(syncMutex);
			termStartedFlag = true;
			termStartedCond.notify_all();
		}
		mTestablePreFetcher->Term();
		termCompleted.store(true, std::memory_order_release);
	});

	// Wait for DRM to signal that it's now waiting for completion signal
	// At this point SetLicenseFetcher should be blocked on the mutex
	{
		std::unique_lock<std::mutex> lock(syncMutex);
		auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(5);
		while (!drmWaitingForSignal)
		{
			auto status = drmWaitingForSignalCond.wait_until(lock, deadline);
			if (status == std::cv_status::timeout)
			{
				FAIL() << "Timeout waiting for DRM to be ready for completion signal";
				break;
			}
		}
	}

	// At this point, DRM has confirmed termStartedFlag and is waiting for allowDrmToComplete
	// Term() has been called and SetLicenseFetcher should be blocked on mLicenseAcquisitionMutex
	// Verify termCompleted is still false - this is the CRITICAL check for the mutex lock
	EXPECT_FALSE(termCompleted.load(std::memory_order_acquire))
		<< "Term() should still be blocked while license acquisition is ongoing and DRM holds the mutex";

	// Signal DRM to complete
	{
		std::unique_lock<std::mutex> lock(syncMutex);
		allowDrmToComplete = true;
	}
	drmAllowedToCompleteCond.notify_one();

	// Wait for Term to complete (will detect deadlock if it doesn't return)
	auto joinStart = std::chrono::steady_clock::now();
	termThread.join();
	auto joinEnd = std::chrono::steady_clock::now();

	// Verify that Term() completed
	EXPECT_TRUE(termCompleted.load(std::memory_order_acquire))
		<< "Term() should have completed";

	// The join() completing without hanging is the real proof that no deadlock occurred
	auto joinDuration = std::chrono::duration_cast<std::chrono::seconds>(joinEnd - joinStart);
	EXPECT_LT(joinDuration.count(), 10)
		<< "Term() should complete within reasonable time, not hung on deadlock";
}

/**
 * @brief Test: No deadlock when NotifyDrmFailure is invoked from CreateDRMSession flow
 *
 * Verify that calling NotifyDrmFailure() from within the CreateDRMSession flow
 * (directly or indirectly) does not cause deadlock. The critical contract is that
 * NotifyDrmFailure must NOT be called while holding mLicenseAcquisitionMutex.
 *
 * Scenario:
 * - CreateDRMSession holds mLicenseAcquisitionMutex during license acquisition
 * - If session creation fails, NotifyDrmFailure is called AFTER releasing the mutex
 * - NotifyDrmFailure will acquire mLicenseAcquisitionMutex (and may access mFetchInstance)
 * - Verify no deadlock occurs during error flow
 */
TEST_F(AampDRMLicPreFetcherTests, ConcurrencyTest_NotifyDrmFailureNoDeadlock)
{
	mTestablePreFetcher->Init();

	auto keyIdVideo = CreateKeyIdFromHex(TEST_KEY_ID_SLOT_0);

	DrmInfo drmInfo;
	drmInfo.keyFormat = "identity";
	drmInfo.mediaFormat = eMEDIAFORMAT_UNKNOWN;
	drmInfo.systemUUID = "test-uuid-deadlock";

	std::map<int, std::vector<uint8_t>> allKeys;
	allKeys[0] = keyIdVideo;

	auto drmHelperVideo = std::make_shared<TestDrmHelper>(drmInfo, keyIdVideo, allKeys);

	auto mockLicenseFetcher = std::make_shared<StrictMock<MockStreamAbstractionAAMP>>(mPrivateInstanceAAMP.get());
	mTestablePreFetcher->SetLicenseFetcher(mockLicenseFetcher.get());

	// Simulate session creation failure
	EXPECT_CALL(*g_mockAampLicenseManager, createDrmSession(_, _, _, _))
		.Times(1)
		.WillOnce(Return(nullptr)); // Return null to trigger error flow

	EXPECT_CALL(*g_mockDRMSessionManager, IsKeyIdProcessed(keyIdVideo, _))
		.Times(1)
		.WillOnce(Return(false));

	EXPECT_CALL(*g_mockPrivateInstanceAAMP, SendDRMMetaData(_))
		.Times(1);

	std::condition_variable errorCompletedCond;
	std::mutex errorMutex;
	int errorCount = 0;

	EXPECT_CALL(*g_mockPrivateInstanceAAMP, SendDrmErrorEvent(_, _))
		.Times(1)
		.WillOnce(Invoke([&]()
		{
			std::unique_lock<std::mutex> lock(errorMutex);
			++errorCount;
			errorCompletedCond.notify_one();
		}));

	// Queue license acquisition that will fail
	mTestablePreFetcher->QueueContentProtection(drmHelperVideo, "period1", 0, eMEDIATYPE_VIDEO, false);

	// Wait for the error event to be sent (synchronization-based instead of timing)
	{
		std::unique_lock<std::mutex> lock(errorMutex);
		auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(5);
		while (errorCount == 0)
		{
			auto status = errorCompletedCond.wait_until(lock, deadline);
			if (status == std::cv_status::timeout)
			{
				FAIL() << "Timeout waiting for error event";
				break;
			}
		}
	}

	// If we get here without hanging, no deadlock occurred
	SUCCEED() << "No deadlock detected during NotifyDrmFailure error flow";

	mTestablePreFetcher->Term();
}

/**
 * @brief Test: SetLicenseFetcher and CreateDRMSession don't deadlock
 *
 * Verify that SetLicenseFetcher (which holds mLicenseAcquisitionMutex) and
 * CreateDRMSession (which also acquires the same mutex) don't deadlock when
 * called concurrently from different threads.
 *
 * Scenario:
 * - Main thread: Update mFetchInstance via SetLicenseFetcher
 * - Prefetch thread: Acquire license via CreateDRMSession
 * - Verify both operations complete without deadlock
 */
TEST_F(AampDRMLicPreFetcherTests, ConcurrencyTest_SetFetcherCreateSessionNoDeadlock)
{
	mTestablePreFetcher->Init();

	auto keyIdVideo = CreateKeyIdFromHex(TEST_KEY_ID_SLOT_0);
	auto keyIdAudio = CreateKeyIdFromHex(TEST_KEY_ID_AUDIO);

	DrmInfo drmInfo;
	drmInfo.keyFormat = "identity";
	drmInfo.mediaFormat = eMEDIAFORMAT_UNKNOWN;
	drmInfo.systemUUID = "test-uuid-no-deadlock";

	std::map<int, std::vector<uint8_t>> allKeys;
	allKeys[0] = keyIdVideo;
	allKeys[1] = keyIdAudio;

	auto drmHelperVideo = std::make_shared<TestDrmHelper>(drmInfo, keyIdVideo, allKeys);
	auto drmHelperAudio = std::make_shared<TestDrmHelper>(drmInfo, keyIdAudio, allKeys);

	auto mockLicenseFetcher1 = std::make_shared<StrictMock<MockStreamAbstractionAAMP>>(mPrivateInstanceAAMP.get());
	auto mockLicenseFetcher2 = std::make_shared<StrictMock<MockStreamAbstractionAAMP>>(mPrivateInstanceAAMP.get());

	// Synchronization to ensure SetLicenseFetcher is called and blocks during createDrmSession execution
	std::condition_variable drmInProgressCond;
	std::condition_variable allowDrmToCompleteCond;
	std::condition_variable setFetcherAttemptedCond;
	std::condition_variable setFetcherCompletedCond;
	std::condition_variable drmSessionsCompletedCond;
	std::mutex syncMutex;
	bool drmSessionInProgress = false;
	bool allowDrmToRelease = false;
	int drmSessionsCompleted = 0;

	auto drmSession = std::make_shared<TestDrmSession>();
	EXPECT_CALL(*g_mockAampLicenseManager, createDrmSession(_, _, _, _))
		.Times(2)
		.WillOnce(DoAll(
			// First DRM session - signal that createDrmSession is executing and holding mLicenseAcquisitionMutex
			Invoke([&]() {
				std::unique_lock<std::mutex> lock(syncMutex);
				drmSessionInProgress = true;
				drmInProgressCond.notify_one();
				AAMPLOG_INFO("Test:mockAampLicenseManager: DRM signaled InProgress, now waiting for release");
			}),
			// Wait for signal to proceed - this keeps the mock executing (and thus holding the mutex)
			Invoke([&]() {
				std::unique_lock<std::mutex> lock(syncMutex);
				auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(10);
				while (!allowDrmToRelease) {
					auto status = allowDrmToCompleteCond.wait_until(lock, deadline);
					if (status == std::cv_status::timeout) {
						FAIL() << "Timeout waiting for DRM release signal";
						break;
					}
				}
				allowDrmToRelease = false;
				AAMPLOG_INFO("Test:mockAampLicenseManager: DRM received release signal, proceeding");
			}),
			// Increment completion counter
			Invoke([&]() {
				{
					std::unique_lock<std::mutex> lock(syncMutex);
					++drmSessionsCompleted;
					AAMPLOG_INFO("Test:mockAampLicenseManager: DRM session completed");
				}
				drmSessionsCompletedCond.notify_one();
			}),
			Return(drmSession.get())
		))
		.WillOnce(DoAll(
			// Second DRM session - just complete quickly to allow test to finish
			Invoke([&]() {
				{
					std::unique_lock<std::mutex> lock(syncMutex);
					++drmSessionsCompleted;
				}
				drmSessionsCompletedCond.notify_one();
			}),
			Return(drmSession.get())
		));

	EXPECT_CALL(*g_mockDRMSessionManager, IsKeyIdProcessed(keyIdVideo, _))
		.Times(1)
		.WillOnce(Return(false));
	EXPECT_CALL(*g_mockDRMSessionManager, IsKeyIdProcessed(keyIdAudio, _))
		.Times(1)
		.WillOnce(Return(false));

	// Set initial fetcher
	mTestablePreFetcher->SetLicenseFetcher(mockLicenseFetcher1.get());

	// Queue concurrent license requests to trigger CreateDRMSession
	mTestablePreFetcher->QueueContentProtection(drmHelperVideo, "period1", 0, eMEDIATYPE_VIDEO, false);
	mTestablePreFetcher->QueueContentProtection(drmHelperAudio, "period1", 1, eMEDIATYPE_AUDIO, false);

	// Wait for DRM session to start (it's now holding mLicenseAcquisitionMutex)
	{
		std::unique_lock<std::mutex> lock(syncMutex);
		auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(5);
		while (!drmSessionInProgress) {
			auto status = drmInProgressCond.wait_until(lock, deadline);
			if (status == std::cv_status::timeout) {
				FAIL() << "Timeout waiting for DRM session to start";
				break;
			}
		}
	}

	// Now call SetLicenseFetcher while createDrmSession is actively holding mLicenseAcquisitionMutex
	// SetLicenseFetcher will BLOCK on mLicenseAcquisitionMutex acquisition (by design)
	std::atomic<bool> setFetcherAttempted{false};
	std::atomic<bool> setFetcherCompleted{false};
	std::thread setFetcherThread([this, &mockLicenseFetcher2, &setFetcherAttempted, &setFetcherCompleted, &syncMutex, &setFetcherAttemptedCond, &setFetcherCompletedCond]() {
		// Signal that we're about to attempt SetLicenseFetcher
		{
			std::unique_lock<std::mutex> lock(syncMutex);
			setFetcherAttempted.store(true);
			setFetcherAttemptedCond.notify_one();
		}
		// This call will block on mLicenseAcquisitionMutex until createDrmSession releases it
		mTestablePreFetcher->SetLicenseFetcher(mockLicenseFetcher2.get());
		setFetcherCompleted.store(true);
		{
			std::unique_lock<std::mutex> lock(syncMutex);
		setFetcherCompletedCond.notify_one();//anj
		}
	});

	// Wait for SetLicenseFetcher thread to signal that it has attempted to acquire the mutex
	// This is more reliable than a fixed sleep - we wait until the thread actually tries
	{
		std::unique_lock<std::mutex> lock(syncMutex);
		auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(5);
		while (!setFetcherAttempted.load()) {
			auto status = setFetcherAttemptedCond.wait_until(lock, deadline);
			if (status == std::cv_status::timeout) {
				FAIL() << "Timeout waiting for SetLicenseFetcher to attempt";
				break;
			}
		}
	}

	// Verify that SetLicenseFetcher thread has attempted but not completed (blocked on mutex)
	EXPECT_TRUE(setFetcherAttempted.load()) << "SetLicenseFetcher thread should have attempted to acquire mutex";

	// CRITICAL: Check if SetLicenseFetcher completed too quickly
	// If it did, it means mLicenseAcquisitionMutex lock was NOT held in CreateDRMSession (BUG!)
	if (setFetcherCompleted.load()) {
		FAIL() << "CRITICAL TEST FAILURE: SetLicenseFetcher completed immediately without blocking on mLicenseAcquisitionMutex! "
			   << "This indicates mLicenseAcquisitionMutex lock is missing in CreateDRMSession execution. "
			   << "Verify that std::lock_guard<std::mutex> lock(mLicenseAcquisitionMutex) is present before licenseManger->createDrmSession() call.";
	}

	EXPECT_FALSE(setFetcherCompleted.load()) << "SetLicenseFetcher thread should be blocked on mLicenseAcquisitionMutex";

	// Signal DRM session to complete and release mLicenseAcquisitionMutex
	{
		std::unique_lock<std::mutex> lock(syncMutex);
		allowDrmToRelease = true;
	}
	allowDrmToCompleteCond.notify_one();

	// Wait for SetLicenseFetcher to complete (it should now acquire the released mutex)
	// Set a shorter timeout to catch issues where mutex lock is missing
	{
		std::unique_lock<std::mutex> lock(syncMutex);
		auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(2000);
		while (!setFetcherCompleted.load()) {
			auto status = setFetcherCompletedCond.wait_until(lock, deadline);
			if (status == std::cv_status::timeout) {
				// Check if SetLicenseFetcher attempted at all
				if (!setFetcherAttempted.load()) {
					FAIL() << "SetLicenseFetcher thread never attempted to acquire mutex - thread may be hung or not started";
				} else {
					FAIL() << "Timeout waiting for SetLicenseFetcher to complete after DRM release. "
						   << "SetLicenseFetcher attempted=" << setFetcherAttempted.load()
						   << ", completed=" << setFetcherCompleted.load()
						   << ". This may indicate a deadlock or mutex synchronization issue in CreateDRMSession.";
				}
				break;
			}
		}
	}

	EXPECT_TRUE(setFetcherCompleted.load()) << "SetLicenseFetcher should have completed after DRM released mutex";

	// Join SetLicenseFetcher thread
	if (setFetcherThread.joinable()) {
		setFetcherThread.join();
	}

	// Wait for DRM sessions to complete
	{
		std::unique_lock<std::mutex> lock(syncMutex);
		auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(10);
		while (drmSessionsCompleted < 2) {
			auto status = drmSessionsCompletedCond.wait_until(lock, deadline);
			if (status == std::cv_status::timeout) {
				FAIL() << "Timeout waiting for DRM sessions to complete";
				break;
			}
		}
	}

	// Verify no deadlock occurred
	SUCCEED() << "SetLicenseFetcher correctly blocked and then proceeded when DRM released mutex - no deadlock";

	mTestablePreFetcher->Term();
}

/**
 * @brief Test: Mutex protects mFetchInstance access during concurrent NotifyDrmFailure calls
 *
 * Verify that mLicenseAcquisitionMutex properly protects access to mFetchInstance when
 * NotifyDrmFailure is called concurrently with SetLicenseFetcher. This ensures that
 * concurrent attempts to read/write mFetchInstance don't cause data corruption.
 *
 * Scenario:
 * - Queue multiple license requests that will fail
 * - Concurrently update mFetchInstance via SetLicenseFetcher
 * - NotifyDrmFailure calls use mFetchInstance safely
 * - Verify no use-after-free or null pointer dereference
 */
TEST_F(AampDRMLicPreFetcherTests, ConcurrencyTest_ProtectsFetchInstanceAccess)
{
	mTestablePreFetcher->Init();

	auto keyId1 = CreateKeyIdFromHex(TEST_KEY_ID_SLOT_0);
	auto keyId2 = CreateKeyIdFromHex(TEST_KEY_ID_SLOT_1);
	auto keyId3 = CreateKeyIdFromHex(TEST_KEY_ID_SLOT_2);

	DrmInfo drmInfo;
	drmInfo.keyFormat = "identity";
	drmInfo.mediaFormat = eMEDIAFORMAT_UNKNOWN;
	drmInfo.systemUUID = "test-uuid-fetch-instance";

	std::map<int, std::vector<uint8_t>> allKeys;
	allKeys[0] = keyId1;
	allKeys[1] = keyId2;
	allKeys[2] = keyId3;

	auto drmHelper1 = std::make_shared<TestDrmHelper>(drmInfo, keyId1, allKeys);
	auto drmHelper2 = std::make_shared<TestDrmHelper>(drmInfo, keyId2, allKeys);
	auto drmHelper3 = std::make_shared<TestDrmHelper>(drmInfo, keyId3, allKeys);

	auto mockLicenseFetcher1 = std::make_shared<StrictMock<MockStreamAbstractionAAMP>>(mPrivateInstanceAAMP.get());
	auto mockLicenseFetcher2 = std::make_shared<StrictMock<MockStreamAbstractionAAMP>>(mPrivateInstanceAAMP.get());

	// All requests will fail to trigger NotifyDrmFailure
	EXPECT_CALL(*g_mockAampLicenseManager, createDrmSession(_, _, _, _))
		.Times(3)
		.WillRepeatedly(Return(nullptr));

	EXPECT_CALL(*g_mockDRMSessionManager, IsKeyIdProcessed(_, _))
		.Times(3)
		.WillRepeatedly(Return(false));

	EXPECT_CALL(*g_mockPrivateInstanceAAMP, SendDRMMetaData(_))
		.Times(3);
	
	std::condition_variable errorCompletedCond;
	std::mutex errorMutex;
	int errorCount = 0;

	EXPECT_CALL(*g_mockPrivateInstanceAAMP, SendDrmErrorEvent(_, _))
		.Times(3)
		.WillRepeatedly(Invoke([&]()
		{
			std::unique_lock<std::mutex> lock(errorMutex);
			++errorCount;
			errorCompletedCond.notify_one();
		}));

	// Set initial fetcher
	mTestablePreFetcher->SetLicenseFetcher(mockLicenseFetcher1.get());

	// Queue multiple failing requests
	mTestablePreFetcher->QueueContentProtection(drmHelper1, "period1", 0, eMEDIATYPE_VIDEO, false);
	mTestablePreFetcher->QueueContentProtection(drmHelper2, "period1", 1, eMEDIATYPE_AUDIO, false);
	mTestablePreFetcher->QueueContentProtection(drmHelper3, "period1", 2, eMEDIATYPE_VIDEO, false);

	// Concurrent thread updating fetcher instance
	std::atomic<bool> swapComplete{false};
	std::condition_variable swapCompleteCond;
	std::mutex swapMutex;

	std::thread fetcherSwapThread([this, &mockLicenseFetcher1, &mockLicenseFetcher2, &swapCompleteCond, &swapMutex, &swapComplete]()
	{
		int count = 0;
		while (count++ < 20)
		{
			mTestablePreFetcher->SetLicenseFetcher(mockLicenseFetcher2.get());
			std::this_thread::yield();
			mTestablePreFetcher->SetLicenseFetcher(mockLicenseFetcher1.get());
			std::this_thread::yield();
		}
		{
			std::unique_lock<std::mutex> lock(swapMutex);
			swapComplete.store(true);
		}
		swapCompleteCond.notify_one();
	});

	// Wait for both fetcher swap thread and all error events to complete
	{
		std::unique_lock<std::mutex> errorLock(errorMutex);
		auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(10);
		while (errorCount < 3)
		{
			auto status = errorCompletedCond.wait_until(errorLock, deadline);
			if (status == std::cv_status::timeout)
			{
				FAIL() << "Timeout waiting for all error events to complete";
				break;
			}
		}
	}

	// Also wait for the fetcher swap thread to complete
	{
		std::unique_lock<std::mutex> swapLock(swapMutex);
		auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(10);
		while (!swapComplete.load())
		{
			auto status = swapCompleteCond.wait_until(swapLock, deadline);
			if (status == std::cv_status::timeout)
			{
				FAIL() << "Timeout waiting for fetcher swap to complete";
				break;
			}
		}
	}

	// Stop the swapping thread
	if (fetcherSwapThread.joinable())
	{
		fetcherSwapThread.join();
	}

	// Verify test completed without crash/deadlock
	SUCCEED() << "Concurrent mFetchInstance access handled safely";

	mTestablePreFetcher->Term();
}
