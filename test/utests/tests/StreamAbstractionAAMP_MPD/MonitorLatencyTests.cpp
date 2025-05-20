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
#include <gmock/gmock.h>
#include <chrono>
#include "priv_aamp.h"
#include "AampConfig.h"
#include "AampScheduler.h"
#include "AampLogManager.h"
#include "fragmentcollector_mpd.h"
#include "MediaStreamContext.h"
#include "MockAampConfig.h"
#include "MockAampUtils.h"
#include "MockAampGstPlayer.h"
#include "MockPrivateInstanceAAMP.h"
#include "MockMediaStreamContext.h"
#include "MockAampMPDDownloader.h"
#include "MockAampStreamSinkManager.h"
#include "MockAdManager.h"

using ::testing::_;
using ::testing::AnyNumber;
using ::testing::Invoke;
using ::testing::NiceMock;
using ::testing::Return;
using ::testing::SetArgReferee;
using ::testing::StrictMock;
using ::testing::WithArgs;
using ::testing::WithoutArgs;
using ::testing::DoAll;

/**
 * @brief MonitorLatencyParams define the varying parameter is different test cases.
 */
struct MonitorLatencyParams {
	int currPos; /** Current position to calculate latency  */
	int endPos; /** Current end position ; latency = end pos - curr pos */
	double currPlaybackRate; /** Current playback rate */
	double expPlaybackRate; /** Expected playback rate as per test */
	int configBuffer; /** Buffer value */
	AdState currAdState; /** Current state of Ad playback */
	int expectedSetPlayBackRateCalls; /** This is indicate number of times SetplaybackRate call ; here only 0 or 1 */
};

/**
 * @brief LinearTests tests common base class.
 */
class MonitorLatencyTests : public ::testing::TestWithParam<MonitorLatencyParams> 
{
protected:
	class TestableStreamAbstractionAAMP_MPD : public StreamAbstractionAAMP_MPD
	{
	public:
		double mBufferDuration;
		// Constructor to pass parameters to the base class constructor
		TestableStreamAbstractionAAMP_MPD(PrivateInstanceAAMP *aamp,
										  double seekpos, float rate)
			: StreamAbstractionAAMP_MPD(aamp, seekpos, rate)
		{
			mBufferDuration = 0;
		}

		AAMPStatusType InvokeUpdateTrackInfo(bool modifyDefaultBW, bool resetTimeLineIndex)
		{
			return UpdateTrackInfo(modifyDefaultBW, resetTimeLineIndex);
		}

		AAMPStatusType InvokeUpdateMPD(bool init)
		{
			return UpdateMPD(init);
		}

		void InvokeFetcherLoop()
		{
			FetcherLoop();
		}

		int GetCurrentPeriodIdx()
		{
			return mCurrentPeriodIdx;
		}

		int GetIteratorPeriodIdx()
		{
			return mIterPeriodIndex;
		}

		void IncrementIteratorPeriodIdx()
		{
			mIterPeriodIndex++;
		}

		void DecrementIteratorPeriodIdx()
		{
			mIterPeriodIndex--;
		}

		void IncrementCurrentPeriodIdx()
		{
			mCurrentPeriodIdx++;
		}

		void SetIteratorPeriodIdx(int idx)
		{
			mIterPeriodIndex = idx;
		}

		bool InvokeSelectSourceOrAdPeriod(bool &periodChanged, bool &mpdChanged, bool &adStateChanged, bool &waitForAdBreakCatchup, bool &requireStreamSelection, std::string &currentPeriodId)
		{
			return SelectSourceOrAdPeriod(periodChanged, mpdChanged, adStateChanged, waitForAdBreakCatchup, requireStreamSelection, currentPeriodId);
		}

		bool InvokeIndexSelectedPeriod(bool &periodChanged, bool &adStateChanged, bool &requireStreamSelection, std::string &currentPeriodId)
		{
			return IndexSelectedPeriod(periodChanged, adStateChanged, requireStreamSelection, currentPeriodId);
		}

		bool InvokeCheckEndOfStream(bool &waitForAdBreakCatchup)
		{
			return CheckEndOfStream(waitForAdBreakCatchup);
		}

		void InvokeDetectDiscontinuityAndFetchInit(bool &periodChanged, uint64_t nextSegTime = 0)
		{
			DetectDiscontinuityAndFetchInit(periodChanged, nextSegTime);
		}

