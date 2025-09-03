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
 * @file AampConfig.cpp
 * @brief Configuration related Functionality for AAMP
 */
#include "AampConfig.h"
#include "_base64.h"
#include "base16.h"
#include "AampJsonObject.h" // For JSON parsing
#include "AampUtils.h"
#include "aampgstplayer.h"
#include "SocUtils.h"
#include "PlayerRfc.h"
#include "PlayerExternalsInterface.h"
#include "PlayerSecInterface.h"
#include <time.h>
#include <map>
//////////////// CAUTION !!!! STOP !!! Read this before you proceed !!!!!!! /////////////
/// 1. This Class handles Configuration Parameters of AAMP Player , only Config related functionality to be added
/// 2. Simple Steps to add a new configuration
///		a) Identify new configuration takes what value ( bool / int  / string )
/// 	b) Add the new configuration string in README.txt with appropriate comment
///		c) Add a enum value for new config in AAMPConfigSettingInt, AAMPConfigSettingBool, AAMPConfigSettingFloat, or AAMPConfigSettingString.
/// 	d) Add the config string added in README and its equivalent enum value in corresponding
/// 		mConfigLookupTableInt, mConfigLookupTableBool, mConfigLookupTableFloat, or mConfigLookupTableString
/// 	e) Go to AampConfig constructor explicitly initialize of different value than default is required (int: 0, bool:false, float:0.0, string:"")
/// 	f) Thats it !! You added a new configuration . Use Set and Get function to
///			store and read value using enum config
/// 	g) IF any conversion required only (from config to usage, ex: sec to millisec ),
///			add specific Get function for each config
///			Not recommended . Better to have the conversion ( enum to string , sec to millisec etc ) where its consumed .
///////////////////////////////// Happy Configuration ////////////////////////////////////

#define ARRAY_SIZE(A) (sizeof(A)/sizeof(A[0]))

#define ERROR_TEXT_BAD_RANGE "Set failed. Input beyond the configured range"

typedef enum
{
	eCONFIG_RANGE_ANY,
	eCONFIG_RANGE_PORT,
	eCONFIG_RANGE_DRM_SYSTEMS, // 0..eDRM_MAX_DRMSystems
	eCONFIG_RANGE_LICENSE_WAIT, // MIN_LICENSE_KEY_ACQUIRE_WAIT_TIME..MAX_LICENSE_ACQ_WAIT_TIME
	eCONFIG_RANGE_PTS_ERROR_THRESHOLD, // 0..MAX_PTS_ERRORS_THRESHOLD
	eCONFIG_RANGE_PLAYLIST_CACHE_SIZE, // 0,15360, // Range for PlaylistCache size - upto 15 MB max
	eCONFIG_RANGE_DASH_DRM_SESSIONS, // 1..MAX_DASH_DRM_SESSIONS
	eCONFIG_RANGE_LANGUAGE_CODE, // 0,3
	eCONFIG_RANGE_DECRYPT_ERROR_THRESHOLD, // 0..MAX_SEG_DRM_DECRYPT_FAIL_COUNT},
	eCONFIG_RANGE_INJECT_ERROR_THRESHOLD, // 0..MAX_SEG_INJECT_FAIL_COUNT},
	eCONFIG_RANGE_DOWNLOAD_DELAY,  // 0..MAX_DOWNLOAD_DELAY_LIMIT_MS
	eCONFIG_RANGE_PAUSE_BEHAVIOR, // ePAUSED_BEHAVIOR_AUTOPLAY_IMMEDIATE..ePAUSED_BEHAVIOR_MAX},
	eCONFIG_RANGE_DOWNLOAD_ERROR_THRESHOLD, // 1..MAX_SEG_DOWNLOAD_FAIL_COUNT
	eCONFIG_RANGE_INIT_FRAGMENT_CACHE, // 1..5
	eCONFIG_RANGE_TIMEOUT, // 0..50
	eCONFIG_RANGE_CURL_SOCK_STORE_SIZE, // 1..10
	eCONFIG_RANGE_CURL_SSL_VERSION, // CURL_SSLVERSION_DEFAULT..CURL_SSLVERSION_TLSv1_3
	eCONFIG_RANGE_TUNED_EVENT_CODE, // eTUNED_EVENT_ON_PLAYLIST_INDEXED..eTUNED_EVENT_ON_GST_PLAYING},
	eCONFIG_RANGE_LIVEOFFSET, // 0..50
	eCONFIG_RANGE_RAMPDOWN_LIMIT, // -1..50
	eCONFIG_RANGE_CEA_PREFERRED, // -1..5
	eCONFIG_RANGE_PLAYBACK_OFFSET, // -99999..INT_MAX
	eCONFIG_RANGE_HARVEST_DURATION, // -1...10 HRS
	eCONFIG_RANGE_ABSOLUTE_REPORTING, // eABSOLUTE_PROGRESS_EPOCH..eABSOLUTE_PROGRESS_MAX
	eCONFIG_RANGE_LLDBUFFER, // 1 to 100 LLD buffer
	eCONFIG_RANGE_SHOW_DIAGNOSTICS_OVERLAY,//0 to 2
	eCONFIG_RANGE_MONITOR_AVSYNC_THRESHOLD_POSITIVE, //1ms to 10000ms
	eCONFIG_RANGE_MONITOR_AVSYNC_THRESHOLD_NEGATIVE, //-1 to -10000ms
	eCONFIG_RANGE_MONITOR_AVSYNC_JUMP_THRESHOLD,//1ms to 10000
	eCONFIG_RANGE_MAX_VALUE,
} ConfigValidRange;
#define CONFIG_RANGE_ENUM_COUNT (eCONFIG_RANGE_MAX_VALUE)

/**
  * @brief lookup table for categories of valid ranges, used for value validation
 */
static const struct
{
	int minValue;
	int maxValue;
	ConfigValidRange type;
} mConfigValueValidRange[] =
{
	{ 0, INT_MAX, eCONFIG_RANGE_ANY },
	{ 1, 65535, eCONFIG_RANGE_PORT },
	{ 0, eDRM_MAX_DRMSystems, eCONFIG_RANGE_DRM_SYSTEMS },
	{ MIN_LICENSE_KEY_ACQUIRE_WAIT_TIME, MAX_LICENSE_ACQ_WAIT_TIME, eCONFIG_RANGE_LICENSE_WAIT },
	{ 0, MAX_PTS_ERRORS_THRESHOLD, eCONFIG_RANGE_PTS_ERROR_THRESHOLD },
	{ 0, 15360, eCONFIG_RANGE_PLAYLIST_CACHE_SIZE },
	{ 1, MAX_DASH_DRM_SESSIONS, eCONFIG_RANGE_DASH_DRM_SESSIONS },
	{ 0, 3, eCONFIG_RANGE_LANGUAGE_CODE },
	{ 0, MAX_SEG_DRM_DECRYPT_FAIL_COUNT, eCONFIG_RANGE_DECRYPT_ERROR_THRESHOLD },
	{ 0, MAX_SEG_INJECT_FAIL_COUNT, eCONFIG_RANGE_INJECT_ERROR_THRESHOLD },
	{ 0, MAX_DOWNLOAD_DELAY_LIMIT_MS, eCONFIG_RANGE_DOWNLOAD_DELAY },
	{ ePAUSED_BEHAVIOR_AUTOPLAY_IMMEDIATE, ePAUSED_BEHAVIOR_MAX, eCONFIG_RANGE_PAUSE_BEHAVIOR },
	{ 1, MAX_SEG_DOWNLOAD_FAIL_COUNT, eCONFIG_RANGE_DOWNLOAD_ERROR_THRESHOLD },
	{ 1, 5, eCONFIG_RANGE_INIT_FRAGMENT_CACHE },
	{ 0, 50, eCONFIG_RANGE_TIMEOUT },
	{ 1, 10, eCONFIG_RANGE_CURL_SOCK_STORE_SIZE },
	{ CURL_SSLVERSION_DEFAULT, CURL_SSLVERSION_TLSv1_3, eCONFIG_RANGE_CURL_SSL_VERSION },
	{ eTUNED_EVENT_ON_PLAYLIST_INDEXED, eTUNED_EVENT_ON_GST_PLAYING, eCONFIG_RANGE_TUNED_EVENT_CODE },
	{ 0, 50, eCONFIG_RANGE_LIVEOFFSET },
	{ -1, 50, eCONFIG_RANGE_RAMPDOWN_LIMIT },
	{ -1, 5, eCONFIG_RANGE_CEA_PREFERRED },
	{AAMP_DEFAULT_PLAYBACK_OFFSET, INT_MAX, eCONFIG_RANGE_PLAYBACK_OFFSET },
	{-1, 60*60*10, eCONFIG_RANGE_HARVEST_DURATION },
	{eABSOLUTE_PROGRESS_EPOCH, eABSOLUTE_PROGRESS_MAX, eCONFIG_RANGE_ABSOLUTE_REPORTING},
	{ 1, 100, eCONFIG_RANGE_LLDBUFFER }, /** Minimum buffer should be a average chunk size(only int is possible), upper limit does not have much impact*/
	{ eDIAG_OVERLAY_NONE, eDIAG_OVERLAY_EXTENDED, eCONFIG_RANGE_SHOW_DIAGNOSTICS_OVERLAY},
	{ MIN_MONITOR_AVSYNC_POSITIVE_DELTA_MS, MAX_MONITOR_AVSYNC_POSITIVE_DELTA_MS, eCONFIG_RANGE_MONITOR_AVSYNC_THRESHOLD_POSITIVE},
	{ MIN_MONITOR_AVSYNC_NEGATIVE_DELTA_MS, MAX_MONITOR_AVSYNC_NEGATIVE_DELTA_MS, eCONFIG_RANGE_MONITOR_AVSYNC_THRESHOLD_NEGATIVE},
	{ MIN_MONITOR_AV_JUMP_THRESHOLD_MS, MAX_MONITOR_AV_JUMP_THRESHOLD_MS, eCONFIG_RANGE_MONITOR_AVSYNC_JUMP_THRESHOLD},
};

static ConfigPriority customOwner;
/**
 * @brief AAMP Config Owners enum-string mapping table
 */
static const AampOwnerLookupEntry mOwnerLookupTable[] =
{
	{"def",AAMP_DEFAULT_SETTING},
	{"oper",AAMP_OPERATOR_SETTING},
	{"stream",AAMP_STREAM_SETTING},
	{"app",AAMP_APPLICATION_SETTING},
	{"tune",AAMP_TUNE_SETTING},
	{"cfg",AAMP_DEV_CFG_SETTING},
	{"customcfg",AAMP_CUSTOM_DEV_CFG_SETTING},
	{"unknown",AAMP_MAX_SETTING}
};

struct ConfigLookupEntryInt
{
	int defaultValue;
	const char* cmdString;
	AAMPConfigSettingInt configEnum;
	bool bConfigurableByOperatorRFC; // better to have a separate list?
	ConfigValidRange validRange;
};

struct ConfigLookupEntryFloat
{
	double defaultValue;
	const char* cmdString;
	AAMPConfigSettingFloat configEnum;
	bool bConfigurableByOperatorRFC; // better to have a separate list?
	ConfigValidRange validRange;
};

struct ConfigLookupEntryBool
{
	bool defaultValue;
	const char* cmdString;
	AAMPConfigSettingBool configEnum;
	bool bConfigurableByOperatorRFC; // better to have a separate list?
};

struct ConfigLookupEntryString
{
	const char *defaultValue;
	const char* cmdString;
	AAMPConfigSettingString configEnum;
	bool bConfigurableByOperatorRFC; // better to have a separate list?
};



#ifdef GST_SUBTEC_ENABLED
#define DEFAULT_VALUE_GST_SUBTEC_ENABLED true
#else
#define DEFAULT_VALUE_GST_SUBTEC_ENABLED false
#endif

#ifdef ENABLE_USE_SINGLE_PIPELINE
#define DEFAULT_VALUE_USE_SINGLE_PIPELINE true
#else
#define DEFAULT_VALUE_USE_SINGLE_PIPELINE false
#endif

/**
 * @brief AAMPConfigSettingString metadata
 * note that order must match the actual order of the enum; this is enforced with asserts to catch any wrong/missing declarations
 */
