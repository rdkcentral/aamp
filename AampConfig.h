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
 * @file AampConfig.h
 * @brief Configurations for AAMP
 */
 
#ifndef __AAMP_CONFIG_H__
#define __AAMP_CONFIG_H__

#include <iostream>
#include <map>
#include <stdio.h>
#include <assert.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <signal.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <algorithm>
#include <string>
#include <vector>
#include <list>
#include <fstream>
#include <math.h>
#include <algorithm>
#include <ctype.h>
#include <sstream>
#include <curl/curl.h>
#include "AampDefine.h"
#include "AampLogManager.h"
#include <cjson/cJSON.h>
#include "DrmSystems.h"


//////////////// CAUTION !!!! STOP !!! Read this before you proceed !!!!!!! /////////////
/// 1. This Class handles Configuration Parameters of AAMP Player , only Config related functionality to be added
/// 2. Simple Steps to add a new configuration
///		a) Identify new configuration takes what value ( bool / int / long / string )
/// 	b) Add the new configuration string in README.txt with appropriate comment
///		c) Add a enum value for new config in AAMPConfigSettings. It should be inserted
///			at right place based on data type
/// 	d) Add the config string added in README and its equivalent enum value at the
///			end of ConfigLookUpTable
/// 	e) Go to AampConfig constructor and assign default value . Again the array to
///			store is based on the datatype of config
/// 	f) Thats it !! You added a new configuration . Use Set and Get function to
///			store and read value using enum config
/// 	g) IF any conversion required only (from config to usage, ex: sec to millisec ),
///			add specific Get function for each config
///			Not recommended . Better to have the conversion ( enum to string , sec to millisec etc ) where its consumed .
///////////////////////////////// Happy Configuration ////////////////////////////////////


#define ISCONFIGSET(x) (aamp->mConfig->IsConfigSet(x))
#define ISCONFIGSET_PRIV(x) (mConfig->IsConfigSet(x))
#define SETCONFIGVALUE(owner,key,value) (aamp->mConfig->SetConfigValue(owner, key ,value))
#define SETCONFIGVALUE_PRIV(owner,key,value) (mConfig->SetConfigValue(owner, key ,value))
#define GETCONFIGVALUE(key) (aamp->mConfig->GetConfigValue( key))
#define GETCONFIGVALUE_PRIV(key) (mConfig->GetConfigValue( key))
#define GETCONFIGOWNER(key) (aamp->mConfig->GetConfigOwner(key))
#define GETCONFIGOWNER_PRIV(key) (mConfig->GetConfigOwner(key))
/**
 * @brief AAMP Config Settings
 */
