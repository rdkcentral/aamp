/*
 * If not stated otherwise in this file or this component's license file the
 * following copyright and licenses apply:
 *
 * Copyright 2018 RDK Management
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

/**
 * @file main_aamp.cpp
 * @brief Advanced Adaptive Media Player (AAMP)
 */


#include "main_aamp.h"
#include "AampConfig.h"
#include "AampCacheHandler.h"
#include "AampUtils.h"
#include "PlayerCCManager.h"
#include "DrmHelper.h"
#include "StreamAbstractionAAMP.h"
#include "AampStreamSinkManager.h"
#include "PlayerExternalsInterface.h"
#include "PlayerLogManager.h"
#include "PlayerMetadata.hpp"
#include "PlayerLogManager.h"

#include <dlfcn.h>
#include <termios.h>
#include <errno.h>
#include <regex>

//#include "priv_aamp.cpp"

AampConfig *gpGlobalConfig=NULL;

#include "ContentSecurityManager.h"

std::mutex PlayerInstanceAAMP::mPrvAampMtx;

static int PLAYERID_CNTR = 0;

/**
 *  @brief PlayerInstanceAAMP Constructor.
 */
PlayerInstanceAAMP::PlayerInstanceAAMP(StreamSink* streamSink
	, std::function< void(const unsigned char *, int, int, int) > exportFrames
	) : // aamp(NULL), sp_aamp(nullptr),
    mJSBinding_DL(),mAsyncRunning(false),mConfig(),mAsyncTuneEnabled(false),mScheduler(),

mReportProgressPosn(0.0), mLastTelemetryTimeMS(0), mDiscontinuityFound(false), mTelemetryInterval(0), mAbrBitrateData(), mLock(),
   mpStreamAbstractionAAMP(NULL), mInitSuccess(false), mVideoFormat(FORMAT_INVALID), mAudioFormat(FORMAT_INVALID), mDownloadsDisabled(),
   mDownloadsEnabled(true), profiler(), licenceFromManifest(false), previousAudioType(eAUDIO_UNKNOWN),isPreferredDRMConfigured(false),
   mbDownloadsBlocked(false), streamerIsActive(false), mFogTSBEnabled(false), mIscDVR(false), mLiveOffset(AAMP_LIVE_OFFSET),
   seek_pos_seconds(-1), rate(0), pipeline_paused(false), mMaxLanguageCount(0), zoom_mode(VIDEO_ZOOM_NONE),
   video_muted(false), subtitles_muted(true), audio_volume(100), subscribedTags(), manifestHeadersNeeded(), httpHeaderResponses(), timedMetadata(), timedMetadataNew(), IsTuneTypeNew(false), trickStartUTCMS(-1), durationSeconds(0.0), culledSeconds(0.0), culledOffset(0.0), maxRefreshPlaylistIntervalSecs(DEFAULT_INTERVAL_BETWEEN_PLAYLIST_UPDATES_MS/1000),
   mEventListener(NULL), mNewSeekInfo(), discardEnteringLiveEvt(false),
   mIsRetuneInProgress(false), mCondDiscontinuity(), mDiscontinuityTuneOperationId(0), mIsVSS(false),
   m_fd(-1), mIsLive(false), mIsAudioContextSkipped(false), mLogTune(false), mTuneCompleted(false), mFirstTune(true), mfirstTuneFmt(-1), mTuneAttempts(0), mPlayerLoadTime(0),
   mState(eSTATE_RELEASED), mMediaFormat(eMEDIAFORMAT_HLS), mPersistedProfileIndex(0), mAvailableBandwidth(0),
   mDiscontinuityTuneOperationInProgress(false), mContentType(ContentType_UNKNOWN), mTunedEventPending(false),
   mSeekOperationInProgress(false), mTrickplayInProgress(false), mPendingAsyncEvents(), mCustomHeaders(),
   mManifestUrl(""), mTunedManifestUrl(""), mOrigManifestUrl(), mServiceZone(), mVssVirtualStreamId(),
   mCurrentLanguageIndex(0),
   preferredLanguagesString(), preferredLanguagesList(), preferredLabelList(),mhAbrManager(),
   mVideoEnd(NULL),
   //mTimeToTopProfile(0),
   mTimeAtTopProfile(0),mPlaybackDuration(0),mTraceUUID(),
   mIsFirstRequestToFOG(false),
   mPausePositionMonitorMutex(), mPausePositionMonitorCV(), mPausePositionMonitoringThreadID(), mPausePositionMonitoringThreadStarted(false),
   mTuneType(eTUNETYPE_NEW_NORMAL)
   ,mCdaiObject(NULL), mAdEventsQ(),mAdEventQMtx(), mAdPrevProgressTime(0), mAdCurOffset(0), mAdDuration(0), mAdProgressId(""), mAdAbsoluteStartTime(0)
   ,mBufUnderFlowStatus(false), mVideoBasePTS(0)
   ,mCustomLicenseHeaders(), mIsIframeTrackPresent(false), mManifestTimeoutMs(-1), mNetworkTimeoutMs(-1)
   ,mbPlayEnabled(true), mPlayerPreBuffered(false), mPlayerId(PLAYERID_CNTR++),mAampCacheHandler(NULL)
   //,mAsyncTuneEnabled(false)
   ,waitforplaystart()
   ,mCurlShared(NULL)
   ,mDrmDecryptFailCount(MAX_SEG_DRM_DECRYPT_FAIL_COUNT)
   ,mPlaylistTimeoutMs(-1)
   ,mMutexPlaystart()
   ,mNetworkBandwidth(0)
   ,mTimeToTopProfile(0)
   , fragmentCdmEncrypted(false) ,drmParserMutex(), aesCtrAttrDataList()
   , drmSessionThreadStarted(false), createDRMSessionThreadID()
   , mDRMLicenseManager(NULL)
   ,  mPreCachePlaylistThreadId(), mPreCacheDnldList()
   , mPreCacheDnldTimeWindow(0), mParallelPlaylistFetchLock(), mAppName()
   , mProgressReportFromProcessDiscontinuity(false)
   , mPlaylistFetchFailError(0L),mAudioDecoderStreamSync(true)
   , mPrevPositionMilliseconds()
   , mGetPositionMillisecondsMutexHard()
   , mGetPositionMillisecondsMutexSoft()
   , mPausePositionMilliseconds(AAMP_PAUSE_POSITION_INVALID_POSITION)
   , mCurrentDrm(), mDrmInitData(), mMinInitialCacheSeconds(DEFAULT_MINIMUM_INIT_CACHE_SECONDS)
   //, mLicenseServerUrls()
   , mFragmentCachingRequired(false), mFragmentCachingLock()
   , mPauseOnFirstVideoFrameDisp(false)
   , mPreferredTextTrack(), mFirstVideoFrameDisplayedEnabled(false)
   , mSessionToken()
   , vDynamicDrmData()
   , midFragmentSeekCache(false)
   , mLiveOffsetDrift(AAMP_DEFAULT_LIVE_OFFSET_DRIFT)
   , mDisableRateCorrection (false)
   , mRateCorrectionThread ()
   , mRateCorrectionWait()
   , mRateCorrectionTimeoutLock()
   , mAbortRateCorrection (false)
   , mCorrectionRate(AAMP_NORMAL_PLAY_RATE)
   , mPreviousAudioType (FORMAT_INVALID)
   , mTsbRecordingId()
   , mthumbIndexValue(-1)
   , mManifestRefreshCount (0)
   , mJumpToLiveFromPause(false), mPausedBehavior(ePAUSED_BEHAVIOR_AUTOPLAY_IMMEDIATE), mSeekFromPausedState(false)
   , mProgramDateTime (0), mMPDPeriodsInfo()
   , mProfileCappedStatus(false),mSchemeIdUriDai("")
   , mDisplayWidth(0)
   , mDisplayHeight(0)
   , preferredRenditionString("")
   , preferredRenditionList()
   , preferredTypeString("")
   , preferredCodecString("")
   , preferredCodecList()
   , mAudioTuple()
   , preferredLabelsString("")
   , preferredAudioAccessibilityNode()
   , preferredTextLanguagesString("")
   , preferredTextLanguagesList()
   , preferredTextRenditionString("")
   , preferredTextTypeString("")
   , preferredTextLabelString("")
   , preferredTextAccessibilityNode()
   , preferredInstreamIdString("")
   , preferredTextNameString("")
   , preferredNameString("")
   , mProgressReportOffset(-1)
   , mFirstFragmentTimeOffset(-1)
   , mProgressReportAvailabilityOffset(-1)
   , mAutoResumeTaskId(AAMP_TASK_ID_INVALID)
   , mAutoResumeTaskPending(false)
   , _mScheduler(NULL)
   , mEventLock()
   , mEventPriority(G_PRIORITY_DEFAULT_IDLE)
   , mStreamLock()
//   , mConfig (config)
   , mSubLanguage()
   , preferredSubtitleLanguageVctr()
   , mHarvestCountLimit(0)
   , mHarvestConfig(0)
   , mIsWVKIDWorkaround(false)
   , mAuxFormat(FORMAT_INVALID), mAuxAudioLanguage()
   , mAbsoluteEndPosition(0), mIsLiveStream(false)
   , mbUsingExternalPlayer (false)
   , mCCId(0)
   , seiTimecode()
   , contentGaps()
   , mAampLLDashServiceData{}
   , bLowLatencyServiceConfigured(false)
   , bLLDashAdjustPlayerSpeed(false)
   , mLLDashCurrentPlayRate(AAMP_NORMAL_PLAY_RATE)
   , vidTimeScale(0)
   , audTimeScale(0)
   , subTimeScale(0)
   , speedCache {}
   , mCurrentLatency(0)
   , mLiveOffsetAppRequest(false)
   , bLowLatencyStartABR(false)
   , mEventManager (NULL)
   , mCMCDCollector(NULL)
   , mbDetached(false)
   , mIsFakeTune(false)
   , mCurrentAudioTrackId(-1)
   , mCurrentVideoTrackId(-1)
   , mIsTrackIdMismatch(false)
   , mIsDefaultOffset(false)
   , mNextPeriodDuration(0)
   , mNextPeriodStartTime(0)
   , mNextPeriodScaledPtoStartTime(0)
   , mOffsetFromTunetimeForSAPWorkaround(0)
   , mLanguageChangeInProgress(false)
   , mAampTsbLanguageChangeInProgress(false)
   , mSupportedTLSVersion(0)
   , mbSeeked(false)
   , mFailureReason("")
   , mTimedMetadataStartTime(0)
   , mTimedMetadataDuration(0)
   , playerStartedWithTrickPlay(false)
   , mPlaybackMode("UNKNOWN")
   , mApplyVideoRect(false)
   , mApplyContentRestriction(false)
   , mVideoRect{}
   , mData()
   , mIsInbandCC(true)
   , bitrateList()
   , userProfileStatus(false)
   , mApplyCachedVideoMute(false)
   , mFirstProgress(false)
   , mTsbSessionRequestUrl()
   , mcurrent_keyIdArray()
   , mDynamicDrmDefaultconfig()
   , mWaitForDynamicDRMToUpdate()
   , mDynamicDrmUpdateLock()
   , mDynamicDrmCache()
   , mAudioComponentCount(-1)
   , mAudioDelta(0)
   , mSubtitleDelta(0)
   , mVideoComponentCount(-1)
   , mAudioOnlyPb(false)
   , mVideoOnlyPb(false)
   , mCurrentAudioTrackIndex(-1)
   , mCurrentTextTrackIndex(-1)
   , mMediaDownloadsEnabled()
   , playerrate(1.0)
   , mSetPlayerRateAfterFirstframe(false)
   , mEncryptedPeriodFound(false)
   , mPipelineIsClear(false)
   , mLLActualOffset(-1)
   , mIsStream4K(false)
   , mTextStyle()
   , mFogDownloadFailReason("")
   , mBlacklistedProfiles()
   , mBufferFor4kRampup(0)
   , mBufferFor4kRampdown(0)
   , mId3MetadataCache{}
   , mMPDDownloaderInstance(nullptr)
   , mMPDStichOption(OPT_1_FULL_MANIFEST_TUNE),mMPDStichRefreshUrl("")
   , mTsbType("none")
   , mTsbDepthMs(0)
   , mDiscStartTime(0)
   , mRateCorrectionDelay(false)
   , mDownloadDelay(0)
   , curlhost{}
   , mWaitForDiscoToComplete()
   , mDiscoCompleteLock()
   , mIsPeriodChangeMarked(false)
   , m_lastSubClockSyncTime()
   , mIsLoggingNeeded(false)
   , mLiveEdgeDeltaFromCurrentTime(0.0)
   , mTrickModePositionEOS(0.0)
   , mTSBSessionManager(NULL)
   , mLocalAAMPTsb(false), mLocalAAMPInjectionEnabled(false)
   , mLocalAAMPTsbFromConfig(false)
   , mbPauseOnStartPlayback(false)
   , mTSBStore(nullptr)
   , mIsFlushFdsInCurlStore(false)
   , mProvidedManifestFile("")
   , mIsChunkMode(false)
   , prevFirstPeriodStartTime(0)
   , mIsFlushOperationInProgress(false)
{
//Need to do iarm initialization process before reading the tr181 aamp parameters.
//Using printf here since AAMP logs can only use after creating the global object
	static bool iarmInitialized = false;
	if(!iarmInitialized)
	{
			char processName[20] = {0};

			snprintf(processName, sizeof(processName), "PLAYER-%u", getpid());

			PlayerExternalsInterface::IARMInit(processName);


			iarmInitialized = true;
	}

	// Create very first instance of Aamp Config to read the cfg & Operator file .This is needed for very first
	// tune only . After that every tune will use the same config parameters
	if(gpGlobalConfig == NULL)
	{
		curl_global_init(CURL_GLOBAL_DEFAULT);
		auto vers = curl_version_info(CURLVERSION_NOW);
		printf( "curl version: %s\n", vers->version );

		gpGlobalConfig =  new AampConfig();
		gpGlobalConfig->Initialize();
		gpGlobalConfig->ApplyDeviceCapabilities();
		SetPlayerName(PLAYER_NAME);

		AAMPLOG_MIL("[AAMP_JS][%p]Creating GlobalConfig Instance[%p]",this,gpGlobalConfig);
		if(!gpGlobalConfig->ReadAampCfgTxtFile())
		{
			if(!gpGlobalConfig->ReadAampCfgJsonFile())
			{
				gpGlobalConfig->ReadAampCfgFromEnv();
			}
		}
		gpGlobalConfig->ReadOperatorConfiguration();
		gpGlobalConfig->ShowDevCfgConfiguration();
		gpGlobalConfig->ShowOperatorSetConfiguration();
	}

#ifdef SUPPORT_JS_EVENTS
#ifdef AAMP_WPEWEBKIT_JSBINDINGS //aamp_LoadJS defined in libaampjsbindings.so
	const char* szJSLib = "libaampjsbindings.so";
#else
	const char* szJSLib = "libaamp.so";
#endif
	mJSBinding_DL = dlopen(szJSLib, RTLD_GLOBAL | RTLD_LAZY);
	AAMPLOG_WARN("[AAMP_JS] dlopen(\"%s\")=%p", szJSLib, mJSBinding_DL);
#endif

#ifdef AAMP_BUILD_INFO
		std::string tmpstr = MACRO_TO_STRING(AAMP_BUILD_INFO);
		AAMPLOG_MIL("AAMP_BUILD_INFO: %s",tmpstr.c_str());
#endif
	// Copy the default configuration to session configuration .
	// App can modify the configuration set
	mConfig = *gpGlobalConfig;

	// sd_journal logging doesn't work with AAMP/Rialto running in Container, so route to Ethan Logger instead
	AampLogManager::enableEthanLogRedirection = mConfig.IsConfigSet(eAAMPConfig_useRialtoSink);

	PlayerLogManager::SetLoggerInfo(AampLogManager::disableLogRedirection, AampLogManager::enableEthanLogRedirection, AampLogManager::aampLoglevel, AampLogManager::locked);
	
    // FIXME
	// sp_aamp = std::make_shared<PlayerInstanceAAMP>(&mConfig);
	// aamp = sp_aamp.get();
    _Init();
    
	UsingPlayerId playerId(mPlayerId);

	// start Scheduler Worker for task handling
	mScheduler.StartScheduler(mPlayerId);
	if (NULL == streamSink)
	{
		auto id3_metadata_handler = std::bind(
                                              &PlayerInstanceAAMP::_ID3MetadataHandler,
                                              this,
                                              std::placeholders::_1, std::placeholders::_2, std::placeholders::_3, std::placeholders::_4, std::placeholders::_5);

		AampStreamSinkManager::GetInstance().CreateStreamSink(this, id3_metadata_handler, exportFrames);
	}
	else
	{
		AampStreamSinkManager::GetInstance().SetStreamSink(this, streamSink);

		// Disable async tune in aamp as plugin mode, since it already called from aamp gst as async call
		SETCONFIGVALUE_PRIV(AAMP_APPLICATION_SETTING, eAAMPConfig_AsyncTune, false);
		mAsyncRunning = false;
	}
	if(FIRST_PLAYER_INSTANCE_ID ==  mPlayerId)
	{
		/** Create fake tsbStore and delete all the content if any*/
		TSB::Store::Config config;
		config.location			=	GETCONFIGVALUE_PRIV(eAAMPConfig_TsbLocation);
		config.minFreePercentage	=	GETCONFIGVALUE_PRIV(eAAMPConfig_TsbMinDiskFreePercentage);
		config.maxCapacity =  GETCONFIGVALUE_PRIV(eAAMPConfig_TsbMaxDiskStorage);
		TSB::LogLevel level = ConvertTsbLogLevel(GETCONFIGVALUE_PRIV(eAAMPConfig_TsbLogLevel)) ;
		try
		{
			std::shared_ptr<TSB::Store> tSBStore = std::make_shared<TSB::Store>(config, AampLogManager::aampLogger, mPlayerId, level);
			if(tSBStore)
			{
				/**< Creating new TSB store object will automatically flush the storage*/
				AAMPLOG_WARN("[AAMP_PLAYER] TSB with path : %s !!", config.location.c_str());
			}
		}
		catch (std::exception &e)
		{
			// This is expected if an AAMP TSB instance is currently alive in another process
			AAMPLOG_WARN("Failed to instantiate TSB Store object for flush, reason: %s", e.what());
		}
	}
	_SetScheduler(&mScheduler);
	AsyncStartStop();
}