static const ConfigLookupEntryString mConfigLookupTableString[AAMPCONFIG_STRING_COUNT] =
{
	{"","harvestPath",eAAMPConfig_HarvestPath,false},
	{"","licenseServerUrl",eAAMPConfig_LicenseServerUrl,false},
	{"","ckLicenseServerUrl",eAAMPConfig_CKLicenseServerUrl,false},
	{"","prLicenseServerUrl",eAAMPConfig_PRLicenseServerUrl,false},
	{"","wvLicenseServerUrl",eAAMPConfig_WVLicenseServerUrl,false},
	{AAMP_USERAGENT_STRING,"userAgent",eAAMPConfig_UserAgent,false},
	{"en,eng","preferredSubtitleLanguage",eAAMPConfig_SubTitleLanguage,false},
	{"","customHeader",eAAMPConfig_CustomHeader,false},
	{"","uriParameter",eAAMPConfig_URIParameter,false},
	{"","networkProxy",eAAMPConfig_NetworkProxy,false},
	{"","licenseProxy",eAAMPConfig_LicenseProxy,false},
	{"","authToken",eAAMPConfig_AuthToken,false},
	{"","log",eAAMPConfig_LogLevel,false},
	{"","customHeaderLicense",eAAMPConfig_CustomHeaderLicense,false},
	{"","preferredAudioRendition",eAAMPConfig_PreferredAudioRendition,false},
	{"","preferredAudioCodec",eAAMPConfig_PreferredAudioCodec,false},
	{"en,eng","preferredAudioLanguage",eAAMPConfig_PreferredAudioLanguage,false},
	{"","preferredAudioLabel",eAAMPConfig_PreferredAudioLabel,false},
	{"","preferredAudioType",eAAMPConfig_PreferredAudioType,false},
	{"","preferredTextRendition",eAAMPConfig_PreferredTextRendition,false},
	{"","preferredTextLanguage",eAAMPConfig_PreferredTextLanguage,false},
	{"","preferredTextLabel",eAAMPConfig_PreferredTextLabel,false},
	{"","preferredTextType",eAAMPConfig_PreferredTextType,false},
	{"","customLicenseData",eAAMPConfig_CustomLicenseData,false},
	{"urn:comcast:dai:2018","SchemeIdUriDaiStream",eAAMPConfig_SchemeIdUriDaiStream,true},
	{"urn:comcast:x1:lin:ck","SchemeIdUriVssStream",eAAMPConfig_SchemeIdUriVssStream,true},
	{"","LRHAcceptValue",eAAMPConfig_LRHAcceptValue,true},
	{"","LRHContentType",eAAMPConfig_LRHContentType,true},
	{"","gstlevel", eAAMPConfig_GstDebugLevel,false},
	{"","tsbType", eAAMPConfig_TsbType, false},
	{DEFAULT_TSB_LOCATION,"tsbLocation",eAAMPConfig_TsbLocation, true},
};

/**
 * @brief AAMPConfigSettingBool metadata
 * note that order must match the actual order of the enum; this is enforced with asserts to catch any wrong/missing declarations
 */
static const ConfigLookupEntryBool mConfigLookupTableBool[AAMPCONFIG_BOOL_COUNT] =
{
	{true,"abr",eAAMPConfig_EnableABR,false},
	{true,"fog",eAAMPConfig_Fog,false},
	{false,"preFetchIframePlaylist",eAAMPConfig_PrefetchIFramePlaylistDL,false},
	{false,"throttle",eAAMPConfig_Throttle,false},
	{false,"demuxAudioBeforeVideo",eAAMPConfig_DemuxAudioBeforeVideo,false},
	{false,"disableEC3",eAAMPConfig_DisableEC3,true},
	{false,"disableATMOS",eAAMPConfig_DisableATMOS,true},
	{false,"disableAC4",eAAMPConfig_DisableAC4,true},
	{false,"stereoOnly",eAAMPConfig_StereoOnly,true},
	{false,"descriptiveTrackName",eAAMPConfig_DescriptiveTrackName,false},
	{false,"disableAC3",eAAMPConfig_DisableAC3,true},
	{true,"disablePlaylistIndexEvent",eAAMPConfig_DisablePlaylistIndexEvent,false},
	{true,"enableSubscribedTags",eAAMPConfig_EnableSubscribedTags,false},
	{false,"dashIgnoreBaseUrlIfSlash",eAAMPConfig_DASHIgnoreBaseURLIfSlash,false},
	{false,"licenseAnonymousRequest",eAAMPConfig_AnonymousLicenseRequest,false},
	{false,"hlsAVTrackSyncUsingPDT",eAAMPConfig_HLSAVTrackSyncUsingStartTime,false},
	{true,"mpdDiscontinuityHandling",eAAMPConfig_MPDDiscontinuityHandling,false},
	{true,"mpdDiscontinuityHandlingCdvr",eAAMPConfig_MPDDiscontinuityHandlingCdvr,false},
	{false,"forceHttp",eAAMPConfig_ForceHttp,false},
	{true,"internalRetune",eAAMPConfig_InternalReTune,false},
	{false,"audioOnlyPlayback",eAAMPConfig_AudioOnlyPlayback,false},
	{false,"b64LicenseWrapping",eAAMPConfig_Base64LicenseWrapping,false},
	{true,"gstBufferAndPlay",eAAMPConfig_GStreamerBufferingBeforePlay,false},
	{false,"playreadyOutputProtection",eAAMPConfig_EnablePROutputProtection,false},
	{true,"retuneOnBufferingTimeout",eAAMPConfig_ReTuneOnBufferingTimeout,false},
	{true,"sslVerifyPeer",eAAMPConfig_SslVerifyPeer,false},
	{false,"client-dai",eAAMPConfig_EnableClientDai,true},  // not changing this name , this is already in use for RFC
	{false,"cdnAdsOnly",eAAMPConfig_PlayAdFromCDN,false},
	{true,"enableVideoEndEvent",eAAMPConfig_EnableVideoEndEvent,true},
	{true,"enableVideoRectangle",eAAMPConfig_EnableRectPropertyCfg,false},
	{false,"reportVideoPTS",eAAMPConfig_ReportVideoPTS,false},
	{false,"decoderUnavailableStrict",eAAMPConfig_DecoderUnavailableStrict,false},
	{false,"appSrcForProgressivePlayback",eAAMPConfig_UseAppSrcForProgressivePlayback,false},
	{false,"descriptiveAudioTrack",eAAMPConfig_DescriptiveAudioTrack,false},
	{true,"reportBufferEvent",eAAMPConfig_ReportBufferEvent,false},
	{false,"info",eAAMPConfig_InfoLogging,true},
	{false,"debug",eAAMPConfig_DebugLogging,false},
	{false,"trace",eAAMPConfig_TraceLogging,false},
	{true,"warn",eAAMPConfig_WarnLogging,false},
	{false,"failover",eAAMPConfig_FailoverLogging,false},
	{false,"gst",eAAMPConfig_GSTLogging,false},
	{false,"progress",eAAMPConfig_ProgressLogging,false},
	{false,"curl",eAAMPConfig_CurlLogging,false},
	{false,"curlLicense",eAAMPConfig_CurlLicenseLogging,false},
	{false,"logMetadata",eAAMPConfig_MetadataLogging,false},
	{false,"curlHeader",eAAMPConfig_CurlHeader,false},
	{false,"stream",eAAMPConfig_StreamLogging,false},
	{false,"id3",eAAMPConfig_ID3Logging,false},
	{true,"gstPositionQueryEnable",eAAMPConfig_EnableGstPositionQuery,false},
	{false,"seekMidFragment",eAAMPConfig_MidFragmentSeek,false},
	{true,"propagateUriParameters",eAAMPConfig_PropagateURIParam,false},
	{true, "useWesterosSink",eAAMPConfig_UseWesterosSink,true},					// Toggle it via config based on platforms
	{true,"useRetuneForUnpairedDiscontinuity",eAAMPConfig_RetuneForUnpairDiscontinuity,false},
	{true,"useRetuneForGstInternalError",eAAMPConfig_RetuneForGSTError,false},
	{false,"useMatchingBaseUrl",eAAMPConfig_MatchBaseUrl,false},
	{false,"wifiCurlHeader",eAAMPConfig_WifiCurlHeader,false},
	{false,"enableSeekableRange",eAAMPConfig_EnableSeekRange,false},
	{false,"enableLiveLatencyCorrection",eAAMPConfig_EnableLiveLatencyCorrection,true},
	{true,"dashParallelFragDownload",eAAMPConfig_DashParallelFragDownload,false},
	{false,"persistBitrateOverSeek",eAAMPConfig_PersistentBitRateOverSeek,true},
	{true,"setLicenseCaching",eAAMPConfig_SetLicenseCaching,false},
	{true,"fragmp4LicensePrefetch",eAAMPConfig_Fragmp4PrefetchLicense,false},
	{true,"useNewABR",eAAMPConfig_ABRBufferCheckEnabled,false},
	{false,"useNewAdBreaker",eAAMPConfig_NewDiscontinuity,false},
	{false,"bulkTimedMetadata",eAAMPConfig_BulkTimedMetaReport,false},
	{false,"bulkTimedMetadataLive",eAAMPConfig_BulkTimedMetaReportLive,false},
	{false,"useAverageBandwidth",eAAMPConfig_AvgBWForABR,false},
	{false,"nativeCCRendering",eAAMPConfig_NativeCCRendering,false},
	{true,"subtecSubtitle",eAAMPConfig_Subtec_subtitle,false},
	{true,"webVttNative",eAAMPConfig_WebVTTNative,false},
	{false,"asyncTune",eAAMPConfig_AsyncTune,true},
	{false,"disableUnderflow",eAAMPConfig_DisableUnderflow,false},
	{false,"limitResolution",eAAMPConfig_LimitResolution,false},
	{false,"useAbsoluteTimeline",eAAMPConfig_UseAbsoluteTimeline,false},
	{true,"enableAccessAttributes",eAAMPConfig_EnableAccessAttributes,false},
	{false,"WideVineKIDWorkaround",eAAMPConfig_WideVineKIDWorkaround,false},
	{false,"repairIframes",eAAMPConfig_RepairIframes,false},
	{true,"seiTimeCode",eAAMPConfig_SEITimeCode,false},
	{false,"disable4K" , eAAMPConfig_Disable4K, false},
	{true,"sharedSSL",eAAMPConfig_EnableSharedSSLSession, true},
	{false,"tsbInterruptHandling", eAAMPConfig_InterruptHandling,true},
	{true,"enableLowLatencyDash",eAAMPConfig_EnableLowLatencyDash,true},
	{true,"disableLowLatencyABR",eAAMPConfig_DisableLowLatencyABR,false},
	{true,"enableLowLatencyCorrection",eAAMPConfig_EnableLowLatencyCorrection,true},					// Toggle it via config based on platforms
	{true,"enableLowLatencyOffsetMin",eAAMPConfig_EnableLowLatencyOffsetMin,false},
	{false,"syncAudioFragments",eAAMPConfig_SyncAudioFragments,false},
	{false,"enableEosSmallFragment", eAAMPConfig_EnableIgnoreEosSmallFragment, false},
	{false,"useSecManager",eAAMPConfig_UseSecManager, true},
	{false,"enablePTO", eAAMPConfig_EnablePTO,false},
	{true,"enableFogConfig", eAAMPConfig_EnableAampConfigToFog, false},
	{false,"xreSupportedTune",eAAMPConfig_XRESupportedTune,false},
	{DEFAULT_VALUE_GST_SUBTEC_ENABLED,"gstSubtecEnabled",eAAMPConfig_GstSubtecEnabled,false},
	{true,"allowPageHeaders",eAAMPConfig_AllowPageHeaders,false},
	{false,"persistHighNetworkBandwidth",eAAMPConfig_PersistHighNetworkBandwidth,false},
	{true,"persistLowNetworkBandwidth",eAAMPConfig_PersistLowNetworkBandwidth,false},
	{false,"changeTrackWithoutRetune", eAAMPConfig_ChangeTrackWithoutRetune, false},
	{true,"curlStore", eAAMPConfig_EnableCurlStore, true},
	{false,"configRuntimeDRM", eAAMPConfig_RuntimeDRMConfig,false},
	{false,"enablePublishingMuxedAudio",eAAMPConfig_EnablePublishingMuxedAudio,false},
	{true,"enableCMCD", eAAMPConfig_EnableCMCD, true},
	{true,"SlowMotion", eAAMPConfig_EnableSlowMotion, true},
	{false,"enableSCTE35PresentationTime", eAAMPConfig_EnableSCTE35PresentationTime, false},
	{false,"jsinfo",eAAMPConfig_JsInfoLogging,false},
	{false,"ignoreAppLiveOffset", eAAMPConfig_IgnoreAppLiveOffset, false},
	{false,"useTCPServerSink",eAAMPConfig_useTCPServerSink,false},
	{true,"enableDisconnectSignals", eAAMPConfig_enableDisconnectSignals, false},
	{false,"sendLicenseResponseHeaders", eAAMPConfig_SendLicenseResponseHeaders, false},
	{false,"suppressDecode", eAAMPConfig_SuppressDecode, false},
	{false,"reconfigPipelineOnDiscontinuity", eAAMPConfig_ReconfigPipelineOnDiscontinuity, false},
	{true,"enableMediaProcessor", eAAMPConfig_EnableMediaProcessor, true},
	{true,"mpdStichingSupport", eAAMPConfig_MPDStitchingSupport, true}, // FIXME - spelling
	{false,"sendUserAgentInLicense", eAAMPConfig_SendUserAgent, false},
	{false,"enablePTSReStamp", eAAMPConfig_EnablePTSReStamp, true},
	{false, "trackMemory", eAAMPConfig_TrackMemory, false},
	{DEFAULT_VALUE_USE_SINGLE_PIPELINE,"useSinglePipeline", eAAMPConfig_UseSinglePipeline, false},
	// ideally would be named enableEarlyId3Processing for clarity, but to avoid partner confusion leaving original spelling for now
	// this will eventually be default enabled and deprecated as a configuration
	{false, "earlyProcessing", eAAMPConfig_EarlyID3Processing, false},
	{false, "seamlessAudioSwitch", eAAMPConfig_SeamlessAudioSwitch, true},
	{false, "useRialtoSink", eAAMPConfig_useRialtoSink, false},
	{false, "localTSBEnabled", eAAMPConfig_LocalTSBEnabled, true},
	{false, "enableIFrameTrackExtract", eAAMPConfig_EnableIFrameTrackExtract, true},
	{false, "forceMultiPeriodDiscontinuity", eAAMPConfig_ForceMultiPeriodDiscontinuity, false},
	{false, "forceLLDFlow", eAAMPConfig_ForceLLDFlow, false},
	{false, "monitorAV", eAAMPConfig_MonitorAV, true},
	{false, "enablePTSRestampForHlsTs", eAAMPConfig_HlsTsEnablePTSReStamp, true},
	{true, "overrideMediaHeaderDuration", eAAMPConfig_OverrideMediaHeaderDuration, true},
	{false, "useMp4Demux", eAAMPConfig_UseMp4Demux,false },
	{false, "curlThroughput", eAAMPConfig_CurlThroughput, false },
	{false, "useFireboltSDK", eAAMPConfig_UseFireboltSDK, false},
	{true, "enableChunkInjection", eAAMPConfig_EnableChunkInjection, true}
};

