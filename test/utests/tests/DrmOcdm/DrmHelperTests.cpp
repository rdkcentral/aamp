/*
 * If not stated otherwise in this file or this component's license file the
 * following copyright and licenses apply:
 *
 * Copyright 2024 RDK Management
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
#include <iostream>
#include <sstream>
#include <iomanip>
#include <algorithm>
#include <iterator>

#include "AampConfig.h"

#include "AampDrmHelper.h"

#include "AampDrmTestUtils.h"
#include "aampMocks.h"
#include "_base64.h"

struct CreateHelperTestData
{
	DrmMethod method;
	MediaFormat mediaFormat;
	std::string uri;
	std::string keyFormat;
	std::string systemUUID;
	std::vector<uint8_t> expectedKeyPayload;
};

DrmInfo createDrmInfo(DrmMethod method, MediaFormat mediaFormat, const std::string& uri = "",
					  const std::string& keyFormat = "", const std::string& systemUUID = "",
					  const std::string& initData = "")
{
	DrmInfo drmInfo;

	drmInfo.method = method;
	drmInfo.mediaFormat = mediaFormat;
	drmInfo.keyURI = uri;
	drmInfo.keyFormat = keyFormat;
	drmInfo.systemUUID = systemUUID;
	drmInfo.initData = initData;

	return drmInfo;
}

class AampDrmHelperTests : public ::testing::Test
{
protected:
	void SetUp() override
	{
		MockAampReset();
	}

	void TearDown() override
	{
		MockAampReset();
	}

public:
};

TEST_F(AampDrmHelperTests, TestDrmIds)
{
	std::vector<std::string> expectedIds({
		"A68129D3-575B-4F1A-9CBA-3223846CF7C3", // VGDRM
		"1077efec-c0b2-4d02-ace3-3c1e52e2fb4b", // ClearKey
		"edef8ba9-79d6-4ace-a3c8-27dcd51d21ed", // Widevine
		"9a04f079-9840-4286-ab92-e65be0885f95"	// PlayReady
	});
	std::sort(expectedIds.begin(), expectedIds.end());

	std::vector<std::string> actualIds;
	AampDrmHelperEngine::getInstance().getSystemIds(actualIds);
	std::sort(actualIds.begin(), actualIds.end());

	ASSERT_EQ(expectedIds, actualIds);
}

TEST_F(AampDrmHelperTests, TestCreateVgdrmHelper)
{
	const std::vector<CreateHelperTestData> testData = {
		// Invalid URI but valid KEYFORMAT
		{eMETHOD_AES_128,
		 eMEDIAFORMAT_HLS,
		 "91701500000810367b131dd025ab0a7bd8d20c1314151600",
		 "A68129D3-575B-4F1A-9CBA-3223846CF7C3",
		 "",
		 {}},

		// Invalid URI but valid KEYFORMAT
		{eMETHOD_AES_128,
		 eMEDIAFORMAT_HLS,
		 "91701500000810367b131dd025ab0a7bd8d20c1314151600",
		 "A68129D3-575B-4F1A-9CBA-3223846CF7C3",
		 "",
		 {}},

		// Valid 48 char URI and KEYFORMAT
		{eMETHOD_AES_128,
		 eMEDIAFORMAT_HLS,
		 "81701500000810367b131dd025ab0a7bd8d20c1314151600",
		 "A68129D3-575B-4F1A-9CBA-3223846CF7C3",
		 "",
		 {0x36, 0x7b, 0x13, 0x1d, 0xd0, 0x25, 0xab, 0x0a, 0x7b, 0xd8, 0xd2, 0x0c, 0x13, 0x14, 0x15,
		  0x16}},

		// Valid 48 char URI and KEYFORMAT. No METHOD (not required to pick up the VGDRM helper)
		{eMETHOD_NONE,
		 eMEDIAFORMAT_HLS,
		 "81701500000810367b131dd025ab0a7bd8d20c1314151600",
		 "A68129D3-575B-4F1A-9CBA-3223846CF7C3",
		 "",
		 {0x36, 0x7b, 0x13, 0x1d, 0xd0, 0x25, 0xab, 0x0a, 0x7b, 0xd8, 0xd2, 0x0c, 0x13, 0x14, 0x15,
		  0x16}},

		// Valid 48 char URI, no KEYFORMAT
		{eMETHOD_AES_128,
		 eMEDIAFORMAT_HLS,
		 "81701500000810367b131dd025ab0a7bd8d20c1314151600",
		 "",
		 "",
		 {0x36, 0x7b, 0x13, 0x1d, 0xd0, 0x25, 0xab, 0x0a, 0x7b, 0xd8, 0xd2, 0x0c, 0x13, 0x14, 0x15,
		  0x16}},

		// Valid 40 char URI, no KEYFORMAT
		{eMETHOD_AES_128,
		 eMEDIAFORMAT_HLS,
		 "8170110000080c367b131dd025ab0a7bd8d20c00",
		 "",
		 "",
		 {0x36, 0x7b, 0x13, 0x1d, 0xd0, 0x25, 0xab, 0x0a, 0x7b, 0xd8, 0xd2, 0x0c}},

		// Valid 48 char URI, uppercase
		{eMETHOD_AES_128,
		 eMEDIAFORMAT_HLS,
		 "81701500000810367B131DD025AB0A7BD8D20C1314151600",
		 "",
		 "",
		 {0x36, 0x7b, 0x13, 0x1d, 0xd0, 0x25, 0xab, 0x0a, 0x7b, 0xd8, 0xd2, 0x0c, 0x13, 0x14, 0x15,
		  0x16}},

		// Valid 40 char URI, uppercase
		{eMETHOD_AES_128,
		 eMEDIAFORMAT_HLS,
		 "8170110000080C367B131DD025AB0A7BD8D20C00",
		 "",
		 "",
		 {0x36, 0x7b, 0x13, 0x1d, 0xd0, 0x25, 0xab, 0x0a, 0x7b, 0xd8, 0xd2, 0x0c}},

		// 48 char URI specifies maximum key length possible without going off the end of the string
		{eMETHOD_AES_128,
		 eMEDIAFORMAT_HLS,
		 "81701500000811367b131dd025ab0a7bd8d20c1314151600",
		 "A68129D3-575B-4F1A-9CBA-3223846CF7C3",
		 "",
		 {0x36, 0x7b, 0x13, 0x1d, 0xd0, 0x25, 0xab, 0x0a, 0x7b, 0xd8, 0xd2, 0x0c, 0x13, 0x14, 0x15,
		  0x16, 0x0}},

		// 48 char URI specifies key length which will just take us off the end of the string.
		// Creation should pass but empty key returned
		{eMETHOD_AES_128,
		 eMEDIAFORMAT_HLS,
		 "81701500000812367b131dd025ab0a7bd8d20c1314151600",
		 "A68129D3-575B-4F1A-9CBA-3223846CF7C3",
		 "",
		 {}},

		// 40 char URI specifies maximum key length possible without going off the end of the string
		{eMETHOD_AES_128,
		 eMEDIAFORMAT_HLS,
		 "8170110000080d367b131dd025ab0a7bd8d20c00",
		 "A68129D3-575B-4F1A-9CBA-3223846CF7C3",
		 "",
		 {0x36, 0x7b, 0x13, 0x1d, 0xd0, 0x25, 0xab, 0x0a, 0x7b, 0xd8, 0xd2, 0x0c, 0x0}},

		// 40 char URI specifies key length which will just take us off the end of the string.
		// Creation should pass but empty key returned
		{eMETHOD_AES_128,
		 eMEDIAFORMAT_HLS,
		 "8170110000080e367b131dd025ab0a7bd8d20c00",
		 "A68129D3-575B-4F1A-9CBA-3223846CF7C3",
		 "",
		 {}},

		// Textual identifier
		{eMETHOD_AES_128, eMEDIAFORMAT_HLS, "", "net.vgdrm", "", {}}};
	DrmInfo drmInfo;

	for (auto& test_data : testData)
	{
		std::vector<uint8_t> keyID;

		drmInfo = createDrmInfo(test_data.method, test_data.mediaFormat, test_data.uri,
								test_data.keyFormat, test_data.systemUUID);

		ASSERT_TRUE(AampDrmHelperEngine::getInstance().hasDRM(drmInfo));

		std::shared_ptr<AampDrmHelper> vgdrmHelper =
			AampDrmHelperEngine::getInstance().createHelper(drmInfo);
		ASSERT_TRUE(vgdrmHelper != nullptr);
		ASSERT_EQ("net.vgdrm", vgdrmHelper->ocdmSystemId());
		ASSERT_EQ(true, vgdrmHelper->isClearDecrypt());
		ASSERT_EQ(true, vgdrmHelper->isHdcp22Required());
		ASSERT_EQ(4, vgdrmHelper->getDrmCodecType());
		ASSERT_EQ(true, vgdrmHelper->isExternalLicense());
		ASSERT_EQ(10000U, vgdrmHelper->licenseGenerateTimeout());
		ASSERT_EQ(10000U, vgdrmHelper->keyProcessTimeout());

		vgdrmHelper->getKey(keyID);

		if (test_data.expectedKeyPayload.size() != 0)
		{
			std::shared_ptr<std::vector<uint8_t>> keyData;
			ASSERT_EQ(test_data.expectedKeyPayload, keyID);
		}
	}
}

TEST_F(AampDrmHelperTests, TestCreateVgdrmHelperNegative)
{
	// Note: using METHOD_NONE on each of the below to avoid picking up
	// the default helper for AES_128 (ClearKey). The positive test proves
	// that we can create a VGDRM helper with METHOD_NONE, providing the
	// other criteria are matched
	const std::vector<CreateHelperTestData> testData = {
		// Start of URI is valid, but payload has a non hex value at start
		{eMETHOD_NONE, eMEDIAFORMAT_HLS, "8170110000080CZ67B331DD025AB0A7BD8D20C00", "", "", {}},

		// Start of URI is valid, but payload has a non hex value in middle
		{eMETHOD_NONE, eMEDIAFORMAT_HLS, "8170110000080C367B331DD025AZ0A7BD8D20C00", "", "", {}},

		// Invalid URI and KEYFORMAT
		{eMETHOD_NONE,
		 eMEDIAFORMAT_HLS,
		 "BAD0110000080c367b131dd025ab0a7bd8d20c00",
		 "BAD129D3-575B-4F1A-9CBA-3223846CF7C3",
		 "",
		 {}}, /* Since the URI data is bad we won't get a payload */

		// Invalid URI and KEYFORMAT (lower-case)
		{eMETHOD_NONE,
		 eMEDIAFORMAT_HLS,
		 "BAD0110000080c367b131dd025ab0a7bd8d20c00",
		 "a68129d3-575b-4f1a-9cba-3223846cf7c3",
		 "",
		 {}}, /* Since the URI data is bad we won't get a payload */

		// Invalid URI, no KEYFORMAT
		{eMETHOD_NONE, eMEDIAFORMAT_HLS, "BAD0110000080c367b131dd025ab0a7bd8d20c00", "", "", {}},

		// Start of URI is valid, but it's 1 character short
		{eMETHOD_NONE, eMEDIAFORMAT_HLS, "8170110000080c367b131dd025ab0a7bd8d20c0", "", "", {}},

		// Start of URI is valid, but it's 1 character too long
		{eMETHOD_NONE, eMEDIAFORMAT_HLS, "8170110000080c367b131dd025ab0a7bd8d20c000", "", "", {}}};
	DrmInfo drmInfo;

	for (auto& test_data : testData)
	{
		drmInfo = createDrmInfo(test_data.method, test_data.mediaFormat, test_data.uri,
								test_data.keyFormat, test_data.systemUUID);

		ASSERT_FALSE(AampDrmHelperEngine::getInstance().hasDRM(drmInfo));

		std::shared_ptr<AampDrmHelper> vgdrmHelper =
			AampDrmHelperEngine::getInstance().createHelper(drmInfo);
		ASSERT_TRUE(vgdrmHelper == nullptr);
	}
}

