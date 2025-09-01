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
 * @file main_aamp.h
 * @brief Types and APIs exposed by the AAMP player.
 */

/**
 @mainpage Advanced Adaptive Micro Player (AAMP)

 <b>AAMP</b> is a native video engine build on top of gstreamer, optimized for
 performance, memory use, and code size.
 <br><b>AAMP</b> downloads and parses HLS/DASH manifests. <b>AAMP</b> has been
 integrated with Adobe Access, PlayReady, CONSEC agnostic, and Widevine DRM

 <b>AAMP</b> is fronted by JS PP (JavaScript Player Platform), which provides an
 additional layer of functionality including player analytics, configuration
 management, and ad insertion.
 */

#ifndef MAINAAMP_H
#define MAINAAMP_H

#include <memory>
#include <functional>
#include <vector>
#include <string>
#include <string.h>
#include <mutex>
#include <stddef.h>
#include <functional>
#include "Accessibility.hpp"
#include "AampEvent.h"
#include "AampEventListener.h"
#include "DrmSystems.h"
#include "AampMediaType.h"
#include "AampScheduler.h"
#include "AampConfig.h"

#include "LangCodePreference.h"
#include "StreamOutputFormat.h"
#include "VideoZoomMode.h"
#include "StreamSink.h"
#include "TimedMetadata.h"

/*! \mainpage
 *
 * \section intro_sec Introduction
 *
 * See PlayerInstanceAAMP for libaamp public C++ API's
 *
 */

#define PrivAAMPState AAMPPlayerState // backwards compatibility for apps using native interface

/**
 * @class PlayerInstanceAAMP
 * @brief Player interface class for the JS plugin.
 */
class PlayerInstanceAAMP
{
public: // FIXME: this should be private, but some tests access it
	class PrivateInstanceAAMP *aamp;  		  /**< AAMP player's private instance */
	const std::vector<TimedMetadata> & GetTimedMetadata( void ) const;

private:
	std::shared_ptr<PrivateInstanceAAMP> sp_aamp; 	  /**< shared pointer for aamp resource */

public:
	AampConfig mConfig;

	/**
	 *   @fn PlayerInstanceAAMP
	 *
	 *   @param  streamSink - custom stream sink, NULL for default.
	 *   @param  exportFrames - callback function to export video frames
	 */
	PlayerInstanceAAMP( StreamSink* streamSink = NULL, std::function< void(const unsigned char *, int, int, int) > exportFrames = nullptr );

	/**
	 *   @fn ~PlayerInstanceAAMP
	 */
	~PlayerInstanceAAMP();

	/**
	 *   @brief copy constructor
	 *
	 *   @param other object to copy
	 */
	PlayerInstanceAAMP(const PlayerInstanceAAMP& other) = delete;

	/**
	 *   @brief To overload = operator for copying
	 *
	 *   @param other object to copy
	 */
	PlayerInstanceAAMP& operator=(const PlayerInstanceAAMP& other) = delete;
    
	/**
	 *   @fn Tune
	 *
	 *   @param[in]  mainManifestUrl - HTTP/HTTPS url to be played.
	 *   @param[in]  contentType - Content type of the asset
	 *   @param[in]  audioDecoderStreamSync - Enable or disable audio decoder stream sync,
	 *                set to 'false' if audio fragments come with additional padding at the end
	 *   @return void
	 */
	void Tune(const char *mainManifestUrl, const char *contentType, bool bFirstAttempt,
				bool bFinalAttempt,const char *traceUUID,bool audioDecoderStreamSync);

	/**
	 *   @fn Tune
	 *
	 *   @param[in]  mainManifestUrl - HTTP/HTTPS url to be played.
	 *   @param[in]  autoPlay - Start playback immediately or not
	 *   @param[in]  contentType - Content type of the asset
	 *   @param[in]  audioDecoderStreamSync - Enable or disable audio decoder stream sync,
	 *                set to 'false' if audio fragments come with additional padding at the end
	 *   @return void
	 */
	void Tune(const char *mainManifestUrl,
				bool autoPlay = true,
				const char *contentType = NULL,
				bool bFirstAttempt = true,
				bool bFinalAttempt = false,
				const char *traceUUID = NULL,
				bool audioDecoderStreamSync = true,
				const char *refreshManifestUrl = NULL,
				int mpdStitchingMode = 0,
				std::string sid = std::string{},
				const char *manifestData = NULL);

	/**
	 *   @brief Stop playback and release resources.
	 *   @param[in]  sendStateChangeEvent - true if state change events need to be sent for Stop operation
	 *   @return void
	 */
	void Stop(bool sendStateChangeEvent = true);

	/**
	 *   @fn ResetConfiguration
	 *   @return void
	 */
	void ResetConfiguration();

	/**
	 *   @fn SetRate
	 *
	 *   @param[in]  rate - Rate of playback.
	 *   @param[in]  overshootcorrection - overshoot correction in milliseconds.
	 *   @return void
	 */
	void SetRate(float rate, int overshootcorrection=0);

	/**
	 *   @fn SetUserAgent
	 *
	 *   @param[in]  userAgent - userAgent value read from browser
	 *   @return bool
	 */

