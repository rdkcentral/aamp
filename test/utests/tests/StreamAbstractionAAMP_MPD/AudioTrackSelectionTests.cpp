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
class SelectAudioTrackTests : public ::testing::Test
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
			.WillOnce(WithoutArgs(Invoke(this, &SelectAudioTrackTests::GetManifestForMPDDownloader)));

		status = mStreamAbstractionAAMP_MPD->Init(tuneType);
		return status;
	}
};

/**
 * @brief Test case to validate the correct audio track indices selection.
 * This test verifies that for a manifest containing a single audio track
 * with the 'opus' codec, the correct adaptation set and representation
 * indices are selected.
 */
TEST_F(SelectAudioTrackTests, ValidAudioIndicesTest)
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
	fragmentUrl = std::string(TEST_BASE_URL) + std::string("opus/audio_init.mp4");
	EXPECT_CALL(*g_mockMediaStreamContext, CacheFragment(fragmentUrl, _, _, _, _, true, _, _, _, _, _))
		.WillOnce(Return(true));

	status = InitializeMPD(manifest);
	EXPECT_EQ(status, eAAMPSTATUS_OK);

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
}

/**
 * @brief Test case to validate behavior when no audio tracks are present.
 * This test checks the behavior when the manifest contains
 * no audio tracks. It ensures that the audio track vector remains empty
 * and the adaptation set and representation indices stay at their initial
 * invalid values.
 */
TEST_F(SelectAudioTrackTests, NoAudioTracks)
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
	fragmentUrl = std::string(TEST_BASE_URL) + std::string("h264/video_init.mp4");
	EXPECT_CALL(*g_mockMediaStreamContext, CacheFragment(fragmentUrl, _, _, _, _, true, _, _, _, _, _))
		.WillOnce(Return(true));
	status = InitializeMPD(manifest);
	EXPECT_EQ(status, eAAMPSTATUS_OK);

	std::vector<AudioTrackInfo> aTracks;
	std::string aTrackIdx;
	int audioRepresentationIndex = -1;
	int audioAdaptationSetIndex = -1;

	mStreamAbstractionAAMP_MPD->SelectAudioTrack(aTracks, aTrackIdx, audioRepresentationIndex, audioAdaptationSetIndex);
	EXPECT_TRUE(aTracks.empty());
	EXPECT_EQ(audioRepresentationIndex, -1);
	EXPECT_EQ(audioAdaptationSetIndex, -1);
}

/**
 * @brief Test case to validate selection of multiple audio tracks.
 * This test verifies manifest containing multiple audio tracks are parsed properly.
 */
TEST_F(SelectAudioTrackTests, MultipleAudioTracks)
{
	std::string fragmentUrl;
	AAMPStatusType status;
	static const char *manifest =
R"(<?xml version="1.0" encoding="utf-8"?>
<MPD xmlns="urn:mpeg:dash:schema:mpd:2011" profiles="urn:mpeg:dash:profile:isoff-live:2011" type="static" mediaPresentationDuration="PT2M0.0S" minBufferTime="PT4.0S">
	<Period id="0" start="PT0.0S">
		<AdaptationSet id="3" contentType="audio">
			<Representation id="0" mimeType="audio/mp4" codecs="aac" bandwidth="64000" audioSamplingRate="48000">
				<SegmentTemplate timescale="48000" initialization="aac/audio_init.mp4" media="aac/audio_$Number$.mp3" startNumber="1">
					<SegmentTimeline>
						<S t="0" d="96000" r="59" />
					</SegmentTimeline>
				</SegmentTemplate>
			</Representation>
			<Representation id="1" mimeType="audio/mp4" codecs="opus" bandwidth="96000" audioSamplingRate="44100">
				<SegmentTemplate timescale="44100" initialization="opus/audio_init.mp4" media="opus/audio_$Number$.mp4" startNumber="1">
					<SegmentTimeline>
						<S t="0" d="88200" r="59" />
					</SegmentTimeline>
				</SegmentTemplate>
			</Representation>
		</AdaptationSet>
	</Period>
</MPD>
)";

	fragmentUrl = std::string(TEST_BASE_URL) + std::string("aac/audio_init.mp4");
	EXPECT_CALL(*g_mockMediaStreamContext, CacheFragment(fragmentUrl, _, _, _, _, true, _, _, _, _, _))
		.WillOnce(Return(true));

	status = InitializeMPD(manifest);
	EXPECT_EQ(status, eAAMPSTATUS_OK);
	std::vector<AudioTrackInfo> audioTracks = mStreamAbstractionAAMP_MPD->GetAvailableAudioTracks();
	bool hasOpus = false;
	bool hasAac = false;

	for (const auto &track : audioTracks)
	{
		if (track.codec == "opus")
		{
			hasOpus = true;
		}
		else if (track.codec == "aac")
		{
			hasAac = true;
		}
	}

	EXPECT_TRUE(hasOpus);
	EXPECT_TRUE(hasAac);
	std::vector<AudioTrackInfo> aTracks;
	std::string aTrackIdx;
	int audioRepresentationIndex = -1;
	int audioAdaptationSetIndex = -1;
	mStreamAbstractionAAMP_MPD->SelectAudioTrack(aTracks, aTrackIdx, audioRepresentationIndex, audioAdaptationSetIndex);
	EXPECT_EQ(audioRepresentationIndex, 0);
	EXPECT_EQ(audioAdaptationSetIndex, 0);
}

/**
 * @brief Test case to validate parsing and selection of AC-4 codec.
 * This test checks that the SelectAudioTrack correctly identifies and selects
 * an audio track with the 'ac-4' codec from the manifest.
 */
TEST_F(SelectAudioTrackTests, ParsesAC4CodecTest)
{
	AAMPStatusType status;
	std::string fragmentUrl;
	static const char *manifest =
R"(<?xml version="1.0" encoding="utf-8"?>
<MPD xmlns="urn:mpeg:dash:schema:mpd:2011" profiles="urn:mpeg:dash:profile:isoff-live:2011" type="static" mediaPresentationDuration="PT2M0.0S" minBufferTime="PT4.0S">
	<Period id="0" start="PT0.0S">
			<AdaptationSet id="3" contentType="audio">
					<Representation id="0" mimeType="audio/mp4" codecs="ac-4" bandwidth="64000" audioSamplingRate="48000">
							<SegmentTemplate timescale="48000" initialization="ac4/audio_init.mp4" media="ac4/audio_$Number$.mp4" startNumber="1">
									<SegmentTimeline>
											<S t="0" d="96000" r="59" />
									</SegmentTimeline>
							</SegmentTemplate>
					</Representation>
			</AdaptationSet>
	</Period>
</MPD>
)";
	fragmentUrl = std::string(TEST_BASE_URL) + std::string("ac4/audio_init.mp4");
	EXPECT_CALL(*g_mockMediaStreamContext, CacheFragment(fragmentUrl, _, _, _, _, true, _, _, _, _, _))
		.WillOnce(Return(true));
	status = InitializeMPD(manifest);
	EXPECT_EQ(status, eAAMPSTATUS_OK);

	std::vector<AudioTrackInfo> audioTracks = mStreamAbstractionAAMP_MPD->GetAvailableAudioTracks();
	EXPECT_EQ(audioTracks.size(), 1);
	EXPECT_EQ(audioTracks[0].codec, "ac-4");
}
