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
#include "_base64.h"
#include "DrmHelper.h"

struct CreateHelperTestData
{
	DrmMethod method;
	MediaFormat mediaFormat;
	std::string uri;
	std::string keyFormat;
	std::string systemUUID;
	std::vector<uint8_t> expectedKeyPayload;
};
#define ARRAY_SIZE(A) (sizeof(A)/sizeof(A[0]))

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

class DrmHelperTests : public ::testing::Test
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

TEST_F(DrmHelperTests, TestDrmIds)
{
	std::vector<std::string> expectedIds({
		"1077efec-c0b2-4d02-ace3-3c1e52e2fb4b", // ClearKey
		"edef8ba9-79d6-4ace-a3c8-27dcd51d21ed", // Widevine
		"9a04f079-9840-4286-ab92-e65be0885f95"	// PlayReady
	});
	std::sort(expectedIds.begin(), expectedIds.end());

	std::vector<std::string> actualIds;
	DrmHelperEngine::getInstance().getSystemIds(actualIds);
	std::sort(actualIds.begin(), actualIds.end());

	ASSERT_EQ(expectedIds, actualIds);
}

TEST_F(DrmHelperTests, TestCreateClearKeyHelper)
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

		ASSERT_TRUE(DrmHelperEngine::getInstance().hasDRM(drmInfo));

		std::shared_ptr<DrmHelper> clearKeyHelper =
			DrmHelperEngine::getInstance().createHelper(drmInfo);
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

TEST_F(DrmHelperTests, TestClearKeyHelperHlsInitDataCreation)
{
	DrmInfo drmInfo = createDrmInfo(eMETHOD_AES_128, eMEDIAFORMAT_HLS_MP4, "file.key",
									"urn:uuid:1077efec-c0b2-4d02-ace3-3c1e52e2fb4b");
	ASSERT_TRUE(DrmHelperEngine::getInstance().hasDRM(drmInfo));
	std::shared_ptr<DrmHelper> clearKeyHelper =
		DrmHelperEngine::getInstance().createHelper(drmInfo);

	std::vector<uint8_t> initData;
	clearKeyHelper->createInitData(initData);
}

TEST_F(DrmHelperTests, TestClearKeyHelperParsePssh)
{
	DrmInfo drmInfo = createDrmInfo(eMETHOD_AES_128, eMEDIAFORMAT_DASH, "file.key",
									"urn:uuid:1077efec-c0b2-4d02-ace3-3c1e52e2fb4b");
	ASSERT_TRUE(DrmHelperEngine::getInstance().hasDRM(drmInfo));
	std::shared_ptr<DrmHelper> clearKeyHelper =
		DrmHelperEngine::getInstance().createHelper(drmInfo);

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

TEST_F(DrmHelperTests, TestClearKeyHelperGenerateLicenseRequest)
{
	DrmInfo drmInfo = createDrmInfo(eMETHOD_AES_128, eMEDIAFORMAT_HLS_MP4, "file.key",
									"urn:uuid:1077efec-c0b2-4d02-ace3-3c1e52e2fb4b");
	drmInfo.manifestURL = "http://stream.example/hls/playlist.m3u8";
	ASSERT_TRUE(DrmHelperEngine::getInstance().hasDRM(drmInfo));
	std::shared_ptr<DrmHelper> clearKeyHelper =
		DrmHelperEngine::getInstance().createHelper(drmInfo);

	ChallengeInfo challengeInfo;
	challengeInfo.url = "http://challengeinfourl.example";
	LicenseRequest licenseRequest;

	// No ClearKey license URL in the license request, expect the URL to be
	// constructed from the information in the DrmInfo
	clearKeyHelper->generateLicenseRequest(challengeInfo, licenseRequest);
	ASSERT_EQ(LicenseRequest::POST, licenseRequest.method);
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
	std::shared_ptr<DrmHelper> clearKeyHelperNoKeyUri =
		DrmHelperEngine::getInstance().createHelper(drmInfoNoKeyUri);
	clearKeyHelperNoKeyUri->generateLicenseRequest(challengeInfo, licenseRequest);
	ASSERT_EQ(challengeInfo.url, licenseRequest.url);
}

TEST_F(DrmHelperTests, TestClearKeyHelperTransformHlsLicenseResponse)
{
	struct TransformLicenseResponseTestData
	{
		std::vector<uint8_t> keyResponse;
		std::string expectedEncodedKey;
	};

	DrmInfo drmInfo = createDrmInfo(eMETHOD_AES_128, eMEDIAFORMAT_HLS_MP4, "file.key",
									"urn:uuid:1077efec-c0b2-4d02-ace3-3c1e52e2fb4b");
	ASSERT_TRUE(DrmHelperEngine::getInstance().hasDRM(drmInfo));
	std::shared_ptr<DrmHelper> clearKeyHelper =
		DrmHelperEngine::getInstance().createHelper(drmInfo);

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
		std::shared_ptr<DrmData> drmData = std::make_shared<DrmData>( reinterpret_cast<const char*>(testCase.keyResponse.data()), testCase.keyResponse.size());
		clearKeyHelper->transformLicenseResponse(drmData);
	}
}

