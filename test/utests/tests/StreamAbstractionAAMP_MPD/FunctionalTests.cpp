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
#include "MockAampUtils.h"
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

	using BoolConfigSettings = std::map<AAMPConfigSettingBool, bool>;
	using IntConfigSettings = std::map<AAMPConfigSettingInt, int>;

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
		{eAAMPConfig_DashParallelFragDownload, false},
		{eAAMPConfig_EnableLowLatencyDash, false}
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
		{eAAMPConfig_VODTrickPlayFPS, TRICKPLAY_VOD_PLAYBACK_FPS},
		{eAAMPConfig_MaxFragmentChunkCached, DEFAULT_CACHED_FRAGMENT_CHUNKS_PER_TRACK}
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
			EXPECT_CALL(*g_mockAampConfig, GetConfigValue(i.first))
			.Times(AnyNumber())
			.WillRepeatedly(Return(i.second));
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

		if (g_mockAampUtils)
		{
			delete g_mockAampUtils;
			g_mockAampUtils = nullptr;
		}

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
	bool GetManifest(std::string remoteUrl, AampGrowableBuffer *buffer)
	{
		EXPECT_STREQ(remoteUrl.c_str(), TEST_MANIFEST_URL);

		/* Setup fake AampGrowableBuffer contents. */
        buffer->Clear();
        buffer->AppendBytes((char *)mManifest, strlen(mManifest));

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

	/* Initialize MPD. The video initialization segment is cached. */
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

	/* Initialize MPD. The video initialization segment is cached. */
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

/**
 * @brief Live segment template test.
 *
 * The $Number$ identifier is set to the segment number which is calculated from the current time
 * and is used to select the uri of the media segment to download and present.
 */
TEST_F(FunctionalTests, Live_Test1)
{
	AAMPStatusType status;
	const char *currentTimeISO = "2023-01-01T00:00:00Z";
	double currentTime;
	double availabilityStartTime;
	double deltaTime;
	long long timeMS;
	long long fragmentNumber;
	double firstPresentationTime;
	char url[64];
	std::string fragmentUrl;
	/* The value of these variables must match the content of the manifest below: */
	const char *availabilityStartTimeISO = "1970-01-01T00:00:00Z";
	constexpr uint32_t segmentTemplateDuration = 5000;
	constexpr uint32_t timescale = 2500;
	constexpr uint32_t startNumber = 0;
	static const char *manifest =
R"(<?xml version="1.0" encoding="utf-8"?>
<MPD xmlns="urn:mpeg:dash:schema:mpd:2011" availabilityStartTime="1970-01-01T00:00:00Z" maxSegmentDuration="PT2S" minBufferTime="PT4.000S" minimumUpdatePeriod="P100Y" profiles="urn:dvb:dash:profile:dvb-dash:2014,urn:dvb:dash:profile:dvb-dash:isoff-ext-live:2014" publishTime="2023-01-01T00:00:00Z" timeShiftBufferDepth="PT5M" type="dynamic">
  <Period id="p0" start="PT0S">
    <AdaptationSet contentType="video" lang="eng" maxFrameRate="25" maxHeight="1080" maxWidth="1920" par="16:9" segmentAlignment="true" startWithSAP="1">
      <SegmentTemplate duration="5000" initialization="video_init.mp4" media="video_$Number$.m4s" startNumber="0" timescale="2500" />
      <Representation id="1" mimeType="video/mp4" codecs="avc1.640028" width="640" height="360" frameRate="25" sar="1:1" bandwidth="1000000" />
    </AdaptationSet>
  </Period>
</MPD>
)";
	constexpr uint32_t segmentDurationSec = segmentTemplateDuration / timescale;

	/* Setup AAMP utils mock. */
	g_mockAampUtils = new StrictMock<MockAampUtils>();
	currentTime = ISO8601DateTimeToUTCSeconds(currentTimeISO);
	availabilityStartTime = ISO8601DateTimeToUTCSeconds(availabilityStartTimeISO);
	deltaTime = currentTime - availabilityStartTime;
	timeMS = 1000LL*((long long)currentTime);
	EXPECT_CALL(*g_mockAampUtils, aamp_GetCurrentTimeMS())
	  .Times(AnyNumber())
	  .WillRepeatedly(Return(timeMS));

	/* Initialize MPD. The video initialization segment is cached. */
	fragmentUrl = std::string(TEST_BASE_URL) + std::string("video_init.mp4");
	EXPECT_CALL(*g_mockMediaStreamContext, CacheFragment(fragmentUrl, _, _, _, _, true, _, _, _, _, _))
		.WillOnce(Return(true));

	status = InitializeMPD(manifest);

	/* The first segment downloaded will be at the live point. */
	fragmentNumber = ((((long long)deltaTime) - AAMP_LIVE_OFFSET) / segmentDurationSec) + startNumber;

	/* Calculate the first frame time (in seconds). The gstreamer interface will perform a seek to
	 * this postion to start playback at the live point. It might not be the first frame in the
	 * downloaded segment.
	 */
	firstPresentationTime = deltaTime - AAMP_LIVE_OFFSET;
	ASSERT_EQ(firstPresentationTime, mStreamAbstractionAAMP_MPD->GetFirstPTS());

	/* Push the first video segment to present. */
	(void)snprintf(url, sizeof(url), "%svideo_%lld.m4s", TEST_BASE_URL, fragmentNumber);
	fragmentUrl = std::string(url);
	EXPECT_CALL(*g_mockMediaStreamContext, CacheFragment(fragmentUrl, _, _, _, _, false, _, _, _, _, _))
		.WillOnce(Return(true));

	PushNextFragment(eTRACK_VIDEO);

	/* Push the second video segment to present. */
	fragmentNumber++;
	(void)snprintf(url, sizeof(url), "%svideo_%lld.m4s", TEST_BASE_URL, fragmentNumber);
	fragmentUrl = std::string(url);
	EXPECT_CALL(*g_mockMediaStreamContext, CacheFragment(fragmentUrl, _, _, _, _, false, _, _, _, _, _))
		.WillOnce(Return(true));

	PushNextFragment(eTRACK_VIDEO);
}

