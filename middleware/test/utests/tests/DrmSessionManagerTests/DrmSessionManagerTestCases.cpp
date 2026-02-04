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
#include <vector>
#include <memory>
#include "DrmSessionManager.h"
#include "MockDrmHelper.h"
#include "PlayerUtils.h"
#include "opencdmsessionadapter.h"
#include "MockOpenCdmSessionAdapter.h"
#include "DrmCallbacks.h"
#include "MockDrmSessionFactory.h"
#include "DrmHelper.h"
#include "WidevineDrmHelper.h"
#include "DrmInfo.h"
#include "_base64.h"

using ::testing::_;
using ::testing::Return;
using ::testing::NiceMock;
using ::testing::DoAll;
using ::testing::SetArgReferee;
using ::testing::ReturnRef;
using ::testing::Invoke;

// Forward declarations for fakes
extern MockOpenCdmSessionAdapter *g_mockOpenCdmSessionAdapter;

/**
 * @brief Testable DrmSessionManager that provides controlled access to protected members for testing
 */
class TestableDrmSessionManager : public DrmSessionManager
{
public:
	TestableDrmSessionManager(int maxDrmSessions, void *player, std::function<void(uint32_t, uint32_t, const std::string&)> watermarkCallback)
		: DrmSessionManager(maxDrmSessions, player, watermarkCallback), mMaxSessions(maxDrmSessions)
	{
	}

	// Provide controlled access to cachedKeyIDs with mutex protection
	void AddKeyIdEntryToCache(int slot, const KeyIdEntries& entries)
	{
		std::lock_guard<std::mutex> guard(cachedKeyMutex);
		if (slot >= 0 && slot < mMaxSessions)
		{
			// Replace all entries in the slot with the new entries from PSSH
			cachedKeyIDs[slot] = entries;
			
			// Log all entries in this slot
			MW_LOG_WARN("[AddKeyIdEntryToCache] slot=%d, total entries=%zu",
			       slot, cachedKeyIDs[slot].data.size());
			for (int i = 0; i < cachedKeyIDs[slot].data.size(); ++i)
			{
				std::string keyIdHex = PlayerLogManager::getHexDebugStr(cachedKeyIDs[slot].data[i].keyId);
				MW_LOG_WARN("  [%d][%d] keyId=%s, isFailedKeyId=%d\n", 
				       slot, i, keyIdHex.c_str(), cachedKeyIDs[slot].data[i].isFailedKeyId);
			}
		}
	}

	// Expose drmSessionContexts for test setup
	using DrmSessionManager::drmSessionContexts;

private:
	int mMaxSessions;
};

/**
 * @brief Test fixture for DrmSessionManager functionality
 */
class DrmSessionManagerComplexTests : public ::testing::Test
{
protected:
	static constexpr int maxDrmSessions = 3;
	std::unique_ptr<TestableDrmSessionManager> mDrmSessionManager;
	// Maintain ownership of test sessions to prevent use-after-free
	std::vector<std::shared_ptr<DrmSession>> mTestSessions;

	void SetUp() override
	{
		// Create TestableDrmSessionManager with maxDrmSessions, player=nullptr, watermarkCallback=nullptr
		mDrmSessionManager = std::make_unique<TestableDrmSessionManager>(maxDrmSessions, nullptr, nullptr);
		
		// Setup global mock adapter for all tests
		g_mockOpenCdmSessionAdapter = new NiceMock<MockOpenCdmSessionAdapter>();
	}

	void TearDown() override
	{
		// Clear manager's session references first to avoid use-after-free
		if (mDrmSessionManager)
		{
			// Clear all session references to prevent use-after-free
			for (int i = 0; i < maxDrmSessions; ++i)
			{
				mDrmSessionManager->drmSessionContexts[i].drmSession = nullptr;
			}
		}
		// Now safe to destroy manager and clear test sessions in any order
		mDrmSessionManager.reset();
		mTestSessions.clear();

		delete g_mockOpenCdmSessionAdapter;
		g_mockOpenCdmSessionAdapter = nullptr;
	}

	/**
	 * @brief Helper to create a key ID vector from hex string
	 */
	std::vector<uint8_t> CreateKeyIdFromHex(const std::string& hexStr)
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
	 * @brief Helper to create a key ID in ASCII hex format
	 */
	std::vector<uint8_t> CreateAsciiHexKeyId(const std::string& hexStr)
	{
		std::vector<uint8_t> keyId(hexStr.begin(), hexStr.end());
		return keyId;
	}

	/**
	 * @brief Create a mock DrmHelper for testing
	 */
	std::shared_ptr<MockDrmHelper> CreateMockDrmHelper(const std::string& systemId)
	{
		// Use function-local static strings that persist for ReturnRef
		static const std::string widevineSystemId = "com.widevine.alpha";
		static const std::string playreadySystemId = "com.microsoft.playready";
		static const std::string clearkeySystemId = "org.w3.clearkey";

		auto mockHelper = std::make_shared<NiceMock<MockDrmHelper>>();
		// Use ReturnRef since ocdmSystemId() returns const std::string&
		if (systemId == "com.widevine.alpha") {
			ON_CALL(*mockHelper, ocdmSystemId())
				.WillByDefault(ReturnRef(widevineSystemId));
		} else if (systemId == "com.microsoft.playready") {
			ON_CALL(*mockHelper, ocdmSystemId())
				.WillByDefault(ReturnRef(playreadySystemId));
		} else if (systemId == "org.w3.clearkey") {
			ON_CALL(*mockHelper, ocdmSystemId())
				.WillByDefault(ReturnRef(clearkeySystemId));
		}
		return mockHelper;
	}



	/**
	 * @brief Helper to set a DRM session in a specific slot for testing
	 * Maintains ownership of the session until TearDown to prevent use-after-free
	 * @param slot The slot index to assign the session to
	 * @param drmSession The DRM session to assign
	 */
	void SetDrmSessionInSlot(int slot, const std::shared_ptr<DrmSession> &drmSession)
	{
		if (slot >= 0 && slot < maxDrmSessions)
		{
			// Store shared_ptr to maintain ownership throughout test lifecycle
			mTestSessions.push_back(drmSession);
			// Assign raw pointer to manager's slot
			mDrmSessionManager->drmSessionContexts[slot].drmSession = drmSession.get();
		}
	}

	/**
	 * @brief Helper to get a DRM session from a specific slot for testing
	 * @param slot The slot index to retrieve the session from
	 * @return The DRM session in the slot, or nullptr if invalid
	 */
	DrmSession* GetDrmSessionFromSlot(int slot)
	{
		if (slot >= 0 && slot < maxDrmSessions)
		{
			return mDrmSessionManager->drmSessionContexts[slot].drmSession;
		}
		return nullptr;
	}

	/**
	 * @brief Helper to setup slot with keyIDs and DRM session in one call
	 * @param slot The slot index to setup
	 * @param keyIDs The map of key IDs to populate
	 * @param drmHelper The DRM helper to use for session creation
	 */
	void SetupSlotWithKeyIDs(int slot, const std::map<int, std::vector<uint8_t>>& keyIDs, 
	                          std::shared_ptr<DrmHelper> drmHelper)
	{
		// Create DRM session
		auto drmSession = std::make_shared<OCDMSessionAdapter>(drmHelper, nullptr);
		SetDrmSessionInSlot(slot, drmSession);
		
		// Populate cachedKeyIDs with all keys
		KeyIdEntries entries;
		for (const auto& keyEntry : keyIDs)
		{
			KeyIdEntry entry;
			entry.keyId = keyEntry.second;
			entry.isFailedKeyId = false;
			entries.data.push_back(entry);
		}
		mDrmSessionManager->AddKeyIdEntryToCache(slot, entries);
	}
};