/**
 *  @brief PlayerInstanceAAMP Destructor.
 */
PlayerInstanceAAMP::~PlayerInstanceAAMP()
{
    UsingPlayerId playerId(mPlayerId);
    AAMPPlayerState state = _GetState();
    // Acquire the lock , to prevent new entries into scheduler
    mScheduler.SuspendScheduler();
    // Remove all the tasks
    mScheduler.RemoveAllTasks();
    if (state != eSTATE_IDLE && state != eSTATE_RELEASED)
    {
        _Stop( true );
    }
    std::lock_guard<std::mutex> lock (mPrvAampMtx);
    
	// Stop the scheduler
	mAsyncRunning = false;
	mScheduler.StopScheduler();

	bool isLastPlayerInstance = !PlayerInstanceAAMP::_IsActiveInstancePresent();

	if (isLastPlayerInstance)
	{
		PlayerCCManager::DestroyInstance();
	}
#ifdef SUPPORT_JS_EVENTS
	if (mJSBinding_DL && isLastPlayerInstance)
	{
		AAMPLOG_WARN("[AAMP_JS] dlclose(%p)", mJSBinding_DL);
		dlclose(mJSBinding_DL);
	}
#endif
	if (isLastPlayerInstance)
	{
		ContentSecurityManager::DestroyInstance();
	}
	if (isLastPlayerInstance && gpGlobalConfig)
	{
		AAMPLOG_WARN("[%p] Release GlobalConfig(%p)",this,gpGlobalConfig);
		SAFE_DELETE(gpGlobalConfig);
	}
    
    _Term();
}


/**
 *   @brief API to reset configuration across tunes for single player instance
 */
void PlayerInstanceAAMP::ResetConfiguration()
{
	UsingPlayerId playerId(mPlayerId);
	AAMPLOG_WARN("Resetting Configuration to default values ");
	// Copy the default configuration to session configuration .App can modify the configuration set
	mConfig = *gpGlobalConfig;

	// Based on the default condition , reset the AsyncTune scheduler
	AsyncStartStop();
}

/**
 *  @brief Stop playback and release resources.
 */
void PlayerInstanceAAMP::Stop(bool sendStateChangeEvent)
{
    UsingPlayerId playerId(mPlayerId);
    AAMPPlayerState state = _GetState();
    
    // 1. Ensure scheduler is suspended and all tasks if any to be cleaned
    // 2. Check for state ,if already in Idle / Released , ignore stopInternal
    // 3. Restart the scheduler , needed if same instance is used for tune again
    
    mScheduler.SuspendScheduler();
    mScheduler.RemoveAllTasks();
    
    //state will be eSTATE_IDLE or eSTATE_RELEASED, right after an init or post-processing of a Stop call
    if (state != eSTATE_IDLE && state != eSTATE_RELEASED)
    {
        StopInternal(sendStateChangeEvent);
    }
    
    //Release lock
    mScheduler.ResumeScheduler();
}

/**
 * @brief older variant for backwards compatibility - to be deprecated.
 */
void PlayerInstanceAAMP::Tune(const char *mainManifestUrl, const char *contentType, bool bFirstAttempt, bool bFinalAttempt,const char *traceUUID,bool audioDecoderStreamSync)
{
	Tune(mainManifestUrl, /*autoPlay*/ true, contentType,bFirstAttempt,bFinalAttempt,traceUUID,audioDecoderStreamSync);
}

/**
 *  @brief Tune to a URL.
 */
void PlayerInstanceAAMP::Tune(const char *mainManifestUrl,
								bool autoPlay,
								const char *contentType,
								bool bFirstAttempt,
								bool bFinalAttempt,
								const char *traceUUID,
								bool audioDecoderStreamSync,
								const char *refreshManifestUrl,
								int mpdStitchingMode,
								std::string sid,
								const char *manifestData
								)
{
	ManageAsyncTuneConfig(mainManifestUrl);
	if(mAsyncTuneEnabled)
	{
		const std::string manifest {mainManifestUrl};
		const std::string cType = (contentType != NULL) ? std::string(contentType) : std::string();
		const std::string sTraceUUID = (traceUUID != NULL)? std::string(traceUUID) : std::string();

		mScheduler.ScheduleTask(AsyncTaskObj(
			[manifest, autoPlay , cType, bFirstAttempt, bFinalAttempt, sTraceUUID, audioDecoderStreamSync, refreshManifestUrl, mpdStitchingMode, sid,manifestData](void *data)
			{
				PlayerInstanceAAMP *instance = static_cast<PlayerInstanceAAMP *>(data);
				const char * trace_uuid = sTraceUUID.empty() ? nullptr : sTraceUUID.c_str();

				instance->TuneInternal(manifest.c_str(), autoPlay, cType.c_str(), bFirstAttempt,
										bFinalAttempt, trace_uuid, audioDecoderStreamSync, refreshManifestUrl, mpdStitchingMode, std::move(sid),manifestData);
			},
			(void *) this,
			__FUNCTION__));
	}
	else
	{
		TuneInternal(mainManifestUrl, autoPlay , contentType, bFirstAttempt, bFinalAttempt,traceUUID,audioDecoderStreamSync, refreshManifestUrl, mpdStitchingMode, std::move(sid),manifestData);
	}
}

/**
 * @brief Tune to a URL.
 */
void PlayerInstanceAAMP::TuneInternal(const char *mainManifestUrl,
										bool autoPlay,
										const char *contentType,
										bool bFirstAttempt,
										bool bFinalAttempt,
										const char *traceUUID,
										bool audioDecoderStreamSync,
										const char *refreshManifestUrl,
										int mpdStitchingMode,
										std::string sid,
										const char* manifestData
										)
{
    UsingPlayerId playerId(mPlayerId);
    
    /* Set single pipeline according to the configuration */
    _UpdateUseSinglePipeline();
    
    _StopPausePositionMonitoring("Tune() called");
    
    AAMPPlayerState state = _GetState();
    bool IsOTAtoOTA =  false;
    
    if((_IsOTAContent()) && (NULL != mainManifestUrl))
    {
        /* OTA to OTA tune does not need to call stop. */
        std::string urlStr(mainManifestUrl); // for convenience, convert to std::string
        if((urlStr.rfind("live:",0)==0) || (urlStr.rfind("tune:",0)==0))
        {
            IsOTAtoOTA = true;
        }
    }
    
    if ((state != eSTATE_IDLE) && (state != eSTATE_RELEASED) && (!IsOTAtoOTA))
    {
        //Calling tune without closing previous tune
        StopInternal(false);
    }
    _getAampCacheHandler()->StartPlaylistCache();
    _Tune(mainManifestUrl, autoPlay, contentType, bFirstAttempt, bFinalAttempt, traceUUID, audioDecoderStreamSync, refreshManifestUrl, mpdStitchingMode, std::move(sid),manifestData);
}

/**
 *  @brief Returns the session ID from the internal player, if present, or an empty string, if not.
 */
std::string PlayerInstanceAAMP::GetSessionId() const {
    return _GetSessionId();
}

/**
 *  @brief Soft stop the player instance.
 */
void PlayerInstanceAAMP::detach()
{
	// detach is similar to Stop , need to run like stop in Sync mode
    UsingPlayerId playerId(mPlayerId);

	//Acquire lock
	mScheduler.SuspendScheduler();
	_StopPausePositionMonitoring("detach() called");
	_detach();
	//Release lock
	mScheduler.ResumeScheduler();
}

/**
 *  @brief Register event handler.
 */
void PlayerInstanceAAMP::RegisterEvent(AAMPEventType type, EventListener* listener)
{
	_RegisterEvent(type, listener);
}

/**
 *  @brief Register event handler.
 */
void PlayerInstanceAAMP::RegisterEvents(EventListener* eventListener)
{
	_RegisterAllEvents(eventListener);
}

/**
 *  @brief UnRegister event handler.
 */
void PlayerInstanceAAMP::UnRegisterEvents(EventListener* eventListener)
{
	_UnRegisterEvents(eventListener);
}

/**
 *  @brief Set retry limit on Segment injection failure.
 */
void PlayerInstanceAAMP::SetSegmentInjectFailCount(int value)
{
	SETCONFIGVALUE_PRIV(AAMP_APPLICATION_SETTING,eAAMPConfig_SegmentInjectThreshold,value);
}

/**
 *  @brief Set retry limit on Segment drm decryption failure.
 */
void PlayerInstanceAAMP::SetSegmentDecryptFailCount(int value)
{
	SETCONFIGVALUE_PRIV(AAMP_APPLICATION_SETTING,eAAMPConfig_DRMDecryptThreshold,value);
}

/**
 *  @brief Set initial buffer duration in seconds
 */
void PlayerInstanceAAMP::SetInitialBufferDuration(int durationSec)
{
	SETCONFIGVALUE_PRIV(AAMP_APPLICATION_SETTING,eAAMPConfig_InitialBuffer,durationSec);
}

/**
 *  @brief Get initial buffer duration in seconds
 */
int PlayerInstanceAAMP::GetInitialBufferDuration(void)
{
	return GETCONFIGVALUE_PRIV(eAAMPConfig_InitialBuffer);
}

/**
 *  @brief Set Maximum Cache Size for storing playlist
 */
void PlayerInstanceAAMP::SetMaxPlaylistCacheSize(int cacheSize)
{
	SETCONFIGVALUE_PRIV(AAMP_APPLICATION_SETTING,eAAMPConfig_MaxPlaylistCacheSize,cacheSize);
}

/**
 *  @brief Set profile ramp down limit.
 */
void PlayerInstanceAAMP::SetRampDownLimit(int limit)
{
	SETCONFIGVALUE_PRIV(AAMP_APPLICATION_SETTING,eAAMPConfig_RampDownLimit,limit);
}

/**
 *  @brief Get profile ramp down limit.
 */
int PlayerInstanceAAMP::GetRampDownLimit(void)
{
	return GETCONFIGVALUE_PRIV(eAAMPConfig_RampDownLimit);
}

/**
 *  @brief Set Language preferred Format
 */
void PlayerInstanceAAMP::SetLanguageFormat(LangCodePreference preferredFormat, bool useRole)
{
	UsingPlayerId playerId(mPlayerId);
	SETCONFIGVALUE_PRIV(AAMP_APPLICATION_SETTING,eAAMPConfig_LanguageCodePreference,(int)preferredFormat);
	if( useRole )
	{
		AAMPLOG_WARN("SetLanguageFormat bDescriptiveAudioTrack deprecated!" );
	}
	//gpGlobalConfig->bDescriptiveAudioTrack = useRole;
}

/**
 *  @brief Set minimum bitrate value.
 */
void PlayerInstanceAAMP::SetMinimumBitrate(BitsPerSecond bitrate)
{
	UsingPlayerId playerId(mPlayerId);
	if (bitrate > 0)
	{
		AAMPLOG_INFO("Setting minimum bitrate: %" BITSPERSECOND_FORMAT, bitrate);
		SETCONFIGVALUE_PRIV(AAMP_APPLICATION_SETTING,eAAMPConfig_MinBitrate,(int)bitrate);
	}
	else
	{
		AAMPLOG_WARN("Invalid bitrate value %" BITSPERSECOND_FORMAT,  bitrate);
	}

}

/**
 *  @brief Get minimum bitrate value.
 */
BitsPerSecond PlayerInstanceAAMP::GetMinimumBitrate(void)
{
	return GETCONFIGVALUE_PRIV(eAAMPConfig_MinBitrate);
}

/**
 *  @brief Set maximum bitrate value.
 */
void PlayerInstanceAAMP::SetMaximumBitrate(BitsPerSecond bitrate)
{
	UsingPlayerId playerId(mPlayerId);
	if (bitrate > 0)
	{
		AAMPLOG_INFO("Setting maximum bitrate : %" BITSPERSECOND_FORMAT, bitrate);
		SETCONFIGVALUE_PRIV(AAMP_APPLICATION_SETTING,eAAMPConfig_MaxBitrate,(int)bitrate);
	}
	else
	{
		AAMPLOG_WARN("Invalid bitrate value %" BITSPERSECOND_FORMAT, bitrate);
	}
}

/**
 *  @brief Get maximum bitrate value.
 */
BitsPerSecond PlayerInstanceAAMP::GetMaximumBitrate(void)
{
	UsingPlayerId playerId(mPlayerId);
	return GETCONFIGVALUE_PRIV(eAAMPConfig_MaxBitrate);
}

/**
 *  @brief Set playback rate.
 */
void PlayerInstanceAAMP::SetRate(float rate,int overshootcorrection)
{
    UsingPlayerId playerId(mPlayerId);
    AAMPLOG_INFO("PLAYER[%d] rate=%f.", mPlayerId, rate);
    if(mAsyncTuneEnabled)
    {
        mScheduler.ScheduleTask(AsyncTaskObj([rate,overshootcorrection](void *data)
                                             {
            PlayerInstanceAAMP *instance = static_cast<PlayerInstanceAAMP *>(data);
            instance->SetRateInternal(rate,overshootcorrection);
        }, (void *) this,__FUNCTION__));
    }
    else
    {
        SetRateInternal(rate,overshootcorrection);
    }
}

/**
 *  @brief Set userAgent.
 */
bool PlayerInstanceAAMP::SetUserAgent(std::string &userAgent)
{
	UsingPlayerId playerId(mPlayerId);
	bool ret = false;
	if(!userAgent.empty())
	{
		std::string userAgentString = userAgent + AAMP_USERAGENT_SUFFIX;
		AAMPLOG_INFO("Setting userAgent : %s ",userAgentString.c_str());
		SETCONFIGVALUE_PRIV(AAMP_APPLICATION_SETTING,eAAMPConfig_UserAgent,userAgentString);
		ret = true;
	}
	return ret;
}

/**
 *  @brief Set playback speed.
 */
void PlayerInstanceAAMP::SetPlaybackSpeed (float speed)
{
    UsingPlayerId playerId(mPlayerId);
    AAMPLOG_INFO("PLAYER[%d] Change playback speed = %f", mPlayerId, speed);
    StreamSink *sink = AampStreamSinkManager::GetInstance().GetStreamSink(this);
    if (sink  && false == sink->SetPlayBackRate(speed))
    {
        AAMPLOG_WARN("PLAYER[%d] Change playback speed failed = %f", mPlayerId, speed);
    }
}

/**
 *  @brief Set playback rate - Internal function
 */