TEST_F(DrmHelperTests, TestTransformDashLicenseResponse)
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
		ASSERT_TRUE(DrmHelperEngine::getInstance().hasDRM(drmInfo));
		std::shared_ptr<DrmHelper> clearKeyHelper =
			DrmHelperEngine::getInstance().createHelper(drmInfo);
		char licenseResponse[] = {'D', 'A', 'S', 'H', 'L', 'I', 'C'};
		std::shared_ptr<DrmData> drmData =
			std::make_shared<DrmData>(licenseResponse, sizeof(licenseResponse));

		clearKeyHelper->transformLicenseResponse(drmData);
		ASSERT_EQ(sizeof(licenseResponse), drmData->getDataLength());
		ASSERT_EQ(std::string("DASHLIC"), drmData->getData());
	}
}

TEST_F(DrmHelperTests, TestCreatePlayReadyHelper)
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

		ASSERT_TRUE(DrmHelperEngine::getInstance().hasDRM(drmInfo));

		std::shared_ptr<DrmHelper> playReadyHelper =
			DrmHelperEngine::getInstance().createHelper(drmInfo);
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

TEST_F(DrmHelperTests, TestCreatePlayReadyHelperNegative)
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

		ASSERT_FALSE(DrmHelperEngine::getInstance().hasDRM(drmInfo));

		std::shared_ptr<DrmHelper> playReadyHelper =
			DrmHelperEngine::getInstance().createHelper(drmInfo);
		ASSERT_TRUE(playReadyHelper == nullptr);
	}
}