	bool SetUserAgent(std::string &userAgent);

	/**
	 *   @fn SetPlaybackSpeed
	 *
	 *   @param[in]  speed - rate to set playback speed.s
	 *   @return void
	 */
	void SetPlaybackSpeed(float speed);

	/**
	 *   @fn PauseAt
	 *
	 *       Any subsequent call to this method will override the previous call.
	 *
	 *   @param[in]  position - Absolute position within the asset for VOD or
	 *           relative position from first tune command for linear content;
	 *           a negative value would cancel any previous PauseAt call.
	 *   @return void
	 */
	void PauseAt(double position);

	/**
	 *   @fn Seek
	 *
	 *   @param[in]  secondsRelativeToTuneTime - Seek position for VOD,
	 *           relative position from first tune command.
	 *   @param[in]  keepPaused - set true if want to keep paused state after seek
	 */
	void Seek(double secondsRelativeToTuneTime, bool keepPaused = false);

	/**
	 *   @fn SeekToLive
	 *
	 *   @param[in]  keepPaused - set true if want to keep paused state after seek
	 *   @return void
	 */
	void SeekToLive(bool keepPaused = false);

	/**
	 *   @fn SetRateAndSeek
	 *
	 *   @param[in]  rate - Rate of playback.
	 *   @param[in]  secondsRelativeToTuneTime - Seek position for VOD,
	 *           relative position from first tune command.
	 *   @return void
	 */
	void SetRateAndSeek(int rate, double secondsRelativeToTuneTime);

	/**
	 *   @brief Set slow motion player speed.
	 *
	 *   @param[in]  rate - Rate of playback.
	 */
	void SetSlowMotionPlayRate (float rate );

	/**
	 * @fn detach
	 * @return void
	 */
	void detach();

	/**
	 *   @brief Registers the event with the corresponding listener
	 *
	 *   @param[in]  type - Event type
	 *   @param[in]  listener - pointer to implementation of EventListener to receive events.
	 *   @return void
	 */
	void RegisterEvent(AAMPEventType type, EventListener* listener);

	/**
	 *   @fn RegisterEvents
	 *
	 *   @param[in]  eventListener - pointer to implementation of EventListener to receive events.
	 *   @return void
	 */
	void RegisterEvents(EventListener* eventListener);
	/**
	 *   @fn UnRegisterEvents
	 *
	 *   @param[in]  eventListener - pointer to implementation of EventListener to receive events.
	 *   @return void
	 */
	void UnRegisterEvents(EventListener* eventListener);

	/**
	 *   @fn SetVideoRectangle
	 *
	 *   @param[in]  x - horizontal start position.
	 *   @param[in]  y - vertical start position.
	 *   @param[in]  w - width.
	 *   @param[in]  h - height.
	 *   @return void
	 */
	void SetVideoRectangle(int x, int y, int w, int h);

	/**
	 *   @fn SetVideoZoom
	 *
	 *   @param[in]  zoom - zoom mode.
	 *   @return void
	 */
	void SetVideoZoom(VideoZoomMode zoom);

	/**
	 *   @fn SetVideoMute
	 *
	 *   @param[in]  muted - true to disable video, false to enable video.
	 *   @return void
	 */
	void SetVideoMute(bool muted);

	/**
	 *   @brief Enable/ Disable Subtitle.
	 *
	 *   @param[in]  muted - true to disable subtitle, false to enable subtitle.
	 *   @return void
	 */
	void SetSubtitleMute(bool muted);

	/**
	 *   @brief Set Audio Volume.
	 *
	 *   @param[in]  volume - Minimum 0, maximum 100.
	 *   @return void
	 */
	void SetAudioVolume(int volume);

	/**
	 *   @fn SetLanguage
	 *
	 *   @param[in]  language - Language of audio track.
	 *   @return void
	 */
	void SetLanguage(const char* language);

	/**
	 *   @brief Set array of subscribed tags.
	 *
	 *   @param[in]  subscribedTags - Array of subscribed tags.
	 *   @return void
	 */
	void SetSubscribedTags(std::vector<std::string> subscribedTags);

	/**
		 *   @fn SubscribeResponseHeaders
		 *
		 *   @param  responseHeaders - Array of response headers.
	 *   @return void
	 */
	void SubscribeResponseHeaders(std::vector<std::string> responseHeaders);

	/**
	 *   @fn LoadJS
	 *
	 *   @param[in]  context - JS context.
	 *   @return void
	 */
	void LoadJS(void* context);

	/**
	 *   @fn UnloadJS
	 *
	 *   @param[in]  context - JS context.
	 *   @return void
	 */
	void UnloadJS(void* context);

	/**
	 *   @fn AddEventListener
	 *
	 *   @param[in]  eventType - type of event.
	 *   @param[in]  eventListener - listener for the eventType.
	 *   @return void
	 */
	void AddEventListener(AAMPEventType eventType, EventListener* eventListener);

	/**
	 *   @fn RemoveEventListener
	 *
	 *   @param[in]  eventType - type of event.
	 *   @param[in]  eventListener - listener to be removed for the eventType.
	 *   @return void
	 */
	void RemoveEventListener(AAMPEventType eventType, EventListener* eventListener);