TEST_F(AampDrmHelperTests, TestVgdrmHelperInitDataCreation)
{
	std::vector<DrmInfo> drmInfoList;

	drmInfoList.push_back(
		createDrmInfo(eMETHOD_AES_128, eMEDIAFORMAT_HLS, "8170110000080c367b131dd025ab0a7bd8d20c00",
					  "A68129D3-575B-4F1A-9CBA-3223846CF7C3", "", "somebase64initdata"));

	// Extra base 64 characters at start and end. Shouldn't cause any issue
	drmInfoList.push_back(
		createDrmInfo(eMETHOD_AES_128, eMEDIAFORMAT_HLS, "8170110000080c367b131dd025ab0a7bd8d20c00",
					  "A68129D3-575B-4F1A-9CBA-3223846CF7C3", "", "+/=somebase64initdata+/="));

	for (const DrmInfo& drmInfo : drmInfoList)
	{
		std::shared_ptr<AampDrmHelper> vgdrmHelper =
			AampDrmHelperEngine::getInstance().createHelper(drmInfo);
		ASSERT_TRUE(vgdrmHelper != nullptr);

		std::vector<uint8_t> initData;
		vgdrmHelper->createInitData(initData);

		TestUtilJsonWrapper jsonWrapper(initData);
		cJSON* initDataObj = jsonWrapper.getJsonObj();
		ASSERT_TRUE(initDataObj != nullptr);

		ASSERT_JSON_STR_VALUE(initDataObj, "initData", drmInfo.initData.c_str());
		ASSERT_JSON_STR_VALUE(initDataObj, "uri", drmInfo.keyURI.c_str());

		// Currently pssh won't be present
		ASSERT_EQ(nullptr, cJSON_GetObjectItem(initDataObj, "pssh"));
	}
}

TEST_F(AampDrmHelperTests, TestVgdrmHelperGenerateLicenseRequest)
{
	DrmInfo drmInfo = createDrmInfo(eMETHOD_AES_128, eMEDIAFORMAT_HLS,
									"81701500000810367b131dd025ab0a7bd8d20c1314151600");
	drmInfo.manifestURL = "http://example.com/hls/playlist.m3u8";
	ASSERT_TRUE(AampDrmHelperEngine::getInstance().hasDRM(drmInfo));
	std::shared_ptr<AampDrmHelper> clearKeyHelper =
		AampDrmHelperEngine::getInstance().createHelper(drmInfo);

	AampChallengeInfo challengeInfo;
	AampLicenseRequest licenseRequest;
	clearKeyHelper->generateLicenseRequest(challengeInfo, licenseRequest);

	ASSERT_EQ(AampLicenseRequest::DRM_RETRIEVE, licenseRequest.method);
	ASSERT_EQ("", licenseRequest.url);
	ASSERT_EQ("", licenseRequest.payload);
}

