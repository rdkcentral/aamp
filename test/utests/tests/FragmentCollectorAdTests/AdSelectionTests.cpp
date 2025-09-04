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
#include "admanager_mpd.h"
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

AampConfig *gpGlobalConfig{nullptr};

/**
 * @brief AdSelectionTests tests common base class.
 */
class AdSelectionTests : public ::testing::Test
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

		bool CallOnAdEvent(AdEvent evt)
		{
			return onAdEvent(evt);
		}
		void SetBasePeriodId(const std::string &basePeriodId)
		{
			mBasePeriodId = basePeriodId;
		}
		void SetBasePeriodoffset(double baseperiodoffset)
		{
			mBasePeriodOffset = baseperiodoffset;
		}
	};

	PrivateInstanceAAMP *mPrivateInstanceAAMP;
	TestableStreamAbstractionAAMP_MPD *mStreamAbstractionAAMP_MPD;
	CDAIObject *mCdaiObj;
	const char *mManifest;
	const char *mAdManifest;
	static constexpr const char *TEST_BASE_URL = "http://host/asset/";
	static constexpr const char *TEST_MANIFEST_URL = "http://host/asset/manifest.mpd";
	static constexpr const char *TEST_AD_MANIFEST_URL = "http://host/ad/manifest.mpd";
	MPD* mAdMPD;
	static constexpr const char *mVodManifest = R"(<?xml version="1.0" encoding="utf-8"?>
<MPD xmlns:xsi="http://www.w3.org/2001/XMLSchema-instance" xmlns="urn:mpeg:dash:schema:mpd:2011" xmlns:xlink="http://www.w3.org/1999/xlink" xsi:schemaLocation="urn:mpeg:DASH:schema:MPD:2011 http://standards.iso.org/ittf/PubliclyAvailableStandards/MPEG-DASH_schema_files/DASH-MPD.xsd" profiles="urn:mpeg:dash:profile:isoff-live:2011" type="static" mediaPresentationDuration="PT15M0.0S" minBufferTime="PT4.0S">
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
		<AdaptationSet id="1" contentType="video" segmentAlignment="true" bitstreamSwitching="true" lang="und">
		<EssentialProperty schemeIdUri="http://dashif.org/guidelines/trickmode" value="1"/>
			<Representation id="4" mimeType="video/mp4" codecs="avc1.4d4016" bandwidth="800000" width="640" height="360" frameRate="1/1">
				<SegmentTemplate timescale="10" initialization="dash/iframe_init.m4s" media="dash/iframe_$Time$.m4s" startNumber="1">
					<SegmentTimeline>
						<S t="0" d="9" r="4" />
					</SegmentTimeline>
				</SegmentTemplate>
			</Representation>
		</AdaptationSet>
	</Period>
	<Period id="p1" start="PT30.0S">
		<AdaptationSet id="0" contentType="video">
			<Representation id="0" mimeType="video/mp4" codecs="avc1.640028" bandwidth="800000" width="640" height="360" frameRate="25">
				<SegmentTemplate timescale="2500" initialization="video_p0_init.mp4" media="video_p0_$Number$.m4s" startNumber="1">
					<SegmentTimeline>
						<S t="0" d="5000" r="14" />
					</SegmentTimeline>
				</SegmentTemplate>
			</Representation>
		</AdaptationSet>
		<AdaptationSet id="1" contentType="video" segmentAlignment="true" bitstreamSwitching="true" lang="und">
		<EssentialProperty schemeIdUri="http://dashif.org/guidelines/trickmode" value="1"/>
			<Representation id="4" mimeType="video/mp4" codecs="avc1.4d4016" bandwidth="800000" width="640" height="360" frameRate="1/1">
				<SegmentTemplate timescale="10" initialization="dash/iframe_init.m4s" media="dash/iframe_$Time$.m4s" startNumber="1">
 					<SegmentTimeline>
						<S t="0" d="9" r="4" />
					</SegmentTimeline>
				</SegmentTemplate>
			</Representation>
		</AdaptationSet>
	</Period>
	<Period id="p2" start="PT60.0S">
		<AdaptationSet id="0" contentType="video">
			<Representation id="0" mimeType="video/mp4" codecs="avc1.640028" bandwidth="800000" width="640" height="360" frameRate="25">
				<SegmentTemplate timescale="2500" initialization="video_p0_init.mp4" media="video_p0_$Number$.m4s" startNumber="1">
					<SegmentTimeline>
						<S t="0" d="5000" r="14" />
					</SegmentTimeline>
				</SegmentTemplate>
			</Representation>
		</AdaptationSet>
		<AdaptationSet id="1" contentType="video" segmentAlignment="true" bitstreamSwitching="true" lang="und">
		<EssentialProperty schemeIdUri="http://dashif.org/guidelines/trickmode" value="1"/>
			<Representation id="4" mimeType="video/mp4" codecs="avc1.4d4016" bandwidth="800000" width="640" height="360" frameRate="1/1">
				<SegmentTemplate timescale="10" initialization="dash/iframe_init.m4s" media="dash/iframe_$Time$.m4s" startNumber="1">
					<SegmentTimeline>
						<S t="0" d="9" r="4" />
					</SegmentTimeline>
				</SegmentTemplate>
			</Representation>
		</AdaptationSet>
	</Period>
</MPD>
)";

	static constexpr const char *mLiveManifest = R"(<?xml version="1.0" encoding="utf-8"?>
				<MPD xmlns="urn:mpeg:dash:schema:mpd:2011" availabilityStartTime="2023-01-01T00:00:00Z" maxSegmentDuration="PT2S" minBufferTime="PT4.000S" minimumUpdatePeriod="P100Y" profiles="urn:dvb:dash:profile:dvb-dash:2014,urn:dvb:dash:profile:dvb-dash:isoff-ext-live:2014" publishTime="2023-01-01T00:01:00Z" timeShiftBufferDepth="PT5M" type="dynamic">
						<Period id="p0" start="PT0S" duration="PT30S">
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
						<Period id="p2" start="PT60S">
								<AdaptationSet id="1" contentType="video">
										<Representation id="0" mimeType="video/mp4" codecs="avc1.640028" bandwidth="800000" width="640" height="360" frameRate="25">
												<SegmentTemplate timescale="2500" initialization="video_p2_init.mp4" media="video_p2_$Number$.m4s" startNumber="1">
														<SegmentTimeline>
																<S t="0" d="5000" r="14" />
														</SegmentTimeline>
												</SegmentTemplate>
										</Representation>
								</AdaptationSet>
						</Period>
				</MPD>
				)";

	ManifestDownloadResponsePtr mResponse = MakeSharedManifestDownloadResponsePtr();
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
			{eAAMPConfig_useRialtoSink, false},
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

		g_mockAampUtils = nullptr;

		g_mockAampGstPlayer = new MockAAMPGstPlayer(mPrivateInstanceAAMP);

		mPrivateInstanceAAMP->mIsDefaultOffset = true;

		g_mockPrivateInstanceAAMP = new StrictMock<MockPrivateInstanceAAMP>();

		g_mockMediaStreamContext = new StrictMock<MockMediaStreamContext>();

		g_mockAampMPDDownloader = new StrictMock<MockAampMPDDownloader>();

		g_mockAampStreamSinkManager = new NiceMock<MockAampStreamSinkManager>();

		mStreamAbstractionAAMP_MPD = nullptr;

		mManifest = nullptr;
		mAdMPD = nullptr;
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

		delete mCdaiObj;
		mCdaiObj = nullptr;

		delete mPrivateInstanceAAMP;
		mPrivateInstanceAAMP = nullptr;

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
		mAdManifest = nullptr;
		if(mAdMPD)
		{
			delete mAdMPD;
			mAdMPD = nullptr;
		}
	}

public:
	void GetMPDFromManifest(ManifestDownloadResponsePtr response)
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
	 * @param[in] remoteUrl Manifest url
	 * @param[out] buffer Buffer containing manifest data
	 * @retval true on success
	 */
	ManifestDownloadResponsePtr GetManifestForMPDDownloader()
	{
		if (!mResponse->mMPDInstance)
		{
			ManifestDownloadResponsePtr response = MakeSharedManifestDownloadResponsePtr();
			response->mMPDStatus = AAMPStatusType::eAAMPSTATUS_OK;
			response->mMPDDownloadResponse->iHttpRetValue = 200;
			response->mMPDDownloadResponse->sEffectiveUrl = std::string(TEST_MANIFEST_URL);
			response->mMPDDownloadResponse->mDownloadData.assign(
																 (uint8_t *)mManifest,
																 (uint8_t *)&mManifest[strlen(mManifest)] );
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

		EXPECT_CALL(*g_mockPrivateInstanceAAMP, DownloadsAreEnabled()).WillRepeatedly(Return(true));
		EXPECT_CALL(*g_mockPrivateInstanceAAMP, GetLLDashChunkMode()).WillRepeatedly(Return(false));
		EXPECT_CALL(*g_mockPrivateInstanceAAMP, SetLLDashChunkMode(_));
		/* Create MPD instance. */
		mStreamAbstractionAAMP_MPD = new TestableStreamAbstractionAAMP_MPD(mPrivateInstanceAAMP, seekPos, rate);
		mCdaiObj = new CDAIObjectMPD(mPrivateInstanceAAMP);
		mStreamAbstractionAAMP_MPD->SetCDAIObject(mCdaiObj);

		mPrivateInstanceAAMP->SetManifestUrl(TEST_MANIFEST_URL);

		/* Initialize MPD. */
		EXPECT_CALL(*g_mockPrivateInstanceAAMP, SetState(eSTATE_PREPARING));

		EXPECT_CALL(*g_mockPrivateInstanceAAMP, GetState())
			.Times(AnyNumber())
			.WillRepeatedly(Return(eSTATE_PREPARING));
		// For the time being return the same manifest again
		EXPECT_CALL(*g_mockAampMPDDownloader, GetManifest(_, _, _))
			.WillRepeatedly(WithoutArgs(Invoke(this, &AdSelectionTests::GetManifestForMPDDownloader)));
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
};

/**
 * @brief WaitForAdFallbackTest tests.
 *
 * The tests verify the SelectSourceOrAdPeriod method of StreamAbstractionAAMP_MPD when transitioning
 * from a regular period to a period with ad break, but no reservation is made for the ad
 */
TEST_F(AdSelectionTests, WaitForAdFallbackTest)
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
	status = mStreamAbstractionAAMP_MPD->InvokeIndexNewMPDDocument(false);
	EXPECT_EQ(mStreamAbstractionAAMP_MPD->GetCurrentPeriodIdx(), 0);

	// Index the next period, wait for the selection
	// Set the ad variables, we have finished ad playback and waiting for base period to catchup
	auto cdaiObj = mStreamAbstractionAAMP_MPD->GetCDAIObject();
	cdaiObj->mAdState = AdState::OUTSIDE_ADBREAK;
	std::string periodId = "p1"; // empty adbreak in p1
	// Add ads to the adBreak
	cdaiObj->mAdBreaks = {
		{periodId, AdBreakObject(30000, std::make_shared<std::vector<AdNode>>(), "", 0, 0)}
	};
	cdaiObj->mPeriodMap[periodId] = Period2AdData(false, periodId, 30000 /*in ms*/, {});

	bool periodChanged = false;
	bool adStateChanged = false; // since we finished playing an ad
	bool waitForAdBreakCatchup = false;
	bool requireStreamSelection = false;
	bool mpdChanged = false;
	std::string currentPeriodId = "p0";
	mStreamAbstractionAAMP_MPD->IncrementIteratorPeriodIdx();

	EXPECT_CALL(*g_mockPrivateInstanceAAMP, GetTSBSessionManager()).WillRepeatedly(Return(nullptr));
	EXPECT_CALL(*g_mockPrivateInstanceAAMP, IsLocalAAMPTsbInjection()).WillRepeatedly(Return(false));

	/*
	 * Test the scenario where ad is not placed and we are waiting for base period to catchup
	 */
	ret = mStreamAbstractionAAMP_MPD->InvokeSelectSourceOrAdPeriod(periodChanged, mpdChanged, adStateChanged, waitForAdBreakCatchup, requireStreamSelection, currentPeriodId);
	EXPECT_TRUE(ret);
	EXPECT_EQ(cdaiObj->mAdBreaks[periodId].invalid, true);
	EXPECT_EQ(cdaiObj->mAdState, AdState::OUTSIDE_ADBREAK);
	EXPECT_EQ(mStreamAbstractionAAMP_MPD->CallOnAdEvent(AdEvent::DEFAULT), false);
}
/**
 * @brief onAdEventTest tests.
 * The tests verify the onAdEvent method of StreamAbstractionAAMP_MPD with starting state OUTSIDE_ADBREAK and then moves to a 
 * period having adbreak and starts playing ad, with state changing to IN_ADBREAK_AD_PLAYING
 */

