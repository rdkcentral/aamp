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
#include <iostream>
#include <iomanip>
#include <algorithm>
#include <unordered_map>
#include <memory>
#include "DrmUtils.h"
#include "AampConfig.h"
#include "main_aamp.h"
#include "aampgstplayer.h"
#include "MockPlayerInstanceAAMP.h"

MockPlayerInstanceAAMP *g_mockPlayerInstanceAAMP = nullptr;

std::shared_ptr<AampConfig> gGlobalConfig;
AampConfig *gpGlobalConfig;

static std::unordered_map<std::string, std::vector<std::string>> fCustomHeaders;

void MockAampReset(void)
{
	gGlobalConfig = std::make_shared<AampConfig>();
    gpGlobalConfig = gGlobalConfig.get();
}

PlayerInstanceAAMP::PlayerInstanceAAMP( StreamSink* streamSink, std::function< void(const unsigned char *, int, int, int) > exportFrames )
{
}

PlayerInstanceAAMP::~PlayerInstanceAAMP()
{
}

void PlayerInstanceAAMP::GetCustomLicenseHeaders(
	std::unordered_map<std::string, std::vector<std::string>> &customHeaders)
{
	customHeaders = fCustomHeaders;
}

void PlayerInstanceAAMP::SendDrmErrorEvent(DrmMetaDataEventPtr event, bool isRetryEnabled)
{
}

void PlayerInstanceAAMP::SendDRMMetaData(DrmMetaDataEventPtr e)
{
}

void PlayerInstanceAAMP::Individualization(const std::string &payload)
{
	if (g_mockPlayerInstanceAAMP != nullptr)
	{
		g_mockPlayerInstanceAAMP->Individualization(payload);
	}
}

void PlayerInstanceAAMP::SendEvent(AAMPEventPtr eventData, AAMPEventMode eventMode)
{
}

void PlayerInstanceAAMP::SetState(AAMPPlayerState state)
{
}

std::string PlayerInstanceAAMP::GetLicenseReqProxy()
{
	return std::string();
}

void PlayerInstanceAAMP::SendErrorEvent(AAMPTuneFailure tuneFailure, const char * description, bool isRetryEnabled, int32_t secManagerClassCode, int32_t secManagerReasonCode, int32_t secClientBusinessStatus, const std::string &responseData)
{
}

std::string PlayerInstanceAAMP::GetLicenseServerUrlForDrm(DRMSystems type)
{
	std::string url;
	if (type == eDRM_PlayReady)
	{
		url = GETCONFIGVALUE_PRIV(eAAMPConfig_PRLicenseServerUrl);
	}
	else if (type == eDRM_WideVine)
	{
		url = GETCONFIGVALUE_PRIV(eAAMPConfig_WVLicenseServerUrl);
	}
	else if (type == eDRM_ClearKey)
	{
		url = GETCONFIGVALUE_PRIV(eAAMPConfig_CKLicenseServerUrl);
	}

	if (url.empty())
	{
		url = GETCONFIGVALUE_PRIV(eAAMPConfig_LicenseServerUrl);
	}
	return url;
}

std::string PlayerInstanceAAMP::GetLicenseCustomData()
{
	return std::string();
}

bool PlayerInstanceAAMP::IsEventListenerAvailable(AAMPEventType eventType)
{
	return false;
}

std::string PlayerInstanceAAMP::GetAppName()
{
	return std::string();
}

int PlayerInstanceAAMP::HandleSSLProgressCallback(void *clientp, double dltotal, double dlnow,
												   double ultotal, double ulnow)
{
	return 0;
}

size_t PlayerInstanceAAMP::HandleSSLHeaderCallback(const char *ptr, size_t size, size_t nmemb,
													void *userdata)
{
	return 0;
}

size_t PlayerInstanceAAMP::HandleSSLWriteCallback(char *ptr, size_t size, size_t nmemb,
												   void *userdata)
{
	return 0;
}

bool PlayerInstanceAAMP::isDecryptClearSamplesRequired()
{
	bool bIsDecryptClearSamplesRequired = false;
	if (g_mockPlayerInstanceAAMP)
	{
		bIsDecryptClearSamplesRequired = g_mockPlayerInstanceAAMP->isDecryptClearSamplesRequired();
	}
	return bIsDecryptClearSamplesRequired;
}

