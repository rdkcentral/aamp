<!--
If not stated otherwise in this file or this component's license file the
following copyright and licenses apply:

Copyright 2026 RDK Management

Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.
-->

# ![](images/logo.png) <br/> AAMP / Universal Video Engine (UVE)

# V7.07

## Overview

### Unified Video Engine (UVE)

UVE is a flexible, full-featured video playback API designed for use from JavaScript. This document and sample applications demonstrate how to use the UVE APIs for video playback.

### Advanced Adaptive Media Player (AAMP)

AAMP is an open source native video engine that is built on top of GStreamer and optimized for performance, memory use, and code size. On RDK platforms, UVE-JS is the primary recommended way to interact with AAMP. AAMP's JavaScript bindings are made available using WebKit Injectedbundle.

## Target Audience

This document is targeted to application developers who are interested in evaluating/adopting AAMP for their media player applications on set-top boxes running RDKV based firmware. UVE API wrapper also exists which can be used in non-RDK browsers.

## Features

- **Formats:** HLS, DASH, Fragmented MP4 HLS, Progressive MP4
- **DRM Systems:** Clear Key, Vanilla AES-128, PlayReady, Widevine
- **Captions:** CEA-608/708 Captions, WebVTT
- **Ad Insertion:** Client DAI / Server Side Ad Insertion
- **Security:** Thumbnail / Watermarking, Intra Asset Encryption / DRM License Rotation
- **Audio:** DD+, Dolby ATMOS, AC4 Support
- **Low Latency:** Low Latency DASH
- **DVR:** [Time Shift Buffer](#tsb-feature) for DASH

## Acronyms

- **AAMP** - Advanced Adaptive Media Player
- **ABR** - Adaptive Bitrate
- **AC3/AC4** - Audio Codec 3/4
- **ATSC** - Advanced Television Systems Committee
- **CDVR** - Cloud Digital Video Recording
- **CMCD** - Common Media Client Data
- **DAI** - Dynamic Ad Insertion
- **DASH** - Dynamic Adaptive Streaming over HTTP
- **DRM** - Digital Rights Management
- **DVR** - Digital Video Recorder
- **HLS** - HTTP Live Streaming
- **JS** - JavaScript
- **LLHLS** - Low Latency HLS
- **LLDASH** - Low Latency DASH
- **MP4** - MPEG-4 Part 14
- **MPEG** - Moving Picture Experts Group
- **OTT** - Over The Top
- **RDK** - Reference Design Kit
- **RDKV** - Reference Design Kit for Video
- **TSB** - Time Shift Buffer
- **UVE** - Universal Video Engine
- **VOD** - Video On Demand
- **VTT** - Video Text Track
- **WebVTT** - Web Video Text Tracks

---

## Minimal Sample Player

```js
	<html><head><title>IP Video Playback in WPE browser using UVE API</title></head>
	<script>
	    window.onload = function() {
		    var player = new AAMPMediaPlayer();
		    var url = "https://example.com/multilang/main.m3u8"; // replace with valid URL
		    player.load(url);
	    }
	</script>
	<body>
		<div id="videoContainer">
			<video style="height:100%; width:100%; position:absolute; bottom:0; left:0">
			    <source src="dummy.mp4" type="video/ave"> <!-- "video/ave" required as WPE browser hint to signal hole punching -->
			</video>
		</div>
	</body>
	</html>
```
Click [here](#setup-reference-player) for Reference player setup for RDK

---

# Table of Contents

## Quick Links

- [Getting Started](#minimal-sample-player)
- [Architecture & Setup](ARCHITECTURE.md)
- [Configuration Guide](CONFIGURATION.md)
- [Troubleshooting](TROUBLESHOOTING.md)

## Core Sections

1. [Configuration](#configuration)
   - [initConfig()](#configuration)
   - [setDRMConfig()](#setdrmconfig-config-)
2. [Properties](#properties)
3. [API Methods](#methods)
   - [Playback Control](#load-uri-autoplay-tuneparams)
   - [Audio & Video Tracks](#getavailableaudiotracks-all)
   - [Text Tracks & Captions](#getavailabletexttracksalltracks)
   - [Thumbnails](#getthumbnails)
4. [Events](#uve-events)
5. [Error Codes](#universal-video-engine-player-errors)
6. [Setup & Reference](#setup-reference-player)

## Advanced Features

- [Client DAI](#client-dai-feature-support)
- [ATSC Support](#atsc---unified-video-engine-features)
- [TSB (Time Shift Buffer)](#tsb-feature)
- [Thumbnails & Watermarking](#thumbnails)
- [Captions](#captions-support)

---

## Configuration

Configuration options are passed to AAMP using the UVE `initConfig()` method. This allows the application to override default configuration used by AAMP player to give more control over player behavior. The parameter is a JSON Object with one or more attribute/value pairs as follows:

| Property | Type | Default Value | Description |
| -------- | ------- | ------------- | ----------- |
| abr | Boolean | true | Enable/disable adaptive bitrate logic. |
| abrCacheLength | Number | 3 | Length of ABR cache for network bandwidth calculation. |
| abrCacheLife | Number | 5000 | Lifetime value for ABR cache for network bandwidth calculation (in milliseconds). |
| abrCacheOutlier | Number | 5000000 | Outlier difference to ignore from network bandwidth calculation (default: 5 MB in bytes). |
| abrNwConsistency | Number | 2 | Number of checks before profile increment/decrement. Prevents frequent profile switching with network fluctuations. |
| abrSkipDuration | Number | 6 | Minimum duration of fragment to download before triggering ABR (in seconds). |
| audioOnlyPlayback | Boolean | false | Enable/disable audio-only playback. |
| enableProducerReferenceDelay | Boolean | false | Enable/disable adding the PRT (ProducerReferenceTime)-derived encoder delay to the live latency calculation. When enabled, AAMP reads the `ProducerReferenceTime` element from the DASH manifest to compute the encoding pipeline delay and accounts for it when measuring and correcting live latency. Disable if the stream's PRT data is absent or unreliable. |
| cdvrLiveOffset | Number | 30 | Live offset time in seconds for CDVR. AAMP starts live playback this much time before the live point for in-progress CDVR. |
| customHeader | String | - | Custom header data to append to HTTP requests. |
| contentProtectionDataUpdateTimeout | Number | 5000 | Timeout for Content Protection Data Update on Dynamic Key Rotation (milliseconds). Player waits for [setContentProtectionDataConfig](#setcontentprotectiondataconfig_json-string) API update within the timeout interval. On timeout, uses last configured values. Also refer API [setContentProtectionDataUpdateTimeout](#setcontentprotectiondataupdatetimeout_timeout). |
| disableLowLatencyABR | Boolean | true | Enable Low Latency ABR handling. |
| disablePlaylistIndexEvent | Boolean | true | Enable/disable generation of playlist indexed event by AAMP on tune/trickplay/seek. |
| downloadBufferChunks | Number | 20 | Low Latency fragment chunk cache length. |
| enableLowLatencyCorrection | Boolean | true | Enable latency correction. If disabled, latency may gradually drift from the live edge, especially under poor network conditions. |
| enableLowLatencyDash | Boolean | true | Enable Low Latency DASH playback mode. Allows media chunks to be injected earlier (even before full fragment download completes), allowing player to start and stay closer to live edge. |
| enableSubscribedTags | Boolean | true | Enable/disable subscribed tags. |
| enableVideoEndEvent | Boolean | true | Enable/disable Video End event generation. |
| enableVideoRectangle | Boolean | true | Enable/disable setting of rectangle property for sink element. |
| forceHttp | Boolean | false | Enable/disable forcing of HTTP protocol for HTTPS URLs. |
| fragmentRetryLimit | Number | -1 | Set fragment rampdown/retry limit for video fragment failure. |
| id3 | Boolean | false | Enable/disable ID3 tag processing. |
| iframeDefaultBitrate | Number | 0 | Default bitrate for iframe track selection for non-4K assets (0 = auto). |
| iframeDefaultBitrate4K | Number | 0 | Default bitrate for iframe track selection for 4K assets (0 = auto). |
| initRampdownLimit | Number | 0 | Maximum number of rampdown/retries for initial playlist retrieval at tune/seek time. |
| latencyMonitorDelayMs | Number | 5000 | Delay in milliseconds before starting latency monitoring after tune completion. |
| latencyMonitorIntervalMs | Number | 1000 | Time between latency checks in milliseconds. Changing the value will only affect monitoring and corrective actions (how frequently latency is sampled and rate corrections are attempted). |
| latencyDangerBufferSec | Float | 1.0 | Buffer level (seconds) below which latency thresholds are dynamically increased to accommodate the low-buffer condition. Once the buffer recovers above this level and remains healthy for `latencyStableDurationSec`, the thresholds are gradually restored toward their configured defaults. Zero disables the adaptive threshold feature entirely. |
| latencyStableDurationSec | Float | 300.0 | Duration (seconds) of consecutive healthy buffer (above `latencyDangerBufferSec`) required before one restoration step is applied to the latency thresholds. Zero disables dynamic restoration entirely. |
| licenseAnonymousRequest | Boolean | false | Enable/disable acquiring of license without token. |
| licenseKeyAcquireWaitTime | Number | 5000 | License key acquire wait time (milliseconds). |
| licenseRetryWaitTime | Number | 500 | License retry wait interval (milliseconds). |
| licenseServerUrl | String | - | URL to be used for license requests for encrypted(PR/WV) assets. |
| linearTrickPlayFps | Number | 8 | Specify the framerate for Linear trickplay. |
| lowLatencyMinValue | Number | 5 | Minimum acceptable latency (seconds). Avoids getting too close to live edge, preventing buffering. If latency drops below this, playback slows down to increase delay and avoid buffer underrun. |
| lowLatencyTargetValue | Number | 6 | Target latency for playback (seconds). If reduced, playback will be closer to live edge, but with increased chance of buffering. |
| lowLatencyMaxValue | Number | 7 | Maximum acceptable latency (seconds). Ensures playback does not fall too far behind live stream. If latency exceeds this, playback speeds up to catch up with live edge. |
| lowLatencyMinBuffer | Float | 2 | Minimum buffer level the player should maintain for low latency buffering (seconds). |
| lowLatencyTargetBuffer | Float | 4 | Target buffer size for low latency mode (seconds). Balances latency and stability by keeping a healthy buffer. |
| maxABRBufferRampup | Number | 15 | Maximum ABR Buffer for Rampup in secs. |
| maxLatencyCorrectionPlaybackRate | Float | 1.03 | Maximum playback speed for latency correction. When the player detects that it’s too far from the live edge (or fall behind target latency), it can speeds up playback slightly to catch up with the live edge without noticeable fast-forward effect. |
| minABRBufferRampdown | Number | 10 | Minimum ABR Buffer for Rampdown in secs. |
| minLatencyCorrectionPlaybackRate | Float | 0.97 | Minimum playback speed for latency correction. When the player detects that it’s too close to the live edge (or ahead of target latency), it can slow down playback slightly to increase latency without causing noticeable slow motion. |
| normalLatencyCorrectionPlaybackRate | Float | 1.0 | Normal playback speed when latency is within acceptable range. Maintains standard playback when no correction is needed. |
| rebufferLatencyMaxIncrementSec | Float | 8.0 | Maximum total accumulated increment (seconds) that can be added to latency thresholds across all rebuffering events. Caps the upward drift of `lowLatencyMinValue`, `lowLatencyTargetValue`, and `lowLatencyMaxValue`. Zero means no cap. |
| rebufferLatencyStepSec | Float | 1.0 | Step size (seconds) added to all three latency thresholds (`lowLatencyMinValue`, `lowLatencyTargetValue`, `lowLatencyMaxValue`) each time the buffer drops below `latencyDangerBufferSec`. Allows the player to tolerate higher latency during poor network conditions. Zero disables the adaptive threshold feature entirely. |
| playreadyOutputProtection | Boolean | false | Enable/disable HDCP output protection for DASH-PlayReady playback. |
| preferredDrm | Number | 2 | Preferred DRM for playback. Refer Preferred DRM table below for available values. 0 - No DRM, 1 - Widevine, 2 - PlayReady (Default), 3 - Consec, 4 - AdobeAccess, 5 - Vanilla AES, 6 - ClearKey |
| preFetchIframePlaylist | Boolean | false | Enable/disable prefetching of I-Frame playlist. |
| preplayBuffercount | Number | 1 | Count of segments to download until reaching play state. |
| ptsErrorThreshold | Number | 4 | Maximum number of back-to-back PTS errors before triggering a retune. |
| seekMidFragment | Boolean | false | Enable/disable mid-fragment seek. |
| segmentInjectFailThreshold | Number | 10 | Maximum number of consecutive segment injection failures allowed before the player triggers its segment error-handling logic. |
| sendUserAgentInLicense | Boolean | false | Enable/disable sending user agent in the DRM license request header. |
| stallTimeout | Number | 10000 | Stall detection timeout (milliseconds). |
| thresholdSizeABR | Number | 6000 | ABR threshold size (bytes). |
| uriParameter | String | - | URI parameter data to append to download URLs during HTTP requests. |
| waitTimeBeforeRetryHttp5xx | Number | 1000 | Wait time before retry for 5xx HTTP errors (milliseconds). |
| wifiCurlHeader | Boolean | false | Enable/disable WiFi custom HTTP header inclusion. |
| initialBitrate | Number | 2500000 | Initial bitrate for playback (bps). |
| initialBitrate4K | Number | 13000000 | Initial bitrate for 4K video playback (bps). |
| minBitrate | Number | - | Minimum profile clamping (bps). Default is lowest bitrate profile in the manifest. |
| maxBitrate | Number | - | Maximum profile clamping (bps). Default is highest bitrate profile in the manifest. |
| disable4K | Boolean | false | Disable 4K profile playback and restrict to non-4K video profiles. |
| limitResolution | Boolean | false | Enable setting maximum playback video profile resolution based on TV display resolution setting. Default disabled, player selects every profile irrespective of TV resolution. |
| persistBitrateOverSeek | Boolean | false | Enable player to persist video profile bitrate during Seek/Trickplay/Audio switching operation. By default player picks initialBitrate configured. |
| useAverageBandwidth | Boolean | false | Enable using AVERAGE-BANDWIDTH instead of BANDWIDTH in HLS stream variants for ABR switching. |
| Offset | Number | 0 | Play position offset to start playback (seconds). Same as seek() method to resume at a position. |
| liveOffset | Number | 15 | Override the default/stream-defined distance from live point for live stream playback (seconds). |
| liveOffset4K | Number | 15 | Override the default/stream-defined distance from live point for 4K live stream playback (seconds). |
| networkTimeout | Number | 10 | Network request timeout for fragment/playlist/manifest downloads (seconds). |
| manifestTimeout | Number | 10 | Manifest download timeout; overrides networkTimeout if both present. Applied to main manifest in HLS and DASH manifest download (seconds). |
| playlistTimeout | Number | 10 | HLS playlist download timeout; overrides networkTimeout if both present. Available starting with version 1.0 (seconds). |
| downloadStallTimeout | Number | - | Optional optimization - Allow fast-failure for class of curl-detectable mid-download stalls (seconds). |
| downloadStartTimeout | Number | - | Optional optimization - Allow fast-failure for class of curl-detectable stall at start of download (seconds). |
| persistHighNetworkBandwidth | Boolean | false | Enable persist high network bitrate from previous tune for profile selection in next tune (if attempted within 10 sec). This will override initialBitrate settings. |
| persistLowNetworkBandwidth | Boolean | true | Disable persisting low network bitrate from previous tune for profile selection in next tune (if attempted within 10 sec). This will override initialBitrate settings. |
| supportTLS | Number | 6 | TLS version to use. Default set to CURL_SSLVERSION_TLSv1_2 (value of 6, uses CURLOPT_SSLVERSION values). |
| sharedSSL | Boolean | true | Enable/disable sharing SSL context for all download sessions, across manifest, playlist and segments. |
| sslVerifyPeer | Boolean | true | Enable/disable SSL peer verification. Default enabled. |
| networkProxy | String | - | Network proxy to use for all downloads (Format: `<http/https>://<IP:PORT>`). Use licenseProxy for license request routing. |
| licenseProxy | String | - | Network proxy to use for license requests (Format same as network proxy). |
| initialBuffer | Number | 0 | Pre-tune buffering (seconds) before playback start. With default of 0, player starts playback with 1st segment downloaded. |
| downloadBuffer | Number | 4 | Max number of segments which can be cached ahead of play head (including initialization header segment). This applies to every track type (Video/Audio/Text). This is player buffer in addition to streamer playback buffer which varies with platform. |
| bulkTimedMetadata | Boolean | false | Send timed metadata using single JSON array string instead of individual events. Available starting with version 0.8. |
| bulkTimedMetadataLive | Boolean | false | Equivalent of bulkTimedMetadata delivered also for live streams. Available starting with version 6.12. |
| parallelPlaylistDownload | Boolean | true | Optional optimization – download audio and video playlists in parallel for HLS. Available starting with version 0.8. |
| parallelPlaylistRefresh | Boolean | true | Optionally disable audio/video playlist parallel download for linear (only for HLS). |
| preCachePlaylistTime | Number | - | Optionally enable pre-caching of playlist and time window for cache (minutes). Available in version 1.0. |
| maxPlaylistCacheSize | Number | 0 | Maximum cache size in KB to store different profile HLS VOD playlists. |
| maxInitFragCachePerTrack | Number | 5 | Number of initialization header files cached per player instance per track type. Use cached data instead of network re-download. |
| progressReportingInterval | Number | 1 | Optionally change progress report interval (seconds). |
| progress | Boolean | false | Enable progress logging. |
| progressLoggingDivisor | Number | 4 | If progress logging is enabled, this divides the progressReportingInterval to reduce the amount of logging. |
| useRetuneForUnpairedDiscontinuity | Boolean | true | Optional unpaired discontinuity retune config. Available in version 1.0. |
| useMatchingBaseUrl | Boolean | false | Use DASH main manifest hostname to select from multiple base URLs in DASH (when present). By default, will always choose first. Available in version 2.4. |
| initFragmentRetryCount | Number | 1 | Maximum number of retries for MP4 header fragment download failures. Available in version 2.4. |
| useRetuneForGstInternalError | Boolean | true | Optional GStreamer error retune config. Available in version 2.7. |
| reportVideoPTS | Boolean | false | Enable Video PTS reporting along with progressReport. Available in version 3.0. |
| propagateUriParameters | Boolean | true | Enable/disable propagating URI parameters from main manifest to segment downloads. |
| enableSeekableRange | Boolean | false | Enable reporting of seekable range for linear scrubbing. |
| livePauseBehavior | Number | 0 | Configure player live pause behavior on linear streams when live window touches eldest position. Options: 0 - Autoplay immediate; 1 - Live immediate; 2 - Autoplay defer; 3 - Live defer; Default - Autoplay immediate. Refer [Appendix](#live-pause-configuration). |
| asyncTune | Boolean | true | Enable asynchronous player API processing. Application/UI caller threads returned immediately without any processing delays. |
| useAbsoluteTimeline | Boolean | false | Enable progress reporting based on Availability Start Time of stream (DASH only). |
| tsbInterruptHandling | Boolean | false | Enable support for network interruption handling with TSB. Network failures will be ignored and TSB will continue building. |
| fragmentDownloadFailThreshold | Number | 10 | Maximum number of fragment download failures before reporting playback error. |
| useSecManager | Boolean | true | Enable/disable usage of SecManager for watermarking functionality (for Comcast streams only). |
| drmDecryptFailThreshold | Number | 10 | Maximum number of fragment decrypt failures before reporting playback error. Available in version 1.0. |
| customHeaderLicense | String | - | Custom header data to add in license request during tune. This can be set with addCustomHTTPHeader API during playback for license rotation if required. |
| setLicenseCaching | Boolean | true | Enable/disable license caching in player. By default 3 DRM sessions are cached. |
| authToken | String | - | AuthService token for license acquisition. Available in version 2.7. |
| configRuntimeDRM | Boolean | false | Enable DRM notification and get application configuration for every license request. Also refer [API](#configruntimedrm_enableconfiguration). |
| preferredAudioLanguage | String | en | ISO-639 audio language preference; for more than one language, provide comma delimited list from highest to lowest priority: ‘<HIGHEST>,<...>,<LOWEST>’.Preferred language can be set using APIs [setAudioTrack](#setaudiotrack_index)/[setAudioLanguage](#setaudiolanguage_language)/[setPreferredAudioLanguage](#setpreferredaudiolanguage_languages_rendition_accessibility_codeclist_label) |
| stereoOnly | Boolean | false | Force playback to only select stereo audio track. Available starting with version 0.8. |
| disableEC3 | Boolean | false | Disable selection of EC3/AC3 audio track. |
| disableATMOS | Boolean | false | Disable selection of ATMOS audio track. |
| disableAC4 | Boolean | false | Disable selection of AC4 audio track. |
| preferredAudioRendition | String | - | Preferred audio rendition setting. DASH: caption, subtitle, main; HLS: GROUP-ID. |
| preferredAudioCodec | String | - | Preferred audio codec. Comma-delimited list of formats, where each format specifies a media sample type present in one or more renditions. Example: mp4a.40.2, avc1.4d401e |
| preferredAudioLabel | String | - | Label of desired audio track in the available audio tracks list. Same can be done with setAudioTrack API also. |
| preferredAudioType | String | - | Preferred accessibility type for descriptive audio in the available audio tracks list. Same can be done with setAudioTrack API also. |
| langCodePreference | Number | 0 | Set the preferred format for language codes in other events/APIs. Available in version 2.6. Values: 0 - NO_LANGCODE_PREFERENCE, 1 - 3_CHAR_BIBLIOGRAPHIC_LANGCODE, 2 - 3_CHAR_TERMINOLOGY_LANGCODE, 3 - 2_CHAR_LANGCODE |
| preferredSubtitleLanguage | String | en | ISO-639 language code used with VTT OOB captions. |
| preferredTextLabel | String | - | Label of desired text track in the available text tracks list. |
| nativeCCRendering | Boolean | false | Use native closed caption support in AAMP. Available in version 2.6. |
| enableLiveLatencyCorrection | Boolean | false | Enable correction of playback delay during regular live streaming (non-LLD). Keeps the video close to real-time by adjusting playback speed if it drifts behind. |
| liveOffsetDriftCorrectionInterval | Number | 1 | The allowed delta from live offset configured (seconds). |
| sendLicenseResponseHeaders | Boolean | false | Enable headers in DRM metadata event after license request. |
| enableCMCD | Boolean | true | Enable/disable CMCD (Common Media Client Data) metrics reporting from player. |
| userAgent | String | - | The User-Agent request header for HTTP requests. |
| drmNetworkTimeout | Number | 5 | Network request timeout for DRM license (seconds). |
| drmStallTimeout | Number | 0 | Optional optimization - Allow fast-failure for class of curl-detectable mid-download stalls for DRM license request (seconds). |
| drmStartTimeout | Number | 0 | Optional optimization - Allow fast-failure for class of curl-detectable stall at start of DRM license request download (seconds). |
| connectTimeout | Number | 3 | cURL socket connection timeout for fragment/playlist/manifest downloads (seconds). |
| dnsCacheTimeout | Number | 180 | Lifetime for DNS cache entries. Name resolve results are cached for manifest and used for this number of seconds. |
| tsbType | String | - | Use the "tsbType" configuration for each playback session, where "local" enables local time shift buffer (FOG or AAMP TSB), "cloud" enables direct CDN streaming, and if "tsbType" is not provided, default to "none," means play as-is. For detailed behavior, see [TSB Feature](#tsb-feature). |
| telemetryInterval | Number | 300 | Telemetry log interval (seconds). Default of 300 seconds. Set to 0 to disable telemetry logging. |
| sendUserAgentInLicense | Boolean | false | Optional field to enable sending User Agent string in license request also |
| useSinglePipeline | Boolean | false | Enable single pipeline while switching between multiple player instances (Ad & Content) to avoid delay in flush operations. Used primarily for Client Side Ad-Insertion with multi-player usage. |
| mpdStichingSupport | Boolean | true | Enable/disable DASH MPD stitching functionality with dual manifest (one manifest used during tune and another manifest during refresh). |
| enablePTSReStamp | Boolean | false | Enable/disable PTS re-stamping functionality across discontinuity while moving from content to ads or vice-versa. Currently only applicable to DASH content. |
| subtitleClockSyncInterval | Number | 30 | Time interval for synchronizing the clock with subtitle module (seconds). Default of 30 seconds. |
| showDiagnosticsOverlay | Number | 0 | Configures the diagnostics overlay: 0 (None), 1 (Minimal), 2 (Extended). Controls the visibility and level of detail for diagnostics displayed during playback. Refer [Diagnostics Overlay Configuration](#diagnostics-overlay-configuration). |
| localTSBEnabled | Boolean | false | Enable use of time shift buffer (TSB) for live playback, leveraging local storage in AAMP. This is a development-only configuration, not to be used by apps. |
| tsbLength | Number | 3600 or 1500 | Max duration (seconds) of local TSB to build up before culling. 3600 (1 hour) or 1500 (25 min). Not recommended for apps to change. Refer to [TSB Feature](#tsb-feature) for complete details. |
| monitorAV | Boolean | false | Enable background monitoring of audio/video positions to infer video freeze, audio drop, and AV sync issues. |
| monitorAVReportingInterval | Number | 1000 | Sampling delay between reported MonitorAV events (milliseconds). |
| monitorAVSyncThresholdPositive | Number | 100 | Threshold for leading (early) audio to be considered worth reporting (milliseconds). |
| monitorAVSyncThresholdNegative | Number | 100 | Threshold for lagging (late) audio to be considered worth reporting (milliseconds). |
| monitorAVJumpThreshold | Number | 100 | Threshold for aligned audio/video positions advancing together to be considered worth reporting (milliseconds). |

**Example:**

```javascript
// Configuration settings for player
var playerInitConfig = {
    initialBitrate: 2500000,    // Start with 2.5 Mbps
    offset: 0,                   // Start from beginning
    networkTimeout: 10,          // 10 second timeout
    preferredAudioLanguage: "en" // English audio preference
};

// Replace with your actual stream URL
var url = "https://example.com/multilang/sample.m3u8";

// Create player instance and initialize with config
var player = new AAMPMediaPlayer();
player.initConfig(playerInitConfig);
player.load(url);
```
---

### setDRMConfig( config )

DRM configuration options are passed to AAMP using the `setDRMConfig()` method. The parameter is a JSON object with pairs of protectionScheme: licenseServerUrl, along with preferredKeySystem specifying a preferred protection scheme.

| Property | Type | Description |
| -------- | ---- | ----------- |
| com.microsoft.playready | String | License server endpoint to use with PlayReady DRM. Example: `http://test.playready.microsoft.com/service/rightsmanager.asmx` |
| com.widevine.alpha | String | License server endpoint to use with Widevine DRM. Example: `https://widevine-proxy.appspot.com/proxy` |
| org.w3.clearkey | String | License server endpoint to use with Clearkey DRM. |
| preferredKeysystem | String | Used to disambiguate which DRM type to use when manifest advertises multiple supported DRM systems. Example: `com.widevine.alpha` |
| customLicenseData | String | Optional custom data for license request. |

**Example:**

```javascript
// Configuration for DRM - Sample for Widevine
var drmConfig = {
    'com.widevine.alpha': 'https://example.com/AcquireLicense', // Replace with valid URL
    'preferredKeysystem': 'com.widevine.alpha'
};

// Replace with your actual stream URL
var url = "https://example.com/multilang/sample.m3u8";

// Create player instance and configure DRM
var player = new AAMPMediaPlayer();
player.setDRMConfig(drmConfig);
player.load(url);
```
---

## Properties

| Name | Type | Description |
| ---- | ---- | ----------- |
| version | Number | May be used to confirm if RDKV build in use supports a newer feature. |
| AAMP.version | Number | Global variable for applications to get UVE API version without creating a player instance. Value will be same as `player.version`. |

---

## Methods

### load( uri, autoplay, tuneParams )

Begin streaming the specified content.

**Parameters:**

| Name | Type | Description |
| ---- | ---- | ----------- |
| uri | String | URI of the media to be played by the Video Engine. |
| autoplay | Boolean | Optional 2nd parameter (defaults to true). If false, causes stream to be prerolled/prebuffered only, but not automatically presented. Available starting with version 0.8. |
| tuneParams | Object | Optional 3rd parameter. The tuneParams object includes six elements: contentType, traceId, isInitialAttempt, isFinalAttempt, sessionId and manifest. Details provided in table below. |

| Name | Type | Description |
| ---- | ---- | ---------- |
| contentType | String | Content Type of the asset taken for playback. Eg: CDVR, VOD, LINEAR_TV, IVOD, EAS, PPV, OTT, OTA, HDMI_IN, COMPOSITE_IN, SLE. Refer below table for contentTypes |
| traceID | String | Trace ID which is unique for a tune |
| isInitialAttempt | Boolean | Flag indicates if it’s the first tune initiated, tune is neither a retry nor a rollback |
| isFinalAttempt | Boolean | Flag indicates if the current tune is the final retry attempt, count has reached the maximum tune retry limit |
| sessionId | String | ID of the Session set by the Video Engine to identify each player. All events emitted by a player will contain a property reporting the player's ID; if the sessionId is not set, then the sessionId will be an empty string. |
| manifest | String | prefetched/preprocessed manifest (plaintext xml) to use instead of the manifest normally downloaded using <uri>. If provided, updated live dash manifest is expected for each manifest refresh interval (refer needManifest event). This is available only for DASH

**Content Types:**

| ContentType | Description |
| ----------- | ----------- |
| CDVR | Cloud Digital Video Recording |
| VOD | Static Video on Demand |
| LINEAR_TV | Live Content |
| IVOD | Video on Demand for Events |
| EAS | Emergency Alert System |
| PPV | Pay Per View |
| OTT | Over the Top |
| OTA | Over the Air content |
| HDMI_IN | Presenting an HDMI input |
| COMPOSITE_IN | Presenting composite input |
| SLE | Single Live Event (similar to IVOD) |

**Examples:**

```javascript
// Example 1: Simple autoplay
var player = new AAMPMediaPlayer();
var url = "https://example.com/multilang/sample.m3u8"; // Replace with valid URL
player.load(url); // Starts playing immediately
```

```javascript
// Example 2: Multiple player instances
var player1 = new AAMPMediaPlayer();
var player2 = new AAMPMediaPlayer();

var url1 = "https://example.com/multilang/sample.m3u8";  // Replace with valid URL
var url2 = "https://example.com/multilang/sample1.m3u8"; // Replace with valid URL

player1.load(url1);        // For immediate playback
player2.load(url2, false); // For background buffering, no playback
```

```javascript
// Example 3: Multiple player instances with session IDs
var player1 = new AAMPMediaPlayer();
var player2 = new AAMPMediaPlayer();

var url1 = "https://example.com/multilang/sample.m3u8";  // Replace with valid URL
var url2 = "https://example.com/multilang/sample1.m3u8"; // Replace with valid URL

var params_1 = { sessionId: "12192978-da71-4da7-8335-76fbd9ae2ae9" }; // base16
var params_2 = { sessionId: "6e3c49cb-6254-4324-9f5e-bddef465bdff" }; // base16

player1.load(url1, true, params_1);  // For immediate playback
player2.load(url2, false, params_2); // For background buffering, no playback
```

```javascript
// Example 4: Preprocessed DASH manifest
var player = new AAMPMediaPlayer();
var url = "https://example.com/VideoTestStream/aamptest/streams/ads/stitched/sample_manifest.mpd";  // Replace with valid URL

// Replace below with valid full DASH manifest XML
const xml = "<?xml version=\"1.0\" encoding=\"utf-8\"?>\n<MPD xmlns:xsi=\"http://www.w3.org/2001/XMLSchema-instance\"" /* ... */;

player.load(url, true, { manifest: xml });
```

---

### play()

Start playback (if stream is in prebuffered state), or resume playback at normal speed. Equivalent to `setPlaybackRate(1)`.

- **Supported:** UVE version 0.7 and above.

**Example:**

```javascript
var player = new AAMPMediaPlayer();
var url = "https://example.com/multilang/main.m3u8"; // Replace with valid URL

// Load stream in background (prebuffered state)
player.load(url, false);

// Application can start the playback later using play API
player.play();  // Or player.setPlaybackRate(1);
```
---

### pause( position )

Pauses playback. Equivalent to `setPlaybackRate(0)`.

- **Supported:** UVE version 0.7 and above.

**Parameters:**

| Name | Type | Description |
| ---- | ---- | ----------- |
| position | Number | Optional input. Position value where player needs to pause during play or trickplay (value in seconds). To cancel a scheduled pause, call pause API with input -1. |

**Examples:**

```javascript
// Example 1: Immediate pause of playback
player.pause();  // Or player.setPlaybackRate(0);
```

```javascript
// Example 2: Schedule a pause at later play position
player.pause(60);  // Schedules a pause at 60 sec of play
```

```javascript
// Example 3: Cancel a scheduled pause
player.pause(-1);
```

**Note:** Starting in RDK 6.9, we support ability to start video paused on first frame.

**Example:**

```javascript
// Start playback backgrounded with autoplay=false
player.load("https://example.com/public/aamptest/streams/main.mpd", false); // Replace with valid URL

player.seek(30); // Optionally jump to new position

player.pause();  // Bring video to foreground and show first frame of video
```

---

### stop( forceCleanup )
### stop( sendStateChangeEvent, forceCleanup )

Stop playback and free resources associated with playback.

- **Supported:** UVE version 0.7 and above.

**Single Parameter Form:**

| Name | Type | Description |
| ---- | ---- | ----------- |
| forceCleanup | Boolean | Optional parameter. If true, forces DRM handle cleanup for Deep Sleep scenarios. Default is false. Prevents playback failures after device wake-up from Deep Sleep by clearing stale DRM sessions and failed key IDs. |

**Two Parameter Form (Advanced Usage):**

| Name | Type | Description |
| ---- | ---- | ----------- |
| sendStateChangeEvent | Boolean | If true, sends state change events during stop operation. Default is true. |
| forceCleanup | Boolean | If true, forces DRM handle cleanup for Deep Sleep scenarios. Default is false. |

**Usage Examples:**

```javascript
// Standard stop - sends state change events
player.stop();

// Stop with DRM cleanup before Deep Sleep
player.stop(true);

// Advanced: Stop without state events, with DRM cleanup
player.stop(false, true);
```
---

### detach()

Optional API that can be used to quickly stop playback of active stream before transitioning to next prebuffered stream.

- **Supported:** UVE version 0.9 and above.

**Example:**

```javascript
var player = new AAMPMediaPlayer();

// Begin streaming main content
player.load("http://test.com/content.m3u8");

// Create background player
var adPlayer = new AAMPMediaPlayer();

// Preroll with autoplay = false
adPlayer.load("http://test.com/ad.m3u8", false);

// Stop playback of active player
player.detach();

// Activate background player (fast transition)
adPlayer.play();

// Release remaining resources for initial player instance
player.stop();
```
---

### resetConfiguration()

Reset the player instance configuration to default values. Can be called by the application at any time.

---

### getConfiguration()

Retrieve current player configuration.

**Returns:** Object containing current player configuration.

---

### release()

Immediately release any native memory associated with player instance. The player instance must no longer be used following `release()` call.

**Warning:** This is a terminal operation. Do not use the player instance after calling this method.

**Note:** If this API is not explicitly called, the garbage collector will eventually take care of this memory release.

---

### seek( offset )
- Supported with UVE version 0.7.
- Specify new playback position to start playback. <br/> This can be called prior to load() call (or implicitly using initConfig’s “offset” parameter), <br/> Or during playback to seek to a new position and continue play.


| Name | Type | Description |
| ---- | ---- | ---------- |
| offset | Number | Offset from beginning of VOD asset. For live playback, offset is relative to eldest portion of initial window, or can pass -1 to seek to logical live edge. Offset value should be in seconds. <br/>**Note:** that ability to seek is by default limited to fragment granularity, though this can be enabled with config "seekMidFragment=true" |
| keepPause | Boolean | Optional input . Default value is false, playback starts automatically after seek.<br/> If True, player will maintain paused state after seek execution if the state was in paused before seek call.  <br/>Available with version 2.6 |

Example:
```js
    {
	    .....
	    // seek to new position and start auto playback
	    player.seek(60);
	    ...
	    // Pause the playback
	    player.pause();
	    // Seek to new position and remain in last player state
	    player.seek(60,true);
    }
```
---

### getCurrentPosition()

Returns current playback position in seconds.

- **Supported:** UVE version 0.7 and above.

**Returns:** Number - Current playback position (seconds).

---

### getCurrentState()

Returns one of the logical player states as a number.

- **Supported:** UVE version 0.7 and above.

**Player States:**

| State Name | Value | Semantics | Remarks |
| ---- | ---- | ---------- | ------- |
| idle | 0 | eSTATE_IDLE | Player is idle |
| initializing | 1 | eSTATE_INITIALIZING | Player is initializing resources to start playback |
| initialized | 2 | eSTATE_INITIALIZED | Player completed playlist download and metadata processing |
| preparing | 3 | eSTATE_PREPARING | Create internal resources required for DRM decryption and playback |
| prepared | 4 | eSTATE_PREPARED | Required resources are initialized successfully |
| buffering | 5 | eSTATE_BUFFERING | When player does internal buffering mid-playback. Note -send out in initial buffering |
| paused | 6 | eSTATE_PAUSED | Indicates player is paused |
| seeking | 7 | eSTATE_SEEKING | Indicates player is seeking |
| playing | 8 | eSTATE_PLAYING | Indicates player is in playing state  |
| stopping | 9 | eSTATE_STOPPING | Deprecated  |
| stopped | 10 | eSTATE_STOPPED | Not supported for all stream types. To be deprecated |
| complete | 11 | eSTATE_COMPLETE | Indicates the end of media |
| error | 12 | eSTATE_ERROR | Indicates error in playback |
| released | 13 | eSTATE_RELEASED | To be deprecated |

---

### getDurationSec()

Returns current duration of content in seconds.

- **Supported:** UVE version 0.7 and above.
- Duration is fixed for VOD content, but may grow with linear content.

**Returns:** Number - Duration in seconds.

---

### getVolume()

Get current volume (value between 0 and 100).

- **Supported:** UVE version 0.7 and above.
- Default audio volume is 100.
- Volume is normally mapped from remote directly to TV, with video engine used to manage an independent mute/unmute state for parental control.

**Returns:** Number - Current volume (0-100).

---

### setVolume( volume )

Sets the current volume (value between 0 and 100). Updated value reflected in subsequent calls to `getVolume()`.

- **Supported:** UVE version 0.7 and above.

**Parameters:**

| Name | Type | Description |
| ---- | ---- | ----------- |
| volume | Number | Pass 0 to mute audio. Pass 100 for normal (max) audio volume. |

**Returns:** Boolean - true if setVolume has been performed.

---

### setVideoMute( state )

Black out video for parental control purposes or enable the video playback.

- **Supported:** UVE version 0.7 and above.

**Parameters:**

| Name | Type | Description |
| ---- | ---- | ----------- |
| state | Boolean | true to mute video. false to disable video mute and enable video playback. |

**Returns:** Boolean - true if setVideoMute has been performed.

---

### getPlaybackRate()

Returns the current playback rate.

- **Supported:** UVE version 0.7 and above.
- Refer table in `setPlaybackRate()` for rate values.

**Returns:** Number - Current playback rate.

---

### setPlaybackRate( rate )

Change playback rate. Supported speeds are given below.

- **Supported:** UVE version 0.7 and above.

**Parameters:**

| Name | Type | Description |
| ---- | ---- | ----------- |
| rate | Number | Playback rate (see table below). |

**Returns:** Boolean - true if setPlaybackRate has been performed.

**Supported Playback Rates:**

| Value | Description |
| ----- | ----------- |
| 0 | Pause |
| 1 | Normal Play |
| 4 | 2x Fast Forward (using iframe track) |
| 16 | 4x Fast Forward (using iframe track) |
| 32 | 8x Fast Forward (using iframe track) |
| 64 | 16x Fast Forward (using iframe track) |
| -4 | 2x Rewind (using iframe track) |
| -16 | 4x Rewind (using iframe track) |
| -32 | 8x Rewind (using iframe track) |
| -64 | 16x Rewind (using iframe track) |

---

### getManifest()
- Return currently presenting manifest as plain text - supported only for DASH/VOD.

---
### getVideoBitrates()
- Supported UVE version 0.7 and above.
- Return array of available video bitrates across profiles.

---
### getCurrentVideoBitrate()
- Supported UVE version 0.7 and above.
- Return current video playback bitrate, as bits per second.

---

### setVideoBitrate( bitrate )
- Supported UVE version 0.7 and above.
- Note : This will disable ABR functionality and lock the player to a single profile for the bitrate passed. To enable the ABR, call the API with 0.
- Returns true if setVideoBitrate has been performed.

|Name|Type|Description|
|----|----|-----------|
|bitrate|Number|To disable ABR and lock playback to single profile bitrate. |

---

### getAudioBitrates()
- Return array of all audio bitrates advertised in manifest.  Note that this is read-only informative list.  We do not yet expose method to force selection of specific audio tracks that differ only by bitrate.

---

### getCurrentAudioBitrate()
- Supported UVE version 0.7 and above.
- Return current audio bitrate, as bits per second.

---

### setVideoRect( x, y, w, h )
- Supported UVE version 0.7 and above.
- Set display video rectangle coordinates. Note that by default video will be full screen.
- Rectangle specified in “graphics resolution” coordinates (coordinate space used by graphics overlay).
- Window size is typically 1280x720, but can be queried at runtime as follows:
    - using getVideoRectangle API
    - Alternate method
        - var w  = window.innerWidth || document.documentElement.clientWidth ||document.body.clientWidth;
        - var h = window.innerHeight|| document.documentElement.clientHeight|| document.body.clientHeight;

|Name|Type|Description|
|----|----|-----------|
| X | Number | Left position for video |
| Y | Number | Top position for video |
| W | Number | Video Width |
| H | Number | Video Height |

#### Minimal UVE Player with Video Scaling
- This is a simple example of how to scale or reposition video using AAMP.
- Video is rendered in the background video plane and UI is rendered in the graphics plane.
- The app will only hole punch through what is configured via the drawVideoRectHelper(x, y, w, h ) function, and the video will be scaled to fit the specified rectangle. The UI will be visible on the rest of the screen. This will ensure that the video is not hidden behind the UI and the UI is not hidden by video hole punching.

```js
<html>
	<head>
		<title>MINIMAL UVE PLAYER - SCALED VIDEO</title>
	</head>

	<script>
		var player;
		window.onload = function() {
			player = new AAMPMediaPlayer(); // create player instance for AAMP
			let url = "http://example.com/12345678-1234-1234-1234-123456789012/SampleVideo.ism/manifest(format=mpd-time-csf)"; // replace with valid URL
			console.log("loading " + url );
			player.load( url ); // tune using AMP
			console.log("screen size: " + screen.width + "x" + screen.height); // typically 1280x720
			let w = screen.width/2; // 50% width
			let h = screen.height/2; // 50% height
			let x = (screen.width-w)/2; // center horizontally
			let y = (screen.height-h)/2; // center vertically
			drawVideoRectHelper(x, y, w, h ); // place video using graphics plane coordinates
		}
​
		// helper function to set video position
		function drawVideoRectHelper(x, y, w, h) {
			let video = document.getElementById("video");
			video.style.left = x + "px";
			video.style.top = y + "px";
			video.style.width = w + "px";
			video.style.height = h + "px";
			player.setVideoRect(x, y, w, h ); // place video using graphics plane coordinates
		}
	</script>
	<style>
		#backDrop {
			position: absolute;
			width: 100%;
			height: 100%;
			left: 0;
			top: 0;
			background-color: rgb(9, 62, 148);
		}

		#video {
			position: absolute;
		}
	</style>
	<body>
		<div id="backDrop"></div>
		<video id="video">
			<source src="dummy.mp4" type="video/ave"> <!-- hole punching -->
		</video>
	</body>
</html>
```

---

### getVideoRectangle()
- Supported UVE version 1.0 and above.
- Returns the string of current video rectangle co-ordinates (x,y,w,h).

---

### setVideoZoom( videoZoom )
- Supported UVE version 0.7 and above.
- Set video zoom, by default its set to “full” (5)
- Returns true if setVideoZoom has been performed.

|Name       |Type    |Value    |Description|
|-----------|--------|---------|-----------|
| videoZoom | String | none    | No zoom is applied; the video is displayed in its original size |
| videoZoom | String | direct  | Used for a straightforward zoom without any aspect ratio adjustments |
| videoZoom | String | normal  | Standard zoom mode that maintains the aspect ratio while zooming in on the video |
| videoZoom | String | stretch | Stretches the video to fit a 16:9 aspect ratio |
| videoZoom | String | pillar  | Displays a 4:3 video with black bars |
| videoZoom | String | full    | Zooms the video to fill the entire screen, results crop parts of the video or distort the aspect ratio |
| videoZoom | String | global  | Applies a global zoom setting that affects all videos uniformly |

---

### addCustomHTTPHeader( headerName, headerValue, requestType )
- Supported UVE version 0.8 and above.
- Add custom headers to HTTP requests

|Name|Type|Description|
|----|----|-----------|
| headerName | String | HTTP header name |
| headerValue | String | HTTP header value |
| headerValue | String Array | Alternate parameter type for multi-value HTTP headers |
| requestType | Boolean | Optional field for scope.  If False (default), the custom HTTP header is applied only to subsequent CDN requests; If True, the custom HTTP header is applied only to subsequent license requests (i.e. Widevine/PlayReady) |

Example:
```js
    {
            // configuration for DRM -Sample for Widevine
            var DrmConfig = {
            'com.widevine.alpha':'https://example.com/AcquireLicense', // replace with valid URL
            'preferredKeysystem':'com.widevine.alpha'
            };


	    var url = "https://example.com/multilang/sample.m3u8"; // replace with valid URL
	    var player = new AAMPMediaPlayer();
	    player.setDRMConfig(DrmConfig);
	    // custom header message for license request
	    player.addCustomHTTPHeader(
	    "X-AxDRM-Message", "eyJhbGciOiJIUzI1NiIsInR5cCI6IkpXVCJ9",
	    true);
	    player.load(url);
    }

```
---

### removeCustomHTTPHeader( headerName, requestType )
- Supported UVE version 0.8 and above.
- Remove a previously set custom header.  If called with no arguments, will completely reset custom header use for the player instance, removing all custom headers for both license and CDN requests.

|Name|Type|Description|
|----|----|-----------|
| headerName | String | Optional field: HTTP header name. If not set then all headers will be removed |
| requestType | Boolean | Optional field for scope.  If False (default), applies only to CDN requests; If True, applies only to License server requests (i.e. Widevine/PlayReady).

Examples:
```
	// remove all custom headers (from CDN and license requests)
	player.removeCustomHTTPHeader();

	// remove custom header "foo" from  CDN requests (not license requests)
	player.removeCustomHTTPHeader( "foo" );

	// remove custom header "foo" from  CDN requests only
	player.removeCustomHTTPHeader( "foo", false );

	// remove custom header "foo" from  license requests only
	player.removeCustomHTTPHeader( "foo", true );

	// remove all custom headers from CDN requests only
	player.removeCustomHTTPHeader( false );

	// remove all custom headers from license requests only
	player.removeCustomHTTPHeader( true );
```

---

---

### getAvailableVideoTracks ()
- Supported UVE version 4.4 and above.
- Returns the video profile information from the manifest.

```
 [{
                "bandwidth":    5000000,
                "width":     	1920,
                "height":       1080,
                "framerate":    25,
                "codec":	"avc1.4d4028",
                "enabled":	1
        }, {
                "bandwidth":    2800000,
                "width":     	1280,
                "height":       720,
                "framerate":    25,
                "codec":	"avc1.4d401f",
                "enabled":	1
        }, {
               "bandwidth":    1400000,
                "width":     	842,
                "height":       474,
                "framerate":    25,
                "codec":       "avc1.4d401e",
                "enabled":  1
} ]
```
---

### setVideoTracks ()
- Supported UVE version 4.4 and above.
- This API will set the Video track(s) to select for playback.
- Application to provide a list of profile bitrate to be included for playback
- This will override the TV resolution based profile selection and MinBitRate/MaxBitRate based profiles
- Returns true if setVideoTracks has been performed.

---

### getAvailableAudioTracks(allTracks)
- Supported UVE version 1.0 and above.
- Returns the available audio tracks information in the content.

|Name|Type|Description|
|----|----|-----------|
| allTracks | Boolean | If False,returns the available audio tracks information in the current playing content/Period(Default false) <br/> If True,returns all the available audio tracks information from the Manifest|

- ##### DASH

| Name  | Type | Description |
| ---- | ---- | ---- |
| name  | String | Human readable language name e.g: Spanish,English. |
| label  | String | Represents the label of the audio track. |
| language  | String | Specifies dominant language of the audio e.g:  spa,eng |
| codec  | String | codec associated with Audio. e.g: mp4a.40.2 |
| rendition  | String | Role for DASH, If not present, the role is assumed to be main e.g: caption,subtitle,main. |
| accessibilityType  | String | Accessibility value for descriptive, visually impaired signaling e.g: description, captions |
| bandwidth  | String | Represents variants of the bitrates available for the media type; e.g: 288000 |
| Type  | String | audio — Primary dialogue and soundtrack;
|||audio_native — Primary dialogue and soundtrack with dialogue that was recorded along with the video;
|||audio_descriptions — Audio track meant to assist the vision impaired in the enjoyment of the video asset |
| Channels | String | Indicates the maximum number of audio channels; 1 = mono, 2=stereo, up to 8 for DD+ |
| availability  | Boolean | Availability of the audio track in current TSB buffer (FOG or AAMP TSB) |
| accessibility  | Object | DASH shall signal a new object accessibility to notify a track as hearing impaired |
| scheme  | String | The SchemeId to indicate the type of Accessibility Example:- "urn:mpeg:dash:role:2011" |
| string_value  | String | The string value of Accessibility object; Example:-  "description" |

- ###### Example:
```sh
[{
    "name":	"root_audio111",
    "label":    "screen1_audio111",
    "language":	"en",
    "codec":	"ec-3",
    "rendition":	"alternate",
    "accessibilityType":	"description",
    "bandwidth":	117600,
    "Type":	"audio_description",
    "availability":	true,
    "accessibility":	{
        "scheme":	"urn:mpeg:dash:role:2011",
        "string_value":	"description"
      }
}]
```
-   ###### Reference

```html
<AdaptationSet id="4" contentType="audio" mimeType="audio/mp4" lang="en">
    <AudioChannelConfiguration schemeIdUri="tag:dolby.com,2014:dash:audio_channel_configuration:2011" value="a000"/>
    <Accessibility schemeIdUri="urn:mpeg:dash:role:2011" value="description"/>
    <Role schemeIdUri="urn:mpeg:dash:role:2011" value="alternate"/>
    <SegmentTemplate initialization="Example_HD_SU_Example_1234_0_1234567890123456789-eac3/track-sap-repid-$RepresentationID$-tc-0-header.mp4" media="Example_HD_SU_Example_1234_0_1234567890123456789-eac3/track-sap-repid-$RepresentationID$-tc-0-frag-$Number$.mp4" timescale="90000" startNumber="123456789" presentationTimeOffset="987654">
    <SegmentTimeline>
        <S t="7394777152" d="172800" r="14"/>
    </SegmentTimeline>
    </SegmentTemplate>
    <Representation id="root_audio111" bandwidth="117600" codecs="ec-3" audioSamplingRate="48000"/>
    <Label>screen1_audio111</Label>
</AdaptationSet>
```

- ##### HLS
    - Returns the available audio tracks information in JSON formatted list. Subset of parameters returned

| Name  | Type | Description |
| ---- | ---- | ---- |
| name  | String | The value is a quoted-string containing a human-readable description of the Rendition; e.g:english, commentary, german |
| language  | String | Identifies the primary language used in the Rendition.
|||In practice, this should be present in vast majority of production manifests, but per HLS specification,his attribute is OPTIONAL e.g: eng,ger,spa. |
| codec  | String | codec associated with Audio. e.g: mp4a.40.2 |
| rendition  | String | Specifies the group to which the Rendition belongs. GROUP-ID for HLS.|
| bandwidth  | String | Decimal-Integer encoding - bits per second. Represents peak segment bit rate of the Variant Stream. |
| Channels | String | Indicates maximum number of audio channels present in any Media Segment in the Rendition. e.g: An AC-3 5.1 rendition would have a CHANNELS=6 |
| characteristics | String | One or more comma-delimited Uniform Type Identifiers [UTI].  This attribute is OPTIONAL. |

- ###### Example:

```sh
[{
    "name": "6",
    "language":     "eng",
    "codec":        "mp4a.40.2",
    "rendition":    "english",
    "bandwidth":    288000
}]
```

- ###### Reference

```h
 #EXT-X-MEDIA:TYPE=AUDIO,GROUP-ID="mono",NAME="english",LANGUAGE="eng",URI="hls/en.m3u8",DEFAULT=YES,AUTOSELECT=YES
 #EXT-X-STREAM-INF:PROGRAM-ID=1,AUDIO="mono",BANDWIDTH=800000,RESOLUTION=640x360,CODECS="avc1.4d400d,mp4a.40.2"
hls/360p.m3u8
```

---

### getAudioTrack( )
- Supported UVE version 2.6 and above.
- Returns the **index** of the current audio track in the available audio tracks list.

---

### setAudioTrack(index )
- Supported UVE version 2.6 and above.
- Set the audio track language from the available audio tracks list.
- Behavior is similar to setPreferredAudioLanguage

|Name|Type|Description|
|----|----|-----------|
| index | Number | Track index of desired audio track in available audio tracks list |

---

### getAudioTrackInfo()
- Supported UVE version 4.2 and above.
- Returns the current playing audio track information in JSON format.
- ###### Example:
```sh
[{
    "name":	"root_audio111",
    "language":	"en",
    "codec":	"ec-3",
    "rendition":	"alternate",
    "accessibilityType":	"description",
    "bandwidth":	117600,
    "Type":	"audio_description",
    "availability":	true,
    "accessibility":	{
        "scheme":	"urn:mpeg:dash:role:2011",
        "string_value":	"description"
      }
}]
```
---

### getPreferredAudioProperties ()
- Supported UVE version 4.2 and above.
- Returns the list of preferred language, codecs,  rendition and audio type configured by application, in JSON format.

---

### setAudioTrack( trackDescriptorObj )
- Supported UVE version 3.2 and above.
- Set the audio track  by language, rendition, label and codec from the available audio tracklist.
- “language” match always takes precedence over “rendition” match.
- While playing passively to new periods with different track order/availability, or when tuning to new locator, heuristic for track selection is automatically re-applied.
- Best codec (AC4>ATMOS > DD+ >AC3 > AAC> Stereo) is always selected, subject to filtering configuration.
- Behavior is similar to setPreferredAudioLanguage

| Name  | Type | Description |
| ---- | ---- | ---- |
| language | String | Language of desired audio track in the available audio tracklist |
| rendition | String | Rendition of desired audio track in the available audio tracklist |
| label	| String	| Label or groupLabel elements of the audio track |
| type	| String	| Optional preferred accessibility type for descriptive audio |
||| Example:- label set "Native" to indicate the track as the original language track. |
|codec|	String|	Preferred codec of the audio track.|
|||Default Priority : (AC4 > ATMOS > D D+ > AC3 > AAC > Others)|

- ###### Example:
```js
var trackDescriptorObject =
{
    "language": "de",
    "rendition": "commentary",
    "type" : "description",
    "codec": "avc",
    "label": "surround"
}
playerInstance.setAudioTrack( trackDescriptorObject );
```

---

### setPreferredAudioLanguage( languages, rendition, accessibility, codecList, label)
- Supported UVE version 3.2 and above.
- Set the audio track  preference by languages, rendition, accessibility, codecList and label
- This is functionally equivalent to passing a trackDescriptorObject to setAudioTrack above.
- May be called pre-tune or post tune.
- Behavior is similar to setPreferredAudioLanguage ( JSON String)
- Returns true if setPreferredAudioLanguage has been performed.

|Name|Type|Description|
|----|----|-----------|
| languages | String | ISO-639 audio language preference; |
|||for more than one language, provide comma delimited list from highest to lowest priority:  ‘<HIGHEST>,<...>,<LOWEST>’ |
| rendition | String | Optional preferred rendition for automatic audio selection |
| accessibilityType | String | Optional preferred accessibility type for descriptive audio |
| codecList | String |	Optional Codec preferences, for more than one codecs, provide comma delimited list from highest to lowest priority: ‘<HIGHEST>,<...>,<LOWEST>’ |
| label | String | Optional Preferred Label for automatic audio selection |

- ###### Example :
```js
playerInstance.setPreferredAudioLanguage( "en,de,mul","alternate","description","","native");
```

---

### setPreferredAudioLanguage ( JSON String)
- Supported UVE version 4.4 and above.
- Set the audio track  preference by languages, label, rendition and accessibility
- May be called pretune or post tune.
- Behavior similar for setAudioTrack or setAudioLanguage
    - If PreferredAudioLanguage is not configured:
        - Player will take default preferred language as English, and
        choose better quality track from the language matching list.
        - For Live (with TSB support), time shift buffer includes all available language tracks in the manifest.
    - If PreferredAudioLanguage is set pretune:
        - Player will pick best quality track from the language matching list.
        - For Live (with TSB support), time shift buffer downloads only preferred language tracks but publishes all available languages to application with availability field as false.
        - If preferred audio language is not available in the manifest, first available track will be selected. For Live (with TSB support), buffer downloads first available track and  publishes the other audio tracks with availability field as false.
    - If setPreferredAudioLanguage (or setAudioTrack) is called  post tune:
        - Player will pick best quality track from the language matching list.
        - For Live (with TSB support), If the new preferred language track is already available in time shift buffer, then player changes to new track without losing TSB buffer.
        - For Live (with TSB support), If the new preferred language track is not available in TSB but available in manifest, then time shift buffer will be restarted with new language audio track.


|Name|Type|Description|
|----|----|-----------|
| languages | String | ISO-639 audio language preference; for more than one language, provide comma delimited list from highest to lowest priority:  ‘<HIGHEST>,<...>,<LOWEST>’ |
| rendition | String | Optional preferred rendition for automatic audio selection |
| label	| String | Preferred Label for automatic audio selection |
| accessibility | Object | Optional preferred accessibility object for audio |
| accessibility.scheme | String | Optional Preferred Accessibility scheme Id  |
|  accessibility.string_value | String | Optional Preferred Accessibility scheme Id value |

- ###### Example :
```js
var trackPreferenceObject =
{
    "languages": ["en", "de", "mul"],
    "label": "native",
    "rendition": "alternate",
    "accessibility":
    {
        "scheme": "urn:mpeg:dash:role:2011",
        "string_value": "description",
    }
}
playerInstance.setPreferredAudioLanguage( trackPreferenceObject );
```
---

### setAudioLanguage( language )
- Supported UVE version 3.0 and above.
- Set the audio track language from the available audio tracks list.
- Behavior is similar to setPreferredAudioLanguage.
- Returns true if setAudioLanguage has been performed.

|Name|Type|Description|
|----|----|-----------|
| language | String | Language of desired audio track in the available audio tracks list |

---

### getAvailableTextTracks(allTracks)
- Supported UVE version 1.0 and above.
- Returns the available text tracks information in the stream.

|Name|Type|Description|
|----|----|-----------|
| allTracks | Boolean | If False,returns the available text tracks information in the current playing content/Period(Default false) <br/> If True,returns all the available text tracks information from the Manifest(across multi Periods)|

- ##### DASH

| Name  | Type | Description |
| ---- | ---- | ---- |
| name  | String | Human readable language name e.g: sub_eng. |
| label  | String | Represents the label of the text track. |
| instreamId | String | Identifier for in-band closed captions (for example, "CC1", "SERVICE1"). Typically present for CLOSED-CAPTIONS / in-band CEA-608/708 tracks when signaled via DASH MPD `<Accessibility ... value=...>`. |
| language  | String | iso language code. e.g: eng |
| codec  | String | codec associated with text track. e.g: stpp |
| rendition  | String | Role for DASH. e.g: caption,subtitle,main. |
| accessibilityType |	String | Accessibility value for descriptive, visually impaired signaling e.g: description, captions |
| type |	String | the supported values are
||| captions — A text track (608/708/TTML) meant for the hearing impaired.
|||            which describes all the dialogue and non-dialogue audio portions of the asset (including music, sound effects, etc) |
||| subtitles — A text track (TTML) meant for translating the spoken dialogue into additional languages |
||| subtitles_native — Subtitle in Native language |
| sub-type |	String |	Closed-caption or subtitles |
| availability |	Boolean |	Availability of the text track in current TSB buffer |
| accessibility	| Object |	DASH shall signal a new object accessibility to notify a track as visually impaired |
| accessibility.scheme |	String |	The SchemeId to indicate the type of Accessibility |
||| Example:- urn:scte:dash:cc:cea-608:2015 for cc |
||| urn:tva:metadata:cs:AudioPurposeCS:2007 for subtitle |
| accessibility.int_value |	Number |	The value of Accessibility object; Number for subtile Example:- 2 |
| accessibility.string_value |	String | The string value of Accessibility object for CC; Example:-  "CC1=en" |

- ###### Example:

```sh
[{
    "sub-type":	"CLOSED-CAPTIONS",
    "language":	"en",
    "rendition":	"urn:scte:dash:cc:cea-608:2015",
    "type": "captions",
    "codec":	"CC1",
    "availability":	true,
    "accessibility":	{
        "scheme":	"urn:scte:dash:cc:cea-608:2015",
        "string_value":	"CC1=en"
    }
}, {
    "name":	"subtitle0",
    "sub-type":	"SUBTITLES",
    "language":	"cy",
    "rendition":	"subtitle",
    "type":	"subtitle",
    "codec":	"stpp.ttml.im1t|etd1",
    "availability":	true
}, {
    "name":	"subtitle0",
    "sub-type":	"SUBTITLES",
    "language": "en",
    "codec": "stpp.ttml.im1t|etd1",
    "type":	"subtitle",
    "rendition": "alternate",
    "availability":	true,
    "accessibility":{
        "scheme": 	"urn:tva:metadata:cs:AudioPurposeCS:2007",
        "int_value": 1
    }
}]
```

- ###### Reference:

```html
<AdaptationSet id="100" contentType="text" mimeType="application/mp4" lang="cy" segmentAlignment="true" startWithSAP="1">
    <Role schemeIdUri="urn:mpeg:dash:role:2011" value="subtitle"/>
    <SegmentTemplate initialization="Example_HD_SU_Example_1234_0_1234567890123456789/track-text-repid-$RepresentationID$-tc--header.mp4" media="Example_HD_SU_Example_1234_0_1234567890123456789/track-text-repid-$RepresentationID$-tc--frag-$Number$.mp4" timescale="90000" startNumber="123456789" presentationTimeOffset="987654">
        <SegmentTimeline>
            <S t="7394947162" d="172800" r="14"/>
        </SegmentTimeline>
    </SegmentTemplate>
    <Representation id="subtitle0" bandwidth="20000" codecs="stpp.ttml.im1t|etd1"/>
</AdaptationSet>
```

- ##### HLS
    - Returns the available text tracks(CC) in the content.

| Name  | Type | Description |
| ---- | ---- | ---- |
| name  | String | Human readable language name e.g: sub_eng. |
| language  | String | Identifies the primary language used in the Rendition. This attribute is OPTIONAL. e.g: es |
| codec  | String | Comma-delimited list of formats, where each format specifies a media sample type that is present in one or more Renditions specified by the Variant Stream. |
| rendition  | String | Specifies the group to which the Rendition belongs. GROUP-ID for HLS. |
| characteristics |	String | Pne or more comma-delimited Uniform Type Identifiers [UTI].  This attribute is OPTIONAL. |
| instreamId	| String	| Specifies a Rendition within the segments in the Media Playlist.
||| This attribute is REQUIRED if the TYPE attribute is CLOSED-CAPTIONS |
||| e.g: "CC1", "CC2", "CC3", "CC4", or "SERVICEn" where n MUST be an integer between 1 and 63 |
| type	| String |	Specifies the media type. |
||| Valid strings are AUDIO, VIDEO, SUBTITLES and CLOSED-CAPTIONS. This attribute is REQUIRED. e.g: CLOSED-CAPTIONS |


- ###### Example:

```sh
[{
    "name": "Deutsch",
    "type": "SUBTITLES",
    "language":     "de",
    "rendition":    "subs"
}]
```

- ###### Reference:

```m
 #EXT-X-MEDIA:TYPE=SUBTITLES,GROUP-ID="subs",NAME="Deutsch",DEFAULT=NO,AUTOSELECT=YES,FORCED=NO,LANGUAGE="de",URI="subtitles_de.m3u8"
 #EXT-X-STREAM-INF:PROGRAM-ID=1,BANDWIDTH=258157,CODECS="avc1.4d400d,mp4a.40.2",AUDIO="stereo",RESOLUTION=422x180,SUBTITLES="subs"
```

---

### setPreferredTextLanguage ( JSON String )
- Supported UVE version 4.4 and above.
- Set the text  track  preference by languages, rendition and accessibility
- Supported pre-tune and post tune.
- If PreferredTextLanguage is not configured:
    - Player will take default preferred language as English,
    - For Live (with TSB support), time shift buffer includes all available language tracks in the manifest.
- If PreferredTextLanguage is set pretune:
    - Player will text track from the language matching list.
    - For Live (with TSB support), time shift buffer downloads only preferred language tracks but publishes all available languages to application with availability field as false.
    - If preferred text language is not available in the manifest, first available track will be selected. For Live (with TSB support), buffer downloads first available track and publishes the other text tracks with availability field as false.
- If setPreferredTextLanguage (or setTextTrack) is called post tune:
    - Player will pick the language matching list.
    - For Live (with TSB support), If the new preferred language track is already available in time shift buffer, then player changes to new track without losing TSB buffer.
    - For Live (with TSB support), If the new preferred language track is not available in TSB but available in manifest, then time shift buffer will be restarted with new language text track.
- The langCodePreference AAMP config impacts how requested ISO-639 language codes are matched to the language codes for tracks in the manifest.  This works as below:
  - NO_LANGCODE_PREFERENCE = 0 (default): the requested language code(s) must match those in the manifest.
  - Other values: language code(s) will be normalized to ISO-639 2-character or 3-character versions before matching, so the client can for example specify "eng" to match a manifest with "en", or vice versa.

|Name|Type|Description|
|----|----|-----------|
| language | String | ISO-639 text language preference. 2-character and 3-character codes are supported. |
| languages | String or String[] | For JSON input, either a single ISO-639 text language code string or an array of ISO-639 language codes in priority order (for example, ["eng","spa"]). When setPreferredTextLanguage is called with a non-JSON string parameter, a comma-delimited list of ISO-639 codes from highest to lowest priority (‘<HIGHEST>,<...>,<LOWEST>’) is also accepted. |
| rendition | String | Optional preferred rendition for automatic text selection |
| instreamId | String | Optional preferred instreamId (i.e. CC1, CC2) for automatic text selection |
| label	| String | Optional preferred label for automatic text selection |
| accessibilityType | String | Optional preferred DASH role/type string for text track selection. This is used as the `preferred-text-type` value and is matched against `<Accessibility ... value=...>` entries with scheme `urn:mpeg:dash:role:2011`. |
| accessibility | Object | Optional preferred Accessibility node criteria for text (for example, to select descriptive / VI tracks). This corresponds to the full `<Accessibility ...>` element and is independent of `accessibilityType`. |
| accessibility.scheme | String | Preferred Accessibility scheme Id for the Accessibility node. |
| accessibility.string_value | String | Preferred Accessibility scheme Id value for the Accessibility node when expressed as a string. |
| accessibility.int_value | Number | Preferred Accessibility scheme Id value for the Accessibility node when expressed as a number. |

- ###### Example :
```js
var trackPreferenceObject =
{
    "languages": ["en", "de", "mul"],
    "rendition": "subtitle",
    "label": "native",
    "accessibility":
    {
        "scheme": "urn:tva:metadata:cs:AudioPurposeCS:2007",
        "int_value": 2
    }
}
playerInstance.setPreferredTextLanguage( trackPreferenceObject );
```
---

### getTextTrack( )
- Supported UVE version 2.6 and above.
- Returns the index of the current text track in the available text tracks list.
- Returns -1 when subtitles are disabled.
- Should be only used after text track is selected using setTextTrack() API. It will return -1 otherwise.

---

### getTextTrackInfo
- Supported UVE version 4.4 and above.
- Returns playing Text track information in JSON format.
- Supports both out-of-band captions and in-band closed captions (when available).
- Note: The in-band CC identifier field for this API is named `instreamID`
  (capital `ID`), whereas `getAvailableTextTracks` uses `instreamId`.
  Use the exact casing expected by each API when consuming these fields.

- ###### Examples :

  - Out-of-band text track (e.g., WebVTT or TTML sidecar):

```js
[
    {
        "name": "English",
        "label": "native",
        "language": "eng",
        "codec": "stpp",
        "type": "captions",
        "rendition": "alternate",
        "accessibility":
        {
            "scheme": "urn:tva:metadata:cs:AudioPurposeCS:2007",
            "int_value": 2
        }
    }
]
```

  - In-band closed captions (instream CC):

```js
[
    {
        "name": "CC1",
        "label": "Service 1",
        "language": "eng",
        "type": "captions",
        "instreamID": "1"
    }
]
```
---

### getPreferredTextProperties
- Supported UVE version 4.4 and above.
- Returns Text track information set by application for preferred Text track selection, in JSON format

- ###### Example :
```js
{
    "preferred-text-languages" : "eng,ger,mul",
    "preferred-text-labels": "native",
    "preferred-text-rendition": "alternate",
    "preferred-text-type": "captions",
    "preferred-text-accessibility":
    {
        "schemeId": "urn:tva:metadata:cs:AudioPurposeCS:2007",
        "int_value": 2
    }
}
```
---

### setTextTrack( trackIndex )
- Supported UVE version 2.6 and above.
- Select text track at trackIndex in the available text tracks list.
- Returns true if setTextTrack has been performed.

|Name|Type|Description|
|----|----|-----------|
| trackIndex | Number | Index of desired text track in the available text tracks list |

---

### setClosedCaptionStatus ( status )
- Supported UVE version 2.6 and above.
- Set the ClosedCaption rendering to on/off.
- Returns true if setClosedCaptionStatus has been performed.

|Name|Type|Description|
|----|----|-----------|
| Status | Boolean | True to enable ClosedCaptions <br/> False to disable display of ClosedCaptions |

---
### isOOBCCRenderingSupported ( )
- Supported UVE version 4.12 and above.
- Returns true if out of band caption rendering (WebVTT, TTML) is supported.  This feature has dependencies outside Video Engine.


---

### getTextStyleOptions ( )
- Supported UVE version 2.6 and above.
- Returns the JSON formatted string of current ClosedCaption style options and values.

---

### setTextStyleOptions ( options )
- Supported UVE version 2.6 and above.
- Set the ClosedCaption style options to be used for rendering.
- Returns true if setTextStyleOptions has been performed.

|Name|Type|Description|
|----|----|-----------|
| options | String | JSON formatted string of different rendering style options and its values |


##### Example: options
    {
        "penItalicized": false,
        "textEdgeStyle": "none",
        "textEdgeColor": "black",
        "penSize": "large",
        "windowFillColor": "black",
        "fontStyle": "default",
        "textForegroundColor": "black",
        "windowFillOpacity": "transparent",
        "textForegroundOpacity": "solid",
        "textBackgroundColor": "white",
        "textBackgroundOpacity": "solid",
        "windowBorderEdgeStyle": "none",
        "windowBorderEdgeColor": "black",
        "penUnderline": false
    }

##### Example: options

    {
        "penItalicized": false,
        "textEdgeStyle": "none",
        "textEdgeColor": "black",
        "penSize": "small",
        "windowFillColor": "black",
        "fontStyle": "default",
        "textForegroundColor": "black",
        "windowFillOpacity": "transparent",
        "textForegroundOpacity": "solid",
        "textBackgroundColor": "white",
        "textBackgroundOpacity": "solid",
        "windowBorderEdgeStyle": "none",
        "windowBorderEdgeColor": "black",
        "penUnderline": false
    }

##### Available values for options

|Option name|Available values|Comments|
|----|----|-----------|
|fontStyle|"default","monospaced_serif/Monospaced serif","proportional_serif/Proportional serif","monospaced_sanserif/Monospaced sans serif","proportional_sanserif/Proportional sans serif","casual","cursive","smallcaps/small capital","auto"| "auto" value is not available for Xclass devices |
|textEdgeColor/textForegroundColor/textBackgroundColor/windowBorderEdgeColor/windowFillColor|"black","white","red","green","blue","yellow","magenta","cyan","auto"| "auto" value and windowBorderEdgeColor option are not available for Xclass devices |
|textEdgeStyle/windowBorderEdgeStyle|"none","raised","depressed","uniform","drop_shadow_left/Left drop shadow","drop_shadow_right/Right drop shadow","auto"| "auto" value and windowBorderEdgeStyle option are not available for Xclass devices |
|textForegroundOpacity/textBackgroundOpacity/windowFillOpacity|"solid","flash","translucent","transparent","auto"| "flash" and "auto" values are not available for Xclass devices |
|penItalicized/penUnderline|"false","true","auto"| These options are not available for Xclass devices |
|penSize|"small","standard/medium","large","extra_large","auto"| "auto" value is not available for Xclass devices |

---

### getAvailableThumbnailTracks ( )
- Returns json array of each thumbnail track's metadata

|Name|Type|Description|
|----|----|-----------|
| Resolution | String | String indicating the width x height of the thumbnail images |
| Bandwidth | String | Decimal-Integer encoding - bits per second. Represents bit rate of the thumbnail track |

##### Example:
     [ {
          "RESOLUTION":	"416x234",
          "BANDWIDTH":	71416 },
          {
          "RESOLUTION":	"336x189",
          "BANDWIDTH":	52375 },
          {
          "RESOLUTION":	"224x126",
          "BANDWIDTH":	27413
      }]

---

### setThumbnailTrack(index)
- Set the desired thumbnail track from the list of available thumbnail track metadata.
- Returns true if setThumbnailTrack has been performed.

|Name|Type|Description|
|----|----|-----------|
| Index | Number | Index value based on the available thumbnail tracks |

---

### getThumbnail(startPosition, endPosition)
- Get the thumbnail data for the time range “startPosition” till “endPosition”.
- For linear streams(e.g Live,Hot CDVR,..) start and endPosition has to be specified w.r.t Availability start time

|Name|Type|Description|
|----|----|-----------|
| startPosition | Number | Start value from which the thumbnail data is fetched |
| endPosition | Number | End value till which the thumbnail data is fetched |
| baseUrl | String | The base url which is appended to tile url to fetch the required thumbnail image |
| raw_w | String | Original width of the thumbnail sprite sheet |
| raw_h | String | Original height of the thumbnail sprite sheet |
| width | String | Width of each thumbnail tile present in the sprite sheet |
| height | String | Height of each thumbnail tile present in the sprite sheet |
| tile | String | JSON array of multiple thumbnail tile information |
| url | String | Url for each tile, which is appended with base url to form complete url |
| t | String | Presentation time for each tile |
| d | String | Duration value of each tile |
| x | String | X co-ordinate position to locate the tile from sprite sheet |
| y | String | Y co-ordinate position to locate the tile from sprite sheet |

```js
    {
	    var player = new AAMPMediaPlayer();
	    player.getAvailableThumbnailTracks();
	    setThumbnailTrack(trackIndex);
	    player.getThumbnail(1729737573,1729737600); //linear streams
	    // for VOD
	    player.getThumbnail(0,120); //time range in relative
    }
```

##### Example:

    Linear Streams:
    {
        // Use valid URL instead of example
        "baseUrl" : "https://example.com/12345678-1234-1234-1234-123456789012/", // replace with valid URL
        "raw_w":1600,
        "raw_h":900,
        "width":320,
        "height":180,
        "tile":
        [{
            "url":"keyframes-root_audio_video5-video=206000-5.jpeg",
            "t":1729736580,
            "d":10,
            "x":1282,
            "y":0
        }]
    }

    VOD Streams:
    {
        "baseUrl" : "https://example.com/pub/global/abc/def/Example_1234567890123_01/cmaf_thumbtest_segtime_d/mpeg_2sec/images/416x234/", // replace with valid URL
        "raw_w": 3744,
        "raw_h": 3978,
        "width": 416,
        "height": 234,
        "tile":
        [{
            "url": "pckimage-1.jpg",
            "t": 328.0,
            "d": 2,
            "x": 832,
            "y": 234
        }]
    }

---

### subscribeResponseHeaders(headerNames)
- Supported UVE version 4.1 and above.
- Subscribe http response headers from manifest download

|Name|Type|Description|
|----|----|-----------|
| headerNames | String Array | List of tag names of interest. Examples: C-XServerSignature, X-Powered-By, X-MoneyTrace |

---

### configRuntimeDRM (enableConfiguration)
- Supported UVE version 5.1 and above.
- API to enable DRM Protection event to application for setting Key ID specific setting ( for key rotation)
- Need to register for [contentProtectionDataUpdate](#contentprotectiondataupdate) event with event registration.
- Returns true if configRuntimeDRM has been performed.

|Name|Type|Description|
|----|----|-----------|
| enableConfiguration | Boolean | If True, enable runtime DRM notification for getting Application configuration.   |


---

### setContentProtectionDataConfig(json string)
- Supported UVE version 5.1 and above.
- Configure the DRM Data for license request for every KeyID reported to application
- This API needs to be called before the ProtectionData Update timeout expires in the player . To set the timeout interval refer [setContentProtectionDataUpdateTimeout](#setcontentprotectiondataupdatetimeout_timeout).
- Returns true if setContentProtectionDataUpdateTimeout has been performed.

|Name|Type|Description|
|----|----|-----------|
| keyID | vector<uint8_t> | KeyID reference for DRM license config |
| com.microsoft.playready | String | License server endpoint to use with PlayReady DRM |
| com.widevine.alpha | String | License server endpoint to use with Widevine DRM |
| customData | String | CustomData for license
acquisition |
| authToken | String | Token to be applied for license acquisition |

```
 {
    "keyID" : [57, 49, 49, 100, 98, 54, 99, 99, 45],
    "com.widevine.alpha" : "example.com", // replace with valid URL
    "customData" : “data”,
    "authToken"  : “token string”,
 }
```
---

### setContentProtectionDataUpdateTimeout(timeout)
- Supported UVE version 5.1 and above.
- Configure the ProtectionData Update timeout, in seconds. Default of 5 sec
- Player waits for setContentProtectionDataConfig API update within the timeout interval .On timeout use last configured values .

|Name|Type|Description|
|----|----|-----------|
| timeout | Number | Timeout value in seconds |

---

### getPlaybackStatistics ()
- Supported UVE version 4.3 and above.
- Returns the playback statistics in JSON format during playback.
- Refer appendix for full JSON format

##### Example:

    {
        "timeToTopProfile": 42,
        "timeInTopProfile": 1096,
        "duration": 1359,
        "profileStepDown_Network": 4,
        "displayWidth": 3840,
        "displayHeight": 2160,
        "profileCappingPresent": 0,
        "mediaType": "DASH"
        “playbackMode": "VOD",
        "totalError": 0,
        "numOfGaps": 0,
        "languageSupported": \{"audio1":"en"},
        "main":{"profiles":{"0":{"manifestStat":{"latencyReport":{"timeWin
        dow_0":1},"sessionSummary":{"200":1},"info":{"DownloadTimeMs":287,
        "ParseTimeMs":6,"PeriodCount":1,"Size":20277}}}}
        },
        "video":{"profiles":{"1807164":{"fragmentStat":{"media":{
        "latencyReport":{"timeWindow_0":3},"sessionSummary":{"200":3}
        },
        "init":{"latencyReport":{"timeWindow_0":1},"sessionSummary":{"200"
        :1}}},
        "width":960,"height":540},"4532710":{"fragmentStat":{"media":
        {"latencyReport":{"timeWindow_0":128},"sessionSummary":{"200":128}
        },
        "init":{"latencyReport":{"timeWindow_0":1},"sessionSummary":{"200"
        :1}}},"width":1280,"height":720},"7518491":{"fragmentStat":{"media
        ":{"latencyReport":{"timeWindow_0":548}
    }

---

### getVideoPlaybackQuality ()
- Supported UVE version 5.7 and above
- Returns the playback quality info in JSON format during playback
- This API returns valid data if the video sink is westerossink

##### Example:
    {
        "rendered": 54321,
        "dropped":  12
    }

---

<div style="page-break-after: always;"></div>


## UVE EVENTS

- Application can receive events from player for various state machine.
- Events can be subscribed/unsubscribed as required by application

### addEventListener( name, handler )

| Name | Type | Description |
| ---- | ---- | ------ |
| name | String | Event Name |
| handler | Function | Callback for processing event |

Example:
``` js
    aampPlayer = new AAMPMediaPlayer();
    aampPlayer.addEventListener("playbackStateChanged", playbackStateChangedFn);;
    aampPlayer.addEventListener("mediaMetadata", mediaMetadataParsedFn);
    aampPlayer.addEventListener("playbackStarted", playbackStartedFn);;
    aampPlayer.addEventListener("blocked", blockedEventHandlerFn);;
    aampPlayer.addEventListener("bitrateChanged", bitrateChangedEventHandlerFn);;

```

---

### removeEventListener( name, handler )
| Name | Type | Description |
| ---- | ---- | ------ |
| name | String | Event Name |
| handler | Function | Callback for processing event |

---
### UVE Supported Events

### playbackStarted

**Description:**
- Supported UVE version 0.7 and above.
- Fired when playback starts.

### playbackFailed

**Event Payload:**
- shouldRetry: boolean
- code: number
- description: string
- sessionId: string Refer to [load](#load-uri_autoplay_tuneparams) API for details


**Description:**
- Supported UVE version 0.7 and above.
- Fired when an error occurs.

---

### playbackSpeedChanged

**Event Payload:**
- speed: number
- reason: string
- sessionId: string Refer to [load](#load-uri_autoplay_tuneparams) API for details

**Description:**
- Supported UVE version 0.7 and above.

---

### playbackCompleted

**Description:**
- Supported UVE version 0.7 and above.
- Fired when there is nothing left to play

---

### playlistIndexed

**Event Payload:**
- sessionId: string Refer to [load](#load-uri_autoplay_tuneparams) API for details

**Description:**
- Fired after manifest / playlist parsing completed

---

### playbackProgressUpdate

**Event Payload:**
- sessionId: string Refer to [load](#load-uri_autoplay_tuneparams) API for details
- durationMiliseconds: number
- positionMiliseconds: number
- playbackSpeed: number
- startMiliseconds: number
- endMiliseconds: number
- currentPTS: number
- videoBufferedMiliseconds : number
- audioBufferedMiliseconds : number
- liveLatency : number
- profileBandwidth : number
- networkBandwidth : number
- currentPlayRate : number

**Description:**
- Supported UVE version 0.7 and above.
- Fired based on the interval set
- Added video PTS reporting if enabled with reportVideoPTS config
- Added video buffer value (2.4 version)
- Added audio buffer value (version 7.07 onwards)

---

### decoderAvailable

**Event Payload:**
- sessionId: string Refer to [load](#load-uri_autoplay_tuneparams) API for details
- decoderHandle: number

**Description:**
- Supported UVE version 0.7 and above.
- Fired when video decoder handle becomes available, required for closedcaption parsing + rendering by RDK ClosedCaptions module

---

### jsEvent

**Description:**
- Generic event for jsbinding . To be deprecated

---

### mediaMetadata

**Event Payload:**
- sessionId: string Refer to [load](#load-uri_autoplay_tuneparams) API for details
- durationMiliseconds: number
- languages: string[]
- bitrates: number[]
- playbackSpeeds: number[]
- width: number
- height: number
- hasDrm: boolean
- isLive: boolean
- programStartTime: number
- DRM: string
- tsbDepth: number
- url: string


**Description:**
- Supported UVE version 0.7 and above.
- Fired with metadata of the asset currently played, includes duration(in ms), audio language list, available bitrate list, hasDrm, supported playback speeds, tsbDepth, url (final, effective URL after any 302 redirection by video engine)

---

### enteringLive

**Description:**
- Supported UVE version 0.7 and above.
- Fired when entering live point of a live playlist during/after a seek/trickplay operation

---

### bitrateChanged

**Event Payload:**
- sessionId: string Refer to [load](#load-uri_autoplay_tuneparams) API for details
- time: number
- bitRate: number
- description: string
- width: number
- height: number
- framerate: number
- position: number
- cappedProfile:bool
- displayWidth:number
- displayHeight:number

**Description:**
- Supported UVE version 0.7 and above.
- Fired when video profile is switched by ABR with the metadata associated with newly selected profile.

---

### timedMetadata

**Event Payload:**
- sessionId: string Refer to [load](#load-uri_autoplay_tuneparams) API for details
- time: number
- duration: number
- name: string
- content: string
- type: number
- metadata: object
- id: string

**Description:**
- Supported UVE version 0.8 and above.
- Fired when a subscribed tag is found in the playlist

---

### bulkTimedMetadata

**Event Payload:**
- sessionId: string Refer to [load](#load-uri_autoplay_tuneparams) API for details
- content: string

**Description:**
- Combine all timedMetadata and fire single event if bulkTimedMetadata is enabled

---

### playbackStateChanged

**Event Payload:**
- sessionId: string Refer to [load](#load-uri_autoplay_tuneparams) API for details
- state: [number](#getcurrentstate)

**Description:**
- Supported UVE version 0.7 and above.
- Fired as player state changes while tuning, during steady state playback, or as trickplay commands (play, pause, seek) are processed.
- Example sequences - refer getCurrentState() for details:
    - while tuning: eSTATE_INITIALIZING -> eSTATE_INITIALIZED -> eSTATE_PREPARING -> eSTATE_PREPARED -> eSTATE_PLAYING
    - seek command: -> eSTATE_SEEKING -> eSTATE_PLAYING
    - seek while paused -> eSTATE_SEEKING -> eSTATE_PAUSED
    - stop command: -> eSTATE_IDLE
    - pause command: -> eSTATE_PAUSED
    - playback rate changed back to 1x (normal) speed: -> eSTATE_PLAYING
    - end of stream (EOS) reached: -> eSTATE_COMPLETE
    - playback has stalled due to video buffer running out: -> eSTATE_BUFFERING
    - buffering complete: -> eSTATE_PLAYING

---

### speedsChanged

**Event Payload:**
- sessionId: string Refer to [load](#load-uri_autoplay_tuneparams) API for details
- playbackSpeeds: number[]

**Description:**
- Supported UVE version 0.7 and above.
- Fired when supported playback speeds changes (based on iframe availability)

---

### seeked

**Event Payload:**
- sessionId: string Refer to [load](#load-uri_autoplay_tuneparams) API for details
- Position: number

**Description:**
- Fired when Seek is triggered with a position

---

### tuneProfiling

**Event Payload:**
- sessionId: string Refer to [load](#load-uri_autoplay_tuneparams) API for details
- microdata:string

**Description:**
- Tune profiling data

---

### bufferingChanged

**Event Payload:**
- sessionId: string Refer to [load](#load-uri_autoplay_tuneparams) API for details
- buffering: bool

**Description:**
- Supported UVE version 0.8 and above.
- bufferingChanged events are generated only after streaming has started.
- bufferingChanged is not generated at tune time, while video is paused, or while seeking.
- buffering flag gives status:
    - FALSE -> Video buffer has run dry post-tune and playback is stalled (underflow)
    - TRUE -> Rebuffering is complete and video is again streaming (healthy buffering)
- Note: bufferingChanged will be followed by a general playbackStateChanged event.

---


### ManifestRefresh

**Event Payload:**
- sessionId: string Refer to [load](#load-uri_autoplay_tuneparams) API for details
- manifestDuration: number (duration in seconds)
- manifestPublishedTime: number (UTC seconds)
- noOfPeriods: number (period count)
- manifestType: string("dynamic" or "static")

**Description:**
- sent when a live DASH manifest is refreshed

---

### tuneMetricsData

**Event Payload:**
- sessionId: string Refer to [load](#load-uri_autoplay_tuneparams) API for details
- tuneMetricsData: string

**Description:**
- Tune Metric Data notification with below metric informations
	- pre -> prefix
	- ver -> version for this protocol, initially zero
	- bld -> build incremented when there are significant player changes/optimizations
	- tbu -> tuneStartBaseUTCMS - when tune logically started from AAMP perspective
	- mms -> ManifestDownloadStartTime - offset in milliseconds from tunestart when main manifest begins download
	- mmt -> ManifestDownloadTotalTime - time (ms) taken for main manifest download, relative to ManifestDownloadStartTime
	- mme -> ManifestDownloadFailCount - errors/retries occurred during this operation
	- vps -> video offset in milliseconds from tunestart when playlist subManifest begins download(in ms)
	- vpt -> time (ms) taken for video playlist subManifest download, relative to PlaylistDownloadStartTime(in ms)
	- vpe -> video playlist errors/retries occurred during this operation
	- aps -> audio offset in milliseconds from tunestart when playlist subManifest begins download(in ms)
	- apt -> time (ms) taken for audio playlist subManifest download, relative to PlaylistDownloadStartTime(in ms)
	- ape -> audio playlist errors/retries occurred during this operation
	- vis -> video init-segment relative start time(in ms)
	- vit -> video init-segment total duration(in ms)
	- vie -> video init-segment errors/retries occurred during this operation
	- ais -> audio init-segment relative start time(in ms)
	- ait -> audio init-segment total duration(in ms)
	- aie -> audio init-segment errors/retries occurred during this operation
	- vfs -> video fragment relative start time(in ms)
	- vft -> video fragment total duration(in ms)
	- vfe -> video fragment errors/retries occurred during this operation
	- vfb -> video bandwidth in bps
	- afs -> audio fragment relative start time(in ms)
	- aft -> audio fragment total duration(in ms)
	- afe -> audio fragment errors/retries occurred during this operation
	- afb -> audio bandwidth in bps
	- las -> drmLicenseRequestStart - offset in milliseconds from tunestart
	- lat ->drmLicenseRequestTotalTime -time (ms) for license acquisition relative to drmLicenseRequestStart
	- dfe ->drmFailErrorCode nonzero if drm license acquisition failed during tuning
	- lpr -> LAPreProcDuration - License acquisition pre-processing duration in ms
	- lnw -> LANetworkDuration - License acquisition network duration in ms
	- lps -> LAPostProcDuration - License acquisition post-processing duration in ms
	- vdd -> Video Decryption Duration - Video fragment decrypt duration in ms
	- add -> Audio Decryption Duration - Audio fragment decrypt duration in ms
	- gps -> gstPlaying: offset in ms from tunestart when pipeline first fed data
	- gff -> Total tune time if successful - offset in ms from tunestart to when first frame of video is decoded/presented.
	- gdt -> gstDecodeTime - Total time taken in ms to decode first frame.
	- cnt -> Content Type
		- Content Types: Unknown(0), CDVR(1), VOD(2), Linear(3), IVOD(4), EAS(5), Camera(6), DVR(7), MDVR(8), IPDVR(9), PPV(10), OTT(11), OTA(12), HDMI Input(13), COMPOSITE Input(14), SLE(15). Refer [load](#load-uri_autoplay_tuneparams) API
	- stt -> Media stream Type + Drm codec Type
		- Media stream Types: DASH (20), HLS(10), PROGRESSIVE(30),  HLS_MP4(40), Others(0)
		- DRM codec Types: Clear Key(0),  Widevine(1) ,  PlayReady(2), Vanilla AES(3)
		- Example:
			stt for playready dash stream = 22 =  20 (DASH ) + 2 ( PlayReady Codec type )
	- ftt -> firstTune - To identify the first tune after load
	- pbm -> If Player was in prebuffered mode
	- tpb -> time spent in prebuffered(BG) mode
	- dus -> Asset duration in seconds
	- ifw -> Connection is wifi or not - wifi(1) ethernet(0)
	- tat -> TuneAttempts
	- tst -> Tunestatus -success(1) failure (0)
	- frs -> Failure Reason
	- app -> AppName
	- tsb -> TSBEnabled or not - enabled(1) not enabled(0)
	- tot -> TotalTime -for failure and interrupt tune -it is time at which failure /interrupt reported
    - pst -> PreviousStopTime - total time (in milliseconds) it took previous playback to stop
---
### needManifest

**Event Payload:**
- sessionId: string updated manifest for live refresh; refer to [load](#load-uri_autoplay_tuneparams) API for details

**Description:**
- Fired when new manifest is required after live refresh interval

---
### durationChanged

**Description:**
- To be deprecated

---

### audioTracksChanged

**Event Payload:**
- sessionId: string Refer to [load](#load-uri_autoplay_tuneparams) API for details

**Description:**
- Fired when Audio track is changed during playback

---

### textTracksChanged

**Event Payload:**
- sessionId: string Refer to [load](#load-uri_autoplay_tuneparams) API for details

**Description:**
- Fired when Text track is changed during playback

---

### contentBreaksChanged

**Description:**
- To be deprecated

---

### contentStarted

**Description:**
- To be deprecated

---

### contentCompleted

**Description:**
- To be deprecated

---

### drmMetadata

**Event Payload:**

| Field | Type | Description |
| ----- | ---- | ----------- |
| sessionId | string | Refer to [load](#load-uri_autoplay_tuneparams) API for details |
| code | number | Access status value from DRM system |
| description | string | Human-readable access status string |
| headers | array | HTTP response headers from the license request. Populated only when `sendLicenseResponseHeaders` config is enabled |
| responseData | string | Raw license server error response body. **Populated on failure only** — set to the server error body when the HTTP status code is not 200/206 (direct HTTP path), or when SecClient/SecManager reports failure. Falls back to `undefined` if the server returned an empty body. Empty string on success |
| networkMetrics | string | **JSON-encoded** telemetry string. Must be `JSON.parse()`d before accessing individual fields. See schema below |

**Description:**
- Supported UVE version 0.7 and above.
- Fired when a DRM license request completes (success or failure), or when DRM auth data changes/expires.

**`networkMetrics` JSON Schema:**

Keys always present:

| Key | Type | Description |
| --- | ---- | ----------- |
| `req` | number | DRM license request type. `0` = direct HTTP fetch via curl (`getLicense`); `1` = SecClient / SecManager / FireboltSDK path (`getLicenseSec`) |
| `res` | number | HTTP or curl response/error code (e.g. `200`, `412`, `28` for `CURLE_OPERATION_TIMEDOUT`). Consistently represents the HTTP/curl status code across all DRM flows |
| `tot` | number | Total time in **milliseconds** from the first attempt to the final outcome, including all retries. Retries happen on network errors (timeout, DNS failure), HTTP responses, or SecClient/SecManager failures|
| `url` | string | License server URL that was contacted |

Keys present only when `req` = `0` (DRM_GET_LICENSE — direct HTTP/curl path). **Not available** when `req` = `1` (DRM_GET_LICENSE_SEC — SecClient/SecManager/FireboltSDK), because in that path the HTTP transaction is handled entirely inside SecClient/SecManager/FireboltSDK; AAMP never invokes curl for the license request and therefore cannot collect curl-level timing metrics.

| Key | Type | Description |
| --- | ---- | ----------- |
| `con` | number | TCP connection establishment time (ms) |
| `str` | number | Time until first response byte received — "start transfer" (ms) |
| `dns` | number | DNS resolution time (ms) |
| `acn` | number | Application-level connect time — TCP + TLS handshake (ms) |
| `ptr` | number | Pre-transfer time; from start until just before first byte is sent (ms) |
| `rdt` | number | Total redirect time (ms); `0` if no redirects |
| `dls` | number | Downloaded response body size (bytes) |
| `rqs` | number | Uploaded request body size (bytes) |

**Example via direct HTTP (`req=0`):**
```json
{
  "req": 0,
  "res": 200,
  "tot": 312,
  "url": "https://license.example.com/widevine",
  "con": 45,
  "str": 290,
  "dns": 12,
  "acn": 80,
  "ptr": 85,
  "rdt": 0,
  "dls": 1024,
  "rqs": 768
}
```

**Example via SecManager (`req=1`):**
```json
{
  "req": 1,
  "res": 412,
  "tot": 520,
  "url": "license.example.com"
}
```
---

### anomalyReport

**Event Payload:**
- sessionId: string Refer to [load](#load-uri_autoplay_tuneparams) API for details
- severity: string
- description:string

**Description:**
- Fired for any anomaly during playback.

---

### vttCueDataListener

**Event Payload:**
- sessionId: string Refer to [load](#load-uri_autoplay_tuneparams) API for details
- start : number
- duration: number
- text:string

**Description:**
- This event is fired for VTT cue parsed from the WebVTT playlist
- Some platforms doesnt support rendering of WebVTT, in that case Application should render the WebVTT.
- Refer "isOOBCCRenderingSupported" API.

---

### adResolved

**Event Payload:**
- sessionId: string Refer to [load](#load-uri_autoplay_tuneparams) API for details
- resolvedStatus: bool
- placementId: string
- placementStartTime: number
- placementDuration: number

**Description:**
- Supported UVE version 0.8 and above.
- Confirmation that an upcoming ad's main manifest has been successfully downloaded and parsed.

---

### reservationStart

**Event Payload:**
- sessionId: string Refer to [load](#load-uri_autoplay_tuneparams) API for details
- adbreakId: string
- time: number

**Description:**
- Supported UVE version 0.8 and above.
- Sent upon playback into an ad break (one or more ads).

---

### reservationEnd

**Event Payload:**
- sessionId: string Refer to [load](#load-uri_autoplay_tuneparams) API for details
- adbreakId: string
- time: number

**Description:**
- Supported UVE version 0.8 and above.
- Sent upon completion of an ad break (back to main content) - it is NOT sent (per previously agreed contract) if user does trickplay or seek to abort ad playback

---

### placementStart

**Event Payload:**
- sessionId: string Refer to [load](#load-uri_autoplay_tuneparams) API for details
- adId: string
- time: number

**Description:**
- Supported UVE version 0.8 and above.
- This is sent in real time when injecting first frame of a new ad on content->ad or ad->ad transition. Should be accurate compared to onscreen frames.

---

### placementEnd

**Event Payload:**
- sessionId: string Refer to [load](#load-uri_autoplay_tuneparams) API for details
- adId: string
- time: number

**Description:**
- Supported UVE version 0.8 and above.
- This is sent in real time after passively playing to end of an ad - it is NOT sent (per previously agreed contract) if user does trickplay or seek to abort ad playback.

---

### placementError

**Event Payload:**
- sessionId: string Refer to [load](#load-uri_autoplay_tuneparams) API for details
- adId: string
- time: number
- error: number

**Description:**
- Supported UVE version 0.8 and above.
- Generated only for exception while attempting to play out ad content.

---

### placementProgress

**Event Payload:**
- sessionId: string Refer to [load](#load-uri_autoplay_tuneparams) API for details
- adId: string
- time: number

**Description:**
- Supported UVE version 0.8 and above.
- Sent periodically while ad is being played out, giving an estimate percentage-watched metric. It's interpolated based on elapsed time, and should repeat same value if paused.

---

### metricsData

**Event Payload:**
- sessionId: string Refer to [load](#load-uri_autoplay_tuneparams) API for details
- metricType:string
- traceID:string
- metricData:string

**Description:**
- Playback data after video end.

---

### id3Metadata

**Event Payload:**
- sessionId: string Refer to [load](#load-uri_autoplay_tuneparams) API for details
- schemeIdUri : string
- value : string
- timescale : number
- presentationTime : number
- eventDuration : number
- id : number
- timestampOffset : number
- data : array
- length: number

**Description:**
- This event is fired when ID3Metadata is parsed from the stream playlist.

---

### drmMessage

**Event Payload:**
- sessionId: string Refer to [load](#load-uri_autoplay_tuneparams) API for details
- data:string

**Description:**
- Drm challenge data after individualization

---

### blocked

**Event Payload:**
- sessionId: string Refer to [load](#load-uri_autoplay_tuneparams) API for details
- reason:string

**Description:**
- Event with reason for video blocked. *used only in ATSC playback.

---

### contentGap

**Event Payload:**
- sessionId: string Refer to [load](#load-uri_autoplay_tuneparams) API for details
- time:number
- duration:number


**Description:**
- Event with Content gap information in TSB due to network interruption

---

### httpResponseHeader

**Event Payload:**
- sessionId: string Refer to [load](#load-uri_autoplay_tuneparams) API for details
- header:string
- response:string


**Description:**
- http header fields received in manifest download

---

### watermarkSessionUpdate

**Event Payload:**
- sessionId: string Refer to [load](#load-uri_autoplay_tuneparams) API for details.
- sessionHandle:string
- system:string Identifies the content watermarking protection provider, i.e. "fmts_asid" (FriendMTS).  Note: this is only valid when using SecManager.
- status:string Additional information regarding security system state.  See below table:

| Code      | Name         |   Definition
| --------- |------------- |--------------
| 1         | GRANTED      | No security issues
| 2         | NOT_REQUIRED | Watermark session granted
| 3         | DENIED       | Watermark session not required
| 4 / 20001 | FAILED       | Watermark session denied

**Description:**
- Watermarking session information

---

### contentProtectionDataUpdate

**Event Payload:**
- sessionId: string Refer to [load](#load-uri_autoplay_tuneparams) API for details
- keyID : vector<uint8_t>
- streamType : string

**Description:**
- Player notifies the application for new KeyId in the manifest, to get new config(if needed) for license fetch.
- Refer setContentProtectionDataConfig for setting license fetch parameters.
```
Example:
{"keyID":
[102,101,97,53,53,48,48,54,45,51,102,48,102,45,99,101,97,99,45,55,54,99,48,45,5
1,55,50,98,99,50,54,100,48,55,100,50,0],"streamType":"VIDEO", "sessionId":""}
```
---

### monitorAVStatus

**Event Payload:**
- sessionId: string Refer to [load](#load-uri_autoplay_tuneparams) API for details
- currentState: string
- videoPosMs : number
- audioPosMs : number
- timeInStateMs : number

**Description:**
- Player periodically reports the video and audio position to the application which could be used to infer video freeze, audio drop, or av sync issues.
- Reporting interval can be configured using monitorAVReportingInterval which ranges from 1s to 60s.
- Player also sends a currentState which is identified by the player based on the continuous a/v positions.

---

<div style="page-break-after: always;"></div>

## Client DAI Feature Support
This feature can be enabled in two methods
* Video Engine Managed
* Application managing Multi Player instance

### CDAI Mechanism #1 – Engine Managed CDAI

Supported for DASH Linear, working with period structure and SCTE35 markers.  Supports optional replacement for like-amount of content.  Source may be left as-is, replaced with a single similar sized alternate ad, or replaced with multiple alternate ads whose total duration is expected to fill the ad break.  If insufficient alternate content duration is provided, or ads manifests are unable to download, player falls back to original (non-DAI) content.

#### setSubscribedTags( tagNames )
- Supported UVE version 0.8 and above.
- Subscribe to specific tags / metadata in manifest
- Returns true if setSubscribedTags has been performed.

| Name | Type | Description |
| ---- | ---- | ------ |
| tagNames | String Array | List of tag names of interest. Examples: #EXT-X-IDENTITY-ADS, #EXT-X-MESSAGE-REF, #EXT-X-CUE, #EXT-X-ASSET-ID, #EXT-X-TRICKMODE-RESTRICTION, #EXT-X-CONTENT-IDENTIFIER |

---

#### setAlternateContent( reservationObject, promiseCallback )
Supported UVE version 0.8 and above

| Name | Type | Description |
| ---- | ---- | ------ |
| reservationObject | Object | context for the alternate content to be played out for an advertisement opportunity. |
| reservationObject.reservationId | String | ad break identifier |
| reservationObject.reservationBehavior | Number | enum (unused)  |
| reservationObject.placementRequest.id | String | advertisement identifier (used in callback) |
| reservationObject.placementRequest.pts | Number | presentation time offset (unused) |
| reservationObject.placementRequest.url | String | Ad manifest locator |
| promiseCallback | Function | Signals success/failure while retrieving ad manifest and preparing for playback. |

---

#### notifyReservationCompletion( reservationId, time )
- Supported UVE version 0.8 and above.
- Notify video engine when all ad placements for a particular reservation have been set via setAlternateContent.

| Name | Type | Description |
| ---- | ---- | ------ |
| reservationId | String |  |
| Time | Number |  |

---

### CDAI Mechanism #2 – Multi player instance
Can be leveraged for quick stream transitions.  Suitable for preroll, and midroll insertions.  No limitations with respect to content type – can transition between DASH and HLS.


##### Example of midroll Ad insertions and resume main content playback:

| Main content (0 – 180 Sec) | AD1 (0 -40 Sec) | AD2 (0 – 30 Sec) | Main Content (180 – 600 Sec) |
| ---- | ---- | --- | ---- |

##### Main Content (0 – 180 sec):
**create foreground player and start streaming of main content**
```
var player = new AAMPMediaPlayer();
player.load( “http://test.com/content.mpd” );
```
**create background player and preload AD1**
```
var adPlayer1 = new AAMPMediaPlayer();
adPlayer1.load( “http://test.com/ad1.mpd”, false );
```

##### AD1 (0 – 40 Sec)
**time of AD1 start, stop active player and activate background player for AD1**
```
var position = Player. getCurrentPosition() // get current playback position
player.detach();
adPlayer1.play();
player.stop();
```
**preload AD2 in background player**
```
var adPlayer2 = new AAMPMediaPlayer();
adPlayer2.load( “http://test.com/ad2.mpd”, false )
```
##### AD2 (0 – 30 Sec)
**EOS of AD1, stop active player and activate background player for AD2**
```
adPlayer1.detach();
adPlayer2.play();
adPlayer1.stop();
```
**preload Main content in background and set last playback position**
```
var player = new AAMPMediaPlayer();
player. Seek (position)
player.load( “http://test.com/content.mpd”, false );
```
##### Main Content (180 – 600 Sec)
**EOS of AD2, stop active player and activate background player for main content**
```
adPlayer2.detach();
player.play();
adPlayer2.stop();
```

When a player instance is no longer needed, recommend to call explicit release() method rather than rely only on eventual garbage collection.  However, player instances can be recycled and reused throughout an app's lifecycle.


### CDAI Mechanism #3 – VOD CDAI (Manifest-Stitching)

Supported for DASH VOD.  Uses the same `setAlternateContent` / `notifyReservationCompletion` reservation API as Mechanism #1 (Engine Managed CDAI).  Ad pods are inserted at registered time positions in a stitched multi-period MPD built by the player before playback begins; the result is a single, seamlessly playable manifest.  Preroll, midroll, and postroll positions are all supported.

**Key characteristics:**
- All ad manifests **must be pre-resolved before `load()` is called** (pre-tune resolution).  The player does not support runtime ad resolution for this mechanism — all `setAlternateContent` + `notifyReservationCompletion` calls must complete before tune.
- The player builds a continuous 0-based stitched timeline.  Position values reported by `getCurrentPosition()` and the ad event `time` fields reflect this stitched timeline, not the source VOD timeline.  Use `getVirtualPosition()` to convert a stitched position back to the source VOD timeline offset.
- `reservationStart`, `placementStart`, `placementProgress`, `placementEnd`, and `reservationEnd` events are fired as the playhead crosses each ad break on the stitched timeline.
- After a seek, all ad events re-fire as the playhead crosses each break again (same behaviour as live CDAI with TSB).

---

#### Workflow

1. Register all VOD insertion points via `registerVodAdBreak` — before `load()`.
2. For each break, call `setAlternateContent` (one call per ad in the pod) followed by `notifyReservationCompletion`.  **All resolution must complete before `load()` is called.**
3. Call `load()`.  The player fetches all resolved ad manifests, stitches them into a multi-period MPD, and begins playback.
4. As the playhead crosses each ad break on the stitched timeline, AAMP fires `reservationStart`, `placementStart`, periodic `placementProgress`, `placementEnd`, and `reservationEnd` events for beaconing.
5. Application may call `cancelVodAdBreak` before `load()` to skip a break entirely.

---

#### registerVodAdBreak( breakInfo )

Register a VOD ad-break insertion point.  Must be called before `load()`.

| Name | Type | Description |
| ---- | ---- | ----------- |
| breakInfo | Object | Insertion point descriptor |
| breakInfo.breakId | String | Unique identifier for this break; used as `reservationId` in `setAlternateContent` |
| breakInfo.insertionPointSec | Number | Position in the source VOD timeline (seconds) at which the ad pod will be inserted.  Use the source asset duration for a postroll. |
| breakInfo.breakDurationSec | Number | Advisory break duration in seconds. |
| breakInfo.breakType | String | `"preroll"`, `"midroll"`, or `"postroll"` (informational only) |

```javascript
player.registerVodAdBreak({
    breakId: "break-preroll",
    insertionPointSec: 0,
    breakDurationSec: 60,
    breakType: "preroll"
});
player.registerVodAdBreak({
    breakId: "break-mid1",
    insertionPointSec: 300,
    breakDurationSec: 30,
    breakType: "midroll"
});
```

---

#### cancelVodAdBreak( breakId )

Cancel a registered VOD ad-break before `load()`.  Has no effect after the player has started.

| Name | Type | Description |
| ---- | ---- | ----------- |
| breakId | String | Identifier previously passed to `registerVodAdBreak` |

```javascript
player.cancelVodAdBreak("break-mid1");
```

---

#### getVirtualPosition()

Returns the current playback position mapped back to the **source VOD timeline** (seconds), excluding ad pod durations.  Because the stitched MPD has a continuous 0-based timeline that includes ad periods, `getCurrentPosition()` advances through ad time.  Use `getVirtualPosition()` when the application needs the equivalent source-content offset (e.g. for resume-point tracking).

```javascript
var sourceOffsetSec = player.getVirtualPosition();
```

---

#### vodAdBreakOpportunity event

> **Note:** For manifest-stitching VOD CDAI, all ad resolution must be complete _before_ `load()`.  The `vodAdBreakOpportunity` event is **not** used in this mechanism; ad URLs are supplied directly via `setAlternateContent` before tune.

---

#### Ad Progress Events for VOD CDAI

As the playhead plays through the stitched timeline, AAMP fires the standard CDAI ad events.  For VOD the `time` field in each event is a **stitched-timeline position in milliseconds**.

| Event | Fired when |
| ----- | ---------- |
| `reservationStart` | Playhead enters the ad break window (before the first ad in the pod) |
| `placementStart` | Playhead enters an individual ad within the pod |
| `placementProgress` | Fired periodically while an ad is playing (same cadence as linear CDAI) |
| `placementEnd` | Playhead exits an individual ad |
| `reservationEnd` | All ads in the pod have completed |

**Notes:**
- Each `breakId` maps to exactly one ad MPD URL. To chain multiple ads at the same timeline position, register each ad under its own unique `breakId` with the same `insertionPointSec` — the stitcher sequences them in registration order. One `placementStart`/`placementEnd` pair fires per `breakId`; `reservationStart`/`reservationEnd` fire once per `breakId`.
- After a seek that lands before a previously-seen ad break, all events for that break re-fire as the playhead crosses it again.
- `reservationEnd` and `placementEnd` **are** sent for VOD CDAI after normal playback to end of break.

---

#### Complete VOD CDAI Example

```javascript
var player = new AAMPMediaPlayer();

// 1. Register all insertion points.
//    Each breakId maps to exactly one ad MPD.
//    To chain two ads at the same position, use two breakIds with the same insertionPointSec.
player.registerVodAdBreak({ breakId:"pre",   insertionPointSec:0,   breakDurationSec:30, breakType:"preroll"  });
player.registerVodAdBreak({ breakId:"mid1a", insertionPointSec:180, breakDurationSec:15, breakType:"midroll"  });
player.registerVodAdBreak({ breakId:"mid1b", insertionPointSec:180, breakDurationSec:15, breakType:"midroll"  });
player.registerVodAdBreak({ breakId:"post",  insertionPointSec:600, breakDurationSec:30, breakType:"postroll" });

// 2. Resolve all ads pre-tune (one setAlternateContent per breakId)
player.setAlternateContent({
    reservationId: "pre",
    reservationBehavior: 0,
    placementRequest: { id: "ad-pre-001", pts: 0, url: "https://ad.example.com/pre.mpd" }
}, function(id, ok) {});
player.notifyReservationCompletion("pre", 0);

player.setAlternateContent({
    reservationId: "mid1a",
    reservationBehavior: 0,
    placementRequest: { id: "ad-mid1-001", pts: 0, url: "https://ad.example.com/mid1a.mpd" }
}, function(id, ok) {});
player.notifyReservationCompletion("mid1a", 0);

player.setAlternateContent({
    reservationId: "mid1b",
    reservationBehavior: 0,
    placementRequest: { id: "ad-mid1-002", pts: 0, url: "https://ad.example.com/mid1b.mpd" }
}, function(id, ok) {});
player.notifyReservationCompletion("mid1b", 0);

player.setAlternateContent({
    reservationId: "post",
    reservationBehavior: 0,
    placementRequest: { id: "ad-post-001", pts: 0, url: "https://ad.example.com/post.mpd" }
}, function(id, ok) {});
player.notifyReservationCompletion("post", 0);

// 3. Subscribe to ad events for beaconing
player.addEventListener("reservationStart",  function(e) { beacon("resStart", e.adbreakId, e.time); });
player.addEventListener("placementStart",    function(e) { beacon("plcStart", e.adId,      e.time); });
player.addEventListener("placementProgress", function(e) { beacon("plcProg",  e.adId,      e.time); });
player.addEventListener("placementEnd",      function(e) { beacon("plcEnd",   e.adId,      e.time); });
player.addEventListener("reservationEnd",    function(e) { beacon("resEnd",   e.adbreakId, e.time); });

// 4. Load — player stitches the manifest and begins playback
player.load("https://content.example.com/movie.mpd");
```

---

#### Sequence Diagram – VOD CDAI Configure and Playback

The following diagram shows the full interaction between the application, the AAMP player, and the ad decision system for a VOD asset with one preroll (30 s) and one midroll (30 s) at source position 180 s.  After stitching, the midroll begins at stitched-timeline position 210 s (180 s of content + 30 s preroll).

```
App                          AAMP Player                Ad Server
 |                                |                         |
 |--registerVodAdBreak(pre)------>|                         |
 |--registerVodAdBreak(mid1)----->|                         |
 |                                |                         |
 |   < out-of-band ad decision >  |                         |
 |<--------------------------------------------ad URLs----->|
 |                                |                         |
 |--setAlternateContent(pre,  ad-pre-001,  pre.mpd)-------->|
 |--notifyReservationCompletion(pre)----------------------->|
 |--setAlternateContent(mid1, ad-mid1-001, mid1a.mpd)------>|
 |--setAlternateContent(mid1, ad-mid1-002, mid1b.mpd)------>|
 |--notifyReservationCompletion(mid1)--------------------->|
 |                                |                         |
 |--load("movie.mpd")------------>|                         |
 |                                |--fetch movie.mpd------->|
 |                                |--fetch pre.mpd--------->|
 |                                |--fetch mid1a.mpd------->|
 |                                |--fetch mid1b.mpd------->|
 |                                |--stitch MPD             |
 |                                |  [pre    0 s – 30 s]    |
 |                                |  [content 30 s – 210 s] |
 |                                |  [mid1a 210 s – 225 s]  |
 |                                |  [mid1b 225 s – 240 s]  |
 |                                |  [content 240 s – ...]  |
 |                                |                         |
 |   == Playhead at stitched 0 ms (preroll) ==              |
 |                                |                         |
 |<--reservationStart(pre,  t=0 ms)------------------------|
 |<--placementStart(ad-pre-001, t=0 ms)--------------------|
 |<--placementProgress(ad-pre-001, t=...) [periodic]-------|
 |                                |                         |
 |   == Playhead at stitched 30 000 ms ==                   |
 |                                |                         |
 |<--placementEnd(ad-pre-001, t=30000 ms)------------------|
 |<--reservationEnd(pre,  t=30000 ms)----------------------|
 |                                |                         |
 |   == Playhead at stitched 210 000 ms (midroll) ==        |
 |                                |                         |
 |<--reservationStart(mid1, t=210000 ms)-------------------|
 |<--placementStart(ad-mid1-001, t=210000 ms)--------------|
 |<--placementProgress(ad-mid1-001, t=...) [periodic]------|
 |                                |                         |
 |   == Playhead at stitched 225 000 ms ==                  |
 |                                |                         |
 |<--placementEnd(ad-mid1-001,  t=225000 ms)---------------|
 |<--placementStart(ad-mid1-002, t=225000 ms)--------------|
 |<--placementProgress(ad-mid1-002, t=...) [periodic]------|
 |                                |                         |
 |   == Playhead at stitched 240 000 ms ==                  |
 |                                |                         |
 |<--placementEnd(ad-mid1-002,  t=240000 ms)---------------|
 |<--reservationEnd(mid1, t=240000 ms)--------------------|
 |                                |                         |
 |   == getVirtualPosition() ==                             |
 |                                |                         |
 |--getVirtualPosition()--------->|                         |
 |<--returns source-content offset (stitched pos – ad time)-|
```

**Key points illustrated:**
- All `setAlternateContent` / `notifyReservationCompletion` calls happen **before** `load()`.
- `time` values in events are stitched-timeline milliseconds, not source-content seconds.
- `getVirtualPosition()` translates the stitched position back to source-content time.
- For a multi-ad pod (`mid1` above), `reservationStart`/`reservationEnd` fire once for the whole pod; `placementStart`/`placementEnd` fire once per individual ad.


---

<div style="page-break-after: always;"></div>
---

<div style="page-break-after: always;"></div>

## Universal Video Engine Player Errors

| Error Code | Code | SubCode | Error String |
| ---- | ---- | ----- | ----- |
| AAMP_TUNE_INIT_FAILED | 10 | 1 | AAMP: init failed |
| AAMP_TUNE_INIT_FAILED_MANIFEST_DNLD_ERROR | 10 | 2 | AAMP: init failed (unable to download manifest) |
| AAMP_TUNE_INIT_FAILED_MANIFEST_CONTENT_ERROR | 10 | 3 | AAMP: init failed (manifest missing tracks) |
| AAMP_TUNE_INIT_FAILED_MANIFEST_PARSE_ERROR | 10 | 4 | AAMP: init failed (corrupt/invalid manifest) |
| AAMP_TUNE_INIT_FAILED_PLAYLIST_VIDEO_DNLD_ERROR | 10 | 5 | AAMP: init failed (unable to download video playlist) |
| AAMP_TUNE_INIT_FAILED_PLAYLIST_AUDIO_DNLD_ERROR | 10 | 6 | AAMP: init failed (unable to download audio playlist) |
| AAMP_TUNE_INIT_FAILED_TRACK_SYNC_ERROR | 10 | 7 | AAMP: init failed (unsynchronized tracks) |
| AAMP_TUNE_CONTENT_NOT_FOUND | 20 | 1 | AAMP: Resource was not found at the URL(HTTP 404) |
| AAMP_TUNE_MANIFEST_REQ_FAILED | 30 | 1 | AAMP: Manifest Download failed |
| AAMP_TUNE_FRAGMENT_DOWNLOAD_FAILURE | 30 | 2 | AAMP: fragment download failures |
| AAMP_TUNE_INIT_FRAGMENT_DOWNLOAD_FAILURE | 30 | 3 | AAMP: init fragment download failed |
| AAMP_TUNE_INVALID_MANIFEST_FAILURE | 30 | 4 | AAMP: Invalid Manifest, parse failed |
| AAMP_TUNE_MP4_INIT_FRAGMENT_MISSING | 30 | 5 | AAMP: init fragments missing in playlist |
| AAMP_TUNE_DNS_RESOLVE_TIMEOUT | 30 | 6 | AAMP: Manifest download failed due to DNS resolve timeout |
| AAMP_TUNE_CURL_CONNECTION_TIMEOUT | 30 | 7 | AAMP: Manifest download failed due to connection timeout |
| AAMP_TUNE_DATA_TRANSFER_TIMEOUT | 30 | 8 | AAMP: Manifest download failed due to data transfer timeout |
| AAMP_TUNE_AUTHORIZATION_FAILURE | 40 | 1 | AAMP: Authorization failure |
| AAMP_TUNE_UNTRACKED_DRM_ERROR | 50 | 1 | AAMP: DRM error untracked error |
| AAMP_TUNE_DRM_INIT_FAILED | 50 | 2 | AAMP: DRM Initialization Failed |
| AAMP_TUNE_DRM_DATA_BIND_FAILED | 50 | 3 | AAMP: InitData-DRM Binding Failed |
| AAMP_TUNE_DRM_SESSIONID_EMPTY | 50 | 4 | AAMP: DRM Session ID Empty |
| AAMP_TUNE_DRM_CHALLENGE_FAILED | 50 | 5 | AAMP: DRM License Challenge Generation Failed |
| AAMP_TUNE_LICENCE_TIMEOUT | 50 | 6 | AAMP: DRM License Request Timed out |
| AAMP_TUNE_LICENCE_REQUEST_FAILED | 50 | 7 | AAMP: DRM License Request Failed |
| AAMP_TUNE_INVALID_DRM_KEY | 50 | 8 | AAMP: Invalid Key Error, from DRM |
| AAMP_TUNE_FAILED_TO_GET_KEYID | 50 | 9 | AAMP: Failed to parse key id from PSSH |
| AAMP_TUNE_CORRUPT_DRM_DATA | 50 | 10 | AAMP: DRM failure due to Corrupt DRM files |
| AAMP_TUNE_CORRUPT_DRM_METADATA | 50 | 11 | AAMP: DRM failure due to Bad DRMMetadata in stream |
| AAMP_TUNE_DRM_DECRYPT_FAILED | 50 | 12 | AAMP: DRM Decryption Failed for Fragments |
| AAMP_TUNE_DRM_UNSUPPORTED | 50 | 13 | AAMP: DRM format Unsupported |
| AAMP_TUNE_DRM_SELF_ABORT | 50 | 14 | AAMP: DRM license request aborted by player |
| AAMP_TUNE_FAILED_TO_GET_ACCESS_TOKEN | 50 | 15 | AAMP: Failed to get access token from Auth Service |
| AAMP_TUNE_DRM_KEY_UPDATE_FAILED | 50 | 16 | AAMP: Failed to process DRM key |
| AAMP_TUNE_DRM_SESSION_CREATE_FAILED | 50 | 17 | AAMP: OCDM session construction failed |
| AAMP_TUNE_DEVICE_NOT_PROVISIONED | 51 | 1 | AAMP: Device not provisioned |
| AAMP_TUNE_HDCP_COMPLIANCE_ERROR | 52 | 1 | AAMP: HDCP Compliance Check Failure |

| AAMP_TUNE_UNSUPPORTED_STREAM_TYPE | 60 | 1 | AAMP: Unsupported Stream Type |
| AAMP_TUNE_UNSUPPORTED_AUDIO_TYPE | 60 | 2 | AAMP: No supported Audio Types in Manifest |
| AAMP_TUNE_GST_PIPELINE_ERROR | 80 | 1 | AAMP: Error from gstreamer pipeline |
| AAMP_TUNE_FAILED_PTS_ERROR | 80 | 2 | AAMP: Playback failed due to PTS error |

| AAMP_TUNE_PLAYBACK_STALLED | 7600 | 1 | AAMP: Playback was stalled due to lack of new fragments |
| AAMP_TUNE_FAILURE_UNKNOWN | 100 | 1 | AAMP: Unknown Failure |

---

## Global User Settings/Preferences Discovery

Firebolt getter APIs may be used to discover user settings/preferences related to caption styling and language preferences.  These are not managed internally by AAMP.

```
import { Accessibility } from '@firebolt-js/sdk'

let closedCaptionsSettings = await Accessibility.closedCaptionsSettings()
console.log(closedCaptionsSettings)
```

Response:
```
{
	"enabled": true,
	"styles": {
		"fontFamily": "monospaced_sanserif",
		"fontSize": 1,
		"fontColor": "#ffffff",
		"fontEdge": "none",
		"fontEdgeColor": "#7F7F7F",
		"fontOpacity": 100,
		"backgroundColor": "#000000",
		"backgroundOpacity": 100,
		"textAlign": "center",
		"textAlignVertical": "middle",
		"windowColor": "white",
		"windowOpacity": 50
	},
	"preferredLanguages": [
		"eng",
		"spa"
	]
}
```


## Inband (CEA608/708) Closed Caption Management (legacy XREReceiver API)
* on scaled X1 devices this is mapped directly to receiver APIs interacting with RDK CC Manager
    * here by default will inherit X1 caption style settings as set by user through guide settings
    * apps can override caption styling, but typically wouldn't need to do so
* on non-XRE devices this is implemented as a wrapper for backwards compatibility, but with limitations - only default styles will ever be applied, and with no way for app to change, and won't reflect guide settings

To use legacy XREReceiver inband closed captions, first register an event listener to discover decoder handle:
```
player.addEventListener("decoderAvailable", decoderHandleAvailable);
```
Along with corresponding event handler to publish the decoder handle to CC subsystem as follows:
```
function decoderHandleAvailable(event) {
console.log("decoderHandleAvailable " + event.decoderHandle);
XREReceiver.onEvent("onDecoderAvailable", { decoderHandle: event.decoderHandle });
}
```
Toggle CC display on or off at runtime:
```
XREReceiver.onEvent("onClosedCaptions", { enable: true });
XREReceiver.onEvent("onClosedCaptions", { enable: false });
```
Select CC track at runtime:
```
XREReceiver.onEvent("onClosedCaptions", { setTrack: trackID });
```
Set CC style options at runtime, using stringified JSON object detailing styling options.
```
XREReceiver.onEvent("onClosedCaptions", { setOptions: defaultCCOptions});
```
When closing stream, detach decoder handle:
```
XREReceiver.onEvent("onDecoderAvailable", { decoderHandle: null });
```

## Inband (CEA608/708) Closed Caption Management (modern UVE/AAMP API)

Whether AAMP manages CC visibility and styles directly depends on the platform.
On **X1 devices**, CC is owned by **XREReceiver**, which runs independently of AAMP.
`nativeCCRendering` must remain `false` (the default) so AAMP does not interfere with
XREReceiver's trickplay muting, parental control gating, or style management.
On **platforms without XREReceiver**, the app must set `nativeCCRendering: true` to
hand AAMP ownership of the CC lifecycle via `PlayerCCManager`. Apps must refrain from
using AAMP CC APIs when `nativeCCRendering` is set to false.

### Platform Summary

| Device Class | `nativeCCRendering` | `player.setTextStyleOptions` |
|---|---|---|
| X1 (XREReceiver present) | `false` (default — do not set to true) | Do **not** call AAMP CC APIs; XREReceiver owns CC visibility and styling and applies guide-configured styles automatically |
| Non-XRE but pre-ENT-OS | `true` (must be explicitly enabled) | Required to apply caption styling; guide settings are not automatically propagated |
| ENT-OS (with Text Track plugin) | `true` | Not required; Text Track plugin automatically maps guide-configured caption styling |

### Configuration

For non-XRE platforms, opt AAMP into managing CC before calling `load()`:
```js
player.initConfig( { nativeCCRendering: true } );
```

### Runtime CC Control

Toggle CC display on or off:
```js
player.setClosedCaptionStatus(true);  // show captions (off by default)
player.setClosedCaptionStatus(false); // hide captions
```

Get/Set CC track:
```js
player.getTextTrack();             // returns the numeric index of the currently selected text track
player.setTextTrack(trackIndex);   // set by numeric index (as returned by getTextTrack), or -1 to disable
```

Get/Set CC style options:
```js
player.getTextStyleOptions();      // returns a JSON-formatted string reflecting current styling options
player.setTextStyleOptions(options); // set styling options (see setTextStyleOptions API for format)
```

On newer devices there is no need to call `setTextStyleOptions`, as the Text Track plugin will automatically map guide-configured caption styling.

---

<div style="page-break-after: always;"></div>

## ATSC - Unified Video Engine Features
Support for ATSC-UVE is included from 3.0 version.
A subset of UVE APIs and Events are available when using UVE JS APIs for ATSC playback

### API Methods
##### load
- URI of the Media to be played by the Video Engine. Optional 2nd parameter.
- Examples for new URLs Supported:
    - live:///75 – ATSC Channel
    - hdmiin://localhost/deviceid/0  - HDMI Input
    - cvbsin://localhost/deviceid/0 - Composite Input
    - tune://atsc?frequency=5700000&serviceid=3 – Direct tune to ATSC Channel

##### play
- Start Playback / Resume playback.

##### stop
- Stop playback and free resources
- Optional forceCleanup parameter for DRM cleanup in Deep Sleep scenarios

##### getAudioTrack
- Get the index of the currently selected Audio track

##### setAudioTrack
- Set the index of the Audio track to be selected.
- Returns true if setAudioTrack has been performed.

##### setAudioTrack
- Set the Audio track to be selected by Language and Rendition.
- JSON formatted argument.
    - language
    - rendition
- Example:
```
{
        "language":"ger",
        "rendition":"commentary"
}
```

##### setAudioLanguage
- Set the language of the Audio track to be selected
- Returns true if setAudioLanguage has been performed.

##### setVideoRect
- Set display video rectangle coordinates. Default configuration (0,0,1280,720)
- Returns true if setVideoRect has been performed.

##### getAvailableAudioTracks
- Returns the available audio tracks information in JSON formatted list. Subset of parameters returned
    - name
    - language
    - codec
- Example:
```
{
        "name": "English (AC3)",
        "language":"eng",
        "codec":"AC3"
}
```

##### setClosedCaptionStatus
- Set the Closed Caption rendering to on/off.

##### getAvailableTextTracks
- Returns the available text track (CC) information in JSON formatted list.
- Subset of parameters returned
    - name
    - type
    - language
    - instreamId
- Example:
```
[
    {
        "name":"English (Closed Caption)",
        "type":"CLOSED-CAPTIONS",
        "language":"eng",
        ”instreamId":"CC1"
    },
    {
        "name":"Spanish (Closed Caption)",
        "type":"CLOSED-CAPTIONS",
        "language":"spa",
        "instreamId":"CC2"
    },
    {
        "name":"English (Closed Caption)",
        "type":"CLOSED-CAPTIONS",
        "language":"eng",
        "instreamId":"SERVICE1"
    },
    {
        "name":"Spanish (Closed Caption)",
        "type":"CLOSED-CAPTIONS",
        "language":"spa",
        "instreamId":"SERVICE2"
    }
]
 ```

##### getTextTrack
- Get the Index of the currently selected Text track.

##### setTextTrack
- Set the Index of the Text track to be selected.

##### getTextStyleOptions
- Returns the JSON formatted string of current ClosedCaption style options and values.

##### setTextStyleOptions
- Set the ClosedCaption style options to be used for rendering.

### updateManifest(manifest)
- Call to pass an updated live DASH manifest
- Returns true if processed without issue.

### New Set of APIs added for ATSC Parental Control  Settings

### disableContentRestrictions (until)
- Temporarily disable content restriction based on the control input provided by the ‘until’ parameter.
- Can be used for unlocking a locked channel (Channel locked due to Restrictions set)

**until {"time": < seconds>} Or {"programChange":true};**
- It is a Json Object.
- Provides control for automatic re-locking conditions.
    - If ‘time’ is set, the seconds will be considered as relative to current time until which the program will be unlocked.
    - If  ‘programChange’ is set,  the program will be unlocked, but re-enable restriction handling on next program boundary
    - If neither specified, parental control locking will be disabled until settop reboot, or explicit call to enableContentRestrictions().
    - If both specified, parental control locking will be re-enabled depending on which condition occurs first.

### enableContentRestrictions ()
- To re-enable parental control locks based on restrictions.

### Events Supported

##### playbackStarted

**Value:** 1
**Description:** Tune Success [OTA, HDMIIN, COMPOSITE IN]

##### playbackStateChanged

**Value:** 14
**Description:**
- Event when player state changes.
- Valid AAMP States  for ATSC OTA Playback: "idle":0, "initializing":1, "initialized":2, "preparing":3, "prepared":4,  "playing":8, "blocked":14
- Valid AAMP States for HDMIIN : "playing":8,“stopped”:10

##### blocked

**Value:** 38
**Description:**
- Blocked event is generated when player status switches to eSTATE BLOCKED.
    - Event Payload:
    - Type : reason – string to describe the reason for the blocked state
    - Type : locator - string to hold aamp url
    - Reason for restriction

- Example:
    “reason”:  (ATSC Playback)
    "STATUS|Low or No Signal"
    "Service Pin Locked"
    "STATUS|Unable to decode"
    "locator": (OTA url)
- If a program is Blocked due to Restrictions set by the Application, the ‘blocked’ event’s reason will be  "Service Pin Locked"
    “reason”: (HDMIIN)
    "NO_SIGNAL"
    "UNSTABLE_SIGNAL"
    "NOT_SUPPORTED_SIGNAL"


##### bitrateChanged

**Value:** 11
**Description:**
- Event notified when bitrate change happens. The event payload provides video stream info Will be notified after  first tuned event for OTA and and after display settings change.
- Event Payload:
    time: number,
    bitRate: number,
    description: string,
    width: number,
    height: number,
    framerate: number,
    position: number(not used),
    cappedProfile:number (not used),
    displayWidth:number (not used),
    displayHeight:number (not used),
    progressive:bool,
    aspectRatioWidth:number,
    aspectRatioHeight:number

### initConfig

| Property | Type | Default Value | Description |
| ---- | ---- | ---- | ---- |
| preferredAudioLanguage | String | en | ISO-639 audio language preference; for more than one language, provide comma delimited list from highest to lowest priority: ‘<HIGHEST>,<...>,<LOWEST>’ |
| nativeCCRendering | Boolean | False | Use native Closed Caption support in AAMP |

<div style="page-break-after: always;"></div>

## TSB Feature
AAMP supports two forms of local storage time shift buffer:
- AAMP Managed Local TSB
- FOG (To be deprecated)

### AAMP Managed Local TSB

In AAMP managed Local TSB, AAMP downloads and stores media fragments locally, allowing viewers to pause, rewind, and replay content within the configured buffer window. The buffer is maintained in local storage, with older content being removed as new content is added once the buffer reaches its maximum size.

### FOG
FOG is a device-resident RDK application that provides time shift buffer management. It runs a mongoose server on `http://127.0.0.1:9080/`. It intercepts live content by transforming original URLs into FOG template URLs, enabling TSB functionality. When requested, FOG generates and delivers a TSB manifest, handling all fragment downloading and buffer management according to configured parameters.

### Configuration Options

#### tsbType
- **Type**: String
- **Default**: "none"
- **Description**: Specifies the type of Time Shift Buffer to use.
  - `local`: FOG TSB if URL is FOG-formatted, otherwise AAMP's built-in TSB.
  - `cloud`: CDN/Server-side TSB used if available, no local buffering.
  - `none` : No TSB functionality, stream plays as-is.

#### tsbLength
- **Type**: Number
- **Default**: 1500
- **Description**: Maximum size of the Time Shift Buffer in seconds

#### Example Configuration

```js
// Enables AAMP-managed TSB of 15mins
player.initConfig({
    tsbType: "local",
    tsbSize: 900  // 15min TSB
});
player.load("https://cdn/manifest.mpd");
```

### TSB Playback Controls

With TSB enabled, the standard playback control APIs can be used to navigate within the buffered content:

- `seek(position)`: Navigate to a specific position within the TSB
- `seek(-1)`: Navigate to live play position
- `pause()`: Pause live content
- `play()`: Resume playback after pause or trick play
- `setRate(rate)`: Fast-forward or rewind within the TSB

### TSB Events

When using TSB, the `playbackProgressUpdate` event provides additional properties that are useful for implementing a scrub bar, provided `enableSeekableRange` init config is set to `True`:
- `startMiliseconds`: The earliest position available in the TSB
- `endMiliseconds`: The latest position available in the TSB (near live point)
- `durationMiliseconds`: Total duration of available content in the TSB
- `positionMiliseconds`: Current playback position within the TSB

Note: The spelling "Miliseconds" (instead of "Milliseconds") is intentional and matches the event implementation.

<div style="page-break-after: always;"></div>

## Appendix

### Live Pause Configuration
Player supports multiple pause exit behavior on live content based on the configuration set in "livePauseBehavior". Default value is 0 - Autoplay Immediately

    0 – Autoplay immediate (default) - Playback resumes when start of TSB/live reaches the paused position
    1 – Live immediate - Playback resumes immediately from the live point when start of TSB/live
    reaches the paused position
    2 – Autoplay defer - Player remains in Paused state when start of TSB/live reaches the paused position.
    When resumed, playback starts from start of the TSB.
    3 – Live defer - Player remains in Paused state when start of TSB/live reaches the paused position.
    When resumed, playback starts from live point


### Setup Reference Player
Procedure to setup the AAMP Reference Player in RDK devices(Comcast):
```markdown
1.  Host the ReferencePlayer folder in a web server.
2.  Use Comcast's IBIS tool to launch the reference player in the device:
        a. Under Launch HTML App, select Select a device to get started.
        b. From the list, find your device (it should be registered previously).
        c. Enter the ReferencePlayer URL in the URL field.
        d. Enter any name in the App name field.
        e. Click Launch.
```
### Folder Structure of Reference Player

- icons
- UVE
    * index.html
    * UVEMediaPlayer.js
    * UVEPlayerUI.js
    * UVERefPlayer.js
    * UVERefPlayerStyle.js
- index.html
- ReferencePlayer.js
- URLs.js
- ReferencePlayerStyle.css

---


### getPlaybackStatistics()
**Description:** Returns the playback statistics in JSON format from beginning of playback till the time API is called.

**JSON Description:**

```json
{
        "mediaType": {
            type: string,
            description: Stream type like HLS, DASH etc
        },
        "playbackMode": {
            type: string,
            description: Playback modes like linear, vod etc
        },
        "liveLatency": {
            type: integer,
            description:  The time from current position to end of seek range,
                        this field would only be available for Linear playback
            unit: milliseconds
        },
        "totalError": {
            type: integer,
            description: Total number of download errors so far (as of now, but will include other errors too in future)
                across profiles/manifests etc
        },
        "numOfGaps": {
            type: integer,
            description: total number of gaps between periods,
                        for Live this would be the number of gaps so far,
                        but for vod this would be the number of gaps across all periods,
                        this field would only be available for DASH
        },
        "timeInTopProfile": {
            type: integer,
            description: Duration of the media playback stayed in the top file profile from beginning of the playback,
            unit: seconds
        },
        "timeToTopProfile": {
            type: integer,
            description: time to reach top profile first time after tune. Provided initial tune bandwidth is not a top bandwidth
            unit: seconds
        },
        "duration": {
            type: integer,
            description: Duration of the playback so far,
            unit: seconds
        },
        "profileStepDown_Network": {
            type: integer,
            description: Number of profile step downs due to network issues,
        },
        "profileStepDown_Error": {
            type: integer,
            description: Number of profile step downs due to playback errors,
        },
        "displayWidth": {
            type: integer,
            description: display width,
        },
        "displayHeight": {
            type: integer,
            description: display height,
        },
        "profileCappingPresent": {
            type: integer,
            description: profile capping status,
        },
        "tsbAvailable": {
            type: integer,
            description: will be 1 if tsb is employed for the playback,
        },
        "languagesSupported": {
            type: object,
            description: lists the supported audio tracks. with name audio1, audio2 etc
             properties: {
                 "audio1": {
                    type: string,
                    description: language corresponds to audio1,
                }
            }
        },
        "main"/"audio1"/"audio2" etc/"video1"/"subtitle"/"iframe"/"ad_audio"/"ad_video"etc: {
            type: object,
            description: different track types ("main" to specify main manifest)
             properties: {
                "profiles": {
                    type: object,
                    description: for each profile in the given track type,
                     properties: {
                        "0/<profile number> etc": {
                            type: object,
                            description: details of a specific profile played, for the main/master manifest, this would be "0",
                             properties: {
                                "manifestStat": {
                                    type: object,
                                    description: For HLS, here it describes the stats of playlist manifest corresponds to each profile, for DASH this field be only available for main manifest,
                                     properties: {
                                        "latencyReport": {
                                           type: object,
                                            description: field indicating the latency of the download,
                                             properties: {
                                                "timeWindow_0/timeWindow_1 etc": {
                                                    type: integer,
                                                    description: gives the number of downloads on an item comes under 0th, 1st, 2nd bucket, with a bucket of duration 250ms
                                                }
                                            }
                                        },
                                        "sessionSummary": {
                                            type: object,
                                            description: Shows the session information so far (400/404,206), number of various error occurrences along with successful (200,206 etc) cases
                                             properties: {
                                                "200": {
                                                    type: integer,
                                                    description: number of http 200 cases
                                                }
                                            }
                                        },
                                        "info": {
                                            type: object,
                                            description: This field will only be present for for manifests. For linear DASH playback, this field will represent the details of the last refreshed manifest
                                             properties: {
                                                "downloadTimeMs": {
                                                    type: integer,
                                                    description: Manifest download time,
                                                    unit: milliseconds
                                                },
                                                "parseTimeMs": {
                                                    type: integer,
                                                    description: Manifest parse time, this field will only be available for main/master manifests,
                                                unit: Milliseconds
                                                },
                                                "size": {
                                                    type: integer,
                                                    description: Downloaded manifest size in bytes,
                                                unit: bytes
                                                },
                                                "periodCount": {
                                                    type: integer,
                                                    description: number of periods, only applicable for DASH
                                                }

                                            }
                                        }
                                    }
                                },
                                "fragmentStat": {
                                    type: object,
                                    description: Statistics of the downloaded fragments, would not be applicable for profile "0" (as is is for manifest),
                                     properties: {
                                        "media": {
                                            type: object
                                            description: Stats of media (non-init) fragments,
                                             properties: {
                                                "latencyReport": {
                                                    //similar as in manifest stats
                                                },
                                                "sessionSummary": {
                                                    //similar as in manifest stats
                                                }
                                            }
                                        }
                                        "init": {
                                            type: object
                                            description: Stats of init fragments,
                                             properties: {
                                                "latencyReport": {
                                                    //similar as in media
                                                },
                                                "sessionSummary": {
                                                    //similar as in media
                                                }
                                            }
                                        },
                                        "lastFailedUrl": {
                                            type: string,
                                            description: url of the last failed fragment ( could be normal fragment/ init  )
                                        },
                                        "width": {
                                            type: integer,
                                            description: video width specified for the particular bitrate (ony for the track type video),
                                        },
                                        "height": {
                                            type: integer,
                                            description: video height specified for the particular bitrate (ony for the track type video),
                                        }
                                    }
                                },
                                "licenseStat": {
                                    type: object,
                                    description: license related stats
                                    properties: {
                                        "totalClearToEncrypted": {
                                            type: integer,
                                            description: Total number of clear to encrypted content switches
                                        },
                                        "totalEncryptedToClear": {
                                            type: string,
                                           description: Total number encrypted to clear content switches
                                        }
                                    }
                                },

                            }
                        }
                    }
                }
            }
        },
        "version": {
            type: string,
            description: Version of this document
        },
        "creationTime": {
            type: string,
            description: UTC timestamp of this document creation
        }
}
```

##### Example:

DASH VOD:

```json
  getPlaybackStatistics Return Val:{
      "timeToTopProfile":8,
      "timeInTopProfile":104,
      "duration":112,
      "profileStepDown_Network":0,
      "mediaType":"DASH",
      "playbackMode":"VOD",
      "totalError":0,
      "numOfGaps":0,
      "languageSupported":{"audio1":"en"},
      "main":{"profiles":{"0":{"manifestStat":{"latencyReport":{"timeWindow_1":1},"sessionSummary":{"200":1},
      "info":{"DownloadTimeMs":255,"ParseTimeMs":1,"PeriodCount":1,"Size":2012}}}}},
      "video":{"profiles":{"2400000":{"fragmentStat":{"media":{"latencyReport":{"timeWindow_1":1,"timeWindow_2":1},
      "sessionSummary":{"200":2}},"init":{"latencyReport":{"timeWindow_0":1},"sessionSummary":{"200":1}}},
      "width":1280,"height":720},"4800000":{“fragmentStat":{"media":{"latencyReport":{"timeWindow_2":5,"timeWindow_3":10,
      "timeWindow_4":9,"timeWindow_5":2},"sessionSummary":{"200":26}},
      "init":{"latencyReport":{"timeWindow_0":1},"sessionSummary":{"200":1}}},"width":1920,"height":1080}}},
      "audio1":{"profiles":{"128000":{"fragmentStat":{"media":{"latencyReport":{"timeWindow_0":23,"timeWindow_1":6,"timeWindow_2":1},
      "sessionSummary":{"200":30}},"init":{"latencyReport":{"timeWindow_1":1},"sessionSummary":{"200":1}}}}}},
      "version":"2.0","creationTime":"2023-05-17.20:15:01"}
```

HLS VOD:

```json
  getPlaybackStatistics Return Val:{
      "timeToTopProfile":6,
      "timeInTopProfile":330,
      "duration":336,
      "profileStepDown_Network":0,
      "mediaType":"HLS",
      "playbackMode":"VOD",
      "totalError":0,
      "languageSupported":{"audio1":"eng","audio2":"fra","audio3":"ger","audio4":"pol","audio5":"spa"},
      "main":{"profiles":{"0":{"manifestStat":{"latencyReport":{"timeWindow_1":1},"sessionSummary":{"200":1},
      "info":{"DownloadTimeMs":415,"ParseTimeMs":0,"Size":1746}}}}},
      "video":{"profiles":{"1400000":{"manifestStat":{"latencyReport":{"timeWindow_0":1},
      "sessionSummary":{"200":1},"info":{"DownloadTimeMs":46,"Size":13613}},
      "fragmentStat":{"media":{"latencyReport":{"timeWindow_0":2,"timeWindow_1":1},
      "sessionSummary":{"200":3}}},"width":842,"height":480},
      "5000000":{"manifestStat":{"latencyReport":{"timeWindow_0":1},"sessionSummary":{"200":1},
      "info":{"DownloadTimeMs":68,"Size":14063}},
      "fragmentStat":{"media":{"latencyReport":{"timeWindow_0":164,"timeWindow_1":1},"sessionSummary":{"200":165}}},"width":1920,"height":1080}}},
      "audio1":{"profiles":{"0":{"manifestStat":{"latencyReport":{"timeWindow_0":1},"sessionSummary":{"200":1},
      "info":{"DownloadTimeMs":39,"Size":12631}},"fragmentStat":{"media":{"latencyReport":{"timeWindow_0":72,"timeWindow_1":1},
      "sessionSummary":{"200":73}}}}}},"version":"2.0","creationTime":"2023-05-18.03:44:31"}
```

DASH Linear:

```json
  "getPlaybackStatistics","returnType":"string"

   getPlaybackStatistics Return Val:{
       "timeInTopProfile":20,
       "duration":20,
       "profileStepDown_Network":0,
       "mediaType":"DASH",
       "playbackMode":"LINEAR_TV",
       "liveLatency":2147483647,
       "totalError”:0,
       ”numOfGaps”:0,
       ”languageSupported":{"audio1":"en"},
       "main":{"profiles":{"0":{"manifestStat":{"latencyReport":{"timeWindow_0":2},"sessionSummary":{"200":2},
       "info":{"DownloadTimeMs":70,"ParseTimeMs":1,"PeriodCount":1,"Size":1955}}}}},
       "video":{"profiles":{"300000":{"fragmentStat":{"media":{"latencyReport":{"timeWindow_0":10},
       "sessionSummary":{"200":10}},"init":{"latencyReport":{"timeWindow_0":1},
       "sessionSummary":{"200":1}}},"width":640,"height":360}}},
       "audio1":{"profiles":{"48000":{"fragmentStat":{"media":{"latencyReport":{"timeWindow_0":10},"sessionSummary":{"200":10}},
       "init":{"latencyReport":{"timeWindow_0":1},"sessionSummary":{"200":1}}}}}},
       "version":"2.0","creationTime":"2023-05-18.21:06:23"}
```

HLS Linear:

```json
  getPlaybackStatistics Return Val:{
      "timeToTopProfile":6,
      "timeInTopProfile":30,
      "duration":36,
      "profileStepDown_Network":0,
      "mediaType":"HLS",
      "playbackMode":"LINEAR_TV",
      "liveLatency":18138,
      "totalError":0,
      "main":{"profiles":{"0":{"manifestStat":{"latencyReport":{"timeWindow_0":1},
      "sessionSummary":{"200":1},"info":{"DownloadTimeMs":149,"ParseTimeMs":0,"Size":1379}}}}},
      "video":{"profiles":{"2305600":{"manifestStat":{"latencyReport":{"timeWindow_0":1},"sessionSummary":{"200":1},
      "info":{"DownloadTimeMs":149,"Size":997}},"fragmentStat":{"media":{"latencyReport":{"timeWindow_0":1},
      "sessionSummary":{"200":1}}},"width":960,"height":540},"6705600":{"manifestStat":{"latencyReport":{"timeWindow_0":3},
      "sessionSummary":{"200":3},"info":{"DownloadTimeMs":35,"Size":997}},
      "fragmentStat":{"media":{"latencyReport":{"timeWindow_1":5},"sessionSummary":{"200":5}}},
      "width":1920,"height":1080}}},
      "version":"2.0","creationTime":"2023-05-18.23:07:08"}
```

### Diagnostics Overlay Configuration

Sample code utilizing showDiagnosticsOverlay configuration to display additional data on UI.

```js
<!DOCTYPE html>
<head>
<meta content="text/html" charset="utf-8" http-equiv="content-type">
<style>
    .urlModal {
        display: none;
        position: absolute;
        background-color: rgba(0,0,0,0.8);
        border: 1.2px solid lightgrey;
        box-shadow: 0 4px 8px 0 rgba(0,0,0,0.2),0 6px 20px 0 rgba(0,0,0,0.19);
        width: 90%;
        top: 5%;
        left: 5%;
        padding: 5px;
        word-break: break-all;
        font: 13px arial, sans-serif;
        color: white
    }
    .overlayModal {
        display: none;
        position: absolute;
        background-color: rgba(0,0,0,0.8);
        border: 1.2px solid lightgrey;
        box-shadow: 0 4px 8px 0 rgba(0,0,0,0.2),0 6px 20px 0 rgba(0,0,0,0.19);
        top: 12%;
        left: 5%;
        padding: 7px;
        font: 14px arial, sans-serif;
        color: white
    }
    ul#bitrateList {
        padding-inline-start: 0;
    }
    ul#bitrateList li {
        display: inline;
        margin-left: 5px;
    }
    .current-bitrate-style {
        border: 2px solid #FFFB55;
        border-radius: 5px;
        display: inline;
        padding: 5px;
        margin-left: 5px;
    }
    .anomalyModal {
        display: none;
        position: absolute;
        background-color: rgba(0,0,0,0.9);
        border: 1.2px solid lightgrey;
        box-shadow: 0 4px 8px 0 rgba(0,0,0,0.2),0 6px 20px 0 rgba(0,0,0,0.19);
        width: 90%;
        top: 25%;
        left: 5%;
        padding: 5px;
        word-break: break-all;
        font: 14px arial, sans-serif;
        color: white
    }
    .red-style {
        color: #FF5953;
    }
    .orange-style {
        color: #ffaf0e;
    }
    .white-style {
        color: #f7f7f7;
    }
</style>
</head>

<body>
    <!-- The URL Modal -->
    <div id="urlModal" class="urlModal">
        <p id="url"></p>
    </div>
    <!-- The Overlay Modal -->
    <div id="overlayModal" class="overlayModal">
        <p id="type"></p>
        <div>
            <ul id="bitrateList">
                <li>Bitrates(Mbps): </li>
            </ul>
        </div>
    </div>
    <!-- The Anomaly Modal -->
    <div id="anomalyModal" class="anomalyModal">
        <ul id="anomalyList"></ul>
    </div>
<script>
    // Sample data for testing
    var anomalyDescriptionList = [];
    var anomalySeverityList = [];
    var overlayConfig;
    var overlayObject = {
        contentURL: "http://localhost:50050/content/main.m3u8",
        appVersion: "1.0",
        appURL: "http://localhost:50050/app",
        manifestType: "HLS",
        drmType: "PlayReady",
        contentType: "VOD",
        bitrates: "1000000,2000000,3000000",
        currentBitrate: "2000000"
    };

    function renderOverlay(paramsString) {
        // If OFF, do not render overlay
        if (overlayConfig === 0) {
            return;
        }
        document.getElementById('overlayModal').style.display = 'block';
        document.getElementById('urlModal').style.display = 'block';

        document.getElementById("url").innerHTML = "APP " + paramsString.appVersion + " | " +  paramsString.appURL + " | CONTENT: " +  paramsString.contentURL;
        document.getElementById("type").innerHTML = "AAMP | " + paramsString.contentType + " | " +  paramsString.manifestType + " | " +  paramsString.drmType;

        //parse the available bitrates
        var availableBitrates =  paramsString.bitrates.split(',');
        availableBitrates.forEach(function(bitrate,index) {
            availableBitrates[index] = convertToMbps(bitrate);
        });

        document.getElementById('bitrateList').innerHTML = "";
        // Add the available bitrates to the list
        var ul = document.getElementById('bitrateList');
        availableBitrates.forEach(function(bitrate) {
            // Create a new list item and attach it to ul
            li = document.createElement('li');
            li.appendChild(document.createTextNode(bitrate));
            if (bitrate === convertToMbps(paramsString.currentBitrate)) {
                // Highlight the currentbitrate
                li.classList.add("current-bitrate-style");
            }
            ul.appendChild(li);
        });
    }

    // Function to convert bitrate into Mbps
    function convertToMbps(bitrate) {
        return (bitrate / 1000000).toFixed(1);
    }

    function renderAnomaly(anomalyDescriptionList, anomalySeverityList) {
        // If not ALL, do not render anomaly overlay
        if (overlayConfig === 0 || overlayConfig === 1) {
            return;
        }
        document.getElementById('anomalyModal').style.display = 'block';
        var ul = document.getElementById('anomalyList');
        ul.innerHTML = "";
        // Add the anomaly strings to the list
        anomalyDescriptionList.forEach(function(description,index) {
            // Create a new list item and attach it to ul
            li = document.createElement('li');
            li.appendChild(document.createTextNode(description));
            // attach text color according to severity
            console.log("anomalySeverityList[index] = " + anomalySeverityList[index]);
            switch(Number(anomalySeverityList[index])) {
                case 0: li.classList.add("red-style");
                    break;
                case 1: li.classList.add("orange-style");
                    break;
                case 2: li.classList.add("white-style");
                    break;
            }
            ul.appendChild(li);
        });
    }

    function onAnomalyReport(event) {
        const tuneRegExp = /Tune attempt#[0-9]+.\s(\w+):\w+=\w+:(\w+)\/(\w+)\sURL:(.*)/;
        const tuneAttempt = tuneRegExp.exec(event.description);
        if (tuneAttempt) {
            overlayObject.contentType = tuneAttempt[1];
            overlayObject.manifestType = tuneAttempt[2];
            overlayObject.drmType = tuneAttempt[3];
            overlayObject.contentURL = tuneAttempt[4];
        }
        renderOverlay(overlayObject);
        anomalyDescriptionList.push(event.description);
        anomalySeverityList.push(event.severity);
        renderAnomaly(anomalyDescriptionList, anomalySeverityList);
    }

    function onMediaMetadata(event) {
        overlayObject.bitrates = event.bitrates;
        overlayObject.drmType = event.DRM;
        renderOverlay(overlayObject);
    }

    window.onload = function() {
        overlayObject.appURL = window.location.href;
        var player = new AAMPMediaPlayer();
        overlayConfig = JSON.parse(player.getConfiguration()).showDiagnosticsOverlay;
        player.addEventListener("anomalyReport", onAnomalyReport);
        player.addEventListener("mediaMetadata", onMediaMetadata);
        renderOverlay(overlayObject);
        onAnomalyReport({
            description: "Tune attempt#1. LINEAR:TSB=false:DASH/Widevine URL:https://localhost:50050/content/main.mpd",
            severity: 0
        });
    }
</script>
</body>
<html>
```
---

## Release Versions

**Version:** 0.7
**Release Notes:**
Initial draft of UVE APIs implemented

**Version:** 0.8
**Release Notes:**
CDAI support, configuration options for tune optimization
- API:
    - setAlternateContent
    - notifyReservationCompletion
    - addCustomHTTPHeader
- Configuration:
    - stereoOnly
    - bulkTimedMetadata
    - useWesterosSink
    - parallelPlaylistDownload
- Events:
    - bufferingChanged
    - timedMetadata
    - adResolved
    - reservationStart
    - reservationEnd
    - placementStart
    - placementEnd
    - placementProgress
    - placementError
    - manifestRefreshNotify
    - tuneMetricsData

**Version:** 0.9
**Release Notes:**
"Player Switching" Feature
- load (autoplay=false support)
- detach() method

**Version:** 1.0
**Release Notes:**
Added support to get available audio track and closed captioning info
- API:
    - getAvailableAudioTracks
    - getAvailableTextTracks
- Configuration:
    - playlistTimeout
    - parallelPlaylistRefresh
    - useAverageBandwidth
    - preCachePlaylistTime
    - progressReportingInterval
    - useRetuneForUnpairedDiscontinuity
    - drmDecryptFailThreshold

**Version:** 2.4
**Release Notes:**
April 2020
- Configuration
    - initialBuffer
    - useMatchingBaseUrl
    - initFragmentRetryCount
- Event Notification

**Version:** 2.6
**Release Notes:**
June 2020
Seek while paused, get/set audio and text track supported
- API:
    - getAudioTrack
    - setAudioTrack
    - getTextTrack
    - setTextTrack
    - setClosedCaptionStatus
    - setTextStyleOptions
    - getTextStyleOptions
- Configuration:
    - nativeCCRendering
    - langCodePreference
    - descriptiveTrackName

**Version:** 2.7
**Release Notes:**
Aug 2020
- Configuration
    - Deprecated useWesterosSink

**Version:** 2.9
**Release Notes:**
Sept 2020
- Configuration
    - authToken
    - useRetuneForGstInternalError

**Version:** 3
**Release Notes:**
Oct 2020
- Updated getAvailableAudioTracks / getAvailableTextTracks
- API:
    - setAudioLanguage
- Configuration:
    - propagateUriParameters
    - reportVideoPTS
ATSC – UVE Features Added .

**Version:** 3.1
**Release Notes:**
Jan 2021
ATSC New APIs / Events
- API:
    - getAvailableThumbnailTracks
    - setThumbnailTrack
    - getThumbnail
- Configuration:
    - sslVerifyPeer
    - persistBitrateOverSeek
    - setLicenseCaching
    - maxPlaylistCacheSize
    - enableSeekableRange

**Version:** 3.2
**Release Notes:**
Mar 2021
- API
    - setPreferredAudioLanguage
    - setAudioTrack
- Configuration:
    - livePauseBehavior
    - limitResolution

**Version:** 3.3
**Release Notes:**
May 2021
- Configuration:
    - useAbsoluteTimeline
    - asyncTune
- Events :
    - Updated bitrateChanged for ATSC

**Version:** 3.4
**Release Notes:**
- Events :
    - audioTracksChanged
    - textTracksChanged
    - seeked
    - vttCueDataListener
    - id3Metadata

**Version:** 3.5
**Release Notes:**
Aug 2021
- API
    - load (updated)
    - setPreferredAudioLanguage (updated)
    - getAvailableAudioTracks (updated)
    - getAvailableTextTracks (updated)
    - downloadBuffer default value(updated)
- Events :
    - id3Metadata

**Version:** 3.6
**Release Notes:**
Sep 2021
- Configuration
    - disable4K
    - sharedSSL
    - preferredAudioRendition
    - preferredAudioCodec
- Events:
    - mediaMetadata (updated)

**Version:** 4.1
**Release Notes:**
Jan 2022
- API
    - subscribeResponseHeaders
- Configuration
    - supportTLS
    - maxInitFragCachePerTrack
    - fragmentDownloadFailThreshold
    - tsbInterruptHandling
    - sslVerifyPeer (updated)
- Events:
     - AAMP_TUNE_UNSUPPORTED_AUDIO_TYPE (updated error code)
     - AAMP_TUNE_UNSUPPORTED_STREAM_TYPE (updated error code)
     - AAMP_EVENT_CONTENT_GAP
     - AAMP_EVENT_HTTP_RESPONSE_HEADER

**Version:** 4.2
**Release Notes:**
Feb 2022
- API
    - getAudioTrackInfo
    - getPreferredAudioProperties
- Configuration
     - Updated asyncTune default state to True
     - useSecManager
- Events:
    - AAMP_EVENT_WATERMARK_SESSION_UPDATE

**Version:** 4.3
**Release Notes:**
Mar 2022
- API
    - getPlaybackStatistics
- Configuration
    - customLicenseData
    - Updated asyncTune default state to False

**Version:** 4.4
**Release Notes:**
Apr 2022
Support for AC4 Audio
- API
    - getAvailableVideoTracks
    - setVideoTracks
- Configuration
    - disableAC4
    - asyncTune default state to True•

**Version:** 4.5
**Release Notes:**
May 2022
- API
    - setPreferredTextLanguage
    - getTextTrackInfo
    - getPreferredTextProperties
    - setPreferredAudioLanguage ( updated)
- Configuration
    - persistProfileAcrossTune

**Version:** 4.6
**Release Notes:**
Jun 2022
- API
- Configuration
    - preferredAudioRendition
    - preferredAudioCodec
    - preferredAudioLabel
    - preferredAudioType

**Version:** 4.12
**Release Notes:**
Dec 2022
- API
- Configuration
    - persistHighNetworkBandwidth
    - persistLowNetworkBandwidth
    - customHeaderLicense

**Version:** 5.1
**Release Notes:**
Jan 2023
- API
    - setContentProtectionDataConfig
    - setContentProtectionDataUpdateTimeout
- Configuration
    - configRuntimeDRM

**Version:** 5.3
**Release Notes:**
Mar 2023
- Configuration
    - enableCMCD
- Event
    - playbackProgressUpdate ( updated for new field )

**Version:** 5.6
**Release Notes:**
Jun 2023
- API
    - getPlaybackStatistics ( updated example )

**Version:** 5.7
**Release Notes:**
Jul 2023
- API
    - getVideoPlaybackQuality

**Version:** 5.9
**Release Notes:**
Sep 2023
- API
    - resetConfiguration
    - getConfiguration

**Version:** 5.10
**Release Notes:**
Oct 2023
- Events:
    - tuneMetricsData(Updated the tune metric info)

**Version:** 5.11
**Release Notes:**
Nov 2023
- Configuration:
    - telemetryInterval

**Version:** 5.12
**Release Notes:**
Dec 2023
- Configuration:
    - useSinglePipeline
    - sendUserAgentInLicense
    - mpdStichingSupport
    - enablePTSReStamp
    - subtitleClockSyncInterval

**Version:** 6.7
**Release Notes:**
Aug 2024
- API
    - getAvailableAudioTracks ( updated example and added missing property )
    - setPreferredTextLanguage ( missing information added )
    - getTextTrackInfo ( missing information added )

**Version:** 6.12
**Release Notes:**
- Configuration:
    - bulkTimedMetadataLive

**Version:** 7.05
**Release Notes:**
- Configuration:
    - lldUrlKeyword ( deprecated )
    - wifiCurlHeader ( default value changed to false )
    - enableMediaProcessor ( default value changed to true )
    - enablePTSRestampForHlsTs
    - monitorAV
    - monitorAVSyncThreshold
    - monitorAVJumpThreshold
    - progressLoggingDivisor
    - showDiagnosticsOverlay ( added example in Appendix )
    - monitorAVReportingInterval

**Version:** 7.07
**Release Notes:**
- Events:
    - Audio buffer added to playbackProgressUpdate
- [TSB Feature](#tsb-feature) documentation
