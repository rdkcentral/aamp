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
 * @file AsyncTuneAbortTests.cpp
 * @brief Unit tests for the async-tune early-abort API introduced in VPAAMP-965.
 *
 * Covers:
 *  - IsAsyncTuneAbortSupported(): DASH-linear with async enabled returns true;
 *    other format/type/config combinations return false.
 *  - IsAsyncTuneAbortRequired() (no-arg): abort flag gates the result.
 *  - IsAsyncTuneAbortRequired(url, typeStr): URL/string-based overload.
 *  - Consistency: both IsAsyncTuneAbortRequired overloads agree for equivalent
 *    inputs, verifying that IsAsyncTuneSupportedForType is the single source of
 *    truth used by both paths.
 */

#include <gtest/gtest.h>
#include <gmock/gmock.h>

#include "priv_aamp.h"
#include "AampConfig.h"
#include "MockAampConfig.h"

using ::testing::_;
using ::testing::NiceMock;
using ::testing::Return;

// Representative DASH and HLS URLs used across tests.
static constexpr const char* kDashUrl = "http://example.com/stream.mpd";
static constexpr const char* kHlsUrl  = "http://example.com/stream.m3u8";

/**
 * @class AsyncTuneAbortTests
 * @brief Fixture for testing the async-tune abort API on PrivateInstanceAAMP.
 *
 * The fixture constructs a real PrivateInstanceAAMP (backed by the real
 * priv_aamp.cpp) with NiceMock dependencies so that all config queries return
 * safe defaults.  Tests directly set the public member fields that control
 * abort eligibility (mMediaFormat, mContentType, mAsyncTuneEnabled) and use
 * SetEarlyAbortRequestFlag() for the abort flag.
 */
class AsyncTuneAbortTests : public ::testing::Test
{
protected:
	PrivateInstanceAAMP *mAamp{};

	void SetUp() override
	{
		if (gpGlobalConfig == nullptr)
		{
			gpGlobalConfig = new AampConfig();
		}
		g_mockAampConfig = std::make_shared<NiceMock<MockAampConfig>>();
		ON_CALL(*g_mockAampConfig, IsConfigSet(_)).WillByDefault(Return(false));
		ON_CALL(*g_mockAampConfig,
			GetConfigValue(testing::Matcher<AAMPConfigSettingInt>(_)))
			.WillByDefault(Return(0));
		ON_CALL(*g_mockAampConfig,
			GetConfigValue(testing::Matcher<AAMPConfigSettingFloat>(_)))
			.WillByDefault(Return(0.0));
		ON_CALL(*g_mockAampConfig,
			GetConfigValue(testing::Matcher<AAMPConfigSettingString>(_)))
			.WillByDefault(Return(""));

		mAamp = new PrivateInstanceAAMP(gpGlobalConfig);
	}

	void TearDown() override
	{
		delete mAamp;
		mAamp = nullptr;
		g_mockAampConfig.reset();
	}

	/** Helper: configure the instance as DASH-linear with async-tune on. */
	void SetupDashLinearAsync()
	{
		mAamp->mMediaFormat    = eMEDIAFORMAT_DASH;
		mAamp->SetContentType("LINEAR_TV");
		mAamp->mAsyncTuneEnabled = true;
	}
};

// ---------------------------------------------------------------------------
// IsAsyncTuneAbortSupported
// ---------------------------------------------------------------------------

/**
 * @test IsAsyncTuneAbortSupported_DashLinearAsyncOn_ReturnsTrue
 * @brief The only combination that should return true.
 */
TEST_F(AsyncTuneAbortTests, IsAsyncTuneAbortSupported_DashLinearAsyncOn_ReturnsTrue)
{
	SetupDashLinearAsync();
	EXPECT_TRUE(mAamp->IsAsyncTuneAbortSupported());
}

/**
 * @test IsAsyncTuneAbortSupported_AsyncTuneDisabled_ReturnsFalse
 * @brief Async-tune disabled gates the result even for the right format/type.
 */
TEST_F(AsyncTuneAbortTests, IsAsyncTuneAbortSupported_AsyncTuneDisabled_ReturnsFalse)
{
	mAamp->mMediaFormat    = eMEDIAFORMAT_DASH;
	mAamp->SetContentType("LINEAR_TV");
	mAamp->mAsyncTuneEnabled = false;
	EXPECT_FALSE(mAamp->IsAsyncTuneAbortSupported());
}