		AAMPStatusType IndexNewMPDDocument(bool updateTrackInfo = false)
		{
			return StreamAbstractionAAMP_MPD::IndexNewMPDDocument(updateTrackInfo);
		}

		void SetCurrentPeriod(dash::mpd::IPeriod *period)
		{
			mCurrentPeriod = period;
		}

		dash::mpd::IPeriod *GetCurrentPeriod()
		{
			return mCurrentPeriod;
		}

		class PrivateCDAIObjectMPD *GetCDAIObject()
		{
			return mCdaiObject;
		}

		void SetNumberOfTracks(int numTracks)
		{
			mNumberOfTracks = numTracks;
		}

		double GetBufferedDuration()
		{
			return mBufferDuration;
		}
	};

	PrivateInstanceAAMP *mPrivateInstanceAAMP;
	TestableStreamAbstractionAAMP_MPD *mStreamAbstractionAAMP_MPD;
	CDAIObject *mCdaiObj;
	using BoolConfigSettings = std::map<AAMPConfigSettingBool, bool>;
	using IntConfigSettings = std::map<AAMPConfigSettingInt, int>;
	using FloatConfigSettings = std::map<AAMPConfigSettingFloat, double>;

	/** @brief Boolean AAMP configuration settings. */
	const BoolConfigSettings mDefaultBoolConfigSettings =
		{
			{eAAMPConfig_EnableMediaProcessor, true},
			{eAAMPConfig_EnableCMCD, false},
			{eAAMPConfig_BulkTimedMetaReport, false},
			{eAAMPConfig_BulkTimedMetaReportLive, false},
			{eAAMPConfig_EnableSCTE35PresentationTime, false},
			{eAAMPConfig_EnableClientDai, true},
			{eAAMPConfig_MatchBaseUrl, false},
			{eAAMPConfig_UseAbsoluteTimeline, false},
			{eAAMPConfig_DisableAC4, true},
			{eAAMPConfig_AudioOnlyPlayback, false},
			{eAAMPConfig_LimitResolution, false},
			{eAAMPConfig_Disable4K, false},
			{eAAMPConfig_PersistHighNetworkBandwidth, false},
			{eAAMPConfig_PersistLowNetworkBandwidth, false},
			{eAAMPConfig_MidFragmentSeek, false},
			{eAAMPConfig_PropagateURIParam, true},
			{eAAMPConfig_DashParallelFragDownload, false},
			{eAAMPConfig_DisableATMOS, false},
			{eAAMPConfig_DisableEC3, false},
			{eAAMPConfig_DisableAC3, false},
			{eAAMPConfig_EnableLowLatencyDash, false},
			{eAAMPConfig_EnableIgnoreEosSmallFragment, false},
			{eAAMPConfig_EnablePTSReStamp, false},
			{eAAMPConfig_LocalTSBEnabled, false},
			{eAAMPConfig_EnableIFrameTrackExtract, false},
			{eAAMPConfig_EnableABR, true},
			{eAAMPConfig_MPDDiscontinuityHandling, true},
			{eAAMPConfig_MPDDiscontinuityHandlingCdvr, true},
			{eAAMPConfig_ForceMultiPeriodDiscontinuity, false},
			{eAAMPConfig_SuppressDecode, false},
			{eAAMPConfig_InterruptHandling, false},
			{eAAMPConfig_EnableLowLatencyCorrection, true},
			{eAAMPConfig_EnableLowLatencyDash, true} };

	BoolConfigSettings mBoolConfigSettings;

