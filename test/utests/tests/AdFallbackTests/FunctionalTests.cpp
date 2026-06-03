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
#include <thread>
#include "priv_aamp.h"
#include "AampConfig.h"
#include "AampUtils.h"
#include "AampLogManager.h"
#include "admanager_mpd.h"
#include "MockPrivateInstanceAAMP.h"
#include "AampMPDUtils.h"
#include "fragmentcollector_mpd.h"
#include "MediaStreamContext.h"
#include "MockAampConfig.h"
#include "MockMediaStreamContext.h"
#include "MockAampMPDDownloader.h"
#include "MockAampLogManager.h"

#include "libdash/IMPD.h"
#include "libdash/INode.h"
#include "libdash/IDASHManager.h"
#include "libdash/IProducerReferenceTime.h"
#include "libdash/xml/Node.h"
#include "libdash/helpers/Time.h"
#include "libdash/xml/DOMParser.h"
#include <libxml/xmlreader.h>


using ::testing::_;
using ::testing::Return;
using ::testing::StrictMock;
using ::testing::WithArgs;
using ::testing::Invoke;
using ::testing::AnyNumber;
using ::testing::Invoke;
using ::testing::NiceMock;
using ::testing::SetArgReferee;
using ::testing::WithoutArgs;

using namespace dash::xml;
using namespace dash::mpd;

AampConfig *gpGlobalConfig{nullptr};

class AdFallbackTests : public ::testing::Test
{
	protected:
		class TestableStreamAbstractionAAMP_MPD : public StreamAbstractionAAMP_MPD
		{
		public:
			using StreamAbstractionAAMP_MPD::mCdaiObject;
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
			int GetCurrentPeriodIdx()
			{
				return mCurrentPeriodIdx;
			}

			int GetIteratorPeriodIdx()
			{
				return mIterPeriodIndex;
			}

			AAMPStatusType IndexNewMPDDocument(bool updateTrackInfo = false)
			{
				return StreamAbstractionAAMP_MPD::IndexNewMPDDocument(updateTrackInfo);
			}
		};
		PrivateInstanceAAMP *mPrivateInstanceAAMP;
		CDAIObjectMPD *mCdaiObj;
		TestableStreamAbstractionAAMP_MPD *mStreamAbstractionAAMP_MPD;
		const char* mManifest;
		const char* mAdManifest;
		static constexpr const char *TEST_MANIFEST_URL = "http://host/asset/manifest.mpd";
		static constexpr const char *TEST_BASE_URL = "http://host/asset/";
		static constexpr const char *TEST_AD_BASE_URL = "http://host/ad/";
		static constexpr const char *TEST_AD_MANIFEST_URL = "http://host/ad/manifest.mpd";

		ManifestDownloadResponsePtr mResponse = MakeSharedManifestDownloadResponsePtr();

