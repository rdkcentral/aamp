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

#include "DrmSessionFactory.h"
#include "DrmSessionManager.h"
#include "ClearKeyHelper.h"
#include "open_cdm.h"

#include "aampMocks.h"

#include "Fakeopencdm.h"
#include "curlMocks.h"

#include "DrmTestUtils.h"

#include "MockOpenCdm.h"
#include "MockPrivateInstanceAAMP.h"
#include "PlayerUtils.h"
using ::testing::_;
using ::testing::DoAll;
using ::testing::NiceMock;
using ::testing::Return;
using ::testing::SetArgPointee;
using ::testing::StrEq;

class DrmSessionTests : public ::testing::Test
{
protected:
	//PrivateInstanceAAMP *mAamp = nullptr;
	//TestUtilDrm *mUtils = nullptr;

	// The URL AAMP uses to fetch the session token
	const std::string mSessionTokenUrl = "http://localhost:50050/authService/getSessionToken";
	// A fixed session token response - any tests which require a session token can use this,
	// since the manager is singleton and will only ever fetch it once. Test order is
	// non-deterministic, so it's not guaranteed which test will actually trigger the fetch
	const std::string mSessionTokenResponse = "{\"token\":\"SESSIONTOKEN\", \"status\":0}";

	void SetUp() override
	{
		//MockAampReset();
		//MockCurlReset();
		MockOpenCdmReset();

		g_mockopencdm = new NiceMock<MockOpenCdm>();
		//g_mockPrivateInstanceAAMP = new NiceMock<MockPrivateInstanceAAMP>();
		//mAamp = new PrivateInstanceAAMP(gpGlobalConfig);
		//mUtils = new TestUtilDrm(mAamp);
	}

	void TearDown() override
	{
		//delete mUtils;
		//mUtils = nullptr;

		//delete mAamp;
		//mAamp = nullptr;

		//delete g_mockPrivateInstanceAAMP;
		//g_mockPrivateInstanceAAMP = nullptr;

		delete g_mockopencdm;
		g_mockopencdm = nullptr;

		//MockAampReset();
		//MockCurlReset();
		MockOpenCdmReset();
	}

public:
};

TEST_F(DrmSessionTests, TestClearKeyLicenseAcquisition)
{
	// Setup Curl and OpenCDM mocks. We expect that curl_easy_perform will be called to fetch
	// the key and that OpenCDM will be called to construct the session and handle the fetched key.
	std::string testKeyData = "TESTKEYDATA";
	//mUtils->setupCurlPerformResponse(testKeyData);
	//mUtils->setupChallengeCallbacks();

	// Begin test
	DrmInfo drmInfo;
	drmInfo.method = eMETHOD_AES_128;
	drmInfo.manifestURL = "http://example.com/assets/test.m3u8";
	drmInfo.keyURI = "file.key";
	std::shared_ptr<ClearKeyHelper> drmHelper =
		std::make_shared<ClearKeyHelper>(drmInfo);

	// We expect the key data to be transformed by the helper before being passed to
	// opencdm_session_update. Thus we call the helper ourselves here (with the data our mock Curl
	// will return) so we know what to expect. Note: testing of the transformation is done in the
	// helper tests, here we just want to ensure that whatever the helper returns is what OpenCDM
	// gets.
	const shared_ptr<DrmData> expectedDrmData =
		make_shared<DrmData>(testKeyData.c_str(), testKeyData.size());
	drmHelper->transformLicenseResponse(expectedDrmData);

	EXPECT_CALL(*g_mockopencdm, opencdm_session_update(OCDM_SESSION,
													   MemBufEq(expectedDrmData->getData().c_str(),
																expectedDrmData->getDataLength()),
													   expectedDrmData->getDataLength()))
		.WillOnce(Return(ERROR_NONE));

	DrmSession *drmSession = mUtils->createDrmSessionForHelper(drmHelper, "org.w3.clearkey");
	ASSERT_TRUE(drmSession != nullptr);
	ASSERT_STREQ("org.w3.clearkey", drmSession->getKeySystem().c_str());

	const MockCurlOpts *curlOpts = MockCurlGetOpts();
	// Key URL should been constructed based on manifestURL and keyURI from the DrmInfo
	ASSERT_STREQ("http://example.com/assets/file.key", curlOpts->url);
	// POST is used
	ASSERT_EQ(0L, curlOpts->httpGet);
}