TEST_F(AdSelectionTests, onAdEventTest_1)
{
	static const char *adManifest = R"(<?xml version="1.0" encoding="UTF-8"?>
<MPD xmlns="urn:mpeg:dash:schema:mpd:2011" type="static" profiles="urn:mpeg:dash:profile:isoff-on-demand:2011" minBufferTime="PT1.5S" mediaPresentationDuration="PT0M30S">
<Period id="ad1" start="PT0H0M0.000S">
	<AdaptationSet contentType="video" mimeType="video/mp4" segmentAlignment="true" startWithSAP="1">
	<SegmentTemplate timescale="90000" initialization="video_init.mp4" media="video$Number$.mp4" duration="900000">
		<SegmentTimeline>
		<S t="0" d="2700000"/>
		</SegmentTimeline>
	</SegmentTemplate>
	<Representation id="1" bandwidth="3000000" codecs="avc1.4d401f" width="1280" height="720" frameRate="30"/>
	</AdaptationSet>
</Period>
</MPD>
)";
	InitializeAdMPDObject(adManifest);
	std::string fragmentUrl;
	AAMPStatusType status;
	/* Initialize MPD. The video initialization segment is cached. */
	fragmentUrl = std::string(TEST_BASE_URL) + std::string("video_p0_init.mp4");

	EXPECT_CALL(*g_mockMediaStreamContext, CacheFragment(fragmentUrl, _, _, _, _, true, _, _, _, _, _))
		.Times(1)
		.WillOnce(Return(true));
	status = InitializeMPD(mVodManifest, eTUNETYPE_NEW_NORMAL);
	EXPECT_EQ(status, eAAMPSTATUS_OK);

	auto cdaiObj = mStreamAbstractionAAMP_MPD->GetCDAIObject();
	cdaiObj->mAdState = AdState::OUTSIDE_ADBREAK;
	std::string adPeriodId = "p1";
	std::string endPeriodId = "p2"; // landing in p2
	std::string adUrl = TEST_AD_MANIFEST_URL;

	// Add ads to the adBreak
	cdaiObj->mAdBreaks = {
	    { adPeriodId, AdBreakObject(30000, std::make_shared<std::vector<AdNode>>(), endPeriodId, 0, 30000) }
	};
	// ad is currently placed
	// This test is for the scenario where ad is placed and we are transitioning to ad period
	// So, add the ad to the adBreak
	cdaiObj->mAdBreaks[adPeriodId].ads->emplace_back(false /*invalid*/, true /*placed*/, true /*resolved*/,
		"adId1" /*adId*/, adUrl /*url*/, 30000 /*duration*/, adPeriodId /*basePeriodId*/, 0 /*basePeriodOffset*/, mAdMPD /*mpd*/);
	cdaiObj->mPeriodMap[adPeriodId] = Period2AdData(false, adPeriodId, 30000 /*in ms*/,
	{
		std::make_pair (0, AdOnPeriod(0, 0)), // for adId1 idx=0, offset=0s
	});
	mStreamAbstractionAAMP_MPD->SetBasePeriodId(adPeriodId);
	EXPECT_CALL(*g_mockPrivateInstanceAAMP, GetTSBSessionManager()).WillRepeatedly(Return(nullptr));
	EXPECT_CALL(*g_mockPrivateInstanceAAMP, IsLocalAAMPTsbInjection()).WillRepeatedly(Return(false));
	EXPECT_CALL(*g_mockPrivateInstanceAAMP, SendAdReservationEvent(AAMP_EVENT_AD_RESERVATION_START, "p1", _, _, _)).Times(1);
	EXPECT_CALL(*g_mockPrivateInstanceAAMP, SendAdPlacementEvent(AAMP_EVENT_AD_PLACEMENT_START, "adId1", _, _, _, _, _, _)).Times(1);
	EXPECT_TRUE(mStreamAbstractionAAMP_MPD->CallOnAdEvent(AdEvent::DEFAULT));
	EXPECT_EQ( cdaiObj->mAdState, AdState::IN_ADBREAK_AD_PLAYING);
}

/**
 * @brief onAdEventTest tests.
 * The tests verify the onAdEvent method of StreamAbstractionAAMP_MPD with starting state OUTSIDE_ADBREAK. Then moves to adbreak,
 * Since ad is not downloaded,it enters the IN_ADBREAK_AD_NOT_PLAYING state
 */
TEST_F(AdSelectionTests, onAdEventTest_2)
{
	std::string fragmentUrl;
	AAMPStatusType status;
	/* Initialize MPD. The video initialization segment is cached. */
	fragmentUrl = std::string(TEST_BASE_URL) + std::string("video_p0_init.mp4");

	EXPECT_CALL(*g_mockMediaStreamContext, CacheFragment(fragmentUrl, _, _, _, _, true, _, _, _, _, _))
		.Times(1)
		.WillOnce(Return(true));
	status = InitializeMPD(mVodManifest, eTUNETYPE_NEW_NORMAL);
	EXPECT_EQ(status, eAAMPSTATUS_OK);
	auto cdaiObj = mStreamAbstractionAAMP_MPD->GetCDAIObject();
	cdaiObj->mAdState = AdState::OUTSIDE_ADBREAK;
	std::string adPeriodId = "p1";
	std::string endPeriodId = "p2"; // landing in p2

	// Add ads to the adBreak
	cdaiObj->mAdBreaks = {
	    { adPeriodId, AdBreakObject(30000, std::make_shared<std::vector<AdNode>>(), endPeriodId, 0, 30000) }
	};
	cdaiObj->mAdBreaks[adPeriodId].ads->emplace_back(false /*invalid*/, false /*placed*/, false /*resolved*/,
		"adId1" /*adId*/, "" /*url*/, 30000 /*duration*/, adPeriodId /*basePeriodId*/, 0 /*basePeriodOffset*/, nullptr /*mpd*/);
	cdaiObj->mPeriodMap[adPeriodId] = Period2AdData(false, adPeriodId, 30000 /*in ms*/,
	{
		std::make_pair (0, AdOnPeriod(0, 0)), // for adId1 idx=0, offset=0s
	});

	mStreamAbstractionAAMP_MPD->SetBasePeriodId(adPeriodId);
	EXPECT_CALL(*g_mockPrivateInstanceAAMP, GetTSBSessionManager()).WillRepeatedly(Return(nullptr));
	EXPECT_CALL(*g_mockPrivateInstanceAAMP, IsLocalAAMPTsbInjection()).WillRepeatedly(Return(false));
	EXPECT_CALL(*g_mockPrivateInstanceAAMP, SendAdReservationEvent(AAMP_EVENT_AD_RESERVATION_START, "p1", _, _, _)).Times(1);
	EXPECT_CALL(*g_mockPrivateInstanceAAMP, SendAdPlacementEvent(_, _, _, _, _, _, _, _)).Times(0);
	EXPECT_TRUE(mStreamAbstractionAAMP_MPD->CallOnAdEvent(AdEvent::DEFAULT));
	EXPECT_EQ(cdaiObj->mAdState, AdState::IN_ADBREAK_AD_NOT_PLAYING);
}

/**
 * @brief onAdEventTest tests.
 * The tests verify the onAdEvent method of StreamAbstractionAAMP_MPD with adbreak not validated, wait for ads to be added.
 * Remains in OUTSIDE_ADBREAK state
 */
TEST_F(AdSelectionTests, onAdEventTest_3)
{
	std::string fragmentUrl;
	AAMPStatusType status;
	/* Initialize MPD. The video initialization segment is cached. */
	fragmentUrl = std::string(TEST_BASE_URL) + std::string("video_p0_init.mp4");

	EXPECT_CALL(*g_mockMediaStreamContext, CacheFragment(fragmentUrl, _, _, _, _, true, _, _, _, _, _))
		.Times(1)
		.WillOnce(Return(true));
	status = InitializeMPD(mVodManifest, eTUNETYPE_NEW_NORMAL);
	EXPECT_EQ(status, eAAMPSTATUS_OK);

	auto cdaiObj = mStreamAbstractionAAMP_MPD->GetCDAIObject();
	cdaiObj->mAdState = AdState::OUTSIDE_ADBREAK;
	std::string adPeriodId = "p1"; // landing in p1

	cdaiObj->mPeriodMap[adPeriodId] = Period2AdData();
	cdaiObj->mPeriodMap[adPeriodId].adBreakId = adPeriodId;
	cdaiObj->mAdBreaks = {
		{ adPeriodId, AdBreakObject() }
	};
	mStreamAbstractionAAMP_MPD->SetBasePeriodId(adPeriodId);

	EXPECT_CALL(*g_mockPrivateInstanceAAMP, SendAdReservationEvent(_, _, _, _, _)).Times(0);
	EXPECT_CALL(*g_mockPrivateInstanceAAMP, SendAdPlacementEvent(_, _, _, _, _, _, _, _)).Times(0);
	EXPECT_TRUE(mStreamAbstractionAAMP_MPD->CallOnAdEvent(AdEvent::DEFAULT));
	EXPECT_EQ( cdaiObj->mAdState,  AdState::OUTSIDE_ADBREAK_WAIT4ADS);
	EXPECT_TRUE(cdaiObj->mAdBreaks[adPeriodId].invalid);

	EXPECT_TRUE(mStreamAbstractionAAMP_MPD->CallOnAdEvent(AdEvent::DEFAULT));
	EXPECT_EQ( cdaiObj->mAdState, AdState::OUTSIDE_ADBREAK);
}

/**
 * @brief onAdEventTest tests.
 * The tests verify the onAdEvent method of StreamAbstractionAAMP_MPD where the ad is invalid, so switching to
 * source content. State transition happens from OUTSIDE_ADBREAK to IN_ADBREAK_AD_NOT_PLAYING 
 */
TEST_F(AdSelectionTests, onAdEventTest_4)
{
	std::string fragmentUrl;
	AAMPStatusType status;
	/* Initialize MPD. The video initialization segment is cached. */
	fragmentUrl = std::string(TEST_BASE_URL) + std::string("video_p0_init.mp4");

	EXPECT_CALL(*g_mockMediaStreamContext, CacheFragment(fragmentUrl, _, _, _, _, true, _, _, _, _, _))
		.Times(1)
		.WillOnce(Return(true));
	status = InitializeMPD(mVodManifest, eTUNETYPE_NEW_NORMAL);
	EXPECT_EQ(status, eAAMPSTATUS_OK);

	auto cdaiObj = mStreamAbstractionAAMP_MPD->GetCDAIObject();
	cdaiObj->mAdState = AdState::OUTSIDE_ADBREAK;
	std::string adPeriodId = "p1";
	std::string endPeriodId = "p2"; // landing in p2
	std::string adUrl = TEST_AD_MANIFEST_URL;

	// Add ads to the adBreak
	cdaiObj->mAdBreaks = {
		{ adPeriodId, AdBreakObject(30000, std::make_shared<std::vector<AdNode>>(), endPeriodId, 0, 30000) }
	};

	cdaiObj->mAdBreaks[adPeriodId].ads = std::make_shared<std::vector<AdNode>>();
	cdaiObj->mAdBreaks[adPeriodId].ads->emplace_back(true /*invalid*/, false /*placed*/, true /*resolved*/,
		"adId1" /*adId*/, adUrl /*url*/, 30000 /*duration*/, adPeriodId /*basePeriodId*/, 0 /*basePeriodOffset*/, nullptr /*mpd*/);
	cdaiObj->mPeriodMap[adPeriodId] = Period2AdData(false, adPeriodId, 30000 /*in ms*/,
	{
		std::make_pair (0, AdOnPeriod(0, 0)), // for adId1 idx=0, offset=0s
	});
	mStreamAbstractionAAMP_MPD->SetBasePeriodId(adPeriodId);

	EXPECT_CALL(*g_mockPrivateInstanceAAMP, GetTSBSessionManager()).WillRepeatedly(Return(nullptr));
	EXPECT_CALL(*g_mockPrivateInstanceAAMP, IsLocalAAMPTsbInjection()).WillRepeatedly(Return(false));
	EXPECT_CALL(*g_mockPrivateInstanceAAMP, SendAdReservationEvent(AAMP_EVENT_AD_RESERVATION_START, adPeriodId, _, _, _)).Times(1);
	EXPECT_CALL(*g_mockPrivateInstanceAAMP, SendAdPlacementEvent(_, _, _, _, _, _, _, _)).Times(0);
	EXPECT_TRUE(mStreamAbstractionAAMP_MPD->CallOnAdEvent(AdEvent::DEFAULT));
	EXPECT_EQ(cdaiObj->mAdState, AdState::IN_ADBREAK_AD_NOT_PLAYING);
}

