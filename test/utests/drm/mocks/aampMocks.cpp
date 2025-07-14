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
#include "priv_aamp.h"
#include "aampgstplayer.h"

#include "MockPrivateInstanceAAMP.h"

MockPrivateInstanceAAMP *g_mockPrivateInstanceAAMP = nullptr;

std::shared_ptr<AampConfig> gGlobalConfig;
AampConfig *gpGlobalConfig;

static std::unordered_map<std::string, std::vector<std::string>> fCustomHeaders;

void MockAampReset(void)
{
	gGlobalConfig = std::make_shared<AampConfig>();

	gpGlobalConfig = gGlobalConfig.get();
}

PrivateInstanceAAMP::PrivateInstanceAAMP(AampConfig *config) : mConfig(config), mIsFakeTune(false), mIsVSS(false)
{
}

PrivateInstanceAAMP::~PrivateInstanceAAMP()
{
}

void PrivateInstanceAAMP::GetCustomLicenseHeaders(
	std::unordered_map<std::string, std::vector<std::string>> &customHeaders)
{
	customHeaders = fCustomHeaders;
}

void PrivateInstanceAAMP::SendDrmErrorEvent(DrmMetaDataEventPtr event, bool isRetryEnabled)
{
}

void PrivateInstanceAAMP::SendDRMMetaData(DrmMetaDataEventPtr e)
{
}

void PrivateInstanceAAMP::Individualization(const std::string &payload)
{
	if (g_mockPrivateInstanceAAMP != nullptr)
	{
		g_mockPrivateInstanceAAMP->Individualization(payload);
	}
}

void PrivateInstanceAAMP::SendEvent(AAMPEventPtr eventData, AAMPEventMode eventMode)
{
}

void PrivateInstanceAAMP::SetState(AAMPPlayerState state)
{
}

std::string PrivateInstanceAAMP::GetLicenseReqProxy()
{
	return std::string();
}

void PrivateInstanceAAMP::SendErrorEvent(AAMPTuneFailure tuneFailure, const char * description, bool isRetryEnabled, int32_t secManagerClassCode, int32_t secManagerReasonCode, int32_t secClientBusinessStatus, const std::string &responseData)
{
}

std::string PrivateInstanceAAMP::GetLicenseServerUrlForDrm(DRMSystems type)
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

std::string PrivateInstanceAAMP::GetLicenseCustomData()
{
	return std::string();
}

bool PrivateInstanceAAMP::IsEventListenerAvailable(AAMPEventType eventType)
{
	return false;
}

std::string PrivateInstanceAAMP::GetAppName()
{
	return std::string();
}

int PrivateInstanceAAMP::HandleSSLProgressCallback(void *clientp, double dltotal, double dlnow,
												   double ultotal, double ulnow)
{
	return 0;
}

size_t PrivateInstanceAAMP::HandleSSLHeaderCallback(const char *ptr, size_t size, size_t nmemb,
													void *userdata)
{
	return 0;
}

