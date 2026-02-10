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
#include "MockAampMPDParseHelper.h"

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
	bool TestAdvanceTsbFetch(int trackIdx, bool trickPlay, double delta, bool &waitForFreeFrag, bool &bCacheFullState)
	{
		return AdvanceTsbFetch(trackIdx, trickPlay, delta, waitForFreeFrag, bCacheFullState);
	}
	
	// Helper method to set mMediaStreamContext for testing
	void SetMediaStreamContext(int trackIdx, MediaStreamContext* context)
	{
		if (trackIdx >= 0 && trackIdx < AAMP_TRACK_COUNT)
		{
			mMediaStreamContext[trackIdx] = context;
		}
	}

	// Setter method to initialize mMPDParseHelper for testing
	void SetMPDParseHelper(AampMPDParseHelperPtr mpdParseHelperPtr)
	{
		this->mMPDParseHelper = mpdParseHelperPtr;
	}

	// Expose public methods for testing
	using StreamAbstractionAAMP_MPD::ShouldCheckOnlyIframeAdaptation;
	using StreamAbstractionAAMP_MPD::IsEmptyPeriod;
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
		g_mockAampMPDParseHelper = new MockAampMPDParseHelper();

		// Test configuration
		mSeekTime = 0.0;
		mRate = AAMP_NORMAL_PLAY_RATE;
		
		// Create the testable component under test
		mMpdStream = new TestableStreamAbstractionAAMP_MPD(mPrivateInstanceAAMP, mSeekTime, mRate);
		
		// Initialize mMPDParseHelper to avoid NULL dereference in tests
		mMpdStream->SetMPDParseHelper(std::make_shared<AampMPDParseHelper>());
	}

	void TearDown() override
	{
		delete mMpdStream;
		delete mPrivateInstanceAAMP;
		delete g_mockPrivateInstanceAAMP;
		delete g_mockTSBSessionManager;
		delete g_mockAampMPDParseHelper;
		g_mockTSBReader.reset();
		mMpdStream = nullptr;
		mPrivateInstanceAAMP = nullptr;
		g_mockPrivateInstanceAAMP = nullptr;
		g_mockTSBSessionManager = nullptr;
		g_mockAampMPDParseHelper = nullptr;
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
	bool result = mMpdStream->TestAdvanceTsbFetch(trackIdx, trickPlay, delta, waitForFreeFrag, bCacheFullState);

	EXPECT_TRUE(result);
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
	bool result = mMpdStream->TestAdvanceTsbFetch(trackIdx, trickPlay, delta, waitForFreeFrag, bCacheFullState);

	EXPECT_FALSE(result);
}

/**
 * @brief Test ShouldCheckOnlyIframeAdaptation() with normal playback rate
 * @details Verify that the method returns false when playing at normal rate (1.0)
 */
TEST_F(StreamAbstractionAAMP_MPD_Test, ShouldCheckOnlyIframeAdaptation_NormalRate_ReturnsFalse)
{
	// Set normal playback rate
	mPrivateInstanceAAMP->rate = AAMP_NORMAL_PLAY_RATE;

	// Ensure TSB is disabled
	mPrivateInstanceAAMP->SetLocalAAMPTsb(false);

	// Should return false for normal playback rate
	EXPECT_FALSE(mMpdStream->ShouldCheckOnlyIframeAdaptation());
}

/**
 * @brief Test ShouldCheckOnlyIframeAdaptation() with fast forward rate
 * @details Verify that the method returns true when playing at fast forward rate
 */
TEST_F(StreamAbstractionAAMP_MPD_Test, ShouldCheckOnlyIframeAdaptation_FastForward_ReturnsTrue)
{
	// Set fast forward rate (e.g., 4x)
	mPrivateInstanceAAMP->rate = 4.0f;

	// Ensure TSB is disabled
	mPrivateInstanceAAMP->SetLocalAAMPTsb(false);

	// Should return true for trick play mode
	EXPECT_TRUE(mMpdStream->ShouldCheckOnlyIframeAdaptation());
}

/**
 * @brief Test ShouldCheckOnlyIframeAdaptation() with rewind rate
 * @details Verify that the method returns true when playing at rewind rate
 */
TEST_F(StreamAbstractionAAMP_MPD_Test, ShouldCheckOnlyIframeAdaptation_Rewind_ReturnsTrue)
{
	// Set rewind rate (e.g., -4x)
	mPrivateInstanceAAMP->rate = -4.0f;

	// Ensure TSB is disabled
	mPrivateInstanceAAMP->SetLocalAAMPTsb(false);

	// Should return true for trick play mode
	EXPECT_TRUE(mMpdStream->ShouldCheckOnlyIframeAdaptation());
}

