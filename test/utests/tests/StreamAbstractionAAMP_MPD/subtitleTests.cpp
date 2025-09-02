
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
class TestStreamAbstractionAAMP_MPD : public StreamAbstractionAAMP_MPD
{
public:
	TestStreamAbstractionAAMP_MPD(PrivateInstanceAAMP *aamp, double seekPos, float rate)
		: StreamAbstractionAAMP_MPD(aamp, seekPos, rate) {}

	// Public wrapper for the protected SelectSubtitleTrack method
	void PublicSelectSubtitleTrack(bool newTune, std::vector<TextTrackInfo> &tTracks, std::string &tTrackIdx)
	{
		SelectSubtitleTrack(newTune, tTracks, tTrackIdx);
	}
	void PublicRefreshTrack(AampMediaType type)
	{
		RefreshTrack(type);
	}
	void PublicSwitchSubtitleTrack(bool newTune)
	{
		SwitchSubtitleTrack(newTune);
	}
	AAMPStatusType PublicIndexNewMPDDocument(bool updateTrackInfo = false)
	{
		return IndexNewMPDDocument(updateTrackInfo);
	}

};
class SubtitleTrackTests : public ::testing::Test
{
public:
	void CallSelectSubtitleTrack(bool newTune, std::vector<TextTrackInfo> &tTracks, std::string &tTrackIdx)
	{
		mStreamAbstractionAAMP_MPD->PublicSelectSubtitleTrack(newTune, tTracks, tTrackIdx);
	}
	void CallRefreshTrack(AampMediaType type)
	{
		mStreamAbstractionAAMP_MPD->PublicRefreshTrack(type);
	}
	void CallSwitchSubtitleTrack(bool newTune)
	{
		EXPECT_CALL(*g_mockPrivateInstanceAAMP, GetPositionMilliseconds()).WillRepeatedly(Return(0.0));
		mStreamAbstractionAAMP_MPD->PublicSwitchSubtitleTrack(newTune);
	}
	AAMPStatusType CallIndexNewMPDDocument(bool updateTrackInfo = false)
	{
		return mStreamAbstractionAAMP_MPD->PublicIndexNewMPDDocument(updateTrackInfo);
	}

	PrivateInstanceAAMP *mPrivateInstanceAAMP;
	TestStreamAbstractionAAMP_MPD *mStreamAbstractionAAMP_MPD; // Use the test subclass
	CDAIObject *mCdaiObj;
	const char *mManifest;
	static constexpr const char *TEST_BASE_URL = "http://host/asset/";
	static constexpr const char *TEST_MANIFEST_URL = "http://host/asset/manifest.mpd";
	ManifestDownloadResponsePtr mResponse = MakeSharedManifestDownloadResponsePtr();
	using BoolConfigSettings = std::map<AAMPConfigSettingBool, bool>;
	const BoolConfigSettings mDefaultBoolConfigSettings = {
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
		{eAAMPConfig_GstSubtecEnabled, false},
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

		mCdaiObj = new CDAIObjectMPD(mPrivateInstanceAAMP);
		mStreamAbstractionAAMP_MPD = new TestStreamAbstractionAAMP_MPD(mPrivateInstanceAAMP, 0.0, AAMP_NORMAL_PLAY_RATE);
		mStreamAbstractionAAMP_MPD->SetCDAIObject(mCdaiObj);
		mPrivateInstanceAAMP->SetManifestUrl(TEST_MANIFEST_URL);

		mManifest = nullptr;
		mResponse = nullptr;
		mBoolConfigSettings = mDefaultBoolConfigSettings;
	}

	void TearDown() override
	{
		delete mStreamAbstractionAAMP_MPD;
		mStreamAbstractionAAMP_MPD = nullptr;
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
		mStreamAbstractionAAMP_MPD = new TestStreamAbstractionAAMP_MPD(mPrivateInstanceAAMP, seekPos, rate);
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
			.WillOnce(WithoutArgs(Invoke(this, &SubtitleTrackTests::GetManifestForMPDDownloader)));
		status = mStreamAbstractionAAMP_MPD->Init(tuneType);
		return status;
	}
};
/**
 * @brief Test case for selecting a subtitle track in the SubtitleTrackTests test suite.
 *
 * This test initializes an MPD manifest with a subtitle track and verifies the selection
 * of the subtitle track using the SelectSubtitleTrack method.
 *
 * The test performs the following steps:
 * 1. Initializes the MPD manifest with a static MPD containing a subtitle track.
 * 2. Verifies that the initialization status is eAAMPSTATUS_OK.
 * 3. Retrieves the subtitle media track and verifies it is not null.
 * 4. Calls the SelectSubtitleTrack method with the appropriate parameters.
 * 5. Verifies that the selected track index is "0-0".
 */
