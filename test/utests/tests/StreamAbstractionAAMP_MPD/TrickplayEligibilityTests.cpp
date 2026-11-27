/*
 * If not stated otherwise in this file or this component's license file the
 * following copyright and licenses apply:
 *
 * Copyright 2026 RDK Management
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
#include <map>
#include <string>
#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include "priv_aamp.h"
#include "AampConfig.h"
#include "AampLogManager.h"
#include "fragmentcollector_mpd.h"
#include "MediaStreamContext.h"
#include "MockAampConfig.h"
#include "MockAampUtils.h"
#include "MockPrivateInstanceAAMP.h"
#include "MockMediaStreamContext.h"
#include "MockAampMPDDownloader.h"

using ::testing::_;
using ::testing::AnyNumber;
using ::testing::Invoke;
using ::testing::NiceMock;
using ::testing::Return;
using ::testing::StrictMock;
using ::testing::WithoutArgs;

namespace
{
	/** Lets a test enable local TSB in the configuration, which the player normally derives from it at tune. */
	class TsbConfigurablePrivateInstanceAAMP : public PrivateInstanceAAMP
	{
	public:
		explicit TsbConfigurablePrivateInstanceAAMP(AampConfig *config) : PrivateInstanceAAMP(config) {}

		/** @param[in] enabled Whether local TSB is enabled in the configuration */
		void SetLocalAAMPTsbFromConfig(bool enabled)
		{
			mLocalAAMPTsbFromConfig = enabled;
		}
	};

	constexpr const char *MPD_PREFIX = R"(<?xml version="1.0" encoding="utf-8"?>
<MPD xmlns="urn:mpeg:dash:schema:mpd:2011" profiles="urn:mpeg:dash:profile:isoff-live:2011" type="static" mediaPresentationDuration="PT2M0.0S" minBufferTime="PT4.0S">
	<Period id="0" start="PT0.0S">)";

	constexpr const char *VIDEO_SET = R"(
		<AdaptationSet id="1" contentType="video">
			<Representation id="0" mimeType="video/mp4" codecs="avc1.640028" bandwidth="800000" width="640" height="360" frameRate="25">
				<SegmentTemplate timescale="12800" initialization="h264/video_init.mp4" media="h264/video_$Number$.m4s" startNumber="1">
					<SegmentTimeline>
						<S t="0" d="25600" r="59"/>
					</SegmentTimeline>
				</SegmentTemplate>
			</Representation>
		</AdaptationSet>)";

	constexpr const char *IFRAME_SET = R"(
		<AdaptationSet id="3" contentType="video">
			<EssentialProperty schemeIdUri="http://dashif.org/guidelines/trickmode" value="1"/>
			<Representation id="2" mimeType="video/mp4" codecs="avc1.640028" bandwidth="200000" width="640" height="360" frameRate="1">
				<SegmentTemplate timescale="12800" initialization="h264/iframe_init.mp4" media="h264/iframe_$Number$.m4s" startNumber="1">
					<SegmentTimeline>
						<S t="0" d="25600" r="59"/>
					</SegmentTimeline>
				</SegmentTemplate>
			</Representation>
		</AdaptationSet>)";

	constexpr const char *AUDIO_SET = R"(
		<AdaptationSet id="2" contentType="audio" lang="eng">
			<Representation id="1" mimeType="audio/mp4" codecs="mp4a.40.2" bandwidth="64000" audioSamplingRate="48000">
				<SegmentTemplate timescale="48000" initialization="aac/audio_init.mp4" media="aac/audio_$Number$.m4s" startNumber="1">
					<SegmentTimeline>
						<S t="0" d="96000" r="59"/>
					</SegmentTimeline>
				</SegmentTemplate>
			</Representation>
		</AdaptationSet>)";

	constexpr const char *MPD_SUFFIX = R"(
	</Period>
