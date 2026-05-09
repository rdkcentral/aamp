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
#include <iterator>
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
#include "AampTrackWorker.hpp"

using ::testing::_;
using ::testing::AnyNumber;
using ::testing::Invoke;
using ::testing::NiceMock;
using ::testing::Return;
using ::testing::SetArgReferee;
using ::testing::StrictMock;
using ::testing::Invoke;
using ::testing::WithArg;
using ::testing::WithArgs;
using ::testing::WithoutArgs;

/**
 * @brief LinearTests tests common base class.
 */
class FetcherLoopTests : public ::testing::Test
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

		void SetIsFogTSB(bool value)
		{
			mIsFogTSB = value;
		}

		void SetPlayRate(float rate)
		{
			mPlayRate = rate;
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
			{eAAMPConfig_DisableAC4, false},
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
			{eAAMPConfig_InterruptHandling, false},
			{eAAMPConfig_UseMp4Demux, false},
};

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
			{eAAMPConfig_MaxDownloadBuffer, DEFAULT_MAX_DOWNLOAD_BUFFER},
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
			mPrivateInstanceAAMP->GetAampTrackWorkerManager()->RemoveWorkers();
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

		/* PrivateInstanceAAMP and the StreamAbstraction object should have the same rate. */
		mPrivateInstanceAAMP->rate = rate;
		/* Create MPD instance. */
		mTestableStreamAbstractionAAMP_MPD = new TestableStreamAbstractionAAMP_MPD(mPrivateInstanceAAMP, seekPos, rate);
		mCdaiObj = new CDAIObjectMPD(mPrivateInstanceAAMP);
		mTestableStreamAbstractionAAMP_MPD->SetCDAIObject(mCdaiObj);

		mPrivateInstanceAAMP->SetManifestUrl(TEST_MANIFEST_URL);

		/* Initialize MPD. */
		EXPECT_CALL(*g_mockPrivateInstanceAAMP, SetState(eSTATE_PREPARING, true));
		EXPECT_CALL(*g_mockPrivateInstanceAAMP, IsLiveStream())
			.Times(AnyNumber())
			.WillRepeatedly(Return(false));
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
	bool ret = false;
	/* Initialize MPD. The video initialization segment is cached. */
	fragmentUrl = std::string(TEST_BASE_URL) + std::string("video_p0_init.mp4");
	EXPECT_CALL(*g_mockMediaStreamContext, CacheFragment(fragmentUrl, _, _, _, _, true, _, _, _))
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
	bool ret = false;
	/* Initialize MPD. The video initialization segment is cached. */
	fragmentUrl = std::string(TEST_BASE_URL) + std::string("video_p1_init.mp4");
	EXPECT_CALL(*g_mockMediaStreamContext, CacheFragment(fragmentUrl, _, _, _, _, true, _, _, _))
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
	bool ret = false;

	/* Initialize MPD. The video initialization segment is cached. */
	fragmentUrl = std::string(TEST_BASE_URL) + std::string("video_p1_init.mp4");
	EXPECT_CALL(*g_mockMediaStreamContext, CacheFragment(fragmentUrl, _, _, _, _, true, _, _, _))
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
	bool ret = false;
	/* Initialize MPD. The video initialization segment is cached. */
	fragmentUrl = std::string(TEST_BASE_URL) + std::string("video_p0_init.mp4");
	EXPECT_CALL(*g_mockMediaStreamContext, CacheFragment(fragmentUrl, _, _, _, _, true, _, _, _))
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
 * @brief IndexSelectedPeriod test when Short Ad occurs and we return to play rest of base period
 *
 */
TEST_F(FetcherLoopTests, IndexSelectedPeriodTests3)
{

	bool periodChanged = false;
	bool adStateChanged = true;
	bool requireStreamSelection = false;
	std::string currentPeriodId = "p1";
	AAMPStatusType status;
	bool ret = false;
	/* important INFO from mLiveManifest2
	 * t="2816000" - presentationTimeOffset="1817600" = 998400
	 * 998400/12800 = 78.0 seconds
	 * i.e 78 seconds has been dropped from the beginning of the period
	 */
	static constexpr const char *liveManifest2 = R"(<?xml version="1.0" encoding="utf-8"?>
				<MPD xmlns="urn:mpeg:dash:schema:mpd:2011" availabilityStartTime="2023-01-01T00:00:00Z" maxSegmentDuration="PT2S" minBufferTime="PT4.000S" minimumUpdatePeriod="P100Y" profiles="urn:dvb:dash:profile:dvb-dash:2014,urn:dvb:dash:profile:dvb-dash:isoff-ext-live:2014" publishTime="2023-01-01T00:01:00Z" timeShiftBufferDepth="PT30S" type="dynamic">
						<Period id="p1" start="PT0S">
						<AdaptationSet id="0" contentType="video">
						<Representation id="1080p" mimeType="video/mp4" codecs="hvc1.1.6.L93.90" bandwidth="5000000" width="1920" height="1080" frameRate="25/1">
						<SegmentTemplate timescale="12800" initialization="dash/1080p_init.m4s" media="dash/1080p_$Number%03d$.m4s" startNumber="111" presentationTimeOffset="1817600">
						<SegmentTimeline>
						<S t="2816000" d="25600" r="14" />
						</SegmentTimeline>
						</SegmentTemplate>
						</Representation>
						</AdaptationSet>
						</Period>
				</MPD>
				)";
	/* Initialize MPD. The video initialization segment is cached. */
	EXPECT_CALL(*g_mockMediaStreamContext, CacheFragment(_, _, _, _, _, true, _, _, _))
		.Times(1)
		.WillOnce(Return(true));
	status = InitializeMPD(liveManifest2);
	EXPECT_EQ(status, eAAMPSTATUS_OK);

	status = mTestableStreamAbstractionAAMP_MPD->InvokeIndexNewMPDDocument(false);
	(void)status;

	MediaStreamContext *pMediaStreamContext = static_cast<MediaStreamContext *>(mTestableStreamAbstractionAAMP_MPD->GetMediaTrack(eTRACK_VIDEO));

	const double seekPos = 88;	// We want to seek 88 from the period start
	const double ptoDelta = 78; // Value determined by manifest. See comments in liveManifest2 for details.

	/* Relative to the period start:
	 * we want to seek to position seekPos (88s)
	 * but ptoDelta (78s) has been dropped from the
	 * beginning of the period, so the actual move forward will be seekPos - ptoDelta (10s)
	 */
	double initialFragTime = pMediaStreamContext->fragmentTime;
	EXPECT_EQ(pMediaStreamContext->fragmentDescriptor.Number, 118);
	auto cdaiObj = mTestableStreamAbstractionAAMP_MPD->GetCDAIObject();
	cdaiObj->mContentSeekOffset = seekPos;

	ret = mTestableStreamAbstractionAAMP_MPD->InvokeIndexSelectedPeriod(periodChanged, adStateChanged, requireStreamSelection, currentPeriodId);
	double positionMove = pMediaStreamContext->fragmentTime - initialFragTime;
	AAMPLOG_INFO("fragmentTime %f initialFragTime  %f", pMediaStreamContext->fragmentTime, initialFragTime);
	EXPECT_TRUE((positionMove >= seekPos - ptoDelta) && (positionMove <= seekPos - ptoDelta + 1)); // Seems like it rounds to 11Sec
	EXPECT_EQ(pMediaStreamContext->fragmentDescriptor.Number, 123);								   // 123 -118 = 5 segments which is 10Sec
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
	/* Initialize MPD. The video initialization segment is cached. */
	fragmentUrl = std::string(TEST_BASE_URL) + std::string("video_p0_init.mp4");
	EXPECT_CALL(*g_mockMediaStreamContext, CacheFragment(fragmentUrl, _, _, _, _, true, _, _, _))
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
	/* Initialize MPD. The video initialization segment is cached. */
	fragmentUrl = std::string(TEST_BASE_URL) + std::string("video_p0_init.mp4");
	EXPECT_CALL(*g_mockMediaStreamContext, CacheFragment(fragmentUrl, _, _, _, _, true, _, _, _))
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
	EXPECT_CALL(*g_mockMediaStreamContext, CacheFragment(fragmentUrl, _, _, _, _, true, true, _, _))
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
	/* Initialize MPD. The video initialization segment is cached. */
	fragmentUrl = std::string(TEST_BASE_URL) + std::string("video_p0_init.mp4");
	EXPECT_CALL(*g_mockMediaStreamContext, CacheFragment(fragmentUrl, _, _, _, _, true, _, _, _))
		.WillOnce(Return(true));
	EXPECT_CALL(*g_mockMediaStreamContext, CacheFragment(_, _, _, _, _, false, _, _, _))
		.WillRepeatedly(Return(true));
	EXPECT_CALL(*g_mockPrivateInstanceAAMP, IsLocalAAMPTsbInjection()).WillRepeatedly(Return(false));
	status = InitializeMPD(mVodManifest);
	EXPECT_EQ(status, eAAMPSTATUS_OK);

	/* Push the first video segment to present.
	 * The segment starts at time 40.0s and has a duration of 2.0s.
	 */
	fragmentUrl = std::string(TEST_BASE_URL) + std::string("video_p1_init.mp4");
	EXPECT_CALL(*g_mockMediaStreamContext, CacheFragment(fragmentUrl, _, _, _, _, true, _, _, _))
		.WillOnce(Return(true));
	EXPECT_CALL(*g_mockMediaStreamContext, CacheFragment(_, _, _, _, _, false, _, _, _))
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
	/* Initialize MPD. The video initialization segment is cached. */
	fragmentUrl = std::string(TEST_BASE_URL) + std::string("video_p0_init.mp4");
	EXPECT_CALL(*g_mockMediaStreamContext, CacheFragment(fragmentUrl, _, _, _, _, true, _, _, _))
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
	EXPECT_CALL(*g_mockMediaStreamContext, CacheFragment(fragmentUrl, _, _, _, _, true, _, _, _))
		.Times(1)
		.WillOnce(Return(true));
	EXPECT_CALL(*g_mockMediaStreamContext, CacheFragment(_, _, _, _, _, false, _, _, _))
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
	bool ret = false;
	/* Initialize MPD. The video initialization segment is cached. */
	fragmentUrl = std::string(TEST_BASE_URL) + std::string("video_p0_init.mp4");
	EXPECT_CALL(*g_mockMediaStreamContext, CacheFragment(fragmentUrl, _, _, _, _, true, _, _, _))
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
	EXPECT_CALL(*g_mockPrivateInstanceAAMP, SendAdReservationEvent(_, _, _, _, _, _)).Times(AnyNumber());

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

/**
 * @brief GenerateFragmentURLList tests.
 *
 * The tests verify the GenerateFragmentURLList method of StreamAbstractionAAMP_MPD for
 * video-only manifest scenarios. The function should generate URLs for all representations
 * in the adaptation set at a specific Number/Time specified in the MediaStreamContext.
 */