/**
 * @brief onAdEventTest tests.
 * The tests verify the onAdEvent method of StreamAbstractionAAMP_MPD where ad playback occurs with state IN_ADBREAK_AD_PLAYING.
 * Once ad download finished and waiting to catch up baseperiod, state changes to IN_ADBREAK_WAIT2CATCHUP
 */
TEST_F(AdSelectionTests, onAdEventTest_5)
{
	static const char *adManifest = R"(<?xml version="1.0" encoding="UTF-8"?>
<MPD xmlns="urn:mpeg:dash:schema:mpd:2011" type="static" profiles="urn:mpeg:dash:profile:isoff-on-demand:2011" minBufferTime="PT1.5S" mediaPresentationDuration="PT0M30S">
<Period id="ad1" start="PT0H0M0.000S">
	<AdaptationSet contentType="video" mimeType="video/mp4" segmentAlignment="true" startWithSAP="1">
	<SegmentTemplate timescale="90000" initialization="video_init.mp4" media="video$Number$.mp4" duration="900000">
		<SegmentTimeline>
		<S t="0" d="2700000"/>
		</SegmentTimeline>
	</SegmentTemplate>
	<Representation id="1" bandwidth="3000000" codecs="avc1.4d401f" width="1280" height="720" frameRate="30"/>
	</AdaptationSet>
</Period>
</MPD>
)";
	InitializeAdMPDObject(adManifest);
	std::string fragmentUrl;
	AAMPStatusType status;
	/* Initialize MPD. The video initialization segment is cached. */
	fragmentUrl = std::string(TEST_BASE_URL) + std::string("video_p0_init.mp4");

	EXPECT_CALL(*g_mockMediaStreamContext, CacheFragment(fragmentUrl, _, _, _, _, true, _, _, _, _, _))
		.Times(1)
		.WillOnce(Return(true));
	status = InitializeMPD(mVodManifest, eTUNETYPE_NEW_NORMAL);
	EXPECT_EQ(status, eAAMPSTATUS_OK);

	auto cdaiObj = mStreamAbstractionAAMP_MPD->GetCDAIObject();
	cdaiObj->mAdState = AdState::IN_ADBREAK_AD_PLAYING;
	std::string adPeriodId = "p1";
	std::string endPeriodId = "p2"; // landing in p2
	std::string adUrl = TEST_AD_MANIFEST_URL;

	// Add ads to the adBreak
	cdaiObj->mAdBreaks = {
		{ adPeriodId, AdBreakObject(30000, std::make_shared<std::vector<AdNode>>(), endPeriodId, 0, 30000) }
	};
	// ad is currently placed
	// This test is for the scenario where ad is placed and we are transitioning to ad period
	// So, add the ad to the adBreak
	cdaiObj->mAdBreaks[adPeriodId].ads->emplace_back(false /*invalid*/, true /*placed*/, true /*resolved*/,
		"adId1" /*adId*/, adUrl /*url*/, 30000 /*duration*/, adPeriodId /*basePeriodId*/, 0 /*basePeriodOffset*/, mAdMPD /*mpd*/);
	cdaiObj->mPeriodMap[adPeriodId] = Period2AdData(false, adPeriodId, 30000 /*in ms*/,
	{
		std::make_pair (0, AdOnPeriod(0, 0)), // for adId1 idx=0, offset=0s
	});
	cdaiObj->mCurAdIdx = 0;
	cdaiObj->mCurAds = cdaiObj->mAdBreaks[adPeriodId].ads;
	mStreamAbstractionAAMP_MPD->SetBasePeriodId(adPeriodId);

	EXPECT_CALL(*g_mockPrivateInstanceAAMP, GetTSBSessionManager()).WillRepeatedly(Return(nullptr));
	EXPECT_CALL(*g_mockPrivateInstanceAAMP, IsLocalAAMPTsbInjection()).WillRepeatedly(Return(false));
	EXPECT_CALL(*g_mockPrivateInstanceAAMP, SendAdReservationEvent(_, _, _, _, _)).Times(0);
	EXPECT_CALL(*g_mockPrivateInstanceAAMP, SendAdPlacementEvent(AAMP_EVENT_AD_PLACEMENT_END, "adId1", _, _, _, _, _, _)).Times(1);
	EXPECT_TRUE(mStreamAbstractionAAMP_MPD->CallOnAdEvent(AdEvent::AD_FINISHED));
	EXPECT_EQ( cdaiObj->mAdState, AdState::IN_ADBREAK_WAIT2CATCHUP);
}

/**
 * @brief onAdEventTest tests.
 * The tests verify the onAdEvent method of StreamAbstractionAAMP_MPD where period is in adbreak and ad starts playing with state
 * IN_ADBREAK_AD_PLAYING. while playing an ad failure happens and state changes to IN_ADBREAK_AD_NOT_PLAYING 
 */
TEST_F(AdSelectionTests, onAdEventTest_6)
{
	static const char *adManifest = R"(<?xml version="1.0" encoding="UTF-8"?>
<MPD xmlns="urn:mpeg:dash:schema:mpd:2011" type="static" profiles="urn:mpeg:dash:profile:isoff-on-demand:2011" minBufferTime="PT1.5S" mediaPresentationDuration="PT0M30S">
<Period id="ad1" start="PT0H0M0.000S">
	<AdaptationSet contentType="video" mimeType="video/mp4" segmentAlignment="true" startWithSAP="1">
	<SegmentTemplate timescale="90000" initialization="video_init.mp4" media="video$Number$.mp4" duration="900000">
		<SegmentTimeline>
		<S t="0" d="2700000"/>
		</SegmentTimeline>
	</SegmentTemplate>
	<Representation id="1" bandwidth="3000000" codecs="avc1.4d401f" width="1280" height="720" frameRate="30"/>
	</AdaptationSet>
</Period>
</MPD>
)";
	InitializeAdMPDObject(adManifest);
	std::string fragmentUrl;
	AAMPStatusType status;
	/* Initialize MPD. The video initialization segment is cached. */
	fragmentUrl = std::string(TEST_BASE_URL) + std::string("video_p0_init.mp4");

	EXPECT_CALL(*g_mockMediaStreamContext, CacheFragment(fragmentUrl, _, _, _, _, true, _, _, _, _, _))
		.Times(1)
		.WillOnce(Return(true));
	status = InitializeMPD(mVodManifest, eTUNETYPE_NEW_NORMAL);
	EXPECT_EQ(status, eAAMPSTATUS_OK);

	auto cdaiObj = mStreamAbstractionAAMP_MPD->GetCDAIObject();
	cdaiObj->mAdState = AdState::IN_ADBREAK_AD_PLAYING;
	std::string adPeriodId = "p1";
	std::string endPeriodId = "p2"; // landing in p2
	std::string adUrl = TEST_AD_MANIFEST_URL;

	// Add ads to the adBreak
	cdaiObj->mAdBreaks = {
	    { adPeriodId, AdBreakObject(30000, std::make_shared<std::vector<AdNode>>(), endPeriodId, 0, 30000) }
	};
	// ad is currently placed
	// This test is for the scenario where ad is placed and we are transitioning to ad period
	// So, add the ad to the adBreak
	cdaiObj->mAdBreaks[adPeriodId].ads->emplace_back(false /*invalid*/, true /*placed*/, true /*resolved*/,
		"adId1" /*adId*/, adUrl /*url*/, 30000 /*duration*/, adPeriodId /*basePeriodId*/, 0 /*basePeriodOffset*/, mAdMPD /*mpd*/);
	cdaiObj->mPeriodMap[adPeriodId] = Period2AdData(false, adPeriodId, 30000 /*in ms*/,
	{
		std::make_pair (0, AdOnPeriod(0, 0)), // for adId1 idx=0, offset=0s
	});
	cdaiObj->mCurAdIdx = 0;
	cdaiObj->mCurAds = cdaiObj->mAdBreaks[adPeriodId].ads;

	mStreamAbstractionAAMP_MPD->SetBasePeriodId(adPeriodId);
	EXPECT_CALL(*g_mockPrivateInstanceAAMP, GetTSBSessionManager()).WillRepeatedly(Return(nullptr));
	EXPECT_CALL(*g_mockPrivateInstanceAAMP, IsLocalAAMPTsbInjection()).WillRepeatedly(Return(false));
	EXPECT_CALL(*g_mockPrivateInstanceAAMP, SendAdReservationEvent(_, _, _, _, _)).Times(0);
	EXPECT_CALL(*g_mockPrivateInstanceAAMP, SendAdPlacementEvent(AAMP_EVENT_AD_PLACEMENT_ERROR, "adId1", _, _, _, _, _, _)).Times(1);
	EXPECT_CALL(*g_mockPrivateInstanceAAMP, SendAdPlacementEvent(AAMP_EVENT_AD_PLACEMENT_END, "adId1", _, _, _, _, _, _)).Times(1);
	EXPECT_TRUE(mStreamAbstractionAAMP_MPD->CallOnAdEvent(AdEvent::AD_FAILED));
	EXPECT_EQ( cdaiObj->mAdState, AdState::IN_ADBREAK_AD_NOT_PLAYING);
}

/**
 * @brief onAdEventTest tests.
 * The tests verify the onAdEvent method of StreamAbstractionAAMP_MPD where current source period is in adbreak,
 * but no ad opportunities are present. Its a bug case. State transition happens from IN_ADBREAK_WAIT2CATCHUP
 * to OUTSIDE_ADBREAK
 */