	/**
	 *   @fn IsLive
	 *
	 *   @return bool - True if live content, false otherwise
	 */
	bool IsLive();

	/**
	 *   @fn  IsJsInfoLoggingEnabled
	 *
	 *   @return bool - True if jsinfo is enabled, false otherwise
	 */
	bool IsJsInfoLoggingEnabled();


	/**
	 *   @brief Schedule insertion of ad at given position.
	 *
	 *   @param[in]  url - HTTP/HTTPS url of the ad
	 *   @param[in]  positionSeconds - position at which ad shall be inserted
	 *   @return void
	 */
	void InsertAd(const char *url, double positionSeconds);

	/**
	 *   @fn GetAudioLanguage
	 *
	 *   @return const char* - current audio language
	 */
	std::string GetAudioLanguage();

	const char * GetCurrentAudioLanguage(); // stub for pxVideo.cpp backwards compatibility - TBR

	/**
	 *   @fn GetDRM
	 *
	 *   @return char* - current drm
	 */
	std::string GetDRM();
	/**
	 * @fn AddPageHeaders
	 * @param customHttpHeaders - customHttpHeaders map of custom http headers
	 * @return void
	 */
	void AddPageHeaders(std::map<std::string, std::string> customHttpHeaders);

	/**
	 *   @fn AddCustomHTTPHeader
	 *
	 *   @param[in]  headerName - Name of custom HTTP header
	 *   @param[in]  headerValue - Value to be passed along with HTTP header.
	 *   @param[in]  isLicenseHeader - true if header is to be used for license HTTP requests
	 *   @return void
	 */
	void AddCustomHTTPHeader(std::string headerName, std::vector<std::string> headerValue, bool isLicenseHeader = false);

	/**
	 *   @fn SetLicenseServerURL
	 *
	 *   @param[in]  url - URL of the server to be used for license requests
	 *   @param[in]  type - DRM Type(PR/WV) for which the server URL should be used, global by default
	 *   @return void
	 */
	void SetLicenseServerURL(const char *url, DRMSystems type = eDRM_MAX_DRMSystems);

	/**
	 *   @fn SetPreferredDRM
	 *
	 *   @param[in] drmType - Preferred DRM type
	 *   @return void
	 */
	void SetPreferredDRM(DRMSystems drmType);

	/**
	 *   @fn GetPreferredDRM
	 *
	 *   @return Preferred DRM type
	 */
	DRMSystems GetPreferredDRM();

	/**
	 *   @fn  SetStereoOnlyPlayback
	 *   @param[in] bValue - disable EC3/ATMOS if the value is true
	 *
	 *   @return void
	 */
	void SetStereoOnlyPlayback(bool bValue);

	/**
	 *   @fn SetBulkTimedMetaReport
	 *   @param[in] bValue - if true Bulk event reporting enabled
	 *
	 *   @return void
	 */
	void SetBulkTimedMetaReport(bool bValue);
	/**
	 *   @fn SetBulkTimedMetaReport for live
	 *   @param[in] bValue - if true Bulk event reporting enabled for live
	 *
	 *   @return void
	 */
	void SetBulkTimedMetaReportLive(bool bValue);

	/**
	 *   @fn SetRetuneForUnpairedDiscontinuity
	 *   @param[in] bValue - true if unpaired discontinuity retune set
	 *
	 *   @return void
	 */
	void SetRetuneForUnpairedDiscontinuity(bool bValue);

	/**
	 *   @fn SetRetuneForGSTInternalError
	 *   @param[in] bValue - true if gst internal error retune set
	 *
	 *   @return void
	 */
	void SetRetuneForGSTInternalError(bool bValue);

	/**
	 *   @fn SetAnonymousRequest
	 *
	 *   @param[in]  isAnonymous - True if session token should be blank and false otherwise.
	 *   @return void
	 */
	void SetAnonymousRequest(bool isAnonymous);

	/**
	 *   @fn SetAvgBWForABR
	 *
	 *   @param  useAvgBW - Flag for true / false
	 */
	void SetAvgBWForABR(bool useAvgBW);

	/**
	 *   @fn SetPreCacheTimeWindow
	 *
	 *   @param nTimeWindow Time in minutes - Max PreCache Time
	 *   @return void
	 */
	void SetPreCacheTimeWindow(int nTimeWindow);

	/**
	 *   @fn SetVODTrickplayFPS
	 *
	 *   @param[in]  vodTrickplayFPS - FPS to be used for VOD Trickplay
	 *   @return void
	 */
	void SetVODTrickplayFPS(int vodTrickplayFPS);

	/**
	 *   @fn SetLinearTrickplayFPS
	 *
	 *   @param[in]  linearTrickplayFPS - FPS to be used for Linear Trickplay
	 *   @return void
	 */
	void SetLinearTrickplayFPS(int linearTrickplayFPS);

	/**
	 *   @fn SetLiveOffset
	 *
	 *   @param[in]  liveoffset- Live Offset
	 *   @return void
	 */
	void SetLiveOffset(double liveoffset);

	/**
	 *   @fn SetLiveOffset4K
	 *
	 *   @param[in]  liveoffset- Live Offset
	 *   @return void
	 */
	void SetLiveOffset4K(double liveoffset);

