/*
* If not stated otherwise in this file or this component's license file the
* following copyright and licenses apply:
*
* Copyright 2023 RDK Management
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
#include "MockAampGstPlayer.h"
#include "MockPrivateInstanceAAMP.h"
#include "MockMediaStreamContext.h"

using ::testing::_;
using ::testing::An;
using ::testing::SetArgReferee;
using ::testing::Return;
using ::testing::StrictMock;
using ::testing::NiceMock;
using ::testing::WithArgs;
using ::testing::AnyNumber;
using ::testing::DoAll;

AampConfig *gpGlobalConfig{nullptr};
AampLogManager *mLogObj{nullptr};

class FunctionalTests : public ::testing::Test
{
protected:
	PrivateInstanceAAMP *mPrivateInstanceAAMP;
	StreamAbstractionAAMP_MPD *mStreamAbstractionAAMP_MPD;
	CDAIObject *mCdaiObj;
	const char *mManifest;
	static constexpr const char *TEST_BASE_URL = "http://host/asset/";
	static constexpr const char *TEST_MANIFEST_URL = "http://host/asset/manifest.mpd";

	using BoolConfigSettings = std::map<AAMPConfigSettings, bool>;
	using IntConfigSettings = std::map<AAMPConfigSettings, int>;

	/** @brief Boolean AAMP configuration settings. */
	const BoolConfigSettings mBoolConfigSettings =
	{
		{eAAMPConfig_EnableCMCD, false},
		{eAAMPConfig_BulkTimedMetaReport, false},
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
		{eAAMPConfig_PropogateURIParam, true},
		{eAAMPConfig_DashParallelFragDownload, false}
	};

	/** @brief Integer AAMP configuration settings. */
	const IntConfigSettings mIntConfigSettings =
	{
		{eAAMPConfig_ABRCacheLength, DEFAULT_ABR_CACHE_LENGTH},
		{eAAMPConfig_MaxABRNWBufferRampUp, AAMP_HIGH_BUFFER_BEFORE_RAMPUP},
		{eAAMPConfig_MinABRNWBufferRampDown, AAMP_LOW_BUFFER_BEFORE_RAMPDOWN},
		{eAAMPConfig_ABRNWConsistency, DEFAULT_ABR_NW_CONSISTENCY_CNT},
		{eAAMPConfig_RampDownLimit, -1},
		{eAAMPConfig_MaxFragmentCached, DEFAULT_CACHED_FRAGMENTS_PER_TRACK},
		{eAAMPConfig_PrePlayBufferCount, DEFAULT_PREBUFFER_COUNT},
		{eAAMPConfig_VODTrickPlayFPS, TRICKPLAY_VOD_PLAYBACK_FPS}
	};

	void SetUp() override
	{
		if(gpGlobalConfig == nullptr)
		{
			gpGlobalConfig =  new AampConfig();
		}

		mPrivateInstanceAAMP = new PrivateInstanceAAMP(gpGlobalConfig);

		g_mockAampConfig = new NiceMock<MockAampConfig>();

		g_mockAampGstPlayer = new MockAAMPGstPlayer(mLogObj, mPrivateInstanceAAMP);
		mPrivateInstanceAAMP->mStreamSink = g_mockAampGstPlayer;
		mPrivateInstanceAAMP->mIsDefaultOffset = true;

		g_mockPrivateInstanceAAMP = new StrictMock<MockPrivateInstanceAAMP>();

		g_mockMediaStreamContext = new StrictMock<MockMediaStreamContext>();

		/* Setup configuration mock. */
		for (const auto & b : mBoolConfigSettings)
		{
			EXPECT_CALL(*g_mockAampConfig, IsConfigSet(b.first))
			  .Times(AnyNumber())
			  .WillRepeatedly(Return(b.second));
		}

		for (const auto & i : mIntConfigSettings)
		{
			EXPECT_CALL(*g_mockAampConfig, GetConfigValue(i.first, An<int &>()))
			  .Times(AnyNumber())
			  .WillRepeatedly(DoAll(SetArgReferee<1>(i.second), Return(true)));
		}

		/* Create MPD instance. */
		mStreamAbstractionAAMP_MPD = new StreamAbstractionAAMP_MPD(mLogObj, mPrivateInstanceAAMP, 0, AAMP_NORMAL_PLAY_RATE);
		mCdaiObj = new CDAIObjectMPD(mLogObj, mPrivateInstanceAAMP);
		mStreamAbstractionAAMP_MPD->SetCDAIObject(mCdaiObj);

		mManifest = nullptr;
	}

	void TearDown() override
	{
		delete mPrivateInstanceAAMP;
		mPrivateInstanceAAMP = nullptr;

		delete mStreamAbstractionAAMP_MPD;
		mStreamAbstractionAAMP_MPD = nullptr;

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

		mManifest = nullptr;
	}

