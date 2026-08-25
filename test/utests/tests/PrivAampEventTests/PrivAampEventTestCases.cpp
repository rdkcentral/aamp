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
 * @file PrivAampEventTestCases.cpp
 * @brief Unit tests for PrivateInstanceAAMP event-related functionality.
 *
 * This test suite validates event generation and dispatching, particularly
 * SendDownloadErrorEvent. Unlike PrivAampTests, this suite includes the real
 * AampEvent.cpp implementation to verify actual event object behavior
 * (descriptions, error codes, retry flags) rather than relying on fakes.
 */

#include <cstdlib>
#include <iostream>
#include <string>

#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include <curl/curl.h>

#include "priv_aamp.h"
#include "AampEvent.h"
#include "AampDefine.h"
#include "AampConfig.h" // for gpGlobalConfig

#include "MockPrivateInstanceAAMP.h"
#include "MockAampConfig.h"
#include "MockAampEventManager.h"

using ::testing::_;
using ::testing::Return;
using ::testing::NiceMock;

AampConfig *gpGlobalConfig{nullptr};

/**
 * @class PrivAampEventTests
 * @brief Test fixture for PrivateInstanceAAMP event generation.
 *
 * Provides a minimal test environment with a real AAMP instance and mocked
 * EventManager to capture and validate dispatched events.
 */
class PrivAampEventTests : public ::testing::Test
{
protected:
	PrivateInstanceAAMP *p_aamp{nullptr};
	AampConfig *config{nullptr};

	void SetUp() override
	{
		config = new AampConfig();
		p_aamp = new PrivateInstanceAAMP(config);
		g_mockAampEventManager = std::make_shared<NiceMock<MockAampEventManager>>();
		g_mockAampConfig = std::make_shared<NiceMock<MockAampConfig>>();
	}

	void TearDown() override
	{
		g_mockAampEventManager.reset();
		g_mockAampConfig.reset();
		delete p_aamp;
		p_aamp = nullptr;
		delete config;
		config = nullptr;
	}
};

/**
 * @test SendErrorEvent_CorruptDrmData_EmitsCode51SubCode1
 * @brief SendErrorEvent(AAMP_TUNE_CORRUPT_DRM_DATA) must produce a
 *        MediaErrorEvent with code==51, subCode==1.
 */
TEST_F(PrivAampEventTests, SendErrorEvent_CorruptDrmData_EmitsCode51SubCode1)
{
	p_aamp->rate = AAMP_NORMAL_PLAY_RATE;
	AAMPEventPtr capturedEvent;
	EXPECT_CALL(*g_mockAampEventManager, SendEvent(AnEventOfType(AAMP_EVENT_TUNE_FAILED), AAMP_EVENT_ASYNC_MODE))
		.WillOnce(::testing::SaveArg<0>(&capturedEvent));

	p_aamp->SendErrorEvent(AAMP_TUNE_CORRUPT_DRM_DATA);

	ASSERT_NE(capturedEvent, nullptr);
	MediaErrorEventPtr errorEvent = std::dynamic_pointer_cast<MediaErrorEvent>(capturedEvent);
	ASSERT_NE(errorEvent, nullptr);
	EXPECT_EQ(errorEvent->getCode(),    51);
	EXPECT_EQ(errorEvent->getSubCode(),  1);
}

/**
 * @test SendErrorEvent_DeviceNotProvisioned_EmitsCode52SubCode1
 * @brief SendErrorEvent(AAMP_TUNE_DEVICE_NOT_PROVISIONED) must produce a
 *        MediaErrorEvent with code==52, subCode==1.
 */
TEST_F(PrivAampEventTests, SendErrorEvent_DeviceNotProvisioned_EmitsCode52SubCode1)
{
	p_aamp->rate = AAMP_NORMAL_PLAY_RATE;
	AAMPEventPtr capturedEvent;
	EXPECT_CALL(*g_mockAampEventManager, SendEvent(AnEventOfType(AAMP_EVENT_TUNE_FAILED), AAMP_EVENT_ASYNC_MODE))
		.WillOnce(::testing::SaveArg<0>(&capturedEvent));

	p_aamp->SendErrorEvent(AAMP_TUNE_DEVICE_NOT_PROVISIONED);

	ASSERT_NE(capturedEvent, nullptr);
	MediaErrorEventPtr errorEvent = std::dynamic_pointer_cast<MediaErrorEvent>(capturedEvent);
	ASSERT_NE(errorEvent, nullptr);
	EXPECT_EQ(errorEvent->getCode(),    52);
	EXPECT_EQ(errorEvent->getSubCode(),  1);
}