TEST_F(DrmHelperTests, TestWidevineHelperParsePsshDrmMetaData)
{
	struct
	{
		const char *psshData;
		const char *expectedKey[4];
		uint32_t protectionScheme;
	} testData[] =
	{
		{ "AAAANXBzc2gAAAAA7e+LqXnWSs6jyCfc1R0h7QAAABUyBnBvbGljeTjAxAdQlZrvOioCU0Q=", // psshData
			{},
			0
			// various deprecated fields:
			//Policy: 'policy'
			//Track Type: 'SD'
			//Crypto Period Index: 123456
			//Crypto Period: 204 weeks, 27 days, 21 hours, 33 minutes, 9 seconds
		},
		{ "AAAAJnBzc2gAAAAA7e+LqXnWSs6jyCfc1R0h7QAAAAZI89yVmwY=", // psshData
			{},
			'cens'
		},
		{ "AAAAJnBzc2gAAAAA7e+LqXnWSs6jyCfc1R0h7QAAAAZIscaJmwY=", // psshData
			{},
			'cbc1'
		},
		{ "AAAAPnBzc2gAAAAA7e+LqXnWSs6jyCfc1R0h7QAAAB4iFnNoYWthX2NlYzJmNjRhYTc4OTBhMTFI49yVmwY=", // psshData
			{
				"0x73,0x68,0x61,0x6B,0x61,0x5F,0x63,0x65,0x63,0x32,0x66,0x36,0x34,0x61,0x61,0x37,0x38,0x39,0x30,0x61,0x31,0x31"
			}, 'cenc'
			// Version 0, AES-CTR full sample encryption
			// no key ids!
			// Content ID = shaka_cec2f64aa7890a11
		},
		{ "AAAANHBzc2gBAAAA7e+LqXnWSs6jyCfc1R0h7QAAAAEttsSNMB9I6rt3G6eorJBCAAAAAA==", // psshData
			{
				"0x2D,0xB6,0xC4,0x8D,0x30,0x1F,0x48,0xEA,0xBB,0x77,0x1B,0xA7,0xA8,0xAC,0x90,0x42"
			}, 0
			// Version 1, single KeyID
		},
		{ "AAAAOHBzc2gAAAAA7e+LqXnWSs6jyCfc1R0h7QAAABgSEC22xI0wH0jqu3cbp6iskEJI49yVmwY=", // psshData
			{
				"0x2D,0xB6,0xC4,0x8D,0x30,0x1F,0x48,0xEA,0xBB,0x77,0x1B,0xA7,0xA8,0xAC,0x90,0x42"
			},'cenc'
			// Version 0, 'cenc'
		},
		{ "AAAAOHBzc2gAAAAA7e+LqXnWSs6jyCfc1R0h7QAAABgSEC22xI0wH0jqu3cbp6iskEJI88aJmwY=", // psshData
			{
				"0x2D,0xB6,0xC4,0x8D,0x30,0x1F,0x48,0xEA,0xBB,0x77,0x1B,0xA7,0xA8,0xAC,0x90,0x42"
			},'cbcs'
			// Version 0, 'cbcs'
		},
		{ "AAAARHBzc2gBAAAA7e+LqXnWSs6jyCfc1R0h7QAAAAIAAAAAAAAAAAAAAAAAAAAAEREREREREREREREREREREQAAAAA=", // psshData
			{
				"0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00",
				"0x11,0x11,0x11,0x11,0x11,0x11,0x11,0x11,0x11,0x11,0x11,0x11,0x11,0x11,0x11,0x11"
			},0
			// Version 1, two KeyIDs
		},
		{"AAAAP3Bzc2gAAAAA7e+LqXnWSs6jyCfc1R0h7QAAAB8SEFYWbwTmlTrikzbYc0PLv+IaBWV6ZHJtSPPGiZsG", // psshData
			{
				"0x56,0x16,0x6f,0x04,0xe6,0x95,0x3a,0xe2,0x93,0x36,0xd8,0x73,0x43,0xcb,0xbf,0xe2"
			},'cbcs'
			// Protection Scheme: 63 62 63 73 (cbcs)
			// Provider: ezdrm
		},
		{"AAAASnBzc2gAAAAA7e+LqXnWSs6jyCfc1R0h7QAAACoIARIQL38/m5MtSFqBGT0XY2KM4yIUTkJDVTA0Mzg3NjU0MDA1NjMwMDc=", // psshData
			{
				"0x2f,0x7f,0x3f,0x9b,0x93,0x2d,0x48,0x5a,0x81,0x19,0x3d,0x17,0x63,0x62,0x8c,0xe3"
			},0
			// includes 'author id'
			// algorithm: AES-CTR full sample encryption
			// Content ID: NBCU0438765400563007
		},
		{"AAAASnBzc2gAAAAA7e+LqXnWSs6jyCfc1R0h7QAAACoIARIQ7zHnTYhMQv+k86p90K3+rCIUTkJDVTkwMDAxOTI5MTUxMzAwMDM=", // psshData
			{
				"0xef,0x31,0xe7,0x4d,0x88,0x4c,0x42,0xff,0xa4,0xf3,0xaa,0x7d,0xd0,0xad,0xfe,0xac"
			},0
			// algorithm: AES-CTR full sample encryption
			// Content ID: NBCU9000192915130003
		},
		{"AAAAfXBzc2gAAAAA7e+LqXnWSs6jyCfc1R0h7QAAAF0SJDhlY2ZiOWZkLTNhODktNTNlMy05YjgwLTg4OTBmM2IyMzNmYiI1aW5kZW1hbmQuY29tSU5UTDAyMTgyMjIwMDA0MTQ5OTMtSU5NVjAyMTgyMjIwMDA0MTQ5OTM=", // psshData
			{
				"0x38,0x65,0x63,0x66,0x62,0x39,0x66,0x64,0x2D,0x33,0x61,0x38,0x39,0x2D,0x35,0x33,0x65,0x33,0x2D,0x39,0x62,0x38,0x30,0x2D,0x38,0x38,0x39,0x30,0x66,0x33,0x62,0x32,0x33,0x33,0x66,0x62"
			},0
			// Content ID: indemand.comINTL0218222000414993-INMV0218222000414993
			// Key ID is 36 bytes long instead of required 16
		},
		{"AAAAMnBzc2gAAAAA7e+LqXnWSs6jyCfc1R0h7QAAABISEG3fTdGQ1VJtmPL1WTLb1a8=", // psshData
			{
				"0x6d,0xdf,0x4d,0xd1,0x90,0xd5,0x52,0x6d,0x98,0xf2,0xf5,0x59,0x32,0xdb,0xd5,0xaf"
			},0
		},
		{"AAAA93Bzc2gAAAAA7e+LqXnWSs6jyCfc1R0h7QAAANcSJDQ3OTZiNjY3LWZkZjYtYjI0My04MTM4LTNmM2VmZmQzN2E0NRIkNjdlZGIwZjQtMzIyYS1hNDgyLWZlNmQtZThiNjZiYWFhOGU4EiRhZWYyZjE3YS0xOWUwLWQ2MTctMGI4Ny02MTdmNjQ5OWNlZjQSJGM2NzE5YWJjLTY4YzctMWYxZC0xOWRiLTUxMjU5YWY2MDJmZSI9dXBmYWl0aGFuZGZhbWlseS5jb21VUFRMMDAwMDAwMDAwODM3Mzg2MS1VUE1WMDAwMDAwMDAwODM3Mzg2MQ==", // psshData
			{
				"0x34,0x37,0x39,0x36,0x62,0x36,0x36,0x37,0x2D,0x66,0x64,0x66,0x36,0x2D,0x62,0x32,0x34,0x33,0x2D,0x38,0x31,0x33,0x38,0x2D,0x33,0x66,0x33,0x65,0x66,0x66,0x64,0x33,0x37,0x61,0x34,0x35",
				"0x36,0x37,0x65,0x64,0x62,0x30,0x66,0x34,0x2D,0x33,0x32,0x32,0x61,0x2D,0x61,0x34,0x38,0x32,0x2D,0x66,0x65,0x36,0x64,0x2D,0x65,0x38,0x62,0x36,0x36,0x62,0x61,0x61,0x61,0x38,0x65,0x38",
				"0x61,0x65,0x66,0x32,0x66,0x31,0x37,0x61,0x2D,0x31,0x39,0x65,0x30,0x2D,0x64,0x36,0x31,0x37,0x2D,0x30,0x62,0x38,0x37,0x2D,0x36,0x31,0x37,0x66,0x36,0x34,0x39,0x39,0x63,0x65,0x66,0x34",
				"0x63,0x36,0x37,0x31,0x39,0x61,0x62,0x63,0x2D,0x36,0x38,0x63,0x37,0x2D,0x31,0x66,0x31,0x64,0x2D,0x31,0x39,0x64,0x62,0x2D,0x35,0x31,0x32,0x35,0x39,0x61,0x66,0x36,0x30,0x32,0x66,0x65"
			},0
			// Content ID: upfaithandfamily.comUPTL0000000008373861-UPMV0000000008373861
			// Key ID is 36 bytes long instead of required 16
		},
		{"AAAAOHBzc2gAAAAA7e+LqXnWSs6jyCfc1R0h7QAAABgSEABFDiYaE6QUtKoKcLv0s6hI49yVmwY=", // psshData
			{
				"0x00,0x45,0x0e,0x26,0x1a,0x13,0xa4,0x14,0xb4,0xaa,0x0a,0x70,0xbb,0xf4,0xb3,0xa8"
			},'cenc'
			// Protection Scheme: 63 65 6E 63 (cenc)
			// AES-CTR full sample encryption
		}
	};

	for( int i=0; i<ARRAY_SIZE(testData); i++ )
	{
		const char *src = testData[i].psshData;
		size_t srcLen = strlen(src);
		size_t psshDataLen = 0;
		unsigned char *psshDataPtr = base64_Decode(src,&psshDataLen,srcLen);
		ASSERT_TRUE(psshDataPtr != nullptr);
		DrmInfo drmInfo = createDrmInfo(eMETHOD_AES_128, eMEDIAFORMAT_DASH, "file.key", "",
								"edef8ba9-79d6-4ace-a3c8-27dcd51d21ed");

		ASSERT_TRUE(DrmHelperEngine::getInstance().hasDRM(drmInfo));
		std::shared_ptr<DrmHelper> widevineHelper =
			DrmHelperEngine::getInstance().createHelper(drmInfo);
		ASSERT_TRUE(widevineHelper != nullptr);

		bool rc = widevineHelper->parsePssh(psshDataPtr, (uint32_t)psshDataLen);
		if( testData[i].expectedKey[0] )
		{ // at least one keyid expected
			ASSERT_TRUE(rc);
		}

		std::vector<uint8_t> initData;
		widevineHelper->createInitData(initData);
		for( int j=0; j<initData.size(); j++ )
		{
			ASSERT_EQ(psshDataPtr[j], initData[j] );
		}

		std::map<int, std::vector<uint8_t>> keyIDs;
		widevineHelper->getKeys(keyIDs);

		printf( "protection scheme=0x%08x\n", widevineHelper->getProtectionScheme() );
		ASSERT_EQ( widevineHelper->getProtectionScheme(), testData[i].protectionScheme );

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

TEST_F(DrmHelperTests, TestCreateWidevineHelper)
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

		ASSERT_TRUE(DrmHelperEngine::getInstance().hasDRM(drmInfo));

		std::shared_ptr<DrmHelper> widevineHelper =
			DrmHelperEngine::getInstance().createHelper(drmInfo);
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

TEST_F(DrmHelperTests, TestCreateWidevineHelperNegative)
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

		ASSERT_FALSE(DrmHelperEngine::getInstance().hasDRM(drmInfo));

		std::shared_ptr<DrmHelper> widevineHelper =
			DrmHelperEngine::getInstance().createHelper(drmInfo);
		ASSERT_TRUE(widevineHelper == nullptr);
	}
}