</MPD>
)";

	/** The content of a single-period VOD manifest, how iframes may be derived, and the expected answer. */
	struct TrickplayEligibilityParams
	{
		const char *name;
		bool hasVideo;
		bool hasIframeSet;
		bool synthesisEnabled; ///< VOD iframe synthesis configured
		bool localTsbExtraction; ///< Local TSB and iframe extraction configured
		bool expectTrickplay;
	};

	/**
	 * @brief Initialises the DASH collector on a single-period VOD manifest with audio, and optionally video
	 *        and an iframe adaptation set, then reads the trickplay support it reports.
	 */
	class TrickplayEligibilityTests : public ::testing::TestWithParam<TrickplayEligibilityParams>
	{
	public:
		void SetUp() override
		{
			if (gpGlobalConfig == nullptr)
			{
				gpGlobalConfig = new AampConfig();
			}
			mPrivateInstanceAAMP = new TsbConfigurablePrivateInstanceAAMP(gpGlobalConfig);
			g_mockAampConfig = std::make_shared<NiceMock<MockAampConfig>>();
			mPrivateInstanceAAMP->mIsDefaultOffset = true;
			g_mockPrivateInstanceAAMP = std::make_shared<NiceMock<MockPrivateInstanceAAMP>>();
			g_mockMediaStreamContext = std::make_shared<StrictMock<MockMediaStreamContext>>();
			g_mockAampMPDDownloader = std::make_shared<StrictMock<MockAampMPDDownloader>>();
		}

		void TearDown() override
		{
			mPrivateInstanceAAMP->GetAampTrackWorkerManager()->RemoveWorkers();
			delete mCollector;
			mCollector = nullptr;
			delete mPrivateInstanceAAMP;
			mPrivateInstanceAAMP = nullptr;
			delete mCdaiObj;
			mCdaiObj = nullptr;
			delete gpGlobalConfig;
			gpGlobalConfig = nullptr;
			g_mockAampConfig.reset();
			g_mockPrivateInstanceAAMP.reset();
			g_mockMediaStreamContext.reset();
			g_mockAampMPDDownloader.reset();
		}

		/** @return the manifest under test, served to the collector in place of a download */
		ManifestDownloadResponsePtr GetManifestForMPDDownloader()
		{
			ManifestDownloadResponsePtr response = MakeSharedManifestDownloadResponsePtr();
			response->mMPDStatus = AAMPStatusType::eAAMPSTATUS_OK;
			response->mMPDDownloadResponse->iHttpRetValue = 200;
			response->mMPDDownloadResponse->sEffectiveUrl = std::string(TEST_MANIFEST_URL);
			response->mMPDDownloadResponse->mDownloadData.assign(mManifest.begin(), mManifest.end());

			xmlTextReaderPtr reader = xmlReaderForMemory(mManifest.c_str(), static_cast<int>(mManifest.length()), NULL, NULL, 0);
			if (reader != NULL)
			{
				if (xmlTextReaderRead(reader))
				{
					response->mRootNode = MPDProcessNode(&reader, TEST_MANIFEST_URL);
					if (response->mRootNode != NULL)
					{
						dash::mpd::MPD *mpd = response->mRootNode->ToMPD();
						if (mpd)
						{
							std::shared_ptr<dash::mpd::IMPD> mpdPtr(mpd);
							response->mMPDInstance = mpdPtr;
							response->GetMPDParseHelper()->Initialize(mpd);
						}
					}
				}
			}
			xmlFreeTextReader(reader);
			return response;
		}

		/**
		 * @brief Create the collector and initialise it on the manifest the parameters describe
		 * @param[in] params Manifest content and iframe configuration
		 * @return eAAMPSTATUS_OK on success or another value on error
		 */
		AAMPStatusType InitializeMPD(const TrickplayEligibilityParams &params)
		{
			mManifest = MPD_PREFIX;
			mManifest += params.hasVideo ? VIDEO_SET : "";
			mManifest += params.hasIframeSet ? IFRAME_SET : "";
			mManifest += AUDIO_SET;
			mManifest += MPD_SUFFIX;

			// Once IsConfigSet has an expectation, every flag the collector reads must be listed
			const std::map<AAMPConfigSettingBool, bool> boolConfigSettings = {
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
				{eAAMPConfig_DisableTTML, false},
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
				{eAAMPConfig_EnableIFrameTrackExtract, params.localTsbExtraction},
				{eAAMPConfig_SynthesizeIframeForVOD, params.synthesisEnabled},
				{eAAMPConfig_GstSubtecEnabled, false},
				{eAAMPConfig_useRialtoSink, false},
				{eAAMPConfig_useDirectRialto, false},
				{eAAMPConfig_UseMp4Demux, false},
				{eAAMPConfig_ProcessLicenseFromEAP, false},
			};
			for (const auto &setting : boolConfigSettings)
			{
				EXPECT_CALL(*g_mockAampConfig, IsConfigSet(setting.first))
					.Times(AnyNumber())
					.WillRepeatedly(Return(setting.second));
			}
			mPrivateInstanceAAMP->SetLocalAAMPTsbFromConfig(params.localTsbExtraction);

			mPrivateInstanceAAMP->rate = AAMP_NORMAL_PLAY_RATE;
			mCollector = new StreamAbstractionAAMP_MPD(mPrivateInstanceAAMP, 0.0, AAMP_NORMAL_PLAY_RATE);
			mCdaiObj = new CDAIObjectMPD(mPrivateInstanceAAMP);
			mCollector->SetCDAIObject(mCdaiObj);
			mPrivateInstanceAAMP->SetManifestUrl(TEST_MANIFEST_URL);

			EXPECT_CALL(*g_mockPrivateInstanceAAMP, SetState(eSTATE_PREPARING, true));
			EXPECT_CALL(*g_mockPrivateInstanceAAMP, GetState())
				.Times(AnyNumber())
				.WillRepeatedly(Return(eSTATE_PREPARING));
			EXPECT_CALL(*g_mockPrivateInstanceAAMP, IsLiveStream())
				.Times(AnyNumber())
				.WillRepeatedly(Return(false));
			EXPECT_CALL(*g_mockPrivateInstanceAAMP, GetLLDashChunkMode()).WillRepeatedly(Return(false));
			EXPECT_CALL(*g_mockPrivateInstanceAAMP, SetLLDashChunkMode(_));
			EXPECT_CALL(*g_mockAampMPDDownloader, GetManifest(_, _, _))
				.WillOnce(WithoutArgs(Invoke(this, &TrickplayEligibilityTests::GetManifestForMPDDownloader)));
			EXPECT_CALL(*g_mockMediaStreamContext, CacheFragment(_, _, _, _, _, _, _, _, _))
				.WillRepeatedly(Return(true));
			return mCollector->Init(TuneType::eTUNETYPE_NEW_NORMAL);
		}

		static constexpr const char *TEST_MANIFEST_URL = "http://host/asset/manifest.mpd";
		TsbConfigurablePrivateInstanceAAMP *mPrivateInstanceAAMP = nullptr;
		StreamAbstractionAAMP_MPD *mCollector = nullptr;
		CDAIObject *mCdaiObj = nullptr;
		std::string mManifest;
	};
}