typedef enum
{
	eAAMPConfig_EnableABR,						/**< Enable/Disable adaptive bitrate logic*/
	eAAMPConfig_Fog, 							/**< Enable / Disable FOG*/
	eAAMPConfig_PrefetchIFramePlaylistDL,					/**< Enabled prefetching of I-Frame playlist*/
	eAAMPConfig_Throttle,							/**< Regulate output data flow*/
	eAAMPConfig_DemuxAudioBeforeVideo,					/**< Demux video track from HLS transport stream track mode*/
	eAAMPConfig_DisableEC3, 						/**< Disable DDPlus*/
	eAAMPConfig_DisableATMOS,						/**< Disable Dolby ATMOS*/
	eAAMPConfig_DisableAC4,							/**< Disable AC4 Audio */
	eAAMPConfig_StereoOnly,							/**< Enable Stereo Only playback, disables EC3/ATMOS.  */
	eAAMPConfig_DescriptiveTrackName,					/**< Enable Descriptive track name*/
	eAAMPConfig_DisableAC3,							/**< Disable AC3 Audio */
	eAAMPConfig_DisablePlaylistIndexEvent,					/**< Disable playlist index event*/
	eAAMPConfig_EnableSubscribedTags,					/**< Enabled subscribed tags*/
	eAAMPConfig_DASHIgnoreBaseURLIfSlash,					/**< Ignore the constructed URI of DASH, if it is / */
	eAAMPConfig_AnonymousLicenseRequest,					/**< Acquire license without token*/
	eAAMPConfig_HLSAVTrackSyncUsingStartTime,				/**< HLS A/V track to be synced with start time*/
	eAAMPConfig_MPDDiscontinuityHandling,					/**< Enable MPD discontinuity handling*/
	eAAMPConfig_MPDDiscontinuityHandlingCdvr,				/**< Enable MPD discontinuity handling for CDVR*/
	eAAMPConfig_ForceHttp,							/**< Force HTTP*/
	eAAMPConfig_InternalReTune, 						/**< Internal re-tune on underflows/ pts errors*/
	eAAMPConfig_AudioOnlyPlayback,						/**< AAMP Audio Only Playback*/
	eAAMPConfig_Base64LicenseWrapping,					/**< Encode and decode the license data in base64 format*/
	eAAMPConfig_GStreamerBufferingBeforePlay,				/**< Enable pre buffering logic which ensures minimum buffering is done before pipeline play*/
	eAAMPConfig_EnablePROutputProtection,					/**< Playready output protection config */
	eAAMPConfig_ReTuneOnBufferingTimeout,					/**< Re-tune on buffering timeout */
	eAAMPConfig_SslVerifyPeer,						/**< Enable curl ssl certificate verification. */
	eAAMPConfig_EnableClientDai,						/**< Enabling the client side DAI*/
	eAAMPConfig_PlayAdFromCDN,						/**< Play Ad from CDN. Not from FOG.*/
	eAAMPConfig_EnableVideoEndEvent,					/**< Enable or disable videoend events */
	eAAMPConfig_EnableRectPropertyCfg,					/**< To allow or deny rectangle property set for sink element*/
	eAAMPConfig_ReportVideoPTS, 						/**< Enables Video PTS reporting */
	eAAMPConfig_DecoderUnavailableStrict,					/**< Reports decoder unavailable GST Warning as aamp error*/
	eAAMPConfig_UseAppSrcForProgressivePlayback,				/**< Enables appsrc for playing progressive AV type */
	eAAMPConfig_DescriptiveAudioTrack,					/**< advertise audio tracks using <langcode>-<role> instead of just <langcode> */
	eAAMPConfig_ReportBufferEvent,						/**< Enables Buffer event reporting */
	eAAMPConfig_InfoLogging,						/**< Enables Info logging */
	eAAMPConfig_DebugLogging,						/**< Enables Debug logging */
	eAAMPConfig_TraceLogging,						/**< Enables Trace logging */
	eAAMPConfig_WarnLogging,						/**< Enables Warn logging */
	eAAMPConfig_FailoverLogging,						/**< Enables failover logging */
	eAAMPConfig_GSTLogging,							/**< Enables Gstreamer logging */
	eAAMPConfig_ProgressLogging,						/**< Enables Progress logging */
	eAAMPConfig_CurlLogging,						/**< Enables Curl logging */
	eAAMPConfig_CurlLicenseLogging, 					/**< Enable verbose curl logging for license request (non-secclient) */
	eAAMPConfig_MetadataLogging,						/**< Enable timed metadata logging */
	eAAMPConfig_CurlHeader, 						/**< enable curl header response logging on curl errors*/
	eAAMPConfig_StreamLogging,						/**< Enables HLS Playlist content logging */
	eAAMPConfig_ID3Logging,        						/**< Enables ID3 logging */
	eAAMPConfig_EnableGstPositionQuery, 					/**< GStreamer position query will be used for progress report events */
	eAAMPConfig_MidFragmentSeek,                                            /**< Enable/Disable the Mid-Fragment seek functionality in aamp.*/
	eAAMPConfig_PropagateURIParam,						/**< Feature where top-level manifest URI parameters included when downloading fragments*/
	eAAMPConfig_UseWesterosSink, 						/**< Enable/Disable player to use westeros sink based video decoding */
	eAAMPConfig_RetuneForUnpairDiscontinuity,				/**< disable unpaired discontinuity retune functionality*/
	eAAMPConfig_RetuneForGSTError,						/**< disable retune mitigation for gst pipeline internal data stream error*/
	eAAMPConfig_MatchBaseUrl,						/**< Enable host of main url will be matched with host of base url*/
	eAAMPConfig_WifiCurlHeader,
	eAAMPConfig_EnableSeekRange,						/**< Enable seekable range reporting via progress events */
	eAAMPConfig_EnableLiveLatencyCorrection,            /**< Enable the live latency (drift) correction by adjusting the playback speed */
	eAAMPConfig_DashParallelFragDownload,					/**< Enable dash fragment parallel download*/
	eAAMPConfig_PersistentBitRateOverSeek,					/**< ABR profile persistence during Seek/Trickplay/Audio switching*/
	eAAMPConfig_SetLicenseCaching,						/**< License caching*/
	eAAMPConfig_Fragmp4PrefetchLicense,					/*** Enable fragment mp4 license prefetching**/
	eAAMPConfig_ABRBufferCheckEnabled,					/**< Flag to enable/disable buffer based ABR handling*/
	eAAMPConfig_NewDiscontinuity,						/**< Flag to enable/disable new discontinuity handling with PDT*/
	eAAMPConfig_BulkTimedMetaReport, 					/**< Enabled Bulk event reporting for TimedMetadata*/
	eAAMPConfig_BulkTimedMetaReportLive,					/**< Enabled Bulk TimedMetadata event reporting for live stream */
	eAAMPConfig_AvgBWForABR,						/**< Enables usage of AverageBandwidth if available for ABR */
	eAAMPConfig_NativeCCRendering,						/**< If native CC rendering to be supported */
	eAAMPConfig_Subtec_subtitle,						/**< Enable subtec-based subtitles */
	eAAMPConfig_WebVTTNative,						/**< Enable subtec-based subtitles */
	eAAMPConfig_AsyncTune,						 	/**< To enable Asynchronous tune */
	eAAMPConfig_DisableUnderflow,                                           /**< Enable/Disable Underflow processing*/
	eAAMPConfig_LimitResolution,                                            /**< Flag to indicate if display resolution based profile selection to be done */
	eAAMPConfig_UseAbsoluteTimeline,					/**< Enable Report Progress report position based on Availability Start Time **/
	eAAMPConfig_EnableAccessAttributes,					/**< Usage of Access Attributes in VSS */
	eAAMPConfig_WideVineKIDWorkaround,                         		/**< partner-specific workaround to use WV DRM KeyId from alternate location */
	eAAMPConfig_RepairIframes,						/**< Enable fragment repair (Stripping and box size correction) for iframe tracks */
	eAAMPConfig_SEITimeCode,						/**< Enables SEI Time Code handling */
	eAAMPConfig_Disable4K,							/**< Enable/Disable 4K stream support*/
	eAAMPConfig_EnableSharedSSLSession,                                     /**< Enable/Disable config for shared ssl session reuse */
	eAAMPConfig_InterruptHandling,						/**< Enables Config for network interrupt handling*/
	eAAMPConfig_EnableLowLatencyDash,                           		/**< Enables Low Latency Dash */
	eAAMPConfig_DisableLowLatencyABR,					/**< Enables Low Latency ABR handling */
	eAAMPConfig_EnableLowLatencyCorrection,                    		/**< Enables Low Latency Correction handling */
	eAAMPConfig_EnableLowLatencyOffsetMin,                                  /**< Enables Low Latency Offset Min handling */
	eAAMPConfig_SyncAudioFragments,						/**< Flag to enable Audio Video Fragment Sync */
	eAAMPConfig_EnableIgnoreEosSmallFragment,                               /**< Enable/Disable Small fragment ignore based on minimum duration Threshold at period End*/
	eAAMPConfig_UseSecManager,                                              /**< Enable/Disable secmanager instead of secclient for license acquisition */
	eAAMPConfig_EnablePTO,								/**< Enable/Disable PTO Handling */
	eAAMPConfig_EnableAampConfigToFog,                                      /**< Enable/Disable player config to Fog on every tune*/
	eAAMPConfig_XRESupportedTune,						/**< Enable/Disable XRE supported tune*/
	eAAMPConfig_GstSubtecEnabled,								/**< Force Gstreamer subtec */
	eAAMPConfig_AllowPageHeaders,						/**< Allow page http headers*/
	eAAMPConfig_PersistHighNetworkBandwidth,				/** Flag to enable Persist High Network Bandwidth across Tunes */
	eAAMPConfig_PersistLowNetworkBandwidth,					/** Flag to enable Persist Low Network Bandwidth across Tunes */
	eAAMPConfig_ChangeTrackWithoutRetune,					/**< Flag to enable audio track change without disturbing video pipeline */
	eAAMPConfig_EnableCurlStore,						/**< Enable/Disable CurlStore to save/reuse curl fds */
	eAAMPConfig_RuntimeDRMConfig,                                           /**< Enable/Disable Dynamic DRM config feature */
	eAAMPConfig_EnablePublishingMuxedAudio,					/**< Enable/Disable publishing the audio track info from muxed contents */
	eAAMPConfig_EnableCMCD,							/**< Enable/Disable CMCD config feature */
	eAAMPConfig_EnableSlowMotion,						/**< Enable/Disable Slowmotion playback */
	eAAMPConfig_EnableSCTE35PresentationTime,				/**< Enable/Disable use of SCTE PTS presentation time */
	eAAMPConfig_JsInfoLogging,						/**< Enable/disable jsinfo logging       */
	eAAMPConfig_IgnoreAppLiveOffset,					/**< Config to ignore the liveOffset from App for LLD */
	eAAMPConfig_useTCPServerSink,						/**< Route audio/video to tcpserversink, suppressing decode and presentation */
	eAAMPConfig_enableDisconnectSignals,			/** When enabled (true which is the default) gstreamer signals are disconnected in AAMPGstPlayer::DisconnectSignals()*/
	eAAMPConfig_SendLicenseResponseHeaders,					/**<Config to enable adding license response headers with drm metadata event */
	eAAMPConfig_SuppressDecode,					/**< To Suppress Decode of segments for playback . Test only Downloader */
	eAAMPConfig_ReconfigPipelineOnDiscontinuity,				/*** Enable/Disable reconfigure pipeline on discontinuity */
	eAAMPConfig_EnableMediaProcessor,					/** <Config to enable injection through MediaProcessor */
	eAAMPConfig_MPDStitchingSupport,					/**< To enable/disable MPD Stich functionality in the player. Default enabled */
	eAAMPConfig_SendUserAgent,						/**< To enable/disable sending user agent in the DRM license request header. Default enabled */
	eAAMPConfig_EnablePTSReStamp,					/** <Config to enable PTS restamping */
	eAAMPConfig_TrackMemory,					/**< To enable/disable AampGrowableBuffer track memory */
	eAAMPConfig_UseSinglePipeline,					/**< To enable/disable using a single gstreamer pipeline */
	eAAMPConfig_EarlyID3Processing,					/**< To enable/disable early ID3 processing */
	eAAMPConfig_SeamlessAudioSwitch,					/**< To enable audio Restart - Currently supported for HLS_MP4 on same codec streams*/
	eAAMPConfig_useRialtoSink,                      /**< Enable/Disable player to use Rialto sink based video and audio pipeline */
	eAAMPConfig_LocalTSBEnabled,                                            /**< To enable/disable Local TSB in LLD */
	eAAMPConfig_EnableIFrameTrackExtract,			/**< Config to enable and disable iFrame extraction from video track*/
	eAAMPConfig_ForceMultiPeriodDiscontinuity,		/**< Config to forcefully process multiperiod discontinuity even if they are continuous in PTS */
	eAAMPConfig_ForceLLDFlow,						/**< Config to forcefully process LLD workflow even if they are live SLD */
	eAAMPConfig_MonitorAV,						/**< enable background monitoring of audio/video positions to infer video freeze, audio drop, or av sync issues */
	eAAMPConfig_HlsTsEnablePTSReStamp,
	eAAMPConfig_OverrideMediaHeaderDuration, /**< enable overriding media header duration for live streams to 0 */
	eAAMPConfig_UseMp4Demux,
	eAAMPConfig_CurlThroughput,
	eAAMPConfig_UseFireboltSDK,						/**< Config to use Firebolt SDK for license Acquisition */
	eAAMPConfig_EnableChunkInjection,					/**< Config to enable chunk injection for low latency DASH */
	eAAMPConfig_BoolMaxValue						/**< Max value of bool config always last element */

} AAMPConfigSettingBool;
#define AAMPCONFIG_BOOL_COUNT (eAAMPConfig_BoolMaxValue)