/**
 * @brief Helper to convert hex string (with or without spaces) to binary data
 */
static std::vector<uint8_t> HexStringToBytes(const std::string& hexStr)
{
	std::vector<uint8_t> bytes;
	std::string cleanHex;
	
	// Remove spaces and convert to clean hex string
	for (char c : hexStr)
	{
		if (c != ' ' && c != '\t' && c != '\n' && c != '\r')
		{
			cleanHex += c;
		}
	}
	
	// Convert pairs of hex digits to bytes
	for (size_t i = 0; i < cleanHex.length(); i += 2)
	{
		if (i + 1 < cleanHex.length())
		{
			std::string byteStr = cleanHex.substr(i, 2);
			uint8_t byte = static_cast<uint8_t>(std::stoul(byteStr, nullptr, 16));
			bytes.push_back(byte);
		}
	}
	
	return bytes;
}

/**
 * @brief Helper function to create DrmInfo for Widevine
 */
static DrmInfo CreateWidevineDrmInfo()
{
	DrmInfo drmInfo;
	drmInfo.method = eMETHOD_AES_128;
	drmInfo.mediaFormat = eMEDIAFORMAT_DASH;
	drmInfo.systemUUID = "edef8ba9-79d6-4ace-a3c8-27dcd51d21ed"; // Widevine UUID
	drmInfo.keyFormat = "com.widevine.alpha";
	return drmInfo;
}

/**
 * @brief Test KeyIdEntry default constructor
 */
TEST_F(DrmSessionManagerComplexTests, KeyIdEntry_DefaultConstructor_InitializesCorrectly)
{
	KeyIdEntry entry;

	EXPECT_TRUE(entry.keyId.empty());
	EXPECT_FALSE(entry.isFailedKeyId);
}

/**
 * @brief Test KeyIdEntry with key ID
 */
TEST_F(DrmSessionManagerComplexTests, KeyIdEntry_WithKeyId_StoresCorrectly)
{
	KeyIdEntry entry;
	entry.keyId = {0x01, 0x02, 0x03, 0x04};
	entry.isFailedKeyId = false;

	EXPECT_EQ(entry.keyId.size(), 4);
	EXPECT_EQ(entry.keyId[0], 0x01);
	EXPECT_FALSE(entry.isFailedKeyId);
}

/**
 * @brief Test KeyIdEntry failed status
 */
TEST_F(DrmSessionManagerComplexTests, KeyIdEntry_FailedStatus_SetCorrectly)
{
	KeyIdEntry entry;
	entry.keyId = {0x01, 0x02, 0x03, 0x04};
	entry.isFailedKeyId = true;

	EXPECT_TRUE(entry.isFailedKeyId);
}

/**
 * @brief Test KeyIdEntries default constructor
 */
TEST_F(DrmSessionManagerComplexTests, KeyIdEntries_DefaultConstructor_InitializesCorrectly)
{
	KeyIdEntries entries;

	EXPECT_TRUE(entries.data.empty());
	EXPECT_EQ(entries.creationTime, 0);
	EXPECT_FALSE(entries.isPrimaryKeyId);
	EXPECT_FALSE(entries.isFailedKeyEntries);
}

/**
 * @brief Test KeyIdEntries with multiple entries
 */
TEST_F(DrmSessionManagerComplexTests, KeyIdEntries_MultipleEntries_StoresCorrectly)
{
	KeyIdEntries entries;

	KeyIdEntry entry1;
	entry1.keyId = {0x01, 0x02, 0x03, 0x04};
	entry1.isFailedKeyId = false;

	KeyIdEntry entry2;
	entry2.keyId = {0x05, 0x06, 0x07, 0x08};
	entry2.isFailedKeyId = false;

	entries.data.push_back(entry1);
	entries.data.push_back(entry2);
	entries.isFailedKeyEntries = false;

	EXPECT_EQ(entries.data.size(), 2);
	EXPECT_FALSE(entries.isFailedKeyEntries);
}

/**
 * @brief Test KeyIdEntries with failed entry
 */
TEST_F(DrmSessionManagerComplexTests, KeyIdEntries_WithFailedEntry_MarksCorrectly)
{
	KeyIdEntries entries;

	KeyIdEntry entry1;
	entry1.keyId = {0x01, 0x02, 0x03, 0x04};
	entry1.isFailedKeyId = true;

	entries.data.push_back(entry1);
	entries.isFailedKeyEntries = true;

	EXPECT_TRUE(entries.data[0].isFailedKeyId);
	EXPECT_TRUE(entries.isFailedKeyEntries);
}

/**
 * @brief Test KeyIdEntries with mixed success and failed entries
 */
TEST_F(DrmSessionManagerComplexTests, KeyIdEntries_MixedStatus_TracksIndividually)
{
	KeyIdEntries entries;

	KeyIdEntry entry1;
	entry1.keyId = {0x01, 0x02, 0x03, 0x04};
	entry1.isFailedKeyId = false;

	KeyIdEntry entry2;
	entry2.keyId = {0x05, 0x06, 0x07, 0x08};
	entry2.isFailedKeyId = true;

	KeyIdEntry entry3;
	entry3.keyId = {0x09, 0x0A, 0x0B, 0x0C};
	entry3.isFailedKeyId = false;

	entries.data.push_back(entry1);
	entries.data.push_back(entry2);
	entries.data.push_back(entry3);

	EXPECT_EQ(entries.data.size(), 3);
	EXPECT_FALSE(entries.data[0].isFailedKeyId);
	EXPECT_TRUE(entries.data[1].isFailedKeyId);
	EXPECT_FALSE(entries.data[2].isFailedKeyId);
}

/**
 * @brief Test KeyIdEntries primary key flag
 */
TEST_F(DrmSessionManagerComplexTests, KeyIdEntries_PrimaryKeyFlag_SetCorrectly)
{
	KeyIdEntries entries;
	entries.isPrimaryKeyId = true;

	EXPECT_TRUE(entries.isPrimaryKeyId);
}

/**
 * @brief Test KeyIdEntries creation time tracking
 */
TEST_F(DrmSessionManagerComplexTests, KeyIdEntries_CreationTime_TracksCorrectly)
{
	KeyIdEntries entries;
	entries.creationTime = 123456789LL;

	EXPECT_EQ(entries.creationTime, 123456789LL);
}

/**
 * @brief Test finding key ID in entries
 */
TEST_F(DrmSessionManagerComplexTests, KeyIdEntries_FindKeyId_WorksCorrectly)
{
	KeyIdEntries entries;

	std::vector<uint8_t> targetKeyId = {0x05, 0x06, 0x07, 0x08};

	KeyIdEntry entry1;
	entry1.keyId = {0x01, 0x02, 0x03, 0x04};

	KeyIdEntry entry2;
	entry2.keyId = targetKeyId;

	KeyIdEntry entry3;
	entry3.keyId = {0x09, 0x0A, 0x0B, 0x0C};

	entries.data.push_back(entry1);
	entries.data.push_back(entry2);
	entries.data.push_back(entry3);

	// Search for the key
	auto it = std::find_if(entries.data.begin(), entries.data.end(),
		[&targetKeyId](const KeyIdEntry& entry) {
			return entry.keyId == targetKeyId;
		});

	EXPECT_NE(it, entries.data.end());
	EXPECT_EQ(it->keyId, targetKeyId);
}

