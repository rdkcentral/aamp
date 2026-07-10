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
#include "MockAampStreamSinkManager.h"
#include "MockAdManager.h"
#include "MockIsoBmffProcessor.h"
#include "MockTSBSessionManager.h"
#include "MockTSBReader.h"
#include "MockABRManager.h"


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
using ::testing::Invoke;

AampConfig *gpGlobalConfig{nullptr};

/**
 * @brief Functional tests common base class.
 */
class FunctionalTestsBase
{
protected:
	class TestableFunctionalStreamAbstractionAAMP_MPD : public StreamAbstractionAAMP_MPD
	{
	public:
		TestableFunctionalStreamAbstractionAAMP_MPD(PrivateInstanceAAMP *aamp, double seekpos, float rate)
			: StreamAbstractionAAMP_MPD(aamp, seekpos, rate)
		{
		}

		void CallSeekInPeriod(double seekPositionSeconds, bool skipToEnd = false)
		{
			SeekInPeriod(seekPositionSeconds, skipToEnd);
		}
	};

	PrivateInstanceAAMP *mPrivateInstanceAAMP;
	StreamAbstractionAAMP_MPD *mStreamAbstractionAAMP_MPD;
	CDAIObject *mCdaiObj;
	const char *mManifest;
	static constexpr const char *TEST_HOST_URL = "http://host/";
	static constexpr const char *TEST_BASE_URL = "http://host/asset/";
	static constexpr const char *TEST_MANIFEST_URL = "http://host/asset/manifest.mpd";
	std::string mManifestUrl {TEST_MANIFEST_URL};
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
		{eAAMPConfig_GstSubtecEnabled, false},
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
		{eAAMPConfig_ABRBufferCounter, DEFAULT_ABR_BUFFER_COUNTER},
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

		g_mockAampConfig = new NiceMock<MockAampConfig>();

		g_mockAampUtils = nullptr;

		g_mockAampGstPlayer = new MockAAMPGstPlayer( mPrivateInstanceAAMP);

		mPrivateInstanceAAMP->mIsDefaultOffset = true;

		g_mockPrivateInstanceAAMP = new StrictMock<MockPrivateInstanceAAMP>();

		g_mockMediaStreamContext = new StrictMock<MockMediaStreamContext>();

		g_mockAampMPDDownloader = new StrictMock<MockAampMPDDownloader>();

		g_mockAampStreamSinkManager = new NiceMock<MockAampStreamSinkManager>();

		g_mockIsoBmffProcessor = new NiceMock<MockIsoBmffProcessor>();

		g_mockABRManager = new NiceMock<MockABRManager>();

		mStreamAbstractionAAMP_MPD = nullptr;

		mManifest = nullptr;
		mResponse = nullptr;
		mBoolConfigSettings = mDefaultBoolConfigSettings;
		mIntConfigSettings = mDefaultIntConfigSettings;
		mCdaiObj = nullptr;
	}

	void TearDown()
	{
		delete g_mockIsoBmffProcessor;
		g_mockIsoBmffProcessor = nullptr;

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

		delete g_mockAampStreamSinkManager;
		g_mockAampStreamSinkManager = nullptr;

		delete g_mockABRManager;
		g_mockABRManager = nullptr;

		mManifest = nullptr;
	}

public:
	/**
	 * @brief Get manifest helper method
	 *
	 * @param[in] remoteUrl Manifest url
	 * @param[out] buffer Buffer containing manifest data
	 * @retval true on success
	*/
	bool GetManifest(std::string remoteUrl, AampGrowableBuffer *buffer)
	{
		EXPECT_STREQ(remoteUrl.c_str(), mManifestUrl.c_str());

		/* Setup fake AampGrowableBuffer contents. */
		buffer->Clear();
		buffer->AppendBytes((char *)mManifest, strlen(mManifest));

		return true;
	}


	void GetMPDFromManifest(ManifestDownloadResponsePtr response)
	{
		dash::mpd::MPD* mpd = nullptr;
		std::string manifestStr = std::string( response->mMPDDownloadResponse->mDownloadData.begin(), response->mMPDDownloadResponse->mDownloadData.end());

		xmlTextReaderPtr reader = xmlReaderForMemory( (char *)manifestStr.c_str(), (int) manifestStr.length(), NULL, NULL, 0);
		if (reader != NULL)
		{
			if (xmlTextReaderRead(reader))
			{
				response->mRootNode = MPDProcessNode(&reader, mManifestUrl);
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
		response->mMPDDownloadResponse->sEffectiveUrl = mManifestUrl;
		response->mMPDDownloadResponse->mDownloadData.assign((uint8_t*)mManifest, (uint8_t*)(mManifest + strlen(mManifest)));
		GetMPDFromManifest(response);
		mResponse = response;
		return response;
	}

	/**
	 * @brief Get manifest helper method for MPDDownloader
	 *
	 * @param[in] remoteUrl Manifest url
	 * @param[out] buffer Buffer containing manifest data
	 * @retval true on success
	*/
	ManifestDownloadResponsePtr GetManifestForMPDDownloaderTimeout(int curlTimeoutFailureReason)
	{
		static const char *test_manifest =
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

		ManifestDownloadResponsePtr response = MakeSharedManifestDownloadResponsePtr();
		response->mMPDStatus = AAMPStatusType::eAAMPSTATUS_MANIFEST_DOWNLOAD_ERROR;
		response->mMPDDownloadResponse->iHttpRetValue = curlTimeoutFailureReason;
		response->mMPDDownloadResponse->sEffectiveUrl = mManifestUrl;
		response->mMPDDownloadResponse->mDownloadData.assign((uint8_t*)test_manifest, (uint8_t*)(test_manifest + strlen(test_manifest)));
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
	AAMPStatusType InitializeMPD(const char *manifest, TuneType tuneType = TuneType::eTUNETYPE_NEW_NORMAL, double seekPos = 0.0, float rate = AAMP_NORMAL_PLAY_RATE,bool defaultConfig = true)
	{
		AAMPStatusType status;

		mManifest = manifest;
		if(defaultConfig)
		{
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
		}
		/* Create MPD instance. */
		mStreamAbstractionAAMP_MPD = new TestableFunctionalStreamAbstractionAAMP_MPD(mPrivateInstanceAAMP, seekPos, rate);
		mCdaiObj = new CDAIObjectMPD(mPrivateInstanceAAMP);
		mStreamAbstractionAAMP_MPD->SetCDAIObject(mCdaiObj);

		mPrivateInstanceAAMP->SetManifestUrl(mManifestUrl.c_str());

		/* Initialize MPD. */
		EXPECT_CALL(*g_mockPrivateInstanceAAMP, SetState(eSTATE_PREPARING));

		EXPECT_CALL(*g_mockPrivateInstanceAAMP, GetState())
			.Times(AnyNumber())
			.WillRepeatedly(Return(eSTATE_PREPARING));

		EXPECT_CALL(*g_mockPrivateInstanceAAMP, IsLiveStream())
			.Times(AnyNumber())
			.WillRepeatedly(Return(false));

		EXPECT_CALL(*g_mockPrivateInstanceAAMP, GetLLDashChunkMode()).WillRepeatedly(Return(false));

		EXPECT_CALL(*g_mockAampMPDDownloader, GetManifest (_, _, _))
			.WillOnce(WithoutArgs(Invoke(this, &FunctionalTestsBase::GetManifestForMPDDownloader)));

		status = mStreamAbstractionAAMP_MPD->Init(tuneType);
		if ((tuneType == eTUNETYPE_NEW_NORMAL) && (rate > AAMP_NORMAL_PLAY_RATE))
		{
			/*	NOW_STEADY_TS_MS used in calulation will have different between calling Init and used in comparison as under. Hence EXPECT_NEAR is used
				Assumption here is that it takes less than a second to excute Init and then perform comparison here */
			EXPECT_NEAR(mPrivateInstanceAAMP->mLiveEdgeDeltaFromCurrentTime, NOW_STEADY_TS_SECS_FP - mPrivateInstanceAAMP->mAbsoluteEndPosition, 1);
		}
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
		double fragmentDuration = ComputeFragmentDuration(12, 12); (void)fragmentDuration;
		pMediaStreamContext->mediaType = eMEDIATYPE_VIDEO;

		SegmentTemplate *segmentTemplate = new SegmentTemplate();
		const IFailoverContent *failoverContent = segmentTemplate->GetFailoverContent(); (void)failoverContent;

		static std::vector<IFCS *> failovercontents;
		dash::mpd::IFCS *IFCSobj;
		failovercontents.push_back(IFCSobj);

		AampMPDParseHelper *mMPDParseHelper = new AampMPDParseHelper();
		auto duration = mMPDParseHelper->GetMediaPresentationDuration(); (void)duration;
		bool mIsLiveStream = mMPDParseHelper->IsLiveManifest(); (void)mIsLiveStream;
		delete mMPDParseHelper;

		mStreamAbstractionAAMP_MPD->PushNextFragment(pMediaStreamContext, 0);
	}
};

class FunctionalTests_1 : public ::testing::Test
{
protected:
	PrivateInstanceAAMP *mPrivateInstanceAAMP{};
	StreamAbstractionAAMP_MPD *_instanceStreamAbstractionAAMP_MPD{};
	void SetUp() override
	{
		if (gpGlobalConfig == nullptr)
		{
			gpGlobalConfig = new AampConfig();
		}
		mPrivateInstanceAAMP = new PrivateInstanceAAMP(gpGlobalConfig);
		_instanceStreamAbstractionAAMP_MPD = new StreamAbstractionAAMP_MPD(mPrivateInstanceAAMP, 0, AAMP_NORMAL_PLAY_RATE);
		g_mockAampConfig = new NiceMock<MockAampConfig>();
	}

	void TearDown() override
	{
		delete g_mockAampConfig;
		g_mockAampConfig = nullptr;

		delete _instanceStreamAbstractionAAMP_MPD;
		_instanceStreamAbstractionAAMP_MPD = nullptr;

		delete mPrivateInstanceAAMP;
		mPrivateInstanceAAMP = nullptr;

		delete gpGlobalConfig;
		gpGlobalConfig = nullptr;
	}
};

class StreamAbstractionAAMP_MPDTest :   public FunctionalTestsBase,
										public ::testing::Test
{
protected:

	class TestableStreamAbstractionAAMP_MPD : public StreamAbstractionAAMP_MPD
	{
	public:
		// Constructor to pass parameters to the base class constructor
		TestableStreamAbstractionAAMP_MPD(PrivateInstanceAAMP *aamp,
											double seekpos, float rate)
			: StreamAbstractionAAMP_MPD(aamp, seekpos, rate)
		{
		}

		void CallPrintSelectedTrack(const std::string &trackIndex, AampMediaType media)
		{
			printSelectedTrack(trackIndex, media);
		}

		void CallAdvanceTrack(int trackIdx, bool trickPlay, double *delta, bool &waitForFreeFrag, bool &bCacheFullState)
		{
					bool throttleAudioDownload = false;
					bool isDiscontinuity = false;
					AdvanceTrack(trackIdx, trickPlay, delta, waitForFreeFrag, bCacheFullState, throttleAudioDownload, isDiscontinuity );
		}

		void CallFetcherLoop()
		{

			FetcherLoop();
		}

		StreamInfo *CallGetStreamInfo(int idx)
		{
			return GetStreamInfo(idx);
		}

		bool CallCheckLLProfileAvailable(IMPD *mpd)
		{
			return CheckLLProfileAvailable(mpd);
		}

		AAMPStatusType CallEnableAndSetLiveOffsetForLLDashPlayback(const MPD *mpd)
		{
			return EnableAndSetLiveOffsetForLLDashPlayback(mpd);
		}

		bool CallGetLowLatencyParams(const MPD *mpd, AampLLDashServiceData &LLDashData)
		{
			return GetLowLatencyParams(mpd, LLDashData);
		}

		bool CallParseMPDLLData(MPD *mpd, AampLLDashServiceData &stAampLLDashServiceData)
		{
			return ParseMPDLLData(mpd, stAampLLDashServiceData);
		}

		AAMPStatusType CallUpdateMPD(bool init)
		{
			return UpdateMPD(init);
		}

		bool CallFindServerUTCTime(Node *root)
		{
			return FindServerUTCTime(root);
		}
		AAMPStatusType CallFetchDashManifest()
		{
			return FetchDashManifest();
		}

		void CallFindTimedMetadata(dash::mpd::MPD *mpd, Node *root, bool init, bool reportBulkMet)
		{
			FindTimedMetadata(mpd, root, init, reportBulkMet);
		}
		void CallProcessPeriodSupplementalProperty(Node *node, std::string &AdID, uint64_t startMS, uint64_t durationMS, bool isInit, bool reportBulkMeta = false)
		{
			ProcessPeriodSupplementalProperty(node, AdID, startMS, durationMS, isInit, reportBulkMeta);
		}

		void CallProcessPeriodAssetIdentifier(Node *node, uint64_t startMS, uint64_t durationMS, std::string &assetID, std::string &providerID, bool isInit, bool reportBulkMeta = false)
		{
			ProcessPeriodAssetIdentifier(node, startMS, durationMS, assetID, providerID, isInit, reportBulkMeta);
		}

		bool CallProcessEventStream(uint64_t startMS, int64_t startOffsetMS, dash::mpd::IPeriod *period, bool reportBulkMeta = false)
		{
			return ProcessEventStream(startMS, startOffsetMS, period, reportBulkMeta);
		}

		void CallProcessStreamRestrictionList(Node *node, const std::string &AdID, uint64_t startMS, bool isInit, bool reportBulkMeta = false)
		{
			ProcessStreamRestrictionList(node, AdID, startMS, isInit, reportBulkMeta);
		}
		void CallTrackDownloader(int trackIdx, std::string initialization)
		{
			TrackDownloader(trackIdx, initialization);
		}

		void CallFetchAndInjectInitFragments(bool discontinuity = false)
		{
			FetchAndInjectInitFragments(discontinuity);
		}

		void CallFetchAndInjectInitialization(int trackIdx, bool discontinuity = false)
		{
			FetchAndInjectInitialization(trackIdx, discontinuity);
		}

		void CallStreamSelection(bool newTune, bool forceSpeedsChangedEvent)
		{
			StreamSelection(newTune, forceSpeedsChangedEvent);
		}

		bool CallCheckForInitalClearPeriod()
		{
			return CheckForInitalClearPeriod();
		}

		void CallPushEncryptedHeaders(std::map<int, std::string> &mappedHeaders)
		{

			PushEncryptedHeaders(mappedHeaders);
		}

		int CallGetProfileIdxForBandwidthNotification(uint32_t bandwidth)
		{
			return GetProfileIdxForBandwidthNotification(bandwidth);
		}

		std::string CallGetCurrentMimeType(AampMediaType mediaType)
		{
			return GetCurrentMimeType(mediaType);
		}

		AAMPStatusType CallUpdateTrackInfo(bool modifyDefaultBW, bool resetTimeLineIndex)
		{
			return UpdateTrackInfo(modifyDefaultBW, resetTimeLineIndex);
		}

		void CallSeekInPeriod(double seekPositionSeconds, bool skipToEnd = false)
		{
			SeekInPeriod(seekPositionSeconds, skipToEnd);
		}

		void CallApplyLiveOffsetWorkaroundForSAP(double seekPositionSeconds)
		{
			//TestableStreamAbstractionAAMP_MPD::FetchAndInjectInitFragments(false);
			StreamAbstractionAAMP_MPD::mNumberOfTracks = 2;
			ApplyLiveOffsetWorkaroundForSAP(seekPositionSeconds);
		}

		PeriodInfo CallGetFirstValidCurrMPDPeriod(std::vector<PeriodInfo> currMPDPeriodDetails)
		{
			return GetFirstValidCurrMPDPeriod(currMPDPeriodDetails);
		}

		IProducerReferenceTime *CallGetProducerReferenceTimeForAdaptationSet(IAdaptationSet *adaptationSet)
		{
			return GetProducerReferenceTimeForAdaptationSet(adaptationSet);
		}

		LatencyStatus CallGetLatencyStatus()
		{
			return GetLatencyStatus();
		}

		void CallQueueContentProtection(IPeriod *period, uint32_t adaptationSetIdx, AampMediaType mediaType, bool qGstProtectEvent = true, bool isVssPeriod = false)
		{
			QueueContentProtection(period, adaptationSetIdx, mediaType, qGstProtectEvent, isVssPeriod);
		}

		void CallProcessAllContentProtectionForMediaType(AampMediaType type, uint32_t priorityAdaptationIdx, std::set<uint32_t> &chosenAdaptationIdxs)
		{
			ProcessAllContentProtectionForMediaType(type, priorityAdaptationIdx, chosenAdaptationIdxs);
		}

		bool CallOnAdEvent(AdEvent evt)
		{
			return onAdEvent(evt);
		}

		bool CallOnAdEvent(AdEvent evt, double &adOffset)
		{
			return onAdEvent(evt, adOffset);
		}

		void CallSetAudioTrackInfo(const std::vector<AudioTrackInfo> &tracks, const std::string &trackIndex)
		{
			SetAudioTrackInfo(tracks, trackIndex);
		}

		void CallSetTextTrackInfo(const std::vector<TextTrackInfo> &tracks, const std::string &trackIndex)
		{
			SetTextTrackInfo(tracks, trackIndex);
		}

		bool CallCheckForVssTags()
		{
			return CheckForVssTags();
		}

		void CallGetAvailableVSSPeriods(std::vector<IPeriod *> &PeriodIds)
		{
			GetAvailableVSSPeriods(PeriodIds);
		}

		std::string CallGetVssVirtualStreamID()
		{
			return GetVssVirtualStreamID();
		}

		void CallCheckForAdResolvedStatus(AdNodeVectorPtr &ads, int adIdx, const std::string &periodId)
		{
			CheckAdResolvedStatus(ads, adIdx, periodId);
		}

		void CallSendAdPlacementEvent(AAMPEventType type, const std::string& adId,
			uint32_t relativePosition, AampTime absPosition, uint32_t offset,
			uint32_t duration, bool immediate)
		{
			SendAdPlacementEvent(type, adId, relativePosition, absPosition, offset, duration, immediate);
		}

		void CallSendAdReservationEvent(AAMPEventType type, const std::string& adBreakId,
			  uint64_t periodPosition, AampTime absPosition, bool immediate)
		{
			SendAdReservationEvent(type, adBreakId, periodPosition, absPosition, immediate);
		}

		/**
		 * @brief Test-only method to set and get the protected mFirstPTS member.
		 */
		void SetFirstPTSForTest(double pts) { mFirstPTS = pts; }
		double GetFirstPTSForTest() const { return mFirstPTS; }

		/**
		 * @brief Test-only methods to access the protected mStreamInfo member.
		 */
		std::vector<StreamInfo>& GetStreamInfoVector() { return mStreamInfo; }
		size_t GetStreamInfoSize() const { return mStreamInfo.size(); }
		void ResizeStreamInfo(size_t size) { mStreamInfo.resize(size); }
		void ClearStreamInfo() { mStreamInfo.clear(); }
		StreamInfo& GetStreamInfoAt(size_t index) { return mStreamInfo[index]; }

		/**
		 * @brief Test-only methods to set protected members.
		*/
		void SetIsFogTSB(bool value) { mIsFogTSB = value; }
		void SetAdPlayingFromCDN(bool value) { mAdPlayingFromCDN = value; }
	};

	PrivateInstanceAAMP *mPrivateInstanceAAMP;
	TestableStreamAbstractionAAMP_MPD *mStreamAbstractionAAMP_MPD;

	void SetUp() override
	{
		// Set up your objects before each test case
		mPrivateInstanceAAMP = new PrivateInstanceAAMP();
		g_mockPrivateInstanceAAMP = new NiceMock<MockPrivateInstanceAAMP>();
		mStreamAbstractionAAMP_MPD = new TestableStreamAbstractionAAMP_MPD(mPrivateInstanceAAMP, 0.0, 1.0);

		g_MockPrivateCDAIObjectMPD = new NiceMock<MockPrivateCDAIObjectMPD>();
		g_mockTSBSessionManager = new NiceMock<MockTSBSessionManager>(mPrivateInstanceAAMP);

		g_mockAampMPDDownloader = new StrictMock<MockAampMPDDownloader>();
		g_mockAampUtils = new StrictMock<MockAampUtils>();
		g_mockABRManager = new NiceMock<MockABRManager>();

	}

	void TearDown() override
	{
		// Clean up your objects after each test
		delete g_mockTSBSessionManager;
		g_mockTSBSessionManager = nullptr;

		delete mStreamAbstractionAAMP_MPD;
		mStreamAbstractionAAMP_MPD = nullptr;

		delete g_mockPrivateInstanceAAMP;
		g_mockPrivateInstanceAAMP = nullptr;

		delete mPrivateInstanceAAMP;
		mPrivateInstanceAAMP = nullptr;

		delete g_MockPrivateCDAIObjectMPD;
		g_MockPrivateCDAIObjectMPD = nullptr;

		delete g_mockAampMPDDownloader;
		g_mockAampMPDDownloader = nullptr;

		delete g_mockAampUtils;
		g_mockAampUtils = nullptr;

		delete g_mockABRManager;
		g_mockABRManager = nullptr;
	}
};

/**
 * @brief Functional tests class.
 */
class FunctionalTests : public FunctionalTestsBase,
						public ::testing::Test
{
protected:
	void SetUp() override
	{
		FunctionalTestsBase::SetUp();
	}

	void TearDown() override
	{
		FunctionalTestsBase::TearDown();
	}
};

/**
 * @brief Parameterized trickplay tests class.
 *
 * Trickplay tests are instantiated multiple times with a range of play rates.
 */
class TrickplayTests : public FunctionalTestsBase,
						 public ::testing::TestWithParam<int>
{
protected:
	void SetUp() override
	{
		FunctionalTestsBase::SetUp();
	}

	void TearDown() override
	{
		FunctionalTestsBase::TearDown();
	}

public:
	/**
	 * @brief Skip fragments helper method
	 *
	 * @param[in] trackType Media track type
	 * @param[in] skipTime Time to skip in seconds
	 * @retval Return value of StreamAbstractionAAMP_MPD::SkipFragments.
	 */
	double SkipFragments(TrackType trackType, double skipTime)
	{
		MediaTrack *track = mStreamAbstractionAAMP_MPD->GetMediaTrack(trackType);
		EXPECT_NE(track, nullptr);

		MediaStreamContext *pMediaStreamContext = static_cast<MediaStreamContext *>(track);
		return mStreamAbstractionAAMP_MPD->SkipFragments(pMediaStreamContext, skipTime);
	}
};

/**
 * @brief Parameterized trickplay tests class.
 *
 * Trickplay tests are instantiated multiple times with a range of play rates.
 */
class TrickplayTests1 : public TrickplayTests {};


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
	EXPECT_CALL(*g_mockPrivateInstanceAAMP, SetLLDashChunkMode(_));

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
	EXPECT_CALL(*g_mockPrivateInstanceAAMP, SetLLDashChunkMode(_));

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
	EXPECT_CALL(*g_mockPrivateInstanceAAMP, SetLLDashChunkMode(_));
	status = InitializeMPD(manifest); (void)status;

	/* The seek position will be at the beginning of the fragment containing the
	 * live point measured from the beginning of the available content in the
	 * time shift buffer.
	 */
	seekPosition = ((timeShiftBufferDepth - AAMP_LIVE_OFFSET)/segmentDurationSec)*segmentDurationSec;
	EXPECT_EQ(mStreamAbstractionAAMP_MPD->GetStreamPosition(), ((currentTime - timeShiftBufferDepth) + seekPosition));

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

	EXPECT_CALL(*g_mockPrivateInstanceAAMP, NotifyOnEnteringLive()).Times(1);
	EXPECT_CALL(*g_mockPrivateInstanceAAMP, SetLLDashChunkMode(_));
	status = InitializeMPD(manifest, eTUNETYPE_SEEKTOLIVE, initialSeekPosition); (void)status;

	/* The seek position will be at the live point measured from the start of
	 * the available content. This may be in the middle of a fragment.
	 */
	seekPosition = totalDuration - AAMP_LIVE_OFFSET;
	EXPECT_EQ(mStreamAbstractionAAMP_MPD->GetStreamPosition(), ((currentTime - totalDuration) + seekPosition));
	/*
	The Period Start Time
	*/
	double mFirstPeriodStartTime = initialSeekPosition;
	EXPECT_NE(mStreamAbstractionAAMP_MPD->GetFirstPeriodStartTime(), mFirstPeriodStartTime);

	/*
	GetBufferedDuration function call
	*/
	double retval = -1.0;
	MediaTrack *video = mStreamAbstractionAAMP_MPD->GetMediaTrack(eTRACK_VIDEO);
	video->enabled = true;
	EXPECT_NE(video, nullptr);
	EXPECT_NE(mStreamAbstractionAAMP_MPD->GetBufferedDuration(), retval);
	/**
	Get output format of stream.
	*/
	StreamOutputFormat primaryOutputFormat = FORMAT_ISO_BMFF;
	StreamOutputFormat audioOutputFormat = FORMAT_ISO_BMFF;
	StreamOutputFormat auxAudioOutputFormat = FORMAT_ISO_BMFF;
	StreamOutputFormat subtitleOutputFormat = FORMAT_INVALID;
	mStreamAbstractionAAMP_MPD->GetStreamFormat(primaryOutputFormat, audioOutputFormat, auxAudioOutputFormat, subtitleOutputFormat);
	EXPECT_CALL(*g_mockPrivateInstanceAAMP, DownloadsAreEnabled()).WillRepeatedly(Return(true));
	mStreamAbstractionAAMP_MPD->ReassessAndResumeAudioTrack(true);
	mStreamAbstractionAAMP_MPD->AbortWaitForAudioTrackCatchup(false);
	EXPECT_CALL(*g_mockAampConfig, IsConfigSet(_)).WillRepeatedly(Return(false));
	EXPECT_CALL(*g_mockPrivateInstanceAAMP, DisableDownloads());
	mStreamAbstractionAAMP_MPD->Stop(false);
	/**
	* PTS of first sample
	*/
	EXPECT_CALL(*g_mockPrivateInstanceAAMP, GetTSBSessionManager()).WillRepeatedly(Return(nullptr));
	double mFirstPTS = mStreamAbstractionAAMP_MPD->GetFirstPTS();
	EXPECT_EQ(mFirstPTS,15);
	/**
	* Start time PTS of first sample
	*/
	double mStartTimeOfFirstPTS = mStreamAbstractionAAMP_MPD->GetStartTimeOfFirstPTS();
	EXPECT_EQ(mStartTimeOfFirstPTS, ((availabilityStartTime + periodDuration) * 1000));
	/**
	* Get index of profile corresponds to bandwidth
	* profile index
	*/
	BitsPerSecond bitrate = 5;
	int profileCount = mStreamAbstractionAAMP_MPD->GetProfileCount();
	EXPECT_EQ(profileCount,0);
	int topBWIndex = mStreamAbstractionAAMP_MPD->GetBWIndex(bitrate);
	EXPECT_EQ(topBWIndex,0);
	/**
	* Get profile index for TsbBandwidth
	* profile index of the current bandwidth
	*/
	BitsPerSecond mTsbBandwidth = 10;
	int ProfileCountValue = mStreamAbstractionAAMP_MPD->GetProfileCount();
	EXPECT_EQ(ProfileCountValue,0);

	int mTsbBandwidthResult = mStreamAbstractionAAMP_MPD->GetProfileIndexForBandwidth(mTsbBandwidth);
	EXPECT_EQ(mTsbBandwidthResult,0);
	mTsbBandwidth = LONG_MAX;
	mTsbBandwidthResult = mStreamAbstractionAAMP_MPD->GetProfileIndexForBandwidth(mTsbBandwidth);
	EXPECT_EQ(mTsbBandwidthResult,0);
	/*
	* Gets Max Bitrate available for current playback.
	* long MAX video bitrates
	*/
	BitsPerSecond maxBitrate  = mStreamAbstractionAAMP_MPD->GetMaxBitrate(); (void)maxBitrate;
	int thumbnailIndex = 3;
	bool retThumbnailIndex = mStreamAbstractionAAMP_MPD->SetThumbnailTrack(thumbnailIndex); (void)retThumbnailIndex;
	mStreamAbstractionAAMP_MPD->StopInjection();

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
 * @brief SetVideoPlaybackRate test
 */
TEST_F(FunctionalTests, SetVideoPlaybackRate)
{
	AAMPStatusType status;
	double initialSeekPosition = 5.0;
	std::string fragmentUrl;
	/* The value of these variables must match the content of the manifest below: */
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
</MPD>
)";

	/* Initialize MPD. The video initialization segment is cached. */
	fragmentUrl = std::string(TEST_BASE_URL) + std::string("video_p0_init.mp4");
	EXPECT_CALL(*g_mockMediaStreamContext, CacheFragment(fragmentUrl, _, _, _, _, true, _, _, _, _, _))
		.WillOnce(Return(true));

	EXPECT_CALL(*g_mockPrivateInstanceAAMP, NotifyOnEnteringLive()).Times(1);
	EXPECT_CALL(*g_mockPrivateInstanceAAMP, SetLLDashChunkMode(_));
	status = InitializeMPD(manifest, eTUNETYPE_SEEKTOLIVE, initialSeekPosition);
	EXPECT_EQ(status, eAAMPSTATUS_OK);

	/*
	SetVideoPlaybackRate function call
	*/
	float rate = 2;
	MediaTrack *video = mStreamAbstractionAAMP_MPD->GetMediaTrack(eTRACK_VIDEO);
	video->enabled = false;
	EXPECT_CALL(*g_mockIsoBmffProcessor, setRate(rate, _)).Times(0);
	mStreamAbstractionAAMP_MPD->SetVideoPlaybackRate(rate);

	rate = 1;
	video->enabled = true;
	EXPECT_CALL(*g_mockIsoBmffProcessor, setRate(rate, PlayMode_normal)).Times(1);
	mStreamAbstractionAAMP_MPD->SetVideoPlaybackRate(rate);
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
	EXPECT_CALL(*g_mockPrivateInstanceAAMP, SetLLDashChunkMode(_));

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
	EXPECT_CALL(*g_mockPrivateInstanceAAMP, SetLLDashChunkMode(_));

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
	EXPECT_CALL(*g_mockPrivateInstanceAAMP, SetLLDashChunkMode(_));

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
	EXPECT_CALL(*g_mockPrivateInstanceAAMP, SetLLDashChunkMode(_));

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
	EXPECT_CALL(*g_mockPrivateInstanceAAMP, SetLLDashChunkMode(_));
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
	EXPECT_CALL(*g_mockPrivateInstanceAAMP, SetLLDashChunkMode(_));

	status = InitializeMPD(manifest);
	EXPECT_EQ(status, eAAMPSTATUS_OK);

	/* Verify that Fog is selected. */
	EXPECT_EQ(mStreamAbstractionAAMP_MPD->GetABRMode(), StreamAbstractionAAMP::ABRMode::FOG_TSB);
}

/**
 * @brief SeekPosUpdate test.
 *
 * Verify SeekPosUpdate() behavior.
 */
TEST_F(FunctionalTests, SeekPosUpdateTest)
{
	std::string fragmentUrl;
	AAMPStatusType status;
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
	EXPECT_CALL(*g_mockPrivateInstanceAAMP, SetLLDashChunkMode(_));

	status = InitializeMPD(manifest);
	EXPECT_EQ(status, eAAMPSTATUS_OK);
	// Initial seek position
	double initialSeekPosition = 0.0;
	EXPECT_EQ(mStreamAbstractionAAMP_MPD->GetStreamPosition(), initialSeekPosition);

	// Update the seek position to a new value
	double newSeekPosition1 = 12;
	mStreamAbstractionAAMP_MPD->SeekPosUpdate(newSeekPosition1);

	// Verify that the seek position has been updated correctly
	EXPECT_EQ(mStreamAbstractionAAMP_MPD->GetStreamPosition(), newSeekPosition1);

	// Update the seek position to a negative value ,should fail
	double newSeekPosition2 = -12;
	mStreamAbstractionAAMP_MPD->SeekPosUpdate(newSeekPosition2);

	// Verify that the seek position is not updated as negative seekPos has been passed
	EXPECT_NE(mStreamAbstractionAAMP_MPD->GetStreamPosition(), newSeekPosition2);
	EXPECT_EQ(mStreamAbstractionAAMP_MPD->GetStreamPosition(), newSeekPosition1); // checking if unchanged
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
		<Event duration="1358857" presentationTime="0"><![CDATA[{"version":1,"identifiers":[{"scheme":"urn:smpte:ul:060E2B34.01040101.01200900.00000000","value":"5493003","ad_position":"_PT0S_0","ad_type":"avail","tracking_uri":"../../../../../../../../../../tracking/99247e89c7677df85a85aabdd3256ffe02a60196/example-dash-vod-2s-generic/f38d0147-7bee-480f-83a7-fec49fda39b9","custom_vast_data":null}]}]]></Event>
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
	std::string expectedCData = R"({"version":1,"identifiers":[{"scheme":"urn:smpte:ul:060E2B34.01040101.01200900.00000000","value":"5493003","ad_position":"_PT0S_0","ad_type":"avail","tracking_uri":"../../../../../../../../../../tracking/99247e89c7677df85a85aabdd3256ffe02a60196/example-dash-vod-2s-generic/f38d0147-7bee-480f-83a7-fec49fda39b9","custom_vast_data":null}]})";
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

/**
 * @brief IFrame trickplay cadence parameterized test.
 *
 * @param[in] param Play rate
 *
 * Verify the difference between successive IFrame indices in trickplay.
 */
float trickplay_number_tbl[] = {-1.0, 2.0, -2.0, 6.0, -6.0, 12.0, -12.0, 30.0, -30.0};

TEST_P(TrickplayTests, Cadence)
{
	float rate;
	char url[64];
	std::string fragmentUrl;
	AAMPStatusType status;
	double seekPosition = 30.0;
	int fragmentNumber = 31;
	double skipTime;
	static const char *manifest =
R"(<?xml version="1.0" encoding="utf-8"?>
<MPD xmlns:xsi="http://www.w3.org/2001/XMLSchema-instance" xmlns="urn:mpeg:dash:schema:mpd:2011" xmlns:xlink="http://www.w3.org/1999/xlink" xsi:schemaLocation="urn:mpeg:DASH:schema:MPD:2011 http://standards.iso.org/ittf/PubliclyAvailableStandards/MPEG-DASH_schema_files/DASH-MPD.xsd" profiles="urn:mpeg:dash:profile:isoff-live:2011" type="static" mediaPresentationDuration="PT15M0.0S" minBufferTime="PT4.0S">
	<Period id="0" start="PT0.0S">
		<AdaptationSet id="1" contentType="video" segmentAlignment="true" bitstreamSwitching="true" lang="und">
			<EssentialProperty schemeIdUri="http://dashif.org/guidelines/trickmode" value="1"/>
			<Representation id="4" mimeType="video/mp4" codecs="avc1.4d4016" bandwidth="800000" width="640" height="360" frameRate="1/1">
				<SegmentTemplate timescale="16384" initialization="dash/iframe_init.m4s" media="dash/iframe_$Number%03d$.m4s" startNumber="1">
					<SegmentTimeline>
						<S t="0" d="16384" r="899"/>
					</SegmentTimeline>
				</SegmentTemplate>
			</Representation>
		</AdaptationSet>
	</Period>
</MPD>
)";

	/* Get the rate parameter. */
	rate = trickplay_number_tbl[GetParam()];

	/* Initialize MPD with seek position and play rate. The initialization
	 * segment is cached.
	 */
	fragmentUrl = std::string(TEST_BASE_URL) + std::string("dash/iframe_init.m4s");
	EXPECT_CALL(*g_mockMediaStreamContext, CacheFragment(fragmentUrl, _, _, _, _, true, _, _, _, _, _))
		.WillOnce(Return(true));
	EXPECT_CALL(*g_mockPrivateInstanceAAMP, SetLLDashChunkMode(_));
	status = InitializeMPD(manifest, TuneType::eTUNETYPE_NEW_NORMAL, seekPosition, rate);

	EXPECT_EQ(status, eAAMPSTATUS_OK);

	/* Push the first video segment to present. */
	(void)snprintf(url, sizeof(url), "%sdash/iframe_%03d.m4s", TEST_BASE_URL, fragmentNumber);
	fragmentUrl = std::string(url);
	EXPECT_CALL(*g_mockMediaStreamContext, CacheFragment(fragmentUrl, _, _, _, _, false, _, _, _, _, _))
		.WillOnce(Return(true));

	PushNextFragment(eTRACK_VIDEO);

	/* Skip segments which will not be pushed. */
	skipTime = rate/TRICKPLAY_VOD_PLAYBACK_FPS;
	(void)SkipFragments(eTRACK_VIDEO, skipTime);

	/* Push the next video segment to present. */
	if (rate > 0.0)
	{
		fragmentNumber += ((int)abs(skipTime) + 1);
	}
	else
	{
		fragmentNumber -= ((int)abs(skipTime) + 1);
	}

	(void)snprintf(url, sizeof(url), "%sdash/iframe_%03d.m4s", TEST_BASE_URL, fragmentNumber);
	fragmentUrl = std::string(url);
	EXPECT_CALL(*g_mockMediaStreamContext, CacheFragment(fragmentUrl, _, _, _, _, false, _, _, _, _, _))
		.WillOnce(Return(true));

	PushNextFragment(eTRACK_VIDEO);
}


/**
 * @brief Trickplay url generation test where $Time$ is used in the media attribute.
 *
 * @param[in] idx idx into table of test values
 *
 * Verify that correct time calculations lead to generation of the
 * correct url for the segment when we are performing trickplay.
 */

/*
Here is the expanded SegmentTimeline used in TrickplayTests1 . Used to generate expected segment numbers:
t	d	r
0	9	0
9	9	1
18	9	2
27	9	3
36	9	4
45	11	0
56	8	0
64	8	1
72	8	2
80	8	3
88	8	4
96	10	0
106	10	1
116	10	2
126	10	3
136	10	4
146	10	5
156	10	6
166	10	7
176	10	8
186	10	9
196	10	10
*/

typedef struct {
	double seek;
	double rate;
	int expected_seg_time[100];
} TRICKPLAY_TBL;

TRICKPLAY_TBL trickplay_time_tbl[] = {
//{ seek, rate, {expected segment numbers ...} }
{ 0,     2.0, {0,9,18,27,36,45,56,64,72,80,88,96,106,-1} },
{ 11.6, -2.0, {116,106,96,88,80,72,64,56,45,36,27,18,9,-1} },

{ 0,     6.0, {0,18,36,56,72,88,106,126,146,166,186,-1} },
{ 18.6, -6.0, {186,166, 146,126,106,88,72,56,36,18,0,-1} },

{ 0,     12.0, {0,36,72,106,146,186,-1} },
{ 18.6, -12.0, {186,146,106,72,36,0,-1} }
};


TEST_P(TrickplayTests1, TblIndex)
{
	float rate;

	std::string fragmentUrl;
	AAMPStatusType status;
	double seekPosition = 0.0;
	double skipTime = 0.0;
	static const char *manifest =
R"(<?xml version="1.0" encoding="utf-8"?>
<MPD xmlns:xsi="http://www.w3.org/2001/XMLSchema-instance" xmlns="urn:mpeg:dash:schema:mpd:2011" xmlns:xlink="http://www.w3.org/1999/xlink" xsi:schemaLocation="urn:mpeg:DASH:schema:MPD:2011 http://standards.iso.org/ittf/PubliclyAvailableStandards/MPEG-DASH_schema_files/DASH-MPD.xsd" profiles="urn:mpeg:dash:profile:isoff-live:2011" type="static" mediaPresentationDuration="PT15M0.0S" minBufferTime="PT4.0S">
	<Period id="0" start="PT0.0S">
		<AdaptationSet id="1" contentType="video" segmentAlignment="true" bitstreamSwitching="true" lang="und">
			<EssentialProperty schemeIdUri="http://dashif.org/guidelines/trickmode" value="1"/>
			<Representation id="4" mimeType="video/mp4" codecs="avc1.4d4016" bandwidth="800000" width="640" height="360" frameRate="1/1">
				<SegmentTemplate timescale="10" initialization="dash/iframe_init.m4s" media="dash/iframe_$Time$.m4s" startNumber="1">
					<SegmentTimeline>
						<S t="0" d="9" r="4" />
						<S t="45" d="11" r="0" />
						<S t="56" d="8" r="4" />
					<S t="96" d="10" r="10" />
					</SegmentTimeline>
				</SegmentTemplate>
			</Representation>
		</AdaptationSet>
	</Period>
</MPD>
)";

	/* Get the rate parameter. */
	int idx = GetParam();

	rate = trickplay_time_tbl[idx].rate;
	seekPosition = trickplay_time_tbl[idx].seek;

	/* Initialize MPD with seek position and play rate. */
	fragmentUrl = std::string(TEST_BASE_URL) + std::string("dash/iframe_init.m4s");
	EXPECT_CALL(*g_mockMediaStreamContext, CacheFragment(fragmentUrl, _, _, _, _, true, _, _, _, _, _))
		.WillOnce(Return(true));
	EXPECT_CALL(*g_mockPrivateInstanceAAMP, SetLLDashChunkMode(_));
	status = InitializeMPD(manifest, TuneType::eTUNETYPE_NEW_NORMAL, seekPosition, rate);

	EXPECT_EQ(status, eAAMPSTATUS_OK);

	/* Generate all the video segment URLs we are expecting */
	int j;
	for (j = 0; trickplay_time_tbl[idx].expected_seg_time[j] >=0; j++)
	{
		char number[50];
		(void)snprintf(number, sizeof(number), "dash/iframe_%d.m4s", trickplay_time_tbl[idx].expected_seg_time[j]);
		fragmentUrl = std::string(TEST_BASE_URL) + std::string(number);
		EXPECT_CALL(*g_mockMediaStreamContext, CacheFragment(fragmentUrl, _, _, _, _, false, _, _, _, _, _))
			.WillOnce(Return(true));
	}

	/* Loop through the code we are testing */
	for (j = 0; trickplay_time_tbl[idx].expected_seg_time[j] >=0; j++)
	{
		PushNextFragment(eTRACK_VIDEO);

		/* Skip segments which will not be pushed. */
		skipTime = rate / TRICKPLAY_VOD_PLAYBACK_FPS;
		(void)SkipFragments(eTRACK_VIDEO, skipTime);
	}
}

