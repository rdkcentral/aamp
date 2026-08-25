# 05 — DASH/MPD Fragment Collector Sequence Diagrams

## Module Overview

The `StreamAbstractionAAMP_MPD` class implements DASH (MPEG-DASH) streaming. It extends `StreamAbstractionAAMP` and handles MPD manifest parsing, period management, segment timeline navigation, fragment fetching (with parallel download support), DRM license pre-fetching, and ad insertion (CDAI).

**Source Files Read:**
- `fragmentcollector_mpd.h` lines 1–600 ✅
- `fragmentcollector_mpd.cpp` lines 1–1000 ✅

**Confidence: 85%**
- Gap: `fragmentcollector_mpd.cpp` lines 1000+ (Init(), Start(), manifest refresh loop, period transitions)

---

## 5.1 — MPD Init & Period Selection

```mermaid
sequenceDiagram
    participant AAMP as PrivateInstanceAAMP
    participant MPD as StreamAbstractionAAMP_MPD
    participant Parser as libdash (IMPD)
    participant DRM as AampDRMLicPreFetcher
    participant Track as MediaStreamContext

    AAMP->>MPD: Init(tuneType)
    MPD->>MPD: Download MPD manifest
    MPD->>Parser: Parse MPD XML → IMPD object
    Parser-->>MPD: mpd (IMPD*)
    MPD->>MPD: Determine live/VOD (mIsLiveStream)
    MPD->>MPD: Select current period (mCurrentPeriodIdx)
    MPD->>MPD: Get adaptation sets for period
    MPD->>MPD: Select video adaptation (resolution/codec match)
    MPD->>MPD: Select audio adaptation (language/codec preference)
    Note over MPD: Audio codec priority: AC4 > ATMOS > DD+ > AAC
    MPD->>MPD: Select subtitle adaptation (if available)
    MPD->>MPD: Build profile/bitrate index (mBitrateIndexVector)
    MPD->>Track: Create MediaStreamContext per track
    MPD->>DRM: SetLicenseFetcher(this)
    MPD->>MPD: Configure DRM preferences (PlayReady > Widevine > ClearKey)
    MPD-->>AAMP: AAMPStatusType (eAAMPSTATUS_OK)
```

---

## 5.2 — Fragment Fetch Flow (FetchFragment)

```mermaid
sequenceDiagram
    participant Fetcher as FragmentCollectorLoop
    participant MPD as StreamAbstractionAAMP_MPD
    participant MSC as MediaStreamContext
    participant Worker as AampTrackWorkerManager
    participant BufMgr as TimeBasedBufferManager
    participant Curl as DownloadEngine

    Fetcher->>MPD: PushNextFragment(pMediaStreamContext, curlInstance)
    MPD->>MPD: Get SegmentTemplate from representation/adaptationSet
    MPD->>MPD: Check SegmentTimeline exists
    alt SegmentTimeline present
        MPD->>MPD: Navigate timelines[] by timeLineIndex
        MPD->>MPD: Handle presentationTimeOffset
        MPD->>MPD: Handle timescale changes (workaround)
        MPD->>MPD: Resolve segment URL from template + time/number
    end
    MPD->>MPD: FetchFragment(mediaStreamContext, media, duration, isInit, ...)

    MPD->>MPD: GenerateFragmentURLList (video) → URLBitrateMap
    alt URLList empty (audio/subtitle)
        MPD->>MPD: ConstructFragmentURL from fragmentDescriptor
    end

    MPD->>MPD: Create DownloadInfo (type, curlInstance, duration, range, PTS, URLs)
    MPD->>MPD: Create MediaSegmentDownloadJob (lambda wraps DownloadFragment)

    MPD->>BufMgr: PopulateBuffer(fragmentDuration)

    alt Parallel download enabled & not SegmentBase init
        MPD->>Worker: SubmitJob(mediaType, downloadJob, profileChanged)
        Worker->>Curl: Async download execution
    else Sequential download
        MPD->>MSC: downloadJob->Execute()
        MSC->>Curl: DownloadFragment(downloadInfo)
        Curl-->>MSC: Fragment data
        MSC-->>MPD: OnFragmentDownloadComplete(status, downloadInfo)
    end

    MPD->>MSC: Update fragmentTime += fragmentDuration
    MPD->>MPD: Update mBasePeriodOffset
    MPD-->>Fetcher: retval (success/failure)
```

---

## 5.3 — Timeline Position Recovery (After Manifest Refresh)

