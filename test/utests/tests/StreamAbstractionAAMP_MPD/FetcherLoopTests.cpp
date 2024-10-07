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

using ::testing::_;
using ::testing::AnyNumber;
using ::testing::Invoke;
using ::testing::NiceMock;
using ::testing::Return;
using ::testing::SetArgReferee;
using ::testing::StrictMock;
using ::testing::WithArgs;
using ::testing::WithoutArgs;

/**
 * @brief LinearTests tests common base class.
 */
class FetcherLoopTests : public testing::TestWithParam<double>
{
protected:
	class TestableStreamAbstractionAAMP_MPD : public StreamAbstractionAAMP_MPD
	{
	public:
		// Constructor to pass parameters to the base class constructor
		TestableStreamAbstractionAAMP_MPD(PrivateInstanceAAMP *aamp,
										  double seekpos, float rate)
			: StreamAbstractionAAMP_MPD(aamp, seekpos, rate)
		{
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
			FetcherLoopNew();
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

		bool InvokeSelectSourceOrAdPeriod(bool &periodChanged, bool &mpdChanged, bool &adStateChanged, bool &waitForAdBreakCatchup, bool &bmanifestupdate, bool &requireStreamSelection, std::string &currentPeriodId)
		{
			return SelectSourceOrAdPeriod(periodChanged, mpdChanged, adStateChanged, waitForAdBreakCatchup, bmanifestupdate, requireStreamSelection, currentPeriodId);
		}

		bool InvokeIndexSelectedPeriod(bool &periodChanged, bool &adStateChanged, bool &bmanifestupdate, bool &requireStreamSelection, std::string &currentPeriodId)
		{
			return IndexSelectedPeriod(periodChanged, adStateChanged, bmanifestupdate, requireStreamSelection, currentPeriodId);
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

		void SetNumberOfTracks(int numTracks)
		{
			mNumberOfTracks = numTracks;
		}
	};

	PrivateInstanceAAMP *mPrivateInstanceAAMP;
	TestableStreamAbstractionAAMP_MPD *mStreamAbstractionAAMP_MPD;
	CDAIObject *mCdaiObj;
	const char *mManifest;
	static constexpr const char *TEST_BASE_URL = "http://host/asset/";
	static constexpr const char *TEST_MANIFEST_URL = "http://host/asset/manifest.mpd";
	static constexpr const char *mVodManifest = R"(<?xml version="1.0" encoding="utf-8"?>
			<MPD xmlns="urn:mpeg:dash:schema:mpd:2011" availabilityStartTime="2023-01-01T00:00:00Z" maxSegmentDuration="PT2S" minBufferTime="PT4.000S" minimumUpdatePeriod="P100Y" profiles="urn:dvb:dash:profile:dvb-dash:2014,urn:dvb:dash:profile:dvb-dash:isoff-ext-live:2014" publishTime="2023-01-01T00:01:00Z" timeShiftBufferDepth="PT5M" type="static">
					<Period id="p0" start="PT0S">
							<AdaptationSet id="0" contentType="video">
									<Representation id="0" mimeType="video/mp4" codecs="avc1.640028" bandwidth="800000" width="640" height="360" frameRate="25">
											<SegmentTemplate timescale="2500" initialization="video_p0_init.mp4" media="video_p0_$Number$.m4s" startNumber="1">
													<SegmentTimeline>
															<S t="0" d="5000" r="14" />
													</SegmentTimeline>
											</SegmentTemplate>
									</Representation>
							</AdaptationSet>
					</Period>
					<Period id="p1" start="PT30S">
							<AdaptationSet id="1" contentType="video">
									<Representation id="0" mimeType="video/mp4" codecs="avc1.640028" bandwidth="800000" width="640" height="360" frameRate="25">
											<SegmentTemplate timescale="2500" initialization="video_p1_init.mp4" media="video_p1_$Number$.m4s" startNumber="16">
													<SegmentTimeline>
															<S t="0" d="5000" r="14" />
													</SegmentTimeline>
											</SegmentTemplate>
									</Representation>
							</AdaptationSet>
					</Period>
			</MPD>
			)";