TEST_F(AdSelectionTests, onAdEventTest_7)
{
	static const char *adManifest = R"(<?xml version="1.0" encoding="UTF-8"?>
<MPD xmlns="urn:mpeg:dash:schema:mpd:2011" type="static" profiles="urn:mpeg:dash:profile:isoff-on-demand:2011" minBufferTime="PT1.5S" mediaPresentationDuration="PT0M30S">
<Period id="ad1" start="PT0H0M0.000S">
	<AdaptationSet contentType="video" mimeType="video/mp4" segmentAlignment="true" startWithSAP="1">
	<SegmentTemplate timescale="90000" initialization="video_init.mp4" media="video$Number$.mp4" duration="900000">
		<SegmentTimeline>
		<S t="0" d="2700000"/>
		</SegmentTimeline>
	</SegmentTemplate>
	<Representation id="1" bandwidth="3000000" codecs="avc1.4d401f" width="1280" height="720" frameRate="30"/>
	</AdaptationSet>
</Period>
</MPD>
)";
	InitializeAdMPDObject(adManifest);
	std::string fragmentUrl;
	AAMPStatusType status;
	/* Initialize MPD. The video initialization segment is cached. */
	fragmentUrl = std::string(TEST_BASE_URL) + std::string("video_p0_init.mp4");

	EXPECT_CALL(*g_mockMediaStreamContext, CacheFragment(fragmentUrl, _, _, _, _, true, _, _, _, _, _))
		.Times(1)
		.WillOnce(Return(true));
	status = InitializeMPD(mVodManifest, eTUNETYPE_NEW_NORMAL);
	EXPECT_EQ(status, eAAMPSTATUS_OK);

	auto cdaiObj = mStreamAbstractionAAMP_MPD->GetCDAIObject();
	cdaiObj->mAdState = AdState::IN_ADBREAK_WAIT2CATCHUP;
	std::string adPeriodId = "p1";
	std::string endPeriodId = "p2"; // landing in p2
	std::string adUrl = TEST_AD_MANIFEST_URL;

	// Add ads to the adBreak
	cdaiObj->mAdBreaks = {
		{ adPeriodId, AdBreakObject(30000, std::make_shared<std::vector<AdNode>>(), endPeriodId, 0, 30000) }
	};
	// ad is currently placed
	// This test is for the scenario where ad is placed and we are transitioning to ad period
	// So, add the ad to the adBreak
	cdaiObj->mAdBreaks[adPeriodId].ads->emplace_back(false /*invalid*/, true /*placed*/, true /*resolved*/,
		"adId1" /*adId*/, adUrl /*url*/, 30000 /*duration*/, adPeriodId /*basePeriodId*/, 0 /*basePeriodOffset*/, mAdMPD /*mpd*/);
	cdaiObj->mPeriodMap[adPeriodId] = Period2AdData(false, adPeriodId, 30000 /*in ms*/,
	{
		std::make_pair (0, AdOnPeriod(0, 0)), // for adId1 idx=0, offset=0s
	});
	cdaiObj->mCurAdIdx = -1;
	cdaiObj->mCurAds = cdaiObj->mAdBreaks[adPeriodId].ads;
	mStreamAbstractionAAMP_MPD->SetBasePeriodId(adPeriodId);

	EXPECT_CALL(*g_mockPrivateInstanceAAMP, SendAdReservationEvent(_, _, _, _, _)).Times(0);
	EXPECT_CALL(*g_mockPrivateInstanceAAMP, SendAdPlacementEvent(_, _, _, _, _, _, _, _)).Times(0);
	mStreamAbstractionAAMP_MPD->CallOnAdEvent(AdEvent::DEFAULT);
	EXPECT_EQ( cdaiObj->mAdState, AdState::OUTSIDE_ADBREAK);
}

/**
 * @brief onAdEventTest tests.
 * The tests verify the onAdEvent method of StreamAbstractionAAMP_MPD where all ads in adbreak finished 
 * and waiting for adbreak to place with state IN_ADBREAK_WAIT2CATCHUP. Once adbreak is placed , state moves
 * to OUTSIDE_ADBREAK 
 */
TEST_F(AdSelectionTests, onAdEventTest_8)
{
	static const char *adManifest = R"(<?xml version="1.0" encoding="UTF-8"?>
<MPD xmlns="urn:mpeg:dash:schema:mpd:2011" type="static" profiles="urn:mpeg:dash:profile:isoff-on-demand:2011" minBufferTime="PT1.5S" mediaPresentationDuration="PT0M30S">
<Period id="ad1" start="PT0H0M0.000S">
	<AdaptationSet contentType="video" mimeType="video/mp4" segmentAlignment="true" startWithSAP="1">
	<SegmentTemplate timescale="90000" initialization="video_init.mp4" media="video$Number$.mp4" duration="900000">
		<SegmentTimeline>
		<S t="0" d="2700000"/>
		</SegmentTimeline>
	</SegmentTemplate>
	<Representation id="1" bandwidth="3000000" codecs="avc1.4d401f" width="1280" height="720" frameRate="30"/>
	</AdaptationSet>
</Period>
</MPD>
)";
	InitializeAdMPDObject(adManifest);
	std::string fragmentUrl;
	AAMPStatusType status;
	/* Initialize MPD. The video initialization segment is cached. */
	fragmentUrl = std::string(TEST_BASE_URL) + std::string("video_p0_init.mp4");

	EXPECT_CALL(*g_mockMediaStreamContext, CacheFragment(fragmentUrl, _, _, _, _, true, _, _, _, _, _))
		.Times(1)
		.WillOnce(Return(true));
	status = InitializeMPD(mVodManifest, eTUNETYPE_NEW_NORMAL);
	EXPECT_EQ(status, eAAMPSTATUS_OK);

	auto cdaiObj = mStreamAbstractionAAMP_MPD->GetCDAIObject();
	cdaiObj->mAdState = AdState::IN_ADBREAK_WAIT2CATCHUP;
	std::string adPeriodId = "p1";
	std::string endPeriodId = "p2"; // landing in p2
	std::string adUrl = TEST_AD_MANIFEST_URL;

	// Add ads to the adBreak
	cdaiObj->mAdBreaks = {
		{ adPeriodId, AdBreakObject(30000, std::make_shared<std::vector<AdNode>>(), endPeriodId, 0, 30000) }
	};
	// ad is currently placed
	// This test is for the scenario where ad is placed and we are transitioning to ad period
	// So, add the ad to the adBreak
	cdaiObj->mAdBreaks[adPeriodId].ads->emplace_back(false /*invalid*/, true /*placed*/, true /*resolved*/,
		"adId1" /*adId*/, adUrl /*url*/, 30000 /*duration*/, adPeriodId /*basePeriodId*/, 0 /*basePeriodOffset*/, mAdMPD /*mpd*/);
	cdaiObj->mPeriodMap[adPeriodId] = Period2AdData(false, adPeriodId, 30000 /*in ms*/,
	{
		std::make_pair (0, AdOnPeriod(0, 0)), // for adId1 idx=0, offset=0s
	});

	cdaiObj->mCurAdIdx = 0;
	cdaiObj->mCurAds = cdaiObj->mAdBreaks[adPeriodId].ads;
	mStreamAbstractionAAMP_MPD->SetBasePeriodId(adPeriodId);

	EXPECT_CALL(*g_mockPrivateInstanceAAMP, GetTSBSessionManager()).WillRepeatedly(Return(nullptr));
	EXPECT_CALL(*g_mockPrivateInstanceAAMP, IsLocalAAMPTsbInjection()).WillRepeatedly(Return(false));
	EXPECT_CALL(*g_mockPrivateInstanceAAMP, SendAdReservationEvent(AAMP_EVENT_AD_RESERVATION_END, adPeriodId, _, _, _)).Times(1);
	EXPECT_CALL(*g_mockPrivateInstanceAAMP, SendAdPlacementEvent(_, _, _, _, _, _, _, _)).Times(0);
	EXPECT_FALSE(mStreamAbstractionAAMP_MPD->CallOnAdEvent(AdEvent::DEFAULT));
	EXPECT_EQ(cdaiObj->mAdState, AdState::IN_ADBREAK_WAIT2CATCHUP);

	// Adbreak is now placed
	cdaiObj->mCurPlayingBreakId = adPeriodId;
	cdaiObj->mAdBreaks[cdaiObj->mCurPlayingBreakId].mAdBreakPlaced = true;
	EXPECT_TRUE(mStreamAbstractionAAMP_MPD->CallOnAdEvent(AdEvent::DEFAULT));
	EXPECT_EQ(cdaiObj->mAdState, AdState::OUTSIDE_ADBREAK);
}

/**
 * @brief onAdEventTest tests.
 * The tests verify the onAdEvent method of StreamAbstractionAAMP_MPD where period 
 * is in adbreak with multiple ads and state is IN_ADBREAK_WAIT2CATCHUP
 * and when ad is available and starts playing, state changes to IN_ADBREAK_AD_PLAYING
 */
TEST_F(AdSelectionTests, onAdEventTest_9)
{
	static const char *adManifest = R"(<?xml version="1.0" encoding="UTF-8"?>
<MPD xmlns="urn:mpeg:dash:schema:mpd:2011" type="static" profiles="urn:mpeg:dash:profile:isoff-on-demand:2011" minBufferTime="PT1.5S" mediaPresentationDuration="PT0M15S">
<Period id="ad1" start="PT0H0M0.000S">
	<AdaptationSet contentType="video" mimeType="video/mp4" segmentAlignment="true" startWithSAP="1">
	<SegmentTemplate timescale="90000" initialization="video_init.mp4" media="video$Number$.mp4" duration="900000">
		<SegmentTimeline>
		<S t="0" d="900000"/>
		<S d="450000"/>
		</SegmentTimeline>
	</SegmentTemplate>
	<Representation id="1" bandwidth="3000000" codecs="avc1.4d401f" width="1280" height="720" frameRate="30"/>
	</AdaptationSet>
</Period>
</MPD>
)";
	InitializeAdMPDObject(adManifest);
	std::string fragmentUrl;
	AAMPStatusType status;
	/* Initialize MPD. The video initialization segment is cached. */
	fragmentUrl = std::string(TEST_BASE_URL) + std::string("video_p0_init.mp4");

	EXPECT_CALL(*g_mockMediaStreamContext, CacheFragment(fragmentUrl, _, _, _, _, true, _, _, _, _, _))
		.Times(1)
		.WillOnce(Return(true));
	status = InitializeMPD(mVodManifest, eTUNETYPE_NEW_NORMAL);
	EXPECT_EQ(status, eAAMPSTATUS_OK);

	auto cdaiObj = mStreamAbstractionAAMP_MPD->GetCDAIObject();
	cdaiObj->mAdState = AdState::IN_ADBREAK_WAIT2CATCHUP;
	std::string adPeriodId = "p1";
	std::string endPeriodId = "p2"; // landing in p2
	std::string adUrl = TEST_AD_MANIFEST_URL;

	// Add ads to the adBreak
	cdaiObj->mAdBreaks = {
		{ adPeriodId, AdBreakObject(30000, std::make_shared<std::vector<AdNode>>(), endPeriodId, 0, 30000) }
	};
	// ad is currently placed
	// This test is for the scenario where ad is placed and we are transitioning to ad period
	// So, add the ad to the adBreak
	cdaiObj->mAdBreaks[adPeriodId].ads->emplace_back(false /*invalid*/, true /*placed*/, true /*resolved*/,
		"adId1" /*adId*/, adUrl /*url*/, 15000 /*duration*/, adPeriodId /*basePeriodId*/, 0 /*basePeriodOffset*/, mAdMPD /*mpd*/);
	cdaiObj->mAdBreaks[adPeriodId].ads->emplace_back(false /*invalid*/, true /*placed*/, true /*resolved*/,
		"adId2" /*adId*/, adUrl /*url*/, 15000 /*duration*/, adPeriodId /*basePeriodId*/, 15000 /*basePeriodOffset*/, mAdMPD /*mpd*/);
	cdaiObj->mPeriodMap[adPeriodId] = Period2AdData(false, adPeriodId, 30000 /*in ms*/,
	{
		std::make_pair (0, AdOnPeriod(0, 0)), // for adId1 idx=0, offset=0s
		std::make_pair (15000, AdOnPeriod(1, 0))
	});
	cdaiObj->mCurAdIdx = 0;
	cdaiObj->mCurAds = cdaiObj->mAdBreaks[adPeriodId].ads;
	mStreamAbstractionAAMP_MPD->SetBasePeriodId(adPeriodId);

	EXPECT_CALL(*g_mockPrivateInstanceAAMP, GetTSBSessionManager()).WillRepeatedly(Return(nullptr));
	EXPECT_CALL(*g_mockPrivateInstanceAAMP, IsLocalAAMPTsbInjection()).WillRepeatedly(Return(false));
	EXPECT_CALL(*g_mockPrivateInstanceAAMP, SendAdReservationEvent(_, _, _, _, _)).Times(0);
	EXPECT_CALL(*g_mockPrivateInstanceAAMP, SendAdPlacementEvent(AAMP_EVENT_AD_PLACEMENT_START, "adId2", _, _, _, _, _, _)).Times(1);
	EXPECT_TRUE(mStreamAbstractionAAMP_MPD->CallOnAdEvent(AdEvent::DEFAULT));
	EXPECT_EQ( cdaiObj->mAdState, AdState::IN_ADBREAK_AD_PLAYING);
}

