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

# AAMP Configuration Guide

## Overview

AAMP provides extensive configuration options to control player behavior. Configuration can be set at multiple levels, with later configurations overriding earlier ones.

## Table of Contents

- [Configuration Priority Order](#configuration-priority-order)
- [Configuration Methods](#configuration-methods)
  - [UVE JavaScript API](#uve-javascript-api)
  - [Environment Variables](#environment-variables)
  - [Configuration Files](#configuration-files)
- [Configuration Reference](#configuration-reference)
  - [Network Timeouts](#network-timeouts)
  - [Adaptive Bitrate (ABR)](#adaptive-bitrate-abr)
  - [Low Latency Playback](#low-latency-playback)
  - [Buffering & Playback](#buffering--playback)
  - [Playlist & Fragment](#playlist--fragment)
  - [Audio Track Demultiplexing](#audio-track-demultiplexing)
  - [Audio Codec & Format](#audio-codec--format)
  - [Video Profile](#video-profile)
  - [DRM & License](#drm--license)
  - [Captions & Timed Metadata](#captions--timed-metadata)
  - [Streaming Format Options](#streaming-format-options)
  - [Network & Protocol](#network--protocol)
  - [Events & Reporting](#events--reporting)
  - [Miscellaneous](#miscellaneous)
- [Best Practices](#best-practices)
- [See Also](#see-also)

## Configuration Priority Order

**Lowest to Highest Priority:**

1. **AAMP Default Settings** - Hardcoded defaults in source code
2. **Operator Settings** - Via RFC (Remote Feature Control) or environment variables
3. **Stream-Provided Settings** - Set by the stream itself
4. **Application Settings** - Set by the application using `initConfig()` API
5. **Developer Configuration** - Text format (`/opt/aamp.cfg`) or JSON format (`/opt/aampcfg.json`)

## Configuration Methods

### UVE JavaScript API

```javascript
var player = new AAMPMediaPlayer();
var config = {
    abr: true,
    abrCacheLength: 3,
    enableLowLatencyDash: true
};
player.initConfig(config);
player.load(url);
```

### Environment Variables

```bash
export AAMP_ABR=false
export AAMP_PARALLEL_PLAYLIST_DOWNLOAD=true
```

### Configuration Files

**JSON Format** (`/opt/aampcfg.json`):
```json
{
    "abr": true,
    "abrCacheLength": 3,
    "enableLowLatencyDash": true,
    "licenseAcquisitionTimeout": 10000
}
```

**Text Format** (`/opt/aamp.cfg`):
```
abr=true
abrCacheLength=3
enableLowLatencyDash=true
```

## Configuration Reference

### Network Timeouts

Network timeout configurations control how long AAMP waits for various network operations before timing out. All timeout values are in seconds.

| Property | Type | Default | Description |
|----------|------|---------|-------------|
| `networkTimeout` | Number | 10 | General network timeout for fragment downloads (seconds) |
| `manifestTimeout` | Number | 10 | Timeout for manifest/playlist downloads (seconds) |
| `playlistTimeout` | Number | 0 | Specific timeout for playlist operations (0 = use manifestTimeout) |
| `connectTimeout` | Number | 3 | TCP socket connection timeout (seconds) |
| `dnsCacheTimeout` | Number | 180 | DNS resolution cache lifetime (seconds, default 3 minutes) |
| `downloadStallTimeout` | Number | 0 | Timeout when download stalls with no progress (0 = disabled) |
| `downloadStartTimeout` | Number | 0 | Timeout waiting for download to start (0 = disabled) |
| `downloadLowBWTimeout` | Number | 0 | Timeout when bandwidth drops below threshold (0 = disabled) |

**Usage Notes:**
- **networkTimeout**: Primary timeout for media fragment downloads. Adjust based on network conditions and fragment sizes.
- **manifestTimeout**: Should be shorter than networkTimeout for faster failure detection on manifest issues.
- **playlistTimeout**: When set to 0, inherits value from manifestTimeout. Use for protocol-specific tuning.
- **connectTimeout**: Critical for handling slow DNS or network connection issues. Keep relatively short (2-5s).
- **dnsCacheTimeout**: Longer values improve performance but may delay updates. Default 180s balances efficiency and freshness.
- **downloadStallTimeout**: Detects stalled connections when data stops flowing mid-transfer. Useful for poor network conditions.
- **downloadStartTimeout**: Catches hung connections that never begin transferring data.
- **downloadLowBWTimeout**: Advanced setting for detecting sustained low bandwidth conditions.

### Adaptive Bitrate (ABR)

| Property | Type | Default | Description |
|----------|------|---------|-------------|
| `abr` | Boolean | true | Enable/disable adaptive bitrate logic |
| `abrCacheLength` | Number | 3 | Length of ABR cache for bandwidth calculation |
| `abrCacheLife` | Number | 5000 | ABR cache lifetime in milliseconds |
| `abrCacheOutlier` | Number | 5000000 | Outlier difference to ignore (bytes) |
| `abrNwConsistency` | Number | 2 | Checks before profile change to avoid flapping |
| `abrSkipDuration` | Number | 6 | Min duration of fragment before ABR trigger (seconds) |
| `useNewABR` | Boolean | true | Enable new buffer-based hybrid ABR |
| `useAverageBandwidth` | Boolean | false | Use average bandwidth for ABR instead of attribute |

### Low Latency Playback

| Property | Type | Default | Description |
|----------|------|---------|-------------|
| `enableLowLatencyDash` | Boolean | true | Enable Low Latency DASH mode |
| `disableLowLatencyABR` | Boolean | true | Disable Low Latency ABR handling |
| `enableLowLatencyCorrection` | Boolean | true | Prevent gradual latency drift |
| `downloadBufferChunks` | Number | 20 | Low Latency fragment chunk cache |

### Buffering & Playback

| Property | Type | Default | Description |
|----------|------|---------|-------------|
| `gstBufferAndPlay` | Boolean | true | Pre-buffer before pipeline play |
| `internalRetune` | Boolean | true | Internal retune on underflows/PTS errors |
| `useRetuneForGstInternalError` | Boolean | true | Retune on GST errors |
| `cdvrLiveOffset` | Number | 30 | Live offset for cDVR (seconds) |

### Playlist & Fragment

| Property | Type | Default | Description |
|----------|------|---------|-------------|
| `parallelPlaylistDownload` | Boolean | false | Parallel fetch of audio/video playlists (HLS tune) |
| `parallelPlaylistRefresh` | Boolean | true | Parallel fetch during playlist refresh |
| `preFetchIframePlaylist` | Boolean | false | Pre-fetch I-frame playlist |
| `preservePipeline` | Boolean | false | Flush instead of teardown |
| `fragmentRetryLimit` | Number | -1 | Fragment retry limit |

### Audio Track Demultiplexing

| Property | Type | Default | Description |
|----------|------|---------|-------------|
| `demuxHlsAudioTrack` | Boolean | true | Demux audio from HLS transport stream |
| `demuxHlsVideoTrack` | Boolean | true | Demux video from HLS transport stream |
| `demuxHlsVideoTrackTrickMode` | Boolean | true | Demux video during trick mode |
| `demuxAudioBeforeVideo` | Boolean | false | Demux audio before video |

### Audio Codec & Format

| Property | Type | Default | Description |
|----------|------|---------|-------------|
| `disableEC3` | Boolean | false | Disable Dolby Digital Plus (DD+) |
| `disableATMOS` | Boolean | false | Disable Dolby ATMOS |
| `stereoOnly` | Boolean | false | Select stereo audio only (overrides EC3/ATMOS) |
| `audioOnlyPlayback` | Boolean | false | Audio-only playback without video |

### Video Profile

| Property | Type | Default | Description |
|----------|------|---------|-------------|
| `disable4K` | Boolean | false | Disable 4K playback on 4K devices |
| `iframeDefaultBitrate` | Number | 0 | Default bitrate for I-frame tracks |
| `synthesizeIframeForVOD` | Boolean | false | When `true`, AAMP downloads the regular video track during VOD DASH trickplay (FF/REW) and strips each segment to its leading I-frame using `IsoBmffHelper::ConvertToKeyFrame()`. Enables VCR-style trickplay on VOD assets (including JITT ads) that do not advertise a dedicated iframe `AdaptationSet`. Has no effect during live/TSB playback. |

### DRM & License

| Property | Type | Default | Description |
|----------|------|---------|-------------|
| `licenseAnonymousRequest` | Boolean | false | Acquire license without token |
| `contentProtectionDataUpdateTimeout` | Number | 5000 | Timeout for dynamic key rotation (ms) |
| `licenseAcquisitionTimeout` | Number | 10000 | License acquisition timeout (ms) |

### Captions & Timed Metadata

| Property | Type | Default | Description |
|----------|------|---------|-------------|
| `bulkTimedMetadata` | Boolean | false | Report timed metadata as single bulk event |
| `nativeCCRendering` | Boolean | false | Enable native caption rendering |
| `id3` | Boolean | false | Enable ID3 tag processing |
| `enableSubscribedTags` | Boolean | true | Enable subscribed tags |

### Streaming Format Options

| Property | Type | Default | Description |
|----------|------|---------|-------------|
| `dashIgnoreBaseUrlIfSlash` | Boolean | false | Ignore constructed DASH URI if it contains slash |
| `mpdDiscontinuityHandling` | Boolean | true | Handle discontinuity during MPD period transition |
| `mpdDiscontinuityHandlingCdvr` | Boolean | true | Handle discontinuity for cDVR MPD transitions |
| `hlsAVTrackSyncUsingPDT` | Boolean | false | Use EXT-X-PROGRAM-DATE for A/V sync |
| `useMatchingBaseUrl` | Boolean | false | Use matching base URL when multiple available |

### Network & Protocol

| Property | Type | Default | Description |
|----------|------|---------|-------------|
| `forceHttp` | Boolean | false | Force HTTP protocol for HTTPS URLs |
| `sslVerifyPeer` | Boolean | false | Verify SSL peer certificate |
| `throttle` | Boolean | false | Regulate output data flow |
| `customHeader` | String | - | Custom headers for curl requests |
| `propagateUriParameters` | Boolean | true | Pass top-level manifest URI params to fragments |

### Events & Reporting

| Property | Type | Default | Description |
|----------|------|---------|-------------|
| `enableVideoEndEvent` | Boolean | true | Generate video end event |
| `disablePlaylistIndexEvent` | Boolean | true | Disable playlist indexed event generation |
| `enableSeekableRange` | Boolean | false | Report seekable range for non-fog content |
| `reportVideoPTS` | Boolean | false | Report video PTS in progress events |
| `enableVideoRectangle` | Boolean | true | Set rectangle property for sink element |
| `asyncTune` | Boolean | false | Asynchronous API/event handling for UI |

### Miscellaneous

| Property | Type | Default | Description |
|----------|------|---------|-------------|
| `fog` | Boolean | true | Enable Fog |
| `useWesterosSink` | Boolean | false | Use Westeros sink for video decoding |
| `useNewAdBreaker` | Boolean | false | Use new discontinuity processing based on PDT |
| `useRetuneForUnpairedDiscontinuity` | Boolean | true | Retune on unpaired discontinuity |
| `netTraceCsvDump` | Boolean | false | Write per-download AAMP network trace CSV files to `/tmp` by default, typically using PID-suffixed filenames such as `/tmp/aamp_net_requests.csv.<pid>` and `/tmp/aamp_net_bursts.csv.<pid>`; environment overrides may change the final paths. |

## Best Practices

1. **Start with Defaults**: Only override defaults when necessary
2. **Platform-Specific Settings**: Use environment variables for platform variations
3. **Timeout Values**: Set based on your network conditions (typically 5-30 seconds)
4. **ABR Tuning**: Monitor bandwidth patterns before adjusting ABR cache parameters
5. **Testing**: Verify configuration changes in dev environment before production

## See Also

- [API Reference](AAMP-UVE-API.md)
- [Architecture](ARCHITECTURE.md)
- [Troubleshooting](TROUBLESHOOTING.md)
