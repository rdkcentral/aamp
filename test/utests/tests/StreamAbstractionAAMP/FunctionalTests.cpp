/*
* If not stated otherwise in this file or this component's license file the
* following copyright and licenses apply:
*
* Copyright 2023 RDK Management
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
#include <chrono>
#include "priv_aamp.h"
#include "AampConfig.h"
#include "AampLogManager.h"
#include "MediaStreamContext.h"
#include "MockAampConfig.h"
#include "MockAampUtils.h"
#include "MockPrivateInstanceAAMP.h"
#include "MockMediaTrack.h"
#include "MockMediaProcessor.h"

using ::testing::_;
using ::testing::An;
using ::testing::SetArgReferee;
using ::testing::Return;
using ::testing::StrictMock;
using ::testing::NiceMock;
using ::testing::WithArgs;
using ::testing::WithoutArgs;
using ::testing::AnyNumber;
using ::testing::DoAll;
using ::testing::Invoke;

AampConfig *gpGlobalConfig{nullptr};


class StreamAbstractionAAMP_Test : public ::testing::Test
{
protected:

	class TestableStreamAbstractionAAMP : public StreamAbstractionAAMP
	{
	public:
		// Constructor to pass parameters to the base class constructor
		TestableStreamAbstractionAAMP(PrivateInstanceAAMP* aamp)
			: StreamAbstractionAAMP(aamp),
			mMockAudioTrack(nullptr),
			mMockVideoTrack(nullptr),
			mMockSubtitleTrack(nullptr)
		{
		}

		~TestableStreamAbstractionAAMP()
		{
			if (mMockAudioTrack)
			{
				delete mMockAudioTrack;
				mMockAudioTrack = nullptr;
			}
			if (mMockVideoTrack)
			{
				delete mMockVideoTrack;
				mMockVideoTrack = nullptr;
			}
			if (mMockSubtitleTrack)
			{
				delete mMockSubtitleTrack;
				mMockSubtitleTrack = nullptr;
			}
		}

		MockMediaTrack *mMockAudioTrack;
		MockMediaTrack *mMockVideoTrack;
		MockMediaTrack *mMockSubtitleTrack;

		virtual AAMPStatusType Init(TuneType tuneType) override {return eAAMPSTATUS_OK;}
		virtual void Start() override {}
		virtual void Stop(bool clearChannelData) override {}
		virtual void GetStreamFormat(StreamOutputFormat &primaryOutputFormat, StreamOutputFormat &audioOutputFormat, StreamOutputFormat &subtitleOutputFormat) override {}

		virtual MediaTrack* GetMediaTrack(TrackType type) override
		{
			if (type == eTRACK_AUDIO)
				return mMockAudioTrack;
			else if (type == eTRACK_VIDEO)
				return mMockVideoTrack;
			else if (type == eTRACK_SUBTITLE)
				return mMockSubtitleTrack;
			else
				return nullptr;
		}

		void testSetTrackState(MediaTrackDiscontinuityState state)
		{
			mTrackState = state;
		}

		double testGetBufferValue(MediaTrack *track)
		{
			return GetBufferValue(track);
		}

		MOCK_METHOD(void, clearFirstPTS, (), (override));

	};

	PrivateInstanceAAMP *mPrivateInstanceAAMP;
	TestableStreamAbstractionAAMP *mStreamAbstractionAAMP;
	AampConfig *mConfig;
	std::shared_ptr<MockMediaProcessor> mMockMediaProcessor;

	void SetUp() override
	{
		if(gpGlobalConfig == nullptr)
		{
			gpGlobalConfig =  new AampConfig();
		}
		g_mockAampConfig = std::make_shared<NiceMock<MockAampConfig>>();

		if (g_mockPrivateInstanceAAMP == nullptr)
		{
			g_mockPrivateInstanceAAMP = std::make_shared<NiceMock<MockPrivateInstanceAAMP>>();
		}

		mPrivateInstanceAAMP = new PrivateInstanceAAMP(mConfig);
		mStreamAbstractionAAMP = new TestableStreamAbstractionAAMP(mPrivateInstanceAAMP);

		// For initialisation of mediatrack
		EXPECT_CALL(*g_mockAampConfig, GetConfigValue(eAAMPConfig_MaxFragmentCached))
			.Times(AnyNumber())
			.WillRepeatedly(Return(0));
		EXPECT_CALL(*g_mockAampConfig, GetConfigValue(eAAMPConfig_MaxLLDFragmentCached))
			.Times(AnyNumber())
			.WillRepeatedly(Return(0));

		mStreamAbstractionAAMP->mMockAudioTrack    = new MockMediaTrack(eTRACK_AUDIO,    mPrivateInstanceAAMP, "audio");
		mStreamAbstractionAAMP->mMockVideoTrack    = new MockMediaTrack(eTRACK_VIDEO,    mPrivateInstanceAAMP, "video");
		mStreamAbstractionAAMP->mMockSubtitleTrack = new MockMediaTrack(eTRACK_SUBTITLE, mPrivateInstanceAAMP, "subtitle");

		mMockMediaProcessor = std::make_shared<NiceMock<MockMediaProcessor>>();
		mStreamAbstractionAAMP->mMockVideoTrack->playContext = mMockMediaProcessor;
		mStreamAbstractionAAMP->mMockVideoTrack->enabled = true;

		mStreamAbstractionAAMP->mMockAudioTrack->fragmentDurationSeconds    = 1.92;
		mStreamAbstractionAAMP->mMockVideoTrack->fragmentDurationSeconds    = 1.92;
		mStreamAbstractionAAMP->mMockSubtitleTrack->fragmentDurationSeconds = 1.92;

	}

	void TearDown() override
	{
		delete mStreamAbstractionAAMP;
		mStreamAbstractionAAMP = nullptr;

		g_mockPrivateInstanceAAMP.reset();

		delete mPrivateInstanceAAMP;
		mPrivateInstanceAAMP = nullptr;

		delete gpGlobalConfig;
		gpGlobalConfig = nullptr;

		g_mockAampConfig.reset();

		mMockMediaProcessor.reset();
	}
};

// Check that WaitForVideoTrackCatchup() waits till injected buffers match
TEST_F(StreamAbstractionAAMP_Test, WaitFor_VideoTrackCatchup_wait)
{
	EXPECT_CALL(*g_mockPrivateInstanceAAMP, DownloadsAreEnabled())
		.WillRepeatedly(Return(true));

	// Check aamp loops till video catches up
	EXPECT_CALL(*mStreamAbstractionAAMP->mMockAudioTrack, GetTotalInjectedDuration())
		.Times(4)
		.WillRepeatedly(Return(2));

	EXPECT_CALL(*mStreamAbstractionAAMP->mMockVideoTrack, GetTotalInjectedDuration())
		.Times(4)
		.WillOnce(Return(0))
		.WillOnce(Return(0))
		.WillOnce(Return(0))
		.WillOnce(Return(2));

	mStreamAbstractionAAMP->WaitForVideoTrackCatchup();
}

// Check that WaitForVideoTrackCatchup() does not wait if video is processing a discontinuity
TEST_F(StreamAbstractionAAMP_Test, WaitFor_VideoTrackCatchup_discontinuity)
{
	EXPECT_CALL(*g_mockPrivateInstanceAAMP, DownloadsAreEnabled())
		.WillRepeatedly(Return(true));
		
	// Set the flag that indicates processing discontinuity
	mStreamAbstractionAAMP->testSetTrackState(eDISCONTINUITY_IN_VIDEO);

	// Check aamp does not loop till video catches up
	EXPECT_CALL(*mStreamAbstractionAAMP->mMockAudioTrack, GetTotalInjectedDuration())
		.Times(1)
		.WillOnce(Return(2));

	EXPECT_CALL(*mStreamAbstractionAAMP->mMockVideoTrack, GetTotalInjectedDuration())
		.Times(1)
		.WillOnce(Return(0));

	mStreamAbstractionAAMP->WaitForVideoTrackCatchup();
}

TEST_F(StreamAbstractionAAMP_Test, ReinitializeInjection_LLDashChunkModeEnabled)
{
	const double test_rate = 2.0;

	// GetLLDashChunkMode() guard removed — setRate is now always called so that
	// AampMp4Demuxer::setRate can set mIsTrickMode/mRate for PTS restamping.
	EXPECT_CALL(*mStreamAbstractionAAMP, clearFirstPTS());
	EXPECT_CALL(*mStreamAbstractionAAMP->mMockAudioTrack, ResetTrickModePtsRestamping()).Times(1);
	EXPECT_CALL(*mStreamAbstractionAAMP->mMockVideoTrack, ResetTrickModePtsRestamping()).Times(1);
	EXPECT_CALL(*mMockMediaProcessor, setRate(test_rate, PlayMode_normal)).Times(1);
	EXPECT_EQ(mStreamAbstractionAAMP->trickplayMode, false);

	mStreamAbstractionAAMP->ReinitializeInjection(test_rate);
}

TEST_F(StreamAbstractionAAMP_Test, ReinitializeInjection_LLDashChunkModeDisabled)
{
	const double test_rate = 2.0;

	EXPECT_CALL(*mStreamAbstractionAAMP, clearFirstPTS());
	EXPECT_CALL(*mStreamAbstractionAAMP->mMockAudioTrack, ResetTrickModePtsRestamping()).Times(1);
	EXPECT_CALL(*mStreamAbstractionAAMP->mMockVideoTrack, ResetTrickModePtsRestamping()).Times(1);
	EXPECT_CALL(*mMockMediaProcessor, setRate(test_rate, PlayMode_normal)).Times(1);
	EXPECT_EQ(mStreamAbstractionAAMP->trickplayMode, false);

	mStreamAbstractionAAMP->ReinitializeInjection(test_rate);
}
/**
 * @brief Verify UpdateTSAfterFetchStats() re-enables the latency monitor after
 *        an audio track switch when mSavedLatencyMonitorStateAudio was true.
 */
