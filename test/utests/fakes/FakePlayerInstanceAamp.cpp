/*
* If not stated otherwise in this file or this component's license file the
* following copyright and licenses apply:
*
* Copyright 2022 RDK Management
*
* Licensed under the Apache License, Version 2.0 (the "License") {  }
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

#include "main_aamp.h"
#include "MockPlayerInstanceAAMP.h"

MockPlayerInstanceAAMP *g_mockPlayerInstanceAAMP = nullptr;

	PlayerInstanceAAMP::PlayerInstanceAAMP(StreamSink* streamSink, std::function< void(const unsigned char *, int, int, int) > exportFrames) {  }
	PlayerInstanceAAMP::~PlayerInstanceAAMP() {  }

	void PlayerInstanceAAMP::Tune(const char *mainManifestUrl,
									const char *contentType,
									bool bFirstAttempt,
									bool bFinalAttempt,
									const char *traceUUID,
									bool audioDecoderStreamSync) { }

/*	void PlayerInstanceAAMP::Tune(const char *mainManifestUrl,
									bool autoPlay,
									const char *contentType,
									bool bFirstAttempt,
									bool bFinalAttempt,
									const char *traceUUID,
									bool audioDecoderStreamSync,
									const char *refreshManifestUrl,
									int mpdStitchingMode,
									std::string session_id,
									const char *preprocessedManifest
									) { } */
    void PlayerInstanceAAMP::Stop(bool sendStateChangeEvent) {  }
	void PlayerInstanceAAMP::ResetConfiguration() {  }
	void PlayerInstanceAAMP::SetRate(float rate, int overshootcorrection) {  }
	void PlayerInstanceAAMP::PauseAt(double  position) {  }
	void PlayerInstanceAAMP::Seek(double  secondsRelativeToTuneTime, bool keepPaused) {  }
	void PlayerInstanceAAMP::SeekToLive(bool keepPaused) {  }
	void PlayerInstanceAAMP::SetRateAndSeek(int rate, double  secondsRelativeToTuneTime) { }
	void PlayerInstanceAAMP::SetSlowMotionPlayRate (float rate ) {  }
	void PlayerInstanceAAMP::detach() {  }
	void PlayerInstanceAAMP::RegisterEvents(EventListener* eventListener) {  }
	void PlayerInstanceAAMP::UnRegisterEvents(EventListener* eventListener) {  }
	void PlayerInstanceAAMP::SetVideoRectangle(int x, int y, int w, int h) {  }
	void PlayerInstanceAAMP::SetVideoZoom(VideoZoomMode zoom) {  }
	void PlayerInstanceAAMP::SetVideoMute(bool muted) {  }
	void PlayerInstanceAAMP::SetSubtitleMute(bool muted) {  }
	void PlayerInstanceAAMP::SetAudioVolume(int volume) {  }
	void PlayerInstanceAAMP::SetLanguage(const char*  language) {  }
	void PlayerInstanceAAMP::SetSubscribedTags(std::vector<std::string> subscribedTags) {  }
	void PlayerInstanceAAMP::SubscribeResponseHeaders(std::vector<std::string> responseHeaders) {  }
	void PlayerInstanceAAMP::LoadJS(void* context) {  }
	void PlayerInstanceAAMP::UnloadJS(void* context) {  }
	void PlayerInstanceAAMP::AddEventListener(AAMPEventType eventType, EventListener* eventListener) {  }
	void PlayerInstanceAAMP::RemoveEventListener(AAMPEventType eventType, EventListener* eventListener) {  }
	void PlayerInstanceAAMP::InsertAd(const char *url, double  positionSeconds) {  }
	void PlayerInstanceAAMP::AddPageHeaders(std::map<std::string, std::string> customHttpHeaders) {  }
	void PlayerInstanceAAMP::AddCustomHTTPHeader(std::string headerName, std::vector<std::string> headerValue, bool isLicenseHeader) {  }
	void PlayerInstanceAAMP::SetLicenseServerURL(const char *url, DRMSystems type) {  }
	void PlayerInstanceAAMP::SetPreferredDRM(DRMSystems drmType) {  }
	void PlayerInstanceAAMP::SetStereoOnlyPlayback(bool bValue) {  }
	void PlayerInstanceAAMP::SetBulkTimedMetaReport(bool bValue) {  }
	void PlayerInstanceAAMP::SetBulkTimedMetaReportLive(bool bValue) {  }
	void PlayerInstanceAAMP::SetRetuneForUnpairedDiscontinuity(bool bValue) {  }
	void PlayerInstanceAAMP::SetRetuneForGSTInternalError(bool bValue) {  }
	void PlayerInstanceAAMP::SetAnonymousRequest(bool isAnonymous) {  }
	void PlayerInstanceAAMP::SetAvgBWForABR(bool useAvgBW) {  }
	void PlayerInstanceAAMP::SetPreCacheTimeWindow(int nTimeWindow) {  }
	void PlayerInstanceAAMP::SetVODTrickplayFPS(int vodTrickplayFPS) {  }
	void PlayerInstanceAAMP::SetLinearTrickplayFPS(int linearTrickplayFPS) {  }
	void PlayerInstanceAAMP::SetLiveOffset(double liveoffset) {  }
	void PlayerInstanceAAMP::SetLiveOffset4K(double liveoffset) {  }
	void PlayerInstanceAAMP::SetStallErrorCode(int errorCode) {  }
	void PlayerInstanceAAMP::SetStallTimeout(int timeoutMS) {  }
	void PlayerInstanceAAMP::SetReportInterval(int reportInterval) {  }
	void PlayerInstanceAAMP::SetInitFragTimeoutRetryCount(int count) {  }
	void PlayerInstanceAAMP::SetVideoBitrate(BitsPerSecond bitrate) {  }
	void PlayerInstanceAAMP::SetAudioBitrate(BitsPerSecond bitrate) {  }
	void PlayerInstanceAAMP::SetInitialBitrate(BitsPerSecond bitrate) {  }
	void PlayerInstanceAAMP::SetInitialBitrate4K(BitsPerSecond bitrate4K) {  }
	void PlayerInstanceAAMP::SetNetworkTimeout(double  timeout) {  }
	void PlayerInstanceAAMP::SetManifestTimeout(double  timeout) {  }
	void PlayerInstanceAAMP::SetPlaylistTimeout(double  timeout) {  }
	void PlayerInstanceAAMP::SetDownloadBufferSize(int bufferSize) {  }
	void PlayerInstanceAAMP::SetNetworkProxy(const char * proxy) {  }
	void PlayerInstanceAAMP::SetLicenseReqProxy(const char * licenseProxy) {  }
	void PlayerInstanceAAMP::SetDownloadStallTimeout(int stallTimeout) {  }
	void PlayerInstanceAAMP::SetDownloadStartTimeout(int startTimeout) {  }
	void PlayerInstanceAAMP::SetDownloadLowBWTimeout(int lowBWTimeout) {  }
	void PlayerInstanceAAMP::SetPreferredSubtitleLanguage(const char*  language) {  }
	void PlayerInstanceAAMP::SetAlternateContents(const std::string &adBreakId, const std::string &adId, const std::string &url) {  }
    void PlayerInstanceAAMP::ManageAsyncTuneConfig(const char*  url) {  }
	void PlayerInstanceAAMP::SetAsyncTuneConfig(bool bValue) {  }
	void PlayerInstanceAAMP::SetWesterosSinkConfig(bool bValue) {  }
	void PlayerInstanceAAMP::SetLicenseCaching(bool bValue) {  }
	void PlayerInstanceAAMP::SetOutputResolutionCheck(bool bValue) {  }
	void PlayerInstanceAAMP::SetMatchingBaseUrlConfig(bool bValue) {  }
	void PlayerInstanceAAMP::SetPropagateUriParameters(bool bValue) {  }
    void PlayerInstanceAAMP::ApplyArtificialDownloadDelay(unsigned int DownloadDelayInMs) {  }
	void PlayerInstanceAAMP::SetSslVerifyPeerConfig(bool bValue) {  }
	void PlayerInstanceAAMP::SetNewABRConfig(bool bValue) {  }
	void PlayerInstanceAAMP::SetNewAdBreakerConfig(bool bValue) {  }
	void PlayerInstanceAAMP::SetVideoTracks(std::vector<BitsPerSecond> bitrates) {  }
	//void PlayerInstanceAAMP::SetAppName(std::string name) {  }
	void PlayerInstanceAAMP::SetPreferredLanguages(const char*  languageList, const char *preferredRendition, const char *preferredType, const char*  codecList, const char*  labelList, const Accessibility *accessibilityItem, const char* preferredName) {  }
	void PlayerInstanceAAMP::SetPreferredTextLanguages(const char*  param) {  }
	void PlayerInstanceAAMP::SetAudioTrack(std::string language, std::string rendition, std::string type, std::string codec, unsigned int channel, std::string label) {  }
	void PlayerInstanceAAMP::SetPreferredCodec(const char *codecList) {  }
	void PlayerInstanceAAMP::SetPreferredLabels(const char *lableList) {  }
	void PlayerInstanceAAMP::SetPreferredRenditions(const char *renditionList) {  }
	void PlayerInstanceAAMP::SetTuneEventConfig(int tuneEventType) {  }
	void PlayerInstanceAAMP::EnableVideoRectangle(bool rectProperty) {  }
	void PlayerInstanceAAMP::SetRampDownLimit(int limit) {  }
	void PlayerInstanceAAMP::SetInitRampdownLimit(int limit) {  }
	void PlayerInstanceAAMP::SetMinimumBitrate(BitsPerSecond bitrate) {  }
	void PlayerInstanceAAMP::SetMaximumBitrate(BitsPerSecond bitrate) {  }
	void PlayerInstanceAAMP::SetSegmentInjectFailCount(int value) {  }
	void PlayerInstanceAAMP::SetSegmentDecryptFailCount(int value) {  }
	void PlayerInstanceAAMP::SetInitialBufferDuration(int durationSec) {  }
	void PlayerInstanceAAMP::SetNativeCCRendering(bool enable) {  }
	void PlayerInstanceAAMP::SetAudioTrack(int trackId) {  }
	void PlayerInstanceAAMP::SetTextTrack(int trackId, char *ccData) {  }
	//void PlayerInstanceAAMP::SetCCStatus(bool enabled) {  }
	void PlayerInstanceAAMP::SetTextStyle(const std::string &options)
    {
    	if (g_mockPlayerInstanceAAMP != nullptr)
    	{
	    	g_mockPlayerInstanceAAMP->SetTextStyle(options);
	    }
    }
	void PlayerInstanceAAMP::SetLanguageFormat(LangCodePreference preferredFormat, bool useRole) {  }
	void PlayerInstanceAAMP::SetCEAFormat(int format) {  }
	void PlayerInstanceAAMP::SetSessionToken(std::string sessionToken) {  }
	void PlayerInstanceAAMP::SetMaxPlaylistCacheSize(int cacheSize) {  }
	void PlayerInstanceAAMP::EnableSeekableRange(bool enabled) {  }
	void PlayerInstanceAAMP::SetReportVideoPTS(bool enabled) {  }
	void PlayerInstanceAAMP::SetDisable4K(bool value) {  }
	void PlayerInstanceAAMP::DisableContentRestrictions(long grace, long time, bool eventChange) {  }
	void PlayerInstanceAAMP::EnableContentRestrictions() {  }
	void PlayerInstanceAAMP::AsyncStartStop() {  }
	void PlayerInstanceAAMP::PersistBitRateOverSeek(bool value) {  }
	void PlayerInstanceAAMP::SetPausedBehavior(int behavior) {  }
	void PlayerInstanceAAMP::SetUseAbsoluteTimeline(bool configState) {  }
	void PlayerInstanceAAMP::XRESupportedTune(bool xreSupported) {  }
	void PlayerInstanceAAMP::EnableAsyncOperation() {  }
	void PlayerInstanceAAMP::SetRepairIframes(bool configState) {  }
	void PlayerInstanceAAMP::SetAuxiliaryLanguage(const std::string &language) {  }
	void PlayerInstanceAAMP::SetLicenseCustomData(const char *customData) {  }
	void PlayerInstanceAAMP::SetContentProtectionDataUpdateTimeout(int timeout) {  }
	void PlayerInstanceAAMP::ProcessContentProtectionDataConfig(const char *jsonbuffer) {  }
	void PlayerInstanceAAMP::SetRuntimeDRMConfigSupport(bool DynamicDRMSupported) {  }
	//bool PlayerInstanceAAMP::IsLive() { return false; }
	bool PlayerInstanceAAMP::GetVideoMute(void) { return false; }
	bool PlayerInstanceAAMP::GetCCStatus(void) { return false; }
	bool PlayerInstanceAAMP::SetThumbnailTrack(int thumbIndex) { return false; }
	bool PlayerInstanceAAMP::InitAAMPConfig(const char *jsonStr) { return false; }
	int PlayerInstanceAAMP::GetVideoZoom(void) { return 0; }
	int PlayerInstanceAAMP::GetAudioVolume(void) { return 0; }
	int PlayerInstanceAAMP::GetPlaybackRate(void) { return 0; }
	int PlayerInstanceAAMP::GetRampDownLimit(void) { return 0; }
	int PlayerInstanceAAMP::GetInitialBufferDuration(void) { return 0; }
	int PlayerInstanceAAMP::GetAudioTrack() { return 0; }
	int PlayerInstanceAAMP::GetTextTrack() { return 0; }
	long PlayerInstanceAAMP::GetVideoBitrate(void) { return 0; }
	long PlayerInstanceAAMP::GetAudioBitrate(void) { return 0; }
	long PlayerInstanceAAMP::GetInitialBitrate(void) { return 0; }
	long PlayerInstanceAAMP::GetInitialBitrate4k(void) { return 0; }
	long PlayerInstanceAAMP::GetMinimumBitrate(void) { return 0; }
	//long PlayerInstanceAAMP::GetMaximumBitrate(void) { return 0; }
	double PlayerInstanceAAMP::GetPlaybackPosition(void) { return 0; }
	double PlayerInstanceAAMP::GetPlaybackDuration(void) { return 0; }
	std::string PlayerInstanceAAMP::GetAudioLanguage() { return ""; }
	std::string PlayerInstanceAAMP::GetDRM() { return ""; }
    std::string PlayerInstanceAAMP::GetPreferredLanguages() { return ""; }
	DRMSystems PlayerInstanceAAMP::GetPreferredDRM() { return eDRM_NONE; }
	std::vector<BitsPerSecond> PlayerInstanceAAMP::GetVideoBitrates(void) { static std::vector<BitsPerSecond> temp; return temp; }
	std::vector<BitsPerSecond> PlayerInstanceAAMP::GetAudioBitrates(void) { static std::vector<BitsPerSecond> temp; return temp; }
    std::string PlayerInstanceAAMP::GetManifest(void) { return nullptr; }
	//std::string PlayerInstanceAAMP::GetAvailableVideoTracks() { return nullptr; }
	//std::string PlayerInstanceAAMP::GetAvailableAudioTracks(bool allTrack) { return nullptr; }
	//std::string PlayerInstanceAAMP::GetAvailableTextTracks(bool allTrack) { return nullptr; }
	//std::string PlayerInstanceAAMP::GetVideoRectangle() { return nullptr; }
	//std::string PlayerInstanceAAMP::GetAudioTrackInfo() { return nullptr; }
	//std::string PlayerInstanceAAMP::GetTextTrackInfo() { return nullptr; }
	//std::string PlayerInstanceAAMP::GetPreferredAudioProperties() { return nullptr; }
	//std::string PlayerInstanceAAMP::GetPreferredTextProperties() { return nullptr; }
	//std::string PlayerInstanceAAMP::GetTextStyle() { return nullptr; }
	std::string PlayerInstanceAAMP::GetAvailableThumbnailTracks(void) { return nullptr; }
	//std::string PlayerInstanceAAMP::GetThumbnails(double  sduration, double  eduration) { return nullptr; }
	std::string PlayerInstanceAAMP::GetAAMPConfig() { return nullptr; }
	//std::string PlayerInstanceAAMP::GetPlaybackStats() { return nullptr; }
	//std::string PlayerInstanceAAMP::GetVideoPlaybackQuality(void) { return nullptr; }
	bool PlayerInstanceAAMP::SetUserAgent(std::string &userAgent){ return false; }
	//void PlayerInstanceAAMP::updateManifest(const char *manifestData){}
	bool PlayerInstanceAAMP::IsJsInfoLoggingEnabled(void){ return false; }
	bool PlayerInstanceAAMP::IsOOBCCRenderingSupported(void){ return false; }
	int PlayerInstanceAAMP::GetId(void){ return 0; }
	//AAMPPlayerState PlayerInstanceAAMP::GetState(void){ return eSTATE_IDLE; }
	std::string PlayerInstanceAAMP::GetSessionId() const { return ""; }