typedef enum
{
	eAAMPConfig_HarvestCountLimit,						/**< Number of files to be harvested */
	eAAMPConfig_HarvestConfig,						/**< Indicate type of file to be  harvest */
	eAAMPConfig_ABRCacheLife,						/**< Adaptive bitrate cache life in seconds*/
	eAAMPConfig_ABRCacheLength,						/**< Adaptive bitrate cache length*/
	eAAMPConfig_TimeShiftBufferLength,					/**< TSB length*/
	eAAMPConfig_ABRCacheOutlier,						/**< Adaptive bitrate outlier, if values goes beyond this*/
	eAAMPConfig_ABRSkipDuration,						/**< Initial duration for ABR skip*/
	eAAMPConfig_ABRNWConsistency,						/**< Adaptive bitrate network consistency*/
	eAAMPConfig_ABRThresholdSize,						/**< AAMP ABR threshold size*/
	eAAMPConfig_MaxFragmentCached,						/**< fragment cache length*/
	eAAMPConfig_BufferHealthMonitorDelay,					/**< Buffer health monitor start delay after tune/ seek*/
	eAAMPConfig_BufferHealthMonitorInterval,				/**< Buffer health monitor interval*/
	eAAMPConfig_PreferredDRM,						/**< Preferred DRM*/
	eAAMPConfig_TuneEventConfig,						/**< When to send TUNED event*/
	eAAMPConfig_VODTrickPlayFPS,						/**< Trickplay frames per second for VOD*/
	eAAMPConfig_LinearTrickPlayFPS,						/**< Trickplay frames per second for Linear*/
	eAAMPConfig_LicenseRetryWaitTime,					/**< License retry wait interval*/
	eAAMPConfig_LicenseKeyAcquireWaitTime,				/**< License key acquire wait time*/
	eAAMPConfig_PTSErrorThreshold,						/**< Max number of back-to-back PTS errors within designated time*/
	eAAMPConfig_MaxPlaylistCacheSize,					/**< Max Playlist Cache Size  */
	eAAMPConfig_MaxDASHDRMSessions,						/**< Max drm sessions that can be cached by AampDRMSessionManager*/
	eAAMPConfig_Http5XXRetryWaitInterval,					/**< Wait time in milliseconds before retry for 5xx errors*/
	eAAMPConfig_LanguageCodePreference,					/**< preferred format for normalizing language code */
	eAAMPConfig_RampDownLimit, 						/**< Set fragment rampdown/retry limit for video fragment failure*/
	eAAMPConfig_InitRampDownLimit,						/**< Maximum number of rampdown/retries for initial playlist retrieval at tune/seek time*/
	eAAMPConfig_DRMDecryptThreshold,					/**< Retry count on drm decryption failure*/
	eAAMPConfig_SegmentInjectThreshold, 					/**< Retry count for segment injection discard/failure*/
	eAAMPConfig_InitFragmentRetryCount, 					/**< Retry attempts for init frag curl timeout failures*/
	eAAMPConfig_MinABRNWBufferRampDown, 					/**< Minimum ABR Buffer for Rampdown*/
	eAAMPConfig_MaxABRNWBufferRampUp,					/**< Maximum ABR Buffer for Rampup*/
	eAAMPConfig_PrePlayBufferCount, 					/**< Count of segments to be downloaded until play state */
	eAAMPConfig_PreCachePlaylistTime,					/**< Max time to complete PreCaching .In Minutes  */
	eAAMPConfig_CEAPreferred,						/**< To force 608/708 track selection in CC manager */
	eAAMPConfig_StallErrorCode,
	eAAMPConfig_StallTimeoutMS,
	eAAMPConfig_InitialBuffer,
	eAAMPConfig_PlaybackBuffer,
	eAAMPConfig_SourceSetupTimeout, 					/**<Timeout value wait for GStreamer appsource setup to complete*/
	eAAMPConfig_DownloadDelay,
	eAAMPConfig_LivePauseBehavior,                                          /**< player paused state behavior */
	eAAMPConfig_GstVideoBufBytes,                                           /**< Gstreamer Max Video buffering bytes*/
	eAAMPConfig_GstAudioBufBytes,                                           /**< Gstreamer Max Audio buffering bytes*/
	eAAMPConfig_LatencyMonitorDelay,               				/**< Latency Monitor Delay */
	eAAMPConfig_LatencyMonitorInterval,           				/**< Latency Monitor Interval */
	eAAMPConfig_MaxFragmentChunkCached,           				/**< fragment chunk cache length*/
	eAAMPConfig_ABRChunkThresholdSize,                			/**< AAMP ABR Chunk threshold size*/
	eAAMPConfig_LLMinLatency,						/**< Low Latency Min Latency Offset */
	eAAMPConfig_LLTargetLatency,						/**< Low Latency Target Latency */
	eAAMPConfig_LLMaxLatency,						/**< Low Latency Max Latency */
	eAAMPConfig_FragmentDownloadFailThreshold, 				/**< Retry attempts for non-init fragment curl timeout failures*/
	eAAMPConfig_MaxInitFragCachePerTrack,					/**< Max no of Init fragment cache per track */
	eAAMPConfig_FogMaxConcurrentDownloads,                                  /**< Concurrent download posted to fog from player*/
	eAAMPConfig_ContentProtectionDataUpdateTimeout,				/**< Default Timeout For ContentProtectionData Update in milliseconds */
	eAAMPConfig_MaxCurlSockStore,						/**< Max no of curl socket to be stored */
	eAAMPConfig_TCPServerSinkPort,						/**< TCP port number */
	eAAMPConfig_DefaultBitrate,						/**< Default bitrate*/
	eAAMPConfig_DefaultBitrate4K,						/**< Default 4K bitrate*/
	eAAMPConfig_IFrameDefaultBitrate,					/**< Default bitrate for iframe track selection for non-4K assets*/
	eAAMPConfig_IFrameDefaultBitrate4K,					/**< Default bitrate for iframe track selection for 4K assets*/
	eAAMPConfig_CurlStallTimeout,						/**< Timeout value for detection curl download stall in seconds*/
	eAAMPConfig_CurlDownloadStartTimeout,					/**< Timeout value for curl download to start after connect in seconds*/
	eAAMPConfig_CurlDownloadLowBWTimeout,					/**< Timeout value for curl download expiry if player can't catchup the selected bitrate buffer*/
	eAAMPConfig_DiscontinuityTimeout,					/**< Timeout value to auto process pending discontinuity after detecting cache is empty*/
	eAAMPConfig_MinBitrate,                         			/**< minimum bitrate filter for playback profiles */
	eAAMPConfig_MaxBitrate,                         			/**< maximum bitrate filter for playback profiles*/
	eAAMPConfig_TLSVersion,							/**< TLS Version value*/
	eAAMPConfig_DrmNetworkTimeout,                                          /**< DRM license request timeout in sec*/
	eAAMPConfig_DrmStallTimeout,                                            /**< Stall Timeout for DRM license request*/
	eAAMPConfig_DrmStartTimeout,						/**< Start Timeout for DRM license request*/
	eAAMPConfig_TimeBasedBufferSeconds,
	eAAMPConfig_TelemetryInterval,						/**< time interval for the telemetry reporting*/
	eAAMPConfig_RateCorrectionDelay,			/**< Delay Rate Correction upon discontinuity in seconds */
	eAAMPConfig_HarvestDuration,						/**< Harvest  duration time */
	eAAMPConfig_SubtitleClockSyncInterval,			/**< time interval for synchronizing subtitle clock */
	eAAMPConfig_PreferredAbsoluteProgressReporting, /**< Preferred settings for absolute progress reporting**/
	eAAMPConfig_EOSInjectionMode,				/**< Determines when EOS is injected. See definition of EOSInjectionModeCode.*/
	eAAMPConfig_ABRBufferCounter,				/** Counter for ABR steadystate rampup/rampdown*/
	eAAMPConfig_TsbLength,                         /** TSB duration for local storage */
	eAAMPConfig_TsbMinDiskFreePercentage,					/**< Minimum percentage of storage to be kept free while storing TSB data */
	eAAMPConfig_TsbMaxDiskStorage,					/** TSB max storage in MB */
	eAAMPConfig_TsbLogLevel,					/** Override the TSB log level */
	eAAMPConfig_AdFulfillmentTimeout,					/**< Ad fulfillment timeout in milliseconds */
	eAAMPConfig_AdFulfillmentTimeoutMax,					/**< Ad fulfillment maximum timeout in milliseconds */
	eAAMPConfig_ShowDiagnosticsOverlay,		       /** configures the diagnostics overlay,accessed by UVE API getConfiguration()*/
	eAAMPConfig_MonitorAVSyncThresholdPositive,				/**< (positive) milliseconds threshold for video ahead of audio to be considered as unacceptable avsync*/
	eAAMPConfig_MonitorAVSyncThresholdNegative,				/**< (negative) milliseconds threshold for video behind audio to be considered as unacceptable avsync*/
	eAAMPConfig_MonitorAVJumpThreshold,				/**< configures threshold aligned audio,video positions advancing together by unexpectedly large delta to be reported as jump in milliseconds*/
	eAAMPConfig_ProgressLoggingDivisor,				/**<  Divisor to avoid printing the progress report too frequently in the log */
	eAAMPConfig_MonitorAVReportingInterval,			/**< Timeout in milliseconds for reporting MonitorAV events */
	eAAMPConfig_IntMaxValue							/**< Max value of int config always last element*/
} AAMPConfigSettingInt;
#define AAMPCONFIG_INT_COUNT (eAAMPConfig_IntMaxValue)

