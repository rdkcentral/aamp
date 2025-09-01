/*
* If not stated otherwise in this file or this component's license file the
* following copyright and licenses apply:
*
* Copyright 2022 RDK Management
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

#include "priv_aamp.h"
#include "MockPrivateInstanceAAMP.h"
#include "AampMPDDownloader.h"
#include "AampStreamSinkManager.h"

#include "ID3Metadata.hpp"
#include "AampSegmentInfo.hpp"

MockPrivateInstanceAAMP *g_mockPrivateInstanceAAMP = nullptr;

static int PLAYERID_CNTR = 0;

PrivateInstanceAAMP::PrivateInstanceAAMP(AampConfig *config) :
	profiler(),
	licenceFromManifest(false),
	previousAudioType(eAUDIO_UNKNOWN),
	isPreferredDRMConfigured(false),
	mFogTSBEnabled(false),
	mLiveOffset(AAMP_LIVE_OFFSET),
	seek_pos_seconds(-1),
	rate(0),
	subtitles_muted(true),
	subscribedTags(),
	httpHeaderResponses(),
	IsTuneTypeNew(false),
	culledSeconds(0.0),
	culledOffset(0.0),
	mNewSeekInfo(),
	mIsVSS(false),
	mIsAudioContextSkipped(false),
	mMediaFormat(eMEDIAFORMAT_HLS),
	mPersistedProfileIndex(0),
	mAvailableBandwidth(0),
	mContentType(ContentType_UNKNOWN),
	mManifestUrl(""),
	mServiceZone(),
	mVssVirtualStreamId(),
	preferredLanguagesList(),
	preferredLabelList(),
	mhAbrManager(),
	mVideoEnd(NULL),
	mIsFirstRequestToFOG(false),
	mTuneType(eTUNETYPE_NEW_NORMAL),
	mCdaiObject(NULL),
	mBufUnderFlowStatus(false),
	mVideoBasePTS(0),
	mIsIframeTrackPresent(false),
	mManifestTimeoutMs(-1),
	mNetworkTimeoutMs(-1),
	waitforplaystart(),
	mMutexPlaystart(),
	drmParserMutex(),
	mDRMLicenseManager(NULL),
	mDrmInitData(),
	mPreferredTextTrack(),
	midFragmentSeekCache(false),
	mDisableRateCorrection (false),
	mthumbIndexValue(-1),
	mMPDPeriodsInfo(),
	mProfileCappedStatus(false),
	mSchemeIdUriDai(""),
	mDisplayWidth(0),
	mDisplayHeight(0),
	preferredRenditionString(""),
	preferredTypeString(""),
	mAudioTuple(),
	preferredAudioAccessibilityNode(),
	preferredTextLanguagesList(),
	preferredTextRenditionString(""),
	preferredTextTypeString(""),
	preferredTextAccessibilityNode(),
	mProgressReportOffset(-1),
	mFirstFragmentTimeOffset(-1),
	mScheduler(NULL),
	mConfig(config),
	mSubLanguage(),
	mPlayerId(PLAYERID_CNTR++),
	mIsWVKIDWorkaround(false),
	mAuxAudioLanguage(),
	mAbsoluteEndPosition(0),
	mIsLive(false),
	mIsLiveStream(false),
	mAampLLDashServiceData{},
	bLLDashAdjustPlayerSpeed(false),
	mLLDashCurrentPlayRate(AAMP_NORMAL_PLAY_RATE),
	mEventManager(NULL),
	mbDetached(false),
	mIsFakeTune(false),
	mIsDefaultOffset(false),
	mNextPeriodDuration(0),
	mNextPeriodStartTime(0),
	mNextPeriodScaledPtoStartTime(0),
	mOffsetFromTunetimeForSAPWorkaround(0),
	mLanguageChangeInProgress(false),
	mbSeeked(false),
	playerStartedWithTrickPlay(false),
	mIsInbandCC(true),
	bitrateList(),
	userProfileStatus(false),
	mCurrentAudioTrackIndex(-1),
	mCurrentTextTrackIndex(-1),
	mEncryptedPeriodFound(false),
	mPipelineIsClear(false),
	mLLActualOffset(-1),
	mIsStream4K(false),
	mFogDownloadFailReason(""),
	mBlacklistedProfiles(),
	mId3MetadataCache{},
	mMPDDownloaderInstance(new AampMPDDownloader()),
	mMPDStichOption(OPT_1_FULL_MANIFEST_TUNE),mMPDStichRefreshUrl(""),
	mDiscoCompleteLock(),
	mWaitForDiscoToComplete(),
	mIsPeriodChangeMarked(false),
	mProgressReportAvailabilityOffset(-1),
	mpStreamAbstractionAAMP(),
	zoom_mode(VIDEO_ZOOM_NONE),
	mLocalAAMPTsb(false),
	mVideoFormat(),
	mAudioFormat(),
	mPreviousAudioType(),
	mAuxFormat(),
	mCurlShared()
{
}

PrivateInstanceAAMP::~PrivateInstanceAAMP()
{
}

double PrivateInstanceAAMP::RecalculatePTS(AampMediaType mediaType, const void *ptr, size_t len)
{
    double pts = 0.0;
    if (g_mockPrivateInstanceAAMP != nullptr)
    {
        pts = g_mockPrivateInstanceAAMP->RecalculatePTS(mediaType, ptr, len);
    }
    return pts;
}

size_t PrivateInstanceAAMP::HandleSSLWriteCallback ( char *ptr, size_t size, size_t nmemb, void* userdata )
{
	return 0;
}

size_t PrivateInstanceAAMP::HandleSSLHeaderCallback ( const char *ptr, size_t size, size_t nmemb, void* user_data )
{
	return 0;
}

int PrivateInstanceAAMP::HandleSSLProgressCallback ( void *clientp, double dltotal, double dlnow, double ultotal, double ulnow )
{
	return 0;
}

void PrivateInstanceAAMP::UpdateUseSinglePipeline( void )
{
}

void PrivateInstanceAAMP::UpdateMaxDRMSessions( void )
{
}

void PrivateInstanceAAMP::ActivatePlayer()
{
}
void PrivateInstanceAAMP::SendMediaMetadataEvent()
{
}
AAMPPlayerState PrivateInstanceAAMP::GetState()
{
	AAMPPlayerState state = eSTATE_IDLE;
	if (g_mockPrivateInstanceAAMP != nullptr)
	{
		state = g_mockPrivateInstanceAAMP->GetState();
	}
	return state;
}

void PrivateInstanceAAMP::SetState(AAMPPlayerState state)
{
	if (g_mockPrivateInstanceAAMP != nullptr)
	{
		g_mockPrivateInstanceAAMP->SetState(state);
	}
}

void PrivateInstanceAAMP::Stop( bool isDestructing )
{
}

void PrivateInstanceAAMP::SetAudioTrack(int)
{

}
bool PrivateInstanceAAMP::IsActiveInstancePresent()
{
	return true;
}

void PrivateInstanceAAMP::StartPausePositionMonitoring(long long pausePositionMilliseconds)
{
	if (g_mockPrivateInstanceAAMP != nullptr)
	{
		g_mockPrivateInstanceAAMP->StartPausePositionMonitoring(pausePositionMilliseconds);
	}
}

void PrivateInstanceAAMP::StopPausePositionMonitoring(std::string reason)
{
	if (g_mockPrivateInstanceAAMP != nullptr)
	{
		g_mockPrivateInstanceAAMP->StopPausePositionMonitoring(reason);
	}
}

AampCacheHandler * PrivateInstanceAAMP::getAampCacheHandler()
{
	return nullptr;
}

void PrivateInstanceAAMP::Tune(const char *mainManifestUrl,
								bool autoPlay,
								const char *contentType,
								bool bFirstAttempt,
								bool bFinalAttempt,
								const char *pTraceID,
								bool audioDecoderStreamSync,
								const char *refreshManifestUrl,
								int mpdStitchingMode,
								std::string sid,
								const char *preprocessedManifest
								)

{
	// Set the Fog TSB flag based on the URL.
	mFogTSBEnabled = strcasestr(mainManifestUrl, "tsb?");
}

void PrivateInstanceAAMP::detach()
{
}

void PrivateInstanceAAMP::NotifySpeedChanged(float rate, bool changeState)
{
}

void PrivateInstanceAAMP::LogPlayerPreBuffered(void)
{
}

bool PrivateInstanceAAMP::IsLive()
{
	return mIsLive;
}

void PrivateInstanceAAMP::NotifyOnEnteringLive()
{
	if (g_mockPrivateInstanceAAMP != nullptr)
	{
		g_mockPrivateInstanceAAMP->NotifyOnEnteringLive();
	}
}

bool PrivateInstanceAAMP::GetPauseOnFirstVideoFrameDisp(void)
{
	return false;
}

long long PrivateInstanceAAMP::GetPositionMilliseconds()
{
	long long positionMs = 0;

	if (g_mockPrivateInstanceAAMP != nullptr)
	{
		positionMs = g_mockPrivateInstanceAAMP->GetPositionMilliseconds();
	}

	return positionMs;
}

bool PrivateInstanceAAMP::SetStateBufferingIfRequired()
{
	return false;
}

void PrivateInstanceAAMP::NotifyFirstBufferProcessed(const std::string&)
{
}

void PrivateInstanceAAMP::StopDownloads()
{
	if (g_mockPrivateInstanceAAMP != nullptr)
	{
		g_mockPrivateInstanceAAMP->StopDownloads();
	}
}

void PrivateInstanceAAMP::ResumeDownloads()
{
	if (g_mockPrivateInstanceAAMP != nullptr)
	{
		g_mockPrivateInstanceAAMP->ResumeDownloads();
	}
}

void PrivateInstanceAAMP::EnableDownloads()
{
}

void PrivateInstanceAAMP::AcquireStreamLock()
{
}

void PrivateInstanceAAMP::TuneHelper(TuneType tuneType, bool seekWhilePaused)
{
	if (g_mockPrivateInstanceAAMP != nullptr)
	{
		g_mockPrivateInstanceAAMP->TuneHelper(tuneType, seekWhilePaused);
	}
}

void PrivateInstanceAAMP::ReleaseStreamLock()
{
}

bool PrivateInstanceAAMP::IsFragmentCachingRequired()
{
	return false;
}

void PrivateInstanceAAMP::TeardownStream(bool newTune, bool disableDownloads)
{
}

void PrivateInstanceAAMP::SetVideoRectangle(int x, int y, int w, int h)
{
}

void PrivateInstanceAAMP::SetVideoZoom(VideoZoomMode zoom)
{
}

bool PrivateInstanceAAMP::TryStreamLock()
{
	return false;
}

void PrivateInstanceAAMP::SetVideoMute(bool muted)
{
}

void PrivateInstanceAAMP::SetSubtitleMute(bool muted)
{
}

void PrivateInstanceAAMP::SetAudioVolume(int volume)
{
}

void PrivateInstanceAAMP::AddEventListener(AAMPEventType eventType, EventListener* eventListener)
{
}

void PrivateInstanceAAMP::RemoveEventListener(AAMPEventType eventType, EventListener* eventListener)
{
}

DrmHelperPtr PrivateInstanceAAMP::GetCurrentDRM(void)
{
	return nullptr;
}

void PrivateInstanceAAMP::AddCustomHTTPHeader(std::string headerName, std::vector<std::string> headerValue, bool isLicenseHeader)
{
}

void PrivateInstanceAAMP::SetLiveOffsetAppRequest(bool LiveOffsetAppRequest)
{
}

long long PrivateInstanceAAMP::GetDurationMs()
{
	return 0;
}

long PrivateInstanceAAMP::GetCurrentLatency()
{
	return 0;
}

bool PrivateInstanceAAMP::IsAtLivePoint()
{
	return false;
}

ContentType PrivateInstanceAAMP::GetContentType() const
{
	return ContentType_UNKNOWN;
}

void PrivateInstanceAAMP::SetAlternateContents(const std::string &adBreakId, const std::string &adId, const std::string &url)
{
}

void PrivateInstanceAAMP::GetMoneyTraceString(std::string &customHeader) const
{
}

void SetPreferredLanguages(const char *languageList, const char *preferredRendition, const char *preferredType, const char *codecList, const char *labelList )
{
}

std::string PrivateInstanceAAMP::GetPreferredAudioProperties()
{
	std::string audio_result = "AudioProperties";
	return audio_result;
}

std::string PrivateInstanceAAMP::GetPreferredTextProperties()
{
	std::string result = "TextProperties";
	return result;
}

void PrivateInstanceAAMP::SetPreferredTextLanguages(const char *param )
{
}

DRMSystems PrivateInstanceAAMP::GetPreferredDRM()
{
	return eDRM_NONE;
}

std::string PrivateInstanceAAMP::GetAvailableVideoTracks()
{
	std::string s = "AvailableVideo";
	return s;
}

void PrivateInstanceAAMP::SetVideoTracks(std::vector<BitsPerSecond> bitrateList)
{
}

std::string PrivateInstanceAAMP::GetAudioTrackInfo()
{
	std::string result = "AudioTrack";
	return result;
}

std::string PrivateInstanceAAMP::GetTextTrackInfo()
{
	std::string text_result = "TextTrack";
	return text_result;
}

int PrivateInstanceAAMP::GetTextTrack()
{
	return 0;
}

std::string PrivateInstanceAAMP::GetAvailableTextTracks(bool allTrack)
{
	return "";
}

std::string PrivateInstanceAAMP::GetAvailableAudioTracks(bool allTrack)
{
	if (g_mockPrivateInstanceAAMP != nullptr) {
		return g_mockPrivateInstanceAAMP->GetAvailableAudioTracks(allTrack);
	}else {
		return "";
	}
}

std::string PrivateInstanceAAMP::GetVideoRectangle()
{
	std::string video = "VideoRectangle";
	return video;
}

void PrivateInstanceAAMP::SetAppName(std::string name)
{
}

std::string PrivateInstanceAAMP::GetAppName()
{
	std::string name = "AppName";
	return name;
}

int PrivateInstanceAAMP::GetAudioTrack()
{
	return 0;
}

void PrivateInstanceAAMP::SetCCStatus(bool enabled)
{
}

bool PrivateInstanceAAMP::GetCCStatus(void)
{
	return false;
}

void PrivateInstanceAAMP::SetTextStyle(const std::string &options)
{
}

std::string PrivateInstanceAAMP::GetTextStyle()
{
	std::string result = "TextStyle";
	return result;
}

std::string PrivateInstanceAAMP::GetThumbnailTracks()
{
	std::string result = "ThumbnailTracks";
	return result;
}

std::string PrivateInstanceAAMP::GetThumbnails(double tStart, double tEnd)
{
	std::string result = "Thumbnail";
	return result;
}

void PrivateInstanceAAMP::DisableContentRestrictions(long grace, long time, bool eventChange)
{
}

void PrivateInstanceAAMP::EnableContentRestrictions()
{
}

MediaFormat PrivateInstanceAAMP::GetMediaFormatType(const char *url)
{
	return eMEDIAFORMAT_UNKNOWN;
}

MediaFormat PrivateInstanceAAMP::GetMediaFormatTypeEnum() const
{
	if (g_mockPrivateInstanceAAMP != nullptr) {
		return g_mockPrivateInstanceAAMP->GetMediaFormatTypeEnum();
	} else {
		return eMEDIAFORMAT_UNKNOWN;
	}
}

void PrivateInstanceAAMP::SetEventPriorityAsyncTune(bool bValue)
{
}

bool PrivateInstanceAAMP::IsTuneCompleted()
{
	return false;
}

void PrivateInstanceAAMP::SendWatermarkSessionUpdateEvent(uint32_t sessionHandle, uint32_t status, const std::string &system)
{
	return;
}

void PrivateInstanceAAMP::TuneFail(bool fail)
{
}

std::string PrivateInstanceAAMP::GetPlaybackStats()
{
	std::string result = "playbackstats";
	return result;
}

void PrivateInstanceAAMP::Individualization(const std::string& payload)
{
}

void PrivateInstanceAAMP::SetTextTrack(int trackId, char *data)
{
}

bool PrivateInstanceAAMP::LockGetPositionMilliseconds()
{
	return false;
}

void PrivateInstanceAAMP::UnlockGetPositionMilliseconds()
{
}

void PrivateInstanceAAMP::SetPreferredLanguages(char const*, char const*, char const*, char const*, char const*, const Accessibility*, char const*)
{
}

/**
 * @brief Check if Live Adjust is required for current content. ( For "vod/ivod/ip-dvr/cdvr/eas", Live Adjust is not required ).
 */