TEST_F(StreamAbstractionAAMP_Test, UpdateTSAfterFetchStats_RestoresLatencyMonitor_WhenSavedStateTrue)
{
        CachedFragment cachedFragment;
        cachedFragment.duration = 1.0;

        // Set GetContext() to return our StreamAbstractionAAMP so
        // pContext->mSavedLatencyMonitorStateAudio is accessible.
        ON_CALL(*mStreamAbstractionAAMP->mMockAudioTrack, GetContext())
                .WillByDefault(Return(mStreamAbstractionAAMP));

        // Simulate: latency monitor was active before the audio track switch.
        mStreamAbstractionAAMP->mSavedLatencyMonitorStateAudio = true;
        mStreamAbstractionAAMP->mMockAudioTrack->LoadNewAudio(true);

        // Expect EnableLatencyMonitor(true) called once to restore the monitor.
        EXPECT_CALL(*g_mockPrivateInstanceAAMP, EnableLatencyMonitor(true)).Times(1);

        mStreamAbstractionAAMP->mMockAudioTrack->UpdateTSAfterFetchStats(&cachedFragment, false);

        // mSavedLatencyMonitorStateAudio must be cleared after restoration.
        EXPECT_FALSE(mStreamAbstractionAAMP->mSavedLatencyMonitorStateAudio);
}