void PlayerInstanceAAMP::GetMoneyTraceString(std::string &customHeader) const
{
}

bool AAMPGstPlayer::IsCodecSupported(const std::string &codecName)
{
	return true;
}

static const char *mLogLevelStr[eLOGLEVEL_ERROR+1] =
{
	"TRACE", // eLOGLEVEL_TRACE
	"DEBUG", // eLOGLEVEL_DEBUG
	"INFO",  // eLOGLEVEL_INFO
	"WARN",  // eLOGLEVEL_WARN
	"MIL",   // eLOGLEVEL_MIL
	"ERROR", // eLOGLEVEL_ERROR
};

bool AampLogManager::disableLogRedirection = false;
bool AampLogManager::enableEthanLogRedirection = false;
AAMP_LogLevel AampLogManager::aampLoglevel = eLOGLEVEL_WARN;
bool AampLogManager::locked = false;

void logprintf(AAMP_LogLevel level, const char *file, int line, const char *format,
			   ...)
{
	int playerId = -1;
	va_list args;
	va_start(args, format);
	char fmt[512];
	snprintf(
			 fmt, sizeof(fmt),
			 "[AAMP-PLAYER][%d][%s][%s][%d]%s\n",
			 playerId,
			 mLogLevelStr[level],
			 file,
			 line,
			 format );
	vprintf(fmt, args);
	va_end(args);
}

void DumpBlob(const unsigned char *ptr, size_t len)
{
}

void PlayerInstanceAAMP::UpdateUseSinglePipeline(void)
{
}

void PlayerInstanceAAMP::UpdateMaxDRMSessions(void)
{
}

void PlayerInstanceAAMP::ActivatePlayer()
{
}

void PlayerInstanceAAMP::SendMediaMetadataEvent()
{
}

void PlayerInstanceAAMP::_Stop( bool isDestructing )
{
}

void PlayerInstanceAAMP::_SetAudioTrack(int)
{
}

bool PlayerInstanceAAMP::IsActiveInstancePresent()
{
	return true;
}

AampCacheHandler *PlayerInstanceAAMP::getAampCacheHandler()
{
	return nullptr;
}

void PlayerInstanceAAMP::Tune(const char *mainManifestUrl, bool autoPlay, const char *contentType,
							   bool bFirstAttempt, bool bFinalAttempt, const char *pTraceID,
							   bool audioDecoderStreamSync, const char *refreshManifestUrl,
							   int mpdStitchingMode, std::string sid,const char *preprocessedManifest)

{
	// Set the Fog TSB flag based on the URL.
	mFogTSBEnabled = strcasestr(mainManifestUrl, "tsb?");
}

void PlayerInstanceAAMP::detach()
{
}

void PlayerInstanceAAMP::NotifySpeedChanged(float rate, bool changeState)
{
}

void PlayerInstanceAAMP::LogPlayerPreBuffered(void)
{
}

bool PlayerInstanceAAMP::IsLive()
{
	return mIsLive;
}

void PlayerInstanceAAMP::NotifyOnEnteringLive()
{
}

bool PlayerInstanceAAMP::GetPauseOnFirstVideoFrameDisp(void)
{
	return false;
}

long long PlayerInstanceAAMP::GetPositionMilliseconds()
{
	return 0;
}

bool PlayerInstanceAAMP::SetStateBufferingIfRequired()
{
	return false;
}

void PlayerInstanceAAMP::NotifyFirstBufferProcessed(const std::string&)
{
}

void PlayerInstanceAAMP::StopDownloads()
{
}

void PlayerInstanceAAMP::ResumeDownloads()
{
}

void PlayerInstanceAAMP::EnableDownloads()
{
}

void PlayerInstanceAAMP::AcquireStreamLock()
{
}

void PlayerInstanceAAMP::TuneHelper(TuneType tuneType, bool seekWhilePaused)
{
}

void PlayerInstanceAAMP::ReleaseStreamLock()
{
}

bool PlayerInstanceAAMP::IsFragmentCachingRequired()
{
	return false;
}

void PlayerInstanceAAMP::TeardownStream(bool newTune, bool disableDownloads)
{
}

void PlayerInstanceAAMP::SetVideoRectangle(int x, int y, int w, int h)
{
}

