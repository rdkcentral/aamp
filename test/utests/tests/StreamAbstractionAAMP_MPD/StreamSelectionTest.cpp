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

/**
 * @brief StreamSelectionTestParams define the varying parameter is different test cases.
 */
struct StreamSelectionTestParams {
	int configProfileCount; /** Current profile count */
	int position; /** Playback start position Ad or content */
	int numTracks; /** Number tracks const now but can extend test in future */
	const char *manifestUsed; /** Manifest to be used  */
	AdState currAdState; /** Current state of Ad playback */
	int expectedTrack; /** This is the track selected */
};

static constexpr const char *mVodManifestSame = R"(<?xml version="1.0" encoding="utf-8"?>
<MPD xmlns="urn:mpeg:dash:schema:mpd:2011" availabilityStartTime="2023-01-01T00:00:00Z" maxSegmentDuration="PT2S" minBufferTime="PT4.000S" minimumUpdatePeriod="P100Y" profiles="urn:dvb:dash:profile:dvb-dash:2014,urn:dvb:dash:profile:dvb-dash:isoff-ext-live:2014" publishTime="2023-01-01T00:01:00Z" timeShiftBufferDepth="PT5M" type="static">
	<Period id="p0" start="PT0S">
		<AdaptationSet id="0" contentType="video" mimeType="video/mp4" segmentAlignment="true" startWithSAP="1">
			<SegmentTemplate timescale="2500" initialization="video_p0_init.mp4" media="video_p0_$Number$.m4s" startNumber="1">
				<SegmentTimeline>
					<S t="0" d="5000" r="14" />
				</SegmentTimeline>
			</SegmentTemplate>
			<Representation id="root_video4" bandwidth="562800" codecs="hvc1.1.6.L63.90" width="640" height="360" frameRate="25000/1000"/>
			<Representation id="root_video3" bandwidth="1328400" codecs="hvc1.1.6.L93.90" width="960" height="540" frameRate="50000/1000"/>
			<Representation id="root_video2" bandwidth="1996000" codecs="hvc1.1.6.L93.90" width="960" height="540" frameRate="50000/1000"/>
			<Representation id="root_video1" bandwidth="4461200" codecs="hvc1.1.6.L120.90" width="1280" height="720" frameRate="50000/1000"/>
			<Representation id="root_video0" bandwidth="6052400" codecs="hvc1.1.6.L123.90" width="1920" height="1080" frameRate="50000/1000"/>
		</AdaptationSet>
	</Period>
	<Period id="p1" start="PT30S">
		<AdaptationSet id="1" contentType="video">
			<SegmentTemplate timescale="2500" initialization="video_p1_init.mp4" media="video_p1_$Number$.m4s" startNumber="1">
				<SegmentTimeline>
					<S t="0" d="5000" r="14" />
				</SegmentTimeline>
			</SegmentTemplate>
			<Representation id="LE1" bandwidth="512275" codecs="hvc1.1.6.L93.b0" width="640" height="360" frameRate="25">
			</Representation>
			<Representation id="LE2" bandwidth="1179434" codecs="hvc1.1.6.L93.b0" width="960" height="540" frameRate="50">
			</Representation>
			<Representation id="LE3" bandwidth="1754934" codecs="hvc1.1.6.L93.b0" width="960" height="540" frameRate="50">
			</Representation>
			<Representation id="LE4" bandwidth="4211409" codecs="hvc1.1.6.L123.b0" width="1280" height="720" frameRate="50">
			</Representation>
			<Representation id="LE5" bandwidth="5811113" codecs="hvc1.1.6.L123.b0" width="1920" height="1080" frameRate="50">
			</Representation>
		</AdaptationSet>
	</Period>
</MPD>)";

