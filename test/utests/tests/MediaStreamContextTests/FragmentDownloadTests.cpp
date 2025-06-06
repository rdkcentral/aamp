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
#include "AampMemoryUtils.h"
#include "isobmff/isobmffbuffer.h"
#include "AampCacheHandler.h"
#include "priv_aamp.h"
#include "AampDRMLicPreFetcherInterface.h"
#include "AampConfig.h"
#include "MockAampConfig.h"
#include "MockMediaTrack.h"
#include "MockStreamAbstractionAAMP.h"
#include "fragmentcollector_mpd.h"
#include "StreamAbstractionAAMP.h"

using namespace testing;

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
	mMediaStreamContext->OnFragmentDownloadSucess(dlInfo);
	// Expect no crash or exception
}

/**
 * @brief Test case for OnFragmentDownloadSuccess with a valid download info
 */
TEST_F(FragmentDownloadTests, OnFragmentDownloadSuccess_ValidDownloadInfo)
{
	mMediaStreamContext->mActiveDownloadInfo = std::make_shared<DownloadInfo>();
	DownloadInfoPtr dlInfo = std::make_shared<DownloadInfo>();
	dlInfo->pts = 123.45;
	dlInfo->fragmentDurationSec = 5.0;
	dlInfo->isDiscontinuity = false;

	// Mock the behavior of GetFetchBuffer, create a dummy CachedFragment
	// and append some data to it to simulate a buffer
	auto cachedFragment = std::make_shared<CachedFragment>();
	cachedFragment->fragment.AppendBytes("test", 4);
	EXPECT_CALL(*g_mockMediaTrack, GetFetchBuffer(false))
		.WillOnce(Return(cachedFragment.get()));

	// Mock the behavior of IsInjectionFromCachedFragmentChunks, return as non-chunk/non-TSB mode
	EXPECT_CALL(*g_mockMediaTrack, IsInjectionFromCachedFragmentChunks())
		.WillRepeatedly(Return(false));

	// Test the behavior of OnFragmentDownloadSuccess in non-chunk mode	
	EXPECT_CALL(*g_mockMediaTrack, UpdateTSAfterFetch(_));
	EXPECT_NO_THROW(mMediaStreamContext->OnFragmentDownloadSucess(dlInfo));
	// Free the cached fragment
	cachedFragment->fragment.Free();
}

/**
 * @brief Test case for OnFragmentDownloadSuccess with a valid download info as chunk mode
 */
TEST_F(FragmentDownloadTests, OnFragmentDownloadSuccess_ValidDownloadInfoChunk)
{
	mMediaStreamContext->mActiveDownloadInfo = std::make_shared<DownloadInfo>();
	DownloadInfoPtr dlInfo = std::make_shared<DownloadInfo>();
	dlInfo->pts = 123.45;
	dlInfo->fragmentDurationSec = 5.0;
	dlInfo->isDiscontinuity = false;

	// Mock the behavior of GetFetchBuffer, create a dummy CachedFragment
	// and append some data to it to simulate a buffer
	auto cachedFragment = std::make_shared<CachedFragment>();
	cachedFragment->fragment.AppendBytes("test", 4);
	EXPECT_CALL(*g_mockMediaTrack, GetFetchBuffer(false))
		.WillOnce(Return(cachedFragment.get()));
	
	// Mock the behavior of IsInjectionFromCachedFragmentChunks, return as chunk mode
	EXPECT_CALL(*g_mockMediaTrack, IsInjectionFromCachedFragmentChunks())
		.WillRepeatedly(Return(true));
	
	// Tes the behaviour of OnFragmentDownloadSuccess in chunk mode
	EXPECT_CALL(*g_mockMediaTrack, UpdateTSAfterFetch(_));
	EXPECT_CALL(*g_mockMediaTrack, UpdateTSAfterInject());
	EXPECT_NO_THROW(mMediaStreamContext->OnFragmentDownloadSucess(dlInfo));
	cachedFragment->fragment.Free();
}

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

	// Set segDLFailCount to 10 and set the fail threshold as 10(default)
	// This should not trigger a ramp down
	mMediaStreamContext->segDLFailCount = 10;
	EXPECT_CALL(*g_mockAampConfig, GetConfigValue(eAAMPConfig_FragmentDownloadFailThreshold)).WillRepeatedly(Return(10));

	// Mock the behavior of GetFetchBuffer, create a dummy CachedFragment
	auto cachedFragment = std::make_shared<CachedFragment>();
	EXPECT_CALL(*g_mockMediaTrack, GetFetchBuffer(false))
		.WillOnce(Return(cachedFragment.get()));

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
	dlInfo->urlList[0].url = "";
	bool result = mMediaStreamContext->DownloadFragment(dlInfo);
	EXPECT_FALSE(result);
}

/**
 * @brief Test case for DownloadFragment with valid DownloadInfo
 */
TEST_F(FragmentDownloadTests, DownloadFragment_ValidDownloadInfo)
{
	DownloadInfoPtr dlInfo = std::make_shared<DownloadInfo>();
	dlInfo->urlList[0].url = "http://example.com/fragment";
	dlInfo->url = "http://example.com/fragment";
	dlInfo->isInitSegment = false;

	auto cachedFragment = std::make_shared<CachedFragment>();
	EXPECT_CALL(*g_mockMediaTrack, GetFetchBuffer(true))
		.WillOnce(Return(cachedFragment.get()));

	EXPECT_NO_THROW({
		bool result = mMediaStreamContext->DownloadFragment(dlInfo);
		EXPECT_TRUE(result);
	});
}