/**
 * @brief Test ShouldCheckOnlyIframeAdaptation() with slow motion rate
 * @details Verify that the method returns true when playing at slow motion rate
 */
TEST_F(StreamAbstractionAAMP_MPD_Test, ShouldCheckOnlyIframeAdaptation_SlowMotion_ReturnsTrue)
{
	// Set slow motion rate (e.g., 0.5x)
	mPrivateInstanceAAMP->rate = 0.5f;

	// Ensure TSB is disabled
	mPrivateInstanceAAMP->SetLocalAAMPTsb(false);

	// Should return true for non-normal playback rate
	EXPECT_TRUE(mMpdStream->ShouldCheckOnlyIframeAdaptation());
}

/**
 * @brief Test ShouldCheckOnlyIframeAdaptation() with paused state
 * @details Verify that the method returns true when paused (rate = 0)
 */
TEST_F(StreamAbstractionAAMP_MPD_Test, ShouldCheckOnlyIframeAdaptation_Paused_ReturnsTrue)
{
	// Set paused rate
	mPrivateInstanceAAMP->rate = AAMP_RATE_PAUSE;

	// Ensure TSB is disabled
	mPrivateInstanceAAMP->SetLocalAAMPTsb(false);

	// Should return true for paused state (not normal rate)
	EXPECT_TRUE(mMpdStream->ShouldCheckOnlyIframeAdaptation());
}

/**
 * @brief Test ShouldCheckOnlyIframeAdaptation() with TSB enabled at normal rate
 * @details Verify that the method returns false when using local AAMP TSB, regardless of rate
 */
TEST_F(StreamAbstractionAAMP_MPD_Test, ShouldCheckOnlyIframeAdaptation_TsbEnabledNormalRate_ReturnsFalse)
{
	// Set normal playback rate
	mPrivateInstanceAAMP->rate = AAMP_NORMAL_PLAY_RATE;

	// Enable AAMP TSB
	mPrivateInstanceAAMP->SetLocalAAMPTsb(true);

	// Should return false when TSB is enabled
	EXPECT_FALSE(mMpdStream->ShouldCheckOnlyIframeAdaptation());
}

/**
 * @brief Test ShouldCheckOnlyIframeAdaptation() with TSB enabled at trick play rate
 * @details Verify that TSB overrides trick play rate and returns false
 */
TEST_F(StreamAbstractionAAMP_MPD_Test, ShouldCheckOnlyIframeAdaptation_TsbEnabledFastForward_ReturnsFalse)
{
	// Set fast forward rate
	mPrivateInstanceAAMP->rate = 4.0f;

	// Enable AAMP TSB - should override the trick play rate
	mPrivateInstanceAAMP->SetLocalAAMPTsb(true);

	// Should return false when TSB is enabled, even in trick play mode
	EXPECT_FALSE(mMpdStream->ShouldCheckOnlyIframeAdaptation());
}

/**
 * @brief Test ShouldCheckOnlyIframeAdaptation() with TSB enabled at rewind rate
 * @details Verify that TSB overrides rewind rate and returns false
 */
TEST_F(StreamAbstractionAAMP_MPD_Test, ShouldCheckOnlyIframeAdaptation_TsbEnabledRewind_ReturnsFalse)
{
	// Set rewind rate
	mPrivateInstanceAAMP->rate = -4.0f;

	// Enable AAMP TSB - should override the trick play rate
	mPrivateInstanceAAMP->SetLocalAAMPTsb(true);

	// Should return false when TSB is enabled, even in trick play mode
	EXPECT_FALSE(mMpdStream->ShouldCheckOnlyIframeAdaptation());
}

/**
 * @brief Test IsEmptyPeriod() returns false when helper indicates non-empty period at normal rate
 * @details Verify delegation to MPDParseHelper with checkIframe=false for normal playback
 */
TEST_F(StreamAbstractionAAMP_MPD_Test, IsEmptyPeriod_NonEmptyPeriodNormalRate_ReturnsFalse)
{
	// Set normal playback rate (ShouldCheckOnlyIframeAdaptation returns false)
	mPrivateInstanceAAMP->rate = AAMP_NORMAL_PLAY_RATE;
	mPrivateInstanceAAMP->SetLocalAAMPTsb(false);

	int periodIndex = 0;

	// Mock helper returns false (period is not empty)
	EXPECT_CALL(*g_mockAampMPDParseHelper, IsEmptyPeriod(periodIndex, false))
		.WillOnce(Return(false));

	// Verify component correctly returns helper's result
	bool result = mMpdStream->IsEmptyPeriod(periodIndex);
	EXPECT_FALSE(result);
}