	static constexpr const char *mLiveManifest = R"(<?xml version="1.0" encoding="utf-8"?>
				<MPD xmlns="urn:mpeg:dash:schema:mpd:2011" availabilityStartTime="2023-01-01T00:00:00Z" maxSegmentDuration="PT2S" minBufferTime="PT4.000S" minimumUpdatePeriod="P100Y" profiles="urn:dvb:dash:profile:dvb-dash:2014,urn:dvb:dash:profile:dvb-dash:isoff-ext-live:2014" publishTime="2023-01-01T00:01:00Z" timeShiftBufferDepth="PT5M" type="dynamic">
						<Period id="p0" start="PT0S">
								<AdaptationSet id="0" contentType="video">
										<Representation id="0" mimeType="video/mp4" codecs="avc1.640028" bandwidth="800000" width="640" height="360" frameRate="25">
												<SegmentTemplate timescale="2500" initialization="video_p0_init.mp4" media="video_p0_$Number$.m4s" startNumber="1">
														<SegmentTimeline>
																<S t="0" d="5000" r="14" />
														</SegmentTimeline>
												</SegmentTemplate>
										</Representation>
								</AdaptationSet>
						</Period>
						<Period id="p1" start="PT30S">
								<AdaptationSet id="1" contentType="video">
										<Representation id="0" mimeType="video/mp4" codecs="avc1.640028" bandwidth="800000" width="640" height="360" frameRate="25">
												<SegmentTemplate timescale="2500" initialization="video_p1_init.mp4" media="video_p1_$Number$.m4s" startNumber="1">
														<SegmentTimeline>
																<S t="0" d="5000" r="14" />
														</SegmentTimeline>
												</SegmentTemplate>
										</Representation>
								</AdaptationSet>
						</Period>
				</MPD>
				)";

	std::shared_ptr<ManifestDownloadResponse> mResponse = std::make_shared<ManifestDownloadResponse>();
	using BoolConfigSettings = std::map<AAMPConfigSettingBool, bool>;
	using IntConfigSettings = std::map<AAMPConfigSettingInt, int>;

	/** @brief Boolean AAMP configuration settings. */
	const BoolConfigSettings mDefaultBoolConfigSettings =
		{
			{eAAMPConfig_EnableMediaProcessor, true},
			{eAAMPConfig_EnableCMCD, false},
			{eAAMPConfig_BulkTimedMetaReport, false},
			{eAAMPConfig_EnableSCTE35PresentationTime, false},
			{eAAMPConfig_EnableClientDai, false},
			{eAAMPConfig_MatchBaseUrl, false},
			{eAAMPConfig_UseAbsoluteTimeline, false},
			{eAAMPConfig_DisableAC4, true},
			{eAAMPConfig_AudioOnlyPlayback, false},
			{eAAMPConfig_LimitResolution, false},
			{eAAMPConfig_Disable4K, false},
			{eAAMPConfig_PersistHighNetworkBandwidth, false},
			{eAAMPConfig_PersistLowNetworkBandwidth, false},
			{eAAMPConfig_MidFragmentSeek, false},
			{eAAMPConfig_PropogateURIParam, true},
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
			{eAAMPConfig_InterruptHandling, false}};

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
			{eAAMPConfig_StallTimeoutMS, DEFAULT_STALL_DETECTION_TIMEOUT}};

	IntConfigSettings mIntConfigSettings;

	void SetUp()
	{
		if (gpGlobalConfig == nullptr)
		{
			gpGlobalConfig = new AampConfig();
		}

		mPrivateInstanceAAMP = new PrivateInstanceAAMP(gpGlobalConfig);
		mPrivateInstanceAAMP->mIsDefaultOffset = true;

		g_mockAampConfig = new NiceMock<MockAampConfig>();

		g_mockAampUtils = nullptr;

		g_mockAampGstPlayer = new MockAAMPGstPlayer(mPrivateInstanceAAMP);

		mPrivateInstanceAAMP->mIsDefaultOffset = true;

		g_mockPrivateInstanceAAMP = new StrictMock<MockPrivateInstanceAAMP>();

		g_mockMediaStreamContext = new StrictMock<MockMediaStreamContext>();

		g_mockAampMPDDownloader = new StrictMock<MockAampMPDDownloader>();

		g_mockAampStreamSinkManager = new NiceMock<MockAampStreamSinkManager>();

		mStreamAbstractionAAMP_MPD = nullptr;

		mManifest = nullptr;
		// mResponse = nullptr;
		mBoolConfigSettings = mDefaultBoolConfigSettings;
		mIntConfigSettings = mDefaultIntConfigSettings;
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

		if (g_mockAampUtils)
		{
			delete g_mockAampUtils;
			g_mockAampUtils = nullptr;
		}

		delete g_mockAampConfig;
		g_mockAampConfig = nullptr;

		delete g_mockAampGstPlayer;
		g_mockAampGstPlayer = nullptr;

		delete g_mockPrivateInstanceAAMP;
		g_mockPrivateInstanceAAMP = nullptr;

		delete g_mockMediaStreamContext;
		g_mockMediaStreamContext = nullptr;

		delete g_mockAampMPDDownloader;
		g_mockAampMPDDownloader = nullptr;

		delete g_mockAampStreamSinkManager;
		g_mockAampStreamSinkManager = nullptr;

		mManifest = nullptr;
		mResponse = nullptr;
	}