	/** @brief Integer AAMP configuration settings. */
	const IntConfigSettings mDefaultIntConfigSettings =
		{
			{eAAMPConfig_ABRCacheLength, DEFAULT_ABR_CACHE_LENGTH},
			{eAAMPConfig_MaxABRNWBufferRampUp, AAMP_HIGH_BUFFER_BEFORE_RAMPUP},
			{eAAMPConfig_MinABRNWBufferRampDown, AAMP_LOW_BUFFER_BEFORE_RAMPDOWN},
			{eAAMPConfig_ABRNWConsistency, DEFAULT_ABR_NW_CONSISTENCY_CNT},
			{eAAMPConfig_RampDownLimit, -1},
			{eAAMPConfig_MaxFragmentCached, DEFAULT_CACHED_FRAGMENTS_PER_TRACK},
			{eAAMPConfig_PrePlayBufferCount, DEFAULT_PREBUFFER_COUNT},
			{eAAMPConfig_VODTrickPlayFPS, TRICKPLAY_VOD_PLAYBACK_FPS},
			{eAAMPConfig_ABRBufferCounter, DEFAULT_ABR_BUFFER_COUNTER},
			{eAAMPConfig_StallTimeoutMS, DEFAULT_STALL_DETECTION_TIMEOUT},
			{eAAMPConfig_AdFulfillmentTimeout, DEFAULT_AD_FULFILLMENT_TIMEOUT},
			{eAAMPConfig_AdFulfillmentTimeoutMax, MAX_AD_FULFILLMENT_TIMEOUT},
			{eAAMPConfig_LatencyMonitorDelay, DEFAULT_LATENCY_MONITOR_DELAY},
			{eAAMPConfig_LatencyMonitorInterval, AAMP_LLD_LATENCY_MONITOR_INTERVAL },
			};

	IntConfigSettings mIntConfigSettings;

	const FloatConfigSettings  mDefaultFloatConfigSettings =
	{
		{eAAMPConfig_MinLatencyCorrectionPlaybackRate, DEFAULT_MIN_RATE_CORRECTION_SPEED },     /**< Latency adjust/buffer correction min playback rate*/
		{eAAMPConfig_MaxLatencyCorrectionPlaybackRate, DEFAULT_MAX_RATE_CORRECTION_SPEED },     /**< Latency correction max playback rate*/
		{eAAMPConfig_NormalLatencyCorrectionPlaybackRate, DEFAULT_NORMAL_RATE_CORRECTION_SPEED},    /**< Nomral playback rate for LLD stream*/
		{eAAMPConfig_LowLatencyMinBuffer, DEFAULT_MIN_BUFFER_LOW_LATENCY },
		{eAAMPConfig_LowLatencyTargetBuffer, DEFAULT_TARGET_BUFFER_LOW_LATENCY}
	};
	FloatConfigSettings mFloatConfigSettings;

	void SetUp()
	{
		if (gpGlobalConfig == nullptr)
		{
			gpGlobalConfig = new AampConfig();
		}

		mPrivateInstanceAAMP = new PrivateInstanceAAMP(gpGlobalConfig);
		mPrivateInstanceAAMP->mIsDefaultOffset = true;

		g_mockAampConfig = new NiceMock<MockAampConfig>();

		g_mockAampGstPlayer = new MockAAMPGstPlayer(mPrivateInstanceAAMP);

		mPrivateInstanceAAMP->mIsDefaultOffset = true;

		g_mockPrivateInstanceAAMP = new StrictMock<MockPrivateInstanceAAMP>();

		g_mockMediaStreamContext = new StrictMock<MockMediaStreamContext>();

		g_mockAampStreamSinkManager = new NiceMock<MockAampStreamSinkManager>();

		g_MockPrivateCDAIObjectMPD = new NiceMock<MockPrivateCDAIObjectMPD>();

		mStreamAbstractionAAMP_MPD = nullptr;
		mBoolConfigSettings = mDefaultBoolConfigSettings;
		mIntConfigSettings = mDefaultIntConfigSettings;
		mFloatConfigSettings = mDefaultFloatConfigSettings;
	}

