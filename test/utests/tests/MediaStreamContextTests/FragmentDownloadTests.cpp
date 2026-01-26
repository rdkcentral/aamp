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
#include "MediaStreamContext.h"
#include "fragmentcollector_mpd.h"
#include "isobmff/isobmffbuffer.h"
#include "AampCacheHandler.h"
#include "priv_aamp.h"
#include "AampDRMLicPreFetcherInterface.h"
#include "AampConfig.h"
#include "MockAampConfig.h"
#include "MockMediaTrack.h"
#include "MockStreamAbstractionAAMP.h"
#include "MockPrivateInstanceAAMP.h"
#include "fragmentcollector_mpd.h"
#include "StreamAbstractionAAMP.h"

using namespace testing;
// Named constants for clarity
static constexpr bool CHUNK_MODE_ENABLED{true};
static constexpr bool CHUNK_MODE_DISABLED{false};
static constexpr bool PTS_RESTAMP_ENABLED{true};
static constexpr bool PTS_RESTAMP_DISABLED{false};

/*
 * Test cases for FragmentDownloadTests
 * These tests are designed to validate the behavior of the MediaStreamContext class
 * when handling fragment downloads.
 */
class FragmentDownloadTests : public testing::Test
{
protected:
	void SetUp() override
	{
		if (gpGlobalConfig == nullptr)
		{
			gpGlobalConfig = new AampConfig();
		}
		mPrivateInstanceAAMP = new PrivateInstanceAAMP(gpGlobalConfig);
		mStreamAbstractionAAMP_MPD = new StreamAbstractionAAMP_MPD(mPrivateInstanceAAMP, 123.45, 12.34);
		mMediaStreamContext = new MediaStreamContext(eTRACK_VIDEO, mStreamAbstractionAAMP_MPD, mPrivateInstanceAAMP, "SAMPLETEXT");
		g_mockAampConfig = new NiceMock<MockAampConfig>();
		g_mockMediaTrack = new StrictMock<MockMediaTrack>();
		g_mockStreamAbstractionAAMP = new NiceMock<MockStreamAbstractionAAMP>(mPrivateInstanceAAMP);
		g_mockPrivateInstanceAAMP = new StrictMock<MockPrivateInstanceAAMP>();
	}

	void TearDown() override
	{
		delete mPrivateInstanceAAMP;
		mPrivateInstanceAAMP = nullptr;

		delete mStreamAbstractionAAMP_MPD;
		mStreamAbstractionAAMP_MPD = nullptr;

		delete mMediaStreamContext;
		mMediaStreamContext = nullptr;

		delete g_mockAampConfig;
		g_mockAampConfig = nullptr;

		delete g_mockMediaTrack;
		g_mockMediaTrack = nullptr;

		delete g_mockStreamAbstractionAAMP;
		g_mockStreamAbstractionAAMP = nullptr;

		delete g_mockPrivateInstanceAAMP;
		g_mockPrivateInstanceAAMP = nullptr;
	}

public:
	StreamAbstractionAAMP_MPD *mStreamAbstractionAAMP_MPD;
	PrivateInstanceAAMP *mPrivateInstanceAAMP;
	MediaStreamContext *mMediaStreamContext;
};


/**
 * @brief Test case for OnFragmentDownloadSuccess with null active download info
 */
TEST_F(FragmentDownloadTests, OnFragmentDownloadSuccess_NullActiveDownloadInfo)
{
	mMediaStreamContext->mActiveDownloadInfo = nullptr;
	DownloadInfoPtr dlInfo = std::make_shared<DownloadInfo>();
	mMediaStreamContext->OnFragmentDownloadSuccess(dlInfo);
	// Expect no crash or exception
}

/**
 * @brief Struct to hold parameterized test data for download success scenarios
 */
struct DownloadSuccessTestData
{
	bool chunkMode;
	bool ptsRestampEnabled;
};

/**
 * @brief Parameterized test cases for OnFragmentDownloadSuccess
 */
