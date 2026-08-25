# HLS Fragment Collector — Sequence Diagrams

## Module: `fragmentcollector_hls.h` / `fragmentcollector_hls.cpp`
## Class: `StreamAbstractionAAMP_HLS` + `TrackState`

---

## 1. HLS Init & Main Manifest Parsing

```mermaid
sequenceDiagram
    participant AAMP as PrivateInstanceAAMP
    participant HLS as StreamAbstractionAAMP_HLS
    participant Track as TrackState (Video/Audio/Subtitle)
    participant ABR as AampABRManager
    participant DRM as AampDRMLicenseManager

    AAMP->>HLS: Init(tuneType)
    HLS->>HLS: FetchMainManifest()
    HLS->>HLS: ParseMainManifest()
    Note over HLS: Parse #EXT-X-STREAM-INF → streamInfoStore[]
    Note over HLS: Parse #EXT-X-MEDIA → mediaInfoStore[]
    Note over HLS: Parse #EXT-X-I-FRAME-STREAM-INF → iframe profiles
    Note over HLS: Parse #EXT-X-IMAGE-STREAM-INF → thumbnail profiles
    Note over HLS: Parse #EXT-X-SESSION-KEY → early DRM init
    HLS->>ABR: clearProfiles() + configure profiles
    alt #EXT-X-SESSION-KEY found
        HLS->>HLS: InitiateDrmProcess()
        HLS->>DRM: QueueProtectionEvent(drmHelper)
        HLS->>DRM: QueueContentProtection(drmHelper)
    end
    HLS->>HLS: Select video profile (ABR)
    HLS->>HLS: Select audio track (language/codec match)
    HLS->>Track: Create TrackState(eTRACK_VIDEO)
    HLS->>Track: Create TrackState(eTRACK_AUDIO)
    opt Subtitle enabled
        HLS->>Track: Create TrackState(eTRACK_SUBTITLE)
    end
    HLS->>Track: FetchPlaylist() per track
    HLS->>Track: IndexPlaylist() per track
    HLS-->>AAMP: return eAAMPSTATUS_OK
```

---

## 2. Playlist Indexing (TrackState::IndexPlaylist)

```mermaid
sequenceDiagram
    participant Track as TrackState
    participant Playlist as playlist[] buffer

    Track->>Track: IndexPlaylist(IsRefresh, culledSec)
    Track->>Track: FlushIndex() (if not refresh)
    loop Each line in playlist
        alt #EXTINF:duration
            Track->>Track: Accumulate duration → IndexNode.completionTimeSecondsFromStart
        else #EXT-X-KEY:METHOD=...
            Track->>Track: ParseKeyAttributeCallback()
            Note over Track: Set mDrmMethod (NONE/AES-128/SAMPLE-AES-CTR)
            Note over Track: Update mDrmInfo.keyURI, IV, keyFormat
            Track->>Track: UpdateDrmCMSha1Hash() / UpdateDrmIV()
        else #EXT-X-MAP:
            Track->>Track: Store initFragmentInfo
            Track->>Track: Set mInjectInitFragment = true
        else #EXT-X-DISCONTINUITY
            Track->>Track: Add DiscontinuityIndexNode
        else #EXT-X-PROGRAM-DATE-TIME
            Track->>Track: Store startTimeForPlaylistSync
        else #EXT-X-BYTERANGE
            Track->>Track: Store byteRangeLength/Offset
        else #EXT-X-ENDLIST
            Track->>Track: Set mReachedEndListTag = true
        else URI line
            Track->>Track: index.push_back(IndexNode{duration, seqNo, fragInfo, drmIdx})
        end
    end
    Track->>Track: FindTimedMetadata()
    Track->>Track: ComputeDeferredKeyRequestTime()
```

---

## 3. Fragment Fetch Loop (TrackState::FragmentCollector)

```mermaid
sequenceDiagram
    participant Track as TrackState
    participant HLS as StreamAbstractionAAMP_HLS
    participant AAMP as PrivateInstanceAAMP
    participant DRM as DRM Decrypt
    participant Cache as CachedFragment

    Track->>Track: FragmentCollector() [thread entry]
    loop Until EOS or abort
        alt Normal playback
            Track->>Track: FetchFragment()
            Track->>Track: FetchFragmentHelper(http_error, decryption_error)
            Track->>Track: GetNextFragmentUriFromPlaylist(reloadUri)
            Note over Track: Walk playlist lines, match playTarget vs playlistPosition
            Note over Track: Handle #EXT-X-DISCONTINUITY sync with other track
            alt Init fragment needed (mInjectInitFragment)
                Track->>Track: FetchInitFragment()
                Track->>Track: FetchInitFragmentHelper()
            end
            Track->>AAMP: Download fragment (HTTP GET)
            alt Fragment encrypted
                Track->>DRM: DrmDecrypt(cachedFragment, bucketType)
                alt AES-128
                    Track->>DRM: AES decrypt with IV
                else SAMPLE-AES-CTR
                    Track->>DRM: CDM decrypt via DrmInterface
                end
            end
            Track->>Cache: Store in CachedFragment ring buffer
        else Trickplay (iframe)
            Track->>Track: GetIframeFragmentUriFromIndex(bSegmentRepeated)
            Note over Track: Binary search in index[] by playTarget
            Track->>AAMP: Download iframe fragment
            Track->>Cache: Store in cache
        end
        alt Playlist refresh needed (live)
            Track->>Track: RefreshPlaylist()
            Track->>Track: FetchPlaylist()
            Track->>Track: IndexPlaylist(IsRefresh=true)
        end
    end
```