static constexpr const char *mLiveManifestSame = R"(<?xml version="1.0" encoding="utf-8"?>
<MPD xmlns="urn:mpeg:dash:schema:mpd:2011" availabilityStartTime="2023-01-01T00:00:00Z" maxSegmentDuration="PT2S" minBufferTime="PT4.000S" minimumUpdatePeriod="P100Y" profiles="urn:dvb:dash:profile:dvb-dash:2014,urn:dvb:dash:profile:dvb-dash:isoff-ext-live:2014" publishTime="2023-01-01T00:01:00Z" timeShiftBufferDepth="PT5M" type="dynamic">
	<Period id="p0" start="PT0S">
		<AdaptationSet id="0" contentType="video" mimeType="video/mp4" segmentAlignment="true" startWithSAP="1">
			<SegmentTemplate timescale="2500" initialization="video_p0_init.mp4" media="video_p0_$Number$.m4s" startNumber="1">
				<SegmentTimeline>
					<S t="0" d="5000" r="14" />
				</SegmentTimeline>
			</SegmentTemplate>
			<Representation id="root_video4" bandwidth="562800" codecs="hvc1.1.6.L63.90" width="640" height="360" frameRate="25000/1000"/>
			<Representation id="root_video3" bandwidth="1328400" codecs="hvc1.1.6.L93.90" width="960" height="540" frameRate="50000/1000"/>
			<Representation id="root_video2" bandwidth="1996000" codecs="hvc1.1.6.L93.90" width="960" height="540" frameRate="50000/1000"/>
			<Representation id="root_video1" bandwidth="4461200" codecs="hvc1.1.6.L120.90" width="1280" height="720" frameRate="50000/1000"/>
			<Representation id="root_video0" bandwidth="6052400" codecs="hvc1.1.6.L123.90" width="1920" height="1080" frameRate="50000/1000"/>
		</AdaptationSet>
	</Period>
	<Period id="p1" start="PT30S">
		<AdaptationSet id="1" contentType="video">
			<SegmentTemplate timescale="2500" initialization="video_p1_init.mp4" media="video_p1_$Number$.m4s" startNumber="1">
				<SegmentTimeline>
					<S t="0" d="5000" r="14" />
				</SegmentTimeline>
			</SegmentTemplate>
			<Representation id="LE1" bandwidth="512275" codecs="hvc1.1.6.L93.b0" width="640" height="360" frameRate="25">
			</Representation>
			<Representation id="LE2" bandwidth="1179434" codecs="hvc1.1.6.L93.b0" width="960" height="540" frameRate="50">
			</Representation>
			<Representation id="LE3" bandwidth="1754934" codecs="hvc1.1.6.L93.b0" width="960" height="540" frameRate="50">
			</Representation>
			<Representation id="LE4" bandwidth="4211409" codecs="hvc1.1.6.L123.b0" width="1280" height="720" frameRate="50">
			</Representation>
			<Representation id="LE5" bandwidth="5811113" codecs="hvc1.1.6.L123.b0" width="1920" height="1080" frameRate="50">
			</Representation>
		</AdaptationSet>
	</Period>
</MPD>)";