/**
 * @brief Trick play is reported as supported only when there is an iframe source and a video track.
 *
 * Oracle: trick play is video-only, so the iframe source is an iframe adaptation set, or a video
 * adaptation set that VOD synthesis or local TSB extraction derives iframes from. Audio-only content
 * is carried in the video slot, but has no video adaptation set, so it never supports trick play,
 * whatever is configured. The reported support decides which speeds are advertised and whether
 * SetRate accepts a trick rate.
 */
TEST_P(TrickplayEligibilityTests, StreamSelection_ContentAndIframeSource_ReportsTrickplaySupport)
{
	const TrickplayEligibilityParams &params = GetParam();

	ASSERT_EQ(InitializeMPD(params), eAAMPSTATUS_OK);

	EXPECT_EQ(mPrivateInstanceAAMP->mIsIframeTrackPresent, params.expectTrickplay);
}

INSTANTIATE_TEST_SUITE_P(ContentAndIframeSources, TrickplayEligibilityTests,
	::testing::Values(
		TrickplayEligibilityParams{"VideoWithIframeSet", true, true, false, false, true},
		TrickplayEligibilityParams{"VideoWithoutIframeSource", true, false, false, false, false},
		TrickplayEligibilityParams{"VideoWithSynthesis", true, false, true, false, true},
		TrickplayEligibilityParams{"VideoWithLocalTsbExtraction", true, false, false, true, true},
		TrickplayEligibilityParams{"AudioOnly", false, false, false, false, false},
		TrickplayEligibilityParams{"AudioOnlyWithSynthesis", false, false, true, false, false},
		TrickplayEligibilityParams{"AudioOnlyWithLocalTsbExtraction", false, false, false, true, false}),
	[](const ::testing::TestParamInfo<TrickplayEligibilityParams> &info)
	{
		return std::string{info.param.name};
	});