/**
 * @test SendErrorEvent_HdcpComplianceError_EmitsCode53SubCode1
 * @brief SendErrorEvent(AAMP_TUNE_HDCP_COMPLIANCE_ERROR) must produce a
 *        MediaErrorEvent with code==53, subCode==1.
 */
TEST_F(PrivAampEventTests, SendErrorEvent_HdcpComplianceError_EmitsCode53SubCode1)
{
	p_aamp->rate = AAMP_NORMAL_PLAY_RATE;
	AAMPEventPtr capturedEvent;
	EXPECT_CALL(*g_mockAampEventManager, SendEvent(AnEventOfType(AAMP_EVENT_TUNE_FAILED), AAMP_EVENT_ASYNC_MODE))
		.WillOnce(::testing::SaveArg<0>(&capturedEvent));

	p_aamp->SendErrorEvent(AAMP_TUNE_HDCP_COMPLIANCE_ERROR);

	ASSERT_NE(capturedEvent, nullptr);
	MediaErrorEventPtr errorEvent = std::dynamic_pointer_cast<MediaErrorEvent>(capturedEvent);
	ASSERT_NE(errorEvent, nullptr);
	EXPECT_EQ(errorEvent->getCode(),    53);
	EXPECT_EQ(errorEvent->getSubCode(),  1);
}

/**
 * @brief Verify SendStalledErrorEvent flushes curl-store FDs and dispatches tune-failed event.
 *
 * SendStalledErrorEvent must mark curl-store FDs for flush so subsequent tune
 * attempts do not reuse stale curl handles.
 */
TEST_F(PrivAampEventTests, SendStalledErrorEvent_SetsCurlStoreFlushAndDispatchesTuneFailed)
{
	p_aamp->rate = AAMP_NORMAL_PLAY_RATE;
	p_aamp->mIsFlushFdsInCurlStore = false;
	EXPECT_CALL(*g_mockAampConfig, GetConfigValue(eAAMPConfig_StallTimeoutMS)).WillOnce(Return(10000));
	EXPECT_CALL(*g_mockAampConfig, GetConfigValue(eAAMPConfig_StallErrorCode)).WillOnce(Return(7600));

	AAMPEventPtr capturedEvent;
	EXPECT_CALL(*g_mockAampEventManager,
			SendEvent(AnEventOfType(AAMP_EVENT_TUNE_FAILED),
					  AAMP_EVENT_ASYNC_MODE))
		.WillOnce(::testing::SaveArg<0>(&capturedEvent));

	p_aamp->SendStalledErrorEvent();

	MediaErrorEventPtr errorEvent = std::dynamic_pointer_cast<MediaErrorEvent>(capturedEvent);
	ASSERT_NE(errorEvent, nullptr);
	EXPECT_EQ(errorEvent->getCode(),    7600);
	EXPECT_EQ(errorEvent->getSubCode(),  1);
	EXPECT_TRUE(p_aamp->mIsFlushFdsInCurlStore) << "Playback stall must force curl-store FD flush for later recovery";
}


/**
 * @brief Verify SendDownloadErrorEvent dispatches AAMP_EVENT_TUNE_FAILED with correct descriptions for curl errors.
 *
 * When a curl error code (< 100) is passed, SendDownloadErrorEvent must:
 * 1. Dispatch AAMP_EVENT_TUNE_FAILED
 * 2. Include "Curl Error Code" in the description
 * 3. Maintain retry status as true (default)
 * 4. Set the curl store flush flag
 */
TEST_F(PrivAampEventTests, SendDownloadErrorEvent_CurlError_DispatchesWithCurlDescription)
{
	p_aamp->rate = AAMP_NORMAL_PLAY_RATE;
	AAMPEventPtr capturedEvent;
	EXPECT_CALL(*g_mockAampEventManager, SendEvent(AnEventOfType(AAMP_EVENT_TUNE_FAILED), AAMP_EVENT_ASYNC_MODE))
		.WillOnce(::testing::SaveArg<0>(&capturedEvent));

	p_aamp->SendDownloadErrorEvent(AAMP_TUNE_MANIFEST_REQ_FAILED, CURLE_COULDNT_RESOLVE_HOST);

	ASSERT_NE(capturedEvent, nullptr);
	MediaErrorEventPtr errorEvent = std::dynamic_pointer_cast<MediaErrorEvent>(capturedEvent);
	ASSERT_NE(errorEvent, nullptr);
	// Verify the description contains "Curl Error Code"
	std::string description(errorEvent->getDescription());
	EXPECT_NE(description.find("Curl Error Code"), std::string::npos) << "Description should mention 'Curl Error Code' for curl errors";
	EXPECT_NE(description.find(std::to_string(CURLE_COULDNT_RESOLVE_HOST)), std::string::npos) << "Description should include the error code";
	EXPECT_TRUE(errorEvent->shouldRetry()) << "Curl errors should be retry-able by default";
	EXPECT_TRUE(p_aamp->mIsFlushFdsInCurlStore) << "Curl errors should set the curl store flush flag";
}