#define CONFIG_INT_ALIAS_COUNT 2
/**
 * @brief AAMPConfigSettingInt metadata
 * note that order must match the actual order of the enum; this is enforced with asserts to catch any wrong/missing declarations
 */
static const ConfigLookupEntryInt mConfigLookupTableInt[AAMPCONFIG_INT_COUNT+CONFIG_INT_ALIAS_COUNT] =
{
	{0,"harvestCountLimit",eAAMPConfig_HarvestCountLimit,false},
	{0,"harvestConfig",eAAMPConfig_HarvestConfig,false},
	{DEFAULT_ABR_CACHE_LIFE,"abrCacheLife",eAAMPConfig_ABRCacheLife,false},
	{DEFAULT_ABR_CACHE_LENGTH,"abrCacheLength",eAAMPConfig_ABRCacheLength,false},
	{0,"timeShiftBufferLength",eAAMPConfig_TimeShiftBufferLength,false},
	{DEFAULT_ABR_OUTLIER,"abrCacheOutlier",eAAMPConfig_ABRCacheOutlier,false},
	{DEFAULT_ABR_SKIP_DURATION,"abrSkipDuration",eAAMPConfig_ABRSkipDuration,false},
	{DEFAULT_ABR_NW_CONSISTENCY_CNT,"abrNwConsistency",eAAMPConfig_ABRNWConsistency,false},
	{DEFAULT_AAMP_ABR_THRESHOLD_SIZE,"thresholdSizeABR",eAAMPConfig_ABRThresholdSize,false},
	{DEFAULT_CACHED_FRAGMENTS_PER_TRACK,"downloadBuffer",eAAMPConfig_MaxFragmentCached,false},
	{DEFAULT_BUFFER_HEALTH_MONITOR_DELAY,"bufferHealthMonitorDelay",eAAMPConfig_BufferHealthMonitorDelay,false},
	{DEFAULT_BUFFER_HEALTH_MONITOR_INTERVAL,"bufferHealthMonitorInterval",eAAMPConfig_BufferHealthMonitorInterval,false},
	{eDRM_PlayReady,"preferredDrm",eAAMPConfig_PreferredDRM,true,eCONFIG_RANGE_DRM_SYSTEMS},
	{eTUNED_EVENT_ON_GST_PLAYING,"tuneEventConfig",eAAMPConfig_TuneEventConfig,false,eCONFIG_RANGE_TUNED_EVENT_CODE},
	{TRICKPLAY_VOD_PLAYBACK_FPS,"vodTrickPlayFps",eAAMPConfig_VODTrickPlayFPS,false},
	{TRICKPLAY_LINEAR_PLAYBACK_FPS,"linearTrickPlayFps",eAAMPConfig_LinearTrickPlayFPS,false},
	{DEFAULT_LICENSE_REQ_RETRY_WAIT_TIME,"licenseRetryWaitTime",eAAMPConfig_LicenseRetryWaitTime,false},
	{DEFAULT_LICENSE_KEY_ACQUIRE_WAIT_TIME,"licenseKeyAcquireWaitTime",eAAMPConfig_LicenseKeyAcquireWaitTime,false,eCONFIG_RANGE_LICENSE_WAIT},
	{DEFAULT_PTS_ERRORS_THRESHOLD,"ptsErrorThreshold",eAAMPConfig_PTSErrorThreshold,true, eCONFIG_RANGE_PTS_ERROR_THRESHOLD },
	{MAX_PLAYLIST_CACHE_SIZE,"maxPlaylistCacheSize",eAAMPConfig_MaxPlaylistCacheSize,false, eCONFIG_RANGE_PLAYLIST_CACHE_SIZE },
	{MIN_DASH_DRM_SESSIONS,"dashMaxDrmSessions",eAAMPConfig_MaxDASHDRMSessions,false,eCONFIG_RANGE_DASH_DRM_SESSIONS },
	{DEFAULT_WAIT_TIME_BEFORE_RETRY_HTTP_5XX_MS,"waitTimeBeforeRetryHttp5xx",eAAMPConfig_Http5XXRetryWaitInterval,false},
	{0,"langCodePreference",eAAMPConfig_LanguageCodePreference,false,eCONFIG_RANGE_LANGUAGE_CODE },
	{-1,"fragmentRetryLimit",eAAMPConfig_RampDownLimit,false, eCONFIG_RANGE_RAMPDOWN_LIMIT},
	{0,"initRampdownLimit",eAAMPConfig_InitRampDownLimit,false},
	{MAX_SEG_DRM_DECRYPT_FAIL_COUNT,"drmDecryptFailThreshold",eAAMPConfig_DRMDecryptThreshold,false,eCONFIG_RANGE_DECRYPT_ERROR_THRESHOLD },
	{MAX_SEG_INJECT_FAIL_COUNT,"segmentInjectFailThreshold",eAAMPConfig_SegmentInjectThreshold,false, eCONFIG_RANGE_INJECT_ERROR_THRESHOLD },
	{DEFAULT_DOWNLOAD_RETRY_COUNT,"initFragmentRetryCount",eAAMPConfig_InitFragmentRetryCount,false },
	{AAMP_LOW_BUFFER_BEFORE_RAMPDOWN,"minABRBufferRampdown",eAAMPConfig_MinABRNWBufferRampDown,false},
	{AAMP_HIGH_BUFFER_BEFORE_RAMPUP,"maxABRBufferRampup",eAAMPConfig_MaxABRNWBufferRampUp,false},
	{DEFAULT_PREBUFFER_COUNT,"preplayBuffercount",eAAMPConfig_PrePlayBufferCount,false},
	{0,"preCachePlaylistTime",eAAMPConfig_PreCachePlaylistTime,false},
	{-1, "ceaFormat",eAAMPConfig_CEAPreferred,false, eCONFIG_RANGE_CEA_PREFERRED},
	{DEFAULT_STALL_ERROR_CODE,"stallErrorCode",eAAMPConfig_StallErrorCode,false},
	{DEFAULT_STALL_DETECTION_TIMEOUT,"stallTimeout",eAAMPConfig_StallTimeoutMS,false},
	{DEFAULT_MINIMUM_INIT_CACHE_SECONDS,"initialBuffer",eAAMPConfig_InitialBuffer,false},
	{DEFAULT_MAXIMUM_PLAYBACK_BUFFER_SECONDS,"playbackBuffer",eAAMPConfig_PlaybackBuffer,false},
	{DEFAULT_TIMEOUT_FOR_SOURCE_SETUP,"maxTimeoutForSourceSetup",eAAMPConfig_SourceSetupTimeout,false},
	{0,"downloadDelay",eAAMPConfig_DownloadDelay,false, eCONFIG_RANGE_DOWNLOAD_DELAY },
	{ePAUSED_BEHAVIOR_AUTOPLAY_IMMEDIATE,"livePauseBehavior",eAAMPConfig_LivePauseBehavior,false,eCONFIG_RANGE_PAUSE_BEHAVIOR },
	{MAX_GST_VIDEO_BUFFER_BYTES,"gstVideoBufBytes", eAAMPConfig_GstVideoBufBytes,true},
	{MAX_GST_AUDIO_BUFFER_BYTES,"gstAudioBufBytes", eAAMPConfig_GstAudioBufBytes,true},
	{DEFAULT_LATENCY_MONITOR_DELAY,"latencyMonitorDelay",eAAMPConfig_LatencyMonitorDelay,false},
	{AAMP_LLD_LATENCY_MONITOR_INTERVAL,"latencyMonitorInterval",eAAMPConfig_LatencyMonitorInterval,false},
	{DEFAULT_CACHED_FRAGMENT_CHUNKS_PER_TRACK,"downloadBufferChunks",eAAMPConfig_MaxFragmentChunkCached,false},
	{DEFAULT_AAMP_ABR_CHUNK_THRESHOLD_SIZE,"abrChunkThresholdSize",eAAMPConfig_ABRChunkThresholdSize,false},
	{DEFAULT_MIN_LOW_LATENCY,"lowLatencyMinValue",eAAMPConfig_LLMinLatency,true},
	{DEFAULT_TARGET_LOW_LATENCY,"lowLatencyTargetValue",eAAMPConfig_LLTargetLatency,true},
	{DEFAULT_MAX_LOW_LATENCY,"lowLatencyMaxValue",eAAMPConfig_LLMaxLatency,true},
	{MAX_SEG_DOWNLOAD_FAIL_COUNT,"fragmentDownloadFailThreshold",eAAMPConfig_FragmentDownloadFailThreshold,false,eCONFIG_RANGE_DOWNLOAD_ERROR_THRESHOLD },
	{MAX_INIT_FRAGMENT_CACHE_PER_TRACK,"maxInitFragCachePerTrack",eAAMPConfig_MaxInitFragCachePerTrack,true, eCONFIG_RANGE_INIT_FRAGMENT_CACHE },
	{FOG_MAX_CONCURRENT_DOWNLOADS,"fogMaxConcurrentDownloads",eAAMPConfig_FogMaxConcurrentDownloads, false },
	{DEFAULT_CONTENT_PROTECTION_DATA_UPDATE_TIMEOUT,"contentProtectionDataUpdateTimeout",eAAMPConfig_ContentProtectionDataUpdateTimeout,false},
	{MAX_CURL_SOCK_STORE,"maxCurlStore", eAAMPConfig_MaxCurlSockStore,false, eCONFIG_RANGE_CURL_SOCK_STORE_SIZE },
	{6123,"TCPServerSinkPort",eAAMPConfig_TCPServerSinkPort,false, eCONFIG_RANGE_ANY },
	{DEFAULT_INIT_BITRATE,"initialBitrate",eAAMPConfig_DefaultBitrate,false },
	{DEFAULT_INIT_BITRATE_4K,"initialBitrate4K",eAAMPConfig_DefaultBitrate4K,false },
	{0,"iframeDefaultBitrate",eAAMPConfig_IFrameDefaultBitrate,false},
	{0,"iframeDefaultBitrate4K",eAAMPConfig_IFrameDefaultBitrate4K,false},
	{0,"downloadStallTimeout",eAAMPConfig_CurlStallTimeout,false,eCONFIG_RANGE_TIMEOUT},
	{0,"downloadStartTimeout",eAAMPConfig_CurlDownloadStartTimeout,false,eCONFIG_RANGE_TIMEOUT},
	{0,"downloadLowBWTimeout",eAAMPConfig_CurlDownloadLowBWTimeout,false,eCONFIG_RANGE_TIMEOUT},
	{DEFAULT_DISCONTINUITY_TIMEOUT,"discontinuityTimeout",eAAMPConfig_DiscontinuityTimeout,false},
	{0,"minBitrate",eAAMPConfig_MinBitrate,true},
	{INT_MAX,"maxBitrate",eAAMPConfig_MaxBitrate,true},
	{CURL_SSLVERSION_TLSv1_2,"supportTLS",eAAMPConfig_TLSVersion,true,eCONFIG_RANGE_CURL_SSL_VERSION},
	{DEFAULT_DRM_NETWORK_TIMEOUT,"drmNetworkTimeout",eAAMPConfig_DrmNetworkTimeout,true,eCONFIG_RANGE_TIMEOUT},
	{0,"drmStallTimeout",eAAMPConfig_DrmStallTimeout,true,eCONFIG_RANGE_TIMEOUT},
	{0,"drmStartTimeout",eAAMPConfig_DrmStartTimeout,true,eCONFIG_RANGE_TIMEOUT},
	{0,"timeBasedBufferSeconds",eAAMPConfig_TimeBasedBufferSeconds,true,eCONFIG_RANGE_PLAYBACK_OFFSET},
	{DEFAULT_TELEMETRY_REPORT_INTERVAL,"telemetryInterval",eAAMPConfig_TelemetryInterval,true},
	{0,"rateCorrectionDelay", eAAMPConfig_RateCorrectionDelay,true},
	{-1,"harvestDuration",eAAMPConfig_HarvestDuration,false,eCONFIG_RANGE_HARVEST_DURATION},
	{DEFAULT_SUBTITLE_CLOCK_SYNC_INTERVAL_S,"subtitleClockSyncInterval",eAAMPConfig_SubtitleClockSyncInterval,true},
	{eABSOLUTE_PROGRESS_WITHOUT_AVAILABILITY_START,"preferredAbsoluteReporting",eAAMPConfig_PreferredAbsoluteProgressReporting,true, eCONFIG_RANGE_ABSOLUTE_REPORTING},
	{EOS_INJECTION_MODE_STOP_ONLY,"EOSInjectionMode", eAAMPConfig_EOSInjectionMode,true},
	{DEFAULT_ABR_BUFFER_COUNTER,"abrBufferCounter", eAAMPConfig_ABRBufferCounter,true},
	{DEFAULT_TSB_DURATION,"tsbLength",eAAMPConfig_TsbLength,true},
	{DEFAULT_MIN_TSB_STORAGE_FREE_PERCENTAGE,"tsbMinDiskFreePercentage",eAAMPConfig_TsbMinDiskFreePercentage,true},
	{DEFAULT_MAX_TSB_STORAGE_MB,"tsbMaxDiskStorage",eAAMPConfig_TsbMaxDiskStorage,true},
	{static_cast<int>(TSB::LogLevel::WARN),"tsbLog",eAAMPConfig_TsbLogLevel,false},
	{DEFAULT_AD_FULFILLMENT_TIMEOUT,"adFulfillmentTimeout",eAAMPConfig_AdFulfillmentTimeout,true},
	{MAX_AD_FULFILLMENT_TIMEOUT,"adFulfillmentTimeoutMax",eAAMPConfig_AdFulfillmentTimeoutMax,true},
	{eDIAG_OVERLAY_NONE,"showDiagnosticsOverlay",eAAMPConfig_ShowDiagnosticsOverlay,true, eCONFIG_RANGE_SHOW_DIAGNOSTICS_OVERLAY },
	{DEFAULT_MONITOR_AVSYNC_POSITIVE_DELTA_MS, "monitorAVSyncThresholdPositive", eAAMPConfig_MonitorAVSyncThresholdPositive,true, eCONFIG_RANGE_MONITOR_AVSYNC_THRESHOLD_POSITIVE },
	{DEFAULT_MONITOR_AVSYNC_NEGATIVE_DELTA_MS,"monitorAVSyncThresholdNegative",eAAMPConfig_MonitorAVSyncThresholdNegative,true,eCONFIG_RANGE_MONITOR_AVSYNC_THRESHOLD_NEGATIVE },
	{DEFAULT_MONITOR_AV_JUMP_THRESHOLD_MS,"monitorAVJumpThreshold",eAAMPConfig_MonitorAVJumpThreshold,true,eCONFIG_RANGE_MONITOR_AVSYNC_JUMP_THRESHOLD },
	{DEFAULT_PROGRESS_LOGGING_DIVISOR,"progressLoggingDivisor",eAAMPConfig_ProgressLoggingDivisor,false},
	{DEFAULT_MONITOR_AV_REPORTING_INTERVAL, "monitorAVReportingInterval", eAAMPConfig_MonitorAVReportingInterval, false},
	// aliases, kept for backwards compatibility
	{DEFAULT_INIT_BITRATE,"defaultBitrate",eAAMPConfig_DefaultBitrate,true },
	{DEFAULT_INIT_BITRATE_4K,"defaultBitrate4K",eAAMPConfig_DefaultBitrate4K,true },
};

