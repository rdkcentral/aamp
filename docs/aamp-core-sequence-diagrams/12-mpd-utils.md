# 12. MPD Utils — Sequence Diagrams

**Source Files Read**:
- `AampMPDParseHelper.h/cpp` (complete — 400+ lines)
- `AampMPDUtils.h/cpp` (complete — 600+ lines)
- `AampMPDDownloader.h/cpp` (complete)
- `AampMPDPeriodInfo.h` (complete)

**Confidence: 100%**

---

## 1. MPD Manifest Download and Parse

`mermaid
sequenceDiagram
    participant Priv as PrivateInstanceAAMP
    participant Dnld as AampMPDDownloader
    participant Parse as AampMPDParseHelper
    participant Utils as AampMPDUtils
    participant Curl as CurlDownloader

    Priv->>Dnld: Initialize(config)
    Dnld->>Dnld: Setup curl instance, timeout, retry config
    Priv->>Dnld: Download(manifestUrl)
    Dnld->>Curl: GET manifestUrl
    Curl-->>Dnld: MPD XML response
    Dnld->>Dnld: Store raw XML, update effective URL
    Dnld-->>Priv: MPD data ready
    Priv->>Parse: ParseMPD(xmlData)
    Parse->>Parse: Parse periods, adaptation sets, representations
    Parse->>Utils: GetDuration(period)
    Parse->>Utils: GetSegmentTemplate(adaptationSet)
    Parse->>Utils: GetSegmentTimeline(segmentTemplate)
    Parse-->>Priv: Parsed MPD structure
`

## 2. Segment URL Resolution (Template-Based)

`mermaid
sequenceDiagram
    participant Collector as FragmentCollectorMPD
    participant Utils as AampMPDUtils
    participant Parse as AampMPDParseHelper

    Collector->>Utils: GetSegmentUrl(representation, segNum, time)
    Utils->>Utils: Check SegmentTemplate vs SegmentList vs SegmentBase
    alt SegmentTemplate with timeline
        Utils->>Utils: Replace $ with segmentNumber
        Utils->>Utils: Replace $ with timeline time
        Utils->>Utils: Replace $ with bitrate
        Utils->>Utils: Replace $ with repId
        Utils->>Utils: Resolve against BaseURL
    else SegmentTemplate with duration
        Utils->>Utils: Calculate segment number from time
        Utils->>Utils: Apply template substitution
    else SegmentList
        Utils->>Utils: Index into SegmentURL list
        Utils->>Utils: Resolve mediaRange
    else SegmentBase
        Utils->>Utils: Use single URL with byte-range indexing
    end
    Utils-->>Collector: Resolved absolute URL + byte range
`

## 3. Timeline Parsing

`mermaid
sequenceDiagram
    participant Parse as AampMPDParseHelper
    participant Utils as AampMPDUtils

    Parse->>Utils: ParseSegmentTimeline(timelineNode)
    loop Each S element in timeline
        Utils->>Utils: Extract t (start time), d (duration), r (repeat count)
        alt r == -1 (repeat until next)
            Utils->>Utils: Calculate repeats from period duration
        else r >= 0
            Utils->>Utils: Generate r+1 segment entries
        end
        Utils->>Utils: Store {startTime, duration} for each segment
    end
    Utils-->>Parse: Vector of segment timing entries
`

## 4. Period Info Extraction

`mermaid
sequenceDiagram
    participant Collector as FragmentCollectorMPD
    participant Parse as AampMPDParseHelper
    participant PInfo as AampMPDPeriodInfo

    Collector->>Parse: GetPeriods(mpd)
    loop Each period in MPD
        Parse->>PInfo: new AampMPDPeriodInfo(period)
        PInfo->>PInfo: Extract id, start, duration
        PInfo->>PInfo: Extract adaptation sets (video, audio, subtitle)
        PInfo->>PInfo: Extract content protection (DRM)
        PInfo->>PInfo: Determine period type (regular, ad, gap)
    end
    Parse-->>Collector: Vector<AampMPDPeriodInfo>
`

---

## Key Implementation Details

| Aspect | Implementation |
|--------|---------------|
| **Template variables** | `$`, `$`, `$`, `$` |
| **Timeline S element** | t=start, d=duration, r=repeat (-1=indefinite) |
| **BaseURL resolution** | Hierarchical: MPD → Period → AdaptationSet → Representation |
| **Segment numbering** | startNumber + offset from timeline position |
| **Duration calculation** | From Period@duration or sum of S@d in timeline |
| **Multi-period** | Sequential periods with unique IDs, gap detection |