void PlayerInstanceAAMP::SetVideoZoom(VideoZoomMode zoom)
{
}

bool PlayerInstanceAAMP::TryStreamLock()
{
	return false;
}

void PlayerInstanceAAMP::SetVideoMute(bool muted)
{
}

void PlayerInstanceAAMP::SetSubtitleMute(bool muted)
{
}

void PlayerInstanceAAMP::SetAudioVolume(int volume)
{
}

void PlayerInstanceAAMP::AddEventListener(AAMPEventType eventType, EventListener *eventListener)
{
}

void PlayerInstanceAAMP::RemoveEventListener(AAMPEventType eventType, EventListener *eventListener)
{
}

DrmHelperPtr PlayerInstanceAAMP::GetCurrentDRM(void)
{
	return nullptr;
}

void PlayerInstanceAAMP::AddCustomHTTPHeader(std::string headerName,
											  std::vector<std::string> headerValue,
											  bool isLicenseHeader)
{
}

void PlayerInstanceAAMP::SetLiveOffsetAppRequest(bool LiveOffsetAppRequest)
{
}

long long PlayerInstanceAAMP::GetDurationMs()
{
	return 0;
}

long PlayerInstanceAAMP::GetCurrentLatency()
{
	return 0;
}

bool PlayerInstanceAAMP::IsAtLivePoint()
{
	return false;
}

ContentType PlayerInstanceAAMP::GetContentType() const
{
	return ContentType_UNKNOWN;
}

void PlayerInstanceAAMP::SetAlternateContents(const std::string &adBreakId,
											   const std::string &adId, const std::string &url)
{
}

void SetPreferredLanguages(const char *languageList, const char *preferredRendition,
						   const char *preferredType, const char *codecList, const char *labelList)
{
}

std::string PlayerInstanceAAMP::GetPreferredAudioProperties()
{
	std::string audio_result = "AudioProperties";
	return audio_result;
}

std::string PlayerInstanceAAMP::GetPreferredTextProperties()
{
	std::string result = "TextProperties";
	return result;
}

void PlayerInstanceAAMP::SetPreferredTextLanguages(const char *param)
{
}

DRMSystems PlayerInstanceAAMP::GetPreferredDRM()
{
	return eDRM_NONE;
}

std::string PlayerInstanceAAMP::GetAvailableVideoTracks()
{
	std::string s = "AvailableVideo";
	return s;
}

void PlayerInstanceAAMP::SetVideoTracks(std::vector<BitsPerSecond> bitrateList)
{
}

std::string PlayerInstanceAAMP::GetAudioTrackInfo()
{
	std::string result = "AudioTrack";
	return result;
}

std::string PlayerInstanceAAMP::GetTextTrackInfo()
{
	std::string text_result = "TextTrack";
	return text_result;
}

int PlayerInstanceAAMP::GetTextTrack()
{
	return 0;
}

std::string PlayerInstanceAAMP::GetAvailableTextTracks(bool allTrack)
{
	return "";
}

std::string PlayerInstanceAAMP::GetVideoRectangle()
{
	std::string video = "videorectangel";
	return video;
}

void PlayerInstanceAAMP::SetAppName(std::string name)
{
}

int PlayerInstanceAAMP::GetAudioTrack()
{
	return 0;
}

void PlayerInstanceAAMP::SetCCStatus(bool enabled)
{
}

bool PlayerInstanceAAMP::GetCCStatus(void)
{
	return false;
}

void PlayerInstanceAAMP::SetTextStyle(const std::string &options)
{
}

std::string PlayerInstanceAAMP::GetTextStyle()
{
	std::string result = "sampleStyle";
	return result;
}

std::string PlayerInstanceAAMP::GetThumbnailTracks()
{
	std::string result = "ThumbnailTracks";
	return result;
}

std::string PlayerInstanceAAMP::GetThumbnails(double tStart, double tEnd)
{
	std::string result = "Thumbnail";
	return result;
}

void PlayerInstanceAAMP::DisableContentRestrictions(long grace, long time, bool eventChange)
{
}

void PlayerInstanceAAMP::EnableContentRestrictions()
{
}

MediaFormat PlayerInstanceAAMP::GetMediaFormatType(const char *url)
{
	return eMEDIAFORMAT_UNKNOWN;
}