TEST_F(FetcherLoopTests, GenerateFragmentURLListVideoOnly)
{
	static const char *videoOnlyManifest = R"(<?xml version="1.0" encoding="utf-8"?>
		<MPD xmlns="urn:mpeg:dash:schema:mpd:2011" availabilityStartTime="2023-01-01T00:00:00Z" maxSegmentDuration="PT2S" minBufferTime="PT4.000S" minimumUpdatePeriod="P100Y" profiles="urn:dvb:dash:profile:dvb-dash:2014,urn:dvb:dash:profile:dvb-dash:isoff-ext-live:2014" publishTime="2023-01-01T00:01:00Z" timeShiftBufferDepth="PT5M" type="static">
			<Period id="p0" start="PT0S">
				<AdaptationSet id="0" contentType="video">
					<Representation id="0" mimeType="video/mp4" codecs="avc1.640028" bandwidth="800000" width="640" height="360" frameRate="25">
						<SegmentTemplate timescale="2500" initialization="video_p0_init.mp4" media="video_p0_$Number$.m4s" startNumber="1">
							<SegmentTimeline>
								<S t="0" d="5000" r="9" />
							</SegmentTimeline>
						</SegmentTemplate>
					</Representation>
				</AdaptationSet>
			</Period>
		</MPD>
		)";

	std::string fragmentUrl;
	AAMPStatusType status;

	/* Initialize MPD. The video initialization segment is cached. */
	fragmentUrl = std::string(TEST_BASE_URL) + std::string("video_p0_init.mp4");
	EXPECT_CALL(*g_mockMediaStreamContext, CacheFragment(fragmentUrl, _, _, _, _, true, _, _, _))
		.Times(1)
		.WillOnce(Return(true));

	status = InitializeMPD(videoOnlyManifest);
	EXPECT_EQ(status, eAAMPSTATUS_OK);

	// Index the MPD document
	status = mTestableStreamAbstractionAAMP_MPD->InvokeIndexNewMPDDocument(false);
	EXPECT_EQ(status, eAAMPSTATUS_OK);

	MediaTrack *videoTrack = mTestableStreamAbstractionAAMP_MPD->GetMediaTrack(eTRACK_VIDEO);
	EXPECT_NE(videoTrack, nullptr);

	MediaStreamContext *pVideoContext = static_cast<MediaStreamContext *>(videoTrack);
	EXPECT_NE(pVideoContext, nullptr);

	// Test GenerateFragmentURLList for video track
	// Set fragment descriptor for segment 5 (Number=5, Time=20000 which is 5th segment at 5000 duration each)
	pVideoContext->fragmentDescriptor.Number = 5;
	pVideoContext->fragmentDescriptor.Time = 20000;  // 5th segment (4*5000 offset)
	pVideoContext->fragmentDescriptor.TimeScale = 2500;
	pVideoContext->fragmentDescriptor.Bandwidth = 800000;

	URLBitrateMap uriList;
	mTestableStreamAbstractionAAMP_MPD->GenerateFragmentURLList(uriList, pVideoContext, false);

	// Should generate URL for the single representation at the specified Number/Time
	EXPECT_EQ(uriList.size(), 1);
	EXPECT_NE(uriList.find(800000), uriList.end());

	// Verify URL format contains correct segment number
	const auto& url = uriList[800000].url;
	EXPECT_TRUE(url.find("video_p0_5.m4s") != std::string::npos);

	// Test with initialization segment
	URLBitrateMap initUriList;
	mTestableStreamAbstractionAAMP_MPD->GenerateFragmentURLList(initUriList, pVideoContext, true);

	EXPECT_EQ(initUriList.size(), 1);
	EXPECT_NE(initUriList.find(800000), initUriList.end());
	const auto& initUrl = initUriList[800000].url;
	EXPECT_TRUE(initUrl.find("video_p0_init.mp4") != std::string::npos);
}

/**
 * @brief GenerateFragmentURLList tests.
 *
 * The test verifies the GenerateFragmentURLList method behavior for a fog TSB
 * scenario spanning two periods:
 * - Period p0 contains a custom AvailableBitrates node.
 * - Period p1 contains normal representations (ad playing from CDN), with no
 *   AvailableBitrates node.
 *
 * Expected behavior:
 * - For p0 init fragment generation, URLs are generated keyed by the
 *   AvailableBitrates bandwidths.
 * - For p1 init fragment generation, URLs are generated keyed by the
 *   representation bandwidths.
 */
TEST_F(FetcherLoopTests, GenerateFragmentURLListFogPlayingAdFromCDN)
{
	static const char *fogTwoPeriodManifest = R"(<?xml version="1.0" encoding="utf-8"?>
		<MPD xmlns="urn:mpeg:dash:schema:mpd:2011" availabilityStartTime="2023-01-01T00:00:00Z" maxSegmentDuration="PT2S" minBufferTime="PT4.000S" minimumUpdatePeriod="P100Y" profiles="urn:dvb:dash:profile:dvb-dash:2014,urn:dvb:dash:profile:dvb-dash:isoff-ext-live:2014" publishTime="2023-01-01T00:01:00Z" timeShiftBufferDepth="PT5M" type="static">
			<Period id="p0" start="PT0S">
				<AdaptationSet id="0" contentType="video">
					<AvailableBitrates>
						<Representation bandwidth="400000" width="640" height="360" />
						<Representation bandwidth="1200000" width="1280" height="720" />
					</AvailableBitrates>
					<Representation id="fog_video" mimeType="video/mp4" codecs="avc1.640028" bandwidth="800000" width="640" height="360" frameRate="25">
						<SegmentTemplate timescale="2500" initialization="fog_p0_init.mp4" media="fog_p0_$Number$.m4s" startNumber="1">
							<SegmentTimeline>
								<S t="0" d="5000" r="1" />
							</SegmentTimeline>
						</SegmentTemplate>
					</Representation>
				</AdaptationSet>
			</Period>
			<Period id="p1" start="PT10S">
				<AdaptationSet id="1" contentType="video">
					<Representation id="cdn_low" mimeType="video/mp4" codecs="avc1.640028" bandwidth="700000" width="640" height="360" frameRate="25">
						<SegmentTemplate timescale="2500" initialization="cdn_p1_low_init.mp4" media="cdn_p1_low_$Number$.m4s" startNumber="1">
							<SegmentTimeline>
								<S t="0" d="5000" r="1" />
							</SegmentTimeline>
						</SegmentTemplate>
					</Representation>
					<Representation id="cdn_high" mimeType="video/mp4" codecs="avc1.640028" bandwidth="1400000" width="1280" height="720" frameRate="25">
						<SegmentTemplate timescale="2500" initialization="cdn_p1_high_init.mp4" media="cdn_p1_high_$Number$.m4s" startNumber="1">
							<SegmentTimeline>
								<S t="0" d="5000" r="1" />
							</SegmentTimeline>
						</SegmentTemplate>
					</Representation>
				</AdaptationSet>
			</Period>
		</MPD>
		)";

	AAMPStatusType status;

	/* Allow Init() to cache any init fragments. */
	EXPECT_CALL(*g_mockMediaStreamContext, CacheFragment(_, _, _, _, _, true, _, _, _))
		.Times(AnyNumber())
		.WillRepeatedly(Return(true));

	status = InitializeMPD(fogTwoPeriodManifest);
	EXPECT_EQ(status, eAAMPSTATUS_OK);

	status = mTestableStreamAbstractionAAMP_MPD->InvokeIndexNewMPDDocument(false);
	EXPECT_EQ(status, eAAMPSTATUS_OK);

	MediaTrack *videoTrack = mTestableStreamAbstractionAAMP_MPD->GetMediaTrack(eTRACK_VIDEO);
	ASSERT_NE(videoTrack, nullptr);

	MediaStreamContext *pVideoContext = static_cast<MediaStreamContext *>(videoTrack);
	ASSERT_NE(pVideoContext, nullptr);

	pVideoContext->fragmentDescriptor.Number = 1;
	pVideoContext->fragmentDescriptor.Time = 0;
	pVideoContext->fragmentDescriptor.TimeScale = 2500;

	/* Enable fog TSB path. */
	mTestableStreamAbstractionAAMP_MPD->SetIsFogTSB(true);

	/* Period p0: AvailableBitrates present -> keyed by AvailableBitrates bandwidths. */
	EXPECT_EQ(mTestableStreamAbstractionAAMP_MPD->GetCurrentPeriod()->GetId(), std::string("p0"));
	URLBitrateMap initUriListP0;
	mTestableStreamAbstractionAAMP_MPD->GenerateFragmentURLList(initUriListP0, pVideoContext, true);

	EXPECT_EQ(initUriListP0.size(), 2);
	EXPECT_NE(initUriListP0.find(400000), initUriListP0.end());
	EXPECT_NE(initUriListP0.find(1200000), initUriListP0.end());
	EXPECT_EQ(initUriListP0.find(800000), initUriListP0.end());
	EXPECT_TRUE(initUriListP0[400000].url.find("fog_p0_init.mp4") != std::string::npos);
	EXPECT_TRUE(initUriListP0[1200000].url.find("fog_p0_init.mp4") != std::string::npos);

	/* Period p1: no AvailableBitrates -> keyed by representation bandwidths. */
	mTestableStreamAbstractionAAMP_MPD->IncrementCurrentPeriodIdx();
	mTestableStreamAbstractionAAMP_MPD->SetCurrentPeriod(mTestableStreamAbstractionAAMP_MPD->GetMPD()->GetPeriods().at(1));
	EXPECT_EQ(mTestableStreamAbstractionAAMP_MPD->GetCurrentPeriod()->GetId(), std::string("p1"));
	URLBitrateMap initUriListP1;
	mTestableStreamAbstractionAAMP_MPD->GenerateFragmentURLList(initUriListP1, pVideoContext, true);

	EXPECT_EQ(initUriListP1.size(), 2);
	EXPECT_NE(initUriListP1.find(700000), initUriListP1.end());
	EXPECT_NE(initUriListP1.find(1400000), initUriListP1.end());
	EXPECT_TRUE(initUriListP1[700000].url.find("cdn_p1_low_init.mp4") != std::string::npos);
	EXPECT_TRUE(initUriListP1[1400000].url.find("cdn_p1_high_init.mp4") != std::string::npos);
}

/**
 * @brief GenerateFragmentURLList tests.
 *
 * The tests verify the GenerateFragmentURLList method of StreamAbstractionAAMP_MPD for
 * intra-asset content with representations split in multiple adaptations. Tests that
 * given a specific Number/Time in the MediaStreamContext, the function generates URLs
 * for ALL representations in the current adaptation set at that same Number/Time.
 */