static constexpr const char *mVodManifestNotSame = R"(<?xml version="1.0" encoding="utf-8"?>
	<MPD xmlns="urn:mpeg:dash:schema:mpd:2011" availabilityStartTime="2023-01-01T00:00:00Z" maxSegmentDuration="PT2S" minBufferTime="PT4.000S" minimumUpdatePeriod="P100Y" profiles="urn:dvb:dash:profile:dvb-dash:2014,urn:dvb:dash:profile:dvb-dash:isoff-ext-live:2014" publishTime="2023-01-01T00:01:00Z" timeShiftBufferDepth="PT5M" type="static">
	<Period id="p0" start="PT0S">
		<AdaptationSet id="0" contentType="video" mimeType="video/mp4" segmentAlignment="true" startWithSAP="1">
			<SegmentTemplate timescale="2500" initialization="video_p0_init.mp4" media="video_p0_$Number$.m4s" startNumber="1">
				<SegmentTimeline>
					<S t="0" d="5000" r="14" />
				</SegmentTimeline>
			</SegmentTemplate>
			<Representation id="root_video4" bandwidth="562800" codecs="hvc1.1.6.L63.90" width="640" height="360" frameRate="25000/1000"/>
			<Representation id="root_video3" bandwidth="1328400" codecs="hvc1.1.6.L93.90" width="960" height="540" frameRate="50000/1000"/>
			<Representation id="root_video2" bandwidth="1996000" codecs="hvc1.1.6.L93.90" width="960" height="540" frameRate="50000/1000"/>
			<Representation id="root_video1" bandwidth="4461200" codecs="hvc1.1.6.L120.90" width="1280" height="720" frameRate="50000/1000"/>
			<Representation id="root_video0" bandwidth="6052400" codecs="hvc1.1.6.L123.90" width="1920" height="1080" frameRate="50000/1000"/>
		</AdaptationSet>
	</Period>
	<Period id="p1" start="PT30S">
		<AdaptationSet id="1" contentType="video">
			<SegmentTemplate timescale="2500" initialization="video_p1_init.mp4" media="video_p1_$Number$.m4s" startNumber="1">
				<SegmentTimeline>
					<S t="0" d="5000" r="14" />
				</SegmentTimeline>
			</SegmentTemplate>
			<Representation id="LE1" bandwidth="512275" codecs="hvc1.1.6.L93.b0" width="640" height="360" frameRate="25">
			</Representation>
			<Representation id="LE2" bandwidth="1179434" codecs="hvc1.1.6.L93.b0" width="960" height="540" frameRate="50">
			</Representation>
			<Representation id="LE3" bandwidth="1754934" codecs="hvc1.1.6.L93.b0" width="960" height="540" frameRate="50">
			</Representation>
			<Representation id="LE4" bandwidth="4211409" codecs="hvc1.1.6.L123.b0" width="1280" height="720" frameRate="50">
			</Representation>
		</AdaptationSet>
	</Period>
</MPD>)";

static constexpr const char *mLiveManifestNotSame = R"(<?xml version="1.0" encoding="utf-8"?>
<MPD xmlns="urn:mpeg:dash:schema:mpd:2011" availabilityStartTime="2023-01-01T00:00:00Z" maxSegmentDuration="PT2S" minBufferTime="PT4.000S" minimumUpdatePeriod="P100Y" profiles="urn:dvb:dash:profile:dvb-dash:2014,urn:dvb:dash:profile:dvb-dash:isoff-ext-live:2014" publishTime="2023-01-01T00:01:00Z" timeShiftBufferDepth="PT5M" type="dynamic">
	<Period id="p0" start="PT0S">
		<AdaptationSet id="0" contentType="video" mimeType="video/mp4" segmentAlignment="true" startWithSAP="1">
			<SegmentTemplate timescale="2500" initialization="video_p0_init.mp4" media="video_p0_$Number$.m4s" startNumber="1">
				<SegmentTimeline>
					<S t="0" d="5000" r="14" />
				</SegmentTimeline>
			</SegmentTemplate>
			<Representation id="root_video4" bandwidth="562800" codecs="hvc1.1.6.L63.90" width="640" height="360" frameRate="25000/1000"/>
			<Representation id="root_video3" bandwidth="1328400" codecs="hvc1.1.6.L93.90" width="960" height="540" frameRate="50000/1000"/>
			<Representation id="root_video2" bandwidth="1996000" codecs="hvc1.1.6.L93.90" width="960" height="540" frameRate="50000/1000"/>
			<Representation id="root_video1" bandwidth="4461200" codecs="hvc1.1.6.L120.90" width="1280" height="720" frameRate="50000/1000"/>
			<Representation id="root_video0" bandwidth="6052400" codecs="hvc1.1.6.L123.90" width="1920" height="1080" frameRate="50000/1000"/>
		</AdaptationSet>
	</Period>
	<Period id="p1" start="PT30S">
		<AdaptationSet id="1" contentType="video">
			<SegmentTemplate timescale="2500" initialization="video_p1_init.mp4" media="video_p1_$Number$.m4s" startNumber="1">
				<SegmentTimeline>
					<S t="0" d="5000" r="14" />
				</SegmentTimeline>
			</SegmentTemplate>
			<Representation id="LE1" bandwidth="512275" codecs="hvc1.1.6.L93.b0" width="640" height="360" frameRate="25">
			</Representation>
			<Representation id="LE2" bandwidth="1179434" codecs="hvc1.1.6.L93.b0" width="960" height="540" frameRate="50">
			</Representation>
			<Representation id="LE3" bandwidth="1754934" codecs="hvc1.1.6.L93.b0" width="960" height="540" frameRate="50">
			</Representation>
			<Representation id="LE4" bandwidth="4211409" codecs="hvc1.1.6.L123.b0" width="1280" height="720" frameRate="50">
			</Representation>
		</AdaptationSet>
	</Period>
