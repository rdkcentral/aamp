# AAMP CMCD Quick Reference Guide

**For Third-Party CDN and Server Integrators**

This guide provides essential information for integrating with AAMP's CMCD implementation.

---

## Quick Facts

- **Specification**: CTA-5004 (Common Media Client Data)
- **Default State**: Enabled
- **Transmission Method**: HTTP Headers only
- **Supported Protocols**: DASH, HLS
- **Coverage**: ~50% of CTA-5004 specification

---

## Configuration

```javascript
// Enable CMCD (default)
player.setConfigValue("enableCMCD", true);

// Disable CMCD
player.setConfigValue("enableCMCD", false);
```

**Auto-Disable**: CMCD is automatically disabled when FOG TSB is enabled.

---

## Expected Headers

### All Requests
```
CMCD-Session: sid=<uuid>
```

### Video Segments
```
CMCD-Session: sid=550e8400-e29b-41d4-a716-446655440000
CMCD-Object: br=5000,ot=v,tb=8000
CMCD-Request: bl=10500,nor=/segment_456.m4s,com.<mso>>-fb=120,com.<mso>-lb=850
```

### Audio Segments
```
CMCD-Session: sid=550e8400-e29b-41d4-a716-446655440000
CMCD-Object: br=128,ot=a,tb=256
CMCD-Request: bl=12000,nor=/audio_segment.m4s,com.<mso>-fb=50,com.<mso>-lb=180
```

### Init Segments
```
CMCD-Object: br=5000,ot=i,tb=8000
```

### Manifests
```
CMCD-Object: ot=m
```

### Buffer Starvation
```
CMCD-Status: bs
```

---

## Supported Fields

| Field | Description | Type | Example |
|-------|-------------|------|---------|
| `sid` | Session ID | UUID | `550e8400-e29b-41d4-a716-446655440000` |
| `ot` | Object type | Token | `v`, `a`, `i`, `m`, `s`, `av` |
| `br` | Bitrate (kbps) | Integer | `5000` |
| `tb` | Top bitrate (kbps) | Integer | `8000` |
| `bl` | Buffer length (ms) | Integer | `10500` |
| `nor` | Next object request | String | `/segment_456.m4s` |
| `nrr` | Next range request | String | `1024-2047` |
| `bs` | Buffer starvation | Boolean | present when true |

### Vendor-Specific Fields (MSO Extensions)

| Field | Description | Type | Example |
|-------|-------------|------|---------|
| `com.<mso>-dns` | DNS lookup time (ms) | Integer | `15` |
| `com.<mso>-fb` | Time to first byte (ms) | Integer | `120` |
| `com.<mso>-lb` | Time to last byte (ms) | Integer | `850` |

---

## Object Types (`ot`)

| Value | Meaning |
|-------|---------|
| `m` | Manifest/Playlist |
| `v` | Video segment |
| `a` | Audio segment |
| `av` | Muxed audio/video segment |
| `i` | Init segment |
| `s` | Subtitle/caption segment |

---

## Fields NOT Supported

⚠️ **Do not rely on these fields** - they are not implemented in AAMP:

- `d` - Object duration
- `mtp` - Measured throughput
- `dl` - Deadline (low latency)
- `pr` - Playback rate
- `su` - Startup flag
- `sf` - Streaming format
- `st` - Stream type (VOD/Live)
- `v` - CMCD version
- `cid` - Content ID
- `rtp` - Requested throughput

---

## Common Use Cases

### 1. CDN Cache Optimization

**Use**: `nor` (next object request)

```
CMCD-Request: nor=/segment_457.m4s
```

Your CDN can prefetch or prioritize the next segment.

### 2. Adaptive Bitrate Decisions

**Use**: `br` (current bitrate), `tb` (top bitrate), `bl` (buffer length)

```
CMCD-Object: br=5000,tb=8000
CMCD-Request: bl=10500
```

If `br` << `tb` and `bl` is high, client has headroom for higher quality.

### 3. Quality of Experience Monitoring

**Use**: `bs` (buffer starvation)

```
CMCD-Status: bs
```

Track buffer starvation events for QoE metrics.

### 4. Network Performance

**Use**: MSO extensions

```
CMCD-Request: com.<mso>-fb=120,com.<mso>-lb=850
```

- First byte: 120ms (network latency indicator)
- Last byte: 850ms (total download time)

### 5. Bandwidth Estimation

**Note**: `mtp` (measured throughput) is NOT implemented.

Estimate from downloaded bytes and time if needed server-side.

---

## Parsing Examples

### Python
```python
import re

def parse_cmcd_headers(headers):
    cmcd = {}
    
    # Parse CMCD-Session
    if 'CMCD-Session' in headers:
        for pair in headers['CMCD-Session'].split(','):
            key, value = pair.split('=')
            cmcd[key] = value
    
    # Parse CMCD-Object
    if 'CMCD-Object' in headers:
        for pair in headers['CMCD-Object'].split(','):
            if '=' in pair:
                key, value = pair.split('=')
                cmcd[key] = int(value) if value.isdigit() else value
    
    # Parse CMCD-Request
    if 'CMCD-Request' in headers:
        for pair in headers['CMCD-Request'].split(','):
            key, value = pair.split('=')
            cmcd[key] = int(value) if value.isdigit() else value
    
    # Parse CMCD-Status
    if 'CMCD-Status' in headers:
        cmcd['bs'] = 'bs' in headers['CMCD-Status']
    
    return cmcd

# Example
headers = {
    'CMCD-Session': 'sid=550e8400-e29b-41d4-a716-446655440000',
    'CMCD-Object': 'br=5000,ot=v,tb=8000',
    'CMCD-Request': 'bl=10500,nor=/segment_456.m4s,com.<mso>-fb=120',
    'CMCD-Status': 'bs'
}

cmcd = parse_cmcd_headers(headers)
print(cmcd)
# {'sid': '550e8400...', 'br': 5000, 'ot': 'v', 'tb': 8000, 
#  'bl': 10500, 'nor': '/segment_456.m4s', 'com.<mso>-fb': 120, 'bs': True}
```