/**
 * @brief AAMPConfigSettingFloat metadata
 * note that order must match the actual order of the enum; this is enforced with asserts to catch any wrong/missing declarations
 */
static const ConfigLookupEntryFloat mConfigLookupTableFloat[AAMPCONFIG_FLOAT_COUNT] =
{
	{CURL_FRAGMENT_DL_TIMEOUT,"networkTimeout",eAAMPConfig_NetworkTimeout,true},
	{CURL_FRAGMENT_DL_TIMEOUT,"manifestTimeout",eAAMPConfig_ManifestTimeout,true},
	{0.0,"playlistTimeout",eAAMPConfig_PlaylistTimeout,true},
	{DEFAULT_REPORT_PROGRESS_INTERVAL,"progressReportingInterval",eAAMPConfig_ReportProgressInterval,false},
	{AAMP_DEFAULT_PLAYBACK_OFFSET,"offset",eAAMPConfig_PlaybackOffset,false,eCONFIG_RANGE_PLAYBACK_OFFSET},
	{AAMP_LIVE_OFFSET,"liveOffset",eAAMPConfig_LiveOffset,true,eCONFIG_RANGE_LIVEOFFSET}, //liveOffset by user
	{AAMP_DEFAULT_LIVE_OFFSET_DRIFT,"liveOffsetDriftCorrectionInterval",eAAMPConfig_LiveOffsetDriftCorrectionInterval,true,eCONFIG_RANGE_LIVEOFFSET}, //liveOffset by user
	{AAMP_LIVE_OFFSET,"liveOffset4K",eAAMPConfig_LiveOffset4K,true,eCONFIG_RANGE_LIVEOFFSET}, //liveOffset for 4K by user
	{AAMP_CDVR_LIVE_OFFSET,"cdvrLiveOffset",eAAMPConfig_CDVRLiveOffset,true,eCONFIG_RANGE_LIVEOFFSET},
	{DEFAULT_CURL_CONNECTTIMEOUT,"connectTimeout",eAAMPConfig_Curl_ConnectTimeout,true},
	{DEFAULT_DNS_CACHE_TIMEOUT,"dnsCacheTimeout",eAAMPConfig_Dns_CacheTimeout,true},
	{DEFAULT_MIN_RATE_CORRECTION_SPEED,"minLatencyCorrectionPlaybackRate",eAAMPConfig_MinLatencyCorrectionPlaybackRate,false},
	{DEFAULT_MAX_RATE_CORRECTION_SPEED,"maxLatencyCorrectionPlaybackRate",eAAMPConfig_MaxLatencyCorrectionPlaybackRate,false},
	{DEFAULT_NORMAL_RATE_CORRECTION_SPEED,"normalLatencyCorrectionPlaybackRate",eAAMPConfig_NormalLatencyCorrectionPlaybackRate,false},
	{DEFAULT_MIN_BUFFER_LOW_LATENCY,"lowLatencyMinBuffer",eAAMPConfig_LowLatencyMinBuffer,true, eCONFIG_RANGE_LLDBUFFER},
	{DEFAULT_TARGET_BUFFER_LOW_LATENCY,"lowLatencyTargetBuffer",eAAMPConfig_LowLatencyTargetBuffer,true, eCONFIG_RANGE_LLDBUFFER},
	{GST_BW_TO_BUFFER_FACTOR,"bandwidthToBufferFactor", eAAMPConfig_BWToGstBufferFactor,true},
};

/**
 * @brief singleton helper class mapping configuration names to configuration metadata (not app/player instance specific)
 */
class ConfigLookup
{
private:
	std::map<std::string, ConfigLookupEntryBool> lookupBool;
	std::map<std::string, ConfigLookupEntryInt> lookupInt;
	std::map<std::string, ConfigLookupEntryFloat> lookupFloat;
	std::map<std::string, ConfigLookupEntryString> lookupString;

public:
	static bool ConfigStringValueToBool( const char *value_cstr )
	{
		bool rc = false;
		if( value_cstr )
		{
			if( isdigit(*value_cstr) )
			{
				int ival = atoi(value_cstr);
				if( ival == 1 )
				{
					rc = true;
				}
				else if( ival!=0 )
				{
					AAMPLOG_ERR( "unexpected input: %s", value_cstr );
				}
			}
			else if( strcasecmp(value_cstr,"true")==0 )
			{
				rc = true;
			}
			else if( strcasecmp(value_cstr,"false")!=0 )
			{
				AAMPLOG_ERR( "unexpected input: %s", value_cstr );
			}
		}
		return rc;
	}

	void Process( AampConfig *aampConfig, ConfigPriority owner, const std::string &key, const std::string &value )
	{ // used while parsing aamp.cfg text
		const char *value_cstr = value.c_str();
		auto iter = lookupBool.find(key);
		if( iter != lookupBool.end())
		{
			auto cfg = iter->second;
			AAMPLOG_MIL("Parsed value for dev cfg property %s - %s", key.c_str(), value_cstr );
			if( value.empty() )
			{
				bool currentValue = aampConfig->GetConfigValue(cfg.configEnum);
				aampConfig->SetConfigValue( owner, cfg.configEnum, !currentValue );
			}
			else
			{
				aampConfig->SetConfigValue( owner, cfg.configEnum, ConfigStringValueToBool(value_cstr) );
			}
		}
		else
		{
			auto iter = lookupInt.find(key);
			if( iter != lookupInt.end() )
			{
				auto cfg = iter->second;
				int conv = atoi( value_cstr );
				aampConfig->SetConfigValue(owner,cfg.configEnum,conv);
			}
			else
			{
				auto iter = lookupFloat.find(key);
				if( iter != lookupFloat.end() )
				{
					auto cfg = iter->second;
					double conv = atof( value_cstr );
					aampConfig->SetConfigValue(owner,cfg.configEnum,conv);
				}
				else
				{
					auto iter = lookupString.find(key);
					if( iter != lookupString.end() )
					{
						auto cfg = iter->second;
						if(value.size())
						{
							aampConfig->SetConfigValue(owner,cfg.configEnum,value);
						}
					}
				}
			}
		}
	}

	void Process( AampConfig *aampConfig, struct customJson &custom )
	{ // called from AampConfig::CustomSearch
		Process( aampConfig, customOwner, custom.config, custom.configValue );
	}

	void Process( AampConfig *aampConfig, cJSON *customVal, customJson &customValues, std::vector<struct customJson> &vCustom )
	{ // called from AampConfig::CustomArrayRead
		// Verify any of ConfigLookupEntryBool item matched with given custom json
		for (auto it = lookupBool.begin(); it != lookupBool.end(); ++it)
		{
			const auto& keyname =  it->first;
			auto searchVal = cJSON_GetObjectItem(customVal,keyname.c_str());
			if(searchVal)
			{
				customValues.config = keyname;
				if(searchVal->valuestring != NULL)
				{
					customValues.configValue = searchVal->valuestring;
					vCustom.push_back(customValues);
				}
				else
				{
					AAMPLOG_ERR("Invalid format for %s ",keyname.c_str());
					continue;
				}
			}
		}
		// Verify any of ConfigLookupEntryInt item matched with given custom json
		for (auto it = lookupInt.begin(); it != lookupInt.end(); ++it)
		{
			const auto& keyname =  it->first;
			auto searchVal = cJSON_GetObjectItem(customVal,keyname.c_str());
			if(searchVal)
			{
				customValues.config = keyname;
				if(searchVal->valuestring != NULL)
				{
					customValues.configValue = searchVal->valuestring;
					vCustom.push_back(customValues);
				}
				else
				{
					AAMPLOG_ERR("Invalid format for %s ",keyname.c_str());
					continue;
				}
			}
		}

		//Verify any of ConfigLookupEntryFloat item matched with given custom json
		for (auto it = lookupFloat.begin(); it != lookupFloat.end(); ++it)
		{
			const auto& keyname =  it->first;
			auto searchVal = cJSON_GetObjectItem(customVal,keyname.c_str());
			if(searchVal)
			{
				customValues.config = keyname;
				if(searchVal->valuestring != NULL)
				{
					customValues.configValue = searchVal->valuestring;
					vCustom.push_back(customValues);
				}
				else
				{
					AAMPLOG_ERR("Invalid format for %s ",keyname.c_str());
					continue;
				}
			}
		}

		// Verify any of ConfigLookupEntryString item matched with given custom json
		for (auto it = lookupString.begin(); it != lookupString.end(); ++it)
		{
			const auto& keyname =  it->first;
			auto searchVal = cJSON_GetObjectItem(customVal,keyname.c_str());
			if(searchVal)
			{
				customValues.config = keyname;
				if(searchVal->valuestring != NULL)
				{
					customValues.configValue = searchVal->valuestring;
					vCustom.push_back(customValues);
				}
				else
				{
					AAMPLOG_ERR("Invalid format for %s ",keyname.c_str());
					continue;
				}
			}
		}

	}

	void Process( AampConfig *aampConfig, ConfigPriority owner, cJSON *searchObj )
	{ // called from AampConfig::ProcessConfigJson
		auto it = lookupBool.find(searchObj->string);
		if( it != lookupBool.end() )
		{
			auto cfg = it->second;
			auto cfgEnum = cfg.configEnum;
			std::string keyname = it->first;
			if(cJSON_IsTrue(searchObj))
			{
				aampConfig->SetConfigValue(owner,cfgEnum,true);
				AAMPLOG_MIL("Parsed value for property %s - true",keyname.c_str());
			}
			else
			{
				aampConfig->SetConfigValue(owner,cfgEnum,false);
				AAMPLOG_MIL("Parsed value for property %s - false",keyname.c_str());
			}
		}
		else
		{
			auto it = lookupInt.find(searchObj->string);
			if( it != lookupInt.end() )
			{
				auto conv = (int)searchObj->valueint;

				auto cfg = it->second;
				auto cfgEnum = cfg.configEnum;
				std::string keyname = it->first;
				aampConfig->SetConfigValue(owner,cfgEnum,conv);
				AAMPLOG_MIL("Parsed value for property %s - %d",keyname.c_str(),conv);
			}
			else
			{
				auto it = lookupFloat.find(searchObj->string);
				if( it != lookupFloat.end() )
				{
					auto conv = (double)searchObj->valuedouble;
					//cJSON_GetNumberValue(searchObj)
					auto cfg = it->second;
					auto cfgEnum = cfg.configEnum;
					std::string keyname = it->first;
					aampConfig->SetConfigValue(owner,cfgEnum,conv);
					AAMPLOG_MIL("Parsed value for property %s - %f",keyname.c_str(),conv);
				}
				else
				{
					auto it = lookupString.find(searchObj->string);
					if( it != lookupString.end() )
					{
						auto conv = std::string(searchObj->valuestring);
						//cJSON_GetStringValue(searchObj)
						auto cfg = it->second;
						auto cfgEnum = cfg.configEnum;
						std::string keyname = it->first;
						aampConfig->SetConfigValue(owner,cfgEnum,conv);
						AAMPLOG_MIL("Parsed value for property %s - %s",keyname.c_str(),conv.c_str() );
					}
				}
			}
		}
	}

