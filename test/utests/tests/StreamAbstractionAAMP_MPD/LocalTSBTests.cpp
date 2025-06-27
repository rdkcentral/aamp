/*
* If not stated otherwise in this file or this component's license file the
* following copyright and licenses apply:
*
* Copyright 2025 RDK Management
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

#include "priv_aamp.h"

#include "AampConfig.h"
#include "AampScheduler.h"
#include "MockAampConfig.h"
#include "MockAampGstPlayer.h"
#include "MockPrivateInstanceAAMP.h"
#include "MockStreamAbstractionAAMP_MPD.h"
#include "MockTSBSessionManager.h"
#include "MockTSBReader.h"
#include "MockAampScheduler.h"
#include "MockAampMPDDownloader.h"


using ::testing::_;
using ::testing::WithParamInterface;
using ::testing::Return;
using ::testing::StrictMock;
using ::testing::NiceMock;
using ::testing::Invoke;
using ::testing::AnyNumber;

class LocalTSBTests : public ::testing::Test
{

protected:

	CDAIObject *mCdaiObj;

	static constexpr const char *TEST_MANIFEST_URL = "http://host/asset/manifest.mpd";
	const char *mManifest;

    ManifestDownloadResponsePtr mResponse = MakeSharedManifestDownloadResponsePtr();

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
			{eAAMPConfig_InterruptHandling, false}};

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

    class TestableStreamAbstractionAAMP_MPD : public StreamAbstractionAAMP_MPD
    {
    public:
            // Constructor to pass parameters to the base class constructor
            TestableStreamAbstractionAAMP_MPD(PrivateInstanceAAMP *aamp, double seekpos, float rate)
                        : StreamAbstractionAAMP_MPD(aamp, seekpos, rate)
            {
            }
    };

    PrivateInstanceAAMP *mPrivateInstanceAAMP;
    std::shared_ptr<AampTsbReader> mAampTsbReader;
   	std::shared_ptr<AampTsbDataManager> mDataMgr;
    TestableStreamAbstractionAAMP_MPD *mStreamAbstractionAAMP_MPD;

    void SetUp() override
	{
		if(gpGlobalConfig == nullptr)
		{
			gpGlobalConfig =  new AampConfig();
		}

   		mPrivateInstanceAAMP = new PrivateInstanceAAMP(gpGlobalConfig);
        mDataMgr = std::make_shared<AampTsbDataManager>();
        mAampTsbReader = std::make_shared<AampTsbReader>(mPrivateInstanceAAMP, mDataMgr, eMEDIATYPE_VIDEO, "");

		g_mockPrivateInstanceAAMP = new StrictMock<MockPrivateInstanceAAMP>();

        g_mockAampConfig = new NiceMock<MockAampConfig>();

		g_mockTSBSessionManager = new MockTSBSessionManager(mPrivateInstanceAAMP);

   		g_mockTSBReader = std::make_shared<MockTSBReader>();

		g_mockAampMPDDownloader = new StrictMock<MockAampMPDDownloader>();

        mStreamAbstractionAAMP_MPD = nullptr;

        //mPrivateInstanceAAMP->mStreamSink = g_mockAampGstPlayer; //TODO fix
		mBoolConfigSettings = mDefaultBoolConfigSettings;
		mIntConfigSettings = mDefaultIntConfigSettings;
   
        mCdaiObj = nullptr;
    }

	void TearDown() override
	{
        g_mockTSBReader.reset();

        delete mPrivateInstanceAAMP;
		mPrivateInstanceAAMP = nullptr;
        
        delete g_mockPrivateInstanceAAMP;
        g_mockPrivateInstanceAAMP = nullptr;

        delete g_mockTSBSessionManager;
		g_mockTSBSessionManager = nullptr;

        delete gpGlobalConfig;
		gpGlobalConfig = nullptr;

		delete g_mockAampConfig;
		g_mockAampConfig = nullptr;

   		delete g_mockAampMPDDownloader;
		g_mockAampMPDDownloader = nullptr;

        if (mCdaiObj)
        {
            delete mCdaiObj;
    		mCdaiObj = nullptr;
        }
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

    AAMPStatusType InitializeMPD(const char *manifest, TuneType tuneType = TuneType::eTUNETYPE_NEW_NORMAL, double seekPos = 0.0, float rate = AAMP_NORMAL_PLAY_RATE, bool isLive = false)
	{
		AAMPStatusType status = eAAMPSTATUS_OK;

		mManifest = manifest;

		/* Setup configuration mock. */
		for (const auto &b : mBoolConfigSettings)
		{
			EXPECT_CALL(*g_mockAampConfig, IsConfigSet(b.first))
				.WillRepeatedly(Return(b.second));
		}

		for (const auto &i : mIntConfigSettings)
		{
			EXPECT_CALL(*g_mockAampConfig, GetConfigValue(i.first))
				.WillRepeatedly(Return(i.second));
		}

		/* Create MPD instance. */
		mStreamAbstractionAAMP_MPD = new TestableStreamAbstractionAAMP_MPD(mPrivateInstanceAAMP, seekPos, rate);

		mCdaiObj = new CDAIObjectMPD(mPrivateInstanceAAMP);
		mStreamAbstractionAAMP_MPD->SetCDAIObject(mCdaiObj);

		mPrivateInstanceAAMP->SetManifestUrl(TEST_MANIFEST_URL);

		EXPECT_CALL(*g_mockAampMPDDownloader, GetManifest(_, _, _))
			.WillOnce(WithoutArgs(Invoke(this, &LocalTSBTests::GetManifestForMPDDownloader)));

		EXPECT_CALL(*g_mockPrivateInstanceAAMP, SetState(eSTATE_PREPARING));

        EXPECT_CALL(*g_mockPrivateInstanceAAMP, GetState())
			.Times(AnyNumber())
			.WillRepeatedly(Return(eSTATE_PREPARING));

        status = mStreamAbstractionAAMP_MPD->Init(tuneType);
        return status;
    }

};

TEST_F(LocalTSBTests, Setup)
{
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

    AAMPStatusType status = InitializeMPD(manifest);
	EXPECT_EQ(status, eAAMPSTATUS_OK);
}

TEST_F(LocalTSBTests, GetFirstPTS)
{
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

    AAMPStatusType status = InitializeMPD(manifest);
	EXPECT_EQ(status, eAAMPSTATUS_OK);

    MediaTrack *videoTrack = mStreamAbstractionAAMP_MPD->GetMediaTrack(eTRACK_VIDEO);
    ASSERT_NE(videoTrack, nullptr);
    videoTrack->SetLocalTSBInjection(true);

    EXPECT_CALL(*g_mockPrivateInstanceAAMP, GetTSBSessionManager()).WillRepeatedly(Return(g_mockTSBSessionManager));
    EXPECT_CALL(*g_mockTSBSessionManager, GetTsbReader(eMEDIATYPE_VIDEO)).WillRepeatedly(Return(mAampTsbReader));

    EXPECT_CALL(*g_mockTSBReader, GetFirstPTS()).WillOnce(Return(5.0));
    EXPECT_CALL(*g_mockTSBReader, GetFirstPTSOffset()).WillOnce(Return(10.0));
    
    EXPECT_EQ(15.0, mStreamAbstractionAAMP_MPD->GetFirstPTS());
}