TEST_F(AampDrmHelperTests, TestCreateClearKeyHelper)
{
	const std::vector<CreateHelperTestData> testData = {
		// Valid KEYFORMAT, HLS
		{eMETHOD_AES_128,
		 eMEDIAFORMAT_HLS_MP4,
		 "file.key",
		 "urn:uuid:1077efec-c0b2-4d02-ace3-3c1e52e2fb4b",
		 "1077efec-c0b2-4d02-ace3-3c1e52e2fb4b",
		 {'1'}},

		// Valid KEYFORMAT, DASH
		{eMETHOD_AES_128,
		 eMEDIAFORMAT_DASH,
		 "file.key",
		 "urn:uuid:1077efec-c0b2-4d02-ace3-3c1e52e2fb4b",
		 "1077efec-c0b2-4d02-ace3-3c1e52e2fb4b",
		 {}}, // For DASH, the key should come from the PSSH, so we won't check that here

		// Textual identifier rather than UUID
		{eMETHOD_AES_128,
		 eMEDIAFORMAT_HLS_MP4,
		 "file.key",
		 "org.w3.clearkey",
		 "1077efec-c0b2-4d02-ace3-3c1e52e2fb4b",
		 {'1'}},

	};
	DrmInfo drmInfo;

	for (auto& test_data : testData)
	{
		drmInfo = createDrmInfo(eMETHOD_AES_128, test_data.mediaFormat, test_data.uri,
								test_data.keyFormat, test_data.systemUUID);

		ASSERT_TRUE(AampDrmHelperEngine::getInstance().hasDRM(drmInfo));

		std::shared_ptr<AampDrmHelper> clearKeyHelper =
			AampDrmHelperEngine::getInstance().createHelper(drmInfo);
		ASSERT_TRUE(clearKeyHelper != nullptr);
		ASSERT_EQ("org.w3.clearkey", clearKeyHelper->ocdmSystemId());
		ASSERT_EQ(true, clearKeyHelper->isClearDecrypt());
		ASSERT_EQ(false, clearKeyHelper->isHdcp22Required());
		ASSERT_EQ(0, clearKeyHelper->getDrmCodecType());
		ASSERT_EQ(false, clearKeyHelper->isExternalLicense());
		ASSERT_EQ(5000U, clearKeyHelper->licenseGenerateTimeout());
		ASSERT_EQ(5000U, clearKeyHelper->keyProcessTimeout());

		if (test_data.expectedKeyPayload.size() != 0)
		{
			std::vector<uint8_t> keyID;
			clearKeyHelper->getKey(keyID);
			ASSERT_EQ(test_data.expectedKeyPayload, keyID);
		}
	}
}

TEST_F(AampDrmHelperTests, TestClearKeyHelperHlsInitDataCreation)
{
	DrmInfo drmInfo = createDrmInfo(eMETHOD_AES_128, eMEDIAFORMAT_HLS_MP4, "file.key",
									"urn:uuid:1077efec-c0b2-4d02-ace3-3c1e52e2fb4b");
	ASSERT_TRUE(AampDrmHelperEngine::getInstance().hasDRM(drmInfo));
	std::shared_ptr<AampDrmHelper> clearKeyHelper =
		AampDrmHelperEngine::getInstance().createHelper(drmInfo);

	std::vector<uint8_t> initData;
	clearKeyHelper->createInitData(initData);

	TestUtilJsonWrapper jsonWrapper(initData);
	cJSON* initDataObj = jsonWrapper.getJsonObj();
	ASSERT_TRUE(initDataObj != nullptr);

	// kids
	cJSON* kidsArray = cJSON_GetObjectItem(initDataObj, "kids");
	ASSERT_TRUE(kidsArray != nullptr);
	ASSERT_EQ(1, cJSON_GetArraySize(kidsArray));
	cJSON* kids0 = cJSON_GetArrayItem(kidsArray, 0);
	ASSERT_STREQ("1", cJSON_GetStringValue(kids0));
}

TEST_F(AampDrmHelperTests, TestClearKeyHelperParsePssh)
{
	DrmInfo drmInfo = createDrmInfo(eMETHOD_AES_128, eMEDIAFORMAT_DASH, "file.key",
									"urn:uuid:1077efec-c0b2-4d02-ace3-3c1e52e2fb4b");
	ASSERT_TRUE(AampDrmHelperEngine::getInstance().hasDRM(drmInfo));
	std::shared_ptr<AampDrmHelper> clearKeyHelper =
		AampDrmHelperEngine::getInstance().createHelper(drmInfo);

	// For DASH the init data should have come from the PSSH, so when asked to create
	// the init data, the helper should just return that
	std::vector<uint8_t> psshData = {
		0x00, 0x00, 0x00, 0x34, 0x70, 0x73, 0x73, 0x68, 0x01, 0x00, 0x00, 0x00, 0x10,
		0x77, 0xef, 0xec, 0xc0, 0xb2, 0x4d, 0x02, 0xac, 0xe3, 0x3c, 0x1e, 0x52, 0xe2,
		0xfb, 0x4b, 0x00, 0x00, 0x00, 0x01, 0xfe, 0xed, 0xf0, 0x0d, 0xee, 0xde, 0xad,
		0xbe, 0xef, 0xf0, 0xba, 0xad, 0xf0, 0x0d, 0xd0, 0x0d, 0x00, 0x00, 0x00, 0x00};

	ASSERT_TRUE(clearKeyHelper->parsePssh(psshData.data(), (uint32_t)psshData.size()));

	std::vector<uint8_t> initData;
	clearKeyHelper->createInitData(initData);
	ASSERT_EQ(psshData, initData);

	// KeyId should have been extracted from the PSSH
	std::vector<uint8_t> keyID;
	const std::vector<uint8_t> expectedKeyID = {0xfe, 0xed, 0xf0, 0x0d, 0xee, 0xde, 0xad, 0xbe,
												0xef, 0xf0, 0xba, 0xad, 0xf0, 0x0d, 0xd0, 0x0d};
	clearKeyHelper->getKey(keyID);
	ASSERT_EQ(expectedKeyID, keyID);
}