public:
	void GetMPDFromManifest(std::shared_ptr<ManifestDownloadResponse> response)
	{
		dash::mpd::MPD *mpd = nullptr;
		std::string manifestStr = std::string(response->mMPDDownloadResponse->mDownloadData.begin(), response->mMPDDownloadResponse->mDownloadData.end());

		xmlTextReaderPtr reader = xmlReaderForMemory((char *)manifestStr.c_str(), (int)manifestStr.length(), NULL, NULL, 0);
		if (reader != NULL)
		{
			if (xmlTextReaderRead(reader))
			{
				response->mRootNode = MPDProcessNode(&reader, TEST_MANIFEST_URL);
				if (response->mRootNode != NULL)
				{
					mpd = response->mRootNode->ToMPD();
					if (mpd)
					{
						std::shared_ptr<dash::mpd::IMPD> tmp_ptr(mpd);
						response->mMPDInstance = tmp_ptr;
						response->GetMPDParseHelper()->Initialize(mpd);
					}
				}
			}
		}
		xmlFreeTextReader(reader);
	}

	/**
	 * @brief Get manifest helper method for MPDDownloader
	 *
	 * @param[in] remoteUrl Manfiest url
	 * @param[out] buffer Buffer containing manifest data
	 * @retval true on success
	 */
	std::shared_ptr<ManifestDownloadResponse> GetManifestForMPDDownloader()
	{
		if (!mResponse->mMPDInstance)
		{
			std::shared_ptr<ManifestDownloadResponse> response = std::make_shared<ManifestDownloadResponse>();
			response->mMPDStatus = AAMPStatusType::eAAMPSTATUS_OK;
			response->mMPDDownloadResponse->iHttpRetValue = 200;
			response->mMPDDownloadResponse->sEffectiveUrl = std::string(TEST_MANIFEST_URL);
			response->mMPDDownloadResponse->mDownloadData.assign((uint8_t *)mManifest, (uint8_t *)(mManifest + strlen(mManifest)));
			GetMPDFromManifest(response);
			mResponse = response;
		}
		return mResponse;
	}

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
	AAMPStatusType InitializeMPD(const char *manifest, TuneType tuneType = TuneType::eTUNETYPE_NEW_NORMAL, double seekPos = 0.0, float rate = AAMP_NORMAL_PLAY_RATE, bool isLive = false)
	{
		AAMPStatusType status;

		mManifest = manifest;

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

		/* Create MPD instance. */
		mStreamAbstractionAAMP_MPD = new TestableStreamAbstractionAAMP_MPD(mPrivateInstanceAAMP, seekPos, rate);
		mCdaiObj = new CDAIObjectMPD(mPrivateInstanceAAMP);
		mStreamAbstractionAAMP_MPD->SetCDAIObject(mCdaiObj);

		mPrivateInstanceAAMP->SetManifestUrl(TEST_MANIFEST_URL);

		/* Initialize MPD. */
		EXPECT_CALL(*g_mockPrivateInstanceAAMP, SetState(eSTATE_PREPARING));

		EXPECT_CALL(*g_mockPrivateInstanceAAMP, GetState(_))
			.Times(AnyNumber())
			.WillRepeatedly(SetArgReferee<0>(eSTATE_PREPARING));
		// For the time being return the same manifest again
		EXPECT_CALL(*g_mockAampMPDDownloader, GetManifest(_, _, _))
			.WillRepeatedly(WithoutArgs(Invoke(this, &FetcherLoopTests::GetManifestForMPDDownloader)));
		status = mStreamAbstractionAAMP_MPD->Init(tuneType);
		return status;
	}

	/**
	 * @brief Push next fragment helper method
	 *
	 * @param[in] trackType Media track type
	 */
	bool PushNextFragment(TrackType trackType)
	{
		MediaTrack *track = mStreamAbstractionAAMP_MPD->GetMediaTrack(trackType);
		EXPECT_NE(track, nullptr);

		MediaStreamContext *pMediaStreamContext = static_cast<MediaStreamContext *>(track);

		return mStreamAbstractionAAMP_MPD->PushNextFragment(pMediaStreamContext, 0);
	}
};