TEST_F(SubtitleTrackTests, selectsubtitleTrack)
{
	std::string fragmentUrl;
	AAMPStatusType status;
	static const char *manifest =
    R"(<?xml version="1.0" encoding="utf-8"?>
<MPD xmlns="urn:mpeg:dash:schema:mpd:2011" profiles="urn:mpeg:dash:profile:isoff-live:2011" type="static" mediaPresentationDuration="PT2M0.0S" minBufferTime="PT4.0S">
    <Period id="0" start="PT0.0S">
        <AdaptationSet id="16" contentType="text" segmentAlignment="true" lang="ger">
            <Role schemeIdUri="urn:mpeg:dash:role:2011" value="caption"/>
            <Representation id="Germany TTML captions" mimeType="application/mp4" codecs="stpp" bandwidth="400">
                <SegmentTemplate timescale="48000" media="dash/ttml_de_$Number%03d$.mp4" startNumber="1">
                    <SegmentTimeline>
                        <S t="0" d="96000" r="449"/>
                    </SegmentTimeline>
                </SegmentTemplate>
            </Representation>
        </AdaptationSet>
    </Period>
</MPD>
)";
	status = InitializeMPD(manifest);
	EXPECT_EQ(status, eAAMPSTATUS_OK);
	std::vector<TextTrackInfo> tTracks;
	std::string tTrackIdx;
	bool newTune = true;
	EXPECT_EQ(tTrackIdx, "");
	CallSelectSubtitleTrack(newTune, tTracks, tTrackIdx);
	EXPECT_EQ(tTrackIdx, "0-0");
}
/**
 * @brief Unit test for verifying the behavior when there are no subtitle tracks in the MPD manifest.
 *
 * This test initializes an MPD manifest without any subtitle tracks and verifies that the subtitle track index remains empty.
 *
 * Test Steps:
 * 1. Define an MPD manifest with a video adaptation set but no subtitle adaptation set.
 * 2. Expect a call to CacheFragment with specific parameters and return true.
 * 3. Initialize the MPD manifest.
 * 4. Verify that the initialization status is eAAMPSTATUS_OK.
 * 5. Create an empty vector for TextTrackInfo and an empty string for the subtitle track index.
 * 6. Call SelectSubtitleTrack with the newTune flag set to true, the empty vector, and the empty string.
 * 7. Verify that the subtitle track index remains empty.
 *
 * Expected Results:
 * - The subtitle track index should remain empty as there are no subtitle tracks in the MPD manifest.
 */
TEST_F(SubtitleTrackTests, Nosubtitletracks)
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
                        <S t="0" d="25600" r="59"/>
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
	std::vector<TextTrackInfo> tTracks;
	std::string tTrackIdx;
	bool newTune = true;
	EXPECT_EQ(tTrackIdx, "");
	CallSelectSubtitleTrack(newTune, tTracks, tTrackIdx);
	EXPECT_EQ(tTrackIdx, "");
}
/**
 * @brief Test case for switching subtitle tracks in the SubtitleTrackTests fixture.
 *
 * This test initializes an MPD manifest with multiple subtitle tracks in different languages
 * (French, Spanish, and German). It verifies the initialization status and checks the initial
 * subtitle track. Then, it switches the preferred subtitle language to German and verifies
 * that the subtitle track is switched correctly.
 *
 * Steps:
 * 1. Initialize the MPD manifest.
 * 2. Verify the initialization status is eAAMPSTATUS_OK.
 * 3. Retrieve the subtitle media track and verify it is not null.
 * 4. Check the initial adaptation set index.
 * 5. Set the preferred text language to German.
 * 6. Switch the subtitle track.
 * 7. Verify the adaptation set index and ID are updated correctly.
 *
 * Expected Results:
 * - The initialization status should be eAAMPSTATUS_OK.
 * - The subtitle media track should not be null.
 * - After switching, the adaptation set index should be 1 and the adaptation set ID should be 16.
 */