</MPD>)";

/**
 * @brief LinearTests tests common base class.
 */
class StreamSelectionTests : public testing::TestWithParam<StreamSelectionTestParams>
{
protected:
	class TestableStreamAbstractionAAMP_MPD : public StreamAbstractionAAMP_MPD
	{
	public:
		int mProfileCount;
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

		void InvokeStreamSelection()
		{
			StreamSelection();
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

		int GetProfileCount()
		{
			return mProfileCount; 
		}

		class MediaStreamContext *GetMediaStreamContext(AampMediaType type)
		{
			return mMediaStreamContext[type];
		}
	};

	PrivateInstanceAAMP *mPrivateInstanceAAMP;
	TestableStreamAbstractionAAMP_MPD *mStreamAbstractionAAMP_MPD;
	CDAIObject *mCdaiObj;
	const char *mManifest;
	static constexpr const char *TEST_BASE_URL = "http://host/asset/";
	static constexpr const char *TEST_MANIFEST_URL = "http://host/asset/manifest.mpd";
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
		mPrivateInstanceAAMP->mIsDefaultOffset = true;
		g_mockPrivateInstanceAAMP = new StrictMock<MockPrivateInstanceAAMP>();
		g_mockMediaStreamContext = new StrictMock<MockMediaStreamContext>();
		g_mockAampMPDDownloader = new StrictMock<MockAampMPDDownloader>();
		mStreamAbstractionAAMP_MPD = nullptr;
		mManifest = nullptr;
		mBoolConfigSettings = mDefaultBoolConfigSettings;
		mIntConfigSettings = mDefaultIntConfigSettings;
		mCdaiObj = nullptr;
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

		delete g_mockPrivateInstanceAAMP;
		g_mockPrivateInstanceAAMP = nullptr;

		delete g_mockMediaStreamContext;
		g_mockMediaStreamContext = nullptr;

		delete g_mockAampMPDDownloader;
		g_mockAampMPDDownloader = nullptr;

