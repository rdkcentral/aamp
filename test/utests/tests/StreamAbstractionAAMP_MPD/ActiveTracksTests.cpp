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

/**
 * @brief Prints a set as its member media types, e.g. {0, 2}, in failure messages.
 * @param[in] set The set to print
 * @param[out] os Stream to print to
 */
inline void PrintTo(const AampTrackSet &set, std::ostream *os)
{
	*os << "{";
	const char *separator = "";
	for (const AampMediaType type : set)
	{
		*os << separator << static_cast<int>(type);
		separator = ", ";
	}
	*os << "}";
}

namespace
{
	/** Exposes the protected track-selection state and queries of the collector. */
	class TrackSelectionMPD : public StreamAbstractionAAMP_MPD
	{
	public:
		TrackSelectionMPD(PrivateInstanceAAMP *aamp, double seekPos, float rate)
			: StreamAbstractionAAMP_MPD(aamp, seekPos, rate) {}

		/** @return the tracks the collector selected for the current period */
		AampTrackSet PublicActiveTracks() const
		{
			return mActiveTracks.Load();
		}

		/**
		 * @param[in] mediaType Track whose current representation is queried
		 * @return MIME type of the track's current representation, empty if the track is not active
		 */
		std::string PublicGetCurrentMimeType(AampMediaType mediaType)
		{
			return GetCurrentMimeType(mediaType);
		}

		/** @return true if encrypted init segments were found; mappedHeaders is keyed by media type */
		bool PublicGetEncryptedHeaders(std::map<int, std::string> &mappedHeaders)
		{
			return GetEncryptedHeaders(mappedHeaders);
		}
	};

	/**
	 * @brief Initialises the DASH collector on a single-period VOD manifest and inspects its track selection.
	 */
	class TrackSelectionTests : public ::testing::Test
	{
	public:
		void SetUp() override
		{
			if (gpGlobalConfig == nullptr)
			{
				gpGlobalConfig = new AampConfig();
			}
			mPrivateInstanceAAMP = new PrivateInstanceAAMP(gpGlobalConfig);
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
			mManifest = nullptr;
		}