void PlayerInstanceAAMP::SetRateInternal(float rate,int overshootcorrection)
{
    AAMPLOG_INFO("PLAYER[%d] rate=%f.", mPlayerId, rate);
    AAMPPlayerState state = GetState();
    
    if (state == eSTATE_ERROR)
    {
        AAMPLOG_WARN("operation is not allowed when player in eSTATE_ERROR state !");
        return;
    }
    
    //convert the incoming rates into acceptable rates
    if(ISCONFIGSET_PRIV(eAAMPConfig_RepairIframes))
    {
        AAMPLOG_WARN("mRepairIframes is true, setting actual rate %f for the received rate %f", getWorkingTrickplayRate(rate), rate);
        rate = getWorkingTrickplayRate(rate);
    }
    
    _StopPausePositionMonitoring("SetRate() called");
    
    if (mpStreamAbstractionAAMP && !(mbUsingExternalPlayer))
    {
        bool playAlreadyEnabled = mbPlayEnabled;
        if ( AAMP_SLOWMOTION_RATE != rate && !mIsIframeTrackPresent && rate != AAMP_NORMAL_PLAY_RATE && rate != 0 && mMediaFormat != eMEDIAFORMAT_PROGRESSIVE)
        {
            AAMPLOG_WARN("Ignoring trickplay. No iframe tracks in stream");
            _NotifySpeedChanged(AAMP_NORMAL_PLAY_RATE); // Send speed change event to XRE to reset the speed to normal play since the trickplay ignored at player level.
            return;
        }
        
        // Special case where playback has not started due to autoplay being false and
        // first rate is paused, set to pause with first frame shown
        if ((AAMP_RATE_PAUSE == rate) && pipeline_paused && !mbPlayEnabled && !mbDetached)
        {
            rate = AAMP_NORMAL_PLAY_RATE;
            _SetPauseOnStartPlayback(true);
        }
        else
        {
            _SetPauseOnStartPlayback(false);
        }
        
        if(!(mbPlayEnabled) && pipeline_paused && (AAMP_RATE_PAUSE != rate) && (mbSeeked || !mbDetached))
        {
            AAMPLOG_WARN("PLAYER[%d] Player %s=>%s.", mPlayerId, STRBGPLAYER, STRFGPLAYER );
            mbPlayEnabled = true;
            if (AAMP_NORMAL_PLAY_RATE == rate)
            {
                _ActivatePlayer();
                _LogPlayerPreBuffered();
                StreamSink *sink = AampStreamSinkManager::GetInstance().GetStreamSink(this);
                if (sink)
                {
                    sink->Configure(mVideoFormat, mAudioFormat, mAuxFormat, mSubtitleFormat, mpStreamAbstractionAAMP->GetESChangeStatus(), mpStreamAbstractionAAMP->GetAudioFwdToAuxStatus());
                    _ResumeDownloads(); //To make sure that the playback resumes after a player switch if player was in paused state before being at background
                    mpStreamAbstractionAAMP->StartInjection();
                    sink->Stream();
                }
                pipeline_paused = false;
                mbSeeked = false;
                return;
            }
            else if(AAMP_RATE_PAUSE != rate)
            {
                AAMPLOG_INFO("Player switched at trickplay %f", rate);
                playerStartedWithTrickPlay = true; //to be used to show at least one frame
            }
        }
        bool retValue = true;
        if ( AAMP_SLOWMOTION_RATE != rate && rate > 0 && _IsLive() && mpStreamAbstractionAAMP->IsStreamerAtLivePoint() && rate >= AAMP_NORMAL_PLAY_RATE && !mbDetached)
        {
            AAMPLOG_WARN("Already at logical live point, hence skipping operation");
            _NotifyOnEnteringLive();
            return;
        }
        
        // If input rate is same as current playback rate, skip duplicate operation
        // Additional check for pipeline_paused is because of 0(PAUSED) -> 1(PLAYING), where rate == 1.0 in PAUSED state
        if ((!pipeline_paused && rate == rate && !_GetPauseOnFirstVideoFrameDisp()) || (rate == 0 && pipeline_paused))
        {
            AAMPLOG_WARN("Already running at playback rate(%f) pipeline_paused(%d), hence skipping set rate for (%f)", rate, pipeline_paused, rate);
            return;
        }
        
        //-- Get the trick play to a closer position
        //Logic adapted
        // XRE gives fixed overshoot position , not suited for aamp . So ignoring overshoot correction value
        // instead use last reported posn vs the time player get play command
        // a. During trickplay , last XRE reported position is mNewSeekInfo.getInfo().Position()
        /// and last reported time is mNewSeekInfo.getInfo().UpdateTime()
        // b. Calculate the time delta	from last reported time
        // c. Using this diff , calculate the best/nearest match position (works out 70-80%)
        // d. If time delta is < 100ms ,still last video fragment rendering is not removed ,but position updated very recently
        // So switch last displayed position - NewPosn -= Posn - ((rate/4)*1000)
        // e. If time delta is > 950ms , possibility of next frame to come by the time play event is processed.
        //So go to next fragment which might get displayed
        // f. If none of above ,maintain the last displayed position .
        //
        // h. TODO (again trial n error) - for 3x/4x , within 1sec there might multiple frame displayed . Can use timedelta to calculate some more near,to be tried
        const auto SeekInfo = mNewSeekInfo.GetInfo();
        
        const int  timeDeltaFromProgReport = SeekInfo.getTimeSinceUpdateMs();
        
        //Skip this logic for either going to paused to coming out of paused scenarios with HLS
        //What we would like to avoid here is the update of seek_pos_seconds because gstreamer position will report proper position
        //Check for 1.0 -> 0.0 and 0.0 -> 1.0 usecase and avoid below logic
        if (!((rate == AAMP_NORMAL_PLAY_RATE && rate == 0) || (pipeline_paused && rate == AAMP_NORMAL_PLAY_RATE)))
        {
            // when switching from trick to play mode only
            // only do this when overshootcorrection is specified by the application
            if ((overshootcorrection > 0) &&
                (rate && ( AAMP_SLOWMOTION_RATE == rate || rate == AAMP_NORMAL_PLAY_RATE) && !pipeline_paused))
            {
                const auto seek_pos_seconds_copy = seek_pos_seconds;	//ensure the same value of seek_pos_seconds used in the check is logged
                if(!SeekInfo.isPositionValid(seek_pos_seconds_copy))
                {
                    AAMPLOG_WARN("Cached seek position (%f) is invalid. seek_pos_seconds = %f, seek_pos_seconds @ last report = %f.",SeekInfo.getPosition(), seek_pos_seconds_copy, SeekInfo.getSeekPositionSec());
                }
                else
                {
                    double newSeekPosInSec = -1;
                    if (ISCONFIGSET_PRIV(eAAMPConfig_EnableGstPositionQuery))
                    {
                        // Get the last frame position when resume from the trick play.
                        newSeekPosInSec = (SeekInfo.getPosition()/1000);
                    }
                    else
                    {
                        if(timeDeltaFromProgReport > 950) // diff > 950 mSec
                        {
                            // increment by 1x trickplay frame , next possible displayed frame
                            newSeekPosInSec = (SeekInfo.getPosition()+(rate*1000))/1000;
                        }
                        else if(timeDeltaFromProgReport > 100) // diff > 100 mSec
                        {
                            // Get the last shown frame itself
                            newSeekPosInSec = SeekInfo.getPosition()/1000;
                        }
                        else
                        {
                            // Go little back to last shown frame
                            newSeekPosInSec = (SeekInfo.getPosition()-(rate*1000))/1000;
                        }
                    }
                    
                    if (newSeekPosInSec >= 0)
                    {
                        /* Note circular calculation:
                         * newSeekPosInSec is based on mNewSeekInfo
                         * mNewSeekInfo's position value is based on PlayerInstanceAAMP::GetPositionMilliseconds()
                         * PlayerInstanceAAMP::GetPositionMilliseconds() uses seek_pos_seconds
                         */
                        seek_pos_seconds = newSeekPosInSec;
                    }
                    else
                    {
                        AAMPLOG_WARN("new seek_pos_seconds calculated is invalid(%f), discarding it!", newSeekPosInSec);
                    }
                }
            }
            else
            {
                // Coming out of pause mode(rate=0) or when going into pause mode (rate=0)
                // Show the last position
                seek_pos_seconds = _GetPositionSeconds();
            }
            
            trickStartUTCMS = -1;
        }
        else
        {
            // For 1.0->0.0 and 0.0->1.0 if eAAMPConfig_EnableGstPositionQuery is enabled, GStreamer position query will give proper value
            // Fallback case added for when eAAMPConfig_EnableGstPositionQuery is disabled, since we will be using elapsedTime to calculate position and
            // trickStartUTCMS has to be reset
            if (!ISCONFIGSET_PRIV(eAAMPConfig_EnableGstPositionQuery) && !mbDetached)
            {
                seek_pos_seconds = _GetPositionSeconds();
                trickStartUTCMS = -1;
            }
        }
        
        if( AAMP_SLOWMOTION_RATE == rate )
        {
            /* Handling of fwd slowmotion playback */
            SetSlowMotionPlayRate(rate);
            _NotifySpeedChanged(rate, false);
            return;
        }
        // Adjusting the play/pause position value
        double offset = _GetFormatPositionOffsetInMSecs();
        double formattedCurrPos = _GetPositionMilliseconds() - offset;
        double formattedSeekPos = (seek_pos_seconds * 1000.0) - offset;
        
        AAMPLOG_WARN("aamp_SetRate (%f)overshoot(%d) ProgressReportDelta:(%d) ", rate, overshootcorrection, timeDeltaFromProgReport);
        AAMPLOG_WARN("aamp_SetRate rate(%f)->(%f) cur pipeline: %s. Adj position: %f Play/Pause Position:%lld",
                     rate, rate,pipeline_paused ? "paused" : "playing", formattedSeekPos, (static_cast<long long int>(formattedCurrPos)));
        
        if (!mSeekFromPausedState && (rate == rate) && !mbDetached)
        { // no change in desired play rate
            // no deferring for playback resume
            if (pipeline_paused && rate != 0)
            {
                AAMPLOG_INFO("Resuming Playback at Position '%lld'.", _GetPositionMilliseconds());
                // Resuming payback from pause
                // If have local TSB, but playing from Live then seek into the TSB
                // Otherwise unpause the pipeline
                if(_IsLocalAAMPTsb() && !_IsLocalAAMPTsbInjection())
                {
                    retValue = false;
                    _SetState(eSTATE_SEEKING);
                    seek_pos_seconds = _GetPositionSeconds();
                    rate = AAMP_NORMAL_PLAY_RATE;
                    pipeline_paused = false;
                    _AcquireStreamLock();
                    _TuneHelper(eTUNETYPE_SEEK, false);
                    _ReleaseStreamLock();
                }
                else
                {
                    // check if unpausing in the middle of fragments caching
                    if(!_SetStateBufferingIfRequired())
                    {
                        mpStreamAbstractionAAMP->NotifyPlaybackPaused(false);
                        StreamSink *sink = AampStreamSinkManager::GetInstance().GetStreamSink(this);
                        if (sink)
                        {
                            retValue = sink->Pause(false, false);
                        }
                        // required since buffers are already cached in paused state
                        _NotifyFirstBufferProcessed(sink ? sink->GetVideoRectangle() : std::string());
                    }
                }
                pipeline_paused = false;
                _ResumeDownloads();
            }
        }
        else if (rate == 0)
        {
            if (!pipeline_paused)
            {
                mpStreamAbstractionAAMP->NotifyPlaybackPaused(true);
                if (!_IsLocalAAMPTsb())
                {
                    _StopDownloads();
                }
                
                StreamSink *sink = AampStreamSinkManager::GetInstance().GetStreamSink(this);
                if (sink)
                {
                    retValue = sink->Pause(true, false);
                }
                pipeline_paused = true;
                
                if(_GetLLDashServiceData()->lowLatencyMode)
                {
                    // PAUSED to PLAY without tune, LLD rate correction is disabled to keep position
                    AAMPLOG_INFO("LL-Dash speed correction disabled after Pause");
                    _SetLLDashAdjustSpeed(false);
                }
                AAMPLOG_INFO("StreamAbstractionAAMP_MPD: Live latency correction is disabled due to the Pause operation!!");
                mDisableRateCorrection = true;
            }
        }
        else
        {
            //Enable playback if setRate call after detach
            if(mbDetached){
                mbPlayEnabled = true;
            }
            
            _ActivatePlayer();
            _LogPlayerPreBuffered();
            if (AAMP_NORMAL_PLAY_RATE != rate)
            {
                /** Rate is not in normal play so expect to clear the cache and redownload the
                 * iframe fragments; So clear the fragments downloaded (buffered data) time **/
                _ResetProfileCache();
            }
            
            TuneType tuneTypePlay = eTUNETYPE_SEEK;
            if(mJumpToLiveFromPause)
            {
                tuneTypePlay = eTUNETYPE_SEEKTOLIVE;
                mJumpToLiveFromPause = false;
            }
            /* if Gstreamer pipeline set to paused state by user, change it to playing state */
            if (playAlreadyEnabled && pipeline_paused == true)
            {
                AAMPLOG_INFO("Play was already enabled, and pipeline paused - unpause");
                StreamSink *sink = AampStreamSinkManager::GetInstance().GetStreamSink(this);
                if (sink)
                {
                    (void)sink->Pause(false, false);
                }
            }
            else
            {
                AAMPLOG_INFO("Play was not already enabled(%d) or pipeline not paused(%d)", playAlreadyEnabled, pipeline_paused);
            }
            rate = rate;
            pipeline_paused = false;
            mSeekFromPausedState = false;
            /* Clear setting playerrate flag */
            mSetPlayerRateAfterFirstframe=false;
            _CalculateTrickModePositionEOS();
            _EnableDownloads();
            _ResumeDownloads();
            _AcquireStreamLock();
            _TuneHelper(tuneTypePlay); // this unpauses pipeline as side effect
            _ReleaseStreamLock();
        }
        
        if(retValue)
        {
            // Do not update state if fragments caching is ongoing and pipeline not paused,
            // target state will be updated once caching completed
            _NotifySpeedChanged(pipeline_paused ? 0 : rate,
                                (!_IsFragmentCachingRequired() || pipeline_paused));
        }
    }
    else
    {
        AAMPLOG_WARN("aamp_SetRate rate[%f] - mpStreamAbstractionAAMP[%p] state[%d]", rate, mpStreamAbstractionAAMP, state);
    }
}

/**
 *  @brief Set PauseAt position.
 */
void PlayerInstanceAAMP::PauseAt(double position)
{
    if( GetState() != eSTATE_ERROR )
    {
        UsingPlayerId playerId(mPlayerId);
        if(mAsyncTuneEnabled)
        {
            (void)mScheduler.ScheduleTask(AsyncTaskObj([position](void *data)
                                                       {
                PlayerInstanceAAMP *instance = static_cast<PlayerInstanceAAMP *>(data);
                instance->PauseAtInternal(position);
            }, (void *) this,__FUNCTION__));
        }
        else
        {
            PauseAtInternal(position);
        }
    }
}

/**
 *  @brief Set PauseAt position - Internal function
 */
void PlayerInstanceAAMP::PauseAtInternal(double position)
{
    AAMPLOG_WARN("PLAYER[%d] aamp_PauseAt position=%f", mPlayerId, position);
    _StopPausePositionMonitoring("PauseAt() called");
    
    if (position >= 0)
    {
        if (!pipeline_paused)
        {
            _StartPausePositionMonitoring(static_cast<long long>(position * 1000));
        }
        else
        {
            AAMPLOG_WARN("PauseAt called when already paused");
        }
    }
}

static gboolean SeekAfterPrepared(gpointer ptr)
{
	PlayerInstanceAAMP* aamp = (PlayerInstanceAAMP*) ptr;
	bool sentSpeedChangedEv = false;
	bool isSeekToLiveOrEnd = false;
	TuneType tuneType = eTUNETYPE_SEEK;
	AAMPPlayerState state = aamp->_GetState();
	if( state == eSTATE_ERROR)
	{
		AAMPLOG_WARN("operation is not allowed when player in eSTATE_ERROR state !");\
		return false;
	}
	if (AAMP_SEEK_TO_LIVE_POSITION == aamp->seek_pos_seconds )
	{
		isSeekToLiveOrEnd = true;
	}

	AAMPLOG_WARN("aamp_Seek(%f) and seekToLiveOrEnd(%d)", aamp->seek_pos_seconds, isSeekToLiveOrEnd);

	if (isSeekToLiveOrEnd)
	{
		if (aamp->_IsLive())
		{
			tuneType = eTUNETYPE_SEEKTOLIVE;
		}
		else
		{
			tuneType = eTUNETYPE_SEEKTOEND;
		}
	}

	if (aamp->_IsLive() && aamp->mpStreamAbstractionAAMP && aamp->mpStreamAbstractionAAMP->IsStreamerAtLivePoint(aamp->seek_pos_seconds))
	{
		double currPositionSecs = aamp->_GetPositionSeconds();
		if ((tuneType == eTUNETYPE_SEEKTOLIVE) || (aamp->seek_pos_seconds >= currPositionSecs))
		{
			AAMPLOG_WARN("Already at live point, skipping operation since requested position(%f) >= currPosition(%f) or seekToLive(%d)", aamp->seek_pos_seconds, currPositionSecs, isSeekToLiveOrEnd);
            aamp->_NotifyOnEnteringLive();
			return false;
		}
	}

	if ((aamp->mbPlayEnabled) && aamp->pipeline_paused)
	{
		// resume downloads and clear paused flag for foreground instance. state change will be done
		// on streamSink configuration.
		AAMPLOG_WARN("paused state, so resume downloads");
        aamp->pipeline_paused = false;
        aamp->_ResumeDownloads();
		sentSpeedChangedEv = true;
	}

	if (tuneType == eTUNETYPE_SEEK)
	{
		AAMPLOG_WARN("tune type is SEEK");
	}
	if (aamp->rate != AAMP_NORMAL_PLAY_RATE)
	{
        aamp->rate = AAMP_NORMAL_PLAY_RATE;
		sentSpeedChangedEv = true;
	}
	if (aamp->mpStreamAbstractionAAMP)
	{ // for seek while streaming

		 /* PositionMillisecondLock is intended to ensure both state and seek_pos_seconds (in TuneHelper)
		 * are updated before GetPositionMilliseconds() can be used*/
		auto PositionMillisecondLocked = aamp->_LockGetPositionMilliseconds();
        aamp->_SetState(eSTATE_SEEKING);
		/* Clear setting playerrate flag */
        aamp->mSetPlayerRateAfterFirstframe=false;
        aamp->_AcquireStreamLock();
        aamp->_TuneHelper(tuneType);
		if(PositionMillisecondLocked)
		{
            aamp->_UnlockGetPositionMilliseconds();
		}
        aamp->_ReleaseStreamLock();
		if (sentSpeedChangedEv)
		{
            aamp->_NotifySpeedChanged(aamp->rate, false);
		}
	}
	return false;  // G_SOURCE_REMOVE = false , G_SOURCE_CONTINUE = true
}