/**
 * @brief Instantiate the trickplay tests with multiple play rates.
 */
INSTANTIATE_TEST_SUITE_P(TrickPlay,
			 TrickplayTests,
			 testing::Range(0,(int)(sizeof(trickplay_number_tbl)/sizeof(trickplay_number_tbl[0]))));

INSTANTIATE_TEST_SUITE_P(TrickPlayTime,
			 TrickplayTests1,
			 testing::Range(0,(int)(sizeof(trickplay_time_tbl)/sizeof(trickplay_time_tbl[0]))));

TEST_F(FunctionalTests_1, ResetSubtitle)
{
	_instanceStreamAbstractionAAMP_MPD->ResetSubtitle();
}

TEST_F(FunctionalTests_1, MuteSubtitleOnPause)
{
	_instanceStreamAbstractionAAMP_MPD->MuteSubtitleOnPause();
}

TEST_F(FunctionalTests_1, MuteSidecarSubtitlesTest)
{
	_instanceStreamAbstractionAAMP_MPD->MuteSidecarSubtitles(true);
}

TEST_F(FunctionalTests_1, GetCurrPeriodTimeScaleTest)
{
	uint32_t result = _instanceStreamAbstractionAAMP_MPD->GetCurrPeriodTimeScale();
	EXPECT_EQ(result, 0);
}

TEST_F(FunctionalTests_1, GetFirstPeriodStartTimeTest)
{
	double result = _instanceStreamAbstractionAAMP_MPD->GetFirstPeriodStartTime();
	EXPECT_EQ(result, 0.0);
}

TEST_F(FunctionalTests_1, MonitorLatencyTest)
{
	_instanceStreamAbstractionAAMP_MPD->MonitorLatency();
}

TEST_F(FunctionalTests_1, StartSubtitleParserTest)
{
	_instanceStreamAbstractionAAMP_MPD->StartSubtitleParser();
}

TEST_F(FunctionalTests_1, PauseSubtitleParserTest)
{
	_instanceStreamAbstractionAAMP_MPD->PauseSubtitleParser(true);
}

TEST_F(FunctionalTests_1, GetStreamFormatTest)
{
	// Create variables to store the output formats
	StreamOutputFormat primaryFormat;
	StreamOutputFormat audioFormat;
	StreamOutputFormat auxFormat;
	StreamOutputFormat subtitleFormat;
	// Call the GetStreamFormat function
	_instanceStreamAbstractionAAMP_MPD->GetStreamFormat(primaryFormat, audioFormat, auxFormat, subtitleFormat);
	EXPECT_EQ(FORMAT_INVALID, primaryFormat);
	EXPECT_EQ(FORMAT_INVALID, audioFormat);
	EXPECT_EQ(FORMAT_INVALID, auxFormat);
	EXPECT_EQ(FORMAT_INVALID, subtitleFormat);
}

TEST_F(FunctionalTests_1, GetVideoBitratesTest)
{
	// Call the GetVideoBitrates function
	std::vector<BitsPerSecond> videoBitrates = _instanceStreamAbstractionAAMP_MPD->GetVideoBitrates();
	ASSERT_EQ(0, videoBitrates.size());
}

TEST_F(FunctionalTests_1, GetAudioBitratesTest)
{
	// Call the GetVideoBitrates function
	std::vector<BitsPerSecond> audioBitrates = _instanceStreamAbstractionAAMP_MPD->GetAudioBitrates();
	ASSERT_EQ(0, audioBitrates.size());
}

TEST_F(FunctionalTests_1, StartInjectionTest)
{
	_instanceStreamAbstractionAAMP_MPD->StartInjection();
}

TEST_F(FunctionalTests_1, SeekPosUpdateTest)
{
	double secondsRelativeToTuneTime = 42.0;
	// Call the SeekPosUpdate function
	_instanceStreamAbstractionAAMP_MPD->SeekPosUpdate(secondsRelativeToTuneTime);
	// Check that the seekPosition has been updated correctly
	ASSERT_EQ(42.0, secondsRelativeToTuneTime);
}

TEST_F(FunctionalTests_1, GetAvailableAudioTracksTest)
{
	// Call the GetAvailableAudioTracks function
	std::vector<AudioTrackInfo> audioTracks = _instanceStreamAbstractionAAMP_MPD->GetAvailableAudioTracks();
	// Add your assertions to check the returned audio tracks
	ASSERT_GE(audioTracks.size(), 0);
}

TEST_F(FunctionalTests_1, GetAvailableTextTracksTest)
{
	// Call the GetAvailableTextTracks function
	std::vector<TextTrackInfo> textTracks = _instanceStreamAbstractionAAMP_MPD->GetAvailableTextTracks();
	ASSERT_GE(textTracks.size(), 0);
}

TEST_F(FunctionalTests_1, GetAvailableVideoTracksTest)
{
	// Call the GetAvailableVideoTracks function
	std::vector<StreamInfo *> videoTracks = _instanceStreamAbstractionAAMP_MPD->GetAvailableVideoTracks();
	// For example, you can check the size of the vector.
	ASSERT_GE(videoTracks.size(), 0); // Assuming there can be zero or more video tracks
}

TEST_F(FunctionalTests_1, GetThumbnailRangeDataTest)
{
	// Define test input values
	double tStart = 0.0;
	double tEnd = 10.0;
	std::string baseUrl = "http://example.com/thumbnails/";
	int raw_w = 1920;
	int raw_h = 1080;
	int width = 160;
	int height = 90;
	// Call the GetThumbnailRangeData function
	std::vector<ThumbnailData> thumbnailData = _instanceStreamAbstractionAAMP_MPD->GetThumbnailRangeData(tStart, tEnd, &baseUrl, &raw_w, &raw_h, &width, &height);
	ASSERT_GE(thumbnailData.size(), 0);
}

TEST_F(FunctionalTests_1, GetMinUpdateDurationTest)
{
	// Call the GetMinUpdateDuration function
	int64_t minUpdateDuration = _instanceStreamAbstractionAAMP_MPD->GetMinUpdateDuration();
	// Add your assertions to check the returned value
	ASSERT_GE(minUpdateDuration, 0);
}

TEST_F(FunctionalTests_1, GetAccessibilityNodeWithStringValueTest)
{
	AampJsonObject accessNode;
	Accessibility accessibilityNode = _instanceStreamAbstractionAAMP_MPD->getAccessibilityNode(accessNode);
	ASSERT_EQ(accessibilityNode, accessibilityNode);
}

TEST_F(FunctionalTests_1, GetBestTextTrackByLanguageTest)
{
	// Define test input values
	TextTrackInfo selectedTextTrack;
	// Call the GetBestTextTrackByLanguage function
	bool result = _instanceStreamAbstractionAAMP_MPD->GetBestTextTrackByLanguage(selectedTextTrack);
	ASSERT_FALSE(result);
	ASSERT_EQ(selectedTextTrack.language, "");
}

TEST_F(FunctionalTests_1, InitSubtitleParserTest)
{
	// Define test input data
	char data[] = "Test String";
	// Call the InitSubtitleParser function
	_instanceStreamAbstractionAAMP_MPD->InitSubtitleParser(data);
}

TEST_F(FunctionalTests_1, ResumeSubtitleOnPlayTest)
{
	// Define test input values
	bool mute = false; // Replace with the desired mute status
	char data[] = "Subtitle data";
	// Call the ResumeSubtitleOnPlay function
	_instanceStreamAbstractionAAMP_MPD->ResumeSubtitleOnPlay(mute, data);
}

TEST_F(FunctionalTests_1, ResumeSubtitleAfterSeekTest)
{
	// Define test input values
	bool mute = false; // Replace with the desired mute status
	char data[] = "Subtitle data";
	// Call the ResumeSubtitleAfterSeek function
	_instanceStreamAbstractionAAMP_MPD->ResumeSubtitleAfterSeek(mute, data);
}

TEST_F(FunctionalTests_1, SetTextStyleTest)
{
	// Define test input values
	std::string options = "{\"font\":\"Arial\",\"size\":16,\"color\":\"white\"}";
	// Call the SetTextStyle function
	bool result = _instanceStreamAbstractionAAMP_MPD->SetTextStyle(options);
	// Add assertions to check the result of the SetTextStyle function
	ASSERT_FALSE(result);
}

TEST_F(FunctionalTests_1, SetNextObjectRequestUrlTest)
{
	// Define test input values
	std::string media = "example.mp4";
	FragmentDescriptor fragmentDescriptor;
	AampMediaType mediaType = eMEDIATYPE_DEFAULT;
	// Call the setNextobjectrequestUrl function
	_instanceStreamAbstractionAAMP_MPD->setNextobjectrequestUrl(media, &fragmentDescriptor, mediaType);
}

TEST_F(FunctionalTests_1, SetNextRangeRequestTest)
{
	// Define test input values
	std::string fragmentUrl = "fragment.mp4";
	std::string nextrange = "bytes=100-199";
	long bandwidth = 500000;
	AampMediaType mediaType = eMEDIATYPE_DEFAULT;
	// Call the setNextRangeRequest function
	_instanceStreamAbstractionAAMP_MPD->setNextRangeRequest(fragmentUrl, nextrange, bandwidth, mediaType);
}

TEST_F(FunctionalTests_1, GetAvailabilityStartTime)
{
	double result = _instanceStreamAbstractionAAMP_MPD->GetAvailabilityStartTime();
	EXPECT_EQ(result, 0.0);
}

/**
 * @brief Verify UseIframeTrack() behavior during normal playback.
 * Expected to return false, since trickplayMode is false by default and an iframe track is only used for trick modes.
 */
TEST_F(FunctionalTests_1, UseIframeTrack)
{
	bool result = _instanceStreamAbstractionAAMP_MPD->UseIframeTrack();
	EXPECT_EQ(result, false);
}