/**
 * @brief onAdEventTest tests.
 * The tests verify the onAdEvent method of StreamAbstractionAAMP_MPD where period is in adbreak and first ad is not able to 
 * download with state IN_ADBREAK_AD_NOT_PLAYING and moving to next ad which is available and able to download and state 
 * changes to IN_ADBREAK_AD_PLAYING 
 */
TEST_F(AdSelectionTests, onAdEventTest_10)
{
	static const char *adManifest = R"(<?xml version="1.0" encoding="UTF-8"?>
<MPD xmlns="urn:mpeg:dash:schema:mpd:2011" type="static" profiles="urn:mpeg:dash:profile:isoff-on-demand:2011" minBufferTime="PT1.5S" mediaPresentationDuration="PT0M15S">
<Period id="ad1" start="PT0H0M0.000S">
	<AdaptationSet contentType="video" mimeType="video/mp4" segmentAlignment="true" startWithSAP="1">
	<SegmentTemplate timescale="90000" initialization="video_init.mp4" media="video$Number$.mp4" duration="900000">
		<SegmentTimeline>
		<S t="0" d="900000"/>
		<S d="450000"/>
		</SegmentTimeline>
	</SegmentTemplate>
	<Representation id="1" bandwidth="3000000" codecs="avc1.4d401f" width="1280" height="720" frameRate="30"/>
	</AdaptationSet>
</Period>
</MPD>
)";
	InitializeAdMPDObject(adManifest);
	std::string fragmentUrl;
	AAMPStatusType status;
	/* Initialize MPD. The video initialization segment is cached. */
	fragmentUrl = std::string(TEST_BASE_URL) + std::string("video_p0_init.mp4");

	EXPECT_CALL(*g_mockMediaStreamContext, CacheFragment(fragmentUrl, _, _, _, _, true, _, _, _, _, _))
		.Times(1)
		.WillOnce(Return(true));
	status = InitializeMPD(mVodManifest, eTUNETYPE_NEW_NORMAL);
	EXPECT_EQ(status, eAAMPSTATUS_OK);

	auto cdaiObj = mStreamAbstractionAAMP_MPD->GetCDAIObject();
	cdaiObj->mAdState = AdState::IN_ADBREAK_AD_NOT_PLAYING;
	std::string adPeriodId = "p1";
	std::string endPeriodId = "p2"; // landing in p2
	std::string adUrl = TEST_AD_MANIFEST_URL;
	// Add ads to the adBreak
	cdaiObj->mAdBreaks = {
		{ adPeriodId, AdBreakObject(30000, std::make_shared<std::vector<AdNode>>(), endPeriodId, 0, 30000) }
	};
	// ad is currently placed
	// This test is for the scenario where ad is placed and we are transitioning to ad period
	// So, add the ad to the adBreak
	cdaiObj->mAdBreaks[adPeriodId].ads->emplace_back(false /*invalid*/, true /*placed*/, true /*resolved*/,
		"adId1" /*adId*/, adUrl /*url*/, 15000 /*duration*/, adPeriodId /*basePeriodId*/, 0 /*basePeriodOffset*/, mAdMPD /*mpd*/);
	cdaiObj->mAdBreaks[adPeriodId].ads->emplace_back(false /*invalid*/, true /*placed*/, true /*resolved*/,
		"adId1" /*adId*/, adUrl /*url*/, 15000 /*duration*/, adPeriodId /*basePeriodId*/, 15000 /*basePeriodOffset*/, mAdMPD /*mpd*/);
	cdaiObj->mPeriodMap[adPeriodId] = Period2AdData(false, adPeriodId, 30000 /*in ms*/,
	{
		std::make_pair (0, AdOnPeriod(0, 0)), // for adId1 idx=0, offset=0s
		std::make_pair (15000, AdOnPeriod(1, 0)),
	});
	mStreamAbstractionAAMP_MPD->SetBasePeriodId(adPeriodId);

	EXPECT_CALL(*g_mockPrivateInstanceAAMP, GetTSBSessionManager()).WillRepeatedly(Return(nullptr));
	EXPECT_CALL(*g_mockPrivateInstanceAAMP, IsLocalAAMPTsbInjection()).WillRepeatedly(Return(false));
	EXPECT_CALL(*g_mockPrivateInstanceAAMP, SendAdPlacementEvent(AAMP_EVENT_AD_PLACEMENT_START, "adId1", _, _, _, _, _, _)).Times(1);
	EXPECT_TRUE(mStreamAbstractionAAMP_MPD->CallOnAdEvent(AdEvent::BASE_OFFSET_CHANGE));
	EXPECT_EQ( cdaiObj->mAdState, AdState::IN_ADBREAK_AD_PLAYING);
}

/**
 * @brief onAdEventTest tests.
 * The tests verify the onAdEvent method of StreamAbstractionAAMP_MPD where state transition happens from 
 * IN_ADBREAK_AD_NOT_PLAYING to OUTSIDE_ADBREAK since adbreak ended with ad not playing
 */
TEST_F(AdSelectionTests, onAdEventTest_11)
{
	std::string fragmentUrl;
	AAMPStatusType status;
	/* Initialize MPD. The video initialization segment is cached. */
	fragmentUrl = std::string(TEST_BASE_URL) + std::string("video_p0_init.mp4");

	EXPECT_CALL(*g_mockMediaStreamContext, CacheFragment(fragmentUrl, _, _, _, _, true, _, _, _, _, _))
		.Times(1)
		.WillOnce(Return(true));
	status = InitializeMPD(mVodManifest, eTUNETYPE_NEW_NORMAL);
	EXPECT_EQ(status, eAAMPSTATUS_OK);

	auto cdaiObj = mStreamAbstractionAAMP_MPD->GetCDAIObject();
	cdaiObj->mAdState = AdState::IN_ADBREAK_AD_NOT_PLAYING;

	std::string basePeriodId = "p2";//After adbreak p1, player moves to baseperiod p2 with period change event
	mStreamAbstractionAAMP_MPD->SetBasePeriodId(basePeriodId);

	EXPECT_TRUE(mStreamAbstractionAAMP_MPD->CallOnAdEvent(AdEvent::PERIOD_CHANGE));
	EXPECT_EQ( cdaiObj->mAdState, AdState::OUTSIDE_ADBREAK);
}

/**
 * @brief onAdEventTest tests.
 * The tests verify the onAdEvent method of StreamAbstractionAAMP_MPD where ads are there,
 * but one ad is yet to be placed. So adbreak is not placed. State remains in IN_ADBREAK_WAIT2CATCHUP state. 
 */
TEST_F(AdSelectionTests, onAdEventTest_12)
{
	static const char *adManifest = R"(<?xml version="1.0" encoding="UTF-8"?>
<MPD xmlns="urn:mpeg:dash:schema:mpd:2011" type="static" profiles="urn:mpeg:dash:profile:isoff-on-demand:2011" minBufferTime="PT1.5S" mediaPresentationDuration="PT0M15S">
<Period id="ad1" start="PT0H0M0.000S">
	<AdaptationSet contentType="video" mimeType="video/mp4" segmentAlignment="true" startWithSAP="1">
	<SegmentTemplate timescale="90000" initialization="video_init.mp4" media="video$Number$.mp4" duration="900000">
		<SegmentTimeline>
		<S t="0" d="900000"/>
		<S d="450000"/>
		</SegmentTimeline>
	</SegmentTemplate>
	<Representation id="1" bandwidth="3000000" codecs="avc1.4d401f" width="1280" height="720" frameRate="30"/>
	</AdaptationSet>
</Period>
</MPD>
)";
	InitializeAdMPDObject(adManifest);
	std::string fragmentUrl;
	AAMPStatusType status;
	/* Initialize MPD. The video initialization segment is cached. */
	fragmentUrl = std::string(TEST_BASE_URL) + std::string("video_p0_init.mp4");

	EXPECT_CALL(*g_mockMediaStreamContext, CacheFragment(fragmentUrl, _, _, _, _, true, _, _, _, _, _))
		.Times(1)
		.WillOnce(Return(true));
	status = InitializeMPD(mVodManifest, eTUNETYPE_NEW_NORMAL);
	EXPECT_EQ(status, eAAMPSTATUS_OK);

	auto cdaiObj = mStreamAbstractionAAMP_MPD->GetCDAIObject();
	cdaiObj->mAdState = AdState::IN_ADBREAK_WAIT2CATCHUP;
	std::string adPeriodId = "p1";
	std::string endPeriodId = "p2"; // landing in p2
	std::string adUrl = TEST_AD_MANIFEST_URL;

	// Add ads to the adBreak
	cdaiObj->mAdBreaks = {
		{ adPeriodId, AdBreakObject(30000, std::make_shared<std::vector<AdNode>>(), endPeriodId, 0, 30000) }
	};
	// ad is currently placed
	// This test is for the scenario where ad is placed and we are transitioning to ad period
	// So, add the ad to the adBreak
	cdaiObj->mAdBreaks[adPeriodId].ads->emplace_back(false /*invalid*/, true /*placed*/, true /*resolved*/,
		"adId1" /*adId*/, adUrl /*url*/, 15000 /*duration*/, adPeriodId /*basePeriodId*/, 0 /*basePeriodOffset*/, mAdMPD /*mpd*/);
	cdaiObj->mAdBreaks[adPeriodId].ads->emplace_back(true /*invalid*/, false /*placed*/, true /*resolved*/,
		"adId2" /*adId*/, adUrl /*url*/, 15000 /*duration*/, adPeriodId /*basePeriodId*/, 15000 /*basePeriodOffset*/, mAdMPD /*mpd*/);
	cdaiObj->mPeriodMap[adPeriodId] = Period2AdData(false, adPeriodId, 30000 /*in ms*/,
	{
		std::make_pair (0, AdOnPeriod(0, 0)), // for adId1 idx=0, offset=0s
		std::make_pair (15000, AdOnPeriod(1, 0)),
	});
	cdaiObj->mCurAdIdx = 0;
	cdaiObj->mCurAds = cdaiObj->mAdBreaks[adPeriodId].ads;
	mStreamAbstractionAAMP_MPD->SetBasePeriodId(adPeriodId);

	EXPECT_CALL(*g_mockPrivateInstanceAAMP, SendAdReservationEvent(_, _, _, _, _)).Times(0);
	EXPECT_CALL(*g_mockPrivateInstanceAAMP, SendAdPlacementEvent(_, _, _, _, _, _, _, _)).Times(0);
	EXPECT_FALSE(mStreamAbstractionAAMP_MPD->CallOnAdEvent(AdEvent::DEFAULT));
	EXPECT_EQ(cdaiObj->mAdState, AdState::IN_ADBREAK_WAIT2CATCHUP);
	EXPECT_FALSE(cdaiObj->mAdBreaks[adPeriodId].mAdBreakPlaced);
}

/**
 * @brief onAdEventTest tests.
 * The tests verify the onAdEvent method of StreamAbstractionAAMP_MPD where period p1 contains adbreak. At first player is in
 * OUTSIDE_ADBREAK state. Upon rewind, valid ad is not found at the offset, its a partial ad, so state moved to 
 * IN_ADBREAK_AD_NOT_PLAYING. After reaching the offset,ad is found and starts playing and changing state to IN_ADBREAK_AD_PLAYING.
 * Once ad finishes state moves to IN_ADBREAK_WAIT2CATCHUP. 
 * All ads in adbreak finished and state moves to OUTSIDE_ADBREAK.
 */
