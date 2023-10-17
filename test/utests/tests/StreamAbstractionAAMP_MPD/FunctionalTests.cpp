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
#include "MockAampMPDDownloader.h"

using ::testing::_;
using ::testing::An;
using ::testing::SetArgReferee;
using ::testing::Return;
using ::testing::StrictMock;
using ::testing::NiceMock;
using ::testing::WithArgs;
using ::testing::WithoutArgs;
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
	std::shared_ptr<ManifestDownloadResponse> mResponse =  std::make_shared<ManifestDownloadResponse> ();
	using BoolConfigSettings = std::map<AAMPConfigSettingBool, bool>;
	using IntConfigSettings = std::map<AAMPConfigSettingInt, int>;

	/** @brief Boolean AAMP configuration settings. */
	const BoolConfigSettings mDefaultBoolConfigSettings =
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
		{eAAMPConfig_DisableATMOS, false},
		{eAAMPConfig_DisableEC3, false},
		{eAAMPConfig_DisableAC3, false},
		{eAAMPConfig_EnableLowLatencyDash, false}
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
		{eAAMPConfig_VODTrickPlayFPS, TRICKPLAY_VOD_PLAYBACK_FPS}
	};

	IntConfigSettings mIntConfigSettings;

	void SetUp() override
	{
		if(gpGlobalConfig == nullptr)
		{
			gpGlobalConfig =  new AampConfig();
		}

		mPrivateInstanceAAMP = new PrivateInstanceAAMP(gpGlobalConfig);

		g_mockAampConfig = new NiceMock<MockAampConfig>();

		g_mockAampUtils = nullptr;

		g_mockAampGstPlayer = new MockAAMPGstPlayer(mLogObj, mPrivateInstanceAAMP);
		mPrivateInstanceAAMP->mStreamSink = g_mockAampGstPlayer;
		mPrivateInstanceAAMP->mIsDefaultOffset = true;

		g_mockPrivateInstanceAAMP = new StrictMock<MockPrivateInstanceAAMP>();

		g_mockMediaStreamContext = new StrictMock<MockMediaStreamContext>();

		g_mockAampMPDDownloader = new StrictMock<MockAampMPDDownloader>();

		mStreamAbstractionAAMP_MPD = nullptr;

		mManifest = nullptr;
		mResponse = nullptr;
		mBoolConfigSettings = mDefaultBoolConfigSettings;
		mIntConfigSettings = mDefaultIntConfigSettings;
	}

	void TearDown() override
	{
		delete mPrivateInstanceAAMP;
		mPrivateInstanceAAMP = nullptr;

		if (mStreamAbstractionAAMP_MPD)
		{
			delete mStreamAbstractionAAMP_MPD;
			mStreamAbstractionAAMP_MPD = nullptr;
		}

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
	

	void GetMPDFromManifest(std::shared_ptr<ManifestDownloadResponse> response)
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
	 * @param[in] remoteUrl Manfiest url
	 * @param[out] buffer Buffer containing manifest data
	 * @retval true on success
	*/
	std::shared_ptr<ManifestDownloadResponse> GetManifestForMPDDownloader()
	{
		std::shared_ptr<ManifestDownloadResponse> response = std::make_shared<ManifestDownloadResponse> ();
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
	 * @return eAAMPSTATUS_OK on success or another value on error
	 */
	AAMPStatusType InitializeMPD(const char *manifest, TuneType tuneType = TuneType::eTUNETYPE_NEW_NORMAL, double seekPos = 0.0)
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
		mStreamAbstractionAAMP_MPD = new StreamAbstractionAAMP_MPD(mLogObj, mPrivateInstanceAAMP, seekPos, AAMP_NORMAL_PLAY_RATE);
		mCdaiObj = new CDAIObjectMPD(mLogObj, mPrivateInstanceAAMP);
		mStreamAbstractionAAMP_MPD->SetCDAIObject(mCdaiObj);

		mPrivateInstanceAAMP->SetManifestUrl(TEST_MANIFEST_URL);

		/* Initialize MPD. */
		EXPECT_CALL(*g_mockPrivateInstanceAAMP, SetState(eSTATE_PREPARING));

		EXPECT_CALL(*g_mockPrivateInstanceAAMP, GetState(_))
			.Times(AnyNumber())
			.WillRepeatedly(SetArgReferee<0>(eSTATE_PREPARING));

		EXPECT_CALL(*g_mockAampMPDDownloader, GetManifest (_, _, _))
			.WillOnce(WithoutArgs(Invoke(this, &FunctionalTests::GetManifestForMPDDownloader)));

		status = mStreamAbstractionAAMP_MPD->Init(tuneType);
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
	char url[64];
	std::string fragmentUrl;
	double seekPosition;
	/* The value of these variables must match the content of the manifest below: */
	const char *availabilityStartTimeISO = "1970-01-01T00:00:00Z";
	constexpr uint32_t segmentTemplateDuration = 5000;
	constexpr uint32_t timescale = 2500;
	constexpr uint32_t startNumber = 0;
	constexpr uint32_t timeShiftBufferDepth = 300;
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

	/* The seek position will be at the beginning of the fragment containing the
	 * live point measured from the beginning of the available content in the
	 * time shift buffer.
	 */
	seekPosition = ((timeShiftBufferDepth - AAMP_LIVE_OFFSET)/segmentDurationSec)*segmentDurationSec;
	EXPECT_EQ(mStreamAbstractionAAMP_MPD->GetStreamPosition(), seekPosition);

	/* The first segment downloaded will be at the live point. */
	fragmentNumber = ((((long long)deltaTime) - AAMP_LIVE_OFFSET) / segmentDurationSec) + startNumber;

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
 * @brief LLDEV-42214 test.
 *
 * In this test:
 * - The last playback started at 5.0 seconds.
 * - A seek to live is performed.
 * - The test manifest has multiple periods.
 * - The mid-fragment seek flag is set.
 *
 * Verify that:
 * - The seek position is set to the live point.
 * - The segment at the live point is pushed.
 */
TEST_F(FunctionalTests, LLDEV_42214)
{
	AAMPStatusType status;
	const char *currentTimeISO = "2023-01-01T00:01:00Z";
	double initialSeekPosition = 5.0;
	double currentTime;
	double availabilityStartTime;
	double deltaTime;
	long long timeMS;
	long long fragmentNumber;
	char url[64];
	std::string fragmentUrl;
	double seekPosition;
	/* The value of these variables must match the content of the manifest below: */
	const char *availabilityStartTimeISO = "2023-01-01T00:00:00Z";
	constexpr uint32_t segmentTemplateDuration = 5000;
	constexpr uint32_t timescale = 2500;
	constexpr uint32_t startNumber = 1;
	double periodDuration = 30.0;
	double totalDuration = 60.0;
	static const char *manifest =
R"(<?xml version="1.0" encoding="utf-8"?>
<MPD xmlns="urn:mpeg:dash:schema:mpd:2011" availabilityStartTime="2023-01-01T00:00:00Z" maxSegmentDuration="PT2S" minBufferTime="PT4.000S" minimumUpdatePeriod="P100Y" profiles="urn:dvb:dash:profile:dvb-dash:2014,urn:dvb:dash:profile:dvb-dash:isoff-ext-live:2014" publishTime="2023-01-01T00:01:00Z" timeShiftBufferDepth="PT5M" type="dynamic">
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
</MPD>
)";
	constexpr uint32_t segmentDurationSec = segmentTemplateDuration / timescale;

	/* Setup the AAMP utils mock. */
	g_mockAampUtils = new StrictMock<MockAampUtils>();
	currentTime = ISO8601DateTimeToUTCSeconds(currentTimeISO);
	availabilityStartTime = ISO8601DateTimeToUTCSeconds(availabilityStartTimeISO);
	deltaTime = currentTime - (availabilityStartTime + periodDuration); /* In period p1. */
	timeMS = 1000LL*((long long)currentTime);
	EXPECT_CALL(*g_mockAampUtils, aamp_GetCurrentTimeMS())
		.Times(AnyNumber())
		.WillRepeatedly(Return(timeMS));

	/* Set the mid-fragment seek flag. */
	mBoolConfigSettings[eAAMPConfig_MidFragmentSeek] = true;

	/* Initialize MPD. The video initialization segment is cached. */
	fragmentUrl = std::string(TEST_BASE_URL) + std::string("video_p1_init.mp4");
	EXPECT_CALL(*g_mockMediaStreamContext, CacheFragment(fragmentUrl, _, _, _, _, true, _, _, _, _, _))
		.WillOnce(Return(true));

	status = InitializeMPD(manifest, eTUNETYPE_SEEKTOLIVE, initialSeekPosition);

	/* The seek position will be at the live point measured from the start of
	 * the available content. This may be in the middle of a fragment.
	 */
	seekPosition = totalDuration - AAMP_LIVE_OFFSET;
	EXPECT_EQ(mStreamAbstractionAAMP_MPD->GetStreamPosition(), seekPosition);

	/* The first segment downloaded will be at the live point. */
	fragmentNumber = ((((long long)deltaTime) - AAMP_LIVE_OFFSET) / segmentDurationSec) + startNumber;

	/* Push the first video segment to present. */
	(void)snprintf(url, sizeof(url), "%svideo_p1_%lld.m4s", TEST_BASE_URL, fragmentNumber);
	fragmentUrl = std::string(url);
	EXPECT_CALL(*g_mockMediaStreamContext, CacheFragment(fragmentUrl, _, _, _, _, false, _, _, _, _, _))
		.WillOnce(Return(true));

	PushNextFragment(eTRACK_VIDEO);

	/* Push the second video segment to present. */
	fragmentNumber++;
	(void)snprintf(url, sizeof(url), "%svideo_p1_%lld.m4s", TEST_BASE_URL, fragmentNumber);
	fragmentUrl = std::string(url);
	EXPECT_CALL(*g_mockMediaStreamContext, CacheFragment(fragmentUrl, _, _, _, _, false, _, _, _, _, _))
		.WillOnce(Return(true));

	PushNextFragment(eTRACK_VIDEO);
}

/**
 * @brief VP8 video and Vorbis audio codec test.
 *
 * VP8 encoded video and Vorbis encoded audio contained in MP4 format.
 */
TEST_F(FunctionalTests, VP8AndVorbisInMP4)
{
	std::string fragmentUrl;
	AAMPStatusType status;
	dash::mpd::IMPD *mpd;
	static const char *manifest =
R"(<?xml version="1.0" encoding="utf-8"?>
<MPD xmlns="urn:mpeg:dash:schema:mpd:2011" profiles="urn:mpeg:dash:profile:isoff-live:2011" type="static" mediaPresentationDuration="PT2M0.0S" minBufferTime="PT4.0S">
	<Period id="0" start="PT0.0S">
		<AdaptationSet id="0" contentType="video">
			<Representation id="0" mimeType="video/mp4" codecs="vp08.00.41.08" bandwidth="800000" width="640" height="360" frameRate="25">
				<SegmentTemplate timescale="12800" initialization="vp8/video_init.mp4" media="vp8/video_$Number$.m4s" startNumber="1">
					<SegmentTimeline>
						<S t="0" d="25600" r="59" />
					</SegmentTimeline>
				</SegmentTemplate>
			</Representation>
		</AdaptationSet>
		<AdaptationSet id="1" contentType="audio">
			<Representation id="0" mimeType="audio/mp4" codecs="vorbis" bandwidth="24605" audioSamplingRate="48000">
				<SegmentTemplate timescale="48000" initialization="vorbis/audio_init.mp4" media="vorbis/audio_$Number$.mp3" startNumber="1">
					<SegmentTimeline>
						<S t="0" d="96000" r="59" />
					</SegmentTimeline>
				</SegmentTemplate>
			</Representation>
		</AdaptationSet>
	</Period>
</MPD>
)";

	/* Initialize MPD. The initialization segments are cached. */
	fragmentUrl = std::string(TEST_BASE_URL) + std::string("vp8/video_init.mp4");
	EXPECT_CALL(*g_mockMediaStreamContext, CacheFragment(fragmentUrl, _, _, _, _, true, _, _, _, _, _))
		.WillOnce(Return(true));

	fragmentUrl = std::string(TEST_BASE_URL) + std::string("vorbis/audio_init.mp4");
	EXPECT_CALL(*g_mockMediaStreamContext, CacheFragment(fragmentUrl, _, _, _, _, true, _, _, _, _, _))
		.WillOnce(Return(true));

	status = InitializeMPD(manifest);
	EXPECT_EQ(status, eAAMPSTATUS_OK);

	/* Push the first video segment to present. */
	fragmentUrl = std::string(TEST_BASE_URL) + std::string("vp8/video_1.m4s");
	EXPECT_CALL(*g_mockMediaStreamContext, CacheFragment(fragmentUrl, _, _, _, _, false, _, _, _, _, _))
		.WillOnce(Return(true));

	PushNextFragment(eTRACK_VIDEO);

	/* Push the first audio segment to present. */
	fragmentUrl = std::string(TEST_BASE_URL) + std::string("vorbis/audio_1.mp3");
	EXPECT_CALL(*g_mockMediaStreamContext, CacheFragment(fragmentUrl, _, _, _, _, false, _, _, _, _, _))
		.WillOnce(Return(true));

	PushNextFragment(eTRACK_AUDIO);
}