/**
 * @brief Test IsEmptyPeriod() returns true when helper indicates empty period at normal rate
 * @details Verify component returns true when helper detects empty period
 */
TEST_F(StreamAbstractionAAMP_MPD_Test, IsEmptyPeriod_EmptyPeriodNormalRate_ReturnsTrue)
{
	// Set normal playback rate
	mPrivateInstanceAAMP->rate = AAMP_NORMAL_PLAY_RATE;
	mPrivateInstanceAAMP->SetLocalAAMPTsb(false);

	int periodIndex = 1;

	// Mock helper returns true (period is empty)
	EXPECT_CALL(*g_mockAampMPDParseHelper, IsEmptyPeriod(periodIndex, false))
		.WillOnce(Return(true));

	// Verify component correctly propagates empty period status
	bool result = mMpdStream->IsEmptyPeriod(periodIndex);
	EXPECT_TRUE(result);
}

/**
 * @brief Test IsEmptyPeriod() passes checkIframe=true during trick play
 * @details Verify that fast forward rate causes checkIframe=true to be passed to helper
 */
TEST_F(StreamAbstractionAAMP_MPD_Test, IsEmptyPeriod_TrickPlayMode_PassesCheckIframeTrue)
{
	// Set fast forward rate (ShouldCheckOnlyIframeAdaptation returns true)
	mPrivateInstanceAAMP->rate = 4.0f;
	mPrivateInstanceAAMP->SetLocalAAMPTsb(false);

	int periodIndex = 0;

	// Verify checkIframe=true is passed to helper during trick play
	EXPECT_CALL(*g_mockAampMPDParseHelper, IsEmptyPeriod(periodIndex, true))
		.WillOnce(Return(false));

	bool result = mMpdStream->IsEmptyPeriod(periodIndex);
	EXPECT_FALSE(result);
}

/**
 * @brief Test IsEmptyPeriod() with empty period during trick play
 * @details Verify proper handling when period has no iframe tracks
 */
TEST_F(StreamAbstractionAAMP_MPD_Test, IsEmptyPeriod_EmptyPeriodTrickPlay_ReturnsTrue)
{
	// Set rewind rate (trick play mode)
	mPrivateInstanceAAMP->rate = -4.0f;
	mPrivateInstanceAAMP->SetLocalAAMPTsb(false);

	int periodIndex = 2;

	// Mock helper returns true (no iframe tracks in period)
	EXPECT_CALL(*g_mockAampMPDParseHelper, IsEmptyPeriod(periodIndex, true))
		.WillOnce(Return(true));

	// Verify component correctly identifies empty period during trick play
	bool result = mMpdStream->IsEmptyPeriod(periodIndex);
	EXPECT_TRUE(result);
}

/**
 * @brief Test IsEmptyPeriod() with TSB enabled overrides trick play mode
 * @details Verify TSB causes checkIframe=false even during fast forward
 */
TEST_F(StreamAbstractionAAMP_MPD_Test, IsEmptyPeriod_TsbEnabledTrickPlay_PassesCheckIframeFalse)
{
	// Set trick play rate with TSB enabled
	mPrivateInstanceAAMP->rate = 4.0f;
	mPrivateInstanceAAMP->SetLocalAAMPTsb(true);

	int periodIndex = 0;

	// Verify TSB overrides trick play: checkIframe=false is passed
	EXPECT_CALL(*g_mockAampMPDParseHelper, IsEmptyPeriod(periodIndex, false))
		.WillOnce(Return(false));

	bool result = mMpdStream->IsEmptyPeriod(periodIndex);
	EXPECT_FALSE(result);
}

/**
 * @brief Test IsEmptyPeriod() with TSB enabled at normal rate
 * @details Verify normal rate with TSB passes checkIframe=false
 */
TEST_F(StreamAbstractionAAMP_MPD_Test, IsEmptyPeriod_TsbEnabledNormalRate_PassesCheckIframeFalse)
{
	// Set normal rate with TSB enabled
	mPrivateInstanceAAMP->rate = AAMP_NORMAL_PLAY_RATE;
	mPrivateInstanceAAMP->SetLocalAAMPTsb(true);

	int periodIndex = 1;

	// Mock helper returns true (empty period)
	EXPECT_CALL(*g_mockAampMPDParseHelper, IsEmptyPeriod(periodIndex, false))
		.WillOnce(Return(true));

	bool result = mMpdStream->IsEmptyPeriod(periodIndex);
	EXPECT_TRUE(result);
}