TEST_F(AampDrmHelperTests, TestClearKeyHelperGenerateLicenseRequest)
{
	DrmInfo drmInfo = createDrmInfo(eMETHOD_AES_128, eMEDIAFORMAT_HLS_MP4, "file.key",
									"urn:uuid:1077efec-c0b2-4d02-ace3-3c1e52e2fb4b");
	drmInfo.manifestURL = "http://stream.example/hls/playlist.m3u8";
	ASSERT_TRUE(AampDrmHelperEngine::getInstance().hasDRM(drmInfo));
	std::shared_ptr<AampDrmHelper> clearKeyHelper =
		AampDrmHelperEngine::getInstance().createHelper(drmInfo);

	AampChallengeInfo challengeInfo;
	challengeInfo.url = "http://challengeinfourl.example";
	AampLicenseRequest licenseRequest;

	// No ClearKey license URL in the license request, expect the URL to be
	// constructed from the information in the DrmInfo
	clearKeyHelper->generateLicenseRequest(challengeInfo, licenseRequest);
	ASSERT_EQ(AampLicenseRequest::POST, licenseRequest.method);
	ASSERT_EQ("http://stream.example/hls/file.key", licenseRequest.url);
	ASSERT_EQ("", licenseRequest.payload);

	// Setting a ClearKey license URL in the license request, expect
	// that to take precedence
	const std::string fixedCkLicenseUrl = "http://cklicenseserver.example";
	licenseRequest.url = fixedCkLicenseUrl;
	clearKeyHelper->generateLicenseRequest(challengeInfo, licenseRequest);
	ASSERT_EQ(fixedCkLicenseUrl, licenseRequest.url);

	// Clearing ClearKey license URL in the license request and creating a
	// helper with no key URI in the DrmInfo. Should use the URI from the challenge
	licenseRequest.url.clear();
	DrmInfo drmInfoNoKeyUri = createDrmInfo(eMETHOD_AES_128, eMEDIAFORMAT_HLS_MP4, "",
											"urn:uuid:1077efec-c0b2-4d02-ace3-3c1e52e2fb4b");
	std::shared_ptr<AampDrmHelper> clearKeyHelperNoKeyUri =
		AampDrmHelperEngine::getInstance().createHelper(drmInfoNoKeyUri);
	clearKeyHelperNoKeyUri->generateLicenseRequest(challengeInfo, licenseRequest);
	ASSERT_EQ(challengeInfo.url, licenseRequest.url);
}

TEST_F(AampDrmHelperTests, TestClearKeyHelperTransformHlsLicenseResponse)
{
	struct TransformLicenseResponseTestData
	{
		std::vector<uint8_t> keyResponse;
		std::string expectedEncodedKey;
	};

	DrmInfo drmInfo = createDrmInfo(eMETHOD_AES_128, eMEDIAFORMAT_HLS_MP4, "file.key",
									"urn:uuid:1077efec-c0b2-4d02-ace3-3c1e52e2fb4b");
	ASSERT_TRUE(AampDrmHelperEngine::getInstance().hasDRM(drmInfo));
	std::shared_ptr<AampDrmHelper> clearKeyHelper =
		AampDrmHelperEngine::getInstance().createHelper(drmInfo);

	const std::vector<TransformLicenseResponseTestData> testData{
		// Empty response - should lead to empty string
		{{}, {""}},
		// Most basic case - 1 maps to AQ
		{{0x1}, {"AQ"}},
		// Should lead to a string containing every possible base64url character
		{{0x00, 0x10, 0x83, 0x10, 0x51, 0x87, 0x20, 0x92, 0x8b, 0x30, 0xd3, 0x8f,
		  0x41, 0x14, 0x93, 0x51, 0x55, 0x97, 0x61, 0x96, 0x9b, 0x71, 0xd7, 0x9f,
		  0x82, 0x18, 0xa3, 0x92, 0x59, 0xa7, 0xa2, 0x9a, 0xab, 0xb2, 0xdb, 0xaf,
		  0xc3, 0x1c, 0xb3, 0xd3, 0x5d, 0xb7, 0xe3, 0x9e, 0xbb, 0xf3, 0xdf, 0xbf},
		 {"ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-_"}}};

	for (auto& testCase : testData)
	{
		std::shared_ptr<DrmData> drmData = std::make_shared<DrmData>(
			(const char*)&testCase.keyResponse[0], testCase.keyResponse.size());
		clearKeyHelper->transformLicenseResponse(drmData);

		TestUtilJsonWrapper jsonWrapper( drmData->getData().c_str(),drmData->getDataLength() );
		cJSON* responseObj = jsonWrapper.getJsonObj();
		ASSERT_TRUE(responseObj != nullptr);

		// keys array
		cJSON* keysArray = cJSON_GetObjectItem(responseObj, "keys");
		ASSERT_TRUE(keysArray != nullptr);
		ASSERT_EQ(1, cJSON_GetArraySize(keysArray));
		cJSON* keys0Obj = cJSON_GetArrayItem(keysArray, 0);
		ASSERT_JSON_STR_VALUE(keys0Obj, "alg", "cbc");
		ASSERT_JSON_STR_VALUE(keys0Obj, "k", testCase.expectedEncodedKey.c_str());
		ASSERT_JSON_STR_VALUE(keys0Obj, "kid",
							  "MQ"); // Expecting the character '1' (0x31) base64url encoded
	}
}

TEST_F(AampDrmHelperTests, TestTransformDashLicenseResponse)
{
	// Unlike HLS (where we do expect a ClearKey license to be transformed),
	// for DASH we expect the response to be given back untouched
	std::vector<DrmInfo> drmInfoList;

	// ClearKey
	drmInfoList.push_back(createDrmInfo(eMETHOD_AES_128, eMEDIAFORMAT_DASH, "file.key",
										"urn:uuid:1077efec-c0b2-4d02-ace3-3c1e52e2fb4b"));

	// PlayReady
	drmInfoList.push_back(createDrmInfo(eMETHOD_NONE, eMEDIAFORMAT_DASH, "", "",
										"9a04f079-9840-4286-ab92-e65be0885f95"));

	for (auto& drmInfo : drmInfoList)
	{
		ASSERT_TRUE(AampDrmHelperEngine::getInstance().hasDRM(drmInfo));
		std::shared_ptr<AampDrmHelper> clearKeyHelper =
			AampDrmHelperEngine::getInstance().createHelper(drmInfo);
		char licenseResponse[] = {'D', 'A', 'S', 'H', 'L', 'I', 'C'};
		std::shared_ptr<DrmData> drmData =
			std::make_shared<DrmData>(licenseResponse, sizeof(licenseResponse));

		clearKeyHelper->transformLicenseResponse(drmData);
		ASSERT_EQ(sizeof(licenseResponse), drmData->getDataLength());
		ASSERT_EQ(std::string("DASHLIC"), drmData->getData());
	}
}

TEST_F(AampDrmHelperTests, TestCreatePlayReadyHelper)
{
	const std::vector<CreateHelperTestData> testData = {
		// Valid UUID
		{eMETHOD_AES_128,
		 eMEDIAFORMAT_DASH, // Note: PlayReady helper currently supports DASH only
		 "file.key",
		 "",
		 "9a04f079-9840-4286-ab92-e65be0885f95",
		 {}}, // For DASH, the key should come from the PSSH, so we won't check that here

		// Valid UUID, no method (method not required)
		{eMETHOD_NONE,
		 eMEDIAFORMAT_DASH, // Note: PlayReady helper currently supports DASH only
		 "file.key",
		 "",
		 "9a04f079-9840-4286-ab92-e65be0885f95",
		 {}}, // For DASH, the key should come from the PSSH, so we won't check that here

		// Textual identifier rather than UUID
		{eMETHOD_AES_128,
		 eMEDIAFORMAT_DASH, // Note: PlayReady helper currently supports DASH only
		 "file.key",
		 "com.microsoft.playready",
		 "",
		 {}} // For DASH, the key should come from the PSSH, so we won't check that here
	};
	DrmInfo drmInfo;

	for (auto& test_data : testData)
	{
		drmInfo = createDrmInfo(test_data.method, test_data.mediaFormat, test_data.uri,
								test_data.keyFormat, test_data.systemUUID);

		ASSERT_TRUE(AampDrmHelperEngine::getInstance().hasDRM(drmInfo));

		std::shared_ptr<AampDrmHelper> playReadyHelper =
			AampDrmHelperEngine::getInstance().createHelper(drmInfo);
		ASSERT_TRUE(playReadyHelper != nullptr);
		ASSERT_EQ("com.microsoft.playready", playReadyHelper->ocdmSystemId());
		ASSERT_EQ(false, playReadyHelper->isClearDecrypt());
		ASSERT_EQ(eDRM_PlayReady, playReadyHelper->getDrmCodecType());
		ASSERT_EQ(false, playReadyHelper->isExternalLicense());
		ASSERT_EQ(5000U, playReadyHelper->licenseGenerateTimeout());
		ASSERT_EQ(5000U, playReadyHelper->keyProcessTimeout());

		// TODO: HDCP checks
	}
}