DownloadSuccessTestData validDownloadTestData[] = {
	{CHUNK_MODE_DISABLED, PTS_RESTAMP_DISABLED},
	{CHUNK_MODE_DISABLED, PTS_RESTAMP_ENABLED},
	{CHUNK_MODE_ENABLED, PTS_RESTAMP_DISABLED},
	{CHUNK_MODE_ENABLED, PTS_RESTAMP_ENABLED}};

/**
 * @brief Parameterized test fixture for OnFragmentDownloadSuccess
 */
class FragmentDownloadSuccessParamTest
	: public FragmentDownloadTests,
	  public ::testing::WithParamInterface<DownloadSuccessTestData>
{
};

/**
 * @brief Test case for OnFragmentDownloadSuccess with various configurations
 */
TEST_P(FragmentDownloadSuccessParamTest, OnFragmentDownloadSuccess)
{
	const auto &param = GetParam();
	bool chunkMode = param.chunkMode;
	bool ptsRestampEnabled = param.ptsRestampEnabled;

	mMediaStreamContext->mActiveDownloadInfo = std::make_shared<DownloadInfo>();
	DownloadInfoPtr dlInfo = std::make_shared<DownloadInfo>();
	dlInfo->pts = 123.45;
	dlInfo->fragmentDurationSec = 5.0;
	dlInfo->isDiscontinuity = false;
	dlInfo->ptsOffset = 1000;

	// Mock necessary method calls and return values
	EXPECT_CALL(*g_mockPrivateInstanceAAMP, GetTSBSessionManager()).WillRepeatedly(Return(nullptr));
	EXPECT_CALL(*g_mockPrivateInstanceAAMP, IsLocalAAMPTsbInjection()).WillRepeatedly(Return(false));
	EXPECT_CALL(*g_mockPrivateInstanceAAMP, DownloadsAreEnabled()).WillRepeatedly(Return(true));

	// Mock buffer creation for the test
	auto cachedFragment = std::make_shared<CachedFragment>();
	cachedFragment->fragment.insert(cachedFragment->fragment.end(), reinterpret_cast<const uint8_t*>("test"), reinterpret_cast<const uint8_t*>("test") + 4);
	EXPECT_CALL(*g_mockMediaTrack, GetFetchBuffer(false)).WillOnce(Return(cachedFragment.get()));

	EXPECT_CALL(*g_mockMediaTrack, IsInjectionFromCachedFragmentChunks()).WillRepeatedly(Return(chunkMode));
	EXPECT_CALL(*g_mockPrivateInstanceAAMP, GetLLDashChunkMode()).WillRepeatedly(Return(chunkMode));
	EXPECT_CALL(*g_mockAampConfig, IsConfigSet(eAAMPConfig_EnablePTSReStamp)).WillRepeatedly(Return(ptsRestampEnabled));

	EXPECT_CALL(*g_mockMediaTrack, UpdateTSAfterFetch(_));
	if (chunkMode)
	{
		EXPECT_CALL(*g_mockMediaTrack, UpdateTSAfterInject());
	}

	EXPECT_NO_THROW(mMediaStreamContext->OnFragmentDownloadSuccess(dlInfo));

	// PTS restamp expectation
	if (ptsRestampEnabled)
	{
		EXPECT_EQ(cachedFragment->position, dlInfo->pts + dlInfo->ptsOffset.inSeconds());
	}
	else
	{
		EXPECT_EQ(cachedFragment->position, dlInfo->pts);
	}

	cachedFragment->fragment.clear(); cachedFragment->fragment.shrink_to_fit();
}

/**
 * @brief Instantiate parameterized tests for OnFragmentDownloadSuccess
 */
INSTANTIATE_TEST_SUITE_P(
	AllDownloadVariants,
	FragmentDownloadSuccessParamTest,
	::testing::ValuesIn(validDownloadTestData));

/**
 * @brief Test case for onFragmentDownloadFailed with null active download info
 */