/**
 *  @brief Seek to a time.
 */
void PlayerInstanceAAMP::Seek(double secondsRelativeToTuneTime, bool keepPaused)
{
    UsingPlayerId playerId(mPlayerId);
    AAMPPlayerState state = _GetState();
    if(mAsyncTuneEnabled && state != eSTATE_IDLE && state != eSTATE_RELEASED)
    {
        mScheduler.ScheduleTask(AsyncTaskObj([secondsRelativeToTuneTime,keepPaused](void *data)
                                             {
            PlayerInstanceAAMP *instance = static_cast<PlayerInstanceAAMP *>(data);
            instance->SeekInternal(secondsRelativeToTuneTime,keepPaused);
        }, (void *) this,__FUNCTION__));
    }
    else
    {
        SeekInternal(secondsRelativeToTuneTime,keepPaused);
    }
}


/**
 *  @brief Seek to a time - Internal function
 */
void PlayerInstanceAAMP::SeekInternal(double secondsRelativeToTuneTime, bool keepPaused)
{
    bool sentSpeedChangedEv = false;
    bool isSeekToLiveOrEnd = false;
    TuneType tuneType = eTUNETYPE_SEEK;
    AAMPPlayerState state = GetState();
    _StopPausePositionMonitoring("Seek() called");
    
    if ((mMediaFormat == eMEDIAFORMAT_HLS || mMediaFormat == eMEDIAFORMAT_HLS_MP4) && (eSTATE_INITIALIZING == state)  && mpStreamAbstractionAAMP)
    {
        AAMPLOG_WARN("aamp_Seek(%f) at the middle of tune, no fragments downloaded yet.state(%d), keep paused(%d)", secondsRelativeToTuneTime, state, keepPaused);
        mpStreamAbstractionAAMP->SeekPosUpdate(secondsRelativeToTuneTime);
        SETCONFIGVALUE_PRIV(AAMP_TUNE_SETTING,eAAMPConfig_PlaybackOffset,secondsRelativeToTuneTime);
    }
    else if (eSTATE_INITIALIZED == state || eSTATE_PREPARING == state)
    {
        AAMPLOG_WARN("aamp_Seek(%f) will be called after preparing the content.state(%d), keep paused(%d)", secondsRelativeToTuneTime, state, keepPaused);
        seek_pos_seconds = secondsRelativeToTuneTime ;
        SETCONFIGVALUE_PRIV(AAMP_TUNE_SETTING,eAAMPConfig_PlaybackOffset,secondsRelativeToTuneTime);
        g_idle_add(SeekAfterPrepared, (gpointer)this);
    }
    else
    {
        if (secondsRelativeToTuneTime == AAMP_SEEK_TO_LIVE_POSITION)
        {
            isSeekToLiveOrEnd = true;
        }
        //This is workaround for partner app that is sometimes passing negative value for seek position,
        //when trying to seek to beginning of VOD content. Default aamp behavior has been to treat seek(-1) as a seek to live.
        //We have an explicit seek to live api that should be instead used.
        
        if(!_IsLive() && mMediaFormat != eMEDIAFORMAT_DASH && secondsRelativeToTuneTime < 0)
        {
            AAMPLOG_WARN("The seek value set to 0 because the seek value is negative");
            isSeekToLiveOrEnd = false;
            secondsRelativeToTuneTime = 0;
        }
        
        AAMPLOG_WARN("aamp_Seek(%f) and seekToLiveOrEnd(%d) state(%d), keep paused(%d)", secondsRelativeToTuneTime, isSeekToLiveOrEnd,state, keepPaused);
        
        if (isSeekToLiveOrEnd)
        {
            if (_IsLive())
            {
                tuneType = eTUNETYPE_SEEKTOLIVE;
            }
            else
            {
                // Rewind over AD using Seek(-1) is implemented only for DASH, so restoring old code for non DASH.
                if (mMediaFormat == eMEDIAFORMAT_DASH)
                {
                    tuneType = eTUNETYPE_SEEKTOEND;
                }
                else
                {
                    AAMPLOG_WARN("Not live, skipping seekToLive for MediaFormat %d", mMediaFormat);
                    return;
                }
            }
        }
        
        if(ISCONFIGSET_PRIV(eAAMPConfig_UseAbsoluteTimeline) &&
           (mProgressReportOffset > 0) &&
           (eABSOLUTE_PROGRESS_WITHOUT_AVAILABILITY_START == GETCONFIGVALUE_PRIV(eAAMPConfig_PreferredAbsoluteProgressReporting)) &&
           !isSeekToLiveOrEnd)
        {
            // Absolute timeline, but Preferred reporting is from availabilityStartTime
            // Culled seconds (tsbStart) is in epoch, so convert secondsRelativeToTuneTime to epoch number
            secondsRelativeToTuneTime += mProgressReportAvailabilityOffset;
            AAMPLOG_WARN("aamp_Seek position adjusted to absolute value: %lf", secondsRelativeToTuneTime);
        }
        else if ((!ISCONFIGSET_PRIV(eAAMPConfig_UseAbsoluteTimeline) || !_IsLiveStream()) && mProgressReportOffset > 0)
        {
            // Relative reporting
            // Convert to epoch using offset for all VOD contents and live with relative positions
            secondsRelativeToTuneTime += mProgressReportOffset;
            AAMPLOG_WARN("aamp_Seek position adjusted to absolute value: %lf", secondsRelativeToTuneTime);
        }
        
        if(_IsLive() && mpStreamAbstractionAAMP)
        {
            //skip seektolive if already at livepoint and latency is within acceptable range
            //avoids hangup if user presses seektolive multiple times in quick succession
            if ((tuneType == eTUNETYPE_SEEKTOLIVE) && mpStreamAbstractionAAMP->mIsAtLivePoint && _IsLocalAAMPTsb())
            {
                double endPos = culledSeconds+durationSeconds;			//calculate end position
                double currentLatency=endPos-_GetPositionSeconds();					//calculate latency
                if(std::floor(currentLatency)<=GETCONFIGVALUE_PRIV(eAAMPConfig_LLMaxLatency))	//if floored latency value is within acceptable range skip seektolive
                {
                    AAMPLOG_WARN("Skipping SeektoLive as already at livepoint and latency(%f)!!",currentLatency);
                    _NotifyOnEnteringLive();
                    return;		//skip seektolive
                }
                else		//live latency is greater thus continue seektolive
                {
                    AAMPLOG_WARN("SeektoLive as latency(%f) !!",currentLatency);
                }
            }
        }
        
        if (_IsLive() && mpStreamAbstractionAAMP && mpStreamAbstractionAAMP->IsStreamerAtLivePoint(secondsRelativeToTuneTime))
        {
            double currPositionSecs = _GetPositionSeconds();
            
            if ((tuneType == eTUNETYPE_SEEKTOLIVE) || secondsRelativeToTuneTime >= currPositionSecs)
            {
                AAMPLOG_WARN("Already at live point, skipping operation since requested position(%f) >= currPosition(%f) or seekToLive(%d)", secondsRelativeToTuneTime, currPositionSecs, isSeekToLiveOrEnd);
                _NotifyOnEnteringLive();
                return;
            }
        }
        
        bool seekWhilePause = false;
        // For autoplay false, pipeline_paused will be true, which denotes a non-playing state
        // as the GST pipeline is not yet created, avoid setting pipeline_paused to false here
        // which might mess up future SetRate call for BG->FG
        if (mbPlayEnabled && pipeline_paused)
        {
            
            if(keepPaused && mMediaFormat != eMEDIAFORMAT_PROGRESSIVE)
            {
                // Enable seek while paused if not Progressive stream
                seekWhilePause = true;
            }
            
            // Clear paused flag. state change will be done
            // on streamSink configuration.
            if (!seekWhilePause)
            {
                AAMPLOG_WARN("Clearing paused flag");
                pipeline_paused = false;
                sentSpeedChangedEv = true;
            }
            // Resume downloads
            AAMPLOG_INFO("Resuming downloads");
            _ResumeDownloads();
        }
        
        // Add additional checks for BG playerInstance
        // If player is in background and only been in PREPARED state
        // and a seek is attempted to the same position it started, then ignore the seek
        if (!mbPlayEnabled && tuneType == eTUNETYPE_SEEK && state == eSTATE_PREPARED &&
            (_GetPositionSeconds() == secondsRelativeToTuneTime))
        {
            AAMPLOG_WARN("Ignoring seek to same position as start position(%lf) for BG player", _GetPositionSeconds());
            return;
        }
        /*
         * PositionMillisecondLock is intended to ensure both state and seek_pos_seconds
         * are updated before GetPositionMilliseconds() can be used*/
        auto PositionMillisecondLocked = _LockGetPositionMilliseconds();
        
        if (tuneType == eTUNETYPE_SEEK)
        {
            SETCONFIGVALUE_PRIV(AAMP_TUNE_SETTING,eAAMPConfig_PlaybackOffset,secondsRelativeToTuneTime);
            seek_pos_seconds = secondsRelativeToTuneTime;
        }
        else if (tuneType == eTUNETYPE_SEEKTOEND)
        {
            SETCONFIGVALUE_PRIV(AAMP_TUNE_SETTING,eAAMPConfig_PlaybackOffset,-1);
            seek_pos_seconds = -1;
        }
        
        if (rate != AAMP_NORMAL_PLAY_RATE)
        {
            rate = AAMP_NORMAL_PLAY_RATE;
            sentSpeedChangedEv = true;
        }
        
        /**Set the flag true to indicate seeked **/
        mbSeeked = true;
        
        if (mpStreamAbstractionAAMP)
        { // for seek while streaming
            _SetState(eSTATE_SEEKING);
            if(PositionMillisecondLocked)
            {
                _UnlockGetPositionMilliseconds();
            }
            /* Clear setting playerrate flag */
            mSetPlayerRateAfterFirstframe=false;
            _AcquireStreamLock();
            _TuneHelper(tuneType, seekWhilePause);
            _ReleaseStreamLock();
            if (sentSpeedChangedEv && (!seekWhilePause) )
            {
                _NotifySpeedChanged(rate, false);
            }
        }
        else if(PositionMillisecondLocked)
        {
            _UnlockGetPositionMilliseconds();
        }
        if (mbPlayEnabled)
        {
            // Clear seeked flag for FG instance after SEEK
            mbSeeked = false;
        }
    }
}

/**
 *  @brief Seek to live point.
 */
void PlayerInstanceAAMP::SeekToLive(bool keepPaused)
{
    UsingPlayerId playerId(mPlayerId);
    if(mAsyncTuneEnabled)
    {
        
        mScheduler.ScheduleTask(AsyncTaskObj([keepPaused](void *data)
                                             {
            PlayerInstanceAAMP *instance = static_cast<PlayerInstanceAAMP *>(data);
            instance->SeekInternal(AAMP_SEEK_TO_LIVE_POSITION, keepPaused);
        }, (void *) this,__FUNCTION__));
    }
    else
    {
        SeekInternal(AAMP_SEEK_TO_LIVE_POSITION, keepPaused);
    }
}

/**
 *  @brief Set slow motion player speed.
 */
void PlayerInstanceAAMP::SetSlowMotionPlayRate( float rate )
{
    UsingPlayerId playerId(mPlayerId);
    AAMPPlayerState state = GetState();
    AAMPLOG_WARN("SetSlowMotionPlay(%f)", rate );
    
    if (mpStreamAbstractionAAMP)
    {
        if (mbPlayEnabled && pipeline_paused)
        {
            //Clear pause state flag & resume download
            pipeline_paused = false;
            _ResumeDownloads();
        }
        
        if(AAMP_SLOWMOTION_RATE == rate)
        {
            mSetPlayerRateAfterFirstframe=true;
            playerrate=rate;
        }
        AAMPLOG_WARN("SetSlowMotionPlay(%f) %lf", rate, seek_pos_seconds );
        _AcquireStreamLock();
        _TeardownStream(false);
        rate = AAMP_NORMAL_PLAY_RATE;
        _TuneHelper(eTUNETYPE_SEEK);
        _ReleaseStreamLock();
    }
    else
    {
        AAMPLOG_WARN("SetSlowMotionPlay rate[%f] - mpStreamAbstractionAAMP[%p] state[%d]", rate, mpStreamAbstractionAAMP, state);
    }
}

/**
 *  @brief Seek to a time and playback with a new rate.
 */
void PlayerInstanceAAMP::SetRateAndSeek(int rate, double secondsRelativeToTuneTime)
{
    UsingPlayerId playerId(mPlayerId);
    AAMPPlayerState state = GetState();
    TuneType tuneType = eTUNETYPE_SEEK;
    AAMPLOG_WARN("aamp_SetRateAndSeek(%d)(%f)", rate, secondsRelativeToTuneTime);
    
    //convert the incoming rates into acceptable rates
    if(ISCONFIGSET_PRIV(eAAMPConfig_RepairIframes))
    {
        AAMPLOG_WARN("mRepairIframes is true, setting actual rate %f for the received rate %d", getWorkingTrickplayRate(rate), rate);
        rate = getWorkingTrickplayRate(rate);
    }
    
    if (secondsRelativeToTuneTime == AAMP_SEEK_TO_LIVE_POSITION)
    {
        if (_IsLive())
        {
            tuneType = eTUNETYPE_SEEKTOLIVE;
        }
        else
        {
            tuneType = eTUNETYPE_SEEKTOEND;
        }
    }
    
    if (mpStreamAbstractionAAMP)
    {
        if ((!mIsIframeTrackPresent && rate != AAMP_NORMAL_PLAY_RATE && rate != 0))
        {
            AAMPLOG_WARN("Ignoring trickplay. No iframe tracks in stream");
            _NotifySpeedChanged(AAMP_NORMAL_PLAY_RATE); // Send speed change event to XRE to reset the speed to normal play since the trickplay ignored at player level.
            return;
        }
        /* Clear setting playerrate flag */
        mSetPlayerRateAfterFirstframe=false;
        _AcquireStreamLock();
        _TeardownStream(false);
        seek_pos_seconds = secondsRelativeToTuneTime;
        _TuneHelper(tuneType);
        _ReleaseStreamLock();
        if(rate == 0)
        {
            if (!pipeline_paused)
            {
                AAMPLOG_WARN("Pausing Playback at Position '%lld'.", _GetPositionMilliseconds());
                mpStreamAbstractionAAMP->NotifyPlaybackPaused(true);
                _StopDownloads();
                StreamSink *sink = AampStreamSinkManager::GetInstance().GetStreamSink(this);
                if (sink)
                {
                    (void)sink->Pause(true, false);
                }
                pipeline_paused = true;
            }
        }
    }
    else
    {
        AAMPLOG_WARN("aamp_SetRateAndSeek rate[%d] - mpStreamAbstractionAAMP[%p] state[%d]", rate, mpStreamAbstractionAAMP, state);
    }
}

/**
 *  @brief Set video rectangle.
 */
void PlayerInstanceAAMP::SetVideoRectangle(int x, int y, int w, int h)
{
    UsingPlayerId playerId(mPlayerId);
    _SetVideoRectangle(x, y, w, h);
}

/**
 *  @brief Set video zoom.
 */
void PlayerInstanceAAMP::SetVideoZoom(VideoZoomMode zoom)
{
    UsingPlayerId playerId(mPlayerId);
    zoom_mode = zoom;
    _AcquireStreamLock();
    if( mpStreamAbstractionAAMP )
    {
        _SetVideoZoom(zoom);
    }
    else
    {
        AAMPLOG_WARN("Player is in state (eSTATE_IDLE), value has been cached");
    }
    _ReleaseStreamLock();
}

/**
 *  @brief Enable/ Disable Video.
 */