TEST_F(AampDrmHelperTests, TestCreatePlayReadyHelperNegative)
{
	const std::vector<CreateHelperTestData> testData = {
		// Valid UUID but HLS media format, which isn't supported for the PlayReady helper
		{eMETHOD_NONE,
		 eMEDIAFORMAT_HLS,
		 "file.key",
		 "",
		 "9a04f079-9840-4286-ab92-e65be0885f95",
		 {}}};
	DrmInfo drmInfo;

	for (auto& test_data : testData)
	{
		drmInfo = createDrmInfo(test_data.method, test_data.mediaFormat, test_data.uri,
								test_data.keyFormat, test_data.systemUUID);

		ASSERT_FALSE(AampDrmHelperEngine::getInstance().hasDRM(drmInfo));

		std::shared_ptr<AampDrmHelper> playReadyHelper =
			AampDrmHelperEngine::getInstance().createHelper(drmInfo);
		ASSERT_TRUE(playReadyHelper == nullptr);
	}
}

TEST_F(AampDrmHelperTests, TestWidevineHelperParsePsshDrmMetaData)
{
	struct
	{
		const char *psshData;
		const char *expectedKey[4];
	} testData[] =
	{
		{ "AAAANHBzc2gBAAAA7e+LqXnWSs6jyCfc1R0h7QAAAAEttsSNMB9I6rt3G6eorJBCAAAAAA==",
			{
				"0x2D,0xB6,0xC4,0x8D,0x30,0x1F,0x48,0xEA,0xBB,0x77,0x1B,0xA7,0xA8,0xAC,0x90,0x42"
			}
			// Version 1, single KeyID
		},
		{ "AAAAOHBzc2gAAAAA7e+LqXnWSs6jyCfc1R0h7QAAABgSEC22xI0wH0jqu3cbp6iskEJI49yVmwY=",
			{
				"0x2D,0xB6,0xC4,0x8D,0x30,0x1F,0x48,0xEA,0xBB,0x77,0x1B,0xA7,0xA8,0xAC,0x90,0x42"
			}
			// Version 0, 'cenc'
		},
		{ "AAAAOHBzc2gAAAAA7e+LqXnWSs6jyCfc1R0h7QAAABgSEC22xI0wH0jqu3cbp6iskEJI88aJmwY=",
			{
				"0x2D,0xB6,0xC4,0x8D,0x30,0x1F,0x48,0xEA,0xBB,0x77,0x1B,0xA7,0xA8,0xAC,0x90,0x42"
			}
			// Version 0, 'cbcs'
		},
		{ "AAAARHBzc2gBAAAA7e+LqXnWSs6jyCfc1R0h7QAAAAIAAAAAAAAAAAAAAAAAAAAAEREREREREREREREREREREQAAAAA=",
			{
				"0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00",
				"0x11,0x11,0x11,0x11,0x11,0x11,0x11,0x11,0x11,0x11,0x11,0x11,0x11,0x11,0x11,0x11"
			}
			// Version 1, two KeyIDs
		},
		{"AAAAP3Bzc2gAAAAA7e+LqXnWSs6jyCfc1R0h7QAAAB8SEFYWbwTmlTrikzbYc0PLv+IaBWV6ZHJtSPPGiZsG",
			{
				"0x56,0x16,0x6f,0x04,0xe6,0x95,0x3a,0xe2,0x93,0x36,0xd8,0x73,0x43,0xcb,0xbf,0xe2"
			}
			// protection scheme: 63 62 63 73 (cbcs)
			// provider: ezdrm
		},
		{"AAAASnBzc2gAAAAA7e+LqXnWSs6jyCfc1R0h7QAAACoIARIQL38/m5MtSFqBGT0XY2KM4yIUTkJDVTA0Mzg3NjU0MDA1NjMwMDc=",
			{
				"0x2f,0x7f,0x3f,0x9b,0x93,0x2d,0x48,0x5a,0x81,0x19,0x3d,0x17,0x63,0x62,0x8c,0xe3"
			}
			// includes 'author id'
			// algorithm: AES-CTR full sample encryption
			// Content ID: NBCU0438765400563007
		},
		{"AAAASnBzc2gAAAAA7e+LqXnWSs6jyCfc1R0h7QAAACoIARIQ7zHnTYhMQv+k86p90K3+rCIUTkJDVTkwMDAxOTI5MTUxMzAwMDM=",
			{
				"0xef,0x31,0xe7,0x4d,0x88,0x4c,0x42,0xff,0xa4,0xf3,0xaa,0x7d,0xd0,0xad,0xfe,0xac"
			}
			// algorithm: AES-CTR full sample encryption
			// Content ID: NBCU9000192915130003
		},
		{"AAAAfXBzc2gAAAAA7e+LqXnWSs6jyCfc1R0h7QAAAF0SJDhlY2ZiOWZkLTNhODktNTNlMy05YjgwLTg4OTBmM2IyMzNmYiI1aW5kZW1hbmQuY29tSU5UTDAyMTgyMjIwMDA0MTQ5OTMtSU5NVjAyMTgyMjIwMDA0MTQ5OTM=",
			{
				"0x38,0x65,0x63,0x66,0x62,0x39,0x66,0x64,0x2D,0x33,0x61,0x38,0x39,0x2D,0x35,0x33,0x65,0x33,0x2D,0x39,0x62,0x38,0x30,0x2D,0x38,0x38,0x39,0x30,0x66,0x33,0x62,0x32,0x33,0x33,0x66,0x62"
			}
			// Content ID: indemand.comINTL0218222000414993-INMV0218222000414993
			// Key ID is 36 bytes long instead of required 16
		},
		{"AAAAMnBzc2gAAAAA7e+LqXnWSs6jyCfc1R0h7QAAABISEG3fTdGQ1VJtmPL1WTLb1a8=",
			{
				"0x6d,0xdf,0x4d,0xd1,0x90,0xd5,0x52,0x6d,0x98,0xf2,0xf5,0x59,0x32,0xdb,0xd5,0xaf"
			}
		},
		{"AAAA93Bzc2gAAAAA7e+LqXnWSs6jyCfc1R0h7QAAANcSJDQ3OTZiNjY3LWZkZjYtYjI0My04MTM4LTNmM2VmZmQzN2E0NRIkNjdlZGIwZjQtMzIyYS1hNDgyLWZlNmQtZThiNjZiYWFhOGU4EiRhZWYyZjE3YS0xOWUwLWQ2MTctMGI4Ny02MTdmNjQ5OWNlZjQSJGM2NzE5YWJjLTY4YzctMWYxZC0xOWRiLTUxMjU5YWY2MDJmZSI9dXBmYWl0aGFuZGZhbWlseS5jb21VUFRMMDAwMDAwMDAwODM3Mzg2MS1VUE1WMDAwMDAwMDAwODM3Mzg2MQ==",
			{
				"0x34,0x37,0x39,0x36,0x62,0x36,0x36,0x37,0x2D,0x66,0x64,0x66,0x36,0x2D,0x62,0x32,0x34,0x33,0x2D,0x38,0x31,0x33,0x38,0x2D,0x33,0x66,0x33,0x65,0x66,0x66,0x64,0x33,0x37,0x61,0x34,0x35",
				"0x36,0x37,0x65,0x64,0x62,0x30,0x66,0x34,0x2D,0x33,0x32,0x32,0x61,0x2D,0x61,0x34,0x38,0x32,0x2D,0x66,0x65,0x36,0x64,0x2D,0x65,0x38,0x62,0x36,0x36,0x62,0x61,0x61,0x61,0x38,0x65,0x38",
				"0x61,0x65,0x66,0x32,0x66,0x31,0x37,0x61,0x2D,0x31,0x39,0x65,0x30,0x2D,0x64,0x36,0x31,0x37,0x2D,0x30,0x62,0x38,0x37,0x2D,0x36,0x31,0x37,0x66,0x36,0x34,0x39,0x39,0x63,0x65,0x66,0x34",
				"0x63,0x36,0x37,0x31,0x39,0x61,0x62,0x63,0x2D,0x36,0x38,0x63,0x37,0x2D,0x31,0x66,0x31,0x64,0x2D,0x31,0x39,0x64,0x62,0x2D,0x35,0x31,0x32,0x35,0x39,0x61,0x66,0x36,0x30,0x32,0x66,0x65"
			},
			// Content ID: upfaithandfamily.comUPTL0000000008373861-UPMV0000000008373861
			// Key ID is 36 bytes long instead of required 16
		},
		{"AAAAOHBzc2gAAAAA7e+LqXnWSs6jyCfc1R0h7QAAABgSEABFDiYaE6QUtKoKcLv0s6hI49yVmwY=",
			{
				"0x00,0x45,0x0e,0x26,0x1a,0x13,0xa4,0x14,0xb4,0xaa,0x0a,0x70,0xbb,0xf4,0xb3,0xa8"
			}
			// Protection Scheme: 63 65 6E 63 (cenc)
			// AES-CTR full sample encryption
		}
	};
	for( int i=0; i<ARRAY_SIZE(testData); i++ )
	{
		const char *src = testData[i].psshData;
		size_t srcLen = strlen(src);
		size_t psshDataLen = 0;
		unsigned char *psshDataPtr = ::base64_Decode(src,&psshDataLen,srcLen);
		ASSERT_TRUE(psshDataPtr != nullptr);
		DrmInfo drmInfo = createDrmInfo(eMETHOD_AES_128, eMEDIAFORMAT_DASH, "file.key", "",
								"edef8ba9-79d6-4ace-a3c8-27dcd51d21ed");

		ASSERT_TRUE(AampDrmHelperEngine::getInstance().hasDRM(drmInfo));
		std::shared_ptr<AampDrmHelper> widevineHelper =
			AampDrmHelperEngine::getInstance().createHelper(drmInfo);
		ASSERT_TRUE(widevineHelper != nullptr);

		ASSERT_TRUE(widevineHelper->parsePssh(psshDataPtr, (uint32_t)psshDataLen));

		std::vector<uint8_t> initData;
		widevineHelper->createInitData(initData);
		for( int j=0; j<initData.size(); j++ )
		{
			ASSERT_EQ(psshDataPtr[j], initData[j] );
		}
		
		std::map<int, std::vector<uint8_t>> keyIDs;
		widevineHelper->getKeys(keyIDs);
		for( int iKey=0; iKey<4; iKey++ )
		{
			const char *expected = testData[i].expectedKey[iKey];
			if( expected )
			{
				int count = (int)(strlen(expected)+1)/5;
				ASSERT_EQ( keyIDs[iKey].size(), count );
				for( int j=0; j<count; j++ )
				{
					int byte = 0;
					assert( sscanf(&expected[j*5],"0x%02x",&byte)==1 );
					ASSERT_EQ( keyIDs[iKey][j], (uint8_t)byte );
				}
			}
		}
		std::string contentMetadata;
		contentMetadata = widevineHelper->getDrmMetaData();
		ASSERT_EQ("", contentMetadata);

		std::string metaData("content meta data");
		widevineHelper->setDrmMetaData(metaData);
		contentMetadata = widevineHelper->getDrmMetaData();
		ASSERT_EQ(metaData, contentMetadata);
		
		free( psshDataPtr );
	}
}

