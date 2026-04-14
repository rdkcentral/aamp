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
#include "AampUtils.h"
#include "priv_aamp.h"
#include "AampDRMLicPreFetcherInterface.h"
#include "AampConfig.h"
#include "MockAampConfig.h"
#include "MockMediaTrack.h"
#include "MockStreamAbstractionAAMP.h"
#include "MockPrivateInstanceAAMP.h"
#include "MockAampTimeBasedBufferManager.h"
#include "MockTSBSessionManager.h"
#include "MockTSBReader.h"
#include "MockStreamAbstractionAAMP_MPD.h"
#include "StreamAbstractionAAMP.h"
#include "AampDownloadInfo.hpp"
#include <functional>
#include <string_view>

using namespace testing;
using namespace std::literals;
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
		g_mockAampTimeBasedBufferManager = new StrictMock<aamp::MockAampTimeBasedBufferManager>();
		// GetBufferedDurationSecs() is called for every video-track fragment via
		// NotifyBufferLevelToLatencyMonitor.  Allow any number of calls so all tests
		// in this fixture pass without needing per-test EXPECT_CALL boilerplate.
		EXPECT_CALL(*g_mockPrivateInstanceAAMP, GetBufferedDurationSecs())
			.Times(AnyNumber()).WillRepeatedly(Return(5.0));
		mTsbSessionMgr = std::make_unique<AampTSBSessionManager>(mPrivateInstanceAAMP);
		mMockTSBSessionMgr = std::make_unique<NiceMock<MockTSBSessionManager>>(mPrivateInstanceAAMP);
		g_mockTSBSessionManager = mMockTSBSessionMgr.get();
		mTsbReader = std::make_shared<AampTsbReader>(mPrivateInstanceAAMP, nullptr, eMEDIATYPE_VIDEO, "sessionId");
		g_mockTSBReader = std::make_shared<MockTSBReader>();
		mMockStreamAbstractionAAMP_MPD = std::make_unique<NiceMock<MockStreamAbstractionAAMP_MPD>>(mPrivateInstanceAAMP, 0, 0);
		g_mockStreamAbstractionAAMP_MPD = mMockStreamAbstractionAAMP_MPD.get();
	}

	void TearDown() override
	{
		g_mockStreamAbstractionAAMP_MPD = nullptr;
		mMockStreamAbstractionAAMP_MPD.reset();
		g_mockTSBReader.reset();
		mTsbReader.reset();
		g_mockTSBSessionManager = nullptr;
		mMockTSBSessionMgr.reset();
		mTsbSessionMgr.reset();

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

		delete g_mockAampTimeBasedBufferManager;
		g_mockAampTimeBasedBufferManager = nullptr;
	}

