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
#include <memory>
#include <future>
#include <thread>

#include "AampUtils.h"
#include "AampConfig.h"
#include "AampLogManager.h"
#include "AampTime.h"
#include "priv_aamp.h"
#include "fragmentcollector_mpd.h"

#include "MockIsoBmffHelper.h"
#include "MockIsoBmffBuffer.h"
#include "MockAampConfig.h"
#include "MockPrivateInstanceAAMP.h"
#include "MockStreamAbstractionAAMP_MPD.h"

using namespace testing;
using ::testing::Values;
using ::testing::ValuesIn;

static constexpr const uint8_t FRAGMENT_TEST_DATA[] = "Fragment test data";
static constexpr size_t FRAGMENT_TEST_DATA_SIZE = sizeof(FRAGMENT_TEST_DATA) - 1; // exclude null terminator
static constexpr float FASTEST_TRICKPLAY_RATE{64};
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
static constexpr bool AAMP_TSB_ENABLED{true};
static constexpr bool AAMP_TSB_DISABLED{false};

AampConfig* gpGlobalConfig{nullptr};

// The matcher compares the data content of a std::vector<uint8_t> argument against a
// std::vector<uint8_t> reference (wrapped in std::cref).  The buffer may be larger
// than the expected data (e.g. due to LLD chunk accumulation), so only the leading
// bytes are compared.
MATCHER_P(VectorRefEq, vecStdConstRef, "")
{
	const std::vector<uint8_t>& vec = vecStdConstRef.get();
	return arg.size() >= vec.size() &&
		   std::memcmp(arg.data(), vec.data(), vec.size()) == 0;
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
	void ProcessPlaylist(std::vector<uint8_t>&, int) override {};
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
		g_mockStreamAbstractionAAMP_MPD = new NiceMock<MockStreamAbstractionAAMP_MPD>(mPrivateInstanceAAMP, 0, 0);

		// A fake StreamAbstractionAAMP_MPD that derives from a *real* StreamAbstractionAAMP.
		// The tests can't use a fake/mock StreamAbstractionAAMP base class because
		// StreamAbstractionAAMP and MediaTrack share the same source file and fakes file.
		mStreamAbstractionAAMP_MPD =
			new StreamAbstractionAAMP_MPD(mPrivateInstanceAAMP, 0, 0);
	}

	void TearDown() override
	{
		delete g_mockStreamAbstractionAAMP_MPD;
		g_mockStreamAbstractionAAMP_MPD = nullptr;

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
										bool lowLatencyMode, bool aampTsb = false)
	{
		// A pointer to the test fragment in the cache buffer
		CachedFragment* bufferedFragment{nullptr};

		// Chunk buffer is used for low-latency, AAMP TSB, or any DASH content
		// (mirrors IsInjectionFromCachedFragmentChunks() in MediaTrack)
		bool isDash = (mPrivateInstanceAAMP->mMediaFormat == eMEDIAFORMAT_DASH);
		if (lowLatencyMode || aampTsb || isDash)
		{
			bufferedFragment = mediaTrack.GetFetchChunkBuffer(true);
			mediaTrack.numberOfFragmentChunksCached = 1;
		}
		// Standard buffer is used for non-DASH SLD when AAMP TSB is disabled
		else
		{
			bufferedFragment = mediaTrack.GetFetchBuffer(true);
			mediaTrack.numberOfFragmentsCached = 1;
		}
		bufferedFragment->Copy(testFragment);
		if (lowLatencyMode && !bufferedFragment->initFragment)
		{
			// Make the buffer parser return the correct position and duration
			EXPECT_CALL(*g_mockIsoBmffBuffer, ParseChunkData(_, _, _, _, _, _, _))
				.WillOnce(DoAll(SetArgReferee<5>(bufferedFragment->position),
								SetArgReferee<6>(bufferedFragment->duration), Return(true)));
		}

		return bufferedFragment;
	}

	void SetLowLatencyMode(bool isEnabled)
	{
		AampLLDashServiceData dashData{};
		dashData.lowLatencyMode = isEnabled;
		mPrivateInstanceAAMP->SetLLDashServiceData(dashData);
	}

	/**
	 * @brief Set up a TestableMediaTrack in chunk mode with an init fragment already injected
	 *        and a media fragment queued with the given timescale.
	 *
	 * Configures low-latency / chunk mode, sets common mock expectations, creates the
	 * track, injects an init fragment, then queues one media fragment whose timescale is
	 * set to @p timeScale.  Returns a pair of the track and a pointer to the queued
	 * (buffered) media fragment.
	 */
	std::pair<std::unique_ptr<TestableMediaTrack>, CachedFragment*>
	SetUpChunkModeTrackWithMediaFragment(uint32_t timeScale)
	{
		SetLowLatencyMode(true);
		mPrivateInstanceAAMP->rate = AAMP_NORMAL_PLAY_RATE;
		mPrivateInstanceAAMP->mMediaFormat = eMEDIAFORMAT_DASH;
		mStreamAbstractionAAMP_MPD->trickplayMode = false;

		EXPECT_CALL(*g_mockAampConfig, IsConfigSet(eAAMPConfig_OverrideMediaHeaderDuration))
			.WillRepeatedly(Return(false));
		EXPECT_CALL(*g_mockAampConfig, IsConfigSet(eAAMPConfig_CurlThroughput))
			.WillRepeatedly(Return(false));
		EXPECT_CALL(*g_mockAampConfig, IsConfigSet(eAAMPConfig_EnablePTSReStamp))
			.WillRepeatedly(Return(false));
		EXPECT_CALL(*g_mockAampConfig, GetConfigValue(eAAMPConfig_MaxFragmentCached))
			.WillRepeatedly(Return(1));
		EXPECT_CALL(*g_mockAampConfig, GetConfigValue(eAAMPConfig_MaxFragmentChunkCached))
			.WillRepeatedly(Return(1));
		EXPECT_CALL(*g_mockIsoBmffBuffer, parseBuffer(_, _)).WillRepeatedly(Return(true));
		EXPECT_CALL(*g_mockPrivateInstanceAAMP, GetLLDashChunkMode()).WillRepeatedly(Return(true));

		auto videoTrack = std::make_unique<TestableMediaTrack>(
			eTRACK_VIDEO, mPrivateInstanceAAMP, "video", mStreamAbstractionAAMP_MPD);

		// Inject an init fragment (required before media fragments)
		CachedFragment initFragment{};
		initFragment.initFragment = true;
		initFragment.fragment.assign(FRAGMENT_TEST_DATA, FRAGMENT_TEST_DATA + FRAGMENT_TEST_DATA_SIZE);
		CachedFragment* buf = videoTrack->GetFetchChunkBuffer(true);
		videoTrack->numberOfFragmentChunksCached = 1;
		buf->Copy(initFragment);
		EXPECT_TRUE(videoTrack->InjectFragment());

		// Queue a media fragment with the requested timescale
		CachedFragment mediaFragment{};
		mediaFragment.initFragment = false;
		mediaFragment.duration = FRAGMENT_DURATION.inSeconds();
		mediaFragment.position = FIRST_PTS.inSeconds();
		mediaFragment.timeScale = timeScale;
		mediaFragment.uri = "test_segment.m4s";
		mediaFragment.fragment.assign(FRAGMENT_TEST_DATA, FRAGMENT_TEST_DATA + FRAGMENT_TEST_DATA_SIZE);

		buf = videoTrack->GetFetchChunkBuffer(true);
		videoTrack->numberOfFragmentChunksCached = 1;
		buf->Copy(mediaFragment);

		return {std::move(videoTrack), buf};
	}
};

