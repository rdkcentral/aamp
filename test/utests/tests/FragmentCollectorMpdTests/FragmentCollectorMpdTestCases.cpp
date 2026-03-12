/*
 * If not stated otherwise in this file or this component's license file the
 * following copyrights and licenses apply:
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
#include "fragmentcollector_mpd.h"
#include "MediaStreamContext.h"
#include "AampTsbReader.h"
#include "MockTSBSessionManager.h"
#include "MockPrivateInstanceAAMP.h"
#include "MockTSBReader.h"

using ::testing::_;
using ::testing::NiceMock;
using ::testing::Return;

/**
 * @class TestableStreamAbstractionAAMP_MPD
 * @brief Testable wrapper to access protected methods of StreamAbstractionAAMP_MPD
 */
class TestableStreamAbstractionAAMP_MPD : public StreamAbstractionAAMP_MPD
{
public:
	TestableStreamAbstractionAAMP_MPD(PrivateInstanceAAMP* aamp, double seekTime, float rate)
		: StreamAbstractionAAMP_MPD(aamp, seekTime, rate) {}
	
	// Expose protected method for testing
	void TestAdvanceTsbFetch(int trackIdx, bool trickPlay, double delta, bool &waitForFreeFrag, bool &bCacheFullState)
	{
		AdvanceTsbFetch(trackIdx, trickPlay, delta, waitForFreeFrag, bCacheFullState);
	}
	
	// Helper method to set mMediaStreamContext for testing
	void SetMediaStreamContext(int trackIdx, MediaStreamContext* context)
	{
		if (trackIdx >= 0 && trackIdx < AAMP_TRACK_COUNT)
		{
			mMediaStreamContext[trackIdx] = context;
		}
	}
};

/**
 * @class StreamAbstractionAAMP_MPD_Test  
 * @brief Test fixture for StreamAbstractionAAMP_MPD component
 * @details Following L1 guidelines: Test component behavior using fake infrastructure
 */
class StreamAbstractionAAMP_MPD_Test : public ::testing::Test
{
protected:
	void SetUp() override
	{
		// Create a minimal PrivateInstanceAAMP using fake infrastructure
		mPrivateInstanceAAMP = new PrivateInstanceAAMP();
		
		// Create global mocks
		g_mockPrivateInstanceAAMP = new MockPrivateInstanceAAMP();
		g_mockTSBSessionManager = new NiceMock<MockTSBSessionManager>(mPrivateInstanceAAMP);
		g_mockTSBReader = std::make_shared<MockTSBReader>();

		// Test configuration
		mSeekTime = 0.0;
		mRate = AAMP_NORMAL_PLAY_RATE;
		
		// Create the testable component under test
		mMpdStream = new TestableStreamAbstractionAAMP_MPD(mPrivateInstanceAAMP, mSeekTime, mRate);
	}

	void TearDown() override
	{
		delete mMpdStream;
		delete mPrivateInstanceAAMP;
		delete g_mockPrivateInstanceAAMP;
		delete g_mockTSBSessionManager;
		g_mockTSBReader.reset();
		mMpdStream = nullptr;
		mPrivateInstanceAAMP = nullptr;
		g_mockPrivateInstanceAAMP = nullptr;
		g_mockTSBSessionManager = nullptr;
	}

	PrivateInstanceAAMP* mPrivateInstanceAAMP;
	TestableStreamAbstractionAAMP_MPD* mMpdStream;
	double mSeekTime;
	float mRate;
};

/**
 * @brief Test StreamAbstractionAAMP_MPD AdvanceTsbFetch() behavior
 */
