/*
 * If not stated otherwise in this file or this component's license file the
 * following copyr    // Test parameters for AdvanceTsbFetch
    int trackIdx = 0;                    // Test with video track
    bool trickPlay = false;              // Normal playback mode
    double delta = 1.0;                  // 1 second advance
    bool waitForFreeFrag = false;        // Output parameter
    bool bCacheFullState = false;        // Output parameter
    
    // Set up MediaStreamContext using the helper method
    MediaStreamContext* context = new MediaStreamContext((TrackType)trackIdx, mpdStream, aamp, "TestTrack");
    mpdStream->SetMediaStreamContext(trackIdx, context);
    
    // Call the protected method through testable wrapper
    mpdStream->TestAdvanceTsbFetch(trackIdx, trickPlay, delta, waitForFreeFrag, bCacheFullState);icenses apply:
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

/**
 * @file FragmentCollectorMpdTests.cpp  
 * @brief L1 Unit Test Cases for StreamAbstractionAAMP_MPD
 * @details Following updated L1 testing instructions:
 *          - Test component behavior, not mock/fake behavior
 *          - Use fakes infrastructure properly
 *          - Focus on YOUR component's state and responses
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
		seekTime = 0.0;
		rate = 1.0f;
		
		// Create the testable component under test
		mpdStream = new TestableStreamAbstractionAAMP_MPD(mPrivateInstanceAAMP, seekTime, rate);
	}

	void TearDown() override
	{
		delete mpdStream;
		delete mPrivateInstanceAAMP;
		delete g_mockPrivateInstanceAAMP;
		delete g_mockTSBSessionManager;
		g_mockTSBReader.reset();
		mpdStream = nullptr;
		mPrivateInstanceAAMP = nullptr;
		g_mockPrivateInstanceAAMP = nullptr;
		g_mockTSBSessionManager = nullptr;
	}

	PrivateInstanceAAMP* mPrivateInstanceAAMP;
	TestableStreamAbstractionAAMP_MPD* mpdStream;
	double seekTime;
	float rate;
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
	mpdStream->SetMediaStreamContext(trackIdx, mediaStreamContext);

	std::shared_ptr<AampTsbDataManager> dataMgr = std::make_shared<AampTsbDataManager>();
	std::shared_ptr<AampTsbReader> tsbReader = std::make_shared<AampTsbReader>(mPrivateInstanceAAMP, dataMgr, eMEDIATYPE_VIDEO, "sessionId");
   	ASSERT_NE(tsbReader, nullptr);

	EXPECT_CALL(*g_mockPrivateInstanceAAMP, GetTSBSessionManager()).WillRepeatedly(Return(g_mockTSBSessionManager));
	EXPECT_CALL(*g_mockTSBSessionManager, GetTsbReader(eMEDIATYPE_VIDEO)).WillRepeatedly(Return(tsbReader));
	EXPECT_CALL(*g_mockTSBReader, TrackEnabled()).WillOnce(Return(true));
	EXPECT_CALL(*g_mockTSBReader, IsEos()).WillOnce(Return(false));
	EXPECT_CALL(*g_mockTSBSessionManager, PushNextTsbFragment(mediaStreamContext, _)).WillOnce(Return(true));

	// Call the protected method through testable wrapper
	mpdStream->TestAdvanceTsbFetch(trackIdx, trickPlay, delta, waitForFreeFrag, bCacheFullState);
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
	mpdStream->SetMediaStreamContext(trackIdx, mediaStreamContext);

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
	mpdStream->TestAdvanceTsbFetch(trackIdx, trickPlay, delta, waitForFreeFrag, bCacheFullState);
}