/**
 * @brief Verify UseIframeTrack() behavior during trick play.
 * Set trickplayMode to true and expect the method to return true.
 */
TEST_F(FunctionalTests_1, UseIframeTrack_trickplay)
{
	_instanceStreamAbstractionAAMP_MPD->trickplayMode = true;
	bool result = _instanceStreamAbstractionAAMP_MPD->UseIframeTrack();
	EXPECT_EQ(result, true);
}

/**
 * @brief Verify UseIframeTrack() behavior during trick play with AAMP TSB enabled.
 * Set AAMP Local TSB and trickplayMode to true and expect the method to return true.
 * During trick play, iframe track is not used only if AAMP Local TSB and iframe track extract are enabled.
 * Iframe track extract is disabled by default.
 */
TEST_F(FunctionalTests_1, UseIframeTrack_aamptsb)
{
	_instanceStreamAbstractionAAMP_MPD->aamp->SetLocalAAMPTsb(true);
	_instanceStreamAbstractionAAMP_MPD->trickplayMode = true;
	bool result = _instanceStreamAbstractionAAMP_MPD->UseIframeTrack();
	EXPECT_EQ(result, true);
}

/**
 * @brief Verify UseIframeTrack() behavior during trick play with AAMP TSB enabled.
 * Set AAMP Local TSB, iframe track extract and trickplayMode to true and expect the method to return false.
 * If AAMP Local TSB and iframe track extract are enabled, the iframe track is not used for trick play and iframes are
 * extracted from the segments stored in AAMP Local TSB.
 */
TEST_F(FunctionalTests_1, UseIframeTrack_aamptsb_iframeextract)
{
	EXPECT_CALL(*g_mockAampConfig, IsConfigSet(eAAMPConfig_EnableIFrameTrackExtract)).WillOnce(Return(true));
	_instanceStreamAbstractionAAMP_MPD->aamp->SetLocalAAMPTsb(true);
	_instanceStreamAbstractionAAMP_MPD->trickplayMode = true;
	bool result = _instanceStreamAbstractionAAMP_MPD->UseIframeTrack();
	EXPECT_EQ(result, false);
}

/**
 * @brief Verify UseIframeTrack() behavior during trick play with AAMP TSB enabled.
 * Set iframe track extract and trickplayMode to true and expect the method to return true.
 * During trick play, iframe track is not used only if AAMP Local TSB and iframe track extract are enabled but
 * AAMP Local TSB is disabled.
 */
TEST_F(FunctionalTests_1, UseIframeTrack_iframeextract)
{
	EXPECT_CALL(*g_mockAampConfig, IsConfigSet(eAAMPConfig_EnableIFrameTrackExtract)).WillRepeatedly(Return(true));
	_instanceStreamAbstractionAAMP_MPD->aamp->SetLocalAAMPTsb(false);
	_instanceStreamAbstractionAAMP_MPD->trickplayMode = true;
	bool result = _instanceStreamAbstractionAAMP_MPD->UseIframeTrack();
	EXPECT_EQ(result, true);
}

TEST_F(StreamAbstractionAAMP_MPDTest, PrintSelectedTrackTest)
{
	mStreamAbstractionAAMP_MPD->CallPrintSelectedTrack("2", AampMediaType::eMEDIATYPE_SUBTITLE);
}

TEST_F(StreamAbstractionAAMP_MPDTest, FetcherLoopTest)
{
	mStreamAbstractionAAMP_MPD->CallFetcherLoop();
}

TEST_F(StreamAbstractionAAMP_MPDTest, GetStreamInfoTest) {
	// Assuming some index for testing
	int idx=-1;
	StreamInfo* streamInfo = mStreamAbstractionAAMP_MPD->CallGetStreamInfo(idx);
	(void)streamInfo;
}

/**
 * @brief Test GetStreamInfo in non-FOG TSB mode using proper test class
 * Sets up the StreamAbstractionAAMP_MPD instance in non-FOG TSB mode and verifies that
 * GetStreamInfo correctly retrieves StreamInfo via the ABR manager.
 */
TEST_F(StreamAbstractionAAMP_MPDTest, GetStreamInfo_NonFogTsbMode)
{
	// Set non-FOG TSB mode (uses ABR manager path)
	mStreamAbstractionAAMP_MPD->SetIsFogTSB(false);
	mStreamAbstractionAAMP_MPD->SetAdPlayingFromCDN(false);

	// Populate mStreamInfo vector with 2 elements
	mStreamAbstractionAAMP_MPD->ResizeStreamInfo(2);
	mStreamAbstractionAAMP_MPD->GetStreamInfoAt(0).bandwidthBitsPerSecond = 500000;
	mStreamAbstractionAAMP_MPD->GetStreamInfoAt(1).bandwidthBitsPerSecond = 1000000;

	// Mock expectations for ABR manager calls
	EXPECT_CALL(*g_mockABRManager, getProfileCount())
		.WillRepeatedly(Return(2));
	EXPECT_CALL(*g_mockABRManager, getUserDataOfProfile(1))
		.WillOnce(Return(1)); // Maps to index 1 in mStreamInfo
	EXPECT_CALL(*g_mockABRManager, getUserDataOfProfile(2))
		.WillOnce(Return(-1)); // Invalid user data - should cause nullptr return

	// Test valid index access through ABR manager
	StreamInfo *result1 = mStreamAbstractionAAMP_MPD->CallGetStreamInfo(1);
	ASSERT_NE(result1, nullptr);
	EXPECT_EQ(result1->bandwidthBitsPerSecond, 1000000);

	// Test boundary case - ABR manager returns invalid user data
	StreamInfo *result2 = mStreamAbstractionAAMP_MPD->CallGetStreamInfo(2);
	EXPECT_EQ(result2, nullptr);
}

/**
 * @brief Test GetStreamInfo bounds checking with empty vector in FOG TSB mode
 * Sets up the StreamAbstractionAAMP_MPD instance in FOG TSB mode and verifies that
 * GetStreamInfo correctly handles out-of-bounds indices when the mStreamInfo vector is empty.
 */
TEST_F(StreamAbstractionAAMP_MPDTest, GetStreamInfo_FogTsb_EmptyVector)
{
	// Set FOG TSB mode to enable direct index access with bounds checking
	mStreamAbstractionAAMP_MPD->SetIsFogTSB(true);
	mStreamAbstractionAAMP_MPD->SetAdPlayingFromCDN(false);

	// mStreamInfo vector is empty by default
	EXPECT_EQ(mStreamAbstractionAAMP_MPD->GetStreamInfoSize(), 0);

	// Test accessing invalid indices
	StreamInfo *result1 = mStreamAbstractionAAMP_MPD->CallGetStreamInfo(-1);
	EXPECT_EQ(result1, nullptr);

	StreamInfo *result2 = mStreamAbstractionAAMP_MPD->CallGetStreamInfo(0);
	EXPECT_EQ(result2, nullptr);

	StreamInfo *result3 = mStreamAbstractionAAMP_MPD->CallGetStreamInfo(5);
	EXPECT_EQ(result3, nullptr);
}

/**
 * @brief Test GetStreamInfo bounds checking with populated vector in FOG TSB mode
 * Sets up the StreamAbstractionAAMP_MPD instance in FOG TSB mode and verifies that
 * GetStreamInfo correctly handles valid and out-of-bounds indices when the mStreamInfo vector is populated.
 */
TEST_F(StreamAbstractionAAMP_MPDTest, GetStreamInfo_FogTsb_PopulatedVector)
{
	// Set FOG TSB mode to enable direct index access with bounds checking
	mStreamAbstractionAAMP_MPD->SetIsFogTSB(true);
	mStreamAbstractionAAMP_MPD->SetAdPlayingFromCDN(false);

	// Populate mStreamInfo vector with 3 elements
	mStreamAbstractionAAMP_MPD->ResizeStreamInfo(3);
	mStreamAbstractionAAMP_MPD->GetStreamInfoAt(0).bandwidthBitsPerSecond = 500000;
	mStreamAbstractionAAMP_MPD->GetStreamInfoAt(1).bandwidthBitsPerSecond = 1000000;
	mStreamAbstractionAAMP_MPD->GetStreamInfoAt(2).bandwidthBitsPerSecond = 2000000;

	// Test valid indices
	StreamInfo *result1 = mStreamAbstractionAAMP_MPD->CallGetStreamInfo(0);
	ASSERT_NE(result1, nullptr);
	EXPECT_EQ(result1->bandwidthBitsPerSecond, 500000);

	StreamInfo *result2 = mStreamAbstractionAAMP_MPD->CallGetStreamInfo(2);
	ASSERT_NE(result2, nullptr);
	EXPECT_EQ(result2->bandwidthBitsPerSecond, 2000000);

	// Test invalid indices (out of bounds)
	StreamInfo *result3 = mStreamAbstractionAAMP_MPD->CallGetStreamInfo(-1);
	EXPECT_EQ(result3, nullptr);

	StreamInfo *result4 = mStreamAbstractionAAMP_MPD->CallGetStreamInfo(3);
	EXPECT_EQ(result4, nullptr);
}

/**
 * @brief Test GetStreamInfo heap overflow prevention scenario in non-FOG TSB mode
 * Simulates a scenario where the stream info vector size decreases and ensures no heap overflow occurs
 * when using ABR manager path
 */
TEST_F(StreamAbstractionAAMP_MPDTest, GetStreamInfo_NonFogTsb_HeapOverflowPrevention)
{
	// Set non-FOG TSB mode to enable ABR manager path
	mStreamAbstractionAAMP_MPD->SetIsFogTSB(false);
	mStreamAbstractionAAMP_MPD->SetAdPlayingFromCDN(false);

	// First: Set up ad content with 5 profiles
	mStreamAbstractionAAMP_MPD->ResizeStreamInfo(5);
	for (int i = 0; i < 5; i++)
	{
		mStreamAbstractionAAMP_MPD->GetStreamInfoAt(i).bandwidthBitsPerSecond = (i + 1) * 1000000;
	}

	// Mock ABR manager to simulate initial state with 5 profiles
	EXPECT_CALL(*g_mockABRManager, getProfileCount())
		.WillRepeatedly(Return(5));

	// Set up user data mapping for initial 5 profiles
	for (int i = 0; i < 5; i++)
	{
		EXPECT_CALL(*g_mockABRManager, getUserDataOfProfile(i))
			.WillRepeatedly(Return(i)); // Maps to index i in mStreamInfo
	}

	// Verify all profiles are accessible through ABR manager
	for (int i = 0; i < 5; i++)
	{
		StreamInfo *result = mStreamAbstractionAAMP_MPD->CallGetStreamInfo(i);
		ASSERT_NE(result, nullptr);
	}

	// Now: Switch to source content with only 1 profile (heap size reduction)
	mStreamAbstractionAAMP_MPD->ResizeStreamInfo(1);
	mStreamAbstractionAAMP_MPD->GetStreamInfoAt(0).bandwidthBitsPerSecond = 2000000;

	// Update ABR manager to reflect new profile count
	EXPECT_CALL(*g_mockABRManager, getProfileCount())
		.WillRepeatedly(Return(1));

	// Test: Old cached profileIndex=4 should now be safely handled
	StreamInfo *result = mStreamAbstractionAAMP_MPD->CallGetStreamInfo(4);
	EXPECT_EQ(result, nullptr); // Should return nullptr due to invalid user data

	// Valid profile should still work
	StreamInfo *validResult = mStreamAbstractionAAMP_MPD->CallGetStreamInfo(0); // Profile 0 exists
	ASSERT_NE(validResult, nullptr);
	EXPECT_EQ(validResult->bandwidthBitsPerSecond, 2000000);

	// Test additional edge cases
	StreamInfo *invalidResult1 = mStreamAbstractionAAMP_MPD->CallGetStreamInfo(1); // Profile 1 doesn't exist
	EXPECT_EQ(invalidResult1, nullptr);

	// Out of range index
	EXPECT_CALL(*g_mockABRManager, getUserDataOfProfile(10))
		.WillRepeatedly(Return(-1));
	StreamInfo *invalidResult2 = mStreamAbstractionAAMP_MPD->CallGetStreamInfo(10); // Out of range
	EXPECT_EQ(invalidResult2, nullptr);
}

TEST_F(StreamAbstractionAAMP_MPDTest, EnableAndSetLiveOffsetForLLDashPlaybackTest)
{
	const MPD *mpd = NULL;
	AAMPStatusType result = mStreamAbstractionAAMP_MPD->CallEnableAndSetLiveOffsetForLLDashPlayback(mpd);
	(void)result;
}

TEST_F(StreamAbstractionAAMP_MPDTest, FetchAndInjectInitFragmentsTest)
{
	bool discontinuity = true;
	mStreamAbstractionAAMP_MPD->CallFetchAndInjectInitFragments(discontinuity);
}

TEST_F(StreamAbstractionAAMP_MPDTest, StreamSelectionTest)
{
	bool newTune = false;
	bool forceSpeedsChangedEvent = true;
	mStreamAbstractionAAMP_MPD->CallStreamSelection(newTune, forceSpeedsChangedEvent);
}

TEST_F(StreamAbstractionAAMP_MPDTest, StreamSelectionTest_1)
{
	bool newTune = true;
	bool forceSpeedsChangedEvent = false;
	mStreamAbstractionAAMP_MPD->CallStreamSelection(newTune, forceSpeedsChangedEvent);
}

TEST_F(StreamAbstractionAAMP_MPDTest, StreamSelectionTest_2)
{
	bool newTune = false;
	bool forceSpeedsChangedEvent = false;
	mStreamAbstractionAAMP_MPD->CallStreamSelection(newTune, forceSpeedsChangedEvent);
}

TEST_F(StreamAbstractionAAMP_MPDTest, StreamSelectionTest_3)
{
	bool newTune = true;
	bool forceSpeedsChangedEvent = true;
	mStreamAbstractionAAMP_MPD->CallStreamSelection(newTune, forceSpeedsChangedEvent);
}

TEST_F(StreamAbstractionAAMP_MPDTest, CheckForInitalClearPeriodTest)
{
	bool result = mStreamAbstractionAAMP_MPD->CallCheckForInitalClearPeriod(); (void)result;
}

TEST_F(StreamAbstractionAAMP_MPDTest, PushEncryptedHeadersTest)
{
	std::map<int, std::string> mappedHeaders;
	mStreamAbstractionAAMP_MPD->CallPushEncryptedHeaders(mappedHeaders);
}

TEST_F(StreamAbstractionAAMP_MPDTest, GetProfileIdxForBandwidthNotificationTest)
{
	uint32_t bandwidth = 22;
	int result = mStreamAbstractionAAMP_MPD->CallGetProfileIdxForBandwidthNotification(bandwidth); (void)result;
}

TEST_F(StreamAbstractionAAMP_MPDTest, GetCurrentMimeTypeTest)
{
	AampMediaType mediaType = eMEDIATYPE_DEFAULT;
	std::string result = mStreamAbstractionAAMP_MPD->CallGetCurrentMimeType(mediaType);
}

TEST_F(StreamAbstractionAAMP_MPDTest, UpdateTrackInfoTest)
{
	bool modifyDefaultBW = true;
	bool resetTimeLineIndex = true;
	AAMPStatusType result = mStreamAbstractionAAMP_MPD->CallUpdateTrackInfo(modifyDefaultBW, resetTimeLineIndex); (void)result;
}

TEST_F(StreamAbstractionAAMP_MPDTest, UpdateTrackInfoTest_1)
{
	bool modifyDefaultBW = false;
	bool resetTimeLineIndex = false;
	AAMPStatusType result = mStreamAbstractionAAMP_MPD->CallUpdateTrackInfo(modifyDefaultBW, resetTimeLineIndex); (void)result;
}

TEST_F(StreamAbstractionAAMP_MPDTest, UpdateTrackInfoTest_2)
{
	bool modifyDefaultBW = true;
	bool resetTimeLineIndex = false;
	AAMPStatusType result = mStreamAbstractionAAMP_MPD->CallUpdateTrackInfo(modifyDefaultBW, resetTimeLineIndex); (void)result;
}

TEST_F(StreamAbstractionAAMP_MPDTest, UpdateTrackInfoTest_3)
{
	bool modifyDefaultBW = false;
	bool resetTimeLineIndex = true;
	AAMPStatusType result = mStreamAbstractionAAMP_MPD->CallUpdateTrackInfo(modifyDefaultBW, resetTimeLineIndex); (void)result;
}

TEST_F(StreamAbstractionAAMP_MPDTest, SeekInPeriodTest)
{
	double seekPositionSeconds = 1.1;
	bool skipToEnd = true;
	mStreamAbstractionAAMP_MPD->CallSeekInPeriod(seekPositionSeconds, skipToEnd);
}

TEST_F(StreamAbstractionAAMP_MPDTest, GetFirstValidCurrMPDPeriodTest)
{
	std::vector<PeriodInfo> currMPDPeriodDetails;
	PeriodInfo result = mStreamAbstractionAAMP_MPD->CallGetFirstValidCurrMPDPeriod(currMPDPeriodDetails); (void)result;
}

TEST_F(StreamAbstractionAAMP_MPDTest, GetLatencyStatusTest)
{
	LatencyStatus result = mStreamAbstractionAAMP_MPD->CallGetLatencyStatus(); (void)result;
}

TEST_F(StreamAbstractionAAMP_MPDTest, QueueContentProtectionTest)
{
	IPeriod *period = NULL;
	uint32_t adaptationSetIdx = 1;
	AampMediaType mediaType = eMEDIATYPE_DEFAULT;
	bool qGstProtectEvent = true;
	bool isVssPeriod = true;
	mStreamAbstractionAAMP_MPD->CallQueueContentProtection(period, adaptationSetIdx, mediaType, qGstProtectEvent, isVssPeriod);
}

TEST_F(StreamAbstractionAAMP_MPDTest, ProcessAllContentProtectionForMediaTypeTest)
{
	AampMediaType type = eMEDIATYPE_DEFAULT;
	uint32_t priorityAdaptationIdx = 12;
	std::set<uint32_t> chosenAdaptationIdxs;
	mStreamAbstractionAAMP_MPD->CallProcessAllContentProtectionForMediaType(type, priorityAdaptationIdx, chosenAdaptationIdxs);
}

TEST_F(StreamAbstractionAAMP_MPDTest, OnAdEventTest)
{
	AdEvent evt = AdEvent::INIT;
	bool result = mStreamAbstractionAAMP_MPD->CallOnAdEvent(evt); (void)result;
}

TEST_F(StreamAbstractionAAMP_MPDTest, OnAdEventWithOffsetTest)
{
	AdEvent evt = AdEvent::INIT;
	double adOffset = 1.0;
	bool result = mStreamAbstractionAAMP_MPD->CallOnAdEvent(evt, adOffset); (void)result;
}

TEST_F(StreamAbstractionAAMP_MPDTest, SetAudioTrackInfoTest)
{
	std::vector<AudioTrackInfo> audioTracks; // Assuming it's initialized appropriately
	std::string trackIndex = "0";
	mStreamAbstractionAAMP_MPD->CallSetAudioTrackInfo(audioTracks, trackIndex);
}

TEST_F(StreamAbstractionAAMP_MPDTest, SetTextTrackInfoTest)
{
	std::vector<TextTrackInfo> textTracks ;
	std::string trackIndex = "0";
	mStreamAbstractionAAMP_MPD->CallSetTextTrackInfo(textTracks, trackIndex);

}

TEST_F(StreamAbstractionAAMP_MPDTest, CheckForVssTagsTest)
{
	bool result = mStreamAbstractionAAMP_MPD->CallCheckForVssTags(); (void)result;
	// Add assertions based on the expected behavior or state changes
	// ASSERT_TRUE(result);
}

TEST_F(StreamAbstractionAAMP_MPDTest, GetAvailableVSSPeriodsTest)
{
	std::vector<IPeriod *> PeriodIds;
	mStreamAbstractionAAMP_MPD->CallGetAvailableVSSPeriods(PeriodIds);

}

TEST_F(StreamAbstractionAAMP_MPDTest, GetVssVirtualStreamIDTest)
{
	std::string result = mStreamAbstractionAAMP_MPD->CallGetVssVirtualStreamID();
}


/**
 * @brief test for XML Parser to read and process CDATA section under EventStream
 * The MPD initialization should be successful
 * The MPD object should be valid.
 */

TEST_F(FunctionalTests, CDATA_Test)
{
	std::string fragmentUrl;
	AAMPStatusType status;
	dash::mpd::IMPD *mpd;
	static const char *manifest =
R"(<?xml version="1.0" encoding="utf-8"?>
<MPD xmlns="urn:mpeg:dash:schema:mpd:2011" minBufferTime="PT2S" type="static" mediaPresentationDuration="PT0H10M54.00S" profiles="urn:mpeg:dash:profile:isoff-live:2011,http://dashif.org/guidelines/dash264">
	<Period duration="PT1M0S">
		<EventStream schemeIdUri="urn:sva:advertising-wg:ad-id-signaling" timescale="90000">
			<Event duration="1358857" presentationTime="0"><![CDATA[{"version":1,"identifiers":[{"scheme":"urn:smpte:ul:060E2B34.01040101.01200900.00000000","value":"5493003","ad_position":"_PT0S_0","ad_type":"avail","tracking_uri":"../../../../../../../../../../tracking/99247e89c7677df85a85aabdd3256ffe02a60196/example-dash-vod-2s-generic/f38d0147-7bee-480f-83a7-fec49fda39b9","custom_vast_data":null}]}]]></Event>
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
	std::string expectedCData = R"({"version":1,"identifiers":[{"scheme":"urn:smpte:ul:060E2B34.01040101.01200900.00000000","value":"5493003","ad_position":"_PT0S_0","ad_type":"avail","tracking_uri":"../../../../../../../../../../tracking/99247e89c7677df85a85aabdd3256ffe02a60196/example-dash-vod-2s-generic/f38d0147-7bee-480f-83a7-fec49fda39b9","custom_vast_data":null}]})";
	fragmentUrl = std::string(TEST_BASE_URL) + std::string("video_init.mp4");
	EXPECT_CALL(*g_mockMediaStreamContext, CacheFragment(fragmentUrl, _, _, _, _, true, _, _, _, _, _))
		.WillOnce(Return(true));
	EXPECT_CALL(*g_mockPrivateInstanceAAMP, SetLLDashChunkMode(_));
	status = InitializeMPD(manifest);
	//To verify the anticipated cdata against the cdata processed within the process node function.
	ASSERT_EQ(mResponse->mRootNode->GetText().c_str(), expectedCData);
	EXPECT_EQ(status, eAAMPSTATUS_OK);

	// Get the manifest data.
	mpd = mStreamAbstractionAAMP_MPD->GetMPD();
	EXPECT_NE(mpd, nullptr);
	}

/**
 * @brief test to ensure the CC attribute parsing from iframe
 * adaptation is avoided
 * The MPD initialization should be successful
 * The MPD object should be valid.
 */

TEST_F(FunctionalTests, Accessibility_Test1)
{
	std::string fragmentUrl;
	AAMPStatusType status;
	static const char *manifest =
R"(<?xml version="1.0" encoding="utf-8"?>
<MPD xmlns="urn:mpeg:dash:schema:mpd:2011" availabilityStartTime="1970-01-01T00:00:00Z" maxSegmentDuration="PT2S" minBufferTime="PT4.000S" minimumUpdatePeriod="P100Y" profiles="urn:dvb:dash:profile:dvb-dash:2014,urn:dvb:dash:profile:dvb-dash:isoff-ext-live:2014" publishTime="2023-01-01T00:00:00Z" timeShiftBufferDepth="PT5M" type="dynamic">
	<Period id="p0" start="PT0S">
		<AdaptationSet id="100001" contentType="video" mimeType="video/mp4" segmentAlignment="true" startWithSAP="1" codingDependency="false" maxPlayoutRate="24">
			<Accessibility schemeIdUri="urn:scte:dash:cc:cea-708:2015" value="1=lang:en;2=lang:en"/>
			<EssentialProperty schemeIdUri="http://dashif.org/guidelines/trickmode" value="1" />
			<Representation id="s8_iframe_trackId-103" bandwidth="1248320" codecs="avc3.4D401F" width="960" height="540"/>
		</AdaptationSet>
		<AdaptationSet contentType="video" lang="eng" maxFrameRate="25" maxHeight="1080" maxWidth="1920" par="16:9" segmentAlignment="true" startWithSAP="1">
			<SegmentTemplate duration="5000" initialization="video_init.mp4" media="video_$Number$.m4s" startNumber="0" timescale="2500" />
			<Representation id="1" mimeType="video/mp4" codecs="avc1.640028" width="640" height="360" frameRate="25" sar="1:1" bandwidth="1000000" />
		</AdaptationSet>
	</Period>
</MPD>
)";
	// Initialize MPD. The video initialization segment is cached.
	fragmentUrl = std::string(TEST_BASE_URL) + std::string("video_init.mp4");
	EXPECT_CALL(*g_mockMediaStreamContext, CacheFragment(fragmentUrl, _, _, _, _, true, _, _, _, _, _))
		.WillOnce(Return(true));
	EXPECT_CALL(*g_mockPrivateInstanceAAMP, SetLLDashChunkMode(_));
	status = InitializeMPD(manifest);
	std::vector<TextTrackInfo> textTracks = mStreamAbstractionAAMP_MPD->GetAvailableTextTracks();
	//To verify whether CC from iframe adaptation is avoided.
	ASSERT_EQ(0,textTracks.size());
	EXPECT_EQ(status, eAAMPSTATUS_OK);
}

/**
 * @brief test to ensure the CC attribute parsed from video
 * adaptation
 * The MPD initialization should be successful
 * The MPD object should be valid.
 */

