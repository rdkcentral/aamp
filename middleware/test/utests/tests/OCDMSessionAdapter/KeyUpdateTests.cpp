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

#include "opencdmsessionadapter.h"
#include "MockDrmHelper.h"
#include "MockOpenCdm.h"
#include "PlayerUtils.h"
#include <cstring>

using ::testing::NiceMock;
using ::testing::Return;
using ::testing::ReturnRef;
using ::testing::_;

// For comparing memory buffers such as C-style arrays
MATCHER_P2(MemBufEq, buffer, elementCount, "")
{
	return std::memcmp(arg, buffer, elementCount * sizeof(buffer[0])) == 0;
}

/**
 * @brief Test fixture for OCDMSessionAdapter key update functionality
 */
class OCDMSessionAdapterKeyUpdateTests : public ::testing::Test
{
protected:
	OCDMSessionAdapter *m_ocdmsessionadapter = nullptr;
	struct OpenCDMSystem *ocdmSystem = (OpenCDMSystem *)0x1234;
	std::string systemId = "com.widevine.alpha";
	std::shared_ptr<MockDrmHelper> drmHelper;

	void SetUp() override
	{
		drmHelper = std::make_shared<MockDrmHelper>();
		g_mockopencdm = new NiceMock<MockOpenCdm>();

		EXPECT_CALL(*drmHelper, keyProcessTimeout()).WillRepeatedly(Return(5000U));
		EXPECT_CALL(*drmHelper, ocdmSystemId()).WillOnce(ReturnRef(systemId));
		EXPECT_CALL(*g_mockopencdm, opencdm_create_system(MemBufEq(systemId.c_str(), systemId.length() + 1)))
			.WillOnce(Return(ocdmSystem));

		m_ocdmsessionadapter = new OCDMSessionAdapter(drmHelper, nullptr);
	}

	void TearDown() override
	{
		delete m_ocdmsessionadapter;
		m_ocdmsessionadapter = nullptr;

		delete g_mockopencdm;
		g_mockopencdm = nullptr;

		drmHelper = nullptr;
	}
};

/**
 * @brief Test keyUpdateOCDM with single key - usable keys list updated
 */
TEST_F(OCDMSessionAdapterKeyUpdateTests, KeyUpdateOCDM_SingleKey_StoresUsableKey)
{
	const uint8_t key[] = {0x01, 0x23, 0x45, 0x67, 0x89, 0xAB, 0xCD, 0xEF};
	const uint8_t keySize = sizeof(key);

	// Trigger key update - simulates OCDM callback
	m_ocdmsessionadapter->keyUpdateOCDM(key, keySize);

	// Verify the key was stored
	const auto& usableKeys = m_ocdmsessionadapter->getUsableKeys();
	EXPECT_EQ(usableKeys.size(), 1);

	// Verify the key ID format (should be hex string)
	std::vector<uint8_t> expectedKeyId = RawKeyToKeyId(key, keySize);
	EXPECT_EQ(usableKeys[0], expectedKeyId);
}

/**
 * @brief Test keyUpdateOCDM with multiple different keys
 */
TEST_F(OCDMSessionAdapterKeyUpdateTests, KeyUpdateOCDM_MultipleKeys_StoresAllKeys)
{
	const uint8_t key1[] = {0x01, 0x23, 0x45, 0x67};
	const uint8_t key2[] = {0x89, 0xAB, 0xCD, 0xEF};
	const uint8_t key3[] = {0xFE, 0xDC, 0xBA, 0x98};

	// Add multiple keys
	m_ocdmsessionadapter->keyUpdateOCDM(key1, sizeof(key1));
	m_ocdmsessionadapter->keyUpdateOCDM(key2, sizeof(key2));
	m_ocdmsessionadapter->keyUpdateOCDM(key3, sizeof(key3));

	// Verify all keys were stored
	const auto& usableKeys = m_ocdmsessionadapter->getUsableKeys();
	EXPECT_EQ(usableKeys.size(), 3);

	// Verify each key
	std::vector<uint8_t> expectedKeyId1 = RawKeyToKeyId(key1, sizeof(key1));
	std::vector<uint8_t> expectedKeyId2 = RawKeyToKeyId(key2, sizeof(key2));
	std::vector<uint8_t> expectedKeyId3 = RawKeyToKeyId(key3, sizeof(key3));

	EXPECT_EQ(usableKeys[0], expectedKeyId1);
	EXPECT_EQ(usableKeys[1], expectedKeyId2);
	EXPECT_EQ(usableKeys[2], expectedKeyId3);
}