---

## 4. Fragment URI Resolution (GetNextFragmentUriFromPlaylist)

```mermaid
sequenceDiagram
    participant Track as TrackState
    participant Other as TrackState (other track)
    participant Context as StreamAbstractionAAMP_HLS

    Track->>Track: GetNextFragmentUriFromPlaylist(reloadUri)
    Note over Track: Start from current fragmentURI position in playlist
    loop Walk playlist lines
        alt #EXTINF:duration
            Track->>Track: playlistPosition += fragmentDurationSeconds
            Track->>Track: fragmentDurationSeconds = new duration
        else #EXT-X-KEY:
            Track->>Track: ParseKeyAttributeCallback → update DRM state
        else #EXT-X-DISCONTINUITY
            Track->>Track: discontinuity = true
        else #EXT-X-PROGRAM-DATE-TIME
            Track->>Track: Store programDateTime for sync
        else URI line
            alt playlistPosition + duration > playTarget
                Note over Track: Found target fragment
                alt discontinuity && other track enabled
                    Track->>Other: HasDiscontinuityAroundPosition()
                    alt Other track has matching discontinuity
                        Track->>Track: Confirm discontinuity
                    else No match
                        Track->>Track: Ignore discontinuity
                    end
                    alt Other track ahead by > targetDuration
                        Track->>Track: Skip fragment, advance playTarget
                    end
                end
                Track-->>Track: return URI
            else
                Track->>Track: Skip, continue walking
            end
        end
    end
```

---

## 5. DRM Key Handling in HLS

```mermaid
sequenceDiagram
    participant Track as TrackState
    participant DRM as HlsDrmBase / DrmInterface
    participant LicMgr as AampDRMLicenseManager
    participant AAMP as PrivateInstanceAAMP

    Note over Track: Key tag detected during IndexPlaylist or GetNextFragment
    Track->>Track: ParseKeyAttributeCallback()
    alt METHOD=AES-128
        Track->>Track: mDrmMethod = eDRM_KEY_METHOD_AES_128
        Track->>Track: mDrmInfo.keyURI = URI value
        Track->>Track: UpdateDrmIV() or CreateInitVectorByMediaSeqNo()
        Track->>Track: SetDrmContext()
        Track->>DRM: Initialize AES decrypt with key + IV
    else METHOD=SAMPLE-AES-CTR
        Track->>Track: mDrmMethod = eDRM_KEY_METHOD_SAMPLE_AES_CTR
        Track->>Track: InitiateDRMKeyAcquisition()
        Track->>LicMgr: QueueProtectionEvent(drmHelper)
        Track->>LicMgr: QueueContentProtection(drmHelper)
    else METHOD=NONE
        Track->>Track: fragmentEncrypted = false
        Track->>Track: Clear DRM context
    end
    Note over Track: On fragment download
    Track->>Track: DrmDecrypt(cachedFragment, bucketType)
    Track->>DRM: Decrypt fragment data
```

---

## 6. ABR Profile Switch

```mermaid
sequenceDiagram
    participant AAMP as PrivateInstanceAAMP
    participant HLS as StreamAbstractionAAMP_HLS
    participant Video as TrackState (Video)
    participant ABR as AampABRManager

    AAMP->>ABR: CheckForProfileChange()
    ABR-->>AAMP: New profile index
    AAMP->>HLS: ABRProfileChanged()
    HLS->>Video: ABRProfileChanged()
    Video->>Video: Update mPlaylistUrl to new bitrate URI
    Video->>Video: FetchPlaylist() (new variant)
    Video->>Video: IndexPlaylist(IsRefresh=false)
    Video->>Video: FindMediaForSequenceNumber() → resume at correct seq#
    Video->>Video: Set isFirstFragmentAfterABR = true
```

---

## 7. Start / Stop Flow

```mermaid
sequenceDiagram
    participant HLS as StreamAbstractionAAMP_HLS
    participant Video as TrackState (Video)
    participant Audio as TrackState (Audio)
    participant Sub as TrackState (Subtitle)

    HLS->>Video: Start()
    HLS->>Audio: Start()
    opt Subtitle
        HLS->>Sub: Start()
    end
    Note over Video: Launch fragmentCollectorThread → FragmentCollector()
    Note over Audio: Launch fragmentCollectorThread → FragmentCollector()

    Note over HLS: ... playback in progress ...

    HLS->>Video: Stop(clearDRM)
    HLS->>Audio: Stop(clearDRM)
    opt Subtitle
        HLS->>Sub: Stop(clearDRM)
    end
    Video->>Video: CancelDrmOperation(clearDRM)
    Video->>Video: StopWaitForPlaylistRefresh()
    Video->>Video: StopInjection()
    Audio->>Audio: CancelDrmOperation(clearDRM)
    Audio->>Audio: StopInjection()
```

---

## Source Coverage

| File | Lines Read | Confidence |
|------|-----------|------------|
| `fragmentcollector_hls.h` | 1–800 (complete) | 100% |
| `fragmentcollector_hls.cpp` | 1–1200 | 85% (key methods: ParseMainManifest, GetIframeFragmentUriFromIndex, GetNextFragmentUriFromPlaylist, FindMediaForSequenceNumber fully read; remaining: FetchFragment, RefreshPlaylist, IndexPlaylist impl details) |

**Overall Module Confidence: 90%**
- Gap: `fragmentcollector_hls.cpp` lines 1200+ (FetchFragment/FetchFragmentHelper implementation, full IndexPlaylist impl, RefreshPlaylist logic)
