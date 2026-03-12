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

# AAMP Common Media Client Data (CMCD) Support

## Overview

AAMP implements partial support for the **Common Media Client Data (CMCD)** specification (CTA-5004). CMCD is a standardized mechanism for video players to communicate playback and client status information to content delivery networks (CDNs) and servers via HTTP request headers.

This document describes AAMP's current CMCD implementation, supported features, configuration, and known limitations for third-party integrators.

---

## Table of Contents

1. [What is CMCD?](#what-is-cmcd)
2. [AAMP CMCD Implementation](#aamp-cmcd-implementation)
3. [Configuration](#configuration)
4. [Supported CMCD Fields](#supported-cmcd-fields)
5. [CMCD Headers Structure](#cmcd-headers-structure)
6. [Usage Example](#usage-example)
7. [Limitations and Gaps](#limitations-and-gaps)
8. [Recommendations](#recommendations)
9. [Architecture](#architecture)

---

## What is CMCD?

Common Media Client Data (CMCD) is defined in the **CTA-5004** specification. It provides a standardized way for video clients to communicate metrics to servers, enabling:

- **Improved CDN decision-making**: Servers can make better caching and delivery decisions
- **Enhanced QoS monitoring**: Better visibility into client-side playback conditions
- **Optimized delivery**: CDNs can adjust delivery based on client buffer state and network conditions

CMCD data is transmitted via HTTP headers organized into four categories:
- **Session**: Information about the playback session (e.g., session ID)
- **Object**: Information about the requested object (e.g., bitrate, object type)
- **Request**: Information about the HTTP request (e.g., buffer length, next object)
- **Status**: Information about playback status (e.g., buffer starvation)

---

## AAMP CMCD Implementation

AAMP's CMCD support is **enabled by default** and can be controlled via configuration. The implementation:

- Supports **DASH (MPD)** and **HLS** streaming protocols
- Sends CMCD headers with manifest, playlist, and media segment requests
- Tracks metrics separately for different media types (video, audio, subtitles, manifests)
- Uses HTTP headers for CMCD transmission (not query parameters)

### Key Components

| Component | File | Description |
|-----------|------|-------------|
| **AampCMCDCollector** | [AampCMCDCollector.h](../AampCMCDCollector.h), [AampCMCDCollector.cpp](../AampCMCDCollector.cpp) | Main collector that manages CMCD data for all media types |
| **CMCDHeaders** | [support/aampmetrics/CMCDHeaders.h](../support/aampmetrics/CMCDHeaders.h), [CMCDHeaders.cpp](../support/aampmetrics/CMCDHeaders.cpp) | Base class for CMCD header generation |
| **VideoCMCDHeaders** | [support/aampmetrics/VideoCMCDHeaders.cpp](../support/aampmetrics/VideoCMCDHeaders.cpp) | Video-specific CMCD header implementation |
| **AudioCMCDHeaders** | [support/aampmetrics/AudioCMCDHeaders.cpp](../support/aampmetrics/AudioCMCDHeaders.cpp) | Audio-specific CMCD header implementation |
| **ManifestCMCDHeaders** | [support/aampmetrics/ManifestCMCDHeaders.cpp](../support/aampmetrics/ManifestCMCDHeaders.cpp) | Manifest-specific CMCD header implementation |
| **SubtitleCMCDHeaders** | [support/aampmetrics/SubtitleCMCDHeaders.cpp](../support/aampmetrics/SubtitleCMCDHeaders.cpp) | Subtitle-specific CMCD header implementation |

---

## Configuration

### Enabling/Disabling CMCD

CMCD support is controlled via the `enableCMCD` configuration parameter:

```javascript
// Enable CMCD (default)
player.setConfigValue("enableCMCD", true);

// Disable CMCD
player.setConfigValue("enableCMCD", false);
```

**Configuration Key**: `eAAMPConfig_EnableCMCD`  
**Default Value**: `true` (enabled)  
**Config File Key**: `"enableCMCD"`

### Automatic Disabling

CMCD is automatically disabled when:
- FOG TSB (Time Shift Buffer) mode is enabled
- The player is configured for FOG-based playback

### Session ID / Trace ID

CMCD uses the player's session trace ID as the session identifier (`sid`):
- If a trace ID is provided during initialization, it is used
- If trace ID is "unknown", a UUID is automatically generated
- The session ID remains constant for the duration of a playback session

---

## Supported CMCD Fields

### Implemented Fields

AAMP implements a **subset** of the CMCD specification. The following fields are supported:

#### Session Keys (CMCD-Session header)

| Key | Type | Description | Support Status |
|-----|------|-------------|----------------|
| `sid` | String (UUID) | Session identifier | ✅ **Supported** |

#### Object Keys (CMCD-Object header)

| Key | Type | Description | Support Status |
|-----|------|-------------|----------------|
| `br` | Integer (kbps) | Encoded bitrate of the current object | ✅ **Supported** |
| `ot` | Token | Object type (m=manifest, a=audio, v=video, i=init, s=subtitle, av=muxed) | ✅ **Supported** |
| `tb` | Integer (kbps) | Top/maximum bitrate available | ✅ **Supported** (video/audio only) |

#### Request Keys (CMCD-Request header)

| Key | Type | Description | Support Status |
|-----|------|-------------|----------------|
| `bl` | Integer (ms) | Buffer length in milliseconds | ✅ **Supported** |
| `nor` | String | Next object request (relative path to next segment) | ✅ **Supported** |
| `nrr` | String | Next range request (for byte-range requests) | ✅ **Supported** |
| `com.<mso>-dns` | Integer (ms) | DNS lookup time (vendor-specific) | ✅ **Supported** |
| `com.<mso>-fb` | Integer (ms) | Time to first byte (vendor-specific) | ✅ **Supported** |
| `com.<mso>-lb` | Integer (ms) | Time to last byte (vendor-specific) | ✅ **Supported** |

#### Status Keys (CMCD-Status header)

| Key | Type | Description | Support Status |
|-----|------|-------------|----------------|
| `bs` | Boolean | Buffer starvation (present when true) | ✅ **Supported** |

### Unsupported Fields

The following standard CMCD fields are **NOT implemented** in AAMP:

#### Object Keys
- `d` - Object duration
- `mtp` - Measured throughput
- `su` - Startup flag

#### Request Keys
- `dl` - Deadline (for low latency)
- `cid` - Content ID

#### Status Keys
- `rtp` - Requested maximum throughput

#### Additional Keys
- `pr` - Playback rate
- `sf` - Stream format
- `st` - Stream type
- `v` - CMCD version

---

## CMCD Headers Structure

### Header Format

CMCD data is sent as HTTP headers in the following format:

```
CMCD-Session: sid=<uuid>
CMCD-Object: br=<bitrate>,ot=<type>,tb=<topBitrate>
CMCD-Request: bl=<bufferLength>,nor=<nextUrl>,com.<mso>-fb=<firstByte>,com.<mso>-lb=<lastByte>
CMCD-Status: bs
```

### Media Type-Specific Headers

#### Video Segments

```
CMCD-Session: sid=550e8400-e29b-41d4-a716-446655440000
CMCD-Object: br=5000,ot=v,tb=8000
CMCD-Request: bl=10500,nor=/segment_456.m4s,com.<mso>-fb=120,com.<mso>-lb=850
```

#### Video Init Segments

```
CMCD-Session: sid=550e8400-e29b-41d4-a716-446655440000
CMCD-Object: br=5000,ot=i,tb=8000
CMCD-Request: bl=10500,nor=/init.mp4,com.<mso>-fb=80,com.<mso>-lb=200
```

#### Muxed Audio/Video

```
CMCD-Session: sid=550e8400-e29b-41d4-a716-446655440000
CMCD-Object: br=5000,ot=av,tb=8000
CMCD-Request: bl=10500,nor=/segment.ts,com.<mso>-fb=120,com.<mso>-lb=850
```

#### Audio Segments

```
CMCD-Session: sid=550e8400-e29b-41d4-a716-446655440000
CMCD-Object: br=128,ot=a,tb=256
CMCD-Request: bl=12000,nor=/audio_segment.m4s,com.<mso>-fb=50,com.<mso>-lb=180
```

#### Subtitle Segments

```
CMCD-Session: sid=550e8400-e29b-41d4-a716-446655440000
CMCD-Object: ot=s
```

#### Manifest/Playlist

```
CMCD-Session: sid=550e8400-e29b-41d4-a716-446655440000
CMCD-Object: ot=m
```

#### Buffer Starvation

When buffer is in RED state (starvation):

```
CMCD-Status: bs
```

---

## Usage Example

### Initialization Flow

```cpp
// CMCD is automatically initialized when playback starts
// The collector is created in PrivateInstanceAAMP constructor:
mCMCDCollector = new AampCMCDCollector();

// Initialized during tune:
bool cmcdEnabled = (ISCONFIGSET_PRIV(eAAMPConfig_EnableCMCD) && !mFogTSBEnabled);
mCMCDCollector->Initialize(cmcdEnabled, sessionTraceId);
```

### Metrics Collection

```cpp
// Set bitrates (called when profile list is loaded)
std::vector<BitsPerSecond> videoBitrates = GetVideoBitrates();
mCMCDCollector->SetBitrates(eMEDIATYPE_VIDEO, videoBitrates);

// Set next object request (called before fragment fetch)
mCMCDCollector->CMCDSetNextObjectRequest(
    nextSegmentUrl,
    currentBandwidth,
    eMEDIATYPE_VIDEO
);

// Set next range request (for SegmentBase MPD)
mCMCDCollector->CMCDSetNextRangeRequest(
    "1024-2047",  // next byte range
    bandwidth,
    mediaType
);

// Set network metrics (called after download completes)
mCMCDCollector->CMCDSetNetworkMetrics(
    mediaType,
    timeToFirstByte,    // milliseconds
    timeToLastByte,     // milliseconds
    dnsLookupTime       // milliseconds
);

// Set track data before download (buffer state, bitrate)
SetCMCDTrackData(mediaType);
```

### Header Injection

```cpp
// Get CMCD headers before download
std::vector<std::string> cmcdHeaders;
mCMCDCollector->CMCDGetHeaders(mediaType, cmcdHeaders);

// Add to curl request
for (const auto& header : cmcdHeaders) {
    curl_slist_append(httpHeaders, header.c_str());
}
```

---

## Limitations and Gaps

### 1. Protocol Coverage

✅ **Supported**:
- DASH (MPD) - SegmentTemplate, SegmentList, SegmentBase
- HLS

⚠️ **Partial**:
- Smooth Streaming (limited testing)

### 2. Missing Standard Fields

The following **CTA-5004 standard fields are NOT implemented**:

- **`d` (duration)**: Object duration not reported
- **`mtp` (measured throughput)**: Not calculated or reported
- **`dl` (deadline)**: Low-latency deadline not supported
- **`pr` (playback rate)**: Trick mode rate not reported
- **`su` (startup)**: Startup flag not sent
- **`sf` (streaming format)**: Format (DASH/HLS) not explicitly reported
- **`st` (stream type)**: VOD vs Live not reported
- **`v` (version)**: CMCD version not included

### 3. Known Issues

#### Issue 1: DNS Time Always Included
**Description**: DNS lookup time is always included in headers even when value is 0.  
**Impact**: Minor - adds unnecessary data to requests when DNS is cached  
**Severity**: Low  
**File**: [VideoCMCDHeaders.cpp:62](../support/aampmetrics/VideoCMCDHeaders.cpp#L62-L64)

```cpp
// Current implementation:
if(dnsLookUptime > 0) {
    // Include DNS time
}
// Should check if DNS was actually performed
```

#### Issue 2: No Measured Throughput
**Description**: `mtp` (measured throughput) is not calculated or reported  
**Impact**: CDNs cannot optimize based on observed client throughput  
**Severity**: Medium  
**Recommendation**: Calculate from downloaded bytes and time

#### Issue 3: Object Duration Not Reported
**Description**: `d` (duration) field is not populated for segments  
**Impact**: Servers cannot estimate how much playback time is being requested  
**Severity**: Medium  
**Note**: Fragment duration is available in AAMP but not passed to CMCD

#### Issue 4: Playback Rate Not Reported
**Description**: `pr` (playback rate) is not included during trick play  
**Impact**: CDNs cannot optimize for fast-forward/rewind operations  
**Severity**: Low  
**Note**: Playback rate is tracked in AAMP but not exposed to CMCD

#### Issue 5: Stream Type Not Reported
**Description**: `st` (stream type) - VOD vs Live distinction not reported  
**Impact**: CDNs cannot differentiate live vs on-demand optimization  
**Severity**: Medium  
**Note**: AAMP knows if stream is live but doesn't include in CMCD

#### Issue 6: Vendor-Specific Extensions
**Description**: Uses MSO-specific field names instead of standard CMCD keys  
**Impact**: Non-portable, vendor lock-in  
**Severity**: Medium  
**Files**:
- `com.<mso>-dns` instead of standard approach
- `com.<mso>-fb` (first byte)
- `com.<mso>-lb` (last byte)

**Recommendation**: Consider using standardized field names or documenting vendor extensions

#### Issue 7: No Query Parameter Support
**Description**: CMCD is only sent via HTTP headers, not query parameters  
**Impact**: Some CDNs may prefer or require query parameter format  
**Severity**: Low  
**Note**: CTA-5004 supports both headers and query parameters

#### Issue 8: No JSON Encoding Support
**Description**: Only supports key-value pair format, not JSON encoding  
**Impact**: Cannot use JSON format if CDN requires it  
**Severity**: Low  

#### Issue 9: Missing Startup Flag
**Description**: `su` (startup) flag not set for initial requests  
**Impact**: CDNs cannot prioritize startup requests  
**Severity**: Low  

### 4. Architecture Concerns

#### Memory Management
**Issue**: Uses raw pointers with manual `delete` instead of smart pointers  
**File**: [AampCMCDCollector.cpp:47](../AampCMCDCollector.cpp#L47-L53)  
**Recommendation**: Refactor to use `std::unique_ptr<CMCDHeaders>` for RAII compliance

#### Thread Safety
**Status**: ✅ Properly protected with mutex  
**File**: [AampCMCDCollector.h:130](../AampCMCDCollector.h#L130)  

---

## Recommendations

### For AAMP Development Team

1. **Implement Missing Standard Fields**:
   - Add `d` (duration) using existing fragment duration
   - Add `mtp` (measured throughput) calculation
   - Add `st` (stream type) using existing live/VOD detection
   - Add `pr` (playback rate) for trick modes
   - Add `su` (startup) flag for initial requests

2. **Code Quality Improvements**:
   - Replace raw pointers with smart pointers (`std::unique_ptr`)
   - Remove vendor-specific field names or document as extensions
   - Add support for query parameter encoding (optional)
   - Consider JSON encoding support

3. **Testing**:
   - Add integration tests for CMCD header validation
   - Test with actual CDN endpoints that consume CMCD
   - Verify CMCD behavior during error conditions and retries

4. **Documentation**:
   - Add API documentation for third-party integrators
   - Document vendor-specific extensions
   - Provide CDN configuration examples

### For Third-Party Integrators

1. **Validation**:
   - Parse and validate CMCD headers received from AAMP
   - Do not rely on fields marked as "unsupported"
   - Account for vendor-specific `com.<mso>-*` fields

2. **CDN Configuration**:
   - Configure CDN to expect CMCD in HTTP headers (not query params)
   - Be aware that CMCD is disabled for FOG TSB sessions

3. **Monitoring**:
   - Monitor for `bs` (buffer starvation) events
   - Track `bl` (buffer length) for quality of experience metrics
   - Use `nor` (next object request) for prefetching optimization

---

## Architecture

### Class Hierarchy

```
CMCDHeaders (base class)
├── VideoCMCDHeaders
├── AudioCMCDHeaders
├── ManifestCMCDHeaders
└── SubtitleCMCDHeaders

AampCMCDCollector
└── std::map<AampMediaType, CMCDHeaders*>
```

### Data Flow

```
1. Initialization
   PrivateInstanceAAMP::Tune()
   └── mCMCDCollector->Initialize(enabled, traceId)
       └── Creates CMCDHeaders instances for each media type

2. Profile Loading
   StreamAbstractionAAMP::GetProfiles()
   └── mCMCDCollector->SetBitrates(mediaType, bitrates)
       └── Sets top bitrate (tb field)

3. Pre-Download
   PrivateInstanceAAMP::GetFile()
   ├── SetCMCDTrackData(mediaType)
   │   └── Sets buffer length, buffer starvation, current bitrate
   └── mCMCDCollector->CMCDGetHeaders(mediaType, headers)
       └── Builds CMCD headers from collected metrics

4. Fragment Collection
   FragmentCollector::FetchFragment()
   ├── CMCDSetNextObjectRequest(url, bandwidth, mediaType)
   └── CMCDSetNextRangeRequest(range, bandwidth, mediaType)

5. Post-Download
   PrivateInstanceAAMP::GetFile() [on completion]
   └── mCMCDCollector->CMCDSetNetworkMetrics(...)
       └── Records DNS time, time to first/last byte
```

### Supported Media Types

| AampMediaType | CMCD Object Type | Class Used | Notes |
|---------------|------------------|------------|-------|
| `eMEDIATYPE_VIDEO` | `v` | VideoCMCDHeaders | Video segments |
| `eMEDIATYPE_INIT_VIDEO` | `i` | VideoCMCDHeaders | Video init segments |
| `eMEDIATYPE_IFRAME` | `v` | VideoCMCDHeaders | I-frame segments |
| `eMEDIATYPE_AUDIO` | `a` | AudioCMCDHeaders | Audio segments |
| `eMEDIATYPE_INIT_AUDIO` | `i` | AudioCMCDHeaders | Audio init segments |
| `eMEDIATYPE_SUBTITLE` | `s` | SubtitleCMCDHeaders | Subtitle segments |
| `eMEDIATYPE_INIT_SUBTITLE` | `s` | SubtitleCMCDHeaders | Subtitle init segments |
| `eMEDIATYPE_MANIFEST` | `m` | ManifestCMCDHeaders | Main manifest |
| `eMEDIATYPE_PLAYLIST_VIDEO` | `m` | ManifestCMCDHeaders | Video playlists |
| `eMEDIATYPE_PLAYLIST_AUDIO` | `m` | ManifestCMCDHeaders | Audio playlists |
| `eMEDIATYPE_PLAYLIST_SUBTITLE` | `m` | ManifestCMCDHeaders | Subtitle playlists |

**Note**: Muxed streams (video+audio in same segment) use object type `av`.

---

## References

- **CTA-5004 Specification**: Common Media Client Data (CMCD) Standard
- **AAMP Source Files**:
  - [AampCMCDCollector.h](../AampCMCDCollector.h)
  - [AampCMCDCollector.cpp](../AampCMCDCollector.cpp)
  - [CMCDHeaders.h](../support/aampmetrics/CMCDHeaders.h)
  - [CMCDHeaders.cpp](../support/aampmetrics/CMCDHeaders.cpp)
  - [VideoCMCDHeaders.cpp](../support/aampmetrics/VideoCMCDHeaders.cpp)
  - [AudioCMCDHeaders.cpp](../support/aampmetrics/AudioCMCDHeaders.cpp)
  - [ManifestCMCDHeaders.cpp](../support/aampmetrics/ManifestCMCDHeaders.cpp)
  - [SubtitleCMCDHeaders.cpp](../support/aampmetrics/SubtitleCMCDHeaders.cpp)

---

## Revision History

| Version | Date | Author | Description |
|---------|------|--------|-------------|
| 1.0 | 2026-02-23 | GitHub Copilot | Initial documentation of CMCD support |