TEST_F(DrmSessionTests, TestMultipleSessionsSameKey)
{
	std::string testKeyData = "TESTKEYDATA";
	mUtils->setupCurlPerformResponse(testKeyData);
	mUtils->setupChallengeCallbacks();

	DrmInfo drmInfo;
	drmInfo.method = eMETHOD_AES_128;
	drmInfo.manifestURL = "http://example.com/assets/test.m3u8";
	drmInfo.keyURI = "file.key";
	std::shared_ptr<ClearKeyHelper> drmHelper =
		std::make_shared<ClearKeyHelper>(drmInfo);

	const shared_ptr<DrmData> expectedDrmData =
		make_shared<DrmData>(testKeyData.c_str(), testKeyData.size());
	drmHelper->transformLicenseResponse(expectedDrmData);

	// Only 1 session update called expected, since a single session should be shared
	EXPECT_CALL(*g_mockopencdm, opencdm_session_update(OCDM_SESSION,
													   MemBufEq(expectedDrmData->getData().c_str(),
																expectedDrmData->getDataLength()),
													   expectedDrmData->getDataLength()))
		.WillOnce(Return(ERROR_NONE));

	// 1st time around - expecting standard session creation
	DrmSession *drmSession1 = mUtils->createDrmSessionForHelper(drmHelper, "org.w3.clearkey");
	ASSERT_TRUE(drmSession1 != nullptr);
	ASSERT_STREQ("org.w3.clearkey", drmSession1->getKeySystem().c_str());

	// 2nd time around - expecting the existing session will be shared, so no OCDM session created
	AampDRMLicenseManager *sessionManager = mUtils->getSessionManager();
	DrmMetaDataEventPtr event = mUtils->createDrmMetaDataEvent();

	EXPECT_CALL(*g_mockopencdm, opencdm_create_system).Times(0);
	EXPECT_CALL(*g_mockopencdm, opencdm_construct_session).Times(0);

	int err =-1;
	DrmSession *drmSession2 =
		sessionManager->createDrmSession(drmHelper,  mAamp, event , (int)eMEDIATYPE_VIDEO);
	ASSERT_EQ(drmSession1, drmSession2);

	// Clear out the sessions. Now a new OCDM session is expected again
	sessionManager->mDRMSessionManager->clearSessionData();

	EXPECT_CALL(*g_mockopencdm, opencdm_session_update(OCDM_SESSION,
													   MemBufEq(expectedDrmData->getData().c_str(),
																expectedDrmData->getDataLength()),
													   expectedDrmData->getDataLength()))
		.WillOnce(Return(ERROR_NONE));

	DrmSession *drmSession3 = mUtils->createDrmSessionForHelper(drmHelper, "org.w3.clearkey");
	ASSERT_TRUE(drmSession3 != nullptr);
	ASSERT_STREQ("org.w3.clearkey", drmSession3->getKeySystem().c_str());
}

