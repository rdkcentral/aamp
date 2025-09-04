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
using ::testing::An;
using ::testing::AnyNumber;
using ::testing::DoAll;
using ::testing::Invoke;
using ::testing::NiceMock;
using ::testing::Return;
using ::testing::SetArgReferee;
using ::testing::StrictMock;
using ::testing::WithArgs;
using ::testing::WithoutArgs;

/**
 * @brief Functional tests common base class.
 */
class SwitchAudioTrackTests : public ::testing::Test
{
protected:
	PrivateInstanceAAMP *mPrivateInstanceAAMP;
	StreamAbstractionAAMP_MPD *mStreamAbstractionAAMP_MPD;
	CDAIObject *mCdaiObj;
	const char *mManifest;
	static constexpr const char *TEST_BASE_URL = "http://host/asset/";
	static constexpr const char *TEST_MANIFEST_URL = "http://host/asset/manifest.mpd";
	ManifestDownloadResponsePtr mResponse = MakeSharedManifestDownloadResponsePtr();
	using BoolConfigSettings = std::map<AAMPConfigSettingBool, bool>;
	double offsetFromStart;

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
		{eAAMPConfig_DisableAC4, false},
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

	void SetUp() override
	{
		if (gpGlobalConfig == nullptr)
		{
			gpGlobalConfig = new AampConfig();
		}

		mPrivateInstanceAAMP = new PrivateInstanceAAMP(gpGlobalConfig);

		g_mockAampConfig = new NiceMock<MockAampConfig>();

		mPrivateInstanceAAMP->mIsDefaultOffset = true;

		g_mockPrivateInstanceAAMP = new StrictMock<MockPrivateInstanceAAMP>();

		g_mockMediaStreamContext = new StrictMock<MockMediaStreamContext>();

		g_mockAampMPDDownloader = new StrictMock<MockAampMPDDownloader>();

		mStreamAbstractionAAMP_MPD = nullptr;

		mManifest = nullptr;
		mResponse = nullptr;
		mBoolConfigSettings = mDefaultBoolConfigSettings;
		mCdaiObj = nullptr;
	}

	void TearDown() override
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
		ManifestDownloadResponsePtr response = MakeSharedManifestDownloadResponsePtr();
		response->mMPDStatus = AAMPStatusType::eAAMPSTATUS_OK;
		response->mMPDDownloadResponse->iHttpRetValue = 200;
		response->mMPDDownloadResponse->sEffectiveUrl = std::string(TEST_MANIFEST_URL);
		response->mMPDDownloadResponse->mDownloadData.assign((uint8_t *)mManifest, (uint8_t *)(mManifest + strlen(mManifest)));
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
	AAMPStatusType InitializeMPD(const char *manifest, TuneType tuneType = TuneType::eTUNETYPE_NEW_NORMAL, double seekPos = 0.0, float rate = AAMP_NORMAL_PLAY_RATE)
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

		/* Create MPD instance. */
		mStreamAbstractionAAMP_MPD = new StreamAbstractionAAMP_MPD(mPrivateInstanceAAMP, seekPos, rate);
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
		EXPECT_CALL(*g_mockAampMPDDownloader, GetManifest(_, _, _))
			.WillOnce(WithoutArgs(Invoke(this, &SwitchAudioTrackTests::GetManifestForMPDDownloader)));

		status = mStreamAbstractionAAMP_MPD->Init(tuneType);
		return status;
	}
};
/**
 * @brief Unit test for verifying the behavior whether the audio tracks were updated properly.
 *
 * This test initializes an MPD manifest and verifies that the audio track params were updated properly.
 *
 * Test Steps:
 * 1. Define an MPD manifest with a audio adaptation set.
 * 2. Expect a call to CacheFragment with specific parameters and return true.
 * 3. Initialize the MPD manifest.
 * 4. Verify that the initialization status is eAAMPSTATUS_OK.
 * 5. Initializing the MediaTrack(AUDIO), checking that it won't be NULL 
 * 6. Initializing the audio track info and checking the params to be properly updated from the manifest
 * 7. Calling the SelectAudioTrack() and checking whether the index values were updated properly
 * 8. Verifying the UpdateMediaTrack() info api for all the params got properly updated, and also checking 
 *    the status to get retrieved as eAAMPSTATUS_OK
 * 
 * Expected Results:
 * - All the Media Context information should be properly updated and also status to get updated as eAAMPSTATUS_OK.
 */