TEST_F(StreamAbstractionAAMP_MPD_Test, AdvanceTsbFetchTest)
{
	int trackIdx = 0;
	bool trickPlay = false;
	double delta = 1.0;
	bool waitForFreeFrag = false;
	bool bCacheFullState = false;

	MediaStreamContext* mediaStreamContext = new MediaStreamContext((TrackType)trackIdx, nullptr, mPrivateInstanceAAMP, GetMediaTypeName(AampMediaType(trackIdx)));
    mediaStreamContext->profileChanged = false; // Ensure profile has not changed
	mMpdStream->SetMediaStreamContext(trackIdx, mediaStreamContext);

	std::shared_ptr<AampTsbDataManager> dataMgr = std::make_shared<AampTsbDataManager>();
	std::shared_ptr<AampTsbReader> tsbReader = std::make_shared<AampTsbReader>(mPrivateInstanceAAMP, dataMgr, eMEDIATYPE_VIDEO, "sessionId");
   	ASSERT_NE(tsbReader, nullptr);

	EXPECT_CALL(*g_mockPrivateInstanceAAMP, GetTSBSessionManager()).WillRepeatedly(Return(g_mockTSBSessionManager));
	EXPECT_CALL(*g_mockTSBSessionManager, GetTsbReader(eMEDIATYPE_VIDEO)).WillRepeatedly(Return(tsbReader));
	EXPECT_CALL(*g_mockTSBReader, TrackEnabled()).WillOnce(Return(true));
	EXPECT_CALL(*g_mockTSBReader, IsEos()).WillOnce(Return(false));
	EXPECT_CALL(*g_mockTSBSessionManager, PushNextTsbFragment(mediaStreamContext, _)).WillOnce(Return(true));

	// Call the protected method through testable wrapper
	mMpdStream->TestAdvanceTsbFetch(trackIdx, trickPlay, delta, waitForFreeFrag, bCacheFullState);
}

/**
 * @brief Test StreamAbstractionAAMP_MPD AdvanceTsbFetch() with disabled track
 * @details Verify that PushNextTsbFragment() is not called when track is disabled
 */
TEST_F(StreamAbstractionAAMP_MPD_Test, AdvanceTsbFetchTest_DisabledTrack_NoPushFragment)
{
	int trackIdx = 0;
	bool trickPlay = false;
	double delta = 1.0;
	bool waitForFreeFrag = false;
	bool bCacheFullState = false;

	MediaStreamContext* mediaStreamContext = new MediaStreamContext((TrackType)trackIdx, nullptr, mPrivateInstanceAAMP, GetMediaTypeName(AampMediaType(trackIdx)));
	mediaStreamContext->profileChanged = false; // Ensure profile has not changed
	mMpdStream->SetMediaStreamContext(trackIdx, mediaStreamContext);

	std::shared_ptr<AampTsbDataManager> dataMgr = std::make_shared<AampTsbDataManager>();
	std::shared_ptr<AampTsbReader> tsbReader = std::make_shared<AampTsbReader>(mPrivateInstanceAAMP, dataMgr, eMEDIATYPE_VIDEO, "sessionId");
	ASSERT_NE(tsbReader, nullptr);

	EXPECT_CALL(*g_mockPrivateInstanceAAMP, GetTSBSessionManager()).WillRepeatedly(Return(g_mockTSBSessionManager));
	EXPECT_CALL(*g_mockTSBSessionManager, GetTsbReader(eMEDIATYPE_VIDEO)).WillRepeatedly(Return(tsbReader));
	
	// Mock track as disabled - this should prevent PushNextTsbFragment from being called
	EXPECT_CALL(*g_mockTSBReader, TrackEnabled()).WillOnce(Return(false));
	EXPECT_CALL(*g_mockTSBReader, IsEos()).Times(0); // Should not be called for disabled track
	
	// Verify that PushNextTsbFragment is NOT called when track is disabled
	EXPECT_CALL(*g_mockTSBSessionManager, PushNextTsbFragment(mediaStreamContext, _)).Times(0);

	// Call the protected method through testable wrapper
	mMpdStream->TestAdvanceTsbFetch(trackIdx, trickPlay, delta, waitForFreeFrag, bCacheFullState);
}