TEST_F(DrmSessionTests, TestDashPlayReadySession)
{
	string prLicenseServerURL = "http://licenseserver.example/license";
	const std::string testKeyData = "TESTKEYDATA";
	const std::string testChallengeData = "PLAYREADY_CHALLENGE_DATA";

	// Setting a PlayReady license server URL in the global config. This
	// should get used to request the license
	gpGlobalConfig->SetConfigValue(AAMP_APPLICATION_SETTING, eAAMPConfig_PRLicenseServerUrl,
								   prLicenseServerURL);

	// Setting up Curl callbacks for the session token and license requests
	mUtils->setupCurlPerformResponses(
		{{mSessionTokenUrl, mSessionTokenResponse}, {prLicenseServerURL, testKeyData}});

	mUtils->setupChallengeCallbacks(MockChallengeData("playready.example", testChallengeData));

	// PSSH string which will get passed to the helper for parsing, so needs to be in valid format
	const std::string psshStr =
		"<WRMHEADER xmlns=\"http://schemas.microsoft.com/DRM/2007/03/PlayReadyHeader\" version=\"4.0.0.0\">"
		"<DATA>"
		"<KID>16bytebase64enckeydata==</KID>"
		"</DATA>"
		"</WRMHEADER>";

	DrmMetaDataEventPtr event = mUtils->createDrmMetaDataEvent();
	DrmSession *drmSession = mUtils->createDashDrmSession(testKeyData, psshStr, event);
	ASSERT_TRUE(drmSession != nullptr);
	ASSERT_STREQ("com.microsoft.playready", drmSession->getKeySystem().c_str());

	const TestCurlResponse *licenseResponse = mUtils->getCurlPerformResponse(prLicenseServerURL);
	ASSERT_EQ(1, licenseResponse->callCount);

	// Check the post data set on Curl, this should be the challenge data returned by OCDM
	ASSERT_EQ(testChallengeData.size(), licenseResponse->opts.postFieldSize);
	ASSERT_EQ(testChallengeData, licenseResponse->opts.postFields);
}

TEST_F(DrmSessionTests, TestDashPlayReadySessionNoCkmPolicy)
{
	string prLicenseServerURL = "http://licenseserver.example/license";
	std::string testKeyData = "TESTKEYDATA";

	// Setting a PlayReady license server URL in the global config. This
	// should get used to request the license
	gpGlobalConfig->SetConfigValue(AAMP_APPLICATION_SETTING, eAAMPConfig_PRLicenseServerUrl,
								   prLicenseServerURL);

	// Setting up Curl callbacks for the session token and license requests
	mUtils->setupCurlPerformResponses(
		{{mSessionTokenUrl, mSessionTokenResponse}, {prLicenseServerURL, testKeyData}});

	mUtils->setupChallengeCallbacks();

	// PSSH string which will get passed to the helper for parsing, so needs to be in valid format
	const std::string psshStr =
		"<WRMHEADER xmlns=\"http://schemas.microsoft.com/DRM/2007/03/PlayReadyHeader\" version=\"4.0.0.0\">"
		"<DATA>"
		"<KID>16bytebase64enckeydata==</KID>"
		"</DATA>"
		"</WRMHEADER>";

	DrmMetaDataEventPtr event = mUtils->createDrmMetaDataEvent();
	DrmSession *drmSession = mUtils->createDashDrmSession(testKeyData, psshStr, event);
	ASSERT_TRUE(drmSession != nullptr);
	ASSERT_STREQ("com.microsoft.playready", drmSession->getKeySystem().c_str());

	const MockCurlOpts *curlOpts = MockCurlGetOpts();

	// Check license URL from the global config was used
	std::string url = gpGlobalConfig->GetConfigValue(eAAMPConfig_PRLicenseServerUrl);
	ASSERT_STREQ(url.c_str(), curlOpts->url);

	// Check the post data set on Curl. Since we didn't pass any metadata (ckm:policy) in the init
	// data, we expect the challenge data returned by OCDM to just be used as-is
	ASSERT_TRUE(curlOpts->postFieldSize > 0);
	ASSERT_STREQ("OCDM_CHALLENGE_DATA", curlOpts->postFields);
}