TEST_F(SwitchAudioTrackTests, UpdateMediaTrackInfoTests)
{
	std::string fragmentUrl;
	AAMPStatusType status;
	static const char *manifest =
R"(<?xml version="1.0" encoding="utf-8"?>
<MPD xmlns="urn:mpeg:dash:schema:mpd:2011" profiles="urn:mpeg:dash:profile:isoff-live:2011" type="static" mediaPresentationDuration="PT2M0.0S" minBufferTime="PT4.0S">
	<Period id="0" start="PT0.0S">
		<AdaptationSet id="3" contentType="audio">
			<Representation id="0" mimeType="audio/mp4" codecs="opus" bandwidth="64000" audioSamplingRate="48000">
				<SegmentTemplate timescale="48000" initialization="opus/audio_init.mp4" media="opus/audio_$Number$.mp3" startNumber="1">
					<SegmentTimeline>
						<S t="0" d="96000" r="59" />
					</SegmentTimeline>
				</SegmentTemplate>
			</Representation>
		</AdaptationSet>
	</Period>
</MPD>
)";
	EXPECT_CALL(*g_mockAampConfig, IsConfigSet(eAAMPConfig_DisableAC4))
		.WillRepeatedly(Return(true));

	fragmentUrl = std::string(TEST_BASE_URL) + std::string("opus/audio_init.mp4");
	EXPECT_CALL(*g_mockMediaStreamContext, CacheFragment(fragmentUrl, _, _, _, _, true, _, _, _, _, _))
		.WillOnce(Return(true));

	status = InitializeMPD(manifest);
	EXPECT_EQ(status, eAAMPSTATUS_OK);

	MediaTrack *track = this->mStreamAbstractionAAMP_MPD->GetMediaTrack(eTRACK_AUDIO);
	EXPECT_NE(track, nullptr);
	MediaStreamContext *pMediaStreamContext = static_cast<MediaStreamContext *>(track);
    
	/* Enabling the MediaStreamContext */
	pMediaStreamContext->enabled = TRUE;

	std::vector<AudioTrackInfo> aTracks;
	std::string aTrackIdx;
	int audioRepresentationIndex = -1;
	int audioAdaptationSetIndex = -1;
	std::vector<AudioTrackInfo> audioTracks = mStreamAbstractionAAMP_MPD->GetAvailableAudioTracks();

	EXPECT_EQ(audioTracks[0].codec, "opus");
	EXPECT_EQ(audioTracks[0].bandwidth, 64000);

	mStreamAbstractionAAMP_MPD->SelectAudioTrack(aTracks, aTrackIdx, audioRepresentationIndex, audioAdaptationSetIndex);
	EXPECT_EQ(audioRepresentationIndex, 0);
	EXPECT_EQ(audioAdaptationSetIndex, 0);

	status = mStreamAbstractionAAMP_MPD->UpdateMediaTrackInfo( eMEDIATYPE_AUDIO );
	EXPECT_EQ(pMediaStreamContext->fragmentTime, 0);
	EXPECT_EQ(pMediaStreamContext->fragmentDescriptor.Time, 0);
	EXPECT_EQ(pMediaStreamContext->fragmentDescriptor.Number, 1);
	EXPECT_EQ(pMediaStreamContext->adaptationSetId, 3);

	EXPECT_EQ(status, eAAMPSTATUS_OK);
}
/**
 * @brief Unit test for verifying the behavior whether the UpdateMediaTrackInfo() should properly
 * return error code when there is no segment timeline in the MPD.
 *
 * This test initializes an MPD manifest and verifies that the UpdateMediaTrackInfo() should properly return Errorcode.
 *
 * Test Steps:
 * 1. Define an MPD manifest with a audio adaptation set.
 * 2. Expect a call to CacheFragment with specific parameters and return true.
 * 3. Initialize the MPD manifest.
 * 4. Verify that the initialization status is eAAMPSTATUS_OK.
 * 5. Initializing the MediaTrack(AUDIO), checking that it won't be NULL 
 * 6. Initializing the audio track info and checking the params to be properly updated from the manifest
 * 7. Calling the SelectAudioTrack() and checking whether the index values were updated properly
 * 8. Verifying the UpdateMediaTrack() should return eAAMPSTATUS_MANIFEST_INVALID_TYPE
 * 
 * Expected Results:
 * -UpdateMediaTrack() need to return eAAMPSTATUS_MANIFEST_INVALID_TYPE
 */