bool PrivateInstanceAAMP::IsLiveAdjustRequired()
{
	bool retValue;

	switch (mContentType)
	{
		case ContentType_IVOD:
		case ContentType_VOD:
		case ContentType_CDVR:
		case ContentType_IPDVR:
		case ContentType_EAS:
			retValue = false;
			break;

		case ContentType_SLE:
			retValue = true;
			break;

		default:
			retValue = true;
			break;
	}
	return retValue;
}

void PrivateInstanceAAMP::UpdateLiveOffset()
{
}

void PrivateInstanceAAMP::StoreLanguageList(const std::set<std::string> &langlist)
{
}

bool PrivateInstanceAAMP::DownloadsAreEnabled(void)
{
	bool retVal = false;//mDownloadsEnabled;
	if (g_mockPrivateInstanceAAMP != nullptr)
	{
		retVal = g_mockPrivateInstanceAAMP->DownloadsAreEnabled();
	}
	return retVal;
}

void PrivateInstanceAAMP::SendDownloadErrorEvent(AAMPTuneFailure tuneFailure, int error_code)
{
}

BitsPerSecond PrivateInstanceAAMP::GetMaximumBitrate()
{
    return LONG_MAX;
}

void PrivateInstanceAAMP::UpdateVideoEndProfileResolution(AampMediaType mediaType, BitsPerSecond bitrate, int width, int height)
{
}