/**
 * @brief Verify UpdateTSAfterFetchStats() does NOT re-enable the latency monitor
 *        after an audio track switch when mSavedLatencyMonitorStateAudio was false.
 */
TEST_F(StreamAbstractionAAMP_Test, UpdateTSAfterFetchStats_DoesNotRestoreLatencyMonitor_WhenSavedStateFalse)
{
        CachedFragment cachedFragment;
        cachedFragment.duration = 1.0;

        ON_CALL(*mStreamAbstractionAAMP->mMockAudioTrack, GetContext())
                .WillByDefault(Return(mStreamAbstractionAAMP));

        // Simulate: latency monitor was NOT active before the audio track switch.
        mStreamAbstractionAAMP->mSavedLatencyMonitorStateAudio = false;
        mStreamAbstractionAAMP->mMockAudioTrack->LoadNewAudio(true);

        // EnableLatencyMonitor(true) must NOT be called.
        EXPECT_CALL(*g_mockPrivateInstanceAAMP, EnableLatencyMonitor(true)).Times(0);

        mStreamAbstractionAAMP->mMockAudioTrack->UpdateTSAfterFetchStats(&cachedFragment, false);

        EXPECT_FALSE(mStreamAbstractionAAMP->mSavedLatencyMonitorStateAudio);
}

/**
 * @brief Verify that concurrent audio+subtitle switches each restore the latency
 *        monitor independently, using their own per-track saved-state flag
 *        (VPAAMP-1195 defect c).
 *
 *        Repro: with a single shared flag, the second RefreshTrack() call reads
 *        IsLatencyMonitorEnabled() as false (already disabled) and overwrites the
 *        first switch's saved state with false.  EnableLatencyMonitor(true) is
 *        never called and LLD latency-rate correction remains permanently disabled.
 */
