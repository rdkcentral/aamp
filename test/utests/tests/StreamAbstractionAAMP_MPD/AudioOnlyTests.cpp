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
// #include "MediaStreamContext.h"
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
using ::testing::Pointee;

extern AampConfig *gpGlobalConfig;

/**
 * @brief LinearTests tests common base class.
 */
class AudioOnlyTests : public ::testing::Test
{
protected:

	PrivateInstanceAAMP *mPrivateInstanceAAMP;
	StreamAbstractionAAMP_MPD *mStreamAbstractionAAMP_MPD;
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
		{eAAMPConfig_ABRBufferCounter,DEFAULT_ABR_BUFFER_COUNTER},
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

		g_mockAampGstPlayer = new MockAAMPGstPlayer(mPrivateInstanceAAMP);

		g_mockPrivateInstanceAAMP = new StrictMock<MockPrivateInstanceAAMP>();

		g_mockMediaStreamContext = new StrictMock<MockMediaStreamContext>();

		g_mockAampMPDDownloader = new StrictMock<MockAampMPDDownloader>();

		g_mockAampStreamSinkManager = new NiceMock<MockAampStreamSinkManager>();

		mStreamAbstractionAAMP_MPD = nullptr;

		mManifest = nullptr;
		mResponse = nullptr;
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
		// For the time being return the same manifest again
		EXPECT_CALL(*g_mockAampMPDDownloader, GetManifest (_, _, _))
			.WillRepeatedly(WithoutArgs(Invoke(this, &AudioOnlyTests::GetManifestForMPDDownloader)));
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
 * @brief H264OpusManifest test
 *
 * The DASH manifest has both audio and video adaptation sets. AudioOnlyPlayback config is set.
 * In audio only playback, only audio track will be enabled downloading audio fragments
 */
TEST_F(AudioOnlyTests, H264OpusManifest)
{
	std::string fragmentUrl;
	AAMPStatusType status;
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
	EXPECT_CALL(*g_mockAampConfig, IsConfigSet(eAAMPConfig_AudioOnlyPlayback))
		.WillRepeatedly(Return(true));
	/* Initialize MPD. The audio initialization segment is cached. */
	fragmentUrl = std::string(TEST_BASE_URL) + std::string("opus/audio_init.mp4");
	EXPECT_CALL(*g_mockMediaStreamContext, CacheFragment(fragmentUrl, _, _, _, _, true, _, _, _, _, _))
		.WillOnce(Return(true));

	status = InitializeMPD(manifest);
	EXPECT_EQ(status, eAAMPSTATUS_OK);
	EXPECT_EQ(mPrivateInstanceAAMP->mAudioOnlyPb, true);
	EXPECT_EQ(mStreamAbstractionAAMP_MPD->GetMediaTrack(eTRACK_VIDEO)->enabled, true);
	EXPECT_EQ(mStreamAbstractionAAMP_MPD->GetMediaTrack(eTRACK_AUDIO)->enabled, false);

	/* Push the first audio segment to present. Here, video is replaced with audio track in audio only case */
	fragmentUrl = std::string(TEST_BASE_URL) + std::string("opus/audio_1.mp3");
	EXPECT_CALL(*g_mockMediaStreamContext, CacheFragment(fragmentUrl, _, _, _, _, false, _, _, _, _, _))
		.WillOnce(Return(true));
	// Call for video track
	PushNextFragment(eTRACK_VIDEO);
}


/**
 * @brief AudioOpusSegmentList test
 *
 * The DASH manifest has only audio adaptation set in SegmentList format.
 * AAMP will move to audio only playback mode
 */
TEST_F(AudioOnlyTests, AudioOpusSegmentList)
{
	std::string fragmentUrl;
	AAMPStatusType status;
	static const char *manifest =
R"(<?xml version="1.0" encoding="utf-8"?>
<MPD xmlns="urn:mpeg:dash:schema:mpd:2011" profiles="urn:mpeg:dash:profile:isoff-on-demand:2011" type="static" mediaPresentationDuration="PT248.1818084716797S" minBufferTime="PT2S">
	<Period id="0">
		<AdaptationSet id="3" contentType="audio">
			<Representation id="1" codecs="opus" audioSamplingRate="48000" bandwidth="53057" mimeType="audio/mp4">
				<BaseURL>http://host/asset/audio.mp4</BaseURL>
				<SegmentList timescale="48000" duration="480000">
					<Initialization range="0-1031"/>
					<SegmentURL mediaRange="1364-61619"/>
					<SegmentURL mediaRange="61620-121755"/>
					<SegmentURL mediaRange="121756-181891"/>
					<SegmentURL mediaRange="181892-246080"/>
					<SegmentURL mediaRange="246081-310269"/>
					<SegmentURL mediaRange="310270-374458"/>
					<SegmentURL mediaRange="374459-438647"/>
					<SegmentURL mediaRange="438648-502836"/>
					<SegmentURL mediaRange="502837-567025"/>
					<SegmentURL mediaRange="567026-631214"/>
					<SegmentURL mediaRange="631215-695403"/>
					<SegmentURL mediaRange="695404-759592"/>
					<SegmentURL mediaRange="759593-823781"/>
					<SegmentURL mediaRange="823782-887970"/>
					<SegmentURL mediaRange="887971-952159"/>
					<SegmentURL mediaRange="952160-1016348"/>
					<SegmentURL mediaRange="1016349-1080537"/>
					<SegmentURL mediaRange="1080538-1144726"/>
					<SegmentURL mediaRange="1144727-1208915"/>
					<SegmentURL mediaRange="1208916-1273104"/>
					<SegmentURL mediaRange="1273105-1337293"/>
					<SegmentURL mediaRange="1337294-1401482"/>
					<SegmentURL mediaRange="1401483-1465671"/>
					<SegmentURL mediaRange="1465672-1529860"/>
					<SegmentURL mediaRange="1529861-1584033"/>
				</SegmentList>
			</Representation>
		</AdaptationSet>
	</Period>
</MPD>
)";
	EXPECT_CALL(*g_mockAampConfig, IsConfigSet(eAAMPConfig_AudioOnlyPlayback))
		.WillRepeatedly(Return(false));
	/* Initialize MPD. The audio initialization segment is cached. */
	fragmentUrl = std::string(TEST_BASE_URL) + std::string("audio.mp4");
	// On a first look, this is a bug in the code. initialization is returned as empty with this manifest
	std::string indexRange = "0-1363";
	EXPECT_CALL(*g_mockMediaStreamContext, CacheFragment(fragmentUrl, _, _, _, testing::StrEq(indexRange.c_str()), true, _, _, _, _, _))
		.WillOnce(Return(true));