size_t PrivateInstanceAAMP::HandleSSLWriteCallback(char *ptr, size_t size, size_t nmemb,
												   void *userdata)
{
	return 0;
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

void PrivateInstanceAAMP::GetMoneyTraceString(std::string &customHeader) const
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

void PrivateInstanceAAMP::UpdateUseSinglePipeline(void)
{
}

void PrivateInstanceAAMP::UpdateMaxDRMSessions(void)
{
}

void PrivateInstanceAAMP::ActivatePlayer()
{
}

void PrivateInstanceAAMP::SendMediaMetadataEvent()
{
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

AampCacheHandler *PrivateInstanceAAMP::getAampCacheHandler()
{
	return nullptr;
}

void PrivateInstanceAAMP::Tune(const char *mainManifestUrl, bool autoPlay, const char *contentType,
							   bool bFirstAttempt, bool bFinalAttempt, const char *pTraceID,
							   bool audioDecoderStreamSync, const char *refreshManifestUrl,
							   int mpdStitchingMode, std::string sid,const char *preprocessedManifest)

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
}

bool PrivateInstanceAAMP::GetPauseOnFirstVideoFrameDisp(void)
{
	return false;
}

long long PrivateInstanceAAMP::GetPositionMilliseconds()
{
	return 0;
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
}

void PrivateInstanceAAMP::ResumeDownloads()
{
}

void PrivateInstanceAAMP::EnableDownloads()
{
}

void PrivateInstanceAAMP::AcquireStreamLock()
{
}

void PrivateInstanceAAMP::TuneHelper(TuneType tuneType, bool seekWhilePaused)
{
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

void PrivateInstanceAAMP::AddEventListener(AAMPEventType eventType, EventListener *eventListener)
{
}

void PrivateInstanceAAMP::RemoveEventListener(AAMPEventType eventType, EventListener *eventListener)
{
}

DrmHelperPtr PrivateInstanceAAMP::GetCurrentDRM(void)
{
	return nullptr;
}

void PrivateInstanceAAMP::AddCustomHTTPHeader(std::string headerName,
											  std::vector<std::string> headerValue,
											  bool isLicenseHeader)
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

void PrivateInstanceAAMP::SetAlternateContents(const std::string &adBreakId,
											   const std::string &adId, const std::string &url)
{
}

void SetPreferredLanguages(const char *languageList, const char *preferredRendition,
						   const char *preferredType, const char *codecList, const char *labelList)
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

void PrivateInstanceAAMP::SetPreferredTextLanguages(const char *param)
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

std::string PrivateInstanceAAMP::GetVideoRectangle()
{
	std::string video = "videorectangel";
	return video;
}

void PrivateInstanceAAMP::SetAppName(std::string name)
{
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
	std::string result = "sampleStyle";
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

void PrivateInstanceAAMP::SetPreferredLanguages(const char *, const char *, const char *,
												const char *, const char *, const Accessibility *,
												const char *)
{
}

/**
 * @brief Check if Live Adjust is required for current content. ( For "vod/ivod/ip-dvr/cdvr/eas",
 * Live Adjust is not required ).
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
	return true;
}

void PrivateInstanceAAMP::SendDownloadErrorEvent(AAMPTuneFailure tuneFailure, int error_code)
{
}

BitsPerSecond PrivateInstanceAAMP::GetMaximumBitrate()
{
	return LONG_MAX;
}

void PrivateInstanceAAMP::UpdateVideoEndProfileResolution(AampMediaType mediaType,
														  BitsPerSecond bitrate, int width,
														  int height)
{
}

BitsPerSecond PrivateInstanceAAMP::GetDefaultBitrate()
{
	return 0;
}

void PrivateInstanceAAMP::UpdateDuration(double seconds)
{
}

void PrivateInstanceAAMP::SetCurlTimeout(long timeoutMS, AampCurlInstance instance)
{
}

void PrivateInstanceAAMP::CurlInit(AampCurlInstance startIdx, unsigned int instanceCount,
								   std::string proxyName)
{
}

void PrivateInstanceAAMP::DisableMediaDownloads(AampMediaType type)
{
}

/**
 * @brief Set Content Type
 */
void PrivateInstanceAAMP::SetContentType(const char *cType)
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

void PrivateInstanceAAMP::UpdateVideoEndMetrics(AampMediaType mediaType, BitsPerSecond bitrate,
												int curlOrHTTPCode, std::string &strUrl,
												double duration, double curlDownloadTime)
{
}

void PrivateInstanceAAMP::CurlTerm(AampCurlInstance startIdx, unsigned int instanceCount)
{
}

void PrivateInstanceAAMP::DisableDownloads(void)
{
}

int PrivateInstanceAAMP::GetInitialBufferDuration()
{
	return 0;
}

BitsPerSecond PrivateInstanceAAMP::GetMinimumBitrate()
{
	return 0;
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

void PrivateInstanceAAMP::ReportTimedMetadata(long long timeMilliseconds, const char *szName,
											  const char *szContent, int nb, bool bSyncCall,
											  const char *id, double durationMS)
{
}

void PrivateInstanceAAMP::ResetCurrentlyAvailableBandwidth(BitsPerSecond bitsPerSecond,
														   bool trickPlay, int profile)
{
}

void PrivateInstanceAAMP::ResumeTrackInjection(AampMediaType type)
{
}

void PrivateInstanceAAMP::SaveTimedMetadata(long long timeMilliseconds, const char *szName,
											const char *szContent, int nb, const char *id,
											double durationMS)
{
}

bool PrivateInstanceAAMP::SendStreamCopy(AampMediaType mediaType, const void *ptr, size_t len,
										 double fpts, double fdts, double fDuration)
{
	return true;
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

void PrivateInstanceAAMP::UpdateVideoEndMetrics(AampMediaType mediaType, BitsPerSecond bitrate,
												int curlOrHTTPCode, std::string &strUrl,
												double duration, double curlDownloadTime,
												bool keyChanged, bool isEncrypted,
												ManifestData *manifestData)
{
}

void PrivateInstanceAAMP::UpdateVideoEndMetrics(AAMPAbrInfo &info)
{
}

void PrivateInstanceAAMP::UpdateVideoEndMetrics(AampMediaType mediaType, BitsPerSecond bitrate,
												int curlOrHTTPCode, std::string &strUrl,
												double curlDownloadTime, ManifestData *manifestData)
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

AampCurlInstance PrivateInstanceAAMP::GetPlaylistCurlInstance(AampMediaType type,
															  bool isInitialDownload)
{
	return eCURLINSTANCE_MANIFEST_PLAYLIST_VIDEO;
}

void PrivateInstanceAAMP::BlockUntilGstreamerWantsData(void (*cb)(void), int periodMs, int track)
{
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

uint32_t PrivateInstanceAAMP::GetAudTimeScale(void)
{
	return 0u;
}

uint32_t PrivateInstanceAAMP::GetSubTimeScale(void)
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

AampLLDashServiceData *PrivateInstanceAAMP::GetLLDashServiceData(void)
{
	return &this->mAampLLDashServiceData;
}

uint32_t PrivateInstanceAAMP::GetVidTimeScale(void)
{
	return 0u;
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

void PrivateInstanceAAMP::NotifyBitRateChangeEvent(BitsPerSecond bitrate,
												   BitrateChangeReason reason, int width,
												   int height, double frameRate, double position,
												   bool GetBWIndex, VideoScanType scantype,
												   int aspectRatioWidth, int aspectRatioHeight)
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

void PrivateInstanceAAMP::ScheduleRetune(PlaybackErrorType errorType, AampMediaType trackType, bool bufferFull )
{
}

void PrivateInstanceAAMP::SendStalledErrorEvent()
{
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

void PrivateInstanceAAMP::SendAnomalyEvent(AAMPAnomalyMessageType type, const char *format, ...)
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
	return 0;
}

void PrivateInstanceAAMP::UpdateVideoEndMetrics(double adjustedRate)
{
}

void PrivateInstanceAAMP::SendAdReservationEvent(AAMPEventType type, const std::string &adBreakId,
												 uint64_t position, uint64_t absolutePositionMs, bool immediate)
{
}

void PrivateInstanceAAMP::SendAdPlacementEvent(AAMPEventType type, const std::string &adId,
											   uint32_t position, uint64_t absolutePositionMs, uint32_t adOffset,
											   uint32_t adDuration, bool immediate, long error_code)
{
}

bool PrivateInstanceAAMP::IsLiveStream(void)
{
	return mIsLiveStream;
}

void PrivateInstanceAAMP::WaitForDiscontinuityProcessToComplete(void)
{
}

void PrivateInstanceAAMP::SendSupportedSpeedsChangedEvent(bool isIframeTrackPresent)
{
}

BitsPerSecond PrivateInstanceAAMP::GetDefaultBitrate4K()
{
	return 0;
}

void PrivateInstanceAAMP::SaveNewTimedMetadata(long long timeMS, const char *szName,
											   const char *szContent, int nb, const char *id,
											   double durationMS)
{
}

void PrivateInstanceAAMP::FoundEventBreak(const std::string &adBreakId, uint64_t startMS,
										  EventBreakInfo brInfo)
{
}

void PrivateInstanceAAMP::SendAdResolvedEvent(const std::string &adId, bool status,
											  uint64_t startMS, uint64_t durationMs, AAMPCDAIError errorCode)
{
}

void PrivateInstanceAAMP::ReportContentGap(long long timeMS, std::string id, double durationMS)
{
}

void PrivateInstanceAAMP::SendHTTPHeaderResponse()
{
}

void PrivateInstanceAAMP::LoadIDX(ProfilerBucketType bucketType, std::string fragmentUrl,
								  std::string &effectiveUrl, AampGrowableBuffer *fragment,
								  unsigned int curlInstance, const char *range, int *http_code,
								  double *downloadTime, AampMediaType fileType, int *fogError)
{
	return;
}

void PrivateInstanceAAMP::LicenseRenewal(DrmHelperPtr drmHelper, void *userData)
{
}

void PrivateInstanceAAMP::ID3MetadataHandler(AampMediaType, const uint8_t *, size_t,
											 const SegmentInfo_t &, const char *scheme_uri)
{
}

void PrivateInstanceAAMP::ResetProfileCache()
{
}

struct curl_slist *PrivateInstanceAAMP::GetCustomHeaders(AampMediaType fileType)
{

	return nullptr;
}

void PrivateInstanceAAMP::ResetDiscontinuityInTracks()
{
}

std::shared_ptr<ManifestDownloadConfig> PrivateInstanceAAMP::prepareManifestDownloadConfig()
{
	return nullptr;
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
void PrivateInstanceAAMP::GetLastDownloadedManifest(std::string &manifestBuffer)
{
}

void PrivateInstanceAAMP::ProcessID3Metadata(char *segment, size_t size, AampMediaType type,
											 uint64_t timeStampOffset)
{
}

void PrivateInstanceAAMP::SetVidTimeScale(uint32_t vidTimeScale)
{
}

void PrivateInstanceAAMP::SetAudTimeScale(uint32_t audTimeScale)
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

void PrivateInstanceAAMP::SetLatencyParam(double latency, double buffer, double playbackRate, double bw)
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
	return 0;
}

bool PrivateInstanceAAMP::RemoveAsyncTask(int taskId)
{
	return false;
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

void PrivateInstanceAAMP::IncrementGaps()
{
}

double PrivateInstanceAAMP::GetStreamPositionMs()
{
	return 0.0;
}