TEST_F(SubtitleTrackTests, SwitchSubtitleTrack)
{
	std::string fragmentUrl;
	AAMPStatusType status;
	static const char *manifest =
    R"(<?xml version="1.0" encoding="utf-8"?>
<MPD xmlns="urn:mpeg:dash:schema:mpd:2011" profiles="urn:mpeg:dash:profile:isoff-live:2011" type="static" mediaPresentationDuration="PT2M0.0S" minBufferTime="PT4.0S">
    <Period id="0" start="PT0.0S">
        <AdaptationSet id="14" contentType="text" segmentAlignment="true" lang="fra">
            <Role schemeIdUri="urn:mpeg:dash:role:2011" value="caption"/>
            <Representation id="French TTML captions" mimeType="application/mp4" codecs="stpp" bandwidth="400">
                <SegmentTemplate timescale="48000" media="dash/ttml_fr_$Number%03d$.mp4" startNumber="1">
                    <SegmentTimeline>
                        <S t="0" d="96000" r="449"/>
                    </SegmentTimeline>
                </SegmentTemplate>
            </Representation>
        </AdaptationSet>
        <AdaptationSet id="15" contentType="text" segmentAlignment="true" lang="spa">
            <Role schemeIdUri="urn:mpeg:dash:role:2011" value="caption"/>
            <Representation id="Spanish TTML captions" mimeType="application/mp4" codecs="stpp" bandwidth="400">
                <SegmentTemplate timescale="48000" media="dash/ttml_es_$Number%03d$.mp4" startNumber="1">
                    <SegmentTimeline>
                        <S t="0" d="96000" r="449"/>
                    </SegmentTimeline>
                </SegmentTemplate>
            </Representation>
        </AdaptationSet>
        <AdaptationSet id="16" contentType="text" segmentAlignment="true" lang="ger">
            <Role schemeIdUri="urn:mpeg:dash:role:2011" value="caption"/>
            <Representation id="Germany TTML captions" mimeType="application/mp4" codecs="stpp" bandwidth="400">
                <SegmentTemplate timescale="48000" media="dash/ttml_de_$Number%03d$.mp4" startNumber="1">
                    <SegmentTimeline>
                        <S t="0" d="96000" r="449"/>
                    </SegmentTimeline>
                </SegmentTemplate>
            </Representation>
        </AdaptationSet>
    </Period>
</MPD>
)";
	status = InitializeMPD(manifest);
	EXPECT_EQ(status, eAAMPSTATUS_OK);
	MediaTrack *track = mStreamAbstractionAAMP_MPD->GetMediaTrack(eTRACK_SUBTITLE);
	EXPECT_NE(track, nullptr);
	MediaStreamContext *pMediaStreamContext = static_cast<MediaStreamContext *>(track);
	EXPECT_EQ(pMediaStreamContext->adaptationSetIdx, 0);
	mPrivateInstanceAAMP->preferredTextLanguagesList.push_back("ger");
	mPrivateInstanceAAMP->SetPreferredTextLanguages("ger"); // switching to german
	CallSwitchSubtitleTrack(true);
	EXPECT_EQ(pMediaStreamContext->adaptationSetIdx, 2);
	EXPECT_EQ(pMediaStreamContext->adaptationSetId, 16);
}
/**
 * @brief Test case for refreshing the subtitle track.
 *
 * This test case verifies the functionality of refreshing the subtitle track.
 * It initializes the MPD with a given manifest and expects the status to be eAAMPSTATUS_OK.
 * Then, it calls the RefreshTrack function of the StreamAbstractionAAMP_MPD class with the subtitle media type.
 */
TEST_F(SubtitleTrackTests, RefreshTrack)
{
	AAMPStatusType status;
	std::string fragmentUrl;
	static const char *manifest =
    R"(<?xml version="1.0" encoding="utf-8"?>
<MPD xmlns="urn:mpeg:dash:schema:mpd:2011" profiles="urn:mpeg:dash:profile:isoff-live:2011" type="static" mediaPresentationDuration="PT2M0.0S" minBufferTime="PT4.0S">
    <Period id="0" start="PT0.0S">
        <AdaptationSet id="16" contentType="text" segmentAlignment="true" lang="ger">
            <Role schemeIdUri="urn:mpeg:dash:role:2011" value="caption"/>
            <Representation id="Germany TTML captions" mimeType="application/mp4" codecs="stpp" bandwidth="400">
                <SegmentTemplate timescale="48000" media="dash/ttml_de_$Number%03d$.mp4" startNumber="1">
                    <SegmentTimeline>
                        <S t="0" d="96000" r="449"/>
                    </SegmentTimeline>
                </SegmentTemplate>
            </Representation>
        </AdaptationSet>
    </Period>
</MPD>
)";
	status = InitializeMPD(manifest);
	EXPECT_EQ(status, eAAMPSTATUS_OK);
	MediaTrack *track = mStreamAbstractionAAMP_MPD->GetMediaTrack(eTRACK_SUBTITLE);
	EXPECT_NE(track, nullptr);
	MediaStreamContext *pMediaStreamContext = static_cast<MediaStreamContext *>(track);
	EXPECT_EQ(pMediaStreamContext->refreshSubtitles,false);
	CallRefreshTrack(eMEDIATYPE_SUBTITLE);
	EXPECT_EQ(pMediaStreamContext->refreshSubtitles,true);
}