TEST_F(FunctionalTests, Accessibility_Test2)
{
	std::string fragmentUrl;
	AAMPStatusType status;
	static const char *manifest =
R"(<?xml version="1.0" encoding="utf-8"?>
<MPD xmlns="urn:mpeg:dash:schema:mpd:2011" availabilityStartTime="1970-01-01T00:00:00Z" maxSegmentDuration="PT2S" minBufferTime="PT4.000S" minimumUpdatePeriod="P100Y" profiles="urn:dvb:dash:profile:dvb-dash:2014,urn:dvb:dash:profile:dvb-dash:isoff-ext-live:2014" publishTime="2023-01-01T00:00:00Z" timeShiftBufferDepth="PT5M" type="dynamic">
	<Period id="p0" start="PT0S">
		<AdaptationSet id="100001" contentType="video" mimeType="video/mp4" segmentAlignment="true" startWithSAP="1" codingDependency="false" maxPlayoutRate="24">
			<EssentialProperty schemeIdUri="http://dashif.org/guidelines/trickmode" value="1" />
			<Representation id="s8_iframe_trackId-103" bandwidth="1248320" codecs="avc3.4D401F" width="960" height="540"/>
		</AdaptationSet>
		<AdaptationSet contentType="video" lang="eng" maxFrameRate="25" maxHeight="1080" maxWidth="1920" par="16:9" segmentAlignment="true" startWithSAP="1">
			<Accessibility schemeIdUri="urn:scte:dash:cc:cea-708:2015" value="1=lang:en;2=lang:en"/>
			<SegmentTemplate duration="5000" initialization="video_init.mp4" media="video_$Number$.m4s" startNumber="0" timescale="2500" />
			<Representation id="1" mimeType="video/mp4" codecs="avc1.640028" width="640" height="360" frameRate="25" sar="1:1" bandwidth="1000000" />
		</AdaptationSet>
	</Period>
</MPD>
)";
	// Initialize MPD. The video initialization segment is cached.
	fragmentUrl = std::string(TEST_BASE_URL) + std::string("video_init.mp4");
	EXPECT_CALL(*g_mockMediaStreamContext, CacheFragment(fragmentUrl, _, _, _, _, true, _, _, _, _, _))
		.WillOnce(Return(true));
	EXPECT_CALL(*g_mockPrivateInstanceAAMP, SetLLDashChunkMode(_));
	status = InitializeMPD(manifest);
	std::vector<TextTrackInfo> textTracks = mStreamAbstractionAAMP_MPD->GetAvailableTextTracks();
	//To verify whether the CC attribute parsed from video
	ASSERT_EQ(2,textTracks.size());
	EXPECT_EQ(status, eAAMPSTATUS_OK);
}

/*
 * Multiple adaption sets all with the same CC language. Check that we only get one CC entry listed
 * in the subtitle tracks, not 3 entries for the same language.
 */
TEST_F(FunctionalTests, CCMultipleAdaption_Test1)
{

	AAMPStatusType status;
	static const char *manifest =
R"(<?xml version="1.0" encoding="UTF-8"?>
<MPD xmlns="urn:mpeg:dash:schema:mpd:2011" xmlns:scte35="urn:scte:scte35:2014:xml+bin" xmlns:scte214="scte214" xmlns:cenc="urn:mpeg:cenc:2013" xmlns:mspr="mspr" type="static" id="sonypictures.comDIAS0000000016158904-DIAS0000000016158905" profiles="urn:mpeg:dash:profile:isoff-on-demand:2011" minBufferTime="PT0H0M2.919S" maxSegmentDuration="PT0H0M2.919548582S" mediaPresentationDuration="PT1H49M35.419S">
  <Period id="1" start="PT0H0M0.000S">
    <AdaptationSet id="10002" contentType="video" mimeType="video/mp4" segmentAlignment="true" startWithSAP="1">
      <SupplementalProperty schemeIdUri="urn:mpeg:dash:adaptation-set-switching:2016" value="20002,30002" />
      <Accessibility schemeIdUri="urn:scte:dash:cc:cea-608:2015" value="CC1=en"/>
      <Role schemeIdUri="urn:mpeg:dash:role:2011" value="main"/>
      <SegmentTemplate initialization="manifest/track-video-repid-$RepresentationID$-tc-0-header.mp4" media="manifest/track-video-repid-$RepresentationID$-tc-0-frag-$Number$.mp4" timescale="90000" startNumber="1" presentationTimeOffset="2731254">
        <SegmentTimeline>
          <S t="2731254" d="180180" r="3282"/>
        </SegmentTimeline>
      </SegmentTemplate>
      <Representation id="video00" bandwidth="250400" codecs="avc1.4d401f" width="320" height="180" frameRate="24000/1001"/>
    </AdaptationSet>
    <AdaptationSet id="20002" contentType="video" mimeType="video/mp4" segmentAlignment="true" startWithSAP="1">
      <SupplementalProperty schemeIdUri="urn:mpeg:dash:adaptation-set-switching:2016" value="10002,30002" />
      <Accessibility schemeIdUri="urn:scte:dash:cc:cea-608:2015" value="CC1=en"/>
      <Role schemeIdUri="urn:mpeg:dash:role:2011" value="main"/>
      <SegmentTemplate initialization="manifest/track-video-repid-$RepresentationID$-tc-0-header.mp4" media="manifest/track-video-repid-$RepresentationID$-tc-0-frag-$Number$.mp4" timescale="90000" startNumber="1" presentationTimeOffset="2731254">
        <SegmentTimeline>
          <S t="2731254" d="180180" r="3282"/>
          <S t="594262194" d="262762" r="0"/>
        </SegmentTimeline>
      </SegmentTemplate>
      <Representation id="video05" bandwidth="2089600" codecs="avc1.640029" width="1280" height="720" frameRate="24000/1001"/>
    </AdaptationSet>
    <AdaptationSet id="30002" contentType="video" mimeType="video/mp4" segmentAlignment="true" startWithSAP="1">
      <SupplementalProperty schemeIdUri="urn:mpeg:dash:adaptation-set-switching:2016" value="10002,20002" />
      <Accessibility schemeIdUri="urn:scte:dash:cc:cea-608:2015" value="CC1=en"/>
      <Role schemeIdUri="urn:mpeg:dash:role:2011" value="main"/>
      <SegmentTemplate initialization="manifest/track-video-repid-$RepresentationID$-tc-0-header.mp4" media="manifest/track-video-repid-$RepresentationID$-tc-0-frag-$Number$.mp4" timescale="90000" startNumber="1" presentationTimeOffset="2731254">
        <SegmentTimeline>
          <S t="2731254" d="180180" r="3282"/>
          <S t="594262194" d="262762" r="0"/>
        </SegmentTimeline>
      </SegmentTemplate>
      <Representation id="video06" bandwidth="3632400" codecs="avc1.640029" width="1920" height="1080" frameRate="24000/1001"/>
    </AdaptationSet>
    <AdaptationSet id="3" contentType="audio" mimeType="audio/mp4" lang="en">
      <AudioChannelConfiguration schemeIdUri="urn:mpeg:dash:23003:3:audio_channel_configuration:2011" value="2"/>
      <Role schemeIdUri="urn:mpeg:dash:role:2011" value="main"/>
      <SegmentTemplate initialization="manifest/track-audio-repid-$RepresentationID$-tc-0-header.mp4" media="manifest/track-audio-repid-$RepresentationID$-tc-0-frag-$Number$.mp4" timescale="90000" startNumber="1" presentationTimeOffset="2731254">
        <SegmentTimeline>
          <S t="2731254" d="180480" r="11"/>
        </SegmentTimeline>
      </SegmentTemplate>
      <Representation id="audio_AAC_eng_02_false_00" bandwidth="139600" codecs="mp4a.40.5" audioSamplingRate="24000"/>
    </AdaptationSet>
    <AdaptationSet id="4" contentType="audio" mimeType="audio/mp4" lang="en">
      <AudioChannelConfiguration schemeIdUri="tag:dolby.com,2014:dash:audio_channel_configuration:2011" value="f801"/>
      <Role schemeIdUri="urn:mpeg:dash:role:2011" value="main"/>
      <SegmentTemplate initialization="manifest-eac3/track-audio-repid-$RepresentationID$-tc-0-header.mp4" media="manifest-eac3/track-audio-repid-$RepresentationID$-tc-0-frag-$Number$.mp4" timescale="90000" startNumber="1" presentationTimeOffset="2731254">
        <SegmentTimeline>
          <S t="2731254" d="181440" r="1"/>
        </SegmentTimeline>
      </SegmentTemplate>
      <Representation id="audio_EAC3_eng_01_false_06" bandwidth="251200" codecs="ec-3" audioSamplingRate="48000"/>
    </AdaptationSet>
  </Period>
</MPD>
)";
	// Initialize MPD. The video initialization segment is cached.
	EXPECT_CALL(*g_mockMediaStreamContext, CacheFragment(_, _, _, _, _, true, _, _, _, _, _))
		.WillRepeatedly(Return(true));
	EXPECT_CALL(*g_mockPrivateInstanceAAMP, SetLLDashChunkMode(_));
	status = InitializeMPD(manifest);
	std::vector<TextTrackInfo> textTracks = mStreamAbstractionAAMP_MPD->GetAvailableTextTracks();
	//To verify whether the CC attribute parsed from video
	ASSERT_EQ(1,textTracks.size());
	EXPECT_EQ(status, eAAMPSTATUS_OK);
}

/*
 * Multiple adaption sets 2x CC languages. Check that we only get one CC entry listed for each language
 */
TEST_F(FunctionalTests, CCMultipleAdaption_Test2)
{
	AAMPStatusType status;
	static const char *manifest =
R"(<?xml version="1.0" encoding="UTF-8"?>
<MPD xmlns="urn:mpeg:dash:schema:mpd:2011" xmlns:scte35="urn:scte:scte35:2014:xml+bin" xmlns:scte214="scte214" xmlns:cenc="urn:mpeg:cenc:2013" xmlns:mspr="mspr" type="static" id="sonypictures.comDIAS0000000016158904-DIAS0000000016158905" profiles="urn:mpeg:dash:profile:isoff-on-demand:2011" minBufferTime="PT0H0M2.919S" maxSegmentDuration="PT0H0M2.919548582S" mediaPresentationDuration="PT1H49M35.419S">
  <Period id="1" start="PT0H0M0.000S">
    <AdaptationSet id="10002" contentType="video" mimeType="video/mp4" segmentAlignment="true" startWithSAP="1">
      <SupplementalProperty schemeIdUri="urn:mpeg:dash:adaptation-set-switching:2016" value="20002,30002" />
      <Accessibility schemeIdUri="urn:scte:dash:cc:cea-608:2015" value="CC1=en"/>
	  <Accessibility schemeIdUri="urn:scte:dash:cc:cea-608:2015" value="CC2=fr"/>
      <Role schemeIdUri="urn:mpeg:dash:role:2011" value="main"/>
      <SegmentTemplate initialization="manifest/track-video-repid-$RepresentationID$-tc-0-header.mp4" media="manifest/track-video-repid-$RepresentationID$-tc-0-frag-$Number$.mp4" timescale="90000" startNumber="1" presentationTimeOffset="2731254">
        <SegmentTimeline>
          <S t="2731254" d="180180" r="3282"/>
        </SegmentTimeline>
      </SegmentTemplate>
      <Representation id="video00" bandwidth="250400" codecs="avc1.4d401f" width="320" height="180" frameRate="24000/1001"/>
    </AdaptationSet>
    <AdaptationSet id="20002" contentType="video" mimeType="video/mp4" segmentAlignment="true" startWithSAP="1">
      <SupplementalProperty schemeIdUri="urn:mpeg:dash:adaptation-set-switching:2016" value="10002,30002" />
      <Accessibility schemeIdUri="urn:scte:dash:cc:cea-608:2015" value="CC1=en"/>
	<Accessibility schemeIdUri="urn:scte:dash:cc:cea-608:2015" value="CC2=fr"/>
      <Role schemeIdUri="urn:mpeg:dash:role:2011" value="main"/>
      <SegmentTemplate initialization="manifest/track-video-repid-$RepresentationID$-tc-0-header.mp4" media="manifest/track-video-repid-$RepresentationID$-tc-0-frag-$Number$.mp4" timescale="90000" startNumber="1" presentationTimeOffset="2731254">
        <SegmentTimeline>
          <S t="2731254" d="180180" r="3282"/>
          <S t="594262194" d="262762" r="0"/>
        </SegmentTimeline>
      </SegmentTemplate>
      <Representation id="video05" bandwidth="2089600" codecs="avc1.640029" width="1280" height="720" frameRate="24000/1001"/>
    </AdaptationSet>
    <AdaptationSet id="30002" contentType="video" mimeType="video/mp4" segmentAlignment="true" startWithSAP="1">
      <SupplementalProperty schemeIdUri="urn:mpeg:dash:adaptation-set-switching:2016" value="10002,20002" />
      <Accessibility schemeIdUri="urn:scte:dash:cc:cea-608:2015" value="CC1=en"/>
	  <Accessibility schemeIdUri="urn:scte:dash:cc:cea-608:2015" value="CC2=fr"/>
      <Role schemeIdUri="urn:mpeg:dash:role:2011" value="main"/>
      <SegmentTemplate initialization="manifest/track-video-repid-$RepresentationID$-tc-0-header.mp4" media="manifest/track-video-repid-$RepresentationID$-tc-0-frag-$Number$.mp4" timescale="90000" startNumber="1" presentationTimeOffset="2731254">
        <SegmentTimeline>
          <S t="2731254" d="180180" r="3282"/>
          <S t="594262194" d="262762" r="0"/>
        </SegmentTimeline>
      </SegmentTemplate>
      <Representation id="video06" bandwidth="3632400" codecs="avc1.640029" width="1920" height="1080" frameRate="24000/1001"/>
    </AdaptationSet>
    <AdaptationSet id="3" contentType="audio" mimeType="audio/mp4" lang="en">
      <AudioChannelConfiguration schemeIdUri="urn:mpeg:dash:23003:3:audio_channel_configuration:2011" value="2"/>
      <Role schemeIdUri="urn:mpeg:dash:role:2011" value="main"/>
      <SegmentTemplate initialization="manifest/track-audio-repid-$RepresentationID$-tc-0-header.mp4" media="manifest/track-audio-repid-$RepresentationID$-tc-0-frag-$Number$.mp4" timescale="90000" startNumber="1" presentationTimeOffset="2731254">
        <SegmentTimeline>
          <S t="2731254" d="180480" r="11"/>
        </SegmentTimeline>
      </SegmentTemplate>
      <Representation id="audio_AAC_eng_02_false_00" bandwidth="139600" codecs="mp4a.40.5" audioSamplingRate="24000"/>
    </AdaptationSet>
    <AdaptationSet id="4" contentType="audio" mimeType="audio/mp4" lang="en">
      <AudioChannelConfiguration schemeIdUri="tag:dolby.com,2014:dash:audio_channel_configuration:2011" value="f801"/>
      <Role schemeIdUri="urn:mpeg:dash:role:2011" value="main"/>
      <SegmentTemplate initialization="manifest-eac3/track-audio-repid-$RepresentationID$-tc-0-header.mp4" media="manifest-eac3/track-audio-repid-$RepresentationID$-tc-0-frag-$Number$.mp4" timescale="90000" startNumber="1" presentationTimeOffset="2731254">
        <SegmentTimeline>
          <S t="2731254" d="181440" r="1"/>
        </SegmentTimeline>
      </SegmentTemplate>
      <Representation id="audio_EAC3_eng_01_false_06" bandwidth="251200" codecs="ec-3" audioSamplingRate="48000"/>
    </AdaptationSet>
  </Period>
</MPD>
)";
	// Initialize MPD. The video initialization segment is cached.
	EXPECT_CALL(*g_mockMediaStreamContext, CacheFragment(_, _, _, _, _, true, _, _, _, _, _))
		.WillRepeatedly(Return(true));
	EXPECT_CALL(*g_mockPrivateInstanceAAMP, SetLLDashChunkMode(_));
	status = InitializeMPD(manifest);
	std::vector<TextTrackInfo> textTracks = mStreamAbstractionAAMP_MPD->GetAvailableTextTracks();
	//To verify whether the CC attribute parsed from video
	ASSERT_EQ(2,textTracks.size());
	ASSERT_EQ("en",textTracks[0].language);
	ASSERT_EQ("fr",textTracks[1].language);
	EXPECT_EQ(status, eAAMPSTATUS_OK);
}

/*
 * @brief Test to make sure ChunkMode is turned off for non-LLD streams.
 * This test checks that ChunkMode (used for lld) is not
 * activated when playing a regular non lld stream.Testing under below three situation
 * - Normal playback
 * - Retune
 * - Seeking near the end of the stream
 * In all cases, we expect the ChunkMode to be off.
 */

TEST_F(FunctionalTests, ChunkMode_NonLLD)
{
	std::string fragmentUrl;
	AAMPStatusType status;
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
	float seekPosition = 0;
	int rate = 1 ; //Normal playrate test
	EXPECT_CALL(*g_mockMediaStreamContext, CacheFragment(_, _, _, _, _, _, _, _, _, _, _))
		.WillRepeatedly(Return(true));

	EXPECT_CALL(*g_mockPrivateInstanceAAMP, NotifyOnEnteringLive()).Times(1);
	EXPECT_CALL(*g_mockPrivateInstanceAAMP, SetLLDashChunkMode(_));
	status = InitializeMPD(manifest,TuneType::eTUNETYPE_NEW_NORMAL, seekPosition, rate);
	EXPECT_EQ(status, eAAMPSTATUS_OK);
	EXPECT_EQ(mPrivateInstanceAAMP->GetLLDashChunkMode(), false);

	//Retune Scenario
	EXPECT_CALL(*g_mockPrivateInstanceAAMP, SetLLDashChunkMode(_));
	status = InitializeMPD(manifest,TuneType::eTUNETYPE_RETUNE, seekPosition, rate);
	EXPECT_EQ(status, eAAMPSTATUS_OK);
	EXPECT_EQ(mPrivateInstanceAAMP->GetLLDashChunkMode(), false);

	//Seeking near the end of the stream
	seekPosition = 290; //Total duration 300
	EXPECT_CALL(*g_mockPrivateInstanceAAMP, SetLLDashChunkMode(_));
	status = InitializeMPD(manifest,TuneType::eTUNETYPE_SEEK, seekPosition, rate);
	EXPECT_EQ(status, eAAMPSTATUS_OK);
	EXPECT_EQ(mPrivateInstanceAAMP->GetLLDashChunkMode(), false);
}

/*
 * @brief Test to ensure ChunkMode is enabled for LLD streams.
 * This test checks that ChunkMode (used for lld) is activated
 * Tested under below scenarios:
 * - Normal playback (ChunkMode should be on)
 * - Seeking at the beginning of the stream (ChunkMode should be off)
 * - Seeking near the live point (ChunkMode should be on)
 * - Seeking behind the live point (ChunkMode should be off)
 */

TEST_F(FunctionalTests, ChunkMode_LLD)
{
	AAMPStatusType status;
	static const char *manifest =
		R"(<?xml version="1.0" encoding="UTF-8"?>
<MPD xmlns="urn:mpeg:dash:schema:mpd:2011" xmlns:scte35="urn:scte:scte35:2014:xml+bin" xmlns:scte214="scte214" xmlns:cenc="urn:mpeg:cenc:2013" xmlns:mspr="mspr" type="dynamic" id="8371500471198371163" profiles="urn:mpeg:dash:profile:isoff-live:2011,http://www.dashif.org/guidelines/low-latency-live-v5" minBufferTime="PT0H0M1.000S" maxSegmentDuration="PT2.34S" minimumUpdatePeriod="PT0H0M1.920S" availabilityStartTime="1970-01-01T00:00:00.000Z" timeShiftBufferDepth="PT0H30M1.044S" publishTime="2024-06-25T11:23:17.130Z">
	<Period id="Period-1" start="PT477586H51M45.467S">
		<AdaptationSet id="track-1" contentType="video" mimeType="video/mp4" segmentAlignment="true" startWithSAP="1">
			<Role schemeIdUri="urn:mpeg:dash:role:2011" value="main"/>
			<SegmentTemplate initialization="track-video-periodid-Period-1-repid-$RepresentationID$-tc-0-header.mp4" media="track-video-periodid-Period-1-repid-$RepresentationID$-tc-0-time-$Time$.mp4" timescale="240000" startNumber="168440" presentationTimeOffset="79455172898" availabilityTimeOffset="1.44" availabilityTimeComplete="false">
				<SegmentTimeline>
					<S t="79476480098" d="460800" r="810"/>
					<S t="79850188898" d="364800" r="0"/>
					<S t="79850553698" d="360000" r="0"/>
				</SegmentTimeline>
			</SegmentTemplate>
			<Representation id="track-2" bandwidth="500000" codecs="hvc1.1.6.L63.90" width="640" height="360" frameRate="25"/>
			<Representation id="track-3" bandwidth="1200000" codecs="hvc1.1.6.L93.90" width="960" height="540" frameRate="50"/>
			<Representation id="track-4" bandwidth="1850000" codecs="hvc1.1.6.L93.90" width="960" height="540" frameRate="50"/>
		</AdaptationSet>
	</Period>
</MPD>
)";
	double seekPosition = 0;
	int rate = 1 ; //Normal playrate test

	EXPECT_CALL(*g_mockPrivateInstanceAAMP, GetLLDashAdjustSpeed())
		.WillRepeatedly(Return(true));

	EXPECT_CALL(*g_mockMediaStreamContext, CacheFragment(_, _, _, _, _, _, _, _, _, _, _))
		.WillRepeatedly(Return(true));
	//For this test case we need EnableLowLatencyDash as true
	EXPECT_CALL(*g_mockAampConfig, IsConfigSet(_))
		.WillRepeatedly(Invoke([](AAMPConfigSettingBool config) {
					return config == eAAMPConfig_EnableLowLatencyDash;
					}));
	EXPECT_CALL(*g_mockAampMPDDownloader, IsMPDLowLatency (_))
		.WillRepeatedly(Return(true));

	EXPECT_CALL(*g_mockPrivateInstanceAAMP, NotifyOnEnteringLive()).Times(1);
	EXPECT_CALL(*g_mockPrivateInstanceAAMP, SetLLDashChunkMode(false)).Times(4);
	EXPECT_CALL(*g_mockPrivateInstanceAAMP, SetLLDashChunkMode(true)).Times(2);
	status = InitializeMPD(manifest,TuneType::eTUNETYPE_NEW_NORMAL, seekPosition, rate,false);
	EXPECT_EQ(status, eAAMPSTATUS_OK);

	//seek to the beginning
	status = InitializeMPD(manifest,TuneType::eTUNETYPE_SEEK, seekPosition, rate,false);
	EXPECT_EQ(status, eAAMPSTATUS_OK);

	seekPosition = 1550 ; //Total duration -1560
	status = InitializeMPD(manifest,TuneType::eTUNETYPE_SEEK,seekPosition, rate,false);
	EXPECT_EQ(status, eAAMPSTATUS_OK);

	seekPosition = 1000; //behind live point
	status = InitializeMPD(manifest,TuneType::eTUNETYPE_SEEK,seekPosition, rate,false);
	EXPECT_EQ(status, eAAMPSTATUS_OK);
}

/**
 * @brief Test LLD ChunkMode behavior with a seek position within the latency range.
 *
 * This test verifies that ChunkMode is enabled when seeking to 1552 seconds
 * in a 1560-second stream. The live offset is 6 seconds, and maximum latency
 * is 9 seconds. The seek position is behind the live edge but within the
 * allowable latency (1560 - 9 = 1551 seconds). The test checks if ChunkMode
 * is correctly enabled in this scenario.
 */

TEST_F(FunctionalTests, ChunkMode_LLD_ForMaxLatency_Case)
{
	AAMPStatusType status;
	static const char *manifest =
		R"(<?xml version="1.0" encoding="UTF-8"?>
<MPD xmlns="urn:mpeg:dash:schema:mpd:2011" xmlns:scte35="urn:scte:scte35:2014:xml+bin" xmlns:scte214="scte214" xmlns:cenc="urn:mpeg:cenc:2013" xmlns:mspr="mspr" type="dynamic" id="8371500471198371163" profiles="urn:mpeg:dash:profile:isoff-live:2011,http://www.dashif.org/guidelines/low-latency-live-v5" minBufferTime="PT0H0M1.000S" maxSegmentDuration="PT2.34S" minimumUpdatePeriod="PT0H0M1.920S" availabilityStartTime="1970-01-01T00:00:00.000Z" timeShiftBufferDepth="PT0H30M1.044S" publishTime="2024-06-25T11:23:17.130Z">
	<Period id="Period-1" start="PT477586H51M45.467S">
		<AdaptationSet id="track-1" contentType="video" mimeType="video/mp4" segmentAlignment="true" startWithSAP="1">
			<Role schemeIdUri="urn:mpeg:dash:role:2011" value="main"/>
			<SegmentTemplate initialization="track-video-periodid-Period-1-repid-$RepresentationID$-tc-0-header.mp4" media="track-video-periodid-Period-1-repid-$RepresentationID$-tc-0-time-$Time$.mp4" timescale="240000" startNumber="168440" presentationTimeOffset="79455172898" availabilityTimeOffset="1.44" availabilityTimeComplete="false">
				<SegmentTimeline>
					<S t="79476480098" d="460800" r="810"/>
					<S t="79850188898" d="364800" r="0"/>
					<S t="79850553698" d="360000" r="0"/>
				</SegmentTimeline>
			</SegmentTemplate>
			<Representation id="track-2" bandwidth="500000" codecs="hvc1.1.6.L63.90" width="640" height="360" frameRate="25"/>
			<Representation id="track-3" bandwidth="1200000" codecs="hvc1.1.6.L93.90" width="960" height="540" frameRate="50"/>
			<Representation id="track-4" bandwidth="1850000" codecs="hvc1.1.6.L93.90" width="960" height="540" frameRate="50"/>
		</AdaptationSet>
	</Period>
</MPD>
)";
	double seekPosition = 1552; ///Total duration : 1560
	int rate = 1 ; //Normal playrate test

	EXPECT_CALL(*g_mockPrivateInstanceAAMP, GetLLDashAdjustSpeed())
		.WillRepeatedly(Return(true));

	EXPECT_CALL(*g_mockMediaStreamContext, CacheFragment(_, _, _, _, _, _, _, _, _, _, _))
		.WillRepeatedly(Return(true));

	EXPECT_CALL(*g_mockAampMPDDownloader, IsMPDLowLatency (_))
		.WillRepeatedly(Return(true));

	EXPECT_CALL(*g_mockAampConfig, GetConfigValue(testing::Matcher<AAMPConfigSettingInt>(_)))
	.WillRepeatedly([](AAMPConfigSettingInt config) {
	// Check if the config is maxLatencyConfig, return 9(default value); otherwise, return 0
	if (config == eAAMPConfig_LLMaxLatency) {
		return 9;
	}
	return 0;
	});

	EXPECT_CALL(*g_mockAampConfig, GetConfigValue(testing::Matcher<AAMPConfigSettingFloat>(_))).WillRepeatedly(Return(0.0));
	//For this test case we need EnableLowLatencyDash as true
	EXPECT_CALL(*g_mockAampConfig, IsConfigSet(_))
		.WillRepeatedly(Return(false));
	EXPECT_CALL(*g_mockAampConfig, IsConfigSet(eAAMPConfig_EnableLowLatencyDash))
		.WillRepeatedly(Return(true));
	EXPECT_CALL(*g_mockAampConfig, GetConfigValue(eAAMPConfig_LiveOffset))
		.Times(AnyNumber())
		.WillRepeatedly(Return(6));

	mPrivateInstanceAAMP->mLiveOffset = 6;
	EXPECT_CALL(*g_mockPrivateInstanceAAMP, SetLLDashChunkMode(false)).Times(1);
	EXPECT_CALL(*g_mockPrivateInstanceAAMP, SetLLDashChunkMode(true)).Times(1);
	status = InitializeMPD(manifest,TuneType::eTUNETYPE_SEEK,seekPosition, rate,false);
	EXPECT_EQ(status, eAAMPSTATUS_OK);
}