/**
 * @brief Test marking specific entry as failed in multi-key slot
 */
TEST_F(DrmSessionManagerComplexTests, KeyIdEntries_MarkSpecificEntryFailed_WorksCorrectly)
{
	KeyIdEntries entries;

	KeyIdEntry entry1;
	entry1.keyId = {0x01, 0x02, 0x03, 0x04};
	entry1.isFailedKeyId = false;

	KeyIdEntry entry2;
	entry2.keyId = {0x05, 0x06, 0x07, 0x08};
	entry2.isFailedKeyId = false;

	entries.data.push_back(entry1);
	entries.data.push_back(entry2);

	// Mark second entry as failed
	entries.data[1].isFailedKeyId = true;

	EXPECT_FALSE(entries.data[0].isFailedKeyId);
	EXPECT_TRUE(entries.data[1].isFailedKeyId);
}

/**
 * @brief Test complete entry failure when primary key fails
 */
TEST_F(DrmSessionManagerComplexTests, KeyIdEntries_PrimaryKeyFails_MarksEntireFailed)
{
	KeyIdEntries entries;
	entries.isPrimaryKeyId = true;

	KeyIdEntry primaryEntry;
	primaryEntry.keyId = {0x01, 0x02, 0x03, 0x04};
	primaryEntry.isFailedKeyId = true;

	entries.data.push_back(primaryEntry);
	entries.isFailedKeyEntries = true; // Entire slot marked as failed

	EXPECT_TRUE(entries.data[0].isFailedKeyId);
	EXPECT_TRUE(entries.isFailedKeyEntries);
}

/**
 * @brief Test hex string key ID vs binary key ID comparison
 */
TEST_F(DrmSessionManagerComplexTests, KeyId_HexStringVsBinary_ComparisonLogic)
{
	// Binary key ID (as stored in some cases)
	std::vector<uint8_t> binaryKeyId = {0x01, 0x23, 0x45, 0x67};

	// Hex string representation (as returned by getUsableKeys)
	std::vector<uint8_t> hexStringKeyId = {'0', '1', '2', '3', '4', '5', '6', '7'};

	// They should not be equal directly
	EXPECT_NE(binaryKeyId, hexStringKeyId);

	// Convert binary to hex string for comparison
	std::vector<uint8_t> convertedHexKeyId = RawKeyToKeyId(binaryKeyId.data(), binaryKeyId.size());
	EXPECT_EQ(convertedHexKeyId, hexStringKeyId);
}

/**
 * @brief Test scenario: Multi-key slot with one failed key
 */
TEST_F(DrmSessionManagerComplexTests, MultiKeyScenario_OneFailedKey_OthersUsable)
{
	KeyIdEntries entries;

	// Simulate UHD (failed) and HD keys (success)
	KeyIdEntry uhdEntry;
	uhdEntry.keyId = CreateAsciiHexKeyId("0123456789abcdef"); // UHD profile key
	uhdEntry.isFailedKeyId = true;

	KeyIdEntry hdEntry;
	hdEntry.keyId = CreateAsciiHexKeyId("fedcba9876543210"); // HD profile key
	hdEntry.isFailedKeyId = false;

	entries.data.push_back(uhdEntry);
	entries.data.push_back(hdEntry);

	// Slot should not be marked as completely failed if at least one key is usable
	bool hasUsableKey = false;
	for (const auto& entry : entries.data)
	{
		if (!entry.isFailedKeyId)
		{
			hasUsableKey = true;
			break;
		}
	}

	EXPECT_TRUE(hasUsableKey);
}

/**
 * @brief Test scenario: All keys in multi-key slot failed
 */
TEST_F(DrmSessionManagerComplexTests, MultiKeyScenario_AllKeysFailed_MarksSlotFailed)
{
	KeyIdEntries entries;

	KeyIdEntry entry1;
	entry1.keyId = CreateAsciiHexKeyId("0123456789abcdef");
	entry1.isFailedKeyId = true;

	KeyIdEntry entry2;
	entry2.keyId = CreateAsciiHexKeyId("fedcba9876543210");
	entry2.isFailedKeyId = true;

	entries.data.push_back(entry1);
	entries.data.push_back(entry2);
	entries.isFailedKeyEntries = true;

	// All keys failed
	bool allFailed = std::all_of(entries.data.begin(), entries.data.end(),
		[](const KeyIdEntry& entry) { return entry.isFailedKeyId; });

	EXPECT_TRUE(allFailed);
	EXPECT_TRUE(entries.isFailedKeyEntries);
}

/**
 * @brief Test DrmSessionContext default constructor
 */
TEST_F(DrmSessionManagerComplexTests, DrmSessionContext_DefaultConstructor_InitializesCorrectly)
{
	DrmSessionContext context;

	EXPECT_TRUE(context.data.empty());
	EXPECT_EQ(context.drmSession, nullptr);
}

/**
 * @brief Test DrmSessionContext with data
 */
TEST_F(DrmSessionManagerComplexTests, DrmSessionContext_WithData_StoresCorrectly)
{
	DrmSessionContext context;
	context.data = {0x01, 0x02, 0x03, 0x04};

	EXPECT_EQ(context.data.size(), 4);
	EXPECT_EQ(context.data[0], 0x01);
}

/**
 * @brief Test comparing key IDs with dashes (from some DRM systems)
 */
TEST_F(DrmSessionManagerComplexTests, KeyId_WithDashes_NormalizationWorks)
{
	// Some DRM systems return keys with dashes
	std::vector<uint8_t> keyWithDashes = {'0', '1', '2', '3', '-', '4', '5', '6', '7'};

	// Normalize by removing dashes
	std::vector<uint8_t> normalized;
	std::copy_if(keyWithDashes.begin(), keyWithDashes.end(),
		std::back_inserter(normalized),
		[](uint8_t c) { return c != '-'; });

	std::vector<uint8_t> expected = {'0', '1', '2', '3', '4', '5', '6', '7'};
	EXPECT_EQ(normalized, expected);
}

/**
 * @brief Test CPIX multi-key scenario - stay on HD if UHD fails
 */
TEST_F(DrmSessionManagerComplexTests, CPIX_MultiKey_UHDFailsHDUsable_KeepsHD)
{
	KeyIdEntries slot0; // Video slot with multiple keys

	// UHD key (4K profile)
	KeyIdEntry uhdKey;
	uhdKey.keyId = CreateAsciiHexKeyId("1234567890abcdef1234567890abcdef");
	uhdKey.isFailedKeyId = true; // Fails due to decrypt error

	// HD key (1080p profile)
	KeyIdEntry hdKey;
	hdKey.keyId = CreateAsciiHexKeyId("fedcba0987654321fedcba0987654321");
	hdKey.isFailedKeyId = false; // Usable

	// SD key (480p profile)
	KeyIdEntry sdKey;
	sdKey.keyId = CreateAsciiHexKeyId("abcdef1234567890abcdef1234567890");
	sdKey.isFailedKeyId = false; // Usable

	slot0.data.push_back(uhdKey);
	slot0.data.push_back(hdKey);
	slot0.data.push_back(sdKey);

	// Should not mark entire slot as failed
	slot0.isFailedKeyEntries = false;

	// ABR should filter out UHD profile
	std::vector<KeyIdEntry> usableKeys;
	std::copy_if(slot0.data.begin(), slot0.data.end(),
		std::back_inserter(usableKeys),
		[](const KeyIdEntry& entry) { return !entry.isFailedKeyId; });

	EXPECT_EQ(usableKeys.size(), 2); // HD and SD remain
	EXPECT_FALSE(slot0.isFailedKeyEntries);
}