typedef enum
{
	eAAMPConfig_NetworkTimeout,						/**< Fragment download timeout in sec*/
	eAAMPConfig_ManifestTimeout,						/**< Manifest download timeout in sec*/
	eAAMPConfig_PlaylistTimeout,						/**< playlist download time out in sec*/
	eAAMPConfig_ReportProgressInterval,					/**< Interval of progress reporting*/
	eAAMPConfig_PlaybackOffset,						/**< playback offset value in seconds*/
	eAAMPConfig_LiveOffset, 						/**< Current LIVE offset*/
	eAAMPConfig_LiveOffsetDriftCorrectionInterval,  			/**< Config to override the allowed live offset drift **/
	eAAMPConfig_LiveOffset4K,						/**< Live offset for 4K content;*/
	eAAMPConfig_CDVRLiveOffset, 						/**< CDVR LIVE offset*/
	eAAMPConfig_Curl_ConnectTimeout,					/**< Curl timeout for the connect phase*/
	eAAMPConfig_Dns_CacheTimeout, 						/**< Curl life-time for DNS cache entries*/
	eAAMPConfig_MinLatencyCorrectionPlaybackRate,       /**< Latency adjust/buffer correction min playback rate*/
	eAAMPConfig_MaxLatencyCorrectionPlaybackRate,       /**< Latency correction max playback rate*/
	eAAMPConfig_NormalLatencyCorrectionPlaybackRate,    /**< Nomral playback rate for LLD stream; backdoor for debug*/
	eAAMPConfig_LowLatencyMinBuffer,                    /**< Low Latency minimum buffer value*/
	eAAMPConfig_LowLatencyTargetBuffer,                 /**< Low Latency target buffer value; Buffer needed for rate correction to trigger*/
	eAAMPConfig_BWToGstBufferFactor,				/**< Factor by multiply GST Base Buffer is multiplied to accommodate HiFi Content*/
	eAAMPConfig_FloatMaxValue						/**< Max value for float config always last element*/
} AAMPConfigSettingFloat;
#define AAMPCONFIG_FLOAT_COUNT (eAAMPConfig_FloatMaxValue)