TEST_F(FetcherLoopTests, GenerateFragmentURLListIntraAssetMultipleAdaptations)
{
	static const char *multiAdaptationManifest = R"(<?xml version="1.0" encoding="utf-8"?>
		<MPD xmlns="urn:mpeg:dash:schema:mpd:2011" availabilityStartTime="2023-01-01T00:00:00Z" maxSegmentDuration="PT2S" minBufferTime="PT4.000S" minimumUpdatePeriod="P100Y" profiles="urn:dvb:dash:profile:dvb-dash:2014,urn:dvb:dash:profile:dvb-dash:isoff-ext-live:2014" publishTime="2023-01-01T00:01:00Z" timeShiftBufferDepth="PT5M" type="static">
			<Period id="p0" start="PT0S">
				<AdaptationSet id="0" contentType="video" intraAssetType="main">
					<Representation id="video_main_low" mimeType="video/mp4" codecs="avc1.640028" bandwidth="800000" width="640" height="360" frameRate="25">
						<SegmentTemplate timescale="2500" initialization="video_main_low_init.mp4" media="video_main_low_$Number$.m4s" startNumber="1">
							<SegmentTimeline>
								<S t="0" d="5000" r="14" />
							</SegmentTimeline>
						</SegmentTemplate>
					</Representation>
					<Representation id="video_main_high" mimeType="video/mp4" codecs="avc1.640028" bandwidth="2000000" width="1280" height="720" frameRate="25">
						<SegmentTemplate timescale="2500" initialization="video_main_high_init.mp4" media="video_main_high_$Number$.m4s" startNumber="1">
							<SegmentTimeline>
								<S t="0" d="5000" r="14" />
							</SegmentTimeline>
						</SegmentTemplate>
					</Representation>
				</AdaptationSet>
				<AdaptationSet id="1" contentType="video" intraAssetType="commentary">
					<Representation id="video_commentary" mimeType="video/mp4" codecs="avc1.640028" bandwidth="400000" width="320" height="240" frameRate="25">
						<SegmentTemplate timescale="2500" initialization="video_commentary_init.mp4" media="video_commentary_$Number$.m4s" startNumber="1">
							<SegmentTimeline>
								<S t="0" d="5000" r="14" />
							</SegmentTimeline>
						</SegmentTemplate>
					</Representation>
				</AdaptationSet>
				<AdaptationSet id="2" contentType="video" intraAssetType="alternate">
					<Representation id="video_alternate_1" mimeType="video/mp4" codecs="avc1.640028" bandwidth="1200000" width="960" height="540" frameRate="25">
						<SegmentTemplate timescale="2500" initialization="video_alt1_init.mp4" media="video_alt1_$Number$.m4s" startNumber="1">
							<SegmentTimeline>
								<S t="0" d="5000" r="14" />
							</SegmentTimeline>
						</SegmentTemplate>
					</Representation>
					<Representation id="video_alternate_2" mimeType="video/mp4" codecs="avc1.640028" bandwidth="1600000" width="1024" height="576" frameRate="25">
						<SegmentTemplate timescale="2500" initialization="video_alt2_init.mp4" media="video_alt2_$Number$.m4s" startNumber="1">
							<SegmentTimeline>
								<S t="0" d="5000" r="14" />
							</SegmentTimeline>
						</SegmentTemplate>
					</Representation>
				</AdaptationSet>
				<AdaptationSet id="3" contentType="audio" lang="eng">
					<Representation id="audio_eng" mimeType="audio/mp4" codecs="ec-3" bandwidth="128000">
						<SegmentTemplate timescale="2500" initialization="audio_eng_init.mp4" media="audio_eng_$Number$.m4s" startNumber="1">
							<SegmentTimeline>
								<S t="0" d="5000" r="14" />
							</SegmentTimeline>
						</SegmentTemplate>
					</Representation>
				</AdaptationSet>
			</Period>
		</MPD>
		)";

	std::string fragmentUrl;
	AAMPStatusType status;

	/* Initialize MPD. The video and audio initialization segments are cached. */
	//fragmentUrl = std::string(TEST_BASE_URL) + std::string("video_main_low_init.mp4");
	EXPECT_CALL(*g_mockMediaStreamContext, CacheFragment(_, _, _, _, _, true, _, _, _))
		.Times(1)
		.WillOnce(Return(true));

	fragmentUrl = std::string(TEST_BASE_URL) + std::string("audio_eng_init.mp4");
	EXPECT_CALL(*g_mockMediaStreamContext, CacheFragment(fragmentUrl, _, _, _, _, true, _, _, _))
		.Times(1)
		.WillOnce(Return(true));

	status = InitializeMPD(multiAdaptationManifest);
	EXPECT_EQ(status, eAAMPSTATUS_OK);

	// Index the MPD document
	status = mTestableStreamAbstractionAAMP_MPD->InvokeIndexNewMPDDocument(false);
	EXPECT_EQ(status, eAAMPSTATUS_OK);

	MediaTrack *videoTrack = mTestableStreamAbstractionAAMP_MPD->GetMediaTrack(eTRACK_VIDEO);
	EXPECT_NE(videoTrack, nullptr);

	MediaStreamContext *pVideoContext = static_cast<MediaStreamContext *>(videoTrack);
	EXPECT_NE(pVideoContext, nullptr);

	// Test GenerateFragmentURLList for video adaptation with multiple representations
	// Set fragment descriptor for segment 3 at time 10000
	pVideoContext->fragmentDescriptor.Number = 3;
	pVideoContext->fragmentDescriptor.Time = 10000;
	pVideoContext->fragmentDescriptor.TimeScale = 2500;
	pVideoContext->fragmentDescriptor.Bandwidth = 800000;  // Current bitrate

	// Test media segments - should generate URLs for BOTH representations in the adaptation set
	URLBitrateMap uriList;
	mTestableStreamAbstractionAAMP_MPD->GenerateFragmentURLList(uriList, pVideoContext, false);

	// Should generate URLs for all unique representations (5) in the adaptation set
	EXPECT_EQ(uriList.size(), 5);

	// Verify low bitrate representation
	EXPECT_NE(uriList.find(800000), uriList.end());
	const auto& lowUrl = uriList[800000].url;
	EXPECT_TRUE(lowUrl.find("video_main_low_3.m4s") != std::string::npos);

	// Verify high bitrate representation
	EXPECT_NE(uriList.find(2000000), uriList.end());
	const auto& highUrl = uriList[2000000].url;
	EXPECT_TRUE(highUrl.find("video_main_high_3.m4s") != std::string::npos);

	// Test initialization segments - should also generate for both representations
	URLBitrateMap initUriList;
	mTestableStreamAbstractionAAMP_MPD->GenerateFragmentURLList(initUriList, pVideoContext, true);

	// Should generate init URLs for all unique representations (5) in the adaptation set
	EXPECT_EQ(initUriList.size(), 5);

	// Verify init segments
	EXPECT_NE(initUriList.find(800000), initUriList.end());
	EXPECT_TRUE(initUriList[800000].url.find("video_main_low_init.mp4") != std::string::npos);

	EXPECT_NE(initUriList.find(2000000), initUriList.end());
	EXPECT_TRUE(initUriList[2000000].url.find("video_main_high_init.mp4") != std::string::npos);
}

/**
 * @brief GenerateFragmentURLList tests.
 *
 * The tests verify the GenerateFragmentURLList method behavior with edge cases
 * such as null context. Tests that the function handles null input gracefully and
 * generates proper URLs when given valid MediaStreamContext with Number/Time set.
 */
TEST_F(FetcherLoopTests, GenerateFragmentURLListEdgeCases)
{
	static const char *simpleManifest = R"(<?xml version="1.0" encoding="utf-8"?>
		<MPD xmlns="urn:mpeg:dash:schema:mpd:2011" availabilityStartTime="2023-01-01T00:00:00Z" maxSegmentDuration="PT2S" minBufferTime="PT4.000S" minimumUpdatePeriod="P100Y" profiles="urn:dvb:dash:profile:dvb-dash:2014,urn:dvb:dash:profile:dvb-dash:isoff-ext-live:2014" publishTime="2023-01-01T00:01:00Z" timeShiftBufferDepth="PT5M" type="static">
			<Period id="p0" start="PT0S">
				<AdaptationSet id="0" contentType="video">
					<Representation id="0" mimeType="video/mp4" codecs="avc1.640028" bandwidth="800000" width="640" height="360" frameRate="25">
						<SegmentTemplate timescale="2500" initialization="video_init.mp4" media="video_$Number$.m4s" startNumber="1">
							<SegmentTimeline>
								<S t="0" d="5000" r="4" />
							</SegmentTimeline>
						</SegmentTemplate>
					</Representation>
				</AdaptationSet>
			</Period>
		</MPD>
		)";

	std::string fragmentUrl;
	AAMPStatusType status;

	fragmentUrl = std::string(TEST_BASE_URL) + std::string("video_init.mp4");
	EXPECT_CALL(*g_mockMediaStreamContext, CacheFragment(fragmentUrl, _, _, _, _, true, _, _, _))
		.Times(1)
		.WillOnce(Return(true));

	status = InitializeMPD(simpleManifest);
	EXPECT_EQ(status, eAAMPSTATUS_OK);

	status = mTestableStreamAbstractionAAMP_MPD->InvokeIndexNewMPDDocument(false);
	EXPECT_EQ(status, eAAMPSTATUS_OK);

	MediaTrack *videoTrack = mTestableStreamAbstractionAAMP_MPD->GetMediaTrack(eTRACK_VIDEO);
	EXPECT_NE(videoTrack, nullptr);
	MediaStreamContext *pVideoContext = static_cast<MediaStreamContext *>(videoTrack);

	URLBitrateMap uriList;

	// Test with null context - should handle gracefully
	uriList.clear();
	mTestableStreamAbstractionAAMP_MPD->GenerateFragmentURLList(uriList, nullptr, false);
	EXPECT_TRUE(uriList.empty());

	// Test with valid context and proper fragment descriptor
	pVideoContext->fragmentDescriptor.Number = 2;
	pVideoContext->fragmentDescriptor.Time = 5000;
	pVideoContext->fragmentDescriptor.TimeScale = 2500;
	pVideoContext->fragmentDescriptor.Bandwidth = 800000;

	uriList.clear();
	mTestableStreamAbstractionAAMP_MPD->GenerateFragmentURLList(uriList, pVideoContext, false);

	// Should generate URL for the representation
	EXPECT_FALSE(uriList.empty());
	EXPECT_NE(uriList.find(800000), uriList.end());
	EXPECT_TRUE(uriList[800000].url.find("video_2.m4s") != std::string::npos);

	// Test init segment
	uriList.clear();
	mTestableStreamAbstractionAAMP_MPD->GenerateFragmentURLList(uriList, pVideoContext, true);
	EXPECT_FALSE(uriList.empty());
	EXPECT_TRUE(uriList[800000].url.find("video_init.mp4") != std::string::npos);
}

/**
 * @brief GenerateFragmentURLList tests.
 *
 * The tests verify the GenerateFragmentURLList method with SegmentList format
 * instead of SegmentTemplate. Tests that given a fragment index in MediaStreamContext,
 * the function generates URLs for all representations at that same fragment index.
 */