public:
	/**
	 * @brief Get manifest helper method
	 *
	 * @param[in] remoteUrl Manfiest url
	 * @param[out] buffer Buffer containing manifest data
	 * @retval true on success
	*/
	bool GetManifest(std::string remoteUrl, struct GrowableBuffer *buffer)
	{
		EXPECT_STREQ(remoteUrl.c_str(), TEST_MANIFEST_URL);

		/* Setup fake GrowableBuffer contents. */
		buffer->ptr = (char *)mManifest;
		buffer->len = strlen(mManifest);

		return true;
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
	 * @return eAAMPSTATUS_OK on success or another value on error
	 */
	AAMPStatusType InitializeMPD(const char *manifest)
	{
		AAMPStatusType status;

		mManifest = manifest;
		mPrivateInstanceAAMP->SetManifestUrl(TEST_MANIFEST_URL);

		EXPECT_CALL(*g_mockPrivateInstanceAAMP, GetFile (_, _, _, _, _, _, _, _, _, _, _, _))
			.WillOnce(WithArgs<0,1>(Invoke(this, &FunctionalTests::GetManifest)));

		EXPECT_CALL(*g_mockPrivateInstanceAAMP, SetState(eSTATE_PREPARING));

		EXPECT_CALL(*g_mockPrivateInstanceAAMP, GetState(_))
			.WillOnce(SetArgReferee<0>(eSTATE_PREPARING));

		status = mStreamAbstractionAAMP_MPD->Init(TuneType::eTUNETYPE_NEW_NORMAL);
		return status;
	}

	/**
	 * @brief Push next fragment helper method
	 *
	 * @param[in] trackType Media track type
	 */
	void PushNextFragment(TrackType trackType)
	{
		MediaTrack *track = mStreamAbstractionAAMP_MPD->GetMediaTrack(trackType);
		EXPECT_NE(track, nullptr);

		MediaStreamContext *pMediaStreamContext = static_cast<MediaStreamContext *>(track);
		mStreamAbstractionAAMP_MPD->PushNextFragment(pMediaStreamContext, 0);
	}
};

/**
 * @brief Segment time identifier test.
 *
 * The $Time$ identifier is set to the segment time and is used to select the uri of the media
 * segment to download and present.
 */
TEST_F(FunctionalTests, SegmentTimeIdentifier_Test1)
{
	std::string fragmentUrl;
	AAMPStatusType status;
	static const char *manifest =
R"(<?xml version="1.0" encoding="utf-8"?>
<MPD xmlns="urn:mpeg:dash:schema:mpd:2011" minBufferTime="PT2S" type="static" mediaPresentationDuration="PT0H10M54.00S" profiles="urn:mpeg:dash:profile:isoff-live:2011,http://dashif.org/guidelines/dash264">
  <Period duration="PT1M0S">
    <AdaptationSet maxWidth="1920" maxHeight="1080" maxFrameRate="25" par="16:9">
      <Representation id="1" mimeType="video/mp4" codecs="avc1.640028" width="640" height="360" frameRate="25" sar="1:1" bandwidth="1000000">
        <SegmentTemplate timescale="2500" media="video_$Time$.mp4" initialization="video_init.mp4">
          <SegmentTimeline>
            <S d="5000" r="29" />
          </SegmentTimeline>
        </SegmentTemplate>
      </Representation>
    </AdaptationSet>
  </Period>
</MPD>
)";

	/* Initialize MPD. The video initialization segement is cached. */
	fragmentUrl = std::string(TEST_BASE_URL) + std::string("video_init.mp4");
	EXPECT_CALL(*g_mockMediaStreamContext, CacheFragment(fragmentUrl, _, _, _, _, true, _, _, _, _, _))
		.WillOnce(Return(true));

	status = InitializeMPD(manifest);
	EXPECT_EQ(status, eAAMPSTATUS_OK);

	/* Push the first video segment to present. The segment time identifier ("$Time$") is zero. The
	 * segment starts at time 0.0s and has a duration of 2.0s.
	 */
	fragmentUrl = std::string(TEST_BASE_URL) + std::string("video_0.mp4");
	EXPECT_CALL(*g_mockMediaStreamContext, CacheFragment(fragmentUrl, _, 0.0, 2.0, _, false, _, _, _, _, _))
		.WillOnce(Return(true));

	PushNextFragment(eTRACK_VIDEO);

	/* Push the second video segment to present. The segment time identifier ("$Time$") is the
	 * start of the second segment which is the duration of the first segment (5000). The segment
	 * starts at time 2.0s and has a duration of 2.0s.
	 */
	fragmentUrl = std::string(TEST_BASE_URL) + std::string("video_5000.mp4");
	EXPECT_CALL(*g_mockMediaStreamContext, CacheFragment(fragmentUrl, _, 2.0, 2.0, _, false, _, _, _, _, _))
		.WillOnce(Return(true));

	PushNextFragment(eTRACK_VIDEO);
}