void PlayerInstanceAAMP::SetVideoMute(bool muted)
{
    UsingPlayerId playerId(mPlayerId);
    AAMPLOG_WARN(" mute == %s subtitles_muted == %s", muted?"true":"false", subtitles_muted?"true":"false");
    video_muted = muted;
    
    //If lock could not be acquired, then cache it
    if(_TryStreamLock())
    {
        if (mpStreamAbstractionAAMP)
        {
            _SetVideoMute(muted); // hide/show video plane
            _CacheAndApplySubtitleMute(muted);
        }
        else
        {
            AAMPLOG_WARN("Player is in state eSTATE_IDLE, value has been cached");
            mApplyCachedVideoMute = true; // can't do it now, but remember that we want video muted
        }
        _ReleaseStreamLock();
    }
    else
    {
        AAMPLOG_WARN("StreamLock is not available, value has been cached");
        mApplyCachedVideoMute = true;
    }
}

/**
 *   @brief Enable/ Disable Subtitles.
 *
 *   @param  muted - true to disable subtitles, false to enable subtitles.
 */
void PlayerInstanceAAMP::SetSubtitleMute(bool muted)
{
    UsingPlayerId playerId(mPlayerId);
    AAMPLOG_WARN(" mute == %s", muted?"true":"false");
    subtitles_muted = muted;
    _AcquireStreamLock();
    if( mpStreamAbstractionAAMP )
    {
        _SetSubtitleMute(muted);
    }
    else
    {
        AAMPLOG_WARN("Player is in state eSTATE_IDLE, value has been cached");
    }
    _ReleaseStreamLock();
}

/**
 *   @brief Set Audio Volume.
 *
 *   @param  volume - Minimum 0, maximum 100.
 */
void PlayerInstanceAAMP::SetAudioVolume(int volume)
{
    UsingPlayerId playerId(mPlayerId);
    AAMPLOG_WARN(" volume == %d", volume);
    if (volume < AAMP_MINIMUM_AUDIO_LEVEL || volume > AAMP_MAXIMUM_AUDIO_LEVEL)
    {
        AAMPLOG_WARN("Audio level (%d) is outside the range supported.. discarding it..",
                     volume);
    }
    else
    {
        audio_volume = volume;
        if( mpStreamAbstractionAAMP )
        {
            _SetAudioVolume(volume);
        }
        else
        {
            AAMPLOG_WARN("Player is in state eSTATE_IDLE, value has been cached");
        }
    }
}

/**
 *  @brief Set Audio language.
 */
void PlayerInstanceAAMP::SetLanguage(const char* language)
{
    UsingPlayerId playerId(mPlayerId);
    AAMPPlayerState state = _GetState();
    if (mAsyncTuneEnabled && state != eSTATE_IDLE && state != eSTATE_RELEASED)
    {
        std::string sLanguage = std::string(language);
        mScheduler.ScheduleTask(AsyncTaskObj(
                                             [sLanguage](void *data)
                                             {
                                                 PlayerInstanceAAMP *instance = static_cast<PlayerInstanceAAMP *>(data);
                                                 instance->SetPreferredLanguages(sLanguage.c_str());
                                             }, (void *) this,__FUNCTION__));
    }
    else
    {
        SetPreferredLanguages(language);
    }
}

/**
 *  @brief Set array of subscribed tags.
 */
void PlayerInstanceAAMP::SetSubscribedTags(std::vector<std::string> subscribedTags)
{
    UsingPlayerId playerId(mPlayerId);
    subscribedTags = subscribedTags;
    
    for (int i=0; i < subscribedTags.size(); i++) {
        AAMPLOG_WARN("    subscribedTags[%d] = '%s'", i, subscribedTags.at(i).data());
    }
}

/**
 *  @brief Subscribe array of http response headers.
 */
void PlayerInstanceAAMP::SubscribeResponseHeaders(std::vector<std::string> responseHeaders)
{
    UsingPlayerId playerId(mPlayerId);
    manifestHeadersNeeded  = responseHeaders;
    
    for (int header=0; header < responseHeaders.size(); header++) {
        AAMPLOG_INFO("    responseHeaders[%d] = '%s'", header, responseHeaders.at(header).data());
    }
}

#ifdef SUPPORT_JS_EVENTS

/**
 *  @brief Load AAMP JS object in the specified JS context.
 */
void PlayerInstanceAAMP::LoadJS(void* context)
{
	AAMPLOG_WARN("[AAMP_JS] (%p)", context);
	if (mJSBinding_DL) {
		void(*loadJS)(void*, void*);
		const char* szLoadJS = "aamp_LoadJS";
		loadJS = (void(*)(void*, void*))dlsym(mJSBinding_DL, szLoadJS);
		if (loadJS) {
			AAMPLOG_WARN("[AAMP_JS]  dlsym(%p, \"%s\")=%p", mJSBinding_DL, szLoadJS, loadJS);
			loadJS(context, this);
		}
	}
}

/**
 *  @brief Unload AAMP JS object in the specified JS context.
 */
void PlayerInstanceAAMP::UnloadJS(void* context)
{
	AAMPLOG_WARN("[AAMP_JS] (%p)", context);
	if (mJSBinding_DL) {
		void(*unloadJS)(void*);
		const char* szUnloadJS = "aamp_UnloadJS";
		unloadJS = (void(*)(void*))dlsym(mJSBinding_DL, szUnloadJS);
		if (unloadJS) {
			AAMPLOG_WARN("[AAMP_JS] dlsym(%p, \"%s\")=%p", mJSBinding_DL, szUnloadJS, unloadJS);
			unloadJS(context);
		}
	}
}
#endif

/**
 *  @brief Support multiple listeners for multiple event type
 */
void PlayerInstanceAAMP::AddEventListener(AAMPEventType eventType, EventListener* eventListener)
{
    _AddEventListener(eventType, eventListener);
}

/**
 *  @brief Remove event listener for eventType.
 */
void PlayerInstanceAAMP::RemoveEventListener(AAMPEventType eventType, EventListener* eventListener)
{
_RemoveEventListener(eventType, eventListener);
}

/**
 *  @brief To check whether the asset is live or not.
 */
bool PlayerInstanceAAMP::IsLive()
{
	bool isLive = _IsLive();
	return isLive;
}

/**
 *  @brief Get jsinfo config value (default false)
 */
bool PlayerInstanceAAMP::IsJsInfoLoggingEnabled(void)
{
    return ISCONFIGSET_PRIV(eAAMPConfig_JsInfoLogging);
}

/**
 *  @brief Get current audio language.
 */
std::string PlayerInstanceAAMP::GetAudioLanguage(void)
{
    static char lang[MAX_LANGUAGE_TAG_LENGTH];
    lang[0] = 0;
    if( mpStreamAbstractionAAMP)
    {
        int trackIndex = GetAudioTrack();
        if( trackIndex>=0 )
        {
            std::vector<AudioTrackInfo> trackInfo = mpStreamAbstractionAAMP->GetAvailableAudioTracks();
            if (!trackInfo.empty())
            {
                strncpy(lang, trackInfo[trackIndex].language.c_str(), sizeof(lang));
                lang[sizeof(lang)-1] = '\0';  //CID:173324 - Buffer size warning
            }
        }
    }
    return lang;
}

const char * PlayerInstanceAAMP::GetCurrentAudioLanguage(void)
{
	static std::string temp = GetAudioLanguage();
	return temp.c_str();
}

/**
 *  @brief Get current drm
 */
std::string PlayerInstanceAAMP::GetDRM(void)
{
    std::string ret;
    DrmHelperPtr helper = _GetCurrentDRM();
    if (helper)
    {
        ret = helper->friendlyName();
    }
    else
    {
        ret = "NONE";
    }
    return ret;
}

/**
 *  @brief Applies the custom http headers for page (Injector bundle) received from the js layer
 */
void PlayerInstanceAAMP::AddPageHeaders(std::map<std::string, std::string> pageHeaders)
{
	if( ISCONFIGSET_PRIV(eAAMPConfig_AllowPageHeaders))
	{
		UsingPlayerId playerId(mPlayerId);
		for(auto &header : pageHeaders)
		{
			AAMPLOG_INFO("PlayerInstanceAAMP: applying the http header key: %s, value: %s", header.first.c_str(), header.second.c_str());
            _AddCustomHTTPHeader(header.first, std::vector<std::string>{header.second}, false);
		}
	}
}

/**
 *   @brief Add/Remove a custom HTTP header and value.
 */
void PlayerInstanceAAMP::AddCustomHTTPHeader(std::string headerName, std::vector<std::string> headerValue, bool isLicenseHeader)
{
    UsingPlayerId playerId(mPlayerId);
    _AddCustomHTTPHeader(headerName, headerValue, isLicenseHeader);
}

/**
 *  @brief Set License Server URL.
 */
void PlayerInstanceAAMP::SetLicenseServerURL(const char *url, DRMSystems type)
{
    UsingPlayerId playerId(mPlayerId);
    switch( type )
    {
        case eDRM_PlayReady:
            SETCONFIGVALUE_PRIV(AAMP_APPLICATION_SETTING,eAAMPConfig_PRLicenseServerUrl,std::string(url));
            break;
        case eDRM_WideVine:
            SETCONFIGVALUE_PRIV(AAMP_APPLICATION_SETTING,eAAMPConfig_WVLicenseServerUrl,std::string(url));
            break;
        case eDRM_ClearKey:
            SETCONFIGVALUE_PRIV(AAMP_APPLICATION_SETTING,eAAMPConfig_CKLicenseServerUrl,std::string(url));
            break;
        case eDRM_MAX_DRMSystems:
            SETCONFIGVALUE_PRIV(AAMP_APPLICATION_SETTING,eAAMPConfig_LicenseServerUrl,std::string(url));
            break;
        default:
            AAMPLOG_ERR("PlayerInstanceAAMP:: invalid drm type(%d) received.", type);
            break;
    }
}

/**
 *  @brief Indicates if session token has to be used with license request or not.
 */
void PlayerInstanceAAMP::SetAnonymousRequest(bool isAnonymous)
{
    SETCONFIGVALUE_PRIV(AAMP_APPLICATION_SETTING,eAAMPConfig_AnonymousLicenseRequest,isAnonymous);
}

/**
 *  @brief Indicates average BW to be used for ABR Profiling.
 */
void PlayerInstanceAAMP::SetAvgBWForABR(bool useAvgBW)
{
    SETCONFIGVALUE_PRIV(AAMP_APPLICATION_SETTING,eAAMPConfig_AvgBWForABR,useAvgBW);
}

/**
 *  @brief SetPreCacheTimeWindow Function to Set PreCache Time
 */
void PlayerInstanceAAMP::SetPreCacheTimeWindow(int nTimeWindow)
{
    SETCONFIGVALUE_PRIV(AAMP_APPLICATION_SETTING,eAAMPConfig_PreCachePlaylistTime,nTimeWindow);
}

/**
 *  @brief Set VOD Trickplay FPS.
 */
void PlayerInstanceAAMP::SetVODTrickplayFPS(int vodTrickplayFPS)
{
    SETCONFIGVALUE_PRIV(AAMP_APPLICATION_SETTING,eAAMPConfig_VODTrickPlayFPS,vodTrickplayFPS);
}

/**
 *  @brief Set Linear Trickplay FPS.
 */
void PlayerInstanceAAMP::SetLinearTrickplayFPS(int linearTrickplayFPS)
{
    SETCONFIGVALUE_PRIV(AAMP_APPLICATION_SETTING,eAAMPConfig_LinearTrickPlayFPS,linearTrickplayFPS);
}

/**
 *  @brief Set Live Offset
 */
void PlayerInstanceAAMP::SetLiveOffset(double liveoffset)
{
    _SetLiveOffsetAppRequest(true);
    SETCONFIGVALUE_PRIV(AAMP_APPLICATION_SETTING,eAAMPConfig_LiveOffset, liveoffset);
}

/**
 *  @brief Set Live Offset
 */
void PlayerInstanceAAMP::SetLiveOffset4K(double liveoffset)
{
    _SetLiveOffsetAppRequest(true);
    SETCONFIGVALUE_PRIV(AAMP_APPLICATION_SETTING,eAAMPConfig_LiveOffset4K, liveoffset);
}

/**
 *  @brief To set the error code to be used for playback stalled error.
 */
void PlayerInstanceAAMP::SetStallErrorCode(int errorCode)
{
    SETCONFIGVALUE_PRIV(AAMP_APPLICATION_SETTING,eAAMPConfig_StallErrorCode,errorCode);
}

/**
 *  @brief To set the timeout value to be used for playback stall detection.
 */
void PlayerInstanceAAMP::SetStallTimeout(int timeoutMS)
{
    SETCONFIGVALUE_PRIV(AAMP_APPLICATION_SETTING,eAAMPConfig_StallTimeoutMS,timeoutMS);
}

/**
 *  @brief To set the Playback Position reporting interval.
 */
void PlayerInstanceAAMP::SetReportInterval(int reportIntervalMs)
{
	if(reportIntervalMs > 0)
	{
        SETCONFIGVALUE_PRIV(AAMP_APPLICATION_SETTING,eAAMPConfig_ReportProgressInterval,reportIntervalMs/1000.0);
	}
}

/**
 *  @brief To set the max retry attempts for init frag curl timeout failures
 */
void PlayerInstanceAAMP::SetInitFragTimeoutRetryCount(int count)
{
	if(count >= 0)
	{
        SETCONFIGVALUE_PRIV(AAMP_APPLICATION_SETTING,eAAMPConfig_InitFragmentRetryCount,count);
	}
}

/**
 *  @brief To get the current playback position.
 */
double PlayerInstanceAAMP::GetPlaybackPosition()
{
    double ret = _GetPositionSeconds();
    if ((!ISCONFIGSET_PRIV(eAAMPConfig_UseAbsoluteTimeline) || !_IsLiveStream()) && mProgressReportOffset > 0)
    {
        // Adjust progress positions for VOD, Linear without absolute timeline
        ret -= mProgressReportOffset;
    }
    else if(ISCONFIGSET_PRIV(eAAMPConfig_UseAbsoluteTimeline) &&
            mProgressReportOffset > 0 && _IsLiveStream() &&
            eABSOLUTE_PROGRESS_WITHOUT_AVAILABILITY_START == GETCONFIGVALUE_PRIV(eAAMPConfig_PreferredAbsoluteProgressReporting))
    {
        // Adjust progress positions for linear stream with absolute timeline config from AST
        ret -= mProgressReportAvailabilityOffset;
    }
    return ret;
}

/**
 *  @brief To get the current asset's duration.
 */
double PlayerInstanceAAMP::GetPlaybackDuration()
{
    double ret = _GetDurationMs() / 1000.00;
	return ret;
}

/**
 *  @fn GetId
 *
 *  @return returns unique id of player,
 */
int PlayerInstanceAAMP::GetId(void)
{
	int iPlayerId = mPlayerId;
	return iPlayerId;
}

void PlayerInstanceAAMP::SetId(int iPlayerId)
{
    mPlayerId = iPlayerId;
}

/**
 *  @brief To get the current AAMP state.
 */
AAMPPlayerState PlayerInstanceAAMP::GetState(void)
{
	AAMPPlayerState currentState = eSTATE_RELEASED;
	try
	{
		std::lock_guard<std::mutex> lock (mPrvAampMtx);
		currentState = _GetState();
	}
	catch (std::exception &e)
	{
		AAMPLOG_WARN("Invalid access to the instance of PlayerInstanceAAMP (%s), returning %s as current state",  e.what(),"eSTATE_RELEASED");
	}
	return currentState;
}

/**
 *  @brief To get the bitrate of current video profile.
 */
long PlayerInstanceAAMP::GetVideoBitrate(void)
{
    BitsPerSecond bitrate = 0;
    _AcquireStreamLock();
    if (mpStreamAbstractionAAMP)
    {
        bitrate = mpStreamAbstractionAAMP->GetVideoBitrate();
    }
    _ReleaseStreamLock();
    return bitrate;
}

/**
 *  @brief To set a preferred bitrate for video profile.
 */
void PlayerInstanceAAMP::SetVideoBitrate(BitsPerSecond bitrate)
{
	UsingPlayerId playerId(mPlayerId);
	if (bitrate != 0)
	{
		// Single bitrate profile selection , with abr disabled
        SETCONFIGVALUE_PRIV(AAMP_APPLICATION_SETTING,eAAMPConfig_EnableABR,false);
        SETCONFIGVALUE_PRIV(AAMP_APPLICATION_SETTING,eAAMPConfig_DefaultBitrate,(int)bitrate);
	}
	else
	{
        SETCONFIGVALUE_PRIV(AAMP_APPLICATION_SETTING,eAAMPConfig_EnableABR,true);
		int gpDefaultBitRate = gpGlobalConfig->GetConfigValue( eAAMPConfig_DefaultBitrate);
        SETCONFIGVALUE_PRIV(AAMP_APPLICATION_SETTING,eAAMPConfig_DefaultBitrate,gpDefaultBitRate);
		AAMPLOG_WARN("Resetting default bitrate to  %d", gpDefaultBitRate);
	}
}

/**
 *  @brief To get the bitrate of current audio profile.
 */