BitsPerSecond PrivateInstanceAAMP::GetDefaultBitrate()
{
    return 0;
}

void PrivateInstanceAAMP::UpdateDuration(double seconds)
{
}

void PrivateInstanceAAMP::SendErrorEvent(AAMPTuneFailure tuneFailure, const char * description, bool isRetryEnabled, int32_t secManagerClassCode, int32_t secManagerReasonCode, int32_t secClientBusinessStatus, const std::string &responseData)
{
	if (g_mockPrivateInstanceAAMP != nullptr)
	{
		g_mockPrivateInstanceAAMP->SendErrorEvent(tuneFailure, description, isRetryEnabled, secManagerClassCode, secManagerReasonCode, secClientBusinessStatus, responseData);
	}
}

void PrivateInstanceAAMP::SetCurlTimeout(long timeoutMS, AampCurlInstance instance)
{
}

void PrivateInstanceAAMP::CurlInit(AampCurlInstance startIdx, unsigned int instanceCount, std::string proxyName)
{
}

bool PrivateInstanceAAMP::GetFile(std::string remoteUrl, AampMediaType mediaType, AampGrowableBuffer *buffer, std::string& effectiveUrl,
                int * http_error, double *downloadTime, const char *range, unsigned int curlInstance,
                bool resetBuffer, BitsPerSecond *bitrate, int * fogError,
                double fragmentDurationSeconds, ProfilerBucketType bucketType, int maxInitDownloadTimeMS)
{
	bool rv = true;

	if (g_mockPrivateInstanceAAMP != nullptr)
	{
		rv = g_mockPrivateInstanceAAMP->GetFile(remoteUrl, mediaType, buffer, effectiveUrl,
				 								http_error, downloadTime, range, curlInstance,
												resetBuffer, bitrate, fogError,
												fragmentDurationSeconds, bucketType, maxInitDownloadTimeMS);
	}

	return rv;
}