TEST_F(SwitchAudioTrackTests, UpdateMediatrackTestHavingNoSegmentTimeline)
{
	std::string fragmentUrl;
	AAMPStatusType status;
	static const char *manifest =
R"(<?xml version="1.0" encoding="UTF-8"?><MPD xmlns="urn:mpeg:dash:schema:mpd:2011" xmlns:xsi="http://www.w3.org/2001/XMLSchema-instance" maxSubsegmentDuration="PT5.0S" mediaPresentationDuration="PT9M57S" minBufferTime="PT5.0S" profiles="urn:mpeg:dash:profile:isoff-on-demand:2011,http://xmlns.sony.net/metadata/mpeg/dash/profile/senvu/2012" type="static" xsi:schemaLocation="urn:mpeg:dash:schema:mpd:2011 DASH-MPD.xsd">
  <Period duration="PT9M57S" id="P1">
    <!-- Adaptation Set for main audio -->
    <AdaptationSet audioSamplingRate="48000" codecs="mp4a.40.5" contentType="audio" group="2" id="2" lang="en" mimeType="audio/mp4" subsegmentAlignment="true" subsegmentStartsWithSAP="1">
      <AudioChannelConfiguration schemeIdUri="urn:mpeg:dash:23003:3:audio_channel_configuration:2011" value="2"/>
      <Role schemeIdUri="urn:mpeg:dash:role:2011" value="main"/>
      <Representation bandwidth="64000" id="2_1">
        <BaseURL>DASH_vodaudio_Track5.m4a</BaseURL>
      </Representation>
    </AdaptationSet>
  </Period>
</MPD>
)";
	status = InitializeMPD(manifest);
	EXPECT_EQ(status, eAAMPSTATUS_OK);

	MediaTrack *track = this->mStreamAbstractionAAMP_MPD->GetMediaTrack(eTRACK_AUDIO);
	EXPECT_NE(track, nullptr);
	MediaStreamContext *pMediaStreamContext = static_cast<MediaStreamContext *>(track);
    
	/* Enabling the MediaStreamContext */
	pMediaStreamContext->enabled = TRUE;

	std::vector<AudioTrackInfo> aTracks;
	std::string aTrackIdx;
	int audioRepresentationIndex = -1;
	int audioAdaptationSetIndex = -1;
	std::vector<AudioTrackInfo> audioTracks = mStreamAbstractionAAMP_MPD->GetAvailableAudioTracks();

	EXPECT_EQ(audioTracks[0].codec, "mp4a.40.5");
	EXPECT_EQ(audioTracks[0].bandwidth, 64000);

	mStreamAbstractionAAMP_MPD->SelectAudioTrack(aTracks, aTrackIdx, audioRepresentationIndex, audioAdaptationSetIndex);
	EXPECT_EQ(audioRepresentationIndex, 0);
	EXPECT_EQ(audioAdaptationSetIndex, 0);

	status = mStreamAbstractionAAMP_MPD->UpdateMediaTrackInfo( eMEDIATYPE_AUDIO );
	EXPECT_EQ(status, eAAMPSTATUS_MANIFEST_INVALID_TYPE);

}
/**
 * @brief Unit test for verifying the behavior whether the offsetStart param is updated properly.
 *
 * This test initializes an MPD manifest and verifies that the offsetStart param should get updated properly
 *
 * Test Steps:
 * 1. Define an MPD manifest with a audio adaptation set.
 * 2. Expect a call to CacheFragment with specific parameters and return true.
 * 3. Initialize the MPD manifest.
 * 4. Verify that the initialization status is eAAMPSTATUS_OK.
 * 5. Initializing the offsetFromStart as 0.5, and verifying that it get properly updated while calling UpdateSeekPeriodOffset()
 * 
 * Expected Results:
 * -UpdateSeekPeriodOffset need to properly return the offsetFromStart value.
 */