/**
 * @brief SelectSourceOrAdPeriod tests.
 *
 * The tests verify the SelectSourceOrAdPeriod method of StreamAbstractionAAMP_MPD in forward period
 * change scenarios.
 */
TEST_F(FetcherLoopTests, SelectSourceOrAdPeriodTests1)
{
	std::string fragmentUrl;
	AAMPStatusType status;
	mPrivateInstanceAAMP->rate = AAMP_NORMAL_PLAY_RATE;
	bool ret = false;
	/* Initialize MPD. The video initialization segment is cached. */
	fragmentUrl = std::string(TEST_BASE_URL) + std::string("video_p0_init.mp4");
	EXPECT_CALL(*g_mockMediaStreamContext, CacheFragment(fragmentUrl, _, _, _, _, true, _, _, _, _, _))
		.Times(1)
		.WillOnce(Return(true));
	status = InitializeMPD(mVodManifest);
	EXPECT_EQ(status, eAAMPSTATUS_OK);

	// Index the first period
	status = mStreamAbstractionAAMP_MPD->IndexNewMPDDocument(false);
	EXPECT_EQ(mStreamAbstractionAAMP_MPD->GetCurrentPeriodIdx(), 0);
	EXPECT_EQ(mStreamAbstractionAAMP_MPD->GetIteratorPeriodIdx(), 0);

	// Index the next period
	mStreamAbstractionAAMP_MPD->IncrementIteratorPeriodIdx();
	bool periodChanged = false;
	bool adStateChanged = false;
	bool waitForAdBreakCatchup = false;
	bool bmanifestupdate = false;
	bool requireStreamSelection = false;
	bool mpdChanged = false;
	std::string currentPeriodId = "p0";

	/*
	 * Test the scenario where period change happens
	 * The period is changed and requireStreamSelection is set to true
	 */
	ret = mStreamAbstractionAAMP_MPD->InvokeSelectSourceOrAdPeriod(periodChanged, mpdChanged, adStateChanged, waitForAdBreakCatchup, bmanifestupdate, requireStreamSelection, currentPeriodId);
	EXPECT_EQ(ret, true);
	EXPECT_EQ(requireStreamSelection, true);
	EXPECT_EQ(periodChanged, true);
	EXPECT_EQ(mStreamAbstractionAAMP_MPD->GetCurrentPeriodIdx(), mStreamAbstractionAAMP_MPD->GetIteratorPeriodIdx());
}

/**
 * @brief SelectSourceOrAdPeriod tests.
 *
 * The tests verify the SelectSourceOrAdPeriod method of StreamAbstractionAAMP_MPD in end of period
 * change scenarios.
 */