	status = InitializeMPD(manifest);
	EXPECT_EQ(status, eAAMPSTATUS_OK);
	EXPECT_EQ(mPrivateInstanceAAMP->mAudioOnlyPb, true);
	EXPECT_EQ(mStreamAbstractionAAMP_MPD->GetMediaTrack(eTRACK_VIDEO)->enabled, true);
	EXPECT_EQ(mStreamAbstractionAAMP_MPD->GetMediaTrack(eTRACK_AUDIO)->enabled, false);

	/* Push the first audio segment to present.*/
	indexRange = "1364-61619";
	EXPECT_CALL(*g_mockMediaStreamContext, CacheFragment(fragmentUrl, _, _, _, testing::StrEq(indexRange.c_str()), false, _, _, _, _, _))
		.WillOnce(Return(true));
	// Call for video track
	PushNextFragment(eTRACK_VIDEO);
}

/**
 * @brief AudioOpusSegmentList_1 test
 *
 * The DASH manifest has audio adaptation set in SegmentList format. Seek to end of the period.
 */
TEST_F(AudioOnlyTests, AudioOpusSegmentList_1)
{
	std::string fragmentUrl;
	AAMPStatusType status;
	static const char *manifest =
R"(<?xml version="1.0" encoding="utf-8"?>
<MPD xmlns="urn:mpeg:dash:schema:mpd:2011" profiles="urn:mpeg:dash:profile:isoff-on-demand:2011" type="static" mediaPresentationDuration="PT248.1818084716797S" minBufferTime="PT2S">
	<Period id="0">
		<AdaptationSet id="3" contentType="audio">
			<Representation id="1" codecs="opus" audioSamplingRate="48000" bandwidth="53057" mimeType="audio/mp4">
				<BaseURL>http://host/asset/audio.mp4</BaseURL>
				<SegmentList timescale="48000" duration="480000">
					<Initialization range="0-1031"/>
					<SegmentURL mediaRange="1364-61619"/>
					<SegmentURL mediaRange="61620-121755"/>
					<SegmentURL mediaRange="121756-181891"/>
					<SegmentURL mediaRange="181892-246080"/>
					<SegmentURL mediaRange="246081-310269"/>
					<SegmentURL mediaRange="310270-374458"/>
					<SegmentURL mediaRange="374459-438647"/>
					<SegmentURL mediaRange="438648-502836"/>
					<SegmentURL mediaRange="502837-567025"/>
					<SegmentURL mediaRange="567026-631214"/>
					<SegmentURL mediaRange="631215-695403"/>
					<SegmentURL mediaRange="695404-759592"/>
					<SegmentURL mediaRange="759593-823781"/>
					<SegmentURL mediaRange="823782-887970"/>
					<SegmentURL mediaRange="887971-952159"/>
					<SegmentURL mediaRange="952160-1016348"/>
					<SegmentURL mediaRange="1016349-1080537"/>
					<SegmentURL mediaRange="1080538-1144726"/>
					<SegmentURL mediaRange="1144727-1208915"/>
					<SegmentURL mediaRange="1208916-1273104"/>
					<SegmentURL mediaRange="1273105-1337293"/>
					<SegmentURL mediaRange="1337294-1401482"/>
					<SegmentURL mediaRange="1401483-1465671"/>
					<SegmentURL mediaRange="1465672-1529860"/>
					<SegmentURL mediaRange="1529861-1584033"/>
				</SegmentList>
			</Representation>
		</AdaptationSet>
	</Period>
</MPD>
)";
	EXPECT_CALL(*g_mockAampConfig, IsConfigSet(eAAMPConfig_AudioOnlyPlayback))
		.WillRepeatedly(Return(false));
	/* Initialize MPD. The audio initialization segment is cached. */
	fragmentUrl = std::string(TEST_BASE_URL) + std::string("audio.mp4");
	// On a first look, this is a bug in the code. initialization is returned as empty with this manifest
	std::string indexRange = "0-1363";
	EXPECT_CALL(*g_mockMediaStreamContext, CacheFragment(fragmentUrl, _, _, _, testing::StrEq("0-1363"), true, _, _, _, _, _))
		.WillOnce(Return(true));

	status = InitializeMPD(manifest, TuneType::eTUNETYPE_NEW_NORMAL, 240);
	EXPECT_EQ(status, eAAMPSTATUS_OK);
	EXPECT_EQ(mPrivateInstanceAAMP->mAudioOnlyPb, true);
	EXPECT_EQ(mStreamAbstractionAAMP_MPD->GetMediaTrack(eTRACK_VIDEO)->enabled, true);
	EXPECT_EQ(mStreamAbstractionAAMP_MPD->GetMediaTrack(eTRACK_AUDIO)->enabled, false);

	/* Push the first audio segment to present.*/
	indexRange = "1529861-1584033";
	EXPECT_CALL(*g_mockMediaStreamContext, CacheFragment(fragmentUrl, _, _, _, testing::StrEq("1529861-1584033"), false, _, _, _, _, _))
		.WillOnce(Return(true));
	// Call for video track
	PushNextFragment(eTRACK_VIDEO);
}