/**
 * @brief Test IsEmptyPeriod() with different period indices
 * @details Verify correct period index is passed to helper
 */
TEST_F(StreamAbstractionAAMP_MPD_Test, IsEmptyPeriod_DifferentIndices_PassesCorrectIndex)
{
	// Set normal playback rate
	mPrivateInstanceAAMP->rate = AAMP_NORMAL_PLAY_RATE;
	mPrivateInstanceAAMP->SetLocalAAMPTsb(false);

	// Test multiple period indices
	EXPECT_CALL(*g_mockAampMPDParseHelper, IsEmptyPeriod(0, false))
		.WillOnce(Return(false));
	EXPECT_FALSE(mMpdStream->IsEmptyPeriod(0));

	EXPECT_CALL(*g_mockAampMPDParseHelper, IsEmptyPeriod(1, false))
		.WillOnce(Return(true));
	EXPECT_TRUE(mMpdStream->IsEmptyPeriod(1));

	EXPECT_CALL(*g_mockAampMPDParseHelper, IsEmptyPeriod(5, false))
		.WillOnce(Return(false));
	EXPECT_FALSE(mMpdStream->IsEmptyPeriod(5));
}

/**
 * @brief Test IsEmptyPeriod() with paused state
 * @details Verify paused state (rate=0) passes checkIframe=true
 */
TEST_F(StreamAbstractionAAMP_MPD_Test, IsEmptyPeriod_PausedState_PassesCheckIframeTrue)
{
	// Set paused rate (ShouldCheckOnlyIframeAdaptation returns true)
	mPrivateInstanceAAMP->rate = AAMP_RATE_PAUSE;
	mPrivateInstanceAAMP->SetLocalAAMPTsb(false);

	int periodIndex = 0;

	// Verify checkIframe=true for paused state
	EXPECT_CALL(*g_mockAampMPDParseHelper, IsEmptyPeriod(periodIndex, true))
		.WillOnce(Return(false));

	bool result = mMpdStream->IsEmptyPeriod(periodIndex);
	EXPECT_FALSE(result);
}

/**
 * @brief Test IsEmptyPeriod() with slow motion rate
 * @details Verify slow motion (0.5x) passes checkIframe=true
 */
TEST_F(StreamAbstractionAAMP_MPD_Test, IsEmptyPeriod_SlowMotion_PassesCheckIframeTrue)
{
	// Set slow motion rate
	mPrivateInstanceAAMP->rate = 0.5f;
	mPrivateInstanceAAMP->SetLocalAAMPTsb(false);

	int periodIndex = 3;

	// Verify checkIframe=true for non-normal rate
	EXPECT_CALL(*g_mockAampMPDParseHelper, IsEmptyPeriod(periodIndex, true))
		.WillOnce(Return(true));

	bool result = mMpdStream->IsEmptyPeriod(periodIndex);
	EXPECT_TRUE(result);
}

/**
 * @brief Test IsEmptyPeriod() correctly propagates both true and false results
 * @details Verify component faithfully returns helper's result without modification
 */
TEST_F(StreamAbstractionAAMP_MPD_Test, IsEmptyPeriod_AlternatingResults_PropagatesCorrectly)
{
	// Set normal playback rate
	mPrivateInstanceAAMP->rate = AAMP_NORMAL_PLAY_RATE;
	mPrivateInstanceAAMP->SetLocalAAMPTsb(false);

	// Sequence: empty, non-empty, empty, non-empty
	EXPECT_CALL(*g_mockAampMPDParseHelper, IsEmptyPeriod(0, false))
		.WillOnce(Return(true));
	EXPECT_TRUE(mMpdStream->IsEmptyPeriod(0));

	EXPECT_CALL(*g_mockAampMPDParseHelper, IsEmptyPeriod(1, false))
		.WillOnce(Return(false));
	EXPECT_FALSE(mMpdStream->IsEmptyPeriod(1));

	EXPECT_CALL(*g_mockAampMPDParseHelper, IsEmptyPeriod(2, false))
		.WillOnce(Return(true));
	EXPECT_TRUE(mMpdStream->IsEmptyPeriod(2));

	EXPECT_CALL(*g_mockAampMPDParseHelper, IsEmptyPeriod(3, false))
		.WillOnce(Return(false));
	EXPECT_FALSE(mMpdStream->IsEmptyPeriod(3));
}