/**
 * @brief Test keyUpdateOCDM with duplicate keys - should not add duplicates
 */
TEST_F(OCDMSessionAdapterKeyUpdateTests, KeyUpdateOCDM_DuplicateKeys_NoDuplicatesStored)
{
	const uint8_t key[] = {0x01, 0x23, 0x45, 0x67};

	// Add the same key multiple times
	m_ocdmsessionadapter->keyUpdateOCDM(key, sizeof(key));
	m_ocdmsessionadapter->keyUpdateOCDM(key, sizeof(key));
	m_ocdmsessionadapter->keyUpdateOCDM(key, sizeof(key));

	// Verify only one key was stored
	const auto& usableKeys = m_ocdmsessionadapter->getUsableKeys();
	EXPECT_EQ(usableKeys.size(), 1);

	std::vector<uint8_t> expectedKeyId = RawKeyToKeyId(key, sizeof(key));
	EXPECT_EQ(usableKeys[0], expectedKeyId);
}

/**
 * @brief Test keyUpdateOCDM with 16-byte key ID (typical DRM key size)
 */
TEST_F(OCDMSessionAdapterKeyUpdateTests, KeyUpdateOCDM_16ByteKey_StoresCorrectly)
{
	const uint8_t key[] = {
		0x12, 0x34, 0x56, 0x78, 0x9A, 0xBC, 0xDE, 0xF0,
		0x01, 0x23, 0x45, 0x67, 0x89, 0xAB, 0xCD, 0xEF
	};

	m_ocdmsessionadapter->keyUpdateOCDM(key, sizeof(key));

	const auto& usableKeys = m_ocdmsessionadapter->getUsableKeys();
	EXPECT_EQ(usableKeys.size(), 1);

	// Should be converted to 32-character hex string
	std::vector<uint8_t> expectedKeyId = RawKeyToKeyId(key, sizeof(key));
	EXPECT_EQ(usableKeys[0].size(), 32);
	EXPECT_EQ(usableKeys[0], expectedKeyId);
}

/**
 * @brief Test getUsableKeys when no keys have been added
 */
TEST_F(OCDMSessionAdapterKeyUpdateTests, GetUsableKeys_NoKeys_ReturnsEmptyVector)
{
	const auto& usableKeys = m_ocdmsessionadapter->getUsableKeys();
	EXPECT_TRUE(usableKeys.empty());
	EXPECT_EQ(usableKeys.size(), 0);
}

/**
 * @brief Test clearDecryptContext clears usable keys
 */
TEST_F(OCDMSessionAdapterKeyUpdateTests, ClearDecryptContext_WithKeys_ClearsUsableKeys)
{
	const uint8_t key1[] = {0x01, 0x23, 0x45, 0x67};
	const uint8_t key2[] = {0x89, 0xAB, 0xCD, 0xEF};

	// Add keys
	m_ocdmsessionadapter->keyUpdateOCDM(key1, sizeof(key1));
	m_ocdmsessionadapter->keyUpdateOCDM(key2, sizeof(key2));

	// Verify keys were added
	EXPECT_EQ(m_ocdmsessionadapter->getUsableKeys().size(), 2);

	// Clear the decrypt context
	m_ocdmsessionadapter->clearDecryptContext();

	// Verify keys were cleared
	const auto& usableKeys = m_ocdmsessionadapter->getUsableKeys();
	EXPECT_TRUE(usableKeys.empty());
	EXPECT_EQ(usableKeys.size(), 0);
}

/**
 * @brief Test keyUpdateOCDM with zero-length key
 */