/**
 * @brief Verify SendDownloadErrorEvent dispatches AAMP_EVENT_TUNE_FAILED with correct descriptions for HTTP errors.
 *
 * When an HTTP error code (>= 100, not special-cased) is passed, SendDownloadErrorEvent must:
 * 1. Dispatch AAMP_EVENT_TUNE_FAILED
 * 2. Include "Http Error Code" in the description
 * 3. Maintain retry status as true (default)
 * 4. Set the curl store flush flag
 */
TEST_F(PrivAampEventTests, SendDownloadErrorEvent_HttpError_DispatchesWithHttpDescription)
{
	p_aamp->rate = AAMP_NORMAL_PLAY_RATE;
	AAMPEventPtr capturedEvent;
	EXPECT_CALL(*g_mockAampEventManager, SendEvent(AnEventOfType(AAMP_EVENT_TUNE_FAILED), AAMP_EVENT_ASYNC_MODE))
		.WillOnce(::testing::SaveArg<0>(&capturedEvent));

	p_aamp->SendDownloadErrorEvent(AAMP_TUNE_MANIFEST_REQ_FAILED, 500);

	ASSERT_NE(capturedEvent, nullptr);
	MediaErrorEventPtr errorEvent = std::dynamic_pointer_cast<MediaErrorEvent>(capturedEvent);
	ASSERT_NE(errorEvent, nullptr);
	// Verify the description contains "Http Error Code"
	std::string description(errorEvent->getDescription());
	EXPECT_NE(description.find("Http Error Code"), std::string::npos) << "Description should mention 'Http Error Code' for HTTP errors";
	EXPECT_NE(description.find("500"), std::string::npos) << "Description should include the HTTP status code";
	EXPECT_TRUE(errorEvent->shouldRetry()) << "HTTP 5xx errors should be retry-able";
	EXPECT_TRUE(p_aamp->mIsFlushFdsInCurlStore) << "HTTP errors should set the curl store flush flag";
}

/**
 * @brief Verify SendDownloadErrorEvent maps HTTP 404 to AAMP_TUNE_CONTENT_NOT_FOUND.
 *
 * When HTTP 404 is passed, SendDownloadErrorEvent must remap the tune failure
 * from the input value to AAMP_TUNE_CONTENT_NOT_FOUND before dispatching the event.
 * Also, the mIsFlushFdsInCurlStore flag should be set to true, and the description should still mention the 404 status code.
 */
TEST_F(PrivAampEventTests, SendDownloadErrorEvent_Http404_RemapsToContentNotFound)
{
	p_aamp->rate = AAMP_NORMAL_PLAY_RATE;
	AAMPEventPtr capturedEvent;
	EXPECT_CALL(*g_mockAampEventManager, SendEvent(AnEventOfType(AAMP_EVENT_TUNE_FAILED), AAMP_EVENT_ASYNC_MODE))
		.WillOnce(::testing::SaveArg<0>(&capturedEvent));

	p_aamp->SendDownloadErrorEvent(AAMP_TUNE_MANIFEST_REQ_FAILED, 404);

	ASSERT_NE(capturedEvent, nullptr);
	MediaErrorEventPtr errorEvent = std::dynamic_pointer_cast<MediaErrorEvent>(capturedEvent);
	ASSERT_NE(errorEvent, nullptr);
	// HTTP 404 should be remapped to AAMP_TUNE_CONTENT_NOT_FOUND (code 20)
	EXPECT_EQ(errorEvent->getCode(), 20) << "HTTP 404 should be remapped to AAMP_TUNE_CONTENT_NOT_FOUND (code 20)";
	std::string description(errorEvent->getDescription()); 
	EXPECT_NE(description.find("404"), std::string::npos) << "Description should still mention the 404 status code";
	EXPECT_TRUE(errorEvent->shouldRetry()) << "404 errors should be retry-able (at least once)";
	EXPECT_TRUE(p_aamp->mIsFlushFdsInCurlStore) << "HTTP 404 error should set the curl store flush flag";
}

/**
 * @brief Verify SendDownloadErrorEvent sets retry=false for HTTP 421 (Fog power-saving mode).
 *
 * HTTP 421 indicates the Fog is in power-saving mode and immediate retry is futile.
 * SendDownloadErrorEvent must set shouldRetry() to false for this error.
 * Also set the mIsFlushFdsInCurlStore flag to true, and the description should mention the 421 status code.
 */