### Node.js
```javascript
function parseCMCDHeaders(headers) {
    const cmcd = {};
    
    // Helper to parse key-value pairs
    const parseKV = (str) => {
        str.split(',').forEach(pair => {
            const [key, value] = pair.split('=');
            if (value) {
                cmcd[key] = isNaN(value) ? value : parseInt(value);
            } else {
                cmcd[key] = true; // Boolean flags like 'bs'
            }
        });
    };
    
    if (headers['cmcd-session']) parseKV(headers['cmcd-session']);
    if (headers['cmcd-object']) parseKV(headers['cmcd-object']);
    if (headers['cmcd-request']) parseKV(headers['cmcd-request']);
    if (headers['cmcd-status']) parseKV(headers['cmcd-status']);
    
    return cmcd;
}
```

### Go
```go
package main

import (
    "net/http"
    "strconv"
    "strings"
)

func ParseCMCDHeaders(headers http.Header) map[string]interface{} {
    cmcd := make(map[string]interface{})
    
    parseKV := func(value string) {
        for _, pair := range strings.Split(value, ",") {
            parts := strings.SplitN(pair, "=", 2)
            if len(parts) == 2 {
                if num, err := strconv.Atoi(parts[1]); err == nil {
                    cmcd[parts[0]] = num
                } else {
                    cmcd[parts[0]] = parts[1]
                }
            } else {
                cmcd[parts[0]] = true
            }
        }
    }
    
    if session := headers.Get("CMCD-Session"); session != "" {
        parseKV(session)
    }
    if object := headers.Get("CMCD-Object"); object != "" {
        parseKV(object)
    }
    if request := headers.Get("CMCD-Request"); request != "" {
        parseKV(request)
    }
    if status := headers.Get("CMCD-Status"); status != "" {
        parseKV(status)
    }
    
    return cmcd
}
```

---

## CDN Configuration Tips

### 1. Session Tracking
Use `sid` to track playback sessions across multiple requests.

### 2. Prefetching
Use `nor` to prefetch the next segment to reduce latency.

### 3. Quality Switching
Monitor `bl` (buffer length):
- `bl` < 5000ms: Client may need lower bitrate
- `bl` > 15000ms: Client has headroom for higher bitrate

### 4. Buffer Starvation Response
When `bs` is present:
- Reduce bitrate recommendations
- Prioritize this client's requests
- Consider edge server health

### 5. Network Performance Tracking
Track MSO vendor extensions for network diagnostics:
- `com.<mso>-dns`: DNS resolution issues
- `com.<mso>-fb`: Network latency
- `com.<mso>-lb`: Download speed issues

---

## Validation Checklist

✅ **Must Support**:
- [ ] Parse `CMCD-Session` header
- [ ] Parse `CMCD-Object` header  
- [ ] Parse `CMCD-Request` header
- [ ] Parse `CMCD-Status` header (optional)
- [ ] Handle `sid` (session ID)
- [ ] Handle `ot` (object type)
- [ ] Handle `br` (bitrate)

✅ **Should Support**:
- [ ] Handle `tb` (top bitrate)
- [ ] Handle `bl` (buffer length)
- [ ] Handle `nor` (next object request)
- [ ] Handle `bs` (buffer starvation)
- [ ] Handle vendor extensions (`com.<mso>-*`)

✅ **Nice to Have**:
- [ ] Track sessions over time
- [ ] Build QoE dashboards
- [ ] Alert on buffer starvation patterns
- [ ] Optimize caching based on `nor`

---

## Troubleshooting

### Problem: No CMCD Headers Received

**Possible Causes**:
1. CMCD disabled in config: `player.setConfigValue("enableCMCD", false)`
2. FOG TSB enabled (auto-disables CMCD)
3. Client using older AAMP version without CMCD support

**Solution**: Verify config and AAMP version.

### Problem: Missing Expected Fields

**Check**: Is the field in the "Not Supported" list?

Many CTA-5004 fields are not implemented in AAMP. See [CMCD-Bugs-and-Gaps.md](CMCD-Bugs-and-Gaps.md).

### Problem: Inconsistent Header Format

**Note**: AAMP only sends headers, never query parameters or JSON.

If you need query parameters, you'll need to translate server-side or request AAMP enhancement.

### Problem: Vendor-Specific Fields

`com.<mso>-*` fields are MSO extensions, not standard CTA-5004.

These can be safely ignored if not needed for your use case.

---

## Performance Considerations

### Header Overhead

Typical CMCD overhead per request:
- **Session**: ~60 bytes
- **Object**: ~40 bytes  
- **Request**: ~100-150 bytes
- **Status**: ~10 bytes (when present)

**Total**: ~200-250 bytes per request

### Request Frequency

CMCD headers sent with:
- Every manifest request
- Every segment request (video, audio, subtitle, init)
- Retry attempts

High-volume streams may generate thousands of CMCD requests per session.

---

## Support and Resources

- **Full Documentation**: [CMCD-Support.md](CMCD-Support.md)
- **Bugs and Gaps**: [CMCD-Bugs-and-Gaps.md](CMCD-Bugs-and-Gaps.md)
- **CTA-5004 Spec**: Contact CTA for official specification
- **AAMP Source**: Review implementation in `support/aampmetrics/`

---

## Version History

| AAMP Version | CMCD Support | Changes |
|--------------|--------------|---------|
| 2023+ | Partial | Initial CMCD implementation |

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