TEST_F(AampDrmHelperTests, TestCreateWidevineHelper)
{
	const std::vector<CreateHelperTestData> testData = {
		{eMETHOD_NONE,
		 eMEDIAFORMAT_DASH,
		 "file.key",
		 "",
		 "edef8ba9-79d6-4ace-a3c8-27dcd51d21ed",
		 {}},

		{eMETHOD_AES_128,
		 eMEDIAFORMAT_DASH,
		 "file.key",
		 "",
		 "edef8ba9-79d6-4ace-a3c8-27dcd51d21ed",
		 {}},

		// Textual identifier rather than UUID
		{eMETHOD_AES_128, eMEDIAFORMAT_DASH, "file.key", "com.widevine.alpha", "", {}}};
	DrmInfo drmInfo;

	for (auto& test_data : testData)
	{
		drmInfo = createDrmInfo(test_data.method, test_data.mediaFormat, test_data.uri,
								test_data.keyFormat, test_data.systemUUID);

		ASSERT_TRUE(AampDrmHelperEngine::getInstance().hasDRM(drmInfo));

		std::shared_ptr<AampDrmHelper> widevineHelper =
			AampDrmHelperEngine::getInstance().createHelper(drmInfo);
		ASSERT_TRUE(widevineHelper != nullptr);
		ASSERT_EQ("com.widevine.alpha", widevineHelper->ocdmSystemId());
		ASSERT_EQ(false, widevineHelper->isClearDecrypt());
		ASSERT_EQ(false, widevineHelper->isHdcp22Required());
		ASSERT_EQ(eDRM_WideVine, widevineHelper->getDrmCodecType());
		ASSERT_EQ(false, widevineHelper->isExternalLicense());
		ASSERT_EQ(5000U, widevineHelper->licenseGenerateTimeout());
		ASSERT_EQ(5000U, widevineHelper->keyProcessTimeout());
	}
}

TEST_F(AampDrmHelperTests, TestCreateWidevineHelperNegative)
{
	const std::vector<CreateHelperTestData> testData = {
		// Valid UUID but HLS media format, which isn't supported for the Widevine helper
		{eMETHOD_NONE,
		 eMEDIAFORMAT_HLS,
		 "file.key",
		 "",
		 "edef8ba9-79d6-4ace-a3c8-27dcd51d21ed",
		 {}}};
	DrmInfo drmInfo;

	for (auto& test_data : testData)
	{
		drmInfo = createDrmInfo(test_data.method, test_data.mediaFormat, test_data.uri,
								test_data.keyFormat, test_data.systemUUID);

		ASSERT_FALSE(AampDrmHelperEngine::getInstance().hasDRM(drmInfo));

		std::shared_ptr<AampDrmHelper> widevineHelper =
			AampDrmHelperEngine::getInstance().createHelper(drmInfo);
		ASSERT_TRUE(widevineHelper == nullptr);
	}
}