TEST_F(FetcherLoopTests, SelectSourceOrAdPeriodTests2)
{
	std::string fragmentUrl;
	AAMPStatusType status;
	mPrivateInstanceAAMP->rate = 1.0;
	bool ret = false;
	/* Initialize MPD. The video initialization segment is cached. */
	fragmentUrl = std::string(TEST_BASE_URL) + std::string("video_p1_init.mp4");
	EXPECT_CALL(*g_mockMediaStreamContext, CacheFragment(fragmentUrl, _, _, _, _, true, _, _, _, _, _))
		.Times(1)
		.WillOnce(Return(true));
	status = InitializeMPD(mVodManifest, eTUNETYPE_SEEK, 35);
	EXPECT_EQ(status, eAAMPSTATUS_OK);

	// Index the initial values
	status = mStreamAbstractionAAMP_MPD->IndexNewMPDDocument(false);
	EXPECT_EQ(mStreamAbstractionAAMP_MPD->GetCurrentPeriodIdx(), 1);
	mStreamAbstractionAAMP_MPD->SetIteratorPeriodIdx(mStreamAbstractionAAMP_MPD->GetCurrentPeriodIdx());

	// Index the next period, wait for the selection
	mStreamAbstractionAAMP_MPD->IncrementIteratorPeriodIdx();
	bool periodChanged = false;
	bool adStateChanged = false;
	bool waitForAdBreakCatchup = false;
	bool bmanifestupdate = false;
	bool requireStreamSelection = false;
	bool mpdChanged = false;
	std::string currentPeriodId = "p1";

	/*
	 * Test the scenario where period change happens, it is already at the boundary
	 * so no change in period
	 */
	ret = mStreamAbstractionAAMP_MPD->InvokeSelectSourceOrAdPeriod(periodChanged, mpdChanged, adStateChanged, waitForAdBreakCatchup, bmanifestupdate, requireStreamSelection, currentPeriodId);
	EXPECT_EQ(ret, false);
	EXPECT_EQ(requireStreamSelection, false);
	EXPECT_EQ(periodChanged, false);
}

/**
 * @brief IndexSelectedPeriod tests.
 *
 * The tests verify the live behaviour of IndexSelectedPeriod method of StreamAbstractionAAMP_MPD
 * when nothing selected.
 */
TEST_F(FetcherLoopTests, IndexSelectedPeriodTests1)
{
	std::string fragmentUrl;
	AAMPStatusType status;
	mPrivateInstanceAAMP->rate = 1.0;
	bool ret = false;

	/* Initialize MPD. The video initialization segment is cached. */
	fragmentUrl = std::string(TEST_BASE_URL) + std::string("video_p1_init.mp4");
	EXPECT_CALL(*g_mockMediaStreamContext, CacheFragment(fragmentUrl, _, _, _, _, true, _, _, _, _, _))
		.Times(1)
		.WillOnce(Return(true));
	status = InitializeMPD(mLiveManifest);
	EXPECT_EQ(status, eAAMPSTATUS_OK);

	// Testing Indexing behaviour of the period
	MediaStreamContext *pMediaStreamContext = static_cast<MediaStreamContext *>(mStreamAbstractionAAMP_MPD->GetMediaTrack(eTRACK_VIDEO));
	bool periodChanged = true;
	bool adStateChanged = false;
	bool waitForAdBreakCatchup = false;
	bool bmanifestupdate = false;
	bool requireStreamSelection = true;
	std::string currentPeriodId = "p1";

	/*
	 * Test the scenario where period index happens
	 * All the values are reset to default
	 */
	ret = mStreamAbstractionAAMP_MPD->InvokeIndexSelectedPeriod(periodChanged, adStateChanged, bmanifestupdate, requireStreamSelection, currentPeriodId);
	EXPECT_EQ(pMediaStreamContext->fragmentDescriptor.Time, 0);
	EXPECT_EQ(pMediaStreamContext->fragmentDescriptor.Number, 1);
	EXPECT_EQ(pMediaStreamContext->eos, false);
	EXPECT_EQ(pMediaStreamContext->fragmentOffset, 0);
	EXPECT_EQ(pMediaStreamContext->fragmentIndex, 0);
	EXPECT_EQ(ret, true);
}