	/**
	 *   @fn SetStallErrorCode
	 *
	 *   @param[in]  errorCode - error code for playback stall errors.
	 *   @return void
	 */
	void SetStallErrorCode(int errorCode);

	/**
	 *   @fn SetStallTimeout
	 *
	 *   @param[in]  timeoutMS - timeout in milliseconds for playback stall detection.
	 *   @return void
	 */
	void SetStallTimeout(int timeoutMS);

	/**
	 *   @fn SetReportInterval
	 *
	 *   @param  reportInterval - playback reporting interval in milliSeconds.
	 *   @return void
	 */
	void SetReportInterval(int reportInterval);

	/**
	 *   @fn SetInitFragTimeoutRetryCount
	 *
	 *   @param  count - max attempt for timeout retry count
	 *   @return void
	 */
	void SetInitFragTimeoutRetryCount(int count);

	/**
	 *   @fn GetPlaybackPosition
	 *
	 *   @return current playback position in seconds
	 */
	double GetPlaybackPosition(void);

	/**
	 *   @fn GetPlaybackDuration
	 *
	 *   @return duration in seconds
	 */
	double GetPlaybackDuration(void);

	/**
	 *  @fn GetId
	 *
	 *  @return returns unique id of player,
	 */
	int GetId(void);
	void SetId( int iPlayerId );


	/**
	 *   @fn GetState
	 *
	 *   @return current AAMP state
	 */
	AAMPPlayerState GetState(void);

	/**
	 *   @fn GetVideoBitrate
	 *
	 *   @return bitrate of video profile
	 */
	long GetVideoBitrate(void);

	/**
	 *   @fn SetVideoBitrate
	 *
	 *   @param[in] bitrate preferred bitrate for video profile
	 */
	void SetVideoBitrate(BitsPerSecond bitrate);

	/**
	 *   @fn GetAudioBitrate
	 *
	 *   @return bitrate of audio profile
	 */
	BitsPerSecond GetAudioBitrate(void);

	/**
	 *   @fn SetAudioBitrate
	 *
	 *   @param[in] bitrate preferred bitrate for audio profile
	 */
	void SetAudioBitrate(BitsPerSecond bitrate);

	/**
	 *   @fn GetVideoZoom
	 *
	 *   @return video zoom mode
	 */
	int GetVideoZoom(void);

	/**
	 *   @fn GetVideoMute
	 *
	 *   @return video mute status
	 *
	 */
	bool GetVideoMute(void);

	/**
	 *   @fn GetAudioVolume
	 *
	 *   @return audio volume
	 */
	int GetAudioVolume(void);

	/**
	 *   @fn GetPlaybackRate
	 *
	 *   @return current playback rate
	 */
	int GetPlaybackRate(void);

	/**
	 *   @fn GetVideoBitrates
	 *
	 *   @return available video bitrates
	 */
	std::vector<BitsPerSecond> GetVideoBitrates(void);

	/**
		 *   @fn GetManifest
		 *
		 *   @return available manifest
		 */
		std::string GetManifest(void);

	/**
	 *   @fn GetAudioBitrates
	 *
	 *   @return available audio bitrates
	 */
	std::vector<BitsPerSecond> GetAudioBitrates(void);

	/**
	 *   @fn SetInitialBitrate
	 *
	 *   @param[in] bitrate initial bitrate to be selected
	 *   @return void
	 */
	void SetInitialBitrate(BitsPerSecond bitrate);

	/**
	 *   @fn GetInitialBitrate
	 *
	 *   @return initial bitrate value.
	 */
	BitsPerSecond GetInitialBitrate(void);

	/**
	 *   @fn SetInitialBitrate4K
	 *
	 *   @param[in] bitrate4K initial bitrate to be selected for 4K assets.
	 *   @return void
	 */
	void SetInitialBitrate4K(BitsPerSecond bitrate4K);

	/**
	 *   @fn GetInitialBitrate4k
	 *
	 *   @return initial bitrate value for 4k assets
	 */
	BitsPerSecond GetInitialBitrate4k(void);

	/**
	 *   @fn SetNetworkTimeout
	 *
	 *   @param[in] timeout preferred timeout value
	 *   @return void
	 */
	void SetNetworkTimeout(double timeout);

	/**
	 *   @fn SetManifestTimeout
	 *
	 *   @param[in] timeout preferred timeout value
	 *   @return void
	 */
	void SetManifestTimeout(double timeout);

	/**
	 *   @fn SetPlaylistTimeout
	 *
	 *   @param[in] timeout preferred timeout value
	 *   @return void
	 */
	void SetPlaylistTimeout(double timeout);

	/**
	 *   @fn SetDownloadBufferSize
	 *
	 *   @param[in] bufferSize preferred download buffer size
	 *   @return void
	 */
	void SetDownloadBufferSize(int bufferSize);

	/**
	 *   @fn SetNetworkProxy
	 *
	 *   @param[in] proxy network proxy to use
	 *   @return void
	 */
	void SetNetworkProxy(const char * proxy);

	/**
	 *   @fn SetLicenseReqProxy
	 *   @param[in] licenseProxy proxy to use for license request
	 *   @return void
	 */
	void SetLicenseReqProxy(const char * licenseProxy);