TEST_F(SubtitleTrackTests, SkipSubtitleFetchTests)
{
	std::string fragmentUrl;
	AAMPStatusType status;
	static const char *manifest =
	R"(<?xml version="1.0" encoding="utf-8"?>
<MPD xmlns:xsi="http://www.w3.org/2001/XMLSchema-instance"
	xmlns="urn:mpeg:dash:schema:mpd:2011"
	xmlns:xlink="http://www.w3.org/1999/xlink"
	xsi:schemaLocation="urn:mpeg:DASH:schema:MPD:2011 http://standards.iso.org/ittf/PubliclyAvailableStandards/MPEG-DASH_schema_files/DASH-MPD.xsd"
	profiles="urn:mpeg:dash:profile:isoff-live:2011"
	type="static"
	mediaPresentationDuration="PT15M0.0S"
	minBufferTime="PT4.0S">
	<ProgramInformation>
	</ProgramInformation>
	<Period id="0" start="PT0.0S">
		<AdaptationSet id="0" contentType="video" segmentAlignment="true" bitstreamSwitching="true" lang="und">
			<Representation id="0" mimeType="video/mp4" codecs="avc1.4d4028" bandwidth="5000000" width="1920" height="1080" frameRate="25/1">
				<SegmentTemplate timescale="12800" initialization="dash/1080p_init.m4s" media="dash/1080p_$Number%03d$.m4s" startNumber="1">
					<SegmentTimeline>
						<S t="0" d="25600" r="449" />
					</SegmentTimeline>
				</SegmentTemplate>
			</Representation>
	</AdaptationSet>
		<AdaptationSet id="3" contentType="audio" segmentAlignment="true" bitstreamSwitching="true" lang="ger">
            <Role schemeIdUri="urn:mpeg:dash:role:2011" value="german" />
			<Representation id="5" mimeType="audio/mp4" codecs="mp4a.40.2" bandwidth="288000" audioSamplingRate="48000">
				<AudioChannelConfiguration schemeIdUri="urn:mpeg:dash:23003:3:audio_channel_configuration:2011" value="1" />
				<SegmentTemplate timescale="48000" initialization="dash/de_init.m4s" media="dash/de_$Number%03d$.mp3" startNumber="1">
					<SegmentTimeline>
						<S t="0" d="95232" />
						<S d="96256" r="447" />
						<S d="30080" />
					</SegmentTimeline>
				</SegmentTemplate>
			</Representation>
		</AdaptationSet>
		<AdaptationSet id="12" contentType="text" segmentAlignment="true" bitstreamSwitching="true" lang="ger">
            <Role schemeIdUri="urn:mpeg:dash:role:2011" value="caption"/>
			<Representation id="Germany TTML captions" mimeType="application/mp4" codecs="stpp" bandwidth="400">
				<SegmentTemplate timescale="48000" media="dash/ttml_de_$Number%03d$.mp4" startNumber="1">
					<SegmentTimeline>
						<S t="0" d="95232" />
						<S d="96256" r="447" />
						<S d="30080" />
					</SegmentTimeline>
				</SegmentTemplate>
			</Representation>
		</AdaptationSet>
	</Period>
</MPD>
)";
	EXPECT_CALL(*g_mockMediaStreamContext,CacheFragment(_, _, _, _, _, true, _, _, _, _, _))
        .Times(2);//init segment is  available for audio and video so set to true
	EXPECT_CALL(*g_mockMediaStreamContext,CacheFragment(_, _, _, _, _, false, _, _, _, _, _))
        .Times(1);//init segment is not available for subtitle so set to false

	status = InitializeMPD(manifest);
	EXPECT_EQ(status, eAAMPSTATUS_OK);
	MediaTrack *track = mStreamAbstractionAAMP_MPD->GetMediaTrack(eTRACK_SUBTITLE);
	EXPECT_NE(track, nullptr);
	MediaStreamContext *pMediaStreamContext = static_cast<MediaStreamContext *>(track);
	CallIndexNewMPDDocument(true);
	mStreamAbstractionAAMP_MPD->PushNextFragment(pMediaStreamContext,eCURLINSTANCE_SUBTITLE);
	pMediaStreamContext->freshManifest=true;
	//when skipfetch sets to true, fetchfragment will be avoided
	EXPECT_CALL(*g_mockMediaStreamContext, CacheFragment(_, eCURLINSTANCE_SUBTITLE, _,_, _, _, _, _, _, _, _))
				.Times(0);
	CallSwitchSubtitleTrack(true);

}