		void SetUp()
		{
			if (gpGlobalConfig == nullptr)
			{
				gpGlobalConfig = new AampConfig();
			}

			mPrivateInstanceAAMP = new PrivateInstanceAAMP(gpGlobalConfig);
			mPrivateInstanceAAMP->mIsDefaultOffset = true;

			mCdaiObj = new CDAIObjectMPD(mPrivateInstanceAAMP);

			g_mockAampConfig = std::make_shared<NiceMock<MockAampConfig>>();

			mPrivateInstanceAAMP->mIsDefaultOffset = true;

			g_mockPrivateInstanceAAMP = std::make_shared<NiceMock<MockPrivateInstanceAAMP>>();

			g_mockMediaStreamContext = std::make_shared<StrictMock<MockMediaStreamContext>>();

			g_mockAampMPDDownloader = std::make_shared<StrictMock<MockAampMPDDownloader>>();

			mStreamAbstractionAAMP_MPD = nullptr;

			mManifest = nullptr;
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

			g_mockAampConfig.reset();

			g_mockPrivateInstanceAAMP.reset();

			g_mockMediaStreamContext.reset();

			g_mockAampMPDDownloader.reset();

			mManifest = nullptr;
			mAdManifest = nullptr;
			mResponse = nullptr;
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

	ManifestDownloadResponsePtr GetManifestForMPDDownloader()
		{
			if (!mResponse->mMPDInstance)
			{
				ManifestDownloadResponsePtr response = MakeSharedManifestDownloadResponsePtr();
				response->mMPDStatus = AAMPStatusType::eAAMPSTATUS_OK;
				response->mMPDDownloadResponse->iHttpRetValue = 200;
				response->mMPDDownloadResponse->sEffectiveUrl = std::string(TEST_MANIFEST_URL);
				response->mMPDDownloadResponse->mDownloadData.assign(mManifest, mManifest + strlen(mManifest));
				GetMPDFromManifest(response);
				mResponse = response;
			}
			return mResponse;
		}

		void InitializeMPD(const char *manifest, TuneType tuneType = TuneType::eTUNETYPE_NEW_NORMAL, double seekPos = 0.0, float rate = AAMP_NORMAL_PLAY_RATE, bool isLive = false)
		{
			mManifest = manifest;

			mPrivateInstanceAAMP->SetManifestUrl(TEST_MANIFEST_URL);

			EXPECT_CALL(*g_mockPrivateInstanceAAMP, GetState())
				.Times(AnyNumber())
				.WillRepeatedly(Return(eSTATE_PREPARING));
			// For the time being return the same manifest again
			EXPECT_CALL(*g_mockAampMPDDownloader, GetManifest(_, _, _))
				.WillRepeatedly(WithoutArgs(Invoke(this, &AdFallbackTests::GetManifestForMPDDownloader)));

			// PrivateInstanceAAMP and the StreamAbstraction object should have the same rate.
			mPrivateInstanceAAMP->rate = rate;
			// Create MPD instance.
			mStreamAbstractionAAMP_MPD = new TestableStreamAbstractionAAMP_MPD(mPrivateInstanceAAMP, seekPos, rate);
			if(!mCdaiObj)
			{
				mCdaiObj = new CDAIObjectMPD(mPrivateInstanceAAMP);
			}
			mStreamAbstractionAAMP_MPD->SetCDAIObject(mCdaiObj);
		}

		AAMPStatusType Init(TuneType tuneType)
		{
			return mStreamAbstractionAAMP_MPD->Init(tuneType);
		}

		bool GetManifest(std::string remoteUrl, std::vector<uint8_t> &buffer, std::string& effectiveUrl, int& httpError)
		{
			/* Setup fake buffer contents. */
			buffer.clear();
			buffer.assign(mAdManifest, mAdManifest + strlen(mAdManifest));
			effectiveUrl = remoteUrl;
			httpError = 200;

			return true;
		}

		void InitializeAdMPD(const char *manifest)
		{
			std::string adManifestUrl = TEST_AD_MANIFEST_URL;
			if (manifest)
			{
				mAdManifest = manifest;
				// remoteUrl, manifest, effectiveUrl
				EXPECT_CALL(*g_mockPrivateInstanceAAMP, GetFile (adManifestUrl, _, _, _, _, _, _, _, _, _, _, _, _, _))
					.WillOnce(WithArgs<0,2,3,4>(Invoke(this, &AdFallbackTests::GetManifest)));
			}
			else
			{
				EXPECT_CALL(*g_mockPrivateInstanceAAMP, GetFile (adManifestUrl, _, _, _, _, _, _, _, _, _, _, _, _, _))
					.WillOnce(Return(true));
			}
		}
};

TEST_F(AdFallbackTests, AdInitFailureTest)
{
	static const char *manifest = R"(<?xml version="1.0" encoding="UTF-8"?>
<MPD xmlns="urn:mpeg:dash:schema:mpd:2011" xmlns:scte35="urn:scte:scte35:2014:xml+bin" type="static" profiles="urn:mpeg:dash:profile:isoff-on-demand:2011" minBufferTime="PT1.5S" mediaPresentationDuration="PT2M0S">
  <!-- Period 1 with Ad Marker in the first 15 seconds -->
  <Period id="1" start="PT0H0M0.000S">
    <AdaptationSet contentType="video" mimeType="video/mp4" segmentAlignment="true" startWithSAP="1">
      <SegmentTemplate timescale="90000" initialization="video_p0_init.mp4" media="video$Number$.mp4" duration="900000">
        <SegmentTimeline>
          <!-- Ad Marker SCTE placed here for first 15 seconds -->
          <S t="0" d="1350000" scte35:signal="SCTE-35 AD_MARKER"/>
          <S t="1350000" d="1350000"/>
          <S t="2700000" d="1350000"/>
          <S t="4050000" d="1350000"/>
        </SegmentTimeline>
      </SegmentTemplate>
      <Representation id="1" bandwidth="3000000" codecs="avc1.4d401f" width="1280" height="720" frameRate="30"/>
    </AdaptationSet>
	<AdaptationSet contentType="audio" mimeType="audio/mp4" segmentAlignment="true" startWithSAP="1">
      <SegmentTemplate timescale="90000" initialization="audio_p0_init.mp4" media="audio$Number$.mp4" duration="900000">
        <SegmentTimeline>
          <S t="0" d="1350000"/>
          <S t="1350000" d="1350000"/>
          <S t="2700000" d="1350000"/>
          <S t="4050000" d="1350000"/>
        </SegmentTimeline>
      </SegmentTemplate>
      <Representation id="2" bandwidth="128000" codecs="mp4a.40.2" audioSamplingRate="48000"/>
    </AdaptationSet>
  </Period>

  <!-- Period 2 -  without Ad Marker -->
  <Period id="2" start="PT1M0S">
    <AdaptationSet contentType="video" mimeType="video/mp4" segmentAlignment="true" startWithSAP="1">
      <SegmentTemplate timescale="90000" initialization="video_p1_init.mp4" media="video$Number$.mp4" duration="900000">
        <SegmentTimeline>
          <S t="0" d="1350000"/>
          <S t="1350000" d="1350000"/>
          <S t="2700000" d="1350000"/>
          <S t="4050000" d="1350000"/>
        </SegmentTimeline>
      </SegmentTemplate>
      <Representation id="1" bandwidth="3000000" codecs="avc1.4d401f" width="1280" height="720" frameRate="30"/>
    </AdaptationSet>
	<AdaptationSet contentType="audio" mimeType="audio/mp4" segmentAlignment="true" startWithSAP="1">
      <SegmentTemplate timescale="90000" initialization="audio_p1_init.mp4" media="audio$Number$.mp4" duration="900000">
        <SegmentTimeline>
          <S t="0" d="1350000"/>
          <S t="1350000" d="1350000"/>
          <S t="2700000" d="1350000"/>
          <S t="4050000" d="1350000"/>
        </SegmentTimeline>
      </SegmentTemplate>
      <Representation id="2" bandwidth="128000" codecs="mp4a.40.2" audioSamplingRate="48000"/>
    </AdaptationSet>
  </Period>
</MPD>
)";

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
	<AdaptationSet contentType="audio" mimeType="audio/mp4" segmentAlignment="true" startWithSAP="1">
      <SegmentTemplate timescale="90000" initialization="audio_init.mp4" media="audio$Number$.mp4" duration="900000">
        <SegmentTimeline>
          <S t="0" d="1350000"/>
          <S t="1350000" d="1350000"/>
          <S t="2700000" d="1350000"/>
          <S t="4050000" d="1350000"/>
        </SegmentTimeline>
      </SegmentTemplate>
      <Representation id="2" bandwidth="128000" codecs="mp4a.40.2" audioSamplingRate="48000"/>
    </AdaptationSet>
  </Period>
</MPD>
)";

	std::string adInitFragmentUrl = std::string(TEST_AD_BASE_URL) + std::string("video_init.mp4");
	std::string sourceVideoInitFragmentUrl = std::string(TEST_BASE_URL) + std::string("video_p0_init.mp4");
	std::string sourceAudioInitFragmentUrl = std::string(TEST_BASE_URL) + std::string("audio_p0_init.mp4");
	AAMPStatusType status;

	//For this test case we need ptsrestamp - false and cdai - true
	EXPECT_CALL(*g_mockAampConfig, IsConfigSet(_))
		.WillRepeatedly(Invoke([](AAMPConfigSettingBool config) {
					return config == eAAMPConfig_EnableClientDai;
					}));

	std::string periodId = "1";
	std::string adId = "Ad1";
	std::string adurl = "";
	uint64_t startMS = 0;
	uint32_t breakdur = 15000;

	InitializeMPD(manifest);

	// Add ads to mPeriodMap
	mStreamAbstractionAAMP_MPD->mCdaiObject->mPeriodMap[periodId] = Period2AdData(false, periodId, breakdur /*in ms*/,
		{
			std::make_pair (0, AdOnPeriod(0, 0)), // for adId1 idx=0, offset=0s
		});

	EXPECT_CALL(*g_mockPrivateInstanceAAMP, DownloadsAreEnabled())
		.Times(AnyNumber())
		.WillRepeatedly(Return(true));

	// To create an empty ad break object, at init the adbreak objects are not created
	mStreamAbstractionAAMP_MPD->mCdaiObject->SetAlternateContents(periodId, adId, "", startMS, breakdur);

	adurl = TEST_AD_MANIFEST_URL;

	// Ad manifest
	InitializeAdMPD(adManifest);

	// Need to fail ad init fragment, This will be called from FetchAndCacheInitHeaders in admanager during fulfilling ad
	EXPECT_CALL(*g_mockPrivateInstanceAAMP, GetFile (adInitFragmentUrl, eMEDIATYPE_INIT_VIDEO, _, _, _, _, _, _, _, _, _, _, _, _))
              .WillOnce(Return(false));
	// Called again to populate mAdBreaks and other variables
	mStreamAbstractionAAMP_MPD->mCdaiObject->SetAlternateContents(periodId, adId, adurl, startMS, breakdur);
	std::this_thread::sleep_for(std::chrono::milliseconds(50));
	EXPECT_EQ(mStreamAbstractionAAMP_MPD->mCdaiObject->mAdBreaks[periodId].ads->size(), 1);
	EXPECT_EQ(mStreamAbstractionAAMP_MPD->mCdaiObject->mAdBreaks[periodId].ads->at(0).adId, adId);
	// After ad init failure, the ad break should not have mpd set
	EXPECT_EQ(mStreamAbstractionAAMP_MPD->mCdaiObject->mAdBreaks[periodId].ads->at(0).mpd, nullptr);

	EXPECT_CALL(*g_mockPrivateInstanceAAMP, DownloadsAreEnabled())
		.Times(AnyNumber())
		.WillRepeatedly([]()
			{
				static int counter = 0;
				return (++counter < 10);
			});

	EXPECT_CALL(*g_mockMediaStreamContext, CacheFragment(_, _, _, _, _, true, _, _, _))
		.Times(4) // 2 audio + 2 video init fragments
		.WillRepeatedly(Return(true));
	EXPECT_CALL(*g_mockMediaStreamContext, CacheFragment(_, _, _, _, _, false, _, _, _))
		.WillRepeatedly(Return(true)); // Media fragments

	TuneType tuneType = TuneType::eTUNETYPE_NEW_NORMAL;
	// Will start fetching the ad, but fails in ad init fragment and should fallback to source period and its init fragment
	status = Init(tuneType);
	EXPECT_EQ(status, eAAMPSTATUS_OK);
	EXPECT_EQ(mStreamAbstractionAAMP_MPD->mCdaiObject->mAdState, AdState::IN_ADBREAK_AD_NOT_PLAYING);
	mStreamAbstractionAAMP_MPD->InvokeFetcherLoop();
	// Gets updated in FetcherLoop
	EXPECT_EQ(mStreamAbstractionAAMP_MPD->mCdaiObject->mAdState, AdState::OUTSIDE_ADBREAK);
	EXPECT_DOUBLE_EQ(mStreamAbstractionAAMP_MPD->mPTSOffset.inSeconds(), 60.0);
}