TEST_F(AdSelectionTests, onAdEventTest_13)
{
	static const char *adManifest = R"(<?xml version="1.0" encoding="UTF-8"?>
<MPD xmlns="urn:mpeg:dash:schema:mpd:2011" type="static" profiles="urn:mpeg:dash:profile:isoff-on-demand:2011" minBufferTime="PT1.5S" mediaPresentationDuration="PT0M10S">
<Period id="ad1" start="PT0H0M0.000S">
	<AdaptationSet contentType="video" mimeType="video/mp4" segmentAlignment="true" startWithSAP="1">
	<SegmentTemplate timescale="90000" initialization="video_init.mp4" media="video$Number$.mp4" duration="900000">
		<SegmentTimeline>
		<S t="0" d="900000"/>
		</SegmentTimeline>
	</SegmentTemplate>
	<Representation id="1" bandwidth="3000000" codecs="avc1.4d401f" width="1280" height="720" frameRate="30"/>
	</AdaptationSet>
</Period>
</MPD>
)";
	InitializeAdMPDObject(adManifest);
	std::string fragmentUrl;
	AAMPStatusType status;
	// Initialize MPD. The video initialization segment is cached.
	fragmentUrl = std::string(TEST_BASE_URL) + std::string("dash/iframe_init.m4s");

	EXPECT_CALL(*g_mockMediaStreamContext, CacheFragment(fragmentUrl, _, _, _, _, true, _, _, _, _, _))
		.Times(1)
		.WillOnce(Return(true));
	status = InitializeMPD(mVodManifest, eTUNETYPE_NEW_NORMAL, 0.0, -1.0);
	EXPECT_EQ(status, eAAMPSTATUS_OK);
	auto cdaiObj = mStreamAbstractionAAMP_MPD->GetCDAIObject();
	cdaiObj->mAdState = AdState::OUTSIDE_ADBREAK;
	std::string adPeriodId = "p1"; // landing in p1
	std::string adUrl = TEST_AD_MANIFEST_URL;

	// Add ads to the adBreak
	cdaiObj->mAdBreaks = {
		{ adPeriodId, AdBreakObject(10000, std::make_shared<std::vector<AdNode>>(), adPeriodId, 5000, 5000) }
	};
	cdaiObj->mAdBreaks[adPeriodId].ads->emplace_back(false /*invalid*/, true /*placed*/, true /*resolved*/,
		"adId1" /*adId*/, adUrl /*url*/, 5000 /*duration*/, adPeriodId /*basePeriodId*/, 0 /*basePeriodOffset*/, mAdMPD /*mpd*/);
	cdaiObj->mPeriodMap[adPeriodId] = Period2AdData(false, adPeriodId, 30000 /*in ms*/,
	{
		std::make_pair (0, AdOnPeriod(0,0)), // for adId1 idx=0, offset=0s
	});

	mStreamAbstractionAAMP_MPD->SetBasePeriodoffset(30);
	mStreamAbstractionAAMP_MPD->SetBasePeriodId(adPeriodId);
	EXPECT_CALL(*g_mockPrivateInstanceAAMP, GetTSBSessionManager()).WillRepeatedly(Return(nullptr));
	EXPECT_CALL(*g_mockPrivateInstanceAAMP, IsLocalAAMPTsbInjection()).WillRepeatedly(Return(false));
	EXPECT_CALL(*g_mockPrivateInstanceAAMP, SendAdPlacementEvent(_, _, _, _, _, _, _, _)).Times(0);
	EXPECT_TRUE(mStreamAbstractionAAMP_MPD->CallOnAdEvent(AdEvent::DEFAULT));
	EXPECT_EQ(cdaiObj->mAdState, AdState::IN_ADBREAK_AD_NOT_PLAYING);

	mStreamAbstractionAAMP_MPD->SetBasePeriodoffset(5);

	EXPECT_TRUE(mStreamAbstractionAAMP_MPD->CallOnAdEvent(AdEvent::BASE_OFFSET_CHANGE));
	EXPECT_EQ( cdaiObj->mAdState, AdState::IN_ADBREAK_AD_PLAYING);

	EXPECT_TRUE(mStreamAbstractionAAMP_MPD->CallOnAdEvent(AdEvent::AD_FINISHED));
	EXPECT_EQ( cdaiObj->mAdState, AdState::IN_ADBREAK_WAIT2CATCHUP);

	cdaiObj->mCurPlayingBreakId = adPeriodId;
	EXPECT_TRUE(mStreamAbstractionAAMP_MPD->CallOnAdEvent(AdEvent::DEFAULT));
	EXPECT_EQ(cdaiObj->mAdState, AdState::OUTSIDE_ADBREAK);
}

/**
 * @brief onAdEventTest tests.
 * The tests verify the onAdEvent method of StreamAbstractionAAMP_MPD where p1 is the period where ad is present.At first
 * state is OUTSIDE_ADBREAK.  After rewind, position reaches to adbreak in Period p1 . In that adbreak, ad is valid 
 * and it gets played and moving to state IN_ADBREAK_AD_PLAYING. Once ad finishes state moves to IN_ADBREAK_WAIT2CATCHUP.
 * Since its an adbreak with one ad, and when ad finishes playing state moves to OUTSIDE_ADBREAK 
 */
TEST_F(AdSelectionTests, onAdEventTest_14)
{
	static const char *adManifest = R"(<?xml version="1.0" encoding="UTF-8"?>
<MPD xmlns="urn:mpeg:dash:schema:mpd:2011" type="static" profiles="urn:mpeg:dash:profile:isoff-on-demand:2011" minBufferTime="PT1.5S" mediaPresentationDuration="PT0M30S">
<Period id="ad1" start="PT0H0M0.000S">
	<AdaptationSet contentType="video" mimeType="video/mp4" segmentAlignment="true" startWithSAP="1">
	<SegmentTemplate timescale="90000" initialization="video_init.mp4" media="video$Number$.mp4" duration="900000">
		<SegmentTimeline>
		<S t="0" d="2700000"/>
		</SegmentTimeline>
	</SegmentTemplate>
	<Representation id="1" bandwidth="3000000" codecs="avc1.4d401f" width="1280" height="720" frameRate="30"/>
	</AdaptationSet>
</Period>
</MPD>
)";
	InitializeAdMPDObject(adManifest);
	std::string fragmentUrl;
	AAMPStatusType status;
	/* Initialize MPD. The video initialization segment is cached. */
	fragmentUrl = std::string(TEST_BASE_URL) + std::string("dash/iframe_init.m4s");
	EXPECT_CALL(*g_mockMediaStreamContext, CacheFragment(fragmentUrl, _, _, _, _, true, _, _, _, _, _))
		.Times(1)
		.WillOnce(Return(true));
	status = InitializeMPD(mVodManifest, eTUNETYPE_NEW_NORMAL, 0.0, -1.0);
	EXPECT_EQ(status, eAAMPSTATUS_OK);

	auto cdaiObj = mStreamAbstractionAAMP_MPD->GetCDAIObject();
	cdaiObj->mAdState = AdState::OUTSIDE_ADBREAK;
	std::string adPeriodId = "p1";
	std::string endPeriodId = "p2";
	std::string adUrl = TEST_AD_MANIFEST_URL;

	//Add ads to the adBreak
	cdaiObj->mAdBreaks = {
		{ adPeriodId, AdBreakObject(30000, std::make_shared<std::vector<AdNode>>(), endPeriodId, 0, 30000) }
	};
	// ad is currently placed
	cdaiObj->mAdBreaks[adPeriodId].ads->emplace_back(false /*invalid*/, true /*placed*/, true /*resolved*/,
		"adId1" /*adId*/, adUrl /*url*/, 30000 /*duration*/, adPeriodId /*basePeriodId*/, 0 /*basePeriodOffset*/, mAdMPD /*mpd*/);
	cdaiObj->mPeriodMap[adPeriodId] = Period2AdData(false, adPeriodId, 30000 /*in ms*/,
	{
		std::make_pair (0, AdOnPeriod(0, 0)),
	});
	mStreamAbstractionAAMP_MPD->SetBasePeriodId(adPeriodId);
	mStreamAbstractionAAMP_MPD->SetBasePeriodoffset(30);
	EXPECT_CALL(*g_mockPrivateInstanceAAMP, GetTSBSessionManager()).WillRepeatedly(Return(nullptr));
	EXPECT_CALL(*g_mockPrivateInstanceAAMP, IsLocalAAMPTsbInjection()).WillRepeatedly(Return(false));
	EXPECT_TRUE(mStreamAbstractionAAMP_MPD->CallOnAdEvent(AdEvent::DEFAULT));
	EXPECT_EQ( cdaiObj->mAdState, AdState::IN_ADBREAK_AD_PLAYING);

	EXPECT_TRUE(mStreamAbstractionAAMP_MPD->CallOnAdEvent(AdEvent::AD_FINISHED));
	EXPECT_EQ( cdaiObj->mAdState, AdState::IN_ADBREAK_WAIT2CATCHUP);

	cdaiObj->mCurPlayingBreakId = adPeriodId;
	EXPECT_TRUE(mStreamAbstractionAAMP_MPD->CallOnAdEvent(AdEvent::DEFAULT));
	EXPECT_EQ(cdaiObj->mAdState, AdState::OUTSIDE_ADBREAK);
	EXPECT_EQ(cdaiObj->mContentSeekOffset, 0); // Make sure content seek offset is set to adbreak offset
}

/**
 * @brief onAdEventTest tests.
 * The tests verify the onAdEvent method of StreamAbstractionAAMP_MPD where p1 is the period where ad is present.At first
 * state is OUTSIDE_ADBREAK.  After forward, position reaches to adbreak in Period p1 . In that adbreak, ad is valid
 * and it gets played and moving to state IN_ADBREAK_AD_PLAYING. Once ad finishes state moves to IN_ADBREAK_WAIT2CATCHUP.
 * Since its an adbreak with one ad, adbreak is placed and state moves to OUTSIDE_ADBREAK
 */
TEST_F(AdSelectionTests, onAdEventTest_15)
{
	static const char *adManifest = R"(<?xml version="1.0" encoding="UTF-8"?>
<MPD xmlns="urn:mpeg:dash:schema:mpd:2011" type="static" profiles="urn:mpeg:dash:profile:isoff-on-demand:2011" minBufferTime="PT1.5S" mediaPresentationDuration="PT0M5S">
<Period id="ad1" start="PT0H0M0.000S">
	<AdaptationSet contentType="video" mimeType="video/mp4" segmentAlignment="true" startWithSAP="1">
	<SegmentTemplate timescale="90000" initialization="video_init.mp4" media="video$Number$.mp4" duration="900000">
		<SegmentTimeline>
		<S t="0" d="450000"/>
		</SegmentTimeline>
	</SegmentTemplate>
	<Representation id="1" bandwidth="3000000" codecs="avc1.4d401f" width="1280" height="720" frameRate="30"/>
	</AdaptationSet>
</Period>
</MPD>
)";
	InitializeAdMPDObject(adManifest);
	std::string fragmentUrl;
	AAMPStatusType status;
	/* Initialize MPD. The video initialization segment is cached. */
	fragmentUrl = std::string(TEST_BASE_URL) + std::string("dash/iframe_init.m4s");
	EXPECT_CALL(*g_mockMediaStreamContext, CacheFragment(fragmentUrl, _, _, _, _, true, _, _, _, _, _))
		.Times(1)
		.WillOnce(Return(true));
	status = InitializeMPD(mVodManifest, eTUNETYPE_NEW_NORMAL, 0.0, 2);
	EXPECT_EQ(status, eAAMPSTATUS_OK);

	auto cdaiObj = mStreamAbstractionAAMP_MPD->GetCDAIObject();
	cdaiObj->mAdState = AdState::OUTSIDE_ADBREAK;
	std::string adPeriodId = "p1";
	std::string adUrl = TEST_AD_MANIFEST_URL;

	cdaiObj->mAdBreaks = {
		{ adPeriodId, AdBreakObject(5000, std::make_shared<std::vector<AdNode>>(), adPeriodId, 5000, 5000) }
	};
	// ad is currently placed
	// This test is for the scenario where ad is placed and we are transitioning to ad period
	// So, add the ad to the adBreak
	cdaiObj->mAdBreaks[adPeriodId].ads->emplace_back(false /*invalid*/, true /*placed*/, true /*resolved*/,
		"adId1" /*adId*/, adUrl /*url*/, 5000 /*duration*/, adPeriodId /*basePeriodId*/, 0 /*basePeriodOffset*/, mAdMPD /*mpd*/);
	cdaiObj->mPeriodMap[adPeriodId] = Period2AdData(false, adPeriodId, 30000 /*in ms*/,
	{
		std::make_pair (0, AdOnPeriod(0, 0)), // for adId1 idx=0, offset=0s
	});

	mStreamAbstractionAAMP_MPD->SetBasePeriodId(adPeriodId);
	EXPECT_CALL(*g_mockPrivateInstanceAAMP, GetTSBSessionManager()).WillRepeatedly(Return(nullptr));
	EXPECT_CALL(*g_mockPrivateInstanceAAMP, IsLocalAAMPTsbInjection()).WillRepeatedly(Return(false));
	EXPECT_TRUE(mStreamAbstractionAAMP_MPD->CallOnAdEvent(AdEvent::INIT));
	EXPECT_EQ( cdaiObj->mAdState, AdState::IN_ADBREAK_AD_PLAYING);

	EXPECT_TRUE(mStreamAbstractionAAMP_MPD->CallOnAdEvent(AdEvent::AD_FINISHED));
	EXPECT_EQ( cdaiObj->mAdState, AdState::IN_ADBREAK_WAIT2CATCHUP);

	cdaiObj->mCurPlayingBreakId = adPeriodId;
	cdaiObj->mAdBreaks[cdaiObj->mCurPlayingBreakId].mAdBreakPlaced = true;
	EXPECT_TRUE(mStreamAbstractionAAMP_MPD->CallOnAdEvent(AdEvent::DEFAULT));
	EXPECT_EQ(cdaiObj->mAdState, AdState::OUTSIDE_ADBREAK);
	EXPECT_EQ(cdaiObj->mContentSeekOffset, 5); // Make sure content seek offset is set to adbreak offset
}

