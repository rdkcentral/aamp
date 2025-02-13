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

#include <string>
#include <iostream>
#include <vector>
#include <iomanip>
#include <algorithm>
#include <iterator>
#include <sstream>

#include "DrmTestUtils.h"
#include "AampUtils.h"

#ifdef USE_OPENCDM_ADAPTER
#include "Fakeopencdm.h"
#endif

using ::testing::DoAll;
using ::testing::Return;
using ::testing::SetArgPointee;
using ::testing::StrEq;

TestUtilJsonWrapper::TestUtilJsonWrapper(const std::string& jsonStr)
{
	mJsonObj = cJSON_Parse(jsonStr.c_str());
}

TestUtilJsonWrapper::~TestUtilJsonWrapper()
{
	cJSON_Delete(mJsonObj);
}

TestUtilDrm::TestUtilDrm(PrivateInstanceAAMP* privAamp)
	: mAamp(privAamp)
{
	mSessionManager = aamp_utils::make_unique<AampDRMSessionManager>(2 /* maxDrmSessions */, mAamp);
}

TestUtilDrm::~TestUtilDrm()
{
	mSessionManager->clearSessionData();
}

AampDRMSessionManager* TestUtilDrm::getSessionManager()
{
	return mSessionManager.get();
}

#ifdef USE_OPENCDM_ADAPTER
DrmSession* TestUtilDrm::createDrmSessionForHelper(DrmHelperPtr drmHelper,
													   const char* keySystem)
{
	AampDRMSessionManager* sessionManager = getSessionManager();
	DrmMetaDataEventPtr event = createDrmMetaDataEvent();

	EXPECT_CALL(*g_mockopencdm, opencdm_create_system(StrEq(keySystem)))
		.WillOnce(Return(OCDM_SYSTEM));
	EXPECT_CALL(*g_mockopencdm, opencdm_construct_session)
		.WillOnce(DoAll(SetArgPointee<9>(OCDM_SESSION), Return(ERROR_NONE)));

	DrmSession* drmSession =
		sessionManager->createDrmSession(drmHelper, event, mAamp, eMEDIATYPE_VIDEO);
	return drmSession;
}

DrmSession* TestUtilDrm::createDashDrmSession(const std::string testKeyData,
												  const std::string psshStr,
												  DrmMetaDataEventPtr& event)
{
	std::vector<uint8_t> testKeyDataVec(testKeyData.begin(), testKeyData.end());
	return createDashDrmSession(testKeyDataVec, psshStr, event);
}

DrmSession* TestUtilDrm::createDashDrmSession(const std::vector<uint8_t> testKeyData,
												  const std::string psshStr,
												  DrmMetaDataEventPtr& event)
{
	AampDRMSessionManager* sessionManager = getSessionManager();

	EXPECT_CALL(*g_mockopencdm, opencdm_create_system(StrEq("com.microsoft.playready")))
		.WillOnce(Return(OCDM_SYSTEM));
	EXPECT_CALL(*g_mockopencdm, opencdm_construct_session)
		.WillOnce(DoAll(SetArgPointee<9>(OCDM_SESSION), Return(ERROR_NONE)));
	EXPECT_CALL(*g_mockopencdm, opencdm_session_update(
									OCDM_SESSION, MemBufEq(testKeyData.data(), testKeyData.size()),
									testKeyData.size()))
		.WillOnce(Return(ERROR_NONE));

	DrmSession* drmSession = sessionManager->createDrmSession(
		"9a04f079-9840-4286-ab92-e65be0885f95", eMEDIAFORMAT_DASH,
		(const unsigned char*)psshStr.c_str(), psshStr.length(), eMEDIATYPE_VIDEO, mAamp, event);

	return drmSession;
}