	ConfigLookup(): lookupBool(), lookupInt(), lookupFloat(), lookupString()
	{ // constructor; populate collection of std::map for lookup by config name
		int i;
		assert( ARRAY_SIZE(mConfigValueValidRange) == CONFIG_RANGE_ENUM_COUNT );
		for( i=0; i<CONFIG_RANGE_ENUM_COUNT; i++ )
		{
			assert( mConfigValueValidRange[i].type == i );
		}

		assert( ARRAY_SIZE(mConfigLookupTableInt) == AAMPCONFIG_INT_COUNT+CONFIG_INT_ALIAS_COUNT );
		i = 0;
		while( i<AAMPCONFIG_INT_COUNT )
		{
			assert( mConfigLookupTableInt[i].configEnum == i );
			lookupInt[mConfigLookupTableInt[i].cmdString] = mConfigLookupTableInt[i];
			i++;
		}
		while( i<ARRAY_SIZE(mConfigLookupTableInt) )
		{ // two final entries with alias initialBitrate/defaultBitrate and initialBitrate4k/defaultBitrate4k
			lookupInt[mConfigLookupTableInt[i].cmdString] = mConfigLookupTableInt[i];
			i++;
		}

		assert( ARRAY_SIZE(mConfigLookupTableBool) == AAMPCONFIG_BOOL_COUNT );
		for(int i=0; i<AAMPCONFIG_BOOL_COUNT; ++i)
		{
			assert( mConfigLookupTableBool[i].configEnum == i );
			lookupBool[mConfigLookupTableBool[i].cmdString] = mConfigLookupTableBool[i];
		}

		assert( ARRAY_SIZE(mConfigLookupTableFloat) == AAMPCONFIG_FLOAT_COUNT );
		for(int i=0; i<AAMPCONFIG_FLOAT_COUNT; ++i)
		{
			assert( mConfigLookupTableFloat[i].configEnum == i );
			lookupFloat[mConfigLookupTableFloat[i].cmdString] = mConfigLookupTableFloat[i];
		}

		assert( ARRAY_SIZE(mConfigLookupTableString) == AAMPCONFIG_STRING_COUNT );
		for(int i=0; i<AAMPCONFIG_STRING_COUNT; ++i)
		{
			assert( mConfigLookupTableString[i].configEnum == i );
			lookupString[mConfigLookupTableString[i].cmdString] = mConfigLookupTableString[i];
		}
	}

	~ConfigLookup()
	{
	}
};

static ConfigLookup mConfigLookup;

/////////////////// Public Functions /////////////////////////////////////
/**
 * @brief AampConfig Constructor function . Default values defined
 *
 * @return None
 */
AampConfig::AampConfig(): mChannelOverrideMap(),vCustom(),vCustomIt(),customFound(false)
{
}

/**
 * @brief AampConfig Copy Constructor function - used to update global config
 */
AampConfig& AampConfig::operator=(const AampConfig& rhs)
{
	mChannelOverrideMap = rhs.mChannelOverrideMap;
	vCustom = rhs.vCustom;
	customFound = rhs.customFound;
	memcpy(configValueBool , rhs.configValueBool , sizeof(configValueBool));
	memcpy(configValueInt , rhs.configValueInt , sizeof(configValueInt));
	memcpy(configValueFloat , rhs.configValueFloat , sizeof(configValueFloat));

 	for(int index=0;index <AAMPCONFIG_STRING_COUNT; index++)
	{
		configValueString[index].owner = rhs.configValueString[index].owner;
		configValueString[index].lastowner = rhs.configValueString[index].lastowner;
		configValueString[index].value = rhs.configValueString[index].value;
		configValueString[index].lastvalue = rhs.configValueString[index].lastvalue;
	}
	return *this;
}

void AampConfig::Initialize()
{
	for( int i=0; i<AAMPCONFIG_BOOL_COUNT; i++ )
	{
		configValueBool[i].value = mConfigLookupTableBool[i].defaultValue;
	}
	for( int i=0; i<AAMPCONFIG_INT_COUNT; i++ )
	{
		configValueInt[i].value = mConfigLookupTableInt[i].defaultValue;
	}
	for( int i=0; i<AAMPCONFIG_FLOAT_COUNT; i++ )
	{
		configValueFloat[i].value = mConfigLookupTableFloat[i].defaultValue;
	}
	for( int i=0; i<AAMPCONFIG_STRING_COUNT; i++ )
	{
		configValueString[i].value = mConfigLookupTableString[i].defaultValue;
	}
}

void AampConfig::ApplyDeviceCapabilities()
{
	std::shared_ptr<PlayerExternalsInterface> pInstance = PlayerExternalsInterface::GetPlayerExternalsInterfaceInstance();
	bool IsWifiCurlHeader = pInstance->IsConfigWifiCurlHeader();	

	configValueBool[eAAMPConfig_UseAppSrcForProgressivePlayback].value = SocUtils::UseAppSrcForProgressivePlayback();
	configValueBool[eAAMPConfig_DisableAC4].value = SocUtils::IsSupportedAC4();
	configValueBool[eAAMPConfig_DisableAC3].value = SocUtils::IsSupportedAC3();
	configValueBool[eAAMPConfig_UseWesterosSink].value = SocUtils::UseWesterosSink();
	configValueBool[eAAMPConfig_SyncAudioFragments].value = SocUtils::IsAudioFragmentSyncSupported();
	SetConfigValue(AAMP_DEFAULT_SETTING, eAAMPConfig_WifiCurlHeader, IsWifiCurlHeader);

	bool isSecMgr = isSecManagerEnabled();
	SetConfigValue(AAMP_DEFAULT_SETTING, eAAMPConfig_UseSecManager, isSecMgr);
}

std::string AampConfig::GetUserAgentString() const
{
	return std::string(configValueString[eAAMPConfig_UserAgent].value);
}

/**
 * @brief Gets the boolean configuration value
 */
bool AampConfig::IsConfigSet(AAMPConfigSettingBool cfg) const
{	if (cfg < AAMPCONFIG_BOOL_COUNT)
	{
		return configValueBool[cfg].value;
	}
	return false;
}

bool AampConfig::GetConfigValue( AAMPConfigSettingBool cfg ) const
{
	if(cfg < AAMPCONFIG_BOOL_COUNT)
	{
		return configValueBool[cfg].value;
	}
	return false;
}
/**
 * @brief GetConfigValue - Gets configuration for integer data type
 *
 */
int AampConfig::GetConfigValue(AAMPConfigSettingInt cfg) const
{
	if(cfg < AAMPCONFIG_INT_COUNT)
	{
		return configValueInt[cfg].value;
	}
	return 0;
}
/**
 * @brief GetConfigValue - Gets configuration for double data type
 *
 */
double AampConfig::GetConfigValue(AAMPConfigSettingFloat cfg) const
{
	if(cfg < AAMPCONFIG_FLOAT_COUNT)
	{
		return configValueFloat[cfg].value;
	}
	return 0.0;
}

/**
 * @brief GetConfigValue - Gets configuration for string data type
 *
 */
std::string AampConfig::GetConfigValue(AAMPConfigSettingString cfg) const
{
	if(cfg < AAMPCONFIG_STRING_COUNT)
	{
		return configValueString[cfg].value;
	}
	return "";
}

/**
 * @brief GetConfigOwner - Gets configuration Owner
 *
 * @return ConfigPriority - owner of the config
 */
ConfigPriority AampConfig::GetConfigOwner(AAMPConfigSettingBool cfg) const
{
	return configValueBool[cfg].owner;
}
ConfigPriority AampConfig::GetConfigOwner(AAMPConfigSettingInt cfg) const
{
	return configValueInt[cfg].owner;
}
ConfigPriority AampConfig::GetConfigOwner(AAMPConfigSettingFloat cfg) const
{
	return configValueFloat[cfg].owner;
}
ConfigPriority AampConfig::GetConfigOwner(AAMPConfigSettingString cfg) const
{
	return configValueString[cfg].owner;
}

/**
 * @brief GetChannelOverride - Gets channel override url for channel Name
 *
 * @return true - if valid return
 */
const char * AampConfig::GetChannelOverride(const std::string manifestUrl) const
{
	if(mChannelOverrideMap.size() && manifestUrl.size())
	{
		for (auto it = mChannelOverrideMap.begin(); it != mChannelOverrideMap.end(); ++it)
		{
			const ConfigChannelInfo &pChannelInfo = *it;
			if (manifestUrl.find(pChannelInfo.name) != std::string::npos)
			{
				return pChannelInfo.uri.c_str();
			}
		}
	}
	return NULL;
}

/**
 * @brief GetChannelLicenseOverride - Gets channel License override url for channel Url
 *
 * @return true - if valid return
 */
const char * AampConfig::GetChannelLicenseOverride(const std::string manifestUrl) const
{
    if(mChannelOverrideMap.size() && manifestUrl.size())
    {
        for (auto it = mChannelOverrideMap.begin(); it != mChannelOverrideMap.end(); ++it)
        {
            const ConfigChannelInfo &pChannelInfo = *it;
            if (manifestUrl.find(pChannelInfo.uri) != std::string::npos)
            {
                if(!pChannelInfo.licenseUri.empty())
                {
                    return pChannelInfo.licenseUri.c_str();
                }
            }
        }
    }
    return NULL;
}

void AampConfig::SetConfigValue(ConfigPriority newowner, AAMPConfigSettingBool cfg ,const bool &value)
{
	const char * cfgName = GetConfigName(cfg);
	ConfigValueBool &setting = configValueBool[cfg];
	if(setting.owner <= newowner )
	{
		if(setting.owner != newowner)
		{
			setting.lastvalue = setting.value;
			setting.lastowner = setting.owner;
		}
		setting.value = value;
		setting.owner = newowner;
		AAMPLOG_MIL("%s New Owner[%d]",cfgName,newowner);
	}
	else
	{
		AAMPLOG_WARN("%s Owner[%d] not allowed to Set ,current Owner[%d]",cfgName,newowner,setting.owner);
	}
}

void AampConfig::SetConfigValue(ConfigPriority newowner, AAMPConfigSettingInt cfg ,const int &value)
{
	auto cfgInfo = mConfigLookupTableInt[cfg];
	ConfigValueInt &setting = configValueInt[cfg];
	auto range = mConfigValueValidRange[cfgInfo.validRange];
	if( value<range.minValue || value>range.maxValue )
	{
		AAMPLOG_ERR(ERROR_TEXT_BAD_RANGE);
	}
	else if(setting.owner <= newowner )
	{
		if(setting.owner != newowner)
		{
			setting.lastvalue = setting.value;
			setting.lastowner = setting.owner;
		}
		setting.value = value;
		setting.owner = newowner;
		AAMPLOG_MIL("%s New Owner[%d]", cfgInfo.cmdString, newowner);
	}
	else
	{
		AAMPLOG_WARN("%s Owner[%d] not allowed to Set ,current Owner[%d]", cfgInfo.cmdString, newowner, setting.owner);
	}
}

void AampConfig::SetConfigValue(ConfigPriority newowner, AAMPConfigSettingFloat cfg ,const double &value)
{
	auto cfgInfo = mConfigLookupTableFloat[cfg];
	ConfigValueFloat &setting = configValueFloat[cfg];
	auto range = mConfigValueValidRange[cfgInfo.validRange];
	if( value<range.minValue || value>range.maxValue )
	{
		AAMPLOG_ERR(ERROR_TEXT_BAD_RANGE);
	}
	else if(setting.owner <= newowner )
	{
		if(setting.owner != newowner)
		{
			setting.lastvalue = setting.value;
			setting.lastowner = setting.owner;
		}
		setting.value = value;
		setting.owner = newowner;
		AAMPLOG_MIL("%s New Owner[%d]",cfgInfo.cmdString,newowner);
	}
	else
	{
		AAMPLOG_WARN("%s Owner[%d] not allowed to Set ,current Owner[%d]", cfgInfo.cmdString, newowner, setting.owner);
	}
}

void AampConfig::SetConfigValue(ConfigPriority newowner, AAMPConfigSettingString cfg ,const std::string &value)
{
	const char * cfgName = GetConfigName(cfg);
	ConfigValueString &setting = configValueString[cfg];
	if(setting.owner <= newowner )
	{
		if(setting.owner != newowner)
		{
			setting.lastvalue = setting.value;
			setting.lastowner = setting.owner;
		}
		setting.value = value;
		setting.owner = newowner;
		AAMPLOG_MIL("%s New Owner[%d]",cfgName,newowner);
	}
	else
	{
		AAMPLOG_WARN("%s Owner[%d] not allowed to Set ,current Owner[%d]", cfgName, newowner, setting.owner);
	}
}

/**
 * @brief ProcessConfigJson - Function to parse and process json configuration string
 *
 * @return bool - true on success
 */