TEST_F(FetcherLoopTests, GenerateFragmentURLListSegmentList)
{
	static const char *segmentListManifest = R"(<?xml version="1.0" encoding="utf-8"?>
		<MPD xmlns="urn:mpeg:dash:schema:mpd:2011" availabilityStartTime="2023-01-01T00:00:00Z" maxSegmentDuration="PT2S" minBufferTime="PT4.000S" minimumUpdatePeriod="P100Y" profiles="urn:dvb:dash:profile:dvb-dash:2014,urn:dvb:dash:profile:dvb-dash:isoff-ext-live:2014" publishTime="2023-01-01T00:01:00Z" timeShiftBufferDepth="PT5M" type="static">
			<Period id="p0" start="PT0S">
				<AdaptationSet id="0" contentType="video">
					<Representation id="0" mimeType="video/mp4" codecs="avc1.640028" bandwidth="800000" width="640" height="360" frameRate="25">
						<SegmentList timescale="2500" duration="5000">
							<Initialization sourceURL="video_init.mp4"/>
							<SegmentURL media="video_seg1.m4s"/>
							<SegmentURL media="video_seg2.m4s"/>
							<SegmentURL media="video_seg3.m4s"/>
							<SegmentURL media="video_seg4.m4s"/>
							<SegmentURL media="video_seg5.m4s"/>
						</SegmentList>
					</Representation>
				</AdaptationSet>
				<AdaptationSet id="1" contentType="video" intraAssetType="alternate">
					<Representation id="1" mimeType="video/mp4" codecs="avc1.640028" bandwidth="1200000" width="960" height="540" frameRate="25">
						<SegmentList timescale="2500" duration="5000">
							<Initialization sourceURL="video_alt_init.mp4"/>
							<SegmentURL media="video_alt_seg1.m4s"/>
							<SegmentURL media="video_alt_seg2.m4s"/>
							<SegmentURL media="video_alt_seg3.m4s"/>
							<SegmentURL media="video_alt_seg4.m4s"/>
							<SegmentURL media="video_alt_seg5.m4s"/>
						</SegmentList>
					</Representation>
				</AdaptationSet>
			</Period>
		</MPD>
		)";

	std::string fragmentUrl;
	AAMPStatusType status;

	fragmentUrl = std::string(TEST_BASE_URL) + std::string("video_init.mp4");
	EXPECT_CALL(*g_mockMediaStreamContext, CacheFragment(fragmentUrl, _, _, _, _, true, _, _, _))
		.Times(1)
		.WillOnce(Return(true));

	status = InitializeMPD(segmentListManifest);
	EXPECT_EQ(status, eAAMPSTATUS_OK);

	status = mTestableStreamAbstractionAAMP_MPD->InvokeIndexNewMPDDocument(false);
	EXPECT_EQ(status, eAAMPSTATUS_OK);

	MediaTrack *videoTrack = mTestableStreamAbstractionAAMP_MPD->GetMediaTrack(eTRACK_VIDEO);
	EXPECT_NE(videoTrack, nullptr);
	MediaStreamContext *pVideoContext = static_cast<MediaStreamContext *>(videoTrack);

	URLBitrateMap uriList;

	// Set fragment index to 2 (3rd segment - 0-indexed)
	pVideoContext->fragmentIndex = 2;
	pVideoContext->fragmentDescriptor.Bandwidth = 800000;

	// Test media segment from SegmentList - should generate URLs for all representations
	mTestableStreamAbstractionAAMP_MPD->GenerateFragmentURLList(uriList, pVideoContext, false);

	// Should generate URLs for both representations (main and alternate) at the same fragment index
	EXPECT_EQ(uriList.size(), 2);

	// Verify main representation (800kbps)
	EXPECT_NE(uriList.find(800000), uriList.end());
	const auto& mainUrl = uriList[800000].url;
	EXPECT_TRUE(mainUrl.find("video_seg3.m4s") != std::string::npos);

	// Verify alternate representation (1200kbps)
	EXPECT_NE(uriList.find(1200000), uriList.end());
	const auto& altUrl = uriList[1200000].url;
	EXPECT_TRUE(altUrl.find("video_alt_seg3.m4s") != std::string::npos);

	// Test initialization segments
	uriList.clear();
	mTestableStreamAbstractionAAMP_MPD->GenerateFragmentURLList(uriList, pVideoContext, true);

	EXPECT_EQ(uriList.size(), 2);

	// Verify init URLs for both representations
	EXPECT_NE(uriList.find(800000), uriList.end());
	EXPECT_TRUE(uriList[800000].url.find("video_init.mp4") != std::string::npos);

	EXPECT_NE(uriList.find(1200000), uriList.end());
	EXPECT_TRUE(uriList[1200000].url.find("video_alt_init.mp4") != std::string::npos);

	// Test with different fragment index
	pVideoContext->fragmentIndex = 0;
	uriList.clear();
	mTestableStreamAbstractionAAMP_MPD->GenerateFragmentURLList(uriList, pVideoContext, false);

	EXPECT_EQ(uriList.size(), 2);
	EXPECT_TRUE(uriList[800000].url.find("video_seg1.m4s") != std::string::npos);
	EXPECT_TRUE(uriList[1200000].url.find("video_alt_seg1.m4s") != std::string::npos);
}

/**
 * @brief GenerateFragmentURLList tests with blacklisted adaptations.
 *
 * The tests verify that GenerateFragmentURLList correctly filters out blacklisted
 * adaptation sets and only generates URLs for non-blacklisted adaptations.
 */
TEST_F(FetcherLoopTests, GenerateFragmentURLListWithBlacklistedAdaptations)
{
	static const char *multiAdaptationManifest = R"(<?xml version="1.0" encoding="utf-8"?>
		<MPD xmlns="urn:mpeg:dash:schema:mpd:2011" availabilityStartTime="2023-01-01T00:00:00Z" maxSegmentDuration="PT2S" minBufferTime="PT4.000S" minimumUpdatePeriod="P100Y" profiles="urn:dvb:dash:profile:dvb-dash:2014,urn:dvb:dash:profile:dvb-dash:isoff-ext-live:2014" publishTime="2023-01-01T00:01:00Z" timeShiftBufferDepth="PT5M" type="static">
			<Period id="p0" start="PT0S">
				<AdaptationSet id="0" contentType="video">
					<Representation id="video_low" mimeType="video/mp4" codecs="avc1.640028" bandwidth="800000" width="640" height="360" frameRate="25">
						<SegmentTemplate timescale="2500" initialization="video_low_init.mp4" media="video_low_$Number$.m4s" startNumber="1">
							<SegmentTimeline>
								<S t="0" d="5000" r="14" />
							</SegmentTimeline>
						</SegmentTemplate>
					</Representation>
					<Representation id="video_high" mimeType="video/mp4" codecs="avc1.640028" bandwidth="2000000" width="1280" height="720" frameRate="25">
						<SegmentTemplate timescale="2500" initialization="video_high_init.mp4" media="video_high_$Number$.m4s" startNumber="1">
							<SegmentTimeline>
								<S t="0" d="5000" r="14" />
							</SegmentTimeline>
						</SegmentTemplate>
					</Representation>
				</AdaptationSet>
				<AdaptationSet id="1" contentType="video" intraAssetType="alternate">
					<Representation id="video_alt1" mimeType="video/mp4" codecs="avc1.640028" bandwidth="1200000" width="960" height="540" frameRate="25">
						<SegmentTemplate timescale="2500" initialization="video_alt1_init.mp4" media="video_alt1_$Number$.m4s" startNumber="1">
							<SegmentTimeline>
								<S t="0" d="5000" r="14" />
							</SegmentTimeline>
						</SegmentTemplate>
					</Representation>
					<Representation id="video_alt2" mimeType="video/mp4" codecs="avc1.640028" bandwidth="1600000" width="1024" height="576" frameRate="25">
						<SegmentTemplate timescale="2500" initialization="video_alt2_init.mp4" media="video_alt2_$Number$.m4s" startNumber="1">
							<SegmentTimeline>
								<S t="0" d="5000" r="14" />
							</SegmentTimeline>
						</SegmentTemplate>
					</Representation>
				</AdaptationSet>
				<AdaptationSet id="2" contentType="video" intraAssetType="alternate2">
					<Representation id="video_alt3" mimeType="video/mp4" codecs="avc1.640028" bandwidth="3000000" width="1920" height="1080" frameRate="30">
						<SegmentTemplate timescale="2500" initialization="video_alt3_init.mp4" media="video_alt3_$Number$.m4s" startNumber="1">
							<SegmentTimeline>
								<S t="0" d="5000" r="14" />
							</SegmentTimeline>
						</SegmentTemplate>
					</Representation>
				</AdaptationSet>
				<AdaptationSet id="3" contentType="audio" lang="eng">
					<Representation id="audio_eng" mimeType="audio/mp4" codecs="ec-3" bandwidth="128000">
						<SegmentTemplate timescale="2500" initialization="audio_eng_init.mp4" media="audio_eng_$Number$.m4s" startNumber="1">
							<SegmentTimeline>
								<S t="0" d="5000" r="14" />
							</SegmentTimeline>
						</SegmentTemplate>
					</Representation>
				</AdaptationSet>
			</Period>
		</MPD>
		)";

	std::string fragmentUrl;
	AAMPStatusType status;

	// Setup blacklist: Adaptation set 1 is blacklisted
	StreamBlacklistProfileInfo blInfo;
	blInfo.mPeriodId = "p0";
	blInfo.mAdaptationSetIdx = 1; // Blacklist adaptation set 1
	blInfo.mReason = PROFILE_BLACKLIST_DRM_FAILURE;
	mPrivateInstanceAAMP->AddToBlacklistedProfiles(blInfo);

	/* Initialize MPD */
	EXPECT_CALL(*g_mockMediaStreamContext, CacheFragment(_, _, _, _, _, true, _, _, _))
		.Times(1)
		.WillOnce(Return(true));

	fragmentUrl = std::string(TEST_BASE_URL) + std::string("audio_eng_init.mp4");
	EXPECT_CALL(*g_mockMediaStreamContext, CacheFragment(fragmentUrl, _, _, _, _, true, _, _, _))
		.Times(1)
		.WillOnce(Return(true));

	status = InitializeMPD(multiAdaptationManifest);
	EXPECT_EQ(status, eAAMPSTATUS_OK);

	// Index the MPD document
	status = mTestableStreamAbstractionAAMP_MPD->InvokeIndexNewMPDDocument(false);
	EXPECT_EQ(status, eAAMPSTATUS_OK);

	MediaTrack *videoTrack = mTestableStreamAbstractionAAMP_MPD->GetMediaTrack(eTRACK_VIDEO);
	EXPECT_NE(videoTrack, nullptr);

	MediaStreamContext *pVideoContext = static_cast<MediaStreamContext *>(videoTrack);
	EXPECT_NE(pVideoContext, nullptr);

	// Set fragment descriptor
	pVideoContext->fragmentDescriptor.Number = 5;
	pVideoContext->fragmentDescriptor.Time = 20000;
	pVideoContext->fragmentDescriptor.TimeScale = 2500;
	pVideoContext->fragmentDescriptor.Bandwidth = 800000;

	// Test GenerateFragmentURLList - should only include non-blacklisted adaptations
	URLBitrateMap uriList;
	mTestableStreamAbstractionAAMP_MPD->GenerateFragmentURLList(uriList, pVideoContext, false);

	// Should generate URLs for 4 representations (2 from adaptation 0 + 1 from adaptation 2)
	// Adaptation 1 is blacklisted, so its 2 representations should be excluded
	EXPECT_EQ(uriList.size(), 3);

	// Verify adaptation 0 representations are included
	EXPECT_NE(uriList.find(800000), uriList.end());
	EXPECT_TRUE(uriList[800000].url.find("video_low_5.m4s") != std::string::npos);

	EXPECT_NE(uriList.find(2000000), uriList.end());
	EXPECT_TRUE(uriList[2000000].url.find("video_high_5.m4s") != std::string::npos);

	// Verify adaptation 1 representations are NOT included (blacklisted)
	EXPECT_EQ(uriList.find(1200000), uriList.end());
	EXPECT_EQ(uriList.find(1600000), uriList.end());

	// Verify adaptation 2 representation is included
	EXPECT_NE(uriList.find(3000000), uriList.end());
	EXPECT_TRUE(uriList[3000000].url.find("video_alt3_5.m4s") != std::string::npos);

	// Test with init segments
	URLBitrateMap initUriList;
	mTestableStreamAbstractionAAMP_MPD->GenerateFragmentURLList(initUriList, pVideoContext, true);

	EXPECT_EQ(initUriList.size(), 3);
	EXPECT_NE(initUriList.find(800000), initUriList.end());
	EXPECT_NE(initUriList.find(2000000), initUriList.end());
	EXPECT_NE(initUriList.find(3000000), initUriList.end());

	// Blacklisted adaptations should not be in init list
	EXPECT_EQ(initUriList.find(1200000), initUriList.end());
	EXPECT_EQ(initUriList.find(1600000), initUriList.end());
}

/**
 * @brief GenerateFragmentURLList tests with multiple blacklisted adaptations.
 *
 * Verifies that when multiple adaptation sets are blacklisted, only
 * non-blacklisted adaptations generate URLs.
 */