struct PlayRateTestData
{
	bool lowLatencyMode;
	float playRate;
};

struct AampTsbTestData
{
	bool lowLatencyMode;
	bool aampTsb;
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
	testFragment.fragment.assign(FRAGMENT_TEST_DATA, FRAGMENT_TEST_DATA + FRAGMENT_TEST_DATA_SIZE);
	testFragment.timeScale = PLAYBACK_TIMESCALE;
	PlayRateTestData testParam = GetParam(); // Test parameter injected here
	SetLowLatencyMode(testParam.lowLatencyMode);
	mPrivateInstanceAAMP->rate = testParam.playRate;
	mStreamAbstractionAAMP_MPD->trickplayMode = true;

	EXPECT_CALL(*g_mockAampConfig, IsConfigSet(eAAMPConfig_OverrideMediaHeaderDuration))
		.WillRepeatedly(Return(false));
	EXPECT_CALL(*g_mockAampConfig, IsConfigSet(eAAMPConfig_CurlThroughput))
		.WillRepeatedly(Return(false));
	EXPECT_CALL(*g_mockAampConfig, IsConfigSet(eAAMPConfig_EnablePTSReStamp))
		.WillRepeatedly(Return(false));
	EXPECT_CALL(*g_mockAampConfig, GetConfigValue(eAAMPConfig_MaxFragmentCached))
		.WillRepeatedly(Return(1));
	EXPECT_CALL(*g_mockAampConfig, GetConfigValue(eAAMPConfig_MaxFragmentChunkCached))
		.WillRepeatedly(Return(1));
	EXPECT_CALL(*g_mockIsoBmffBuffer, parseBuffer(_, _)).WillRepeatedly(Return(true));
	EXPECT_CALL(*g_mockPrivateInstanceAAMP, GetLLDashChunkMode()).WillRepeatedly(Return(testParam.lowLatencyMode));

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
						 ValuesIn(ptsRestampNotConfiguredPlayRateTestData));

class MediaTrackDashQtDemuxOverrideConfiguredTests
	: public MediaTrackTests,
	  public testing::WithParamInterface<PlayRateTestData>
{
};