/**
 * @brief AdTransitionTest tests.
 *
 * The tests verify the SelectSourceOrAdPeriod method of StreamAbstractionAAMP_MPD when transitioning
 * from a regular period to an ad period, and back to a regular period.
 */
TEST_F(AdSelectionTests, AdTransitionTest)
{
	static const char *adManifest = R"(<?xml version="1.0" encoding="UTF-8"?>
<MPD xmlns="urn:mpeg:dash:schema:mpd:2011" type="static" profiles="urn:mpeg:dash:profile:isoff-on-demand:2011" minBufferTime="PT1.5S" mediaPresentationDuration="PT0M15S">
<Period id="ad1" start="PT0H0M0.000S">
	<AdaptationSet contentType="video" mimeType="video/mp4" segmentAlignment="true" startWithSAP="1">
	<SegmentTemplate timescale="90000" initialization="video_init.mp4" media="video$Number$.mp4" duration="900000">
		<SegmentTimeline>
		<S t="0" d="1350000"/>
		<S t="1350000" d="1350000"/>
		<S t="2700000" d="1350000"/>
		<S t="4050000" d="1350000"/>
		</SegmentTimeline>
	</SegmentTemplate>
	<Representation id="1" bandwidth="3000000" codecs="avc1.4d401f" width="1280" height="720" frameRate="30"/>
	</AdaptationSet>
</Period>
</MPD>
)";
	InitializeAdMPDObject(adManifest);
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
	status = mStreamAbstractionAAMP_MPD->InvokeIndexNewMPDDocument(false);
	EXPECT_EQ(mStreamAbstractionAAMP_MPD->GetCurrentPeriodIdx(), 0);

	// Index the next period, wait for the selection
	// Set the ad variables, we have finished ad playback and waiting for base period to catchup
	auto cdaiObj = mStreamAbstractionAAMP_MPD->GetCDAIObject();
	cdaiObj->mAdState = AdState::OUTSIDE_ADBREAK;
	std::string periodId = "p1";
	std::string endPeriodId = "p2"; // landing in p1
	std::string adUrl = TEST_AD_MANIFEST_URL;
	// Add ads to the adBreak
	cdaiObj->mAdBreaks = {
		{periodId, AdBreakObject(30000, std::make_shared<std::vector<AdNode>>(), endPeriodId, 0, 30000)}};
	// ad is currently placed
	// This test is for the scenario where ad is placed and we are transitioning to ad period
	// So, add the ad to the adBreak
	cdaiObj->mAdBreaks[periodId].ads->emplace_back(false /*invalid*/, true /*placed*/, true /*resolved*/,
													  "adId1" /*adId*/, adUrl /*url*/, 30000 /*duration*/, periodId /*basePeriodId*/, 0 /*basePeriodOffset*/, mAdMPD /*mpd*/);

	// Marking adbreak as placed as well
	cdaiObj->mAdBreaks[periodId].mAdBreakPlaced = true;
	cdaiObj->mPeriodMap[periodId] = Period2AdData(false, periodId, 30000 /*in ms*/,
	{
	  std::make_pair (0, AdOnPeriod(0, 0)), // for adId1 idx=0, offset=0s
	});

	bool periodChanged = false;
	bool adStateChanged = false; // since we finished playing an ad
	bool waitForAdBreakCatchup = false;
	bool requireStreamSelection = false;
	bool mpdChanged = false;
	std::string currentPeriodId = "p0";
	mStreamAbstractionAAMP_MPD->IncrementIteratorPeriodIdx();

	EXPECT_CALL(*g_mockPrivateInstanceAAMP, GetTSBSessionManager()).WillRepeatedly(Return(nullptr));
	EXPECT_CALL(*g_mockPrivateInstanceAAMP, IsLocalAAMPTsbInjection()).WillRepeatedly(Return(false));
	EXPECT_CALL(*g_mockPrivateInstanceAAMP, SendAdReservationEvent(_, _, _, _, _)).Times(AnyNumber());
	EXPECT_CALL(*g_mockPrivateInstanceAAMP, SendAdPlacementEvent(_, _, _, _, _, _, _, _)).Times(AnyNumber());

	/*
	 * Test the scenario where ad is placed
	 */
	ret = mStreamAbstractionAAMP_MPD->InvokeSelectSourceOrAdPeriod(periodChanged, mpdChanged, adStateChanged, waitForAdBreakCatchup, requireStreamSelection, currentPeriodId);
	EXPECT_TRUE(ret);
	EXPECT_EQ(cdaiObj->mAdState, AdState::IN_ADBREAK_AD_PLAYING);

	// Reset function arguments
	periodChanged = false;
	// Finished ad playback in P0
	adStateChanged = true;
	cdaiObj->mAdState = AdState::IN_ADBREAK_WAIT2CATCHUP;
	waitForAdBreakCatchup = false;
	requireStreamSelection = false;
	mpdChanged = false;

	EXPECT_CALL(*g_mockPrivateInstanceAAMP, WaitForDiscontinuityProcessToComplete()).Times(1);

	ret = mStreamAbstractionAAMP_MPD->InvokeSelectSourceOrAdPeriod(periodChanged, mpdChanged, adStateChanged, waitForAdBreakCatchup, requireStreamSelection, currentPeriodId);
	EXPECT_EQ(mStreamAbstractionAAMP_MPD->GetCurrentPeriodIdx(), mStreamAbstractionAAMP_MPD->GetIteratorPeriodIdx());
	EXPECT_EQ(cdaiObj->mAdState, AdState::OUTSIDE_ADBREAK);
	EXPECT_EQ(currentPeriodId, "p2");
	EXPECT_EQ(periodChanged, true);
}

/**
 * @brief AdTransitionTest tests.
 *
 * The tests verify the SelectSourceOrAdPeriod method of StreamAbstractionAAMP_MPD when transitioning
 * from a regular period to an ad period, and back to a regular period.
 * Aamp TSB enabled, and paused
 */
TEST_F(AdSelectionTests, AdTransitionTest_PausedWithAampTSB)
{
	static const char *adManifest = R"(<?xml version="1.0" encoding="UTF-8"?>
<MPD xmlns="urn:mpeg:dash:schema:mpd:2011" type="static" profiles="urn:mpeg:dash:profile:isoff-on-demand:2011" minBufferTime="PT1.5S" mediaPresentationDuration="PT0M15S">
<Period id="ad1" start="PT0H0M0.000S">
	<AdaptationSet contentType="video" mimeType="video/mp4" segmentAlignment="true" startWithSAP="1">
	<SegmentTemplate timescale="90000" initialization="video_init.mp4" media="video$Number$.mp4" duration="900000">
		<SegmentTimeline>
		<S t="0" d="1350000"/>
		<S t="1350000" d="1350000"/>
		<S t="2700000" d="1350000"/>
		<S t="4050000" d="1350000"/>
		</SegmentTimeline>
	</SegmentTemplate>
	<Representation id="1" bandwidth="3000000" codecs="avc1.4d401f" width="1280" height="720" frameRate="30"/>
	</AdaptationSet>
</Period>
</MPD>
)";
	InitializeAdMPDObject(adManifest);
	std::string fragmentUrl;
	AAMPStatusType status;
	mPrivateInstanceAAMP->rate = 1.0;
	mPrivateInstanceAAMP->pipeline_paused = true;
	mPrivateInstanceAAMP->SetLocalAAMPTsb(true);

	bool ret = false;
	/* Initialize MPD. The video initialization segment is cached. */
	fragmentUrl = std::string(TEST_BASE_URL) + std::string("video_p0_init.mp4");


	EXPECT_CALL(*g_mockMediaStreamContext, CacheFragment(fragmentUrl, _, _, _, _, true, _, _, _, _, _))
		.Times(1)
		.WillOnce(Return(true));
	status = InitializeMPD(mLiveManifest, eTUNETYPE_SEEK, 10);
	EXPECT_EQ(status, eAAMPSTATUS_OK);

	// Index the initial values
	status = mStreamAbstractionAAMP_MPD->InvokeIndexNewMPDDocument(false);
	EXPECT_EQ(mStreamAbstractionAAMP_MPD->GetCurrentPeriodIdx(), 0);

	// Index the next period, wait for the selection
	// Set the ad variables, we have finished ad playback and waiting for base period to catchup
	auto cdaiObj = mStreamAbstractionAAMP_MPD->GetCDAIObject();
	cdaiObj->mAdState = AdState::OUTSIDE_ADBREAK;
	std::string periodId = "p1";
	std::string endPeriodId = "p2"; // landing in p1
	std::string adUrl = TEST_AD_MANIFEST_URL;
	// Add ads to the adBreak
	cdaiObj->mAdBreaks = {
		{periodId, AdBreakObject(30000, std::make_shared<std::vector<AdNode>>(), endPeriodId, 0, 30000)}};
	// ad is currently placed
	// This test is for the scenario where ad is placed and we are transitioning to ad period
	// So, add the ad to the adBreak
	cdaiObj->mAdBreaks[periodId].ads->emplace_back(false /*invalid*/, true /*placed*/, true /*resolved*/,
													  "adId1" /*adId*/, adUrl /*url*/, 30000 /*duration*/, periodId /*basePeriodId*/, 0 /*basePeriodOffset*/, mAdMPD /*mpd*/);

	// Marking adbreak as placed as well
	cdaiObj->mAdBreaks[periodId].mAdBreakPlaced = true;
	cdaiObj->mPeriodMap[periodId] = Period2AdData(false, periodId, 30000 /*in ms*/,
	{
	  std::make_pair (0, AdOnPeriod(0, 0)), // for adId1 idx=0, offset=0s
	});

	bool periodChanged = false;
	bool adStateChanged = false; // since we finished playing an ad
	bool waitForAdBreakCatchup = false;
	bool requireStreamSelection = false;
	bool mpdChanged = false;
	std::string currentPeriodId = "p0";
	mStreamAbstractionAAMP_MPD->IncrementIteratorPeriodIdx();

	EXPECT_CALL(*g_mockPrivateInstanceAAMP, GetTSBSessionManager()).WillRepeatedly(Return(nullptr));
	EXPECT_CALL(*g_mockPrivateInstanceAAMP, IsLocalAAMPTsbInjection()).WillRepeatedly(Return(false));
	EXPECT_CALL(*g_mockPrivateInstanceAAMP, SendAdReservationEvent(_, _, _, _, _)).Times(AnyNumber());
	EXPECT_CALL(*g_mockPrivateInstanceAAMP, SendAdPlacementEvent(_, _, _, _, _, _, _, _)).Times(AnyNumber());

	/*
	 * Test the scenario where ad is placed
	 */
	ret = mStreamAbstractionAAMP_MPD->InvokeSelectSourceOrAdPeriod(periodChanged, mpdChanged, adStateChanged, waitForAdBreakCatchup, requireStreamSelection, currentPeriodId);
	EXPECT_TRUE(ret);
	EXPECT_EQ(cdaiObj->mAdState, AdState::IN_ADBREAK_AD_PLAYING);

	// Reset function arguments
	periodChanged = false;
	// Finished ad playback in P0
	adStateChanged = true;
	cdaiObj->mAdState = AdState::IN_ADBREAK_WAIT2CATCHUP;
	waitForAdBreakCatchup = false;
	requireStreamSelection = false;
	mpdChanged = false;

	EXPECT_CALL(*g_mockPrivateInstanceAAMP, WaitForDiscontinuityProcessToComplete()).Times(0);

	ret = mStreamAbstractionAAMP_MPD->InvokeSelectSourceOrAdPeriod(periodChanged, mpdChanged, adStateChanged, waitForAdBreakCatchup, requireStreamSelection, currentPeriodId);
	EXPECT_EQ(mStreamAbstractionAAMP_MPD->GetCurrentPeriodIdx(), mStreamAbstractionAAMP_MPD->GetIteratorPeriodIdx());
	EXPECT_EQ(cdaiObj->mAdState, AdState::OUTSIDE_ADBREAK);
	EXPECT_EQ(currentPeriodId, "p2");
	EXPECT_EQ(periodChanged, true);
}

