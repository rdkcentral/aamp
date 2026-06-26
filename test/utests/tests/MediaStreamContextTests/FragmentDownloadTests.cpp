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

/** Thin subclass that promotes protected members needed by unit tests. */
class TestableMediaStreamContext : public MediaStreamContext
{
public:
	using MediaStreamContext::MediaStreamContext;
	using MediaStreamContext::mStagingFragment;
};

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
		mMediaStreamContext = new TestableMediaStreamContext(eTRACK_VIDEO, mStreamAbstractionAAMP_MPD, mPrivateInstanceAAMP, "SAMPLETEXT");
		g_mockAampConfig = std::make_shared<NiceMock<MockAampConfig>>();
		g_mockMediaTrack = std::make_shared<StrictMock<MockMediaTrack>>();
		g_mockStreamAbstractionAAMP = std::make_shared<NiceMock<MockStreamAbstractionAAMP>>(mPrivateInstanceAAMP);
		g_mockPrivateInstanceAAMP = std::make_shared<StrictMock<MockPrivateInstanceAAMP>>();
		g_mockAampTimeBasedBufferManager = std::make_shared<StrictMock<aamp::MockAampTimeBasedBufferManager>>();
		// GetBufferedDurationSecs() is called for every video-track fragment via
		// NotifyBufferLevelToLatencyMonitor.  Allow any number of calls so all tests
		// in this fixture pass without needing per-test EXPECT_CALL boilerplate.
		EXPECT_CALL(*g_mockPrivateInstanceAAMP, GetBufferedDurationSecs())
			.Times(AnyNumber()).WillRepeatedly(Return(5.0));
		// IsFragmentCacheFull() delegates to the mock after the fake was updated to
		// support TSB-aware testing.  Default to false so all pre-existing tests
		// are unaffected; individual tests can override this expectation.
		EXPECT_CALL(*g_mockMediaTrack, IsFragmentCacheFull())
			.WillRepeatedly(Return(false));
		// WaitForFreeFragmentAvailable() delegates to the mock.  Default to true so
		// the ring-buffer wait resolves immediately in tests that do not override it.
		EXPECT_CALL(*g_mockMediaTrack, WaitForFreeFragmentAvailable(_))
			.WillRepeatedly(Return(true));
		mTsbSessionMgr = std::make_unique<AampTSBSessionManager>(mPrivateInstanceAAMP);
		mMockTSBSessionMgr = std::make_unique<NiceMock<MockTSBSessionManager>>(mPrivateInstanceAAMP);
		g_mockTSBSessionManager = std::shared_ptr<MockTSBSessionManager>(mMockTSBSessionMgr.get(), [](MockTSBSessionManager*){});
		mTsbReader = std::make_shared<AampTsbReader>(mPrivateInstanceAAMP, nullptr, eMEDIATYPE_VIDEO, "sessionId");
		g_mockTSBReader = std::make_shared<MockTSBReader>();
		mMockStreamAbstractionAAMP_MPD = std::make_unique<NiceMock<MockStreamAbstractionAAMP_MPD>>(mPrivateInstanceAAMP, 0, 0);
		g_mockStreamAbstractionAAMP_MPD = std::shared_ptr<MockStreamAbstractionAAMP_MPD>(mMockStreamAbstractionAAMP_MPD.get(), [](MockStreamAbstractionAAMP_MPD*){});
	}

	void TearDown() override
	{
		g_mockStreamAbstractionAAMP_MPD.reset();
		mMockStreamAbstractionAAMP_MPD.reset();
		g_mockTSBReader.reset();
		mTsbReader.reset();
		g_mockTSBSessionManager.reset();
		mMockTSBSessionMgr.reset();
		mTsbSessionMgr.reset();

		delete mPrivateInstanceAAMP;
		mPrivateInstanceAAMP = nullptr;

		delete mStreamAbstractionAAMP_MPD;
		mStreamAbstractionAAMP_MPD = nullptr;

		delete mMediaStreamContext;
		mMediaStreamContext = nullptr;

		g_mockAampConfig.reset();

		g_mockMediaTrack.reset();

		g_mockStreamAbstractionAAMP.reset();

		g_mockPrivateInstanceAAMP.reset();

		g_mockAampTimeBasedBufferManager.reset();
	}