void PrivateInstanceAAMP::DisableMediaDownloads(AampMediaType type)
{
}

/**
 * @brief Set Content Type
 */
void PrivateInstanceAAMP::SetContentType(const char *cType)
{
	mContentType = ContentType_UNKNOWN; //default unknown
	if(NULL != cType)
	{
		mPlaybackMode = std::string(cType);
		if(mPlaybackMode == "CDVR")
		{
			mContentType = ContentType_CDVR; //cdvr
		}
		else if(mPlaybackMode == "VOD")
		{
			mContentType = ContentType_VOD; //vod
		}
		else if(mPlaybackMode == "LINEAR_TV")
		{
			mContentType = ContentType_LINEAR; //linear
		}
		else if(mPlaybackMode == "IVOD")
		{
			mContentType = ContentType_IVOD; //ivod
		}
		else if(mPlaybackMode == "EAS")
		{
			mContentType = ContentType_EAS; //eas
		}
		else if(mPlaybackMode == "xfinityhome")
		{
			mContentType = ContentType_CAMERA; //camera
		}
		else if(mPlaybackMode == "DVR")
		{
			mContentType = ContentType_DVR; //dvr
		}
		else if(mPlaybackMode == "MDVR")
		{
			mContentType = ContentType_MDVR; //mdvr
		}
		else if(mPlaybackMode == "IPDVR")
		{
			mContentType = ContentType_IPDVR; //ipdvr
		}
		else if(mPlaybackMode == "PPV")
		{
			mContentType = ContentType_PPV; //ppv
		}
		else if(mPlaybackMode == "OTT")
		{
			mContentType = ContentType_OTT; //ott
		}
		else if(mPlaybackMode == "OTA")
		{
			mContentType = ContentType_OTA; //ota
		}
		else if(mPlaybackMode == "HDMI_IN")
		{
			mContentType = ContentType_HDMIIN; //ota
		}
		else if(mPlaybackMode == "COMPOSITE_IN")
		{
			mContentType = ContentType_COMPOSITEIN; //ota
		}
		else if(mPlaybackMode == "SLE")
		{
			mContentType = ContentType_SLE; //single live event
		}
	}
}

void PrivateInstanceAAMP::UpdateVideoEndMetrics(AampMediaType mediaType, BitsPerSecond bitrate, int curlOrHTTPCode, std::string& strUrl, double duration, double curlDownloadTime)
{
}

void PrivateInstanceAAMP::CurlTerm(AampCurlInstance startIdx, unsigned int instanceCount)
{
}

void PrivateInstanceAAMP::DisableDownloads(void)
{
	if (g_mockPrivateInstanceAAMP != nullptr)
	{
		g_mockPrivateInstanceAAMP->DisableDownloads();
	}
}

int PrivateInstanceAAMP::GetInitialBufferDuration()
{
    return 0;
}

BitsPerSecond PrivateInstanceAAMP::GetMinimumBitrate()
{
    return 0;
}

long long PrivateInstanceAAMP::GetPositionMs()
{
	long long positionMs = 0;

	if (g_mockPrivateInstanceAAMP != nullptr)
	{
		positionMs = g_mockPrivateInstanceAAMP->GetPositionMs();
	}

	return positionMs;
}

bool PrivateInstanceAAMP::IsAuxiliaryAudioEnabled(void)
{
    return true;
}

bool PrivateInstanceAAMP::IsPlayEnabled()
{
    return true;
}

bool PrivateInstanceAAMP::IsSubtitleEnabled(void)
{
    return true;
}

void PrivateInstanceAAMP::NotifyAudioTracksChanged()
{
}

void PrivateInstanceAAMP::NotifyFirstFragmentDecrypted()
{
}

void PrivateInstanceAAMP::NotifyTextTracksChanged()
{
}

void PrivateInstanceAAMP::PreCachePlaylistDownloadTask()
{
}

void PrivateInstanceAAMP::ReportBulkTimedMetadata()
{
}

void PrivateInstanceAAMP::ReportTimedMetadata(bool init)
{
}

void PrivateInstanceAAMP::ReportTimedMetadata(long long timeMilliseconds, const char *szName, const char *szContent, int nb, bool bSyncCall, const char *id, double durationMS)
{
}

void PrivateInstanceAAMP::ResetCurrentlyAvailableBandwidth(BitsPerSecond bitsPerSecond , bool trickPlay,int profile)
{
}

void PrivateInstanceAAMP::ResumeTrackInjection(AampMediaType type)
{
}

void PrivateInstanceAAMP::SaveTimedMetadata(long long timeMilliseconds, const char* szName, const char* szContent, int nb, const char* id, double durationMS)
{
}