```mermaid
sequenceDiagram
    participant MPD as StreamAbstractionAAMP_MPD
    participant MSC as MediaStreamContext
    participant Timeline as ISegmentTimeline

    Note over MPD: After manifest refresh, need to find position
    MPD->>MPD: FindPositionInTimeline(pMediaStreamContext, timelines)
    MPD->>Timeline: Iterate timelines[]
    loop For each timeline entry
        MPD->>Timeline: GetStartTime(), GetDuration(), GetRepeatCount()
        MPD->>MPD: Calculate nextStartTime = start + (repeat+1)*duration
        alt lastSegmentTime < nextStartTime
            MPD->>MPD: Break — found the right row
        else
            MPD->>MSC: fragmentDescriptor.Number += (repeatCount+1)
        end
    end
    MPD->>MPD: Traverse repeat index within row
    loop While fragmentRepeatCount < repeatCount && startTime < lastSegmentTime
        MPD->>MSC: startTime += duration
        MPD->>MSC: fragmentDescriptor.Number++
        MPD->>MSC: fragmentRepeatCount++
    end
    MPD-->>MPD: Return startTime (position found)
```

---

## 5.4 — Audio Codec Selection

```mermaid
sequenceDiagram
    participant MPD as StreamAbstractionAAMP_MPD
    participant AdapSet as IAdaptationSet
    participant Rep as IRepresentation

    MPD->>MPD: GetPreferredCodecIndex(adaptationSet, ...)
    MPD->>AdapSet: GetRepresentation()
    loop For each representation
        MPD->>Rep: GetCodecs() / GetBandwidth()
        MPD->>MPD: getCodecType(codecValue, rep)
        alt codecValue == "ec+3"
            MPD->>MPD: audioType = eAUDIO_ATMOS
        else codecValue starts with "ac-4"
            MPD->>MPD: audioType = eAUDIO_DOLBYAC4
        else codecValue == "ac-3"
            MPD->>MPD: audioType = eAUDIO_DOLBYAC3
        else codecValue == "ec-3"
            MPD->>MPD: audioType = eAUDIO_DDPLUS
            MPD->>MPD: IsAtmosAudio(rep) → check SupplementalProperty
            alt ETSI TS 103 420 Atmos flag set
                MPD->>MPD: audioType = eAUDIO_ATMOS
            end
        else codecValue contains "mp4" or "aac"
            MPD->>MPD: audioType = eAUDIO_AAC
        end
        MPD->>MPD: Calculate score (preferredCodecList position * CODEC_SCORE + codecType)
        alt Codec disabled by config (disableATMOS/disableEC3/disableAC4/disableAC3)
            MPD->>MPD: score = 0
        end
        alt score > bestScore OR (same score + better bandwidth)
            MPD->>MPD: Select this representation
        end
    end
    MPD-->>MPD: Return selectedRepIdx, selectedCodecType, selectedRepBandwidth
```

---

## 5.5 — Constructor & DRM Preference Setup

```mermaid
sequenceDiagram
    participant AAMP as PrivateInstanceAAMP
    participant MPD as StreamAbstractionAAMP_MPD
    participant LicMgr as AampDRMLicenseManager
    participant ABR as ABRManager

    AAMP->>MPD: new StreamAbstractionAAMP_MPD(aamp, seekPos, rate)
    MPD->>MPD: Initialize all member variables
    MPD->>MPD: Default DRM prefs: {ClearKey:1, Widevine:2, PlayReady:3}
    MPD->>LicMgr: SetLicenseFetcher(this)
    MPD->>ABR: clearProfiles()
    MPD->>MPD: mLastPlaylistDownloadTimeMs = now()

    MPD->>AAMP: GetPreferredDRM()
    alt eDRM_WideVine
        MPD->>MPD: mDrmPrefs[WIDEVINE_UUID] = highest+1
    else eDRM_ClearKey
        MPD->>MPD: mDrmPrefs[CLEARKEY_UUID] = highest+1
    else eDRM_PlayReady (default)
        MPD->>MPD: mDrmPrefs[PLAYREADY_UUID] = highest+1
    end

    MPD->>MPD: trickplayMode = (rate != NORMAL_PLAY_RATE)
```

---

## Key Classes & Relationships

| Class | Role |
|-------|------|
| `StreamAbstractionAAMP_MPD` | Main DASH fragment collector, extends `StreamAbstractionAAMP` |
| `MediaStreamContext` | Per-track state (fragmentDescriptor, timeLineIndex, lastSegmentTime) |
| `TimeSyncClient` | UTC time sync with remote server for live streams |
| `AampDashWorkerJob` | Wrapper for parallel download jobs |
| `HeaderFetchParams` | Init segment fetch parameters |
| `FragmentDownloadParams` | Fragment download context |
| `ProfileInfo` | Maps profile index → adaptationSet + representation indices |

---

## Key Constants

| Constant | Value | Purpose |
|----------|-------|---------|
| `SEGMENT_COUNT_FOR_ABR_CHECK` | 5 | Segments before ABR decision |
| `MIN_TSB_BUFFER_DEPTH` | 6 sec | Minimum TSB buffer (DASH-IF IOP) |
| `MAX_MANIFEST_DOWNLOAD_RETRY_MPD` | 2 | Manifest download retries |
| `TIMELINE_START_RESET_DIFF` | 4000000000 | Timeline discontinuity threshold |