TEST_P(MediaTrackDashQtDemuxOverrideConfiguredTests, QtDemuxOverrideConfiguredTest)
{
	CachedFragment* bufferedFragment{nullptr};
	CachedFragment testFragment;
	testFragment.fragment.assign(FRAGMENT_TEST_DATA, FRAGMENT_TEST_DATA + FRAGMENT_TEST_DATA_SIZE);
	testFragment.timeScale = PLAYBACK_TIMESCALE;
	PlayRateTestData testParam = GetParam(); // Test parameter injected here
	SetLowLatencyMode(testParam.lowLatencyMode);
	mPrivateInstanceAAMP->rate = testParam.playRate;
	mStreamAbstractionAAMP_MPD->trickplayMode = true;

	EXPECT_CALL(*g_mockAampConfig, IsConfigSet(eAAMPConfig_OverrideMediaHeaderDuration))
		.WillRepeatedly(Return(false));
	EXPECT_CALL(*g_mockAampConfig, IsConfigSet(eAAMPConfig_CurlThroughput))
		.WillRepeatedly(Return(false));
	EXPECT_CALL(*g_mockAampConfig, IsConfigSet(eAAMPConfig_EnablePTSReStamp))
		.WillRepeatedly(Return(false));
	EXPECT_CALL(*g_mockAampConfig, GetConfigValue(eAAMPConfig_MaxFragmentCached))
		.WillRepeatedly(Return(1));
	EXPECT_CALL(*g_mockAampConfig, GetConfigValue(eAAMPConfig_MaxFragmentChunkCached))
		.WillRepeatedly(Return(1));
	EXPECT_CALL(*g_mockIsoBmffBuffer, parseBuffer(_, _)).WillRepeatedly(Return(true));
	EXPECT_CALL(*g_mockPrivateInstanceAAMP, GetLLDashChunkMode()).WillRepeatedly(Return(testParam.lowLatencyMode));

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
						 ValuesIn(qtDemuxOverrideConfiguredPlayRateTestData));

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

	EXPECT_CALL(*g_mockAampConfig, IsConfigSet(eAAMPConfig_CurlThroughput))
		.WillRepeatedly(Return(false));
	EXPECT_CALL(*g_mockAampConfig, IsConfigSet(eAAMPConfig_EnablePTSReStamp))
		.WillRepeatedly(Return(true));
	EXPECT_CALL(*g_mockAampConfig, GetConfigValue(eAAMPConfig_VODTrickPlayFPS))
		.WillRepeatedly(Return(TRICKMODE_FPS));
	EXPECT_CALL(*g_mockAampConfig, GetConfigValue(eAAMPConfig_MaxFragmentCached))
		.WillRepeatedly(Return(1));
	EXPECT_CALL(*g_mockAampConfig, GetConfigValue(eAAMPConfig_MaxFragmentChunkCached))
		.WillRepeatedly(Return(1));
	EXPECT_CALL(*g_mockIsoBmffBuffer, parseBuffer(_, _)).WillRepeatedly(Return(true));
	EXPECT_CALL(*g_mockPrivateInstanceAAMP, GetLLDashChunkMode()).WillRepeatedly(Return(testParam.lowLatencyMode));

	TestableMediaTrack iframeTrack{eTRACK_VIDEO, mPrivateInstanceAAMP, "iframe",
								   mStreamAbstractionAAMP_MPD};

	// Init segment
	CachedFragment testFragment;
	testFragment.initFragment = true;
	testFragment.fragment.assign(FRAGMENT_TEST_DATA, FRAGMENT_TEST_DATA + FRAGMENT_TEST_DATA_SIZE);
	bufferedFragment = AddFragmentToBuffer(iframeTrack, testFragment, testParam.lowLatencyMode);

	EXPECT_CALL(*g_mockIsoBmffHelper,
				SetTimescale(VectorRefEq(std::cref(testFragment.fragment)),
									TRICKMODE_TIMESCALE))
		.WillOnce(Return(true));

	ASSERT_TRUE(iframeTrack.InjectFragment());
	ASSERT_EQ(bufferedFragment->position, restampedPts);

	// First media segment
	testFragment = CachedFragment{};
	testFragment.initFragment = false;
	testFragment.timeScale = PLAYBACK_TIMESCALE;
	testFragment.duration = FRAGMENT_DURATION.inSeconds();
	testFragment.position = FIRST_PTS.inSeconds();
	testFragment.absPosition = FIRST_PTS.inSeconds();
	testFragment.fragment.assign(FRAGMENT_TEST_DATA, FRAGMENT_TEST_DATA + FRAGMENT_TEST_DATA_SIZE);
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
		SetPtsAndDuration(VectorRefEq(std::cref(testFragment.fragment)),
								 expectedPts, expectedDuration))
		.WillOnce(Return(true));

	if (testParam.lowLatencyMode)
	{
		// Check that the PTS that is (eventually) passed on to GStreamer is as expected
		EXPECT_CALL(*g_mockPrivateInstanceAAMP,
					SendStreamTransfer(eMEDIATYPE_VIDEO,
									   VectorRefEq(std::cref(testFragment.fragment)),
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
		testFragment.fragment.assign(FRAGMENT_TEST_DATA, FRAGMENT_TEST_DATA + FRAGMENT_TEST_DATA_SIZE);
		bufferedFragment = AddFragmentToBuffer(iframeTrack, testFragment, testParam.lowLatencyMode);
		EXPECT_CALL(*g_mockIsoBmffHelper,
					SetTimescale(VectorRefEq(std::cref(testFragment.fragment)),
								 TRICKMODE_TIMESCALE)
					).WillOnce(Return(true));
		if (testParam.lowLatencyMode)
		{	// PTS / DTS is not relevant for init segment, so ignore the values
			EXPECT_CALL(*g_mockPrivateInstanceAAMP,
						SendStreamTransfer(eMEDIATYPE_VIDEO,
										   VectorRefEq(std::cref(testFragment.fragment)),
										   _,  _, _,
										   _, _, _));
		}
		ASSERT_TRUE(iframeTrack.InjectFragment());

		testFragment = CachedFragment{};
		testFragment.initFragment = false;
		testFragment.timeScale = PLAYBACK_TIMESCALE;
		testFragment.duration = FRAGMENT_DURATION.inSeconds();
		AampTime nextPts{FIRST_PTS + (FRAGMENT_DURATION * i)};
		testFragment.position = nextPts.inSeconds();
		testFragment.absPosition = nextPts.inSeconds();
		testFragment.fragment.assign(FRAGMENT_TEST_DATA, FRAGMENT_TEST_DATA + FRAGMENT_TEST_DATA_SIZE);
		AampTime positionDelta{fabs(testFragment.position - lastPosition)};
		lastPosition = testFragment.position;
		bufferedFragment = AddFragmentToBuffer(iframeTrack, testFragment, testParam.lowLatencyMode);

		restampedDuration = positionDelta / std::fabs(mPrivateInstanceAAMP->rate);
		restampedPts += restampedDuration;
		expectedDuration = static_cast<int64_t>(restampedDuration * TRICKMODE_TIMESCALE);
		expectedPts = static_cast<int64_t>(restampedPts * TRICKMODE_TIMESCALE);
		EXPECT_CALL(
			*g_mockIsoBmffHelper,
			SetPtsAndDuration(VectorRefEq(std::cref(testFragment.fragment)),
									 expectedPts, expectedDuration))
			.WillOnce(Return(true));

		if (testParam.lowLatencyMode)
		{
			// Check that the PTS that is (eventually) passed on to GStreamer is as expected
			EXPECT_CALL(*g_mockPrivateInstanceAAMP,
						SendStreamTransfer(eMEDIATYPE_VIDEO,
										   VectorRefEq(std::cref(testFragment.fragment)),
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
						 ValuesIn(validPlayRateTestData));

class MediaTrackDashPlaybackPtsRestampTests : public MediaTrackTests,
											  public testing::WithParamInterface<AampTsbTestData>
{
};

TEST_P(MediaTrackDashPlaybackPtsRestampTests, PlaybackTest)
{
	std::string expectedUri{"Dummy URI"};
	CachedFragment* bufferedFragment{nullptr};
	CachedFragment testFragment;
	testFragment.fragment.assign(FRAGMENT_TEST_DATA, FRAGMENT_TEST_DATA + FRAGMENT_TEST_DATA_SIZE);
	testFragment.position = FIRST_PTS.inSeconds();
	testFragment.PTSOffsetSec = PTS_OFFSET_SEC;
	testFragment.timeScale = PLAYBACK_TIMESCALE;
	testFragment.uri = expectedUri;
	AampTsbTestData aampTsbTestData = GetParam(); // Test parameter injected here
	bool lowLatencyMode = aampTsbTestData.lowLatencyMode;
	bool aampTsb = aampTsbTestData.aampTsb;
	AAMPLOG_MIL("PlaybackTest lowLatencyMode %d aampTsb %d", lowLatencyMode, aampTsb);
	SetLowLatencyMode(lowLatencyMode);
	mPrivateInstanceAAMP->rate = AAMP_NORMAL_PLAY_RATE;
	mPrivateInstanceAAMP->mMediaFormat = eMEDIAFORMAT_DASH;
	mPrivateInstanceAAMP->SetLocalAAMPTsb(aampTsb);
	mStreamAbstractionAAMP_MPD->trickplayMode = false;

	EXPECT_CALL(*g_mockAampConfig, IsConfigSet(eAAMPConfig_OverrideMediaHeaderDuration))
		.WillRepeatedly(Return(false));
	EXPECT_CALL(*g_mockAampConfig, IsConfigSet(eAAMPConfig_CurlThroughput))
		.WillRepeatedly(Return(false));
	EXPECT_CALL(*g_mockAampConfig, IsConfigSet(eAAMPConfig_UseMp4Demux))
		.WillRepeatedly(Return(false));
	EXPECT_CALL(*g_mockAampConfig, IsConfigSet(eAAMPConfig_EnablePTSReStamp))
		.WillRepeatedly(Return(true));
	EXPECT_CALL(*g_mockAampConfig, GetConfigValue(eAAMPConfig_MaxFragmentCached))
		.WillRepeatedly(Return(1));
	EXPECT_CALL(*g_mockAampConfig, GetConfigValue(eAAMPConfig_MaxFragmentChunkCached))
		.WillRepeatedly(Return(1));
	EXPECT_CALL(*g_mockIsoBmffBuffer, parseBuffer(_, _)).WillRepeatedly(Return(true));
	EXPECT_CALL(*g_mockPrivateInstanceAAMP, GetLLDashChunkMode()).WillRepeatedly(Return(lowLatencyMode));

	TestableMediaTrack videoTrack{eTRACK_VIDEO, mPrivateInstanceAAMP, "video",
								  mStreamAbstractionAAMP_MPD};

	// Init segment
	testFragment.initFragment = true;
	bufferedFragment = AddFragmentToBuffer(videoTrack, testFragment, lowLatencyMode, aampTsb);
	EXPECT_CALL(*g_mockIsoBmffHelper, RestampPts(_, _, _, _, _)).Times(0);
	EXPECT_CALL(*g_mockIsoBmffHelper, SetTimescale(_, _)).Times(0);

	ASSERT_TRUE(videoTrack.InjectFragment());

	// Media segment (normal case: UseMp4Demux disabled)
	testFragment.initFragment = false;
	testFragment.duration = FRAGMENT_DURATION.inSeconds();
	bufferedFragment = AddFragmentToBuffer(videoTrack, testFragment, lowLatencyMode, aampTsb);
	EXPECT_CALL(*g_mockIsoBmffHelper,
				RestampPts(VectorRefEq(std::cref(testFragment.fragment)),
								  (PTS_OFFSET_SEC * PLAYBACK_TIMESCALE), expectedUri, "video", PLAYBACK_TIMESCALE));
	EXPECT_CALL(*g_mockIsoBmffHelper, SetPtsAndDuration(_, _, _)).Times(0);
	if (lowLatencyMode)
	{
		// In chunk mode, PTS offset is applied to fpts/fdts
		// (FIRST_PTS + PTS_OFFSET_SEC) and also passed separately to
		// GStreamer via the fragmentPTSoffset argument
		double expectedPts = FIRST_PTS.inSeconds() + PTS_OFFSET_SEC;
		EXPECT_CALL(*g_mockPrivateInstanceAAMP,
					SendStreamTransfer(eMEDIATYPE_VIDEO,
									VectorRefEq(std::cref(testFragment.fragment)),
									expectedPts, expectedPts, _, _, _, _));
	}
	ASSERT_TRUE(videoTrack.InjectFragment());
	ASSERT_DOUBLE_EQ(videoTrack.GetTotalInjectedDuration(), FRAGMENT_DURATION.inSeconds());

	// Media segment (UseMp4Demux enabled: RestampPts should NOT be called)	
	EXPECT_CALL(*g_mockAampConfig, IsConfigSet(eAAMPConfig_UseMp4Demux)).WillRepeatedly(Return(true));
	testFragment = CachedFragment{};
	testFragment.initFragment = false;
	testFragment.duration = FRAGMENT_DURATION.inSeconds();
	testFragment.position = FIRST_PTS.inSeconds();
	testFragment.PTSOffsetSec = PTS_OFFSET_SEC;
	testFragment.timeScale = PLAYBACK_TIMESCALE;
	testFragment.uri = expectedUri;
	testFragment.fragment.assign(FRAGMENT_TEST_DATA, FRAGMENT_TEST_DATA + FRAGMENT_TEST_DATA_SIZE);
	bufferedFragment = AddFragmentToBuffer(videoTrack, testFragment, lowLatencyMode, aampTsb);
	videoTrack.numberOfFragmentsCached = 1;
	ASSERT_NE(bufferedFragment, nullptr);
	ASSERT_GT(bufferedFragment->fragment.size(), 0);
	EXPECT_CALL(*g_mockIsoBmffHelper, RestampPts(_, _, _, _, _)).Times(0);
	EXPECT_CALL(*g_mockIsoBmffHelper, SetPtsAndDuration(_, _, _)).Times(0);
	if (lowLatencyMode)
	{
		// In chunk mode, PTS offset is passed separately to GStreamer
		double expectedPts = FIRST_PTS.inSeconds();
		EXPECT_CALL(*g_mockPrivateInstanceAAMP,
					SendStreamTransfer(eMEDIATYPE_VIDEO,
									VectorRefEq(std::cref(testFragment.fragment)),
									expectedPts, expectedPts, _, _, _, _));
	}
	ASSERT_TRUE(videoTrack.InjectFragment());
}

AampTsbTestData aampTsbTestData[] =
{
	{LLD_DISABLED, AAMP_TSB_DISABLED},
	{LLD_DISABLED, AAMP_TSB_ENABLED},
	{LLD_ENABLED, AAMP_TSB_DISABLED},
	{LLD_ENABLED, AAMP_TSB_ENABLED}
};

INSTANTIATE_TEST_SUITE_P(MediaTrackTests, MediaTrackDashPlaybackPtsRestampTests, ValuesIn(aampTsbTestData));

class MediaTrackDashTrickModePtsRestampInvalidPlayRateTests
	: public MediaTrackTests,
	  public testing::WithParamInterface<float>
{
};

TEST_P(MediaTrackDashTrickModePtsRestampInvalidPlayRateTests, InvalidPlayRateTest)
{
	CachedFragment* bufferedFragment{nullptr};
	CachedFragment testFragment;
	testFragment.fragment.assign(FRAGMENT_TEST_DATA, FRAGMENT_TEST_DATA + FRAGMENT_TEST_DATA_SIZE);
	mPrivateInstanceAAMP->rate = GetParam(); // Test parameter injected here
	mStreamAbstractionAAMP_MPD->trickplayMode = true;

	// There should be no PTS restamping for normal play rate media fragments in this test
	EXPECT_CALL(*g_mockIsoBmffHelper, RestampPts(_, _, _, _, _)).Times(0);

		EXPECT_CALL(*g_mockAampConfig, IsConfigSet(eAAMPConfig_OverrideMediaHeaderDuration))
		.WillRepeatedly(Return(false));
	EXPECT_CALL(*g_mockAampConfig, IsConfigSet(eAAMPConfig_CurlThroughput))
		.WillRepeatedly(Return(false));
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
						 Values(AAMP_RATE_PAUSE, AAMP_SLOWMOTION_RATE));

TEST_F(MediaTrackTests, DashTrickModePtsRestampDiscontinuityTest)
{
	CachedFragment* bufferedFragment{nullptr};
	AampTime restampedPts; // Restamped PTS is an offset from the start of trickplay
	mPrivateInstanceAAMP->rate = FASTEST_TRICKPLAY_RATE;
	mPrivateInstanceAAMP->mMediaFormat = eMEDIAFORMAT_DASH;
	mStreamAbstractionAAMP_MPD->trickplayMode = true;

	// There should be no PTS restamping for normal play rate media fragments in this test
	EXPECT_CALL(*g_mockIsoBmffHelper, RestampPts(_, _, _, _, _)).Times(0);

	EXPECT_CALL(*g_mockAampConfig, IsConfigSet(eAAMPConfig_CurlThroughput))
		.WillRepeatedly(Return(false));
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
	testFragment.fragment.assign(FRAGMENT_TEST_DATA, FRAGMENT_TEST_DATA + FRAGMENT_TEST_DATA_SIZE);
	bufferedFragment = AddFragmentToBuffer(iframeTrack, testFragment, LLD_DISABLED);

	EXPECT_CALL(*g_mockIsoBmffHelper, SetTimescale(_, _)).WillOnce(Return(true));
	ASSERT_TRUE(iframeTrack.InjectFragment());

	// First media segment
	testFragment = CachedFragment{};
	testFragment.initFragment = false;
	testFragment.duration = FRAGMENT_DURATION.inSeconds();
	testFragment.position = FIRST_PTS.inSeconds();
	testFragment.fragment.assign(FRAGMENT_TEST_DATA, FRAGMENT_TEST_DATA + FRAGMENT_TEST_DATA_SIZE);
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
	testFragment.fragment.assign(FRAGMENT_TEST_DATA, FRAGMENT_TEST_DATA + FRAGMENT_TEST_DATA_SIZE);
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
	testFragment.fragment.assign(FRAGMENT_TEST_DATA, FRAGMENT_TEST_DATA + FRAGMENT_TEST_DATA_SIZE);
	bufferedFragment = AddFragmentToBuffer(iframeTrack, testFragment, LLD_DISABLED);

	// Assume no change in restamped duration on discontinuity
	restampedPts += restampedDuration;
	EXPECT_CALL(*g_mockIsoBmffHelper,
				SetTimescale(VectorRefEq(std::cref(testFragment.fragment)),
									TRICKMODE_TIMESCALE))
		.WillOnce(Return(true));
	ASSERT_TRUE(iframeTrack.InjectFragment());
	ASSERT_DOUBLE_EQ(bufferedFragment->position, restampedPts.inSeconds());

	// First media segment for advert
	testFragment = CachedFragment{};
	testFragment.initFragment = false;
	testFragment.duration = FRAGMENT_DURATION.inSeconds();
	testFragment.position = FIRST_PTS.inSeconds();
	testFragment.fragment.assign(FRAGMENT_TEST_DATA, FRAGMENT_TEST_DATA + FRAGMENT_TEST_DATA_SIZE);
	AampTime lastPosition{testFragment.position};
	bufferedFragment = AddFragmentToBuffer(iframeTrack, testFragment, LLD_DISABLED);

	int64_t expectedDuration{restampedDuration * TRICKMODE_TIMESCALE};
	int64_t expectedPts = (int64_t)(restampedPts.inSeconds() * TRICKMODE_TIMESCALE);

	EXPECT_CALL(
		*g_mockIsoBmffHelper,
		SetPtsAndDuration(VectorRefEq(std::cref(testFragment.fragment)),
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
	testFragment.fragment.assign(FRAGMENT_TEST_DATA, FRAGMENT_TEST_DATA + FRAGMENT_TEST_DATA_SIZE);
	bufferedFragment = AddFragmentToBuffer(iframeTrack, testFragment, LLD_DISABLED);

	positionDelta = fabs(nextPts - FIRST_PTS);
	restampedDuration = positionDelta / std::fabs(mPrivateInstanceAAMP->rate);
	restampedPts += restampedDuration;
	expectedDuration = static_cast<int64_t>(restampedDuration * TRICKMODE_TIMESCALE);
	expectedPts = static_cast<int64_t>(restampedPts.inSeconds() * TRICKMODE_TIMESCALE);
	EXPECT_CALL(
		*g_mockIsoBmffHelper,
		SetPtsAndDuration(VectorRefEq(std::cref(testFragment.fragment)),
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

	EXPECT_CALL(*g_mockAampConfig, IsConfigSet(eAAMPConfig_CurlThroughput))
		.WillRepeatedly(Return(false));
	EXPECT_CALL(*g_mockAampConfig, IsConfigSet(eAAMPConfig_EnablePTSReStamp))
		.WillRepeatedly(Return(true));
	EXPECT_CALL(*g_mockAampConfig, GetConfigValue(eAAMPConfig_MaxFragmentCached))
		.WillRepeatedly(Return(5));
	EXPECT_CALL(*g_mockAampConfig, GetConfigValue(eAAMPConfig_MaxFragmentChunkCached))
		.WillRepeatedly(Return(5));

	TestableMediaTrack videoTrack{eTRACK_VIDEO, mPrivateInstanceAAMP, "video", mStreamAbstractionAAMP_MPD};

	bufferedFragment1 = videoTrack.GetFetchBuffer(true);
	bufferedFragment1->initFragment = true;
	bufferedFragment1->fragment.assign(FRAGMENT_TEST_DATA, FRAGMENT_TEST_DATA + FRAGMENT_TEST_DATA_SIZE);
	videoTrack.UpdateTSAfterFetch(bufferedFragment1->initFragment);

	// First media segment
	bufferedFragment2 = videoTrack.GetFetchBuffer(true);
	bufferedFragment2->initFragment = false;
	bufferedFragment2->duration = FRAGMENT_DURATION.inSeconds();
	bufferedFragment2->position = FIRST_PTS.inSeconds();
	bufferedFragment2->fragment.assign(FRAGMENT_TEST_DATA, FRAGMENT_TEST_DATA + FRAGMENT_TEST_DATA_SIZE);
	videoTrack.UpdateTSAfterFetch(bufferedFragment2->initFragment);

	// Second media segment, not updated for injection
	bufferedFragment3 = videoTrack.GetFetchBuffer(true);
	bufferedFragment3->initFragment = false;
	bufferedFragment3->duration = FRAGMENT_DURATION.inSeconds();
	bufferedFragment3->position = 2 * FIRST_PTS.inSeconds();
	bufferedFragment3->fragment.assign(FRAGMENT_TEST_DATA, FRAGMENT_TEST_DATA + FRAGMENT_TEST_DATA_SIZE);

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

TEST_F(MediaTrackTests, MediaTrackConstructorTest)
{
	constexpr int kMaxFragmentCached{4};
	constexpr int kMaxFragmentChunkCached{20};

	EXPECT_CALL(*g_mockAampConfig, GetConfigValue(eAAMPConfig_MaxFragmentCached))
		.WillRepeatedly(Return(kMaxFragmentCached));
	EXPECT_CALL(*g_mockAampConfig, GetConfigValue(eAAMPConfig_MaxFragmentChunkCached))
		.WillRepeatedly(Return(kMaxFragmentChunkCached));

	TestableMediaTrack videoTrack{eTRACK_VIDEO, mPrivateInstanceAAMP, "video", mStreamAbstractionAAMP_MPD};
	EXPECT_EQ(videoTrack.GetCachedFragmentChunksSize(), kMaxFragmentCached);
}

TEST_F(MediaTrackTests, MediaTrackConstructorChunkModeTest)
{
	constexpr int kMaxFragmentCached{4};
	constexpr int kMaxFragmentChunkCached{20};

	EXPECT_CALL(*g_mockAampConfig, GetConfigValue(eAAMPConfig_MaxFragmentCached))
		.WillRepeatedly(Return(kMaxFragmentCached));
	EXPECT_CALL(*g_mockAampConfig, GetConfigValue(eAAMPConfig_MaxFragmentChunkCached))
		.WillRepeatedly(Return(kMaxFragmentChunkCached));
	EXPECT_CALL(*g_mockPrivateInstanceAAMP, GetLLDashChunkMode()).WillOnce(Return(true));
	TestableMediaTrack videoTrack{eTRACK_VIDEO, mPrivateInstanceAAMP, "video", mStreamAbstractionAAMP_MPD};
	EXPECT_EQ(videoTrack.GetCachedFragmentChunksSize(), kMaxFragmentChunkCached);
}

/**
 * @brief Test that ProcessFragmentChunk uses the cached fragment's timescale
 *
 * This test verifies that when processing fragment chunks, the timescale from
 * the cached fragment is used. This is critical for TSB (Time-Shift Buffer)
 * scenarios where:
 * - The segment being downloaded at the live edge may be from an ad with one timescale
 * - The segment being injected from TSB may be from base content with a different timescale
 */
TEST_F(MediaTrackTests, ProcessFragmentChunkUsesFragmentTimescale)
{
	constexpr uint32_t kFragmentTimeScale{90000};
	auto [videoTrack, bufferedFragment] = SetUpChunkModeTrackWithMediaFragment(kFragmentTimeScale);

	// Key assertion: ParseChunkData should be called with the fragment's timescale
	EXPECT_CALL(*g_mockIsoBmffBuffer,
				ParseChunkData(_, _, kFragmentTimeScale, _, _, _, _))
		.WillOnce(DoAll(SetArgReferee<5>(bufferedFragment->position),
						SetArgReferee<6>(bufferedFragment->duration), Return(true)));

	ASSERT_TRUE(videoTrack->InjectFragment());
}

/**
 * @brief Test ProcessFragmentChunk behaviour when fragment timescale is zero
 *
 * This test verifies the behaviour when a cached fragment has a zero timescale.
 * This should never happen in real playback scenarios. When it does occur,
 * ProcessFragmentChunk returns early without calling ParseChunkData, as the
 * timescale is required for correct PTS calculation.
 */
TEST_F(MediaTrackTests, ProcessFragmentChunkWithZeroTimescale)
{
	constexpr uint32_t kZeroTimeScale{0};
	auto [videoTrack, bufferedFragment] = SetUpChunkModeTrackWithMediaFragment(kZeroTimeScale);

	// When timescale is 0, ProcessFragmentChunk returns early without calling ParseChunkData
	EXPECT_CALL(*g_mockIsoBmffBuffer, ParseChunkData(_, _, _, _, _, _, _)).Times(0);

	ASSERT_TRUE(videoTrack->InjectFragment());
}

/**
 * @brief Test that WaitForManifestUpdate can be aborted successfully.
 * This is important to avoid deadlocks if the manifest update takes a long time or fails to complete.
 */
TEST_F(MediaTrackTests, WaitForManifestUpdateTest)
{
	EXPECT_CALL(*g_mockPrivateInstanceAAMP, DownloadsAreEnabled()).WillRepeatedly(Return(true));
	TestableMediaTrack videoTrack{eTRACK_VIDEO, mPrivateInstanceAAMP, "video", mStreamAbstractionAAMP_MPD};

	std::thread manifestUpdateThread([&videoTrack]() {
		videoTrack.WaitForManifestUpdate();
	});

	std::this_thread::sleep_for(std::chrono::milliseconds(200)); // Give the thread a moment to start and block on the wait
	videoTrack.AbortWaitForManifestUpdate();
	manifestUpdateThread.join();
}

/**
 * @brief Test that GetManifestUpdateCounter() returns the current counter value and
 * that AbortWaitForManifestUpdate() increments it.
 */
TEST_F(MediaTrackTests, GetManifestUpdateCounterTest)
{
	TestableMediaTrack videoTrack{eTRACK_VIDEO, mPrivateInstanceAAMP, "video", mStreamAbstractionAAMP_MPD};

	const uint32_t initialCounter = videoTrack.GetManifestUpdateCounter();

	videoTrack.AbortWaitForManifestUpdate();
	EXPECT_EQ(videoTrack.GetManifestUpdateCounter(), initialCounter + 1);

	videoTrack.AbortWaitForManifestUpdate();
	EXPECT_EQ(videoTrack.GetManifestUpdateCounter(), initialCounter + 2);
}

/**
 * @brief Test the race prevention pattern: snapshot the counter with
 * GetManifestUpdateCounter() *before* doing work, then call
 * WaitForManifestUpdate(snapshotCounter).  If AbortWaitForManifestUpdate() fires
 * between the snapshot and the wait call, the predicate is already satisfied and
 * WaitForManifestUpdate(snapshotCounter) must return immediately without blocking.
 */
TEST_F(MediaTrackTests, WaitForManifestUpdateSnapshotRacePreventionTest)
{
	EXPECT_CALL(*g_mockPrivateInstanceAAMP, DownloadsAreEnabled()).WillRepeatedly(Return(true));
	TestableMediaTrack videoTrack{eTRACK_VIDEO, mPrivateInstanceAAMP, "video", mStreamAbstractionAAMP_MPD};

	// Step 1: snapshot the counter before any work begins
	const uint32_t snapshot = videoTrack.GetManifestUpdateCounter();

	// Step 2: simulate the race — AbortWaitForManifestUpdate() fires BEFORE the wait call
	videoTrack.AbortWaitForManifestUpdate();

	// Step 3: WaitForManifestUpdate(snapshot) must return immediately because the
	// counter has already advanced past the snapshot.  Run it on a background thread
	// so we can enforce a tight deadline without hanging the test runner.
	auto future = std::async(std::launch::async, [&videoTrack, snapshot]() {
		videoTrack.WaitForManifestUpdate(snapshot);
	});

	// 500 ms is generous for an already-satisfied predicate; any blocking would fail this.
	EXPECT_EQ(future.wait_for(std::chrono::milliseconds(500)), std::future_status::ready)
		<< "WaitForManifestUpdate(snapshotCounter) blocked even though the counter was "
		   "already incremented — lost-wakeup race prevention is broken";
}

/**
 * @brief Test that GetBufferStatus() returns BUFFER_STATUS_GREEN when the buffer is sufficient
 * in low latency mode.
 */
TEST_F(MediaTrackTests, GetBufferStatus_ReturnsGreen_WhenBufferIsSufficient)
{
	TestableMediaTrack videoTrack{eTRACK_VIDEO, mPrivateInstanceAAMP, "video", mStreamAbstractionAAMP_MPD};
	// Low Latency mode enabled, which doesn't check for cached fragments
	SetLowLatencyMode(true);
	EXPECT_CALL(*g_mockAampConfig, GetConfigValue(eAAMPConfig_UnderflowDetectThresholdSec))
		.WillRepeatedly(Return(1.0)); // Set underflow threshold to 1 second
	// Simulate sufficient buffered duration above the green threshold
	EXPECT_CALL(*g_mockStreamAbstractionAAMP_MPD, GetBufferedDuration())
		.WillOnce(Return(AAMP_BUFFER_MONITOR_GREEN_THRESHOLD_LLD + 2.0));
	EXPECT_EQ(videoTrack.GetBufferStatus(), BUFFER_STATUS_GREEN);
}

/**
 * @brief Test that GetBufferStatus() returns BUFFER_STATUS_YELLOW
 * when the buffer is below threshold and above underflow threshold in low latency mode.
 */
TEST_F(MediaTrackTests, GetBufferStatus_ReturnsYellow_WhenBufferIsBelowThreshold)
{
	TestableMediaTrack videoTrack{eTRACK_VIDEO, mPrivateInstanceAAMP, "video", mStreamAbstractionAAMP_MPD};
	// Low Latency mode enabled, which doesn't check for cached fragments
	SetLowLatencyMode(true);
	EXPECT_CALL(*g_mockAampConfig, GetConfigValue(eAAMPConfig_UnderflowDetectThresholdSec))
		.WillRepeatedly(Return(0.5)); // Set underflow threshold to 0.5 second
	// Simulate buffered duration just below the threshold and above underflow threshold
	EXPECT_CALL(*g_mockStreamAbstractionAAMP_MPD, GetBufferedDuration())
		.WillOnce(Return(AAMP_BUFFER_MONITOR_GREEN_THRESHOLD_LLD - 0.1));
	EXPECT_EQ(videoTrack.GetBufferStatus(), BUFFER_STATUS_YELLOW);
}

/**
 * @brief Test that GetBufferStatus() returns BUFFER_STATUS_RED
 * when the buffer is below underflow threshold in low latency mode.
 */
TEST_F(MediaTrackTests, GetBufferStatus_ReturnsRed_WhenBufferIsBelowUnderflowThreshold)
{
	TestableMediaTrack videoTrack{eTRACK_VIDEO, mPrivateInstanceAAMP, "video", mStreamAbstractionAAMP_MPD};
	// Low Latency mode enabled, which doesn't check for cached fragments
	SetLowLatencyMode(true);
	EXPECT_CALL(*g_mockAampConfig, GetConfigValue(eAAMPConfig_UnderflowDetectThresholdSec))
		.WillRepeatedly(Return(0.5)); // Set underflow threshold to 0.5 second
	// Simulate buffered duration just below the underflow threshold
	EXPECT_CALL(*g_mockStreamAbstractionAAMP_MPD, GetBufferedDuration())
		.WillOnce(Return(0.1));
	EXPECT_EQ(videoTrack.GetBufferStatus(), BUFFER_STATUS_RED);
}

/**
 * @brief When the chunk cache is not full, WaitForCachedFragmentChunkInjected returns
 * true immediately without waiting.
 */
TEST_F(MediaTrackTests, WaitForCachedFragmentChunkInjected_CacheHasSpace_ReturnsTrueImmediately)
{
	EXPECT_CALL(*g_mockAampConfig, GetConfigValue(eAAMPConfig_MaxFragmentCached))
		.WillRepeatedly(Return(1));
	EXPECT_CALL(*g_mockAampConfig, GetConfigValue(eAAMPConfig_MaxFragmentChunkCached))
		.WillRepeatedly(Return(1));
	EXPECT_CALL(*g_mockPrivateInstanceAAMP, GetLLDashChunkMode()).WillRepeatedly(Return(false));

	TestableMediaTrack videoTrack{eTRACK_VIDEO, mPrivateInstanceAAMP, "video", mStreamAbstractionAAMP_MPD};

	// Cache is empty (not full), so no wait is entered and the function returns true.
	ASSERT_LT(videoTrack.numberOfFragmentChunksCached,
			  static_cast<int>(videoTrack.GetCachedFragmentChunksSize()));
	EXPECT_TRUE(videoTrack.WaitForCachedFragmentChunkInjected(0));
}

/**
 * @brief When the chunk cache is full and the timeout expires with no signal,
 * WaitForCachedFragmentChunkInjected returns false.
 */
TEST_F(MediaTrackTests, WaitForCachedFragmentChunkInjected_TimeoutWithCacheFull_ReturnsFalse)
{
	EXPECT_CALL(*g_mockAampConfig, GetConfigValue(eAAMPConfig_MaxFragmentCached))
		.WillRepeatedly(Return(1));
	EXPECT_CALL(*g_mockAampConfig, GetConfigValue(eAAMPConfig_MaxFragmentChunkCached))
		.WillRepeatedly(Return(1));
	EXPECT_CALL(*g_mockPrivateInstanceAAMP, GetLLDashChunkMode()).WillRepeatedly(Return(false));

	TestableMediaTrack videoTrack{eTRACK_VIDEO, mPrivateInstanceAAMP, "video", mStreamAbstractionAAMP_MPD};

	// Fill the cache to capacity so WaitForCachedFragmentChunkInjected will block.
	videoTrack.numberOfFragmentChunksCached = static_cast<int>(videoTrack.GetCachedFragmentChunksSize());

	// No signal is ever fired; the short timeout must cause a false return.
	EXPECT_FALSE(videoTrack.WaitForCachedFragmentChunkInjected(50 /*ms*/));
}

/**
 * @brief Exercises the "signaled but still full" branch (streamabstraction.cpp ~R725-R729):
 * the cache is full, the condition variable is signaled without the abort flag being set
 * and without numberOfFragmentChunksCached being decremented.
 * WaitForCachedFragmentChunkInjected must return false because the cache is still full
 * after the wakeup.
 */
TEST_F(MediaTrackTests, WaitForCachedFragmentChunkInjected_SignaledButStillFull_ReturnsFalse)
{
	EXPECT_CALL(*g_mockAampConfig, GetConfigValue(eAAMPConfig_MaxFragmentCached))
		.WillRepeatedly(Return(1));
	EXPECT_CALL(*g_mockAampConfig, GetConfigValue(eAAMPConfig_MaxFragmentChunkCached))
		.WillRepeatedly(Return(1));
	EXPECT_CALL(*g_mockPrivateInstanceAAMP, GetLLDashChunkMode()).WillRepeatedly(Return(false));

	TestableMediaTrack videoTrack{eTRACK_VIDEO, mPrivateInstanceAAMP, "video", mStreamAbstractionAAMP_MPD};

	// Fill the cache to capacity so WaitForCachedFragmentChunkInjected will block.
	videoTrack.numberOfFragmentChunksCached = static_cast<int>(videoTrack.GetCachedFragmentChunksSize());

	// From a background thread: signal the CV without draining the cache and without
	// setting abort — this mirrors the scenario introduced at ~R725-R729 where the
	// caller is woken up spuriously or by an unrelated event.
	std::thread signalThread([&videoTrack]() {
		std::this_thread::sleep_for(std::chrono::milliseconds(50));
		// AbortWaitForCachedFragmentChunk signals fragmentChunkInjected but does NOT
		// decrement numberOfFragmentChunksCached or set the abort flag.
		videoTrack.AbortWaitForCachedFragmentChunk();
	});

	// Must return false: was signaled, abort is clear, but cache is still full.
	bool result = videoTrack.WaitForCachedFragmentChunkInjected(5000 /*ms*/);
	signalThread.join();

	EXPECT_FALSE(result);
}

/**
 * @brief When the condition variable is signaled and numberOfFragmentChunksCached is
 * decremented before the caller wakes up, WaitForCachedFragmentChunkInjected returns true.
 * This is the complementary positive case confirming the non-full path still works.
 */
TEST_F(MediaTrackTests, WaitForCachedFragmentChunkInjected_SignaledAndCacheCleared_ReturnsTrue)
{
	EXPECT_CALL(*g_mockAampConfig, GetConfigValue(eAAMPConfig_MaxFragmentCached))
		.WillRepeatedly(Return(1));
	EXPECT_CALL(*g_mockAampConfig, GetConfigValue(eAAMPConfig_MaxFragmentChunkCached))
		.WillRepeatedly(Return(1));
	EXPECT_CALL(*g_mockPrivateInstanceAAMP, GetLLDashChunkMode()).WillRepeatedly(Return(false));

	TestableMediaTrack videoTrack{eTRACK_VIDEO, mPrivateInstanceAAMP, "video", mStreamAbstractionAAMP_MPD};

	// Fill the cache to capacity so WaitForCachedFragmentChunkInjected will block.
	videoTrack.numberOfFragmentChunksCached = static_cast<int>(videoTrack.GetCachedFragmentChunksSize());

	// From a background thread: simulate an injector consuming a slot, then signal.
	std::thread signalThread([&videoTrack]() {
		std::this_thread::sleep_for(std::chrono::milliseconds(50));
		// Decrement the cache count (simulating a completed injection), then notify.
		videoTrack.numberOfFragmentChunksCached--;
		videoTrack.AbortWaitForCachedFragmentChunk();
	});

	// Must return true: was signaled and cache now has a free slot.
	bool result = videoTrack.WaitForCachedFragmentChunkInjected(5000 /*ms*/);
	signalThread.join();

	EXPECT_TRUE(result);
}