typedef enum
{
	eAAMPConfig_HarvestPath,						/**< Path to store Harvested files */
	eAAMPConfig_LicenseServerUrl,						/**< License server URL ( if no individual configuration */
	eAAMPConfig_CKLicenseServerUrl,						/**< ClearKey License server URL*/
	eAAMPConfig_PRLicenseServerUrl,						/**< PlayReady License server URL*/
	eAAMPConfig_WVLicenseServerUrl,						/**< Widevine License server URL*/
	eAAMPConfig_UserAgent,							/**< Curl user-agent string */
	eAAMPConfig_SubTitleLanguage,						/**< User preferred subtitle language*/
	eAAMPConfig_CustomHeader,						/**< custom header string data to be appended to curl request*/
	eAAMPConfig_URIParameter,						/**< uri parameter data to be appended on download-url during curl request*/
	eAAMPConfig_NetworkProxy,						/**< Network Proxy */
	eAAMPConfig_LicenseProxy,						/**< License Proxy */
	eAAMPConfig_AuthToken,							/**< Session Token  */
	eAAMPConfig_LogLevel,							/**< New Configuration to overide info/debug/trace */
	eAAMPConfig_CustomHeaderLicense,                       			/**< custom header string data to be appended to curl License request*/
	eAAMPConfig_PreferredAudioRendition,					/**< New Configuration to save preferred Audio rendition/role descriptor field; support only single string value*/
	eAAMPConfig_PreferredAudioCodec,					/**< New Configuration to save preferred Audio codecs values; support comma separated multiple string values*/
	eAAMPConfig_PreferredAudioLanguage,					/**< New Configuration to save preferred Audio languages; support comma separated multiple string values*/
	eAAMPConfig_PreferredAudioLabel,					/**< New Configuration to save preferred Audio label field; Label is a textual description of the content. Support only single string value*/ 
	eAAMPConfig_PreferredAudioType,						/**< New Configuration to save preferred Audio Type field; type indicate the accessibility type of audio track*/ 
	eAAMPConfig_PreferredTextRendition,					/**< New Configuration to save preferred Text rendition/role descriptor field; support only single string value*/
	eAAMPConfig_PreferredTextLanguage,					/**<  New Configuration to save preferred Text languages; support comma separated multiple string values*/
	eAAMPConfig_PreferredTextLabel,						/**< New Configuration to save preferred Text label field; Label is a textual description of the content. Support only single string value*/
	eAAMPConfig_PreferredTextType,						/**< New Configuration to save preferred Text Type field; type indicate the accessibility type of text track*/
	eAAMPConfig_CustomLicenseData,                      /**< Custom Data for License Request */
	eAAMPConfig_SchemeIdUriDaiStream,					/**< Scheme Id URI String for DAI Stream */
	eAAMPConfig_SchemeIdUriVssStream,					/**< Scheme Id URI String for VSS Stream */
	eAAMPConfig_LRHAcceptValue,							/**< Custom License Request Header Data */
	eAAMPConfig_LRHContentType,							/**< Custom License Request ContentType Data */
	eAAMPConfig_GstDebugLevel,							/**< gstreamer debug level as you'd define in GST_DEBUG */
	eAAMPConfig_TsbType,
	eAAMPConfig_TsbLocation,                                                        /**< tsbType location for local TSB storage*/
	eAAMPConfig_StringMaxValue						/**< Max value for string config always last element */
} AAMPConfigSettingString;
#define AAMPCONFIG_STRING_COUNT (eAAMPConfig_StringMaxValue)

