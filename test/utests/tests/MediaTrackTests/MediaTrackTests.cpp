/*
 * If not stated otherwise in this file or this component's license file the
 * following copyright and licenses apply:
 *
 * Copyright 2024 Synamedia Ltd.
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
#include "MockAampConfig.h"

using namespace testing;

static constexpr const char* FRAGMENT_TEST_DATA{"Fragment test data"};
static constexpr float FASTEST_TRICKPLAY_RATE{AAMP_RATE_TRICKPLAY_MAX};
static constexpr float SLOWEST_TRICKPLAY_RATE{2};
static constexpr int TRICKMODE_FPS{4};
static constexpr uint32_t TRICKMODE_TIMESCALE{100000};
static constexpr AampTime FRAGMENT_DURATION{1.92};
static constexpr AampTime FRAGMENT_DURATION_BEFORE_AD_BREAK{1.11};
static constexpr AampTime FIRST_PTS{1000};

static constexpr uint32_t PLAYBACK_TIMESCALE{90000};
static constexpr double PTS_OFFSET_SEC{123.4};

AampConfig* gpGlobalConfig{nullptr};
AampLogManager* mLogObj{nullptr};

// The matcher is passed a std::cref() to avoid copy-constructing the fake AampGrowableBuffer, which
// crashes and is not really desirable anyway. (Copy-construction of the argument is default matcher
// behaviour, done in case it's modified or destructed later.)
MATCHER_P(AampGrowableBufferEq, bufferStdConstRef, "")
{
	const AampGrowableBuffer& buffer = bufferStdConstRef.get();
	return (&arg == &buffer) &&
		   (arg.GetPtr() == buffer.GetPtr()) &&
		   (arg.GetLen() == buffer.GetLen());
}

// MediaTrack is an abstract base class, so must be tested via a derived class
class TestableMediaTrack : public MediaTrack
{
public:
	TestableMediaTrack(AampLogManager* logObj, TrackType type, PrivateInstanceAAMP* aamp,
					   const char* name, StreamAbstractionAAMP* context)
		: MediaTrack(logObj, type, aamp, name), mContext(context)
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
		mLogObj = new AampLogManager();
		mLogObj->aampLoglevel = eLOGLEVEL_TRACE;		//To enable all levels of AAMP logging
		gpGlobalConfig = new AampConfig();
		g_mockAampConfig = new NiceMock<MockAampConfig>();

		// A fake PrivateInstanceAAMP
		mPrivateInstanceAAMP = new PrivateInstanceAAMP(gpGlobalConfig);

		g_mockIsoBmffHelper = new NiceMock<MockIsoBmffHelper>();

		// A fake StreamAbstractionAAMP_MPD that derives from a *real* StreamAbstractionAAMP.
		// The tests can't use a fake/mock StreamAbstractionAAMP base class because
		// StreamAbstractionAAMP and MediaTrack share the same source file and fakes file.
		mStreamAbstractionAAMP_MPD =
			new StreamAbstractionAAMP_MPD(nullptr, mPrivateInstanceAAMP, 0, 0);
	}

	void TearDown() override
	{
		delete mStreamAbstractionAAMP_MPD;
		mStreamAbstractionAAMP_MPD = nullptr;

		delete g_mockIsoBmffHelper;
		g_mockIsoBmffHelper = nullptr;

		delete mPrivateInstanceAAMP;
		mPrivateInstanceAAMP = nullptr;

		delete g_mockAampConfig;
		g_mockAampConfig = nullptr;

		delete gpGlobalConfig;
		gpGlobalConfig = nullptr;

		delete mLogObj;
		mLogObj = nullptr;
	}
};

TEST_F(MediaTrackTests, DashTrickModePtsRestampNotConfiguredTest)
{
	CachedFragment cachedFragment;
	cachedFragment.fragment.AppendBytes(FRAGMENT_TEST_DATA, strlen(FRAGMENT_TEST_DATA));
	bool fragmentDiscarded{false};
	bool isDiscontinuity{false};
	bool ret{true};
	mPrivateInstanceAAMP->rate = SLOWEST_TRICKPLAY_RATE;
	mStreamAbstractionAAMP_MPD->trickplayMode = true;

	TestableMediaTrack iframeTrack{mLogObj, eTRACK_VIDEO, mPrivateInstanceAAMP, "iframe",
								   mStreamAbstractionAAMP_MPD};

	EXPECT_CALL(*g_mockAampConfig, IsConfigSet(eAAMPConfig_EnablePTSReStamp))
		.WillRepeatedly(Return(false));
	EXPECT_CALL(*g_mockAampConfig, IsConfigSet(eAAMPConfig_QtDemuxOverride))
		.WillRepeatedly(Return(false));

	// Init segment
	cachedFragment.initFragment = true;
	EXPECT_CALL(*g_mockIsoBmffHelper, IsoBmffSetTimescale(_, _)).Times(0);

	iframeTrack.ProcessAndInjectFragment(&cachedFragment, fragmentDiscarded, isDiscontinuity, ret);
	ASSERT_TRUE(ret);

	// Media segment
	cachedFragment.initFragment = false;
	EXPECT_CALL(*g_mockIsoBmffHelper, IsoBmffSetPtsAndDuration(_, _, _)).Times(0);

	iframeTrack.ProcessAndInjectFragment(&cachedFragment, fragmentDiscarded, isDiscontinuity, ret);
	ASSERT_TRUE(ret);
}

TEST_F(MediaTrackTests, DashTrickModeQtDemuxOverrideConfiguredTest)
{
	CachedFragment cachedFragment;
	cachedFragment.fragment.AppendBytes(FRAGMENT_TEST_DATA, strlen(FRAGMENT_TEST_DATA));
	bool fragmentDiscarded{false};
	bool isDiscontinuity{false};
	bool ret{true};
	mPrivateInstanceAAMP->rate = SLOWEST_TRICKPLAY_RATE;
	mStreamAbstractionAAMP_MPD->trickplayMode = true;

	TestableMediaTrack iframeTrack{mLogObj, eTRACK_VIDEO, mPrivateInstanceAAMP, "iframe",
								   mStreamAbstractionAAMP_MPD};

	EXPECT_CALL(*g_mockAampConfig, IsConfigSet(eAAMPConfig_EnablePTSReStamp))
		.WillRepeatedly(Return(true));
	EXPECT_CALL(*g_mockAampConfig, IsConfigSet(eAAMPConfig_QtDemuxOverride))
		.WillRepeatedly(Return(true));

	// Init segment
	cachedFragment.initFragment = true;
	EXPECT_CALL(*g_mockIsoBmffHelper, IsoBmffSetTimescale(_, _)).Times(0);

	iframeTrack.ProcessAndInjectFragment(&cachedFragment, fragmentDiscarded, isDiscontinuity, ret);
	ASSERT_TRUE(ret);

	// Media segment
	cachedFragment.initFragment = false;
	EXPECT_CALL(*g_mockIsoBmffHelper, IsoBmffSetPtsAndDuration(_, _, _)).Times(0);

	iframeTrack.ProcessAndInjectFragment(&cachedFragment, fragmentDiscarded, isDiscontinuity, ret);
	ASSERT_TRUE(ret);
}

class MediaTrackDashTrickModePtsRestampValidPlayRateTests
	: public MediaTrackTests,
	  public testing::WithParamInterface<float>
{
};

TEST_P(MediaTrackDashTrickModePtsRestampValidPlayRateTests, ValidPlayRateTest)
{
	AampTime restampedPts{0}; // Restamped PTS is an offset from the start of trickplay
	bool fragmentDiscarded{false};
	bool isDiscontinuity{false};
	bool ret{true};
	mPrivateInstanceAAMP->rate = GetParam(); // Test parameter injected here
	mStreamAbstractionAAMP_MPD->trickplayMode = true;

	TestableMediaTrack iframeTrack{mLogObj, eTRACK_VIDEO, mPrivateInstanceAAMP, "iframe",
								   mStreamAbstractionAAMP_MPD};

	// There should be no PTS restamping for normal play rate media fragments in this test
	EXPECT_CALL(*g_mockIsoBmffHelper, IsoBmffRestampPts(_, _, _)).Times(0);

	EXPECT_CALL(*g_mockAampConfig, IsConfigSet(eAAMPConfig_EnablePTSReStamp))
		.WillRepeatedly(Return(true));
	EXPECT_CALL(*g_mockAampConfig, IsConfigSet(eAAMPConfig_QtDemuxOverride))
		.WillRepeatedly(Return(false));
	EXPECT_CALL(*g_mockAampConfig, GetConfigValue(eAAMPConfig_VODTrickPlayFPS))
		.WillRepeatedly(Return(TRICKMODE_FPS));

	// Init segment
	CachedFragment cachedFragment;
	cachedFragment.initFragment = true;
	cachedFragment.fragment.AppendBytes(FRAGMENT_TEST_DATA, strlen(FRAGMENT_TEST_DATA));
	EXPECT_CALL(*g_mockIsoBmffHelper,
				IsoBmffSetTimescale(AampGrowableBufferEq(std::cref(cachedFragment.fragment)),
									TRICKMODE_TIMESCALE))
		.WillOnce(Return(true));

	iframeTrack.ProcessAndInjectFragment(&cachedFragment, fragmentDiscarded, isDiscontinuity, ret);
	ASSERT_TRUE(ret);
	ASSERT_EQ(cachedFragment.position, restampedPts);

	// First media segment
	cachedFragment = CachedFragment{};
	cachedFragment.initFragment = false;
	cachedFragment.duration = FRAGMENT_DURATION.inSeconds();
	cachedFragment.position = FIRST_PTS.inSeconds();
	cachedFragment.fragment.AppendBytes(FRAGMENT_TEST_DATA, strlen(FRAGMENT_TEST_DATA));
	AampTime lastPosition{cachedFragment.position};

	// This is an estimate - don't know how long the duration should be, as there isn't a previous
	// PTS to calculate a delta.  Better to avoid too small a number, so limited to 0.25 seconds.
	// GStreamer works ok with this in practice.
	AampTime restampedDuration{std::max(
		cachedFragment.duration / std::fabs(mPrivateInstanceAAMP->rate), 1.0 / TRICKMODE_FPS)};
	int64_t expectedDuration{restampedDuration * TRICKMODE_TIMESCALE};
	int64_t expectedPts{restampedPts * TRICKMODE_TIMESCALE};
	EXPECT_CALL(*g_mockIsoBmffHelper,
				IsoBmffSetPtsAndDuration(AampGrowableBufferEq(std::cref(cachedFragment.fragment)),
										 expectedPts, expectedDuration))
		.WillOnce(Return(true));

	iframeTrack.ProcessAndInjectFragment(&cachedFragment, fragmentDiscarded, isDiscontinuity, ret);
	ASSERT_TRUE(ret);
	ASSERT_EQ(cachedFragment.duration, restampedDuration.inSeconds());
	ASSERT_EQ(cachedFragment.position, restampedPts.inSeconds());

	// Verify the next two steady-state media segments
	for (int i = 1; i <= 2; i++)
	{
		cachedFragment = CachedFragment{};
		cachedFragment.initFragment = false;
		cachedFragment.duration = FRAGMENT_DURATION.inSeconds();
		AampTime nextPts{FIRST_PTS + (FRAGMENT_DURATION * i)};
		cachedFragment.position = nextPts.inSeconds();
		cachedFragment.fragment.AppendBytes(FRAGMENT_TEST_DATA, strlen(FRAGMENT_TEST_DATA));
		AampTime positionDelta{fabs(cachedFragment.position - lastPosition)};
		lastPosition = cachedFragment.position;

		restampedDuration = positionDelta / std::fabs(mPrivateInstanceAAMP->rate);
		restampedPts += restampedDuration;
		expectedDuration = static_cast<int64_t>(restampedDuration * TRICKMODE_TIMESCALE);
		expectedPts = static_cast<int64_t>(restampedPts * TRICKMODE_TIMESCALE);
		EXPECT_CALL(
			*g_mockIsoBmffHelper,
			IsoBmffSetPtsAndDuration(AampGrowableBufferEq(std::cref(cachedFragment.fragment)),
									 expectedPts, expectedDuration))
			.WillOnce(Return(true));

		iframeTrack.ProcessAndInjectFragment(&cachedFragment, fragmentDiscarded, isDiscontinuity,
											 ret);
		ASSERT_TRUE(ret);
		ASSERT_EQ(cachedFragment.duration, restampedDuration.inSeconds());
		ASSERT_EQ(cachedFragment.position, restampedPts.inSeconds());
	}
}

INSTANTIATE_TEST_SUITE_P(MediaTrackTests, MediaTrackDashTrickModePtsRestampValidPlayRateTests,
						 ::testing::Values(FASTEST_TRICKPLAY_RATE, SLOWEST_TRICKPLAY_RATE,
										   -SLOWEST_TRICKPLAY_RATE, -FASTEST_TRICKPLAY_RATE));

TEST_F(MediaTrackTests, DashPlaybackPtsRestampTest)
{
	std::string expectedUri{"Dummy URI"};
	CachedFragment cachedFragment;
	cachedFragment.fragment.AppendBytes(FRAGMENT_TEST_DATA, strlen(FRAGMENT_TEST_DATA));
	cachedFragment.PTSOffsetSec = PTS_OFFSET_SEC;
	cachedFragment.timeScale = PLAYBACK_TIMESCALE;
	cachedFragment.uri = expectedUri;
	bool fragmentDiscarded{false};
	bool isDiscontinuity{false};
	bool ret{true};
	mPrivateInstanceAAMP->rate = AAMP_NORMAL_PLAY_RATE;
	mStreamAbstractionAAMP_MPD->trickplayMode = false;

	TestableMediaTrack videoTrack{mLogObj, eTRACK_VIDEO, mPrivateInstanceAAMP, "video",
								  mStreamAbstractionAAMP_MPD};

	EXPECT_CALL(*g_mockAampConfig, IsConfigSet(eAAMPConfig_EnablePTSReStamp))
		.WillRepeatedly(Return(true));
	EXPECT_CALL(*g_mockAampConfig, IsConfigSet(eAAMPConfig_QtDemuxOverride))
		.WillRepeatedly(Return(false));

	// Init segment
	cachedFragment.initFragment = true;
	EXPECT_CALL(*g_mockIsoBmffHelper, IsoBmffRestampPts(_, _, _)).Times(0);
	EXPECT_CALL(*g_mockIsoBmffHelper, IsoBmffSetTimescale(_, _)).Times(0);

	videoTrack.ProcessAndInjectFragment(&cachedFragment, fragmentDiscarded, isDiscontinuity, ret);
	ASSERT_TRUE(ret);

	// Media segment
	cachedFragment.initFragment = false;
	EXPECT_CALL(*g_mockIsoBmffHelper, IsoBmffRestampPts(AampGrowableBufferEq(std::cref(cachedFragment.fragment)),
														(PTS_OFFSET_SEC * PLAYBACK_TIMESCALE), expectedUri));
	EXPECT_CALL(*g_mockIsoBmffHelper, IsoBmffSetPtsAndDuration(_, _, _)).Times(0);

	videoTrack.ProcessAndInjectFragment(&cachedFragment, fragmentDiscarded, isDiscontinuity, ret);
	ASSERT_TRUE(ret);
}

class MediaTrackDashTrickModePtsRestampInvalidPlayRateTests
	: public MediaTrackTests,
	  public testing::WithParamInterface<float>
{
};

TEST_P(MediaTrackDashTrickModePtsRestampInvalidPlayRateTests, InvalidPlayRateTest)
{
	CachedFragment cachedFragment;
	cachedFragment.fragment.AppendBytes(FRAGMENT_TEST_DATA, strlen(FRAGMENT_TEST_DATA));
	bool fragmentDiscarded{false};
	bool isDiscontinuity{false};
	bool ret{true};
	mPrivateInstanceAAMP->rate = GetParam(); // Test parameter injected here
	mStreamAbstractionAAMP_MPD->trickplayMode = true;

	TestableMediaTrack iframeTrack{mLogObj, eTRACK_VIDEO, mPrivateInstanceAAMP, "iframe",
								   mStreamAbstractionAAMP_MPD};

	// There should be no PTS restamping for normal play rate media fragments in this test
	EXPECT_CALL(*g_mockIsoBmffHelper, IsoBmffRestampPts(_, _, _)).Times(0);

	EXPECT_CALL(*g_mockAampConfig, IsConfigSet(eAAMPConfig_EnablePTSReStamp))
		.WillRepeatedly(Return(true));
	EXPECT_CALL(*g_mockAampConfig, IsConfigSet(eAAMPConfig_QtDemuxOverride))
		.WillRepeatedly(Return(false));

	// Init segment
	cachedFragment.initFragment = true;
	EXPECT_CALL(*g_mockIsoBmffHelper, IsoBmffSetTimescale(_, _)).Times(0);

	iframeTrack.ProcessAndInjectFragment(&cachedFragment, fragmentDiscarded, isDiscontinuity, ret);
	ASSERT_TRUE(ret);

	// Media segment
	cachedFragment.initFragment = false;
	EXPECT_CALL(*g_mockIsoBmffHelper, IsoBmffSetPtsAndDuration(_, _, _)).Times(0);

	iframeTrack.ProcessAndInjectFragment(&cachedFragment, fragmentDiscarded, isDiscontinuity, ret);
	ASSERT_TRUE(ret);
}

INSTANTIATE_TEST_SUITE_P(MediaTrackTests, MediaTrackDashTrickModePtsRestampInvalidPlayRateTests,
						 ::testing::Values(AAMP_RATE_PAUSE, AAMP_SLOWMOTION_RATE));

TEST_F(MediaTrackTests, DashTrickModePtsRestampDiscontinuityTest)
{
	AampTime restampedPts{0}; // Restamped PTS is an offset from the start of trickplay
	bool fragmentDiscarded{false};
	bool isDiscontinuity{false};
	bool ret{true};
	mPrivateInstanceAAMP->rate = FASTEST_TRICKPLAY_RATE;
	mStreamAbstractionAAMP_MPD->trickplayMode = true;

	TestableMediaTrack iframeTrack{mLogObj, eTRACK_VIDEO, mPrivateInstanceAAMP, "iframe",
								   mStreamAbstractionAAMP_MPD};

	// There should be no PTS restamping for normal play rate media fragments in this test
	EXPECT_CALL(*g_mockIsoBmffHelper, IsoBmffRestampPts(_, _, _)).Times(0);

	EXPECT_CALL(*g_mockAampConfig, IsConfigSet(eAAMPConfig_EnablePTSReStamp))
		.WillRepeatedly(Return(true));
	EXPECT_CALL(*g_mockAampConfig, IsConfigSet(eAAMPConfig_QtDemuxOverride))
		.WillRepeatedly(Return(false));
	EXPECT_CALL(*g_mockAampConfig, GetConfigValue(eAAMPConfig_VODTrickPlayFPS))
		.WillRepeatedly(Return(TRICKMODE_FPS));

	// Init segment
	CachedFragment cachedFragment;
	cachedFragment.initFragment = true;
	cachedFragment.fragment.AppendBytes(FRAGMENT_TEST_DATA, strlen(FRAGMENT_TEST_DATA));

	EXPECT_CALL(*g_mockIsoBmffHelper, IsoBmffSetTimescale(_, _)).WillOnce(Return(true));
	iframeTrack.ProcessAndInjectFragment(&cachedFragment, fragmentDiscarded, isDiscontinuity, ret);

	// First media segment
	cachedFragment = CachedFragment{};
	cachedFragment.initFragment = false;
	cachedFragment.duration = FRAGMENT_DURATION.inSeconds();
	cachedFragment.position = FIRST_PTS.inSeconds();
	cachedFragment.fragment.AppendBytes(FRAGMENT_TEST_DATA, strlen(FRAGMENT_TEST_DATA));

	EXPECT_CALL(*g_mockIsoBmffHelper, IsoBmffSetPtsAndDuration(_, _, _)).WillOnce(Return(true));
	iframeTrack.ProcessAndInjectFragment(&cachedFragment, fragmentDiscarded, isDiscontinuity, ret);

	// Second media segment
	// (shorter duration, as might happen for the last segment before an ad break)
	cachedFragment = CachedFragment{};
	cachedFragment.initFragment = false;
	cachedFragment.duration = FRAGMENT_DURATION_BEFORE_AD_BREAK.inSeconds();
	AampTime nextPts{FIRST_PTS + FRAGMENT_DURATION_BEFORE_AD_BREAK};
	cachedFragment.position = nextPts.inSeconds();
	cachedFragment.fragment.AppendBytes(FRAGMENT_TEST_DATA, strlen(FRAGMENT_TEST_DATA));

	AampTime positionDelta{fabs(nextPts - FIRST_PTS)};
	AampTime restampedDuration{positionDelta / std::fabs(mPrivateInstanceAAMP->rate)};
	restampedPts += restampedDuration;

	EXPECT_CALL(*g_mockIsoBmffHelper, IsoBmffSetPtsAndDuration(_, _, _)).WillOnce(Return(true));
	iframeTrack.ProcessAndInjectFragment(&cachedFragment, fragmentDiscarded, isDiscontinuity, ret);

	// New init segment for advert (transition from steady state to discontinuity)
	cachedFragment = CachedFragment{};
	cachedFragment.initFragment = true;
	// For trickplay, this flag appears to be used to signal a discontinuity - not the
	// isDiscontinuity flag passed to ProcessAndInjectFragment()
	cachedFragment.discontinuity = true;
	cachedFragment.fragment.AppendBytes(FRAGMENT_TEST_DATA, strlen(FRAGMENT_TEST_DATA));

	// Assume no change in restamped duration on discontinuity
	restampedPts += restampedDuration;
	EXPECT_CALL(*g_mockIsoBmffHelper,
				IsoBmffSetTimescale(AampGrowableBufferEq(std::cref(cachedFragment.fragment)),
									TRICKMODE_TIMESCALE))
		.WillOnce(Return(true));
	iframeTrack.ProcessAndInjectFragment(&cachedFragment, fragmentDiscarded, isDiscontinuity, ret);
	ASSERT_TRUE(ret);
	ASSERT_EQ(cachedFragment.position, restampedPts);

	// First media segment for advert
	cachedFragment = CachedFragment{};
	cachedFragment.initFragment = false;
	cachedFragment.duration = FRAGMENT_DURATION.inSeconds();
	cachedFragment.position = FIRST_PTS.inSeconds();
	cachedFragment.fragment.AppendBytes(FRAGMENT_TEST_DATA, strlen(FRAGMENT_TEST_DATA));
	AampTime lastPosition{cachedFragment.position};

	int64_t expectedDuration{restampedDuration * TRICKMODE_TIMESCALE};
	int64_t expectedPts{restampedPts * TRICKMODE_TIMESCALE};
	EXPECT_CALL(*g_mockIsoBmffHelper,
				IsoBmffSetPtsAndDuration(AampGrowableBufferEq(std::cref(cachedFragment.fragment)),
										 expectedPts, expectedDuration))
		.WillOnce(Return(true));

	iframeTrack.ProcessAndInjectFragment(&cachedFragment, fragmentDiscarded, isDiscontinuity, ret);
	ASSERT_TRUE(ret);
	ASSERT_EQ(cachedFragment.duration, restampedDuration.inSeconds());
	ASSERT_EQ(cachedFragment.position, restampedPts.inSeconds());

	// Second media segment for advert (transition from discontinuity back to steady state)
	cachedFragment = CachedFragment{};
	cachedFragment.initFragment = false;
	cachedFragment.duration = FRAGMENT_DURATION.inSeconds();
	nextPts = FIRST_PTS + FRAGMENT_DURATION;
	cachedFragment.position = nextPts.inSeconds();
	cachedFragment.fragment.AppendBytes(FRAGMENT_TEST_DATA, strlen(FRAGMENT_TEST_DATA));

	positionDelta = fabs(nextPts - FIRST_PTS);
	restampedDuration = positionDelta / std::fabs(mPrivateInstanceAAMP->rate);
	restampedPts += restampedDuration;
	expectedDuration = static_cast<int64_t>(restampedDuration * TRICKMODE_TIMESCALE);
	expectedPts = static_cast<int64_t>(restampedPts * TRICKMODE_TIMESCALE);
	EXPECT_CALL(*g_mockIsoBmffHelper,
				IsoBmffSetPtsAndDuration(AampGrowableBufferEq(std::cref(cachedFragment.fragment)),
										 expectedPts, expectedDuration))
		.WillOnce(Return(true));

	iframeTrack.ProcessAndInjectFragment(&cachedFragment, fragmentDiscarded, isDiscontinuity, ret);
	ASSERT_TRUE(ret);
	ASSERT_EQ(cachedFragment.duration, restampedDuration.inSeconds());
	ASSERT_EQ(cachedFragment.position, restampedPts.inSeconds());
}
