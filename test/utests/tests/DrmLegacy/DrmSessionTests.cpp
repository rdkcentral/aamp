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

#include <vector>
#include <iostream>
#include <iomanip>
#include <algorithm>
#include <iterator>
#include <memory>
#include <cjson/cJSON.h>

#include "drmsessionfactory.h"
#include "AampDRMSessionManager.h"
#include "priv_aamp.h"

#include <gtest/gtest.h>

#include "aampMocks.h"
#include "curlMocks.h"

#include "DrmTestUtils.h"

class AampLegacyDrmSessionTests : public ::testing::Test
{
protected:
	PrivateInstanceAAMP *mAamp = nullptr;
	AampLogManager mLogging;
	TestUtilDrm *mUtils = nullptr;

	void SetUp() override
	{
		MockAampReset();
		MockCurlReset();

		mAamp = new PrivateInstanceAAMP(gpGlobalConfig);
		mUtils = new TestUtilDrm(mAamp);
	}

	void TearDown() override
	{
		delete mUtils;
		mUtils = nullptr;

		delete mAamp;
		mAamp = nullptr;

		MockAampReset();
		MockCurlReset();
	}

public:
};

TEST_F(AampLegacyDrmSessionTests, TestCreateClearkeySession)
{
	AampDRMSessionManager *sessionManager = mUtils->getSessionManager();

	cJSON *keysObj = cJSON_CreateObject();
	cJSON *keyInstanceObj = cJSON_CreateObject();
	cJSON_AddStringToObject(keyInstanceObj, "alg", "cbc");
	cJSON_AddStringToObject(keyInstanceObj, "k", "_u3wDe7erb7v8Lqt8A3QDQ");
	cJSON_AddStringToObject(keyInstanceObj, "kid", "_u3wDe7erb7v8Lqt8A3QDQ");
	cJSON *keysArr = cJSON_AddArrayToObject(keysObj, "keys");
	cJSON_AddItemToArray(keysArr, keyInstanceObj);

	char *keyResponse = cJSON_PrintUnformatted(keysObj);
	mUtils->setupCurlPerformResponse(keyResponse);
	cJSON_free(keyResponse);
	cJSON_Delete(keysObj);

	const unsigned char initData[] = {
		0x00, 0x00, 0x00, 0x34, 0x70, 0x73, 0x73, 0x68, 0x01, 0x00, 0x00, 0x00, 0x10,
		0x77, 0xef, 0xec, 0xc0, 0xb2, 0x4d, 0x02, 0xac, 0xe3, 0x3c, 0x1e, 0x52, 0xe2,
		0xfb, 0x4b, 0x00, 0x00, 0x00, 0x01, 0xfe, 0xed, 0xf0, 0x0d, 0xee, 0xde, 0xad,
		0xbe, 0xef, 0xf0, 0xba, 0xad, 0xf0, 0x0d, 0xd0, 0x0d, 0x00, 0x00, 0x00, 0x00};

	DrmMetaDataEventPtr aampEvent =
		std::make_shared<DrmMetaDataEvent>(AAMP_TUNE_FAILURE_UNKNOWN, "", 0, 0, false, "");

	// Setting a ClearKey license server URL in the global config.
	// This should get used to request the license.
	std::string ckLicenseServerURL = "http://licenseserver.example/license";
	gpGlobalConfig->SetConfigValue(AAMP_APPLICATION_SETTING, eAAMPConfig_CKLicenseServerUrl,
								   ckLicenseServerURL);

	DrmSession *drmSession = sessionManager->createDrmSession(
		"1077efec-c0b2-4d02-ace3-3c1e52e2fb4b", eMEDIAFORMAT_DASH, initData, sizeof(initData),
		eMEDIATYPE_VIDEO, mAamp, aampEvent, NULL, true);
	ASSERT_TRUE(drmSession != NULL);

	// Check license URL from the global config was used
	const MockCurlOpts *curlOpts = MockCurlGetOpts();
	ASSERT_STREQ(ckLicenseServerURL.c_str(), curlOpts->url);
}