void PrivateInstanceAAMP::SendEvent(AAMPEventPtr eventData, AAMPEventMode eventMode)
{
}

bool PrivateInstanceAAMP::SendStreamCopy(AampMediaType mediaType, const void *ptr, size_t len, double fpts, double fdts, double fDuration)
{
	if (g_mockPrivateInstanceAAMP != nullptr)
	{
		return g_mockPrivateInstanceAAMP->SendStreamCopy(mediaType, ptr, len, fpts, fdts, fDuration);
	}
	else
	{
		return true;
	}
}

bool PrivateInstanceAAMP::SendTunedEvent(bool isSynchronous)
{
    return true;
}

void PrivateInstanceAAMP::SetPreCacheDownloadList(PreCacheUrlList &dnldListInput)
{
}

void PrivateInstanceAAMP::StopTrackDownloads(AampMediaType type)
{
}

void PrivateInstanceAAMP::StopTrackInjection(AampMediaType type)
{
}

void PrivateInstanceAAMP::SyncBegin(void)
{
}

void PrivateInstanceAAMP::SyncEnd(void)
{
}

void PrivateInstanceAAMP::UpdateCullingState(double culledSecs)
{
}

void PrivateInstanceAAMP::UpdateRefreshPlaylistInterval(float maxIntervalSecs)
{
}

void PrivateInstanceAAMP::UpdateVideoEndMetrics(AampMediaType mediaType, BitsPerSecond bitrate, int curlOrHTTPCode, std::string& strUrl, double duration, double curlDownloadTime, bool keyChanged, bool isEncrypted, ManifestData * manifestData)
{
}

void PrivateInstanceAAMP::UpdateVideoEndMetrics(AAMPAbrInfo & info)
{
}

void PrivateInstanceAAMP::UpdateVideoEndMetrics(AampMediaType mediaType, BitsPerSecond bitrate, int curlOrHTTPCode, std::string& strUrl, double curlDownloadTime, ManifestData * manifestData)
{
}

bool PrivateInstanceAAMP::WebVTTCueListenersRegistered(void)
{
    return true;
}

LangCodePreference PrivateInstanceAAMP::GetLangCodePreference() const
{
    return ISO639_NO_LANGCODE_PREFERENCE;
}

TunedEventConfig PrivateInstanceAAMP::GetTuneEventConfig(bool isLive)
{
    return eTUNED_EVENT_ON_PLAYLIST_INDEXED;
}

std::string PrivateInstanceAAMP::GetNetworkProxy()
{
    std::string s;
    return s;
}

AampCurlInstance PrivateInstanceAAMP::GetPlaylistCurlInstance(AampMediaType type, bool isInitialDownload)
{
    return eCURLINSTANCE_MANIFEST_PLAYLIST_VIDEO;
}

void PrivateInstanceAAMP::BlockUntilGstreamerWantsData(void(*cb)(void), int periodMs, int track)
{
	if (g_mockPrivateInstanceAAMP != nullptr) {
		return g_mockPrivateInstanceAAMP->BlockUntilGstreamerWantsData(cb, periodMs, track);
	}
}

void PrivateInstanceAAMP::CheckForDiscontinuityStall(AampMediaType mediaType)
{
}

bool PrivateInstanceAAMP::Discontinuity(AampMediaType track, bool setDiscontinuityFlag)
{
    return true;
}

bool PrivateInstanceAAMP::DiscontinuitySeenInAllTracks()
{
    return true;
}

bool PrivateInstanceAAMP::DiscontinuitySeenInAnyTracks()
{
    return true;
}

void PrivateInstanceAAMP::EnableMediaDownloads(AampMediaType type)
{
}

void PrivateInstanceAAMP::EndOfStreamReached(AampMediaType mediaType)
{
}

uint32_t  PrivateInstanceAAMP::GetAudTimeScale(void)
{
	if (g_mockPrivateInstanceAAMP != nullptr) {
		return g_mockPrivateInstanceAAMP->GetAudTimeScale();
	}else {
		return 0u;
	}
}

uint32_t  PrivateInstanceAAMP::GetSubTimeScale(void)
{
	return 0u;
}

BitsPerSecond PrivateInstanceAAMP::GetCurrentlyAvailableBandwidth(void)
{
    return 0;
}

BitsPerSecond PrivateInstanceAAMP::GetIframeBitrate()
{
    return 0;
}

BitsPerSecond PrivateInstanceAAMP::GetIframeBitrate4K()
{
    return 0;
}

AampLLDashServiceData*  PrivateInstanceAAMP::GetLLDashServiceData(void)
{
	return &this->mAampLLDashServiceData;
}

uint32_t  PrivateInstanceAAMP::GetVidTimeScale(void)
{
	if (g_mockPrivateInstanceAAMP != nullptr) {
		return g_mockPrivateInstanceAAMP->GetVidTimeScale();
	}else {
		return 0u;
	}
}

void PrivateInstanceAAMP::interruptibleMsSleep(int timeInMs)
{
}

bool PrivateInstanceAAMP::IsDiscontinuityIgnoredForOtherTrack(AampMediaType track)
{
    return true;
}

bool PrivateInstanceAAMP::IsDiscontinuityIgnoredForCurrentTrack(AampMediaType track)
{
    return true;
}

bool PrivateInstanceAAMP::IsDiscontinuityProcessPending()
{
    return true;
}

bool PrivateInstanceAAMP::IsSinkCacheEmpty(AampMediaType mediaType)
{
    return true;
}

void PrivateInstanceAAMP::NotifyBitRateChangeEvent( BitsPerSecond bitrate, BitrateChangeReason reason, int width, int height, double frameRate, double position, bool GetBWIndex, VideoScanType scantype, int aspectRatioWidth, int aspectRatioHeight)
{
}

void PrivateInstanceAAMP::NotifyFragmentCachingComplete()
{
}

void PrivateInstanceAAMP::ResetEOSSignalledFlag()
{
}

void PrivateInstanceAAMP::ResetTrackDiscontinuityIgnoredStatus(void)
{
}

void PrivateInstanceAAMP::ResetTrackDiscontinuityIgnoredStatusForTrack(AampMediaType track)
{
}

void PrivateInstanceAAMP::ScheduleRetune(PlaybackErrorType errorType, AampMediaType trackType, bool bufferFull)
{
}