		mManifest = nullptr;
	}

public:
	void GetMPDFromManifest(ManifestDownloadResponsePtr response)
	{
		std::string manifestStr = std::string(
											  response->mMPDDownloadResponse->mDownloadData.begin(),
											  response->mMPDDownloadResponse->mDownloadData.end());
		size_t len = manifestStr.length();
		xmlTextReaderPtr reader = xmlReaderForMemory( manifestStr.c_str(), (int)len, NULL, NULL, 0 );
		if (reader != NULL)
		{
			if (xmlTextReaderRead(reader))
			{
				response->mRootNode = MPDProcessNode(&reader, TEST_MANIFEST_URL);
				if( response->mRootNode )
				{
					dash::mpd::MPD *mpd = response->mRootNode->ToMPD();
					if( mpd )
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
		ManifestDownloadResponsePtr response = MakeSharedManifestDownloadResponsePtr();
		response->mMPDStatus = AAMPStatusType::eAAMPSTATUS_OK;
		response->mMPDDownloadResponse->iHttpRetValue = 200;
		response->mMPDDownloadResponse->sEffectiveUrl = std::string(TEST_MANIFEST_URL);
		size_t len = strlen(mManifest);
		response->mMPDDownloadResponse->mDownloadData.assign( (uint8_t *)mManifest, (uint8_t *)&mManifest[len] );
		GetMPDFromManifest(response);
		return response;
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
		EXPECT_CALL(*g_mockPrivateInstanceAAMP, SetState(eSTATE_PREPARING))
			.Times(AnyNumber());

		EXPECT_CALL(*g_mockPrivateInstanceAAMP, GetState())
			.Times(AnyNumber())
			.WillRepeatedly(Return(eSTATE_PREPARING));
		EXPECT_CALL(*g_mockPrivateInstanceAAMP, GetLLDashChunkMode()).WillRepeatedly(Return(false));
		EXPECT_CALL(*g_mockPrivateInstanceAAMP, SetLLDashChunkMode(_));
		// For the time being return the same manifest again
		EXPECT_CALL(*g_mockAampMPDDownloader, GetManifest(_, _, _))
			.WillRepeatedly(WithoutArgs(Invoke(this, &StreamSelectionTests::GetManifestForMPDDownloader)));
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
TEST_P(StreamSelectionTests, TestCorrectTrackSelection)
{
	const auto& params = GetParam(); /*Retrieve the parameter values */
	mPrivateInstanceAAMP->rate = AAMP_NORMAL_PLAY_RATE;
	EXPECT_CALL(*g_mockMediaStreamContext, CacheFragment(_, _, _, _, _, _, _, _, _, _, _))
		.Times(AnyNumber())
		.WillOnce(Return(true));
	AAMPStatusType status = InitializeMPD(params.manifestUsed, TuneType::eTUNETYPE_NEW_NORMAL, params.position);
	EXPECT_EQ(status, eAAMPSTATUS_OK);
	mStreamAbstractionAAMP_MPD->SetNumberOfTracks(params.numTracks); // only video now
	mStreamAbstractionAAMP_MPD->mProfileCount = params.configProfileCount; //  Current profile count
	auto cdaiObj = mStreamAbstractionAAMP_MPD->GetCDAIObject();
	cdaiObj->mAdState = params.currAdState; // Set AD state
	class MediaStreamContext *pMediaStreamContext = mStreamAbstractionAAMP_MPD->GetMediaStreamContext(eMEDIATYPE_VIDEO);
	pMediaStreamContext->representationIndex = 4; // Previous representation
	mStreamAbstractionAAMP_MPD->InvokeStreamSelection();
	EXPECT_EQ(pMediaStreamContext->representationIndex, params.expectedTrack); //what is new representation
}

INSTANTIATE_TEST_SUITE_P( TestCorrectTrackSelection, StreamSelectionTests,
	::testing::ValuesIn(std::vector<StreamSelectionTestParams>{
		/** Case 1 & 2: current number of profiles 5, start in second period (30+ pos),
		 * number of tracks only video now, live manifest used with representations are same
		 * AD state IN and OUT  
		 * Expected to same track from previous if outside and reset to -1 if inside AD
		 */
		{5, 32, 1, mLiveManifestSame, AdState::OUTSIDE_ADBREAK, 4},
		{5, 32, 1, mLiveManifestSame, AdState::IN_ADBREAK_AD_PLAYING, -1},

		/** Case 3 & 4: current number of profiles 5, start in second period (30+ pos),
		 * number of tracks only video now, VOD manifest used with representations are same
		 * AD state IN and OUT  
		 * Expected to same track from previous if outside and reset to -1 if inside AD
		 */
		{5, 32, 1, mVodManifestSame , AdState::OUTSIDE_ADBREAK, 4},
		{5, 32, 1, mVodManifestSame, AdState::IN_ADBREAK_AD_PLAYING, -1},

		/** Case 1 & 2: current number of profiles 5, start in second period (30+ pos),
		 * number of tracks only video now, live manifest used with representations are not same
		 * AD state IN and OUT  
		 * Expected to to reset to -1 in both cases
		 */
		{5, 32, 1, mLiveManifestNotSame, AdState::OUTSIDE_ADBREAK, -1},
		{5, 32, 1, mLiveManifestNotSame, AdState::IN_ADBREAK_AD_PLAYING, -1},

		/** Case 3 & 4: current number of profiles 5, start in second period (30+ pos),
		 * number of tracks only video now, VOD manifest used with representations are not sssame
		 * AD state IN and OUT  
		 * Expected to to reset to -1 in both cases
		 */
		{5, 32, 1, mVodManifestNotSame, AdState::OUTSIDE_ADBREAK, -1},
		{5, 32, 1, mVodManifestNotSame, AdState::IN_ADBREAK_AD_PLAYING, -1}
	})
);