/*
#include "main_aamp.h"
#include "MockPlayerInstanceAAMP.h"
#include "AampMPDDownloader.h"
#include "AampStreamSinkManager.h"
#include "ID3Metadata.hpp"
#include "AampSegmentInfo.hpp"

MockPlayerInstanceAAMP *g_mockPlayerInstanceAAMP = nullptr;

bool PlayerInstanceAAMP::mTrackGrowableBufMem;

static int PLAYERID_CNTR = 0;
*/
PlayerInstanceAAMP::PlayerInstanceAAMP(AampConfig *config)
{
}

size_t PlayerInstanceAAMP::HandleSSLWriteCallback ( char *ptr, size_t size, size_t nmemb, void* userdata )
{
    return 0;
}

size_t PlayerInstanceAAMP::HandleSSLHeaderCallback ( const char *ptr, size_t size, size_t nmemb, void* user_data )
{
    return 0;
}

int PlayerInstanceAAMP::HandleSSLProgressCallback ( void *clientp, double dltotal, double dlnow, double ultotal, double ulnow )
{
    return 0;
}

void PlayerInstanceAAMP::UpdateUseSinglePipeline( void )
{
}

void PlayerInstanceAAMP::UpdateMaxDRMSessions( void )
{
}

void PlayerInstanceAAMP::ActivatePlayer()
{
}
void PlayerInstanceAAMP::SendMediaMetadataEvent()
{
}
AAMPPlayerState PlayerInstanceAAMP::GetState()
{
    AAMPPlayerState state = eSTATE_IDLE;
    if (g_mockPlayerInstanceAAMP != nullptr)
    {
        state = g_mockPlayerInstanceAAMP->GetState();
    }
    return state;
}

