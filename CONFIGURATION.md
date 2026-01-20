# AAMP Configuration Guide

## Overview

AAMP provides extensive configuration options to control player behavior. Configuration can be set at multiple levels, with later configurations overriding earlier ones.

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
| `disableLowLatencyABR` | Boolean | true | Enable Low Latency ABR handling |
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
| `fragmentRetryLimit` | Number | -1 | Fragment retry limit (-1 = unlimited) |

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