TEST_F(DrmHelperTests, TestPlayReadyHelperParsePssh)
{
	DrmInfo drmInfo = createDrmInfo(eMETHOD_NONE, eMEDIAFORMAT_DASH, "", "",
									"9a04f079-9840-4286-ab92-e65be0885f95");
	ASSERT_TRUE(DrmHelperEngine::getInstance().hasDRM(drmInfo));
	std::shared_ptr<DrmHelper> playReadyHelper =
		DrmHelperEngine::getInstance().createHelper(drmInfo);

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

TEST_F(DrmHelperTests, TestPlayReadyHelperParsePsshNoPolicy)
{
	// As before but with no ckm:policy in the PSSH data.
	// Should be OK but lead to empty metadata
	DrmInfo drmInfo = createDrmInfo(eMETHOD_NONE, eMEDIAFORMAT_DASH, "", "",
									"9a04f079-9840-4286-ab92-e65be0885f95");
	ASSERT_TRUE(DrmHelperEngine::getInstance().hasDRM(drmInfo));
	std::shared_ptr<DrmHelper> playReadyHelper =
		DrmHelperEngine::getInstance().createHelper(drmInfo);

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

TEST_F(DrmHelperTests, TestPlayReadyHelperGenerateLicenseRequest)
{
	DrmInfo drmInfo = createDrmInfo(eMETHOD_NONE, eMEDIAFORMAT_DASH, "", "",
									"9a04f079-9840-4286-ab92-e65be0885f95");
	ASSERT_TRUE(DrmHelperEngine::getInstance().hasDRM(drmInfo));
	std::shared_ptr<DrmHelper> playReadyHelper =
		DrmHelperEngine::getInstance().createHelper(drmInfo);

	ChallengeInfo challengeInfo;
	challengeInfo.url = "http://challengeinfourl.example";
	std::string challengeData = "OCDM_CHALLENGE_DATA";

	challengeInfo.data =
		std::make_shared<DrmData>( challengeData.c_str(), challengeData.length());
	challengeInfo.accessToken = "ACCESS_TOKEN";

	// No PSSH parsed. Expecting data from the provided challenge to be given back in the request
	// info
	LicenseRequest licenseRequest1;
	playReadyHelper->generateLicenseRequest(challengeInfo, licenseRequest1);
	ASSERT_EQ(challengeInfo.url, licenseRequest1.url);
	ASSERT_EQ(challengeInfo.data->getData(), licenseRequest1.payload);

	// Parse a PSSH with a ckm:policy. This should cause generateLicenseRequest to return a JSON
	// payload
	LicenseRequest licenseRequest2;
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

	// Finally, checking the license uri override works
	LicenseRequest licenseRequest3;
	const std::string fixedPrLicenseUrl = "http://prlicenseserver.example";
	licenseRequest3.url = fixedPrLicenseUrl;

	playReadyHelper->generateLicenseRequest(challengeInfo, licenseRequest3);
	ASSERT_EQ(fixedPrLicenseUrl, licenseRequest3.url);
}

TEST_F(DrmHelperTests, TestCompareHelpers)
{
	std::shared_ptr<DrmHelper> playreadyHelper =
		DrmHelperEngine::getInstance().createHelper(
			createDrmInfo(eMETHOD_AES_128, eMEDIAFORMAT_DASH, "file.key", "",
						  "9a04f079-9840-4286-ab92-e65be0885f95"));
	ASSERT_TRUE(playreadyHelper != nullptr);

	std::shared_ptr<DrmHelper> widevineHelper = DrmHelperEngine::getInstance().createHelper(
		createDrmInfo(eMETHOD_AES_128, eMEDIAFORMAT_DASH, "file.key", "",
					  "edef8ba9-79d6-4ace-a3c8-27dcd51d21ed"));
	ASSERT_TRUE(widevineHelper != nullptr);

	std::shared_ptr<DrmHelper> clearKeyHelperHls =
		DrmHelperEngine::getInstance().createHelper(
			createDrmInfo(eMETHOD_AES_128, eMEDIAFORMAT_HLS_MP4, "file.key", "",
						  "1077efec-c0b2-4d02-ace3-3c1e52e2fb4b"));
	ASSERT_TRUE(clearKeyHelperHls != nullptr);

	std::shared_ptr<DrmHelper> clearKeyHelperDash =
		DrmHelperEngine::getInstance().createHelper(
			createDrmInfo(eMETHOD_AES_128, eMEDIAFORMAT_DASH, "file.key", "",
						  "1077efec-c0b2-4d02-ace3-3c1e52e2fb4b"));
	ASSERT_TRUE(clearKeyHelperDash != nullptr);

	// All helpers should equal themselves
	ASSERT_TRUE(widevineHelper->compare(widevineHelper));
	ASSERT_TRUE(playreadyHelper->compare(playreadyHelper));
	ASSERT_TRUE(clearKeyHelperHls->compare(clearKeyHelperHls));

	// Different helper types, should not equal
	ASSERT_FALSE(playreadyHelper->compare(widevineHelper) ||
				 playreadyHelper->compare(clearKeyHelperHls));
	ASSERT_FALSE(widevineHelper->compare(playreadyHelper) ||
				 widevineHelper->compare(clearKeyHelperHls));

	// Same helper type but one is HLS and the other is DASH, so should not equal
	ASSERT_FALSE(clearKeyHelperHls->compare(clearKeyHelperDash));

	// Comparison against null helper, should not equal, should not cause a problem
	std::shared_ptr<DrmHelper> nullHelper;
	ASSERT_FALSE(clearKeyHelperHls->compare(nullHelper));

	std::shared_ptr<DrmHelper> playreadyHelper2 =
		DrmHelperEngine::getInstance().createHelper(
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
	std::shared_ptr<DrmHelper> playreadyHelper3 =
		DrmHelperEngine::getInstance().createHelper(
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