void TestUtilDrm::setupChallengeCallbacks(const MockChallengeData& challengeData)
{
	mMockChallengeData = challengeData;
	MockOpenCdmCallbacks callbacks = {nullptr, nullptr};
	callbacks.constructSessionCallback =
		[](const MockOpenCdmSessionInfo* mockSessionInfo, void* mockUserData)
	{
		MockChallengeData* pChallengeData = (MockChallengeData*)mockUserData;
		// OpenCDM should come back to us with a URL + challenge payload.
		// The content of these shouldn't matter for us, since we use the request info from the DRM
		// helper instead
		const char* url = pChallengeData->url.c_str();
		const std::string challenge = pChallengeData->challenge;
		mockSessionInfo->callbacks.process_challenge_callback(
			(OpenCDMSession*)mockSessionInfo->session, mockSessionInfo->userData, url,
			(const uint8_t*)challenge.c_str(), challenge.size());
	};

	callbacks.sessionUpdateCallback = [](const MockOpenCdmSessionInfo* mockSessionInfo,
										 const uint8_t keyMessage[], const uint16_t keyLength)
	{
		mockSessionInfo->callbacks.key_update_callback((OpenCDMSession*)mockSessionInfo->session,
													   mockSessionInfo->userData, keyMessage,
													   keyLength);
		mockSessionInfo->callbacks.keys_updated_callback((OpenCDMSession*)mockSessionInfo->session,
														 mockSessionInfo->userData);
	};
	MockOpenCdmSetCallbacks(callbacks, &mMockChallengeData);
}

void TestUtilDrm::setupChallengeCallbacksForExternalLicense()
{
	MockOpenCdmCallbacks callbacks = {nullptr, nullptr};
	callbacks.constructSessionCallback =
		[](const MockOpenCdmSessionInfo* mockSessionInfo, void* mockUserData)
	{
		// OpenCDM should come back to us with a URL + challenge payload.
		// The content of these shouldn't matter for us, since we use the request info from the DRM
		// helper instead
		const char* url = "test";
		uint8_t challenge[] = {'A'};
		mockSessionInfo->callbacks.process_challenge_callback(
			(OpenCDMSession*)mockSessionInfo->session, mockSessionInfo->userData, url, challenge,
			1);

		// For DRM's which perform license acquisition outside the AampDRMSessionManager
		// context(PlayerLicenseRequest::DRM_RETRIEVE) there wont be an opencdm_session_update
		// call,hence trigger the keys_updated_callback as well within this callback
		mockSessionInfo->callbacks.key_update_callback((OpenCDMSession*)mockSessionInfo->session,
													   mockSessionInfo->userData, nullptr, 0);
		mockSessionInfo->callbacks.keys_updated_callback((OpenCDMSession*)mockSessionInfo->session,
														 mockSessionInfo->userData);
	};

	MockOpenCdmSetCallbacks(callbacks, nullptr);
}
#endif /* USE_OPENCDM_ADAPTER */

DrmMetaDataEventPtr TestUtilDrm::createDrmMetaDataEvent()
{
	return std::make_shared<DrmMetaDataEvent>(AAMP_TUNE_FAILURE_UNKNOWN, "", 0, 0, false, "");
}

void TestUtilDrm::setupCurlPerformResponse(std::string response)
{
	static string responseStr = response;

	MockCurlSetPerformCallback(
		[](CURL* curl, MockCurlWriteCallback writeCallback, void* writeData, void* userData)
		{ writeCallback((char*)responseStr.c_str(), 1, responseStr.size(), writeData); },
		this);
}

void TestUtilDrm::setupCurlPerformResponses(const std::map<std::string, std::string>& responses)
{
	for (auto& response : responses)
	{
		mCurlResponses.emplace(response.first, TestCurlResponse(response.second));
	}

	MockCurlSetPerformCallback(
		[](CURL* curl, MockCurlWriteCallback writeCallback, void* writeData, void* userData)
		{
			const auto* responseMap = (const std::map<std::string, TestCurlResponse>*)userData;
			const MockCurlOpts* curlOpts = MockCurlGetOpts();

			// Check if there is a response setup for this URL
			auto iter = responseMap->find(curlOpts->url);
			if (iter != responseMap->end())
			{
				TestCurlResponse* curlResponse = const_cast<TestCurlResponse*>(&iter->second);
				curlResponse->opts =
					*curlOpts; // Taking a copy of the opts so we can check them later
				curlResponse->callCount++;
				// Issue the write callback with the user-provided response
				writeCallback((char*)curlResponse->response.c_str(), 1,
							  curlResponse->response.size(), writeData);
			}
		},
		&mCurlResponses);
}

const TestCurlResponse* TestUtilDrm::getCurlPerformResponse(std::string url)
{
	auto iter = mCurlResponses.find(url);
	if (iter != mCurlResponses.end())
	{
		return &iter->second;
	}

	return nullptr;
}
