# AAMP CMCD: Bugs and Gaps Analysis

## Executive Summary

This document identifies bugs, gaps, and improvements needed in AAMP's CMCD (Common Media Client Data) implementation to achieve full CTA-5004 compliance.

**Status**: AAMP implements **8 out of 18 standard CTA-5004 fields (~44% coverage)**

**Priority Categories**:
- 🔴 **High**: Critical for CDN optimization
- 🟡 **Medium**: Important for full spec compliance
- 🟢 **Low**: Nice-to-have features

---

## Bugs

### � BUG-1: DNS Condition Takes Precedence Over Range Requests

**File**: [support/aampmetrics/VideoCMCDHeaders.cpp](../support/aampmetrics/VideoCMCDHeaders.cpp#L62-L70)  
**Severity**: Medium  
**Impact**: Range requests (`nrr`) cannot be reported when DNS lookup time is > 0, breaking SegmentBase MPD support

**Description**:
The if-else logic prioritizes DNS time over range requests. When `dnsLookUptime > 0`, it always uses `nor` (next object URL) even if `mNextRange` is populated for SegmentBase MPDs. This means range-based requests never get properly reported when DNS timing is captured.

**Current Code**:
```cpp
if(dnsLookUptime > 0)
{
    // Always uses 'nor' (next URL)
    headerValue.push_back(CMCDBUFFERLENGTH+...+CMCDNEXTURL+nextUrl+...+CMCDDns+...);
}
else if(!mNextRange.empty())
{
    // Only reaches here if dnsLookUptime <= 0
    headerValue.push_back(CMCDBUFFERLENGTH+...+CMCDNEXTRANGEREQUEST+mNextRange+...);
}
else
{
    headerValue.push_back(CMCDBUFFERLENGTH+...+CMCDNEXTURL+nextUrl+...);
}
```

**Issue**: Should check for range requests first, or include DNS alongside range requests rather than as mutually exclusive.

**Fix Option 1** (Check range first):
```cpp
if(!mNextRange.empty())
{
    // Use range request if available
    std::string metrics = CMCDBUFFERLENGTH+std::to_string(bufferLength)+delimiter+
        CMCDNEXTRANGEREQUEST+mNextRange+delimiter+
        CMCDFirstByte+std::to_string(firstByte)+delimiter+
        CMCDLastByte+std::to_string(lastByte);
    if(dnsLookUptime > 0) {
        metrics += delimiter + CMCDDns+std::to_string(dnsLookUptime);
    }
    headerValue.push_back(metrics);
}
else if(dnsLookUptime > 0)
{
    headerValue.push_back(CMCDBUFFERLENGTH+...+CMCDNEXTURL+nextUrl+...+CMCDDns+...);
}
else
{
    headerValue.push_back(CMCDBUFFERLENGTH+...+CMCDNEXTURL+nextUrl+...);
}
```

**Fix Option 2** (Always include DNS when available):
```cpp
std::string nextRequest = (!mNextRange.empty()) 
    ? CMCDNEXTRANGEREQUEST+mNextRange 
    : CMCDNEXTURL+nextUrl;

std::string metrics = CMCDBUFFERLENGTH+std::to_string(bufferLength)+delimiter+
    nextRequest+delimiter+
    CMCDFirstByte+std::to_string(firstByte)+delimiter+
    CMCDLastByte+std::to_string(lastByte);

if(dnsLookUptime > 0) {
    metrics += delimiter + CMCDDns+std::to_string(dnsLookUptime);
}
headerValue.push_back(metrics);
```

**Same Issue In**: [AudioCMCDHeaders.cpp](../support/aampmetrics/AudioCMCDHeaders.cpp#L50-L58)

---

### 🟡 BUG-2: Memory Management - Raw Pointers Instead of Smart Pointers

**File**: [AampCMCDCollector.cpp](../AampCMCDCollector.cpp)  
**Severity**: Medium  
**Impact**: Potential memory leaks, not following AAMP coding standards

**Description**:
Uses raw `new`/`delete` instead of smart pointers, violating modern C++ and AAMP's own guidelines.

**Current Code**:
```cpp
AampCMCDCollector::~AampCMCDCollector()
{
    // Free the memory if allocated
    if(mCMCDStreamData.size())
    {
        for(StreamTypeCMCDIter it=mCMCDStreamData.begin(); it!=mCMCDStreamData.end(); it++)
        {
            SAFE_DELETE(it->second);
        }
        mCMCDStreamData.clear();
    }
}
```

**Fix**:
```cpp
// In AampCMCDCollector.h
typedef std::map<int, std::unique_ptr<CMCDHeaders>> StreamTypeCMCD;

// In Initialize()
mCMCDStreamData[eMEDIATYPE_MANIFEST] = std::make_unique<ManifestCMCDHeaders>();
pCMCDMetrics = mCMCDStreamData[eMEDIATYPE_MANIFEST].get();
pCMCDMetrics->SetSessionId(mTraceId);
pCMCDMetrics->SetMediaType("MANIFEST");

// Destructor becomes automatic - no manual cleanup needed
```

---

### 🟢 BUG-3: Hexadecimal Conversion Function Never Used

**File**: [AampCMCDCollector.cpp](../AampCMCDCollector.cpp#L176-L192)  
**Severity**: Low  
**Impact**: Dead code

**Description**:
The `convertHexa()` function is defined but never called anywhere in the codebase.

**Current Code**:
```cpp
std::string AampCMCDCollector::convertHexa(long long number)
{
    std::string hexa;
    // ... conversion logic
    return hexa;
}
```

**Action**: Remove dead code or document its intended purpose.

---

## Missing CTA-5004 Standard Fields

### 🔴 GAP-1: Missing Object Duration (`d`)

**Priority**: High  
**Impact**: CDNs cannot estimate requested playback time  
**Effort**: Low

**Description**:
The `d` (duration) field is required for CDNs to understand how much playback time is being requested. AAMP has fragment duration but doesn't expose it.

**Implementation**:
```cpp
// In CMCDHeaders.h
protected:
    int objectDuration; // milliseconds

// In BuildCMCDCustomHeaders (Video/Audio)
if (objectDuration > 0) {
    headerValue += delimiter + "d=" + std::to_string(objectDuration);
}
```

**Required Changes**:
1. Pass fragment duration to `CMCDSetNextObjectRequest()`
2. Store in `CMCDHeaders::objectDuration`
3. Include in `CMCD-Object` header

---

### 🔴 GAP-2: Missing Measured Throughput (`mtp`)

**Priority**: High  
**Impact**: CDNs cannot optimize based on observed client throughput  
**Effort**: Medium

**Description**:
The `mtp` (measured throughput) field reports the client's observed download speed, critical for adaptive bitrate decisions.

**Implementation**:
```cpp
// Calculate from download metrics
int measuredThroughputKbps = (downloadedBytes * 8) / (downloadTimeMs * 1000);

// Add to CMCD-Object
headerValue += ",mtp=" + std::to_string(measuredThroughputKbps);
```

**Required Changes**:
1. Calculate throughput in `CMCDSetNetworkMetrics()`
2. Store in `CMCDHeaders::measuredThroughput`
3. Include in `CMCD-Object` header

---

### 🟡 GAP-3: Missing Stream Type (`st`)

**Priority**: Medium  
**Impact**: CDNs cannot differentiate VOD vs Live optimization  
**Effort**: Low

**Description**:
The `st` (stream type) field indicates whether content is VOD or live. AAMP tracks this but doesn't expose it.

**Values**: `v` (VOD), `l` (live)

**Implementation**:
```cpp
// In CMCDHeaders.h
protected:
    std::string streamType; // "v" or "l"

// In BuildCMCDCustomHeaders
if (!streamType.empty()) {
    headerValue.push_back("st=" + streamType);
    mCMCDCustomHeaders["CMCD-Session:"] = headerValue;
}
```

**Required Changes**:
1. Pass live/VOD info to collector during initialization
2. Store in `CMCDHeaders::streamType`
3. Include in `CMCD-Session` header

---

### 🟡 GAP-4: Missing Playback Rate (`pr`)

**Priority**: Medium  
**Impact**: CDNs cannot optimize for trick play operations  
**Effort**: Low

**Description**:
The `pr` (playback rate) field indicates the current playback speed, important during fast-forward/rewind.

**Values**: Decimal (e.g., `1.0` = normal, `2.0` = 2x speed)

**Implementation**:
```cpp
// In SetTrackData() or similar
void SetPlaybackRate(float rate) {
    if (rate != 1.0) {
        // Include in CMCD-Request
        headerValue += ",pr=" + std::to_string(rate);
    }
}
```

---

### 🟢 GAP-5: Missing Startup Flag (`su`)

**Priority**: Low  
**Impact**: CDNs cannot prioritize startup requests  
**Effort**: Low

**Description**:
The `su` (startup) flag indicates the initial requests during playback startup.

**Implementation**:
```cpp
// Track if in startup phase
bool isStartup = (segmentNumber <= 2);

// In CMCD-Request
if (isStartup) {
    headerValue += ",su";
}
```

---

### 🟢 GAP-6: Missing Streaming Format (`sf`)

**Priority**: Low  
**Impact**: Minor - server can often infer from URL  
**Effort**: Low

**Description**:
The `sf` (streaming format) field explicitly states the format.

**Values**: `d` (DASH), `h` (HLS), `s` (Smooth), `o` (other)

**Implementation**:
```cpp
// Store during initialization based on detected format
std::string streamingFormat; // "d" for DASH, "h" for HLS

// Include in CMCD-Session
headerValue.push_back("sid=" + sessionId + ",sf=" + streamingFormat);
```

---

### 🟢 GAP-7: Missing Content ID (`cid`)

**Priority**: Low  
**Impact**: Useful for multi-asset analytics  
**Effort**: Low

**Description**:
The `cid` (content ID) provides a unique identifier for the content being played.

**Implementation**:
```cpp
// If available from app or manifest
if (!contentId.empty()) {
    headerValue.push_back("cid=" + contentId);
}
```

---

### 🟢 GAP-8: Missing Requested Throughput (`rtp`)

**Priority**: Low  
**Impact**: Used in some ABR algorithms  
**Effort**: Medium

**Description**:
The `rtp` (requested throughput) field indicates the throughput needed for current profile.

**Implementation**:
```cpp
// Calculate based on current bitrate and buffer state
int requestedThroughputKbps = currentBitrate * 1.2; // 20% headroom

// Include in CMCD-Status
headerValue.push_back("rtp=" + std::to_string(requestedThroughputKbps));
```

---

## Architectural Issues

### 🟡 ISSUE-1: Vendor-Specific Field Names

**Priority**: Medium  
**Impact**: Non-portable, vendor lock-in  
**Effort**: Low

**Description**:
Uses MSO-specific field names instead of standard CMCD or documented extensions:
- `com.<mso>-dns` - DNS lookup time
- `com.<mso>-fb` - First byte time  
- `com.<mso>-lb` - Last byte time

**Recommendation**:
1. Either use standard CMCD fields for network timing
2. Or properly document as vendor extensions per CTA-5004 Section 3.1

**Standard Approach**:
The specification doesn't define standard network timing fields, so vendor extensions are acceptable but should be documented.

---

### 🟢 ISSUE-2: No Query Parameter Support

**Priority**: Low  
**Impact**: Some CDNs may prefer query parameters  
**Effort**: Medium

**Description**:
CMCD is only sent via HTTP headers. CTA-5004 also supports query parameters and JSON encoding.

**Recommendation**:
Add configuration option to choose transmission mode:
- HTTP Headers (current)
- Query Parameters
- JSON (request body)

---

### 🟢 ISSUE-3: No CMCD Version Field

**Priority**: Low  
**Impact**: Future compatibility  
**Effort**: Low

**Description**:
The `v` (version) field should indicate CMCD specification version.

**Implementation**:
```cpp
// In CMCD-Session header
headerValue.push_back("sid=" + sessionId + ",v=1");
```

---

## Feature Gaps

### 🟡 FEATURE-1: No Low Latency Support

**Priority**: Medium  
**Impact**: Cannot optimize for LL-DASH or LL-HLS  
**Effort**: High

**Description**:
Missing support for low-latency specific fields:
- `dl` (deadline) - Target latency deadline
- Chunk-level timing for low latency

**Note**: AAMP has Low Latency DASH support but doesn't expose metrics to CMCD.

---

### 🟢 FEATURE-2: No Custom Key Support

**Priority**: Low  
**Impact**: Cannot extend CMCD for specific use cases  
**Effort**: Medium

**Description**:
No API to add custom CMCD keys for application-specific metrics.

**Recommendation**:
```cpp
// Add API for custom keys
void AddCustomKey(const std::string& key, const std::string& value);
```

---

## Test Coverage Gaps

### 🟡 TEST-1: No Integration Tests for CMCD Headers

**Priority**: Medium  
**Impact**: Cannot verify correct header generation  
**Effort**: Medium

**Description**:
Unit tests exist but no integration tests that validate:
- Complete header format
- All media types
- Edge cases (buffer starvation, rate changes, etc.)

**Recommendation**:
Add test that captures actual HTTP headers and validates against CTA-5004.

---

### 🟡 TEST-2: No CDN Endpoint Validation

**Priority**: Medium  
**Impact**: Cannot verify CDN compatibility  
**Effort**: High

**Description**:
No testing against actual CDN endpoints that consume CMCD.

**Recommendation**:
Set up test environment with CDN that parses and validates CMCD data.

---

## Priority Implementation Roadmap

### Phase 1: Critical Gaps (High Priority)
1. **GAP-1**: Add object duration (`d`)
2. **GAP-2**: Add measured throughput (`mtp`)
3. **BUG-2**: Refactor to smart pointers

### Phase 2: Compliance (Medium Priority)
4. **GAP-3**: Add stream type (`st`)
5. **GAP-4**: Add playback rate (`pr`)
6. **ISSUE-1**: Document vendor extensions
7. **TEST-1**: Add integration tests

### Phase 3: Enhancement (Low Priority)
8. **GAP-5**: Add startup flag (`su`)
9. **GAP-6**: Add streaming format (`sf`)
10. **BUG-1**: Fix DNS conditional logic
11. **BUG-3**: Remove dead code
12. **ISSUE-3**: Add CMCD version field

### Phase 4: Optional Features
13. **ISSUE-2**: Query parameter support
14. **FEATURE-1**: Low latency support
15. **FEATURE-2**: Custom key API

---

## Compliance Matrix

| Category | Field | CTA-5004 | AAMP | Gap |
|----------|-------|----------|------|-----|
| **Session** | sid | Required | ✅ | None |
| **Session** | sf | Optional | ❌ | GAP-6 |
| **Session** | st | Optional | ❌ | GAP-3 |
| **Session** | v | Optional | ❌ | ISSUE-3 |
| **Object** | br | Required | ✅ | None |
| **Object** | ot | Required | ✅ | None |
| **Object** | d | Optional | ❌ | GAP-1 |
| **Object** | mtp | Optional | ❌ | GAP-2 |
| **Object** | tb | Optional | ✅ | None |
| **Request** | bl | Optional | ✅ | None |
| **Request** | dl | Optional | ❌ | FEATURE-1 |
| **Request** | nor | Optional | ✅ | None |
| **Request** | nrr | Optional | ✅ | None |
| **Request** | su | Optional | ❌ | GAP-5 |
| **Status** | bs | Optional | ✅ | None |
| **Status** | rtp | Optional | ❌ | GAP-8 |
| **Custom** | pr | Optional | ❌ | GAP-4 |
| **Custom** | cid | Optional | ❌ | GAP-7 |

**Compliance Score**: 8/18 standard fields = **44% coverage**

**Note**: AAMP also implements 3 vendor-specific extensions (`com.comcast-dns`, `com.comcast-fb`, `com.comcast-lb`) which are not part of the standard CTA-5004 specification.

---

## References

- CTA-5004: Common Media Client Data (CMCD) Specification
- [AAMP CMCD Support Documentation](CMCD-Support.md)
- [AAMP Architecture](../ARCHITECTURE.md)
- [C++ Coding Guidelines](../.github/instructions/cpp.instructions.md)

---

**Copyright 2026 RDK Management**

Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.