/**
 * @brief VP9 video and Opus audio codec test.
 *
 * VP9 encoded video and Opus encoded audio contained in MP4 format.
 */
TEST_F(FunctionalTests, VP9AndOpusInMP4)
{
	std::string fragmentUrl;
	AAMPStatusType status;
	dash::mpd::IMPD *mpd;
	static const char *manifest =
R"(<?xml version="1.0" encoding="utf-8"?>
<MPD xmlns="urn:mpeg:dash:schema:mpd:2011" profiles="urn:mpeg:dash:profile:isoff-live:2011" type="static" mediaPresentationDuration="PT2M0.0S" minBufferTime="PT4.0S">
	<Period id="0" start="PT0.0S">
		<AdaptationSet id="0" contentType="video">
			<Representation id="0" mimeType="video/mp4" codecs="vp09.00.10.08" bandwidth="800000" width="640" height="360" frameRate="25">
				<SegmentTemplate timescale="12800" initialization="vp9/video_init.mp4" media="vp9/video_$Number$.m4s" startNumber="1">
					<SegmentTimeline>
						<S t="0" d="25600" r="59" />
					</SegmentTimeline>
				</SegmentTemplate>
			</Representation>
		</AdaptationSet>
		<AdaptationSet id="2" contentType="audio">
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

	/* Initialize MPD. The initialization segments are cached. */
	fragmentUrl = std::string(TEST_BASE_URL) + std::string("vp9/video_init.mp4");
	EXPECT_CALL(*g_mockMediaStreamContext, CacheFragment(fragmentUrl, _, _, _, _, true, _, _, _, _, _))
		.WillOnce(Return(true));

	fragmentUrl = std::string(TEST_BASE_URL) + std::string("opus/audio_init.mp4");
	EXPECT_CALL(*g_mockMediaStreamContext, CacheFragment(fragmentUrl, _, _, _, _, true, _, _, _, _, _))
		.WillOnce(Return(true));

	status = InitializeMPD(manifest);
	EXPECT_EQ(status, eAAMPSTATUS_OK);

	/* Push the first video segment to present. */
	fragmentUrl = std::string(TEST_BASE_URL) + std::string("vp9/video_1.m4s");
	EXPECT_CALL(*g_mockMediaStreamContext, CacheFragment(fragmentUrl, _, _, _, _, false, _, _, _, _, _))
		.WillOnce(Return(true));

	PushNextFragment(eTRACK_VIDEO);

	/* Push the first audio segment to present. */
	fragmentUrl = std::string(TEST_BASE_URL) + std::string("opus/audio_1.mp3");
	EXPECT_CALL(*g_mockMediaStreamContext, CacheFragment(fragmentUrl, _, _, _, _, false, _, _, _, _, _))
		.WillOnce(Return(true));

	PushNextFragment(eTRACK_AUDIO);
}