		/** @return the manifest under test, served to the collector in place of a download */
		ManifestDownloadResponsePtr GetManifestForMPDDownloader()
		{
			ManifestDownloadResponsePtr response = MakeSharedManifestDownloadResponsePtr();
			response->mMPDStatus = AAMPStatusType::eAAMPSTATUS_OK;
			response->mMPDDownloadResponse->iHttpRetValue = 200;
			response->mMPDDownloadResponse->sEffectiveUrl = std::string(TEST_MANIFEST_URL);
			response->mMPDDownloadResponse->mDownloadData.assign(mManifest, mManifest + strlen(mManifest));

			std::string manifestStr(response->mMPDDownloadResponse->mDownloadData.begin(),
				response->mMPDDownloadResponse->mDownloadData.end());
			xmlTextReaderPtr reader = xmlReaderForMemory(manifestStr.c_str(), static_cast<int>(manifestStr.length()), NULL, NULL, 0);
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
		 * @brief Create the collector and initialise it on a manifest
		 * @param[in] manifest Manifest text; must outlive the call
		 * @return eAAMPSTATUS_OK on success or another value on error
		 */
		AAMPStatusType InitializeMPD(const char *manifest)
		{
			mManifest = manifest;
			// Once IsConfigSet has an expectation, every flag the collector reads must be listed
			static const std::map<AAMPConfigSettingBool, bool> boolConfigSettings = {
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
				{eAAMPConfig_EnableIFrameTrackExtract, false},
				{eAAMPConfig_SynthesizeIframeForVOD, false},
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

			mPrivateInstanceAAMP->rate = AAMP_NORMAL_PLAY_RATE;
			mCollector = new TrackSelectionMPD(mPrivateInstanceAAMP, 0.0, AAMP_NORMAL_PLAY_RATE);
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
				.WillOnce(WithoutArgs(Invoke(this, &TrackSelectionTests::GetManifestForMPDDownloader)));
			EXPECT_CALL(*g_mockPrivateInstanceAAMP, ResumeTrackDownloads(eMEDIATYPE_SUBTITLE))
				.Times(AnyNumber());
			return mCollector->Init(TuneType::eTUNETYPE_NEW_NORMAL);
		}

		static constexpr const char *TEST_MANIFEST_URL = "http://host/asset/manifest.mpd";
		PrivateInstanceAAMP *mPrivateInstanceAAMP = nullptr;
		TrackSelectionMPD *mCollector = nullptr;
		CDAIObject *mCdaiObj = nullptr;
		const char *mManifest = nullptr;
	};

	/** Manifest fragments that ActiveTracksTests combine into one period. */
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

	constexpr const char *SUBTITLE_SET = R"(
		<AdaptationSet id="3" contentType="text" segmentAlignment="true" lang="eng">
			<Role schemeIdUri="urn:mpeg:dash:role:2011" value="caption"/>
			<Representation id="2" mimeType="application/mp4" codecs="stpp" bandwidth="400">
				<SegmentTemplate timescale="48000" initialization="dash/ttml_init.mp4" media="dash/ttml_en_$Number%03d$.mp4" startNumber="1">
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

	/** Which adaptation sets the manifest holds, and what the collector must make of them. */
	struct ActiveTracksParams
	{
		const char *name;
		bool hasVideo;
		bool hasAudio;
		bool hasSubtitle;
		AampTrackSet expectedActive;
		const char *expectedVideoSlotMime; ///< Content carried by the video slot (empty when the slot is unused)
		bool expectAudioOnlyPb; ///< Whether the tune is flagged as audio-only playback
	};

	/**
	 * @brief Builds a single-period manifest holding the adaptation sets the parameters ask for.
	 * @param[in] params Which of video, audio and subtitle the manifest contains
	 * @return the manifest text
	 */
	std::string ManifestFor(const ActiveTracksParams &params)
	{
		std::string manifest{MPD_PREFIX};
		manifest += params.hasVideo ? VIDEO_SET : "";
		manifest += params.hasAudio ? AUDIO_SET : "";
		manifest += params.hasSubtitle ? SUBTITLE_SET : "";
		manifest += MPD_SUFFIX;
		return manifest;
	}

	/**
	 * @brief Selects the tracks of a single-period VOD manifest holding any mix of video, audio and subtitle.
	 */
	class ActiveTracksTests : public TrackSelectionTests, public ::testing::WithParamInterface<ActiveTracksParams>
	{
	};
}

/**
 * @brief The tracks selected for a period are exactly the ones whose adaptation sets exist.
 *
 * Oracle: a track is active when the manifest offers it, so the active set and the
 * per-type MIME type lookups must agree with the manifest contents. Audio-only content is
 * the one deliberate exception: the audio is carried in the video slot, so the video slot is
 * active, reports the audio MIME type, and the audio slot is not active.
 *
 * Video plus subtitle and audio plus subtitle (no video) are the regression cases: the subtitle
 * slot is not adjacent to the occupied slots, so a count of active tracks cannot express it. Audio
 * plus subtitle is not audio-only, so no swap happens and the video slot stays unused.
 */
TEST_P(ActiveTracksTests, StreamSelection_TrackMix_ActivatesExpectedTracks)
{
	const ActiveTracksParams &params = GetParam();
	const std::string manifest = ManifestFor(params);
	EXPECT_CALL(*g_mockMediaStreamContext, CacheFragment(_, _, _, _, _, _, _, _, _))
		.WillRepeatedly(Return(true));
	if (params.expectedActive.Contains(eMEDIATYPE_SUBTITLE))
	{
		// A selected subtitle track has its initialization segment fetched while the tune is
		// initialised (the 6th CacheFragment argument, true, marks an initialization segment)
		EXPECT_CALL(*g_mockMediaStreamContext, CacheFragment(_, eCURLINSTANCE_SUBTITLE, _, _, _, true, _, _, _))
			.Times(1)
			.WillRepeatedly(Return(true));
	}
	else
	{
		EXPECT_CALL(*g_mockMediaStreamContext, CacheFragment(_, eCURLINSTANCE_SUBTITLE, _, _, _, true, _, _, _))
			.Times(0);
	}

	const AAMPStatusType status = InitializeMPD(manifest.c_str());

	ASSERT_EQ(status, eAAMPSTATUS_OK);
	// Deliberate white-box check: the audio-only slot move is only visible in the active set
	EXPECT_EQ(mCollector->PublicActiveTracks(), params.expectedActive);
	for (const AampMediaType type : {eMEDIATYPE_VIDEO, eMEDIATYPE_AUDIO, eMEDIATYPE_SUBTITLE})
	{
		SCOPED_TRACE(GetMediaTypeName(type));
		const std::string mime = mCollector->PublicGetCurrentMimeType(type);
		if (params.expectedActive.Contains(type))
		{
			EXPECT_FALSE(mime.empty());
		}
		else
		{
			EXPECT_TRUE(mime.empty());
		}
	}
	// The video slot carries the audio for audio-only content, and is unused without video or the swap
	EXPECT_EQ(mCollector->PublicGetCurrentMimeType(eMEDIATYPE_VIDEO),
		std::string{params.expectedVideoSlotMime});
	EXPECT_EQ(mPrivateInstanceAAMP->mAudioOnlyPb, params.expectAudioOnlyPb);
}

INSTANTIATE_TEST_SUITE_P(TrackMixes, ActiveTracksTests,
	::testing::Values(
		ActiveTracksParams{"VideoOnly", true, false, false, AampTrackSet{eMEDIATYPE_VIDEO}, "video/mp4", false},
		ActiveTracksParams{"VideoAndAudio", true, true, false, AampTrackSet{eMEDIATYPE_VIDEO, eMEDIATYPE_AUDIO}, "video/mp4", false},
		ActiveTracksParams{"VideoAudioAndSubtitle", true, true, true, AampTrackSet{eMEDIATYPE_VIDEO, eMEDIATYPE_AUDIO, eMEDIATYPE_SUBTITLE}, "video/mp4", false},
		ActiveTracksParams{"VideoAndSubtitleNoAudio", true, false, true, AampTrackSet{eMEDIATYPE_VIDEO, eMEDIATYPE_SUBTITLE}, "video/mp4", false},
		ActiveTracksParams{"AudioAndSubtitleNoVideo", false, true, true, AampTrackSet{eMEDIATYPE_AUDIO, eMEDIATYPE_SUBTITLE}, "", false},
		ActiveTracksParams{"AudioOnlyIsCarriedInVideoSlot", false, true, false, AampTrackSet{eMEDIATYPE_VIDEO}, "audio/mp4", true}),
	[](const ::testing::TestParamInfo<ActiveTracksParams> &info)
	{
		return std::string{info.param.name};
	});

/**
 * @brief Encrypted initialization headers are looked up for every selected, protected track.
 *
 * Oracle: each protected track contributes one header URL, stored under its own media type, built
 * from that track's initialization template with the bandwidth of the representation chosen for it
 * (the URL template uses $Bandwidth$, so a header built without the bandwidth would be wrong).
 */
TEST_F(TrackSelectionTests, GetEncryptedHeaders_VideoAndAudioProtected_ReturnsHeaderPerTrackWithItsBandwidth)
{
	const std::string manifest = R"(<?xml version="1.0" encoding="utf-8"?>
<MPD xmlns="urn:mpeg:dash:schema:mpd:2011" profiles="urn:mpeg:dash:profile:isoff-live:2011" type="static" mediaPresentationDuration="PT2M0.0S" minBufferTime="PT4.0S">
	<Period id="0" start="PT0.0S">
		<AdaptationSet id="1" contentType="video">
			<ContentProtection schemeIdUri="urn:mpeg:dash:mp4protection:2011" value="cenc"/>
			<Representation id="0" mimeType="video/mp4" codecs="avc1.640028" bandwidth="800000" width="640" height="360" frameRate="25">
				<SegmentTemplate timescale="12800" initialization="video_init_$Bandwidth$.mp4" media="video_$Number$.m4s" startNumber="1">
					<SegmentTimeline>
						<S t="0" d="25600" r="59"/>
					</SegmentTimeline>
				</SegmentTemplate>
			</Representation>
		</AdaptationSet>
		<AdaptationSet id="2" contentType="audio" lang="eng">
			<ContentProtection schemeIdUri="urn:mpeg:dash:mp4protection:2011" value="cenc"/>
			<Representation id="1" mimeType="audio/mp4" codecs="mp4a.40.2" bandwidth="64000" audioSamplingRate="48000">
				<SegmentTemplate timescale="48000" initialization="audio_init_$Bandwidth$.mp4" media="audio_$Number$.m4s" startNumber="1">
					<SegmentTimeline>
						<S t="0" d="96000" r="59"/>
					</SegmentTimeline>
				</SegmentTemplate>
			</Representation>
		</AdaptationSet>
	</Period>
</MPD>
)";
	EXPECT_CALL(*g_mockMediaStreamContext, CacheFragment(_, _, _, _, _, _, _, _, _))
		.WillRepeatedly(Return(true));
	ASSERT_EQ(InitializeMPD(manifest.c_str()), eAAMPSTATUS_OK);

	std::map<int, std::string> headers;
	const bool found = mCollector->PublicGetEncryptedHeaders(headers);

	EXPECT_TRUE(found);
	ASSERT_EQ(headers.size(), 2u);
	ASSERT_EQ(headers.count(eMEDIATYPE_VIDEO), 1u);
	ASSERT_EQ(headers.count(eMEDIATYPE_AUDIO), 1u);
	EXPECT_NE(headers[eMEDIATYPE_VIDEO].find("video_init_800000.mp4"), std::string::npos) << headers[eMEDIATYPE_VIDEO];
	EXPECT_NE(headers[eMEDIATYPE_AUDIO].find("audio_init_64000.mp4"), std::string::npos) << headers[eMEDIATYPE_AUDIO];
}