/**
 * @struct ConfigChannelInfo
 * @brief Holds information of a channel
 */
struct ConfigChannelInfo
{
	ConfigChannelInfo() : name(), uri(), licenseUri()
	{
	}
	std::string name;
	std::string uri;
	std::string licenseUri;
};

/**
 * @struct customJson
 * @brief Holds information of a custom JSON array
 */

struct customJson
{
        customJson() : config(), configValue()
        { }
        std::string config;
        std::string configValue;
};

/**
 * @struct AampOwnerLookupEntry
 * @brief AAMP Config ownership enum string mapping table
 */
struct AampOwnerLookupEntry
{
	const char* ownerName;
	ConfigPriority ownerValue;
};

/**
 * @struct ConfigValueBool
 * @brief AAMP Config Boolean data type
 */
typedef struct ConfigValueBool
{
	ConfigPriority owner;
	bool value;
	ConfigPriority lastowner;
	bool lastvalue;
	ConfigValueBool():owner(AAMP_DEFAULT_SETTING),value(false),lastowner(AAMP_DEFAULT_SETTING),lastvalue(false){}
} ConfigValueBool;

/**
 * @brief AAMP Config Int data type
 */
typedef struct ConfigValueInt
{
	ConfigPriority owner;
	int value;
	ConfigPriority lastowner;
	int lastvalue;
	ConfigValueInt():owner(AAMP_DEFAULT_SETTING),value(0),lastowner(AAMP_DEFAULT_SETTING),lastvalue(0){}
} ConfigValueInt;