/**
 * @brief Multi MP4 codec test.
 *
 * VP9 and H264 encoded video with Opus and AAC encoded audio contained in MP4
 * format. H264 and AAC is selected in preference to VP9 and Opus.
 */
TEST_F(FunctionalTests, MultiCodecMP4)
{
	std::string fragmentUrl;
	AAMPStatusType status;
	dash::mpd::IMPD *mpd;
	static const char *manifest =
R"(<?xml version="1.0" encoding="utf-8"?>
<MPD xmlns="urn:mpeg:dash:schema:mpd:2011" profiles="urn:mpeg:dash:profile:isoff-live:2011" type="static" mediaPresentationDuration="PT2M0.0S" minBufferTime="PT4.0S">
	<Period id="0" start="PT0.0S">
		<AdaptationSet id="0" contentType="video">
			<Representation id="0" mimeType="video/mp4" codecs="vp09.00.10.08" bandwidth="800000" width="640" height="360" frameRate="25">
				<SegmentTemplate timescale="12800" initialization="vp9/video_init.mp4" media="vp9/video_$Number$.m4s" startNumber="1">
					<SegmentTimeline>
						<S t="0" d="25600" r="59" />
					</SegmentTimeline>
				</SegmentTemplate>
			</Representation>
		</AdaptationSet>
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
		<AdaptationSet id="4" contentType="audio">
			<Representation id="0" mimeType="audio/mp4" codecs="mp4a.40.2" bandwidth="64000" audioSamplingRate="48000">
				<SegmentTemplate timescale="48000" initialization="aac/audio_init.mp4" media="aac/audio_$Number$.mp3" startNumber="1">
					<SegmentTimeline>
						<S t="0" d="96000" r="59" />
					</SegmentTimeline>
				</SegmentTemplate>
			</Representation>
		</AdaptationSet>
	</Period>
</MPD>
)";

	/* Initialize MPD. The initialization segments are cached. H264 video is
	 * selected in prefernce to VP9, AAC audio is selected in preference to
	 * Opus.
	 */
	fragmentUrl = std::string(TEST_BASE_URL) + std::string("h264/video_init.mp4");
	EXPECT_CALL(*g_mockMediaStreamContext, CacheFragment(fragmentUrl, _, _, _, _, true, _, _, _, _, _))
		.WillOnce(Return(true));

	fragmentUrl = std::string(TEST_BASE_URL) + std::string("aac/audio_init.mp4");
	EXPECT_CALL(*g_mockMediaStreamContext, CacheFragment(fragmentUrl, _, _, _, _, true, _, _, _, _, _))
		.WillOnce(Return(true));

	status = InitializeMPD(manifest);
	EXPECT_EQ(status, eAAMPSTATUS_OK);

	/* Push the first video segment to present. */
	fragmentUrl = std::string(TEST_BASE_URL) + std::string("h264/video_1.m4s");
	EXPECT_CALL(*g_mockMediaStreamContext, CacheFragment(fragmentUrl, _, _, _, _, false, _, _, _, _, _))
		.WillOnce(Return(true));

	PushNextFragment(eTRACK_VIDEO);

	/* Push the first audio segment to present. */
	fragmentUrl = std::string(TEST_BASE_URL) + std::string("aac/audio_1.mp3");
	EXPECT_CALL(*g_mockMediaStreamContext, CacheFragment(fragmentUrl, _, _, _, _, false, _, _, _, _, _))
		.WillOnce(Return(true));

	PushNextFragment(eTRACK_AUDIO);
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