TEST_F(FetcherLoopTests, GenerateFragmentURLListWithMultipleBlacklistedAdaptations)
{
	static const char *manifest = R"(<?xml version="1.0" encoding="utf-8"?>
		<MPD xmlns="urn:mpeg:dash:schema:mpd:2011" availabilityStartTime="2023-01-01T00:00:00Z" type="static">
			<Period id="p0" start="PT0S">
				<AdaptationSet id="0" contentType="video">
					<Representation id="v0" bandwidth="500000">
						<SegmentTemplate timescale="1000" initialization="v0_init.mp4" media="v0_$Number$.m4s" startNumber="1">
							<SegmentTimeline><S t="0" d="2000" r="9" /></SegmentTimeline>
						</SegmentTemplate>
					</Representation>
				</AdaptationSet>
				<AdaptationSet id="1" contentType="video">
					<Representation id="v1" bandwidth="1000000">
						<SegmentTemplate timescale="1000" initialization="v1_init.mp4" media="v1_$Number$.m4s" startNumber="1">
							<SegmentTimeline><S t="0" d="2000" r="9" /></SegmentTimeline>
						</SegmentTemplate>
					</Representation>
				</AdaptationSet>
				<AdaptationSet id="2" contentType="video">
					<Representation id="v2" bandwidth="1500000">
						<SegmentTemplate timescale="1000" initialization="v2_init.mp4" media="v2_$Number$.m4s" startNumber="1">
							<SegmentTimeline><S t="0" d="2000" r="9" /></SegmentTimeline>
						</SegmentTemplate>
					</Representation>
				</AdaptationSet>
				<AdaptationSet id="3" contentType="audio">
					<Representation id="a0" bandwidth="128000">
						<SegmentTemplate timescale="1000" initialization="a0_init.mp4" media="a0_$Number$.m4s" startNumber="1">
							<SegmentTimeline><S t="0" d="2000" r="9" /></SegmentTimeline>
						</SegmentTemplate>
					</Representation>
				</AdaptationSet>
			</Period>
		</MPD>
		)";

	// Blacklist adaptation sets 0 and 2
	StreamBlacklistProfileInfo blInfo1;
	blInfo1.mPeriodId = "p0";
	blInfo1.mAdaptationSetIdx = 0;
	blInfo1.mReason = PROFILE_BLACKLIST_DRM_FAILURE;
	mPrivateInstanceAAMP->AddToBlacklistedProfiles(blInfo1);

	StreamBlacklistProfileInfo blInfo2;
	blInfo2.mPeriodId = "p0";
	blInfo2.mAdaptationSetIdx = 2;
	blInfo2.mReason = PROFILE_BLACKLIST_DRM_FAILURE;
	mPrivateInstanceAAMP->AddToBlacklistedProfiles(blInfo2);

	EXPECT_CALL(*g_mockMediaStreamContext, CacheFragment(_, _, _, _, _, true, _, _, _))
		.Times(::testing::AtLeast(1))
		.WillRepeatedly(Return(true));

	AAMPStatusType status = InitializeMPD(manifest);
	EXPECT_EQ(status, eAAMPSTATUS_OK);

	status = mTestableStreamAbstractionAAMP_MPD->InvokeIndexNewMPDDocument(false);
	EXPECT_EQ(status, eAAMPSTATUS_OK);

	MediaTrack *videoTrack = mTestableStreamAbstractionAAMP_MPD->GetMediaTrack(eTRACK_VIDEO);
	ASSERT_NE(videoTrack, nullptr);

	MediaStreamContext *pVideoContext = static_cast<MediaStreamContext *>(videoTrack);
	pVideoContext->fragmentDescriptor.Number = 3;
	pVideoContext->fragmentDescriptor.Time = 4000;
	pVideoContext->fragmentDescriptor.TimeScale = 1000;
	pVideoContext->fragmentDescriptor.Bandwidth = 1000000;

	URLBitrateMap uriList;
	mTestableStreamAbstractionAAMP_MPD->GenerateFragmentURLList(uriList, pVideoContext, false);

	// Only adaptation 1 should be included (adaptations 0 and 2 are blacklisted)
	EXPECT_EQ(uriList.size(), 1);
	EXPECT_NE(uriList.find(1000000), uriList.end());
	EXPECT_TRUE(uriList[1000000].url.find("v1_3.m4s") != std::string::npos);

	// Blacklisted adaptations should not appear
	EXPECT_EQ(uriList.find(500000), uriList.end());
	EXPECT_EQ(uriList.find(1500000), uriList.end());
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
	EXPECT_CALL(*g_mockMediaStreamContext, CacheFragment(_, _, _, _, _, true, _, _, _))
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
	EXPECT_CALL(*g_mockMediaStreamContext, CacheFragment(_, eCURLINSTANCE_AUDIO, _,_, _, false, _, _, _))
				.Times(0);
	EXPECT_CALL(*g_mockPrivateInstanceAAMP, GetPositionMilliseconds()).WillRepeatedly(Return(0.0));

	mTestableStreamAbstractionAAMP_MPD->SwitchAudioTrack();


}

/**
 * @brief FetcherLoop tests.
 *
 * Verifies that when playing ad content at the live edge, AdvanceTrack is skipped
 * if the fragment time exceeds the live edge.
 */
