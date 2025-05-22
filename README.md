# ![](images/logo.png) <br/> AAMP / Universal Video Engine (UVE)


### Advanced Adaptive Media Player (AAMP)
AAMP is an open source native video engine that is built on top of GStreamer and optimized for performance, memory use, and code size.  


Index 
---
1. [AAMP Source Overview](#aamp-source-overview)
2. [AAMP Configuration](#aamp-configuration)
3. [Channel Override Settings](#channel-override-settings)
4. [Westeros Settings](#westeros-settings)
5. [AAMP Tunetime](#aamp-tunetime) 
6. [VideoEnd (Session Statistics) Event](#videoend-session-statistics-event) 
7. [UVE Reference Document](AAMP-UVE-API.md)
8. [AAMP Simulator Installation](#aamp-simulator-installation)
---

# AAMP Source Overview:

aampcli.cpp
- entry point for command line test app

aampgstplayer.cpp
- gstreamer abstraction - allows playback of unencrypted video fragments

base16, _base64
- utility functions

fragmentcollector_hls
- hls playlist parsing and fragment collection

fragmentcollector_mpd
- dash manifest parsing and fragment collection

fragmentcollector_progressive
- raw mp4 playback support

drm
- digital rights management support and plugins

---
# AAMP Configuration

AAMP Configuration can be set with different method . Below is the list (from 
lowest priority to highest priority override ownership).
	a) AAMP Default Settings within Code 
	b) AAMP Configuration from Operator ( RFC / ENV variables )
	c) AAMP Settings from Stream 
	d) AAMP Settings from Application settings 
	e) AAMP Settings from Dev configuration ( /opt/aamp.cfg - text format  , /opt/aampcfg.json - JSON format input)

Configuration Field						Description	
---
On / OFF Switches : All Enable/Disable configuration needs true/false input .
Example : abr=false -> to disable ABR 

```
abr				Enable/Disable adaptive bitrate logic. Default: true
fog				Enable/Disable Fog. Default: true
parallelPlaylistDownload	Enable parallel fetching of audio & video playlists for HLS during Tune. Default: false
parallelPlaylistRefresh		Enable parallel fetching of audio & video playlists for HLS during refresh. Default: true
preFetchIframePlaylist		Enable prefetching of I-Frame playlist. Default: false
preservePipeline		Flush instead of teardown. Default: false
demuxHlsAudioTrack		Demux Audio track from HLS transport stream. Default: true
demuxHlsVideoTrack		Demux Video track from HLS transport stream. Default: true
demuxHlsVideoTrackTrickMode	Demux Video track from HLS transport stream during TrickMode. Default: true
throttle			Regulate output data flow,used with restamping. Default: false
demuxAudioBeforeVideo		Demux video track from HLS transport stream track mode. Default: false
stereoOnly			Enable selection of stereo only audio. Overrides disableEC3/disableATMOS. Default: false
disableEC3			Disable DDPlus. Default: false
disableATMOS			Disable Dolby ATMOS. Default: false
disable4K			Disable 4K Profile playback in 4K Supported device. Default: false
disablePlaylistIndexEvent	Disables generation of playlist indexed event by AAMP on tune/trickplay/seek. Default: true
enableSubscribedTags		Enable subscribed tags. Default: true
dashIgnoreBaseUrlIfSlash	Ignore the constructed URI of DASH. Default: false
licenseAnonymousRequest		Acquire license without token. Default: false
hlsAVTrackSyncUsingPDT		Use EXT-X-PROGRAM-DATE to synchronize audio and video playlists. Default: false
mpdDiscontinuityHandling	Discontinuity handling during MPD period transition. Default: true
mpdDiscontinuityHandlingCdvr	Discontinuity handling during MPD period transition for cDvr. Default: true
forceHttp			Allow forcing of HTTP protocol for HTTPS URLs . Default: false
internalRetune			Internal reTune logic on underflows/ pts errors. Default: true
audioOnlyPlayback		Audio only Playback . Default: false
gstBufferAndPlay		Pre-buffering which ensures minimum buffering is done before pipeline play. Default: true
bulkTimedMetadata		Report timed Metadata as single bulk event. Default: false
asyncTune			Asynchronous API / Event handling for UI. Default: false
useWesterosSink			Enable/Disable westeros sink based video decoding. 
useNewABR			Enable/Disable New buffer based hybrid ABR . Default: true (enables useNewAdBreaker & PDT based A/V sync)
useNewAdBreaker			Enable/Disable New discontinuity processing based on PDT. Default: false
useAverageBandwidth		Enable/Disable use of average bandwidth in manifest for ABR instead of Bandwidth attribute. Default: false
useRetuneForUnpairedDiscontinuity	Enable/Disable internal retune on unpaired discontinuity. Default: true
useMatchingBaseUrl		Enable/Disable use of matching base url, whenever there are multiple base urls are available. Default: false
nativeCCRendering		Enable/Disable Native CC rendering in AAMP Player. Default: false
enableVideoRectangle		Enable/Disable Setting of rectangle property for sink element. Default: true
useRetuneForGstInternalError	Enable/Disable Retune on GST Error. Default: true
enableSeekableRange		Enable/Disable Seekable range reporting in progress event for non-fog content. Default: false
reportVideoPTS 			Enable/Disable video pts reporting in progress events. Default: false
propagateUriParameters		Enable/Disable top-level manifest URI parameters while downloading fragments.  Default: true
sslVerifyPeer			Enable/Disable SSL Peer verification for curl connection . Default: false
setLicenseCaching		Enable/Disable license caching in WV . Default: true
persistBitrateOverSeek		Enable/Disable ABR profile persistence during Seek/Trickplay/Audio switching. Default: false
fragmp4LicensePrefetch		Enable/Disable fragment mp4 license prefetching. Default: true
gstPositionQueryEnable		GStreamer position query will be used for progress report events. Default: true
playreadyOutputProtection  	Enable/Disable HDCP output protection for DASH-PlayReady playback. Default: false
enableVideoEndEvent		Enable/Disable Video End event generation; Default: true
playreadyOutputProtection	Enable/Disable output protection for PlayReady DRM. Default: false
descriptiveAudioTrack   	Enable/Disable role in audio track selection.syntax <langcode>-<role> instead of just <langcode>. Default: false
decoderUnavailableStrict	Reports decoder unavailable GST Warning as aamp error. Default: false
retuneOnBufferingTimeout 	Enable/Disable internal re-tune on buffering time-out. Default: true
client-dai			Enable/Disable Client-DAI. Default: false
cdnAdsOnly			Enable/Disable picking Ads from Fog or CDN . Default: false
appSrcForProgressivePlayback 	Enables appsrc for playing progressive AV type. Default: false
seekMidFragment			Enable/Disable Mid-Fragment seek. Default: false
wifiCurlHeader			Enable/Disable wifi custom curl header inclusion. Default: true
reportBufferEvent		Enables Buffer event reporting. Default: true.
info            		Enable/Disable logging of requested urls. Default: false
gst             		Enable/Disable gstreamer logging including pipeline dump. Default: false
gstlevel                String to set (final) override of gstreamer debug level, e.g. gstlevel=*:3,westeros*:5
progress        		Enable/Disable periodic logging of position. Default: false
trace           		Enable/Disable dumps of manifests. Default: false
curl            		Enable/Disable verbose curl logging for manifest/playlist/segment downloads. Default: false
curlLicense     		Enable/Disable verbose curl logging for license request (non-secclient). Default: false
debug           		Enable/Disable debug level logs. Default: false
logMetadata     		Enable/Disable timed metadata logging. Default: false
dashParallelFragDownload	Enable/Disable dash fragment parallel download. Default: true
enableAccessAttributes		Enable/Disable Usage of Access Attributes in VSS. Default: true 
subtecSubtitle			Enable/Disable subtec-based subtitles. Default: false
webVttNative			Enable/Disable Native WebVTT processing. Default: false
failover			Enable/Disable failover logging. Default: false
curlHeader			Enable/Disable curl header response logging on curl errors. Default: false
stream				Enable/Disable HLS Playlist content logging. Default: false
isPreferredDRMConfigured	Check whether preferred DRM has set. Default: false
limitResolution			Check if display resolution based profile selection to be done. Default: false
disableUnderflow		Enable/Disable Underflow processing. Default: false
useAbsoluteTimeline		Enable Report Progress report position based on Availability Start Time. Default: false
id3				Enable/Disable ID3 tag. Default: false
repairIframes			Enable/Disable iframe fragment repair (stripping and box adjustment) for HLS mp4 when whole file is received for ranged request. Default: false
sharedSSL			Enabled/Disable curl shared SSL session. Default: true
enableLowLatencyDash		Enable/Disable Low Latency Dash. Default: false
disableLowLatencyMonitor	Enable/Disable Low Latency Monitor. Default: true
disableLowLatencyABR		Enable/Disable Low Latency ABR. Default: true
enableLowLatencyCorrection	Enable/Disable Low Latency Correction. Default: false
enableFogConfig			Enable/Disable setting player configurations to Fog. Default: true
suppressDecode			Enable/Disable setting to suppress decode of content for playback, only Downloader test. Default: false
gstSubtecEnabled		Enable/Disable subtec via gstreamer plugins (plugins in gst-plugins-rdk-aamp repo)
sendLicenseResponseHeaders	Enable/Disable Sending License response header as a part of DRMMetadata event(Non SecClient/SecManager DRM license).
useTCPServerSink		Enable "tcpserverSink" in conjunction with playbin. For use in automated testing when there is no window for video output
sendUserAgentInLicense		Enable/disable sending user agent in the DRM license request header. Default: disabled.
useSinglePipeline		Enable/Disable using single gstreamer pipeline for main and secondary assets
earlyProcessing			Enable/Disable processing fragments on download to extract ID3 metadata
useRialtoSink              Enable/Disable player to use Rialto sink based video and audio pipeline

// Integer inputs
ptsErrorThreshold		aamp maximum number of back-to-back pts errors to be considered for triggering a retune
waitTimeBeforeRetryHttp5xx 	Specify the wait time before retry for 5xx http errors. Default: 1s.
harvestCountLimit		Specify the limit of number of files to be harvested
harvestDuration			Specify the time limit of files to be harvested in seconds
harvestConfig			*Specify the value to indicate the type of file to be harvested. Refer table below for masking table 
bufferHealthMonitorDelay 	Override for buffer health monitor start delay after tune/ seek (in secs)
bufferHealthMonitorInterval	Override for buffer health monitor interval(in secs)
abrCacheLife 			Lifetime value (ms) for abr cache  for network bandwidth calculation. Default: 5000ms
abrCacheLength  		Length of abr cache for network bandwidth calculation (# of segments. Default 3
abrCacheOutlier 		Outlier difference which will be ignored from network bandwidth calculation. Default: 5MB (in bytes)
abrNwConsistency		Number of checks before profile increment/decrement by 1.This is to avoid frequent profile switching with network change: Default 2
abrSkipDuration			Minimum duration of fragment to be downloaded before triggering abr. Default: 6s
progressReportingInterval	Interval (seconds) for progress reporting(in seconds. Default: 1
licenseRetryWaitTime		License retry wait (ms) interval. Default: 500
licenseKeyAcquireWaitTime	License key acquire wait time (ms). Default: 5000
liveOffset    			live offset time in seconds, Live playback this much time before true live edge. Default: 1
timeBasedBufferSeconds  time in seconds to buffer ahead of the current gst play position.  setting this value to <=0 disables time based buffering and byte based buffering is used instead.
cdvrLiveOffset    		live offset time in seconds for cdvr, aamp starts live playback this much time before the live point for inprogress cdvr. Default: 30s
tuneEventConfig 		Send streamplaying for live/VOD when 
					0 - playlist acquired 
					1 - first fragment decrypted
					2 - first frame visible (default)
preferredDrm			Preferred DRM for playback  
					0 - No DRM 
					1 - Widevine
					2 - PlayReady ( Default)
					3 - Consec
					4 - AdobeAccess
					5 - Vanilla AES
					6 - ClearKey
ceaFormat			Preferred CEA option for CC. Default stream based . Override value 
					0 - CEA 608
					1 - CEA 708
maxPlaylistCacheSize            Max Size of Cache to store the VOD Manifest/playlist . Size in KBytes. Default: 3072.
initRampdownLimit		Maximum number of rampdown/retries for initial playlist retrieval at tune/seek time. Default: 0 (disabled).
downloadBuffer                  Fragment cache length: Default 3 fragments
vodTrickPlayFps		        Specify the framerate for VOD trickplay. Default: 4
linearTrickPlayFps      	Specify the framerate for Linear trickplay. Default: 8
fragmentRetryLimit		Set fragment rampdown/retry limit for video fragment failure. Default: -1
initRampdownLimit		Maximum number of rampdown/retries for initial playlist retrieval at tune/seek time. Default: 0 (disabled).
initFragmentRetryCount	    	Max retry attempts for init frag curl timeout failures. Default: 1 (extra retry after failed download)
langCodePreference		preferred format for normalizing language code. Default: 0
initialBuffer			cached duration before playback start, in seconds. Default: 0
maxTimeoutForSourceSetup	Timeout value wait for GStreamer appsource setup to complete. Default: 1000
drmDecryptFailThreshold		Retry count on drm decryption failure. Default: 10
segmentInjectFailThreshold	Retry count for segment injection discard/failure. Default: 10
preCachePlaylistTime		Max time to complete PreCaching. Default: 0min
thresholdSizeABR		ABR threshold size. Default: 6000
stallTimeout			Stall detection timeout. Default: 10s
stallErrorCode			Stall error code. Default: 7600
minABRBufferRampdown		Minimum ABR Buffer for Rampdown. Default: 10s
maxABRBufferRampup		Maximum ABR Buffer for Rampup. Default: 15s
preplayBuffercount		Count of segments to be downloaded until play state. Default: 2
downloadDelay			Delay for downloads to simulate network latency. Default: 0
dashMaxDrmSessions		Max drm sessions that can be cached by AampDRMSessionManager. Default; 3
log				New Configuration to override info/debug/trace. Default: 0
livePauseBehavior               Player paused state behavior. Default is 0(ePAUSED_BEHAVIOR_AUTOPLAY_IMMEDIATE)
latencyMonitorDelay		Low Latency Monitor delay. Default is 5(DEFAULT_LATENCY_MONITOR_DELAY)
latencyMonitorInterval		Low Latency Monitor Interval. Default is 2(DEFAULT_LATENCY_MONITOR_INTERVAL)
downloadBufferChunks		Low Latency Fragment chunk cache length. Defaults is 20
fragmentDownloadFailThreshold	Max retry attempts for non-init fragment curl timeout failures, range 1-10, Default is 10.
fogMaxConcurrentDownloads	Max concurrent download configured to Fog, Default is 5
TCPServerSinkPort   		See useTCPServerSink. Port number for video, audio will be video+1
drmNetworkTimeout		Curl Download Timeout for DRM in seconds. Default: 5s
drmStallTimeout			Timeout value for detection curl download stall for DRM in second. Default: 0s
drmStartTimeout            	Timeout value for curl download to start for DRM after connect in seconds. Default: 0s
connectTimeout			Curl socket connection timeout for fragment/playlist/manifest downloads. Default: 3s
dnsCacheTimeout			life-time for DNS cache entries ,Name resolve results are cached for manifest and used for this number of seconds. Default: 180s
telemetryInterval		Time interval for the telemetry reporting in seconds. Telemetry is disabled if set to 0. Default: 300 seconds.
subtitleClockSyncInterval   Time interval for synchronizing the clock with subtitle module. Default: 30s
preferredAbsoluteReporting	User preferred absolute progress reporting format, Default: eABSOLUTE_PROGRESS_WITHOUT_AVAILABILITY_START
EOSInjectionMode		replaces enableEOSInjectionDuringStop
					0 - Old behavior - EOS is injected at the end of asset and on discontinuity only.
					1 - EOS is injected during stop in addition to the old behavior.
showDiagnosticsOverlay		Configures the diagnostics overlay: 0 (none), 1 (minimal), 2 (extended). Controls the visibility and level of detail for diagnostics displayed during playback

// String inputs
licenseServerUrl		URL to be used for license requests for encrypted(PR/WV) assets
harvestPath			Specify the path where fragments has to be harvested,check folder permissions specifying the path
networkProxy			proxy address to set for all file downloads. Default: None  
licenseProxy			proxy address to set for license fetch. Default: None
AuthToken			SessionToken string to override from Application. Default: None
userAgent			Curl user-agent string. Default: {Mozilla/5.0 (Linux; x86_64 GNU/Linux) AppleWebKit/601.1 (KHTML, like Gecko) Version/8.0 Safari/601.1 WPE}
customHeader			custom header data to be appended to curl request. Default: None
uriParameter			uri parameter data to be appended on download-url during curl request. Default: None
preferredSubtitleLanguage	User preferred subtitle language. Default: None
ckLicenseServerUrl		ClearKey License server URL. Default: None
prLicenseServerUrl		PlayReady License server URL. Default: None
wvLicenseServerUrl		Widevine License server URL. Default: None
customHeaderLicense             custom header data to be appended to curl License request. Default: None

// Long input
minBitrate			Set minimum bitrate filter for playback profiles. Default: 0.
maxBitrate			Set maximum bitrate filter for playback profiles. Default: LONG_MAX.
downloadStallTimeout		Timeout value for detection curl download stall in seconds. Default: 0s
downloadStartTimeout		Timeout value for curl download to start after connect in seconds. Default: 0s.
discontinuityTimeout		Value in MS after which AAMP will try recovery for discontinuity stall, after detecting empty buffer, 0 will disable the feature. Default: 3000
InitialBitrate			Default bitrate. Default: 2500000
InitialBitrate4K		Default 4K bitrate. Default: 13000000
iframeDefaultBitrate		Default bitrate for iframe track selection for non-4K assets. Default: 0
iframeDefaultBitrate4K		Default bitrate for iframe track selection for 4K assets. Default: 0


// Double inputs
networkTimeout			Specify download time out in seconds. Default: 10s
manifestTimeout			Specify manifest download time out in seconds. Default: 10s
playlistTimeout			Playlist download time out in sec. Default: 10s

*File Harvest Config :
    By default aamp will dump all the type of data, set 0 for disabling harvest
	0x00000001 (1)      - Enable Harvest Video fragments - set 1st bit 
	0x00000002 (2)      - Enable Harvest audio - set 2nd bit 
	0x00000004 (4)      - Enable Harvest subtitle - set 3rd bit 
	0x00000008 (8)      - Enable Harvest auxiliary audio - set 4th bit 
	0x00000010 (16)     - Enable Harvest manifest - set 5th bit 
	0x00000020 (32)     - Enable Harvest license - set 6th bit , TODO: not yet supported license dumping
	0x00000040 (64)     - Enable Harvest iframe - set 7th bit 
	0x00000080 (128)    - Enable Harvest video init fragment - set 8th bit 
	0x00000100 (256)    - Enable Harvest audio init fragment - set 9th bit 
	0x00000200 (512)    - Enable Harvest subtitle init fragment - set 10th bit 
	0x00000400 (1024)   - Enable Harvest auxiliary audio init fragment - set 11th bit 
	0x00000800 (2048)   - Enable Harvest video playlist - set 12th bit 
	0x00001000 (4096)   - Enable Harvest audio playlist - set 13th bit 
	0x00002000 (8192)   - Enable Harvest subtitle playlist - set 14th bit 
	0x00004000 (16384)  - Enable Harvest auxiliary audio playlist - set 15th bit 
	0x00008000 (32768)  - Enable Harvest Iframe playlist - set 16th bit 
	0x00010000 (65536)  - Enable Harvest IFRAME init fragment - set 17th bit  
	example :- if you want harvest only manifest and video fragments , set value like 0x00000001 + 0x00000010 = 0x00000011 = 17
	harvest-config=17
```
---

# Channel Override Settings

Overriding channels in aamp.cfg
aamp.cfg allows to map channels to custom urls as follows
```
*<Token> <Custom url>
```
This will make aamp tune to the <Custom url> when ever aamp gets tune request to any url with <Token> in it.

Example adding the following in aamp.cfg will make tune to the given url (Spring_4Ktest) on tuning to url with USAHD in it
This can be done for n number of channels.
```
*USAHD https://example.com/manifest.mpd
*FXHD https://example.com/manifest2.mpd
```
---
# Westeros Settings

To enable Westeros


Currently, use of Westeros is default-disabled, and can be enabled via RFC.  To apply, Developers can add below
flag in SetEnv.sh under /opt, then restart the receiver process:

	export AAMP_ENABLE_WESTEROS_SINK=true

Note: Above is now used as a common FLAG by AAMP and Receiver module to configure Westeros direct rendering
instead of going through browser rendering.  This allows for smoother video zoom animations

However, note that with this optimization applied, the AAMP Diagnostics overlays cannot be made visible.
As a temporary workaround, the following flag can be used  by developers which will make diagnostic overlay
again visible at expense of zoom smoothness:

	export DISABLE_NONCOMPOSITED_WEBGL_FOR_IPVIDEO=1

---

# AAMP-CLI Commands

CLI-specific commands:
```
<enter>		dump currently available profiles
help		show usage notes
batch       Execute commands line by line as batch defined in #Home/aampcli.bat (~/aampcli.bat)
http://...	tune to specified URL
<number>	tune to specified channel (based on canned aamp channel map)
next        tune to next virtual channel
prev        tune to previous virtual channel
seek <sec>	time-based seek within current content (stub)
ff <speed>	set desired trick speed to <speed>, i.e. ff8 for 8x playback using iframe track
rew <speed>	set desired trick speed to <speed>, i.e. rew8 for -8x playback using iframe track
stop		stop streaming
status		dump gstreamer state
rect		Set video rectangle. eg. rect 0 0 640 360
zoom <val>	Set video zoom mode. mode "none" if val is zero, else mode "full"
pause       Pause playback
play        Resume playback
live        Seek to live point
exit        Gracefully exit application
sap <lang>  Select alternate audio language track.
bps <val>   Set video bitrate in bps
fog <url|host=ip:port> 'fog url' tune to arbitrary locator via fog. 'fog host=ip:port' set fog location (default: 127.0.0.1:9080)
adtesting   Toggle indexed ad insertion that does NOT check for any duration match
advert <params>
	advert map <adBreakId> <url>
		specify ad locator to present instead of source content during specified ad break
		note: multiple sequential ads can be mapped to fill a single ad break by calling advert map multiple times with same adBreakId
	advert clear (clear current advert map)
	advert list	(display the advert list)
new <name>	create a new player instance with optional name
select <val|name> move player val or name to foreground. With no option list all players
detach		move current foreground player to background
release <val|name>	delete detached player val or name
```

To add channelmap for CLI, enter channel entries in below format in /opt/aampcli.cfg

    *<Channel Number> <Channel Name> <Channel URL>

or

To add channelmap for CLI, enter channel entries in below format in /opt/aampcli.csv
    
    <Channel Number>,<Channel Name>,<Channel URL>
    
---

# AAMP Tunetime 

Following line can be added as a header while making CSV with profiler data.

version#4
```
version,build,tuneStartBaseUTCMS,ManifestDLStartTime,ManifestDLTotalTime,ManifestDLFailCount,VideoPlaylistDLStartTime,VideoPlaylistDLTotalTime,VideoPlaylistDLFailCount,AudioPlaylistDLStartTime,AudioPlaylistDLTotalTime,AudioPlaylistDLFailCount,VideoInitDLStartTime,VideoInitDLTotalTime,VideoInitDLFailCount,AudioInitDLStartTime,AudioInitDLTotalTime,AudioInitDLFailCount,VideoFragmentDLStartTime,VideoFragmentDLTotalTime,VideoFragmentDLFailCount,VideoBitRate,AudioFragmentDLStartTime,AudioFragmentDLTotalTime,AudioFragmentDLFailCount,AudioBitRate,drmLicenseAcqStartTime,drmLicenseAcqTotalTime,drmFailErrorCode,LicenseAcqPreProcessingDuration,LicenseAcqNetworkDuration,LicenseAcqPostProcDuration,VideoFragmentDecryptDuration,AudioFragmentDecryptDuration,gstPlayStartTime,gstFirstFrameTime,contentType,streamType,firstTune,Prebuffered,PreBufferedTime
```
version#5
```
version,build,tuneStartBaseUTCMS,ManifestDLStartTime,ManifestDLTotalTime,ManifestDLFailCount,VideoPlaylistDLStartTime,VideoPlaylistDLTotalTime,VideoPlaylistDLFailCount,AudioPlaylistDLStartTime,AudioPlaylistDLTotalTime,AudioPlaylistDLFailCount,VideoInitDLStartTime,VideoInitDLTotalTime,VideoInitDLFailCount,AudioInitDLStartTime,AudioInitDLTotalTime,AudioInitDLFailCount,VideoFragmentDLStartTime,VideoFragmentDLTotalTime,VideoFragmentDLFailCount,VideoBitRate,AudioFragmentDLStartTime,AudioFragmentDLTotalTime,AudioFragmentDLFailCount,AudioBitRate,drmLicenseAcqStartTime,drmLicenseAcqTotalTime,drmFailErrorCode,LicenseAcqPreProcessingDuration,LicenseAcqNetworkDuration,LicenseAcqPostProcDuration,VideoFragmentDecryptDuration,AudioFragmentDecryptDuration,gstPlayStartTime,gstFirstFrameTime,contentType,streamType,firstTune,playerPreBuffered,playerPreBufferedTime,durationSeconds,interfaceWifi,TuneAttempts,TuneSuccess,FailureReason,Appname,Numbers of TimedMetadata(Ads),StartTime to Report TimedEvent,Time taken to ReportTimedMetadata,TSBEnabled,Total Time
```
---
# VideoEnd (Session Statistics) Event 
```
vr = version of video end event (currently "2.0")
tt = time to reach top profile first time after tune. Provided initial tune bandwidth is not a top bandwidth
ta = time at top profile. This includes all the fragments which are downloaded/injected at top profile for total duration of playback. 
d = duration - estimate of total playback duration.  Note that this is based on fragments downloaded/injected - user may interrupt buffered playback with seek/stop, causing estimates to skew higher in edge cases.
dn = Download step-downs due to bad Network bandwidth
de = Download step-downs due to Error handling ramp-down/retry logic
rc = Rate corrections made in player to catch up live latency
w = Display Width :  value > 0 = Valid Width.. value -1 means HDMI display resolution could NOT be read successfully. Only for HDMI Display else wont be available.
h = Display Height : value > 0 = Valid Height,  value -1 means HDMI display resolution could NOT be read successfully. Only for HDMI Display else wont be available.
t = indicates that FOG time shift buffer (TSB) was used for playback
m =  main manifest
v = video Profile
i = Iframe Profile
a1 = audio track 1
a2 = audio track 2
a3 = audio track 3
...
u = Unknown Profile or track type

l = supported languages,
	In version 2.0, same tag is reused to represent download latency report if it appears under 'n' or 'i' or 'ms'(See their representations below).
p = profile-specific metrics encapsulation
w = profile frame width
h = profile frame height
ls = license statistics

ms = manifest statistics
fs = fragment statistics

r = total license rotations / stream switches
e = encrypted to clear switches
c = clear to encrypted switches

in version 1.0,
	4 = HTTP-4XX error count
	5 = HTTP-5XX error count
	t = CURL timeout error count
	c = CURL error count (other)
	s = successful download count
in version 2.0
	S = Session summary
	200 = http success
	18(0) - Curl 18 occurred, network connectivity is down
	18(1) = Curl 18 occurred, network connectivity is up
	28(0) - Curl 28 occurred, network connectivity is down
	28(1) = Curl 28 occurred, network connectivity is up
	404, 42, 7, etc.. = http/curl error code occurred during download.
		Example : "S":{"200":341,"404":6} - 341 success attempts and 4 attempts with 404
			  "S":{"200":116,"28(1)":1,"404":114} - 115 success attempts, 114 attempts with 404 and 1 attempt with curl-28
	T0
	T1
	T2
	...
	Ty = Latency report in a specific time window, where y represents window number (comes under 'l').
		T0 - is 0ms - 250ms window (Window calculations: start = (250ms x y), end = (start + 250ms))
		For example : T13 represents window 3250ms - 3500ms (13x250ms = 3250ms).

u = URL of most recent (last) failed download
n = normal fragment statistics
i = "init" fragment statistics (used in case of DASH and fragmented mp4)
pc = Video profile capped status due to resolution constraints or bitrate filtering
st = subtitle track
```
---

# AAMP Simulator Installation

## Setting up AAMP Simulator (Mac/OSX)
Prerequisites: xcode, git

Open terminal app
```
git clone -b dev_sprint_23_1 https://code.rdkcentral.com/r/rdk/components/generic/aamp
```
note: the branch changes over time - dev_sprint_YY_Q

currently dev_sprint_23_1
```
cd aamp
bash install-aamp.sh
```
select aamp-cli as default project

Apple configuration in xcode under "Scheme: Edit Scheme..." if not already set:
- Diagnostics: Runtime Sanitization: Address Sanitizer (strong memory checks at runtime)
- Options: Console: Use Terminal (avoid doubled keypress bug)os
    
## Setting Up AAMP Simulator (Linux/Ubuntu)
- Install Visual Studio Code on your Linux Machine https://code.visualstudio.com/download
- Installing AAMP .Run the following 4 commands
```
apt-get install git
git clone -b dev_sprint_23_1 https://code.rdkcentral.com/r/rdk/components/generic/aamp
cd aamp
bash install-aamp.sh
```
Choose "Y" when asked

Enter system password when prompted

Upon completion, Visual Studio Code should have opened.

## Setting Up Ubuntu Virtual Machine if Needed
Setting up VM on Windows 10
- Install Virtual Box from Oracle, version 6.1.26. It is possible that it will work with later versions, but this one has been tested.
- Download Ubuntu 22.04 from https://ubuntu.com/download/desktop/thank-you?version=22.04.1&architecture=amd64 .
- Follow the tutorial at https://ubuntu.com/tutorials/how-to-run-ubuntu-desktop-on-a-virtual-machine-using-virtualbox#1-overview to install an Ubuntu desktop on a virtual machine.


---
