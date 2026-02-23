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
#include <cstdint>
#include "PlayerUtils.h"

using ::testing::_;
using ::testing::Return;

/**
 * @brief Test fixture for PlayerUtils functionality
 */
class PlayerUtilsTests : public ::testing::Test
{
protected:
	void SetUp() override
	{
	}

	void TearDown() override
	{
	}
};

/**
 * @brief Test RawKeyToKeyId with valid key data
 */
TEST_F(PlayerUtilsTests, RawKeyToKeyId_ValidKey_ReturnsHexString)
{
	// Test with a simple key
	const uint8_t key[] = {0x01, 0x23, 0x45, 0x67, 0x89, 0xAB, 0xCD, 0xEF};
	const size_t keySize = sizeof(key);

	std::vector<uint8_t> result = RawKeyToKeyId(key, keySize);

	// Expected: each byte becomes two hex characters
	EXPECT_EQ(result.size(), keySize * 2);

	// Verify the hex encoding is correct
	std::vector<uint8_t> expected = {
		'0', '1', '2', '3', '4', '5', '6', '7',
		'8', '9', 'a', 'b', 'c', 'd', 'e', 'f'
	};
	EXPECT_EQ(result, expected);
}

/**
 * @brief Test RawKeyToKeyId with all zeros
 */
TEST_F(PlayerUtilsTests, RawKeyToKeyId_AllZeros_ReturnsAllZeroHexString)
{
	const uint8_t key[] = {0x00, 0x00, 0x00, 0x00};
	const size_t keySize = sizeof(key);

	std::vector<uint8_t> result = RawKeyToKeyId(key, keySize);

	EXPECT_EQ(result.size(), keySize * 2);

	// All zeros should become '0' characters
	std::vector<uint8_t> expected = {'0', '0', '0', '0', '0', '0', '0', '0'};
	EXPECT_EQ(result, expected);
}

/**
 * @brief Test RawKeyToKeyId with all 0xFF
 */
TEST_F(PlayerUtilsTests, RawKeyToKeyId_AllOnes_ReturnsFFHexString)
{
	const uint8_t key[] = {0xFF, 0xFF, 0xFF, 0xFF};
	const size_t keySize = sizeof(key);

	std::vector<uint8_t> result = RawKeyToKeyId(key, keySize);

	EXPECT_EQ(result.size(), keySize * 2);

	// All 0xFF should become 'ff' characters
	std::vector<uint8_t> expected = {'f', 'f', 'f', 'f', 'f', 'f', 'f', 'f'};
	EXPECT_EQ(result, expected);
}

/**
 * @brief Test RawKeyToKeyId with mixed upper and lower nibbles
 */
TEST_F(PlayerUtilsTests, RawKeyToKeyId_MixedNibbles_ReturnsCorrectHexString)
{
	const uint8_t key[] = {0xA5, 0x5A, 0x0F, 0xF0};
	const size_t keySize = sizeof(key);

	std::vector<uint8_t> result = RawKeyToKeyId(key, keySize);

	EXPECT_EQ(result.size(), keySize * 2);

	std::vector<uint8_t> expected = {'a', '5', '5', 'a', '0', 'f', 'f', '0'};
	EXPECT_EQ(result, expected);
}

/**
 * @brief Test RawKeyToKeyId with single byte
 */
TEST_F(PlayerUtilsTests, RawKeyToKeyId_SingleByte_ReturnsTwoCharHexString)
{
	const uint8_t key[] = {0xAB};
	const size_t keySize = sizeof(key);

	std::vector<uint8_t> result = RawKeyToKeyId(key, keySize);

	EXPECT_EQ(result.size(), 2);

	std::vector<uint8_t> expected = {'a', 'b'};
	EXPECT_EQ(result, expected);
}

/**
 * @brief Test RawKeyToKeyId with nullptr
 */
TEST_F(PlayerUtilsTests, RawKeyToKeyId_NullPointer_ReturnsEmptyVector)
{
	std::vector<uint8_t> result = RawKeyToKeyId(nullptr, 10);

	EXPECT_TRUE(result.empty());
	EXPECT_EQ(result.size(), 0);
}

/**
 * @brief Test RawKeyToKeyId with zero size
 */
TEST_F(PlayerUtilsTests, RawKeyToKeyId_ZeroSize_ReturnsEmptyVector)
{
	const uint8_t key[] = {0x01, 0x02, 0x03};

	std::vector<uint8_t> result = RawKeyToKeyId(key, 0);

	EXPECT_TRUE(result.empty());
	EXPECT_EQ(result.size(), 0);
}

/**
 * @brief Test RawKeyToKeyId with typical 16-byte key ID
 */