TEST_F(AampDrmHelperTests, TestPlayReadyHelperParsePssh)
{
	DrmInfo drmInfo = createDrmInfo(eMETHOD_NONE, eMEDIAFORMAT_DASH, "", "",
									"9a04f079-9840-4286-ab92-e65be0885f95");
	ASSERT_TRUE(AampDrmHelperEngine::getInstance().hasDRM(drmInfo));
	std::shared_ptr<AampDrmHelper> playReadyHelper =
		AampDrmHelperEngine::getInstance().createHelper(drmInfo);

	const std::string expectedMetadata = "testpolicydata";

	std::ostringstream psshSs;
	psshSs
		<< "<WRMHEADER xmlns=\"http://schemas.microsoft.com/DRM/2007/03/PlayReadyHeader\" version=\"4.0.0.0\">"
		<< "<DATA>"
		<< "<KID>16bytebase64enckeydata==</KID>"
		<< "<ckm:policy xmlns:ckm=\"urn:ccp:ckm\">" << expectedMetadata << "</ckm:policy>"
		<< "</DATA>"
		<< "</WRMHEADER>";
	const std::string psshStr = psshSs.str();

	ASSERT_TRUE(playReadyHelper->parsePssh((const unsigned char*)psshStr.data(), (uint32_t)psshStr.size()));

	// Check keyId and metadata, both of which should be based on the PSSH
	std::vector<uint8_t> keyId;
	playReadyHelper->getKey(keyId);

	const std::string expectedKeyId = "b5f2a6d7-dae6-eeb1-b87a-77247b275ab5";
	const std::string actualKeyId = std::string(keyId.begin(), keyId.begin() + keyId.size());

	ASSERT_EQ(expectedKeyId, actualKeyId);
	ASSERT_EQ(expectedMetadata, playReadyHelper->getDrmMetaData());
	// Ensure the helper doesn't set the meta data
	playReadyHelper->setDrmMetaData("content meta data that should be ignored");
	ASSERT_EQ(expectedMetadata, playReadyHelper->getDrmMetaData());

	// Dodgy PSSH data should lead to false return value
	const std::string badPssh = "somerandomdatawhichisntevenxml";
	ASSERT_FALSE(playReadyHelper->parsePssh((const unsigned char*)badPssh.data(), (uint32_t)badPssh.size()));
}

TEST_F(AampDrmHelperTests, TestPlayReadyHelperParsePsshNoPolicy)
{
	// As before but with no ckm:policy in the PSSH data.
	// Should be OK but lead to empty metadata
	DrmInfo drmInfo = createDrmInfo(eMETHOD_NONE, eMEDIAFORMAT_DASH, "", "",
									"9a04f079-9840-4286-ab92-e65be0885f95");
	ASSERT_TRUE(AampDrmHelperEngine::getInstance().hasDRM(drmInfo));
	std::shared_ptr<AampDrmHelper> playReadyHelper =
		AampDrmHelperEngine::getInstance().createHelper(drmInfo);

	const std::string psshStr =
		"<WRMHEADER xmlns=\"http://schemas.microsoft.com/DRM/2007/03/PlayReadyHeader\" version=\"4.0.0.0\">"
		"<DATA>"
		"<KID>16bytebase64enckeydata==</KID>"
		"</DATA>"
		"</WRMHEADER>";

	ASSERT_TRUE(playReadyHelper->parsePssh((const unsigned char*)psshStr.data(), (uint32_t)psshStr.size()));

	// Check keyId and metadata, both of which should be based on the PSSH
	std::vector<uint8_t> keyId;
	playReadyHelper->getKey(keyId);

	const std::string expectedKeyId = "b5f2a6d7-dae6-eeb1-b87a-77247b275ab5";
	const std::string actualKeyId = std::string(keyId.begin(), keyId.begin() + keyId.size());

	ASSERT_EQ(expectedKeyId, actualKeyId);
	// Not expecting any metadata
	ASSERT_EQ("", playReadyHelper->getDrmMetaData());
}

TEST_F(AampDrmHelperTests, TestPlayReadyHelperGenerateLicenseRequest)
{
	DrmInfo drmInfo = createDrmInfo(eMETHOD_NONE, eMEDIAFORMAT_DASH, "", "",
									"9a04f079-9840-4286-ab92-e65be0885f95");
	ASSERT_TRUE(AampDrmHelperEngine::getInstance().hasDRM(drmInfo));
	std::shared_ptr<AampDrmHelper> playReadyHelper =
		AampDrmHelperEngine::getInstance().createHelper(drmInfo);

	AampChallengeInfo challengeInfo;
	challengeInfo.url = "http://challengeinfourl.example";
	std::string challengeData = "OCDM_CHALLENGE_DATA";

	challengeInfo.data =
		std::make_shared<DrmData>( challengeData.c_str(), challengeData.length());
	challengeInfo.accessToken = "ACCESS_TOKEN";

	// No PSSH parsed. Expecting data from the provided challenge to be given back in the request
	// info
	AampLicenseRequest licenseRequest1;
	playReadyHelper->generateLicenseRequest(challengeInfo, licenseRequest1);
	ASSERT_EQ(challengeInfo.url, licenseRequest1.url);
	ASSERT_EQ(challengeInfo.data->getData(), licenseRequest1.payload);

	// Parse a PSSH with a ckm:policy. This should cause generateLicenseRequest to return a JSON
	// payload
	AampLicenseRequest licenseRequest2;
	const std::string psshStr =
		"<WRMHEADER xmlns=\"http://schemas.microsoft.com/DRM/2007/03/PlayReadyHeader\" version=\"4.0.0.0\">"
		"<DATA>"
		"<KID>16bytebase64enckeydata==</KID>"
		"<ckm:policy xmlns:ckm=\"urn:ccp:ckm\">policy</ckm:policy>"
		"</DATA>"
		"</WRMHEADER>";
	ASSERT_TRUE(playReadyHelper->parsePssh((const unsigned char*)psshStr.data(), (uint32_t)psshStr.size()));

	playReadyHelper->generateLicenseRequest(challengeInfo, licenseRequest2);
	ASSERT_EQ(challengeInfo.url, licenseRequest2.url);

	TestUtilJsonWrapper jsonWrapper(licenseRequest2.payload);
	cJSON* postFieldObj = jsonWrapper.getJsonObj();
	ASSERT_TRUE(postFieldObj != nullptr);
	ASSERT_JSON_STR_VALUE(postFieldObj, "keySystem", "playReady");
	ASSERT_JSON_STR_VALUE(postFieldObj, "mediaUsage", "stream");
	// For the licenseRequest we expect the base64 encoding of the string
	// we placed in the challenge data: 'OCDM_CHALLENGE_DATA'
	ASSERT_JSON_STR_VALUE(postFieldObj, "licenseRequest", "T0NETV9DSEFMTEVOR0VfREFUQQ==");
	ASSERT_JSON_STR_VALUE(postFieldObj, "contentMetadata", "cG9saWN5");
	ASSERT_JSON_STR_VALUE(postFieldObj, "accessToken", challengeInfo.accessToken.c_str());

	// Finally, checking the license uri override works
	AampLicenseRequest licenseRequest3;
	const std::string fixedPrLicenseUrl = "http://prlicenseserver.example";
	licenseRequest3.url = fixedPrLicenseUrl;

	playReadyHelper->generateLicenseRequest(challengeInfo, licenseRequest3);
	ASSERT_EQ(fixedPrLicenseUrl, licenseRequest3.url);
}

