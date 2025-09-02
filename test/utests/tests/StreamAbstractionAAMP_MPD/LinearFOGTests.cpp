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
using ::testing::SetArgReferee;
using ::testing::Return;
using ::testing::StrictMock;
using ::testing::NiceMock;
using ::testing::WithArgs;
using ::testing::WithoutArgs;
using ::testing::AnyNumber;
using ::testing::Invoke;

extern AampConfig *gpGlobalConfig;

/**
 * @brief LinearTests tests common base class.
 */
class LinearFOGTests : public testing::TestWithParam<double>
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
		};

		PrivateInstanceAAMP *mPrivateInstanceAAMP;
		TestableStreamAbstractionAAMP_MPD *mStreamAbstractionAAMP_MPD;
		CDAIObject *mCdaiObj;
		const char *mManifest;
		static constexpr const char *TEST_BASE_URL = "http://host/asset/";
		static constexpr const char *TEST_MANIFEST_URL = "http://host/asset/manifest.mpd";
	ManifestDownloadResponsePtr mResponse =  MakeSharedManifestDownloadResponsePtr();
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
				{eAAMPConfig_MaxFragmentChunkCached, DEFAULT_CACHED_FRAGMENT_CHUNKS_PER_TRACK}
		};

		IntConfigSettings mIntConfigSettings;

		void SetUp()
		{
				if(gpGlobalConfig == nullptr)
				{
						gpGlobalConfig =  new AampConfig();
				}

				mPrivateInstanceAAMP = new PrivateInstanceAAMP(gpGlobalConfig);
				mPrivateInstanceAAMP->mIsDefaultOffset = true;

				g_mockAampConfig = new NiceMock<MockAampConfig>();

				g_mockAampUtils = nullptr;

				g_mockAampGstPlayer = new MockAAMPGstPlayer( mPrivateInstanceAAMP);

				mPrivateInstanceAAMP->mIsDefaultOffset = true;

				g_mockPrivateInstanceAAMP = new StrictMock<MockPrivateInstanceAAMP>();

				g_mockMediaStreamContext = new StrictMock<MockMediaStreamContext>();

				g_mockAampMPDDownloader = new StrictMock<MockAampMPDDownloader>();

				g_mockAampStreamSinkManager = new NiceMock<MockAampStreamSinkManager>();

				mStreamAbstractionAAMP_MPD = nullptr;

				mManifest = nullptr;
				mResponse = nullptr;
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

		void GetMPDFromManifest(ManifestDownloadResponsePtr response)
		{
				dash::mpd::MPD* mpd = nullptr;
				std::string manifestStr = std::string( response->mMPDDownloadResponse->mDownloadData.begin(), response->mMPDDownloadResponse->mDownloadData.end());

				xmlTextReaderPtr reader = xmlReaderForMemory( (char *)manifestStr.c_str(), (int) manifestStr.length(), NULL, NULL, 0);
				if (reader != NULL)
				{
						if (xmlTextReaderRead(reader))
						{
								response->mRootNode = MPDProcessNode(&reader, TEST_MANIFEST_URL);
								if(response->mRootNode != NULL)
								{
										mpd = response->mRootNode->ToMPD();
										if (mpd)
										{
												std::shared_ptr<dash::mpd::IMPD> tmp_ptr(mpd);
												response->mMPDInstance	=	tmp_ptr;
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
				response->mMPDDownloadResponse->mDownloadData.assign((uint8_t*)mManifest, (uint8_t*)(mManifest + strlen(mManifest)));
				GetMPDFromManifest(response);
				mResponse = response;
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
				for (const auto & b : mBoolConfigSettings)
				{
						EXPECT_CALL(*g_mockAampConfig, IsConfigSet(b.first))
								.Times(AnyNumber())
								.WillRepeatedly(Return(b.second));
				}

				for (const auto & i : mIntConfigSettings)
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

				EXPECT_CALL(*g_mockPrivateInstanceAAMP, GetState())
						.Times(AnyNumber())
						.WillRepeatedly(Return(eSTATE_PREPARING));

				EXPECT_CALL(*g_mockPrivateInstanceAAMP, GetLLDashChunkMode()).WillRepeatedly(Return(false));
				EXPECT_CALL(*g_mockPrivateInstanceAAMP, SetLLDashChunkMode(_));
				// For the time being return the same manifest again
				EXPECT_CALL(*g_mockAampMPDDownloader, GetManifest (_, _, _))
						.WillRepeatedly(WithoutArgs(Invoke(this, &LinearFOGTests::GetManifestForMPDDownloader)));
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
 * @brief ABR mode test.
 *
 * The DASH manifest has the fogtsb attribute set. Verify that Fog is selected
 * to manage ABR.
 */
TEST_P(LinearFOGTests, FogABRTest)
{
		std::string fragmentUrl;
		AAMPStatusType status;
		static const char *manifest =
R"(<?xml version="1.0" encoding="utf-8"?>
<MPD xmlns="urn:mpeg:dash:schema:mpd:2011" minBufferTime="PT2S" mediaPresentationDuration="PT0H10M54.00S" type="dynamic" profiles="urn:mpeg:dash:profile:isoff-live:2011,http://dashif.org/guidelines/dash264" fogtsb="true">
 <Period duration="PT1M0S">
  <AdaptationSet maxWidth="1920" maxHeight="1080" maxFrameRate="25" par="16:9">
   <Representation id="1" mimeType="video/mp4" codecs="avc1.640028" width="640" height="360" frameRate="25" sar="1:1" bandwidth="1000000">
		<SegmentTemplate timescale="2500" media="video_$Number$.mp4" initialization="video_init.mp4">
		 <SegmentTimeline>
		  <S d="5000" r="14" />
		  <S d="5000" r="14" />
		 </SegmentTimeline>
		</SegmentTemplate>
   </Representation>
  </AdaptationSet>
 </Period>
</MPD>
)";
		double seekPos = GetParam();
		int fragNum = (seekPos / 2) + 1;
		bool ret = false;
		/* Initialize MPD. The video initialization segment is cached. */
		fragmentUrl = std::string(TEST_BASE_URL) + std::string("video_init.mp4");
		EXPECT_CALL(*g_mockMediaStreamContext, CacheFragment(fragmentUrl, _, _, _, _, true, _, _, _, _, _))
				.WillOnce(Return(true));

		status = InitializeMPD(manifest, eTUNETYPE_NEW_SEEK, seekPos, 1.0, true);
		EXPECT_EQ(status, eAAMPSTATUS_OK);

		MediaTrack *track = this->mStreamAbstractionAAMP_MPD->GetMediaTrack(eTRACK_VIDEO);
		EXPECT_NE(track, nullptr);
		MediaStreamContext *pMediaStreamContext = static_cast<MediaStreamContext *>(track);

		/* Push the first video segment to present.
		 * The segment starts at time 40.0s and has a duration of 2.0s.
		 */
		fragmentUrl = std::string(TEST_BASE_URL) + std::string("video_") + std::to_string(fragNum) + std::string(".mp4");
		EXPECT_CALL(*g_mockMediaStreamContext, CacheFragment(fragmentUrl, _, seekPos, 2.0, _, false, _, _, _, _, _))
				.WillOnce([pMediaStreamContext]() {
						pMediaStreamContext->mDownloadedFragment.AppendBytes("0x0a", 2);
						return false;
				});

		ret = PushNextFragment(eTRACK_VIDEO);
		EXPECT_EQ(ret, false);

		// Invoke UpdateMPD to mimic a playlist refresh, it will internally call UpdateTrackInfo and reset all variables
		mStreamAbstractionAAMP_MPD->InvokeUpdateMPD(false);

		// The same fragment should be attempted again
		EXPECT_CALL(*g_mockMediaStreamContext, CacheFragment(fragmentUrl, _, seekPos, 2.0, _, false, _, _, _, _, _))
				.WillOnce([pMediaStreamContext]() {
						pMediaStreamContext->mDownloadedFragment.Free();
						return true;
				});

		// This seeks in the current playlist and reaches the lastSegmentTime
		ret = PushNextFragment(eTRACK_VIDEO);
		if (seekPos != 0)
		{
				// Downloads the segment this time
				ret = PushNextFragment(eTRACK_VIDEO);
		}
		EXPECT_EQ(ret, true);
}

// Run LinearFOGTests tests at various position values
INSTANTIATE_TEST_SUITE_P(TestLinearFOG,
						 LinearFOGTests,
						 testing::Values(40.0, 0.0, 30.0, 58.0));