TEST_F(SwitchAudioTrackTests, UpdateSeekPeriodOffsetTest)
{
	std::string fragmentUrl;
	AAMPStatusType status;
	static const char *manifest =
R"(<?xml version="1.0" encoding="utf-8"?>
<MPD xmlns="urn:mpeg:dash:schema:mpd:2011" profiles="urn:mpeg:dash:profile:isoff-live:2011" type="static" mediaPresentationDuration="PT2M0.0S" minBufferTime="PT4.0S">
	<Period id="0" start="PT0.0S">
		<AdaptationSet id="3" contentType="audio">
			<Representation id="0" mimeType="audio/mp4" codecs="opus" bandwidth="64000" audioSamplingRate="48000">
				<SegmentTemplate timescale="48000" initialization="opus/audio_init.mp4" media="opus/audio_$Number$.mp3" startNumber="1">
					<SegmentTimeline>
						<S t="0" d="96000" r="59" />
					</SegmentTimeline>
				</SegmentTemplate>
			</Representation>
		</AdaptationSet>
	</Period>
</MPD>
)";
	EXPECT_CALL(*g_mockAampConfig, IsConfigSet(eAAMPConfig_DisableAC4))
		.WillRepeatedly(Return(true));

	fragmentUrl = std::string(TEST_BASE_URL) + std::string("opus/audio_init.mp4");
	EXPECT_CALL(*g_mockMediaStreamContext, CacheFragment(fragmentUrl, _, _, _, _, true, _, _, _, _, _))
		.WillOnce(Return(true));

	status = InitializeMPD(manifest);
	EXPECT_EQ(status, eAAMPSTATUS_OK);

	offsetFromStart = 0.5;
	double initialOffset = offsetFromStart;
    mStreamAbstractionAAMP_MPD->UpdateSeekPeriodOffset(offsetFromStart);
    EXPECT_EQ(initialOffset, offsetFromStart);
}
/**
 * @brief Unit test for verifying whether MPD has no audio tracks present
 * 
 * This test initializes an MPD manifest and verifies that the test case works as expected
 *
 * Test Steps:
 * 1. Define an MPD manifest with no audiotrack params.
 * 2. Expect a call to CacheFragment with specific parameters and return true.
 * 3. Initialize the MPD manifest.
 * 4. Expected to call the SwitchAudioTrack()
 * 5. Need to check the aTrackIdx is none
 * 
 * Expected Results:
 * -Audio aTrackIdx should be NULL
 */
TEST_F(SwitchAudioTrackTests, NoAudioTracks)
{
	AAMPStatusType status;
    std::string fragmentUrl;
    static const char *manifest =
R"(<?xml version="1.0" encoding="utf-8"?>
<MPD xmlns="urn:mpeg:dash:schema:mpd:2011" profiles="urn:mpeg:dash:profile:isoff-live:2011" type="static" mediaPresentationDuration="PT2M0.0S" minBufferTime="PT4.0S">
    <Period id="0" start="PT0.0S">
        <AdaptationSet id="1" contentType="video">
            <Representation id="0" mimeType="video/mp4" codecs="avc1.640028" bandwidth="800000" width="640" height="360" frameRate="25">
                <SegmentTemplate timescale="12800" initialization="h264/video_init.mp4" media="h264/video_$Number$.m4s" startNumber="1">
                    <SegmentTimeline>
                        <S t="0" d="25600" r="59" />
                    </SegmentTimeline>
                </SegmentTemplate>
            </Representation>
        </AdaptationSet>
    </Period>
</MPD>
)";
    EXPECT_CALL(*g_mockMediaStreamContext, CacheFragment(_, _, _, _, _, true, _, _, _, _, _))
        .WillOnce(Return(true));
    status = InitializeMPD(manifest);
    EXPECT_EQ(status, eAAMPSTATUS_OK);
    std::vector<AudioTrackInfo> aTracks;
    std::string aTrackIdx;
    EXPECT_EQ(aTrackIdx,"");
    mStreamAbstractionAAMP_MPD->SwitchAudioTrack();
    EXPECT_EQ(aTrackIdx,"");
}
/**
 * @brief Unit test for verifying whether the Audio track switched to particular language as expected
 * 
 * This test initializes an MPD manifest and verifies that the offsetStart param should get updated properly
 *
 * Test Steps:
 * 1. Define an MPD manifest with no audiotrack params.
 * 2. Expect a call to CacheFragment with specific parameters and return true.
 * 3. Initialize the MPD manifest.
 * 4. Call the SetPreferredLanguages() to set the expected lang
 * 5. Call the SwitchAudioTrack()
 * 6. Expected to check the adaptationSetIdx and adaptationSetId for the selected lang is set properly
 * 
 * Expected Results:
 * Expected to check the adaptationSetIdx and adaptationSetId for the selected lang is set properly
 */