	void TearDown()
	{
		if (mStreamAbstractionAAMP_MPD)
		{
			delete mStreamAbstractionAAMP_MPD;
			mStreamAbstractionAAMP_MPD = nullptr;
		}

		delete mPrivateInstanceAAMP;
		mPrivateInstanceAAMP = nullptr;

		delete mCdaiObj;
		mCdaiObj = nullptr;

		delete gpGlobalConfig;
		gpGlobalConfig = nullptr;

		delete g_mockAampConfig;
		g_mockAampConfig = nullptr;

		delete g_mockAampGstPlayer;
		g_mockAampGstPlayer = nullptr;

		delete g_mockPrivateInstanceAAMP;
		g_mockPrivateInstanceAAMP = nullptr;

		delete g_mockMediaStreamContext;
		g_mockMediaStreamContext = nullptr;

		delete g_mockAampStreamSinkManager;
		g_mockAampStreamSinkManager = nullptr;

		delete g_MockPrivateCDAIObjectMPD;
		g_MockPrivateCDAIObjectMPD = nullptr;

	}

public:
	/**
	 * @brief Initialize the MPD instance
	 *
	 * This will:
	 *  - Download the manifest.
	 *  - Parse the manifest.
	 *  - Cache the initialization fragments.
	 *
	 * @param[in] manifest Manifest data
	 * @param[in] tuneType Optional tune type
	 * @param[in] seekPos Optional seek position in seconds
	 * @param[in] rate Optional play rate
	 * @return eAAMPSTATUS_OK on success or another value on error
	 */
	AAMPStatusType InitializeMPD(TuneType tuneType = TuneType::eTUNETYPE_NEW_NORMAL, double seekPos = 0.0, float rate = AAMP_NORMAL_PLAY_RATE)
	{
		/* Setup configuration mock. */
		for (const auto &b : mBoolConfigSettings)
		{
			EXPECT_CALL(*g_mockAampConfig, IsConfigSet(b.first))
				.Times(AnyNumber())
				.WillRepeatedly(Return(b.second));
		}

		for (const auto &i : mIntConfigSettings)
		{
			EXPECT_CALL(*g_mockAampConfig, GetConfigValue(i.first))
				.Times(AnyNumber())
				.WillRepeatedly(Return(i.second));
		}

		for (const auto &i : mFloatConfigSettings)
		{
			EXPECT_CALL(*g_mockAampConfig, GetConfigValue(i.first))
				.Times(AnyNumber())
				.WillRepeatedly(Return(i.second));
		}

		/* Create MPD instance. */
		mStreamAbstractionAAMP_MPD = new TestableStreamAbstractionAAMP_MPD(mPrivateInstanceAAMP, seekPos, rate);
		mCdaiObj = new CDAIObjectMPD(mPrivateInstanceAAMP);
		mStreamAbstractionAAMP_MPD->SetCDAIObject(mCdaiObj);
		return eAAMPSTATUS_OK;
	}
};

/**
 * @brief LatencyChangeTest tests.
 *
 * This is the normal case of latency correction if latency is high or low, with or without enough buffer
 * inside and outside AD break, and with different current playback rate; Only Rate change expected scenarios 
 */
TEST_P(MonitorLatencyTests, LatencyChangeExpectedScenarios)
{
	const auto& params = GetParam(); /*Retrieve the parameter values */

	// Initialize the test case variables with parameterized values
	std::string fragmentUrl;
	AAMPStatusType status;
	mPrivateInstanceAAMP->rate = 1.0;
	status = InitializeMPD();
	EXPECT_EQ(status, eAAMPSTATUS_OK);

	EXPECT_CALL(*g_mockPrivateInstanceAAMP, GetState())
		.Times(AnyNumber())
		.WillRepeatedly(Return(eSTATE_PLAYING));

	EXPECT_CALL(*g_mockPrivateInstanceAAMP, DownloadsAreEnabled())
		.Times(AnyNumber())
		.WillRepeatedly(Return(true));

	EXPECT_CALL(*g_mockPrivateInstanceAAMP, GetPositionMs())
		.Times(AnyNumber())
		.WillRepeatedly(Return(params.currPos));

	EXPECT_CALL(*g_mockPrivateInstanceAAMP, DurationFromStartOfPlaybackMs())
		.Times(AnyNumber())
		.WillRepeatedly(Return(params.endPos));

	EXPECT_CALL(*g_mockPrivateInstanceAAMP, GetLLDashAdjustSpeed())
		.WillOnce(Return(true))
		.WillOnce(Return(true))
		.WillOnce(Return(true))
		.WillOnce(Return(false));

	EXPECT_CALL(*g_mockPrivateInstanceAAMP, GetLLDashCurrentPlayBackRate())
		.WillOnce(Return(params.currPlaybackRate))
		.WillOnce(Return(params.currPlaybackRate));

	EXPECT_CALL(*g_mockAampStreamSinkManager, GetStreamSink(_))
		.WillRepeatedly(Return(g_mockAampGstPlayer));

	EXPECT_CALL(*g_mockAampGstPlayer, SetPlayBackRate(params.expPlaybackRate))
		.Times(params.expectedSetPlayBackRateCalls)
		.WillRepeatedly(Return(true));

	AampLLDashServiceData lLDashServiceData;
	lLDashServiceData.lowLatencyMode = true;
	lLDashServiceData.availabilityTimeOffset = 2; //dummy
	lLDashServiceData.targetLatency = DEFAULT_TARGET_LOW_LATENCY;
	lLDashServiceData.minLatency = DEFAULT_MIN_LOW_LATENCY;
	lLDashServiceData.maxLatency = DEFAULT_MAX_LOW_LATENCY;
	lLDashServiceData.minPlaybackRate = DEFAULT_MIN_RATE_CORRECTION_SPEED;
	lLDashServiceData.maxPlaybackRate = DEFAULT_MAX_RATE_CORRECTION_SPEED;
	lLDashServiceData.isSegTimeLineBased = true;
	lLDashServiceData.fragmentDuration = 2; //dummy
	mPrivateInstanceAAMP->SetLLDashServiceData(lLDashServiceData);

	mStreamAbstractionAAMP_MPD->mBufferDuration = params.configBuffer;

	auto cdaiObj = mStreamAbstractionAAMP_MPD->GetCDAIObject();
	cdaiObj->mAdState = params.currAdState;
	mStreamAbstractionAAMP_MPD->MonitorLatency();
}