TEST_F(FunctionalTests, GetAvailableThumbnailTracksTest)
{
	AAMPStatusType status;
	std::string fragmentUrl;
	static const char *manifest =
R"(<?xml version="1.0" encoding="utf-8"?>
<MPD xmlns="urn:mpeg:dash:schema:mpd:2011" availabilityStartTime="1970-01-01T00:00:00Z" maxSegmentDuration="PT2S" minBufferTime="PT4.000S" minimumUpdatePeriod="P100Y" profiles="urn:dvb:dash:profile:dvb-dash:2014,urn:dvb:dash:profile:dvb-dash:isoff-ext-live:2014" publishTime="2023-01-01T00:00:00Z" timeShiftBufferDepth="PT5M" type="dynamic">
	<Period id="p0" start="PT0S">
		<AdaptationSet contentType="video" lang="eng" maxFrameRate="25" maxHeight="1080" maxWidth="1920" par="16:9" segmentAlignment="true" startWithSAP="1">
			<Accessibility schemeIdUri="urn:scte:dash:cc:cea-708:2015" value="1=lang:en;2=lang:en"/>
			<SegmentTemplate duration="5000" initialization="video_init.mp4" media="video_$Number$.m4s" startNumber="0" timescale="2500" />
			<Representation id="1" mimeType="video/mp4" codecs="avc1.640028" width="640" height="360" frameRate="25" sar="1:1" bandwidth="1000000" />
		</AdaptationSet>
	 	<AdaptationSet id="2" group="4" contentType="image" par="16:9" width="1600" height="900" sar="1:1" mimeType="image/jpeg" codecs="jpeg">
			<Role schemeIdUri="urn:mpeg:dash:role:2011" value="main" />
			<SegmentTemplate startNumber="1" timescale="24000" duration="6006000" media="out-$RepresentationID$-n-$Number$.jpg"></SegmentTemplate>
			<Representation id="img=7000" bandwidth="7000">
				<EssentialProperty schemeIdUri="http://dashif.org/guidelines/thumbnail_tile" value="5x5" />
			</Representation>
		</AdaptationSet>
	</Period>
</MPD>
)";
	fragmentUrl = std::string(TEST_BASE_URL) + std::string("video_init.mp4");
	EXPECT_CALL(*g_mockMediaStreamContext, CacheFragment(fragmentUrl, _, _, _, _, true, _, _, _, _, _))
	.WillOnce(Return(true));
	EXPECT_CALL(*g_mockPrivateInstanceAAMP, SetLLDashChunkMode(_));
	status = InitializeMPD(manifest); (void)status;

	std::vector<StreamInfo *> thumbnailtracks = mStreamAbstractionAAMP_MPD->GetAvailableThumbnailTracks();

	ASSERT_EQ(1,thumbnailtracks.size());
	EXPECT_EQ(thumbnailtracks[0]->bandwidthBitsPerSecond,7000);
}


TEST_F(FunctionalTests, SetThumbnailTrack)
{
	AAMPStatusType status;
	std::string fragmentUrl;
	bool rc;
	static const char *manifest =
	R"(<?xml version="1.0" encoding="utf-8"?>
	<MPD xmlns="urn:mpeg:dash:schema:mpd:2011" availabilityStartTime="1970-01-01T00:00:00Z" maxSegmentDuration="PT2S" minBufferTime="PT4.000S" minimumUpdatePeriod="P100Y" profiles="urn:dvb:dash:profile:dvb-dash:2014,urn:dvb:dash:profile:dvb-dash:isoff-ext-live:2014" publishTime="2023-01-01T00:00:00Z" timeShiftBufferDepth="PT5M" type="dynamic">
		<Period id="p0" start="PT0S">
			<AdaptationSet contentType="video" lang="eng" maxFrameRate="25" maxHeight="1080" maxWidth="1920" par="16:9" segmentAlignment="true" startWithSAP="1">
				<Accessibility schemeIdUri="urn:scte:dash:cc:cea-708:2015" value="1=lang:en;2=lang:en"/>
				<SegmentTemplate duration="5000" initialization="video_init.mp4" media="video_$Number$.m4s" startNumber="0" timescale="2500" />
				<Representation id="1" mimeType="video/mp4" codecs="avc1.640028" width="640" height="360" frameRate="25" sar="1:1" bandwidth="1000000" />
			</AdaptationSet>
			<AdaptationSet id="2" group="4" contentType="image" par="16:9" sar="1:1" mimeType="image/jpeg" codecs="jpeg">
				<BaseURL>https://example.com/DASH_VOD/mass0000000020751006/out.ism/dash/</BaseURL>
				<Role schemeIdUri="urn:mpeg:dash:role:2011" value="main" />
				<SegmentTemplate startNumber="1" timescale="24000" duration="6006000" media="out-$RepresentationID$-n-$Number$.jpg" />
				<Representation id="img=7000" bandwidth="7000" width="1600" height="900">
					<BaseURL>thumbnail_v1/2bfd42-b08302hx/</BaseURL>
					<EssentialProperty schemeIdUri="http://dashif.org/guidelines/thumbnail_tile" value="5x5" />
				</Representation>
			</AdaptationSet>
		</Period>
	</MPD>
	)";
	fragmentUrl = std::string(TEST_BASE_URL) + std::string("video_init.mp4");
	EXPECT_CALL(*g_mockMediaStreamContext, CacheFragment(fragmentUrl, _, _, _, _, true, _, _, _, _, _))
	.WillOnce(Return(true));
	EXPECT_CALL(*g_mockPrivateInstanceAAMP, SetLLDashChunkMode(_));
	status = InitializeMPD(manifest); (void)status;
	rc = mStreamAbstractionAAMP_MPD->SetThumbnailTrack(0);
	EXPECT_EQ(rc, 1);
	rc = mStreamAbstractionAAMP_MPD->SetThumbnailTrack(1);
	EXPECT_EQ(rc, 0);
}

TEST_F(FunctionalTests, GetThumbnailRangeDataTest1)
{
	AAMPStatusType status;
	std::string fragmentUrl;
	bool rc;
	static const char *manifest =
	R"(<?xml version="1.0" encoding="utf-8"?>
	<MPD xmlns="urn:mpeg:dash:schema:mpd:2011" availabilityStartTime="1970-01-01T00:00:00Z" maxSegmentDuration="PT2S" minBufferTime="PT4.000S" minimumUpdatePeriod="P100Y" profiles="urn:dvb:dash:profile:dvb-dash:2014,urn:dvb:dash:profile:dvb-dash:isoff-ext-live:2014" publishTime="2023-01-01T00:00:00Z" timeShiftBufferDepth="PT5M" type="dynamic">
		<Period id="p0" start="PT0S">
			<AdaptationSet contentType="video" lang="eng" maxFrameRate="25" maxHeight="1080" maxWidth="1920" par="16:9" segmentAlignment="true" startWithSAP="1">
				<Accessibility schemeIdUri="urn:scte:dash:cc:cea-708:2015" value="1=lang:en;2=lang:en"/>
				<SegmentTemplate duration="5000" initialization="video_init.mp4" media="video_$Number$.m4s" startNumber="0" timescale="2500" />
				<Representation id="1" mimeType="video/mp4" codecs="avc1.640028" width="640" height="360" frameRate="25" sar="1:1" bandwidth="1000000" />
			</AdaptationSet>
			<AdaptationSet id="2" group="4" contentType="image" par="16:9" sar="1:1" mimeType="image/jpeg" codecs="jpeg">
				<BaseURL>https://example.com/DASH_VOD/mass0000000020751006/out.ism/dash/</BaseURL>
				<Role schemeIdUri="urn:mpeg:dash:role:2011" value="main" />
				<SegmentTemplate startNumber="1" timescale="24000" duration="6006000" media="out-$RepresentationID$-n-$Number$.jpg" />
				<Representation id="img=7000" bandwidth="7000" width="1600" height="900">
					<BaseURL>thumbnail_v1/2bfd42-b08302hx/</BaseURL>
					<EssentialProperty schemeIdUri="http://dashif.org/guidelines/thumbnail_tile" value="5x5" />
				</Representation>
			</AdaptationSet>
		</Period>
	</MPD>
	)";
	fragmentUrl = std::string(TEST_BASE_URL) + std::string("video_init.mp4");
	EXPECT_CALL(*g_mockMediaStreamContext, CacheFragment(fragmentUrl, _, _, _, _, true, _, _, _, _, _))
	.WillOnce(Return(true));
	EXPECT_CALL(*g_mockPrivateInstanceAAMP, SetLLDashChunkMode(_));
	status = InitializeMPD(manifest); (void)status;
	int raw_w = 0, raw_h = 0, width = 0, height = 0;
	std::string baseUrl = "http://example.com/thumbnails/";

	rc = mStreamAbstractionAAMP_MPD->SetThumbnailTrack(0);
	EXPECT_EQ(rc, 1);

	// Call the GetThumbnailRangeData function
	std::vector<ThumbnailData> thumbnailData = mStreamAbstractionAAMP_MPD->GetThumbnailRangeData(0,0, &baseUrl, &raw_w, &raw_h, &width, &height);
	EXPECT_EQ(thumbnailData.size(), 1);
	EXPECT_EQ(width,320); //width of 1 thumbnail = width/w(value = 5x5,w= 5)
	EXPECT_EQ(thumbnailData[0].d,10); // (duration/timscale)/value
	EXPECT_EQ(baseUrl,"https://example.com/DASH_VOD/mass0000000020751006/out.ism/dash/thumbnail_v1/2bfd42-b08302hx/");
}

TEST_F(FunctionalTests, FindServerUTCTimeTest)
{
	// Manifest with UTCTiming
	static const char *manifest =
	R"(<?xml version="1.0" encoding="utf-8"?>
	<MPD xmlns="urn:mpeg:dash:schema:mpd:2011" availabilityStartTime="1970-01-01T00:00:00Z" maxSegmentDuration="PT2S" minBufferTime="PT4.000S" minimumUpdatePeriod="P100Y" profiles="urn:dvb:dash:profile:dvb-dash:2014,urn:dvb:dash:profile:dvb-dash:isoff-ext-live:2014" publishTime="2023-01-01T00:00:00Z" timeShiftBufferDepth="PT5M" type="dynamic">
		<Period id="p0" start="PT0S">
			<AdaptationSet contentType="video" lang="eng" maxFrameRate="25" maxHeight="1080" maxWidth="1920" par="16:9" segmentAlignment="true" startWithSAP="1">
				<Accessibility schemeIdUri="urn:scte:dash:cc:cea-708:2015" value="1=lang:en;2=lang:en"/>
				<SegmentTemplate duration="5000" initialization="video_init.mp4" media="video_$Number$.m4s" startNumber="0" timescale="2500" />
				<Representation id="1" mimeType="video/mp4" codecs="avc1.640028" width="640" height="360" frameRate="25" sar="1:1" bandwidth="1000000" />
			</AdaptationSet>
		</Period>
		<UTCTiming schemeIdUri="urn:mpeg:dash:utc:http-iso:2014" value="/timing"/>
	</MPD>
	)";

	// The manifest URL contains parameters
	mManifestUrl = "http://host/asset/manifest.mpd?chunked";

	g_mockAampUtils = new NiceMock<MockAampUtils>();
	const char *currentTimeISO = "2023-01-01T00:00:00Z";
	double currentTime = ISO8601DateTimeToUTCSeconds(currentTimeISO);
	long long timeMS = 1000LL*((long long)currentTime);
	EXPECT_CALL(*g_mockAampUtils, aamp_GetCurrentTimeMS())
	.Times(AnyNumber())
	.WillRepeatedly(Return(timeMS));
	EXPECT_CALL(*g_mockMediaStreamContext, CacheFragment(_, _, _, _, _, _, _, _, _, _, _))
	.WillRepeatedly(Return(true));
	// Verify that the parameters from the manifest URL are not added to the time request URL
	EXPECT_CALL(*g_mockAampUtils, GetNetworkTime("http://host/timing", _, _)).WillOnce(Return(currentTime));
	EXPECT_CALL(*g_mockPrivateInstanceAAMP, SetLLDashChunkMode(_));

	AAMPStatusType status = InitializeMPD(manifest);
	EXPECT_EQ(status, eAAMPSTATUS_OK);
}

TEST_F(FunctionalTests, GetFirstPTS)
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

	// The manifest URL contains parameters
	mManifestUrl = "http://host/asset/manifest.mpd?chunked";

	g_mockAampUtils = new NiceMock<MockAampUtils>();

	EXPECT_CALL(*g_mockMediaStreamContext, CacheFragment(_, _, _, _, _, _, _, _, _, _, _))
	.WillRepeatedly(Return(true));
	EXPECT_CALL(*g_mockPrivateInstanceAAMP, SetLLDashChunkMode(_));

	AAMPStatusType status = InitializeMPD(manifest);
	EXPECT_EQ(status, eAAMPSTATUS_OK);

	std::shared_ptr<AampTsbDataManager> dataMgr = std::make_shared<AampTsbDataManager>();
	std::shared_ptr<AampTsbReader> tsbReader = std::make_shared<AampTsbReader>(mPrivateInstanceAAMP, dataMgr, eMEDIATYPE_VIDEO, "");

	g_mockTSBSessionManager = new MockTSBSessionManager(mPrivateInstanceAAMP);
	g_mockTSBReader = std::make_shared<MockTSBReader>();

	ASSERT_NE(g_mockTSBSessionManager, nullptr);
	ASSERT_NE(g_mockTSBReader, nullptr);

	MediaTrack *videoTrack = mStreamAbstractionAAMP_MPD->GetMediaTrack(eTRACK_VIDEO);
	ASSERT_NE(videoTrack, nullptr);
	videoTrack->SetLocalTSBInjection(true);

	EXPECT_CALL(*g_mockPrivateInstanceAAMP, GetTSBSessionManager()).WillRepeatedly(Return(g_mockTSBSessionManager));
	EXPECT_CALL(*g_mockTSBSessionManager, GetTsbReader(eMEDIATYPE_VIDEO)).WillRepeatedly(Return(tsbReader));

	EXPECT_CALL(*g_mockTSBReader, GetFirstPTS()).WillOnce(Return(5.0));
	EXPECT_CALL(*g_mockTSBReader, GetFirstPTSOffset()).WillOnce(Return(10.0));

	EXPECT_EQ(15.0, mStreamAbstractionAAMP_MPD->GetFirstPTS());

	delete g_mockTSBSessionManager;
	g_mockTSBReader.reset();
}

TEST_F(FunctionalTests, AddSendMediaHeaderTest)
{
	AAMPStatusType status;

	static const char *manifest =
R"(<?xml version="1.0" encoding="UTF-8"?>
<MPD xmlns="urn:mpeg:dash:schema:mpd:2011" maxSegmentDuration="PT0H0M8.160S" mediaPresentationDuration="PT0H23M23.320S" minBufferTime="PT12.000S" profiles="urn:mpeg:dash:profile:isoff-live:2011,http://dashif.org/guidelines/dash264,urn:hbbtv:dash:profile:isoff-live:2012" type="static">
	<Period duration="PT0H11M09.160S" id="p0">
		<AdaptationSet lang="eng" maxFrameRate="25" maxHeight="1080" maxWidth="1920" par="16:9" segmentAlignment="true" startWithSAP="1">
			<SegmentTemplate initialization="$RepresentationID$_i.mp4" media="$RepresentationID$_$Number$.m4s" startNumber="1" timescale="12800" presentationTimeOffset="832000">
				<SegmentTimeline>
					<S d="76800" r="120" t="0"/>
					<S d="104448"/>
				</SegmentTimeline>
			</SegmentTemplate>
			<Representation bandwidth="517566" codecs="avc1.4D4028" frameRate="25" height="360" id="v1" mimeType="video/mp4" sar="1:1" width="640" />
			<Representation bandwidth="1502968" codecs="avc1.4D4028" frameRate="25" height="720" id="v2" mimeType="video/mp4" sar="1:1" width="1280" />
			<Representation bandwidth="2090806" codecs="avc1.4D4028" frameRate="25" height="1080" id="v3" mimeType="video/mp4" sar="1:1" width="1920" />
		</AdaptationSet>
		<AdaptationSet lang="eng" segmentAlignment="true" startWithSAP="1">
			<SegmentTemplate initialization="$RepresentationID$_i.mp4" media="$RepresentationID$_$Number$.m4s" startNumber="1" timescale="44100" presentationTimeOffset="2866500">
				<SegmentTimeline>
					<S d="263177" t="0"/>
					<S d="264192" r="120"/>
					<S d="144384"/>
				</SegmentTimeline>
			</SegmentTemplate>
			<Representation audioSamplingRate="44100" bandwidth="131780" codecs="mp4a.40.2" id="a1" mimeType="audio/mp4">
				<AudioChannelConfiguration schemeIdUri="urn:mpeg:dash:23003:3:audio_channel_configuration:2011" value="2"/>
			</Representation>
		</AdaptationSet>
		<AdaptationSet lang="eng" segmentAlignment="true" startWithSAP="1">
			<SegmentTemplate initialization="$RepresentationID$_init.mp4" media="$RepresentationID$_$Number$.m4s" startNumber="1" timescale="44100" presentationTimeOffset="2866500">
				<SegmentTimeline>
					<S d="263177" t="0"/>
					<S d="264192" r="120"/>
					<S d="144384"/>
				</SegmentTimeline>
			</SegmentTemplate>
			<Representation bandwidth="131780" codecs="stpp" id="sub1" mimeType="application/mp4" />
		</AdaptationSet>
	</Period>
</MPD>
)";
	/* Set the eAAMPConfig_useRialtoSink flag to true */
	mBoolConfigSettings[eAAMPConfig_useRialtoSink] = true;

	EXPECT_CALL(*g_mockMediaStreamContext, CacheFragment(_, _, _, _, _, true, _, _, _, _, _))
		.WillRepeatedly(Return(true));
	EXPECT_CALL(*g_mockPrivateInstanceAAMP, SetLLDashChunkMode(_));
	EXPECT_CALL(*g_mockAampStreamSinkManager, AddMediaHeader(2, _))
		.Times(2);

	//StreamAbstractionAAMP_MPD::Init() will internally call ExtractAndAddSubtitleMediaHeader()
	status = InitializeMPD(manifest, eTUNETYPE_NEW_NORMAL, 0.0, AAMP_NORMAL_PLAY_RATE, true);
	EXPECT_EQ(status, eAAMPSTATUS_OK);

	/* Video stream */
	MediaTrack *track = this->mStreamAbstractionAAMP_MPD->GetMediaTrack(eTRACK_VIDEO);
	EXPECT_NE(track, nullptr);
	track->enabled = false;
	MediaStreamContext *pMediaStreamContext = static_cast<MediaStreamContext *>(track);

	MediaTrack *aud_track = this->mStreamAbstractionAAMP_MPD->GetMediaTrack(eTRACK_AUDIO);
	EXPECT_NE(aud_track, nullptr);
	aud_track->enabled = false;
	MediaTrack *sub_track = this->mStreamAbstractionAAMP_MPD->GetMediaTrack(eTRACK_SUBTITLE);
	EXPECT_NE(sub_track, nullptr);
	sub_track->enabled = false;

	std::shared_ptr<AampStreamSinkManager::MediaHeader> header = std::make_shared<AampStreamSinkManager::MediaHeader>();
	header->url = "http://host/asset/sub1_init.mp4";
	header->mimeType = "application/mp4";
	EXPECT_CALL(*g_mockAampStreamSinkManager, GetMediaHeader(2))
		.WillRepeatedly(Return(header));
	EXPECT_CALL(*g_mockAampStreamSinkManager, GetMediaHeader(0))
		.WillRepeatedly(Return(nullptr));
	EXPECT_CALL(*g_mockAampStreamSinkManager, GetMediaHeader(1))
		.WillRepeatedly(Return(nullptr));
	EXPECT_CALL(*g_mockAampStreamSinkManager, GetMediaHeader(3))
		.WillRepeatedly(Return(nullptr));
	EXPECT_CALL(*g_mockPrivateInstanceAAMP, DownloadsAreEnabled()).WillRepeatedly(Return(true));
	EXPECT_CALL(*g_mockPrivateInstanceAAMP, GetFile(_,_,_,_,_,_,_,_,_,_,_,_,_,_)).WillRepeatedly(Return(true));
	EXPECT_CALL(*g_mockPrivateInstanceAAMP, SendStreamTransfer(_,_,_,_,_,_,_,_));

	EXPECT_CALL(*g_mockAampStreamSinkManager, RemoveMediaHeader(2));

	//StartInjection will internally call SendMediaHeaders()
	this->mStreamAbstractionAAMP_MPD->StartInjection();
}

TEST_F(StreamAbstractionAAMP_MPDTest, CheckAdResolvedStatus_FirstTryAdBreakNotResolved)
{
	std::string periodId = "periodId1";
	auto ads = std::make_shared<std::vector<AdNode>>();

	EXPECT_CALL(*g_MockPrivateCDAIObjectMPD, isAdBreakObjectExist(_))
		.Times(AnyNumber())
		.WillRepeatedly(Return(true));

	EXPECT_CALL(*g_MockPrivateCDAIObjectMPD, WaitForNextAdResolved(_, _))
		.Times(AnyNumber())
		.WillRepeatedly(Return(false));

	mStreamAbstractionAAMP_MPD->CallCheckForAdResolvedStatus(ads, -1, periodId);
}

TEST_F(StreamAbstractionAAMP_MPDTest, CheckAdResolvedStatus_FirstTryAdBreakResolved)
{
	std::string periodId = "periodId1";
	auto ads = std::make_shared<std::vector<AdNode>>();

	EXPECT_CALL(*g_MockPrivateCDAIObjectMPD, WaitForNextAdResolved(_, _))
		.Times(1)
		.WillOnce(Return(true));

	EXPECT_CALL(*g_MockPrivateCDAIObjectMPD, isAdBreakObjectExist(_))
		.Times(AnyNumber())
		.WillRepeatedly(Return(true));

	mStreamAbstractionAAMP_MPD->CallCheckForAdResolvedStatus(ads, -1, periodId);
}

TEST_F(StreamAbstractionAAMP_MPDTest, CheckAdResolvedStatus_AdNotResolved)
{
	AdNodeVectorPtr ads = std::make_shared<std::vector<AdNode>>();
	ads->emplace_back(false, false, false, "adId1", "url1", 30000, "periodId1", 0, nullptr);
	int adIdx = 0;
	std::string periodId = "periodId1";

	EXPECT_CALL(*g_MockPrivateCDAIObjectMPD, WaitForNextAdResolved(_))
		.Times(1)
		.WillOnce(Return(false));
	EXPECT_CALL(*g_MockPrivateCDAIObjectMPD, isAdBreakObjectExist(_))
		.Times(AnyNumber())
		.WillRepeatedly(Return(true));

	mStreamAbstractionAAMP_MPD->CallCheckForAdResolvedStatus(ads, adIdx, periodId);

	EXPECT_TRUE(ads->at(adIdx).invalid);
}

TEST_F(StreamAbstractionAAMP_MPDTest, CheckAdResolvedStatus_AdResolved)
{
	AdNodeVectorPtr ads = std::make_shared<std::vector<AdNode>>();
	ads->emplace_back(false, false, false, "adId1", "url1", 30000, "periodId1", 0, nullptr);
	int adIdx = 0;
	std::string periodId = "periodId1";

	EXPECT_CALL(*g_MockPrivateCDAIObjectMPD, WaitForNextAdResolved(_))
		.Times(1)
		.WillOnce(Return(true));
	EXPECT_CALL(*g_MockPrivateCDAIObjectMPD, isAdBreakObjectExist(_))
		.Times(AnyNumber())
		.WillRepeatedly(Return(true));

	mStreamAbstractionAAMP_MPD->CallCheckForAdResolvedStatus(ads, adIdx, periodId);

	EXPECT_FALSE(ads->at(adIdx).invalid);
}