void PlayerInstanceAAMP::SetState(AAMPPlayerState state)
{
    if (g_mockPlayerInstanceAAMP != nullptr)
    {
        g_mockPlayerInstanceAAMP->SetState(state);
    }
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

void PlayerInstanceAAMP::StartPausePositionMonitoring(long long pausePositionMilliseconds)
{
    if (g_mockPlayerInstanceAAMP != nullptr)
    {
        g_mockPlayerInstanceAAMP->StartPausePositionMonitoring(pausePositionMilliseconds);
    }
}

void PlayerInstanceAAMP::StopPausePositionMonitoring(std::string reason)
{
    if (g_mockPlayerInstanceAAMP != nullptr)
    {
        g_mockPlayerInstanceAAMP->StopPausePositionMonitoring(reason);
    }
}

AampCacheHandler * PlayerInstanceAAMP::getAampCacheHandler()
{
    return nullptr;
}

void PlayerInstanceAAMP::Tune(const char *mainManifestUrl,
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
    if (g_mockPlayerInstanceAAMP != nullptr)
    {
        g_mockPlayerInstanceAAMP->NotifyOnEnteringLive();
    }
}

bool PlayerInstanceAAMP::GetPauseOnFirstVideoFrameDisp(void)
{
    return false;
}

long long PlayerInstanceAAMP::GetPositionMilliseconds()
{
    long long positionMs = 0;

    if (g_mockPlayerInstanceAAMP != nullptr)
    {
        positionMs = g_mockPlayerInstanceAAMP->GetPositionMilliseconds();
    }

    return positionMs;
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
    if (g_mockPlayerInstanceAAMP != nullptr)
    {
        g_mockPlayerInstanceAAMP->StopDownloads();
    }
}

void PlayerInstanceAAMP::ResumeDownloads()
{
    if (g_mockPlayerInstanceAAMP != nullptr)
    {
        g_mockPlayerInstanceAAMP->ResumeDownloads();
    }
}

void PlayerInstanceAAMP::EnableDownloads()
{
}

void PlayerInstanceAAMP::AcquireStreamLock()
{
}

void PlayerInstanceAAMP::TuneHelper(TuneType tuneType, bool seekWhilePaused)
{
    if (g_mockPlayerInstanceAAMP != nullptr)
    {
        g_mockPlayerInstanceAAMP->TuneHelper(tuneType, seekWhilePaused);
    }
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

bool PlayerInstanceAAMP::TryStreamLock()
{
    return false;
}

DrmHelperPtr PlayerInstanceAAMP::GetCurrentDRM(void)
{
    return nullptr;
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

void PlayerInstanceAAMP::GetMoneyTraceString(std::string &customHeader) const
{
}

void SetPreferredLanguages(const char *languageList, const char *preferredRendition, const char *preferredType, const char *codecList, const char *labelList )
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

std::string PlayerInstanceAAMP::GetAvailableVideoTracks()
{
    std::string s = "AvailableVideo";
    return s;
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

std::string PlayerInstanceAAMP::GetAvailableTextTracks(bool allTrack)
{
    return "";
}

std::string PlayerInstanceAAMP::GetAvailableAudioTracks(bool allTrack)
{
    if (g_mockPlayerInstanceAAMP != nullptr) {
        return g_mockPlayerInstanceAAMP->GetAvailableAudioTracks(allTrack);
    }else {
        return "";
    }
}

std::string PlayerInstanceAAMP::GetVideoRectangle()
{
    std::string video = "VideoRectangle";
    return video;
}

void PlayerInstanceAAMP::SetAppName(std::string name)
{
}

std::string PlayerInstanceAAMP::GetAppName()
{
    std::string name = "AppName";
    return name;
}

void PlayerInstanceAAMP::SetCCStatus(bool enabled)
{
}

std::string PlayerInstanceAAMP::GetTextStyle()
{
    std::string result = "TextStyle";
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

MediaFormat PlayerInstanceAAMP::GetMediaFormatType(const char *url)
{
    return eMEDIAFORMAT_UNKNOWN;
}

MediaFormat PlayerInstanceAAMP::GetMediaFormatTypeEnum() const
{
    if (g_mockPlayerInstanceAAMP != nullptr) {
        return g_mockPlayerInstanceAAMP->GetMediaFormatTypeEnum();
    } else {
        return eMEDIAFORMAT_UNKNOWN;
    }
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

void PlayerInstanceAAMP::Individualization(const std::string& payload)
{
}

bool PlayerInstanceAAMP::LockGetPositionMilliseconds()
{
    return false;
}

void PlayerInstanceAAMP::UnlockGetPositionMilliseconds()
{
}

/**
 * @brief Check if Live Adjust is required for current content. ( For "vod/ivod/ip-dvr/cdvr/eas", Live Adjust is not required ).
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
    bool retVal = false;//mDownloadsEnabled;
    if (g_mockPlayerInstanceAAMP != nullptr)
    {
        retVal = g_mockPlayerInstanceAAMP->DownloadsAreEnabled();
    }
    return retVal;
}

void PlayerInstanceAAMP::SendDownloadErrorEvent(AAMPTuneFailure tuneFailure, int error_code)
{
}

BitsPerSecond PlayerInstanceAAMP::GetMaximumBitrate()
{
    return LONG_MAX;
}

void PlayerInstanceAAMP::UpdateVideoEndProfileResolution(AampMediaType mediaType, BitsPerSecond bitrate, int width, int height)
{
}

BitsPerSecond PlayerInstanceAAMP::GetDefaultBitrate()
{
    return 0;
}

void PlayerInstanceAAMP::UpdateDuration(double seconds)
{
}

void PlayerInstanceAAMP::SendErrorEvent(AAMPTuneFailure tuneFailure, const char * description, bool isRetryEnabled, int32_t secManagerClassCode, int32_t secManagerReasonCode, int32_t secClientBusinessStatus, const std::string &responseData)
{
    if (g_mockPlayerInstanceAAMP != nullptr)
    {
        g_mockPlayerInstanceAAMP->SendErrorEvent(tuneFailure, description, isRetryEnabled, secManagerClassCode, secManagerReasonCode, secClientBusinessStatus, responseData);
    }
}

void PlayerInstanceAAMP::SetCurlTimeout(long timeoutMS, AampCurlInstance instance)
{
}

void PlayerInstanceAAMP::CurlInit(AampCurlInstance startIdx, unsigned int instanceCount, std::string proxyName)
{
}

bool PlayerInstanceAAMP::GetFile(std::string remoteUrl, AampMediaType mediaType, AampGrowableBuffer *buffer, std::string& effectiveUrl,
                int * http_error, double *downloadTime, const char *range, unsigned int curlInstance,
                bool resetBuffer, BitsPerSecond *bitrate, int * fogError,
                double fragmentDurationSeconds, ProfilerBucketType bucketType, int maxInitDownloadTimeMS)
{
    bool rv = true;

    if (g_mockPlayerInstanceAAMP != nullptr)
    {
        rv = g_mockPlayerInstanceAAMP->GetFile(remoteUrl, mediaType, buffer, effectiveUrl,
                                                 http_error, downloadTime, range, curlInstance,
                                                resetBuffer, bitrate, fogError,
                                                fragmentDurationSeconds, bucketType, maxInitDownloadTimeMS);
    }

    return rv;
}

void PlayerInstanceAAMP::DisableMediaDownloads(AampMediaType type)
{
}

/**
 * @brief Set Content Type
 */
void PlayerInstanceAAMP::SetContentType(const char *cType)
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

void PlayerInstanceAAMP::UpdateVideoEndMetrics(AampMediaType mediaType, BitsPerSecond bitrate, int curlOrHTTPCode, std::string& strUrl, double duration, double curlDownloadTime)
{
}

void PlayerInstanceAAMP::CurlTerm(AampCurlInstance startIdx, unsigned int instanceCount)
{
}

void PlayerInstanceAAMP::DisableDownloads(void)
{
    if (g_mockPlayerInstanceAAMP != nullptr)
    {
        g_mockPlayerInstanceAAMP->DisableDownloads();
    }
}

long long PlayerInstanceAAMP::GetPositionMs()
{
    long long positionMs = 0;

    if (g_mockPlayerInstanceAAMP != nullptr)
    {
        positionMs = g_mockPlayerInstanceAAMP->GetPositionMs();
    }

    return positionMs;
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

void PlayerInstanceAAMP::ReportTimedMetadata(long long timeMilliseconds, const char *szName, const char *szContent, int nb, bool bSyncCall, const char *id, double durationMS)
{
}

void PlayerInstanceAAMP::ResetCurrentlyAvailableBandwidth(BitsPerSecond bitsPerSecond , bool trickPlay,int profile)
{
}

void PlayerInstanceAAMP::ResumeTrackInjection(AampMediaType type)
{
}

void PlayerInstanceAAMPSaveTimedMetadata(long long timeMilliseconds, const char* szName, const char* szContent, int nb, const char* id, double durationMS)
{
}

void PlayerInstanceAAMP::SendEvent(AAMPEventPtr eventData, AAMPEventMode eventMode)
{
}

bool PlayerInstanceAAMP::SendStreamCopy(AampMediaType mediaType, const void *ptr, size_t len, double fpts, double fdts, double fDuration)
{
    if (g_mockPlayerInstanceAAMP != nullptr)
    {
        return g_mockPlayerInstanceAAMP->SendStreamCopy(mediaType, ptr, len, fpts, fdts, fDuration);
    }
    else
    {
        return true;
    }
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

void PlayerInstanceAAMP::UpdateVideoEndMetrics(AampMediaType mediaType, BitsPerSecond bitrate, int curlOrHTTPCode, std::string& strUrl, double duration, double curlDownloadTime, bool keyChanged, bool isEncrypted, ManifestData * manifestData)
{
}

void PlayerInstanceAAMP::UpdateVideoEndMetrics(AAMPAbrInfo & info)
{
}

void PlayerInstanceAAMP::UpdateVideoEndMetrics(AampMediaType mediaType, BitsPerSecond bitrate, int curlOrHTTPCode, std::string& strUrl, double curlDownloadTime, ManifestData * manifestData)
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

AampCurlInstance PlayerInstanceAAMP::GetPlaylistCurlInstance(AampMediaType type, bool isInitialDownload)
{
    return eCURLINSTANCE_MANIFEST_PLAYLIST_VIDEO;
}

void PlayerInstanceAAMPBlockUntilGstreamerWantsData(void(*cb)(void), int periodMs, int track)
{
    if (g_mockPlayerInstanceAAMP != nullptr) {
        return g_mockPlayerInstanceAAMP->BlockUntilGstreamerWantsData(cb, periodMs, track);
    }
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

uint32_t  PlayerInstanceAAMP::GetAudTimeScale(void)
{
    if (g_mockPlayerInstanceAAMP != nullptr) {
        return g_mockPlayerInstanceAAMP->GetAudTimeScale();
    }else {
        return 0u;
    }
}

uint32_t  PlayerInstanceAAMP::GetSubTimeScale(void)
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

AampLLDashServiceData*  PlayerInstanceAAMP::GetLLDashServiceData(void)
{
    return &this->mAampLLDashServiceData;
}

uint32_t  PlayerInstanceAAMP::GetVidTimeScale(void)
{
    if (g_mockPlayerInstanceAAMP != nullptr) {
        return g_mockPlayerInstanceAAMP->GetVidTimeScale();
    }else {
        return 0u;
    }
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

void PlayerInstanceAAMP::NotifyBitRateChangeEvent( BitsPerSecond bitrate, BitrateChangeReason reason, int width, int height, double frameRate, double position, bool GetBWIndex, VideoScanType scantype, int aspectRatioWidth, int aspectRatioHeight)
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

void PlayerInstanceAAMP::ScheduleRetune(PlaybackErrorType errorType, AampMediaType trackType, bool bufferFull)
{
}

void PlayerInstanceAAMP::SendStalledErrorEvent()
{
}

void PlayerInstanceAAMP::SendStreamTransfer(AampMediaType mediaType, AampGrowableBuffer* buffer, double fpts, double fdts, double fDuration, double fragmentPTSoffset, bool initFragment, bool discontinuity)
{
    if (g_mockPlayerInstanceAAMP != nullptr)
    {
        return g_mockPlayerInstanceAAMP->SendStreamTransfer(mediaType, buffer, fpts, fdts, fDuration, fragmentPTSoffset, initFragment, discontinuity);
    }
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

void PlayerInstanceAAMP::SendAnomalyEvent(AAMPAnomalyMessageType type, const char* format, ...)
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
    if (g_mockPlayerInstanceAAMP != nullptr)
    {
        return g_mockPlayerInstanceAAMP->DurationFromStartOfPlaybackMs();
    }
    else
    {
        return 0;
    }
}

void PlayerInstanceAAMP::UpdateVideoEndMetrics(double adjustedRate)
{
}

void PlayerInstanceAAMP::SendAdReservationEvent(AAMPEventType type, const std::string &adBreakId, uint64_t position, uint64_t absolutePositionMs, bool immediate)
{
    if (g_mockPlayerInstanceAAMP != nullptr)
    {
        g_mockPlayerInstanceAAMP->SendAdReservationEvent(type, adBreakId, position, absolutePositionMs, immediate);
    }
}

void PlayerInstanceAAMP::SendAdPlacementEvent(AAMPEventType type, const std::string &adId, uint32_t position, uint64_t absolutePositionMs, uint32_t adOffset, uint32_t adDuration, bool immediate, long error_code)
{
    if (g_mockPlayerInstanceAAMP != nullptr)
    {
        g_mockPlayerInstanceAAMP->SendAdPlacementEvent(type, adId, position, absolutePositionMs, adOffset, adDuration, immediate, error_code);
    }
}

bool PlayerInstanceAAMP::IsLiveStream(void)
{
    return mIsLiveStream;
}

void PlayerInstanceAAMP::WaitForDiscontinuityProcessToComplete(void)
{
    if (g_mockPlayerInstanceAAMP != nullptr)
    {
        g_mockPlayerInstanceAAMP->WaitForDiscontinuityProcessToComplete();
    }
}

void PlayerInstanceAAMP::SendSupportedSpeedsChangedEvent(bool isIframeTrackPresent)
{
}

BitsPerSecond PlayerInstanceAAMP::GetDefaultBitrate4K()
{
    return 0;
}

void PlayerInstanceAAMPSaveNewTimedMetadata(long long timeMS, const char* szName, const char* szContent, int nb, const char* id, double durationMS)
{
    if (g_mockPlayerInstanceAAMP != nullptr)
    {
        g_mockPlayerInstanceAAMP->SaveNewTimedMetadata(timeMS, szName, szContent, nb, id, durationMS);
    }
}

void PlayerInstanceAAMP::FoundEventBreak(const std::string &adBreakId, uint64_t startMS, EventBreakInfo brInfo)
{
    if (g_mockPlayerInstanceAAMP != nullptr)
    {
        g_mockPlayerInstanceAAMP->FoundEventBreak(adBreakId, startMS, brInfo);
    }
}

void PlayerInstanceAAMP::SendAdResolvedEvent(const std::string &adId, bool status, uint64_t startMS, uint64_t durationMs)
{
    if (g_mockPlayerInstanceAAMP != nullptr)
    {
        g_mockPlayerInstanceAAMP->SendAdResolvedEvent(adId, status, startMS, durationMs);
    }
}

void PlayerInstanceAAMP::ReportContentGap(long long timeMS, std::string id, double durationMS)
{
}

void PlayerInstanceAAMP::SendHTTPHeaderResponse()
{
}

void PlayerInstanceAAMP::LoadIDX(ProfilerBucketType bucketType, std::string fragmentUrl, std::string& effectiveUrl, AampGrowableBuffer *fragment, unsigned int curlInstance, const char *range, int * http_code, double *downloadTime, AampMediaType mediaType,int * fogError)
{
        return;
}

bool PlayerInstanceAAMP::IsAudioLanguageSupported (const char *checkLanguage)
{
    return false;
}

void PlayerInstanceAAMP::LicenseRenewal(DrmHelperPtr drmHelper,void* userData)
{
}

bool PlayerInstanceAAMP::IsEventListenerAvailable(AAMPEventType eventType)
{
    return false;
}

void PlayerInstanceAAMP::ID3MetadataHandler(AampMediaType, const uint8_t *, size_t, const SegmentInfo_t &, const char * scheme_uri)
{
}

void PlayerInstanceAAMP::ResetProfileCache()
{
}

struct curl_slist* PlayerInstanceAAMP::GetCustomHeaders(AampMediaType mediaType)
{

       return NULL;
}

void PlayerInstanceAAMP::ResetDiscontinuityInTracks()
{
}

std::shared_ptr<ManifestDownloadConfig> PlayerInstanceAAMP::prepareManifestDownloadConfig()
{
    return NULL;
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

void PlayerInstanceAAMP::SetStreamFormat(StreamOutputFormat videoFormat, StreamOutputFormat audioFormat, StreamOutputFormat auxFormat)
{
    if (g_mockPlayerInstanceAAMP != nullptr)
    {
        g_mockPlayerInstanceAAMP->SetStreamFormat(videoFormat, audioFormat, auxFormat);
    }
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
void PlayerInstanceAAMP::GetLastDownloadedManifest(std::string& manifestBuffer)
{
}

void PlayerInstanceAAMP::ProcessID3Metadata(char *segment, size_t size, AampMediaType type, uint64_t timeStampOffset)
{
    if (g_mockPlayerInstanceAAMP != nullptr)
    {
        g_mockPlayerInstanceAAMP->ProcessID3Metadata(segment, size, type, timeStampOffset);
    }
}

void PlayerInstanceAAMP::SetVidTimeScale(uint32_t vidTimeScale)
{
}

void PlayerInstanceAAMP::SetAudTimeScale(uint32_t audTimeScale)
{
}

void PlayerInstanceAAMPSetSubTimeScale(uint32_t audTimeScale)
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

void PlayerInstanceAAMP::SetLatencyParam(double latency, double buff, double rate, double bw)
{
}

void PlayerInstanceAAMP::SetLLDLowBufferParam(double latency, double buff, double rate, double bw, double buffLowCount)
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
    int retval = 0;
    if (g_mockPlayerInstanceAAMP != nullptr)
    {
        retval = g_mockPlayerInstanceAAMP->ScheduleAsyncTask(task, arg, taskName);
    }
    return retval;
}

bool PlayerInstanceAAMP::RemoveAsyncTask(int taskId)
{
    bool retval = false;
    if (g_mockPlayerInstanceAAMP != nullptr)
    {
        retval = g_mockPlayerInstanceAAMP->RemoveAsyncTask(taskId);
    }
    return retval;
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

double PlayerInstanceAAMP::GetMidSeekPosOffset()
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

void PlayerInstanceAAMP::FlushTrack(AampMediaType mediaType,double pos)
{
}

void PlayerInstanceAAMP::ReleaseDynamicDRMToUpdateWait(void)
{
}

std::shared_ptr<TSB::Store> PlayerInstanceAAMP::GetTSBStore(const TSB::Store::Config& config, TSB::LogFunction logger, TSB::LogLevel level)
{
    if (g_mockPlayerInstanceAAMP)
    {
        return g_mockPlayerInstanceAAMP->GetTSBStore(config, logger, level);
    }
    return nullptr;
}

void PlayerInstanceAAMP::SetLocalAAMPTsbInjection(bool value)
{
}

bool PlayerInstanceAAMP::IsLocalAAMPTsbInjection()
{
    if (g_mockPlayerInstanceAAMP)
    {
        return g_mockPlayerInstanceAAMP->IsLocalAAMPTsbInjection();
    }
    return false;
}

void PlayerInstanceAAMP::UpdateLocalAAMPTsbInjection()
{
    if (g_mockPlayerInstanceAAMP)
    {
        g_mockPlayerInstanceAAMP->UpdateLocalAAMPTsbInjection();
    }
}

bool PlayerInstanceAAMP::GetLLDashAdjustSpeed(void)
{
    if (g_mockPlayerInstanceAAMP)
    {
        return g_mockPlayerInstanceAAMP->GetLLDashAdjustSpeed();
    }
    return false;
}

double PlayerInstanceAAMP::GetLLDashCurrentPlayBackRate(void)
{
    if (g_mockPlayerInstanceAAMP)
    {
        return g_mockPlayerInstanceAAMP->GetLLDashCurrentPlayBackRate();
    }
    return 1.0;
}

void PlayerInstanceAAMP::TimedWaitForLatencyCheck(int timeInMs)
{
}

void PlayerInstanceAAMP::WakeupLatencyCheck()
{
}

void PlayerInstanceAAMP::IncreaseGSTBufferSize()
{
}

AampTSBSessionManager *PlayerInstanceAAMP::GetTSBSessionManager()
{
    AampTSBSessionManager *aampTsbSessionManager = nullptr;

    if (g_mockPlayerInstanceAAMP)
    {
        aampTsbSessionManager = g_mockPlayerInstanceAAMP->GetTSBSessionManager();
    }

    return aampTsbSessionManager;
}

std::string PlayerInstanceAAMP::GetLicenseReqProxy()
{
    return "";
}

std::string PlayerInstanceAAMP::GetLicenseCustomData()
{
    return "";
}

void PlayerInstanceAAMP::GetCustomLicenseHeaders(std::unordered_map<std::string, std::vector<std::string>>& customHeaders)
{
}

std::string PlayerInstanceAAMP::GetLicenseServerUrlForDrm(DRMSystems type)
{
    return "";
}

bool PlayerInstanceAAMP::ReconfigureForCodecChange()
{
    return false;
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


void PlayerInstanceAAMP::SetPauseOnStartPlayback(bool enable)
{
    if (g_mockPlayerInstanceAAMP)
    {
        g_mockPlayerInstanceAAMP->SetPauseOnStartPlayback(enable);
    }
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

void PlayerInstanceAAMP::ResetTrickStartUTCTime()
{
}

void PlayerInstanceAAMP::SetLLDashChunkMode(bool enable)
{
    if (g_mockPlayerInstanceAAMP)
    {
        g_mockPlayerInstanceAAMP->SetLLDashChunkMode(enable);
    }
}

bool PlayerInstanceAAMP::GetLLDashChunkMode()
{
    bool bIsChunkMode = false;
    if (g_mockPlayerInstanceAAMP)
    {
        bIsChunkMode = g_mockPlayerInstanceAAMP->GetLLDashChunkMode();
    }
    return bIsChunkMode;
}

const char* PlayerInstanceAAMP::getStringForPlaybackError(PlaybackErrorType errorType)
{
    return "";
}

unsigned char* PlayerInstanceAAMP::ReplaceKeyIDPsshData(const unsigned char *InputData, const size_t InputDataLength,  size_t & OutputDataLength) {

    return NULL;
}

void PlayerInstanceAAMP::SendBlockedEvent(const std::string & reason, const std::string currentLocator)
{

}
void PlayerInstanceAAMP::GetPlayerVideoSize(int &width, int &height)
{
}

void PlayerInstanceAAMP::SendVTTCueDataAsEvent(VTTCue* cue)
{
}

void PlayerInstanceAAMP::UpdateCCTrackInfo(const std::vector<TextTrackInfo>& textTracksCopy, std::vector<CCTrackInfo>& updatedTextTracks)
{
}

void PlayerInstanceAAMP::CalculateTrickModePositionEOS(void)
{
    if (g_mockPlayerInstanceAAMP)
    {
        g_mockPlayerInstanceAAMP->CalculateTrickModePositionEOS();
    }
}

double PlayerInstanceAAMP::GetLivePlayPosition(void)
{
    double livePlayPosition = 0.0;
    if (g_mockPlayerInstanceAAMP)
    {
        livePlayPosition = g_mockPlayerInstanceAAMP->GetLivePlayPosition();
    }
    return livePlayPosition;
}

void PlayerInstanceAAMP::IncrementGaps()
{
}

double PlayerInstanceAAMP::GetStreamPositionMs()
{
    return 0.0;
}

void PlayerInstanceAAMP::SendMonitorAvEvent(const std::string &status, int64_t videoPositionMS, int64_t audioPositionMS, uint64_t timeInStateMS, uint64_t droppedFrames)
{
}
double PlayerInstanceAAMP::GetFormatPositionOffsetInMSecs()
{
    return 0;
}