TEST_F(StreamAbstractionAAMP_Test, LatencyMonitor_ConcurrentAudioAndSubtitleSwitch_BothRestored)
{
        CachedFragment cachedFragment;
        cachedFragment.duration = 1.0;

        ON_CALL(*mStreamAbstractionAAMP->mMockAudioTrack, GetContext())
                .WillByDefault(Return(mStreamAbstractionAAMP));
        ON_CALL(*mStreamAbstractionAAMP->mMockSubtitleTrack, GetContext())
                .WillByDefault(Return(mStreamAbstractionAAMP));

        // Latency monitor was active before both switches started.
        mStreamAbstractionAAMP->mSavedLatencyMonitorStateAudio    = true;
        mStreamAbstractionAAMP->mSavedLatencyMonitorStateSubtitle = true;
        mStreamAbstractionAAMP->mMockAudioTrack->LoadNewAudio(true);
        mStreamAbstractionAAMP->mMockSubtitleTrack->LoadNewSubtitle(true);

        // EnableLatencyMonitor(true) must be called once per completed switch.
        EXPECT_CALL(*g_mockPrivateInstanceAAMP, EnableLatencyMonitor(true)).Times(2);

        // Audio switch completes first.
        mStreamAbstractionAAMP->mMockAudioTrack->UpdateTSAfterFetchStats(&cachedFragment, false);
        EXPECT_FALSE(mStreamAbstractionAAMP->mSavedLatencyMonitorStateAudio);
        // Subtitle flag must still be set — it has its own independent bool.
        EXPECT_TRUE(mStreamAbstractionAAMP->mSavedLatencyMonitorStateSubtitle);

        // Subtitle switch completes.
        mStreamAbstractionAAMP->mMockSubtitleTrack->UpdateTSAfterFetchStats(&cachedFragment, false);
        EXPECT_FALSE(mStreamAbstractionAAMP->mSavedLatencyMonitorStateSubtitle);
}

/**
 * @brief Verify that a subtitle RefreshTrack() (latency monitor was OFF when the
 *        subtitle switch started) does not corrupt the audio switch's saved state.
 *
 *        With the old shared flag the second RefreshTrack() would have set the
 *        shared flag to false, preventing the audio restore from firing.
 */
TEST_F(StreamAbstractionAAMP_Test, LatencyMonitor_SubtitleSwitchLLDOff_DoesNotCorruptAudioState)
{
        CachedFragment cachedFragment;
        cachedFragment.duration = 1.0;

        ON_CALL(*mStreamAbstractionAAMP->mMockAudioTrack, GetContext())
                .WillByDefault(Return(mStreamAbstractionAAMP));
        ON_CALL(*mStreamAbstractionAAMP->mMockSubtitleTrack, GetContext())
                .WillByDefault(Return(mStreamAbstractionAAMP));

        // Audio switch: LLD was ON.  Subtitle switch: LLD was already OFF.
        mStreamAbstractionAAMP->mSavedLatencyMonitorStateAudio    = true;
        mStreamAbstractionAAMP->mSavedLatencyMonitorStateSubtitle = false;
        mStreamAbstractionAAMP->mMockAudioTrack->LoadNewAudio(true);
        mStreamAbstractionAAMP->mMockSubtitleTrack->LoadNewSubtitle(true);

        // Only the audio restore should call EnableLatencyMonitor(true).
        EXPECT_CALL(*g_mockPrivateInstanceAAMP, EnableLatencyMonitor(true)).Times(1);

        // Subtitle switch completes first — must not touch the audio flag.
        mStreamAbstractionAAMP->mMockSubtitleTrack->UpdateTSAfterFetchStats(&cachedFragment, false);
        EXPECT_TRUE(mStreamAbstractionAAMP->mSavedLatencyMonitorStateAudio);

        // Audio switch completes — monitor restored.
        mStreamAbstractionAAMP->mMockAudioTrack->UpdateTSAfterFetchStats(&cachedFragment, false);
        EXPECT_FALSE(mStreamAbstractionAAMP->mSavedLatencyMonitorStateAudio);
}

/**
 * @brief Verify GetBufferedAudioDurationSec() reports audio track buffer
 *        duration when the audio track is enabled.
 */