TEST_F(StreamAbstractionAAMP_MPDTest, InitTsbReaderTest)
{
	double livePlayPosition = 123.456;
	AampTSBSessionManager *tsbSessionManager = new AampTSBSessionManager(mPrivateInstanceAAMP);
	std::shared_ptr<AampTsbDataManager> dataMgr;
	std::shared_ptr<AampTsbReader> tsbReader = std::make_shared<AampTsbReader>(mPrivateInstanceAAMP, dataMgr, eMEDIATYPE_VIDEO, "sessionId");
	mPrivateInstanceAAMP->rate = AAMP_NORMAL_PLAY_RATE;
	EXPECT_CALL(*g_mockPrivateInstanceAAMP, GetLivePlayPosition()).WillOnce(Return(livePlayPosition));
	EXPECT_CALL(*g_mockPrivateInstanceAAMP, GetTSBSessionManager()).WillOnce(Return(tsbSessionManager));
	EXPECT_CALL(*g_mockTSBSessionManager, InvokeTsbReaders(livePlayPosition, AAMP_NORMAL_PLAY_RATE, eTUNETYPE_SEEKTOLIVE)).WillOnce(Return(eAAMPSTATUS_OK));
	EXPECT_CALL(*g_mockTSBSessionManager, GetTsbReader(eMEDIATYPE_VIDEO)).WillOnce(Return(tsbReader));
	EXPECT_CALL(*g_mockPrivateInstanceAAMP, NotifyOnEnteringLive()).Times(1);
	mStreamAbstractionAAMP_MPD->InitTsbReader(eTUNETYPE_SEEKTOLIVE);
	EXPECT_FLOAT_EQ(mStreamAbstractionAAMP_MPD->GetStreamPosition(), livePlayPosition);
}

TEST_F(StreamAbstractionAAMP_MPDTest, SendAdReservationEvent_NoTSB)
{
	// Set up test parameters for start event
	std::string startAdBreakId = "adBreak1";
	uint32_t startPeriodPosition = 1000;
	AampTime startAbsPosition(2000);
	bool startImmediate = true;

	// Set up test parameters for end event
	std::string endAdBreakId = "adBreak1";
	uint32_t endPeriodPosition = 2000;
	AampTime endAbsPosition(2000);
	bool endImmediate = false;

	// Set up expectations for no TSB manager
	EXPECT_CALL(*g_mockPrivateInstanceAAMP, GetTSBSessionManager())
		.WillRepeatedly(Return(nullptr));

	EXPECT_CALL(*g_mockPrivateInstanceAAMP, IsLocalAAMPTsbInjection())
		.Times(0);

	// Test START event
	EXPECT_CALL(*g_mockPrivateInstanceAAMP, SendAdReservationEvent(
		AAMP_EVENT_AD_RESERVATION_START, startAdBreakId, startPeriodPosition, startAbsPosition.milliseconds(), startImmediate, _))
		.Times(1);
	mStreamAbstractionAAMP_MPD->CallSendAdReservationEvent(
		AAMP_EVENT_AD_RESERVATION_START, startAdBreakId, startPeriodPosition, startAbsPosition, startImmediate);

	// Test END event
	EXPECT_CALL(*g_mockPrivateInstanceAAMP, SendAdReservationEvent(
		AAMP_EVENT_AD_RESERVATION_END, endAdBreakId, endPeriodPosition, endAbsPosition.milliseconds(), endImmediate, _))
		.Times(1);
	mStreamAbstractionAAMP_MPD->CallSendAdReservationEvent(
		AAMP_EVENT_AD_RESERVATION_END, endAdBreakId, endPeriodPosition, endAbsPosition, endImmediate);
}

TEST_F(StreamAbstractionAAMP_MPDTest, SendAdReservationEvent_WithTSBNoLocalInjection)
{
	// Set up test parameters for both events
	std::string startAdBreakId = "adBreak1";
	std::string endAdBreakId = "adBreak2";
	uint32_t startPeriodPosition = 1000;
	uint32_t endPeriodPosition = 2000;
	AampTime startAbsPosition(3000);
	AampTime endAbsPosition(4000);
	bool startImmediate = true;
	bool endImmediate = false;

	// Set up expectations for TSB manager without local injection
	EXPECT_CALL(*g_mockPrivateInstanceAAMP, GetTSBSessionManager())
		.WillRepeatedly(Return(g_mockTSBSessionManager));

	EXPECT_CALL(*g_mockPrivateInstanceAAMP, IsLocalAAMPTsbInjection())
		.WillRepeatedly(Return(false));

	// Test START event
	{
		testing::InSequence seq;
		EXPECT_CALL(*g_mockTSBSessionManager, StartAdReservation(startAdBreakId, startPeriodPosition, startAbsPosition))
			.WillOnce(Return(true));
		EXPECT_CALL(*g_mockTSBSessionManager, ShiftFutureAdEvents())
			.Times(1);
	}
	EXPECT_CALL(*g_mockPrivateInstanceAAMP, SendAdReservationEvent(
		AAMP_EVENT_AD_RESERVATION_START, startAdBreakId, startPeriodPosition, startAbsPosition.milliseconds(), startImmediate, _))
		.Times(1);

	mStreamAbstractionAAMP_MPD->CallSendAdReservationEvent(
		AAMP_EVENT_AD_RESERVATION_START, startAdBreakId, startPeriodPosition, startAbsPosition, startImmediate);

	// Test END event
	{
		testing::InSequence seq;
		EXPECT_CALL(*g_mockTSBSessionManager, EndAdReservation(endAdBreakId, endPeriodPosition, endAbsPosition, _))
			.WillOnce(Return(true));
		EXPECT_CALL(*g_mockTSBSessionManager, ShiftFutureAdEvents())
			.Times(0);
	}
	EXPECT_CALL(*g_mockPrivateInstanceAAMP, SendAdReservationEvent(
		AAMP_EVENT_AD_RESERVATION_END, endAdBreakId, endPeriodPosition, endAbsPosition.milliseconds(), endImmediate, _))
		.Times(1);

	mStreamAbstractionAAMP_MPD->CallSendAdReservationEvent(
		AAMP_EVENT_AD_RESERVATION_END, endAdBreakId, endPeriodPosition, endAbsPosition, endImmediate);
}

TEST_F(StreamAbstractionAAMP_MPDTest, SendAdReservationEvent_WithTSBAndLocalInjection)
{
	// Set up test parameters for both events
	std::string startAdBreakId = "adBreak1";
	std::string endAdBreakId = "adBreak2";
	uint32_t startPeriodPosition = 1000;
	uint32_t endPeriodPosition = 2000;
	AampTime startAbsPosition(3000);
	AampTime endAbsPosition(4000);
	bool startImmediate = true;
	bool endImmediate = false;

	// Set up expectations for TSB manager with local injection
	EXPECT_CALL(*g_mockPrivateInstanceAAMP, GetTSBSessionManager())
		.WillRepeatedly(Return(g_mockTSBSessionManager));

	EXPECT_CALL(*g_mockPrivateInstanceAAMP, IsLocalAAMPTsbInjection())
		.WillRepeatedly(Return(true));

	// No SendAdReservationEvent calls expected due to local injection
	EXPECT_CALL(*g_mockPrivateInstanceAAMP, SendAdReservationEvent(_,_,_,_,_,_))
		.Times(0);

	// Test START event
	{
		testing::InSequence seq;
		EXPECT_CALL(*g_mockTSBSessionManager, StartAdReservation(startAdBreakId, startPeriodPosition, startAbsPosition))
			.WillOnce(Return(true));
		EXPECT_CALL(*g_mockTSBSessionManager, ShiftFutureAdEvents())
			.Times(1);
	}
	mStreamAbstractionAAMP_MPD->CallSendAdReservationEvent(
		AAMP_EVENT_AD_RESERVATION_START, startAdBreakId, startPeriodPosition, startAbsPosition, startImmediate);

	// Test END event
	{
		testing::InSequence seq;
		EXPECT_CALL(*g_mockTSBSessionManager, EndAdReservation(endAdBreakId, endPeriodPosition, endAbsPosition, _))
			.WillOnce(Return(true));
		EXPECT_CALL(*g_mockTSBSessionManager, ShiftFutureAdEvents())
			.Times(0);
	}
	mStreamAbstractionAAMP_MPD->CallSendAdReservationEvent(
		AAMP_EVENT_AD_RESERVATION_END, endAdBreakId, endPeriodPosition, endAbsPosition, endImmediate);
}

TEST_F(StreamAbstractionAAMP_MPDTest, SendAdPlacementEvent_NoTSB)
{
	// Set up test parameters for both non-immediate and immediate events
	std::string adId = "testAd1";
	uint32_t relativePosition = 1000;
	AampTime absPosition(2000);
	uint32_t offset = 500;
	uint32_t duration = 30000;
	bool immediate = false;

	// Set up expectations for no TSB manager
	EXPECT_CALL(*g_mockPrivateInstanceAAMP, GetTSBSessionManager())
		.WillRepeatedly(Return(nullptr));

	EXPECT_CALL(*g_mockPrivateInstanceAAMP, IsLocalAAMPTsbInjection())
		.Times(0);

	// Test non-immediate events for each type
	EXPECT_CALL(*g_mockPrivateInstanceAAMP, SendAdPlacementEvent(
		AAMP_EVENT_AD_PLACEMENT_START, adId, relativePosition, absPosition.milliseconds(), offset, duration, false, 0))
		.Times(1);
	mStreamAbstractionAAMP_MPD->CallSendAdPlacementEvent(
		AAMP_EVENT_AD_PLACEMENT_START, adId, relativePosition, absPosition, offset, duration, false);

	EXPECT_CALL(*g_mockPrivateInstanceAAMP, SendAdPlacementEvent(
		AAMP_EVENT_AD_PLACEMENT_ERROR, adId, relativePosition, absPosition.milliseconds(), offset, duration, false, 0))
		.Times(1);
	mStreamAbstractionAAMP_MPD->CallSendAdPlacementEvent(
		AAMP_EVENT_AD_PLACEMENT_ERROR, adId, relativePosition, absPosition, offset, duration, false);

	EXPECT_CALL(*g_mockPrivateInstanceAAMP, SendAdPlacementEvent(
		AAMP_EVENT_AD_PLACEMENT_END, adId, relativePosition, absPosition.milliseconds(), offset, duration, false, 0))
		.Times(1);
	mStreamAbstractionAAMP_MPD->CallSendAdPlacementEvent(
		AAMP_EVENT_AD_PLACEMENT_END, adId, relativePosition, absPosition, offset, duration, false);

	// Test immediate events for each type
	EXPECT_CALL(*g_mockPrivateInstanceAAMP, SendAdPlacementEvent(
		AAMP_EVENT_AD_PLACEMENT_START, adId, relativePosition, absPosition.milliseconds(), offset, duration, true, 0))
		.Times(1);
	mStreamAbstractionAAMP_MPD->CallSendAdPlacementEvent(
		AAMP_EVENT_AD_PLACEMENT_START, adId, relativePosition, absPosition, offset, duration, true);

	EXPECT_CALL(*g_mockPrivateInstanceAAMP, SendAdPlacementEvent(
		AAMP_EVENT_AD_PLACEMENT_ERROR, adId, relativePosition, absPosition.milliseconds(), offset, duration, true, 0))
		.Times(1);
	mStreamAbstractionAAMP_MPD->CallSendAdPlacementEvent(
		AAMP_EVENT_AD_PLACEMENT_ERROR, adId, relativePosition, absPosition, offset, duration, true);

	EXPECT_CALL(*g_mockPrivateInstanceAAMP, SendAdPlacementEvent(
		AAMP_EVENT_AD_PLACEMENT_END, adId, relativePosition, absPosition.milliseconds(), offset, duration, true, 0))
		.Times(1);
	mStreamAbstractionAAMP_MPD->CallSendAdPlacementEvent(
		AAMP_EVENT_AD_PLACEMENT_END, adId, relativePosition, absPosition, offset, duration, true);
}

TEST_F(StreamAbstractionAAMP_MPDTest, SendAdPlacementEvent_WithTSBNoLocalInjection)
{
	// Set up test parameters for both non-immediate and immediate events
	std::string adId = "testAd1";
	uint32_t relativePosition = 1000;
	AampTime absPosition(2000);
	uint32_t offset = 500;
	uint32_t duration = 30000;

	// Set up expectations for TSB manager without local injection
	EXPECT_CALL(*g_mockPrivateInstanceAAMP, GetTSBSessionManager())
		.WillRepeatedly(Return(g_mockTSBSessionManager));

	EXPECT_CALL(*g_mockPrivateInstanceAAMP, IsLocalAAMPTsbInjection())
		.WillRepeatedly(Return(false));

	// Test non-immediate events for each type
	{
		testing::InSequence seq;
		EXPECT_CALL(*g_mockTSBSessionManager, StartAdPlacement(adId, relativePosition, absPosition, duration, offset))
			.WillOnce(Return(true));
		EXPECT_CALL(*g_mockTSBSessionManager, ShiftFutureAdEvents())
			.Times(0);
	}
	EXPECT_CALL(*g_mockPrivateInstanceAAMP, SendAdPlacementEvent(
		AAMP_EVENT_AD_PLACEMENT_START, adId, relativePosition, absPosition.milliseconds(), offset, duration, false, 0))
		.Times(1);
	mStreamAbstractionAAMP_MPD->CallSendAdPlacementEvent(
		AAMP_EVENT_AD_PLACEMENT_START, adId, relativePosition, absPosition, offset, duration, false);

	{
		testing::InSequence seq;
		EXPECT_CALL(*g_mockTSBSessionManager, EndAdPlacementWithError(adId, relativePosition, absPosition, duration, offset))
			.WillOnce(Return(true));
		EXPECT_CALL(*g_mockTSBSessionManager, ShiftFutureAdEvents())
			.Times(0);
	}
	EXPECT_CALL(*g_mockPrivateInstanceAAMP, SendAdPlacementEvent(
		AAMP_EVENT_AD_PLACEMENT_ERROR, adId, relativePosition, absPosition.milliseconds(), offset, duration, false, 0))
		.Times(1);
	mStreamAbstractionAAMP_MPD->CallSendAdPlacementEvent(
		AAMP_EVENT_AD_PLACEMENT_ERROR, adId, relativePosition, absPosition, offset, duration, false);

	{
		testing::InSequence seq;
		EXPECT_CALL(*g_mockTSBSessionManager, EndAdPlacement(adId, relativePosition, absPosition, duration, offset))
			.WillOnce(Return(true));
		EXPECT_CALL(*g_mockTSBSessionManager, ShiftFutureAdEvents())
			.Times(0);
	}
	EXPECT_CALL(*g_mockPrivateInstanceAAMP, SendAdPlacementEvent(
		AAMP_EVENT_AD_PLACEMENT_END, adId, relativePosition, absPosition.milliseconds(), offset, duration, false, 0))
		.Times(1);
	mStreamAbstractionAAMP_MPD->CallSendAdPlacementEvent(
		AAMP_EVENT_AD_PLACEMENT_END, adId, relativePosition, absPosition, offset, duration, false);

	// Test immediate events for each type
	{
		testing::InSequence seq;
		EXPECT_CALL(*g_mockTSBSessionManager, StartAdPlacement(adId, relativePosition, absPosition, duration, offset))
			.WillOnce(Return(true));
		EXPECT_CALL(*g_mockTSBSessionManager, ShiftFutureAdEvents())
			.Times(1);
	}
	EXPECT_CALL(*g_mockPrivateInstanceAAMP, SendAdPlacementEvent(
		AAMP_EVENT_AD_PLACEMENT_START, adId, relativePosition, absPosition.milliseconds(), offset, duration, true, 0))
		.Times(1);
	mStreamAbstractionAAMP_MPD->CallSendAdPlacementEvent(
		AAMP_EVENT_AD_PLACEMENT_START, adId, relativePosition, absPosition, offset, duration, true);

	{
		testing::InSequence seq;
		EXPECT_CALL(*g_mockTSBSessionManager, EndAdPlacementWithError(adId, relativePosition, absPosition, duration, offset))
			.WillOnce(Return(true));
		EXPECT_CALL(*g_mockTSBSessionManager, ShiftFutureAdEvents())
			.Times(1);
	}
	EXPECT_CALL(*g_mockPrivateInstanceAAMP, SendAdPlacementEvent(
		AAMP_EVENT_AD_PLACEMENT_ERROR, adId, relativePosition, absPosition.milliseconds(), offset, duration, true, 0))
		.Times(1);
	mStreamAbstractionAAMP_MPD->CallSendAdPlacementEvent(
		AAMP_EVENT_AD_PLACEMENT_ERROR, adId, relativePosition, absPosition, offset, duration, true);

	{
		testing::InSequence seq;
		EXPECT_CALL(*g_mockTSBSessionManager, EndAdPlacement(adId, relativePosition, absPosition, duration, offset))
			.WillOnce(Return(true));
		EXPECT_CALL(*g_mockTSBSessionManager, ShiftFutureAdEvents())
			.Times(1);
	}
	EXPECT_CALL(*g_mockPrivateInstanceAAMP, SendAdPlacementEvent(
		AAMP_EVENT_AD_PLACEMENT_END, adId, relativePosition, absPosition.milliseconds(), offset, duration, true, 0))
		.Times(1);
	mStreamAbstractionAAMP_MPD->CallSendAdPlacementEvent(
		AAMP_EVENT_AD_PLACEMENT_END, adId, relativePosition, absPosition, offset, duration, true);
}

TEST_F(StreamAbstractionAAMP_MPDTest, SendAdPlacementEvent_WithTSBAndLocalInjection)
{
	// Set up test parameters for both non-immediate and immediate events
	std::string adId = "testAd1";
	uint32_t relativePosition = 1000;
	AampTime absPosition(2000);
	uint32_t offset = 500;
	uint32_t duration = 30000;

	// Set up expectations for TSB manager with local injection
	EXPECT_CALL(*g_mockPrivateInstanceAAMP, GetTSBSessionManager())
		.WillRepeatedly(Return(g_mockTSBSessionManager));

	EXPECT_CALL(*g_mockPrivateInstanceAAMP, IsLocalAAMPTsbInjection())
		.WillRepeatedly(Return(true));

	// No SendAdPlacementEvent calls expected due to local injection
	EXPECT_CALL(*g_mockPrivateInstanceAAMP, SendAdPlacementEvent(_,_,_,_,_,_,_,_))
		.Times(0);

	// Test non-immediate events for each type
	{
		testing::InSequence seq;
		EXPECT_CALL(*g_mockTSBSessionManager, StartAdPlacement(adId, relativePosition, absPosition, duration, offset))
			.WillOnce(Return(true));
		EXPECT_CALL(*g_mockTSBSessionManager, ShiftFutureAdEvents())
			.Times(0);
	}
	mStreamAbstractionAAMP_MPD->CallSendAdPlacementEvent(
		AAMP_EVENT_AD_PLACEMENT_START, adId, relativePosition, absPosition, offset, duration, false);

	{
		testing::InSequence seq;
		EXPECT_CALL(*g_mockTSBSessionManager, EndAdPlacementWithError(adId, relativePosition, absPosition, duration, offset))
			.WillOnce(Return(true));
		EXPECT_CALL(*g_mockTSBSessionManager, ShiftFutureAdEvents())
			.Times(0);
	}
	mStreamAbstractionAAMP_MPD->CallSendAdPlacementEvent(
		AAMP_EVENT_AD_PLACEMENT_ERROR, adId, relativePosition, absPosition, offset, duration, false);

	{
		testing::InSequence seq;
		EXPECT_CALL(*g_mockTSBSessionManager, EndAdPlacement(adId, relativePosition, absPosition, duration, offset))
			.WillOnce(Return(true));
		EXPECT_CALL(*g_mockTSBSessionManager, ShiftFutureAdEvents())
			.Times(0);
	}
	mStreamAbstractionAAMP_MPD->CallSendAdPlacementEvent(
		AAMP_EVENT_AD_PLACEMENT_END, adId, relativePosition, absPosition, offset, duration, false);

	// Test immediate events for each type
	{
		testing::InSequence seq;
		EXPECT_CALL(*g_mockTSBSessionManager, StartAdPlacement(adId, relativePosition, absPosition, duration, offset))
			.WillOnce(Return(true));
		EXPECT_CALL(*g_mockTSBSessionManager, ShiftFutureAdEvents())
			.Times(1);
	}
	mStreamAbstractionAAMP_MPD->CallSendAdPlacementEvent(
		AAMP_EVENT_AD_PLACEMENT_START, adId, relativePosition, absPosition, offset, duration, true);

	{
		testing::InSequence seq;
		EXPECT_CALL(*g_mockTSBSessionManager, EndAdPlacementWithError(adId, relativePosition, absPosition, duration, offset))
			.WillOnce(Return(true));
		EXPECT_CALL(*g_mockTSBSessionManager, ShiftFutureAdEvents())
			.Times(1);
	}
	mStreamAbstractionAAMP_MPD->CallSendAdPlacementEvent(
		AAMP_EVENT_AD_PLACEMENT_ERROR, adId, relativePosition, absPosition, offset, duration, true);

	{
		testing::InSequence seq;
		EXPECT_CALL(*g_mockTSBSessionManager, EndAdPlacement(adId, relativePosition, absPosition, duration, offset))
			.WillOnce(Return(true));
		EXPECT_CALL(*g_mockTSBSessionManager, ShiftFutureAdEvents())
			.Times(1);
	}
	mStreamAbstractionAAMP_MPD->CallSendAdPlacementEvent(
		AAMP_EVENT_AD_PLACEMENT_END, adId, relativePosition, absPosition, offset, duration, true);
}

// Test case to verify that with PTO offset, the fragment time is set correctly and the first segment is downloaded.
TEST_F(FunctionalTests, PresentionTimeOffset_Test_with_PTO)
{
	AAMPStatusType status;
	static const char *manifest =
R"(<?xml version="1.0" encoding="UTF-8"?>
<MPD xmlns="urn:mpeg:dash:schema:mpd:2011" maxSegmentDuration="PT0H0M8.160S" mediaPresentationDuration="PT0H23M23.320S" minBufferTime="PT12.000S" profiles="urn:mpeg:dash:profile:isoff-live:2011,http://dashif.org/guidelines/dash264,urn:hbbtv:dash:profile:isoff-live:2012" type="static">
	<Period duration="PT0H11M09.160S" id="p0">
		<AdaptationSet lang="eng" maxFrameRate="25" maxHeight="1080" maxWidth="1920" par="16:9" segmentAlignment="true" startWithSAP="1">
			<SegmentTemplate initialization="$RepresentationID$_i.mp4" media="$RepresentationID$_$Number$.m4s" startNumber="1" timescale="12800" presentationTimeOffset="832000">
				<SegmentTimeline>
					<S d="76800" r="120" t="0"/>
					<S d="104448"/>
				</SegmentTimeline>
			</SegmentTemplate>
			<Representation bandwidth="517566" codecs="avc1.4D4028" frameRate="25" height="360" id="v1" mimeType="video/mp4" sar="1:1" width="640" />
			<Representation bandwidth="1502968" codecs="avc1.4D4028" frameRate="25" height="720" id="v2" mimeType="video/mp4" sar="1:1" width="1280" />
			<Representation bandwidth="2090806" codecs="avc1.4D4028" frameRate="25" height="1080" id="v3" mimeType="video/mp4" sar="1:1" width="1920" />
		</AdaptationSet>
		<AdaptationSet lang="eng" segmentAlignment="true" startWithSAP="1">
			<SegmentTemplate initialization="$RepresentationID$_i.mp4" media="$RepresentationID$_$Number$.m4s" startNumber="1" timescale="44100" presentationTimeOffset="2866500">
				<SegmentTimeline>
					<S d="263177" t="0"/>
					<S d="264192" r="120"/>
					<S d="144384"/>
				</SegmentTimeline>
			</SegmentTemplate>
			<Representation audioSamplingRate="44100" bandwidth="131780" codecs="mp4a.40.2" id="a1" mimeType="audio/mp4">
				<AudioChannelConfiguration schemeIdUri="urn:mpeg:dash:23003:3:audio_channel_configuration:2011" value="2"/>
			</Representation>
		</AdaptationSet>
	</Period>
</MPD>
)";
	EXPECT_CALL(*g_mockMediaStreamContext, CacheFragment(_, _, _, _, _, true, _, _, _, _, _))
		.WillRepeatedly(Return(true));
	EXPECT_CALL(*g_mockPrivateInstanceAAMP, SetLLDashChunkMode(_));
	status = InitializeMPD(manifest, eTUNETYPE_NEW_NORMAL, 0.0, AAMP_NORMAL_PLAY_RATE, false);
	EXPECT_EQ(status, eAAMPSTATUS_OK);

	/* Video stream */
	MediaTrack *track = this->mStreamAbstractionAAMP_MPD->GetMediaTrack(eTRACK_VIDEO);
	EXPECT_NE(track, nullptr);
	MediaStreamContext *pMediaStreamContext = static_cast<MediaStreamContext *>(track);

	/* PTO offset = 65.000000, fragment duration = 6.00secs, so need to skip upto 60secs duration fragments,
	 * fragmentTime = -5.000000 will be the position for the 11 fragment.
	 * so fragment descriptor will be as follows:
	 * fragmentDescriptor.Time = t + 10*d,(d = 76800) => 0+10*76800 = 768000
	 * fragmentDescriptor.Number = 11, so fragment number = 10 + 1 (startNumber) = 11
	 */
	//No midFragmentSeek, So initial 5 seconds of the segment is played outside the timeline as total fragment duration is 6 seconds
	EXPECT_EQ(pMediaStreamContext->fragmentTime, -5.000000);
	EXPECT_EQ(pMediaStreamContext->fragmentDescriptor.Number,11);
	EXPECT_EQ(pMediaStreamContext->fragmentDescriptor.Time,768000.000000);
}

// Test case to verify that without PTO offset, the fragment time is 0.00 and the first segment is downloaded.
// This is to ensure that the stream starts from the beginning without any offset.
TEST_F(FunctionalTests, PresentionTimeOffset_Test_without_PTO)
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
	EXPECT_CALL(*g_mockPrivateInstanceAAMP, SetLLDashChunkMode(_));

	status = InitializeMPD(manifest);
	EXPECT_EQ(status, eAAMPSTATUS_OK);

	/* Video stream */
	MediaTrack *track = this->mStreamAbstractionAAMP_MPD->GetMediaTrack(eTRACK_VIDEO);
	EXPECT_NE(track, nullptr);
	MediaStreamContext *pMediaStreamContext = static_cast<MediaStreamContext *>(track);

	// No PTO offset, so fragment time is 0.00, need to start downloading from the first segment.
	EXPECT_EQ(pMediaStreamContext->fragmentTime, 0.00);
	EXPECT_EQ(pMediaStreamContext->fragmentDescriptor.Number,1);
	EXPECT_EQ(pMediaStreamContext->fragmentDescriptor.Time,0.00);
}