	/**
	 *   @fn SetDownloadStallTimeout
	 *
	 *   @param[in] stallTimeout curl stall timeout value
	 *   @return void
	 */
	void SetDownloadStallTimeout(int stallTimeout);

	/**
	 *   @fn SetDownloadStartTimeout
	 *
	 *   @param[in] startTimeout curl download start timeout
	 *   @return void
	 */
	void SetDownloadStartTimeout(int startTimeout);

	/**
	 *   @fn SetDownloadLowBWTimeout
	 *
	 *   @param[in] lowBWTimeout curl download low bandwidth timeout
	 *   @return void
	 */
	void SetDownloadLowBWTimeout(int lowBWTimeout);

	/**
	 *   @fn SetPreferredSubtitleLanguage
	 *
	 *   @param[in]  language - Language of text track.
	 *   @return void
	 */
	void SetPreferredSubtitleLanguage(const char* language);

	/**
	 *   @fn SetAlternateContents
	 *
	 *   @param[in] adBreakId Adbreak's unique identifier.
	 *   @param[in] adId Individual Ad's id
	 *   @param[in] url Ad URL
	 *   @return void
	 */
	void SetAlternateContents(const std::string &adBreakId, const std::string &adId, const std::string &url);

	/**
	 *   @fn ManageAsyncTuneConfig
	 *   @param[in] url - main manifest url
	 *
	 *   @return void
	 */
	void ManageAsyncTuneConfig(const char* url);

	/**
	 *   @fn SetAsyncTuneConfig
	 *   @param[in] bValue - true if async tune enabled
	 *
	 *   @return void
	 */
	void SetAsyncTuneConfig(bool bValue);

	/**
	 *   @fn SetWesterosSinkConfig
	 *   @param[in] bValue - true if westeros sink enabled
	 *
	 *   @return void
	 */
	void SetWesterosSinkConfig(bool bValue);

	/**
	 *	@fn SetLicenseCaching
	 *	@param[in] bValue - true/false to enable/disable license caching
	 *
	 *	@return void
	 */
	void SetLicenseCaching(bool bValue);

	/**
		 *      @fn SetOutputResolutionCheck
		 *      @param[in] bValue - true/false to enable/disable profile filtering by display resolution
		 *
		 *      @return void
		 */
	void SetOutputResolutionCheck(bool bValue);

	/**
	 *   @fn SetMatchingBaseUrlConfig
	 *
	 *   @param[in] bValue - true if Matching BaseUrl enabled
	 *   @return void
	 */
	void SetMatchingBaseUrlConfig(bool bValue);

		/**
		 *   @fn SetPropagateUriParameters
		 *
		 *   @param[in] bValue - default value: true
		 *   @return void
		 */
	void SetPropagateUriParameters(bool bValue);

		/**
		 *   @fn ApplyArtificialDownloadDelay
		 *
		 *   @param[in] DownloadDelayInMs - default value: zero
		 *   @return void
		 */
		void ApplyArtificialDownloadDelay(unsigned int DownloadDelayInMs);

	/**
	 *   @fn SetSslVerifyPeerConfig
	 *
	 *   @param[in] bValue - default value: false
	 *   @return void
	 */
	void SetSslVerifyPeerConfig(bool bValue);

	/**
	 *   @brief Configure New ABR Enable/Disable
	 *   @param[in] bValue - true if new ABR enabled
	 *
	 *   @return void
	 */
	void SetNewABRConfig(bool bValue);

	/**
	 *   @fn SetNewAdBreakerConfig
	 *   @param[in] bValue - true if new AdBreaker enabled
	 *
	 *   @return void
	 */
	void SetNewAdBreakerConfig(bool bValue);

	/**
	 *   @fn SetBase64LicenseWrapping
	 *   @param[in] bValue - true if json formatted base64 license data payload is expected
	 *
	 *   @return void
	 */
	void SetBase64LicenseWrapping(bool bValue);

	/**
	 *   @fn GetAvailableVideoTracks
	 *
	 *   @return std::string JSON formatted list of video tracks
	 */
	std::string GetAvailableVideoTracks();

	/**
	 *   @fn SetVideoTracks
	 *   @param[in] bitrate - video bitrate list
	 *
	 *   @return void
	 */
	void SetVideoTracks(std::vector<BitsPerSecond> bitrates);

	/**
	 *   @fn GetAvailableAudioTracks
	 *
	 *   @return std::string JSON formatted list of audio tracks
	 */
	std::string GetAvailableAudioTracks(bool allTrack=false);

	/**
	 *   @fn GetAvailableTextTracks
	 */
	std::string GetAvailableTextTracks(bool allTrack = false);

	/**
	 *   @fn GetVideoRectangle
	 *
	 *   @return current video co-ordinates in x,y,w,h format
	 */
	std::string GetVideoRectangle();

	/**
	 *   @fn SetAppName
	 *
	 *   @return void
	 */
	void SetAppName(std::string name);
	std::string GetAppName();

