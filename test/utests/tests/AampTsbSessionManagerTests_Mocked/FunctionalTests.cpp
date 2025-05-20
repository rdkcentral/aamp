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
#include "priv_aamp.h"
#include "AampTSBSessionManager.h"
#include "AampTsbReader.h"
#include "AampConfig.h"
#include "StreamAbstractionAAMP.h"
#include "MockAampConfig.h"
#include "MockPrivateInstanceAAMP.h"
#include "MockTSBReader.h"
#include "MockTSBStore.h"
#include "MockTSBDataManager.h"
#include "MockMediaStreamContext.h"
#include "MockAampUtils.h"
#include <memory>

using ::testing::_;
using ::testing::Return;
using ::testing::NiceMock;
using ::testing::StrictMock;

AampConfig *gpGlobalConfig{nullptr};

// Test fixture.  Set up mocks here.
class AampTsbSessionManagerTests : public ::testing::Test {
protected:
	static constexpr const char *TEST_BASE_URL = "http://server/";
	static constexpr const char *TEST_DATA = "This is a dummy data";
	std::string TEST_PERIOD_ID = "1";

	void SetUp() override
	{
		if (gpGlobalConfig == nullptr)
		{
			gpGlobalConfig = new AampConfig();
		}
		g_mockAampConfig = new NiceMock<MockAampConfig>();
		mAamp = std::make_shared<PrivateInstanceAAMP>(gpGlobalConfig);

		// Create mocks for the AAMP objects
		g_mockPrivateInstanceAAMP = new NiceMock<MockPrivateInstanceAAMP>();
		g_mockTSBReader = std::make_shared<StrictMock<MockTSBReader>>();
		g_mockTSBDataManager = new NiceMock<MockTSBDataManager>();
		g_mockTSBStore = new NiceMock<MockTSBStore>();
		g_mockMediaStreamContext = new NiceMock<MockMediaStreamContext>();
		g_mockAampUtils = new NiceMock<MockAampUtils>();

		// Create a TSBDataManager object with the mock data
		mTsbDataManager = std::make_shared<AampTsbDataManager>();

		TSB::Store::Config expectedTSBConfig;
		expectedTSBConfig.location = "/path/to/tsb/store";
		expectedTSBConfig.maxCapacity = 1024 * 1024 * 100;
		expectedTSBConfig.minFreePercentage = 10;

		mTsbStore = std::make_shared<TSB::Store>(expectedTSBConfig, TSB::LogFunction(), TSB::LogLevel::TRACE);

		EXPECT_CALL(*g_mockPrivateInstanceAAMP, GetTSBStore(_, _, _)).WillRepeatedly(Return(mTsbStore));
		mAampTSBSessionManager = std::make_shared<AampTSBSessionManager>(mAamp.get());
		mAampTSBSessionManager->Init();

		// Create a new MediaStreamContext object with dummy data
		mMediaStreamContext = std::make_shared<MediaStreamContext>(eTRACK_VIDEO, nullptr, mAamp.get(), "dummyName");
	}

	void TearDown() override
	{
		// reset all the shared pointers in Setup() in the reverse order they were created
		delete g_mockAampUtils;
		g_mockAampUtils = nullptr;
		g_mockTSBReader.reset();
		delete (g_mockTSBDataManager);
		g_mockTSBDataManager = nullptr;
		mMediaStreamContext.reset();
		mAampTSBSessionManager.reset();
		//g_mockTsbFragmentData.reset();
		mAamp.reset();
		delete (g_mockTSBStore);
		g_mockTSBStore = nullptr;
		//g_mockTsbInitData.reset();
		delete (g_mockMediaStreamContext);
		g_mockMediaStreamContext = nullptr;
		mTsbDataManager.reset();
		delete (gpGlobalConfig);
		gpGlobalConfig = nullptr;
		delete(g_mockAampConfig);
		g_mockAampConfig = nullptr;
		delete(g_mockPrivateInstanceAAMP);
		g_mockPrivateInstanceAAMP = nullptr;
		mTsbStore.reset();
	}

	std::shared_ptr<PrivateInstanceAAMP> mAamp;
	std::shared_ptr<AampTSBSessionManager> mAampTSBSessionManager;
	std::shared_ptr<MediaStreamContext> mMediaStreamContext;
	std::shared_ptr<AampTsbDataManager> mTsbDataManager;
	std::shared_ptr<TSB::Store> mTsbStore;

