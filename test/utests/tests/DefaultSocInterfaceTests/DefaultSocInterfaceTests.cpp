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
#include <gst/gst.h>
#include <glib-object.h>

// Unit under test
#include "vendor/default/DefaultSocInterface.h"

// Mock dependencies
#include "MockGLib.h"

using ::testing::_;
using ::testing::DoAll;
using ::testing::Return;
using ::testing::SetArgPointee;
using ::testing::StrictMock;
using ::testing::TestWithParam;
using ::testing::Values;
using ::testing::Combine;

// Test parameter structure
struct TestParams {
	bool isRialto;
	bool hasVideoSink;
	gboolean isMasterValue;
	bool expectedIsPlatformSegmentReady;
	bool expectedIsVideoMaster;
	std::string testName;
};

class DefaultSocInterfaceParameterizedTests : public TestWithParam<TestParams>
{
protected:
	DefaultSocInterface* mDefaultSocInterface;
	StrictMock<MockGLib>* mMockGLib;
	GstElement* mMockVideoSink;

	void SetUp() override
	{
		mDefaultSocInterface = new DefaultSocInterface();
		mMockGLib = new StrictMock<MockGLib>();
		g_mockGLib = mMockGLib;

		// Create a mock video sink element (just use a valid pointer)
		mMockVideoSink = reinterpret_cast<GstElement*>(malloc(1));
	}

	void TearDown() override
	{
		free(mMockVideoSink);

		delete mDefaultSocInterface;
		mDefaultSocInterface = nullptr;

		delete mMockGLib;
		g_mockGLib = nullptr;
	}
};

// Parameterized test for IsPlatformSegmentReady
TEST_P(DefaultSocInterfaceParameterizedTests, IsPlatformSegmentReady)
{
	TestParams params = GetParam();
	GstElement* videoSink = params.hasVideoSink ? mMockVideoSink : nullptr;

	// Set up mock expectations only when g_object_get should be called
	if (params.isRialto && params.hasVideoSink) {
		EXPECT_CALL(*mMockGLib, g_object_get(mMockVideoSink, _, _))
			.WillOnce(DoAll(SetArgPointee<2>(params.isMasterValue), Return()));
	}

	bool result = mDefaultSocInterface->IsPlatformSegmentReady(videoSink, params.isRialto);
	EXPECT_EQ(result, params.expectedIsPlatformSegmentReady)
		<< "Test case: " << params.testName;
}

// Parameterized test for IsVideoMaster
TEST_P(DefaultSocInterfaceParameterizedTests, IsVideoMaster)
{
	TestParams params = GetParam();
	GstElement* videoSink = params.hasVideoSink ? mMockVideoSink : nullptr;

	// Set up mock expectations only when g_object_get should be called
	if (params.isRialto && params.hasVideoSink) {
		EXPECT_CALL(*mMockGLib, g_object_get(mMockVideoSink, _, _))
			.WillOnce(DoAll(SetArgPointee<2>(params.isMasterValue), Return()));
	}

	bool result = mDefaultSocInterface->IsVideoMaster(videoSink, params.isRialto);
	EXPECT_EQ(result, params.expectedIsVideoMaster)
		<< "Test case: " << params.testName;
}

// Test data for parameterized tests
INSTANTIATE_TEST_SUITE_P(
	DefaultSocInterfaceTests,
	DefaultSocInterfaceParameterizedTests,
	Values(
		// isRialto, hasVideoSink, isMasterValue, expectedIsPlatformSegmentReady, expectedIsVideoMaster, testName
		TestParams{false, true,  TRUE,  false, true,  "RialtoFalse_WithVideoSink"},
		TestParams{false, false, TRUE,  false, true,  "RialtoFalse_NullVideoSink"},
		TestParams{true,  false, TRUE,  false, true,  "RialtoTrue_NullVideoSink"},
		TestParams{true,  true,  TRUE,  false, true,  "RialtoTrue_IsMasterTrue"},
		TestParams{true,  true,  FALSE, true,  false, "RialtoTrue_IsMasterFalse"}
	)
);

// Separate test class for multiple calls to ensure state independence
class DefaultSocInterfaceMultiCallTests : public ::testing::Test
{
protected:
	DefaultSocInterface* mDefaultSocInterface;
	StrictMock<MockGLib>* mMockGLib;
	GstElement* mMockVideoSink;

	void SetUp() override
	{
		mDefaultSocInterface = new DefaultSocInterface();
		mMockGLib = new StrictMock<MockGLib>();
		g_mockGLib = mMockGLib;

		// Create a mock video sink element (just use a valid pointer)
		mMockVideoSink = reinterpret_cast<GstElement*>(malloc(1));
	}

	void TearDown() override
	{
		free(mMockVideoSink);

		delete mDefaultSocInterface;
		mDefaultSocInterface = nullptr;

		delete mMockGLib;
		g_mockGLib = nullptr;
	}
};

// Test multiple calls to ensure state independence
TEST_F(DefaultSocInterfaceMultiCallTests, IsPlatformSegmentReady_MultipleCalls_StateIndependent)
{
	// First call: g_object_get sets isMaster = TRUE
	EXPECT_CALL(*mMockGLib, g_object_get(mMockVideoSink, _, _))
		.WillOnce(DoAll(SetArgPointee<2>(TRUE), Return()));
	bool result1 = mDefaultSocInterface->IsPlatformSegmentReady(mMockVideoSink, true);
	EXPECT_FALSE(result1);

	// Second call: g_object_get sets isMaster = FALSE
	EXPECT_CALL(*mMockGLib, g_object_get(mMockVideoSink, _, _))
		.WillOnce(DoAll(SetArgPointee<2>(FALSE), Return()));
	bool result2 = mDefaultSocInterface->IsPlatformSegmentReady(mMockVideoSink, true);
	EXPECT_TRUE(result2);
}

TEST_F(DefaultSocInterfaceMultiCallTests, IsVideoMaster_MultipleCalls_StateIndependent)
{
	// First call: g_object_get sets isMaster = TRUE
	EXPECT_CALL(*mMockGLib, g_object_get(mMockVideoSink, _, _))
		.WillOnce(DoAll(SetArgPointee<2>(TRUE), Return()));
	bool result1 = mDefaultSocInterface->IsVideoMaster(mMockVideoSink, true);
	EXPECT_TRUE(result1);

	// Second call: g_object_get sets isMaster = FALSE
	EXPECT_CALL(*mMockGLib, g_object_get(mMockVideoSink, _, _))
		.WillOnce(DoAll(SetArgPointee<2>(FALSE), Return()));
	bool result2 = mDefaultSocInterface->IsVideoMaster(mMockVideoSink, true);
	EXPECT_FALSE(result2);
}