void PrivateInstanceAAMP::SendStalledErrorEvent()
{
}

void PrivateInstanceAAMP::SendStreamTransfer(AampMediaType mediaType, AampGrowableBuffer* buffer, double fpts, double fdts, double fDuration, double fragmentPTSoffset, bool initFragment, bool discontinuity)
{
	if (g_mockPrivateInstanceAAMP != nullptr)
	{
		return g_mockPrivateInstanceAAMP->SendStreamTransfer(mediaType, buffer, fpts, fdts, fDuration, fragmentPTSoffset, initFragment, discontinuity);
	}
}

void PrivateInstanceAAMP::SetTrackDiscontinuityIgnoredStatus(AampMediaType track)
{
}

void PrivateInstanceAAMP::StopBuffering(bool forceStop)
{
}

bool PrivateInstanceAAMP::TrackDownloadsAreEnabled(AampMediaType type)
{
    return true;
}

void PrivateInstanceAAMP::UnblockWaitForDiscontinuityProcessToComplete(void)
{
}

void PrivateInstanceAAMP::CompleteDiscontinuityDataDeliverForPTSRestamp(AampMediaType type)
{
}

void PrivateInstanceAAMP::SendAnomalyEvent(AAMPAnomalyMessageType type, const char* format, ...)
{
}

void PrivateInstanceAAMP::LoadAampAbrConfig(void)
{
}

void PrivateInstanceAAMP::SetLowLatencyServiceConfigured(bool bConfig)
{
}

void PrivateInstanceAAMP::SetLLDashServiceData(AampLLDashServiceData &stAampLLDashServiceData)
{
	this->mAampLLDashServiceData = stAampLLDashServiceData;
}

bool PrivateInstanceAAMP::GetLowLatencyServiceConfigured()
{
	return false;
}

long long PrivateInstanceAAMP::DurationFromStartOfPlaybackMs(void)
{
	if (g_mockPrivateInstanceAAMP != nullptr)
	{
		return g_mockPrivateInstanceAAMP->DurationFromStartOfPlaybackMs();
	}
	else
	{
		return 0;
	}
}

void PrivateInstanceAAMP::UpdateVideoEndMetrics(double adjustedRate)
{
}

void PrivateInstanceAAMP::SendAdReservationEvent(AAMPEventType type, const std::string &adBreakId, uint64_t position, uint64_t absolutePositionMs, bool immediate)
{
	if (g_mockPrivateInstanceAAMP != nullptr)
	{
		g_mockPrivateInstanceAAMP->SendAdReservationEvent(type, adBreakId, position, absolutePositionMs, immediate);
	}
}

void PrivateInstanceAAMP::SendAdPlacementEvent(AAMPEventType type, const std::string &adId, uint32_t position, uint64_t absolutePositionMs, uint32_t adOffset, uint32_t adDuration, bool immediate, long error_code)
{
	if (g_mockPrivateInstanceAAMP != nullptr)
	{
		g_mockPrivateInstanceAAMP->SendAdPlacementEvent(type, adId, position, absolutePositionMs, adOffset, adDuration, immediate, error_code);
	}
}

bool PrivateInstanceAAMP::IsLiveStream(void)
{
	return mIsLiveStream;
}

void PrivateInstanceAAMP::WaitForDiscontinuityProcessToComplete(void)
{
	if (g_mockPrivateInstanceAAMP != nullptr)
	{
		g_mockPrivateInstanceAAMP->WaitForDiscontinuityProcessToComplete();
	}
}

void PrivateInstanceAAMP::SendSupportedSpeedsChangedEvent(bool isIframeTrackPresent)
{
}

BitsPerSecond PrivateInstanceAAMP::GetDefaultBitrate4K()
{
	return 0;
}

void PrivateInstanceAAMP::SaveNewTimedMetadata(long long timeMS, const char* szName, const char* szContent, int nb, const char* id, double durationMS)
{
	if (g_mockPrivateInstanceAAMP != nullptr)
	{
		g_mockPrivateInstanceAAMP->SaveNewTimedMetadata(timeMS, id, durationMS);
	}
}

void PrivateInstanceAAMP::FoundEventBreak(const std::string &adBreakId, uint64_t startMS, EventBreakInfo brInfo)
{
	if (g_mockPrivateInstanceAAMP != nullptr)
	{
		g_mockPrivateInstanceAAMP->FoundEventBreak(adBreakId, startMS, brInfo);
	}
}

void PrivateInstanceAAMP::SendAdResolvedEvent(const std::string &adId, bool status, uint64_t startMS, uint64_t durationMs, AAMPCDAIError errorCode)
{
	if (g_mockPrivateInstanceAAMP != nullptr)
	{
		g_mockPrivateInstanceAAMP->SendAdResolvedEvent(adId, status, startMS, durationMs,errorCode);
	}
}

void PrivateInstanceAAMP::ReportContentGap(long long timeMS, std::string id, double durationMS)
{
}

void PrivateInstanceAAMP::SendHTTPHeaderResponse()
{
}

void PrivateInstanceAAMP::LoadIDX(ProfilerBucketType bucketType, std::string fragmentUrl, std::string& effectiveUrl, AampGrowableBuffer *fragment, unsigned int curlInstance, const char *range, int * http_code, double *downloadTime, AampMediaType mediaType,int * fogError)
{
        return;
}

bool PrivateInstanceAAMP::IsAudioLanguageSupported (const char *checkLanguage)
{
	return false;
}

void PrivateInstanceAAMP::LicenseRenewal(DrmHelperPtr drmHelper,void* userData)
{
}

bool PrivateInstanceAAMP::IsEventListenerAvailable(AAMPEventType eventType)
{
	return false;
}

void PrivateInstanceAAMP::ID3MetadataHandler(AampMediaType, const uint8_t *, size_t, const SegmentInfo_t &, const char * scheme_uri)
{
}

void PrivateInstanceAAMP::ResetProfileCache()
{
}

struct curl_slist* PrivateInstanceAAMP::GetCustomHeaders(AampMediaType mediaType)
{

       return NULL;
}

void PrivateInstanceAAMP::ResetDiscontinuityInTracks()
{
}