/**
 * @brief IndexSelectedPeriod tests.
 *
 * The tests verify the IndexSelectedPeriod method of StreamAbstractionAAMP_MPD when period change happens.
 */
TEST_F(FetcherLoopTests, IndexSelectedPeriodTests2)
{
	std::string fragmentUrl;
	AAMPStatusType status;
	mPrivateInstanceAAMP->rate = 1.0;
	bool ret = false;
	/* Initialize MPD. The video initialization segment is cached. */
	fragmentUrl = std::string(TEST_BASE_URL) + std::string("video_p0_init.mp4");
	EXPECT_CALL(*g_mockMediaStreamContext, CacheFragment(fragmentUrl, _, _, _, _, true, _, _, _, _, _))
		.Times(1)
		.WillOnce(Return(true));
	status = InitializeMPD(mVodManifest, eTUNETYPE_SEEK, 15);
	EXPECT_EQ(status, eAAMPSTATUS_OK);

	status = mStreamAbstractionAAMP_MPD->IndexNewMPDDocument(false);

	MediaStreamContext *pMediaStreamContext = static_cast<MediaStreamContext *>(mStreamAbstractionAAMP_MPD->GetMediaTrack(eTRACK_VIDEO));
	EXPECT_EQ(mStreamAbstractionAAMP_MPD->GetCurrentPeriodIdx(), 0);
	// seek to 15s ends up in segment starting at epoch 1672531214
	EXPECT_EQ(pMediaStreamContext->fragmentTime, 1672531214.0);
	mStreamAbstractionAAMP_MPD->SetIteratorPeriodIdx(mStreamAbstractionAAMP_MPD->GetCurrentPeriodIdx());

	// Set current period as next period
	mStreamAbstractionAAMP_MPD->IncrementCurrentPeriodIdx();
	mStreamAbstractionAAMP_MPD->SetCurrentPeriod(mStreamAbstractionAAMP_MPD->GetMPD()->GetPeriods().at(1));
	bool periodChanged = true;
	bool adStateChanged = false;
	bool waitForAdBreakCatchup = false;
	bool bmanifestupdate = false;
	bool requireStreamSelection = true;
	std::string currentPeriodId = "p1";

	/*
	 * Test the scenario where period index happens
	 * New period start is indexed at 1672531230
	 */
	ret = mStreamAbstractionAAMP_MPD->InvokeIndexSelectedPeriod(periodChanged, adStateChanged, bmanifestupdate, requireStreamSelection, currentPeriodId);
	EXPECT_EQ(pMediaStreamContext->fragmentTime, 1672531230.0);
	EXPECT_EQ(ret, true);
}

/**
 * @brief DetectDiscotinuityAndFetchInit tests.
 *
 * The tests verify the DetectDiscotinuityAndFetchInit method of StreamAbstractionAAMP_MPD without discontinuity detection.
 */
TEST_F(FetcherLoopTests, DetectDiscotinuityAndFetchInitTests1)
{
	std::string fragmentUrl;
	AAMPStatusType status;
	mPrivateInstanceAAMP->rate = 1.0;
	bool ret = false;
	/* Initialize MPD. The video initialization segment is cached. */
	fragmentUrl = std::string(TEST_BASE_URL) + std::string("video_p0_init.mp4");
	EXPECT_CALL(*g_mockMediaStreamContext, CacheFragment(fragmentUrl, _, _, _, _, true, _, _, _, _, _))
		.Times(1)
		.WillOnce(Return(true));
	status = InitializeMPD(mVodManifest, eTUNETYPE_SEEK, 0);
	EXPECT_EQ(status, eAAMPSTATUS_OK);

	status = mStreamAbstractionAAMP_MPD->IndexNewMPDDocument(false);

	MediaStreamContext *pMediaStreamContext = static_cast<MediaStreamContext *>(mStreamAbstractionAAMP_MPD->GetMediaTrack(eTRACK_VIDEO));
	EXPECT_EQ(mStreamAbstractionAAMP_MPD->GetCurrentPeriodIdx(), 0);
	// seek to 0 ends up in segment starting at epoch 1672531200
	EXPECT_EQ(pMediaStreamContext->fragmentTime, 1672531200.0);

	// Change and index the next period,
	mStreamAbstractionAAMP_MPD->IncrementCurrentPeriodIdx();
	mStreamAbstractionAAMP_MPD->SetCurrentPeriod(mStreamAbstractionAAMP_MPD->GetMPD()->GetPeriods().at(1));
	mPrivateInstanceAAMP->SetIsPeriodChangeMarked(true);
	bool periodChanged = true;
	std::string currentPeriodId = "p1";
	mStreamAbstractionAAMP_MPD->InvokeUpdateTrackInfo(false, false);

	/* Test API to detect discontinuity and fetch the initialization segment
	 * for the next period.
	 * Test the period change (discontinuity) is not marked
	 */
	mStreamAbstractionAAMP_MPD->InvokeDetectDiscontinuityAndFetchInit(periodChanged);
	EXPECT_EQ(mPrivateInstanceAAMP->GetIsPeriodChangeMarked(), false);
}