void PlayerInstanceAAMP::SetEventPriorityAsyncTune(bool bValue)
{
}

bool PlayerInstanceAAMP::IsTuneCompleted()
{
	return false;
}

void PlayerInstanceAAMP::SendWatermarkSessionUpdateEvent(uint32_t sessionHandle, uint32_t status, const std::string &system)
{
	return;
}

void PlayerInstanceAAMP::TuneFail(bool fail)
{
}

std::string PlayerInstanceAAMP::GetPlaybackStats()
{
	std::string result = "playbackstats";
	return result;
}

void PlayerInstanceAAMP::SetTextTrack(int trackId, char *data)
{
}

bool PlayerInstanceAAMP::LockGetPositionMilliseconds()
{
	return false;
}

void PlayerInstanceAAMP::UnlockGetPositionMilliseconds()
{
}

void PlayerInstanceAAMP::SetPreferredLanguages(const char *, const char *, const char *,
												const char *, const char *, const Accessibility *,
												const char *)
{
}

/**
 * @brief Check if Live Adjust is required for current content. ( For "vod/ivod/ip-dvr/cdvr/eas",
 * Live Adjust is not required ).
 */
bool PlayerInstanceAAMP::IsLiveAdjustRequired()
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

void PlayerInstanceAAMP::UpdateLiveOffset()
{
}

void PlayerInstanceAAMP::StoreLanguageList(const std::set<std::string> &langlist)
{
}

bool PlayerInstanceAAMP::DownloadsAreEnabled(void)
{
	return true;
}

void PlayerInstanceAAMP::SendDownloadErrorEvent(AAMPTuneFailure tuneFailure, int error_code)
{
}

BitsPerSecond PlayerInstanceAAMP::GetMaximumBitrate()
{
	return LONG_MAX;
}

void PlayerInstanceAAMP::UpdateVideoEndProfileResolution(AampMediaType mediaType,
														  BitsPerSecond bitrate, int width,
														  int height)
{
}

BitsPerSecond PlayerInstanceAAMP::GetDefaultBitrate()
{
	return 0;
}

void PlayerInstanceAAMP::UpdateDuration(double seconds)
{
}

void PlayerInstanceAAMP::SetCurlTimeout(long timeoutMS, AampCurlInstance instance)
{
}

void PlayerInstanceAAMP::CurlInit(AampCurlInstance startIdx, unsigned int instanceCount,
								   std::string proxyName)
{
}

void PlayerInstanceAAMP::DisableMediaDownloads(AampMediaType type)
{
}

/**
 * @brief Set Content Type
 */
void PlayerInstanceAAMP::SetContentType(const char *cType)
{
	mContentType = ContentType_UNKNOWN; // default unknown
	if (nullptr != cType)
	{
		mPlaybackMode = std::string(cType);
		if (mPlaybackMode == "CDVR")
		{
			mContentType = ContentType_CDVR; // cdvr
		}
		else if (mPlaybackMode == "VOD")
		{
			mContentType = ContentType_VOD; // vod
		}
		else if (mPlaybackMode == "LINEAR_TV")
		{
			mContentType = ContentType_LINEAR; // linear
		}
		else if (mPlaybackMode == "IVOD")
		{
			mContentType = ContentType_IVOD; // ivod
		}
		else if (mPlaybackMode == "EAS")
		{
			mContentType = ContentType_EAS; // eas
		}
		else if (mPlaybackMode == "xfinityhome")
		{
			mContentType = ContentType_CAMERA; // camera
		}
		else if (mPlaybackMode == "DVR")
		{
			mContentType = ContentType_DVR; // dvr
		}
		else if (mPlaybackMode == "MDVR")
		{
			mContentType = ContentType_MDVR; // mdvr
		}
		else if (mPlaybackMode == "IPDVR")
		{
			mContentType = ContentType_IPDVR; // ipdvr
		}
		else if (mPlaybackMode == "PPV")
		{
			mContentType = ContentType_PPV; // ppv
		}
		else if (mPlaybackMode == "OTT")
		{
			mContentType = ContentType_OTT; // ott
		}
		else if (mPlaybackMode == "OTA")
		{
			mContentType = ContentType_OTA; // ota
		}
		else if (mPlaybackMode == "HDMI_IN")
		{
			mContentType = ContentType_HDMIIN; // ota
		}
		else if (mPlaybackMode == "COMPOSITE_IN")
		{
			mContentType = ContentType_COMPOSITEIN; // ota
		}
		else if (mPlaybackMode == "SLE")
		{
			mContentType = ContentType_SLE; // single live event
		}
	}
}