std::shared_ptr<ManifestDownloadConfig> PrivateInstanceAAMP::prepareManifestDownloadConfig()
{
	return NULL;
}

std::string PrivateInstanceAAMP::GetVideoPlaybackQuality()
{
	std::string result = "videoplayback";
        return result;
}

bool PrivateInstanceAAMP::PipelineValid(AampMediaType track)
{
	return true;
}

void PrivateInstanceAAMP::SetStreamFormat(StreamOutputFormat videoFormat, StreamOutputFormat audioFormat, StreamOutputFormat auxFormat)
{
	if (g_mockPrivateInstanceAAMP != nullptr)
	{
		g_mockPrivateInstanceAAMP->SetStreamFormat(videoFormat, audioFormat, auxFormat);
	}
}

void PrivateInstanceAAMP::NotifyFirstVideoPTS(unsigned long long pts, unsigned long timeScale)
{
}

void PrivateInstanceAAMP::NotifyVideoBasePTS(unsigned long long basepts, unsigned long timeScale)
{
}

/**
 * @brief Get Last downloaded manifest for DASH
 * @return last downloaded manifest data
 */
void PrivateInstanceAAMP::GetLastDownloadedManifest(std::string& manifestBuffer)
{
}

void PrivateInstanceAAMP::ProcessID3Metadata(char *segment, size_t size, AampMediaType type, uint64_t timeStampOffset)
{
	if (g_mockPrivateInstanceAAMP != nullptr)
	{
		g_mockPrivateInstanceAAMP->ProcessID3Metadata(segment, size, type, timeStampOffset);
	}
}

void PrivateInstanceAAMP::SetVidTimeScale(uint32_t vidTimeScale)
{
}

void PrivateInstanceAAMP::SetAudTimeScale(uint32_t audTimeScale)
{
}

void PrivateInstanceAAMP::SetSubTimeScale(uint32_t audTimeScale)
{
}

void PrivateInstanceAAMP::SignalTrickModeDiscontinuity()
{
}

/**
 * @brief Resume downloads for a track.
 * Called from StreamSink to control flow
 */
void PrivateInstanceAAMP::ResumeTrackDownloads(AampMediaType)
{
}

void PrivateInstanceAAMP::SetDiscontinuityParam()
{
}

void PrivateInstanceAAMP::SetLatencyParam(double latency, double buff, double rate, double bw)
{
}

void PrivateInstanceAAMP::SetLLDLowBufferParam(double latency, double buff, double rate, double bw, double buffLowCount)
{
}

void PrivateInstanceAAMP::FlushStreamSink(double position, double rate)
{
}

/**
 * @brief to check gstsubtec flag and vttcueventlistener
 */

bool PrivateInstanceAAMP::IsGstreamerSubsEnabled(void)
{
        return false;
}

/**
 * @brief Set Discontinuity handling period change marked flag
 * @param[in] value Period change marked flag
 */
void PrivateInstanceAAMP::SetIsPeriodChangeMarked(bool value)
{
	mIsPeriodChangeMarked = value;
}

/**
 * @brief Get Discontinuity handling period change marked flag
 * @return Period change marked flag
 */
bool PrivateInstanceAAMP::GetIsPeriodChangeMarked()
{
	return mIsPeriodChangeMarked;
}

long long PrivateInstanceAAMP::GetVideoPTS()
{
	return 0;
}

bool PrivateInstanceAAMP::SignalSubtitleClock( void )
{
	return false;
}

int PrivateInstanceAAMP::ScheduleAsyncTask(IdleTask task, void *arg, std::string taskName)
{
	int retval = 0;
	if (g_mockPrivateInstanceAAMP != nullptr)
	{
		retval = g_mockPrivateInstanceAAMP->ScheduleAsyncTask(task, arg, taskName);
	}
	return retval;
}

bool PrivateInstanceAAMP::RemoveAsyncTask(int taskId)
{
	bool retval = false;
	if (g_mockPrivateInstanceAAMP != nullptr)
	{
		retval = g_mockPrivateInstanceAAMP->RemoveAsyncTask(taskId);
	}
	return retval;
}

void PrivateInstanceAAMP::NotifyFirstFrameReceived(unsigned long)
{
}

void PrivateInstanceAAMP::NotifyEOSReached()
{
}

void PrivateInstanceAAMP::ReportProgress(bool sync, bool beginningOfStream)
{
}

void PrivateInstanceAAMP::NotifyFirstVideoFrameDisplayed()
{
}

void PrivateInstanceAAMP::LogFirstFrame(void)
{
}

void PrivateInstanceAAMP::LogTuneComplete(void)
{
}

void PrivateInstanceAAMP::InitializeCC(unsigned long)
{
}

bool PrivateInstanceAAMP::IsFirstVideoFrameDisplayedRequired()
{
	return false;
}

void PrivateInstanceAAMP::UpdateSubtitleTimestamp()
{
}

double PrivateInstanceAAMP::GetFirstPTS()
{
	return 0;
}

double PrivateInstanceAAMP::GetMidSeekPosOffset()
{
	return 0;
}

int PrivateInstanceAAMP::GetCurrentAudioTrackId()
{
	return 0;
}

void PrivateInstanceAAMP::PauseSubtitleParser(bool pause)
{
}

bool PrivateInstanceAAMP::PausePipeline(bool pause, bool forceStopGstreamerPreBuffering)
{
	return false;
}

void PrivateInstanceAAMP::SendBufferChangeEvent(bool bufferingStopped)
{
}

long long PrivateInstanceAAMP::GetPositionRelativeToSeekMilliseconds(long long rate,
																	 long long trickStartUTCMS)
{
	return 0;
}

void PrivateInstanceAAMP::CacheAndApplySubtitleMute(bool muted)
{
}

void PrivateInstanceAAMP::FlushTrack(AampMediaType mediaType,double pos)
{
}

void PrivateInstanceAAMP::ReleaseDynamicDRMToUpdateWait(void)
{
}

std::shared_ptr<TSB::Store> PrivateInstanceAAMP::GetTSBStore(const TSB::Store::Config& config, TSB::LogFunction logger, TSB::LogLevel level)
{
	if (g_mockPrivateInstanceAAMP)
	{
		return g_mockPrivateInstanceAAMP->GetTSBStore(config, logger, level);
	}
	return nullptr;
}

