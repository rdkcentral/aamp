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
	cachedFragment->fragment.AppendBytes("test", 4);
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

	cachedFragment->fragment.Free();
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
	EXPECT_CALL(*g_mockPrivateInstanceAAMP, IsLocalAAMPTsbInjection()).WillRepeatedly(Return(true));
	EXPECT_CALL(*g_mockPrivateInstanceAAMP, GetFile(_, _, _, _, _, _, _, _, _, _, _, _, _, _)).WillOnce(Return(true));

	auto cachedFragment = std::make_shared<CachedFragment>();
	EXPECT_CALL(*g_mockMediaTrack, GetFetchBuffer(true))
		.WillOnce(Return(cachedFragment.get()));

	EXPECT_NO_THROW({
		bool result = mMediaStreamContext->DownloadFragment(dlInfo);
		EXPECT_TRUE(result);
	});
}

/**
 * @brief Verify DownloadFragment never caches when track downloads are disabled in low latency mode
 */
TEST_F(FragmentDownloadTests, DownloadFragment_LLD_TrackDownloadsDisabled_DoesNotCache)
{
	// This test validates that in Low Latency DASH mode, when track downloads
	// are disabled, DownloadFragment should not attempt to cache/download the
	// fragment (i.e., it should not call GetFetchBuffer/GetFile). The function
	// should still return true because the "download loop" can exit cleanly.
	int maxCache = 5;

	// Enable low-latency mode so the wait loop path is exercised.
	mPrivateInstanceAAMP->GetLLDashServiceData()->lowLatencyMode = true;

	// Ensure there is cache capacity so only the "track downloads disabled"
	// condition prevents caching.
	mMediaStreamContext->numberOfFragmentsCached = 0;

	// Configure the max fragment cache size (not full).
	EXPECT_CALL(*g_mockAampConfig, GetConfigValue(eAAMPConfig_MaxFragmentCached))
		.WillRepeatedly(Return(maxCache));

	// Simulate that track downloads are disabled for the video track.
	// This should prevent any caching attempt in the LLD path.
	EXPECT_CALL(*g_mockPrivateInstanceAAMP, TrackDownloadsAreEnabled(eMEDIATYPE_VIDEO))
		.WillRepeatedly(Return(false));

	// Ensure we are not injecting from local TSB; otherwise the wait loop is
	// skipped and caching could proceed.
	EXPECT_CALL(*g_mockPrivateInstanceAAMP, IsLocalAAMPTsbInjection())
		.WillRepeatedly(Return(false));

	// Verify no caching/download is attempted.
	EXPECT_CALL(*g_mockMediaTrack, GetFetchBuffer(true)).Times(0);
	EXPECT_CALL(*g_mockPrivateInstanceAAMP, GetFile(_, _, _, _, _, _, _, _, _, _, _, _, _, _))
		.Times(0);

	// Force the low-latency wait loop to execute once and then stop:
	// - The function checks DownloadsAreEnabled() before entering the loop.
	// - WaitForLowLatencyDashDownloads() typically checks DownloadsAreEnabled()
	//   while waiting.
	// - We return false after the first iteration to stop the loop and prevent
	//   any caching work from continuing.
	{
		InSequence seq;
		EXPECT_CALL(*g_mockPrivateInstanceAAMP, DownloadsAreEnabled())
			.WillOnce(Return(true));
		EXPECT_CALL(*g_mockPrivateInstanceAAMP, DownloadsAreEnabled())
			.WillOnce(Return(true));
		EXPECT_CALL(*g_mockPrivateInstanceAAMP, DownloadsAreEnabled())
			.WillRepeatedly(Return(false));
	}

	// Build a minimal valid download request.
	DownloadInfoPtr dlInfo = std::make_shared<DownloadInfo>();
	dlInfo->uriList[0].url = "http://example.com/fragment";
	dlInfo->url = "http://example.com/fragment";
	dlInfo->isInitSegment = false;

	// Expect success (clean early-exit behavior) and no caching side effects.
	const bool result = mMediaStreamContext->DownloadFragment(dlInfo);
	EXPECT_TRUE(result);
}

/**
 * @brief Verify DownloadFragment never caches when fragment cache is full
 */
TEST_F(FragmentDownloadTests, DownloadFragment_CacheFull_DoesNotCache)
{
	// This test validates that when the fragment cache is already full,
	// DownloadFragment returns without attempting to cache/download.
	constexpr int maxCache = 1;

	// Simulate cache already at capacity.
	mMediaStreamContext->numberOfFragmentsCached = maxCache;

	// Configure the cache size so "full" condition is true.
	EXPECT_CALL(*g_mockAampConfig, GetConfigValue(eAAMPConfig_MaxFragmentCached))
		.WillRepeatedly(Return(maxCache));

	// Verify no caching/download is attempted because cache is full.
	EXPECT_CALL(*g_mockMediaTrack, GetFetchBuffer(true)).Times(0);
	EXPECT_CALL(*g_mockPrivateInstanceAAMP, GetFile(_, _, _, _, _, _, _, _, _, _, _, _, _, _))
		.Times(0);

	// Allow the initial entry check to proceed, then disable downloads to
	// ensure we do not loop and accidentally attempt any caching path.
	{
		InSequence seq;
		EXPECT_CALL(*g_mockPrivateInstanceAAMP, DownloadsAreEnabled())
			.WillOnce(Return(true));
		EXPECT_CALL(*g_mockPrivateInstanceAAMP, DownloadsAreEnabled())
			.WillRepeatedly(Return(false));
	}

	// Build a minimal valid download request.
	DownloadInfoPtr dlInfo = std::make_shared<DownloadInfo>();
	dlInfo->uriList[0].url = "http://example.com/fragment";
	dlInfo->url = "http://example.com/fragment";
	dlInfo->isInitSegment = false;

	// Expect success (clean early-exit) and no caching.
	const bool result = mMediaStreamContext->DownloadFragment(dlInfo);
	EXPECT_TRUE(result);
}