TEST_F(PlayerUtilsTests, RawKeyToKeyId_TypicalKeyID_ReturnsCorrectHexString)
{
	// Typical 16-byte key ID
	const uint8_t key[] = {
		0x12, 0x34, 0x56, 0x78, 0x9A, 0xBC, 0xDE, 0xF0,
		0x01, 0x23, 0x45, 0x67, 0x89, 0xAB, 0xCD, 0xEF
	};
	const size_t keySize = sizeof(key);

	std::vector<uint8_t> result = RawKeyToKeyId(key, keySize);

	EXPECT_EQ(result.size(), 32); // 16 bytes * 2 chars per byte

	std::vector<uint8_t> expected = {
		'1', '2', '3', '4', '5', '6', '7', '8',
		'9', 'a', 'b', 'c', 'd', 'e', 'f', '0',
		'0', '1', '2', '3', '4', '5', '6', '7',
		'8', '9', 'a', 'b', 'c', 'd', 'e', 'f'
	};
	EXPECT_EQ(result, expected);
}

/**
 * @brief Test RawKeyToKeyId with boundary values (0x00-0x0F and 0xF0-0xFF)
 */
TEST_F(PlayerUtilsTests, RawKeyToKeyId_BoundaryValues_ReturnsCorrectHex)
{
	const uint8_t key[] = {0x00, 0x01, 0x0E, 0x0F, 0x10, 0xF0, 0xFE, 0xFF};
	const size_t keySize = sizeof(key);

	std::vector<uint8_t> result = RawKeyToKeyId(key, keySize);

	EXPECT_EQ(result.size(), 16);
	std::vector<uint8_t> expected = {
		'0', '0', '0', '1', '0', 'e', '0', 'f',
		'1', '0', 'f', '0', 'f', 'e', 'f', 'f'
	};
	EXPECT_EQ(result, expected);
}

/**
 * @brief Test RawKeyToKeyId hex output is lowercase
 */
TEST_F(PlayerUtilsTests, RawKeyToKeyId_HexOutput_IsLowercase)
{
	const uint8_t key[] = {0xAB, 0xCD, 0xEF};
	const size_t keySize = sizeof(key);

	std::vector<uint8_t> result = RawKeyToKeyId(key, keySize);

	// Check that all hex letters are lowercase
	for (auto c : result)
	{
		if (c >= 'a' && c <= 'f')
		{
			EXPECT_GE(c, 'a');
			EXPECT_LE(c, 'f');
		}
		else
		{
			EXPECT_GE(c, '0');
			EXPECT_LE(c, '9');
		}
	}
}

/**
 * @brief Test RawKeyToKeyId with alternating pattern
 */
TEST_F(PlayerUtilsTests, RawKeyToKeyId_AlternatingPattern_ReturnsCorrectHex)
{
	const uint8_t key[] = {0xAA, 0x55, 0xAA, 0x55};
	const size_t keySize = sizeof(key);

	std::vector<uint8_t> result = RawKeyToKeyId(key, keySize);

	EXPECT_EQ(result.size(), 8);
	std::vector<uint8_t> expected = {'a', 'a', '5', '5', 'a', 'a', '5', '5'};
	EXPECT_EQ(result, expected);
}

/**
 * @brief Test RawKeyToKeyId with Widevine-style 16-byte key ID
 */
TEST_F(PlayerUtilsTests, RawKeyToKeyId_WidevineKeyID_ReturnsCorrectFormat)
{
	// Typical Widevine key ID format
	const uint8_t key[] = {
		0x10, 0x77, 0x15, 0x58, 0x5A, 0x65, 0x48, 0x37,
		0xA0, 0x20, 0xDF, 0xFE, 0xAB, 0xCD, 0x12, 0x34
	};
	const size_t keySize = sizeof(key);

	std::vector<uint8_t> result = RawKeyToKeyId(key, keySize);

	EXPECT_EQ(result.size(), 32);
	std::vector<uint8_t> expected = {
		'1', '0', '7', '7', '1', '5', '5', '8',
		'5', 'a', '6', '5', '4', '8', '3', '7',
		'a', '0', '2', '0', 'd', 'f', 'f', 'e',
		'a', 'b', 'c', 'd', '1', '2', '3', '4'
	};
	EXPECT_EQ(result, expected);
}

/**
 * @brief Test RawKeyToKeyId with PlayReady-style key ID
 */
TEST_F(PlayerUtilsTests, RawKeyToKeyId_PlayReadyKeyID_ReturnsCorrectFormat)
{
	// Typical PlayReady key ID
	const uint8_t key[] = {
		0xB0, 0x3F, 0xDA, 0x97, 0xF6, 0x6E, 0x4D, 0x28,
		0x9B, 0x5E, 0xFF, 0x8B, 0x9C, 0x83, 0xD8, 0x67
	};
	const size_t keySize = sizeof(key);

	std::vector<uint8_t> result = RawKeyToKeyId(key, keySize);

	EXPECT_EQ(result.size(), 32);
	std::vector<uint8_t> expected = {
		'b', '0', '3', 'f', 'd', 'a', '9', '7',
		'f', '6', '6', 'e', '4', 'd', '2', '8',
		'9', 'b', '5', 'e', 'f', 'f', '8', 'b',
		'9', 'c', '8', '3', 'd', '8', '6', '7'
	};
	EXPECT_EQ(result, expected);
}