/**
 * @brief Live segment template test with no startNumber specified.
 *
 * The $Number$ identifier is set to the segment number which is calculated from the current time
 * and is used to select the uri of the media segment to download and present.
 * The default startNumber used to calculate the value of $Number$ is 1, if it is missing from
 * the manifest.
 */
TEST_F(FunctionalTests, Live_Test2)
{
	AAMPStatusType status;
	const char *currentTimeISO = "2023-01-01T00:00:00Z";
	double currentTime;
	double availabilityStartTime;
	double deltaTime;
	long long timeMS;
	long long fragmentNumber;
	double firstPresentationTime;
	char url[64];
	std::string fragmentUrl;
	/* The value of these variables must match the content of the manifest below: */
	const char *availabilityStartTimeISO = "1973-01-14T01:30:00Z";
	constexpr uint32_t segmentTemplateDuration = 5000;
	constexpr uint32_t timescale = 2500;
	constexpr uint32_t startNumber = 1;				// default value, as it is not specified in the manifest
	static const char *manifest =
R"(<?xml version="1.0" encoding="utf-8"?>
<MPD xmlns="urn:mpeg:dash:schema:mpd:2011" availabilityStartTime="1973-01-14T01:30:00Z" maxSegmentDuration="PT2S" minBufferTime="PT4.000S" minimumUpdatePeriod="P100Y" profiles="urn:dvb:dash:profile:dvb-dash:2014,urn:dvb:dash:profile:dvb-dash:isoff-ext-live:2014" publishTime="2023-01-01T00:00:00Z" timeShiftBufferDepth="PT5M" type="dynamic">
  <Period id="p0" start="PT0S">
    <AdaptationSet contentType="video" lang="eng" maxFrameRate="25" maxHeight="1080" maxWidth="1920" par="16:9" segmentAlignment="true" startWithSAP="1">
      <SegmentTemplate duration="5000" initialization="video_init.mp4" media="video_$Number$.m4s" timescale="2500" />
      <Representation id="1" mimeType="video/mp4" codecs="avc1.640028" width="640" height="360" frameRate="25" sar="1:1" bandwidth="1000000" />
    </AdaptationSet>
  </Period>
</MPD>
)";
	constexpr uint32_t segmentDurationSec = segmentTemplateDuration / timescale;

	/* Setup AAMP utils mock. */
	g_mockAampUtils = new StrictMock<MockAampUtils>();
	currentTime = ISO8601DateTimeToUTCSeconds(currentTimeISO);
	availabilityStartTime = ISO8601DateTimeToUTCSeconds(availabilityStartTimeISO);
	deltaTime = currentTime - availabilityStartTime;
	timeMS = 1000LL*((long long)currentTime);
	EXPECT_CALL(*g_mockAampUtils, aamp_GetCurrentTimeMS())
	  .Times(AnyNumber())
	  .WillRepeatedly(Return(timeMS));

	/* Initialize MPD. The video initialization segment is cached. */
	fragmentUrl = std::string(TEST_BASE_URL) + std::string("video_init.mp4");
	EXPECT_CALL(*g_mockMediaStreamContext, CacheFragment(fragmentUrl, _, _, _, _, true, _, _, _, _, _))
		.WillOnce(Return(true));

	status = InitializeMPD(manifest);
	EXPECT_EQ(status, eAAMPSTATUS_OK);

	/* The first segment downloaded will be at the live point. */
	fragmentNumber = ((((long long)deltaTime) - AAMP_LIVE_OFFSET) / segmentDurationSec) + startNumber;

	/* Calculate the first frame time (in seconds). The gstreamer interface will perform a seek to
	 * this postion to start playback at the live point. It might not be the first frame in the
	 * downloaded segment.
	 */
	firstPresentationTime = deltaTime - AAMP_LIVE_OFFSET;
	ASSERT_EQ(firstPresentationTime, mStreamAbstractionAAMP_MPD->GetFirstPTS());

	/* Push the first video segment to present. */
	(void)snprintf(url, sizeof(url), "%svideo_%lld.m4s", TEST_BASE_URL, fragmentNumber);
	fragmentUrl = std::string(url);
	EXPECT_CALL(*g_mockMediaStreamContext, CacheFragment(fragmentUrl, _, _, _, _, false, _, _, _, _, _))
		.WillOnce(Return(true));

	PushNextFragment(eTRACK_VIDEO);

	/* Push the second video segment to present. */
	fragmentNumber++;
	(void)snprintf(url, sizeof(url), "%svideo_%lld.m4s", TEST_BASE_URL, fragmentNumber);
	fragmentUrl = std::string(url);
	EXPECT_CALL(*g_mockMediaStreamContext, CacheFragment(fragmentUrl, _, _, _, _, false, _, _, _, _, _))
		.WillOnce(Return(true));

	PushNextFragment(eTRACK_VIDEO);
}