public:
	StreamAbstractionAAMP_MPD *mStreamAbstractionAAMP_MPD;
	PrivateInstanceAAMP *mPrivateInstanceAAMP;
	MediaStreamContext *mMediaStreamContext;
	std::unique_ptr<AampTSBSessionManager> mTsbSessionMgr;
	std::unique_ptr<NiceMock<MockTSBSessionManager>> mMockTSBSessionMgr;
	std::shared_ptr<AampTsbReader> mTsbReader;
	std::unique_ptr<NiceMock<MockStreamAbstractionAAMP_MPD>> mMockStreamAbstractionAAMP_MPD;
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
 *
 * After unifying DASH to use the chunk cache path (IsInjectionFromCachedFragmentChunks
 * returns true for all DASH), OnFragmentDownloadSuccess no longer uses the ring buffer
 * (GetFetchBuffer / UpdateTSAfterFetch).  Instead:
 *  - Non-LLD (chunkMode=false): CacheTsbFragment is called, which writes the staging
 *    fragment into a chunk buffer slot via GetFetchChunkBuffer / UpdateTSAfterChunkFetch.
 *  - LLD (chunkMode=true): media data was already pushed during SSL callbacks; here only
 *    the time-based buffer counter is consumed.
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

	// Pre-populate the staging fragment so CacheTsbFragment has data to process on the
	// non-LLD path.  Without this the fragment is empty and CacheTsbFragment is a no-op.
	static constexpr uint8_t kTestData[] = {0xAA, 0xBB, 0xCC, 0xDD};
	mMediaStreamContext->mStagingFragment.fragment.assign(kTestData, kTestData + sizeof(kTestData));

	// Mock necessary method calls and return values
	EXPECT_CALL(*g_mockPrivateInstanceAAMP, GetTSBSessionManager()).WillRepeatedly(Return(nullptr));
	EXPECT_CALL(*g_mockPrivateInstanceAAMP, IsLocalAAMPTsbInjection()).WillRepeatedly(Return(false));
	EXPECT_CALL(*g_mockPrivateInstanceAAMP, DownloadsAreEnabled()).WillRepeatedly(Return(true));
	EXPECT_CALL(*g_mockPrivateInstanceAAMP, GetLLDashChunkMode()).WillRepeatedly(Return(chunkMode));
	EXPECT_CALL(*g_mockAampConfig, IsConfigSet(eAAMPConfig_EnablePTSReStamp)).WillRepeatedly(Return(ptsRestampEnabled));

	// LLD path: staging data was already pushed to the pipeline via SSL callbacks;
	// OnFragmentDownloadSuccess only consumes the time-based buffer entry.
	// Non-LLD path: time-based buffer consumption only happens when IsLocalAAMPTsb() is
	// true, which it is not in this test.
	EXPECT_CALL(*g_mockAampTimeBasedBufferManager, ConsumeBuffer(dlInfo->fragmentDurationSec)).Times(chunkMode ? 1 : 0);

	// For non-LLD DASH, CacheTsbFragment writes the staging fragment into a chunk buffer
	// slot.  Verify the position is propagated correctly.
	auto chunkSlot = std::make_shared<CachedFragment>();
	if (!chunkMode)
	{
		EXPECT_CALL(*g_mockMediaTrack, GetFetchChunkBuffer(true)).WillOnce(Return(chunkSlot.get()));
		EXPECT_CALL(*g_mockMediaTrack, UpdateTSAfterChunkFetch());
	}

	EXPECT_NO_THROW(mMediaStreamContext->OnFragmentDownloadSuccess(dlInfo));

	// PTS restamp expectation — verified through the chunk slot that CacheTsbFragment
	// populated (non-LLD only; in LLD mode data is pre-injected via SSL callbacks).
	if (!chunkMode)
	{
		if (ptsRestampEnabled)
		{
			EXPECT_EQ(chunkSlot->position, dlInfo->pts + dlInfo->ptsOffset.inSeconds());
		}
		else
		{
			EXPECT_EQ(chunkSlot->position, dlInfo->pts);
		}
		aamp_utils::ClearAndRelease(chunkSlot->fragment);
	}
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
 * @brief Verify that when rampdown is not possible (lowest profile) and a
 *        segment is skipped, the time-based buffer is consumed.
 */