	constexpr static double kDefaultBandwidth{1000000}; // 1Mbps
};

// Test calling PushNextTsbFragment() for a track that is disabled
TEST_F(AampTsbSessionManagerTests, TrackDisabled)
{
	const uint32_t numFreeFragments = 2;

	mAampTSBSessionManager->GetTsbReader(eMEDIATYPE_VIDEO)->mTrackEnabled = false;

	EXPECT_FALSE(mAampTSBSessionManager->PushNextTsbFragment(mMediaStreamContext.get(), numFreeFragments));
}

// Test the behaviour when reading the next fragment returns nullptr
TEST_F(AampTsbSessionManagerTests, FindNextNull)
{
	const uint32_t numFreeFragments = 2;

	mAampTSBSessionManager->GetTsbReader(eMEDIATYPE_VIDEO)->mTrackEnabled = true;

	EXPECT_CALL(*g_mockTSBReader, FindNext(_)).WillOnce(Return(nullptr));

	EXPECT_CALL(*g_mockTSBReader, GetPlaybackRate()).WillOnce(Return(AAMP_NORMAL_PLAY_RATE));

	EXPECT_CALL(*g_mockTSBReader, ReadNext(_)).Times(0);

	EXPECT_FALSE(mAampTSBSessionManager->PushNextTsbFragment(mMediaStreamContext.get(), numFreeFragments));
}

// Test the behaviour when there are no free fragments in the cache
TEST_F(AampTsbSessionManagerTests, NoFreeFragments)
{
	const uint32_t numFreeFragments = 0;

	mAampTSBSessionManager->GetTsbReader(eMEDIATYPE_VIDEO)->mTrackEnabled = true;

	EXPECT_CALL(*g_mockTSBReader, FindNext(_)).Times(0);

	EXPECT_CALL(*g_mockTSBReader, ReadNext(_)).Times(0);

	EXPECT_FALSE(mAampTSBSessionManager->PushNextTsbFragment(mMediaStreamContext.get(), numFreeFragments));
}

// Test the behaviour when reading the init fragment fails
TEST_F(AampTsbSessionManagerTests, ReadInitFragmentFailure)
{
	const uint32_t numFreeFragments = 2;

	// Create a dummy TsbInitData object (needed for the constructor)
	std::shared_ptr<TsbInitData> mockInitData = std::make_shared<TsbInitData>(
		"dummyInitUrl", eMEDIATYPE_VIDEO, 0.0, StreamInfo{}, "dummyPeriodId", 0
	);

	// Create dummy parameters
	std::string dummyUrl = "dummyUrl";
	AampMediaType dummyMediaType = eMEDIATYPE_VIDEO;
	double dummyPosition = 0.0;
	double dummyDuration = 1.0;
	double dummyPts = 0.0;
	bool dummyDisc = false;
	std::string dummyPrId = "dummyPeriodId";
	uint32_t dummyTimeScale = 1000;
	double dummyPTSOffsetSec = 0.0;


	// Create a TsbFragmentData object with the dummy parameters
	auto mockFragmentData{std::make_shared<TsbFragmentData>(
		dummyUrl, dummyMediaType, dummyPosition, dummyDuration, dummyPts, dummyDisc,
		dummyPrId, mockInitData, dummyTimeScale, dummyPTSOffsetSec)};

	mAampTSBSessionManager->GetTsbReader(eMEDIATYPE_VIDEO)->mTrackEnabled = true;

	EXPECT_CALL(*g_mockTSBReader, GetPlaybackRate()).WillOnce(Return(AAMP_NORMAL_PLAY_RATE));

	EXPECT_CALL(*g_mockTSBReader, FindNext(_)).WillOnce(Return(mockFragmentData));

	EXPECT_CALL(*g_mockTSBReader, ReadNext(_)).Times(1);

	EXPECT_CALL(*g_mockTSBStore, GetSize(_)).WillRepeatedly(Return(10));
	// Simulate Read failure for init fragment
	EXPECT_CALL(*g_mockTSBStore, Read(_, _, _)).WillOnce(Return(TSB::Status::FAILED));

	EXPECT_FALSE(mAampTSBSessionManager->PushNextTsbFragment(mMediaStreamContext.get(), numFreeFragments));
}