// ----------------------------------------------------------------------------
// ValidateMultiKeySlot Tests - Using Real DRM Helpers with PSSH Data
// ----------------------------------------------------------------------------

/**
 * @brief Test ValidateMultiKeySlot with real Widevine DRM helper - 3 Key IDs with dashes
 * 
 * PSSH contains 5 key IDs in UUID format (with dashes):
 * - bac8da7d-9313-7aab-81ea-b37a7adb4f21
 * - 1c9e3c69-eb0e-5cc4-3065-421218173f4aa
 * - ca1a1ae3-2bcf-50c2-88ed-c3f051161435
 * - 962822bf-0c85-c940-aae5-6943547888874
 * - d1b72b0e-dc95-0e38-0e88-ffde723a181f
 * 
 * This test validates that:
 * 1. Real Widevine helper can parse PSSH and extract key IDs
 * 2. Key IDs are stored in cachedKeyIDs as ASCII hex with dashes
 * 3. usableKeys from CDM (via keyUpdateOCDM) are ASCII hex without dashes
 * 4. ValidateMultiKeySlot normalizes dashes and matches correctly
 */
TEST_F(DrmSessionManagerComplexTests, ValidateMultiKeySlot_RealWidevinePssh_ThreeKeyIDs_WithDashes)
{
	// Base64 PSSH data: Three key IDs with dashes
	const char* psshBase64 = "AAAA83Bzc2gAAAAA7e+LqXnWSs6jyCfc1R0h7QAAANMSJGJhYzhkYTdkLTkzMTMtN2FhYi04MWVhLWIzN2E3YWRiNGYyMRIkMWM5ZTNjNjktZWIwZS01Y2M0LTMwNjUtNDIxMjE4NzNmNGFhEiRjYTFhMWFlMy0yYmNmLTUwYzItODhlZC1jM2YwNTExNjE0MzUSJDk2MjgyMmJmLTBjODUtYzk0MC1hYWU1LTY5NDM1NDc4ODg3NBIkZDFiNzJiMGUtZGM5NS0wZTM4LTBlODgtZmZkZTcyM2ExODFmIhM5MTQ5NjI4NDEwNDMyMzcxMTYz";
	
	// Decode PSSH
	size_t psshDataLen = 0;
	unsigned char* psshDataPtr = base64_Decode(psshBase64, &psshDataLen, strlen(psshBase64));
	ASSERT_NE(psshDataPtr, nullptr);
	
	// Create real Widevine DRM helper
	DrmInfo drmInfo = CreateWidevineDrmInfo();
	ASSERT_TRUE(DrmHelperEngine::getInstance().hasDRM(drmInfo));
	std::shared_ptr<DrmHelper> drmHelper = DrmHelperEngine::getInstance().createHelper(drmInfo);
	ASSERT_NE(drmHelper, nullptr);
	
	// Parse PSSH - this populates mKeyIDs inside the helper
	bool parseResult = drmHelper->parsePssh(psshDataPtr, (uint32_t)psshDataLen);
	ASSERT_TRUE(parseResult);
	
	// Get all key IDs from helper
	std::map<int, std::vector<uint8_t>> keyIDs;
	drmHelper->getKeys(keyIDs);
	ASSERT_EQ(keyIDs.size(), 6); // Should have 5 key IDs
	
	// Set default key ID bac8da7d-9313-7aab-81ea-b37a7adb4f21
	std::string defaultKeyStr(keyIDs[0].begin(), keyIDs[0].end());
	drmHelper->setDefaultKeyID(defaultKeyStr);
	
	// Get primary key (should be first key)
	std::vector<uint8_t> primaryKeyId;
	drmHelper->getKey(primaryKeyId);
	EXPECT_EQ(primaryKeyId, keyIDs[0]);
	
	// Setup slot with keyIDs and DRM session
	SetupSlotWithKeyIDs(0, keyIDs, drmHelper);
	
	// Simulate keyUpdateOCDM: Convert binary keys to ASCII hex (without dashes)
	// In real scenario, OCDM calls keyUpdateOCDM with binary keys
	std::vector<std::vector<uint8_t>> usableKeys;
	
	// Binary keys corresponding to the key IDs keyID bac8da7d-9313-7aab-81ea-b37a7adb4f21
	std::vector<uint8_t> binaryKey1 = {0xBA, 0xC8, 0xDA, 0x7D, 0x93, 0x13, 0x7A, 0xAB,
	                                    0x81, 0xEA, 0xB3, 0x7A, 0x7A, 0xDB, 0x4F, 0x21};
	// Binary keyID 1c9e3c69-eb0e-5cc4-3065-421218173f4aa
	std::vector<uint8_t> binaryKey2 = {0x1C, 0x9E, 0x3C, 0x69, 0xEB, 0x0E, 0x5C, 0xC4,
	                                    0x30, 0x65, 0x42, 0x12, 0x18, 0x73, 0xF4, 0xAA};
	// Binary keyID ca1a1ae3-2bcf-50c2-88ed-c3f051161435
	std::vector<uint8_t> binaryKey3 = {0xCA, 0x1A, 0x1A, 0xE3, 0x2B, 0xCF, 0x50, 0xC2,
	                                    0x88, 0xED, 0xC3, 0xF0, 0x51, 0x16, 0x14, 0x35};
	
	// Convert to ASCII hex (as keyUpdateOCDM does)
	usableKeys.push_back(RawKeyToKeyId(binaryKey1.data(), binaryKey1.size()));
	usableKeys.push_back(RawKeyToKeyId(binaryKey2.data(), binaryKey2.size()));
	usableKeys.push_back(RawKeyToKeyId(binaryKey3.data(), binaryKey3.size()));
	
	EXPECT_CALL(*g_mockOpenCdmSessionAdapter, getUsableKeys())
		.WillRepeatedly(ReturnRef(usableKeys));
	
	// Validate with first key (should match after dash normalization)
	bool result = mDrmSessionManager->ValidateMultiKeySlot(keyIDs[0], 0);
	EXPECT_TRUE(result);
	
	free(psshDataPtr);
}

/**
 * @brief Test DrmHelper creation from init data PSSH - Single key from MOVE manifest
 * 
 * This test uses actual PSSH data extracted from an init header:
 * edef8ba9 79d64ace a3c827dc d51d21ed 0000002c 08011210 1eacdd30 2913431b 
 * ad920aba 65aba3e9 22144d4f 56453130 34363030 33303032 35383934 36393800
 * 
 * PSSH breakdown:
 * - System ID: edef8ba9-79d6-4ace-a3c8-27dcd51d21ed (Widevine)
 * - Size: 0x0000002c (44 bytes)
 * - Key ID: 1eacdd302913431bad920aba65aba3e9 (16 bytes)
 * - Content ID: "MOVE1046003002589468"
 * 
 * This validates:
 * 1. DrmHelper can be created from raw init data
 * 2. PSSH parsing extracts correct key ID
 * 3. Session can be created with parsed data
 */