TEST_F(FragmentDownloadTests, OnFragmentDownloadFailed_LowestProfile_ConsumesTimeBasedBuffer)
{
	mMediaStreamContext->mActiveDownloadInfo = std::make_shared<DownloadInfo>();
	DownloadInfoPtr dlInfo = std::make_shared<DownloadInfo>();
	dlInfo->url = "http://example.com/fragment";
	dlInfo->isInitSegment = false;
	dlInfo->isPlayingAd = false;
	dlInfo->pts = 123.45;
	dlInfo->fragmentDurationSec = 5.0;

	// Ensure we take the lowest-profile skip path.
	mMediaStreamContext->segDLFailCount = 0;
	mMediaStreamContext->mSkipSegmentOnError = true;
	mMediaStreamContext->httpErrorCode = 404;

	// Mock necessary method calls and return values
	EXPECT_CALL(*g_mockPrivateInstanceAAMP, DownloadsAreEnabled())
		.WillRepeatedly(Return(true));
	EXPECT_CALL(*g_mockAampConfig,
		GetConfigValue(eAAMPConfig_FragmentDownloadFailThreshold))
		.WillRepeatedly(Return(10));
	EXPECT_CALL(*g_mockAampConfig, IsConfigSet(_))
		.WillRepeatedly(Return(false));

	// Return false for CheckForRampDownLimitReached to indicate that the limit is reached
	EXPECT_CALL(*g_mockStreamAbstractionAAMP, CheckForRampDownLimitReached())
		.WillOnce(Return(false));
	EXPECT_CALL(*g_mockStreamAbstractionAAMP, CheckForRampDownProfile(_))
		.WillOnce(Return(false));

	// Expect the time-based buffer to be consumed for the fragment duration
	EXPECT_CALL(*g_mockAampTimeBasedBufferManager,
		ConsumeBuffer(dlInfo->fragmentDurationSec))
		.Times(1);

	EXPECT_NO_THROW(mMediaStreamContext->OnFragmentDownloadFailed(dlInfo));
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
	constexpr int maxCache = 5;

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

	// Expect exactly one successful "download".
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
	constexpr int numCalls = 5;

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

	// Expect one successful "download" per call.
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

/**
 * @brief Minimal IPeriod stub for tests that call EnqueueWrite
 *        (which dereferences context->GetPeriod()->GetId()).
 */
namespace
{
class DummyPeriod : public IPeriod
{
public:
	DummyPeriod() = default;
	virtual ~DummyPeriod() = default;

	const std::string& GetId() const override { return mId; }
	const std::vector<IAdaptationSet*>& GetAdaptationSets() const override { static std::vector<IAdaptationSet*> v; return v; }
	const std::string& GetStart() const override { return mStart; }
	const std::string& GetDuration() const override { return mDuration; }
	bool GetBitstreamSwitching() const override { return false; }
	const std::vector<IBaseUrl*>& GetBaseURLs() const override { static std::vector<IBaseUrl*> v; return v; }
	ISegmentBase* GetSegmentBase() const override { return nullptr; }
	ISegmentList* GetSegmentList() const override { return nullptr; }
	ISegmentTemplate* GetSegmentTemplate() const override { return nullptr; }
	const std::vector<ISubset*>& GetSubsets() const override { static std::vector<ISubset*> v; return v; }
	const std::vector<IEventStream*>& GetEventStreams() const override { static std::vector<IEventStream*> v; return v; }
	const std::string& GetXlinkHref() const override { static std::string s; return s; }
	const std::vector<dash::xml::INode*> GetAdditionalSubNodes() const override { return {}; }
	const std::string& GetXlinkActuate() const override { static std::string s; return s; }
	const std::map<std::string, std::string, std::less<std::string>,
		std::allocator<std::pair<const std::string, std::string>>>
		GetRawAttributes() const override
	{
		return {};
	}

private:
	std::string mId{"dummyPeriodId"};
	std::string mStart{"0.0"};
	std::string mDuration{"0.0"};
};
} // anonymous namespace

/**
 * @brief Verify that when CheckEos() returns true with the pipeline paused
 *        due to underflow (mSinkPaused=true, GetBufUnderFlowStatus()=true),
 *        SetLocalTSBInjection(false) and UpdateLocalAAMPTsbInjection() are
 *        called.
 */
TEST_F(FragmentDownloadTests, OnFragmentDownloadSuccess_CheckEos_PausedDueToUnderflow)
{
	DummyPeriod dummyPeriod;

	// --- Prepare scenario conditions ---
	mPrivateInstanceAAMP->rate = AAMP_NORMAL_PLAY_RATE;
	mPrivateInstanceAAMP->mSinkPaused.store(true);
	mPrivateInstanceAAMP->SetBufUnderFlowStatus(true);
	mStreamAbstractionAAMP_MPD->mTuneType = eTUNETYPE_SEEKTOLIVE;

	// Populate staging fragment with data so the tsbSessionManager block is entered.
	constexpr std::string_view testData{"test_fragment_data"};
	mMediaStreamContext->mStagingFragment.fragment.assign(
		reinterpret_cast<const uint8_t*>(testData.data()),
		reinterpret_cast<const uint8_t*>(testData.data()) + testData.size());

	// A CachedFragment for CacheTsbFragment's GetFetchChunkBuffer call.
	auto chunkBuffer = std::make_shared<CachedFragment>();

	mMediaStreamContext->mActiveDownloadInfo = std::make_shared<DownloadInfo>();
	DownloadInfoPtr dlInfo = std::make_shared<DownloadInfo>();
	dlInfo->pts = 100.0;
	dlInfo->fragmentDurationSec = 2.0;
	dlInfo->isDiscontinuity = false;
	dlInfo->isInitSegment = false;
	dlInfo->absolutePosition = 500.0;
	dlInfo->mediaType = eMEDIATYPE_VIDEO;

	// --- Mock expectations ---
	EXPECT_CALL(*g_mockPrivateInstanceAAMP, DownloadsAreEnabled())
		.WillRepeatedly(Return(true));

	// TSB session manager is non-null to trigger the CheckEos path.
	EXPECT_CALL(*g_mockPrivateInstanceAAMP, GetTSBSessionManager())
		.WillOnce(Return(mTsbSessionMgr.get()));

	// IsLocalTSBInjection: true in CheckEos, false after SetLocalTSBInjection(false).
	{
		InSequence seq;
		EXPECT_CALL(*g_mockMediaTrack, IsLocalTSBInjection())
			.WillOnce(Return(true));   // CheckEos
		EXPECT_CALL(*g_mockMediaTrack, SetLocalTSBInjection(false))
			.Times(1);
		EXPECT_CALL(*g_mockMediaTrack, IsLocalTSBInjection())
			.WillRepeatedly(Return(false));   // Subsequent checks
	}

	// TSB reader returns EOS so CheckEos returns true.
	EXPECT_CALL(*g_mockTSBSessionManager, GetTsbReader(eMEDIATYPE_VIDEO))
		.WillRepeatedly(Return(mTsbReader));
	EXPECT_CALL(*g_mockTSBReader, IsEos())
		.WillRepeatedly(Return(true));

	// EnqueueWrite calls context->GetPeriod()->GetId().
	EXPECT_CALL(*g_mockStreamAbstractionAAMP_MPD, GetPeriod())
		.WillOnce(Return(&dummyPeriod));

	// LLDash chunk mode off: CacheTsbFragment skipped inside CheckEos,
	// but called in the SLD re-cache path after the TSB injection check.
	EXPECT_CALL(*g_mockPrivateInstanceAAMP, GetLLDashChunkMode())
		.WillRepeatedly(Return(false));

	// ** KEY EXPECTATION ** — the behaviour under test.
	EXPECT_CALL(*g_mockPrivateInstanceAAMP, UpdateLocalAAMPTsbInjection())
		.Times(1);

	// Else block: SLD re-cache via CacheTsbFragment.
	EXPECT_CALL(*g_mockMediaTrack, GetFetchChunkBuffer(true))
		.WillOnce(Return(chunkBuffer.get()));
	EXPECT_CALL(*g_mockMediaTrack, UpdateTSAfterChunkFetch());

	// --- Execute ---
	EXPECT_NO_THROW(mMediaStreamContext->OnFragmentDownloadSuccess(dlInfo));
}

// ---------------------------------------------------------------------------
// CacheTsbFragment: move semantics transfer data without copying
// ---------------------------------------------------------------------------

/**
 * @brief Verify that CacheTsbFragment uses move semantics to transfer
 *        fragment data into the ring buffer slot, avoiding a deep copy.
 *
 * The source shared_ptr is passed by value (not moved at the call site) so
 * that sourceFragment still points to the same CachedFragment object after
 * the call.  The move assignment inside CacheTsbFragment empties that object,
 * which is then observable through sourceFragment.  If the implementation
 * were changed to Copy() instead of move, sourceFragment->fragment would
 * still hold the original data and the source-empty assertions below would
 * fail, catching the regression.
 */
TEST_F(FragmentDownloadTests, CacheTsbFragment_MoveSemantics_TransfersOwnership)
{
	// --- Arrange ---
	constexpr std::string_view testPayload{"MOVE_SEMANTICS_TEST_DATA"};
	auto sourceFragment = std::make_shared<CachedFragment>();
	sourceFragment->fragment.assign(
		reinterpret_cast<const uint8_t*>(testPayload.data()),
		reinterpret_cast<const uint8_t*>(testPayload.data()) + testPayload.size());
	sourceFragment->position = 42.0;
	sourceFragment->duration = 2.0;
	sourceFragment->initFragment = true;
	sourceFragment->discontinuity = true;

	// The ring buffer slot that GetFetchChunkBuffer will return.
	auto ringBufferSlot = std::make_shared<CachedFragment>();

	// WaitForCachedFragmentChunkInjected is handled by the fake (always returns true).
	EXPECT_CALL(*g_mockMediaTrack, GetFetchChunkBuffer(true))
		.WillOnce(Return(ringBufferSlot.get()));
	EXPECT_CALL(*g_mockMediaTrack, UpdateTSAfterChunkFetch());

	// --- Act ---
	// std::move transfers the shared_ptr into the && parameter without
	// decrementing the ref count, so sourceFragment still aliases the same
	// CachedFragment after the call.  The move assignment inside
	// CacheTsbFragment empties that object, observable through sourceFragment.
	bool result = mMediaStreamContext->CacheTsbFragment(std::move(sourceFragment));

	// --- Assert ---
	EXPECT_TRUE(result);

	// Destination ring buffer slot must own the data.
	EXPECT_EQ(ringBufferSlot->fragment.size(), testPayload.size());
	EXPECT_DOUBLE_EQ(ringBufferSlot->position, 42.0);
	EXPECT_DOUBLE_EQ(ringBufferSlot->duration, 2.0);
	EXPECT_TRUE(ringBufferSlot->initFragment);
	EXPECT_TRUE(ringBufferSlot->discontinuity);

	// Source CachedFragment must be in a moved-from (empty) state, proving
	// that a move — not a copy — transferred the payload.
	EXPECT_TRUE(sourceFragment->fragment.empty());
	EXPECT_DOUBLE_EQ(sourceFragment->position, 0.0);
	EXPECT_DOUBLE_EQ(sourceFragment->duration, 0.0);
	EXPECT_FALSE(sourceFragment->initFragment);
	EXPECT_FALSE(sourceFragment->discontinuity);
}

// ---------------------------------------------------------------------------
// Regression: wasUnderFlowActive guard prevents discarding the recovery fragment
// ---------------------------------------------------------------------------
// The hook defined in FakeStreamAbstractionAamp.cpp is called inside
// NotifyVideoFragmentToUnderflowMonitor. Tests in this binary can register a
// callback to simulate state changes that happen between the wasUnderFlowActive
// snapshot and the discard check.
extern std::function<void()> g_notifyVideoFragmentSideEffect;

/**
 * @brief Regression test for the wasUnderFlowActive guard in OnFragmentDownloadSuccess.
 *
 * The race this test replicates:
 *   1. wasUnderFlowActive is snapshotted as true (mBufUnderFlowStatus == true).
 *   2. NotifyVideoFragmentToUnderflowMonitor calls SetBufferingState(false), clearing
 *      mBufUnderFlowStatus and resuming the GStreamer pipeline.
 *   3. GStreamer fires a buffering(0) event on a separate thread, re-setting
 *      mSinkPaused = true before the discard check runs.
 *   4. At the discard check the state is:
 *        isPipelinePaused        = true   (mSinkPaused re-set by step 3)
 *        !GetBufUnderFlowStatus()= true   (cleared in step 2)
 *        !wasUnderFlowActive     = false  (captured as true in step 1)
 *
 * Without the guard the discard condition would be:
 *   tsbSessionManager && isPipelinePaused && !GetBufUnderFlowStatus()
 *   = true && true && true → fragment DISCARDED (stall bug)
 *
 * With the guard (!wasUnderFlowActive must also be true):
 *   condition = true && true && true && false → false → fragment INJECTED (correct)
 *
 * The g_notifyVideoFragmentSideEffect hook simulates steps 2+3 atomically
 * within the notify call so the discard check sees the post-race state.
 *
 * Using an empty cachedFragment avoids the EnqueueWrite/GetPeriod path while
 * still exercising the discard check with a non-null tsbSessionManager.
 */
TEST_F(FragmentDownloadTests, OnFragmentDownloadSuccess_UnderflowRecoveryRace_FragmentInjected)
{
	// --- Arrange ---
	// (1) Underflow is active when wasUnderFlowActive is snapshotted.
	mPrivateInstanceAAMP->SetBufUnderFlowStatus(true);
	mPrivateInstanceAAMP->mSinkPaused.store(false);

	// (2+3) The side effect simulates SetBufferingState(false) + buffering(0) race.
	g_notifyVideoFragmentSideEffect = [this]()
	{
		mPrivateInstanceAAMP->ResetBufUnderFlowStatus(); // cleared by SetBufferingState(false)
		mPrivateInstanceAAMP->mSinkPaused.store(true);  // re-set by GStreamer buffering(0)
	};

	// An empty cachedFragment: skips EnqueueWrite (avoids GetPeriod call) but
	// still lets the discard check run with tsbSessionManager != null.
	auto cachedFragment = std::make_shared<CachedFragment>();
	// fragment data left intentionally empty — triggers the EnqueueWrite guard
	// (if(tsbSessionManager && cachedFragment->fragment.size())) to be false.

	// --- Mock setup ---
	EXPECT_CALL(*g_mockPrivateInstanceAAMP, DownloadsAreEnabled())
		.WillRepeatedly(Return(true));
	EXPECT_CALL(*g_mockPrivateInstanceAAMP, GetTSBSessionManager())
		.WillOnce(Return(mMockTSBSessionMgr.get())); // non-null → discard guard activates
	EXPECT_CALL(*g_mockPrivateInstanceAAMP, GetLLDashChunkMode())
		.WillRepeatedly(Return(false));
	EXPECT_CALL(*g_mockMediaTrack, GetFetchBuffer(false))
		.WillOnce(Return(cachedFragment.get()));
	EXPECT_CALL(*g_mockMediaTrack, IsInjectionFromCachedFragmentChunks())
		.WillRepeatedly(Return(false));

	// KEY assertion: UpdateTSAfterFetch is called iff the fragment is NOT discarded.
	// Without the !wasUnderFlowActive guard this call would be suppressed (bug).
	EXPECT_CALL(*g_mockMediaTrack, UpdateTSAfterFetch(false)).Times(1);
	// IsLocalTSBInjection() is called twice in OnFragmentDownloadSuccess:
	//   1. in the discard-guard condition
	//   2. in the SLD-copy-to-TSB guard inside the inject path
	// Both must be non-blocking with StrictMock.
	EXPECT_CALL(*g_mockMediaTrack, IsLocalTSBInjection()).WillRepeatedly(Return(false));

	// --- Build dlInfo ---
	mMediaStreamContext->mActiveDownloadInfo = std::make_shared<DownloadInfo>();
	DownloadInfoPtr dlInfo = std::make_shared<DownloadInfo>();
	dlInfo->pts                = 10.0;
	dlInfo->absolutePosition   = 10.0;
	dlInfo->fragmentDurationSec= 2.0;
	dlInfo->isDiscontinuity    = false;
	dlInfo->isInitSegment      = false;
	dlInfo->mediaType          = eMEDIATYPE_VIDEO;
	dlInfo->url                = "http://example.com/fragment";

	// --- Execute ---
	EXPECT_NO_THROW(mMediaStreamContext->OnFragmentDownloadSuccess(dlInfo));

	// --- Cleanup ---
	g_notifyVideoFragmentSideEffect = nullptr;
}