/**
 * @brief AAMP Config double data type
 */
typedef struct ConfigValueFloat
{
    ConfigPriority owner;
    double value;
    ConfigPriority lastowner;
    double lastvalue;
    ConfigValueFloat():owner(AAMP_DEFAULT_SETTING),value(0),lastowner(AAMP_DEFAULT_SETTING),lastvalue(0){}
} ConfigValueFloat;

/**
 * @brief AAMP Config String data type
 */
typedef struct ConfigValueString
{
	ConfigPriority owner;
	std::string value;
	ConfigPriority lastowner;
	std::string lastvalue;
	ConfigValueString():owner(AAMP_DEFAULT_SETTING),value(""),lastowner(AAMP_DEFAULT_SETTING),lastvalue(""){}
} ConfigValueString;

/**
 * @class AampConfig
 * @brief AAMP Config Class defn
 */
class AampConfig
{
public:
	/**
    	 * @fn AampConfig
    	 *
    	 * @return None
    	 */
	AampConfig();
	/**
         * @brief AampConfig Distructor function
         *
         * @return None
         */
	~AampConfig(){};
	/**
         * @brief Copy constructor disabled
         *
         */
	AampConfig(const AampConfig&) = delete;
	/**
     	 * @fn operator= 
     	 *
     	 * @return New Config instance with copied values
     	 */
	AampConfig& operator=(const AampConfig&);
	void Initialize();
	/**
	 * @fn ApplyDeviceCapabilities
	 * @return Void
	 */
	void ApplyDeviceCapabilities();
	/**
     	 * @fn ShowOperatorSetConfiguration
     	 * @return Void
     	 */
	void ShowOperatorSetConfiguration();
	/**
     	 * @fn ShowAppSetConfiguration
     	 * @return void
     	 */
	void ShowAppSetConfiguration();
	/**
     	 * @fn ShowStreamSetConfiguration
	 *
     	 * @return Void
     	 */
	void ShowStreamSetConfiguration();
	/**
     	 * @fn ShowDefaultAampConfiguration 
     	 *
     	 * @return Void
     	 */
	void ShowDefaultAampConfiguration();	
	/**
     	 *@fn ShowDevCfgConfiguration
	 *
     	 * @return Void
     	 */
	void ShowDevCfgConfiguration();
	/**
     	 * @fn ShowAAMPConfiguration
     	 *
     	 * @return Void
     	 */
	void ShowAAMPConfiguration();
	/**
     	 * @fn ReadAampCfgTxtFile
	 *
     	 * @return Void
     	 */
	bool ReadAampCfgTxtFile();

	/**
	* @fn ReadAampCfgFromEnv
	*
	* @return Void
	*/
	void ReadAampCfgFromEnv();

	/**
	* @fn ProcessBase64AampCfg
	* @return bool
	*/
	bool ProcessBase64AampCfg(const char * base64Config, size_t configLen,ConfigPriority cfgPriority);

	/**
     	 * @fn ReadAampCfgJsonFile
    	 */
	bool ReadAampCfgJsonFile();

	/**
     	 * @fn ReadOperatorConfiguration
     	 * @return void
     	 */
	void ReadOperatorConfiguration();
	/**
	 * @fn ReadBase64TR181Param reads Tr181 parameter at Device.DeviceInfo.X_RDKCENTRAL-COM_RFC.Feature.AAMP_CFG.b64Config
	 * @return void
	 */
	void ReadBase64TR181Param();
	/**
	 * @fn ReadAllTR181Params reads  All Tr181 parameters at Device.DeviceInfo.X_RDKCENTRAL-COM_RFC.Feature.AAMP_CFG.<param-name>
	 * @return void
	 */
	void ReadAllTR181Params();
	/**
         * @brief ParseAampCfgTxtString - It parses the aamp configuration 
         *
         * @return Void
         */
	void ParseAampCfgTxtString(std::string &cfg);
	/**
         * @brief ParseAampCfgJsonString - It parses the aamp configuration from json format
         *
         * @return Void
         */
	void ParseAampCfgJsonString(std::string &cfg);	
	
	/**
     	 * @fn SetConfigValue
     	 * @param[in] owner  - ownership of new set call
     	 * @param[in] cfg	- Configuration enum to set
     	 * @param[in] value   - value to set
     	 */
	void SetConfigValue(ConfigPriority owner, AAMPConfigSettingBool cfg , const bool &value);
	void SetConfigValue(ConfigPriority owner, AAMPConfigSettingInt cfg , const int &value);
	void SetConfigValue(ConfigPriority owner, AAMPConfigSettingFloat cfg , const double &value);
	void SetConfigValue(ConfigPriority owner, AAMPConfigSettingString cfg , const std::string &value);
	/**
     	 * @fn IsConfigSet
     	 *
     	 * @param[in] cfg - Configuration enum
     	 * @return true / false 
     	 */
	bool IsConfigSet(AAMPConfigSettingBool cfg) const;
	bool GetConfigValue( AAMPConfigSettingBool cfg ) const;
	int GetConfigValue( AAMPConfigSettingInt cfg ) const;
	double GetConfigValue( AAMPConfigSettingFloat cfg ) const;
	std::string GetConfigValue( AAMPConfigSettingString cfg ) const;
	