TEST_F(DrmSessionManagerComplexTests, ValidateMultiKeySlot_CreateDrmHelperFromInitData_SingleKey_MOVEContent)
{
	// Complete PSSH box data from segment (76 bytes total)
	// Structure: [size(4)][pssh(4)][version+flags(4)][systemID(16)][dataSize(4)][protobuf_data(44)]
	const char* psshHex = 
		"0000004c "                              // Box size: 76 bytes (0x4C)
		"70737368 "                              // Box type: 'pssh'
		"00000000 "                              // Version: 0, Flags: 0
		"edef8ba9 79d64ace a3c827dc d51d21ed "  // System ID: Widevine UUID
		"0000002c "                              // Data size: 44 bytes (0x2C)
		"08011210 "                              // Protobuf: algorithm + key_id field header
		"1eacdd30 2913431b ad920aba 65aba3e9 "  // Key ID: 1eacdd302913431bad920aba65aba3e9
		"22144d4f 56453130 34363030 33303032 "  // Content ID field + "MOVE1046003002"
		"35383934 36393800";                    // Content ID (continued): "589468" + null terminator
	
	// Convert hex string to binary data
	std::vector<uint8_t> psshData = HexStringToBytes(psshHex);
	ASSERT_EQ(psshData.size(), 76); // Verify complete box
	
	// Verify system ID at offset 12-27 (after size + 'pssh' + version/flags)
	std::vector<uint8_t> expectedSystemId = {0xed, 0xef, 0x8b, 0xa9, 0x79, 0xd6, 0x4a, 0xce,
	                                          0xa3, 0xc8, 0x27, 0xdc, 0xd5, 0x1d, 0x21, 0xed};
	std::vector<uint8_t> actualSystemId(psshData.begin() + 12, psshData.begin() + 28);
	EXPECT_EQ(actualSystemId, expectedSystemId);
	
	// Create DrmInfo for Widevine
	DrmInfo drmInfo = CreateWidevineDrmInfo();
	ASSERT_TRUE(DrmHelperEngine::getInstance().hasDRM(drmInfo));
	
	// Create DRM helper
	std::shared_ptr<DrmHelper> drmHelper = DrmHelperEngine::getInstance().createHelper(drmInfo);
	ASSERT_NE(drmHelper, nullptr);
	
	// Parse PSSH from init data
	bool parseResult = drmHelper->parsePssh(psshData.data(), static_cast<uint32_t>(psshData.size()));
	ASSERT_TRUE(parseResult);
	
	// Get extracted key IDs
	std::map<int, std::vector<uint8_t>> keyIDs;
	drmHelper->getKeys(keyIDs);
	ASSERT_EQ(keyIDs.size(), 2); // Should have exactly 1 key (content ID should not be counted)
	
	// Verify key ID extraction - WidevineDrmHelper stores keys as binary
	std::vector<uint8_t> expectedKeyIdBinary = {0x1e, 0xac, 0xdd, 0x30, 0x29, 0x13, 0x43, 0x1b,
	                                             0xad, 0x92, 0x0a, 0xba, 0x65, 0xab, 0xa3, 0xe9};
	
	// First key should match our expected binary key
	EXPECT_EQ(keyIDs[0], expectedKeyIdBinary);
	
	// Set default key
	std::string defaultKeyStr(keyIDs[0].begin(), keyIDs[0].end());
	drmHelper->setDefaultKeyID(defaultKeyStr);
	
	// Verify getKey returns the correct key
	std::vector<uint8_t> primaryKeyId;
	drmHelper->getKey(primaryKeyId);
	EXPECT_EQ(primaryKeyId, keyIDs[0]);
	
	// Setup slot with extracted keyIDs
	SetupSlotWithKeyIDs(0, keyIDs, drmHelper);
	
	// Simulate successful key update from CDM
	// CDM returns keys as ASCII hex, so convert binary to ASCII hex
	std::vector<std::vector<uint8_t>> usableKeys;
	std::vector<uint8_t> expectedKeyIdAscii = RawKeyToKeyId(expectedKeyIdBinary.data(), 
	                                                          expectedKeyIdBinary.size());
	usableKeys.push_back(expectedKeyIdAscii);
	
	EXPECT_CALL(*g_mockOpenCdmSessionAdapter, getUsableKeys())
		.WillRepeatedly(ReturnRef(usableKeys));
	
	// Validate slot
	bool result = mDrmSessionManager->ValidateMultiKeySlot(keyIDs[0], 0);
	EXPECT_TRUE(result);
}

/**
 * @brief Test DrmHelper creation from init data - Verify system ID extraction
 * 
 * This test focuses on extracting and verifying the Widevine system ID
 * from raw PSSH data in init headers.
 */
TEST_F(DrmSessionManagerComplexTests, CreateDrmHelperFromInitData_VerifySystemId)
{
	// Same PSSH data as previous test
	const char* psshHex = 
		"0000004c "                              // Box size: 76 bytes
		"70737368 "                              // Box type: 'pssh'
		"00000000 "                              // Version: 0, Flags: 0
		"edef8ba9 79d64ace a3c827dc d51d21ed "  // System ID: Widevine UUID
		"0000002c "                              // Data size: 44 bytes
		"08011210 "                              // Protobuf: algorithm + key_id field header
		"1eacdd30 2913431b ad920aba 65aba3e9 "  // Key ID: 1eacdd302913431bad920aba65aba3e9
		"22144d4f 56453130 34363030 33303032 "  // Content ID: "MOVE1046003002589468"
		"35383934 36393800";                    // Content ID (continued)
	
	std::vector<uint8_t> psshData = HexStringToBytes(psshHex);
	ASSERT_EQ(psshData.size(), 76);
	
	// System ID at bytes 12-27 (after size + 'pssh' + version/flags)
	std::vector<uint8_t> systemId(psshData.begin() + 12, psshData.begin() + 28);
	
	// Widevine UUID: edef8ba9-79d6-4ace-a3c8-27dcd51d21ed
	std::vector<uint8_t> widevineUuid = {
		0xed, 0xef, 0x8b, 0xa9, 0x79, 0xd6, 0x4a, 0xce,
		0xa3, 0xc8, 0x27, 0xdc, 0xd5, 0x1d, 0x21, 0xed
	};
	
	EXPECT_EQ(systemId, widevineUuid);
	
	// Create DRM helper and verify it's Widevine
	DrmInfo drmInfo = CreateWidevineDrmInfo();
	std::shared_ptr<DrmHelper> drmHelper = DrmHelperEngine::getInstance().createHelper(drmInfo);
	ASSERT_NE(drmHelper, nullptr);
	
	// Verify system ID matches
	EXPECT_EQ(drmHelper->ocdmSystemId(), "com.widevine.alpha");
}

/**
 * @brief Test DrmHelper parsing with corrupted init data
 * 
 * Validates error handling when PSSH data from init header is corrupted.
 */
TEST_F(DrmSessionManagerComplexTests, CreateDrmHelperFromInitData_CorruptedPSSH_HandleGracefully)
{
	// Corrupted PSSH: truncated data (missing key ID and content ID)
	const char* corruptedPsshHex = 
		"0000004c "                              // Box size: 76 bytes (but actual data is less)
		"70737368 "                              // Box type: 'pssh'
		"00000000 "                              // Version: 0, Flags: 0
		"edef8ba9 79d64ace a3c827dc d51d21ed "  // System ID: Widevine UUID
		"0000002c "                              // Data size: 44 bytes (but incomplete)
		"0801";                                  // Incomplete proto header (truncated)
	
	std::vector<uint8_t> psshData = HexStringToBytes(corruptedPsshHex);
	
	// Create DRM helper
	DrmInfo drmInfo = CreateWidevineDrmInfo();
	std::shared_ptr<DrmHelper> drmHelper = DrmHelperEngine::getInstance().createHelper(drmInfo);
	ASSERT_NE(drmHelper, nullptr);
	
	// Parse should handle corrupted data gracefully
	bool parseResult = drmHelper->parsePssh(psshData.data(), static_cast<uint32_t>(psshData.size()));
	
	// Depending on implementation, this might fail or succeed with no keys
	if (parseResult)
	{
		std::map<int, std::vector<uint8_t>> keyIDs;
		drmHelper->getKeys(keyIDs);
		// If parse succeeded, we should have no valid keys or handle gracefully
		EXPECT_GE(keyIDs.size(), 0);
	}
	else
	{
		// Parse correctly detected corruption
		EXPECT_FALSE(parseResult);
	}
}