bool AampConfig::ProcessConfigJson(const cJSON *cfgdata, ConfigPriority owner )
{
	bool retval = false;

	if(cfgdata != NULL)
	{
		cJSON *custom = cJSON_GetObjectItem(cfgdata, "Custom");
		if((custom != NULL) && (owner == AAMP_DEV_CFG_SETTING))
		{
			CustomArrayRead( custom,owner );
			customFound = true;
		}

		for(cJSON *searchObj = cfgdata->child; NULL != searchObj; searchObj=searchObj->next)
		{
			mConfigLookup.Process( this, owner, searchObj );
		}
		// checked all the config string in json
		// next check is channel override array is present
		cJSON *chMap = cJSON_GetObjectItem(cfgdata,"chmap");
		if(chMap)
		{
			if(cJSON_IsArray(chMap))
			{
				for (int i = 0 ; i < cJSON_GetArraySize(chMap) ; i++)
				{
					cJSON * subitem = cJSON_GetArrayItem(chMap, i);
					char *name      = (char *)cJSON_GetObjectItem(subitem, "name")->valuestring;
					char *url       = (char *)cJSON_GetObjectItem(subitem, "url")->valuestring;
					cJSON * license = cJSON_GetObjectItem(subitem, "licenseServerUrl");
					const char *licenseUrl= license ? license->valuestring : "";
					if(name && url )
					{
						ConfigChannelInfo channelInfo;
						channelInfo.uri = url;
						channelInfo.name = name;
						channelInfo.licenseUri = licenseUrl;
						mChannelOverrideMap.push_back(channelInfo);
					}
				}
			}
			else
			{
				AAMPLOG_ERR("JSON Channel Override format is wrong");
			}
		}
		cJSON *drmConfig = cJSON_GetObjectItem(cfgdata,"drmConfig");
		if(drmConfig)
		{
			AAMPLOG_MIL("Parsed value for property DrmConfig");
			cJSON *subitem = drmConfig->child;
			DRMSystems drmType = eDRM_PlayReady;
			while( subitem )
			{
				std::string conv = std::string(subitem->valuestring);
				if(strcasecmp("com.microsoft.playready",subitem->string)==0)
				{
					AAMPLOG_MIL("Playready License Server URL config param received - %s", conv.c_str());
					SetConfigValue(owner,eAAMPConfig_PRLicenseServerUrl,conv);
					drmType = eDRM_PlayReady;
				}
				if(strcasecmp("com.widevine.alpha",subitem->string)==0)
				{
					AAMPLOG_MIL("Widevine License Server URL config param received - %s", conv.c_str());
					SetConfigValue(owner,eAAMPConfig_WVLicenseServerUrl,conv);
					drmType = eDRM_WideVine;
				}
				if(strcasecmp("org.w3.clearkey",subitem->string)==0)
				{
					AAMPLOG_MIL("ClearKey License Server URL config param received - %s", conv.c_str());
					SetConfigValue(owner,eAAMPConfig_CKLicenseServerUrl,conv);
					drmType = eDRM_ClearKey;
				}
				if(strcasecmp("customData",subitem->string)==0)
				{
					AAMPLOG_MIL("customData received - %s", conv.c_str());
					SetConfigValue(owner,eAAMPConfig_CustomLicenseData,conv);
				}
				subitem = subitem->next;
			}

			// preferredKeysystem used to disambiguate DRM type to use when manifest advertises multiple supported systems.
			cJSON *preferredKeySystemItem = cJSON_GetObjectItem(drmConfig, "preferredKeysystem");
			if (preferredKeySystemItem && cJSON_IsString(preferredKeySystemItem))
			{
				const char * preferredKeySystem = preferredKeySystemItem->valuestring;
				AAMPLOG_MIL("preferredKeySystem received - %s", preferredKeySystem );
				if( strcmp(preferredKeySystem,"com.widevine.alpha")==0 )
				{
					drmType = eDRM_WideVine;
				}
				else if ( strcmp(preferredKeySystem,"com.microsoft.playready")==0 )
				{
					drmType = eDRM_PlayReady;
				}
				else if ( strcmp(preferredKeySystem,"org.w3.clearkey")==0 )
				{
					drmType = eDRM_ClearKey;
				}
			}
			SetConfigValue(owner, eAAMPConfig_PreferredDRM, (int)drmType);
		}
		retval = true;
	}

	return retval;
}

/**
 * @brief CustomArrayRead - Function to Read Custom JSON Array
 * @return void
 */
void AampConfig::CustomArrayRead( cJSON *customArray,ConfigPriority owner )
{
	std::string keyname;
	customJson customValues;
	cJSON *customVal=NULL;
	cJSON *searchVal=NULL;
	customOwner = owner;
	if(owner == AAMP_DEV_CFG_SETTING)
	{
		customOwner = AAMP_CUSTOM_DEV_CFG_SETTING;
	}

	int length = cJSON_GetArraySize(customArray);
	if(customArray != NULL)
	{
		for(int i = 0; i < length ; i++)
		{
			customVal = cJSON_GetArrayItem(customArray,i);
			if((searchVal = cJSON_GetObjectItem(customVal,"url")) != NULL)
			{
				keyname = "url";
			}
			else if((searchVal = cJSON_GetObjectItem(customVal,"playerId")) != NULL)
			{
				keyname = "playerId";
			}
			else if((searchVal = cJSON_GetObjectItem(customVal,"appName")) != NULL)
			{
				keyname = "appName";
			}
			customValues.config = keyname;
			if(searchVal && searchVal->valuestring != NULL)
			{
				customValues.configValue = searchVal->valuestring;
				vCustom.push_back(customValues);
			}
			else
			{
				AAMPLOG_ERR("Invalid format for %s",keyname.c_str());
				continue;
			}
			mConfigLookup.Process( this, customVal, customValues, vCustom );
		}
		for(int i = 0; i < vCustom.size(); i++)
		{
			AAMPLOG_MIL("Custom Values listed %s %s",vCustom[i].config.c_str(),vCustom[i].configValue.c_str());
		}
	}
}

/**
 * @brief (re)apply configuration for specified player instance at tune time
 * @param url: locator being tuned
 * @param playerId identifer for player instance
 * @param appname
 */
bool AampConfig::CustomSearch( std::string url, int playerId , std::string appname)
{
	if(customFound == false)
	{
		return false;
	}
	bool found = false;
	AAMPLOG_INFO("url %s playerid %d appname %s ",url.c_str(),playerId,appname.c_str());
	std::string url_custom = url;
	std::string playerId_custom = std::to_string(playerId);
	std::string appName_custom = appname;
	std::string keyname;
	std::string urlName = "url";
	std::string player = "playerId";
	std::string appName = "appName";
	size_t foundurl;
	int index = 0;
	do{
		auto it = std::find_if( vCustom.begin(), vCustom.end(),[](const customJson & item) { return item.config == "url"; });
		if (it != vCustom.end())
		{
			int distance = (int)std::distance(vCustom.begin(),it);
			foundurl = url_custom.find(vCustom[distance].configValue);
			if( foundurl != std::string::npos)
			{
				index = distance;
				AAMPLOG_INFO("FOUND URL %s", vCustom[index].configValue.c_str());
				found = true;
				break;
			}
		}
		auto it1 = std::find_if( vCustom.begin(), vCustom.end(),[](const customJson & item) { return item.config == "playerId"; });
		if (it1 != vCustom.end())
		{
			int distance = (int)std::distance(vCustom.begin(),it1);
			foundurl = playerId_custom.find(vCustom[distance].configValue);
			if( foundurl != std::string::npos)
			{
				index = distance;
				AAMPLOG_INFO("FOUND PLAYERID %s", vCustom[index].configValue.c_str());
				found = true;
				break;
			}
		}
		auto it2 = std::find_if( vCustom.begin(), vCustom.end(),[](const customJson & item) { return item.config == "appName"; });
		if (it2 != vCustom.end())
		{
			int distance = (int)std::distance(vCustom.begin(),it2);
			foundurl = appName_custom.find(vCustom[distance].configValue);
			if( foundurl != std::string::npos)
			{
				index = distance;
				AAMPLOG_INFO("FOUND AAPNAME %s",vCustom[index].configValue.c_str());
				found = true;
				break;
			}
		}
        	//Not applicable values
	}while (0);

	if (found == true)
	{
		for( int i = index+1; i < vCustom.size(); i++ )
		{
			if(vCustom[i].config == urlName){
				break;}
			else if(vCustom[i].config == player){
				break;}
			else if(vCustom[i].config == appName){
				break;}
			else
			{
				mConfigLookup.Process( this, vCustom[i] );
			}
		}

		ConfigureLogSettings();
	}
	return found;
}

/**
 * @brief GetAampConfigJSONStr - Function to Complete Config as JSON str
 *
 * @return true
 */
bool AampConfig::GetAampConfigJSONStr(std::string &str) const
{
	AampJsonObject jsondata;

	// All Bool values
	for(int i=0;i<AAMPCONFIG_BOOL_COUNT;i++)
	{
		jsondata.add(GetConfigName((AAMPConfigSettingBool)i),configValueBool[i].value);
	}

	// All integer values
	for(int i=0;i<AAMPCONFIG_INT_COUNT;i++)
	{
		jsondata.add(GetConfigName((AAMPConfigSettingInt)i),configValueInt[i].value);
	}

	// All double values
	for(int i=0;i<AAMPCONFIG_FLOAT_COUNT;i++)
	{
		jsondata.add(GetConfigName((AAMPConfigSettingFloat)i),configValueFloat[i].value);
	}

	// All String values
	for(int i=0;i<AAMPCONFIG_STRING_COUNT;i++)
	{
		jsondata.add(GetConfigName((AAMPConfigSettingString)i),configValueString[i].value);
	}

	str = jsondata.print_UnFormatted();
	return true;
}

/**
 * @brief ProcessConfigText - Function to parse and process configuration text
 *
 * @return true if config process success
 */
void AampConfig::ProcessConfigText(std::string &cfg, ConfigPriority owner )
{
	if( !cfg.empty() )
	{
		char c = cfg[0];
		if( c<' ' )
		{ // ignore newline
		}
		else if( c == '#')
		{ // ignore comments
		}
		else if( c == '*')
		{// wildcard matching for channel override feature
			std::size_t pos = cfg.find_first_of(' ');
			if (pos != std::string::npos)
			{ // at least one space delimiter
				ConfigChannelInfo channelInfo;
				std::stringstream iss(cfg.substr(1));
				std::string token;
				while (getline(iss, token, ' '))
				{
					const char *uri = token.c_str();
					if( aamp_isTuneScheme(uri) )
					{
						AAMPLOG_INFO("Override %s", uri );
						channelInfo.uri = token;
					}
					else if (token.compare(0,17,"licenseServerUrl=") == 0)
					{
						channelInfo.licenseUri = token.substr(17);
					}
					else
					{
						channelInfo.name = token;
					}
				}
				mChannelOverrideMap.push_back(channelInfo);
			}
		}
		else
		{
			//trim whitespace from the end of the string
			cfg.erase(std::find_if(cfg.rbegin(), cfg.rend(), [](unsigned char ch) {return !std::isspace(ch);}).base(), cfg.end());
			size_t position = 0;
			std::string key,value;
			std::size_t delimiterPos = cfg.find("=");
			if(delimiterPos != std::string::npos)
			{
				key = cfg.substr(0, delimiterPos);
				key.erase(std::remove_if(key.begin(), key.end(), ::isspace), key.end());
				value = cfg.substr(delimiterPos + 1);
				position = value.find_first_not_of(' ');
				if( position == std::string::npos )
				{
					AAMPLOG_WARN( "unexpected cfg: '%s'", cfg.c_str() );
				}
				else
				{
					value = value.substr(position);
				}
			}
			else
			{
				key = cfg.substr(0);
			}

			mConfigLookup.Process( this, owner, key, value );
		}
	}
}

/**
 * @brief ReadAampCfgJsonFile - Function to parse and process configuration file in json format
 *
 * @return true if read successfully
 */
bool AampConfig::ReadAampCfgJsonFile()
{
	bool retVal=false;
	std::string cfgPath = aamp_GetConfigPath(AAMP_JSON_PATH);

	if (!cfgPath.empty())
	{
		std::ifstream f(cfgPath, std::ifstream::in | std::ifstream::binary);
		if (f.good())
		{
			AAMPLOG_MIL("opened aampcfg.json");
			std::filebuf* pbuf = f.rdbuf();
			std::size_t size = pbuf->pubseekoff (0,f.end,f.in);
			pbuf->pubseekpos (0,f.in);
			char* jsonbuffer=new char[size+1];
			pbuf->sgetn (jsonbuffer,size);
			jsonbuffer[size] = 0x00;
			f.close();

			if( jsonbuffer )
			{
				cJSON *cfgdata = cJSON_Parse(jsonbuffer);
				ProcessConfigJson(cfgdata, AAMP_DEV_CFG_SETTING);
				cJSON_Delete(cfgdata);
			}
			SAFE_DELETE_ARRAY(jsonbuffer);
			DoCustomSetting(AAMP_DEV_CFG_SETTING);
			retVal = true;
		}
	}
	return retVal;
}

/**
 * @brief ReadAampCfgTxtFile - Function to parse and process configuration file in text format
 *
 */