/**
 * @brief AdTransitionTest tests.
 *
 * The tests verify the SelectSourceOrAdPeriod method of StreamAbstractionAAMP_MPD when transitioning
 * from a ad period to a tiny period
 */
TEST_F(AdSelectionTests, PeriodChangeTestFromAdToTinyPeriod)
{
	static const char *adManifest =
R"(<?xml version="1.0" encoding="UTF-8"?>
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
	// No need of scte35, ad manager data structures are populated manually
	static const char *mManifest =
R"(<?xml version="1.0" encoding="utf-8"?>
<MPD xmlns="urn:mpeg:dash:schema:mpd:2011" availabilityStartTime="2023-01-01T00:00:00Z" maxSegmentDuration="PT2S" minBufferTime="PT4.000S" minimumUpdatePeriod="P100Y" profiles="urn:dvb:dash:profile:dvb-dash:2014,urn:dvb:dash:profile:dvb-dash:isoff-ext-live:2014" publishTime="2023-01-01T00:01:00Z" timeShiftBufferDepth="PT5M" type="dynamic">
	<Period id="p0" start="PT2S">
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
	<Period id="p1" start="PT32S">
		<AdaptationSet id="0" contentType="video">
			<Representation id="0" mimeType="video/mp4" codecs="avc1.640028" bandwidth="800000" width="640" height="360" frameRate="25">
				<SegmentTemplate timescale="2500" initialization="video_p0_init.mp4" media="video_p0_$Number$.m4s" startNumber="1" presentationTimeOffset="75000">
					<SegmentTimeline>
						<S t="75000" d="5000" r="14" />
					</SegmentTimeline>
				</SegmentTemplate>
			</Representation>
		</AdaptationSet>
		<AdaptationSet id="1" contentType="audio" lang="eng">
			<Representation id="0" mimeType="audio/mp4" codecs="ec-3" bandwidth="800000" width="640" height="360" frameRate="25">
				<SegmentTemplate timescale="2500" initialization="audio_p0_init.mp4" media="audio_p0_$Number$.m4s" startNumber="1" presentationTimeOffset="75000">
					<SegmentTimeline>
						<S t="75000" d="5000" r="14" />
					</SegmentTimeline>
				</SegmentTemplate>
			</Representation>
		</AdaptationSet>
	</Period>
	<Period id="p2" start="PT62S">
		<AdaptationSet id="0" contentType="video">
			<Representation id="0" mimeType="video/mp4" codecs="avc1.640028" bandwidth="800000" width="640" height="360" frameRate="25">
				<SegmentTemplate timescale="2500" initialization="video_p1_init.mp4" media="video_p1_$Number$.m4s" startNumber="16" presentationTimeOffset="150000">
					<SegmentTimeline>
						<S t="150000" d="625"/>
					</SegmentTimeline>
				</SegmentTemplate>
			</Representation>
		</AdaptationSet>
		<AdaptationSet id="1" contentType="audio" lang="eng">
			<Representation id="0" mimeType="audio/mp4" codecs="ec-3" bandwidth="800000" width="640" height="360" frameRate="25">
				<SegmentTemplate timescale="2500" initialization="audio_p1_init.mp4" media="audio_p1_$Number$.m4s" startNumber="16" presentationTimeOffset="150000">
					<SegmentTimeline>
						<S t="150000" d="625"/>
					</SegmentTimeline>
				</SegmentTemplate>
			</Representation>
		</AdaptationSet>
	</Period>
	<Period id="p3" start="PT62.250S">
		<AdaptationSet id="0" contentType="video">
			<Representation id="0" mimeType="video/mp4" codecs="avc1.640028" bandwidth="800000" width="640" height="360" frameRate="25">
				<SegmentTemplate timescale="2500" initialization="video_p1_init.mp4" media="video_p1_$Number$.m4s" startNumber="16" presentationTimeOffset="150625">
					<SegmentTimeline>
						<S t="150625" d="625"/>
					</SegmentTimeline>
				</SegmentTemplate>
			</Representation>
		</AdaptationSet>
		<AdaptationSet id="1" contentType="audio" lang="eng">
			<Representation id="0" mimeType="audio/mp4" codecs="ec-3" bandwidth="800000" width="640" height="360" frameRate="25">
				<SegmentTemplate timescale="2500" initialization="audio_p1_init.mp4" media="audio_p1_$Number$.m4s" startNumber="16" presentationTimeOffset="150625">
					<SegmentTimeline>
						<S t="150625" d="625"/>
					</SegmentTimeline>
				</SegmentTemplate>
			</Representation>
		</AdaptationSet>
	</Period>
	<Period id="p4" start="PT62.500S">
		<AdaptationSet id="0" contentType="video">
			<Representation id="0" mimeType="video/mp4" codecs="avc1.640028" bandwidth="800000" width="640" height="360" frameRate="25">
				<SegmentTemplate timescale="2500" initialization="video_p1_init.mp4" media="video_p1_$Number$.m4s" startNumber="16" presentationTimeOffset="151250">
					<SegmentTimeline>
						<S t="151250" d="5000" r="14" />
					</SegmentTimeline>
				</SegmentTemplate>
			</Representation>
		</AdaptationSet>
		<AdaptationSet id="1" contentType="audio" lang="eng">
			<Representation id="0" mimeType="audio/mp4" codecs="ec-3" bandwidth="800000" width="640" height="360" frameRate="25">
				<SegmentTemplate timescale="2500" initialization="audio_p1_init.mp4" media="audio_p1_$Number$.m4s" startNumber="16" presentationTimeOffset="151250">
					<SegmentTimeline>
						<S t="151250" d="5000" r="14" />
					</SegmentTimeline>
				</SegmentTemplate>
			</Representation>
		</AdaptationSet>
	</Period>
</MPD>
)";
	InitializeAdMPDObject(adManifest);
	std::string videoInitFragmentUrl;
	std::string audioInitFragmentUrl;
	AAMPStatusType status;
	mPrivateInstanceAAMP->rate = 1.0;
	bool ret = false;

	/* Initialize MPD. The video initialization segment is cached. */
	videoInitFragmentUrl = std::string(TEST_BASE_URL) + std::string("video_p0_init.mp4");
	audioInitFragmentUrl = std::string(TEST_BASE_URL) + std::string("audio_p0_init.mp4");

	EXPECT_CALL(*g_mockMediaStreamContext, CacheFragment(videoInitFragmentUrl, _, _, _, _, true, _, _, _, _, _))
		.Times(1)
		.WillOnce(Return(true));
	EXPECT_CALL(*g_mockMediaStreamContext, CacheFragment(audioInitFragmentUrl, _, _, _, _, true, _, _, _, _, _))
		.Times(1)
		.WillOnce(Return(true));

	EXPECT_CALL(*g_mockPrivateInstanceAAMP, GetTSBSessionManager()).WillRepeatedly(Return(nullptr));
	EXPECT_CALL(*g_mockPrivateInstanceAAMP, IsLocalAAMPTsbInjection()).WillRepeatedly(Return(false));
	EXPECT_CALL(*g_mockPrivateInstanceAAMP, SendAdReservationEvent(_, _, _, _, _)).Times(AnyNumber());
	EXPECT_CALL(*g_mockPrivateInstanceAAMP, SendAdPlacementEvent(_, _, _, _, _, _, _, _)).Times(AnyNumber());

	// Start the playback at P0
	status = InitializeMPD(mManifest, eTUNETYPE_SEEK, 10);
	EXPECT_EQ(status, eAAMPSTATUS_OK);

	// Set the ad variables
	auto mPrivateCDAIObject = mStreamAbstractionAAMP_MPD->GetCDAIObject();
	std::string periodId = "p1";
	std::string endPeriodId = "p2"; // landing in p1
	std::string adUrl = TEST_AD_MANIFEST_URL;
	// Add ads to the adBreak
	mPrivateCDAIObject->mAdBreaks =
	{
		{periodId, AdBreakObject(30000, std::make_shared<std::vector<AdNode>>(), endPeriodId, 0, 30000)}
	};
	// Ad is marked as placed since we are not calling any explicit PlaceAds()
	mPrivateCDAIObject->mAdBreaks[periodId].ads->emplace_back(false, true, true, "adId1", adUrl, 30000, periodId, 0, mAdMPD);
	// Marking adbreak as placed as well
	mPrivateCDAIObject->mAdBreaks[periodId].mAdBreakPlaced = true;
	mPrivateCDAIObject->mPeriodMap[periodId] = Period2AdData(false, periodId, 30000 /*in ms*/,
    {
      std::make_pair (0, AdOnPeriod(0, 0)), // for adId1 idx=0, offset=0s
    });

	// Index the initial values
	status = mStreamAbstractionAAMP_MPD->InvokeIndexNewMPDDocument(false);
	EXPECT_EQ(mStreamAbstractionAAMP_MPD->GetCurrentPeriodIdx(), 0);

	bool periodChanged = false;
	bool adStateChanged = false;
	bool waitForAdBreakCatchup = false;
	bool requireStreamSelection = false;
	bool mpdChanged = false;
	std::string currentPeriodId = "p0";
	mStreamAbstractionAAMP_MPD->IncrementIteratorPeriodIdx();

	// Move to P1
	ret = mStreamAbstractionAAMP_MPD->InvokeSelectSourceOrAdPeriod(periodChanged, mpdChanged, adStateChanged, waitForAdBreakCatchup, requireStreamSelection, currentPeriodId);
	EXPECT_TRUE(ret);
	// Moved into CDAI ad in P1
	EXPECT_EQ(currentPeriodId, "ad1");
	EXPECT_EQ(periodChanged, true);
	EXPECT_EQ(mPrivateCDAIObject->mAdState, AdState::IN_ADBREAK_AD_PLAYING);

	// Reset function arguments
	periodChanged = false;
	// Finished ad playback in P1
	adStateChanged = true;
	mPrivateCDAIObject->mAdState = AdState::IN_ADBREAK_WAIT2CATCHUP;
	waitForAdBreakCatchup = false;
	requireStreamSelection = false;
	mpdChanged = false;

	EXPECT_CALL(*g_mockPrivateInstanceAAMP, WaitForDiscontinuityProcessToComplete()).Times(1);

	ret = mStreamAbstractionAAMP_MPD->InvokeSelectSourceOrAdPeriod(periodChanged, mpdChanged, adStateChanged, waitForAdBreakCatchup, requireStreamSelection, currentPeriodId);
	EXPECT_EQ(mStreamAbstractionAAMP_MPD->GetCurrentPeriodIdx(), mStreamAbstractionAAMP_MPD->GetIteratorPeriodIdx());
	EXPECT_EQ(mPrivateCDAIObject->mAdState, AdState::OUTSIDE_ADBREAK);
	EXPECT_EQ(currentPeriodId, "p4");
	EXPECT_EQ(periodChanged, true);
}