TEST_F(OCDMSessionAdapterKeyUpdateTests, KeyUpdateOCDM_ZeroLengthKey_NoKeyStored)
{
	const uint8_t key[] = {0x01};

	// Try to add a zero-length key
	m_ocdmsessionadapter->keyUpdateOCDM(key, 0);

	// Since keySize is 0, no key should be added
	const auto& usableKeys = m_ocdmsessionadapter->getUsableKeys();
	EXPECT_TRUE(usableKeys.empty());
}

/**
 * @brief Test keyUpdateOCDM with nullptr
 */
TEST_F(OCDMSessionAdapterKeyUpdateTests, KeyUpdateOCDM_NullKey_NoKeyStored)
{
	// Try to add a null key
	m_ocdmsessionadapter->keyUpdateOCDM(nullptr, 10);

	// Since RawKeyToKeyId returns empty vector for nullptr, no key should be added
	const auto& usableKeys = m_ocdmsessionadapter->getUsableKeys();
	EXPECT_TRUE(usableKeys.empty());
}

/**
 * @brief Test keyUpdateOCDM followed by successful state - keys retained
 */
TEST_F(OCDMSessionAdapterKeyUpdateTests, KeyUpdateOCDM_AfterSuccessState_KeysRetained)
{
	const uint8_t key[] = {0x01, 0x23, 0x45, 0x67};

	// Add key
	m_ocdmsessionadapter->keyUpdateOCDM(key, sizeof(key));
	EXPECT_EQ(m_ocdmsessionadapter->getUsableKeys().size(), 1);

	// Verify keys are retained (not cleared by state changes unless explicit clear)
	const auto& usableKeys = m_ocdmsessionadapter->getUsableKeys();
	EXPECT_EQ(usableKeys.size(), 1);
}

/**
 * @brief Test multiple keys with mixed duplicates
 */
TEST_F(OCDMSessionAdapterKeyUpdateTests, KeyUpdateOCDM_MixedDuplicates_OnlyUniquesStored)
{
	const uint8_t key1[] = {0x01, 0x23};
	const uint8_t key2[] = {0x45, 0x67};
	const uint8_t key3[] = {0x01, 0x23}; // Duplicate of key1

	m_ocdmsessionadapter->keyUpdateOCDM(key1, sizeof(key1));
	m_ocdmsessionadapter->keyUpdateOCDM(key2, sizeof(key2));
	m_ocdmsessionadapter->keyUpdateOCDM(key3, sizeof(key3));

	// Should only have 2 unique keys
	const auto& usableKeys = m_ocdmsessionadapter->getUsableKeys();
	EXPECT_EQ(usableKeys.size(), 2);
}

/**
 * @brief Test keyUpdateOCDM with all zero key
 */
TEST_F(OCDMSessionAdapterKeyUpdateTests, KeyUpdateOCDM_AllZeroKey_StoresCorrectly)
{
	const uint8_t key[] = {0x00, 0x00, 0x00, 0x00};

	m_ocdmsessionadapter->keyUpdateOCDM(key, sizeof(key));

	const auto& usableKeys = m_ocdmsessionadapter->getUsableKeys();
	EXPECT_EQ(usableKeys.size(), 1);

	// Should be "00000000"
	std::vector<uint8_t> expected = {'0', '0', '0', '0', '0', '0', '0', '0'};
	EXPECT_EQ(usableKeys[0], expected);
}

/**
 * @brief Test keyUpdateOCDM with all 0xFF key
 */
TEST_F(OCDMSessionAdapterKeyUpdateTests, KeyUpdateOCDM_AllFFKey_StoresCorrectly)
{
	const uint8_t key[] = {0xFF, 0xFF, 0xFF, 0xFF};

	m_ocdmsessionadapter->keyUpdateOCDM(key, sizeof(key));

	const auto& usableKeys = m_ocdmsessionadapter->getUsableKeys();
	EXPECT_EQ(usableKeys.size(), 1);

	// Should be "ffffffff"
	std::vector<uint8_t> expected = {'f', 'f', 'f', 'f', 'f', 'f', 'f', 'f'};
	EXPECT_EQ(usableKeys[0], expected);
}