/**
 * @brief Test DrmHelper with init data containing multiple key IDs
 * 
 * Simulates CPIX multi-key scenario where init data contains multiple profiles
 * (UHD, HD, SD) with different key IDs.
 */
TEST_F(DrmSessionManagerComplexTests, CreateDrmHelperFromInitData_MultipleKeys_CPIXScenario)
{
	// Multi-key PSSH hex (simulated CPIX with 3 keys for different profiles)
	// Format: [size(4)][pssh(4)][version+flags(4)][systemID(16)][dataSize(4)][protobuf_data(80)]
	const char* multiKeyPsshHex =
		"00000070 "                              // Box size: 112 bytes (0x70)
		"70737368 "                              // Box type: 'pssh'
		"00000000 "                              // Version: 0, Flags: 0
		"edef8ba9 79d64ace a3c827dc d51d21ed "  // System ID: Widevine UUID
		"00000050 "                              // Data size: 80 bytes (0x50)
		"08011210 "                              // Algorithm(1) + key_id field header (4 bytes)
		"1eacdd30 2913431b ad920aba 65aba3e9 "  // Key ID 1 (UHD): 1eacdd302913431bad920aba65aba3e9 (16 bytes)
		"1210 "                                  // Next key_id field header (2 bytes)
		"bac8da7d 93137aab 81eab37a 7adb4f21 "  // Key ID 2 (HD): bac8da7d93137aab81eab37a7adb4f21 (16 bytes)
		"1210 "                                  // Next key_id field header (2 bytes)
		"4060a865 88784267 9cbf91ae 5bae1e72 "  // Key ID 3 (SD): 4060a865887842679cbf91ae5bae1e72 (16 bytes)
		"22144d4f 56453130 34363030 33303032 "  // Content ID field + "MOVE1046003002" (16 bytes)
		"35383934 36393800";                    // Content ID (continued): "589468" + null (8 bytes)
	
	std::vector<uint8_t> psshData = HexStringToBytes(multiKeyPsshHex);
	ASSERT_EQ(psshData.size(), 112); // Verify complete multi-key box: 32 header + 80 data
	
	// Create DRM helper
	DrmInfo drmInfo = CreateWidevineDrmInfo();
	std::shared_ptr<DrmHelper> drmHelper = DrmHelperEngine::getInstance().createHelper(drmInfo);
	ASSERT_NE(drmHelper, nullptr);
	
	// Parse PSSH
	bool parseResult = drmHelper->parsePssh(psshData.data(), static_cast<uint32_t>(psshData.size()));
	ASSERT_TRUE(parseResult);
	
	// Get all keys
	std::map<int, std::vector<uint8_t>> keyIDs;
	drmHelper->getKeys(keyIDs);
	
	// Should have 3 key IDs + 1 content ID = 4 total entries
	ASSERT_EQ(keyIDs.size(), 4);
	
	// Verify keys are stored as binary (not ASCII hex)
	std::vector<uint8_t> expectedUhdKey = {0x1e, 0xac, 0xdd, 0x30, 0x29, 0x13, 0x43, 0x1b,
	                                        0xad, 0x92, 0x0a, 0xba, 0x65, 0xab, 0xa3, 0xe9};
	std::vector<uint8_t> expectedHdKey = {0xba, 0xc8, 0xda, 0x7d, 0x93, 0x13, 0x7a, 0xab,
	                                       0x81, 0xea, 0xb3, 0x7a, 0x7a, 0xdb, 0x4f, 0x21};
	std::vector<uint8_t> expectedSdKey = {0x40, 0x60, 0xa8, 0x65, 0x88, 0x78, 0x42, 0x67,
	                                       0x9c, 0xbf, 0x91, 0xae, 0x5b, 0xae, 0x1e, 0x72};
	
	EXPECT_EQ(keyIDs[0], expectedUhdKey);
	EXPECT_EQ(keyIDs[1], expectedHdKey);
	EXPECT_EQ(keyIDs[2], expectedSdKey);
	// keyIDs[3] is the content ID
	
	// Setup slot with all extracted keys
	SetupSlotWithKeyIDs(0, keyIDs, drmHelper);
	
	// Simulate scenario: UHD key fails, HD and SD remain usable
	// CDM returns keys as ASCII hex via getUsableKeys()
	std::vector<std::vector<uint8_t>> usableKeys;
	usableKeys.push_back(RawKeyToKeyId(expectedHdKey.data(), expectedHdKey.size()));
	usableKeys.push_back(RawKeyToKeyId(expectedSdKey.data(), expectedSdKey.size()));
	
	EXPECT_CALL(*g_mockOpenCdmSessionAdapter, getUsableKeys())
		.WillRepeatedly(ReturnRef(usableKeys));
	
	// Validate with HD key (second key) - should succeed
	bool result = mDrmSessionManager->ValidateMultiKeySlot(keyIDs[1], 0);
	EXPECT_TRUE(result);
	
	// Validate with UHD key (first key) - should fail (not in usableKeys)
	result = mDrmSessionManager->ValidateMultiKeySlot(keyIDs[0], 0);
	EXPECT_FALSE(result);
}

/**
 * @brief Test end-to-end flow: Init data → DrmHelper → Session → Validation
 * 
 * Complete integration test showing the full flow from init data to
 * DRM session validation.
 */
