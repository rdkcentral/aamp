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

//Enable the define below to get AAMP logging out when running tests
//#define ENABLE_LOGGING
#define TEST_LOG_LEVEL eLOGLEVEL_TRACE

MockPrivateInstanceAAMP *g_mockPrivateInstanceAAMP = nullptr;

PrivateInstanceAAMP::PrivateInstanceAAMP(AampConfig *config) : mConfig (config)
{
}

PrivateInstanceAAMP::~PrivateInstanceAAMP()
{
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

void PrivateInstanceAAMP::SetStreamSink(StreamSink* streamSink)
{
}

void PrivateInstanceAAMP::GetState(PrivAAMPState& state)
{
	if (g_mockPrivateInstanceAAMP != nullptr)
	{
		g_mockPrivateInstanceAAMP->GetState(state);
	}
}

void PrivateInstanceAAMP::SetState(PrivAAMPState state)
{
	if (g_mockPrivateInstanceAAMP != nullptr)
	{
		g_mockPrivateInstanceAAMP->SetState(state);
	}
}

void PrivateInstanceAAMP::Stop()
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

void PrivateInstanceAAMP::Tune(const char *mainManifestUrl, bool autoPlay, const char *contentType, bool bFirstAttempt, bool bFinalAttempt,const char *pTraceID,bool audioDecoderStreamSync)
{
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
	return false;
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

void PrivateInstanceAAMP::NotifyFirstBufferProcessed()
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

void PrivateInstanceAAMP::TeardownStream(bool newTune)
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

std::shared_ptr<AampDrmHelper> PrivateInstanceAAMP::GetCurrentDRM(void)
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

ContentType PrivateInstanceAAMP::GetContentType() const
{
	return ContentType_UNKNOWN;
}

void PrivateInstanceAAMP::SetAlternateContents(const std::string &adBreakId, const std::string &adId, const std::string &url)
{
}

void SetPreferredLanguages(const char *languageList, const char *preferredRendition, const char *preferredType, const char *codecList, const char *labelList )
{
}

std::string PrivateInstanceAAMP::GetPreferredAudioProperties()
{
	return nullptr;
}

std::string PrivateInstanceAAMP::GetPreferredTextProperties()
{
	return nullptr;
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
	return nullptr;
}

void PrivateInstanceAAMP::SetVideoTracks(std::vector<long> bitrateList)
{
}

std::string PrivateInstanceAAMP::GetAudioTrackInfo()
{
	return nullptr;
}

std::string PrivateInstanceAAMP::GetTextTrackInfo()
{
	return nullptr;
}

int PrivateInstanceAAMP::GetTextTrack()
{
	return 0;
}

std::string PrivateInstanceAAMP::GetAvailableTextTracks(bool allTrack)
{
	return nullptr;
}

std::string PrivateInstanceAAMP::GetAvailableAudioTracks(bool allTrack)
{
	return nullptr;
}

std::string PrivateInstanceAAMP::GetVideoRectangle()
{
	return nullptr;
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
	return nullptr;
}

std::string PrivateInstanceAAMP::GetThumbnailTracks()
{
	return nullptr;
}

std::string PrivateInstanceAAMP::GetThumbnails(double tStart, double tEnd)
{
	return nullptr;
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

void PrivateInstanceAAMP::TuneFail(bool fail)
{
}

std::string PrivateInstanceAAMP::GetPlaybackStats()
{
	return nullptr;
}

void PrivateInstanceAAMP::individualization(const std::string& payload)
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

void PrivateInstanceAAMP::SetPreferredLanguages(char const*, char const*, char const*, char const*, char const*)
{
}

bool PrivateInstanceAAMP::IsLiveAdjustRequired()
{
    return false;
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

long PrivateInstanceAAMP::GetMaximumBitrate()
{
    return 0;
}

void PrivateInstanceAAMP::UpdateVideoEndProfileResolution(MediaType mediaType, long bitrate, int width, int height)
{
}

long PrivateInstanceAAMP::GetDefaultBitrate()
{
    return 0;
}

void PrivateInstanceAAMP::UpdateDuration(double seconds)
{
}

void PrivateInstanceAAMP::SendErrorEvent(AAMPTuneFailure tuneFailure, const char * description, bool isRetryEnabled, int32_t secManagerClassCode, int32_t secManagerReasonCode, int32_t secClientBusinessStatus)
{
}

void PrivateInstanceAAMP::SetCurlTimeout(long timeoutMS, AampCurlInstance instance)
{
}

void PrivateInstanceAAMP::CurlInit(AampCurlInstance startIdx, unsigned int instanceCount, std::string proxyName)
{
}

bool PrivateInstanceAAMP::GetFile(std::string remoteUrl,struct GrowableBuffer *buffer, std::string& effectiveUrl,
                int * http_error, double *downloadTime, const char *range, unsigned int curlInstance,
                bool resetBuffer, MediaType fileType, long *bitrate, int * fogError,
                double fragmentDurationSeconds,CMCDHeaders *pCMCDMetrics)
{
    return true;
}

void PrivateInstanceAAMP::DisableMediaDownloads(MediaType type)
{
}

void PrivateInstanceAAMP::SetContentType(const char *cType)
{
}

void PrivateInstanceAAMP::UpdateVideoEndMetrics(MediaType mediaType, long bitrate, int curlOrHTTPCode, std::string& strUrl, double duration, double curlDownloadTime)
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

long PrivateInstanceAAMP::GetMinimumBitrate()
{
    return 0;
}

long long PrivateInstanceAAMP::GetPositionMs()
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

void PrivateInstanceAAMP::ReportTimedMetadata(long long timeMilliseconds, const char *szName, const char *szContent, int nb, bool bSyncCall, const char *id, double durationMS)
{
}

void PrivateInstanceAAMP::ResetCurrentlyAvailableBandwidth(long bitsPerSecond , bool trickPlay,int profile)
{
}

void PrivateInstanceAAMP::ResumeTrackInjection(MediaType type)
{
}

void PrivateInstanceAAMP::SaveTimedMetadata(long long timeMilliseconds, const char* szName, const char* szContent, int nb, const char* id, double durationMS)
{
}

void PrivateInstanceAAMP::SendEvent(AAMPEventPtr eventData, AAMPEventMode eventMode)
{
}

void PrivateInstanceAAMP::SendStreamCopy(MediaType mediaType, const void *ptr, size_t len, double fpts, double fdts, double fDuration)
{
}

bool PrivateInstanceAAMP::SendTunedEvent(bool isSynchronous)
{
    return true;
}

void PrivateInstanceAAMP::SetPreCacheDownloadList(PreCacheUrlList &dnldListInput)
{
}

void PrivateInstanceAAMP::StopTrackDownloads(MediaType type)
{
}

void PrivateInstanceAAMP::StopTrackInjection(MediaType type)
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

void PrivateInstanceAAMP::UpdateVideoEndMetrics(MediaType mediaType, long bitrate, int curlOrHTTPCode, std::string& strUrl, double duration, double curlDownloadTime, bool keyChanged, bool isEncrypted, ManifestData * manifestData)
{
}

void PrivateInstanceAAMP::UpdateVideoEndMetrics(AAMPAbrInfo & info)
{
}

void PrivateInstanceAAMP::UpdateVideoEndMetrics(MediaType mediaType, long bitrate, int curlOrHTTPCode, std::string& strUrl, double curlDownloadTime, ManifestData * manifestData)
{
}

bool PrivateInstanceAAMP::WebVTTCueListenersRegistered(void)
{
    return true;
}

LangCodePreference PrivateInstanceAAMP::GetLangCodePreference()
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

AampCurlInstance PrivateInstanceAAMP::GetPlaylistCurlInstance(MediaType type, bool isInitialDownload)
{
    return eCURLINSTANCE_MANIFEST_PLAYLIST_VIDEO;
}

void PrivateInstanceAAMP::BlockUntilGstreamerWantsData(void(*cb)(void), int periodMs, int track)
{
}

void PrivateInstanceAAMP::CheckForDiscontinuityStall(MediaType mediaType)
{
}

bool PrivateInstanceAAMP::Discontinuity(MediaType track, bool setDiscontinuityFlag)
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

void PrivateInstanceAAMP::EnableMediaDownloads(MediaType type)
{
}

void PrivateInstanceAAMP::EndOfStreamReached(MediaType mediaType)
{
}

uint32_t  PrivateInstanceAAMP::GetAudTimeScale(void)
{
    return 0u;
}

long PrivateInstanceAAMP::GetCurrentlyAvailableBandwidth(void)
{
    return 0;
}

long PrivateInstanceAAMP::GetIframeBitrate()
{
    return 0;
}

long PrivateInstanceAAMP::GetIframeBitrate4K()
{
    return 0;
}

AampLLDashServiceData*  PrivateInstanceAAMP::GetLLDashServiceData(void)
{
    return nullptr;
}

uint32_t  PrivateInstanceAAMP::GetVidTimeScale(void)
{
    return 0u;
}

void PrivateInstanceAAMP::InterruptableMsSleep(int timeInMs)
{
}

bool PrivateInstanceAAMP::IsDiscontinuityIgnoredForOtherTrack(MediaType track)
{
    return true;
}

bool PrivateInstanceAAMP::IsDiscontinuityProcessPending()
{
    return true;
}

bool PrivateInstanceAAMP::IsSinkCacheEmpty(MediaType mediaType)
{
    return true;
}

const char* PrivateInstanceAAMP::MediaTypeString(MediaType fileType)
{
    return nullptr;
}

void PrivateInstanceAAMP::NotifyBitRateChangeEvent(int bitrate, BitrateChangeReason reason, int width, int height, double frameRate, double position, bool GetBWIndex, VideoScanType scantype, int aspectRatioWidth, int aspectRatioHeight)
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

void PrivateInstanceAAMP::ScheduleRetune(PlaybackErrorType errorType, MediaType trackType)
{
}

void PrivateInstanceAAMP::SendStalledErrorEvent()
{
}

void PrivateInstanceAAMP::SendStreamTransfer(MediaType mediaType, GrowableBuffer* buffer, double fpts, double fdts, double fDuration, bool initFragment, bool discontinuity)
{
}

void PrivateInstanceAAMP::SetTrackDiscontinuityIgnoredStatus(MediaType track)
{
}

void PrivateInstanceAAMP::StopBuffering(bool forceStop)
{
}

bool PrivateInstanceAAMP::TrackDownloadsAreEnabled(MediaType type)
{
    return true;
}

void PrivateInstanceAAMP::UnblockWaitForDiscontinuityProcessToComplete(void)
{
}

void PrivateInstanceAAMP::UpdateMaxDRMSessions(void)
{
}