void PlayerInstanceAAMP::UpdateVideoEndMetrics(AampMediaType mediaType, BitsPerSecond bitrate,
												int curlOrHTTPCode, std::string &strUrl,
												double duration, double curlDownloadTime)
{
}

void PlayerInstanceAAMP::CurlTerm(AampCurlInstance startIdx, unsigned int instanceCount)
{
}

void PlayerInstanceAAMP::DisableDownloads(void)
{
}

int PlayerInstanceAAMP::GetInitialBufferDuration()
{
	return 0;
}

BitsPerSecond PlayerInstanceAAMP::GetMinimumBitrate()
{
	return 0;
}

bool PlayerInstanceAAMP::IsAuxiliaryAudioEnabled(void)
{
	return true;
}

bool PlayerInstanceAAMP::IsPlayEnabled()
{
	return true;
}

bool PlayerInstanceAAMP::IsSubtitleEnabled(void)
{
	return true;
}

void PlayerInstanceAAMP::NotifyAudioTracksChanged()
{
}

void PlayerInstanceAAMP::NotifyFirstFragmentDecrypted()
{
}

void PlayerInstanceAAMP::NotifyTextTracksChanged()
{
}

void PlayerInstanceAAMP::PreCachePlaylistDownloadTask()
{
}

void PlayerInstanceAAMP::ReportBulkTimedMetadata()
{
}

void PlayerInstanceAAMP::ReportTimedMetadata(bool init)
{
}

void PlayerInstanceAAMP::ReportTimedMetadata(long long timeMilliseconds, const char *szName,
											  const char *szContent, int nb, bool bSyncCall,
											  const char *id, double durationMS)
{
}

void PlayerInstanceAAMP::ResetCurrentlyAvailableBandwidth(BitsPerSecond bitsPerSecond,
														   bool trickPlay, int profile)
{
}

void PlayerInstanceAAMP::ResumeTrackInjection(AampMediaType type)
{
}

void PlayerInstanceAAMP::SaveTimedMetadata(long long timeMilliseconds, const char *szName,
											const char *szContent, int nb, const char *id,
											double durationMS)
{
}

bool PlayerInstanceAAMP::SendStreamCopy(AampMediaType mediaType, const void *ptr, size_t len,
										 double fpts, double fdts, double fDuration)
{
	return true;
}

bool PlayerInstanceAAMP::SendTunedEvent(bool isSynchronous)
{
	return true;
}

void PlayerInstanceAAMP::SetPreCacheDownloadList(PreCacheUrlList &dnldListInput)
{
}

void PlayerInstanceAAMP::StopTrackDownloads(AampMediaType type)
{
}

void PlayerInstanceAAMP::StopTrackInjection(AampMediaType type)
{
}

void PlayerInstanceAAMP::SyncBegin(void)
{
}

void PlayerInstanceAAMP::SyncEnd(void)
{
}

void PlayerInstanceAAMP::UpdateCullingState(double culledSecs)
{
}

void PlayerInstanceAAMP::UpdateRefreshPlaylistInterval(float maxIntervalSecs)
{
}

void PlayerInstanceAAMP::UpdateVideoEndMetrics(AampMediaType mediaType, BitsPerSecond bitrate,
												int curlOrHTTPCode, std::string &strUrl,
												double duration, double curlDownloadTime,
												bool keyChanged, bool isEncrypted,
												ManifestData *manifestData)
{
}

void PlayerInstanceAAMP::UpdateVideoEndMetrics(AAMPAbrInfo &info)
{
}

void PlayerInstanceAAMP::UpdateVideoEndMetrics(AampMediaType mediaType, BitsPerSecond bitrate,
												int curlOrHTTPCode, std::string &strUrl,
												double curlDownloadTime, ManifestData *manifestData)
{
}

bool PlayerInstanceAAMP::WebVTTCueListenersRegistered(void)
{
	return true;
}

LangCodePreference PlayerInstanceAAMP::GetLangCodePreference() const
{
	return ISO639_NO_LANGCODE_PREFERENCE;
}