TEST_F(SwitchAudioTrackTests, SwitchAudioTrack)
{
	std::string fragmentUrl;
    AAMPStatusType status;
    static const char *manifest =
R"(<?xml version="1.0" encoding="utf-8"?>
<MPD xmlns="urn:mpeg:dash:schema:mpd:2011" profiles="urn:mpeg:dash:profile:isoff-live:2011" type="static" mediaPresentationDuration="PT2M0.0S" minBufferTime="PT4.0S">
    <Period id="0" start="PT0.0S">
            <AdaptationSet id="1" contentType="audio" segmentAlignment="true" lang="eng">
              <Role schemeIdUri="urn:mpeg:dash:role:2011" value="main"/>
              <Representation id="English Stereo" mimeType="audio/mp4" codecs="mp4a.40.2" bandwidth="128000">
                <SegmentTemplate timescale="48000" media="dash/audio_eng_$Number%03d$.mp4" startNumber="1">
                  <SegmentTimeline>
                    <S t="0" d="96000" r="449" />
                  </SegmentTimeline>
                </SegmentTemplate>
              </Representation>
            </AdaptationSet>
            <AdaptationSet id="2" contentType="audio" segmentAlignment="true" lang="fra">
              <Role schemeIdUri="urn:mpeg:dash:role:2011" value="main"/>
              <Representation id="French Stereo" mimeType="audio/mp4" codecs="mp4a.40.2" bandwidth="128000">
                <SegmentTemplate timescale="48000" media="dash/audio_fra_$Number%03d$.mp4" startNumber="1">
                  <SegmentTimeline>
                    <S t="0" d="96000" r="449" />
                  </SegmentTimeline>
                </SegmentTemplate>
              </Representation>
            </AdaptationSet>
            <AdaptationSet id="3" contentType="audio" segmentAlignment="true" lang="ger">
              <Role schemeIdUri="urn:mpeg:dash:role:2011" value="main"/>
              <Representation id="German Stereo" mimeType="audio/mp4" codecs="mp4a.40.2" bandwidth="128000">
                <SegmentTemplate timescale="48000" media="dash/audio_ger_$Number%03d$.mp4" startNumber="1">
                  <SegmentTimeline>
                    <S t="0" d="96000" r="449" />
                  </SegmentTimeline>
                </SegmentTemplate>
              </Representation>
            </AdaptationSet>
    </Period>
</MPD>
)";
    status = InitializeMPD(manifest);
    EXPECT_EQ(status, eAAMPSTATUS_OK);
    MediaTrack *track = mStreamAbstractionAAMP_MPD->GetMediaTrack(eTRACK_AUDIO);
    EXPECT_NE(track, nullptr);
    MediaStreamContext *pMediaStreamContext = static_cast<MediaStreamContext *>(track);
    EXPECT_EQ(pMediaStreamContext->adaptationSetIdx,0);
	//mPrivateInstanceAAMP->SetPreferredLanguages("ger",NULL,NULL,NULL,NULL,NULL,NULL);//switching to german audio
    pMediaStreamContext->enabled = true;
	mPrivateInstanceAAMP->preferredLanguagesList.push_back("ger");

	EXPECT_CALL(*g_mockPrivateInstanceAAMP, GetPositionMilliseconds()).WillRepeatedly(Return(0.0));

	mStreamAbstractionAAMP_MPD->SwitchAudioTrack();
    EXPECT_EQ(pMediaStreamContext->adaptationSetIdx,2);
    EXPECT_EQ(pMediaStreamContext->adaptationSetId,3);
}