// Test that the init fragment is not injected if it has not changed
TEST_F(AampTsbSessionManagerTests, SameInitFragment)
{
	const uint32_t numFreeFragments = 2;
	std::string dummyUrl = "dummyUrl";
	AampMediaType dummyMediaType = eMEDIATYPE_VIDEO;
	double dummyPosition = 0.0;
	double dummyDuration = 1.0;
	double dummyPts = 0.0;
	bool dummyDisc = false;
	std::string dummyPrId = "dummyPeriodId";
	uint32_t dummyTimeScale = 1000;
	double dummyPTSOffsetSec = 0.0;

	auto mockInitData = std::make_shared<TsbInitData>("dummyInitUrl", eMEDIATYPE_VIDEO, 0.0, StreamInfo{}, "dummyPeriodId", 0);
	// Create a TsbFragmentData object with the dummy parameters
	auto mockFragmentData{std::make_shared<TsbFragmentData>(
		dummyUrl, dummyMediaType, dummyPosition, dummyDuration, dummyPts, dummyDisc,
		dummyPrId, mockInitData, dummyTimeScale, dummyPTSOffsetSec
	)};

	mAampTSBSessionManager->GetTsbReader(eMEDIATYPE_VIDEO)->mTrackEnabled = true;
	// Last init fragment data is set to the same value as the init fragment data for mockFragmentData
	mAampTSBSessionManager->GetTsbReader(eMEDIATYPE_VIDEO)->mLastInitFragmentData = mockInitData;

	EXPECT_CALL(*g_mockTSBReader, GetPlaybackRate()).WillOnce(Return(AAMP_NORMAL_PLAY_RATE));

	EXPECT_CALL(*g_mockTSBReader, FindNext(_)).WillOnce(Return(mockFragmentData));

	EXPECT_CALL(*g_mockTSBReader, ReadNext(mockFragmentData)).Times(1);

	// Called by AampTSBSessionManager::Read(). It should be called only once
	// for the media fragment. It has to return a value > 0
	EXPECT_CALL(*g_mockTSBStore, GetSize(_)).WillOnce(Return(10));

	// Called only once for the media fragment injection
	EXPECT_CALL(*g_mockMediaStreamContext, CacheTsbFragment(_)).WillOnce(Return(true));

	EXPECT_TRUE(mAampTSBSessionManager->PushNextTsbFragment(mMediaStreamContext.get(), numFreeFragments));
}

// Test that the init fragment is injected if it has changed
TEST_F(AampTsbSessionManagerTests, FirstDownload_Success)
{
	const uint32_t numFreeFragments = 2;
	std::string dummyUrl = "dummyUrl";
	AampMediaType dummyMediaType = eMEDIATYPE_VIDEO;
	double dummyPosition = 0.0;
	double dummyDuration = 1.0;
	double dummyPts = 0.0;
	bool dummyDisc = false;
	std::string dummyPrId = "dummyPeriodId";
	uint32_t dummyTimeScale = 1000;
	double dummyPTSOffsetSec = 0.0;

	auto mockInitData = std::make_shared<TsbInitData>("dummyInitUrl", eMEDIATYPE_VIDEO, 0.0, StreamInfo{}, "dummyPeriodId", 0);
	// Create a TsbFragmentData object with the dummy parameters
	auto mockFragmentData{std::make_shared<TsbFragmentData>(
		dummyUrl, dummyMediaType, dummyPosition, dummyDuration, dummyPts, dummyDisc,
		dummyPrId, mockInitData, dummyTimeScale, dummyPTSOffsetSec
	)};

	mAampTSBSessionManager->GetTsbReader(eMEDIATYPE_VIDEO)->mTrackEnabled = true;

	EXPECT_CALL(*g_mockTSBReader, GetPlaybackRate()).WillOnce(Return(AAMP_NORMAL_PLAY_RATE));

	EXPECT_CALL(*g_mockTSBReader, FindNext(_)).WillOnce(Return(mockFragmentData));

	EXPECT_CALL(*g_mockTSBReader, ReadNext(mockFragmentData)).Times(1);

	// Called by AampTSBSessionManager::Read(), once for the init fragment and
	// once for the first media fragment. It has to return a value > 0
	EXPECT_CALL(*g_mockTSBStore, GetSize(_)).Times(2).WillRepeatedly(Return(10));

	// Called for the init fragment injection followed by the first media fragment
	EXPECT_CALL(*g_mockMediaStreamContext, CacheTsbFragment(_)).Times(2).WillRepeatedly(Return(true));

	EXPECT_TRUE(mAampTSBSessionManager->PushNextTsbFragment(mMediaStreamContext.get(), numFreeFragments));
}

