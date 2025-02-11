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
#include <iomanip>
#include <algorithm>
#include <iterator>
#include <memory>

#ifdef USE_SECCLIENT
#include "aampDrmSessionFactory.h"
#include "DRMSessionManager.h"
#include "AampClearKeyHelper.h"
#include "open_cdm.h"

#include "aampMocks.h"

#include "Fakeopencdm.h"
#include "curlMocks.h"

#include "DrmTestUtils.h"

#include "MockOpenCdm.h"
#include "MockSecureClient.h"
#include "MockPrivateInstanceAAMP.h"
#endif // USE_SECCLIENT

using ::testing::_;
using ::testing::DoAll;
using ::testing::NiceMock;
using ::testing::Return;
using ::testing::SetArgPointee;
using ::testing::StrEq;


class AampDrmSessionTests : public ::testing::Test
{
#ifdef USE_SECCLIENT
protected:
	PrivateInstanceAAMP *mAamp = nullptr;
	AampLogManager mLogging;
	TestUtilDrm *mUtils = nullptr;

	// The URL AAMP uses to fetch the session token
	const std::string mSessionTokenUrl = "http://localhost:50050/authService/getSessionToken";
	// A fixed session token response - any tests which require a session token can use this,
	// since the manager is singleton and will only ever fetch it once. Test order is
	// non-deterministic, so it's not guaranteed which test will actually trigger the fetch
	const std::string mSessionTokenResponse = "{\"token\":\"SESSIONTOKEN\", \"status\":0}";

	void SetUp() override
	{
		MockAampReset();
		MockCurlReset();
		MockOpenCdmReset();

		g_mockopencdm = new NiceMock<MockOpenCdm>();
		g_mocksecclient = new NiceMock<MockSecureClient>();
		g_mockPrivateInstanceAAMP = new NiceMock<MockPrivateInstanceAAMP>();
		mAamp = new PrivateInstanceAAMP(gpGlobalConfig);
		mUtils = new TestUtilDrm(mAamp);
	}

	void TearDown() override
	{
		delete mUtils;
		mUtils = nullptr;

		delete mAamp;
		mAamp = nullptr;

		delete g_mockPrivateInstanceAAMP;
		g_mockPrivateInstanceAAMP = nullptr;

		delete g_mocksecclient;
		g_mocksecclient = nullptr;

		delete g_mockopencdm;
		g_mockopencdm = nullptr;

		MockAampReset();
		MockCurlReset();
		MockOpenCdmReset();
	}

public:
#endif // USE_SECCLIENT
};

TEST_F(AampDrmSessionTests, TestDashPlayReadySessionSecClient)
{
#ifdef USE_SECCLIENT
	std::string prLicenseServerURL = "http://licenseserver.example/license";
	std::string expectedServiceHostUrl = "licenseserver.example";

	// Setting a PlayReady license server URL in the global config. This
	// should get used to request the license
	gpGlobalConfig->SetConfigValue(AAMP_APPLICATION_SETTING, eAAMPConfig_PRLicenseServerUrl,
								   prLicenseServerURL);

	// Setting up Curl repsonse for the session token
	mUtils->setupCurlPerformResponses({{mSessionTokenUrl, mSessionTokenResponse}});

	mUtils->setupChallengeCallbacks();

	// PSSH string which will get passed to the helper for parsing, so needs to be in valid format
	const std::string psshStr =
		"<WRMHEADER xmlns=\"http://schemas.microsoft.com/DRM/2007/03/PlayReadyHeader\" version=\"4.0.0.0\">"
		"<DATA>"
		"<KID>16bytebase64enckeydata==</KID>"
		"<ckm:policy xmlns:ckm=\"urn:ccp:ckm\">policy</ckm:policy>"
		"</DATA>"
		"</WRMHEADER>";

	std::string expectedKeyData = "TESTSECKEYDATA";
	size_t respLength = expectedKeyData.length();
	const char *respPtr = expectedKeyData.c_str();

	// The license should be acquired using the secure client, rather than curl
	EXPECT_CALL(*g_mocksecclient,
				SecClient_AcquireLicense(StrEq(expectedServiceHostUrl.c_str()), _, _))
		.WillOnce(DoAll(SetArgPointee<1>(const_cast<char *>(respPtr)), SetArgPointee<2>(respLength),
						Return(0)));

	DrmMetaDataEventPtr event = mUtils->createDrmMetaDataEvent();
	AampDrmSession *drmSession = mUtils->createDashDrmSession(expectedKeyData, psshStr, event);
	ASSERT_TRUE(drmSession != nullptr);
	ASSERT_STREQ("com.microsoft.playready", drmSession->getKeySystem().c_str());
#else
	GTEST_SKIP() << "Skipping test due to Secure Client headers not present";
#endif // USE_SECCLIENT
}