/**
 * @brief ABR mode test.
 *
 * Verify that the ABR manager is selected to manage ABR.
 */
TEST_F(FunctionalTests, ABRManagerMode)
{
	std::string fragmentUrl;
	AAMPStatusType status;
	static const char *manifest =
R"(<?xml version="1.0" encoding="utf-8"?>
<MPD xmlns="urn:mpeg:dash:schema:mpd:2011" minBufferTime="PT2S" type="static" mediaPresentationDuration="PT0H10M54.00S" profiles="urn:mpeg:dash:profile:isoff-live:2011,http://dashif.org/guidelines/dash264">
 <Period duration="PT1M0S">
  <AdaptationSet maxWidth="1920" maxHeight="1080" maxFrameRate="25" par="16:9">
   <Representation id="1" mimeType="video/mp4" codecs="avc1.640028" width="640" height="360" frameRate="25" sar="1:1" bandwidth="1000000">
	<SegmentTemplate timescale="2500" media="video_$Number$.mp4" initialization="video_init.mp4">
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

	/* Verify that the ABR manager is selected. */
	EXPECT_EQ(mStreamAbstractionAAMP_MPD->GetABRMode(), StreamAbstractionAAMP::ABRMode::ABR_MANAGER);
}

/**
 * @brief ABR mode test.
 *
 * The DASH manifest has the fogtsb attribute set. Verify that Fog is selected
 * to manage ABR.
 */