BitsPerSecond PlayerInstanceAAMP::GetAudioBitrate(void)
{
	//ERROR_OR_IDLE_STATE_CHECK_VAL(0);
	BitsPerSecond bitrate = 0;
	_AcquireStreamLock();
    if( mpStreamAbstractionAAMP)
    {
        bitrate = mpStreamAbstractionAAMP->GetAudioBitrate();
    }
    _ReleaseStreamLock();
	return bitrate;
}

/**
 *  @brief To set a preferred bitrate for audio profile.
 */
void PlayerInstanceAAMP::SetAudioBitrate(BitsPerSecond bitrate)
{
	//no-op for now
}

/**
 *  @brief To get video zoom mode
 */
int PlayerInstanceAAMP::GetVideoZoom(void)
{
	int ret = zoom_mode;
	return ret;
}

/**
 *  @brief To get video mute status
 */
bool PlayerInstanceAAMP::GetVideoMute(void)
{
	bool ret = video_muted;
	return ret;
}

/**
 *  @brief To get the current audio volume.
 */
int PlayerInstanceAAMP::GetAudioVolume(void)
{
	int ret = 0;
    UsingPlayerId playerId(mPlayerId);
    AAMPPlayerState state = GetState();
    if (eSTATE_IDLE == state)
    {
        AAMPLOG_WARN(" GetAudioVolume is returning cached value since player is at %s",
						 "eSTATE_IDLE");
    }
    ret = audio_volume;
	return ret;
}

/**
 *   @brief To get the current playback rate.
 */
int PlayerInstanceAAMP::GetPlaybackRate(void)
{
	int ret = 0;
	if(!pipeline_paused )
	{
		ret = rate;
	}
	return ret;
}

/**
 *  @brief To get the available video bitrates.
 */
std::vector<BitsPerSecond> PlayerInstanceAAMP::GetVideoBitrates(void)
{
    std::vector<BitsPerSecond> bitrates;
    UsingPlayerId playerId(mPlayerId);
    _AcquireStreamLock();
    if( mpStreamAbstractionAAMP )
    {
        bitrates = mpStreamAbstractionAAMP->GetVideoBitrates();
    }
    _ReleaseStreamLock();
	return bitrates;
}

/**
 *  @brief To get the available manifest.
 */
std::string PlayerInstanceAAMP::GetManifest(void)
{
    std::string ret;
    UsingPlayerId playerId(mPlayerId);
    AAMPPlayerState state = _GetState();
    switch( state )
    {
        case eSTATE_ERROR:
        case eSTATE_IDLE:
        case eSTATE_RELEASED:
        case eSTATE_STOPPED:
            AAMPLOG_WARN( "PlayerState=%d",state );
            break;
        default:
            if( mMediaFormat == eMEDIAFORMAT_DASH)
            {
                _GetLastDownloadedManifest(ret);
                AAMPLOG_INFO("PlayerInstanceAAMP: Retrieved manifest [len:%zu]",ret.length());
            }
            else
            {
                AAMPLOG_WARN( "mediaFormat=%d", mMediaFormat );
            }
            break;
    }
    return ret;
}

/**
 *  @brief To get the available audio bitrates.
 */
std::vector<BitsPerSecond> PlayerInstanceAAMP::GetAudioBitrates(void)
{
    //ERROR_OR_IDLE_STATE_CHECK_VAL(std::vector<BitsPerSecond>());
    std::vector<BitsPerSecond> bitrates;
    UsingPlayerId playerId(mPlayerId);
    _AcquireStreamLock();
    if (mpStreamAbstractionAAMP)
    {
        bitrates = mpStreamAbstractionAAMP->GetAudioBitrates();
    }
    _ReleaseStreamLock();
    return bitrates;
}

/**
 *  @brief To set the initial bitrate value.
 */
void PlayerInstanceAAMP::SetInitialBitrate(BitsPerSecond bitrate)
{
    SETCONFIGVALUE_PRIV(AAMP_APPLICATION_SETTING,eAAMPConfig_DefaultBitrate,(int)bitrate);
}

/**
 *  @brief To get the initial bitrate value.
 */
BitsPerSecond PlayerInstanceAAMP::GetInitialBitrate(void)
{
	return GETCONFIGVALUE_PRIV(eAAMPConfig_DefaultBitrate);
}

/**
 *  @brief To set the initial bitrate value for 4K assets.
 */
void PlayerInstanceAAMP::SetInitialBitrate4K(BitsPerSecond bitrate4K)
{
    SETCONFIGVALUE_PRIV(AAMP_APPLICATION_SETTING,eAAMPConfig_DefaultBitrate4K,(int)bitrate4K);
}

/**
 *  @brief To get the initial bitrate value for 4K assets.
 */
BitsPerSecond PlayerInstanceAAMP::GetInitialBitrate4k(void)
{
	return GETCONFIGVALUE_PRIV(eAAMPConfig_DefaultBitrate4K);
}

/**
 *   @brief To override default curl timeout for playlist/fragment downloads
 */
void PlayerInstanceAAMP::SetNetworkTimeout(double timeout)
{
    SETCONFIGVALUE_PRIV(AAMP_APPLICATION_SETTING,eAAMPConfig_NetworkTimeout,timeout);
}

/**
 *   @brief Optionally override default HLS main manifest download timeout with app-specific value.
 */
void PlayerInstanceAAMP::SetManifestTimeout(double timeout)
{
    SETCONFIGVALUE_PRIV(AAMP_APPLICATION_SETTING,eAAMPConfig_ManifestTimeout,timeout);
}

/**
 *  @brief Optionally override default HLS main manifest download timeout with app-specific value.
 */
void PlayerInstanceAAMP::SetPlaylistTimeout(double timeout)
{
    SETCONFIGVALUE_PRIV(AAMP_APPLICATION_SETTING,eAAMPConfig_PlaylistTimeout,timeout);
}

/**
 *  @brief To set the download buffer size value
 */
void PlayerInstanceAAMP::SetDownloadBufferSize(int bufferSize)
{
    SETCONFIGVALUE_PRIV(AAMP_APPLICATION_SETTING,eAAMPConfig_MaxFragmentCached,bufferSize);
}

/**
 *  @brief Set Preferred DRM.
 */
void PlayerInstanceAAMP::SetPreferredDRM(DRMSystems drmType)
{
	if(drmType != eDRM_NONE)
	{
        SETCONFIGVALUE_PRIV(AAMP_APPLICATION_SETTING,eAAMPConfig_PreferredDRM,(int)drmType);
		isPreferredDRMConfigured = true;
	}
	else
	{
		isPreferredDRMConfigured = false;
	}
}

/**
 *  @brief Set Stereo Only Playback.
 */
void PlayerInstanceAAMP::SetStereoOnlyPlayback(bool bValue)
{
	if(bValue)
	{
        SETCONFIGVALUE_PRIV(AAMP_APPLICATION_SETTING,eAAMPConfig_DisableEC3,true);
        SETCONFIGVALUE_PRIV(AAMP_APPLICATION_SETTING,eAAMPConfig_DisableAC3,true);
        SETCONFIGVALUE_PRIV(AAMP_APPLICATION_SETTING,eAAMPConfig_DisableAC4,true);
        SETCONFIGVALUE_PRIV(AAMP_APPLICATION_SETTING,eAAMPConfig_DisableATMOS,true);
        SETCONFIGVALUE_PRIV(AAMP_APPLICATION_SETTING,eAAMPConfig_StereoOnly,true);
	}
	else
	{
        SETCONFIGVALUE_PRIV(AAMP_APPLICATION_SETTING,eAAMPConfig_DisableEC3,false);
        SETCONFIGVALUE_PRIV(AAMP_APPLICATION_SETTING,eAAMPConfig_DisableAC3,false);
        SETCONFIGVALUE_PRIV(AAMP_APPLICATION_SETTING,eAAMPConfig_DisableAC4,false);
        SETCONFIGVALUE_PRIV(AAMP_APPLICATION_SETTING,eAAMPConfig_DisableATMOS,false);
        SETCONFIGVALUE_PRIV(AAMP_APPLICATION_SETTING,eAAMPConfig_StereoOnly,false);
	}
}

/**
 *  @brief Disable 4K Support in player
 */
void PlayerInstanceAAMP::SetDisable4K(bool bValue)
{
    SETCONFIGVALUE_PRIV(AAMP_APPLICATION_SETTING,eAAMPConfig_Disable4K,bValue);
}


/**
 *  @brief Set Bulk TimedMetadata Reporting flag
 */
void PlayerInstanceAAMP::SetBulkTimedMetaReport(bool bValue)
{
    SETCONFIGVALUE_PRIV(AAMP_APPLICATION_SETTING,eAAMPConfig_BulkTimedMetaReport,bValue);
}

/**
 *  @brief Set the flag if live playback needs bulk timed metadata.
 */
void PlayerInstanceAAMP::SetBulkTimedMetaReportLive(bool bValue)
{
    SETCONFIGVALUE_PRIV(AAMP_APPLICATION_SETTING,eAAMPConfig_BulkTimedMetaReportLive,bValue);
}

/**
 *  @brief Set unpaired discontinuity retune flag
 */
void PlayerInstanceAAMP::SetRetuneForUnpairedDiscontinuity(bool bValue)
{
    SETCONFIGVALUE_PRIV(AAMP_APPLICATION_SETTING,eAAMPConfig_RetuneForUnpairDiscontinuity,bValue);
}

/**
 *  @brief Set retune configuration for gstpipeline internal data stream error.
 */
void PlayerInstanceAAMP::SetRetuneForGSTInternalError(bool bValue)
{
    SETCONFIGVALUE_PRIV(AAMP_APPLICATION_SETTING,eAAMPConfig_RetuneForGSTError,bValue);
}

/**
 *  @brief Setting the alternate contents' (Ads/blackouts) URL
 */
void PlayerInstanceAAMP::SetAlternateContents(const std::string &adBreakId, const std::string &adId, const std::string &url)
{
	_SetAlternateContents(adBreakId, adId, url);
}

/**
 *  @brief To set the network proxy
 */
void PlayerInstanceAAMP::SetNetworkProxy(const char * proxy)
{
    SETCONFIGVALUE_PRIV(AAMP_APPLICATION_SETTING,eAAMPConfig_NetworkProxy ,(std::string)proxy);
}

/**
 *  @brief To set the proxy for license request
 */
void PlayerInstanceAAMP::SetLicenseReqProxy(const char * licenseProxy)
{
    SETCONFIGVALUE_PRIV(AAMP_APPLICATION_SETTING,eAAMPConfig_LicenseProxy ,(std::string)licenseProxy);
}

/**
 *  @brief To set the curl stall timeout value
 */
void PlayerInstanceAAMP::SetDownloadStallTimeout(int stallTimeout)
{
	if( stallTimeout >= 0 )
	{
        SETCONFIGVALUE_PRIV(AAMP_APPLICATION_SETTING,eAAMPConfig_CurlStallTimeout,stallTimeout);
	}
}

/**
 *  @brief To set the curl download start timeout
 */
void PlayerInstanceAAMP::SetDownloadStartTimeout(int startTimeout)
{
	if( startTimeout >= 0 )
	{
        SETCONFIGVALUE_PRIV(AAMP_APPLICATION_SETTING,eAAMPConfig_CurlDownloadStartTimeout,startTimeout);
	}
}

/**
 *  @brief To set the curl download low bandwidth timeout value
 */
void PlayerInstanceAAMP::SetDownloadLowBWTimeout(int lowBWTimeout)
{
	if( lowBWTimeout >= 0 )
	{
        SETCONFIGVALUE_PRIV(AAMP_APPLICATION_SETTING,eAAMPConfig_CurlDownloadLowBWTimeout,lowBWTimeout);
	}
}

/**
 *  @brief Set preferred subtitle language.
 */
void PlayerInstanceAAMP::SetPreferredSubtitleLanguage(const char* language)
{
    UsingPlayerId playerId(mPlayerId);
    AAMPPlayerState state = GetState();
    AAMPLOG_WARN("PlayerInstanceAAMP::(%s)->(%s)",  mSubLanguage.c_str(), language);
    
    //Compare it with the first element and update it to the new preferred language if they don't match.
    if(1 == preferredSubtitleLanguageVctr.size() && preferredSubtitleLanguageVctr.front() == language )
    {
        return;
    }
    
    if (state == eSTATE_IDLE || state == eSTATE_RELEASED)
    {
        AAMPLOG_WARN("PlayerInstanceAAMP:: \"%s\" language set prior to tune start",  language);
    }
    else
    {
        AAMPLOG_WARN("PlayerInstanceAAMP:: \"%s\" language set - will take effect on next tune", language);
    }
    SETCONFIGVALUE_PRIV(AAMP_APPLICATION_SETTING,eAAMPConfig_SubTitleLanguage,(std::string)language);
}

/**
 *  @brief Set Westeros sink configuration
 */
void PlayerInstanceAAMP::SetWesterosSinkConfig(bool bValue)
{
    SETCONFIGVALUE_PRIV(AAMP_APPLICATION_SETTING,eAAMPConfig_UseWesterosSink,bValue);
}

/**
 *  @brief Set license caching
 */
void PlayerInstanceAAMP::SetLicenseCaching(bool bValue)
{
    SETCONFIGVALUE_PRIV(AAMP_APPLICATION_SETTING,eAAMPConfig_SetLicenseCaching,bValue);
}

/**
 *  @brief Set Display resolution check for video profile filtering
 */
void PlayerInstanceAAMP::SetOutputResolutionCheck(bool bValue)
{
    SETCONFIGVALUE_PRIV(AAMP_APPLICATION_SETTING,eAAMPConfig_LimitResolution,bValue);
}

/**
 *  @brief Set Matching BaseUrl Config Configuration
 */
void PlayerInstanceAAMP::SetMatchingBaseUrlConfig(bool bValue)
{
    SETCONFIGVALUE_PRIV(AAMP_APPLICATION_SETTING,eAAMPConfig_MatchBaseUrl,bValue);
}

/**
 *  @brief Configure New ABR Enable/Disable
 */
void PlayerInstanceAAMP::SetNewABRConfig(bool bValue)
{
    SETCONFIGVALUE_PRIV(AAMP_APPLICATION_SETTING,eAAMPConfig_ABRBufferCheckEnabled,bValue);
    SETCONFIGVALUE_PRIV(AAMP_APPLICATION_SETTING,eAAMPConfig_NewDiscontinuity,bValue);
    SETCONFIGVALUE_PRIV(AAMP_APPLICATION_SETTING,eAAMPConfig_HLSAVTrackSyncUsingStartTime,bValue);
}

/**
 *  @brief to configure URI parameters for fragment downloads
 */
void PlayerInstanceAAMP::SetPropagateUriParameters(bool bValue)
{
    SETCONFIGVALUE_PRIV(AAMP_APPLICATION_SETTING,eAAMPConfig_PropagateURIParam,bValue);
}

/**
 *  @brief to optionally configure simulated per-download network latency for negative testing
 */
void PlayerInstanceAAMP::ApplyArtificialDownloadDelay(unsigned int DownloadDelayInMs)
{
	if( DownloadDelayInMs <= MAX_DOWNLOAD_DELAY_LIMIT_MS )
	{
        SETCONFIGVALUE_PRIV(AAMP_APPLICATION_SETTING,eAAMPConfig_DownloadDelay,(int)DownloadDelayInMs);
	}
}

/**
 *   @brief Configure URI  parameters
 */
void PlayerInstanceAAMP::SetSslVerifyPeerConfig(bool bValue)
{
    SETCONFIGVALUE_PRIV(AAMP_APPLICATION_SETTING,eAAMPConfig_SslVerifyPeer,bValue);
}


/**
 *   @brief Set audio track
 */
void PlayerInstanceAAMP::SetAudioTrack(std::string language, std::string rendition, std::string type, std::string codec, unsigned int channel, std::string label)
{
    UsingPlayerId playerId(mPlayerId);
    if (mAsyncTuneEnabled)
    {
        mScheduler.ScheduleTask(AsyncTaskObj(
                                             [language,rendition,type,codec,channel, label](void *data)
                                             {
                                                 PlayerInstanceAAMP *instance = static_cast<PlayerInstanceAAMP *>(data);
                                                 instance->SetAudioTrackInternal(language,rendition,type,codec,channel, label);
                                             }, (void *) this,__FUNCTION__));
    }
    else
    {
        SetAudioTrackInternal(language,rendition,type,codec,channel,label);
    }
}

/**
 *   @brief Set audio only playback
 *   @param[in] audioOnlyPlayback - true if audio only playback
 */
void PlayerInstanceAAMP::SetAudioOnlyPlayback(bool audioOnlyPlayback)
{
    SETCONFIGVALUE_PRIV(AAMP_APPLICATION_SETTING, eAAMPConfig_AudioOnlyPlayback, audioOnlyPlayback);
}

/**
 *  @brief Set audio track by audio parameters like language , rendition, codec etc..
 */