TEST_F(PrivAampEventTests, SendDownloadErrorEvent_Http421_DisablesRetry)
{
	p_aamp->rate = AAMP_NORMAL_PLAY_RATE;
	AAMPEventPtr capturedEvent;
	EXPECT_CALL(*g_mockAampEventManager, SendEvent(AnEventOfType(AAMP_EVENT_TUNE_FAILED), AAMP_EVENT_ASYNC_MODE))
		.WillOnce(::testing::SaveArg<0>(&capturedEvent));

	p_aamp->SendDownloadErrorEvent(AAMP_TUNE_MANIFEST_REQ_FAILED, 421);

	ASSERT_NE(capturedEvent, nullptr);
	MediaErrorEventPtr errorEvent = std::dynamic_pointer_cast<MediaErrorEvent>(capturedEvent);
	ASSERT_NE(errorEvent, nullptr);
	std::string description(errorEvent->getDescription());
	EXPECT_NE(description.find("421"), std::string::npos) << "Description should include the 421 status code";
	EXPECT_FALSE(errorEvent->shouldRetry()) << "HTTP 421 should not be retry-able";
	EXPECT_TRUE(p_aamp->mIsFlushFdsInCurlStore) << "HTTP 421 error should set the curl store flush flag";
}

/**
 * @brief Verify SendDownloadErrorEvent handles CURLE_OPERATION_TIMEDOUT specially.
 *
 * CURLE_OPERATION_TIMEDOUT should produce a description mentioning "Download time expired".
 * mIsFlushFdsInCurlStore should be set to true, and the error should still be retry-able.
 */
TEST_F(PrivAampEventTests, SendDownloadErrorEvent_OperationTimedout_SpecialDescription)
{
	p_aamp->rate = AAMP_NORMAL_PLAY_RATE;
	AAMPEventPtr capturedEvent;
	EXPECT_CALL(*g_mockAampEventManager, SendEvent(AnEventOfType(AAMP_EVENT_TUNE_FAILED), AAMP_EVENT_ASYNC_MODE))
		.WillOnce(::testing::SaveArg<0>(&capturedEvent));

	p_aamp->SendDownloadErrorEvent(AAMP_TUNE_FRAGMENT_DOWNLOAD_FAILURE, CURLE_OPERATION_TIMEDOUT);

	ASSERT_NE(capturedEvent, nullptr);
	MediaErrorEventPtr errorEvent = std::dynamic_pointer_cast<MediaErrorEvent>(capturedEvent);
	ASSERT_NE(errorEvent, nullptr);
	std::string description(errorEvent->getDescription());
	EXPECT_NE(description.find("Download time expired"), std::string::npos) << "Description should mention 'Download time expired' for CURLE_OPERATION_TIMEDOUT";
	EXPECT_TRUE(errorEvent->shouldRetry()) << "Timeout errors should be retry-able";
	EXPECT_TRUE(p_aamp->mIsFlushFdsInCurlStore) << "CURLE_OPERATION_TIMEDOUT should set the curl store flush flag";
}

/**
 * @brief Verify SendDownloadErrorEvent handles unknown/out-of-range tuneFailure gracefully.
 *
 * When an invalid tuneFailure code is passed, SendDownloadErrorEvent must dispatch
 * AAMP_TUNE_FAILURE_UNKNOWN without crashing. mIsFlushFdsInCurlStore should remain false,
 * and the description should indicate an unknown failure.
 */
TEST_F(PrivAampEventTests, SendDownloadErrorEvent_UnknownTuneFailure_DispatchesFailureUnknown)
{
	p_aamp->rate = AAMP_NORMAL_PLAY_RATE;
	AAMPEventPtr capturedEvent;
	EXPECT_CALL(*g_mockAampEventManager, SendEvent(AnEventOfType(AAMP_EVENT_TUNE_FAILED), AAMP_EVENT_ASYNC_MODE))
		.WillOnce(::testing::SaveArg<0>(&capturedEvent));

	// Pass an out-of-range tuneFailure code
	p_aamp->SendDownloadErrorEvent(static_cast<AAMPTuneFailure>(9999), 500);

	ASSERT_NE(capturedEvent, nullptr);
	MediaErrorEventPtr errorEvent = std::dynamic_pointer_cast<MediaErrorEvent>(capturedEvent);
	ASSERT_NE(errorEvent, nullptr);
	// AAMP_TUNE_FAILURE_UNKNOWN maps to code 100
	EXPECT_EQ(errorEvent->getCode(), 100) << "Unknown tuneFailure should be remapped to AAMP_TUNE_FAILURE_UNKNOWN (code 100)";
	EXPECT_FALSE(p_aamp->mIsFlushFdsInCurlStore) << "Unknown tuneFailure should not set the curl store flush flag";
}