/**
 * @brief DetectDiscotinuityAndFetchInit tests.
 *
 * The tests verify the DetectDiscotinuityAndFetchInit method  of StreamAbstractionAAMP_MPD with discontinuity process
 */
TEST_F(FetcherLoopTests, DetectDiscotinuityAndFetchInitTests2)
{
	std::string fragmentUrl;
	AAMPStatusType status;
	mPrivateInstanceAAMP->rate = 1.0;
	bool ret = false;
	/* Initialize MPD. The video initialization segment is cached. */
	fragmentUrl = std::string(TEST_BASE_URL) + std::string("video_p0_init.mp4");
	EXPECT_CALL(*g_mockMediaStreamContext, CacheFragment(fragmentUrl, _, _, _, _, true, _, _, _, _, _))
		.Times(1)
		.WillOnce(Return(true));
	status = InitializeMPD(mVodManifest, eTUNETYPE_SEEK, 15);
	EXPECT_EQ(status, eAAMPSTATUS_OK);

	// Index the first period
	status = mStreamAbstractionAAMP_MPD->IndexNewMPDDocument(false);

	// Take MediaStreamContext for video track
	MediaStreamContext *pMediaStreamContext = static_cast<MediaStreamContext *>(mStreamAbstractionAAMP_MPD->GetMediaTrack(eTRACK_VIDEO));
	EXPECT_EQ(mStreamAbstractionAAMP_MPD->GetCurrentPeriodIdx(), 0);
	// seek to 15s ends up in segment starting at epoch 1672531214
	EXPECT_EQ(pMediaStreamContext->fragmentTime, 1672531214.0);

	// Index the next period
	mStreamAbstractionAAMP_MPD->IncrementCurrentPeriodIdx();
	mStreamAbstractionAAMP_MPD->SetCurrentPeriod(mStreamAbstractionAAMP_MPD->GetMPD()->GetPeriods().at(1));
	mPrivateInstanceAAMP->SetIsPeriodChangeMarked(true);
	bool periodChanged = true;
	std::string currentPeriodId = "p1";
	uint64_t nextSegTime = 75000;
	mStreamAbstractionAAMP_MPD->InvokeUpdateTrackInfo(false, true);

	/* Test API to detect discontinuity and fetch the initialization segment
	 * for the next period.
	 */
	fragmentUrl = std::string(TEST_BASE_URL) + std::string("video_p1_init.mp4");
	EXPECT_CALL(*g_mockMediaStreamContext, CacheFragment(fragmentUrl, _, _, _, _, true, true, _, _, _, _))
		.Times(1)
		.WillOnce(Return(true));

	mStreamAbstractionAAMP_MPD->InvokeDetectDiscontinuityAndFetchInit(periodChanged, nextSegTime);
	EXPECT_EQ(mPrivateInstanceAAMP->GetIsPeriodChangeMarked(), true);
}

/**
 * @brief BasicFetcherLoop tests.
 *
 * The tests verify the basic fetcher loop functionality for a VOD multi-period MPD.
 */
