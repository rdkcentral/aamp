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
#include "MockTSBSessionManager.h"
#include "MockAdManager.h"
#include "AampTrackWorker.h"

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

		AAMPStatusType InvokeIndexNewMPDDocument(bool updateTrackInfo = false)
		{
			return IndexNewMPDDocument(updateTrackInfo);
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

		void InvokeInitializeWorkers()
		{
			InitializeWorkers();
		}
	};

	PrivateInstanceAAMP *mPrivateInstanceAAMP;
	TestableStreamAbstractionAAMP_MPD *mTestableStreamAbstractionAAMP_MPD;
	CDAIObject *mCdaiObj;
	const char *mManifest;
	MPD *mAdMPD;
	const char *mAdManifest;
	static constexpr const char *TEST_AD_MANIFEST_URL = "http://host/ad/manifest.mpd";
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
	ManifestDownloadResponsePtr mResponse;
	using BoolConfigSettings = std::map<AAMPConfigSettingBool, bool>;
	using IntConfigSettings = std::map<AAMPConfigSettingInt, int>;

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
			{eAAMPConfig_SetLicenseCaching, false},
			{eAAMPConfig_PropagateURIParam, true},
			{eAAMPConfig_DashParallelFragDownload, true},
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
			{eAAMPConfig_useRialtoSink, false},
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
			{eAAMPConfig_StallTimeoutMS, DEFAULT_STALL_DETECTION_TIMEOUT},
			{eAAMPConfig_AdFulfillmentTimeout, DEFAULT_AD_FULFILLMENT_TIMEOUT},
			{eAAMPConfig_AdFulfillmentTimeoutMax, MAX_AD_FULFILLMENT_TIMEOUT},
			{eAAMPConfig_MaxFragmentChunkCached, DEFAULT_CACHED_FRAGMENT_CHUNKS_PER_TRACK}
		};

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
		assert( g_mockAampUtils == nullptr );
		g_mockAampGstPlayer = new MockAAMPGstPlayer(mPrivateInstanceAAMP);
		mPrivateInstanceAAMP->mIsDefaultOffset = true;
		g_mockPrivateInstanceAAMP = new StrictMock<MockPrivateInstanceAAMP>();
		g_mockMediaStreamContext = new StrictMock<MockMediaStreamContext>();
		g_mockAampMPDDownloader = new StrictMock<MockAampMPDDownloader>();
		g_mockAampStreamSinkManager = new NiceMock<MockAampStreamSinkManager>();
		g_MockPrivateCDAIObjectMPD = new NiceMock<MockPrivateCDAIObjectMPD>();
		mTestableStreamAbstractionAAMP_MPD = nullptr;
		//assert( mTestableStreamAbstractionAAMP_MPD == nullptr );
		mManifest = NULL;
		assert( mManifest == nullptr );
		mBoolConfigSettings = mDefaultBoolConfigSettings;
		mIntConfigSettings = mDefaultIntConfigSettings;
		mResponse = MakeSharedManifestDownloadResponsePtr();
		mCdaiObj = NULL;
		mAdManifest = nullptr;
		mAdMPD = nullptr;
		assert( mCdaiObj == NULL );
	}

	void TearDown()
	{
		if (mTestableStreamAbstractionAAMP_MPD)
		{
			delete mTestableStreamAbstractionAAMP_MPD;
			mTestableStreamAbstractionAAMP_MPD = nullptr;
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

		delete g_MockPrivateCDAIObjectMPD;
		g_MockPrivateCDAIObjectMPD = nullptr;

		mManifest = nullptr;
		mResponse = nullptr;
		if(mAdMPD)
		{
			delete mAdMPD;
			mAdMPD = nullptr;
		}
	}

public:
	void GetMPDFromManifest(ManifestDownloadResponsePtr response)
	{
		std::string manifestStr = std::string(
											  response->mMPDDownloadResponse->mDownloadData.begin(),
											  response->mMPDDownloadResponse->mDownloadData.end() );
		xmlTextReaderPtr reader = xmlReaderForMemory(
													 (char *)manifestStr.c_str(),
													 (int)manifestStr.length(),
													 NULL, NULL, 0);
		assert( reader );
		if (reader != NULL)
		{
			assert( response->mRootNode == NULL );
			if (xmlTextReaderRead(reader))
			{
				response->mRootNode = MPDProcessNode(&reader, TEST_MANIFEST_URL);
				assert( response->mRootNode );
				if (response->mRootNode != NULL)
				{
					dash::mpd::MPD *mpd = response->mRootNode->ToMPD();
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
	 * @param[in] remoteUrl Manifest url
	 * @param[out] buffer Buffer containing manifest data
	 * @retval true on success
	 */
	ManifestDownloadResponsePtr GetManifestForMPDDownloader()
	{
		//assert( !mResponse->mMPDInstance );
		if (!mResponse->mMPDInstance)
		{
			ManifestDownloadResponsePtr response = MakeSharedManifestDownloadResponsePtr();
			response->mMPDStatus = AAMPStatusType::eAAMPSTATUS_OK;
			response->mMPDDownloadResponse->iHttpRetValue = 200;
			response->mMPDDownloadResponse->sEffectiveUrl = std::string(TEST_MANIFEST_URL);
			assert( mManifest);
			response->mMPDDownloadResponse->mDownloadData.assign( (uint8_t *)mManifest, (uint8_t *)&mManifest[strlen(mManifest)] );
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
		mTestableStreamAbstractionAAMP_MPD = new TestableStreamAbstractionAAMP_MPD(mPrivateInstanceAAMP, seekPos, rate);
		mCdaiObj = new CDAIObjectMPD(mPrivateInstanceAAMP);
		mTestableStreamAbstractionAAMP_MPD->SetCDAIObject(mCdaiObj);

		mPrivateInstanceAAMP->SetManifestUrl(TEST_MANIFEST_URL);

		/* Initialize MPD. */
		EXPECT_CALL(*g_mockPrivateInstanceAAMP, SetState(eSTATE_PREPARING));

		EXPECT_CALL(*g_mockPrivateInstanceAAMP, GetState())
			.Times(AnyNumber())
			.WillRepeatedly(Return(eSTATE_PREPARING));
		EXPECT_CALL(*g_mockPrivateInstanceAAMP, GetLLDashChunkMode()).WillRepeatedly(Return(false));
		EXPECT_CALL(*g_mockPrivateInstanceAAMP, SetLLDashChunkMode(_));

		// For the time being return the same manifest again
		EXPECT_CALL(*g_mockAampMPDDownloader, GetManifest(_, _, _))
			.WillRepeatedly(WithoutArgs(Invoke(this, &FetcherLoopTests::GetManifestForMPDDownloader)));
		status = mTestableStreamAbstractionAAMP_MPD->Init(tuneType);
		return status;
	}

	/**
	 * @brief Initialize the Ad MPD instance
	 *
	 * This will:
	 *  - Parse the manifest.
	 *
	 * @param[in] manifest Manifest data
	 */
	void InitializeAdMPDObject(const char *manifest)
	{
		if (manifest)
		{
			mAdManifest = manifest;
			std::string manifestStr = mAdManifest;
			xmlTextReaderPtr reader = xmlReaderForMemory((char *)manifestStr.c_str(), (int)manifestStr.length(), NULL, NULL, 0);
			if (reader != NULL)
			{
				if (xmlTextReaderRead(reader))
				{
					Node *rootNode = MPDProcessNode(&reader, TEST_AD_MANIFEST_URL);
					if (rootNode != NULL)
					{
						if (mAdMPD)
						{
							delete mAdMPD;
							mAdMPD = nullptr;
						}
						mAdMPD = rootNode->ToMPD();
						delete rootNode;
					}
				}
			}
			xmlFreeTextReader(reader);
		}
	}

	/**
	 * @brief Push next fragment helper method
	 *
	 * @param[in] trackType Media track type
	 */
	bool PushNextFragment(TrackType trackType)
	{
		MediaTrack *track = mTestableStreamAbstractionAAMP_MPD->GetMediaTrack(trackType);
		EXPECT_NE(track, nullptr);

		MediaStreamContext *pMediaStreamContext = static_cast<MediaStreamContext *>(track);

		return mTestableStreamAbstractionAAMP_MPD->PushNextFragment(pMediaStreamContext, 0);
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
	status = mTestableStreamAbstractionAAMP_MPD->InvokeIndexNewMPDDocument(false); (void)status;
	EXPECT_EQ(mTestableStreamAbstractionAAMP_MPD->GetCurrentPeriodIdx(), 0);
	EXPECT_EQ(mTestableStreamAbstractionAAMP_MPD->GetIteratorPeriodIdx(), 0);

	// Index the next period
	mTestableStreamAbstractionAAMP_MPD->IncrementIteratorPeriodIdx();
	bool periodChanged = false;
	bool adStateChanged = false;
	bool waitForAdBreakCatchup = false;
	bool requireStreamSelection = false;
	bool mpdChanged = false;
	std::string currentPeriodId = "p0";

	/*
	 * Test the scenario where period change happens
	 * The period is changed and requireStreamSelection is set to true
	 */
	ret = mTestableStreamAbstractionAAMP_MPD->InvokeSelectSourceOrAdPeriod(periodChanged, mpdChanged, adStateChanged, waitForAdBreakCatchup, requireStreamSelection, currentPeriodId);
	EXPECT_EQ(ret, true);
	EXPECT_EQ(requireStreamSelection, true);
	EXPECT_EQ(periodChanged, true);
	EXPECT_EQ(mTestableStreamAbstractionAAMP_MPD->GetCurrentPeriodIdx(), mTestableStreamAbstractionAAMP_MPD->GetIteratorPeriodIdx());
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
	status = mTestableStreamAbstractionAAMP_MPD->InvokeIndexNewMPDDocument(false); (void)status;
	EXPECT_EQ(mTestableStreamAbstractionAAMP_MPD->GetCurrentPeriodIdx(), 1);
	mTestableStreamAbstractionAAMP_MPD->SetIteratorPeriodIdx(mTestableStreamAbstractionAAMP_MPD->GetCurrentPeriodIdx());

	// Index the next period, wait for the selection
	mTestableStreamAbstractionAAMP_MPD->IncrementIteratorPeriodIdx();
	bool periodChanged = false;
	bool adStateChanged = false;
	bool waitForAdBreakCatchup = false;
	bool requireStreamSelection = false;
	bool mpdChanged = false;
	std::string currentPeriodId = "p1";

	/*
	 * Test the scenario where period change happens, it is already at the boundary
	 * so no change in period
	 */
	ret = mTestableStreamAbstractionAAMP_MPD->InvokeSelectSourceOrAdPeriod(periodChanged, mpdChanged, adStateChanged, waitForAdBreakCatchup, requireStreamSelection, currentPeriodId);
	EXPECT_EQ(ret, false);
	EXPECT_EQ(requireStreamSelection, false);
	EXPECT_EQ(periodChanged, false);
}

/**
 * @brief IndexSelectedPeriod tests.
 *
 * The tests verify the live behavior of IndexSelectedPeriod method of StreamAbstractionAAMP_MPD
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

	// Testing Indexing behavior of the period
	MediaStreamContext *pMediaStreamContext = static_cast<MediaStreamContext *>(mTestableStreamAbstractionAAMP_MPD->GetMediaTrack(eTRACK_VIDEO));
	bool periodChanged = true;
	bool adStateChanged = false;
	bool requireStreamSelection = true;
	std::string currentPeriodId = "p1";

	/*
	 * Test the scenario where period index happens
	 * All the values are reset to default
	 */
	ret = mTestableStreamAbstractionAAMP_MPD->InvokeIndexSelectedPeriod(periodChanged, adStateChanged, requireStreamSelection, currentPeriodId);
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

	status = mTestableStreamAbstractionAAMP_MPD->InvokeIndexNewMPDDocument(false); (void)status;

	MediaStreamContext *pMediaStreamContext = static_cast<MediaStreamContext *>(mTestableStreamAbstractionAAMP_MPD->GetMediaTrack(eTRACK_VIDEO));
	EXPECT_EQ(mTestableStreamAbstractionAAMP_MPD->GetCurrentPeriodIdx(), 0);
	// seek to 15s ends up in segment starting at epoch 1672531214
	EXPECT_EQ(pMediaStreamContext->fragmentTime, 1672531214.0);
	mTestableStreamAbstractionAAMP_MPD->SetIteratorPeriodIdx(mTestableStreamAbstractionAAMP_MPD->GetCurrentPeriodIdx());

	// Set current period as next period
	mTestableStreamAbstractionAAMP_MPD->IncrementCurrentPeriodIdx();
	mTestableStreamAbstractionAAMP_MPD->SetCurrentPeriod(mTestableStreamAbstractionAAMP_MPD->GetMPD()->GetPeriods().at(1));
	bool periodChanged = true;
	bool adStateChanged = false;
	bool requireStreamSelection = true;
	std::string currentPeriodId = "p1";

	/*
	 * Test the scenario where period index happens
	 * New period start is indexed at 1672531230
	 */
	ret = mTestableStreamAbstractionAAMP_MPD->InvokeIndexSelectedPeriod(periodChanged, adStateChanged, requireStreamSelection, currentPeriodId);
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
	/* Initialize MPD. The video initialization segment is cached. */
	fragmentUrl = std::string(TEST_BASE_URL) + std::string("video_p0_init.mp4");
	EXPECT_CALL(*g_mockMediaStreamContext, CacheFragment(fragmentUrl, _, _, _, _, true, _, _, _, _, _))
		.Times(1)
		.WillOnce(Return(true));
	status = InitializeMPD(mVodManifest, eTUNETYPE_SEEK, 0);
	EXPECT_EQ(status, eAAMPSTATUS_OK);

	status = mTestableStreamAbstractionAAMP_MPD->InvokeIndexNewMPDDocument(false); (void)status;

	MediaStreamContext *pMediaStreamContext = static_cast<MediaStreamContext *>(mTestableStreamAbstractionAAMP_MPD->GetMediaTrack(eTRACK_VIDEO));
	EXPECT_EQ(mTestableStreamAbstractionAAMP_MPD->GetCurrentPeriodIdx(), 0);
	// seek to 0 ends up in segment starting at epoch 1672531200
	EXPECT_EQ(pMediaStreamContext->fragmentTime, 1672531200.0);

	// Change and index the next period,
	mTestableStreamAbstractionAAMP_MPD->IncrementCurrentPeriodIdx();
	mTestableStreamAbstractionAAMP_MPD->SetCurrentPeriod(mTestableStreamAbstractionAAMP_MPD->GetMPD()->GetPeriods().at(1));
	mPrivateInstanceAAMP->SetIsPeriodChangeMarked(true);
	bool periodChanged = true;
	std::string currentPeriodId = "p1";
	mTestableStreamAbstractionAAMP_MPD->InvokeUpdateTrackInfo(false, false);

	/* Test API to detect discontinuity and fetch the initialization segment
	 * for the next period.
	 * Test the period change (discontinuity) is not marked
	 */
	mTestableStreamAbstractionAAMP_MPD->InvokeDetectDiscontinuityAndFetchInit(periodChanged);
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
	/* Initialize MPD. The video initialization segment is cached. */
	fragmentUrl = std::string(TEST_BASE_URL) + std::string("video_p0_init.mp4");
	EXPECT_CALL(*g_mockMediaStreamContext, CacheFragment(fragmentUrl, _, _, _, _, true, _, _, _, _, _))
		.Times(1)
		.WillOnce(Return(true));
	status = InitializeMPD(mVodManifest, eTUNETYPE_SEEK, 15);
	EXPECT_EQ(status, eAAMPSTATUS_OK);

	// Index the first period
	status = mTestableStreamAbstractionAAMP_MPD->InvokeIndexNewMPDDocument(false); (void)status;

	// Take MediaStreamContext for video track
	MediaStreamContext *pMediaStreamContext = static_cast<MediaStreamContext *>(mTestableStreamAbstractionAAMP_MPD->GetMediaTrack(eTRACK_VIDEO));
	EXPECT_EQ(mTestableStreamAbstractionAAMP_MPD->GetCurrentPeriodIdx(), 0);
	// seek to 15s ends up in segment starting at epoch 1672531214
	EXPECT_EQ(pMediaStreamContext->fragmentTime, 1672531214.0);

	// Index the next period
	mTestableStreamAbstractionAAMP_MPD->IncrementCurrentPeriodIdx();
	mTestableStreamAbstractionAAMP_MPD->SetCurrentPeriod(mTestableStreamAbstractionAAMP_MPD->GetMPD()->GetPeriods().at(1));
	mPrivateInstanceAAMP->SetIsPeriodChangeMarked(true);
	bool periodChanged = true;
	std::string currentPeriodId = "p1";
	uint64_t nextSegTime = 75000;
	mTestableStreamAbstractionAAMP_MPD->InvokeUpdateTrackInfo(false, true);

	/* Test API to detect discontinuity and fetch the initialization segment
	 * for the next period.
	 */
	fragmentUrl = std::string(TEST_BASE_URL) + std::string("video_p1_init.mp4");
	EXPECT_CALL(*g_mockMediaStreamContext, CacheFragment(fragmentUrl, _, _, _, _, true, true, _, _, _, _))
		.Times(1)
		.WillOnce(Return(true));

	mTestableStreamAbstractionAAMP_MPD->InvokeDetectDiscontinuityAndFetchInit(periodChanged, nextSegTime);
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
	const double expectedFirstPTS = 0.0;
	const AampTime expectedFirstPTSOffset = 30.0;

	AAMPStatusType status;
	mPrivateInstanceAAMP->rate = AAMP_NORMAL_PLAY_RATE;
	/* Initialize MPD. The video initialization segment is cached. */
	fragmentUrl = std::string(TEST_BASE_URL) + std::string("video_p0_init.mp4");
	EXPECT_CALL(*g_mockMediaStreamContext, CacheFragment(fragmentUrl, _, _, _, _, true, _, _, _, _, _))
		.WillOnce(Return(true));
	EXPECT_CALL(*g_mockMediaStreamContext, CacheFragment(_, _, _, _, _, false, _, _, _, _, _))
		.WillRepeatedly(Return(true));
	EXPECT_CALL(*g_mockPrivateInstanceAAMP, IsLocalAAMPTsbInjection()).WillRepeatedly(Return(false));
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
	mTestableStreamAbstractionAAMP_MPD->InvokeInitializeWorkers();
	mTestableStreamAbstractionAAMP_MPD->InvokeFetcherLoop();
	EXPECT_EQ(mTestableStreamAbstractionAAMP_MPD->GetCurrentPeriodIdx(), 1);
	EXPECT_EQ(mTestableStreamAbstractionAAMP_MPD->GetIteratorPeriodIdx(), 2);

	EXPECT_CALL(*g_mockPrivateInstanceAAMP, GetTSBSessionManager()).WillRepeatedly(Return(nullptr));
	EXPECT_CALL(*g_mockAampConfig, IsConfigSet(eAAMPConfig_EnablePTSReStamp)).WillOnce(Return(false));
	// GetFirstPTS should return the first PTS value if EnablePTSReStamp is not set */
	EXPECT_EQ(expectedFirstPTS, mTestableStreamAbstractionAAMP_MPD->GetFirstPTS());

	EXPECT_CALL(*g_mockAampConfig, IsConfigSet(eAAMPConfig_EnablePTSReStamp)).WillOnce(Return(true));
	// GetFirstPTS should return the restamped first PTS value if EnablePTSReStamp is set */
	EXPECT_EQ(expectedFirstPTS + expectedFirstPTSOffset.inSeconds(), mTestableStreamAbstractionAAMP_MPD->GetFirstPTS());
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
	/* Initialize MPD. The video initialization segment is cached. */
	fragmentUrl = std::string(TEST_BASE_URL) + std::string("video_p0_init.mp4");
	EXPECT_CALL(*g_mockMediaStreamContext, CacheFragment(fragmentUrl, _, _, _, _, true, _, _, _, _, _))
		.Times(1)
		.WillOnce(Return(true));
	EXPECT_CALL(*g_mockPrivateInstanceAAMP, IsLocalAAMPTsbInjection()).WillRepeatedly(Return(false));

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
	mTestableStreamAbstractionAAMP_MPD->InvokeInitializeWorkers();
	mTestableStreamAbstractionAAMP_MPD->InvokeFetcherLoop();
	EXPECT_EQ(mTestableStreamAbstractionAAMP_MPD->GetCurrentPeriodIdx(), 1);
	EXPECT_EQ(mTestableStreamAbstractionAAMP_MPD->GetIteratorPeriodIdx(), 1);
}

/**
 * @brief SelectSourceOrAdPeriod tests.
 *
 * The tests verify the SelectSourceOrAdPeriod method of StreamAbstractionAAMP_MPD when transitioning
 * from a CDAI ad period to a regular period
 */
TEST_F(FetcherLoopTests, SelectSourceOrAdPeriodTests3)
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
	status = InitializeMPD(mLiveManifest, eTUNETYPE_SEEK, 10);
	EXPECT_EQ(status, eAAMPSTATUS_OK);

	// Index the initial values
	status = mTestableStreamAbstractionAAMP_MPD->InvokeIndexNewMPDDocument(false); (void)status;
	EXPECT_EQ(mTestableStreamAbstractionAAMP_MPD->GetCurrentPeriodIdx(), 0);
	mTestableStreamAbstractionAAMP_MPD->SetIteratorPeriodIdx(mTestableStreamAbstractionAAMP_MPD->GetCurrentPeriodIdx());

	// Index the next period, wait for the selection
	// mTestableStreamAbstractionAAMP_MPD->IncrementIteratorPeriodIdx();
	// Set the ad variables, we have finished ad playback and waiting for base period to catchup
	auto cdaiObj = mTestableStreamAbstractionAAMP_MPD->GetCDAIObject();
	cdaiObj->mAdState = AdState::IN_ADBREAK_WAIT2CATCHUP;
	std::string periodId = "p0";
	std::string endPeriodId = "p1"; // landing in p1
	// Add ads to the adBreak
	cdaiObj->mAdBreaks = {
		{periodId, AdBreakObject(30000, std::make_shared<std::vector<AdNode>>(), "", 0, 30000)}};
	// ad is currently not placed
	cdaiObj->mAdBreaks[periodId].ads->emplace_back(false /*invalid*/, false /*placed*/, true /*resolved*/,
												   "adId1" /*adId*/, "url" /*url*/, 30000 /*duration*/, periodId /*basePeriodId*/, 0 /*basePeriodOffset*/, nullptr /*mpd*/);
	cdaiObj->mCurAdIdx = 0;
	cdaiObj->mCurAds = cdaiObj->mAdBreaks[periodId].ads;
	cdaiObj->mCurPlayingBreakId = periodId;

	bool periodChanged = false;
	bool adStateChanged = true; // since we finished playing an ad
	bool waitForAdBreakCatchup = false;
	bool requireStreamSelection = false;
	bool mpdChanged = false;
	std::string currentPeriodId = "p0";


	EXPECT_CALL(*g_mockPrivateInstanceAAMP, GetTSBSessionManager()).WillRepeatedly(Return(nullptr));
	EXPECT_CALL(*g_mockPrivateInstanceAAMP, IsLocalAAMPTsbInjection()).WillRepeatedly(Return(false));
	EXPECT_CALL(*g_mockPrivateInstanceAAMP, SendAdReservationEvent(_, _, _, _, _)).Times(AnyNumber());

	/*
	 * Test the scenario where ad is not placed and we are waiting for base period to catchup
	 */
	ret = mTestableStreamAbstractionAAMP_MPD->InvokeSelectSourceOrAdPeriod(periodChanged, mpdChanged, adStateChanged, waitForAdBreakCatchup, requireStreamSelection, currentPeriodId);
	EXPECT_FALSE(ret);
	// Still in IN_ADBREAK_WAIT2CATCHUP
	EXPECT_FALSE(adStateChanged);
	EXPECT_TRUE(waitForAdBreakCatchup);
	EXPECT_EQ(cdaiObj->mAdState, AdState::IN_ADBREAK_WAIT2CATCHUP);

	/*
	 * Test the scenario where the manifest is refreshed, but ad is not placed and we are waiting for base period to catchup
	 */
	mpdChanged = true;
	ret = mTestableStreamAbstractionAAMP_MPD->InvokeSelectSourceOrAdPeriod(periodChanged, mpdChanged, adStateChanged, waitForAdBreakCatchup, requireStreamSelection, currentPeriodId);
	EXPECT_FALSE(ret);
	// Still in IN_ADBREAK_WAIT2CATCHUP
	EXPECT_FALSE(adStateChanged);
	EXPECT_TRUE(waitForAdBreakCatchup);
	EXPECT_EQ(cdaiObj->mAdState, AdState::IN_ADBREAK_WAIT2CATCHUP);

	/*
	 * Test the scenario where the manifest is refreshed, ad is placed but the next period is empty and hence adbreak is not placed
	 */
	mpdChanged = true;
	cdaiObj->mAdBreaks[periodId].ads->at(cdaiObj->mCurAdIdx).placed = true;
	cdaiObj->mAdBreaks[periodId].mAdBreakPlaced = false;
	ret = mTestableStreamAbstractionAAMP_MPD->InvokeSelectSourceOrAdPeriod(periodChanged, mpdChanged, adStateChanged, waitForAdBreakCatchup, requireStreamSelection, currentPeriodId);
	EXPECT_FALSE(ret);
	// Still in IN_ADBREAK_WAIT2CATCHUP
	EXPECT_FALSE(adStateChanged);
	EXPECT_TRUE(waitForAdBreakCatchup);
	EXPECT_EQ(cdaiObj->mAdState, AdState::IN_ADBREAK_WAIT2CATCHUP);

	/*
	 * Test the scenario where the manifest is refreshed, ad and adbreak are placed
	 */
	mpdChanged = true;
	cdaiObj->mAdBreaks[periodId].ads->at(cdaiObj->mCurAdIdx).placed = true;
	cdaiObj->mAdBreaks[periodId].endPeriodId = endPeriodId;
	cdaiObj->mAdBreaks[periodId].endPeriodOffset = 0; // aligned to boundary
	cdaiObj->mAdBreaks[periodId].mAdBreakPlaced = true;
	ret = mTestableStreamAbstractionAAMP_MPD->InvokeSelectSourceOrAdPeriod(periodChanged, mpdChanged, adStateChanged, waitForAdBreakCatchup, requireStreamSelection, currentPeriodId);
	EXPECT_TRUE(ret);
	// Now in OUTSIDE_ADBREAK
	EXPECT_FALSE(adStateChanged);
	// periodChanged is now true
	EXPECT_TRUE(periodChanged);
	EXPECT_FALSE(waitForAdBreakCatchup);
	EXPECT_EQ(cdaiObj->mAdState, AdState::OUTSIDE_ADBREAK);
	EXPECT_EQ(currentPeriodId, endPeriodId);
	EXPECT_EQ(mTestableStreamAbstractionAAMP_MPD->GetCurrentPeriod()->GetId(), endPeriodId);
	EXPECT_EQ(mTestableStreamAbstractionAAMP_MPD->GetIteratorPeriodIdx(), 1);
}

TEST_F(FetcherLoopTests, SkipFetchAudioTests)
{
	static const char *manifest =
R"(<?xml version="1.0" encoding="UTF-8"?>
<MPD xmlns="urn:mpeg:dash:schema:mpd:2011" xmlns:scte35="urn:scte:scte35:2014:xml+bin" xmlns:scte214="scte214" xmlns:cenc="urn:mpeg:cenc:2013" xmlns:mspr="mspr" type="dynamic" id="0000000000000018163" profiles="urn:mpeg:dash:profile:isoff-live:2011" minBufferTime="PT2.000S" maxSegmentDuration="PT0H0M1.92S" minimumUpdatePeriod="PT0H0M1.920S" availabilityStartTime="1977-05-25T18:00:00.000Z" timeShiftBufferDepth="PT0H0M30.000S" publishTime="2024-11-08T12:53:09.725Z">
	<Period id="901591170" start="PT416006H37M27.854S">
		<AdaptationSet id="2" contentType="video" mimeType="video/mp4" segmentAlignment="true" startWithSAP="1">
			<EssentialProperty schemeIdUri="urn:mpeg:mpegB:cicp:ColourPrimaries" value="1"/>
			<EssentialProperty schemeIdUri="urn:mpeg:mpegB:cicp:MatrixCoefficients" value="1"/>
			<EssentialProperty schemeIdUri="urn:mpeg:mpegB:cicp:TransferCharacteristics" value="1"/>
			<Role schemeIdUri="urn:mpeg:dash:role:2011" value="main"/>
			<SegmentTemplate initialization="SKYNEHD_HD_SUD_SKYUKD_4050_18_0000000000000018163/track-video-repid-$RepresentationID$-tc--enc--header.mp4" media="SKYNEHD_HD_SUD_SKYUKD_4050_18_0000000000000018163/track-video-repid-$RepresentationID$-tc--enc--frag-$Number$.mp4" timescale="90000" startNumber="901599260" presentationTimeOffset="20213">
				<SegmentTimeline>
					<S t="1377581813" d="172800" r="14"/>
				</SegmentTimeline>
			</SegmentTemplate>
			<Representation id="root_video4" bandwidth="562800" codecs="hvc1.1.6.L63.90" width="640" height="360" frameRate="25000/1000"/>
			<Representation id="root_video3" bandwidth="1328400" codecs="hvc1.1.6.L93.90" width="960" height="540" frameRate="50000/1000"/>
			<Representation id="root_video2" bandwidth="1996000" codecs="hvc1.1.6.L93.90" width="960" height="540" frameRate="50000/1000"/>
			<Representation id="root_video1" bandwidth="4461200" codecs="hvc1.1.6.L120.90" width="1280" height="720" frameRate="50000/1000"/>
			<Representation id="root_video0" bandwidth="6052400" codecs="hvc1.1.6.L123.90" width="1920" height="1080" frameRate="50000/1000"/>
		</AdaptationSet>
		<AdaptationSet id="3" contentType="audio" mimeType="audio/mp4" lang="en">
			<AudioChannelConfiguration schemeIdUri="tag:dolby.com,2014:dash:audio_channel_configuration:2011" value="a000"/>
			<Role schemeIdUri="urn:mpeg:dash:role:2011" value="main"/>
			<SegmentTemplate initialization="SKYNEHD_HD_SUD_SKYUKD_4050_18_0000000000000018163-eac3/track-audio-repid-$RepresentationID$-tc--enc--header.mp4" media="SKYNEHD_HD_SUD_SKYUKD_4050_18_0000000000000018163-eac3/track-audio-repid-$RepresentationID$-tc--enc--frag-$Number$.mp4" timescale="90000" startNumber="901599260" presentationTimeOffset="20213">
				<SegmentTimeline>
					<S t="1377583936" d="172800" r="14"/>
				</SegmentTimeline>
			</SegmentTemplate>
			<Representation id="root_audio110" bandwidth="215200" codecs="ec-3" audioSamplingRate="48000"/>
		</AdaptationSet>
	</Period>
	<SupplementalProperty schemeIdUri="urn:scte:dash:powered-by" value="example-mod_super8-4.4.0-1"/>
</MPD>
)";
	EXPECT_CALL(*g_mockMediaStreamContext, CacheFragment(_, _, _, _, _, true, _, _, _, _, _))
				.WillRepeatedly(Return(true));

	AAMPStatusType status = InitializeMPD(manifest, eTUNETYPE_NEW_NORMAL, 10.0);
	EXPECT_EQ(status, eAAMPSTATUS_OK);

	MediaTrack *track = mTestableStreamAbstractionAAMP_MPD->GetMediaTrack(eTRACK_AUDIO);
	EXPECT_NE(track, nullptr);
	MediaStreamContext *pMediaStreamContext = static_cast<MediaStreamContext *>(track);
	mTestableStreamAbstractionAAMP_MPD->InvokeIndexNewMPDDocument(true);
	mTestableStreamAbstractionAAMP_MPD->PushNextFragment(pMediaStreamContext,eCURLINSTANCE_AUDIO);
	pMediaStreamContext->freshManifest=true;
	//when skipfetch sets to true, fetchfragment will be avoided
	EXPECT_CALL(*g_mockMediaStreamContext, CacheFragment(_, eCURLINSTANCE_AUDIO, _,_, _, _, _, _, _, _, _))
				.Times(0);
	EXPECT_CALL(*g_mockPrivateInstanceAAMP, GetPositionMilliseconds()).WillRepeatedly(Return(0.0));

	mTestableStreamAbstractionAAMP_MPD->SwitchAudioTrack();


}

/**
 * @brief BasicFetcherLoop tests.
 *
 * The tests verify the basic fetcher loop functionality for a Live multi-period MPD.
 */
TEST_F(FetcherLoopTests, BasicFetcherLoopLiveWithParallelDownload)
{
	std::string videoFragmentUrl;
	std::string audioFragmentUrl;
	AAMPStatusType status;
	mPrivateInstanceAAMP->rate = AAMP_NORMAL_PLAY_RATE;
	static const char *multiTrackManifest = R"(<?xml version="1.0" encoding="utf-8"?>
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
							<AdaptationSet id="1" contentType="audio" lang="eng">
								<Representation id="0" mimeType="audio/mp4" codecs="ec-3" bandwidth="800000" width="640" height="360" frameRate="25">
									<SegmentTemplate timescale="2500" initialization="audio_p0_init.mp4" media="audio_p0_$Number$.m4s" startNumber="1">
										<SegmentTimeline>
											<S t="0" d="5000" r="14" />
										</SegmentTimeline>
									</SegmentTemplate>
								</Representation>
							</AdaptationSet>
						</Period>
						<Period id="p1" start="PT30S">
							<AdaptationSet id="0" contentType="video">
								<Representation id="0" mimeType="video/mp4" codecs="avc1.640028" bandwidth="800000" width="640" height="360" frameRate="25">
									<SegmentTemplate timescale="2500" initialization="video_p1_init.mp4" media="video_p1_$Number$.m4s" startNumber="16">
										<SegmentTimeline>
											<S t="0" d="5000" r="14" />
										</SegmentTimeline>
									</SegmentTemplate>
								</Representation>
							</AdaptationSet>
							<AdaptationSet id="1" contentType="audio" lang="eng">
								<Representation id="0" mimeType="audio/mp4" codecs="ec-3" bandwidth="800000" width="640" height="360" frameRate="25">
									<SegmentTemplate timescale="2500" initialization="audio_p1_init.mp4" media="audio_p1_$Number$.m4s" startNumber="16">
										<SegmentTimeline>
											<S t="0" d="5000" r="14" />
										</SegmentTimeline>
									</SegmentTemplate>
								</Representation>
							</AdaptationSet>
						</Period>
				</MPD>
				)";

	/* Initialize MPD. The video initialization segment is cached. */
	videoFragmentUrl = std::string(TEST_BASE_URL) + std::string("video_p0_init.mp4");
	audioFragmentUrl = std::string(TEST_BASE_URL) + std::string("audio_p0_init.mp4");
	EXPECT_CALL(*g_mockMediaStreamContext, CacheFragment(videoFragmentUrl, _, _, _, _, true, _, _, _, _, _)).Times(1).WillOnce(Return(true));
	EXPECT_CALL(*g_mockMediaStreamContext, CacheFragment(audioFragmentUrl, _, _, _, _, true, _, _, _, _, _)).Times(1).WillOnce(Return(true));
	EXPECT_CALL(*g_mockPrivateInstanceAAMP, IsLocalAAMPTsbInjection()).WillRepeatedly(Return(false));

	status = InitializeMPD(multiTrackManifest, eTUNETYPE_SEEK, 24.0);

	/* Invoke Worker threads */
	mTestableStreamAbstractionAAMP_MPD->InvokeInitializeWorkers();

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
	videoFragmentUrl = std::string(TEST_BASE_URL) + std::string("video_p1_init.mp4");
	audioFragmentUrl = std::string(TEST_BASE_URL) + std::string("audio_p1_init.mp4");
	EXPECT_CALL(*g_mockMediaStreamContext, CacheFragment(videoFragmentUrl, _, _, _, _, true, _, _, _, _, _)).Times(1).WillOnce(Return(true));
	EXPECT_CALL(*g_mockMediaStreamContext, CacheFragment(audioFragmentUrl, _, _, _, _, true, _, _, _, _, _)).Times(1).WillOnce(Return(true));
	// Expect the segments to be downloaded from track
	EXPECT_CALL(*g_mockMediaStreamContext, CacheFragment(_, _, _, _, _, false, _, _, _, _, _)).WillRepeatedly(Return(true));

	/* Invoke the fetcher loop. */
	mTestableStreamAbstractionAAMP_MPD->InvokeFetcherLoop();
	EXPECT_EQ(mTestableStreamAbstractionAAMP_MPD->GetCurrentPeriodIdx(), 1);
	EXPECT_EQ(mTestableStreamAbstractionAAMP_MPD->GetIteratorPeriodIdx(), 1);
}

/**
 * @brief SelectSourceOrAdPeriod tests.
 *
 * The tests verify the SelectSourceOrAdPeriod method of StreamAbstractionAAMP_MPD in forward period
 * change scenarios with the next period and the next one being tiny periods which will be all skipped.
 */
TEST_F(FetcherLoopTests, SelectSourceOrAdPeriodTests4)
{
	std::string videoInitFragmentUrl;
	std::string audioInitFragmentUrl;
	AAMPStatusType status;
	static const char *mManifest =
R"(<?xml version="1.0" encoding="utf-8"?>
<MPD xmlns="urn:mpeg:dash:schema:mpd:2011" availabilityStartTime="2023-01-01T00:00:00Z" maxSegmentDuration="PT2S" minBufferTime="PT4.000S" minimumUpdatePeriod="P100Y" profiles="urn:dvb:dash:profile:dvb-dash:2014,urn:dvb:dash:profile:dvb-dash:isoff-ext-live:2014" publishTime="2023-01-01T00:01:00Z" timeShiftBufferDepth="PT5M" type="dynamic">
	<Period id="p0" start="PT0S">
		<AdaptationSet id="0" contentType="video">
			<Representation id="0" mimeType="video/mp4" codecs="avc1.640028" bandwidth="800000" width="640" height="360" frameRate="25">
				<SegmentTemplate timescale="2500" initialization="video_p0_init.mp4" media="video_p0_$Number$.m4s" startNumber="1" presentationTimeOffset="0">
					<SegmentTimeline>
						<S t="0" d="5000" r="14" />
					</SegmentTimeline>
				</SegmentTemplate>
			</Representation>
		</AdaptationSet>
		<AdaptationSet id="1" contentType="audio" lang="eng">
			<Representation id="0" mimeType="audio/mp4" codecs="ec-3" bandwidth="800000" width="640" height="360" frameRate="25">
				<SegmentTemplate timescale="2500" initialization="audio_p0_init.mp4" media="audio_p0_$Number$.m4s" startNumber="1" presentationTimeOffset="0">
					<SegmentTimeline>
						<S t="0" d="5000" r="14" />
					</SegmentTimeline>
				</SegmentTemplate>
			</Representation>
		</AdaptationSet>
	</Period>
	<Period id="p1" start="PT30S">
		<AdaptationSet id="0" contentType="video">
			<Representation id="0" mimeType="video/mp4" codecs="avc1.640028" bandwidth="800000" width="640" height="360" frameRate="25">
				<SegmentTemplate timescale="2500" initialization="video_p1_init.mp4" media="video_p1_$Number$.m4s" startNumber="16" presentationTimeOffset="75000">
					<SegmentTimeline>
						<S t="75000" d="625"/>
					</SegmentTimeline>
				</SegmentTemplate>
			</Representation>
		</AdaptationSet>
		<AdaptationSet id="1" contentType="audio" lang="eng">
			<Representation id="0" mimeType="audio/mp4" codecs="ec-3" bandwidth="800000" width="640" height="360" frameRate="25">
				<SegmentTemplate timescale="2500" initialization="audio_p1_init.mp4" media="audio_p1_$Number$.m4s" startNumber="16" presentationTimeOffset="75000">
					<SegmentTimeline>
						<S t="75000" d="625"/>
					</SegmentTimeline>
				</SegmentTemplate>
			</Representation>
		</AdaptationSet>
	</Period>
	<Period id="p2" start="PT30.250S">
		<AdaptationSet id="0" contentType="video">
			<Representation id="0" mimeType="video/mp4" codecs="avc1.640028" bandwidth="800000" width="640" height="360" frameRate="25">
				<SegmentTemplate timescale="2500" initialization="video_p1_init.mp4" media="video_p1_$Number$.m4s" startNumber="16" presentationTimeOffset="75625">
					<SegmentTimeline>
						<S t="75625" d="625"/>
					</SegmentTimeline>
				</SegmentTemplate>
			</Representation>
		</AdaptationSet>
		<AdaptationSet id="1" contentType="audio" lang="eng">
			<Representation id="0" mimeType="audio/mp4" codecs="ec-3" bandwidth="800000" width="640" height="360" frameRate="25">
				<SegmentTemplate timescale="2500" initialization="audio_p1_init.mp4" media="audio_p1_$Number$.m4s" startNumber="16" presentationTimeOffset="75625">
					<SegmentTimeline>
						<S t="75625" d="625"/>
					</SegmentTimeline>
				</SegmentTemplate>
			</Representation>
		</AdaptationSet>
	</Period>
	<Period id="p3" start="PT30.500S">
		<AdaptationSet id="0" contentType="video">
			<Representation id="0" mimeType="video/mp4" codecs="avc1.640028" bandwidth="800000" width="640" height="360" frameRate="25">
				<SegmentTemplate timescale="2500" initialization="video_p1_init.mp4" media="video_p1_$Number$.m4s" startNumber="16" presentationTimeOffset="76250">
					<SegmentTimeline>
						<S t="76250" d="5000" r="14" />
					</SegmentTimeline>
				</SegmentTemplate>
			</Representation>
		</AdaptationSet>
		<AdaptationSet id="1" contentType="audio" lang="eng">
			<Representation id="0" mimeType="audio/mp4" codecs="ec-3" bandwidth="800000" width="640" height="360" frameRate="25">
				<SegmentTemplate timescale="2500" initialization="audio_p1_init.mp4" media="audio_p1_$Number$.m4s" startNumber="16" presentationTimeOffset="76250">
					<SegmentTimeline>
						<S t="76250" d="5000" r="14" />
					</SegmentTimeline>
				</SegmentTemplate>
			</Representation>
		</AdaptationSet>
	</Period>
</MPD>
)";
	mPrivateInstanceAAMP->rate = AAMP_NORMAL_PLAY_RATE;
	bool ret = false;
	/* Initialize MPD. The video/audio initialization segment is cached. */
	videoInitFragmentUrl = std::string(TEST_BASE_URL) + std::string("video_p0_init.mp4");
	audioInitFragmentUrl = std::string(TEST_BASE_URL) + std::string("audio_p0_init.mp4");
	EXPECT_CALL(*g_mockMediaStreamContext, CacheFragment(videoInitFragmentUrl, _, _, _, _, true, _, _, _, _, _))
		.Times(1)
		.WillOnce(Return(true));
	EXPECT_CALL(*g_mockMediaStreamContext, CacheFragment(audioInitFragmentUrl, _, _, _, _, true, _, _, _, _, _))
		.Times(1)
		.WillOnce(Return(true));
	// Seek to Period 1
	status = InitializeMPD(mManifest, eTUNETYPE_SEEK, 24.0);
	EXPECT_EQ(status, eAAMPSTATUS_OK);

	// Index the first period
	status = mTestableStreamAbstractionAAMP_MPD->InvokeIndexNewMPDDocument(false); (void)status;
	EXPECT_EQ(mTestableStreamAbstractionAAMP_MPD->GetCurrentPeriodIdx(), 0);
	EXPECT_EQ(mTestableStreamAbstractionAAMP_MPD->GetIteratorPeriodIdx(), 0);

	// Index the next period
	mTestableStreamAbstractionAAMP_MPD->IncrementIteratorPeriodIdx();
	bool periodChanged = false;
	bool adStateChanged = false;
	bool waitForAdBreakCatchup = false;
	bool requireStreamSelection = false;
	bool mpdChanged = false;
	std::string currentPeriodId = "p0";

	/*
	 * Test the scenario where period change happens
	 * The period is changed and requireStreamSelection is set to true
	 */
	ret = mTestableStreamAbstractionAAMP_MPD->InvokeSelectSourceOrAdPeriod(periodChanged, mpdChanged, adStateChanged, waitForAdBreakCatchup, requireStreamSelection, currentPeriodId);
	EXPECT_EQ(ret, true);
	EXPECT_EQ(requireStreamSelection, true);
	EXPECT_EQ(periodChanged, true);
	EXPECT_EQ(mTestableStreamAbstractionAAMP_MPD->GetCurrentPeriodIdx(), mTestableStreamAbstractionAAMP_MPD->GetIteratorPeriodIdx());
	EXPECT_EQ(currentPeriodId, "p3");
}

/**
 * @brief SelectSourceOrAdPeriod tests.
 *
 * The test verifies the scenario where the player transitions from an ad break (waiting to catch up)
 * to the next ad before the placement of that landing ad, validating state transitions and source selection logic.
 */
TEST_F(FetcherLoopTests, SelectSourceOrAdPeriodTests5)
{
	static const char *adManifest =
		R"(<?xml version="1.0" encoding="UTF-8"?>
<!-- A simple DASH manifest with a single ad period -->
<MPD xmlns="urn:mpeg:dash:schema:mpd:2011" type="static" profiles="urn:mpeg:dash:profile:isoff-on-demand:2011" minBufferTime="PT1.5S" mediaPresentationDuration="PT0M30S">
	<Period id="ad1" start="PT0H0M0.000S">
		<AdaptationSet id="0" contentType="video" mimeType="video/mp4" segmentAlignment="true" startWithSAP="1">
			<SegmentTemplate timescale="48000" initialization="video_init.mp4" media="video_$Number$.mp4" startNumber="1">
				<SegmentTimeline>
					<S t="0" d="96000" r="14"/>
				</SegmentTimeline>
			</SegmentTemplate>
			<Representation id="0" bandwidth="3000000" codecs="avc1.4d401f" width="1280" height="720" frameRate="30"/>
		</AdaptationSet>
		<AdaptationSet id="1" contentType="audio" lang="eng">
			<Representation id="1" mimeType="audio/mp4" codecs="ec-3" bandwidth="800000" width="640" height="360" frameRate="25"/>
			<SegmentTemplate timescale="48000" initialization="audio_init.mp4" media="audio_$Number$.m4s" startNumber="1">
				<SegmentTimeline>
					<S t="0" d="96000" r="14"/>
				</SegmentTimeline>
			</SegmentTemplate>
		</AdaptationSet>
	</Period>
</MPD>
)";

	std::string fragmentUrl;
	AAMPStatusType status;
	mPrivateInstanceAAMP->rate = 1.0;
	bool ret = false;

	// Expect initialization fragment to be cached
	fragmentUrl = std::string(TEST_BASE_URL) + std::string("video_p0_init.mp4");
	EXPECT_CALL(*g_mockMediaStreamContext, CacheFragment(fragmentUrl, _, _, _, _, true, _, _, _, _, _))
		.Times(1)
		.WillOnce(Return(true));

	// Initialize with live manifest and check status
	status = InitializeMPD(mLiveManifest, eTUNETYPE_SEEK, 0);
	EXPECT_EQ(status, eAAMPSTATUS_OK);

	// Initial indexing of MPD document
	status = mTestableStreamAbstractionAAMP_MPD->InvokeIndexNewMPDDocument(false);
	(void)status;
	EXPECT_EQ(mTestableStreamAbstractionAAMP_MPD->GetCurrentPeriodIdx(), 0);

	// Set the iterator to the current period
	mTestableStreamAbstractionAAMP_MPD->SetIteratorPeriodIdx(mTestableStreamAbstractionAAMP_MPD->GetCurrentPeriodIdx());

	// Prepare CDAI object to simulate the end of an ad break
	auto cdaiObj = mTestableStreamAbstractionAAMP_MPD->GetCDAIObject();
	cdaiObj->mAdState = AdState::IN_ADBREAK_WAIT2CATCHUP; // simulate waiting for base content after ad

	std::string periodId = "p0"; // ad period ID
	std::string endPeriodId = "p1"; // next period after the ad

	// Load ad manifest into a mock MPD object
	InitializeAdMPDObject(adManifest);

	// Set up ad breaks and ad metadata
	cdaiObj->mPeriodMap = {
		{periodId, Period2AdData()},
		{endPeriodId, Period2AdData()}};

	cdaiObj->mAdBreaks = {
		{periodId, AdBreakObject(30000, std::make_shared<std::vector<AdNode>>(), endPeriodId, 0, 30000)},
		{endPeriodId, AdBreakObject(30000, std::make_shared<std::vector<AdNode>>(), "", 0, 30000)}};

	// First ad is already placed and resolved; second is pending
	cdaiObj->mAdBreaks[periodId].ads->emplace_back(false, true, true, "adId1", "url", 30000, periodId, 0, nullptr);
	cdaiObj->mAdBreaks[endPeriodId].ads->emplace_back(false, false, false, "adId2", "url", 30000, endPeriodId, 0, mAdMPD);

	cdaiObj->mCurAdIdx = 0;
	cdaiObj->mCurAds = cdaiObj->mAdBreaks[periodId].ads;
	cdaiObj->mCurPlayingBreakId = periodId;
	cdaiObj->mAdBreaks[periodId].mAdBreakPlaced = true;

	// Move to next period for evaluation
	mTestableStreamAbstractionAAMP_MPD->IncrementIteratorPeriodIdx();

	// Track changes post invocation
	bool periodChanged = false;
	bool adStateChanged = false;
	bool waitForAdBreakCatchup = false;
	bool requireStreamSelection = false;
	bool mpdChanged = false;
	std::string currentPeriodId = "p0";

	// Set expectations for various AAMP and CDAI method calls
	EXPECT_CALL(*g_mockPrivateInstanceAAMP, GetTSBSessionManager()).WillRepeatedly(Return(nullptr));
	EXPECT_CALL(*g_mockPrivateInstanceAAMP, IsLocalAAMPTsbInjection()).WillRepeatedly(Return(false));
	EXPECT_CALL(*g_mockPrivateInstanceAAMP, SendAdPlacementEvent(_, _, _, _, _, _, _, _)).Times(1);
	EXPECT_CALL(*g_mockPrivateInstanceAAMP, SendAdReservationEvent(_, _, _, _, _)).Times(2);

	EXPECT_CALL(*g_MockPrivateCDAIObjectMPD, CheckForAdStart(_, _, _, _, _, _))
		.Times(AnyNumber())
		.WillOnce(Invoke([](const float &rate, bool init, const std::string &periodId, double offSet, std::string &breakId, double &adOffset)
		{
			breakId = "p1";
			return -1; // no ad triggered on first check
		}))
		.WillOnce(Invoke([](const float &rate, bool init, const std::string &periodId, double offSet, std::string &breakId, double &adOffset)
		{
			breakId = "p1";
			return 0; // ad triggered on second check
		}));

	EXPECT_CALL(*g_MockPrivateCDAIObjectMPD, isAdBreakObjectExist(_)).WillRepeatedly(Return(true));

	EXPECT_CALL(*g_MockPrivateCDAIObjectMPD, WaitForNextAdResolved(_)).Times(1).WillRepeatedly(Invoke([cdaiObj, endPeriodId](int timeout)
		{
			// Simulate resolution of next ad
			cdaiObj->mAdBreaks[endPeriodId].ads->at(0).placed = true;
			cdaiObj->mAdBreaks[endPeriodId].ads->at(0).resolved = true;
			cdaiObj->mAdBreaks[endPeriodId].invalid = false;
			return true;
		}));
	/*
	 * Now test the scenario where the player transitions from an ad break (waiting to catch up)
	 * to the next ad, validating state transitions and source selection logic
	 */
	ret = mTestableStreamAbstractionAAMP_MPD->InvokeSelectSourceOrAdPeriod(
		periodChanged, mpdChanged, adStateChanged, waitForAdBreakCatchup,
		requireStreamSelection, currentPeriodId);

	EXPECT_TRUE(ret);
	EXPECT_EQ(cdaiObj->mAdState, AdState::IN_ADBREAK_AD_PLAYING); // Validate expected state transition
}