void PlayerInstanceAAMP::SetAudioTrackInternal(std::string language,  std::string rendition, std::string type, std::string codec, unsigned int channel, std::string label)
{
	mAudioTuple.clear();
	mAudioTuple.setAudioTrackTuple(language, rendition, codec, channel);
	/* Now we have an option to set language and rendition only*/
	SetPreferredLanguages( language.empty()?NULL:language.c_str(),
							rendition.empty()?NULL:rendition.c_str(),
							type.empty()?NULL:type.c_str(),
							codec.empty()?NULL:codec.c_str(),
							label.empty()?NULL:label.c_str());
}

/**
 *  @brief Set optional preferred codec list
 */
void PlayerInstanceAAMP::SetPreferredCodec(const char *codecList)
{
    _SetPreferredLanguages(NULL, NULL, NULL, codecList, NULL, NULL);
}

/**
 *  @brief Set optional preferred label list
 */
void PlayerInstanceAAMP::SetPreferredLabels(const char *labelList)
{
    _SetPreferredLanguages(NULL, NULL, NULL, NULL, labelList, NULL);
}

/**
 *  @brief Set optional preferred rendition list
 */
void PlayerInstanceAAMP::SetPreferredRenditions(const char *renditionList)
{
    _SetPreferredLanguages(NULL, renditionList, NULL, NULL, NULL, NULL);
}

/**
 *  @brief Get preferred audio properties
 */
std::string PlayerInstanceAAMP::GetPreferredAudioProperties()
{
	return _GetPreferredAudioProperties();
}

/**
 *   @brief Get preferred text properties
 *
 *   @return text preferred properties in json format
 */
std::string PlayerInstanceAAMP::GetPreferredTextProperties()
{
	return _GetPreferredTextProperties();
}

/**
 *  @brief Set optional preferred language list
 */
void PlayerInstanceAAMP::SetPreferredLanguages(const char *languageList, const char *preferredRendition, const char *preferredType, const char* codecList, const char* labelList, const Accessibility *accessibilityItem, const char *preferredName)
{
    _SetPreferredLanguages(languageList, preferredRendition, preferredType, codecList, labelList, accessibilityItem, preferredName);
}

/**
 *  @brief Set optional preferred language list
 */
void PlayerInstanceAAMP::SetPreferredTextLanguages(const char *param)
{
    _SetPreferredTextLanguages(param);
}

/**
 *  @brief Get Preferred DRM.
 */
DRMSystems PlayerInstanceAAMP::GetPreferredDRM()
{
	return _GetPreferredDRM();
}

/**
 *  @brief Get current preferred language list
 */
std::string PlayerInstanceAAMP::GetPreferredLanguages()
{
	return preferredLanguagesString;
}

/**
 *  @brief Configure New AdBreaker Enable/Disable
 */
void PlayerInstanceAAMP::SetNewAdBreakerConfig(bool bValue)
{
    SETCONFIGVALUE_PRIV(AAMP_APPLICATION_SETTING,eAAMPConfig_NewDiscontinuity,bValue);
	// Piggyback the PDT based processing for new Adbreaker processing.
    SETCONFIGVALUE_PRIV(AAMP_APPLICATION_SETTING,eAAMPConfig_HLSAVTrackSyncUsingStartTime,bValue);
}

/**
 *   @brief Set json formatted base64 license data payload
 */
void PlayerInstanceAAMP::SetBase64LicenseWrapping(bool bValue)
{
    SETCONFIGVALUE_PRIV(AAMP_APPLICATION_SETTING, eAAMPConfig_Base64LicenseWrapping, bValue);
}

/**
 *  @brief Get available video tracks.
 */
std::string PlayerInstanceAAMP::GetAvailableVideoTracks()
{
	std::string ret = _GetAvailableVideoTracks();
	return ret;
}

/**
 *  @brief Set video tracks.
 */
void PlayerInstanceAAMP::SetVideoTracks(std::vector<BitsPerSecond> bitrates)
{
    _SetVideoTracks(bitrates);
}

/**
 *  @brief Get available audio tracks.
 */
std::string PlayerInstanceAAMP::GetAvailableAudioTracks(bool allTrack)
{
	std::string ret;
    AAMPPlayerState state = _GetState();
    if (state != eSTATE_IDLE && state != eSTATE_ERROR)
    {
        ret = _GetAvailableAudioTracks(allTrack);
    }
    else
    {
        AAMPLOG_WARN("operation is not allowed when player in %d state !", state);
    }
	return ret;
}

/**
 *  @brief Get current audio track index
 */
std::string PlayerInstanceAAMP::GetAudioTrackInfo()
{
	std::string ret = _GetAudioTrackInfo();
	return ret;
}

/**
 *  @brief Get current audio track index
 */
std::string PlayerInstanceAAMP::GetTextTrackInfo()
{
	std::string ret = _GetTextTrackInfo();
	return ret;
}

/**
 *  @brief Get available text tracks.
 *
 *  @return std::string JSON formatted list of text tracks
 */
std::string PlayerInstanceAAMP::GetAvailableTextTracks(bool allTrack)
{
	std::string ret = _GetAvailableTextTracks(allTrack);
	return ret;
}

/**
 *   @brief Get the video window co-ordinates
 */
std::string PlayerInstanceAAMP::GetVideoRectangle()
{
	std::string ret = _GetVideoRectangle();
	return ret;
}

/**
 *  @brief Set the application name which has created PlayerInstanceAAMP, for logging purposes
 */
void PlayerInstanceAAMP::SetAppName(std::string name)
{
    _SetAppName(name);
}

/**
 *  @brief Return the associated application name
 */
std::string PlayerInstanceAAMP::GetAppName()
{
	return _GetAppName();
}

/**
 *  @brief Enable/disable the native CC rendering feature
 */
void PlayerInstanceAAMP::SetNativeCCRendering(bool enable)
{
    SETCONFIGVALUE_PRIV(AAMP_APPLICATION_SETTING,eAAMPConfig_NativeCCRendering,enable);
}

/**
 *  @brief To set the vod-tune-event according to the player.
 */
void PlayerInstanceAAMP::SetTuneEventConfig(int tuneEventType)
{
    SETCONFIGVALUE_PRIV(AAMP_APPLICATION_SETTING,eAAMPConfig_TuneEventConfig,tuneEventType);
}

/**
 *  @brief Set video rectangle property
 */
void PlayerInstanceAAMP::EnableVideoRectangle(bool rectProperty)
{
	if(!rectProperty)
	{
		if(ISCONFIGSET_PRIV(eAAMPConfig_UseWesterosSink))
		{
            SETCONFIGVALUE_PRIV(AAMP_APPLICATION_SETTING,eAAMPConfig_EnableRectPropertyCfg,false);
		}
		else
		{
			AAMPLOG_WARN("Skipping the configuration value[%d], since westerossink is disabled",  rectProperty);
		}
	}
	else
	{
        SETCONFIGVALUE_PRIV(AAMP_APPLICATION_SETTING,eAAMPConfig_EnableRectPropertyCfg,true);
	}
}

/**
 *  @brief Set audio track
 */
void PlayerInstanceAAMP::SetAudioTrack(int trackId)
{
    UsingPlayerId playerId(mPlayerId);
    if( mpStreamAbstractionAAMP)
    {
        std::vector<AudioTrackInfo> tracks = mpStreamAbstractionAAMP->GetAvailableAudioTracks();
        if (!tracks.empty() && (trackId >= 0 && trackId < tracks.size()))
        {
            if (mAsyncTuneEnabled)
            {
                mScheduler.ScheduleTask(AsyncTaskObj(
                                                     [tracks , trackId](void *data)
                                                     {
                                                         PlayerInstanceAAMP *instance = static_cast<PlayerInstanceAAMP *>(data);
                                                         instance->SetPreferredLanguages(tracks[trackId].language.c_str(), tracks[trackId].rendition.c_str(), tracks[trackId].accessibilityType.c_str(), tracks[trackId].codec.c_str(), tracks[trackId].label.c_str(), &tracks[trackId].accessibilityItem, tracks[trackId].name.c_str());
                                                     }, (void *) this,__FUNCTION__));
            }
            else
            {
                SetPreferredLanguages(tracks[trackId].language.c_str(), tracks[trackId].rendition.c_str(), tracks[trackId].accessibilityType.c_str(), tracks[trackId].codec.c_str(), tracks[trackId].label.c_str(), &tracks[trackId].accessibilityItem, tracks[trackId].name.c_str());
            }
        }
    }
}

/**
 *  @brief Get current audio track index
 */
int PlayerInstanceAAMP::GetAudioTrack()
{
	int ret = _GetAudioTrack();
	return ret;
}

/**
 *  @brief Set text track
 */
void PlayerInstanceAAMP::SetTextTrack(int trackId, char *ccData)
{
    UsingPlayerId playerId(mPlayerId);
    if( mpStreamAbstractionAAMP)
    {
        std::vector<TextTrackInfo> tracks = mpStreamAbstractionAAMP->GetAvailableTextTracks();
        AAMPLOG_INFO("trackId: %d tracks size %zu", trackId, tracks.size());
        if (!tracks.empty() && (MUTE_SUBTITLES_TRACKID == trackId || (trackId >= 0 && trackId < tracks.size())))
        {
            if (mAsyncTuneEnabled)
            {
                mScheduler.ScheduleTask(AsyncTaskObj(
                                                     [trackId, ccData ](void *data)
                                                     {
                                                         PlayerInstanceAAMP *instance = static_cast<PlayerInstanceAAMP *>(data);
                                                         instance->SetTextTrackInternal(trackId, ccData);
                                                     }, (void *) this,__FUNCTION__));
            }
            else
            {
                SetTextTrackInternal(trackId, ccData);
            }
        }
        else
        {
            SetTextTrackInternal(trackId, ccData);
        }
    }
    else
    {
        AAMPLOG_ERR("null aamp or Stream Abstraction AAMP");
        if (ccData != NULL)
        {
            SAFE_DELETE_ARRAY(ccData);
            ccData = NULL;
        }
    }
}

/**
 *  @brief Set text track by Id
 */
void PlayerInstanceAAMP::SetTextTrackInternal(int trackId, char *data)
{
	if( mpStreamAbstractionAAMP)
	{
        _SetTextTrack(trackId, data);
	}
}


/**
 *  @brief Get current text track index
 */
int PlayerInstanceAAMP::GetTextTrack()
{
	int ret = _GetTextTrack();
	return ret;
}

/**
 *  @brief Set CC visibility on/off
 */
void PlayerInstanceAAMP::SetCCStatus(bool enabled)
{
    _SetCCStatus(enabled);
}

/**
 *  @brief Get CC visibility on/off
 */
bool PlayerInstanceAAMP::GetCCStatus(void)
{
	return _GetCCStatus();
}

/**
 *  @brief Set style options for text track rendering
 */
void PlayerInstanceAAMP::SetTextStyle(const std::string &options)
{
    _SetTextStyle(options);
}

/**
 *  @brief Get style options for text track rendering
 */
std::string PlayerInstanceAAMP::GetTextStyle()
{
	std::string ret = _GetTextStyle();
	return ret;
}

/**
 *  @brief Set Initial profile ramp down limit.
 */
void PlayerInstanceAAMP::SetInitRampdownLimit(int limit)
{
    SETCONFIGVALUE_PRIV(AAMP_APPLICATION_SETTING,eAAMPConfig_InitRampDownLimit,limit);
}


/**
 *  @brief Set the CEA format for force setting
 */
void PlayerInstanceAAMP::SetCEAFormat(int format)
{
    SETCONFIGVALUE_PRIV(AAMP_APPLICATION_SETTING,eAAMPConfig_CEAPreferred,format);
}


/**
 *   @brief To get the available bitrates for thumbnails.
 */
std::string PlayerInstanceAAMP::GetAvailableThumbnailTracks(void)
{
	std::string ret = _GetThumbnailTracks();
	return ret;
}

/**
 *  @brief To set a preferred bitrate for thumbnail profile.
 */
bool PlayerInstanceAAMP::SetThumbnailTrack(int thumbIndex)
{
	bool ret = false;
    UsingPlayerId playerId(mPlayerId);
    _AcquireStreamLock();
    if(thumbIndex >= 0 && mpStreamAbstractionAAMP)
    {
        ret = mpStreamAbstractionAAMP->SetThumbnailTrack(thumbIndex);
    }
    _ReleaseStreamLock();
    AAMPLOG_INFO(" SetThumbnailTrack [%d] result: %s", thumbIndex, (ret ? "success" : "fail"));
	return ret;
}

/**
 *  @brief To get preferred thumbnails for the duration.
 */
std::string PlayerInstanceAAMP::GetThumbnails(double tStart, double tEnd)
{
	std::string ret = _GetThumbnails(tStart, tEnd);
	return ret;
}

/**
 *  @brief Set the session token for player
 */
void PlayerInstanceAAMP::SetSessionToken(std::string sessionToken)
{ // Stored as tune setting , this will get cleared after one tune session
    SETCONFIGVALUE_PRIV(AAMP_TUNE_SETTING,eAAMPConfig_AuthToken,sessionToken);
    mDynamicDrmDefaultconfig.authToken = sessionToken;
}

/**
 *  @brief Enable seekable range values in progress event
 */
void PlayerInstanceAAMP::EnableSeekableRange(bool bValue)
{
    SETCONFIGVALUE_PRIV(AAMP_APPLICATION_SETTING,eAAMPConfig_EnableSeekRange,bValue);
}

/**
 *  @brief Enable video PTS reporting in progress event
 */
void PlayerInstanceAAMP::SetReportVideoPTS(bool bValue)
{
    SETCONFIGVALUE_PRIV(AAMP_APPLICATION_SETTING,eAAMPConfig_ReportVideoPTS,bValue);
}

/**
 *  @brief Disable Content Restrictions - unlock
 */
void PlayerInstanceAAMP::DisableContentRestrictions(long grace, long time, bool eventChange)
{
	_DisableContentRestrictions(grace, time, eventChange);
}

/**
 *  @brief Enable Content Restrictions - lock
 */
void PlayerInstanceAAMP::EnableContentRestrictions()
{
    _EnableContentRestrictions();
}

/**
 *  @brief Manage async tune configuration for specific contents
 */
void PlayerInstanceAAMP::ManageAsyncTuneConfig(const char* mainManifestUrl)
{
	MediaFormat mFormat = _GetMediaFormatType(mainManifestUrl);
	if(mFormat == eMEDIAFORMAT_HDMI || mFormat == eMEDIAFORMAT_COMPOSITE || mFormat == eMEDIAFORMAT_OTA)
	{
		SetAsyncTuneConfig(false);
	}
}

/**
 *  @brief Set async tune configuration
 */
void PlayerInstanceAAMP::SetAsyncTuneConfig(bool bValue)
{
    SETCONFIGVALUE_PRIV(AAMP_APPLICATION_SETTING,eAAMPConfig_AsyncTune,bValue);
	// Start it for the playerinstance if default not started and App wants
	// Stop Async operation for the playerinstance if default started and App doesn't want
	AsyncStartStop();
}

/**
 *  @brief Enable/Disable async operation
 */
void PlayerInstanceAAMP::AsyncStartStop()
{
	// Check if global configuration is set to false
	// Additional check added here, since this API can be called from jsbindings/native app
	mAsyncTuneEnabled = ISCONFIGSET_PRIV(eAAMPConfig_AsyncTune);
	if (mAsyncTuneEnabled && !mAsyncRunning)
	{
		AAMPLOG_WARN("Enable async tune operation!!" );
		mAsyncRunning = true;
		//mScheduler.StartScheduler();
        _SetEventPriorityAsyncTune(true);
	}
	else if(!mAsyncTuneEnabled && mAsyncRunning)
	{
		AAMPLOG_WARN("Disable async tune operation!!");
        _SetEventPriorityAsyncTune(false);
		//mScheduler.StopScheduler();
		mAsyncRunning = false;
	}
}

/**
 *  @brief Enable/disable configuration to persist ABR profile over seek/SAP
 */
void PlayerInstanceAAMP::PersistBitRateOverSeek(bool bValue)
{
    SETCONFIGVALUE_PRIV(AAMP_APPLICATION_SETTING,eAAMPConfig_PersistentBitRateOverSeek,bValue);
}


/**
 *  @brief Stop playback and release resources.
 */
void PlayerInstanceAAMP::StopInternal(bool sendStateChangeEvent)
{
	_StopPausePositionMonitoring("Stop() called");
	AAMPPlayerState state = _GetState();
	if(!_IsTuneCompleted())
	{
		_TuneFail(true);
	}
	AAMPLOG_MIL("aamp_stop PlayerState=%d",state);
	_Stop();
	// Revert all custom specific setting, tune specific setting and stream specific setting , back to App/default setting
	mConfig.RestoreConfiguration(AAMP_CUSTOM_DEV_CFG_SETTING);
	mConfig.RestoreConfiguration(AAMP_TUNE_SETTING);
	mConfig.RestoreConfiguration(AAMP_STREAM_SETTING);
	mIsStream4K = false;
}