/**
 * @brief Missing period ids test.
 *
 * Unique period ids are added if none is specified in the manifest. AAMP uses
 * these to distinguish between periods.
 */
TEST_F(FunctionalTests, MissingPeriodIds)
{
	std::string fragmentUrl;
	AAMPStatusType status;
	dash::mpd::IMPD *mpd;
	static const char *manifest =
R"(<?xml version="1.0"?>
<MPD xmlns="urn:mpeg:dash:schema:mpd:2011" minBufferTime="PT2S" type="static" mediaPresentationDuration="PT0H0M6.000S" maxSegmentDuration="PT0H0M2.000S" profiles="urn:mpeg:dash:profile:full:2011,urn:mpeg:dash:profile:cmaf:2019">
  <Period duration="PT0H0M2.000S">
    <AdaptationSet maxWidth="1920" maxHeight="1080" maxFrameRate="25" par="16:9">
      <Representation id="1" mimeType="video/mp4" codecs="avc1.640028" width="640" height="360" frameRate="25" sar="1:1" bandwidth="1000000">
        <SegmentTemplate timescale="2500" media="video_$Number$.mp4" initialization="video_init.mp4">
          <SegmentTimeline>
            <S d="5000" />
          </SegmentTimeline>
        </SegmentTemplate>
      </Representation>
    </AdaptationSet>
  </Period>
  <Period id="ad" duration="PT0H0M2.000S">
    <AdaptationSet maxWidth="1920" maxHeight="1080" maxFrameRate="25" par="16:9">
      <Representation id="1" mimeType="video/mp4" codecs="avc1.640028" width="640" height="360" frameRate="25" sar="1:1" bandwidth="1000000">
        <SegmentTemplate timescale="2500" media="ad_$Number$.mp4" initialization="ad_init.mp4">
          <SegmentTimeline>
            <S d="5000" />
          </SegmentTimeline>
        </SegmentTemplate>
      </Representation>
    </AdaptationSet>
  </Period>
  <Period duration="PT0H0M2.000S">
    <AdaptationSet maxWidth="1920" maxHeight="1080" maxFrameRate="25" par="16:9">
      <Representation id="1" mimeType="video/mp4" codecs="avc1.640028" width="640" height="360" frameRate="25" sar="1:1" bandwidth="1000000">
        <SegmentTemplate timescale="2500" media="video_$Number$.mp4" initialization="video_init.mp4">
          <SegmentTimeline>
            <S t="5000" d="5000" />
          </SegmentTimeline>
        </SegmentTemplate>
      </Representation>
    </AdaptationSet>
  </Period>
</MPD>
)";

	/* Initialize MPD. The first video initialization segment is cached. */
	fragmentUrl = std::string(TEST_BASE_URL) + std::string("video_init.mp4");
	EXPECT_CALL(*g_mockMediaStreamContext, CacheFragment(fragmentUrl, _, _, _, _, true, _, _, _, _, _))
		.WillOnce(Return(true));

	status = InitializeMPD(manifest);
	EXPECT_EQ(status, eAAMPSTATUS_OK);

	/* Get the manifest data. */
	mpd = mStreamAbstractionAAMP_MPD->GetMPD();
	EXPECT_NE(mpd, nullptr);
	EXPECT_EQ(mpd->GetPeriods().size(), 3);

	/* Verify that a unique period id is added to the first and last period. The
	 * actual id values depend on the test running order.
	 */
	EXPECT_NE(mpd->GetPeriods().at(0)->GetId(), std::string());
	EXPECT_EQ(mpd->GetPeriods().at(1)->GetId(), std::string("ad"));
	EXPECT_NE(mpd->GetPeriods().at(2)->GetId(), std::string());
	EXPECT_NE(mpd->GetPeriods().at(0)->GetId(), mpd->GetPeriods().at(2)->GetId());
}