	/**
	 *   @fn SetPreferredLanguages
	 *   @param[in] languageList - string with comma-delimited language list in ISO-639
	 *             from most to least preferred: "lang1,lang2". Set NULL to clear current list.
	 	 *   @param[in] preferredRendition  - preferred rendition from role
	 	 *   @param[in] preferredType -  preferred accessibility type
	 *   @param[in] codecList - string with comma-delimited codec list
	 *             from most to least preferred: "codec1,codec2". Set NULL to clear current list.
	 *   @param[in] labelList - string with comma-delimited label list
	 *             from most to least preferred: "label1,label2". Set NULL to clear current list.
	 *   @param[in] accessibilityItem - preferred accessibilityNode with scheme id and value
	 *   @param[in] preferredName - preferred name of track
	 *   @return void
	 */
	void SetPreferredLanguages(const char* languageList, const char *preferredRendition = NULL, const char *preferredType = NULL, const char* codecList = NULL, const char* labelList = NULL, const Accessibility *accessibilityItem = NULL, const char *preferredName = NULL);


	/**
	 *   @fn SetPreferredTextLanguages
	 *   @param[in] languageList - string with comma-delimited language list in ISO-639
	 *   @return void
	 */
	void SetPreferredTextLanguages(const char* param);

	/**
	 *   @fn SetAudioTrack
	 *   @param[in] language - Language to set
	 *   @param[in] rendition - Role/rendition to set
	 *   @param[in] codec - Codec to set
	 *   @param[in] channel - Channel number to set
	 *   @param[in] label - Label to set
	 *
	 *   @return void
	 */
	void SetAudioTrack(std::string language="", std::string rendition="", std::string type="", std::string codec="", unsigned int channel=0, std::string label="");
	/**
	 *   @fn SetPreferredCodec
	 *   @param[in] codecList - string with array with codec list
	 *
	 *   @return void
	 */
	void SetPreferredCodec(const char *codecList);

	/**
	 *   @fn SetPreferredLabels
	 *   @param[in] lableList - string with array with label list
	 *
	 *   @return void
	 */
	void SetPreferredLabels(const char *lableList);

	/**
	 *   @fn SetPreferredRenditions
	 *   @param[in] renditionList - string with comma-delimited rendition list in ISO-639
	 *             from most to least preferred. Set NULL to clear current list.
	 *
	 *   @return void
	 */
	void SetPreferredRenditions(const char *renditionList);

	/**
	 *   @fn GetPreferredLanguages
	 *
	 *   @return const char* - current comma-delimited language list or NULL if not set
	 */
	 std::string GetPreferredLanguages();

	/**
	 *   @fn SetTuneEventConfig
	 *
	 *   @param[in] tuneEventType preferred tune event type
	 */
	void SetTuneEventConfig(int tuneEventType);

	/**
	 *   @fn EnableVideoRectangle
	 *
	 *   @param[in] rectProperty video rectangle property
	 */
	void EnableVideoRectangle(bool rectProperty);

	/**
	 *   @fn SetRampDownLimit
	 *   @return void
	 */
	void SetRampDownLimit(int limit);

	/**
	 * @fn GetRampDownLimit
	 * @return rampdownlimit config value
	 */
	int GetRampDownLimit(void);

	/**
	 * @fn SetInitRampdownLimit
	 * @return void
	 */
	void SetInitRampdownLimit(int limit);

	/**
	 * @fn SetMinimumBitrate
	 * @return void
	 */
	void SetMinimumBitrate(BitsPerSecond bitrate);

	/**
	 * @fn GetMinimumBitrate
	 * @return Minimum bitrate value
	 *
	 */
	BitsPerSecond GetMinimumBitrate(void);

	/**
	 * @fn SetMaximumBitrate
	 * @return void
	 */
	void SetMaximumBitrate(BitsPerSecond bitrate);

	/**
	 * @fn GetMaximumBitrate
	 * @return Max bit rate value
	 */
	BitsPerSecond GetMaximumBitrate(void);

	/**
	 * @fn SetSegmentInjectFailCount
	 * @return void
	 */
	void SetSegmentInjectFailCount(int value);

	/**
	 * @fn SetSegmentDecryptFailCount
	 * @return void
	 */
	void SetSegmentDecryptFailCount(int value);

	/**
	 * @fn SetInitialBufferDuration
	 * @return void
	 */
	void SetInitialBufferDuration(int durationSec);

	/**
	 * @fn GetInitialBufferDuration
	 *
	 * @return int - Initial Buffer Duration
	 */
	int GetInitialBufferDuration(void);

	/**
	 *   @fn SetNativeCCRendering
	 *
	 *   @param[in] enable - true for native CC rendering on
	 *   @return void
	 */
	void SetNativeCCRendering(bool enable);

	/**
	 *   @fn SetAudioTrack
	 *
	 *   @param[in] trackId - index of audio track in available track list
	 *   @return void
	 */
	void SetAudioTrack(int trackId);

	/**
	 *   @fn SetAudioOnlyPlayback
	 *
	 *   @param[in] audioOnlyPlayback - true if audio only playback
	 */
	void SetAudioOnlyPlayback(bool audioOnlyPlayback);

	/**
	 *   @fn GetAudioTrack
	 *
	 *   @return int - index of current audio track in available track list
	 */
	int GetAudioTrack();