TEST_F(StreamAbstractionAAMP_Test, GetBufferedAudioDurationSec_ReturnsAudioBuffer_WhenAudioTrackEnabled)
{
	mPrivateInstanceAAMP->rate = AAMP_NORMAL_PLAY_RATE;
	mStreamAbstractionAAMP->mMockAudioTrack->enabled = true;
	mStreamAbstractionAAMP->mMockVideoTrack->enabled = true;

	EXPECT_CALL(*mStreamAbstractionAAMP->mMockAudioTrack, GetBufferedDuration())
		.WillOnce(Return(12.5));
	EXPECT_CALL(*mStreamAbstractionAAMP->mMockVideoTrack, GetBufferedDuration())
		.Times(0);

	EXPECT_DOUBLE_EQ(12.5, mStreamAbstractionAAMP->GetBufferedAudioDurationSec());
}

/**
 * @brief Verify GetBufferedAudioDurationSec() falls back to video buffer
 *        duration when audio is muxed and the audio track is disabled.
 */
TEST_F(StreamAbstractionAAMP_Test, GetBufferedAudioDurationSec_ReturnsVideoBuffer_WhenAudioTrackDisabled)
{
	mPrivateInstanceAAMP->rate = AAMP_NORMAL_PLAY_RATE;
	mStreamAbstractionAAMP->mMockAudioTrack->enabled = false;
	mStreamAbstractionAAMP->mMockVideoTrack->enabled = true;

	EXPECT_CALL(*mStreamAbstractionAAMP->mMockAudioTrack, GetBufferedDuration())
		.Times(0);
	EXPECT_CALL(*mStreamAbstractionAAMP->mMockVideoTrack, GetBufferedDuration())
		.WillOnce(Return(8.75));

	EXPECT_DOUBLE_EQ(8.75, mStreamAbstractionAAMP->GetBufferedAudioDurationSec());
}

/**
 * @brief Verify GetBufferedAudioDurationSec() does not fall back to video
 *        when AudioOnlyPlayback is enabled.
 */
TEST_F(StreamAbstractionAAMP_Test, GetBufferedAudioDurationSec_ReturnsSentinel_WhenAudioOnlyPlaybackEnabled)
{
	mPrivateInstanceAAMP->rate = AAMP_NORMAL_PLAY_RATE;
	mStreamAbstractionAAMP->mMockAudioTrack->enabled = false;
	mStreamAbstractionAAMP->mMockVideoTrack->enabled = true;

	EXPECT_CALL(*g_mockAampConfig, IsConfigSet(eAAMPConfig_AudioOnlyPlayback))
		.WillRepeatedly(Return(true));
	EXPECT_CALL(*mStreamAbstractionAAMP->mMockAudioTrack, GetBufferedDuration())
		.Times(0);
	EXPECT_CALL(*mStreamAbstractionAAMP->mMockVideoTrack, GetBufferedDuration())
		.Times(0);

	EXPECT_DOUBLE_EQ(-1.0, mStreamAbstractionAAMP->GetBufferedAudioDurationSec());
}

/**
 * @brief Verify GetBufferValue() returns 0 when the track pointer is null.
 */
TEST_F(StreamAbstractionAAMP_Test, GetBufferValue_ReturnsZero_WhenTrackIsNull)
{
	EXPECT_DOUBLE_EQ(0.0, mStreamAbstractionAAMP->testGetBufferValue(nullptr));
}

/**
 * @brief Verify GetBufferValue() returns GetBufferedDuration() for
 *        standard (non-local-TSB) playback.
 */
TEST_F(StreamAbstractionAAMP_Test, GetBufferValue_ReturnsBufferedDuration_WhenLocalTSBDisabled)
{
	mPrivateInstanceAAMP->SetLocalAAMPTsb(false);

	EXPECT_CALL(*mStreamAbstractionAAMP->mMockVideoTrack, GetBufferedDuration())
		.WillOnce(Return(8.0));

	EXPECT_DOUBLE_EQ(8.0, mStreamAbstractionAAMP->testGetBufferValue(
		mStreamAbstractionAAMP->mMockVideoTrack));
}

/**
 * @brief Verify GetBufferValue() returns lastDownloadedPosition minus
 *        GetLivePlayPosition() during active local TSB injection.
 */