TEST_F(DrmSessionTests, TestSessionBadChallenge)
{
	DrmInfo drmInfo;
	drmInfo.method = eMETHOD_AES_128;
	drmInfo.manifestURL = "http://example.com/assets/test.m3u8";
	drmInfo.keyURI = "file.key";
	std::shared_ptr<ClearKeyHelper> drmHelper =
		std::make_shared<ClearKeyHelper>(drmInfo);

	// Cause OpenCDM to return an empty challenge. This should cause an error
	mUtils->setupChallengeCallbacks(MockChallengeData("", ""));

	EXPECT_CALL(*g_mockopencdm, opencdm_create_system(StrEq("org.w3.clearkey")))
		.WillOnce(Return(OCDM_SYSTEM));
	EXPECT_CALL(*g_mockopencdm, opencdm_construct_session)
		.WillOnce(DoAll(SetArgPointee<9>(OCDM_SESSION), Return(ERROR_NONE)));

	int err = -1;
	DrmMetaDataEventPtr event = mUtils->createDrmMetaDataEvent();
	DrmSession *drmSession =
		mUtils->getSessionManager()->createDrmSession(drmHelper,  mAamp,event,  (int)eMEDIATYPE_VIDEO);
	ASSERT_EQ(nullptr, drmSession);
	ASSERT_EQ(AAMP_TUNE_DRM_CHALLENGE_FAILED, event->getFailure());
}

TEST_F(DrmSessionTests, TestSessionBadLicenseResponse)
{
	DrmInfo drmInfo;
	drmInfo.method = eMETHOD_AES_128;
	drmInfo.manifestURL = "http://example.com/assets/test.m3u8";
	drmInfo.keyURI = "file.key";
	std::shared_ptr<ClearKeyHelper> drmHelper =
		std::make_shared<ClearKeyHelper>(drmInfo);

	// Make curl return empty data for the key. This should cause an error
	mUtils->setupCurlPerformResponses({{"http://example.com/assets/file.key", ""}});

	EXPECT_CALL(*g_mockopencdm, opencdm_create_system(StrEq("org.w3.clearkey")))
		.WillOnce(Return(OCDM_SYSTEM));
	EXPECT_CALL(*g_mockopencdm, opencdm_construct_session)
		.WillOnce(DoAll(SetArgPointee<9>(OCDM_SESSION), Return(ERROR_NONE)));

	mUtils->setupChallengeCallbacks();

	DrmMetaDataEventPtr event = mUtils->createDrmMetaDataEvent();
	int err = -1;
	DrmSession *drmSession =
		mUtils->getSessionManager()->createDrmSession( drmHelper,  mAamp,event , (int)eMEDIATYPE_VIDEO);
	ASSERT_EQ(nullptr, drmSession);
	ASSERT_EQ(AAMP_TUNE_LICENCE_REQUEST_FAILED, event->getFailure());
}

TEST_F(DrmSessionTests, TestDashSessionBadPssh)
{
	std::string testKeyData = "TESTKEYDATA";

	// Pass a bad PSSH string. This should cause an error
	const std::string psshStr = "bad data with no KID";

	DrmMetaDataEventPtr event = mUtils->createDrmMetaDataEvent();
        void *ptr= static_cast<void*>(&event);
	int err =-1;
	DrmSession *drmSession = mUtils->getSessionManager()->mDRMSessionManager->createDrmSession(err,
		"9a04f079-9840-4286-ab92-e65be0885f95", eMEDIAFORMAT_DASH,
		(const unsigned char *)psshStr.c_str(), psshStr.length(), eMEDIATYPE_VIDEO, mAamp, ptr);
	ASSERT_EQ(nullptr, drmSession);
	AAMPTuneFailure val = mUtils->getSessionManager()->MapDrmToAampTuneFailure((DrmTuneFailure)err);
printf("val = %d", val);
         if(err != -1)
         {
                 event->setFailure(val);
         }

	ASSERT_EQ(AAMP_TUNE_CORRUPT_DRM_METADATA, event->getFailure());
}