/**
 * @brief Test RawKeyToKeyId with very large key (32 bytes)
 */
TEST_F(PlayerUtilsTests, RawKeyToKeyId_LargeKey32Bytes_ReturnsCorrectLength)
{
	const uint8_t key[32] = {
		0x00, 0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77,
		0x88, 0x99, 0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF,
		0xFF, 0xEE, 0xDD, 0xCC, 0xBB, 0xAA, 0x99, 0x88,
		0x77, 0x66, 0x55, 0x44, 0x33, 0x22, 0x11, 0x00
	};
	const size_t keySize = sizeof(key);

	std::vector<uint8_t> result = RawKeyToKeyId(key, keySize);

	EXPECT_EQ(result.size(), 64); // 32 bytes * 2
	
	// Verify first few and last few bytes
	EXPECT_EQ(result[0], '0');
	EXPECT_EQ(result[1], '0');
	EXPECT_EQ(result[2], '1');
	EXPECT_EQ(result[3], '1');
	EXPECT_EQ(result[62], '0');
	EXPECT_EQ(result[63], '0');
}

/**
 * @brief Test RawKeyToKeyId with ClearKey format
 */
TEST_F(PlayerUtilsTests, RawKeyToKeyId_ClearKeyFormat_ReturnsCorrectHex)
{
	// ClearKey typically uses 16-byte key IDs
	const uint8_t key[] = {
		0xEB, 0xDD, 0x62, 0xF1, 0x68, 0x14, 0xD2, 0x7B,
		0x68, 0xEF, 0x12, 0x2A, 0xFC, 0xE4, 0xAE, 0x3C
	};
	const size_t keySize = sizeof(key);

	std::vector<uint8_t> result = RawKeyToKeyId(key, keySize);

	EXPECT_EQ(result.size(), 32);
	
	// Verify key ID format is correct lowercase hex
	std::vector<uint8_t> expected = {
		'e', 'b', 'd', 'd', '6', '2', 'f', '1',
		'6', '8', '1', '4', 'd', '2', '7', 'b',
		'6', '8', 'e', 'f', '1', '2', '2', 'a',
		'f', 'c', 'e', '4', 'a', 'e', '3', 'c'
	};
	EXPECT_EQ(result, expected);
}

/**
 * @brief Test RawKeyToKeyId output contains only valid hex characters
 */
TEST_F(PlayerUtilsTests, RawKeyToKeyId_OutputValidation_OnlyHexCharacters)
{
	const uint8_t key[] = {0x00, 0x55, 0xAA, 0xFF, 0x12, 0x34, 0x56, 0x78};
	const size_t keySize = sizeof(key);

	std::vector<uint8_t> result = RawKeyToKeyId(key, keySize);

	// Verify all characters are valid hex (0-9, a-f)
	for (auto c : result)
	{
		bool isValidHex = (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f');
		EXPECT_TRUE(isValidHex) << "Character '" << static_cast<char>(c) << "' is not valid hex";
	}
}

/**
 * @brief Test RawKeyToKeyId with DRM multi-key scenario
 */
TEST_F(PlayerUtilsTests, RawKeyToKeyId_MultiKeyScenario_DifferentKeys)
{
	// Simulate multiple keys in multi-key DRM scenario
	const uint8_t key1[] = {0x01, 0x02, 0x03, 0x04};
	const uint8_t key2[] = {0xFF, 0xFE, 0xFD, 0xFC};
	
	std::vector<uint8_t> result1 = RawKeyToKeyId(key1, sizeof(key1));
	std::vector<uint8_t> result2 = RawKeyToKeyId(key2, sizeof(key2));

	// Ensure different keys produce different key IDs
	EXPECT_NE(result1, result2);
	
	// Verify both are correct
	std::vector<uint8_t> expected1 = {'0', '1', '0', '2', '0', '3', '0', '4'};
	std::vector<uint8_t> expected2 = {'f', 'f', 'f', 'e', 'f', 'd', 'f', 'c'};
	
	EXPECT_EQ(result1, expected1);
	EXPECT_EQ(result2, expected2);
}

/**
 * @brief Test RawKeyToKeyId maintains byte order (big-endian representation)
 */
TEST_F(PlayerUtilsTests, RawKeyToKeyId_ByteOrder_PreservedCorrectly)
{
	const uint8_t key[] = {0x12, 0x34, 0x56, 0x78};
	const size_t keySize = sizeof(key);

	std::vector<uint8_t> result = RawKeyToKeyId(key, keySize);

	// First byte (0x12) should become '1' '2'
	EXPECT_EQ(result[0], '1');
	EXPECT_EQ(result[1], '2');
	// Second byte (0x34) should become '3' '4'
	EXPECT_EQ(result[2], '3');
	EXPECT_EQ(result[3], '4');
	// Third byte (0x56) should become '5' '6'
	EXPECT_EQ(result[4], '5');
	EXPECT_EQ(result[5], '6');
	// Fourth byte (0x78) should become '7' '8'
	EXPECT_EQ(result[6], '7');
	EXPECT_EQ(result[7], '8');
}