// Test that the init fragment is injected but the fragment is not
TEST_F(AampTsbSessionManagerTests, OnlyFreeFragmentForInit)
{
	const uint32_t numFreeFragments = 2;
	std::string dummyUrl = "dummyUrl";
	AampMediaType dummyMediaType = eMEDIATYPE_VIDEO;
	double dummyPosition = 0.0;
	double dummyDuration = 1.0;
	double dummyPts = 0.0;
	bool dummyDisc = false;
	std::string dummyPrId = "dummyPeriodId";
	uint32_t dummyTimeScale = 1000;
	double dummyPTSOffsetSec = 0.0;

	auto mockInitData = std::make_shared<TsbInitData>("dummyInitUrl", eMEDIATYPE_VIDEO, dummyPosition, StreamInfo{}, "dummyPeriodId", 0);
	// Create a TsbFragmentData object with the dummy parameters
	auto mockFragmentData{std::make_shared<TsbFragmentData>(
		dummyUrl, dummyMediaType, dummyPosition, dummyDuration, dummyPts, dummyDisc,
		dummyPrId, mockInitData, dummyTimeScale, dummyPTSOffsetSec
	)};

	mAampTSBSessionManager->GetTsbReader(eMEDIATYPE_VIDEO)->mTrackEnabled = true;

	EXPECT_CALL(*g_mockTSBReader, GetPlaybackRate()).WillRepeatedly(Return(AAMP_NORMAL_PLAY_RATE));

	EXPECT_CALL(*g_mockTSBReader, FindNext(_)).WillOnce(Return(mockFragmentData));
	EXPECT_CALL(*g_mockTSBReader, ReadNext(_)).Times(0);
	// CacheFragment not called because need space for both init and media fragments
	EXPECT_CALL(*g_mockTSBStore, GetSize(_)).Times(0);
	EXPECT_CALL(*g_mockMediaStreamContext, CacheTsbFragment(_)).Times(0);
	EXPECT_FALSE(mAampTSBSessionManager->PushNextTsbFragment(mMediaStreamContext.get(), 1));

	EXPECT_CALL(*g_mockTSBReader, FindNext(_)).WillOnce(Return(mockFragmentData));
	EXPECT_CALL(*g_mockTSBReader, ReadNext(_)).Times(1);
	// Called twice for init and media fragments
	EXPECT_CALL(*g_mockTSBStore, GetSize(_)).Times(2).WillRepeatedly(Return(10));
	EXPECT_CALL(*g_mockMediaStreamContext, CacheTsbFragment(_)).Times(2).WillRepeatedly(Return(true));
	EXPECT_TRUE(mAampTSBSessionManager->PushNextTsbFragment(mMediaStreamContext.get(), numFreeFragments));
}