/**
 * @brief Segment time identifier test with presentation time offset.
 *
 * The $Time$ identifier is set to the segment time and is used to select the uri of the media
 * segment to download and present at the presentation time offset.
 */
TEST_F(FunctionalTests, SegmentTimeIdentifier_Test2)
{
	std::string fragmentUrl;
	AAMPStatusType status;
	static const char *manifest =
R"(<?xml version="1.0" encoding="utf-8"?>
<MPD xmlns="urn:mpeg:dash:schema:mpd:2011" minBufferTime="PT2S" type="static" mediaPresentationDuration="PT0H10M54.00S" profiles="urn:mpeg:dash:profile:isoff-live:2011,http://dashif.org/guidelines/dash264">
  <Period duration="PT1M0S">
    <AdaptationSet maxWidth="1920" maxHeight="1080" maxFrameRate="25" par="16:9">
      <Representation id="1" mimeType="video/mp4" codecs="avc1.640028" width="640" height="360" frameRate="25" sar="1:1" bandwidth="1000000">
        <SegmentTemplate timescale="2500" presentationTimeOffset="100" media="video_$Time$.mp4" initialization="video_init.mp4">
          <SegmentTimeline>
            <S d="5000" r="29" />
          </SegmentTimeline>
        </SegmentTemplate>
      </Representation>
    </AdaptationSet>
  </Period>
</MPD>
)";

	/* Initialize MPD. The video initialization segement is cached. */
	fragmentUrl = std::string(TEST_BASE_URL) + std::string("video_init.mp4");
	EXPECT_CALL(*g_mockMediaStreamContext, CacheFragment(fragmentUrl, _, _, _, _, true, _, _, _, _, _))
		.WillOnce(Return(true));

	status = InitializeMPD(manifest);
	EXPECT_EQ(status, eAAMPSTATUS_OK);

	/* Push the first video segment to present. This segment contains the frame at the presentation
	 * time offset. The segment time identifier ("$Time$") is zero. The segment starts at time 0.0s
	 * and has a duration of 2.0s.
	 */
	fragmentUrl = std::string(TEST_BASE_URL) + std::string("video_0.mp4");
	EXPECT_CALL(*g_mockMediaStreamContext, CacheFragment(fragmentUrl, _, 0.0, 2.0, _, false, _, _, _, _, _))
		.WillOnce(Return(true));

	PushNextFragment(eTRACK_VIDEO);

	/* Push the second video segment to present. The segment time identifier ("$Time$") is the
	 * start of the second segment which is the duration of the first segment (5000). The segment
	 * starts at time 2.0s and has a duration of 2.0s.
	 */
	fragmentUrl = std::string(TEST_BASE_URL) + std::string("video_5000.mp4");
	EXPECT_CALL(*g_mockMediaStreamContext, CacheFragment(fragmentUrl, _, 2.0, 2.0, _, false, _, _, _, _, _))
		.WillOnce(Return(true));

	PushNextFragment(eTRACK_VIDEO);
}
