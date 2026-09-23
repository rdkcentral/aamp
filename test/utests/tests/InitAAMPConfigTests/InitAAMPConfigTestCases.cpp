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

/**
 * @file InitAAMPConfigTestCases.cpp
 * @brief L1 unit tests for PlayerInstanceAAMP::InitAAMPConfig() method
 * 
 * Tests the smart auto-detection feature that allows InitAAMPConfig to accept
 * both JSON and config string formats.
 */

#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include "main_aamp.h"
#include "AampConfig.h"

/**
 * @brief Test fixture for InitAAMPConfig tests
 */
class InitAAMPConfigTest : public ::testing::Test
{
protected:
	PlayerInstanceAAMP *player;

	void SetUp() override
	{
		player = new PlayerInstanceAAMP(nullptr);
	}

	void TearDown() override
	{
		delete player;
		player = nullptr;
	}
};

/**
 * @brief Test InitAAMPConfig with empty string
 * Empty string should return false without processing
 */
TEST_F(InitAAMPConfigTest, EmptyString_ReturnsFalse)
{
	bool result = player->InitAAMPConfig("");

	EXPECT_FALSE(result) << "InitAAMPConfig should return false for empty string";
}

/**
 * @brief Test InitAAMPConfig with NULL pointer
 * NULL pointer should return false without crashing
 */
TEST_F(InitAAMPConfigTest, NullPointer_ReturnsFalse)
{
	bool result = player->InitAAMPConfig(nullptr);

	EXPECT_FALSE(result) << "InitAAMPConfig should return false for NULL pointer";
}

/**
 * @brief Test InitAAMPConfig with whitespace-only string
 * Whitespace-only string should return false
 */
TEST_F(InitAAMPConfigTest, WhitespaceOnly_ReturnsFalse)
{
	bool result = player->InitAAMPConfig("   ");

	EXPECT_FALSE(result) << "InitAAMPConfig should return false for whitespace-only string";
}

/**
 * @brief Test InitAAMPConfig with junk/invalid values
 * Invalid config strings should return false
 */
TEST_F(InitAAMPConfigTest, JunkValue_ProcessedAsConfigString)
{
	const char* junkValues[] = {
		"random_junk_text",
		"12345",
		"!@#$%^&*()",
		"key_without_equals",
		"=value_without_key"
	};

	for (const char* junk : junkValues)
	{
		bool result = player->InitAAMPConfig(junk);
		// ProcessConfigText will process these but may not find valid config
		EXPECT_FALSE(result) << "InitAAMPConfig should not handle junk input: " << junk;
	}
}

/**
 * @brief Test InitAAMPConfig with invalid JSON
 * Invalid JSON should return false
 */
TEST_F(InitAAMPConfigTest, InvalidJSON_ReturnsFalse)
{
	const char* invalidJson[] = {
		"{invalid json}",
		"{\"key\": }",
		"{\"key\": \"value\"",  // Missing closing brace
		"[1, 2, 3",              // Missing closing bracket
		"{key: value}"           // Unquoted key
	};

	for (const char* json : invalidJson)
	{
		bool result = player->InitAAMPConfig(json);
		// These will be processed as config strings after JSON parse fails
		EXPECT_FALSE(result) << "InitAAMPConfig should not handle invalid JSON: " << json;
	}
}

/**
 * @brief Test InitAAMPConfig with valid config string - boolean value
 * Valid boolean config string should return true
 */
TEST_F(InitAAMPConfigTest, ValidConfigString_Boolean_UpdatesConfig)
{
	bool result = player->InitAAMPConfig("abr=true");

	// Verify the value was set
	EXPECT_TRUE(result) << "InitAAMPConfig should return true for valid boolean config";
	EXPECT_TRUE(player->mConfig.IsConfigSet(eAAMPConfig_EnableABR)) << "abr should be set to true";

	result = player->InitAAMPConfig("abr=false");
	EXPECT_TRUE(result) << "InitAAMPConfig should return true for valid boolean config";
	EXPECT_FALSE(player->mConfig.IsConfigSet(eAAMPConfig_EnableABR)) << "abr should be set to false";
}