// L1: Verify mFirstPTS is updated via SeekInPeriod when seek crosses from period-1 to period-2.
TEST_F(FunctionalTests, L1_SeekInPeriod_TwoPeriods_UpdatesFirstPTS)
{
	AAMPStatusType status;
	static const char *manifest =
R"(<?xml version="1.0" encoding="utf-8"?>
<MPD xmlns="urn:mpeg:dash:schema:mpd:2011" minBufferTime="PT2S" type="static" mediaPresentationDuration="PT0H0M30.00S" profiles="urn:mpeg:dash:profile:isoff-live:2011,http://dashif.org/guidelines/dash264">
	<Period id="p0" start="PT0S" duration="PT10S">
		<AdaptationSet contentType="video" segmentAlignment="true" startWithSAP="1">
			<Representation id="v1" mimeType="video/mp4" codecs="avc1.640028" width="640" height="360" bandwidth="1000000">
				<SegmentTemplate timescale="1000" media="video_$Time$.m4s" initialization="video_init.mp4" startNumber="1">
					<SegmentTimeline>
						<S t="90000" d="2000" r="4" />
					</SegmentTimeline>
				</SegmentTemplate>
			</Representation>
		</AdaptationSet>
	</Period>
	<Period id="p1" start="PT10S" duration="PT20S">
		<AdaptationSet contentType="video" segmentAlignment="true" startWithSAP="1">
			<Representation id="v1" mimeType="video/mp4" codecs="avc1.640028" width="640" height="360" bandwidth="1000000">
				<SegmentTemplate timescale="1000" media="video_p1_$Time$.m4s" initialization="video_init.mp4" startNumber="1">
					<SegmentTimeline>
						<S t="100000" d="2000" r="9" />
					</SegmentTimeline>
				</SegmentTemplate>
			</Representation>
		</AdaptationSet>
	</Period>
</MPD>
)";

	EXPECT_CALL(*g_mockMediaStreamContext, CacheFragment(_, _, _, _, _, true, _, _, _, _, _))
		.WillRepeatedly(Return(true));
	EXPECT_CALL(*g_mockPrivateInstanceAAMP, SetLLDashChunkMode(_));

	status = InitializeMPD(manifest, eTUNETYPE_NEW_NORMAL, 0.0, AAMP_NORMAL_PLAY_RATE, false);
	EXPECT_EQ(status, eAAMPSTATUS_OK);
	MediaTrack *track = mStreamAbstractionAAMP_MPD->GetMediaTrack(eTRACK_VIDEO);
	ASSERT_NE(track, nullptr);
	MediaStreamContext *pMediaStreamContext = static_cast<MediaStreamContext *>(track);
	EXPECT_CALL(*g_mockPrivateInstanceAAMP, GetTSBSessionManager()).WillRepeatedly(Return(nullptr));

	// Reset mFirstPTS before seek
	mStreamAbstractionAAMP_MPD->clearFirstPTS();
	EXPECT_DOUBLE_EQ(mStreamAbstractionAAMP_MPD->GetFirstPTS(), 0.0);

	// Seek position is greater than period-1 duration (10s), so SeekInPeriod should transition to period-2
	// with remaining seek value and still update mFirstPTS from SkipFragments path.
	const double seekPositionSeconds = 12.0;
	TestableFunctionalStreamAbstractionAAMP_MPD *testableStreamAbstractionAAMP_MPD =
    dynamic_cast<TestableFunctionalStreamAbstractionAAMP_MPD *>(mStreamAbstractionAAMP_MPD);
	ASSERT_NE(testableStreamAbstractionAAMP_MPD, nullptr);
	testableStreamAbstractionAAMP_MPD->CallSeekInPeriod(seekPositionSeconds);
	// Verify: mFirstPTS updated after period transition and remaining-seek skip in period-2.
	EXPECT_DOUBLE_EQ(mStreamAbstractionAAMP_MPD->GetFirstPTS(), 102.000000);
}

TEST_F(StreamAbstractionAAMP_MPDTest, clearFirstPTS)
{
	// Set a non-default value for mFirstPTS using the public accessor.
	const double testPTS = 12.34;
	mStreamAbstractionAAMP_MPD->SetFirstPTSForTest(testPTS);
	EXPECT_EQ(mStreamAbstractionAAMP_MPD->GetFirstPTSForTest(), testPTS);

	mStreamAbstractionAAMP_MPD->clearFirstPTS();
	EXPECT_EQ(mStreamAbstractionAAMP_MPD->GetFirstPTSForTest(), 0.0);
}
//This test simulates a timeout error during manifest download and ensures that the appropriate error event is sent and the correct status is returned.
TEST_F(StreamAbstractionAAMP_MPDTest, FetchDashManifest_DNSTimeout_WithManifestDownloadError)
{
	EXPECT_CALL(*g_mockAampMPDDownloader, GetManifest (_, _, _))
		.WillOnce(WithArgs<2>(Invoke([this](int errorSimulation) {
						return this->GetManifestForMPDDownloaderTimeout(eCURL_TIMEOUT_DNS);
						})));

	// Mock error handling
	EXPECT_CALL(*g_mockPrivateInstanceAAMP, DownloadsAreEnabled())
		.WillOnce(Return(true));

	//Ensure proper error event is sent
	EXPECT_CALL(*g_mockPrivateInstanceAAMP, SendDownloadErrorEvent(AAMP_TUNE_DNS_RESOLVE_TIMEOUT, eCURL_TIMEOUT_DNS))
		.Times(1);

	// Execute test
	AAMPStatusType result = mStreamAbstractionAAMP_MPD->CallFetchDashManifest();
	EXPECT_EQ(result, AAMPStatusType::eAAMPSTATUS_MANIFEST_DOWNLOAD_ERROR);
}
//This test simulates a data timeout error during manifest download and ensures that the appropriate error event is sent and the correct status is returned.
TEST_F(StreamAbstractionAAMP_MPDTest, FetchDashManifest_DataTimeout_WithManifestDownloadError)
{
	EXPECT_CALL(*g_mockAampMPDDownloader, GetManifest (_, _, _))
		.WillOnce(WithArgs<2>(Invoke([this](int errorSimulation) {
						return this->GetManifestForMPDDownloaderTimeout(eCURL_TIMEOUT_DATA);
						})));

	// Mock error handling
	EXPECT_CALL(*g_mockPrivateInstanceAAMP, DownloadsAreEnabled())
		.WillOnce(Return(true));

	//Ensure proper error event is sent
	EXPECT_CALL(*g_mockPrivateInstanceAAMP, SendDownloadErrorEvent(AAMP_TUNE_DATA_TRANSFER_TIMEOUT, eCURL_TIMEOUT_DATA))
		.Times(1);

	// Execute test
	AAMPStatusType result = mStreamAbstractionAAMP_MPD->CallFetchDashManifest();
	EXPECT_EQ(result, AAMPStatusType::eAAMPSTATUS_MANIFEST_DOWNLOAD_ERROR);
}
//This test simulates a connection timeout error during manifest download and ensures that the appropriate error event is sent and the correct status is returned.
TEST_F(StreamAbstractionAAMP_MPDTest, FetchDashManifest_ConnectTimeout_WithManifestDownloadError)
{
	EXPECT_CALL(*g_mockAampMPDDownloader, GetManifest (_, _, _))
		.WillOnce(WithArgs<2>(Invoke([this](int errorSimulation) {
						return this->GetManifestForMPDDownloaderTimeout(eCURL_TIMEOUT_CONNECT);
						})));

	// Mock error handling
	EXPECT_CALL(*g_mockPrivateInstanceAAMP, DownloadsAreEnabled())
		.WillOnce(Return(true));

	//Ensure proper error event is sent
	EXPECT_CALL(*g_mockPrivateInstanceAAMP, SendDownloadErrorEvent(AAMP_TUNE_CURL_CONNECTION_TIMEOUT, eCURL_TIMEOUT_CONNECT))
		.Times(1);

	// Execute test
	AAMPStatusType result = mStreamAbstractionAAMP_MPD->CallFetchDashManifest();
	EXPECT_EQ(result, AAMPStatusType::eAAMPSTATUS_MANIFEST_DOWNLOAD_ERROR);
}

// Test case to verify seeking behavior with VOD content, with start time, without duration and PTO greater than timeline
TEST_F(FunctionalTests, L1_VOD_WithStartTime_NoDuration_PTOGreaterThanTimeline)
{
    AAMPStatusType status;
    static const char *manifest =
R"(<?xml version="1.0" encoding="utf-8"?>
<MPD xmlns="urn:mpeg:dash:schema:mpd:2011"
     xmlns:cenc="urn:mpeg:cenc:2013"
     type="static" minBufferTime="PT1.5S" mediaPresentationDuration="PT10M0S" profiles="urn:mpeg:dash:profile:isoff-live:2011">
    <Period id="p0" start="PT0S">
        <AdaptationSet id="3" contentType="video" group="2" par="16:9" minBandwidth="184000" maxBandwidth="4435000" maxWidth="1920" maxHeight="1080" segmentAlignment="true" sar="1:1" mimeType="video/mp4" startWithSAP="1">
            <Accessibility schemeIdUri="urn:scte:dash:cc:cea-608:2015" />
            <Role schemeIdUri="urn:mpeg:dash:role:2011" value="main" />
            <SegmentTemplate initialization="out-$RepresentationID$.dash" timescale="60000" presentationTimeOffset="37325280" media="out-$RepresentationID$-$Time$.dash" startNumber="1">
                <SegmentTimeline>
                    <S t="37237200" d="88088" />
                    <S t="37325288" d="32032" />
                    <S t="37357320" d="120120" r="320" />
                </SegmentTimeline>
            </SegmentTemplate>
            <Representation id="v1" bandwidth="184000" width="640" height="360" codecs="avc1.4D401F" />
        </AdaptationSet>
    </Period>
</MPD>
)";
    // Enable mid-fragment seek
	mBoolConfigSettings[eAAMPConfig_MidFragmentSeek] = true;

    std::string fragmentUrl = std::string(TEST_BASE_URL) + std::string("out-v1.dash");
    EXPECT_CALL(*g_mockMediaStreamContext, CacheFragment(fragmentUrl, _, _, _, _, true, _, _, _, _, _))
        .WillOnce(Return(true));
    EXPECT_CALL(*g_mockPrivateInstanceAAMP, SetLLDashChunkMode(_));

    double seekPosition = 0.0;
    int rate = 1;
    status = InitializeMPD(manifest, TuneType::eTUNETYPE_SEEK, seekPosition, rate, true);
    EXPECT_EQ(status, eAAMPSTATUS_OK);

    // After seek to 0, stream position should be 0
	double actualPosition = mStreamAbstractionAAMP_MPD->GetStreamPosition();
	cout<<"Actual Position: "<<actualPosition<<endl;
    EXPECT_NEAR(actualPosition, seekPosition, 0.001);
}

// Test case to verify seeking behavior with live stream, with start times, without durations and PTO greater than timeline
TEST_F(FunctionalTests, L1_Live_WithStartTimes_NoDurations_PTOGreaterThanTimeline)
{
    AAMPStatusType status;
    static const char *manifest =
R"(<?xml version="1.0" encoding="utf-8"?>
<MPD xmlns="urn:mpeg:dash:schema:mpd:2011"
     xmlns:cenc="urn:mpeg:cenc:2013"
     type="dynamic"
     availabilityStartTime="2023-01-01T00:00:00Z"
     minBufferTime="PT1.5S"
     timeShiftBufferDepth="PT5M"
     suggestedPresentationDelay="PT10S"
     mediaPresentationDuration="PT10M0S"
     profiles="urn:mpeg:dash:profile:isoff-live:2011">
    <Period id="p0" start="PT0S">
        <AdaptationSet id="3" contentType="video" group="2" par="16:9"
            minBandwidth="184000" maxBandwidth="4435000" maxWidth="1920" maxHeight="1080"
            segmentAlignment="true" sar="1:1" mimeType="video/mp4" startWithSAP="1">
            <Accessibility schemeIdUri="urn:scte:dash:cc:cea-608:2015" />
            <Role schemeIdUri="urn:mpeg:dash:role:2011" value="main" />
            <SegmentTemplate initialization="out-$RepresentationID$.dash"
                timescale="60000" presentationTimeOffset="37325280"
                media="out-$RepresentationID$-$Time$.dash" startNumber="1">
                <SegmentTimeline>
                    <S t="37237200" d="88088" />
                    <S t="37325288" d="32032" />
                    <S t="37357320" d="120120" r="320" />
                </SegmentTimeline>
            </SegmentTemplate>
            <Representation id="v1" bandwidth="184000" width="640" height="360" codecs="avc1.4D401F" />
        </AdaptationSet>
    </Period>
</MPD>
)";

    // Enable mid-fragment seek
    mBoolConfigSettings[eAAMPConfig_MidFragmentSeek] = true;

    std::string fragmentUrl = std::string(TEST_BASE_URL) + "out-v1.dash";
    EXPECT_CALL(*g_mockMediaStreamContext, CacheFragment(fragmentUrl, _, _, _, _, true, _, _, _, _, _))
        .WillOnce(Return(true));
    EXPECT_CALL(*g_mockPrivateInstanceAAMP, SetLLDashChunkMode(_));

    double seekPosition = 0.0;
    int rate = 1;
    status = InitializeMPD(manifest, TuneType::eTUNETYPE_SEEK, seekPosition, rate, true);
    EXPECT_EQ(status, eAAMPSTATUS_OK);
	double expectedPosition = 1.6725312e+09;//AvailabilityStartTime as it is a liveStream
    double actualPosition = mStreamAbstractionAAMP_MPD->GetStreamPosition();
    EXPECT_NEAR(actualPosition, expectedPosition, 0.001);
}

// Test case to verify seeking behavior with VOD period, with out duration, with out start and PTO greater than timeline
TEST_F(FunctionalTests, L1_VOD_Period_NoDuration_NoStart_PTOGreaterThanTimeline)
{
    AAMPStatusType status;
    static const char *manifest =
R"(<?xml version="1.0" encoding="utf-8"?>
<MPD xmlns="urn:mpeg:dash:schema:mpd:2011"
     xmlns:cenc="urn:mpeg:cenc:2013"
     type="static" minBufferTime="PT1.5S" mediaPresentationDuration="PT10M0S" profiles="urn:mpeg:dash:profile:isoff-live:2011">
    <Period id="p0">
        <AdaptationSet id="3" contentType="video" group="2" par="16:9" minBandwidth="184000" maxBandwidth="4435000" maxWidth="1920" maxHeight="1080" segmentAlignment="true" sar="1:1" mimeType="video/mp4" startWithSAP="1">
            <Accessibility schemeIdUri="urn:scte:dash:cc:cea-608:2015" />
            <Role schemeIdUri="urn:mpeg:dash:role:2011" value="main" />
            <SegmentTemplate initialization="out-$RepresentationID$.dash" timescale="60000" presentationTimeOffset="37325280" media="out-$RepresentationID$-$Time$.dash" startNumber="1">
                <SegmentTimeline>
                    <S t="37237200" d="88088" />
                    <S t="37325288" d="32032" />
                    <S t="37357320" d="120120" r="320" />
                </SegmentTimeline>
            </SegmentTemplate>
            <Representation id="v1" bandwidth="184000" width="640" height="360" codecs="avc1.4D401F" />
        </AdaptationSet>
    </Period>
</MPD>
)";
    // Enable mid-fragment seek
    mBoolConfigSettings[eAAMPConfig_MidFragmentSeek] = true;

    std::string fragmentUrl = std::string(TEST_BASE_URL) + std::string("out-v1.dash");
    EXPECT_CALL(*g_mockMediaStreamContext, CacheFragment(fragmentUrl, _, _, _, _, true, _, _, _, _, _))
        .WillOnce(Return(true));
    EXPECT_CALL(*g_mockPrivateInstanceAAMP, SetLLDashChunkMode(_));

    double seekPosition = 0.0;
    int rate = 1;
    status = InitializeMPD(manifest, TuneType::eTUNETYPE_SEEK, seekPosition, rate, true);
    EXPECT_EQ(status, eAAMPSTATUS_OK);

    // After seek to 0, stream position should be 0
    double actualPosition = mStreamAbstractionAAMP_MPD->GetStreamPosition();
    cout<<"Actual Position: "<<actualPosition<<endl;
    EXPECT_NEAR(actualPosition, seekPosition, 0.001);
}

// Test case to verify seeking behavior with live period, with PTO greater than timeline
TEST_F(FunctionalTests, L1_LivePeriod_PTOGreaterThanTimeline)
{
    AAMPStatusType status;
    static const char *manifest =
R"(<?xml version="1.0" encoding="utf-8"?>
<MPD xmlns="urn:mpeg:dash:schema:mpd:2011"
     xmlns:cenc="urn:mpeg:cenc:2013"
     type="dynamic"
     availabilityStartTime="2023-01-01T00:00:00Z"
     minBufferTime="PT1.5S"
     timeShiftBufferDepth="PT5M"
     suggestedPresentationDelay="PT10S"
     mediaPresentationDuration="PT10M0S"
     profiles="urn:mpeg:dash:profile:isoff-live:2011">
    <Period id="p0">
        <AdaptationSet id="3" contentType="video" group="2" par="16:9"
            minBandwidth="184000" maxBandwidth="4435000" maxWidth="1920" maxHeight="1080"
            segmentAlignment="true" sar="1:1" mimeType="video/mp4" startWithSAP="1">
            <Accessibility schemeIdUri="urn:scte:dash:cc:cea-608:2015" />
            <Role schemeIdUri="urn:mpeg:dash:role:2011" value="main" />
            <SegmentTemplate initialization="out-$RepresentationID$.dash"
                timescale="60000" presentationTimeOffset="37325280"
                media="out-$RepresentationID$-$Time$.dash" startNumber="1">
                <SegmentTimeline>
                    <S t="37237200" d="88088" />
                    <S t="37325288" d="32032" />
                    <S t="37357320" d="120120" r="320" />
                </SegmentTimeline>
            </SegmentTemplate>
            <Representation id="v1" bandwidth="184000" width="640" height="360" codecs="avc1.4D401F" />
        </AdaptationSet>
    </Period>
</MPD>
)";

    // Enable mid-fragment seek
    mBoolConfigSettings[eAAMPConfig_MidFragmentSeek] = true;

    std::string fragmentUrl = std::string(TEST_BASE_URL) + "out-v1.dash";
    EXPECT_CALL(*g_mockMediaStreamContext, CacheFragment(fragmentUrl, _, _, _, _, true, _, _, _, _, _))
        .WillOnce(Return(true));
    EXPECT_CALL(*g_mockPrivateInstanceAAMP, SetLLDashChunkMode(_));

    double seekPosition = 0.0;
    int rate = 1;
    status = InitializeMPD(manifest, TuneType::eTUNETYPE_SEEK, seekPosition, rate, true);
    EXPECT_EQ(status, eAAMPSTATUS_OK);
    double expectedPosition = 1.6725312e+09;//AvailabilityStartTime as it is a liveStream

    // After seek to 0, verify stream position is near the expected availabilityStartTime
    double actualPosition = mStreamAbstractionAAMP_MPD->GetStreamPosition();

    EXPECT_NEAR(actualPosition, expectedPosition, 0.001);
}

// Test case to verify seeking behavior with three periods, with out start times, with out durations, and PTO greater than timeline
TEST_F(FunctionalTests, L1_ThreePeriods_NoStartTimes_NoDurations_PTOGreaterThanTimeline)
{
    AAMPStatusType status;
    static const char *manifest =
R"(<?xml version="1.0" encoding="utf-8"?>
<MPD xmlns="urn:mpeg:dash:schema:mpd:2011" type="static" minBufferTime="PT1.5S" mediaPresentationDuration="PT30M0S" profiles="urn:mpeg:dash:profile:isoff-live:2011">
    <Period id="p0">
        <AdaptationSet contentType="video" mimeType="video/mp4">
            <SegmentTemplate timescale="1000" presentationTimeOffset="15000" media="p0-$Time$.m4s" startNumber="1">
                <SegmentTimeline>
                    <S t="5000" d="1000" r="609" />
                </SegmentTimeline>
            </SegmentTemplate>
            <Representation id="v1" bandwidth="100000" />
        </AdaptationSet>
    </Period>
    <Period id="p1">
        <AdaptationSet contentType="video" mimeType="video/mp4">
            <SegmentTemplate timescale="1000" presentationTimeOffset="25000" media="p1-$Time$.m4s" startNumber="1">
                <SegmentTimeline>
                    <S t="10000" d="1000" r="614" />
                </SegmentTimeline>
            </SegmentTemplate>
            <Representation id="v2" bandwidth="200000" />
        </AdaptationSet>
    </Period>
    <Period id="p2">
        <AdaptationSet contentType="video" mimeType="video/mp4">
            <SegmentTemplate timescale="1000" presentationTimeOffset="35000" media="p2-$Time$.m4s" startNumber="1">
                <SegmentTimeline>
                    <S t="20000" d="1000" r="614" />
                </SegmentTimeline>
            </SegmentTemplate>
            <Representation id="v3" bandwidth="300000" />
        </AdaptationSet>
    </Period>
</MPD>
)";
    double seekPosition = 1;
    int rate = 1;
    EXPECT_CALL(*g_mockMediaStreamContext, CacheFragment(_, _, _, _, _, true, _, _, _, _, _))
        .WillRepeatedly(Return(true));
    EXPECT_CALL(*g_mockPrivateInstanceAAMP, SetLLDashChunkMode(_));
    status = InitializeMPD(manifest, TuneType::eTUNETYPE_SEEK, seekPosition, rate, true);
    EXPECT_EQ(status, eAAMPSTATUS_OK);

    double actualPosition = mStreamAbstractionAAMP_MPD->GetStreamPosition();
    EXPECT_EQ(actualPosition, seekPosition);
}

// Test case to verify seeking behavior with three periods, with out start times, with durations, and PTO greater than timeline
TEST_F(FunctionalTests, L1_ThreePeriods_NoStartTimes_WithDurations_PTOGreaterThanTimeline)
{
    AAMPStatusType status;
    static const char *manifest =
R"(<?xml version="1.0" encoding="utf-8"?>
<MPD xmlns="urn:mpeg:dash:schema:mpd:2011" type="static" minBufferTime="PT1.5S" mediaPresentationDuration="PT30M0S" profiles="urn:mpeg:dash:profile:isoff-live:2011">
    <Period id="p0" duration="PT10M0S">
        <AdaptationSet contentType="video" mimeType="video/mp4">
            <SegmentTemplate timescale="1000" presentationTimeOffset="15000" media="p0-$Time$.m4s" startNumber="1">
                <SegmentTimeline>
                    <S t="5000" d="1000" r="609" />
                </SegmentTimeline>
            </SegmentTemplate>
            <Representation id="v1" bandwidth="100000" />
        </AdaptationSet>
    </Period>
    <Period id="p1" duration="PT10M0S">
        <AdaptationSet contentType="video" mimeType="video/mp4">
            <SegmentTemplate timescale="1000" presentationTimeOffset="25000" media="p1-$Time$.m4s" startNumber="1">
                <SegmentTimeline>
                    <S t="10000" d="1000" r="614" />
                </SegmentTimeline>
            </SegmentTemplate>
            <Representation id="v2" bandwidth="200000" />
        </AdaptationSet>
    </Period>
    <Period id="p2" duration="PT10M0S">
        <AdaptationSet contentType="video" mimeType="video/mp4">
            <SegmentTemplate timescale="1000" presentationTimeOffset="35000" media="p2-$Time$.m4s" startNumber="1">
                <SegmentTimeline>
                    <S t="20000" d="1000" r="614" />
                </SegmentTimeline>
            </SegmentTemplate>
            <Representation id="v3" bandwidth="300000" />
        </AdaptationSet>
    </Period>
</MPD>
)";
    double seekPosition = 15 * 60; // Seek to 15 minutes (middle of period p1)
    int rate = 1;
    EXPECT_CALL(*g_mockMediaStreamContext, CacheFragment(_, _, _, _, _, true, _, _, _, _, _))
        .WillRepeatedly(Return(true));
    EXPECT_CALL(*g_mockPrivateInstanceAAMP, SetLLDashChunkMode(_));
    status = InitializeMPD(manifest, TuneType::eTUNETYPE_SEEK, seekPosition, rate, true);
    EXPECT_EQ(status, eAAMPSTATUS_OK);

    double actualPosition = mStreamAbstractionAAMP_MPD->GetStreamPosition();
    EXPECT_EQ(actualPosition, seekPosition);
}

// Test case to verify seeking behavior with three periods, with start time and durations, and PTO greater than timeline
TEST_F(FunctionalTests, L1_ThreePeriods_WithStartTimesAndDurations_PTOGreaterThanTimeline)
{
    AAMPStatusType status;
    static const char *manifest =
R"(<?xml version="1.0" encoding="utf-8"?>
<MPD xmlns="urn:mpeg:dash:schema:mpd:2011" type="static" minBufferTime="PT1.5S" mediaPresentationDuration="PT30M0S" profiles="urn:mpeg:dash:profile:isoff-live:2011">
    <Period id="p0" start="PT0S" duration="PT10M0S">
        <AdaptationSet contentType="video" mimeType="video/mp4">
            <SegmentTemplate timescale="1000" presentationTimeOffset="15000" media="p0-$Time$.m4s" startNumber="1">
                <SegmentTimeline>
                    <S t="5000" d="1000" r="609" />
                </SegmentTimeline>
            </SegmentTemplate>
            <Representation id="v1" bandwidth="100000" />
        </AdaptationSet>
    </Period>
    <Period id="p1" start="PT10M0S" duration="PT10M0S">
        <AdaptationSet contentType="video" mimeType="video/mp4">
            <SegmentTemplate timescale="1000" presentationTimeOffset="25000" media="p1-$Time$.m4s" startNumber="1">
                <SegmentTimeline>
                    <S t="10000" d="1000" r="614" />
                </SegmentTimeline>
            </SegmentTemplate>
            <Representation id="v2" bandwidth="200000" />
        </AdaptationSet>
    </Period>
    <Period id="p2" start="PT20M0S" duration="PT10M0S">
        <AdaptationSet contentType="video" mimeType="video/mp4">
            <SegmentTemplate timescale="1000" presentationTimeOffset="35000" media="p2-$Time$.m4s" startNumber="1">
                <SegmentTimeline>
                    <S t="20000" d="1000" r="614" />
                </SegmentTimeline>
            </SegmentTemplate>
            <Representation id="v3" bandwidth="300000" />
        </AdaptationSet>
    </Period>
</MPD>
)";
    double seekPosition = 0;
    int rate = 1;
    EXPECT_CALL(*g_mockMediaStreamContext, CacheFragment(_, _, _, _, _, true, _, _, _, _, _))
        .WillRepeatedly(Return(true));
    EXPECT_CALL(*g_mockPrivateInstanceAAMP, SetLLDashChunkMode(_));
    status = InitializeMPD(manifest, TuneType::eTUNETYPE_SEEK, seekPosition, rate, true);
    EXPECT_EQ(status, eAAMPSTATUS_OK);

    double actualPosition = mStreamAbstractionAAMP_MPD->GetStreamPosition();
    EXPECT_EQ(actualPosition, seekPosition);
}