bool AampConfig::ReadAampCfgTxtFile()
{
	bool retVal = false;
	std::string cfgPath = aamp_GetConfigPath(AAMP_CFG_PATH);

	if (!cfgPath.empty())
	{
		std::ifstream f(cfgPath, std::ifstream::in | std::ifstream::binary);
		if (f.good())
		{
			AAMPLOG_MIL("opened aamp.cfg");
			std::string buf;
			while (f.good())
			{
				std::getline(f, buf);
				ProcessConfigText(buf, AAMP_DEV_CFG_SETTING);
			}
			f.close();
			DoCustomSetting(AAMP_DEV_CFG_SETTING);
			retVal = true;
		}
	}
	return retVal;
}

/**
 * @brief ProcessBase64AampCfg - Function to parse and process Base64 AampConfig
 *
 * @return true if Base64 process successfully
 */
bool AampConfig::ProcessBase64AampCfg(const char * base64Config, size_t configLen, ConfigPriority cfgPriority)
{
	bool bCharCompliant = false;
	if(base64Config && (configLen > 0))
	{
		bCharCompliant = true;
		for (int i = 0; i < configLen; i++)
		{
			if (!( base64Config[i] == 0xD || base64Config[i] == 0xA) && // ignore LF and CR chars
				((base64Config[i] < 0x20) || (base64Config[i] > 0x7E)))
			{
				bCharCompliant = false;
				AAMPLOG_ERR("Non Compliant char[0x%X] found, Ignoring whole config ",base64Config[i]);
				break;
			}
		}

		if (bCharCompliant)
		{
			std::string strCfg(base64Config,configLen);
			cJSON *cfgdata = cJSON_Parse(strCfg.c_str());
			if(!ProcessConfigJson(cfgdata,cfgPriority))
			{
				// Input received is not json format, parse as text
				std::istringstream iSteam(strCfg);
				std::string line;
				while (std::getline(iSteam, line))
				{
					if (line.length() > 0)
					{
						AAMPLOG_INFO("aamp-cmd:[%s]", line.c_str());
						ProcessConfigText(line,cfgPriority);
					}
				}
			}
		}
	}
	return bCharCompliant;
}

/**
 * @fn ReadBase64TR181Param reads Tr181 parameter at Device.DeviceInfo.X_RDKCENTRAL-COM_RFC.Feature.AAMP_CFG.b64Config
 * @return void
 */
void AampConfig::ReadBase64TR181Param()
{
	size_t iConfigLen = 0;
	if(PlayerExternalsInterface::IsPlayerExternalsInterfaceInstanceActive())
	{
		std::shared_ptr<PlayerExternalsInterface> pInstance = PlayerExternalsInterface::GetPlayerExternalsInterfaceInstance();
		char * cloudConf = pInstance->GetTR181PlayerConfig("Device.DeviceInfo.X_RDKCENTRAL-COM_RFC.Feature.AAMP_CFG.b64Config", iConfigLen);
		if(NULL != cloudConf)
		{	
			ProcessBase64AampCfg(cloudConf, iConfigLen,AAMP_OPERATOR_SETTING);
			free(cloudConf); // allocated by base64_Decode in GetTR181PlayerConfig
			ConfigureLogSettings();
		}
	}
}

/**
* @fn ReadAampCfgFromEnv parse and process AampCfg from environment variable
* Ex usage : "AAMP_CFG_TEXT=info=true,progress=true" (pass as comma separated text)
*       ( or ) "AAMP_CFG_BASE64=aW5mbz10cnVlCnByb2dyZXNzPXRydWU=" (Base64 for info=true and progress=true)
* @return Void
*/
void AampConfig::ReadAampCfgFromEnv()
{
	const char *envConf = getenv("AAMP_CFG_TEXT");
	// First check for Comma separated config text, this is done to make config  human readable
	// e.g info=true,progress=true
	if(NULL != envConf)
	{
		std::string strEnvConfig = envConf; // make sure we copy this as recommended by getEnv doc
		AAMPLOG_MIL("ReadAampCfgFromEnv:Text ENV:%s len:%zu ",strEnvConfig.c_str(),strEnvConfig.length());
		std::stringstream ss (strEnvConfig);
		std::string item;

		while (getline (ss, item, ',')) { // split on comma and get as line
			if (item.length() > 0)
			{
				ProcessConfigText(item,AAMP_DEV_CFG_SETTING);
			}
			else
			{
			}
		}
	}

	// Now check for base64 based env, this is back up in case above string becomes big and becomes error prone, also  base64 covers json format as well.
	envConf = getenv("AAMP_CFG_BASE64");
	if (NULL != envConf)
	{
		std::string strEnvConfig = envConf; // make sure we copy this as recommended by getEnv doc
		size_t iConfigLen = strEnvConfig.length();
		AAMPLOG_MIL("ReadAampCfgFromEnv:BASE64 ENV:%s len:%zu ", strEnvConfig.c_str(), iConfigLen);
		char *strConfig = (char *)base64_Decode(strEnvConfig.c_str(), &iConfigLen);
		if (NULL != strConfig)
		{
			ProcessBase64AampCfg(strConfig, iConfigLen, AAMP_DEV_CFG_SETTING);
			free(strConfig); // free mem allocated by base64_Decode
		}
	}

	DoCustomSetting(AAMP_DEV_CFG_SETTING);
}

/**
 * @fn ReadAllTR181Params reads  All Tr181 parameters at Device.DeviceInfo.X_RDKCENTRAL-COM_RFC.aamp.<param-name>
 * @return void
 */
static std::string getRFCValue( const char *strParamName )
{
	const std::string  strAAMPTr181BasePath = "Device.DeviceInfo.X_RDKCENTRAL-COM_RFC.aamp.";
	std::string value = RFCSettings::readRFCValue(strAAMPTr181BasePath+strParamName,PLAYER_NAME);
	return value;
}

void AampConfig::ReadAllTR181Params()
{
	long long begin = NOW_STEADY_TS_MS; // profile execution time of ReadAllTR181Params

	ConfigPriority owner = AAMP_OPERATOR_SETTING;

	for( int i =0; i < AAMPCONFIG_BOOL_COUNT; i++ )
	{
		const ConfigLookupEntryBool &entry = mConfigLookupTableBool[i];
		if( entry.bConfigurableByOperatorRFC )
		{
			std::string value = getRFCValue(entry.cmdString);
			if( !value.empty() )
			{
				SetConfigValue( owner, entry.configEnum, ConfigLookup::ConfigStringValueToBool(value.c_str()) );
			}
		}
	}

	for( int i =0; i < ARRAY_SIZE(mConfigLookupTableInt); i++ )
	{
		const ConfigLookupEntryInt &entry = mConfigLookupTableInt[i];
		if( entry.bConfigurableByOperatorRFC )
		{
			std::string value = getRFCValue(entry.cmdString);
			if( !value.empty() )
			{
				SetConfigValue( owner, entry.configEnum, std::stoi(value) );
			}
		}
	}

	for( int i =0; i < AAMPCONFIG_FLOAT_COUNT; i++ )
	{
		const ConfigLookupEntryFloat &entry = mConfigLookupTableFloat[i];
		if( entry.bConfigurableByOperatorRFC )
		{
			std::string value = getRFCValue(entry.cmdString);
			if( !value.empty() )
			{
				SetConfigValue( owner, entry.configEnum, std::stod(value) );
			}
		}
	}

	for( int i =0; i < AAMPCONFIG_STRING_COUNT; i++ )
	{
		const ConfigLookupEntryString &entry = mConfigLookupTableString[i];
		if( entry.bConfigurableByOperatorRFC )
		{
			std::string value = getRFCValue(entry.cmdString);
			if( !value.empty() )
			{
				SetConfigValue( owner, entry.configEnum, value );
			}
		}
	}
	AAMPLOG_MIL("ReadAllTR181Params took %lld ms to execute", (NOW_STEADY_TS_MS - begin));
}


/**
 * @brief ReadOperatorConfiguration - Reads Operator configuration from RFC and env variables
 *
 */
void AampConfig::ReadOperatorConfiguration()
{
	// Tr181 doesn't work in container environment hence ignore it if it is container
	// this will improve load time of aamp in container environment
	
	// Not all parameters are supported as  individual  tr181 parameter hence keeping base64 version.
	ReadBase64TR181Param();

	// new way of reading RFC for each separate parameter it will override any parameter set before ReadBase64TR181Param
	// read all individual  config parameters,
	ReadAllTR181Params();

	// this required to set log settings based on configs either default or read from Tr181
	ConfigureLogSettings();   
	///////////// Read environment variables set specific to Operator ///////////////////
	const char *env_aamp_force_aac = getenv("AAMP_FORCE_AAC");
	if(env_aamp_force_aac)
	{
		AAMPLOG_INFO("AAMP_FORCE_AAC present: Changing preference to AAC over ATMOS & DD+");
		SetConfigValue(AAMP_OPERATOR_SETTING,eAAMPConfig_DisableAC4,true);
		SetConfigValue(AAMP_OPERATOR_SETTING,eAAMPConfig_DisableEC3,true);
		SetConfigValue(AAMP_OPERATOR_SETTING,eAAMPConfig_DisableAC3,true);
		SetConfigValue(AAMP_OPERATOR_SETTING,eAAMPConfig_DisableATMOS,true);
		SetConfigValue(AAMP_OPERATOR_SETTING,eAAMPConfig_StereoOnly,true);
	}

	const char *env_aamp_min_init_cache = getenv("AAMP_MIN_INIT_CACHE");
	if(env_aamp_min_init_cache)
	{
		int minInitCache = 0;
		if(sscanf(env_aamp_min_init_cache,"%d",&minInitCache) && minInitCache >= 0)
		{
			AAMPLOG_INFO("AAMP_MIN_INIT_CACHE present: Changing min initial cache to %d seconds",minInitCache);
			SetConfigValue(AAMP_OPERATOR_SETTING,eAAMPConfig_InitialBuffer,minInitCache);
		}
	}


	const char *env_enable_cdai = getenv("CLIENT_SIDE_DAI");
	if(env_enable_cdai)
	{
		AAMPLOG_INFO("CLIENT_SIDE_DAI present: Enabling CLIENT_SIDE_DAI.");
		SetConfigValue(AAMP_OPERATOR_SETTING,eAAMPConfig_EnableClientDai,true);
	}

	const char *env_enable_westeros_sink = getenv("AAMP_ENABLE_WESTEROS_SINK");
	if(env_enable_westeros_sink)
	{

		int iValue = atoi(env_enable_westeros_sink);
		bool bValue = (strcasecmp(env_enable_westeros_sink,"true") == 0);

		AAMPLOG_INFO("AAMP_ENABLE_WESTEROS_SINK present, Value = %d", (bValue ? bValue : (iValue ? iValue : 0)));

		if(iValue || bValue)
		{
			AAMPLOG_INFO("AAMP_ENABLE_WESTEROS_SINK present: Enabling westeros-sink.");
			SetConfigValue(AAMP_OPERATOR_SETTING,eAAMPConfig_UseWesterosSink,true);
		}

	}

	const char *env_enable_lld = getenv("LOW_LATENCY_DASH");
	if(env_enable_lld)
	{
		AAMPLOG_INFO("LOW_LATENCY_DASH present: Enabling LOW_LATENCY_DASH");
		SetConfigValue(AAMP_OPERATOR_SETTING,eAAMPConfig_EnableLowLatencyDash,true);
	}
}

/**
 * @brief ConfigureLogSettings - This function configures log settings for LogManager instance
 *
 */
void AampConfig::ConfigureLogSettings()
{
	std::string logString = configValueString[eAAMPConfig_LogLevel].value;

	if(configValueBool[eAAMPConfig_TraceLogging].value || logString.compare("trace") == 0)
	{
		AampLogManager::setLogLevel(eLOGLEVEL_TRACE);
		AampLogManager::lockLogLevel(true);
	}
	else if(configValueBool[eAAMPConfig_DebugLogging].value || logString.compare("debug") == 0)
	{
		AampLogManager::setLogLevel(eLOGLEVEL_DEBUG);
		AampLogManager::lockLogLevel(true);
	}
	else if((configValueBool[eAAMPConfig_InfoLogging].value || logString.compare("info") == 0))
	{
		AampLogManager::setLogLevel(eLOGLEVEL_INFO);
		AampLogManager::lockLogLevel(true);
	}
}

/**
 * @brief ShowOperatorSetConfiguration - List all operator configured settings
 */
void AampConfig::ShowOperatorSetConfiguration()
{
	////////////////// AAMP Config (Operator Set) //////////
	ShowConfiguration(AAMP_OPERATOR_SETTING);
}

/**
 * @brief ShowAppSetConfiguration - List all Application configured settings
 */
void AampConfig::ShowAppSetConfiguration()
{
	////////////////// AAMP Config (Application Set) //////////
	ShowConfiguration(AAMP_APPLICATION_SETTING);
}

/**
 * @brief ShowStreamSetConfiguration - List all stream configured settings
 */
void AampConfig::ShowStreamSetConfiguration()
{
	ShowConfiguration(AAMP_STREAM_SETTING);
}

/**
 * @brief ShowDefaultAampConfiguration - List all AAMP Default settings
 *
 */
void AampConfig::ShowDefaultAampConfiguration()
{
	ShowConfiguration(AAMP_DEFAULT_SETTING);
}

/**
 * @brief ShowDevCfgConfiguration - List all developer configured settings
 */