TEST_F(StreamAbstractionAAMP_Test, GetBufferValue_ReturnsLiveEdgeDelta_WhenLocalTSBInjectionActive)
{
	mPrivateInstanceAAMP->SetLocalAAMPTsb(true);
	mPrivateInstanceAAMP->mSinkPaused = false;
	mStreamAbstractionAAMP->mMockVideoTrack->MediaTrack::SetLocalTSBInjection(true);

	EXPECT_CALL(*mStreamAbstractionAAMP->mMockVideoTrack, GetBufferedDuration())
		.Times(AnyNumber())
		.WillRepeatedly(Return(0.0));
	EXPECT_CALL(*mStreamAbstractionAAMP->mMockVideoTrack, GetLastDownloadedPosition())
		.WillOnce(Return(120.0));
	EXPECT_CALL(*g_mockPrivateInstanceAAMP, GetLivePlayPosition())
		.WillOnce(Return(114.0));

	EXPECT_DOUBLE_EQ(6.0, mStreamAbstractionAAMP->testGetBufferValue(
		mStreamAbstractionAAMP->mMockVideoTrack));
}

/**
 * @brief Verify GetBufferValue() returns lastDownloadedPosition minus
 *        GetLivePlayPosition() when local TSB is enabled and the pipeline is
 *        paused (injection not yet active).
 */
TEST_F(StreamAbstractionAAMP_Test, GetBufferValue_ReturnsLiveEdgeDelta_WhenPipelinePaused)
{
	mPrivateInstanceAAMP->SetLocalAAMPTsb(true);
	mPrivateInstanceAAMP->mSinkPaused = true;
	// mIsLocalTSBInjection defaults to false — injection not yet active

	EXPECT_CALL(*mStreamAbstractionAAMP->mMockVideoTrack, GetBufferedDuration())
		.Times(AnyNumber())
		.WillRepeatedly(Return(0.0));
	EXPECT_CALL(*mStreamAbstractionAAMP->mMockVideoTrack, GetLastDownloadedPosition())
		.WillOnce(Return(120.0));
	EXPECT_CALL(*g_mockPrivateInstanceAAMP, GetLivePlayPosition())
		.WillOnce(Return(114.0));

	EXPECT_DOUBLE_EQ(6.0, mStreamAbstractionAAMP->testGetBufferValue(
		mStreamAbstractionAAMP->mMockVideoTrack));
}

/**
 * @brief Verify GetBufferValue() falls back to GetBufferedDuration() when
 *        playing at the live edge and not paused (TSB enabled but injection
 *        not yet active).
 */
TEST_F(StreamAbstractionAAMP_Test, GetBufferValue_ReturnsBufferedDuration_WhenAtLiveEdgeAndNotPaused)
{
	mPrivateInstanceAAMP->SetLocalAAMPTsb(true);
	mPrivateInstanceAAMP->mSinkPaused = false;
	// mIsLocalTSBInjection defaults to false — injection not yet active

	EXPECT_CALL(*mStreamAbstractionAAMP->mMockVideoTrack, GetBufferedDuration())
		.WillOnce(Return(4.5));

	EXPECT_DOUBLE_EQ(4.5, mStreamAbstractionAAMP->testGetBufferValue(
		mStreamAbstractionAAMP->mMockVideoTrack));
}

/**
 * @brief Verify GetBufferValue() clamps to 0 when the live downloader has
 *        fallen behind the live play position (negative delta).
 */
TEST_F(StreamAbstractionAAMP_Test, GetBufferValue_ClampsToZero_WhenLiveEdgeDeltaNegative)
{
	mPrivateInstanceAAMP->SetLocalAAMPTsb(true);
	mPrivateInstanceAAMP->mSinkPaused = false;
	mStreamAbstractionAAMP->mMockVideoTrack->MediaTrack::SetLocalTSBInjection(true);

	EXPECT_CALL(*mStreamAbstractionAAMP->mMockVideoTrack, GetBufferedDuration())
		.Times(AnyNumber())
		.WillRepeatedly(Return(0.0));
	EXPECT_CALL(*mStreamAbstractionAAMP->mMockVideoTrack, GetLastDownloadedPosition())
		.WillOnce(Return(110.0));
	EXPECT_CALL(*g_mockPrivateInstanceAAMP, GetLivePlayPosition())
		.WillOnce(Return(114.0));

	EXPECT_DOUBLE_EQ(0.0, mStreamAbstractionAAMP->testGetBufferValue(
		mStreamAbstractionAAMP->mMockVideoTrack));
}