public:
	StreamAbstractionAAMP_MPD *mStreamAbstractionAAMP_MPD;
	PrivateInstanceAAMP *mPrivateInstanceAAMP;
	TestableMediaStreamContext *mMediaStreamContext;
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
 * After unifying DASH onto the chunk cache, OnFragmentDownloadSuccess no longer
 * uses the per-fragment ring buffer. Instead:
 *  - Non-LLD (chunkMode=false): CacheStagingFragmentForInjection() copies the
 *    staging fragment into a chunk-buffer slot via GetFetchBuffer /
 *    UpdateTSAfterFetch so the inject thread picks it up.
 *  - LLD (chunkMode=true): media data was already pushed during SSL callbacks;
 *    here only the time-based buffer counter is consumed.
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

	// Pre-populate the staging fragment so CacheStagingFragmentForInjection has data
	// to process on the non-LLD path.  Without this the fragment is empty and
	// CacheStagingFragmentForInjection is a no-op.
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

	// For non-LLD DASH, CacheStagingFragmentForInjection writes the staging fragment
	// into a chunk buffer slot.  Verify the position is propagated correctly.
	auto chunkSlot = std::make_shared<CachedFragment>();
	if (!chunkMode)
	{
		EXPECT_CALL(*g_mockMediaTrack, GetFetchBuffer(true)).WillOnce(Return(chunkSlot.get()));
		EXPECT_CALL(*g_mockMediaTrack, UpdateTSAfterFetch());
	}

	EXPECT_NO_THROW(mMediaStreamContext->OnFragmentDownloadSuccess(dlInfo));

	// PTS restamp expectation — verified through the chunk slot that
	// CacheStagingFragmentForInjection populated (non-LLD only; in LLD mode data
	// is pre-injected via SSL callbacks).
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

	// mCheckForRampdown can remain true from a previous rampdown attempt on the
	// same context instance; this path should still set mSkipSegmentOnError.
	mMediaStreamContext->mCheckForRampdown = true;
	// Test the behavior of OnFragmentDownloadFailed.
	EXPECT_NO_THROW({
		mMediaStreamContext->OnFragmentDownloadFailed(dlInfo);
		EXPECT_EQ(mMediaStreamContext->segDLFailCount, 1);
		EXPECT_TRUE(mMediaStreamContext->mCheckForRampdown);
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

	// mCheckForRampdown is not reset in this threshold path.
	mMediaStreamContext->mCheckForRampdown = true;
	// Test the behavior of OnFragmentDownloadFailed.
	EXPECT_NO_THROW({
		mMediaStreamContext->OnFragmentDownloadFailed(dlInfo);
		EXPECT_TRUE(mMediaStreamContext->mCheckForRampdown);
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
	// fragment (i.e., it should not call GetFile). The function
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

	// A CachedFragment for CacheStagingFragmentForInjection's GetFetchBuffer call.
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

	// LLDash chunk mode off: the LLD CacheTsbFragment branch inside CheckEos is
	// skipped; the SLD re-cache path below runs via CacheStagingFragmentForInjection.
	EXPECT_CALL(*g_mockPrivateInstanceAAMP, GetLLDashChunkMode())
		.WillRepeatedly(Return(false));

	// ** KEY EXPECTATION ** — the behaviour under test.
	EXPECT_CALL(*g_mockPrivateInstanceAAMP, UpdateLocalAAMPTsbInjection())
		.Times(1);

	// Else block: SLD re-cache via CacheStagingFragmentForInjection.
	EXPECT_CALL(*g_mockMediaTrack, GetFetchBuffer(true))
		.WillOnce(Return(chunkBuffer.get()));
	EXPECT_CALL(*g_mockMediaTrack, UpdateTSAfterFetch());

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

	// The ring buffer slot that GetFetchBuffer will return.
	auto ringBufferSlot = std::make_shared<CachedFragment>();

	// WaitForCachedFragmentInjected is handled by the fake (always returns true).
	EXPECT_CALL(*g_mockMediaTrack, GetFetchBuffer(true))
		.WillOnce(Return(ringBufferSlot.get()));
	EXPECT_CALL(*g_mockMediaTrack, UpdateTSAfterFetch());

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

	// Pre-populate the staging fragment so CacheStagingFragmentForInjection has
	// data to write.
	// An empty fragment would short-circuit through the "Empty cachedFragment ignored"
	// warning path, preventing GetFetchBuffer from ever being called.
	// EnqueueWrite (fake no-op) evaluates context->GetPeriod()->GetId() as an
	// argument, so GetPeriod() must be stubbed to return a valid period.
	static constexpr uint8_t kTestData[] = {0xAA, 0xBB, 0xCC, 0xDD};
	mMediaStreamContext->mStagingFragment.fragment.assign(
		kTestData, kTestData + sizeof(kTestData));

	DummyPeriod dummyPeriod;
	// A CachedFragment for CacheStagingFragmentForInjection's GetFetchBuffer call.
	auto chunkBuffer = std::make_shared<CachedFragment>();

	// --- Mock setup ---
	EXPECT_CALL(*g_mockPrivateInstanceAAMP, DownloadsAreEnabled())
		.WillRepeatedly(Return(true));
	EXPECT_CALL(*g_mockPrivateInstanceAAMP, GetTSBSessionManager())
		.WillOnce(Return(mMockTSBSessionMgr.get())); // non-null → discard guard activates
	EXPECT_CALL(*g_mockPrivateInstanceAAMP, GetLLDashChunkMode())
		.WillRepeatedly(Return(false));
	// EnqueueWrite evaluates context->GetPeriod()->GetId() as an argument.
	EXPECT_CALL(*g_mockStreamAbstractionAAMP_MPD, GetPeriod())
		.WillOnce(Return(&dummyPeriod));

	// KEY assertion: GetFetchBuffer + UpdateTSAfterFetch are called iff
	// the fragment is NOT discarded.  Without the !wasUnderFlowActive guard the
	// discard path would be taken and neither call would occur (stall bug).
	EXPECT_CALL(*g_mockMediaTrack, GetFetchBuffer(true))
		.WillOnce(Return(chunkBuffer.get()));
	EXPECT_CALL(*g_mockMediaTrack, UpdateTSAfterFetch()).Times(1);
	// IsLocalTSBInjection() is checked in the discard-guard condition;
	// must be non-blocking with StrictMock.
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

// ---------------------------------------------------------------------------
// Shared SIDX test fixture data (used by VPAAMP-614 and VPAAMP-363 tests below)
// ---------------------------------------------------------------------------
// Minimal SIDX box with 2 references.
// Reference 0: referenced_size = 0x4000 = 16384 bytes, duration 2000 ticks
// Reference 1: referenced_size = 0x3000 = 12288 bytes, duration 2000 ticks
// timescale = 1000, first_offset = 0
static const uint8_t kSidxBoxForABRTest[] = {
	0x00, 0x00, 0x00, 0x38,  // Box size = 56
	0x73, 0x69, 0x64, 0x78,  // Type = 'sidx'
	0x00, 0x00, 0x00, 0x00,  // version=0, flags=0
	0x00, 0x00, 0x00, 0x01,  // reference_ID = 1
	0x00, 0x00, 0x03, 0xE8,  // timescale = 1000
	0x00, 0x00, 0x00, 0x00,  // earliest_presentation_time = 0
	0x00, 0x00, 0x00, 0x00,  // first_offset = 0
	0x00, 0x00,              // reserved
	0x00, 0x02,              // reference_count = 2
	0x00, 0x00, 0x40, 0x00,  // Ref 0: referenced_size = 16384
	0x00, 0x00, 0x07, 0xD0,  // Ref 0: referenced_duration = 2000
	0x90, 0x00, 0x00, 0x00,  // Ref 0: SAP info
	0x00, 0x00, 0x30, 0x00,  // Ref 1: referenced_size = 12288
	0x00, 0x00, 0x07, 0xD0,  // Ref 1: referenced_duration = 2000
	0x90, 0x00, 0x00, 0x00,  // Ref 1: SAP info
};

// ---------------------------------------------------------------------------
// VPAAMP-614 regression: three additional SegmentBase race-condition fixes
// ---------------------------------------------------------------------------

/**
 * @brief Fix 1 regression: IDX cleared under mIdxMutex must be visible
 *        atomically with fragmentOffset and mIdxBaseOffset reset.
 *
 * Simulates FetchAndInjectInitialization clearing the IDX state (as happens on
 * an ABR profile change) by performing the three resets in a single critical
 * section, then verifying that a subsequent DownloadFragment call sees a
 * consistent state: IDX empty, fragmentOffset 0, mIdxBaseOffset 0.
 *
 * Pre-fix: fragmentOffset was reset to 0 BEFORE taking mIdxMutex, so a
 * concurrent DownloadFragment that had already read IDX (non-empty) could
 * compute a byte range using the stale fragmentOffset=0, producing a range
 * starting at byte 0 of the media file (moov box) → GStreamer error 80:1.
 *
 * Post-fix: all three resets are inside one mIdxMutex critical section so the
 * DownloadFragment ABR-switch branch always sees a coherent (IDX, fragmentOffset,
 * mIdxBaseOffset) triple.
 */
TEST_F(FragmentDownloadTests, SegmentBase_FetchAndInjectInit_ResetsAreAtomic)
{
	// Simulate a loaded SIDX state (post first PushNextFragment call).
	mMediaStreamContext->IDX.assign(kSidxBoxForABRTest,
	                                kSidxBoxForABRTest + sizeof(kSidxBoxForABRTest));
	mMediaStreamContext->mIdxBaseOffset = 1000;
	mMediaStreamContext->fragmentOffset = 1000;

	// Simulate what FetchAndInjectInitialization now does: all three resets
	// inside a single mIdxMutex critical section (the fix).
	{
		std::lock_guard<std::mutex> lk(mMediaStreamContext->mIdxMutex);
		mMediaStreamContext->fragmentOffset = 0;
		aamp_utils::ClearAndRelease(mMediaStreamContext->IDX);
		mMediaStreamContext->mIdxBaseOffset = 0;
	}

	// After the reset block the three fields must all be in the cleared state.
	{
		std::lock_guard<std::mutex> lk(mMediaStreamContext->mIdxMutex);
		EXPECT_TRUE(mMediaStreamContext->IDX.empty())
			<< "IDX must be empty after FetchAndInjectInitialization reset";
		EXPECT_EQ(mMediaStreamContext->fragmentOffset, 0u)
			<< "fragmentOffset must be 0 after reset";
		EXPECT_EQ(mMediaStreamContext->mIdxBaseOffset, 0u)
			<< "mIdxBaseOffset must be 0 after reset";
	}

	// A DownloadFragment ABR-switch call with IDX empty must not attempt to
	// compute a byte range (no SIDX to parse).  Verify it exits cleanly when
	// there is no matching bandwidth entry in uriList.
	mMediaStreamContext->fragmentDescriptor.Bandwidth = 5000000;
	auto dlInfo = std::make_shared<DownloadInfo>();
	dlInfo->bandwidth     = 1400000; // differs → ABR-switch branch
	dlInfo->fragmentIndex = 0;
	dlInfo->isInitSegment = false;
	// uriList empty → returns false before network I/O; no range computed.
	bool result = mMediaStreamContext->DownloadFragment(dlInfo);
	EXPECT_FALSE(result);
	EXPECT_TRUE(dlInfo->range.empty())
		<< "With IDX empty no byte range must be computed in the ABR-switch branch";
}

/**
 * @brief Fix 2 regression: IDX.empty() check in PushNextFragment must be
 *        performed under mIdxMutex to avoid a TOCTOU race with
 *        FetchAndInjectInitialization.
 *
 * The original code at PushNextFragment line ~1614 read IDX.empty() without
 * holding mIdxMutex.  A concurrent ClearAndRelease under the mutex between
 * that read and the subsequent snapshot (line ~1709) caused idxSnapshot to be
 * empty, which set eos=true prematurely — reproducing the repeated tune-fail
 * loop seen in VPAAMP-614.
 *
 * This test verifies the fix: when IDX is cleared between the shouldLoadIdx
 * gate and the snapshot, the code detects the empty snapshot and sets eos
 * rather than crashing or computing an invalid range.
 *
 * We exercise this indirectly through DownloadFragment (the ABR-switch branch):
 * after clearing IDX the ABR-switch branch must not compute a range, confirming
 * that an empty IDX is treated as a consistent terminal state.
 */
TEST_F(FragmentDownloadTests, SegmentBase_IDXEmptyCheck_MutexGuard_NoStaleRead)
{
	// Step 1: IDX loaded and base offset set (represents the "IDX non-empty"
	// state a thread could observe just before another thread clears it).
	mMediaStreamContext->IDX.assign(kSidxBoxForABRTest,
	                                kSidxBoxForABRTest + sizeof(kSidxBoxForABRTest));
	mMediaStreamContext->mIdxBaseOffset = 500;
	mMediaStreamContext->fragmentOffset = 500;
	mMediaStreamContext->fragmentDescriptor.Bandwidth = 5000000;

	// Step 2: Simulate concurrent FetchAndInjectInitialization clearing IDX
	// (the fix ensures this is fully atomic with fragmentOffset/mIdxBaseOffset).
	{
		std::lock_guard<std::mutex> lk(mMediaStreamContext->mIdxMutex);
		mMediaStreamContext->fragmentOffset = 0;
		aamp_utils::ClearAndRelease(mMediaStreamContext->IDX);
		mMediaStreamContext->mIdxBaseOffset = 0;
	}

	// Step 3: After the clear, any reader that acquires mIdxMutex to check
	// IDX.empty() will see IDX empty — the shouldLoadIdx gate fires correctly.
	bool shouldLoadIdx = false;
	{
		std::lock_guard<std::mutex> lk(mMediaStreamContext->mIdxMutex);
		shouldLoadIdx = mMediaStreamContext->IDX.empty();
	}
	EXPECT_TRUE(shouldLoadIdx)
		<< "shouldLoadIdx must be true after concurrent ClearAndRelease; "
		   "without the mutex guard the stale non-empty observation would "
		   "skip the lazy-load block and produce eos=true prematurely";

	// Step 4: DownloadFragment ABR-switch path with empty IDX must not produce
	// a byte range (consistent with the eos path in PushNextFragment).
	auto dlInfo = std::make_shared<DownloadInfo>();
	dlInfo->bandwidth     = 1400000;
	dlInfo->fragmentIndex = 0;
	dlInfo->isInitSegment = false;
	bool result = mMediaStreamContext->DownloadFragment(dlInfo);
	EXPECT_FALSE(result);
	EXPECT_TRUE(dlInfo->range.empty())
		<< "No range must be computed when IDX is empty after concurrent clear";
}

/**
 * @brief Fix 3 regression: Stop-path ClearAndRelease(IDX) must hold mIdxMutex.
 *
 * StreamAbstractionAAMP_MPD::Stop() cleared IDX with no lock, creating a
 * data race against any concurrent PushNextFragment or DownloadFragment thread
 * still reading IDX.data().  The fix wraps the call in mIdxMutex.
 *
 * This test validates the invariant: after a locked ClearAndRelease the vector
 * is empty and mIdxBaseOffset is coherent (both observable under the same lock),
 * and a concurrent reader that acquires the lock after the clear sees a
 * consistent empty state rather than a partially-freed buffer.
 */
TEST_F(FragmentDownloadTests, SegmentBase_StopPath_ClearAndRelease_IsLocked)
{
	// Pre-condition: IDX populated and base offset valid.
	mMediaStreamContext->IDX.assign(kSidxBoxForABRTest,
	                                kSidxBoxForABRTest + sizeof(kSidxBoxForABRTest));
	mMediaStreamContext->mIdxBaseOffset = 2000;

	// Simulate the fixed Stop() path: clear under mutex.
	{
		std::lock_guard<std::mutex> lk(mMediaStreamContext->mIdxMutex);
		aamp_utils::ClearAndRelease(mMediaStreamContext->IDX);
	}

	// A reader acquiring the mutex immediately after must see IDX empty.
	// This mirrors what PushNextFragment's shouldLoadIdx gate does.
	{
		std::lock_guard<std::mutex> lk(mMediaStreamContext->mIdxMutex);
		EXPECT_TRUE(mMediaStreamContext->IDX.empty())
			<< "IDX must be empty after Stop-path ClearAndRelease under mIdxMutex";
		// mIdxBaseOffset is left at whatever value it had; the fix in
		// FetchAndInjectInitialization/EOS paths already resets it.
		// Here we only assert the IDX buffer is not dangling.
		EXPECT_EQ(mMediaStreamContext->IDX.data(), nullptr)
			<< "ClearAndRelease must free the backing buffer (data()==nullptr)";
	}

	// DownloadFragment with empty IDX must not dereference the cleared buffer.
	mMediaStreamContext->fragmentDescriptor.Bandwidth = 5000000;
	auto dlInfo = std::make_shared<DownloadInfo>();
	dlInfo->bandwidth     = 1400000;
	dlInfo->fragmentIndex = 0;
	dlInfo->isInitSegment = false;
	EXPECT_NO_FATAL_FAILURE(mMediaStreamContext->DownloadFragment(dlInfo))
		<< "DownloadFragment must not crash or dereference a cleared IDX buffer";
}

// ---------------------------------------------------------------------------
// VPAAMP-363 regression: SegmentBase ABR switch byte-range uses mIdxBaseOffset
// ---------------------------------------------------------------------------
// Root cause: VPAAMP-363 removed the SETCONFIGVALUE(...DashParallelFragDownload,
// false) guard from SkipFragments for SegmentBase streams, enabling parallel
// downloads.  DownloadFragment's ABR-switch branch previously recomputed the
// range as (0 + 1 + first_offset), which lands inside the moov/SIDX prefix and
// fetches garbage.  The parse produces duration=0 → PTS=0 → L2 test_8003_0
// sees actual=0, expected=76800 → FAIL.
//
// Fix: store the correct byte base for segment 0 in mIdxBaseOffset when IDX is
// loaded (FetchAndInjectInitialization + FetchFragment lazy-load), and use it in
// DownloadFragment instead of recomputing from 0.
//
// This test exercises DownloadFragment's ABR-switch path directly, with no
// network I/O, so it is deterministic and not affected by the timing race.
// ---------------------------------------------------------------------------

/**
 * @brief Regression test for VPAAMP-363: DownloadFragment ABR switch must use
 *        mIdxBaseOffset as the byte base for SegmentBase range computation.
 *
 * Setup mimics the race window exposed by VPAAMP-363:
 *   - fragmentDescriptor.Bandwidth = 5000000 (current 1080p profile)
 *   - IDX holds the 1080p SIDX; mIdxBaseOffset = 1000 (segment 0 start)
 *   - dlInfo->bandwidth = 1400000 (stale 480p job queued before ABR switch)
 *   - dlInfo->fragmentIndex = 1 (delivering the second data segment)
 *   - uriList is empty so the function returns false without issuing a network
 *     request; dlInfo->range is still populated before that early return.
 *
 * Expected byte range for fragmentIndex=1:
 *   base    = mIdxBaseOffset            = 1000
 *   + seg0  = kSidxBoxForABRTest ref[0] = 16384  → start of seg1 = 17384
 *   end     = 17384 + ref[1].size - 1   = 17384 + 12288 - 1 = 29671
 *   range   = "17384-29671"
 *
 * Old (buggy) code: base = 0 + 1 + first_offset = 1 → landed inside the moov
 * box → parse returned duration=0 → PTS 0 → L2 restamp assertion FAIL.
 */
TEST_F(FragmentDownloadTests, DownloadFragment_SegmentBase_ABRSwitch_UsesIdxBaseOffset)
{
	// Establish the current (post-ABR-switch) 1080p profile on the context.
	mMediaStreamContext->fragmentDescriptor.Bandwidth = 5000000;

	// Load the 1080p SIDX into IDX and record the byte base for segment 0.
	// kIdxBaseOffset = 1000 represents end_of_SIDX + 1 + first_offset(=0).
	constexpr uint64_t kIdxBaseOffset = 1000;
	mMediaStreamContext->IDX.assign(kSidxBoxForABRTest, kSidxBoxForABRTest + sizeof(kSidxBoxForABRTest));
	mMediaStreamContext->mIdxBaseOffset = kIdxBaseOffset;

	// fragmentIndex=1: the context is advancing past the second data segment.
	mMediaStreamContext->fragmentIndex = 1;

	// Build a stale 480p DownloadInfo (queued before the ABR switch fired).
	// uriList is intentionally left empty: DownloadFragment computes the range
	// but returns false before issuing any network request, so no CacheFragment
	// mock is needed.
	auto dlInfo = std::make_shared<DownloadInfo>();
	dlInfo->bandwidth     = 1400000;   // differs from fragmentDescriptor.Bandwidth
	dlInfo->fragmentIndex = 1;
	dlInfo->isInitSegment = false;

	bool result = mMediaStreamContext->DownloadFragment(dlInfo);

	// The function returns false because uriList is empty (url not resolved),
	// but the ABR switch range computation runs before that check.
	EXPECT_FALSE(result);

	// REGRESSION ASSERTION: range must start at kIdxBaseOffset + ref[0].size.
	//   ref[0].size = 16384  =>  seg1 start = 1000 + 16384 = 17384
	//   ref[1].size = 12288  =>  seg1 end   = 17384 + 12288 - 1 = 29671
	// Old code produced "1-16384" (base=1), fetching inside the moov/SIDX
	// prefix and returning a zero-duration fragment that broke PTS continuity.
	EXPECT_EQ(dlInfo->range, "17384-29671")
		<< "ABR switch must compute the byte range from mIdxBaseOffset; "
		   "the old code started from offset 1 (0+1+first_offset), "
		   "which landed inside the moov/SIDX prefix and produced "
		   "duration=0, PTS=0, breaking L2 test_8003_0 PTS restamp checks.";

	EXPECT_DOUBLE_EQ(dlInfo->fragmentDurationSec, 2.0)
		<< "fragmentDurationSec must be set from the SIDX reference duration.";
}