	/**
	 *   @fn GetAudioTrackInfo
	 *
	 *   @return int - index of current audio track in available track list
	 */
	std::string GetAudioTrackInfo();

	/**
	 *   @fn GetTextTrackInfo
	 *
	 *   @return int - index of current audio track in available track list
	 */
	std::string GetTextTrackInfo();

	/**
	 *   @fn GetPreferredAudioProperties
	 *
	 *   @return json string
	 */
	std::string GetPreferredAudioProperties();

	/**
	 *   @fn GetPreferredTextProperties
	 */
	std::string GetPreferredTextProperties();

	/**
	 *   @fn SetTextTrack
	 *
	 *   @param[in] trackId - index of text track in available track list
	 *   @param[in] ccData - subtitle data from application.
	 *   @return void
	 */
	void SetTextTrack(int trackId, char *ccData=NULL);

	/**
	 *   @fn GetTextTrack
	 *
	 *   @return int - index of current text track in available track list
	 */
	int GetTextTrack();

	/**
	 *   @fn SetCCStatus
	 *
	 *   @param[in] enabled - true for CC on, false otherwise
	 *   @return void
	 */
	void SetCCStatus(bool enabled);

	/**
	 *   @fn GetCCStatus
	 *
	 *   @return bool true (enabled) else false(disabled)
	 */
	bool GetCCStatus(void);

	/**
	 *   @fn SetTextStyle
	 *
	 *   @param[in] options - JSON formatted style options
	 *   @return void
	 */
	void SetTextStyle(const std::string &options);

	/**
	 *   @fn GetTextStyle
	 *
	 *   @return std::string - JSON formatted style options
	 */
	std::string GetTextStyle();
	/**
	 * @fn SetLanguageFormat
	 * @param[in] preferredFormat - one of \ref LangCodePreference
	 * @param[in] useRole - if enabled, the language in format <lang>-<role>
	 *                      if <role> attribute available in stream
	 *
	 * @return void
	 */
	void SetLanguageFormat(LangCodePreference preferredFormat, bool useRole = false);

	/**
	 *   @brief Set the CEA format for force setting
	 *
	 *   @param[in] format - 0 for 608, 1 for 708
	 *   @return void
	 */
	void SetCEAFormat(int format);

	/**
	 *   @brief Set the session token for player
	 *
	 *   @param[in]string -  sessionToken
	 *   @return void
	 */
	void SetSessionToken(std::string sessionToken);

	/**
	 *   @fn SetMaxPlaylistCacheSize
	 *   @param cacheSize size of cache to store
	 *   @return void
	 */
	void SetMaxPlaylistCacheSize(int cacheSize);

	/**
	 *   @fn EnableSeekableRange
	 *
	 *   @param[in] enabled - true if enabled
	 */
	void EnableSeekableRange(bool enabled);

	/**
	 *   @fn SetReportVideoPTS
	 *
	 *   @param[in] enabled - true if enabled
	 */
	void SetReportVideoPTS(bool enabled);

	/**
	 *   @fn SetDisable4K
	 *
	 *   @param[in] value - disabled if true
	 *   @return void
	 */
	void SetDisable4K(bool value);

	/**
	 *   @fn DisableContentRestrictions
	 *   @param[in] grace - seconds from current time, grace period, grace = -1 will allow an unlimited grace period
	 *   @param[in] time - seconds from current time,time till which the channel need to be kept unlocked
	 *   @param[in] eventChange - disable restriction handling till next program event boundary
	 *
	 *   @return void
	 */
	void DisableContentRestrictions(long grace, long time, bool eventChange);

	/**
	 *   @fn EnableContentRestrictions
	 *   @return void
	 */
	void EnableContentRestrictions();

	/**
	 *   @fn AsyncStartStop
	 *
	 *   @return void
	 */
	void AsyncStartStop();

	/**
	 *   @fn PersistBitRateOverSeek
	 *
	 *   @param[in] value - To enable/disable configuration
	 *   @return void
	 */
	void PersistBitRateOverSeek(bool value);

	/**
	 *   @fn GetAvailableThumbnailTracks
	 *
	 *   @return bitrate of thumbnail track.
	 */
	std::string GetAvailableThumbnailTracks(void);

	/**
	 *   @fn SetThumbnailTrack
	 *
	 *   @param[in] thumbIndex preferred bitrate for thumbnail profile
	 */
	bool SetThumbnailTrack(int thumbIndex);

	/**
	 *   @fn GetThumbnails
	 *
	 *   @param[in] eduration duration  for thumbnails
	 */
	std::string GetThumbnails(double sduration, double eduration);

	/**
	 *   @fn SetPausedBehavior
	 *
	 *   @param[in]  behavior paused behavior
	 */
	void SetPausedBehavior(int behavior);

	/**
	 * @fn InitAAMPConfig
	 */
	bool InitAAMPConfig(const char *jsonStr);

	/**
	 * @fn GetAAMPConfig
	 */
	std::string GetAAMPConfig();

	/**
	 *   @fn SetUseAbsoluteTimeline
	 *
	 *   @param[in] configState bool enable/disable configuration
	 */
	void SetUseAbsoluteTimeline(bool configState);