// Test that when skip fragments is called, the next fragment is read
// and the init fragment for the 2nd test fragment is injected
TEST_F(AampTsbSessionManagerTests, SkipFragments)
{
	const uint32_t numFreeFragments = 2;
	std::string dummyUrl = "dummyUrl";
	AampMediaType dummyMediaType = eMEDIATYPE_VIDEO;
	double dummyPosition = 0.0;
	double dummyDuration = 1.0;
	double dummyPts = 0.0;
	bool dummyDisc = false;
	std::string dummyPrId = "dummyPeriodId";
	uint32_t dummyTimeScale = 1000;
	double dummyPTSOffsetSec = 0.0;
	StreamInfo dummyStreamInfo;
	dummyStreamInfo.bandwidthBitsPerSecond = kDefaultBandwidth;

	auto mockInitData = std::make_shared<TsbInitData>("dummyInitUrl", eMEDIATYPE_VIDEO, 0.0, dummyStreamInfo, "dummyPeriodId", 0);

	std::shared_ptr<AampTsbReader> tsbReader = mAampTSBSessionManager->GetTsbReader(eMEDIATYPE_VIDEO);
	// Set the bandwidth of the reader to the same value as the init fragment to ensure that the new init fragment is
	// injected, even if the bandwidth does not change.
	tsbReader->mCurrentBandwidth = kDefaultBandwidth;
	tsbReader->mTrackEnabled = true;

	EXPECT_CALL(*g_mockAampConfig, GetConfigValue(eAAMPConfig_VODTrickPlayFPS)).WillRepeatedly(Return(4));

	// Force SkipFragment to be called
	EXPECT_CALL(*g_mockTSBReader, GetPlaybackRate()).WillRepeatedly(Return(30.0));

	EXPECT_CALL(*g_mockTSBReader, GetPlaybackRate()).WillOnce(Return(AAMP_NORMAL_PLAY_RATE));

	EXPECT_CALL(*g_mockTSBReader, FindNext(_)).WillRepeatedly([=](AampTime offset) mutable {
		static double currentPosition = dummyPosition + offset.inSeconds();
		auto fragmentData = std::make_shared<TsbFragmentData>(
			dummyUrl, dummyMediaType, currentPosition, dummyDuration, dummyPts, dummyDisc,
			dummyPrId, mockInitData, dummyTimeScale, dummyPTSOffsetSec
		);
		return fragmentData;
	});

	EXPECT_CALL(*g_mockTSBReader, ReadNext(_)).Times(1);

	// Called by AampTSBSessionManager::Read(), once for the init fragment and
	// once for the first media fragment. It has to return a value > 0
	EXPECT_CALL(*g_mockTSBStore, GetSize(_)).Times(2).WillRepeatedly(Return(10));

	// Called for the init fragment injection followed by the first media fragment
	EXPECT_CALL(*g_mockMediaStreamContext, CacheTsbFragment(_)).Times(2).WillRepeatedly(Return(true));

	// Call PushNextTsbFragment, expect the init fragment to be read and injected
	EXPECT_TRUE(mAampTSBSessionManager->PushNextTsbFragment(mMediaStreamContext.get(), numFreeFragments));
}

// Test that EnqueueWrite does not call RecalculatePTS when TSBWrite is called with the wrong media type
TEST_F(AampTsbSessionManagerTests, TSBWriteTests_WrongMediaType)
{
	std::shared_ptr<CachedFragment> cachedFragment = std::make_shared<CachedFragment>();
	double FRAG_DURATION = 3.0;

	cachedFragment->initFragment = true;
	cachedFragment->duration = 0;
	cachedFragment->position = 0;
	cachedFragment->fragment.AppendBytes(TEST_DATA, strlen(TEST_DATA));
	// Valid media types are only VIDEO, AUDIO, SUBTITLE, AUX_AUDIO and INIT fragments
	cachedFragment->type = eMEDIATYPE_DEFAULT;

	EXPECT_CALL(*g_mockAampUtils, RecalculatePTS(_,_,_,_)).Times(0);
	mAampTSBSessionManager->EnqueueWrite(TEST_BASE_URL, cachedFragment, TEST_PERIOD_ID);
}

// Test EnqueueWrite behaviour for a video init fragment
TEST_F(AampTsbSessionManagerTests, TSBWriteTests_InitFragmentSuccess)
{
	std::shared_ptr<CachedFragment> cachedFragment = std::make_shared<CachedFragment>();
	double FRAG_DURATION = 3.0;

	cachedFragment->initFragment = true;
	cachedFragment->duration = 0;
	cachedFragment->position = 0;
	cachedFragment->fragment.AppendBytes(TEST_DATA, strlen(TEST_DATA));
	cachedFragment->type = eMEDIATYPE_INIT_VIDEO;

	EXPECT_CALL(*g_mockAampUtils, RecalculatePTS(eMEDIATYPE_INIT_VIDEO, _, _, _)).Times(1).WillOnce(Return(0.0));
	mAampTSBSessionManager->EnqueueWrite(TEST_BASE_URL, cachedFragment, TEST_PERIOD_ID);
}