/* Instantiate the test cases with various parameters 
* AdState::OUTSIDE_ADBREAK and IN_ADBREAK_AD_NOT_PLAYING means outside ad, rest all consider as inside AD
* buffer value - 2 means min buffer , 6 enough buffer
* latency calculated by endPos - currPos ( ex:- 100 - 90 = 10 sec)
*/
INSTANTIATE_TEST_SUITE_P(
	LatencyChangeExpectedConfigurations,
	MonitorLatencyTests,
	::testing::ValuesIn(std::vector<MonitorLatencyParams>{
		/** Cases of normal to max expected to call */
		/** Case 1: Latency - high (10), current rate is normal, available buffer is enough (6), ad state is OUTSIDE_ADBREAK, 
		 * Expected to call setPlaybackRate with max rate once
		 */
		{90, 100, DEFAULT_NORMAL_RATE_CORRECTION_SPEED, DEFAULT_MAX_RATE_CORRECTION_SPEED, 6, AdState::OUTSIDE_ADBREAK, 1},

		/** Cases of normal to max not expected to call*/
		/** Case 2: Latency - high (10), current rate is normal, buffer is sufficient (6), ad state is IN_ADBREAK_AD_PLAYING,
		 * Not expected to call setPlaybackRate */
		{90, 100, DEFAULT_NORMAL_RATE_CORRECTION_SPEED, DEFAULT_MAX_RATE_CORRECTION_SPEED, 6, AdState::IN_ADBREAK_AD_PLAYING, 0},
		/** Case 3: Latency - high (10), current rate is normal, buffer is low (2), ad state is OUTSIDE_ADBREAK,
		 * Not expected to call setPlaybackRate */
		{90, 100, DEFAULT_NORMAL_RATE_CORRECTION_SPEED, DEFAULT_MAX_RATE_CORRECTION_SPEED, 2, AdState::OUTSIDE_ADBREAK, 0},
		/** Case 4: Latency - high (10), current rate is normal, buffer is low (2), ad state is IN_ADBREAK_AD_PLAYING,
		 * Not expected to call setPlaybackRate */
		{90, 100, DEFAULT_NORMAL_RATE_CORRECTION_SPEED, DEFAULT_MAX_RATE_CORRECTION_SPEED, 2, AdState::IN_ADBREAK_AD_PLAYING, 0},
		/** Case 5: Latency - noormal (6), current rate is normal, buffer is sufficient (6), ad state is OUTSIDE_ADBREAK,
		 * Not expected to call setPlaybackRate */
		{94, 100, DEFAULT_NORMAL_RATE_CORRECTION_SPEED, DEFAULT_MAX_RATE_CORRECTION_SPEED, 6, AdState::OUTSIDE_ADBREAK, 0},
		/** Case 6: Latency - normal (6), current rate is normal, buffer is sufficient (6), ad state is IN_ADBREAK_AD_PLAYING,
		 * Not expected to call setPlaybackRate */
		{94, 100, DEFAULT_NORMAL_RATE_CORRECTION_SPEED, DEFAULT_MAX_RATE_CORRECTION_SPEED, 6, AdState::IN_ADBREAK_AD_PLAYING, 0},
		/** Case 7: Latency - normal (6), current rate is normal, buffer is low (2), ad state is OUTSIDE_ADBREAK,
		 * Not expected to call setPlaybackRate */
		{94, 100, DEFAULT_NORMAL_RATE_CORRECTION_SPEED, DEFAULT_MAX_RATE_CORRECTION_SPEED, 2, AdState::OUTSIDE_ADBREAK, 0},
		/** Case 8: Latency - normal (6), current rate is normal, buffer is low (2), ad state is IN_ADBREAK_AD_PLAYING,
		 * Not expected to call setPlaybackRate */
		{94, 100, DEFAULT_NORMAL_RATE_CORRECTION_SPEED, DEFAULT_MAX_RATE_CORRECTION_SPEED, 2, AdState::IN_ADBREAK_AD_PLAYING, 0},

		/** Cases of high to normal expected to call*/
		/** Case 9: Latency - high (10), current rate is max, buffer is sufficient (6), ad state is IN_ADBREAK_AD_PLAYING,
		 * Expected to call setPlaybackRate with normal rate once */
		{90, 100, DEFAULT_MAX_RATE_CORRECTION_SPEED, DEFAULT_NORMAL_RATE_CORRECTION_SPEED, 6, AdState::IN_ADBREAK_AD_PLAYING, 1},
		/** Case 10: Latency - normal (5), current rate is max, buffer is sufficient (6), ad state is IN_ADBREAK_AD_PLAYING,
		 * Expected to call setPlaybackRate with normal rate once */
		{95, 100, DEFAULT_MAX_RATE_CORRECTION_SPEED, DEFAULT_NORMAL_RATE_CORRECTION_SPEED, 6, AdState::IN_ADBREAK_AD_PLAYING, 1},
		/** Case 11: Latency - normal (5), current rate is max, buffer is sufficient (6), ad state is OUTSIDE_ADBREAK,
		 * Expected to call setPlaybackRate with normal rate once */
		{95, 100, DEFAULT_MAX_RATE_CORRECTION_SPEED, DEFAULT_NORMAL_RATE_CORRECTION_SPEED, 6, AdState::OUTSIDE_ADBREAK, 1},
		/** Case 12: Latency - high (10), current rate is max, buffer is low (2), ad state is IN_ADBREAK_AD_PLAYING,
		 * Expected to call setPlaybackRate with normal rate once */
		{90, 100, DEFAULT_MAX_RATE_CORRECTION_SPEED, DEFAULT_NORMAL_RATE_CORRECTION_SPEED, 2, AdState::IN_ADBREAK_AD_PLAYING, 1},
		/** Case 13: Latency - high (10), current rate is max, buffer is low (2), ad state is OUTSIDE_ADBREAK,
		 * Expected to call setPlaybackRate with normal rate once */
		{90, 100, DEFAULT_MAX_RATE_CORRECTION_SPEED, DEFAULT_NORMAL_RATE_CORRECTION_SPEED, 2, AdState::OUTSIDE_ADBREAK, 1},

		/** Cases of high to normal not expected to call*/
		/** Case 14: Latency - high (10), current rate is max, buffer is sufficient (6), ad state is OUTSIDE_ADBREAK,
		 * Not expected to call setPlaybackRate */
		{90, 100, DEFAULT_MAX_RATE_CORRECTION_SPEED, DEFAULT_NORMAL_RATE_CORRECTION_SPEED, 6, AdState::OUTSIDE_ADBREAK, 0},

		/** Cases of normal to low not expected to call */
		/** Case 15: Latency - low (2), current rate is normal, buffer is sufficient (6), ad state is IN_ADBREAK_AD_PLAYING,
		 * Not expected to call setPlaybackRate */
		{98, 100, DEFAULT_NORMAL_RATE_CORRECTION_SPEED, DEFAULT_MIN_RATE_CORRECTION_SPEED, 6, AdState::IN_ADBREAK_AD_PLAYING, 0},
		/** Case 16: Latency - low (2), current rate is normal, buffer is low (2), ad state is IN_ADBREAK_AD_PLAYING,
		 * Not expected to call setPlaybackRate */
		{98, 100, DEFAULT_NORMAL_RATE_CORRECTION_SPEED, DEFAULT_MIN_RATE_CORRECTION_SPEED, 2, AdState::IN_ADBREAK_AD_PLAYING, 0},

		/** Cases of normal to low expected to call */
		/** Case 17: Latency - low (2), current rate is normal, buffer is sufficient (6), ad state is OUTSIDE_ADBREAK,
		 * Expected to call setPlaybackRate with min rate once */
		{98, 100, DEFAULT_NORMAL_RATE_CORRECTION_SPEED, DEFAULT_MIN_RATE_CORRECTION_SPEED, 6, AdState::OUTSIDE_ADBREAK, 1},
		/** Case 18: Latency - low (2), current rate is normal, buffer is low (2), ad state is OUTSIDE_ADBREAK,
		 * Expected to call setPlaybackRate with min rate once */
		{98, 100, DEFAULT_NORMAL_RATE_CORRECTION_SPEED, DEFAULT_MIN_RATE_CORRECTION_SPEED, 2, AdState::OUTSIDE_ADBREAK, 1},

		/*cases of low to normal expected to call*/
		/** Case 19: Latency - low (2), current rate is min, buffer is sufficient (6), ad state is IN_ADBREAK_AD_PLAYING,
		 * Expected to call setPlaybackRate with normal rate once */
		{98, 100, DEFAULT_MIN_RATE_CORRECTION_SPEED, DEFAULT_NORMAL_RATE_CORRECTION_SPEED, 6, AdState::IN_ADBREAK_AD_PLAYING, 1},
		/** Case 20: Latency - low (2), current rate is min, buffer is low (2), ad state is IN_ADBREAK_AD_PLAYING,
		 * Expected to call setPlaybackRate with normal rate once */
		{98, 100, DEFAULT_MIN_RATE_CORRECTION_SPEED, DEFAULT_NORMAL_RATE_CORRECTION_SPEED, 2, AdState::IN_ADBREAK_AD_PLAYING, 1},
		/** Case 21: Latency - normal (7), current rate is min, buffer is sufficient (6), ad state is IN_ADBREAK_AD_PLAYING,
		 * Expected to call setPlaybackRate with normal rate once */
		{93, 100, DEFAULT_MIN_RATE_CORRECTION_SPEED, DEFAULT_NORMAL_RATE_CORRECTION_SPEED, 6, AdState::IN_ADBREAK_AD_PLAYING, 1},
		/** Case 22: Latency - normal (7), current rate is min, buffer is low (2), ad state is IN_ADBREAK_AD_PLAYING,
		 * Expected to call setPlaybackRate with normal rate once */
		{93, 100, DEFAULT_MIN_RATE_CORRECTION_SPEED, DEFAULT_NORMAL_RATE_CORRECTION_SPEED, 2, AdState::IN_ADBREAK_AD_PLAYING, 1},
		/** Case 23: Latency - normal (7), current rate is min, buffer is sufficient (6), ad state is OUTSIDE_ADBREAK,
		 * Expected to call setPlaybackRate with normal rate once */
		{93, 100, DEFAULT_MIN_RATE_CORRECTION_SPEED, DEFAULT_NORMAL_RATE_CORRECTION_SPEED, 6, AdState::OUTSIDE_ADBREAK, 1},

		/*cases of low to normal not expected to call*/
		/** Case 24: Latency - normal (7), current rate is min, buffer is low (2), ad state is OUTSIDE_ADBREAK,
		 * Not expected to call setPlaybackRate */
		{93, 100, DEFAULT_MIN_RATE_CORRECTION_SPEED, DEFAULT_NORMAL_RATE_CORRECTION_SPEED, 2, AdState::OUTSIDE_ADBREAK, 0},
		/** Case 25: Latency - low (2), current rate is min, buffer is low (2), ad state is OUTSIDE_ADBREAK,
		 * Not expected to call setPlaybackRate */
		{98, 100, DEFAULT_MIN_RATE_CORRECTION_SPEED, DEFAULT_NORMAL_RATE_CORRECTION_SPEED, 2, AdState::OUTSIDE_ADBREAK, 0},
		/** Case 26: Latency - low (2), current rate is min, buffer is sufficient (6), ad state is OUTSIDE_ADBREAK,
		 * Not expected to call setPlaybackRate */
		{98, 100, DEFAULT_MIN_RATE_CORRECTION_SPEED, DEFAULT_NORMAL_RATE_CORRECTION_SPEED, 6, AdState::OUTSIDE_ADBREAK, 0}
	})
);
