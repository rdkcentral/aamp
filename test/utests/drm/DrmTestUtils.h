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

#ifndef AAMP_DRM_TEST_UTILS_H
#define AAMP_DRM_TEST_UTILS_H

#include <vector>
#include <cstring>
#include <memory>
#include <cjson/cJSON.h>

#include <gmock/gmock.h>

#include "curlMocks.h"
#include "AampDRMSessionManager.h"

#ifdef USE_OPENCDM_ADAPTER
#include "AampHlsOcdmBridge.h"
#include "MockOpenCdm.h"

#define OCDM_SESSION ((OpenCDMSession*)0x0CD12345)
#define OCDM_SYSTEM ((OpenCDMSystem*)0x0CDACC12345)
#endif /* USE_OPENCDM_ADAPTER */

// Useful macros for checking JSON
#define ASSERT_JSON_STR_VALUE(o, p, e)                                                             \
	{                                                                                              \
		cJSON* jsonV = cJSON_GetObjectItem(o, p);                                                  \
		ASSERT_TRUE(jsonV != nullptr);                                                             \
		ASSERT_TRUE(cJSON_IsString(jsonV));                                                        \
		ASSERT_STREQ(e, cJSON_GetStringValue(jsonV));                                              \
	}

// For comparing memory buffers such as C-style arrays
MATCHER_P2(MemBufEq, buffer, elementCount, "")
{
	return std::memcmp(arg, buffer, elementCount * sizeof(buffer[0])) == 0;
}

class TestUtilJsonWrapper
{
public:
	TestUtilJsonWrapper(const std::string& jsonStr);
	TestUtilJsonWrapper(const char* jsonStr, size_t size)
		: TestUtilJsonWrapper(std::string(jsonStr, size)){};
	TestUtilJsonWrapper(const std::vector<uint8_t> jsonData)
		: TestUtilJsonWrapper(std::string((const char*)jsonData.data(), jsonData.size())){};
	~TestUtilJsonWrapper();

	cJSON* getJsonObj()
	{
		return mJsonObj;
	};

private:
	cJSON* mJsonObj;
};

struct TestCurlResponse
{
	std::string response;
	MockCurlOpts opts;
	unsigned int callCount;

	TestCurlResponse(std::string response) : response(response), callCount(0)
	{
	}

	std::vector<std::string> getHeaders() const
	{
		std::vector<std::string> headerList;

		for (int i = 0; i < opts.headerCount; ++i)
		{
			headerList.push_back(std::string(opts.headers[i]));
		}
		std::sort(headerList.begin(), headerList.end());
		return headerList;
	}
};

struct MockChallengeData
{
	std::string url;
	std::string challenge;

	MockChallengeData(std::string url = "", std::string challenge = "")
		: url(url), challenge(challenge)
	{
	}
};

class TestUtilDrm
{
private:
	PrivateInstanceAAMP* mAamp = nullptr;
	std::map<std::string, TestCurlResponse> mCurlResponses;
	MockChallengeData mMockChallengeData;
	std::unique_ptr<AampDRMSessionManager> mSessionManager;

public:
	TestUtilDrm(PrivateInstanceAAMP* privAamp);
	~TestUtilDrm();

	AampDRMSessionManager* getSessionManager();
#ifdef USE_OPENCDM_ADAPTER
	DrmSession* createDrmSessionForHelper(DrmHelperPtr drmHelper,
											  const char* keySystem);
	DrmSession* createDashDrmSession(const std::string testKeyData, const std::string psshStr,
										 DrmMetaDataEventPtr& event);
	DrmSession* createDashDrmSession(const std::vector<uint8_t> testKeyData,
										 const std::string psshStr, DrmMetaDataEventPtr& event);
	void setupChallengeCallbacks(const MockChallengeData& challengeData =
									 MockChallengeData("challenge.example", "OCDM_CHALLENGE_DATA"));
	void setupChallengeCallbacksForExternalLicense();
#endif /* USE_OPENCDM_ADAPTER */
	DrmMetaDataEventPtr createDrmMetaDataEvent();
	void setupCurlPerformResponse(std::string response);
	void setupCurlPerformResponses(const std::map<std::string, std::string>& responses);
	const TestCurlResponse* getCurlPerformResponse(std::string url);
};

#endif /* AAMP_DRM_TEST_UTILS_H */