TEST_F(FetcherLoopTests, FetcherLoopSkipsAdvanceTrackWhenExceedsLiveEdge)
{
	std::string videoInitFragmentUrl;
	AAMPStatusType status;

	videoInitFragmentUrl = std::string(TEST_BASE_URL) + std::string("video_p0_init.mp4");
	EXPECT_CALL(*g_mockMediaStreamContext, CacheFragment(videoInitFragmentUrl, _, _, _, _, true, _, _, _))
		.Times(AnyNumber())
		.WillRepeatedly(Return(true));
	EXPECT_CALL(*g_mockMediaStreamContext, CacheFragment(_, _, _, _, _, false, _, _, _))
		.Times(0);
	EXPECT_CALL(*g_mockPrivateInstanceAAMP, IsLocalAAMPTsbInjection())
		.WillRepeatedly(Return(false));

	status = InitializeMPD(mLiveManifest, eTUNETYPE_SEEK, 0.0);
	EXPECT_EQ(status, eAAMPSTATUS_OK);

	mTestableStreamAbstractionAAMP_MPD->InvokeInitializeWorkers();

	auto *cdaiObj = mTestableStreamAbstractionAAMP_MPD->GetCDAIObject();
	ASSERT_NE(cdaiObj, nullptr);
	cdaiObj->mAdState = AdState::IN_ADBREAK_AD_PLAYING;

	mPrivateInstanceAAMP->mAbsoluteEndPosition = 10.0;
	MediaTrack *track = mTestableStreamAbstractionAAMP_MPD->GetMediaTrack(eTRACK_VIDEO);
	ASSERT_NE(track, nullptr);
	auto *pMediaStreamContext = static_cast<MediaStreamContext *>(track);
	pMediaStreamContext->fragmentTime = 11.0;

	int downloadsCounter = 0;
	EXPECT_CALL(*g_mockPrivateInstanceAAMP, DownloadsAreEnabled())
		.Times(AnyNumber())
		.WillRepeatedly([&downloadsCounter]()
			{
				return (++downloadsCounter < 3);
			});

	mTestableStreamAbstractionAAMP_MPD->InvokeFetcherLoop();
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
	EXPECT_CALL(*g_mockMediaStreamContext, CacheFragment(videoFragmentUrl, _, _, _, _, true, _, _, _)).Times(1).WillOnce(Return(true));
	EXPECT_CALL(*g_mockMediaStreamContext, CacheFragment(audioFragmentUrl, _, _, _, _, true, _, _, _)).Times(1).WillOnce(Return(true));
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
	EXPECT_CALL(*g_mockMediaStreamContext, CacheFragment(videoFragmentUrl, _, _, _, _, true, _, _, _)).Times(1).WillOnce(Return(true));
	EXPECT_CALL(*g_mockMediaStreamContext, CacheFragment(audioFragmentUrl, _, _, _, _, true, _, _, _)).Times(1).WillOnce(Return(true));
	// Expect the segments to be downloaded from track
	EXPECT_CALL(*g_mockMediaStreamContext, CacheFragment(_, _, _, _, _, false, _, _, _)).WillRepeatedly(Return(true));

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
	bool ret = false;
	/* Initialize MPD. The video/audio initialization segment is cached. */
	videoInitFragmentUrl = std::string(TEST_BASE_URL) + std::string("video_p0_init.mp4");
	audioInitFragmentUrl = std::string(TEST_BASE_URL) + std::string("audio_p0_init.mp4");
	EXPECT_CALL(*g_mockMediaStreamContext, CacheFragment(videoInitFragmentUrl, _, _, _, _, true, _, _, _))
		.Times(1)
		.WillOnce(Return(true));
	EXPECT_CALL(*g_mockMediaStreamContext, CacheFragment(audioInitFragmentUrl, _, _, _, _, true, _, _, _))
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
	bool ret = false;

	// Expect initialization fragment to be cached
	fragmentUrl = std::string(TEST_BASE_URL) + std::string("video_p0_init.mp4");
	EXPECT_CALL(*g_mockMediaStreamContext, CacheFragment(fragmentUrl, _, _, _, _, true, _, _, _))
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
	EXPECT_CALL(*g_mockPrivateInstanceAAMP, SendAdReservationEvent(_, _, _, _, _, _)).Times(2);

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
// Structure to hold test parameters
struct TestParams
{
	const char *manifest;
	double seekPos;
	bool mockIDXDownload;
	const char *videoInitFragment;
	const char *audioInitFragment;
	const char *videoFragmentP1;
	const char *audioFragmentP1;
	const char *endVideoFragmentP1;
};

// Test cases
TestParams testCases[] = {
	{
		R"(<?xml version="1.0" encoding="utf-8"?>
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
		)",
		24.0,
		false,
		"video_p0_init.mp4",
		"audio_p0_init.mp4",
		"video_p1_init.mp4",
		"audio_p1_init.mp4",
		"video_p1_18.m4s"},
	{
		R"(<?xml version="1.0" encoding="utf-8"?>
			<MPD xmlns="urn:mpeg:dash:schema:mpd:2011" availabilityStartTime="2023-01-01T00:00:00Z" maxSegmentDuration="PT2S" minBufferTime="PT4.000S" minimumUpdatePeriod="P100Y" profiles="urn:dvb:dash:profile:dvb-dash:2014,urn:dvb:dash:profile:dvb-dash:isoff-ext-live:2014" publishTime="2023-01-01T00:01:00Z" timeShiftBufferDepth="PT5M" type="dynamic">
				<Period id="p0" start="PT0S">
					<AdaptationSet id="0" contentType="video">
						<Representation id="0" mimeType="video/mp4" codecs="avc1.640028" bandwidth="800000" width="640" height="360" frameRate="25">
							<SegmentList timescale="2500" duration="5000">
								<Initialization sourceURL="video_p0_init.m4s"/>
								<SegmentURL media="video_p0_1.m4s"/>
								<SegmentURL media="video_p0_2.m4s"/>
							</SegmentList>
						</Representation>
					</AdaptationSet>
					<AdaptationSet id="1" contentType="audio" lang="eng">
						<Representation id="0" mimeType="audio/mp4" codecs="ec-3" bandwidth="800000" width="640" height="360" frameRate="25">
							<SegmentList timescale="2500" duration="5000">
								<Initialization sourceURL="audio_p0_init.m4s"/>
								<SegmentURL media="audio_p0_1.m4s"/>
								<SegmentURL media="audio_p0_2.m4s"/>
							</SegmentList>
						</Representation>
					</AdaptationSet>
				</Period>
				<Period id="p1" start="PT30S" duration="PT30S">
					<AdaptationSet id="0" contentType="video">
						<Representation id="0" mimeType="video/mp4" codecs="avc1.640028" bandwidth="800000" width="640" height="360" frameRate="25">
							<SegmentList timescale="2500" duration="5000">
								<Initialization sourceURL="video_p1_init.m4s"/>
								<SegmentURL media="video_p1_1.m4s"/>
								<SegmentURL media="video_p1_2.m4s"/>
							</SegmentList>
						</Representation>
					</AdaptationSet>
					<AdaptationSet id="1" contentType="audio" lang="eng">
						<Representation id="0" mimeType="audio/mp4" codecs="ec-3" bandwidth="800000" width="640" height="360" frameRate="25">
							<SegmentList timescale="2500" duration="5000">
								<Initialization sourceURL="audio_p1_init.m4s"/>
								<SegmentURL media="audio_p1_1.m4s"/>
								<SegmentURL media="audio_p1_2.m4s"/>
							</SegmentList>
						</Representation>
					</AdaptationSet>
				</Period>
			</MPD>
		)",
		0,
		false,
		"video_p0_init.m4s",
		"audio_p0_init.m4s",
		"video_p1_init.m4s",
		"audio_p1_init.m4s",
		"video_p1_2.m4s"},
	{
		R"(<?xml version="1.0" encoding="utf-8"?>
			<MPD xmlns="urn:mpeg:dash:schema:mpd:2011" availabilityStartTime="2023-01-01T00:00:00Z" maxSegmentDuration="PT2S" minBufferTime="PT4.000S" minimumUpdatePeriod="P100Y" profiles="urn:dvb:dash:profile:dvb-dash:2014,urn:dvb:dash:profile:dvb-dash:isoff-ext-live:2014" publishTime="2023-01-01T00:01:00Z" timeShiftBufferDepth="PT5M" type="dynamic">
				<Period id="p0" start="PT0S">
					<AdaptationSet id="0" contentType="video">
						<Representation id="0" mimeType="video/mp4" codecs="avc1.640028" bandwidth="800000" width="640" height="360" frameRate="25">
							<BaseURL>http://host/asset/video_p0.m4s</BaseURL>
							<SegmentList timescale="2500" duration="5000">
								<Initialization range="0-496"/>
								<SegmentURL mediaRange="500-999"/>
								<SegmentURL mediaRange="1000-1499"/>
							</SegmentList>
						</Representation>
					</AdaptationSet>
					<AdaptationSet id="1" contentType="audio" lang="eng">
						<Representation id="0" mimeType="audio/mp4" codecs="ec-3" bandwidth="800000" width="640" height="360" frameRate="25">
							<BaseURL>http://host/asset/audio_p0.m4s</BaseURL>
							<SegmentList timescale="2500" duration="5000">
								<Initialization range="0-496"/>
								<SegmentURL mediaRange="500-999"/>
								<SegmentURL mediaRange="1000-1499"/>
							</SegmentList>
						</Representation>
					</AdaptationSet>
				</Period>
				<Period id="p1" start="PT30S" duration="PT30S">
					<AdaptationSet id="0" contentType="video">
						<Representation id="0" mimeType="video/mp4" codecs="avc1.640028" bandwidth="800000" width="640" height="360" frameRate="25">
							<BaseURL>http://host/asset/video_p1.m4s</BaseURL>
							<SegmentList timescale="2500" duration="5000">
								<Initialization range="0-496"/>
								<SegmentURL mediaRange="500-999"/>
								<SegmentURL mediaRange="1000-1499"/>
							</SegmentList>
						</Representation>
					</AdaptationSet>
					<AdaptationSet id="1" contentType="audio" lang="eng">
						<Representation id="0" mimeType="audio/mp4" codecs="ec-3" bandwidth="800000" width="640" height="360" frameRate="25">
							<BaseURL>http://host/asset/audio_p1.m4s</BaseURL>
							<SegmentList timescale="2500" duration="5000">
								<Initialization range="0-496"/>
								<SegmentURL mediaRange="500-999"/>
								<SegmentURL mediaRange="1000-1499"/>
							</SegmentList>
						</Representation>
					</AdaptationSet>
				</Period>
			</MPD>
		)",
		0,
		false,
		"video_p0.m4s",
		"audio_p0.m4s",
		"video_p1.m4s",
		"audio_p1.m4s",
		"video_p1.m4s"},
	{
		R"(<?xml version="1.0" encoding="utf-8"?>
			<MPD xmlns="urn:mpeg:dash:schema:mpd:2011" availabilityStartTime="2023-01-01T00:00:00Z" maxSegmentDuration="PT2S" minBufferTime="PT4.000S" minimumUpdatePeriod="P100Y" profiles="urn:dvb:dash:profile:dvb-dash:2014,urn:dvb:dash:profile:dvb-dash:isoff-ext-live:2014" publishTime="2023-01-01T00:01:00Z" timeShiftBufferDepth="PT5M" type="dynamic">
				<Period id="p0" start="PT0S" duration="PT30S">
					<AdaptationSet id="0" contentType="video">
						<Representation id="0" mimeType="video/mp4" codecs="avc1.640028" bandwidth="800000" width="640" height="360" frameRate="25">
							<BaseURL>http://host/asset/video_p0.m4s</BaseURL>
							<SegmentBase indexRange="500-999"/>
						</Representation>
					</AdaptationSet>
					<AdaptationSet id="1" contentType="audio" lang="eng">
						<Representation id="0" mimeType="audio/mp4" codecs="ec-3" bandwidth="800000" width="640" height="360" frameRate="25">
							<BaseURL>http://host/asset/audio_p0.m4s</BaseURL>
							<SegmentBase indexRange="500-999"/>
						</Representation>
					</AdaptationSet>
				</Period>
				<Period id="p1" start="PT30S" duration="PT30S">
					<AdaptationSet id="0" contentType="video">
						<Representation id="0" mimeType="video/mp4" codecs="avc1.640028" bandwidth="800000" width="640" height="360" frameRate="25">
							<BaseURL>http://host/asset/video_p1.m4s</BaseURL>
							<SegmentBase indexRange="500-999"/>
						</Representation>
					</AdaptationSet>
					<AdaptationSet id="1" contentType="audio" lang="eng">
						<Representation id="0" mimeType="audio/mp4" codecs="ec-3" bandwidth="800000" width="640" height="360" frameRate="25">
							<BaseURL>http://host/asset/audio_p1.m4s</BaseURL>
							<SegmentBase indexRange="500-999"/>
						</Representation>
					</AdaptationSet>
				</Period>
			</MPD>
		)",
		0,
		true,
		"video_p0.m4s",
		"audio_p0.m4s",
		"video_p1.m4s",
		"audio_p1.m4s",
		"video_p1.m4s"},
	{
		R"(<?xml version="1.0" encoding="utf-8"?>
			<MPD xmlns="urn:mpeg:dash:schema:mpd:2011" availabilityStartTime="2023-01-01T00:00:00Z" maxSegmentDuration="PT2S" minBufferTime="PT4.000S" minimumUpdatePeriod="P100Y" profiles="urn:dvb:dash:profile:dvb-dash:2014,urn:dvb:dash:profile:dvb-dash:isoff-ext-live:2014" publishTime="2023-01-01T00:01:00Z" timeShiftBufferDepth="PT5M" type="dynamic">
				<Period id="p0" start="PT0S" duration="PT30S">
					<AdaptationSet id="0" contentType="video">
						<Representation id="0" mimeType="video/mp4" codecs="avc1.640028" bandwidth="800000" width="640" height="360" frameRate="25">
							<BaseURL>http://host/asset/video_p0.m4s</BaseURL>
							<SegmentBase indexRange="500-1999">
								<Initialization range="0-499"/>
							</SegmentBase>
						</Representation>
					</AdaptationSet>
					<AdaptationSet id="1" contentType="audio" lang="eng">
						<Representation id="0" mimeType="audio/mp4" codecs="ec-3" bandwidth="800000" width="640" height="360" frameRate="25">
							<BaseURL>http://host/asset/audio_p0.m4s</BaseURL>
							<SegmentBase indexRange="500-1999">
								<Initialization range="0-499"/>
							</SegmentBase>
						</Representation>
					</AdaptationSet>
				</Period>
				<Period id="p1" start="PT30S" duration="PT30S">
					<AdaptationSet id="0" contentType="video">
						<Representation id="0" mimeType="video/mp4" codecs="avc1.640028" bandwidth="800000" width="640" height="360" frameRate="25">
							<BaseURL>http://host/asset/video_p1.m4s</BaseURL>
							<SegmentBase indexRange="500-1999">
								<Initialization range="0-499"/>
							</SegmentBase>
						</Representation>
					</AdaptationSet>
					<AdaptationSet id="1" contentType="audio" lang="eng">
						<Representation id="0" mimeType="audio/mp4" codecs="ec-3" bandwidth="800000" width="640" height="360" frameRate="25">
							<BaseURL>http://host/asset/audio_p1.m4s</BaseURL>
							<SegmentBase indexRange="500-1999">
								<Initialization range="0-499"/>
							</SegmentBase>
						</Representation>
					</AdaptationSet>
				</Period>
			</MPD>
		)",
		0,
		true,
		"video_p0.m4s",
		"audio_p0.m4s",
		"video_p1.m4s",
		"audio_p1.m4s",
		"video_p1.m4s"}
};

// Sample SIDX box for testing IndexedSegment download scenarios.
// Contains 2 segment references with 2-second durations each.
// timescale=1000, total duration=4000ms (4 seconds)
static const uint8_t sidxBox[] = {
	// Box size = 56
	0x00, 0x00, 0x00, 0x38,
	// Type = 'sidx'
	0x73, 0x69, 0x64, 0x78,
	// version=0, flags=0
	0x00, 0x00, 0x00, 0x00,
	// reference_ID = 1
	0x00, 0x00, 0x00, 0x01,
	// timescale = 1000
	0x00, 0x00, 0x03, 0xE8,
	// earliest_presentation_time = 0 (32-bit, version 0)
	0x00, 0x00, 0x00, 0x00,
	// first_offset = 0
	0x00, 0x00, 0x00, 0x00,
	// reserved = 0
	0x00, 0x00,
	// reference_count = 2
	0x00, 0x02,
	// Reference 0: size=16384, duration=2000, flags
	0x00, 0x00, 0x40, 0x00,
	0x00, 0x00, 0x07, 0xD0,
	0x90, 0x00, 0x00, 0x00,
	// Reference 1: size=12288, duration=2000, flags
	0x00, 0x00, 0x30, 0x00,
	0x00, 0x00, 0x07, 0xD0,
	0x90, 0x00, 0x00, 0x00,
};

class AdvancedFetcherLoopTests : public FetcherLoopTests, public ::testing::WithParamInterface<TestParams>
{
public:
	void SetUp() override
	{
		shouldExitTest = false;
		FetcherLoopTests::SetUp();
	}

	void TearDown() override
	{
		FetcherLoopTests::TearDown();
	}
	bool shouldExitTest;
};

/**
 * @brief FetcherLoopTests
 * Verifies the fetcher loop with different formats of MPDs
 */