/**
 *  @brief To set preferred paused state behavior
 */
void PlayerInstanceAAMP::SetPausedBehavior(int behavior)
{
    UsingPlayerId playerId(mPlayerId);
    if(behavior >= 0 && behavior < ePAUSED_BEHAVIOR_MAX)
    {
        AAMPLOG_WARN("Player Paused behavior : %d", behavior);
        SETCONFIGVALUE_PRIV(AAMP_APPLICATION_SETTING,eAAMPConfig_LivePauseBehavior,behavior);
    }
}

/**
 *  @brief To set UseAbsoluteTimeline for DASH
 */
void PlayerInstanceAAMP::SetUseAbsoluteTimeline(bool configState)
{
    SETCONFIGVALUE_PRIV(AAMP_APPLICATION_SETTING,eAAMPConfig_UseAbsoluteTimeline,configState);
}

/**
 *  @brief To set the repairIframes flag
 */
void PlayerInstanceAAMP::SetRepairIframes(bool configState)
{
    SETCONFIGVALUE_PRIV(AAMP_APPLICATION_SETTING,eAAMPConfig_RepairIframes,configState);
}

/**
 *  @brief InitAAMPConfig - Initialize the media player session with json config
 */
bool PlayerInstanceAAMP::InitAAMPConfig(const char *jsonStr)
{
	bool retVal = false;
	cJSON *cfgdata = NULL;
	if(jsonStr)
	{
		cfgdata = cJSON_Parse(jsonStr);
		if(cfgdata != NULL)
		{
			retVal = mConfig.ProcessConfigJson(cfgdata,AAMP_APPLICATION_SETTING);
		}
	}
	mConfig.DoCustomSetting(AAMP_APPLICATION_SETTING);
	if(GETCONFIGOWNER_PRIV(eAAMPConfig_AsyncTune) == AAMP_APPLICATION_SETTING)
	{
		AsyncStartStop();
	}

	if(GETCONFIGOWNER_PRIV(eAAMPConfig_MaxDASHDRMSessions) == AAMP_APPLICATION_SETTING)
	{
        _UpdateMaxDRMSessions();
	}

	if(cfgdata != NULL)
	{
		cJSON *drmConfig = cJSON_GetObjectItem(cfgdata,"drmConfig");
		if(drmConfig)
		{
			std::string LicenseServerUrl = GETCONFIGVALUE_PRIV(eAAMPConfig_PRLicenseServerUrl);
			mDynamicDrmDefaultconfig.licenseEndPoint.insert(std::pair<std::string, std::string>("com.microsoft.playready", LicenseServerUrl.c_str()));
			LicenseServerUrl = GETCONFIGVALUE_PRIV(eAAMPConfig_WVLicenseServerUrl);
			mDynamicDrmDefaultconfig.licenseEndPoint.insert(std::pair<std::string, std::string>("com.widevine.alpha",LicenseServerUrl.c_str()));
			LicenseServerUrl = GETCONFIGVALUE_PRIV(eAAMPConfig_CKLicenseServerUrl);
			mDynamicDrmDefaultconfig.licenseEndPoint.insert(std::pair<std::string, std::string>("org.w3.clearkey",LicenseServerUrl.c_str()));
			std::string customData = GETCONFIGVALUE_PRIV(eAAMPConfig_CustomLicenseData);
			mDynamicDrmDefaultconfig.customData = customData;
		}
		cJSON_Delete(cfgdata);
	}

	if(NULL == curlhost[0] && mConfig.IsConfigSet(eAAMPConfig_EnableCurlStore))
	{
		for (int i = 0; i < eCURLINSTANCE_MAX; i++)
		{
			curlhost[i] = new eCurlHostMap();
		}
	}

	// also enable Ethan log redirection if useRialtoSink enabled using initconfig option.
	AampLogManager::enableEthanLogRedirection = ISCONFIGSET_PRIV(eAAMPConfig_useRialtoSink);
	PlayerLogManager::SetLoggerInfo(AampLogManager::disableLogRedirection, AampLogManager::enableEthanLogRedirection, AampLogManager::aampLoglevel, AampLogManager::locked);
	return retVal;
}

/**
 *  @brief GetAAMPConfig - GetAamp Config as JSON string
 */
std::string PlayerInstanceAAMP::GetAAMPConfig()
{
	std::string jsonStr;
	mConfig.GetAampConfigJSONStr(jsonStr);
	return jsonStr;
}

/**
 *  @brief To set whether the JS playback session is from XRE or not.
 */
void PlayerInstanceAAMP::XRESupportedTune(bool xreSupported)
{
    SETCONFIGVALUE_PRIV(AAMP_APPLICATION_SETTING,eAAMPConfig_XRESupportedTune,xreSupported);
}


/**
 *  @brief Set auxiliary language
 */
void PlayerInstanceAAMP::SetAuxiliaryLanguage(const std::string &language)
{
	if(mAsyncTuneEnabled)
	{

		mScheduler.ScheduleTask(AsyncTaskObj([language](void *data)
					{
						PlayerInstanceAAMP *instance = static_cast<PlayerInstanceAAMP *>(data);
						instance->SetAuxiliaryLanguageInternal(language);
					}, (void *)this , __FUNCTION__));
	}
	else
	{
		SetAuxiliaryLanguageInternal(language);
	}

}

/**
 *  @brief Set auxiliary track language.
 */
void PlayerInstanceAAMP::SetAuxiliaryLanguageInternal(const std::string &language)
{ // note: this feature available only on bluetooth enabled devices
    UsingPlayerId playerId(mPlayerId);
    std::string currentLanguage = _GetAuxiliaryAudioLanguage();
    AAMPLOG_WARN("aamp_SetAuxiliaryLanguage(%s)->(%s)", currentLanguage.c_str(), language.c_str());
    if(language != currentLanguage)
    {
        
        AAMPPlayerState state = _GetState();
        // There is no active playback session, save the language for later
        if (state == eSTATE_IDLE || state == eSTATE_RELEASED)
        {
            _SetAuxiliaryLanguage(language);
        }
        // check if language is supported in manifest languagelist
        else if((_IsAudioLanguageSupported(language.c_str())) || (!mMaxLanguageCount))
        {
            _SetAuxiliaryLanguage(language);
            if( mpStreamAbstractionAAMP )
            {
                AAMPLOG_WARN("aamp_SetAuxiliaryLanguage(%s) retuning", language.c_str());
                
                discardEnteringLiveEvt = true;
                
                seek_pos_seconds = _GetPositionSeconds();
                _TeardownStream(false);
                _TuneHelper(eTUNETYPE_SEEK);
                
                discardEnteringLiveEvt = false;
            }
        }
    }
}

/**
 *  @brief Set License Custom Data
 */
void PlayerInstanceAAMP::SetLicenseCustomData(const char *customData)
{
    SETCONFIGVALUE_PRIV(AAMP_APPLICATION_SETTING,eAAMPConfig_CustomLicenseData,std::string(customData));
}

/**
 *  @brief Get playback statistics formated for partner apps
 */
std::string PlayerInstanceAAMP::GetPlaybackStats()
{
	std::string stats = _GetPlaybackStats();
	return stats;
}

void PlayerInstanceAAMP::ProcessContentProtectionDataConfig(const char *jsonbuffer)
{
    UsingPlayerId playerId(mPlayerId);
    AAMPLOG_INFO("ProcessContentProtectionDataConfig received DRM config data from app");
    //In case of tune failure, It is necessary to trigger the release of the mWaitForDynamicDRMToUpdate condition
    //wait before exiting the API.
    //Otherwise it may result in crash by the player attempting to access the cleared DRMSession after the timeout.
    //The timeout may happen in next tune.
    AAMPPlayerState state = GetState();
    if (eSTATE_ERROR == state)
    {
        _ReleaseDynamicDRMToUpdateWait();
        return;
    }
    std::vector<uint8_t> tempKeyId;
    DynamicDrmInfo dynamicDrmCache;
    if(vDynamicDrmData.size()>9)
    {
        vDynamicDrmData.erase(vDynamicDrmData.begin());
    }
    int empty_config;
    cJSON *cfgdata = cJSON_Parse(jsonbuffer);
    empty_config = cJSON_GetArraySize(cfgdata);
    if(cfgdata)
    {
        cJSON *arrayItem = cJSON_GetObjectItem(cfgdata, "keyID" );
        if(arrayItem) {
            cJSON *iterator = NULL;
            cJSON_ArrayForEach(iterator, arrayItem) {
                if (cJSON_IsNumber(iterator)) {
                    tempKeyId.push_back(iterator->valueint);
                }
            }
            dynamicDrmCache.keyID=tempKeyId;
        }
        else {
            AAMPLOG_WARN("Response message doesn't have keyID ignoring the message");
            _ReleaseDynamicDRMToUpdateWait();
            return;
        }
        
        //Remove old config if response keyId already in cache
        int iter1 = 0;
        while (iter1 < vDynamicDrmData.size()) {
            DynamicDrmInfo dynamicDrmCache = vDynamicDrmData.at(iter1);
            if(tempKeyId == dynamicDrmCache.keyID) {
                AAMPLOG_WARN("Deleting old config and updating new config");
                vDynamicDrmData.erase(vDynamicDrmData.begin()+iter1);
                break;
            }
            iter1++;
        }
        cJSON *playReadyObject = cJSON_GetObjectItem(cfgdata, "com.microsoft.playready");
        std::string playreadyurl="";
        if(playReadyObject) {
            playreadyurl = playReadyObject->valuestring;
            AAMPLOG_TRACE("App configured Playready License server URL : %s",playreadyurl.c_str());
            
        }
        dynamicDrmCache.licenseEndPoint.insert(std::pair<std::string, std::string>("com.microsoft.playready",playreadyurl.c_str()));
        
        cJSON *wideVineObject = cJSON_GetObjectItem(cfgdata, "com.widevine.alpha");
        std::string widevineurl = "";
        if(wideVineObject) {
            widevineurl = wideVineObject->valuestring;
            AAMPLOG_TRACE("App configured widevine License server URL : %s",widevineurl.c_str());
        }
        dynamicDrmCache.licenseEndPoint.insert(std::pair<std::string, std::string>("com.widevine.alpha",widevineurl.c_str()));
        
        cJSON *clearKeyObject = cJSON_GetObjectItem(cfgdata, "org.w3.clearkey");
        std::string clearkeyurl = "";
        if(clearKeyObject) {
            clearkeyurl = clearKeyObject->valuestring;
            AAMPLOG_TRACE("App configured clearkey License server URL : %s",clearkeyurl.c_str());
        }
        dynamicDrmCache.licenseEndPoint.insert(std::pair<std::string, std::string>("org.w3.clearkey",clearkeyurl.c_str()));
        
        cJSON *customDataObject = cJSON_GetObjectItem(cfgdata, "customData");
        std::string customdata = "";
        if(customDataObject) {
            customdata = customDataObject->valuestring;
            AAMPLOG_TRACE("App configured customData : %s",customdata.c_str());
        }
        dynamicDrmCache.customData = customdata;
        
        cJSON *authTokenObject = cJSON_GetObjectItem(cfgdata, "authToken");
        std::string authToken = "";
        if(authTokenObject) {
            authToken = authTokenObject->valuestring;
            AAMPLOG_TRACE("App configured authToken : %s",authToken.c_str());
        }
        dynamicDrmCache.authToken = authToken;
        
        cJSON *licenseResponseObject = cJSON_GetObjectItem(cfgdata, "licenseResponse");
        if(licenseResponseObject) {
            std::string licenseResponse = licenseResponseObject->valuestring;
            if(!licenseResponse.empty()) {
                AAMPLOG_TRACE("App configured License Response");
            }
        }
        if(empty_config == 1){
            mDynamicDrmDefaultconfig.keyID=tempKeyId;
            AAMPLOG_WARN("Received empty config applying default config");
            vDynamicDrmData.push_back(mDynamicDrmDefaultconfig);
            DynamicDrmInfo dynamicDrmCache = mDynamicDrmDefaultconfig;
            std::map<std::string,std::string>::iterator itr;
            for(itr = dynamicDrmCache.licenseEndPoint.begin();itr!=dynamicDrmCache.licenseEndPoint.end();itr++) {
                if(strcasecmp("com.microsoft.playready",itr->first.c_str())==0) {
                    playreadyurl = itr->second;
                }
                if(strcasecmp("com.widevine.alpha",itr->first.c_str())==0) {
                    widevineurl = itr->second;
                }
                if(strcasecmp("org.w3.clearkey",itr->first.c_str())==0) {
                    clearkeyurl = itr->second;
                }
            }
            authToken = dynamicDrmCache.authToken;
            customdata = dynamicDrmCache.customData;
        }
        else {
            vDynamicDrmData.push_back(dynamicDrmCache);
        }
        
        if(tempKeyId == mcurrent_keyIdArray){
            AAMPLOG_WARN("Player received the config for requested keyId applying the configs");
            SETCONFIGVALUE_PRIV(AAMP_APPLICATION_SETTING,eAAMPConfig_PRLicenseServerUrl,playreadyurl);
            SETCONFIGVALUE_PRIV(AAMP_APPLICATION_SETTING,eAAMPConfig_WVLicenseServerUrl,widevineurl);
            SETCONFIGVALUE_PRIV(AAMP_APPLICATION_SETTING,eAAMPConfig_CKLicenseServerUrl,clearkeyurl);
            SETCONFIGVALUE_PRIV(AAMP_APPLICATION_SETTING,eAAMPConfig_CustomLicenseData,customdata);
            SETCONFIGVALUE_PRIV(AAMP_APPLICATION_SETTING,eAAMPConfig_AuthToken,authToken);
            _ReleaseDynamicDRMToUpdateWait();
            AAMPLOG_WARN("Updated new Content Protection Data Configuration");
        }
        
    }
    cJSON_Delete(cfgdata);
}

/**
 *   @brief To set the dynamic drm update on key rotation timeout value.
 *
 *   @param[in] preferred timeout value in seconds
 */
void PlayerInstanceAAMP::SetContentProtectionDataUpdateTimeout(int timeoutS)
{
    SETCONFIGVALUE_PRIV(AAMP_APPLICATION_SETTING,eAAMPConfig_ContentProtectionDataUpdateTimeout,timeoutS*1000);
}

/**
 *  @brief To set Dynamic DRM feature by Application
 */
void PlayerInstanceAAMP::SetRuntimeDRMConfigSupport(bool DynamicDRMSupported)
{
    SETCONFIGVALUE_PRIV(AAMP_APPLICATION_SETTING,eAAMPConfig_RuntimeDRMConfig,DynamicDRMSupported);
}

/**
 * @fn IsOOBCCRenderingSupported
 *
 * @return bool, True if Out of Band Closed caption/subtitle rendering supported
 */
bool PlayerInstanceAAMP::IsOOBCCRenderingSupported()
{
	return PlayerCCManager::GetInstance()->IsOOBCCRenderingSupported();
}

/**
 *  @brief To get video playback quality
 */
std::string PlayerInstanceAAMP::GetVideoPlaybackQuality(void)
{
	std::string ret = _GetVideoPlaybackQuality();
	return ret;
}

void PlayerInstanceAAMP::updateManifest(const char *manifestData)
{
    _updateManifest(manifestData);
}

#include "isobmff/isobmffbuffer.h"

/**
 * @fn RecalculatePTS
 * @param[in] mediaType stream type
 * @param[in] ptr buffer pointer
 * @param[in] len length of buffer
 */
double RecalculatePTS(AampMediaType mediaType, const void *ptr, size_t len, PlayerInstanceAAMP *aamp)
{
    double ret = 0;
    uint32_t timeScale = 0;
    switch( mediaType )
    {
    case eMEDIATYPE_VIDEO:
        timeScale = aamp->_GetVidTimeScale();
        break;
    case eMEDIATYPE_AUDIO:
    case eMEDIATYPE_AUX_AUDIO:
        timeScale = aamp->_GetAudTimeScale();
        break;
    case eMEDIATYPE_SUBTITLE:
        timeScale = aamp->_GetSubTimeScale();
        break;
    default:
        AAMPLOG_WARN("Invalid media type %d", mediaType);
        break;
    }
    IsoBmffBuffer isobuf;
    isobuf.setBuffer((uint8_t *)ptr, len);
    bool bParse = false;
    try
    {
        bParse = isobuf.parseBuffer();
    }
    catch( std::bad_alloc& ba)
    {
        AAMPLOG_ERR("Bad allocation: %s", ba.what() );
    }
    catch( std::exception &e)
    {
        AAMPLOG_ERR("Unhandled exception: %s", e.what() );
    }
    catch( ... )
    {
        AAMPLOG_ERR("Unknown exception");
    }
    if(bParse && (0 != timeScale))
    {
        uint64_t fPts = 0;
        bool bParse = isobuf.getFirstPTS(fPts);
        if (bParse)
        {
            ret = fPts/(timeScale*1.0);
        }
    }
    return ret;
}
