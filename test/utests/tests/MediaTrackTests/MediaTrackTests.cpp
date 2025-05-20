/*
 * If not stated otherwise in this file or this component's license file the
 * following copyright and licenses apply:
 *
 * Copyright 2024 RDK Management
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
#include <algorithm>

#include "AampUtils.h"
#include "AampConfig.h"
#include "AampLogManager.h"
#include "AampTime.h"
#include "priv_aamp.h"
#include "fragmentcollector_mpd.h"
#include "AampGrowableBuffer.h"

#include "MockIsoBmffHelper.h"
#include "MockIsoBmffBuffer.h"
#include "MockAampConfig.h"
#include "MockPrivateInstanceAAMP.h"

using namespace testing;

static constexpr const char* FRAGMENT_TEST_DATA{"Fragment test data"};
static constexpr float FASTEST_TRICKPLAY_RATE{AAMP_RATE_TRICKPLAY_MAX};
static constexpr float SLOWEST_TRICKPLAY_RATE{2};
static constexpr int TRICKMODE_FPS{4};
static constexpr uint32_t TRICKMODE_TIMESCALE{100000};
static constexpr AampTime FRAGMENT_DURATION{1.92};
static constexpr AampTime FRAGMENT_DURATION_BEFORE_AD_BREAK{1.11};
static constexpr AampTime FIRST_PTS{1000};
static constexpr bool LLD_ENABLED{true};
static constexpr bool LLD_DISABLED{false};
static constexpr uint32_t PLAYBACK_TIMESCALE{90000};
static constexpr double PTS_OFFSET_SEC{123.4};

AampConfig* gpGlobalConfig{nullptr};

// The matcher is passed a std::cref() to avoid copy-constructing the fake AampGrowableBuffer, which
// crashes and is not really desirable anyway. (Copy-construction of the argument is default matcher
// behavior, done in case it's modified or destructed later.)
MATCHER_P(AampGrowableBufferRefEq, bufferStdConstRef, "")
{
	const AampGrowableBuffer& buffer = bufferStdConstRef.get();
	return std::memcmp(arg.GetPtr(), buffer.GetPtr(), buffer.GetLen()) == 0;
}

MATCHER_P(AampGrowableBufferPtrEq, bufferPtr, "")
{
	return std::memcmp(arg->GetPtr(), bufferPtr->GetPtr(), bufferPtr->GetLen()) == 0;
}

// MediaTrack is an abstract base class, so must be tested via a derived class
class TestableMediaTrack : public MediaTrack
{
public:
	TestableMediaTrack(TrackType type, PrivateInstanceAAMP* aamp,
					   const char* name, StreamAbstractionAAMP* context)
		: MediaTrack(type, aamp, name), mContext(context)
	{
	}

	// Provide overrides for pure virtuals - this is just to keep the compiler happy
	void ProcessPlaylist(AampGrowableBuffer&, int) override {};
	std::string& GetPlaylistUrl() override { return mFakeStr; };
	std::string& GetEffectivePlaylistUrl() override { return mFakeStr; };
	void SetEffectivePlaylistUrl(std::string) override {};
	long long GetLastPlaylistDownloadTime() override { return 0; };
	long GetMinUpdateDuration() override { return 0; };
	int GetDefaultDurationBetweenPlaylistUpdates() override { return 0; };
	void SetLastPlaylistDownloadTime(long long) override {};
	void ABRProfileChanged() override {};
	void updateSkipPoint(double, double) override {};
	void setDiscontinuityState(bool) override {};
	void abortWaitForVideoPTS() override {};
	double GetBufferedDuration() override { return 0; };

protected:
	// Must return something non-null to avoid a crash
	StreamAbstractionAAMP* GetContext() override { return mContext; };
	void InjectFragmentInternal(CachedFragment*, bool&, bool) override {};

private:
	std::string mFakeStr;
	StreamAbstractionAAMP* mContext;
};

class MediaTrackTests : public testing::Test
{
protected:
	PrivateInstanceAAMP* mPrivateInstanceAAMP{nullptr};
	StreamAbstractionAAMP_MPD* mStreamAbstractionAAMP_MPD{nullptr};

	void SetUp() override
	{
		gpGlobalConfig = new AampConfig();
		g_mockAampConfig = new NiceMock<MockAampConfig>();

		// A fake PrivateInstanceAAMP
		mPrivateInstanceAAMP = new PrivateInstanceAAMP(gpGlobalConfig);

		g_mockPrivateInstanceAAMP = new NiceMock<MockPrivateInstanceAAMP>();
		g_mockIsoBmffHelper = new NiceMock<MockIsoBmffHelper>();
		g_mockIsoBmffBuffer = new NiceMock<MockIsoBmffBuffer>();

		// A fake StreamAbstractionAAMP_MPD that derives from a *real* StreamAbstractionAAMP.
		// The tests can't use a fake/mock StreamAbstractionAAMP base class because
		// StreamAbstractionAAMP and MediaTrack share the same source file and fakes file.
		mStreamAbstractionAAMP_MPD =
			new StreamAbstractionAAMP_MPD(mPrivateInstanceAAMP, 0, 0);
	}

	void TearDown() override
	{
		delete mStreamAbstractionAAMP_MPD;
		mStreamAbstractionAAMP_MPD = nullptr;

		delete g_mockIsoBmffHelper;
		g_mockIsoBmffHelper = nullptr;

		delete g_mockIsoBmffBuffer;
		g_mockIsoBmffBuffer = nullptr;

		delete g_mockPrivateInstanceAAMP;
		g_mockPrivateInstanceAAMP = nullptr;

		delete mPrivateInstanceAAMP;
		mPrivateInstanceAAMP = nullptr;

		delete g_mockAampConfig;
		g_mockAampConfig = nullptr;

		delete gpGlobalConfig;
		gpGlobalConfig = nullptr;
	}

	CachedFragment* AddFragmentToBuffer(MediaTrack& mediaTrack, CachedFragment& testFragment,
										bool lowLatencyMode)
	{
		// A pointer to the test fragment in the cache buffer
		CachedFragment* bufferedFragment{nullptr};

		if (lowLatencyMode)
		{
			bufferedFragment = mediaTrack.GetFetchChunkBuffer(true);
			bufferedFragment->Copy(&testFragment, testFragment.fragment.GetLen());
			mediaTrack.numberOfFragmentChunksCached = 1;

			if (!bufferedFragment->initFragment)
			{
				// Make the buffer parser return the correct position and duration
				EXPECT_CALL(*g_mockIsoBmffBuffer, ParseChunkData(_, _, _, _, _, _, _))
					.WillOnce(DoAll(SetArgReferee<5>(bufferedFragment->position),
									SetArgReferee<6>(bufferedFragment->duration), Return(true)));
			}
		}
		else // Standard latency mode
		{
			bufferedFragment = mediaTrack.GetFetchBuffer(true);
			bufferedFragment->Copy(&testFragment, testFragment.fragment.GetLen());
			mediaTrack.numberOfFragmentsCached = 1;
		}

		return bufferedFragment;
	}

	void SetLowLatencyMode(bool isEnabled)
	{
		AampLLDashServiceData dashData{};
		dashData.lowLatencyMode = isEnabled;
		mPrivateInstanceAAMP->SetLLDashServiceData(dashData);
		// In these tests, chunk mode is set for all low-latency tests
		mPrivateInstanceAAMP->SetLLDashChunkMode(isEnabled);
	}
};

struct PlayRateTestData
{
	bool lowLatencyMode;
	float playRate;
};

class MediaTrackDashPtsRestampNotConfiguredTests
	: public MediaTrackTests,
	  public testing::WithParamInterface<PlayRateTestData>
{
};

TEST_P(MediaTrackDashPtsRestampNotConfiguredTests, PtsRestampNotConfiguredTest)
{
	CachedFragment* bufferedFragment{nullptr};
	CachedFragment testFragment;
	testFragment.fragment.AppendBytes(FRAGMENT_TEST_DATA, strlen(FRAGMENT_TEST_DATA));
	PlayRateTestData testParam = GetParam(); // Test parameter injected here
	SetLowLatencyMode(testParam.lowLatencyMode);
	mPrivateInstanceAAMP->rate = testParam.playRate;
	mStreamAbstractionAAMP_MPD->trickplayMode = true;

	EXPECT_CALL(*g_mockAampConfig, IsConfigSet(eAAMPConfig_EnablePTSReStamp))
		.WillRepeatedly(Return(false));
	EXPECT_CALL(*g_mockAampConfig, GetConfigValue(eAAMPConfig_MaxFragmentCached))
		.WillRepeatedly(Return(1));
	EXPECT_CALL(*g_mockAampConfig, GetConfigValue(eAAMPConfig_MaxFragmentChunkCached))
		.WillRepeatedly(Return(1));
	EXPECT_CALL(*g_mockIsoBmffBuffer, parseBuffer(_, _)).WillRepeatedly(Return(true));

	TestableMediaTrack mediaTrack{eTRACK_VIDEO, mPrivateInstanceAAMP, "media",
								  mStreamAbstractionAAMP_MPD};

	// Init segment
	testFragment.initFragment = true;
	bufferedFragment = AddFragmentToBuffer(mediaTrack, testFragment, testParam.lowLatencyMode);
	EXPECT_CALL(*g_mockIsoBmffHelper, SetTimescale(_, _)).Times(0);

	ASSERT_TRUE(mediaTrack.InjectFragment());

	// Media segment
	testFragment.initFragment = false;
	bufferedFragment = AddFragmentToBuffer(mediaTrack, testFragment, testParam.lowLatencyMode);
	EXPECT_CALL(*g_mockIsoBmffHelper, RestampPts(_, _, _, _, _)).Times(0);
	EXPECT_CALL(*g_mockIsoBmffHelper, SetPtsAndDuration(_, _, _)).Times(0);

	ASSERT_TRUE(mediaTrack.InjectFragment());
}

PlayRateTestData ptsRestampNotConfiguredPlayRateTestData[] = {
	{LLD_DISABLED, AAMP_NORMAL_PLAY_RATE},
	{LLD_DISABLED, SLOWEST_TRICKPLAY_RATE},
	{LLD_ENABLED, AAMP_NORMAL_PLAY_RATE},
	{LLD_ENABLED, SLOWEST_TRICKPLAY_RATE},
};

INSTANTIATE_TEST_SUITE_P(MediaTrackTests, MediaTrackDashPtsRestampNotConfiguredTests,
						 ::testing::ValuesIn(ptsRestampNotConfiguredPlayRateTestData));

class MediaTrackDashQtDemuxOverrideConfiguredTests
	: public MediaTrackTests,
	  public testing::WithParamInterface<PlayRateTestData>
{
};

TEST_P(MediaTrackDashQtDemuxOverrideConfiguredTests, QtDemuxOverrideConfiguredTest)
{
	CachedFragment* bufferedFragment{nullptr};
	CachedFragment testFragment;
	testFragment.fragment.AppendBytes(FRAGMENT_TEST_DATA, strlen(FRAGMENT_TEST_DATA));
	PlayRateTestData testParam = GetParam(); // Test parameter injected here
	SetLowLatencyMode(testParam.lowLatencyMode);
	mPrivateInstanceAAMP->rate = testParam.playRate;
	mStreamAbstractionAAMP_MPD->trickplayMode = true;

	EXPECT_CALL(*g_mockAampConfig, IsConfigSet(eAAMPConfig_EnablePTSReStamp))
		.WillRepeatedly(Return(false));
	EXPECT_CALL(*g_mockAampConfig, GetConfigValue(eAAMPConfig_MaxFragmentCached))
		.WillRepeatedly(Return(1));
	EXPECT_CALL(*g_mockAampConfig, GetConfigValue(eAAMPConfig_MaxFragmentChunkCached))
		.WillRepeatedly(Return(1));
	EXPECT_CALL(*g_mockIsoBmffBuffer, parseBuffer(_, _)).WillRepeatedly(Return(true));

	TestableMediaTrack mediaTrack{eTRACK_VIDEO, mPrivateInstanceAAMP, "media",
								  mStreamAbstractionAAMP_MPD};

	// Init segment
	testFragment.initFragment = true;
	bufferedFragment = AddFragmentToBuffer(mediaTrack, testFragment, testParam.lowLatencyMode);
	EXPECT_CALL(*g_mockIsoBmffHelper, SetTimescale(_, _)).Times(0);

	ASSERT_TRUE(mediaTrack.InjectFragment());

	// Media segment
	testFragment.initFragment = false;
	bufferedFragment = AddFragmentToBuffer(mediaTrack, testFragment, testParam.lowLatencyMode);
	EXPECT_CALL(*g_mockIsoBmffHelper, RestampPts(_, _, _, _, _)).Times(0);
	EXPECT_CALL(*g_mockIsoBmffHelper, SetPtsAndDuration(_, _, _)).Times(0);

	ASSERT_TRUE(mediaTrack.InjectFragment());
}

PlayRateTestData qtDemuxOverrideConfiguredPlayRateTestData[] = {
	{LLD_DISABLED, AAMP_NORMAL_PLAY_RATE},
	{LLD_DISABLED, SLOWEST_TRICKPLAY_RATE},
	{LLD_ENABLED, AAMP_NORMAL_PLAY_RATE},
	{LLD_ENABLED, SLOWEST_TRICKPLAY_RATE},
};

INSTANTIATE_TEST_SUITE_P(MediaTrackTests, MediaTrackDashQtDemuxOverrideConfiguredTests,
						 ::testing::ValuesIn(qtDemuxOverrideConfiguredPlayRateTestData));

class MediaTrackDashTrickModePtsRestampValidPlayRateTests
	: public MediaTrackTests,
	  public testing::WithParamInterface<PlayRateTestData>
{
};

TEST_P(MediaTrackDashTrickModePtsRestampValidPlayRateTests, ValidPlayRateTest)
{
	AampTime restampedPts{0}; // Restamped PTS is an offset from the start of trickplay
	CachedFragment* bufferedFragment{nullptr};
	PlayRateTestData testParam = GetParam(); // Test parameter injected here
	mPrivateInstanceAAMP->rate = testParam.playRate;
	mPrivateInstanceAAMP->mMediaFormat = eMEDIAFORMAT_DASH;
	SetLowLatencyMode(testParam.lowLatencyMode);
	mStreamAbstractionAAMP_MPD->trickplayMode = true;

	// There should be no PTS restamping for normal play rate media fragments in this test
	EXPECT_CALL(*g_mockIsoBmffHelper, RestampPts(_, _, _, _, _)).Times(0);

	EXPECT_CALL(*g_mockAampConfig, IsConfigSet(eAAMPConfig_EnablePTSReStamp))
		.WillRepeatedly(Return(true));
	EXPECT_CALL(*g_mockAampConfig, GetConfigValue(eAAMPConfig_VODTrickPlayFPS))
		.WillRepeatedly(Return(TRICKMODE_FPS));
	EXPECT_CALL(*g_mockAampConfig, GetConfigValue(eAAMPConfig_MaxFragmentCached))
		.WillRepeatedly(Return(1));
	EXPECT_CALL(*g_mockAampConfig, GetConfigValue(eAAMPConfig_MaxFragmentChunkCached))
		.WillRepeatedly(Return(1));
	EXPECT_CALL(*g_mockIsoBmffBuffer, parseBuffer(_, _)).WillRepeatedly(Return(true));

	TestableMediaTrack iframeTrack{eTRACK_VIDEO, mPrivateInstanceAAMP, "iframe",
								   mStreamAbstractionAAMP_MPD};

	// Init segment
	CachedFragment testFragment;
	testFragment.initFragment = true;
	testFragment.fragment.AppendBytes(FRAGMENT_TEST_DATA, strlen(FRAGMENT_TEST_DATA));
	bufferedFragment = AddFragmentToBuffer(iframeTrack, testFragment, testParam.lowLatencyMode);

	EXPECT_CALL(*g_mockIsoBmffHelper,
				SetTimescale(AampGrowableBufferRefEq(std::cref(testFragment.fragment)),
									TRICKMODE_TIMESCALE))
		.WillOnce(Return(true));

	ASSERT_TRUE(iframeTrack.InjectFragment());
	ASSERT_EQ(bufferedFragment->position, restampedPts);

	// First media segment
	testFragment = CachedFragment{};
	testFragment.initFragment = false;
	testFragment.duration = FRAGMENT_DURATION.inSeconds();
	testFragment.position = FIRST_PTS.inSeconds();
	testFragment.absPosition = FIRST_PTS.inSeconds();
	testFragment.fragment.AppendBytes(FRAGMENT_TEST_DATA, strlen(FRAGMENT_TEST_DATA));
	AampTime lastPosition{testFragment.position};
	bufferedFragment = AddFragmentToBuffer(iframeTrack, testFragment, testParam.lowLatencyMode);

	// This is an estimate - don't know how long the duration should be, as there isn't a previous
	// PTS to calculate a delta.  Better to avoid too small a number, so limited to 0.25 seconds.
	// GStreamer works ok with this in practice.
	AampTime restampedDuration{std::max(
		testFragment.duration / std::fabs(mPrivateInstanceAAMP->rate), 1.0 / TRICKMODE_FPS)};
	int64_t expectedDuration{restampedDuration * TRICKMODE_TIMESCALE};
	int64_t expectedPts{restampedPts * TRICKMODE_TIMESCALE};
	EXPECT_CALL(
		*g_mockIsoBmffHelper,
		SetPtsAndDuration(AampGrowableBufferRefEq(std::cref(testFragment.fragment)),
								 expectedPts, expectedDuration))
		.WillOnce(Return(true));

	if (testParam.lowLatencyMode)
	{
		// Check that the PTS that is (eventually) passed on to GStreamer is as expected
		EXPECT_CALL(*g_mockPrivateInstanceAAMP,
					SendStreamTransfer(eMEDIATYPE_VIDEO,
									   AampGrowableBufferPtrEq(&(testFragment.fragment)),
									   restampedPts.inSeconds(), restampedPts.inSeconds(),
									   restampedDuration.inSeconds(), _, _, _));
	}
	ASSERT_TRUE(iframeTrack.InjectFragment());
	if (!testParam.lowLatencyMode)
	{
		// Check that the PTS that is (eventually) passed on to GStreamer is as expected
		ASSERT_EQ(bufferedFragment->duration, restampedDuration.inSeconds());
		ASSERT_EQ(bufferedFragment->position, restampedPts.inSeconds());
	}

	// Verify the next two steady-state media segments
	for (int i = 1; i <= 2; i++)
	{
		// Inject an init segment as if there was an ABR change in the "recorded" content. This should not reset the restamp PTS.
		testFragment = CachedFragment{};
		testFragment.initFragment = true;
		testFragment.fragment.AppendBytes(FRAGMENT_TEST_DATA, strlen(FRAGMENT_TEST_DATA));
		bufferedFragment = AddFragmentToBuffer(iframeTrack, testFragment, testParam.lowLatencyMode);
		EXPECT_CALL(*g_mockIsoBmffHelper,
					SetTimescale(AampGrowableBufferRefEq(std::cref(testFragment.fragment)),
								 TRICKMODE_TIMESCALE)
					).WillOnce(Return(true));
		if (testParam.lowLatencyMode)
		{	// PTS / DTS is not relevant for init segment, so ignore the values
			EXPECT_CALL(*g_mockPrivateInstanceAAMP,
						SendStreamTransfer(eMEDIATYPE_VIDEO,
										   AampGrowableBufferPtrEq(&(testFragment.fragment)),
										   _,  _, _,
										   _, _, _));
		}
		ASSERT_TRUE(iframeTrack.InjectFragment());

		testFragment = CachedFragment{};
		testFragment.initFragment = false;
		testFragment.duration = FRAGMENT_DURATION.inSeconds();
		AampTime nextPts{FIRST_PTS + (FRAGMENT_DURATION * i)};
		testFragment.position = nextPts.inSeconds();
		testFragment.absPosition = nextPts.inSeconds();
		testFragment.fragment.AppendBytes(FRAGMENT_TEST_DATA, strlen(FRAGMENT_TEST_DATA));
		AampTime positionDelta{fabs(testFragment.position - lastPosition)};
		lastPosition = testFragment.position;
		bufferedFragment = AddFragmentToBuffer(iframeTrack, testFragment, testParam.lowLatencyMode);

		restampedDuration = positionDelta / std::fabs(mPrivateInstanceAAMP->rate);
		restampedPts += restampedDuration;
		expectedDuration = static_cast<int64_t>(restampedDuration * TRICKMODE_TIMESCALE);
		expectedPts = static_cast<int64_t>(restampedPts * TRICKMODE_TIMESCALE);
		EXPECT_CALL(
			*g_mockIsoBmffHelper,
			SetPtsAndDuration(AampGrowableBufferRefEq(std::cref(testFragment.fragment)),
									 expectedPts, expectedDuration))
			.WillOnce(Return(true));

		if (testParam.lowLatencyMode)
		{
			// Check that the PTS that is (eventually) passed on to GStreamer is as expected
			EXPECT_CALL(*g_mockPrivateInstanceAAMP,
						SendStreamTransfer(eMEDIATYPE_VIDEO,
										   AampGrowableBufferPtrEq(&(testFragment.fragment)),
										   restampedPts.inSeconds(), restampedPts.inSeconds(),
										   restampedDuration.inSeconds(), _, _, _));
		}
		ASSERT_TRUE(iframeTrack.InjectFragment());
		if (!testParam.lowLatencyMode)
		{
			// Check that the PTS that is (eventually) passed on to GStreamer is as expected
			ASSERT_EQ(bufferedFragment->duration, restampedDuration.inSeconds());
			ASSERT_EQ(bufferedFragment->position, restampedPts.inSeconds());
		}
	}
}

PlayRateTestData validPlayRateTestData[] = {
	{LLD_DISABLED, FASTEST_TRICKPLAY_RATE},	 {LLD_DISABLED, SLOWEST_TRICKPLAY_RATE},
	{LLD_DISABLED, -SLOWEST_TRICKPLAY_RATE}, {LLD_DISABLED, -FASTEST_TRICKPLAY_RATE},
	{LLD_ENABLED, FASTEST_TRICKPLAY_RATE},	 {LLD_ENABLED, SLOWEST_TRICKPLAY_RATE},
	{LLD_ENABLED, -SLOWEST_TRICKPLAY_RATE},	 {LLD_ENABLED, -FASTEST_TRICKPLAY_RATE},
};

INSTANTIATE_TEST_SUITE_P(MediaTrackTests, MediaTrackDashTrickModePtsRestampValidPlayRateTests,
						 ::testing::ValuesIn(validPlayRateTestData));

class MediaTrackDashPlaybackPtsRestampTests : public MediaTrackTests,
											  public testing::WithParamInterface<bool>
{
};

TEST_P(MediaTrackDashPlaybackPtsRestampTests, PlaybackTest)
{
	std::string expectedUri{"Dummy URI"};
	CachedFragment* bufferedFragment{nullptr};
	CachedFragment testFragment;
	testFragment.fragment.AppendBytes(FRAGMENT_TEST_DATA, strlen(FRAGMENT_TEST_DATA));
	testFragment.position = FIRST_PTS.inSeconds();
	testFragment.PTSOffsetSec = PTS_OFFSET_SEC;
	testFragment.timeScale = PLAYBACK_TIMESCALE;
	testFragment.uri = expectedUri;
	bool lowLatencyMode = GetParam(); // Test parameter injected here
	SetLowLatencyMode(lowLatencyMode);
	mPrivateInstanceAAMP->rate = AAMP_NORMAL_PLAY_RATE;
	mPrivateInstanceAAMP->mMediaFormat = eMEDIAFORMAT_DASH;
	mStreamAbstractionAAMP_MPD->trickplayMode = false;

	EXPECT_CALL(*g_mockAampConfig, IsConfigSet(eAAMPConfig_EnablePTSReStamp))
		.WillRepeatedly(Return(true));
	EXPECT_CALL(*g_mockAampConfig, GetConfigValue(eAAMPConfig_MaxFragmentCached))
		.WillRepeatedly(Return(1));
	EXPECT_CALL(*g_mockAampConfig, GetConfigValue(eAAMPConfig_MaxFragmentChunkCached))
		.WillRepeatedly(Return(1));
	EXPECT_CALL(*g_mockIsoBmffBuffer, parseBuffer(_, _)).WillRepeatedly(Return(true));

	TestableMediaTrack videoTrack{eTRACK_VIDEO, mPrivateInstanceAAMP, "video",
								  mStreamAbstractionAAMP_MPD};

	// Init segment
	testFragment.initFragment = true;
	bufferedFragment = AddFragmentToBuffer(videoTrack, testFragment, lowLatencyMode);
	EXPECT_CALL(*g_mockIsoBmffHelper, RestampPts(_, _, _, _, _)).Times(0);
	EXPECT_CALL(*g_mockIsoBmffHelper, SetTimescale(_, _)).Times(0);

	ASSERT_TRUE(videoTrack.InjectFragment());

	// Media segment
	testFragment.initFragment = false;
	bufferedFragment = AddFragmentToBuffer(videoTrack, testFragment, lowLatencyMode);
	EXPECT_CALL(*g_mockIsoBmffHelper,
				RestampPts(AampGrowableBufferRefEq(std::cref(testFragment.fragment)),
								  (PTS_OFFSET_SEC * PLAYBACK_TIMESCALE), expectedUri, "video", PLAYBACK_TIMESCALE));
	EXPECT_CALL(*g_mockIsoBmffHelper, SetPtsAndDuration(_, _, _)).Times(0);
	if (lowLatencyMode)
	{
		// Check that the PTS that is (eventually) passed on to GStreamer is as expected
		double expectedPts = FIRST_PTS.inSeconds() + PTS_OFFSET_SEC;
		EXPECT_CALL(*g_mockPrivateInstanceAAMP,
					SendStreamTransfer(eMEDIATYPE_VIDEO,
									   AampGrowableBufferPtrEq(&(testFragment.fragment)),
									   expectedPts, expectedPts, _, _, _, _));
	}

	ASSERT_TRUE(videoTrack.InjectFragment());
}

INSTANTIATE_TEST_SUITE_P(MediaTrackTests, MediaTrackDashPlaybackPtsRestampTests,
						 ::testing::Values(LLD_DISABLED, LLD_ENABLED));

class MediaTrackDashTrickModePtsRestampInvalidPlayRateTests
	: public MediaTrackTests,
	  public testing::WithParamInterface<float>
{
};

TEST_P(MediaTrackDashTrickModePtsRestampInvalidPlayRateTests, InvalidPlayRateTest)
{
	CachedFragment* bufferedFragment{nullptr};
	CachedFragment testFragment;
	testFragment.fragment.AppendBytes(FRAGMENT_TEST_DATA, strlen(FRAGMENT_TEST_DATA));
	mPrivateInstanceAAMP->rate = GetParam(); // Test parameter injected here
	mStreamAbstractionAAMP_MPD->trickplayMode = true;

	// There should be no PTS restamping for normal play rate media fragments in this test
	EXPECT_CALL(*g_mockIsoBmffHelper, RestampPts(_, _, _, _, _)).Times(0);

	EXPECT_CALL(*g_mockAampConfig, IsConfigSet(eAAMPConfig_EnablePTSReStamp))
		.WillRepeatedly(Return(true));
	EXPECT_CALL(*g_mockAampConfig, GetConfigValue(eAAMPConfig_MaxFragmentCached))
		.WillRepeatedly(Return(1));
	EXPECT_CALL(*g_mockAampConfig, GetConfigValue(eAAMPConfig_MaxFragmentChunkCached))
		.WillRepeatedly(Return(1));
	EXPECT_CALL(*g_mockIsoBmffBuffer, parseBuffer(_, _)).WillRepeatedly(Return(true));

	TestableMediaTrack iframeTrack{eTRACK_VIDEO, mPrivateInstanceAAMP, "iframe",
								   mStreamAbstractionAAMP_MPD};

	// Init segment
	testFragment.initFragment = true;
	bufferedFragment = AddFragmentToBuffer(iframeTrack, testFragment, LLD_DISABLED);
	EXPECT_CALL(*g_mockIsoBmffHelper, SetTimescale(_, _)).Times(0);

	ASSERT_TRUE(iframeTrack.InjectFragment());

	// Media segment
	testFragment.initFragment = false;
	bufferedFragment = AddFragmentToBuffer(iframeTrack, testFragment, LLD_DISABLED);
	EXPECT_CALL(*g_mockIsoBmffHelper, SetPtsAndDuration(_, _, _)).Times(0);

	ASSERT_TRUE(iframeTrack.InjectFragment());
}

INSTANTIATE_TEST_SUITE_P(MediaTrackTests, MediaTrackDashTrickModePtsRestampInvalidPlayRateTests,
						 ::testing::Values(AAMP_RATE_PAUSE, AAMP_SLOWMOTION_RATE));

TEST_F(MediaTrackTests, DashTrickModePtsRestampDiscontinuityTest)
{
	CachedFragment* bufferedFragment{nullptr};
	AampTime restampedPts; // Restamped PTS is an offset from the start of trickplay
	mPrivateInstanceAAMP->rate = FASTEST_TRICKPLAY_RATE;
	mPrivateInstanceAAMP->mMediaFormat = eMEDIAFORMAT_DASH;
	mStreamAbstractionAAMP_MPD->trickplayMode = true;

	// There should be no PTS restamping for normal play rate media fragments in this test
	EXPECT_CALL(*g_mockIsoBmffHelper, RestampPts(_, _, _, _, _)).Times(0);

	EXPECT_CALL(*g_mockAampConfig, IsConfigSet(eAAMPConfig_EnablePTSReStamp))
		.WillRepeatedly(Return(true));
	EXPECT_CALL(*g_mockAampConfig, GetConfigValue(eAAMPConfig_VODTrickPlayFPS))
		.WillRepeatedly(Return(TRICKMODE_FPS));
	EXPECT_CALL(*g_mockAampConfig, GetConfigValue(eAAMPConfig_MaxFragmentCached))
		.WillRepeatedly(Return(1));
	EXPECT_CALL(*g_mockAampConfig, GetConfigValue(eAAMPConfig_MaxFragmentChunkCached))
		.WillRepeatedly(Return(1));

	TestableMediaTrack iframeTrack{eTRACK_VIDEO, mPrivateInstanceAAMP, "iframe",
								   mStreamAbstractionAAMP_MPD};

	// Init segment
	CachedFragment testFragment;
	testFragment.initFragment = true;
	testFragment.fragment.AppendBytes(FRAGMENT_TEST_DATA, strlen(FRAGMENT_TEST_DATA));
	bufferedFragment = AddFragmentToBuffer(iframeTrack, testFragment, LLD_DISABLED);

	EXPECT_CALL(*g_mockIsoBmffHelper, SetTimescale(_, _)).WillOnce(Return(true));
	ASSERT_TRUE(iframeTrack.InjectFragment());

	// First media segment
	testFragment = CachedFragment{};
	testFragment.initFragment = false;
	testFragment.duration = FRAGMENT_DURATION.inSeconds();
	testFragment.position = FIRST_PTS.inSeconds();
	testFragment.fragment.AppendBytes(FRAGMENT_TEST_DATA, strlen(FRAGMENT_TEST_DATA));
	bufferedFragment = AddFragmentToBuffer(iframeTrack, testFragment, LLD_DISABLED);

	EXPECT_CALL(*g_mockIsoBmffHelper, SetPtsAndDuration(_, _, _)).WillOnce(Return(true));
	ASSERT_TRUE(iframeTrack.InjectFragment());

	// Second media segment
	// (shorter duration, as might happen for the last segment before an ad break)
	testFragment = CachedFragment{};
	testFragment.initFragment = false;
	testFragment.duration = FRAGMENT_DURATION_BEFORE_AD_BREAK.inSeconds();
	AampTime nextPts{FIRST_PTS + FRAGMENT_DURATION_BEFORE_AD_BREAK};
	testFragment.position = nextPts.inSeconds();
	testFragment.fragment.AppendBytes(FRAGMENT_TEST_DATA, strlen(FRAGMENT_TEST_DATA));
	bufferedFragment = AddFragmentToBuffer(iframeTrack, testFragment, LLD_DISABLED);

	AampTime positionDelta{fabs(nextPts - FIRST_PTS)};
	AampTime restampedDuration{positionDelta / std::fabs(mPrivateInstanceAAMP->rate)};
	restampedPts += restampedDuration;

	EXPECT_CALL(*g_mockIsoBmffHelper, SetPtsAndDuration(_, _, _)).WillOnce(Return(true));
	ASSERT_TRUE(iframeTrack.InjectFragment());

	// New init segment for advert (transition from steady state to discontinuity)
	testFragment = CachedFragment{};
	testFragment.initFragment = true;
	// For trickplay, this flag appears to be used to signal a discontinuity - not the
	// isDiscontinuity flag passed to ProcessAndInjectFragment()
	testFragment.discontinuity = true;
	testFragment.fragment.AppendBytes(FRAGMENT_TEST_DATA, strlen(FRAGMENT_TEST_DATA));
	bufferedFragment = AddFragmentToBuffer(iframeTrack, testFragment, LLD_DISABLED);

	// Assume no change in restamped duration on discontinuity
	restampedPts += restampedDuration;
	EXPECT_CALL(*g_mockIsoBmffHelper,
				SetTimescale(AampGrowableBufferRefEq(std::cref(testFragment.fragment)),
									TRICKMODE_TIMESCALE))
		.WillOnce(Return(true));
	ASSERT_TRUE(iframeTrack.InjectFragment());
	ASSERT_DOUBLE_EQ(bufferedFragment->position, restampedPts.inSeconds());

	// First media segment for advert
	testFragment = CachedFragment{};
	testFragment.initFragment = false;
	testFragment.duration = FRAGMENT_DURATION.inSeconds();
	testFragment.position = FIRST_PTS.inSeconds();
	testFragment.fragment.AppendBytes(FRAGMENT_TEST_DATA, strlen(FRAGMENT_TEST_DATA));
	AampTime lastPosition{testFragment.position};
	bufferedFragment = AddFragmentToBuffer(iframeTrack, testFragment, LLD_DISABLED);

	int64_t expectedDuration{restampedDuration * TRICKMODE_TIMESCALE};
	int64_t expectedPts = (int64_t)(restampedPts.inSeconds() * TRICKMODE_TIMESCALE);

	EXPECT_CALL(
		*g_mockIsoBmffHelper,
		SetPtsAndDuration(AampGrowableBufferRefEq(std::cref(testFragment.fragment)),
								 expectedPts, expectedDuration))
		.WillOnce(Return(true));

	ASSERT_TRUE(iframeTrack.InjectFragment());
	ASSERT_DOUBLE_EQ(bufferedFragment->duration, restampedDuration.inSeconds());
	ASSERT_DOUBLE_EQ(bufferedFragment->position, restampedPts.inSeconds());

	// Second media segment for advert (transition from discontinuity back to steady state)
	testFragment = CachedFragment{};
	testFragment.initFragment = false;
	testFragment.duration = FRAGMENT_DURATION.inSeconds();
	nextPts = FIRST_PTS + FRAGMENT_DURATION;
	testFragment.position = nextPts.inSeconds();
	testFragment.fragment.AppendBytes(FRAGMENT_TEST_DATA, strlen(FRAGMENT_TEST_DATA));
	bufferedFragment = AddFragmentToBuffer(iframeTrack, testFragment, LLD_DISABLED);

	positionDelta = fabs(nextPts - FIRST_PTS);
	restampedDuration = positionDelta / std::fabs(mPrivateInstanceAAMP->rate);
	restampedPts += restampedDuration;
	expectedDuration = static_cast<int64_t>(restampedDuration * TRICKMODE_TIMESCALE);
	expectedPts = static_cast<int64_t>(restampedPts.inSeconds() * TRICKMODE_TIMESCALE);
	EXPECT_CALL(
		*g_mockIsoBmffHelper,
		SetPtsAndDuration(AampGrowableBufferRefEq(std::cref(testFragment.fragment)),
								 expectedPts, expectedDuration))
		.WillOnce(Return(true));

	ASSERT_TRUE(iframeTrack.InjectFragment());
	ASSERT_DOUBLE_EQ(bufferedFragment->duration, restampedDuration.inSeconds());
	ASSERT_DOUBLE_EQ(bufferedFragment->position, restampedPts.inSeconds());
}

TEST_F(MediaTrackTests, FlushFetchedFragmentsTest)
{
	CachedFragment* bufferedFragment1{nullptr};
	CachedFragment* bufferedFragment2{nullptr};
	CachedFragment* bufferedFragment3{nullptr};

	mPrivateInstanceAAMP->rate = FASTEST_TRICKPLAY_RATE;
	mPrivateInstanceAAMP->mMediaFormat = eMEDIAFORMAT_DASH;
	mStreamAbstractionAAMP_MPD->trickplayMode = true;

	EXPECT_CALL(*g_mockAampConfig, IsConfigSet(eAAMPConfig_EnablePTSReStamp))
		.WillRepeatedly(Return(true));
	EXPECT_CALL(*g_mockAampConfig, GetConfigValue(eAAMPConfig_MaxFragmentCached))
		.WillRepeatedly(Return(5));
	EXPECT_CALL(*g_mockAampConfig, GetConfigValue(eAAMPConfig_MaxFragmentChunkCached))
		.WillRepeatedly(Return(5));

	TestableMediaTrack videoTrack{eTRACK_VIDEO, mPrivateInstanceAAMP, "video", mStreamAbstractionAAMP_MPD};

	bufferedFragment1 = videoTrack.GetFetchBuffer(true);
	bufferedFragment1->initFragment = true;
	bufferedFragment1->fragment.AppendBytes(FRAGMENT_TEST_DATA, strlen(FRAGMENT_TEST_DATA));
	videoTrack.UpdateTSAfterFetch(bufferedFragment1->initFragment);

	// First media segment
	bufferedFragment2 = videoTrack.GetFetchBuffer(true);
	bufferedFragment2->initFragment = false;
	bufferedFragment2->duration = FRAGMENT_DURATION.inSeconds();
	bufferedFragment2->position = FIRST_PTS.inSeconds();
	bufferedFragment2->fragment.AppendBytes(FRAGMENT_TEST_DATA, strlen(FRAGMENT_TEST_DATA));
	videoTrack.UpdateTSAfterFetch(bufferedFragment2->initFragment);

	// Second media segment, not updated for injection
	bufferedFragment3 = videoTrack.GetFetchBuffer(true);
	bufferedFragment3->initFragment = false;
	bufferedFragment3->duration = FRAGMENT_DURATION.inSeconds();
	bufferedFragment3->position = 2 * FIRST_PTS.inSeconds();
	bufferedFragment3->fragment.AppendBytes(FRAGMENT_TEST_DATA, strlen(FRAGMENT_TEST_DATA));

	ASSERT_EQ(videoTrack.numberOfFragmentsCached, 2);
	ASSERT_EQ(bufferedFragment1->position, 0);
	ASSERT_EQ(bufferedFragment2->position, FIRST_PTS.inSeconds());
	ASSERT_EQ(bufferedFragment3->position, (2 * FIRST_PTS.inSeconds()));

	videoTrack.FlushFetchedFragments();

	// Check that the fragments added for injection have been removed
	// But the current fragment has not been cleared
	EXPECT_EQ(videoTrack.numberOfFragmentsCached, 0);
	EXPECT_EQ(bufferedFragment1->position, 0);
	EXPECT_EQ(bufferedFragment2->position, 0);
	EXPECT_EQ(bufferedFragment3->position, (2 * FIRST_PTS.inSeconds()));
}