TEST_F(FunctionalTests, FogABRMode)
{
	std::string fragmentUrl;
	AAMPStatusType status;
	static const char *manifest =
R"(<?xml version="1.0" encoding="utf-8"?>
<MPD xmlns="urn:mpeg:dash:schema:mpd:2011" minBufferTime="PT2S" type="static" mediaPresentationDuration="PT0H10M54.00S" profiles="urn:mpeg:dash:profile:isoff-live:2011,http://dashif.org/guidelines/dash264" fogtsb="true">
 <Period duration="PT1M0S">
  <AdaptationSet maxWidth="1920" maxHeight="1080" maxFrameRate="25" par="16:9">
   <Representation id="1" mimeType="video/mp4" codecs="avc1.640028" width="640" height="360" frameRate="25" sar="1:1" bandwidth="1000000">
	<SegmentTemplate timescale="2500" media="video_$Number$.mp4" initialization="video_init.mp4">
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

	/* Verify that Fog is selected. */
	EXPECT_EQ(mStreamAbstractionAAMP_MPD->GetABRMode(), StreamAbstractionAAMP::ABRMode::FOG_TSB);
}

/**
 * @brief test for XML Parser to read and process CDATA section under EventStream
 *
 * The MPD initialization should be successful
 * The MPD object should be valid.
 */