TEST_F(FragmentDownloadTests, OnFragmentDownloadFailed_NullActiveDownloadInfo)
{
	mMediaStreamContext->mActiveDownloadInfo = nullptr;
	DownloadInfoPtr dlInfo = std::make_shared<DownloadInfo>();
	EXPECT_NO_THROW(mMediaStreamContext->OnFragmentDownloadFailed(dlInfo));
}

/**
 * @brief Test case for onFragmentDownloadFailed with a ramp down attempt
 */
TEST_F(FragmentDownloadTests, OnFragmentDownloadFailed_RampDownAttempt)
{
	mMediaStreamContext->mActiveDownloadInfo = std::make_shared<DownloadInfo>();
	DownloadInfoPtr dlInfo = std::make_shared<DownloadInfo>();
	dlInfo->url = "http://example.com/fragment";
	dlInfo->isInitSegment = false;

	EXPECT_CALL(*g_mockPrivateInstanceAAMP, DownloadsAreEnabled()).WillRepeatedly(Return(true));

	// Set segDLFailCount to 0 and set the fail threshold as 10(default)
	mMediaStreamContext->segDLFailCount = 0;
	EXPECT_CALL(*g_mockAampConfig, GetConfigValue(eAAMPConfig_FragmentDownloadFailThreshold)).WillRepeatedly(Return(10));

	// Mock the behavior of GetFetchBuffer, create a dummy CachedFragment
	auto cachedFragment = std::make_shared<CachedFragment>();
	EXPECT_CALL(*g_mockMediaTrack, GetFetchBuffer(false))
		.WillOnce(Return(cachedFragment.get()));

	// Return false for CheckForRampDownLimitReached to allow ramp down, and true for CheckForRampDownProfile
	EXPECT_CALL(*g_mockStreamAbstractionAAMP, CheckForRampDownLimitReached())
		.WillOnce(Return(false));
	EXPECT_CALL(*g_mockStreamAbstractionAAMP, CheckForRampDownProfile(_))
		.WillOnce(Return(true));

	// Test the behavior of OnFragmentDownloadFailed
	EXPECT_NO_THROW({
		mMediaStreamContext->OnFragmentDownloadFailed(dlInfo);
		EXPECT_TRUE(mMediaStreamContext->mCheckForRampdown);
	});
}

TEST_F(FragmentDownloadTests, OnFragmentDownloadFailed_ValidDownloadInfoLowestProfile)
{
	mMediaStreamContext->mActiveDownloadInfo = std::make_shared<DownloadInfo>();
	DownloadInfoPtr dlInfo = std::make_shared<DownloadInfo>();
	dlInfo->url = "http://example.com/fragment";
	dlInfo->isInitSegment = false;

	EXPECT_CALL(*g_mockPrivateInstanceAAMP, DownloadsAreEnabled()).WillRepeatedly(Return(true));

	// Set segDLFailCount to 1 for showing the ramp down histiry and set the fail threshold as 10(default)
	mMediaStreamContext->segDLFailCount = 1;
	mMediaStreamContext->mSkipSegmentOnError = false;
	EXPECT_CALL(*g_mockAampConfig, GetConfigValue(eAAMPConfig_FragmentDownloadFailThreshold)).WillRepeatedly(Return(10));

	// Mock the behavior of GetFetchBuffer, create a dummy CachedFragment
	auto cachedFragment = std::make_shared<CachedFragment>();
	EXPECT_CALL(*g_mockMediaTrack, GetFetchBuffer(false))
		.WillOnce(Return(cachedFragment.get()));
	
	// Return true for CheckForRampDownLimitReached to indicate that the limit is reached
	EXPECT_CALL(*g_mockStreamAbstractionAAMP, CheckForRampDownLimitReached())
		.WillOnce(Return(true));

	// Test the behavior of OnFragmentDownloadFailed, mCheckForRampdown should be set to false
	// and mSkipSegmentOnError should be set to true
	EXPECT_NO_THROW({
		mMediaStreamContext->OnFragmentDownloadFailed(dlInfo);
		EXPECT_EQ(mMediaStreamContext->segDLFailCount, 1);
		EXPECT_FALSE(mMediaStreamContext->mCheckForRampdown);
		EXPECT_TRUE(mMediaStreamContext->mSkipSegmentOnError);
	});
}