	/**
  	 *   @fn XRESupportedTune
   	 *   @param[in] xreSupported bool On/Off
	 	 */
	void XRESupportedTune(bool xreSupported);

	/**
	 *   @brief Enable async operation and initialize resources
	 *
	 *   @return void
	 */
	void EnableAsyncOperation();

	/**
	 *   @fn SetRepairIframes
	 *
	 *   @param[in] configState bool enable/disable configuration
	 */
	void SetRepairIframes(bool configState);

	/**
	 *   @fn SetAuxiliaryLanguage
	 *
	 *   @param[in] language - auxiliary language
	 *   @return void
	 */
	void SetAuxiliaryLanguage(const std::string &language);

	/**
	 *   @fn SetLicenseCustomData
	 *
	 *   @param[in]  customData - custom data string to be passed to the license server.
	 *   @return void
	 */
	void SetLicenseCustomData(const char *customData);

	/**
	 *   @brief To set default timeout for Dynamic ContentProtectionDataUpdate on Key Rotation.
	 *
	 *   @param[in] preferred timeout value in seconds
	 */
	void SetContentProtectionDataUpdateTimeout(int timeout);

	/**
	 * @fn ProcessContentProtectionDataConfig
	 *
	 * @param jsonbuffer Received DRM config for content protection data update event
	 */
	void ProcessContentProtectionDataConfig(const char *jsonbuffer);

	/**
	 * @fn EnableDynamicDRMConfigSupport
	 *
	 * @param[in] Enable/Disable Dynamic DRM config feature
	 */
	void SetRuntimeDRMConfigSupport(bool DynamicDRMSupported);

	/**
	 * @fn IsOOBCCRenderingSupported
	 *
	 * @return bool, True if Out of Band Closed caption/subtitle rendering supported
	 */
	bool IsOOBCCRenderingSupported();

	/**
	 *   @fn GetPlaybackStats
		 *
   	 *   @return json string representing the stats
  	 */
	std::string GetPlaybackStats();

	/**
	 *   @fn GetVideoPlaybackQuality
	 *
	 *   @return json string with video playback quality
	 */
	std::string GetVideoPlaybackQuality(void);

	/**
	 *  @brief Returns the session ID from the internal player, if present, or an empty string, if not.
	 */
	std::string GetSessionId() const;

	/**
	* @fn updateManifest
	*
	* @param Processed manifest data  from app
	*/
	void updateManifest(const char *manifestData);

protected:
		/**
		 *   @fn TuneInternal
		 *
		 *   @param  mainManifestUrl - HTTP/HTTPS url to be played.
		 *   @param[in] autoPlay - Start playback immediately or not
		 *   @param  contentType - content Type.
		 */
	void TuneInternal(const char *mainManifestUrl,
						bool autoPlay,
						const char *contentType,
						bool bFirstAttempt,
						bool bFinalAttempt,
						const char *traceUUID,
						bool audioDecoderStreamSync,
						const char *refreshManifestUrl = NULL,
						int mpdStitchingMode = 0,
						std::string sid = {},
						const char *manifestData = NULL );
	/**
		 *   @fn SetRateInternal
		 *
		 *   @param  rate - Rate of playback.
		 *   @param  overshootcorrection - overshoot correction in milliseconds.
		 */
	void SetRateInternal(float rate,int overshootcorrection);
	/**
		 *   @fn PauseAtInternal
		 *
		 *   @param[in]  secondsRelativeToTuneTime - Relative position from first tune command.
		 */
	void PauseAtInternal(double secondsRelativeToTuneTime);
	/**
		 *   @fn SeekInternal
		 *
		 *   @param  secondsRelativeToTuneTime - Seek position for VOD,
		 *           relative position from first tune command.
		 *   @param  keepPaused - set true if want to keep paused state after seek
		 */

	void SeekInternal(double secondsRelativeToTuneTime, bool keepPaused);
	/**
	 *   @fn SetAudioTrackInternal
	 *   @param[in] language, rendition, codec, channel
	 *   @return void
	 */
	void SetAudioTrackInternal(std::string language,  std::string rendition, std::string codec,  std::string type, unsigned int channel, std::string label);
	/**
	 *   @fn SetAuxiliaryLanguageInternal
	 *   @param[in][optional] language
	 *   @return void
	 */
	void SetAuxiliaryLanguageInternal(const std::string &language);
	/**
	 *   @fn SetTextTrackInternal
	 *   @param[in] trackId
	 *   @param[in] data
	 *   @return void
	 */
	void SetTextTrackInternal(int trackId, char *data);
	
private:

	/**
	 *   @fn StopInternal
	 *
	 *   @param[in]  sendStateChangeEvent - true if state change events need to be sent for Stop operation
	 *   @return void
	 */
	void StopInternal(bool sendStateChangeEvent);

	void* mJSBinding_DL;                /**< Handle to AAMP plugin dynamic lib.  */
	static std::mutex mPrvAampMtx;      /**< Mutex to protect aamp instance in GetState() */
	bool mAsyncRunning;                 /**< Flag denotes if async mode is on or not */
	bool mAsyncTuneEnabled;		    /**< Flag indicating async tune status */
	AampScheduler mScheduler;
};

#endif // MAINAAMP_H