void AampConfig::ShowDevCfgConfiguration()
{
	ShowConfiguration(AAMP_DEV_CFG_SETTING);
}

/**
 * @brief ShowAAMPConfiguration - Show all settings for every owner
 *
 */
void AampConfig::ShowAAMPConfiguration()
{
	ShowConfiguration(AAMP_MAX_SETTING);
}

///////////////////////////////// Private Functions ///////////////////////////////////////////

/**
 * @brief DoCustomSetting - Function to do override , to avoid complexity with multiple configs
 */
void AampConfig::DoCustomSetting(ConfigPriority owner)
{
	if(IsConfigSet(eAAMPConfig_StereoOnly))
	{
		// If Stereo Only flag is set , it will override all other sub setting with audio
		SetConfigValue(owner,eAAMPConfig_DisableEC3,true);
		SetConfigValue(owner,eAAMPConfig_DisableATMOS,true);
		SetConfigValue(owner,eAAMPConfig_DisableAC4,true);
		SetConfigValue(owner,eAAMPConfig_DisableAC3,true);
	}
	if(IsConfigSet(eAAMPConfig_ABRBufferCheckEnabled) && (GetConfigOwner(eAAMPConfig_ABRBufferCheckEnabled) == AAMP_APPLICATION_SETTING))
	{
		SetConfigValue(AAMP_APPLICATION_SETTING,eAAMPConfig_NewDiscontinuity,true);
		SetConfigValue(AAMP_APPLICATION_SETTING,eAAMPConfig_HLSAVTrackSyncUsingStartTime,true);

	}
	if((!IsConfigSet(eAAMPConfig_EnableRectPropertyCfg)) && (GetConfigOwner(eAAMPConfig_EnableRectPropertyCfg) == AAMP_APPLICATION_SETTING))
	{
		if(!IsConfigSet(eAAMPConfig_UseWesterosSink))
		{
			SetConfigValue(AAMP_APPLICATION_SETTING,eAAMPConfig_EnableRectPropertyCfg,true);
		}
	}
	if(IsConfigSet(eAAMPConfig_NewDiscontinuity) && (GetConfigOwner(eAAMPConfig_NewDiscontinuity) == AAMP_APPLICATION_SETTING))
	{
		SetConfigValue(AAMP_APPLICATION_SETTING,eAAMPConfig_HLSAVTrackSyncUsingStartTime,true);
	}
	if(GetConfigOwner(eAAMPConfig_AuthToken) == AAMP_APPLICATION_SETTING)
	{
		ConfigPriority tempowner;
		std::string tempvalue;
		std::string sessionToken;
		tempowner = configValueString[eAAMPConfig_AuthToken].lastowner;
		tempvalue = configValueString[eAAMPConfig_AuthToken].lastvalue;

		sessionToken = GetConfigValue(eAAMPConfig_AuthToken);
		SetConfigValue(AAMP_TUNE_SETTING,eAAMPConfig_AuthToken,sessionToken);
		configValueString[eAAMPConfig_AuthToken].lastowner = tempowner;
		configValueString[eAAMPConfig_AuthToken].lastvalue = tempvalue;

	}
	if(GetConfigValue(eAAMPConfig_InitialBuffer) > 0)
	{
		//Enabling initialBuffer and gstBufferAndPlay together cause first frame freeze in specific platform.
		SetConfigValue(owner, eAAMPConfig_GStreamerBufferingBeforePlay, false);
	}
	ConfigureLogSettings();
}

const char * AampConfig::GetConfigName(AAMPConfigSettingBool cfg ) const
{
	return mConfigLookupTableBool[cfg].cmdString;
}
const char * AampConfig::GetConfigName(AAMPConfigSettingInt cfg ) const
{
	return mConfigLookupTableInt[cfg].cmdString;
}
const char * AampConfig::GetConfigName(AAMPConfigSettingFloat cfg ) const
{
	return mConfigLookupTableFloat[cfg].cmdString;
}
const char *AampConfig::GetConfigName(AAMPConfigSettingString cfg ) const
{
	return mConfigLookupTableString[cfg].cmdString;
}

/**
 * @brief RestoreConfiguration - Function is restore last configuration value from current ownership
 */
void AampConfig::RestoreConfiguration(ConfigPriority owner )
{
	// All Bool values
	for(int i=0;i<AAMPCONFIG_BOOL_COUNT;i++)
	{
		if(configValueBool[i].owner == owner && configValueBool[i].owner != configValueBool[i].lastowner)
		{
			AAMPLOG_MIL("Cfg [%-3d][%-20s][%-5s]->[%-5s][%s]->[%s]",i,GetConfigName((AAMPConfigSettingBool)i), mOwnerLookupTable[configValueBool[i].owner].ownerName,
				mOwnerLookupTable[configValueBool[i].lastowner].ownerName,configValueBool[i].value?"true":"false",configValueBool[i].lastvalue?"true":"false");
			configValueBool[i].owner = configValueBool[i].lastowner;
			configValueBool[i].value = configValueBool[i].lastvalue;
		}
	}

	// All integer values
	for(int i=0;i<AAMPCONFIG_INT_COUNT;i++)
	{
		// for int array
		if(configValueInt[i].owner == owner && configValueInt[i].owner != configValueInt[i].lastowner)
		{
			AAMPLOG_MIL("Cfg [%-3d][%-20s][%-5s]->[%-5s][%d]->[%d]",i,GetConfigName((AAMPConfigSettingInt)i), mOwnerLookupTable[configValueInt[i].owner].ownerName,
				mOwnerLookupTable[configValueInt[i].lastowner].ownerName,configValueInt[i].value,configValueInt[i].lastvalue);
			configValueInt[i].owner = configValueInt[i].lastowner;
			configValueInt[i].value = configValueInt[i].lastvalue;

		}
	}

	// All double values
	for(int i=0;i<AAMPCONFIG_FLOAT_COUNT;i++)
	{
		// for double array
		if(configValueFloat[i].owner == owner && configValueFloat[i].owner != configValueFloat[i].lastowner)
		{
			AAMPLOG_MIL("Cfg [%-3d][%-20s][%-5s]->[%-5s][%f]->[%f]",i,GetConfigName((AAMPConfigSettingFloat)i), mOwnerLookupTable[configValueFloat[i].owner].ownerName,
						mOwnerLookupTable[configValueFloat[i].lastowner].ownerName,configValueFloat[i].value,configValueFloat[i].lastvalue);
			configValueFloat[i].owner = configValueFloat[i].lastowner;
			configValueFloat[i].value = configValueFloat[i].lastvalue;
		}
	}


	// All String values
	for(int i=0;i<AAMPCONFIG_STRING_COUNT;i++)
	{
		// for string array
		if(configValueString[i].owner == owner && configValueString[i].owner != configValueString[i].lastowner)
		{
			AAMPLOG_MIL("Cfg [%-3d][%-20s][%-5s]->[%-5s][%s]->[%s]",i,GetConfigName((AAMPConfigSettingString)i), mOwnerLookupTable[configValueString[i].owner].ownerName,
				mOwnerLookupTable[configValueString[i].lastowner].ownerName,configValueString[i].value.c_str(),configValueString[i].lastvalue.c_str());
			configValueString[i].owner = configValueString[i].lastowner;
			configValueString[i].value = configValueString[i].lastvalue;
		}
	}

	if(owner == AAMP_CUSTOM_DEV_CFG_SETTING)
	{
		ConfigureLogSettings();
	}
}

/**
 * @brief RestoreConfiguration - Function is to restore last configuration value of a particular config given configpriority matches
 */
void AampConfig::RestoreConfiguration(ConfigPriority owner, AAMPConfigSettingBool cfg)
{
	if(configValueBool[cfg].owner == owner && configValueBool[cfg].owner != configValueBool[cfg].lastowner)
	{
		AAMPLOG_MIL("Cfg restoring [%-20s][%-5s]->[%-5s][%s]->[%s]",GetConfigName(cfg), mOwnerLookupTable[configValueBool[cfg].owner].ownerName,
					mOwnerLookupTable[configValueBool[cfg].lastowner].ownerName,configValueBool[cfg].value?"true":"false",configValueBool[cfg].lastvalue?"true":"false");
		configValueBool[cfg].owner = configValueBool[cfg].lastowner;
		configValueBool[cfg].value = configValueBool[cfg].lastvalue;
	}
}

/**
 * @brief RestoreConfiguration - Function is to restore last configuration value of a particular config given configpriority matches
 */
void AampConfig::RestoreConfiguration(ConfigPriority owner, AAMPConfigSettingInt cfg)
{
	if(configValueInt[cfg].owner == owner && configValueInt[cfg].owner != configValueInt[cfg].lastowner)
	{
		AAMPLOG_MIL("Cfg restoring [%-20s][%-5s]->[%-5s][%d]->[%d]",GetConfigName(cfg), mOwnerLookupTable[configValueInt[cfg].owner].ownerName,
					mOwnerLookupTable[configValueInt[cfg].lastowner].ownerName,configValueInt[cfg].value,configValueInt[cfg].lastvalue);
		configValueInt[cfg].owner = configValueInt[cfg].lastowner;
		configValueInt[cfg].value = configValueInt[cfg].lastvalue;

	}
}

/**
 * @brief RestoreConfiguration - Function is to restore last configuration value of a particular config given configpriority matches
 */
void AampConfig::RestoreConfiguration(ConfigPriority owner, AAMPConfigSettingFloat cfg)
{
	if(configValueFloat[cfg].owner == owner && configValueFloat[cfg].owner != configValueFloat[cfg].lastowner)
	{
		AAMPLOG_MIL("Cfg restoring [%-20s][%-5s]->[%-5s][%f]->[%f]",GetConfigName(cfg), mOwnerLookupTable[configValueFloat[cfg].owner].ownerName,
					mOwnerLookupTable[configValueFloat[cfg].lastowner].ownerName,configValueFloat[cfg].value,configValueFloat[cfg].lastvalue);
		configValueFloat[cfg].owner = configValueFloat[cfg].lastowner;
		configValueFloat[cfg].value = configValueFloat[cfg].lastvalue;
	}
}

/**
 * @brief RestoreConfiguration - Function is to restore last configuration value of a particular config given configpriority matches
 */
void AampConfig::RestoreConfiguration(ConfigPriority owner, AAMPConfigSettingString cfg)
{
	if(configValueString[cfg].owner == owner && configValueString[cfg].owner != configValueString[cfg].lastowner)
	{
		AAMPLOG_MIL("Cfg restoring [%-20s][%-5s]->[%-5s][%s]->[%s]",GetConfigName(cfg), mOwnerLookupTable[configValueString[cfg].owner].ownerName,
					mOwnerLookupTable[configValueString[cfg].lastowner].ownerName,configValueString[cfg].value.c_str(),configValueString[cfg].lastvalue.c_str());
		configValueString[cfg].owner = configValueString[cfg].lastowner;
		configValueString[cfg].value = configValueString[cfg].lastvalue;

	}
}

/**
 * @brief ShowConfiguration - Function to list configuration values based on the owner
 */
void AampConfig::ShowConfiguration(ConfigPriority owner)
{
	for( int i=0; i<AAMPCONFIG_BOOL_COUNT; i++ )
	{
		if(configValueBool[i].owner == owner || owner == AAMP_MAX_SETTING)
		{
			AAMPLOG_MIL("Cfg [%-34s][%-5s][%s]", GetConfigName((AAMPConfigSettingBool)i), mOwnerLookupTable[configValueBool[i].owner].ownerName,configValueBool[i].value?"true":"false");
		}
	}

	for( int i=0; i<AAMPCONFIG_INT_COUNT; i++ )
	{
		if(configValueInt[i].owner == owner || owner == AAMP_MAX_SETTING)
		{
			AAMPLOG_MIL("Cfg [%-34s][%-5s][%d]", GetConfigName((AAMPConfigSettingInt)i), mOwnerLookupTable[configValueInt[i].owner].ownerName,configValueInt[i].value);
		}
	}

	for( int i=0;  i<AAMPCONFIG_FLOAT_COUNT;i++ )
	{
		if(configValueFloat[i].owner == owner || owner == AAMP_MAX_SETTING)
		{
			AAMPLOG_MIL("Cfg [%-34s][%-5s][%f]", GetConfigName((AAMPConfigSettingFloat)i), mOwnerLookupTable[configValueFloat[i].owner].ownerName,configValueFloat[i].value);
		}
	}

	for(int i=0;i<AAMPCONFIG_STRING_COUNT;i++)
	{
		if(configValueString[i].owner == owner || owner == AAMP_MAX_SETTING)
		{
			AAMPLOG_MIL("Cfg [%-34s][%-5s][%s]", GetConfigName((AAMPConfigSettingString)i), mOwnerLookupTable[configValueString[i].owner].ownerName,configValueString[i].value.c_str());
		}
	}

	if(mChannelOverrideMap.size() && (owner == AAMP_DEV_CFG_SETTING || owner == AAMP_MAX_SETTING))
	{
		for ( auto iter = mChannelOverrideMap.begin(); iter != mChannelOverrideMap.end(); ++iter)
		{
			AAMPLOG_INFO("Cfg Channel[%s]-> [%s]",iter->name.c_str(),iter->uri.c_str());
			AAMPLOG_INFO("Cfg Channel[%s]-> License Uri: [%s]",iter->name.c_str(),iter->licenseUri.c_str());
		}
	}

}