TEST_P(AdvancedFetcherLoopTests, FetcherLoopTestsWithDifferentMPD)
{
	std::string videoFragmentUrl;
	std::string audioFragmentUrl;
	AAMPStatusType status;
	mPrivateInstanceAAMP->rate = AAMP_NORMAL_PLAY_RATE;
	bool ret = false;

	// Access struct elements
	TestParams param = GetParam();
	const char *manifest = param.manifest;
	double seekPos = param.seekPos;
	bool mockIDXDownload = param.mockIDXDownload;
	const char *videoInitFragment = param.videoInitFragment;
	const char *audioInitFragment = param.audioInitFragment;
	const char *videoFragmentP1 = param.videoFragmentP1;
	const char *audioFragmentP1 = param.audioFragmentP1;
	std::string endVideoFragmentUrl = std::string(TEST_BASE_URL) + std::string(param.endVideoFragmentP1);

	/* Initialize MPD. The video/audio initialization segment is cached. */
	videoFragmentUrl = std::string(TEST_BASE_URL) + std::string(videoInitFragment);
	audioFragmentUrl = std::string(TEST_BASE_URL) + std::string(audioInitFragment);
	if (mockIDXDownload)
	{
		EXPECT_CALL(*g_mockPrivateInstanceAAMP, LoadIDX(_, _, _, _, _, _, _, _, _, _))
			.WillRepeatedly(WithArg<3>(Invoke([](std::vector<uint8_t>& idxBuffer)
			{
				idxBuffer.insert(idxBuffer.end(), std::cbegin(sidxBox), std::cend(sidxBox));
			})));
	}
	EXPECT_CALL(*g_mockMediaStreamContext, CacheFragment(videoFragmentUrl, _, _, _, _, true, _, _, _)).Times(1).WillOnce(Return(true));
	EXPECT_CALL(*g_mockMediaStreamContext, CacheFragment(audioFragmentUrl, _, _, _, _, true, _, _, _)).Times(1).WillOnce(Return(true));
	EXPECT_CALL(*g_mockPrivateInstanceAAMP, IsLocalAAMPTsbInjection()).WillRepeatedly(Return(false));

	status = InitializeMPD(manifest, eTUNETYPE_SEEK, seekPos);

	/* Invoke Worker threads */
	mTestableStreamAbstractionAAMP_MPD->InvokeInitializeWorkers();

	EXPECT_EQ(status, eAAMPSTATUS_OK);

	// Run test until the end segment of the period is cached, then exit
	EXPECT_CALL(*g_mockPrivateInstanceAAMP, DownloadsAreEnabled())
		.Times(AnyNumber())
		.WillRepeatedly([this]() { return !shouldExitTest; });
	videoFragmentUrl = std::string(TEST_BASE_URL) + std::string(videoFragmentP1);
	audioFragmentUrl = std::string(TEST_BASE_URL) + std::string(audioFragmentP1);
	EXPECT_CALL(*g_mockMediaStreamContext, CacheFragment(videoFragmentUrl, _, _, _, _, true, _, _, _)).Times(1).WillOnce(Return(true));
	EXPECT_CALL(*g_mockMediaStreamContext, CacheFragment(audioFragmentUrl, _, _, _, _, true, _, _, _)).Times(1).WillOnce(Return(true));

	// Default expect
	EXPECT_CALL(*g_mockMediaStreamContext, CacheFragment(_, _, _, _, _, false, _, _, _)).WillRepeatedly(Return(true));
	// Expect the last segment of the period to be cached, and then exit the test
	EXPECT_CALL(*g_mockMediaStreamContext, CacheFragment(endVideoFragmentUrl, _, _, _, _, false, _, _, _))
		.WillOnce([this]() { shouldExitTest = true; return true; });
	/* Invoke the fetcher loop. */
	mTestableStreamAbstractionAAMP_MPD->InvokeFetcherLoop();
	EXPECT_EQ(mTestableStreamAbstractionAAMP_MPD->GetCurrentPeriodIdx(), 1);
	EXPECT_EQ(mTestableStreamAbstractionAAMP_MPD->GetIteratorPeriodIdx(), 1);
}

INSTANTIATE_TEST_SUITE_P(
	BasicFetcherLoopMPDTests,
	AdvancedFetcherLoopTests,
	::testing::ValuesIn(testCases));

/**
 * @brief Regression test for VPAAMP-205 / revert-1427 regression.
 *
 * Background
 * ----------
 * VPAAMP-205 correctly added the `(mPlayRate >= AAMP_RATE_PAUSE)` guard to
 * HandleSeekEOSAndPeriodTransition, preventing forward-period advancement during
 * reverse seeks.  Before that guard the seek would accidentally jump past ad-break
 * periods, which incidentally stopped the FetcherLoop from hitting the adbreak
 * boundary.  After the guard was in place, the FetcherLoop operates correctly at
 * the boundary but a pre-existing bug in SelectSourceOrAdPeriod becomes visible:
 *
 * When "All Ads FINISHED" fires during reverse trick-play, SelectSourceOrAdPeriod
 * sets mBasePeriodOffset = period_duration (end of the new base period) and then
 * immediately calls onAdEvent(DEFAULT).  CheckForAdStart's
 *   `(rate < 0) && (key == end)`
 * special case fires because key == curP2Ad.duration == period_duration * 1000,
 * re-entering the just-completed adbreak before any source content is fetched.
 * The resulting period oscillation (source -> ad -> source -> ad -> ...) drives
 * GStreamer into a "This file is corrupt" error.
 *
 * The fix (SelectSourceOrAdPeriod): skip the proactive onAdEvent(DEFAULT) call
 * that follows "All Ads FINISHED" when mPlayRate < AAMP_RATE_PAUSE.  The inner
 * fragment-download loop detects the next adbreak via BASE_OFFSET_CHANGE naturally
 * as mBasePeriodOffset decreases.
 *
 * Test 1 - ReverseTrickPlay_AllAdsFinished_NoImmediateAdBreakReentry
 *   Verifies that after "All Ads FINISHED" in reverse trick-play, the ad state
 *   machine stays in OUTSIDE_ADBREAK and does NOT immediately re-enter an adbreak
 *   in the new base period.
 *
 * Test 2 - ForwardPlayback_AllAdsFinished_AdDetectionStillWorks
 *   Verifies that the fix does not regress forward playback: onAdEvent(DEFAULT)
 *   is still called after "All Ads FINISHED" for forward rates so that a
 *   back-to-back adbreak in the next period is detected immediately.
 */

// Three-period VOD manifest used by the CDAI reverse trick-play regression tests.
// p0 (0-30 s), p1 (30-60 s), p2 (60-90 s) - no in-band ad periods; adbreaks are
// injected via the CDAI object.
static constexpr const char *kCdaiRewindManifest = R"(<?xml version="1.0" encoding="utf-8"?>
<MPD xmlns="urn:mpeg:dash:schema:mpd:2011"
     availabilityStartTime="2023-01-01T00:00:00Z"
     maxSegmentDuration="PT2S" minBufferTime="PT4S"
     minimumUpdatePeriod="P100Y"
     profiles="urn:dvb:dash:profile:dvb-dash:2014"
     type="static">
  <Period id="p0" start="PT0S">
    <AdaptationSet id="0" contentType="video">
      <Representation id="0" mimeType="video/mp4" codecs="avc1.640028"
                      bandwidth="800000" width="640" height="360" frameRate="25">
        <SegmentTemplate timescale="2500" initialization="video_p0_init.mp4"
                         media="video_p0_$Number$.m4s" startNumber="1">
          <SegmentTimeline><S t="0" d="5000" r="14"/></SegmentTimeline>
        </SegmentTemplate>
      </Representation>
    </AdaptationSet>
  </Period>
  <Period id="p1" start="PT30S">
    <AdaptationSet id="1" contentType="video">
      <Representation id="0" mimeType="video/mp4" codecs="avc1.640028"
                      bandwidth="800000" width="640" height="360" frameRate="25">
        <SegmentTemplate timescale="2500" initialization="video_p1_init.mp4"
                         media="video_p1_$Number$.m4s" startNumber="16">
          <SegmentTimeline><S t="0" d="5000" r="14"/></SegmentTimeline>
        </SegmentTemplate>
      </Representation>
    </AdaptationSet>
  </Period>
  <Period id="p2" start="PT60S">
    <AdaptationSet id="2" contentType="video">
      <Representation id="0" mimeType="video/mp4" codecs="avc1.640028"
                      bandwidth="800000" width="640" height="360" frameRate="25">
        <SegmentTemplate timescale="2500" initialization="video_p2_init.mp4"
                         media="video_p2_$Number$.m4s" startNumber="31">
          <SegmentTimeline><S t="0" d="5000" r="14"/></SegmentTimeline>
        </SegmentTemplate>
      </Representation>
    </AdaptationSet>
  </Period>
</MPD>)";

/**
 * @brief VPAAMP-205 regression: reverse trick-play through CDAI ads must not
 *        oscillate between the base period and the ad period after "All Ads Finished".
 *
 * Setup
 * -----
 * - 3-period manifest: p0 (0-30 s), p1 (30-60 s), p2 (60-90 s).
 * - mPlayRate = -12 (reverse trick-play).
 * - mCurrentPeriodIdx = 1 (playing from period p1 going backward).
 * - CDAI state = IN_ADBREAK_WAIT2CATCHUP (we just finished playing the sole ad
 *   in adbreak "p1" in reverse; mCurAdIdx=0 so GetNextAdInBreak(-1) → -1 → done).
 * - mCurPlayingBreakId = "p1": "All Ads Finished" reverse path sets
 *   mBasePeriodId = prevPId("p1") = "p0".
 * - mPeriodMap["p0"] has adBreakId="p0" with duration=30000 ms (full period),
 *   so CheckForAdStart at (mBasePeriodOffset=30.0, key=30000==end) would
 *   re-trigger the adbreak if the fix were absent.
 *
 * Expected outcome (with fix)
 * ---------------------------
 * - onAdEvent(DEFAULT) is NOT called for the new base period ("p0") immediately
 *   after the OUTSIDE_ADBREAK transition.
 * - mAdState stays OUTSIDE_ADBREAK.
 * - adStateChanged is false (no re-entry).
 * - mCurrentPeriod is the source content period "p0" (not an ad period).
 */