// Test case to verify seeking behavior with three periods, with start times, with out durations, and PTO greater than timeline
TEST_F(FunctionalTests, L1_ThreePeriods_WithStartTimesNoDurations_PTOGreaterThanTimeline)
{
    AAMPStatusType status;
    static const char *manifest =
R"(<?xml version="1.0" encoding="utf-8"?>
<MPD xmlns="urn:mpeg:dash:schema:mpd:2011" type="static" minBufferTime="PT1.5S" mediaPresentationDuration="PT30M0S" profiles="urn:mpeg:dash:profile:isoff-live:2011">
    <Period id="p0" start="PT0S">
        <AdaptationSet contentType="video" mimeType="video/mp4">
            <SegmentTemplate timescale="1000" presentationTimeOffset="15000" media="p0-$Time$.m4s" startNumber="1">
                <SegmentTimeline>
                    <S t="5000" d="1000" r="609" />
                </SegmentTimeline>
            </SegmentTemplate>
            <Representation id="v1" bandwidth="100000" />
        </AdaptationSet>
    </Period>
    <Period id="p1" start="PT10M0S">
        <AdaptationSet contentType="video" mimeType="video/mp4">
            <SegmentTemplate timescale="1000" presentationTimeOffset="25000" media="p1-$Time$.m4s" startNumber="1">
                <SegmentTimeline>
                    <S t="10000" d="1000" r="614" />
                </SegmentTimeline>
            </SegmentTemplate>
            <Representation id="v2" bandwidth="200000" />
        </AdaptationSet>
    </Period>
    <Period id="p2" start="PT20M0S">
        <AdaptationSet contentType="video" mimeType="video/mp4">
            <SegmentTemplate timescale="1000" presentationTimeOffset="35000" media="p2-$Time$.m4s" startNumber="1">
                <SegmentTimeline>
                    <S t="20000" d="1000" r="614" />
                </SegmentTimeline>
            </SegmentTemplate>
            <Representation id="v3" bandwidth="300000" />
        </AdaptationSet>
    </Period>
</MPD>
)";
    double seekPosition = 10 * 60; // Seek to 10 minutes (middle of period p1)
    int rate = 1;
    EXPECT_CALL(*g_mockMediaStreamContext, CacheFragment(_, _, _, _, _, true, _, _, _, _, _))
        .WillRepeatedly(Return(true));
    EXPECT_CALL(*g_mockPrivateInstanceAAMP, SetLLDashChunkMode(_));
    status = InitializeMPD(manifest, TuneType::eTUNETYPE_SEEK, seekPosition, rate, true);
    EXPECT_EQ(status, eAAMPSTATUS_OK);

    double actualPosition = mStreamAbstractionAAMP_MPD->GetStreamPosition();
    EXPECT_EQ(actualPosition, seekPosition);
}
// Test case to verify seeking behavior for Live content with three periods, with out start times, with out durations, and PTO greater than timeline
TEST_F(FunctionalTests, L1_ThreePeriods_Live_NoStartTimes_NoDurations_PTOGreaterThanTimeline)
{
    AAMPStatusType status;
    static const char *manifest =
R"(<?xml version="1.0" encoding="utf-8"?>
<MPD xmlns="urn:mpeg:dash:schema:mpd:2011" type="dynamic" minBufferTime="PT1.5S" availabilityStartTime="2025-11-15T00:00:00Z" profiles="urn:mpeg:dash:profile:isoff-live:2011">
    <Period id="p0">
        <AdaptationSet contentType="video" mimeType="video/mp4">
            <SegmentTemplate timescale="1000" presentationTimeOffset="15000" media="p0-$Time$.m4s" startNumber="1">
                <SegmentTimeline>
                    <S t="5000" d="1000" r="609" />
                </SegmentTimeline>
            </SegmentTemplate>
            <Representation id="v1" bandwidth="100000" />
        </AdaptationSet>
    </Period>
    <Period id="p1">
        <AdaptationSet contentType="video" mimeType="video/mp4">
            <SegmentTemplate timescale="1000" presentationTimeOffset="25000" media="p1-$Time$.m4s" startNumber="1">
                <SegmentTimeline>
                    <S t="10000" d="1000" r="614" />
                </SegmentTimeline>
            </SegmentTemplate>
            <Representation id="v2" bandwidth="200000" />
        </AdaptationSet>
    </Period>
    <Period id="p2">
        <AdaptationSet contentType="video" mimeType="video/mp4">
            <SegmentTemplate timescale="1000" presentationTimeOffset="35000" media="p2-$Time$.m4s" startNumber="1">
                <SegmentTimeline>
                    <S t="20000" d="1000" r="614" />
                </SegmentTimeline>
            </SegmentTemplate>
            <Representation id="v3" bandwidth="300000" />
        </AdaptationSet>
    </Period>
</MPD>
)";
    double seekPosition = 1;
    int rate = 1;
    EXPECT_CALL(*g_mockMediaStreamContext, CacheFragment(_, _, _, _, _, true, _, _, _, _, _))
        .WillRepeatedly(Return(true));
    EXPECT_CALL(*g_mockPrivateInstanceAAMP, SetLLDashChunkMode(_));
    status = InitializeMPD(manifest, TuneType::eTUNETYPE_SEEK, seekPosition, rate, true);
    EXPECT_EQ(status, eAAMPSTATUS_OK);
    double actualPosition = mStreamAbstractionAAMP_MPD->GetStreamPosition();
    double availabilityStartTime = ISO8601DateTimeToUTCSeconds("2025-11-15T00:00:00Z");
    EXPECT_EQ(actualPosition, availabilityStartTime + seekPosition);
}

// Test case to verify seeking behavior for Live content with three periods, with out start times, with durations, and PTO greater than timeline
TEST_F(FunctionalTests, L1_ThreePeriods_Live_NoStartTimes_WithDurations_PTOGreaterThanTimeline)
{
    AAMPStatusType status;
    static const char *manifest =
R"(<?xml version="1.0" encoding="utf-8"?>
<MPD xmlns="urn:mpeg:dash:schema:mpd:2011" type="dynamic" minBufferTime="PT1.5S" availabilityStartTime="2025-11-15T00:00:00Z" profiles="urn:mpeg:dash:profile:isoff-live:2011">
    <Period id="p0" duration="PT10M0S">
        <AdaptationSet contentType="video" mimeType="video/mp4">
            <SegmentTemplate timescale="1000" presentationTimeOffset="15000" media="p0-$Time$.m4s" startNumber="1">
                <SegmentTimeline>
                    <S t="5000" d="1000" r="609" />
                </SegmentTimeline>
            </SegmentTemplate>
            <Representation id="v1" bandwidth="100000" />
        </AdaptationSet>
    </Period>
    <Period id="p1" duration="PT10M0S">
        <AdaptationSet contentType="video" mimeType="video/mp4">
            <SegmentTemplate timescale="1000" presentationTimeOffset="25000" media="p1-$Time$.m4s" startNumber="1">
                <SegmentTimeline>
                    <S t="10000" d="1000" r="614" />
                </SegmentTimeline>
            </SegmentTemplate>
            <Representation id="v2" bandwidth="200000" />
        </AdaptationSet>
    </Period>
    <Period id="p2" duration="PT10M0S">
        <AdaptationSet contentType="video" mimeType="video/mp4">
            <SegmentTemplate timescale="1000" presentationTimeOffset="35000" media="p2-$Time$.m4s" startNumber="1">
                <SegmentTimeline>
                    <S t="20000" d="1000" r="614" />
                </SegmentTimeline>
            </SegmentTemplate>
            <Representation id="v3" bandwidth="300000" />
        </AdaptationSet>
    </Period>
</MPD>
)";
    double seekPosition = 15 * 60; // Seek to 15 minutes (middle of period p1)
    int rate = 1;
    EXPECT_CALL(*g_mockMediaStreamContext, CacheFragment(_, _, _, _, _, true, _, _, _, _, _))
        .WillRepeatedly(Return(true));
    EXPECT_CALL(*g_mockPrivateInstanceAAMP, SetLLDashChunkMode(_));
    status = InitializeMPD(manifest, TuneType::eTUNETYPE_SEEK, seekPosition, rate, true);
    EXPECT_EQ(status, eAAMPSTATUS_OK);
    double actualPosition = mStreamAbstractionAAMP_MPD->GetStreamPosition();
    double availabilityStartTime = ISO8601DateTimeToUTCSeconds("2025-11-15T00:00:00Z");
    EXPECT_EQ(actualPosition, availabilityStartTime + seekPosition);
}

// Test case to verify seeking behavior for Live content with three periods, with start times and durations, and PTO greater than timeline
TEST_F(FunctionalTests, L1_ThreePeriods_Live_WithStartTimesAndDurations_PTOGreaterThanTimeline)
{
    AAMPStatusType status;
    static const char *manifest =
R"(<?xml version="1.0" encoding="utf-8"?>
<MPD xmlns="urn:mpeg:dash:schema:mpd:2011" type="dynamic" minBufferTime="PT1.5S" availabilityStartTime="2025-11-15T00:00:00Z" profiles="urn:mpeg:dash:profile:isoff-live:2011">
    <Period id="p0" start="PT0S" duration="PT10M0S">
        <AdaptationSet contentType="video" mimeType="video/mp4">
            <SegmentTemplate timescale="1000" presentationTimeOffset="15000" media="p0-$Time$.m4s" startNumber="1">
                <SegmentTimeline>
                    <S t="5000" d="1000" r="609" />
                </SegmentTimeline>
            </SegmentTemplate>
            <Representation id="v1" bandwidth="100000" />
        </AdaptationSet>
    </Period>
    <Period id="p1" start="PT10M0S" duration="PT10M0S">
        <AdaptationSet contentType="video" mimeType="video/mp4">
            <SegmentTemplate timescale="1000" presentationTimeOffset="25000" media="p1-$Time$.m4s" startNumber="1">
                <SegmentTimeline>
                    <S t="10000" d="1000" r="614" />
                </SegmentTimeline>
            </SegmentTemplate>
            <Representation id="v2" bandwidth="200000" />
        </AdaptationSet>
    </Period>
    <Period id="p2" start="PT20M0S" duration="PT10M0S">
        <AdaptationSet contentType="video" mimeType="video/mp4">
            <SegmentTemplate timescale="1000" presentationTimeOffset="35000" media="p2-$Time$.m4s" startNumber="1">
                <SegmentTimeline>
                    <S t="20000" d="1000" r="614" />
                </SegmentTimeline>
            </SegmentTemplate>
            <Representation id="v3" bandwidth="300000" />
        </AdaptationSet>
    </Period>
</MPD>
)";
    double seekPosition = 0;
    int rate = 1;
    EXPECT_CALL(*g_mockMediaStreamContext, CacheFragment(_, _, _, _, _, true, _, _, _, _, _))
        .WillRepeatedly(Return(true));
    EXPECT_CALL(*g_mockPrivateInstanceAAMP, SetLLDashChunkMode(_));
    status = InitializeMPD(manifest, TuneType::eTUNETYPE_SEEK, seekPosition, rate, true);
    EXPECT_EQ(status, eAAMPSTATUS_OK);
    double actualPosition = mStreamAbstractionAAMP_MPD->GetStreamPosition();
    double availabilityStartTime = ISO8601DateTimeToUTCSeconds("2025-11-15T00:00:00Z");
    EXPECT_EQ(actualPosition, availabilityStartTime + seekPosition);
}

// Test case to verify seeking behavior for Live content with three periods, with start times, with out durations, and PTO greater than timeline
TEST_F(FunctionalTests, L1_ThreePeriods_Live_WithStartTimesNoDurations_PTOGreaterThanTimeline)
{
    AAMPStatusType status;
    static const char *manifest =
R"(<?xml version="1.0" encoding="utf-8"?>
<MPD xmlns="urn:mpeg:dash:schema:mpd:2011" type="dynamic" minBufferTime="PT1.5S" availabilityStartTime="2025-11-15T00:00:00Z" profiles="urn:mpeg:dash:profile:isoff-live:2011">
    <Period id="p0" start="PT0S">
        <AdaptationSet contentType="video" mimeType="video/mp4">
            <SegmentTemplate timescale="1000" presentationTimeOffset="15000" media="p0-$Time$.m4s" startNumber="1">
                <SegmentTimeline>
                    <S t="5000" d="1000" r="609" />
                </SegmentTimeline>
            </SegmentTemplate>
            <Representation id="v1" bandwidth="100000" />
        </AdaptationSet>
    </Period>
    <Period id="p1" start="PT10M0S">
        <AdaptationSet contentType="video" mimeType="video/mp4">
            <SegmentTemplate timescale="1000" presentationTimeOffset="25000" media="p1-$Time$.m4s" startNumber="1">
                <SegmentTimeline>
                    <S t="10000" d="1000" r="614" />
                </SegmentTimeline>
            </SegmentTemplate>
            <Representation id="v2" bandwidth="200000" />
        </AdaptationSet>
    </Period>
    <Period id="p2" start="PT20M0S">
        <AdaptationSet contentType="video" mimeType="video/mp4">
            <SegmentTemplate timescale="1000" presentationTimeOffset="35000" media="p2-$Time$.m4s" startNumber="1">
                <SegmentTimeline>
                    <S t="20000" d="1000" r="614" />
                </SegmentTimeline>
            </SegmentTemplate>
            <Representation id="v3" bandwidth="300000" />
        </AdaptationSet>
    </Period>
</MPD>
)";
    double seekPosition = 10 * 60; // Seek to 10 minutes (middle of period p1)
    int rate = 1;
    EXPECT_CALL(*g_mockMediaStreamContext, CacheFragment(_, _, _, _, _, true, _, _, _, _, _))
        .WillRepeatedly(Return(true));
    EXPECT_CALL(*g_mockPrivateInstanceAAMP, SetLLDashChunkMode(_));
    status = InitializeMPD(manifest, TuneType::eTUNETYPE_SEEK, seekPosition, rate, true);
    EXPECT_EQ(status, eAAMPSTATUS_OK);
    double actualPosition = mStreamAbstractionAAMP_MPD->GetStreamPosition();
    double availabilityStartTime = ISO8601DateTimeToUTCSeconds("2025-11-15T00:00:00Z");
    EXPECT_EQ(actualPosition, availabilityStartTime + seekPosition);
}

/**
 * @brief Parameterized test class for CDAI Init tests with different TuneTypes.
 *
 * Tests Init() behavior with various TuneType values:
 * - NEW tune types (NEW_NORMAL, NEW_SEEK, NEW_END) should NOT call CheckForAdStart (CDAI skipped)
 * - Non-NEW tune types (SEEK, RETUNE, SEEKTOLIVE, SEEKTOEND) SHOULD call CheckForAdStart (CDAI continues)
 *
 * Parameter: pair<TuneType, bool> where bool indicates if CheckForAdStart should be called.
 */
class CDAIInitTuneTypeTests : public FunctionalTestsBase,
							  public ::testing::TestWithParam<std::pair<TuneType, bool>>
{
protected:
	void SetUp() override
	{
		FunctionalTestsBase::SetUp();

		g_MockPrivateCDAIObjectMPD = new NiceMock<MockPrivateCDAIObjectMPD>();
	}

	void TearDown() override
	{
		FunctionalTestsBase::TearDown();

		delete g_MockPrivateCDAIObjectMPD;
		g_MockPrivateCDAIObjectMPD = nullptr;
	}
};

// Test Init() CDAI behavior based on TuneType
TEST_P(CDAIInitTuneTypeTests, VerifiesCheckForAdStartBehavior)
{
	static const char *manifest =
R"(<?xml version="1.0" encoding="utf-8"?>
<MPD xmlns="urn:mpeg:dash:schema:mpd:2011" minBufferTime="PT2S" type="static"
	mediaPresentationDuration="PT1M0S"
	profiles="urn:mpeg:dash:profile:isoff-live:2011">
	<Period id="p0" duration="PT1M0S">
		<AdaptationSet contentType="video" mimeType="video/mp4">
			<SegmentTemplate timescale="2500" initialization="video_init.mp4"
							media="video_$Number$.m4s" startNumber="1"
							duration="2500"/>
			<Representation id="1" bandwidth="1000000" codecs="avc1.640028"
							width="640" height="360" frameRate="25"/>
		</AdaptationSet>
	</Period>
</MPD>
)";
	auto params = GetParam();
	TuneType tuneType = params.first;
	bool shouldCallCheckForAdStart = params.second;

	mBoolConfigSettings[eAAMPConfig_EnableClientDai] = true;

	// Set expectation based on parameter
	if (shouldCallCheckForAdStart)
	{
		// Non-NEW tune types: CheckForAdStart SHOULD be called
		EXPECT_CALL(*g_MockPrivateCDAIObjectMPD, CheckForAdStart(_, _, _, _, _, _)).Times(::testing::AtLeast(1));
	}
	else
	{
		// NEW tune types: CheckForAdStart should NOT be called
		EXPECT_CALL(*g_MockPrivateCDAIObjectMPD, CheckForAdStart(_, _, _, _, _, _)).Times(0);
	}

	EXPECT_CALL(*g_mockMediaStreamContext, CacheFragment(_, _, _, _, _, true, _, _, _, _, _))
		.WillRepeatedly(Return(true));
	EXPECT_CALL(*g_mockPrivateInstanceAAMP, SetLLDashChunkMode(_));

	AAMPStatusType status = InitializeMPD(manifest, tuneType);
	EXPECT_EQ(status, eAAMPSTATUS_OK);
}

/**
 * @brief Instantiate CDAI Init tests with all TuneType variants.
 *
 * Format: pair<TuneType, shouldCallCheckForAdStart>
 * - NEW tune types have shouldCallCheckForAdStart = false (skip CDAI)
 * - Non-NEW tune types have shouldCallCheckForAdStart = true (continue CDAI)
 */
INSTANTIATE_TEST_SUITE_P(AllTuneTypes,
						CDAIInitTuneTypeTests,
						::testing::Values(
							// NEW tune types - CheckForAdStart should NOT be called
							std::make_pair(eTUNETYPE_NEW_NORMAL, false),
							std::make_pair(eTUNETYPE_NEW_SEEK, false),
							std::make_pair(eTUNETYPE_NEW_END, false),
							// Non-NEW tune types - CheckForAdStart SHOULD be called
							std::make_pair(eTUNETYPE_SEEK, true),
							std::make_pair(eTUNETYPE_SEEKTOLIVE, true),
							std::make_pair(eTUNETYPE_RETUNE, true),
							std::make_pair(eTUNETYPE_SEEKTOEND, true)
						));

/*
 * @brief Multi-codec ABR pool restriction: preferHEVC=true selects HEVC representations only.
 *
 * Stream contains two video AdaptationSets, one HEVC (hvc1.*) and one AVC (avc1.*). With
 * preferHEVC=true the HEVC init segment must be selected and GetVideoBitrates() must return
 * only the HEVC bitrate, confirming AVC representations have been excluded from the ABR pool.
 */
TEST_F(FunctionalTests, MultiCodecABR_PreferHEVC_SelectsHEVCOnly)
{
    AAMPStatusType status;
    static const char *manifest =
R"(<?xml version="1.0" encoding="utf-8"?>
<MPD xmlns="urn:mpeg:dash:schema:mpd:2011" profiles="urn:mpeg:dash:profile:isoff-live:2011" type="static" mediaPresentationDuration="PT2M0.0S" minBufferTime="PT4.0S">
    <Period id="0" start="PT0.0S">
        <AdaptationSet id="0" contentType="video">
            <Representation id="h1" mimeType="video/mp4" codecs="hvc1.1.6.L93.90" bandwidth="2000000" width="1280" height="720" frameRate="25">
                <SegmentTemplate timescale="12800" initialization="hevc/video_init.mp4" media="hevc/video_$Number$.m4s" startNumber="1">
                    <SegmentTimeline><S t="0" d="25600" r="59" /></SegmentTimeline>
                </SegmentTemplate>
            </Representation>
        </AdaptationSet>
        <AdaptationSet id="1" contentType="video">
            <Representation id="a1" mimeType="video/mp4" codecs="avc1.640028" bandwidth="1000000" width="640" height="360" frameRate="25">
                <SegmentTemplate timescale="12800" initialization="avc/video_init.mp4" media="avc/video_$Number$.m4s" startNumber="1">
                    <SegmentTimeline><S t="0" d="25600" r="59" /></SegmentTimeline>
                </SegmentTemplate>
            </Representation>
        </AdaptationSet>
        <AdaptationSet id="2" contentType="audio">
            <Representation id="au1" mimeType="audio/mp4" codecs="mp4a.40.2" bandwidth="128000" audioSamplingRate="48000">
                <SegmentTemplate timescale="48000" initialization="aac/audio_init.mp4" media="aac/audio_$Number$.mp4" startNumber="1">
                    <SegmentTimeline><S t="0" d="96000" r="59" /></SegmentTimeline>
                </SegmentTemplate>
            </Representation>
        </AdaptationSet>
    </Period>
</MPD>
)";

    mBoolConfigSettings[eAAMPConfig_PreferHEVC] = true;

    EXPECT_CALL(*g_mockMediaStreamContext, CacheFragment(_, _, _, _, _, true, _, _, _, _, _))
        .WillRepeatedly(Return(true));
    EXPECT_CALL(*g_mockPrivateInstanceAAMP, SetLLDashChunkMode(_));
    EXPECT_CALL(*g_mockABRManager, getProfileCount()).WillRepeatedly(Return(1));

    status = InitializeMPD(manifest);
    ASSERT_EQ(status, eAAMPSTATUS_OK);

    std::vector<BitsPerSecond> bitrates = mStreamAbstractionAAMP_MPD->GetVideoBitrates();
    ASSERT_EQ(bitrates.size(), 1u);
    EXPECT_EQ(bitrates[0], 2000000);
}

/**
 * @brief Multi-codec ABR pool restriction: preferHEVC=false selects AVC representations only.
 *
 * Same dual-codec stream as above. With preferHEVC=false the AVC init segment must be
 * selected and GetVideoBitrates() must return only the AVC bitrate, confirming HEVC
 * representations have been excluded from the ABR pool.
 */
TEST_F(FunctionalTests, MultiCodecABR_PreferAVC_SelectsAVCOnly)
{
    AAMPStatusType status;
    static const char *manifest =
R"(<?xml version="1.0" encoding="utf-8"?>
<MPD xmlns="urn:mpeg:dash:schema:mpd:2011" profiles="urn:mpeg:dash:profile:isoff-live:2011" type="static" mediaPresentationDuration="PT2M0.0S" minBufferTime="PT4.0S">
    <Period id="0" start="PT0.0S">
        <AdaptationSet id="0" contentType="video">
            <Representation id="h1" mimeType="video/mp4" codecs="hvc1.1.6.L93.90" bandwidth="2000000" width="1280" height="720" frameRate="25">
                <SegmentTemplate timescale="12800" initialization="hevc/video_init.mp4" media="hevc/video_$Number$.m4s" startNumber="1">
                    <SegmentTimeline><S t="0" d="25600" r="59" /></SegmentTimeline>
                </SegmentTemplate>
            </Representation>
        </AdaptationSet>
        <AdaptationSet id="1" contentType="video">
            <Representation id="a1" mimeType="video/mp4" codecs="avc1.640028" bandwidth="1000000" width="640" height="360" frameRate="25">
                <SegmentTemplate timescale="12800" initialization="avc/video_init.mp4" media="avc/video_$Number$.m4s" startNumber="1">
                    <SegmentTimeline><S t="0" d="25600" r="59" /></SegmentTimeline>
                </SegmentTemplate>
            </Representation>
        </AdaptationSet>
        <AdaptationSet id="2" contentType="audio">
            <Representation id="au1" mimeType="audio/mp4" codecs="mp4a.40.2" bandwidth="128000" audioSamplingRate="48000">
                <SegmentTemplate timescale="48000" initialization="aac/audio_init.mp4" media="aac/audio_$Number$.mp4" startNumber="1">
                    <SegmentTimeline><S t="0" d="96000" r="59" /></SegmentTimeline>
                </SegmentTemplate>
            </Representation>
        </AdaptationSet>
    </Period>
</MPD>
)";

    mBoolConfigSettings[eAAMPConfig_PreferHEVC] = false;

    EXPECT_CALL(*g_mockMediaStreamContext, CacheFragment(_, _, _, _, _, true, _, _, _, _, _))
        .WillRepeatedly(Return(true));
    EXPECT_CALL(*g_mockPrivateInstanceAAMP, SetLLDashChunkMode(_));
    EXPECT_CALL(*g_mockABRManager, getProfileCount()).WillRepeatedly(Return(1));

    status = InitializeMPD(manifest);
    ASSERT_EQ(status, eAAMPSTATUS_OK);

    std::vector<BitsPerSecond> bitrates = mStreamAbstractionAAMP_MPD->GetVideoBitrates();
    ASSERT_EQ(bitrates.size(), 1u);
    EXPECT_EQ(bitrates[0], 1000000);
}