/*TEST_F(FunctionalTests, CDATA_Test)
{
	std::string fragmentUrl;
	AAMPStatusType status;
	dash::mpd::IMPD *mpd;
	static const char *manifest =
R"(<?xml version="1.0" encoding="utf-8"?>
<MPD xmlns="urn:mpeg:dash:schema:mpd:2011" minBufferTime="PT2S" type="static" mediaPresentationDuration="PT0H10M54.00S" profiles="urn:mpeg:dash:profile:isoff-live:2011,http://dashif.org/guidelines/dash264">
  <Period duration="PT1M0S">
   <EventStream schemeIdUri="urn:sva:advertising-wg:ad-id-signaling" timescale="90000">
            <Event duration="1358857" presentationTime="0"><![CDATA[{"version":1,"identifiers":[{"scheme":"urn:smpte:ul:060E2B34.01040101.01200900.00000000","value":"5493003","ad_position":"_PT0S_0","ad_type":"avail","tracking_uri":"../../../../../../../../../../tracking/99247e89c7677df85a85aabdd3256ffe02a60196/peacock-dash-vod-2s-generic/f38d0147-7bee-480f-83a7-fec49fda39b9","custom_vast_data":null}]}]]></Event>
        </EventStream>
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
	// Initialize MPD. The video initialization segment is cached. 
	std::string expectedCData = R"({"version":1,"identifiers":[{"scheme":"urn:smpte:ul:060E2B34.01040101.01200900.00000000","value":"5493003","ad_position":"_PT0S_0","ad_type":"avail","tracking_uri":"../../../../../../../../../../tracking/99247e89c7677df85a85aabdd3256ffe02a60196/peacock-dash-vod-2s-generic/f38d0147-7bee-480f-83a7-fec49fda39b9","custom_vast_data":null}]})";
	fragmentUrl = std::string(TEST_BASE_URL) + std::string("video_init.mp4");
	EXPECT_CALL(*g_mockMediaStreamContext, CacheFragment(fragmentUrl, _, _, _, _, true, _, _, _, _, _))
		.WillOnce(Return(true));

	status = InitializeMPD(manifest);
	//To verify the anticipated cdata against the cdata processed within the process node function.
	ASSERT_EQ(mResponse->mRootNode->GetText().c_str(), expectedCData);
	EXPECT_EQ(status, eAAMPSTATUS_OK);

	// Get the manifest data. 
	mpd = mStreamAbstractionAAMP_MPD->GetMPD();
	EXPECT_NE(mpd, nullptr);
	}*/