/**
 * @test IsAsyncTuneAbortSupported_HlsLinear_ReturnsFalse
 * @brief HLS is not an abortable format.
 */
TEST_F(AsyncTuneAbortTests, IsAsyncTuneAbortSupported_HlsLinear_ReturnsFalse)
{
	mAamp->mMediaFormat    = eMEDIAFORMAT_HLS;
	mAamp->SetContentType("LINEAR_TV");
	mAamp->mAsyncTuneEnabled = true;
	EXPECT_FALSE(mAamp->IsAsyncTuneAbortSupported());
}

/**
 * @test IsAsyncTuneAbortSupported_DashVod_ReturnsFalse
 * @brief VOD content type is not abortable.
 */
TEST_F(AsyncTuneAbortTests, IsAsyncTuneAbortSupported_DashVod_ReturnsFalse)
{
	mAamp->mMediaFormat    = eMEDIAFORMAT_DASH;
	mAamp->SetContentType("VOD");
	mAamp->mAsyncTuneEnabled = true;
	EXPECT_FALSE(mAamp->IsAsyncTuneAbortSupported());
}

// ---------------------------------------------------------------------------
// IsAsyncTuneAbortRequired (no-arg)
// ---------------------------------------------------------------------------

/**
 * @test IsAsyncTuneAbortRequired_AbortFlagSet_ReturnsTrue
 * @brief Abort flag + supported format → abort required.
 */
TEST_F(AsyncTuneAbortTests, IsAsyncTuneAbortRequired_AbortFlagSet_ReturnsTrue)
{
	SetupDashLinearAsync();
	mAamp->SetEarlyAbortRequestFlag(true);
	EXPECT_TRUE(mAamp->IsAsyncTuneAbortRequired());
}

/**
 * @test IsAsyncTuneAbortRequired_AbortFlagClear_ReturnsFalse
 * @brief Even the right format/type returns false when no abort is requested.
 */
TEST_F(AsyncTuneAbortTests, IsAsyncTuneAbortRequired_AbortFlagClear_ReturnsFalse)
{
	SetupDashLinearAsync();
	mAamp->SetEarlyAbortRequestFlag(false);
	EXPECT_FALSE(mAamp->IsAsyncTuneAbortRequired());
}

/**
 * @test IsAsyncTuneAbortRequired_UnsupportedFormat_ReturnsFalse
 * @brief Abort flag set, but HLS → abort not required.
 */
TEST_F(AsyncTuneAbortTests, IsAsyncTuneAbortRequired_UnsupportedFormat_ReturnsFalse)
{
	mAamp->mMediaFormat    = eMEDIAFORMAT_HLS;
	mAamp->SetContentType("LINEAR_TV");
	mAamp->mAsyncTuneEnabled = true;
	mAamp->SetEarlyAbortRequestFlag(true);
	EXPECT_FALSE(mAamp->IsAsyncTuneAbortRequired());
}

// ---------------------------------------------------------------------------
// IsAsyncTuneAbortRequired (url, contentTypeString)
// ---------------------------------------------------------------------------

/**
 * @test IsAsyncTuneAbortRequired_UrlArg_DashLinear_AbortFlagSet_ReturnsTrue
 * @brief DASH URL + "LINEAR_TV" + abort flag → abort required.
 */
TEST_F(AsyncTuneAbortTests, IsAsyncTuneAbortRequired_UrlArg_DashLinear_AbortFlagSet_ReturnsTrue)
{
	mAamp->mAsyncTuneEnabled = true;
	mAamp->SetEarlyAbortRequestFlag(true);
	EXPECT_TRUE(mAamp->IsAsyncTuneAbortRequired(kDashUrl, "LINEAR_TV"));
}

/**
 * @test IsAsyncTuneAbortRequired_UrlArg_DashLinear_AbortFlagClear_ReturnsFalse
 * @brief Abort flag clear → always false, regardless of URL/type.
 */
TEST_F(AsyncTuneAbortTests, IsAsyncTuneAbortRequired_UrlArg_DashLinear_AbortFlagClear_ReturnsFalse)
{
	mAamp->mAsyncTuneEnabled = true;
	mAamp->SetEarlyAbortRequestFlag(false);
	EXPECT_FALSE(mAamp->IsAsyncTuneAbortRequired(kDashUrl, "LINEAR_TV"));
}