/**
 * @brief Test InitAAMPConfig with valid config string - numeric value (double)
 * Valid numeric config string should return true
 */
TEST_F(InitAAMPConfigTest, ValidConfigString_Numeric_UpdatesConfig)
{
	bool result = player->InitAAMPConfig("networkTimeout=10.5");

	EXPECT_TRUE(result) << "InitAAMPConfig should return true for valid numeric config";

	// Verify the value was set
	double timeout = player->mConfig.GetConfigValue(eAAMPConfig_NetworkTimeout);
	EXPECT_DOUBLE_EQ(10.5, timeout) << "networkTimeout should be set to 10.5";
}

/**
 * @brief Test InitAAMPConfig with valid config string - integer value
 * Valid integer config string should return true
 */
TEST_F(InitAAMPConfigTest, ValidConfigString_Integer_UpdatesConfig)
{
	bool result = player->InitAAMPConfig("initialBuffer=5");

	EXPECT_TRUE(result) << "InitAAMPConfig should return true for valid integer config";

	// Verify the value was set
	int buffer = player->mConfig.GetConfigValue(eAAMPConfig_InitialBuffer);
	EXPECT_EQ(5, buffer) << "initialBuffer should be set to 5";
}

/**
 * @brief Test InitAAMPConfig with valid config string - string value
 * Valid string config string should return true
 */
TEST_F(InitAAMPConfigTest, ValidConfigString_String_UpdatesConfig)
{
	bool result = player->InitAAMPConfig("userAgent=TestAgent/1.0");

	EXPECT_TRUE(result) << "InitAAMPConfig should return true for valid string config";

	// Verify the value was set
	std::string userAgent = player->mConfig.GetConfigValue(eAAMPConfig_UserAgent);
	EXPECT_EQ("TestAgent/1.0", userAgent) << "userAgent should be set to 'TestAgent/1.0'";
}

/**
 * @brief Test InitAAMPConfig with valid config string - URL with special characters
 * Valid URL config string should return true
 */
TEST_F(InitAAMPConfigTest, ValidConfigString_URL_UpdatesConfig)
{
	bool result = player->InitAAMPConfig("networkProxy=http://proxy.example.com:8080");

	EXPECT_TRUE(result) << "InitAAMPConfig should return true for valid URL config";

	// Verify the value was set
	std::string proxy = player->mConfig.GetConfigValue(eAAMPConfig_NetworkProxy);
	EXPECT_EQ("http://proxy.example.com:8080", proxy) << "networkProxy should be set to 'http://proxy.example.com:8080'";
}

/**
 * @brief Test InitAAMPConfig with valid JSON - single boolean value
 * Valid JSON with boolean should update configuration and return true
 */
TEST_F(InitAAMPConfigTest, ValidJSON_SingleBoolean_UpdatesConfig)
{
	bool result = player->InitAAMPConfig("{\"abr\": true}");

	EXPECT_TRUE(result) << "InitAAMPConfig should return true for valid JSON";
	EXPECT_TRUE(player->mConfig.IsConfigSet(eAAMPConfig_EnableABR)) << "abr should be set to true from JSON";
}

/**
 * @brief Test InitAAMPConfig with valid JSON - single numeric value
 * Valid JSON with number should update configuration and return true
 */
TEST_F(InitAAMPConfigTest, ValidJSON_SingleNumeric_UpdatesConfig)
{
	bool result = player->InitAAMPConfig("{\"networkTimeout\": 15.0}");

	EXPECT_TRUE(result) << "InitAAMPConfig should return true for valid JSON";

	// Verify the value was set
	double timeout = player->mConfig.GetConfigValue(eAAMPConfig_NetworkTimeout);
	EXPECT_DOUBLE_EQ(15.0, timeout) << "networkTimeout should be set to 15.0 from JSON";
}