	ConfigPriority GetConfigOwner(AAMPConfigSettingBool cfg) const;
	ConfigPriority GetConfigOwner(AAMPConfigSettingInt cfg) const;
	ConfigPriority GetConfigOwner(AAMPConfigSettingFloat cfg) const;
	ConfigPriority GetConfigOwner(AAMPConfigSettingString cfg) const;
	
 	/**
     	 * @fn GetChannelOverride
     	 * @param[in] chName - channel name to search
     	 */
	const char * GetChannelOverride(const std::string chName) const;
 	/**
     	 * @fn GetChannelLicenseOverride
     	 * @param[in] chName - channel Name to override
     	 */
 	const char * GetChannelLicenseOverride(const std::string chName) const;

	/**
         * @fn ProcessConfigJson
         * @param[in] cfg - json format
         * @param[in] owner   - Owner who is setting the value
         */
	bool ProcessConfigJson(const cJSON *cfgdata, ConfigPriority owner );
	/**
     	 * @fn ProcessConfigText
     	 * @param[in] cfg - config text ( new line separated)
     	 * @param[in] owner   - Owner who is setting the value
     	 */
	void ProcessConfigText(std::string &cfg, ConfigPriority owner );
	/**
     	 * @fn RestoreConfiguration
     	 * @param[in] owner - Owner value for reverting
     	 * @return None
     	 */
	void RestoreConfiguration(ConfigPriority owner);
	/**
     	 * @fn RestoreConfiguration
     	 * @param[in] owner - Restore from this owner to previous owner
     	 * @param[in] cfg - Config value for restoring
     	 * @return None
		 */
	void RestoreConfiguration(ConfigPriority owner, AAMPConfigSettingBool cfg);
	/**
     	 * @fn RestoreConfiguration
     	 * @param[in] owner - Restore from this owner to previous owner
     	 * @param[in] cfg - Config value for restoring
     	 * @return None
		 */
	void RestoreConfiguration(ConfigPriority owner, AAMPConfigSettingInt cfg);
	/**
     	 * @fn RestoreConfiguration
     	 * @param[in] owner - Restore from this owner to previous owner
     	 * @param[in] cfg - Config value for restoring
     	 * @return None
		 */
	void RestoreConfiguration(ConfigPriority owner, AAMPConfigSettingFloat cfg);
	/**
     	 * @fn RestoreConfiguration
     	 * @param[in] owner - Restore from this owner to previous owner
     	 * @param[in] cfg - Config value for restoring
     	 * @return None
		 */
	void RestoreConfiguration(ConfigPriority owner, AAMPConfigSettingString cfg);
	/**
     	 * @fn ConfigureLogSettings
     	 * @return None
     	 */
	void ConfigureLogSettings();
	/**
     	 * @fn GetAampConfigJSONStr
     	 * @param[in] str  - input string where config json will be stored
     	 */
	bool GetAampConfigJSONStr(std::string &str) const;
	/**
     	 * @fn DoCustomSetting 
     	 *
	 * @param[in] owner - ConfigPriority owner
     	 * @return None
     	 */
	void DoCustomSetting(ConfigPriority owner);

	/**
     	 * @fn CustomSearch
     	 * @param[in] url  - input string where url name will be stored
     	 * @param[in] playerId  - input int variable where playerId will be stored
     	 * @param[in] appname  - input string where appname will be stored
     	 */
	bool CustomSearch( std::string url, int playerId , std::string appname);

	std::string GetUserAgentString() const;
private:

	/**
     	 * @fn SetValue
     	 *
     	 * @param[in] setting - Config variable to set
     	 * @param[in] newowner - New owner value
     	 * @param[in] value - Value to set
       	 * @return void
    	 */
	template<class J,class K>
	void SetValue(J &setting, ConfigPriority newowner, const K &value,std::string cfgName);
	void trim(std::string& src);
	
	void ShowConfiguration(ConfigPriority owner);	
	/**
     	 * @fn GetConfigName
     	 * @param[in] cfg  - configuration enum
     	 * @return string - configuration name
     	 */

	/**
	 * @fn CustomArrayRead
		 * @param[in] customArray - input string where custom config json will be stored
		 * @param[in] owner - ownership of configs will be stored
		 */
	void CustomArrayRead( cJSON *customArray,ConfigPriority owner );

	const char * GetConfigName(AAMPConfigSettingBool cfg ) const;
	const char * GetConfigName(AAMPConfigSettingInt cfg ) const;
	const char * GetConfigName(AAMPConfigSettingFloat cfg ) const;
	const char * GetConfigName(AAMPConfigSettingString cfg ) const;
	
	std::vector<struct customJson>vCustom;
	std::vector<struct customJson>::iterator vCustomIt;
	bool customFound;
	
	ConfigValueBool configValueBool[AAMPCONFIG_BOOL_COUNT];
	ConfigValueInt configValueInt[AAMPCONFIG_INT_COUNT];
	ConfigValueFloat configValueFloat[AAMPCONFIG_FLOAT_COUNT];
	ConfigValueString configValueString[AAMPCONFIG_STRING_COUNT];
	
	typedef std::list<ConfigChannelInfo> ChannelMap;
	ChannelMap mChannelOverrideMap;
};

/**
 * @brief Global configuration */
extern AampConfig  *gpGlobalConfig;
#endif