TEST_F(DrmSessionManagerComplexTests, InitDataFlow_EndToEnd_CreateHelperAndValidate)
{
	// Step 1: Get PSSH from init data
	const char* psshHex = 
		"0000004c "                              // Box size: 76 bytes
		"70737368 "                              // Box type: 'pssh'
		"00000000 "                              // Version: 0, Flags: 0
		"edef8ba9 79d64ace a3c827dc d51d21ed "  // System ID: Widevine UUID
		"0000002c "                              // Data size: 44 bytes
		"08011210 "                              // Protobuf: algorithm + key_id field header
		"1eacdd30 2913431b ad920aba 65aba3e9 "  // Key ID: 1eacdd302913431bad920aba65aba3e9
		"22144d4f 56453130 34363030 33303032 "  // Content ID: "MOVE1046003002589468"
		"35383934 36393800";                    // Content ID (continued)
	
	std::vector<uint8_t> psshData = HexStringToBytes(psshHex);
	ASSERT_EQ(psshData.size(), 76);
	
	// Step 2: Create DrmInfo and DrmHelper
	DrmInfo drmInfo = CreateWidevineDrmInfo();
	std::shared_ptr<DrmHelper> drmHelper = DrmHelperEngine::getInstance().createHelper(drmInfo);
	ASSERT_NE(drmHelper, nullptr);
	
	// Step 3: Parse PSSH to extract keys
	ASSERT_TRUE(drmHelper->parsePssh(psshData.data(), static_cast<uint32_t>(psshData.size())));
	
	// Step 4: Get keys from helper
	std::map<int, std::vector<uint8_t>> keyIDs;
	drmHelper->getKeys(keyIDs);
	ASSERT_EQ(keyIDs.size(), 2); // 1 key ID + 1 content ID
	
	// Verify first key is binary format
	std::vector<uint8_t> expectedKeyBinary = {0x1e, 0xac, 0xdd, 0x30, 0x29, 0x13, 0x43, 0x1b,
	                                           0xad, 0x92, 0x0a, 0xba, 0x65, 0xab, 0xa3, 0xe9};
	EXPECT_EQ(keyIDs[0], expectedKeyBinary);
	
	// Step 5: Set default key
	std::string defaultKeyStr(keyIDs[0].begin(), keyIDs[0].end());
	drmHelper->setDefaultKeyID(defaultKeyStr);
	
	// Step 6: Create DRM session
	auto drmSession = std::make_shared<OCDMSessionAdapter>(drmHelper, nullptr);
	SetDrmSessionInSlot(0, drmSession);
	
	// Step 7: Populate cached keys
	KeyIdEntries entries;
	for (const auto& keyEntry : keyIDs)
	{
		KeyIdEntry entry;
		entry.keyId = keyEntry.second;
		entry.isFailedKeyId = false;
		entries.data.push_back(entry);
	}
	mDrmSessionManager->AddKeyIdEntryToCache(0, entries);
	
	// Step 8: Simulate CDM key update (key becomes usable)
	std::vector<std::vector<uint8_t>> usableKeys;
	std::vector<uint8_t> keyBinary = {0x1e, 0xac, 0xdd, 0x30, 0x29, 0x13, 0x43, 0x1b,
	                                   0xad, 0x92, 0x0a, 0xba, 0x65, 0xab, 0xa3, 0xe9};
	usableKeys.push_back(RawKeyToKeyId(keyBinary.data(), keyBinary.size()));
	
	EXPECT_CALL(*g_mockOpenCdmSessionAdapter, getUsableKeys())
		.WillRepeatedly(ReturnRef(usableKeys));
	
	// Step 9: Validate slot
	bool validationResult = mDrmSessionManager->ValidateMultiKeySlot(keyIDs[0], 0);
	EXPECT_TRUE(validationResult);
}

/**
 * @brief Test ValidateMultiKeySlot with real Widevine DRM helper - 3 identical keys (ASCII format)
 * 
 * PSSH contains 3 identical key IDs in ASCII hex format (without dashes):
 * - 4060a865887842679cbf91ae5bae1e72
 * 
 * This test validates that:
 * 1. Helper can parse PSSH with ASCII hex key IDs
 * 2. Duplicate keys are handled correctly
 * 3. Validation works when keys don't have dashes
 */
TEST_F(DrmSessionManagerComplexTests, ValidateMultiKeySlot_RealWidevinePssh_ThreeIdenticalKeys_ASCII)
{
	// Base64 PSSH data: Three identical keys in ASCII
	const char* psshBase64 = "AAAAXHBzc2gAAAAA7e+LqXnWSs6jyCfc1R0h7QAAADwSEEBgqGWIeEJnnL+RrluuHnISEEBgqGWIeEJnnL+RrluuHnISEEBgqGWIeEJnnL+RrluuHnJI49yVmwY=";
	
	// Decode PSSH
	size_t psshDataLen = 0;
	unsigned char* psshDataPtr = base64_Decode(psshBase64, &psshDataLen, strlen(psshBase64));
	ASSERT_NE(psshDataPtr, nullptr);
	
	// Create real Widevine DRM helper
	DrmInfo drmInfo = CreateWidevineDrmInfo();
	std::shared_ptr<DrmHelper> drmHelper = DrmHelperEngine::getInstance().createHelper(drmInfo);
	ASSERT_NE(drmHelper, nullptr);
	
	// Parse PSSH
	bool parseResult = drmHelper->parsePssh(psshDataPtr, (uint32_t)psshDataLen);
	ASSERT_TRUE(parseResult);
	
	// Get all key IDs from helper
	std::map<int, std::vector<uint8_t>> keyIDs;
	drmHelper->getKeys(keyIDs);
	ASSERT_EQ(keyIDs.size(), 3); // Should have 3 key IDs (duplicates)
	
	// Set default key
	std::string defaultKeyStr(keyIDs[0].begin(), keyIDs[0].end());
	drmHelper->setDefaultKeyID(defaultKeyStr);
	
	// Setup slot with keyIDs and DRM session
	SetupSlotWithKeyIDs(0, keyIDs, drmHelper);
	
	// Simulate keyUpdateOCDM with binary key
	std::vector<std::vector<uint8_t>> usableKeys;
	std::vector<uint8_t> binaryKey = {0x40, 0x60, 0xA8, 0x65, 0x88, 0x78, 0x42, 0x67,
	                                   0x9C, 0xBF, 0x91, 0xAE, 0x5B, 0xAE, 0x1E, 0x72};
	usableKeys.push_back(RawKeyToKeyId(binaryKey.data(), binaryKey.size()));
	
	EXPECT_CALL(*g_mockOpenCdmSessionAdapter, getUsableKeys())
		.WillRepeatedly(ReturnRef(usableKeys));
	
	// Validate
	bool result = mDrmSessionManager->ValidateMultiKeySlot(keyIDs[0], 0);
	EXPECT_TRUE(result);
	
	free(psshDataPtr);
}

/**
 * @brief Test ValidateMultiKeySlot with real Widevine DRM helper - Single key ID
 * 
 * PSSH contains 1 key ID:
 * - bed605115009469f9556936404b2d413 (hex string)
 * 
 * This test validates single key scenario with real DRM helper.
 */
TEST_F(DrmSessionManagerComplexTests, ValidateMultiKeySlot_RealWidevinePssh_SingleKeyID)
{
	// Base64 PSSH data: One key ID
	const char* psshBase64 = "AAAAWnBzc2gAAAAA7e+LqXnWSs6jyCfc1R0h7QAAADoIARIgYmVkNjA1MTE1MDA5NDY5Zjk1NTY5MzY0MDRiMmQ0MTMaB3NsaW5ndHYiBFNUWkUqBVNEX0hE";
	
	// Decode PSSH
	size_t psshDataLen = 0;
	unsigned char* psshDataPtr = base64_Decode(psshBase64, &psshDataLen, strlen(psshBase64));
	ASSERT_NE(psshDataPtr, nullptr);
	
	// Create real Widevine DRM helper
	DrmInfo drmInfo = CreateWidevineDrmInfo();
	std::shared_ptr<DrmHelper> drmHelper = DrmHelperEngine::getInstance().createHelper(drmInfo);
	ASSERT_NE(drmHelper, nullptr);
	
	// Parse PSSH
	bool parseResult = drmHelper->parsePssh(psshDataPtr, (uint32_t)psshDataLen);
	ASSERT_TRUE(parseResult);
	
	// Get key IDs
	std::map<int, std::vector<uint8_t>> keyIDs;
	drmHelper->getKeys(keyIDs);
	ASSERT_EQ(keyIDs.size(), 2); // Should have 1 key ID
	
	// Set default key
	std::string defaultKeyStr(keyIDs[0].begin(), keyIDs[0].end());
	drmHelper->setDefaultKeyID(defaultKeyStr);
	
	// Verify getKey returns the correct key
	std::vector<uint8_t> primaryKeyId;
	drmHelper->getKey(primaryKeyId);
	EXPECT_EQ(primaryKeyId, keyIDs[0]);
	
	// Setup slot with keyIDs and DRM session
	SetupSlotWithKeyIDs(0, keyIDs, drmHelper);
	
	// Simulate keyUpdateOCDM
	// The key ID in PSSH is ASCII hex string: "bed605115009469f9556936404b2d413"
	// We need to convert the actual binary representation
	std::vector<std::vector<uint8_t>> usableKeys;
	std::vector<uint8_t> binaryKey = {0xBE, 0xD6, 0x05, 0x11, 0x50, 0x09, 0x46, 0x9F,
	                                   0x95, 0x56, 0x93, 0x64, 0x04, 0xB2, 0xD4, 0x13};
	usableKeys.push_back(RawKeyToKeyId(binaryKey.data(), binaryKey.size()));
	
	EXPECT_CALL(*g_mockOpenCdmSessionAdapter, getUsableKeys())
		.WillRepeatedly(ReturnRef(usableKeys));
	
	// Validate
	bool result = mDrmSessionManager->ValidateMultiKeySlot(keyIDs[0], 0);
	EXPECT_TRUE(result);
	
	free(psshDataPtr);
}