/**
 * @brief Verify low-latency mode skips the wait loop when injecting from local TSB
 */
TEST_F(FragmentDownloadTests, DownloadFragment_LLD_LocalTSBInjection_Caches)
{
	// This test validates that in Low Latency DASH mode, if we are injecting
	// from a local TSB, the wait loop is skipped and the fragment can be
	// cached even if TrackDownloadsAreEnabled() is false.
	constexpr int maxCache = 5;

	// Enable low-latency mode and leave cache capacity available.
	mPrivateInstanceAAMP->GetLLDashServiceData()->lowLatencyMode = true;
	mMediaStreamContext->numberOfFragmentsCached = 0;

	// Simulate local TSB injection; this should bypass the LLD wait loop.
	EXPECT_CALL(*g_mockPrivateInstanceAAMP, IsLocalAAMPTsbInjection())
		.WillRepeatedly(Return(true));

	// Configure cache size so caching is allowed.
	EXPECT_CALL(*g_mockAampConfig, GetConfigValue(eAAMPConfig_MaxFragmentCached))
		.WillRepeatedly(Return(maxCache));

	// Even with track downloads disabled, local injection should allow caching.
	EXPECT_CALL(*g_mockPrivateInstanceAAMP, TrackDownloadsAreEnabled(eMEDIATYPE_VIDEO))
		.WillRepeatedly(Return(false));

	// Downloads enabled so the function proceeds through its normal flow.
	EXPECT_CALL(*g_mockPrivateInstanceAAMP, DownloadsAreEnabled())
		.WillRepeatedly(Return(true));

	// Expect exactly one cache buffer allocation and one successful "download".
	auto cachedFragment = std::make_shared<CachedFragment>();
	EXPECT_CALL(*g_mockMediaTrack, GetFetchBuffer(true))
		.WillOnce(Return(cachedFragment.get()));
	EXPECT_CALL(*g_mockPrivateInstanceAAMP, GetFile(_, _, _, _, _, _, _, _, _, _, _, _, _, _))
		.WillOnce(Return(true));

	// Build a minimal valid download request.
	DownloadInfoPtr dlInfo = std::make_shared<DownloadInfo>();
	dlInfo->uriList[0].url = "http://example.com/fragment";
	dlInfo->url = "http://example.com/fragment";
	dlInfo->isInitSegment = false;

	// Expect caching to occur and the call to succeed.
	EXPECT_TRUE(mMediaStreamContext->DownloadFragment(dlInfo));
}

/**
 * @brief Verify DownloadFragment caches when not blocked
 */
TEST_F(FragmentDownloadTests, DownloadFragment_NotBlocked_CachesExpected)
{
	// This test validates the "happy path" in Low Latency DASH mode:
	// - Track downloads enabled
	// - Downloads enabled
	// - Not local TSB injection (so LLD logic is active)
	// Expect caching/download to be attempted for each call.
	constexpr int maxCache = 5;
	int numCalls = 5;

	// Enable low-latency mode and ensure cache has room.
	mPrivateInstanceAAMP->GetLLDashServiceData()->lowLatencyMode = true;
	mMediaStreamContext->numberOfFragmentsCached = 0;

	// Configure cache size to permit caching.
	EXPECT_CALL(*g_mockAampConfig, GetConfigValue(eAAMPConfig_MaxFragmentCached))
		.WillRepeatedly(Return(maxCache));

	// Allow track downloads and global downloads so caching can proceed.
	EXPECT_CALL(*g_mockPrivateInstanceAAMP, TrackDownloadsAreEnabled(eMEDIATYPE_VIDEO))
		.WillRepeatedly(Return(true));
	EXPECT_CALL(*g_mockPrivateInstanceAAMP, DownloadsAreEnabled())
		.WillRepeatedly(Return(true));

	// Not local injection, so the non-TSB path is used.
	EXPECT_CALL(*g_mockPrivateInstanceAAMP, IsLocalAAMPTsbInjection())
		.WillRepeatedly(Return(false));

	// Expect one buffer request and one "download" per call.
	auto cachedFragment = std::make_shared<CachedFragment>();
	EXPECT_CALL(*g_mockMediaTrack, GetFetchBuffer(true))
		.Times(numCalls)
		.WillRepeatedly(Return(cachedFragment.get()));
	EXPECT_CALL(*g_mockPrivateInstanceAAMP, GetFile(_, _, _, _, _, _, _, _, _, _, _, _, _, _))
		.Times(numCalls)
		.WillRepeatedly(Return(true));

	// Build a minimal valid download request.
	DownloadInfoPtr dlInfo = std::make_shared<DownloadInfo>();
	dlInfo->uriList[0].url = "http://example.com/fragment";
	dlInfo->url = "http://example.com/fragment";
	dlInfo->isInitSegment = false;

	// Repeated calls should succeed and perform caching each time.
	for (int i = 0; i < numCalls; ++i)
	{
		EXPECT_TRUE(mMediaStreamContext->DownloadFragment(dlInfo));
	}
}