TunedEventConfig PlayerInstanceAAMP::GetTuneEventConfig(bool isLive)
{
	return eTUNED_EVENT_ON_PLAYLIST_INDEXED;
}

std::string PlayerInstanceAAMP::GetNetworkProxy()
{
	std::string s;
	return s;
}

AampCurlInstance PlayerInstanceAAMP::GetPlaylistCurlInstance(AampMediaType type,
															  bool isInitialDownload)
{
	return eCURLINSTANCE_MANIFEST_PLAYLIST_VIDEO;
}

void PlayerInstanceAAMP::BlockUntilGstreamerWantsData(void (*cb)(void), int periodMs, int track)
{
}

void PlayerInstanceAAMP::CheckForDiscontinuityStall(AampMediaType mediaType)
{
}

bool PlayerInstanceAAMP::Discontinuity(AampMediaType track, bool setDiscontinuityFlag)
{
	return true;
}

bool PlayerInstanceAAMP::DiscontinuitySeenInAllTracks()
{
	return true;
}

bool PlayerInstanceAAMP::DiscontinuitySeenInAnyTracks()
{
	return true;
}

void PlayerInstanceAAMP::EnableMediaDownloads(AampMediaType type)
{
}

void PlayerInstanceAAMP::EndOfStreamReached(AampMediaType mediaType)
{
}

uint32_t PlayerInstanceAAMP::GetAudTimeScale(void)
{
	return 0u;
}

uint32_t PlayerInstanceAAMP::GetSubTimeScale(void)
{
	return 0u;
}

BitsPerSecond PlayerInstanceAAMP::GetCurrentlyAvailableBandwidth(void)
{
	return 0;
}

BitsPerSecond PlayerInstanceAAMP::GetIframeBitrate()
{
	return 0;
}

BitsPerSecond PlayerInstanceAAMP::GetIframeBitrate4K()
{
	return 0;
}

AampLLDashServiceData *PlayerInstanceAAMP::GetLLDashServiceData(void)
{
	return &this->mAampLLDashServiceData;
}

uint32_t PlayerInstanceAAMP::GetVidTimeScale(void)
{
	return 0u;
}

void PlayerInstanceAAMP::interruptibleMsSleep(int timeInMs)
{
}

bool PlayerInstanceAAMP::IsDiscontinuityIgnoredForOtherTrack(AampMediaType track)
{
	return true;
}

bool PlayerInstanceAAMP::IsDiscontinuityIgnoredForCurrentTrack(AampMediaType track)
{
	return true;
}

bool PlayerInstanceAAMP::IsDiscontinuityProcessPending()
{
	return true;
}

bool PlayerInstanceAAMP::IsSinkCacheEmpty(AampMediaType mediaType)
{
	return true;
}

void PlayerInstanceAAMP::NotifyBitRateChangeEvent(BitsPerSecond bitrate,
												   BitrateChangeReason reason, int width,
												   int height, double frameRate, double position,
												   bool GetBWIndex, VideoScanType scantype,
												   int aspectRatioWidth, int aspectRatioHeight)
{
}

void PlayerInstanceAAMP::NotifyFragmentCachingComplete()
{
}

void PlayerInstanceAAMP::ResetEOSSignalledFlag()
{
}

void PlayerInstanceAAMP::ResetTrackDiscontinuityIgnoredStatus(void)
{
}

void PlayerInstanceAAMP::ResetTrackDiscontinuityIgnoredStatusForTrack(AampMediaType track)
{
}

void PlayerInstanceAAMP::ScheduleRetune(PlaybackErrorType errorType, AampMediaType trackType, bool bufferFull )
{
}

void PlayerInstanceAAMP::SendStalledErrorEvent()
{
}

void PlayerInstanceAAMP::SetTrackDiscontinuityIgnoredStatus(AampMediaType track)
{
}

void PlayerInstanceAAMP::StopBuffering(bool forceStop)
{
}

bool PlayerInstanceAAMP::TrackDownloadsAreEnabled(AampMediaType type)
{
	return true;
}

void PlayerInstanceAAMP::UnblockWaitForDiscontinuityProcessToComplete(void)
{
}

void PlayerInstanceAAMP::CompleteDiscontinuityDataDeliverForPTSRestamp(AampMediaType type)
{
}