TEST_F(FetcherLoopTests, BasicFetcherLoop)
{
	std::string fragmentUrl;
	AAMPStatusType status;
	mPrivateInstanceAAMP->rate = AAMP_NORMAL_PLAY_RATE;
	bool ret = false;
	/* Initialize MPD. The video initialization segment is cached. */
	fragmentUrl = std::string(TEST_BASE_URL) + std::string("video_p0_init.mp4");
	EXPECT_CALL(*g_mockMediaStreamContext, CacheFragment(fragmentUrl, _, _, _, _, true, _, _, _, _, _))
		.WillOnce(Return(true));
	EXPECT_CALL(*g_mockMediaStreamContext, CacheFragment(_, _, _, _, _, false, _, _, _, _, _))
		.WillRepeatedly(Return(true));

	status = InitializeMPD(mVodManifest);
	EXPECT_EQ(status, eAAMPSTATUS_OK);

	/* Push the first video segment to present.
	 * The segment starts at time 40.0s and has a duration of 2.0s.
	 */
	fragmentUrl = std::string(TEST_BASE_URL) + std::string("video_p1_init.mp4");
	EXPECT_CALL(*g_mockMediaStreamContext, CacheFragment(fragmentUrl, _, _, _, _, true, _, _, _, _, _))
		.WillOnce(Return(true));
	EXPECT_CALL(*g_mockMediaStreamContext, CacheFragment(_, _, _, _, _, false, _, _, _, _, _))
		.WillRepeatedly(Return(true));

	EXPECT_CALL(*g_mockPrivateInstanceAAMP, DownloadsAreEnabled())
		.Times(AnyNumber())
		.WillRepeatedly(Return(true));

	/* Invoke the fetcher loop. */
	mStreamAbstractionAAMP_MPD->InvokeFetcherLoop();
	EXPECT_EQ(mStreamAbstractionAAMP_MPD->GetCurrentPeriodIdx(), 1);
	EXPECT_EQ(mStreamAbstractionAAMP_MPD->GetIteratorPeriodIdx(), 2);
}

/**
 * @brief BasicFetcherLoop tests.
 *
 * The tests verify the basic fetcher loop functionality for a Live multi-period MPD.
 */
TEST_F(FetcherLoopTests, BasicFetcherLoopLive)
{
	std::string fragmentUrl;
	AAMPStatusType status;
	mPrivateInstanceAAMP->rate = AAMP_NORMAL_PLAY_RATE;
	bool ret = false;
	/* Initialize MPD. The video initialization segment is cached. */
	fragmentUrl = std::string(TEST_BASE_URL) + std::string("video_p0_init.mp4");
	EXPECT_CALL(*g_mockMediaStreamContext, CacheFragment(fragmentUrl, _, _, _, _, true, _, _, _, _, _))
		.Times(1)
		.WillOnce(Return(true));
	status = InitializeMPD(mLiveManifest, eTUNETYPE_SEEK, 27.0);
	EXPECT_EQ(status, eAAMPSTATUS_OK);

	/* Push the first video segment to present.
	 * The segment starts at time 40.0s and has a duration of 2.0s.
	 */
	// Add the new EXPECT_CALL for DownloadsAreEnabled
	EXPECT_CALL(*g_mockPrivateInstanceAAMP, DownloadsAreEnabled())
		.Times(AnyNumber())
		.WillRepeatedly([]()
						{
        					static int counter = 0;
        					return (++counter < 20); });
	fragmentUrl = std::string(TEST_BASE_URL) + std::string("video_p1_init.mp4");
	EXPECT_CALL(*g_mockMediaStreamContext, CacheFragment(fragmentUrl, _, _, _, _, true, _, _, _, _, _))
		.Times(1)
		.WillOnce(Return(true));
	EXPECT_CALL(*g_mockMediaStreamContext, CacheFragment(_, _, _, _, _, false, _, _, _, _, _))
		.WillRepeatedly(Return(true));

	/* Invoke the fetcher loop. */
	mStreamAbstractionAAMP_MPD->InvokeFetcherLoop();
	EXPECT_EQ(mStreamAbstractionAAMP_MPD->GetCurrentPeriodIdx(), 1);
	EXPECT_EQ(mStreamAbstractionAAMP_MPD->GetIteratorPeriodIdx(), 1);
}