TEST_F(AampDrmHelperTests, TestCompareHelpers)
{
	std::shared_ptr<AampDrmHelper> vgdrmHelper =
		AampDrmHelperEngine::getInstance().createHelper(createDrmInfo(
			eMETHOD_AES_128, eMEDIAFORMAT_HLS, "91701500000810367b131dd025ab0a7bd8d20c1314151600",
			"A68129D3-575B-4F1A-9CBA-3223846CF7C3"));
	ASSERT_TRUE(vgdrmHelper != nullptr);

	std::shared_ptr<AampDrmHelper> playreadyHelper =
		AampDrmHelperEngine::getInstance().createHelper(
			createDrmInfo(eMETHOD_AES_128, eMEDIAFORMAT_DASH, "file.key", "",
						  "9a04f079-9840-4286-ab92-e65be0885f95"));
	ASSERT_TRUE(playreadyHelper != nullptr);

	std::shared_ptr<AampDrmHelper> widevineHelper = AampDrmHelperEngine::getInstance().createHelper(
		createDrmInfo(eMETHOD_AES_128, eMEDIAFORMAT_DASH, "file.key", "",
					  "edef8ba9-79d6-4ace-a3c8-27dcd51d21ed"));
	ASSERT_TRUE(widevineHelper != nullptr);

	std::shared_ptr<AampDrmHelper> clearKeyHelperHls =
		AampDrmHelperEngine::getInstance().createHelper(
			createDrmInfo(eMETHOD_AES_128, eMEDIAFORMAT_HLS_MP4, "file.key", "",
						  "1077efec-c0b2-4d02-ace3-3c1e52e2fb4b"));
	ASSERT_TRUE(clearKeyHelperHls != nullptr);

	std::shared_ptr<AampDrmHelper> clearKeyHelperDash =
		AampDrmHelperEngine::getInstance().createHelper(
			createDrmInfo(eMETHOD_AES_128, eMEDIAFORMAT_DASH, "file.key", "",
						  "1077efec-c0b2-4d02-ace3-3c1e52e2fb4b"));
	ASSERT_TRUE(clearKeyHelperDash != nullptr);

	// All helpers should equal themselves
	ASSERT_TRUE(vgdrmHelper->compare(vgdrmHelper));
	ASSERT_TRUE(widevineHelper->compare(widevineHelper));
	ASSERT_TRUE(playreadyHelper->compare(playreadyHelper));
	ASSERT_TRUE(clearKeyHelperHls->compare(clearKeyHelperHls));

	// Different helper types, should not equal
	ASSERT_FALSE(vgdrmHelper->compare(playreadyHelper) || vgdrmHelper->compare(widevineHelper) ||
				 vgdrmHelper->compare(clearKeyHelperHls));
	ASSERT_FALSE(playreadyHelper->compare(vgdrmHelper) ||
				 playreadyHelper->compare(widevineHelper) ||
				 playreadyHelper->compare(clearKeyHelperHls));
	ASSERT_FALSE(widevineHelper->compare(vgdrmHelper) || widevineHelper->compare(playreadyHelper) ||
				 widevineHelper->compare(clearKeyHelperHls));

	// Same helper type but one is HLS and the other is DASH, so should not equal
	ASSERT_FALSE(clearKeyHelperHls->compare(clearKeyHelperDash));

	// Comparison against null helper, should not equal, should not cause a problem
	std::shared_ptr<AampDrmHelper> nullHelper;
	ASSERT_FALSE(clearKeyHelperHls->compare(nullHelper));

	std::shared_ptr<AampDrmHelper> vgdrmHelper2 = AampDrmHelperEngine::getInstance().createHelper(
		createDrmInfo(eMETHOD_AES_128, eMEDIAFORMAT_HLS,
					  "91701500000810387b131dd025ab0a7bd8d20c1314151600", // Different key
					  "A68129D3-575B-4F1A-9CBA-3223846CF7C3"));
	ASSERT_TRUE(vgdrmHelper != nullptr);

	// Different key, should not equal
	ASSERT_FALSE(vgdrmHelper->compare(vgdrmHelper2));

	std::shared_ptr<AampDrmHelper> playreadyHelper2 =
		AampDrmHelperEngine::getInstance().createHelper(
			createDrmInfo(eMETHOD_AES_128, eMEDIAFORMAT_DASH, "file.key", "",
						  "9a04f079-9840-4286-ab92-e65be0885f95"));
	ASSERT_TRUE(playreadyHelper2 != nullptr);

	const std::string pssh1 =
		"<WRMHEADER xmlns=\"http://schemas.microsoft.com/DRM/2007/03/PlayReadyHeader\" version=\"4.0.0.0\">"
		"<DATA>"
		"<KID>16bytebase64enckeydata==</KID>"
		"</DATA>"
		"</WRMHEADER>";
	playreadyHelper->parsePssh((const unsigned char*)pssh1.data(), (uint32_t)pssh1.size());
	playreadyHelper2->parsePssh((const unsigned char*)pssh1.data(), (uint32_t)pssh1.size());

	// Same key in the PSSH data, should equal
	ASSERT_TRUE(playreadyHelper->compare(playreadyHelper2));

	const std::string pssh2 =
		"<WRMHEADER xmlns=\"http://schemas.microsoft.com/DRM/2007/03/PlayReadyHeader\" version=\"4.0.0.0\">"
		"<DATA>"
		"<KID>differentbase64keydata==</KID>"
		"</DATA>"
		"</WRMHEADER>";
	playreadyHelper2->parsePssh((const unsigned char*)pssh2.data(), (uint32_t)pssh2.size());

	// Different key in the PSSH data, should not equal
	ASSERT_FALSE(playreadyHelper->compare(playreadyHelper2));

	// Create another PR helper, same details as PR helper 1
	std::shared_ptr<AampDrmHelper> playreadyHelper3 =
		AampDrmHelperEngine::getInstance().createHelper(
			createDrmInfo(eMETHOD_AES_128, eMEDIAFORMAT_DASH, "file.key", "",
						  "9a04f079-9840-4286-ab92-e65be0885f95"));
	ASSERT_TRUE(playreadyHelper3 != nullptr);

	// But no PSSH parsed for the 3rd PR helper, so shouldn't be equal
	ASSERT_FALSE(playreadyHelper->compare(playreadyHelper3));

	// Parse the same PSSH as used for 1, now should be equal
	playreadyHelper3->parsePssh((const unsigned char*)pssh1.data(), (uint32_t)pssh1.size());
	ASSERT_TRUE(playreadyHelper->compare(playreadyHelper3));

	// Finally keep the same key but add in metadata. Now PR helpers 1 & 3 shouldn't be equal
	const std::string pssh3 =
		"<WRMHEADER xmlns=\"http://schemas.microsoft.com/DRM/2007/03/PlayReadyHeader\" version=\"4.0.0.0\">"
		"<DATA>"
		"<KID>16bytebase64enckeydata==</KID>"
		"<ckm:policy xmlns:ckm=\"urn:ccp:ckm\">policy</ckm:policy>"
		"</DATA>"
		"</WRMHEADER>";
	playreadyHelper3->parsePssh((const unsigned char*)pssh3.data(), (uint32_t)pssh3.size());
	ASSERT_FALSE(playreadyHelper->compare(playreadyHelper3));
}