void PlayerInstanceAAMP::SendAnomalyEvent(AAMPAnomalyMessageType type, const char *format, ...)
{
}

void PlayerInstanceAAMP::LoadAampAbrConfig(void)
{
}

void PlayerInstanceAAMP::SetLowLatencyServiceConfigured(bool bConfig)
{
}

void PlayerInstanceAAMP::SetLLDashServiceData(AampLLDashServiceData &stAampLLDashServiceData)
{
	this->mAampLLDashServiceData = stAampLLDashServiceData;
}

bool PlayerInstanceAAMP::GetLowLatencyServiceConfigured()
{
	return false;
}

long long PlayerInstanceAAMP::DurationFromStartOfPlaybackMs(void)
{
	return 0;
}

void PlayerInstanceAAMP::UpdateVideoEndMetrics(double adjustedRate)
{
}

void PlayerInstanceAAMP::SendAdReservationEvent(AAMPEventType type, const std::string &adBreakId,
												 uint64_t position, uint64_t absolutePositionMs, bool immediate)
{
}

void PlayerInstanceAAMP::SendAdPlacementEvent(AAMPEventType type, const std::string &adId,
											   uint32_t position, uint64_t absolutePositionMs, uint32_t adOffset,
											   uint32_t adDuration, bool immediate, long error_code)
{
}

bool PlayerInstanceAAMP::IsLiveStream(void)
{
	return mIsLiveStream;
}

void PlayerInstanceAAMP::WaitForDiscontinuityProcessToComplete(void)
{
}

void PlayerInstanceAAMP::SendSupportedSpeedsChangedEvent(bool isIframeTrackPresent)
{
}

BitsPerSecond PlayerInstanceAAMP::GetDefaultBitrate4K()
{
	return 0;
}

void PlayerInstanceAAMPSaveNewTimedMetadata(long long timeMS, const char *szName,
											   const char *szContent, int nb, const char *id,
											   double durationMS)
{
}

void PlayerInstanceAAMP::FoundEventBreak(const std::string &adBreakId, uint64_t startMS,
										  EventBreakInfo brInfo)
{
}

void PlayerInstanceAAMP::SendAdResolvedEvent(const std::string &adId, bool status,
											  uint64_t startMS, uint64_t durationMs)
{
}

void PlayerInstanceAAMP::ReportContentGap(long long timeMS, std::string id, double durationMS)
{
}

void PlayerInstanceAAMP::SendHTTPHeaderResponse()
{
}

void PlayerInstanceAAMP::LoadIDX(ProfilerBucketType bucketType, std::string fragmentUrl,
								  std::string &effectiveUrl, AampGrowableBuffer *fragment,
								  unsigned int curlInstance, const char *range, int *http_code,
								  double *downloadTime, AampMediaType fileType, int *fogError)
{
	return;
}

void PlayerInstanceAAMP::LicenseRenewal(DrmHelperPtr drmHelper, void *userData)
{
}

void PlayerInstanceAAMP::ID3MetadataHandler(AampMediaType, const uint8_t *, size_t,
											 const SegmentInfo_t &, const char *scheme_uri)
{
}

void PlayerInstanceAAMP::ResetProfileCache()
{
}

struct curl_slist *PlayerInstanceAAMP::GetCustomHeaders(AampMediaType fileType)
{

	return nullptr;
}

void PlayerInstanceAAMP::ResetDiscontinuityInTracks()
{
}

std::shared_ptr<ManifestDownloadConfig> PlayerInstanceAAMP::prepareManifestDownloadConfig()
{
	return nullptr;
}

std::string PlayerInstanceAAMP::GetVideoPlaybackQuality()
{
	std::string result = "videoplayback";
	return result;
}

bool PlayerInstanceAAMP::PipelineValid(AampMediaType track)
{
	return true;
}

void PlayerInstanceAAMP::NotifyFirstVideoPTS(unsigned long long pts, unsigned long timeScale)
{
}

void PlayerInstanceAAMP::NotifyVideoBasePTS(unsigned long long basepts, unsigned long timeScale)
{
}

/**
 * @brief Get Last downloaded manifest for DASH
 * @return last downloaded manifest data
 */