void PrivateInstanceAAMP::SetLocalAAMPTsbInjection(bool value)
{
}

bool PrivateInstanceAAMP::IsLocalAAMPTsbInjection()
{
	if (g_mockPrivateInstanceAAMP)
	{
		return g_mockPrivateInstanceAAMP->IsLocalAAMPTsbInjection();
	}
	return false;
}

void PrivateInstanceAAMP::UpdateLocalAAMPTsbInjection()
{
	if (g_mockPrivateInstanceAAMP)
	{
		g_mockPrivateInstanceAAMP->UpdateLocalAAMPTsbInjection();
	}
}

bool PrivateInstanceAAMP::GetLLDashAdjustSpeed(void)
{
	if (g_mockPrivateInstanceAAMP)
	{
		return g_mockPrivateInstanceAAMP->GetLLDashAdjustSpeed();
	}
	return false;
}

double PrivateInstanceAAMP::GetLLDashCurrentPlayBackRate(void)
{
	if (g_mockPrivateInstanceAAMP)
	{
		return g_mockPrivateInstanceAAMP->GetLLDashCurrentPlayBackRate();
	}
	return 1.0;
}

void PrivateInstanceAAMP::TimedWaitForLatencyCheck(int timeInMs)
{
}

void PrivateInstanceAAMP::WakeupLatencyCheck()
{
}

void PrivateInstanceAAMP::IncreaseGSTBufferSize()
{
}

AampTSBSessionManager *PrivateInstanceAAMP::GetTSBSessionManager()
{
	AampTSBSessionManager *aampTsbSessionManager = nullptr;

	if (g_mockPrivateInstanceAAMP)
	{
		aampTsbSessionManager = g_mockPrivateInstanceAAMP->GetTSBSessionManager();
	}

	return aampTsbSessionManager;
}

std::string PrivateInstanceAAMP::GetLicenseReqProxy()
{
	return "";
}

std::string PrivateInstanceAAMP::GetLicenseCustomData()
{
	return "";
}

void PrivateInstanceAAMP::GetCustomLicenseHeaders(std::unordered_map<std::string, std::vector<std::string>>& customHeaders)
{
}

std::string PrivateInstanceAAMP::GetLicenseServerUrlForDrm(DRMSystems type)
{
    return "";
}

bool PrivateInstanceAAMP::ReconfigureForCodecChange()
{
	return false;
}

std::string PrivateInstanceAAMP::SendManifestPreProcessEvent()
{
	std::string  bRetManifestData;
	if(!mProvidedManifestFile.empty())
	{
		bRetManifestData = std::move(mProvidedManifestFile);
	}
	return bRetManifestData;
}

void PrivateInstanceAAMP::updateManifest(const char *manifestData)
{
	if(NULL != manifestData)
		mProvidedManifestFile = manifestData;
}


void PrivateInstanceAAMP::SetPauseOnStartPlayback(bool enable)
{
	if (g_mockPrivateInstanceAAMP)
	{
		g_mockPrivateInstanceAAMP->SetPauseOnStartPlayback(enable);
	}
}

bool PrivateInstanceAAMP::isDecryptClearSamplesRequired()
{
	bool bIsDecryptClearSamplesRequired = false;
	if (g_mockPrivateInstanceAAMP)
	{
		bIsDecryptClearSamplesRequired = g_mockPrivateInstanceAAMP->isDecryptClearSamplesRequired();
	}
	return bIsDecryptClearSamplesRequired;
}

void PrivateInstanceAAMP::ResetTrickStartUTCTime()
{
}

void PrivateInstanceAAMP::SetLLDashChunkMode(bool enable)
{
	if (g_mockPrivateInstanceAAMP)
	{
		g_mockPrivateInstanceAAMP->SetLLDashChunkMode(enable);
	}
}

bool PrivateInstanceAAMP::GetLLDashChunkMode()
{
	bool bIsChunkMode = false;
	if (g_mockPrivateInstanceAAMP)
	{
		bIsChunkMode = g_mockPrivateInstanceAAMP->GetLLDashChunkMode();
	}
	return bIsChunkMode;
}

const char* PrivateInstanceAAMP::getStringForPlaybackError(PlaybackErrorType errorType)
{
	return "";
}

unsigned char* PrivateInstanceAAMP::ReplaceKeyIDPsshData(const unsigned char *InputData, const size_t InputDataLength,  size_t & OutputDataLength) {

	return NULL;
}

void PrivateInstanceAAMP::SendBlockedEvent(const std::string & reason, const std::string currentLocator)
{

}
void PrivateInstanceAAMP::GetPlayerVideoSize(int &width, int &height)
{
}

void PrivateInstanceAAMP::SendVTTCueDataAsEvent(VTTCue* cue)
{
}

void PrivateInstanceAAMP::UpdateCCTrackInfo(const std::vector<TextTrackInfo>& textTracksCopy, std::vector<CCTrackInfo>& updatedTextTracks)
{
}

void PrivateInstanceAAMP::CalculateTrickModePositionEOS(void)
{
	if (g_mockPrivateInstanceAAMP)
	{
		g_mockPrivateInstanceAAMP->CalculateTrickModePositionEOS();
	}
}

double PrivateInstanceAAMP::GetLivePlayPosition(void)
{
	double livePlayPosition = 0.0;
	if (g_mockPrivateInstanceAAMP)
	{
		livePlayPosition = g_mockPrivateInstanceAAMP->GetLivePlayPosition();
	}
	return livePlayPosition;
}

void PrivateInstanceAAMP::IncrementGaps()
{
}

double PrivateInstanceAAMP::GetStreamPositionMs()
{
	return 0.0;
}

void PrivateInstanceAAMP::SendMonitorAvEvent(const std::string &status, int64_t videoPositionMS, int64_t audioPositionMS, uint64_t timeInStateMS, uint64_t droppedFrames)
{
}
double PrivateInstanceAAMP::GetFormatPositionOffsetInMSecs()
{
	return 0;
}

const std::vector<TimedMetadata> & PrivateInstanceAAMP::GetTimedMetadata( void ) const
{
	static std::vector<TimedMetadata> rc;
	return rc;
}