/**
 * @brief Test InitAAMPConfig with valid JSON - single string value
 * Valid JSON with string should update configuration and return true
 */
TEST_F(InitAAMPConfigTest, ValidJSON_SingleString_UpdatesConfig)
{
	bool result = player->InitAAMPConfig("{\"userAgent\": \"JSONAgent/2.0\"}");

	EXPECT_TRUE(result) << "InitAAMPConfig should return true for valid JSON";

	// Verify the value was set
	std::string userAgent = player->mConfig.GetConfigValue(eAAMPConfig_UserAgent);
	EXPECT_EQ("JSONAgent/2.0", userAgent) << "userAgent should be set to 'JSONAgent/2.0' from JSON";
}

/**
 * @brief Test InitAAMPConfig with valid JSON - multiple values
 * Valid JSON with multiple configs should update all and return true
 */
TEST_F(InitAAMPConfigTest, ValidJSON_MultipleValues_UpdatesAllConfigs)
{
	bool result = player->InitAAMPConfig(R"({
		"abr": false,
		"networkTimeout": 20.0,
		"initialBuffer": 3,
		"userAgent": "MultiConfigAgent/1.0"
	})");

	EXPECT_TRUE(result) << "InitAAMPConfig should return true for valid multi-value JSON";

	// Verify all values were set
	EXPECT_FALSE(player->mConfig.IsConfigSet(eAAMPConfig_EnableABR)) << "abr should be false";
	EXPECT_DOUBLE_EQ(20.0, player->mConfig.GetConfigValue(eAAMPConfig_NetworkTimeout)) << "networkTimeout should be 20.0";
	EXPECT_EQ(3, player->mConfig.GetConfigValue(eAAMPConfig_InitialBuffer)) << "initialBuffer should be 3";
	EXPECT_EQ("MultiConfigAgent/1.0", player->mConfig.GetConfigValue(eAAMPConfig_UserAgent)) << "userAgent should be 'MultiConfigAgent/1.0'";
}

/**
 * @brief Test InitAAMPConfig with valid JSON - nested objects (DRM config)
 * Valid JSON with nested objects should be processed
 */
TEST_F(InitAAMPConfigTest, ValidJSON_NestedDRMConfig_Processed)
{
	bool result = player->InitAAMPConfig(R"({
		"abr": true,
		"drmConfig": {
			"com.widevine.alpha": "https://license.example.com"
		}
	})");

	EXPECT_TRUE(result) << "InitAAMPConfig should return true for JSON with drmConfig";
	EXPECT_TRUE(player->mConfig.IsConfigSet(eAAMPConfig_EnableABR)) << "abr should be set from JSON with drmConfig";
}

/**
 * @brief Test InitAAMPConfig with valid JSON - nested objects (DRM config in invalid format)
 * JSON with nested objects in the unexpected format should be handled gracefully
 * Sample DRMConfig as per specification
 * var drmConfig = {
 *   'com.widevine.alpha': 'https://example.com/AcquireLicense', // Replace with valid URL
 *   'preferredKeysystem': 'com.widevine.alpha'
 * };
 */
TEST_F(InitAAMPConfigTest, ValidJSON_NestedInvalidDRMConfig_Handled)
{
	bool result = player->InitAAMPConfig(R"({
		"abr": true,
		"drmConfig": {
			"com.widevine.alpha": {
				"licenseUrl": "https://license.example.com"
			}
		}
	})");

	EXPECT_TRUE(result) << "InitAAMPConfig should return true for JSON with drmConfig";
	EXPECT_TRUE(player->mConfig.IsConfigSet(eAAMPConfig_EnableABR)) << "abr should be set from JSON with drmConfig";
}

