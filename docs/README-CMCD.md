# AAMP CMCD Documentation

This directory contains comprehensive documentation for AAMP's implementation of the **Common Media Client Data (CMCD)** specification (CTA-5004).

---

## Documentation Files

### 📘 [CMCD-Support.md](CMCD-Support.md)
**Complete technical documentation for AAMP's CMCD implementation**

Audience: AAMP developers, third-party integrators  
Contents:
- Detailed CMCD overview and CTA-5004 specification summary
- Complete list of supported and unsupported fields
- Architecture and class hierarchy
- Data flow and integration points
- Configuration options
- Header format and examples
- Implementation details

### 🐛 [CMCD-Bugs-and-Gaps.md](CMCD-Bugs-and-Gaps.md)
**Analysis of bugs, gaps, and compliance issues**

Audience: AAMP development team, QA  
Contents:
- Identified bugs with severity ratings
- Missing CTA-5004 fields
- Architectural issues
- Feature gaps
- Priority implementation roadmap
- Compliance matrix (50% coverage)
- Test coverage gaps

### ⚡ [CMCD-Quick-Reference.md](CMCD-Quick-Reference.md)
**Quick reference guide for third-party integrators**

Audience: CDN providers, third-party server integrators  
Contents:
- Quick facts and configuration
- Expected header formats
- Supported vs unsupported fields
- Parsing examples (Python, Node.js, Go)
- CDN configuration tips
- Troubleshooting guide
- Performance considerations

---

## Quick Navigation

**I want to...**

- ✅ **Understand AAMP's CMCD support** → [CMCD-Support.md](CMCD-Support.md)
- ✅ **Integrate my CDN with AAMP** → [CMCD-Quick-Reference.md](CMCD-Quick-Reference.md)
- ✅ **Fix bugs or add features** → [CMCD-Bugs-and-Gaps.md](CMCD-Bugs-and-Gaps.md)
- ✅ **Know what fields are supported** → [Quick Reference: Supported Fields](CMCD-Quick-Reference.md#supported-fields)
- ✅ **Parse CMCD headers** → [Quick Reference: Parsing Examples](CMCD-Quick-Reference.md#parsing-examples)

---

## Key Findings Summary

### Implementation Status
- ✅ **CMCD Enabled**: By default (can be disabled via config)
- ⚠️ **Specification Coverage**: ~50% of CTA-5004 fields implemented
- ✅ **Protocol Support**: DASH (MPD) and HLS
- ✅ **Transmission**: HTTP headers (query parameters not supported)

### What Works
✅ Session tracking (`sid`)  
✅ Object type identification (`ot`)  
✅ Bitrate reporting (`br`, `tb`)  
✅ Buffer status (`bl`, `bs`)  
✅ Next object prefetch hints (`nor`, `nrr`)  
✅ Network timing (vendor extensions)  

### What's Missing
❌ Object duration (`d`)  
❌ Measured throughput (`mtp`)  
❌ Stream type (`st`) - VOD vs Live  
❌ Playback rate (`pr`)  
❌ Startup flag (`su`)  
❌ Low latency fields (`dl`)  

### Known Issues
🐛 Memory management uses raw pointers instead of smart pointers  
🐛 Vendor-specific field names (`com.comcast-*`)  
🐛 No query parameter encoding support  
🐛 Missing integration tests  

---

## For AAMP Developers

### Priority Action Items

**High Priority** (Critical for CDN optimization):
1. Implement object duration (`d`) - already tracked internally
2. Add measured throughput (`mtp`) - calculate from download metrics
3. Refactor to smart pointers for memory safety

**Medium Priority** (Specification compliance):
4. Add stream type (`st`) - VOD vs Live
5. Add playback rate (`pr`) - for trick play
6. Document vendor extensions properly
7. Add integration tests

**Low Priority** (Nice to have):
8. Add remaining CTA-5004 fields
9. Support query parameter encoding
10. Add CMCD version field

See [CMCD-Bugs-and-Gaps.md](CMCD-Bugs-and-Gaps.md) for detailed implementation guidance.

### Testing Checklist
- [ ] Unit tests for all CMCD classes ✅ (exists)
- [ ] Integration tests for header generation ❌ (missing)
- [ ] CDN endpoint validation ❌ (missing)
- [ ] FOG TSB disable behavior ✅ (verified)
- [ ] Multi-bitrate switching ✅ (verified)

---

## For Third-Party Integrators

### Getting Started

1. **Read**: [CMCD-Quick-Reference.md](CMCD-Quick-Reference.md)
2. **Implement**: Header parsing (see examples in Quick Reference)
3. **Test**: Validate against expected header formats
4. **Monitor**: Track `bs` (buffer starvation) events

### Integration Checklist

✅ **Must Have**:
- [ ] Parse `CMCD-Session`, `CMCD-Object`, `CMCD-Request` headers
- [ ] Handle session ID tracking
- [ ] Support all object types (`m`, `v`, `a`, `i`, `s`, `av`)
- [ ] Process bitrate fields (`br`, `tb`)

✅ **Should Have**:
- [ ] Use buffer length (`bl`) for QoS decisions
- [ ] Implement prefetching based on `nor`
- [ ] Track buffer starvation (`bs`) events
- [ ] Handle vendor extensions if needed

✅ **Nice to Have**:
- [ ] Build QoE dashboards
- [ ] Optimize caching strategies
- [ ] Alert on starvation patterns

### Example Integration

```python
# Python CDN middleware example
from parse_cmcd import parse_cmcd_headers

def handle_request(request):
    # Extract CMCD data
    cmcd = parse_cmcd_headers(request.headers)
    
    # Session tracking
    session_id = cmcd.get('sid')
    
    # Prefetch next segment
    if 'nor' in cmcd:
        prefetch_segment(cmcd['nor'])
    
    # Adjust caching based on buffer
    buffer_length = cmcd.get('bl', 0)
    if buffer_length < 5000:
        # Client is running low, prioritize
        set_high_priority(session_id)
    
    # Monitor quality
    if cmcd.get('bs'):
        log_buffer_starvation(session_id)
    
    return serve_content(request)
```

---

## Related Documentation

- [AAMP Architecture](../ARCHITECTURE.md)
- [C++ Coding Guidelines](../.github/instructions/cpp.instructions.md)
- [Testing Guidelines](../.github/instructions/testing.instructions.md)
- [AAMP README](../README.md)

---

## Contributing

Found a bug or want to contribute?

1. Check [CMCD-Bugs-and-Gaps.md](CMCD-Bugs-and-Gaps.md) for known issues
2. Follow [C++ coding guidelines](../.github/instructions/cpp.instructions.md)
3. Add tests per [testing guidelines](../.github/instructions/testing.instructions.md)
4. Submit PR with clear description

---

## Support

For questions or issues:
- Review documentation in this directory
- Check source code in `support/aampmetrics/`
- Consult AAMP architecture documentation

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