TEST_F(FetcherLoopTests, ReverseTrickPlay_AllAdsFinished_NoImmediateAdBreakReentry)
{
    AAMPStatusType status;

    // Initialize with the 3-period manifest at normal rate, seeking to p1.
    // We set the play rate to -12 after Init so that the Init path
    // (which only supports forward rates) completes successfully.
    EXPECT_CALL(*g_mockMediaStreamContext, CacheFragment(_, _, _, _, _, true, _, _, _))
        .WillOnce(Return(true));
    status = InitializeMPD(kCdaiRewindManifest, eTUNETYPE_SEEK, 35.0, AAMP_NORMAL_PLAY_RATE);
    EXPECT_EQ(status, eAAMPSTATUS_OK);

    status = mTestableStreamAbstractionAAMP_MPD->InvokeIndexNewMPDDocument(false);
    (void)status;

    // Simulate trick-play at -12x: set rate after Init so Init completes normally.
    // Only update the internal mPlayRate member; leave aamp->rate at 1.0 so that
    // ShouldCheckOnlyIframeAdaptation() does not mark content periods as empty.
    mTestableStreamAbstractionAAMP_MPD->SetPlayRate(-12.0f);

    // Point the iterator/current period at p1 (index 1) - simulating that the
    // FetcherLoop has been traversing p1 in reverse and is about to finish the ad.
    mTestableStreamAbstractionAAMP_MPD->SetIteratorPeriodIdx(1);

    // Configure CDAI state: one placed, valid ad in adbreak "p1"; we are waiting
    // for the base content to catch up (last state before "All Ads Finished").
    auto *cdaiObj = mTestableStreamAbstractionAAMP_MPD->GetCDAIObject();

    auto adsP1 = std::make_shared<std::vector<AdNode>>();
    adsP1->emplace_back(
        /*invalid*/   false,
        /*placed*/    true,
        /*resolved*/  true,
        /*adId*/      "adId-p1",
        /*url*/       TEST_AD_MANIFEST_URL,
        /*duration*/  30000,
        /*basePId*/   "p1",
        /*baseOffset*/0,
        /*mpd*/       nullptr);

    cdaiObj->mAdBreaks["p1"] = AdBreakObject(30000, adsP1, "p2", 0, 30000);
    cdaiObj->mAdBreaks["p1"].mAdBreakPlaced = true;
    cdaiObj->mAdBreaks["p1"].mAdFailed = false;

    cdaiObj->mCurAds = adsP1;
    cdaiObj->mCurAdIdx = 0;           // GetNextAdInBreak(-1) -> -1 -> all done
    cdaiObj->mCurPlayingBreakId = "p1";
    cdaiObj->mAdState = AdState::IN_ADBREAK_WAIT2CATCHUP;

    // Set up period "p0" with an adbreak so that CheckForAdStart at
    // mBasePeriodOffset==30.0 (key==end==30000) would fire for reverse
    // playback if the proactive onAdEvent(DEFAULT) call were still present.
    auto adsP0 = std::make_shared<std::vector<AdNode>>();
    adsP0->emplace_back(false, true, true, "adId-p0", TEST_AD_MANIFEST_URL,
                        30000, "p0", 0, nullptr);
    cdaiObj->mAdBreaks["p0"] = AdBreakObject(30000, adsP0, "p1", 0, 30000);
    cdaiObj->mAdBreaks["p0"].mAdBreakPlaced = true;

    Period2AdData p0AdData;
    p0AdData.adBreakId = "p0";
    p0AdData.duration  = 30000; // full 30-second period = 30000 ms
    p0AdData.filled    = true;
    p0AdData.offset2Ad[0] = {0 /*adStartOffset*/, 0 /*adIdx*/};
    cdaiObj->mPeriodMap["p0"] = p0AdData;

    // Invoke SelectSourceOrAdPeriod.  adStateChanged=true signals that we just
    // finished ad playback and entered WAIT2CATCHUP from the previous inner-loop
    // iteration (mirrors the real FetcherLoop flow).
    bool periodChanged       = false;
    bool mpdChanged          = false;
    bool adStateChanged      = true;
    bool waitForAdBreakCatchup = false;
    bool requireStreamSelection = false;
    std::string currentPeriodId = "p1";

    EXPECT_CALL(*g_mockPrivateInstanceAAMP, GetTSBSessionManager())
        .WillRepeatedly(Return(nullptr));
    EXPECT_CALL(*g_mockPrivateInstanceAAMP, IsLocalAAMPTsbInjection())
        .WillRepeatedly(Return(false));
    EXPECT_CALL(*g_mockPrivateInstanceAAMP, SendAdReservationEvent(_, _, _, _, _, _))
        .Times(AnyNumber());
    // GetIsPeriodChangeMarked is on the real PrivateInstanceAAMP (not mocked).
    mPrivateInstanceAAMP->SetIsPeriodChangeMarked(false);

    // KEY regression assertion: the WAIT2CATCHUP→OUTSIDE_ADBREAK transition
    // uses GetNextAdInBreak, never CheckForAdStart.  The proactive
    // onAdEvent(DEFAULT) that follows "All Ads Finished" in the UNFIXED code
    // is the first and only caller of CheckForAdStart in this code path.
    // Times(0) therefore proves the proactive call was completely suppressed.
    // Without the fix (in SelectSourceOrAdPeriod), this expectation FAILS
    // because the proactive onAdEvent runs and invokes CheckForAdStart.
    EXPECT_CALL(*g_MockPrivateCDAIObjectMPD, CheckForAdStart(_, _, _, _, _, _))
        .Times(0);

    bool ret = mTestableStreamAbstractionAAMP_MPD->InvokeSelectSourceOrAdPeriod(
        periodChanged, mpdChanged, adStateChanged, waitForAdBreakCatchup,
        requireStreamSelection, currentPeriodId);

    // After "All Ads Finished" for reverse trick-play, the adbreak for "p0"
    // must NOT be re-triggered immediately.
    EXPECT_EQ(cdaiObj->mAdState, AdState::OUTSIDE_ADBREAK)
        << "Reverse trick-play: adState should stay OUTSIDE_ADBREAK after All Ads Finished";
    EXPECT_FALSE(adStateChanged)
        << "Reverse trick-play: adStateChanged must be false - no immediate adbreak re-entry";
    EXPECT_TRUE(ret);
    // Verify we ended up on the correct source content period, not an ad period.
    ASSERT_NE(mTestableStreamAbstractionAAMP_MPD->GetCurrentPeriod(), nullptr);
    EXPECT_EQ(mTestableStreamAbstractionAAMP_MPD->GetCurrentPeriod()->GetId(), std::string("p0"))
        << "Reverse trick-play: after All Ads Finished must land on source period p0";
}

/**
 * @brief Forward-playback regression guard for VPAAMP-205 fix.
 *
 * Verifies that the fix does NOT suppress onAdEvent(DEFAULT) for forward
 * playback: after "All Ads Finished" for a forward rate, the FetcherLoop
 * must still detect an immediately-following adbreak in the next period.
 *
 * Setup
 * -----
 * - Same 3-period manifest: p0, p1, p2.
 * - mPlayRate = AAMP_NORMAL_PLAY_RATE (1.0).
 * - CDAI state = IN_ADBREAK_WAIT2CATCHUP; adbreak "p0" placed, endPeriodId="p1".
 * - Period "p1" has its own adbreak in mPeriodMap so that the proactive
 *   onAdEvent(DEFAULT) after "All Ads Finished" can detect it.
 *
 * Expected outcome
 * ----------------
 * - onAdEvent(DEFAULT) IS called for forward playback.
 * - The adbreak for "p1" is detected: adState transitions to
 *   IN_ADBREAK_AD_NOT_PLAYING (because the ad starts at offset 0 which
 *   matches the mBasePeriodOffset=0 set after forward "All Ads Finished").
 * - adStateChanged is true.
 */
TEST_F(FetcherLoopTests, ForwardPlayback_AllAdsFinished_AdDetectionStillWorks)
{
    AAMPStatusType status;

    EXPECT_CALL(*g_mockMediaStreamContext, CacheFragment(_, _, _, _, _, true, _, _, _))
        .WillOnce(Return(true));
    status = InitializeMPD(kCdaiRewindManifest, eTUNETYPE_SEEK, 5.0, AAMP_NORMAL_PLAY_RATE);
    EXPECT_EQ(status, eAAMPSTATUS_OK);

    status = mTestableStreamAbstractionAAMP_MPD->InvokeIndexNewMPDDocument(false);
    (void)status;

    mTestableStreamAbstractionAAMP_MPD->SetIteratorPeriodIdx(0);

    auto *cdaiObj = mTestableStreamAbstractionAAMP_MPD->GetCDAIObject();

    // Adbreak "p0" with one placed ad; forward "All Ads Finished" will read
    // endPeriodId and set mBasePeriodId = "p1", mBasePeriodOffset = 0.
    auto adsP0 = std::make_shared<std::vector<AdNode>>();
    adsP0->emplace_back(false, true, true, "adId-p0-fwd", TEST_AD_MANIFEST_URL,
                        30000, "p0", 0, nullptr);
    cdaiObj->mAdBreaks["p0"] = AdBreakObject(30000, adsP0, "p1", 0, 30000);
    cdaiObj->mAdBreaks["p0"].mAdBreakPlaced = true;
    cdaiObj->mAdBreaks["p0"].mAdFailed = false;

    cdaiObj->mCurAds = adsP0;
    cdaiObj->mCurAdIdx = 0;           // GetNextAdInBreak(+1) -> 1 >= size -> done
    cdaiObj->mCurPlayingBreakId = "p0";
    cdaiObj->mAdState = AdState::IN_ADBREAK_WAIT2CATCHUP;

    // Period "p1" has an adbreak starting at offset 0, covering the full period.
    // The proactive onAdEvent(DEFAULT) after forward "All Ads Finished" should
    // detect this and transition state (adIdx=-1 → IN_ADBREAK_AD_NOT_PLAYING
    // or similar, depending on whether the ad is resolved yet).
    auto adsP1 = std::make_shared<std::vector<AdNode>>();
    adsP1->emplace_back(false, false /*not yet placed*/, true, "adId-p1-fwd",
                        TEST_AD_MANIFEST_URL, 30000, "p1", 0, nullptr);
    cdaiObj->mAdBreaks["p1"] = AdBreakObject(30000, adsP1, "p2", 0, 30000);

    Period2AdData p1AdData;
    p1AdData.adBreakId = "p1";
    p1AdData.duration  = 30000;
    p1AdData.filled    = false; // not fully placed yet -> adIdx will be -1 at offset 0
    cdaiObj->mPeriodMap["p1"] = p1AdData;

    bool periodChanged       = false;
    bool mpdChanged          = false;
    bool adStateChanged      = true;
    bool waitForAdBreakCatchup = false;
    bool requireStreamSelection = false;
    std::string currentPeriodId = "p0";

    EXPECT_CALL(*g_mockPrivateInstanceAAMP, GetTSBSessionManager())
        .WillRepeatedly(Return(nullptr));
    EXPECT_CALL(*g_mockPrivateInstanceAAMP, IsLocalAAMPTsbInjection())
        .WillRepeatedly(Return(false));
    EXPECT_CALL(*g_mockPrivateInstanceAAMP, SendAdReservationEvent(_, _, _, _, _, _))
        .Times(AnyNumber());
    mPrivateInstanceAAMP->SetIsPeriodChangeMarked(false);

    // The WAIT2CATCHUP→OUTSIDE_ADBREAK transition (adbreak "p0") does NOT call
    // CheckForAdStart - that path uses GetNextAdInBreak.  Only the proactive
    // onAdEvent(DEFAULT) that my fix makes for forward rates calls
    // CheckForAdStart for the NEW base period ("p1").
    // Require CheckForAdStart to be called at least once: this FAILS if the fix
    // accidentally suppresses the proactive call for forward playback.
    EXPECT_CALL(*g_MockPrivateCDAIObjectMPD, CheckForAdStart(_, _, _, _, _, _))
        .Times(testing::AtLeast(1))
        .WillRepeatedly(
            [](const float& /*rate*/, bool /*init*/, const std::string& /*periodId*/,
               double /*offSet*/, std::string& breakId, double& /*adOffset*/) -> int {
                breakId = "p1";
                return -1; // adbreak found, no ad resolved yet
            });

    bool ret = mTestableStreamAbstractionAAMP_MPD->InvokeSelectSourceOrAdPeriod(
        periodChanged, mpdChanged, adStateChanged, waitForAdBreakCatchup,
        requireStreamSelection, currentPeriodId);

    // The Times(AtLeast(1)) expectation above is the primary assertion: it
    // proves CheckForAdStart was called, i.e. the proactive onAdEvent ran for
    // forward playback (the fix must not suppress it).
    // Additionally verify the WAIT2CATCHUP state was cleared and the function
    // returned true (no early exit).
    EXPECT_NE(cdaiObj->mAdState, AdState::IN_ADBREAK_WAIT2CATCHUP)
        << "Forward playback: WAIT2CATCHUP must have been cleared";
    EXPECT_TRUE(ret);
}