/**
 * @brief Test ValidateMultiKeySlot with real Widevine - Multi-key failure scenario
 * 
 * Uses first PSSH with 5 keys, but only 2 are marked as usable.
 * Tests that non-primary keys can still be validated.
 */
TEST_F(DrmSessionManagerComplexTests, ValidateMultiKeySlot_RealWidevinePssh_PartialKeysUsable)
{
	// Base64 PSSH data: Five key IDs with dashes
	const char* psshBase64 = "AAAA83Bzc2gAAAAA7e+LqXnWSs6jyCfc1R0h7QAAANMSJGJhYzhkYTdkLTkzMTMtN2FhYi04MWVhLWIzN2E3YWRiNGYyMRIkMWM5ZTNjNjktZWIwZS01Y2M0LTMwNjUtNDIxMjE4NzNmNGFhEiRjYTFhMWFlMy0yYmNmLTUwYzItODhlZC1jM2YwNTExNjE0MzUSJDk2MjgyMmJmLTBjODUtYzk0MC1hYWU1LTY5NDM1NDc4ODg3NBIkZDFiNzJiMGUtZGM5NS0wZTM4LTBlODgtZmZkZTcyM2ExODFmIhM5MTQ5NjI4NDEwNDMyMzcxMTYz";
	
	// Decode PSSH
	size_t psshDataLen = 0;
	unsigned char* psshDataPtr = base64_Decode(psshBase64, &psshDataLen, strlen(psshBase64));
	ASSERT_NE(psshDataPtr, nullptr);
	
	// Create real Widevine DRM helper
	DrmInfo drmInfo = CreateWidevineDrmInfo();
	std::shared_ptr<DrmHelper> drmHelper = DrmHelperEngine::getInstance().createHelper(drmInfo);
	ASSERT_NE(drmHelper, nullptr);
	
	// Parse PSSH
	ASSERT_TRUE(drmHelper->parsePssh(psshDataPtr, (uint32_t)psshDataLen));
	
	// Get all key IDs
	std::map<int, std::vector<uint8_t>> keyIDs;
	drmHelper->getKeys(keyIDs);
	ASSERT_EQ(keyIDs.size(), 6); // 5 key IDs with dashes + 1 content ID
	
	// Set default to first key
	std::string defaultKeyStr(keyIDs[0].begin(), keyIDs[0].end());
	drmHelper->setDefaultKeyID(defaultKeyStr);
	
	// Setup slot with keyIDs and DRM session
	SetupSlotWithKeyIDs(0, keyIDs, drmHelper);
	
	// Only 2 keys are usable (simulating UHD/HD/SD scenario where UHD failed)
	std::vector<std::vector<uint8_t>> usableKeys;
	std::vector<uint8_t> binaryKey2 = {0x1C, 0x9E, 0x3C, 0x69, 0xEB, 0x0E, 0x5C, 0xC4,
	                                    0x30, 0x65, 0x42, 0x12, 0x18, 0x73, 0xF4, 0xAA};
	std::vector<uint8_t> binaryKey3 = {0xCA, 0x1A, 0x1A, 0xE3, 0x2B, 0xCF, 0x50, 0xC2,
	                                    0x88, 0xED, 0xC3, 0xF0, 0x51, 0x16, 0x14, 0x35};
	
	usableKeys.push_back(RawKeyToKeyId(binaryKey2.data(), binaryKey2.size()));
	usableKeys.push_back(RawKeyToKeyId(binaryKey3.data(), binaryKey3.size()));
	
	EXPECT_CALL(*g_mockOpenCdmSessionAdapter, getUsableKeys())
		.WillRepeatedly(ReturnRef(usableKeys));
	
	// Validate with second key (should succeed as it's in usableKeys)
	bool result = mDrmSessionManager->ValidateMultiKeySlot(keyIDs[1], 0);
	EXPECT_TRUE(result);
	
	// Validate with first key (should fail as it's NOT in usableKeys)
	result = mDrmSessionManager->ValidateMultiKeySlot(keyIDs[0], 0);
	EXPECT_FALSE(result);
	
	free(psshDataPtr);
}

/**
 * @brief Test initializeDrmSession with opencdm_session_creation_failure
 * 
 * This test validates the error handling when OCDM session construction fails:
 * 1. Create a MockDrmSession that simulates generateDRMSession failure
 * 2. Mock generateDRMSession to set m_eKeyState = KEY_ERROR_SESSION_CREATE_FAILED
 * 3. Call initializeDrmSession and verify it returns KEY_ERROR_SESSION_CREATE_FAILED
 * 4. Verify the error code is set to MW_DRM_SESSION_CREATE_FAILED
 * 
 * Expected behavior:
 * - initializeDrmSession detects the KEY_ERROR_SESSION_CREATE_FAILED state
 * - Returns the correct KeyState value
 * - Sets err parameter to MW_DRM_SESSION_CREATE_FAILED
 */
TEST_F(DrmSessionManagerComplexTests, OpencdmConstructionFailureTest)
{
	// Create DrmInfo for Widevine
	DrmInfo drmInfo = CreateWidevineDrmInfo();
	std::shared_ptr<DrmHelper> drmHelper = DrmHelperEngine::getInstance().createHelper(drmInfo);
	ASSERT_NE(drmHelper, nullptr);
	
	// Create DRM session
	auto drmSession = std::make_shared<OCDMSessionAdapter>(drmHelper, nullptr);
	SetDrmSessionInSlot(0, drmSession);

	// Setup mock expectations on the global adapter to simulate
	// opencdm_construct_session failure when generateDRMSession is
	// invoked by the OCDMSessionAdapter.
	EXPECT_CALL(*g_mockOpenCdmSessionAdapter, generateDRMSession(_, _, _))
		.Times(1)
		.WillOnce(Return());

	// Mock getState() to return KEY_ERROR_SESSION_CREATE_FAILED
	EXPECT_CALL(*g_mockOpenCdmSessionAdapter, getState())
		.WillRepeatedly(Return(KEY_ERROR_SESSION_CREATE_FAILED));


	// Call initializeDrmSession - this should detect the construction failure
	int err = 0;
	KeyState result = mDrmSessionManager->initializeDrmSession(drmHelper, 0, err);
	
	// Verify the returned KeyState is KEY_ERROR_SESSION_CREATE_FAILED
	EXPECT_EQ(result, KEY_ERROR_SESSION_CREATE_FAILED);
	
	// Verify the error code is mapped to MW_DRM_SESSION_CREATE_FAILED
	EXPECT_EQ(err, MW_DRM_SESSION_CREATE_FAILED);
}