/**
 * @test IsAsyncTuneAbortRequired_UrlArg_HlsLinear_ReturnsFalse
 * @brief HLS URL is not abortable even when abort flag is set.
 */
TEST_F(AsyncTuneAbortTests, IsAsyncTuneAbortRequired_UrlArg_HlsLinear_ReturnsFalse)
{
	mAamp->mAsyncTuneEnabled = true;
	mAamp->SetEarlyAbortRequestFlag(true);
	EXPECT_FALSE(mAamp->IsAsyncTuneAbortRequired(kHlsUrl, "LINEAR_TV"));
}

/**
 * @test IsAsyncTuneAbortRequired_UrlArg_DashVod_ReturnsFalse
 * @brief DASH URL + "VOD" content type is not abortable.
 */
TEST_F(AsyncTuneAbortTests, IsAsyncTuneAbortRequired_UrlArg_DashVod_ReturnsFalse)
{
	mAamp->mAsyncTuneEnabled = true;
	mAamp->SetEarlyAbortRequestFlag(true);
	EXPECT_FALSE(mAamp->IsAsyncTuneAbortRequired(kDashUrl, "VOD"));
}

/**
 * @test IsAsyncTuneAbortRequired_UrlArg_AsyncTuneDisabled_ReturnsFalse
 * @brief AsyncTune config disabled gates the URL-arg overload too.
 */
TEST_F(AsyncTuneAbortTests, IsAsyncTuneAbortRequired_UrlArg_AsyncTuneDisabled_ReturnsFalse)
{
	mAamp->mAsyncTuneEnabled = false;
	mAamp->SetEarlyAbortRequestFlag(true);
	EXPECT_FALSE(mAamp->IsAsyncTuneAbortRequired(kDashUrl, "LINEAR_TV"));
}

// ---------------------------------------------------------------------------
// Consistency: both overloads must agree for equivalent inputs
// ---------------------------------------------------------------------------

/**
 * @test Consistency_BothOverloads_AgreeForDashLinear
 * @brief The no-arg and URL-arg overloads must return the same value when the
 *        stored state matches the URL/type parameters.  This is the regression
 *        guard for the single-source-of-truth refactoring: if the criteria in
 *        IsAsyncTuneSupportedForType ever diverge from a copy-pasted check in
 *        the URL-arg path, this test will catch it.
 */
TEST_F(AsyncTuneAbortTests, Consistency_BothOverloads_AgreeForDashLinear)
{
	SetupDashLinearAsync();
	mAamp->SetEarlyAbortRequestFlag(true);

	bool noArgResult  = mAamp->IsAsyncTuneAbortRequired();
	bool urlArgResult = mAamp->IsAsyncTuneAbortRequired(kDashUrl, "LINEAR_TV");

	EXPECT_EQ(noArgResult, urlArgResult);
	EXPECT_TRUE(noArgResult);
}

/**
 * @test Consistency_BothOverloads_AgreeForHlsLinear
 * @brief Both overloads return false for HLS linear.
 */
TEST_F(AsyncTuneAbortTests, Consistency_BothOverloads_AgreeForHlsLinear)
{
	mAamp->mMediaFormat    = eMEDIAFORMAT_HLS;
	mAamp->SetContentType("LINEAR_TV");
	mAamp->mAsyncTuneEnabled = true;
	mAamp->SetEarlyAbortRequestFlag(true);

	bool noArgResult  = mAamp->IsAsyncTuneAbortRequired();
	bool urlArgResult = mAamp->IsAsyncTuneAbortRequired(kHlsUrl, "LINEAR_TV");

	EXPECT_EQ(noArgResult, urlArgResult);
	EXPECT_FALSE(noArgResult);
}

/**
 * @test Consistency_BothOverloads_AbortFlagClear
 * @brief Both overloads return false when the abort flag is not set,
 *        even for the supported DASH-linear format.
 */
TEST_F(AsyncTuneAbortTests, Consistency_BothOverloads_AbortFlagClear)
{
	SetupDashLinearAsync();
	mAamp->SetEarlyAbortRequestFlag(false);

	EXPECT_FALSE(mAamp->IsAsyncTuneAbortRequired());
	EXPECT_FALSE(mAamp->IsAsyncTuneAbortRequired(kDashUrl, "LINEAR_TV"));
}