/**
 * @brief Test InitAAMPConfig format auto-detection
 * Same config name should work with both JSON and config string
 */
TEST_F(InitAAMPConfigTest, AutoDetection_BothFormatsWork)
{
	// Test config string format
	bool result1 = player->InitAAMPConfig("networkTimeout=25.0");
	EXPECT_TRUE(result1) << "Config string format should work";
	EXPECT_DOUBLE_EQ(25.0, player->mConfig.GetConfigValue(eAAMPConfig_NetworkTimeout));

	// Test JSON format
	bool result2 = player->InitAAMPConfig("{\"networkTimeout\": 30.0}");
	EXPECT_TRUE(result2) << "JSON format should work";
	EXPECT_DOUBLE_EQ(30.0, player->mConfig.GetConfigValue(eAAMPConfig_NetworkTimeout));
}

/**
 * @brief Test InitAAMPConfig with config string containing equals in value
 * Config string with '=' in value should be handled correctly
 */
TEST_F(InitAAMPConfigTest, ConfigString_EqualsInValue_HandledCorrectly)
{
	// URL with query parameters containing '='
	bool result = player->InitAAMPConfig("networkProxy=http://proxy.com?key=value");

	EXPECT_TRUE(result) << "Config string with '=' in value should be processed";

	// Verify the value was set correctly
	std::string proxy = player->mConfig.GetConfigValue(eAAMPConfig_NetworkProxy);
	EXPECT_EQ("http://proxy.com?key=value", proxy) << "networkProxy should preserve '=' in value";
}

/**
 * @brief Test InitAAMPConfig with config string containing whitespace
 * Config string with whitespace should be trimmed and processed
 */
TEST_F(InitAAMPConfigTest, ConfigString_WithWhitespace_Trimmed)
{
	bool result = player->InitAAMPConfig("  networkTimeout=35.0  ");

	EXPECT_TRUE(result) << "Config string with whitespace should be processed";
	EXPECT_DOUBLE_EQ(35.0, player->mConfig.GetConfigValue(eAAMPConfig_NetworkTimeout)) << "networkTimeout should be set despite whitespace";
}

/**
 * @brief Test InitAAMPConfig with unknown config name
 * Unknown config name should not be processed but not update any config
 */
TEST_F(InitAAMPConfigTest, UnknownConfigName_ProcessedButNotSet)
{
	bool result = player->InitAAMPConfig("unknownConfigName=someValue");

	// ProcessConfigText will process but won't find a matching config
	// The return value depends on whether it's treated as an error
	EXPECT_FALSE(result) << "Unknown config name should be discarded";
}

/**
 * @brief Test InitAAMPConfig backward compatibility with existing JSON usage
 * Existing JSON configs should continue to work unchanged
 */
TEST_F(InitAAMPConfigTest, BackwardCompatibility_ExistingJSONWorks)
{
	// Arrange - Simulate existing application JSON config
	const char* existingConfig = R"({
		"abr": true,
		"networkTimeout": 10.0,
		"manifestTimeout": 10.0,
		"initialBuffer": 0,
		"userAgent": "ExistingApp/1.0"
	})";

	bool result = player->InitAAMPConfig(existingConfig);

	EXPECT_TRUE(result) << "Existing JSON configs should work unchanged";
	EXPECT_TRUE(player->mConfig.IsConfigSet(eAAMPConfig_EnableABR));
	EXPECT_DOUBLE_EQ(10.0, player->mConfig.GetConfigValue(eAAMPConfig_NetworkTimeout));
	EXPECT_DOUBLE_EQ(10.0, player->mConfig.GetConfigValue(eAAMPConfig_ManifestTimeout));
	EXPECT_EQ(0, player->mConfig.GetConfigValue(eAAMPConfig_InitialBuffer));
	EXPECT_EQ("ExistingApp/1.0", player->mConfig.GetConfigValue(eAAMPConfig_UserAgent));
}