void PlayerInstanceAAMP::GetLastDownloadedManifest(std::string &manifestBuffer)
{
}

void PlayerInstanceAAMP::ProcessID3Metadata(char *segment, size_t size, AampMediaType type,
											 uint64_t timeStampOffset)
{
}

void PlayerInstanceAAMP::SetVidTimeScale(uint32_t vidTimeScale)
{
}

void PlayerInstanceAAMP::SetAudTimeScale(uint32_t audTimeScale)
{
}

void PlayerInstanceAAMP::SignalTrickModeDiscontinuity()
{
}

/**
 * @brief Resume downloads for a track.
 * Called from StreamSink to control flow
 */
void PlayerInstanceAAMP::ResumeTrackDownloads(AampMediaType)
{
}

void PlayerInstanceAAMP::SetDiscontinuityParam()
{
}

void PlayerInstanceAAMP::SetLatencyParam(double latency, double buffer, double playbackRate, double bw)
{
}

void PlayerInstanceAAMP::FlushStreamSink(double position, double rate)
{
}

/**
 * @brief to check gstsubtec flag and vttcueventlistener
 */

bool PlayerInstanceAAMP::IsGstreamerSubsEnabled(void)
{
	return false;
}

/**
 * @brief Set Discontinuity handling period change marked flag
 * @param[in] value Period change marked flag
 */
void PlayerInstanceAAMP::SetIsPeriodChangeMarked(bool value)
{
	mIsPeriodChangeMarked = value;
}

/**
 * @brief Get Discontinuity handling period change marked flag
 * @return Period change marked flag
 */
bool PlayerInstanceAAMP::GetIsPeriodChangeMarked()
{
	return mIsPeriodChangeMarked;
}

long long PlayerInstanceAAMP::GetVideoPTS()
{
	return 0;
}

bool PlayerInstanceAAMP::SignalSubtitleClock( void )
{
	return false;
}

int PlayerInstanceAAMP::ScheduleAsyncTask(IdleTask task, void *arg, std::string taskName)
{
	return 0;
}

bool PlayerInstanceAAMP::RemoveAsyncTask(int taskId)
{
	return false;
}

void PlayerInstanceAAMP::NotifyFirstFrameReceived(unsigned long)
{
}

void PlayerInstanceAAMP::NotifyEOSReached()
{
}

void PlayerInstanceAAMP::ReportProgress(bool sync, bool beginningOfStream)
{
}

void PlayerInstanceAAMP::NotifyFirstVideoFrameDisplayed()
{
}

void PlayerInstanceAAMP::LogFirstFrame(void)
{
}

void PlayerInstanceAAMP::LogTuneComplete(void)
{
}

void PlayerInstanceAAMP::InitializeCC(unsigned long)
{
}

bool PlayerInstanceAAMP::IsFirstVideoFrameDisplayedRequired()
{
	return false;
}

void PlayerInstanceAAMP::UpdateSubtitleTimestamp()
{
}

double PlayerInstanceAAMP::GetFirstPTS()
{
	return 0;
}

int PlayerInstanceAAMP::GetCurrentAudioTrackId()
{
	return 0;
}

void PlayerInstanceAAMP::PauseSubtitleParser(bool pause)
{
}

bool PlayerInstanceAAMP::PausePipeline(bool pause, bool forceStopGstreamerPreBuffering)
{
	return false;
}

void PlayerInstanceAAMP::SendBufferChangeEvent(bool bufferingStopped)
{
}

long long PlayerInstanceAAMP::GetPositionRelativeToSeekMilliseconds(long long rate,
																	 long long trickStartUTCMS)
{
	return 0;
}

void PlayerInstanceAAMP::CacheAndApplySubtitleMute(bool muted)
{
}

std::string PlayerInstanceAAMP::SendManifestPreProcessEvent()
{
	std::string  bRetManifestData;
	if(!mProvidedManifestFile.empty())
	{
		bRetManifestData = std::move(mProvidedManifestFile);
	}
	return bRetManifestData;
}

void PlayerInstanceAAMP::updateManifest(const char *manifestData)
{
	if(NULL != manifestData)
		mProvidedManifestFile = manifestData;
}

void PlayerInstanceAAMP::IncrementGaps()
{
}

double PlayerInstanceAAMP::GetStreamPositionMs()
{
	return 0.0;
}