/**
 * @brief Test case for OnFragmentDownloadFailed with a retry attempt threshold
 */
TEST_F(FragmentDownloadTests, OnFragmentDownloadFailed_RetryAttemptThreshold)
{
	mMediaStreamContext->mActiveDownloadInfo = std::make_shared<DownloadInfo>();
	DownloadInfoPtr dlInfo = std::make_shared<DownloadInfo>();
	dlInfo->url = "http://example.com/fragment";
	dlInfo->isInitSegment = false;

	EXPECT_CALL(*g_mockPrivateInstanceAAMP, DownloadsAreEnabled()).WillRepeatedly(Return(true));

	// Set segDLFailCount to 10 and set the fail threshold as 10(default)
	// This should not trigger a ramp down
	mMediaStreamContext->segDLFailCount = 10;
	EXPECT_CALL(*g_mockAampConfig, GetConfigValue(eAAMPConfig_FragmentDownloadFailThreshold)).WillRepeatedly(Return(10));

	// Mock the behavior of GetFetchBuffer, create a dummy CachedFragment
	auto cachedFragment = std::make_shared<CachedFragment>();
	EXPECT_CALL(*g_mockMediaTrack, GetFetchBuffer(false))
		.WillOnce(Return(cachedFragment.get()));

	//Ensure proper error event is sent
	EXPECT_CALL(*g_mockPrivateInstanceAAMP, SendDownloadErrorEvent(AAMP_TUNE_FRAGMENT_DOWNLOAD_FAILURE, _))
		.Times(1);

	// Test the behavior of OnFragmentDownloadFailed, mCheckForRampdown should be set to false
	EXPECT_NO_THROW({
		mMediaStreamContext->OnFragmentDownloadFailed(dlInfo);
		EXPECT_FALSE(mMediaStreamContext->mCheckForRampdown);
	});
}

/**
 * @brief Test case for DownloadFragment with null download info
 */
TEST_F(FragmentDownloadTests, DownloadFragment_NullDownloadInfo)
{
	DownloadInfoPtr dlInfo = nullptr;
	bool result = mMediaStreamContext->DownloadFragment(dlInfo);
	EXPECT_FALSE(result);
}

/**
 * @brief Test case for DownloadFragment with empty media URL
 */
TEST_F(FragmentDownloadTests, DownloadFragment_EmptyMediaUrl)
{
	DownloadInfoPtr dlInfo = std::make_shared<DownloadInfo>();
	dlInfo->uriList[0].url = "";
	bool result = mMediaStreamContext->DownloadFragment(dlInfo);
	EXPECT_FALSE(result);
}

/**
 * @brief Test case for DownloadFragment with valid DownloadInfo
 */
TEST_F(FragmentDownloadTests, DownloadFragment_ValidDownloadInfo)
{
	DownloadInfoPtr dlInfo = std::make_shared<DownloadInfo>();
	dlInfo->uriList[0].url = "http://example.com/fragment";
	dlInfo->url = "http://example.com/fragment";
	dlInfo->isInitSegment = false;

	EXPECT_CALL(*g_mockPrivateInstanceAAMP, DownloadsAreEnabled()).WillRepeatedly(Return(true));
	EXPECT_CALL(*g_mockPrivateInstanceAAMP, GetFile(_, _, _, _, _, _, _, _, _, _, _, _, _, _)).WillOnce(Return(true));

	auto cachedFragment = std::make_shared<CachedFragment>();
	EXPECT_CALL(*g_mockMediaTrack, GetFetchBuffer(true))
		.WillOnce(Return(cachedFragment.get()));

	EXPECT_NO_THROW({
		bool result = mMediaStreamContext->DownloadFragment(dlInfo);
		EXPECT_TRUE(result);
	});
}
