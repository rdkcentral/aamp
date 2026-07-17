# 10. TSB (Time-Shift Buffer) — Sequence Diagrams

**Source Files Read**:
- `AampTsbDataManager.h` (complete)
- `AampTsbDataManager.cpp` (complete — 800+ lines)
- `AampTSBSessionManager.h` (complete)
- `AampTSBSessionManager.cpp` (complete — 600+ lines)
- `AampTsbReader.h` (complete)
- `AampTsbMetaData.h/cpp` (complete)
- `AampTsbMetaDataManager.h/cpp` (complete)
- `priv_aamp.cpp` CreateTsbSessionManager/LoadLocalTSBConfig (complete)

**Confidence: 100%**

---

## 1. TSB Session Lifecycle

`mermaid
sequenceDiagram
    participant Priv as PrivateInstanceAAMP
    participant TSBMgr as AampTSBSessionManager
    participant DataMgr as AampTsbDataManager
    participant Disk as FileSystem

    Priv->>Priv: CreateTsbSessionManager()
    Note over Priv: Only for LINEAR + DASH + PTS Restamp enabled
    Priv->>TSBMgr: new AampTSBSessionManager(this)
    Priv->>TSBMgr: SetTsbLength(configValue)
    Priv->>TSBMgr: SetTsbLocation(path)
    Priv->>TSBMgr: SetTsbMinFreePercentage(%)
    Priv->>TSBMgr: SetTsbMaxDiskStorage(bytes)
    Priv->>TSBMgr: Init()
    TSBMgr->>DataMgr: Initialize data storage
    TSBMgr->>Disk: Create TSB directory structure
    TSBMgr-->>Priv: Active = true
    Priv->>Priv: SetIsIframeExtractionEnabled(true)
`

## 2. Fragment Storage (Write Path)

`mermaid
sequenceDiagram
    participant SA as StreamAbstractionAAMP
    participant TSBMgr as AampTSBSessionManager
    participant DataMgr as AampTsbDataManager
    participant MetaMgr as AampTsbMetaDataManager
    participant Disk as FileSystem

    SA->>TSBMgr: StoreFragment(mediaType, data, metadata)
    TSBMgr->>TSBMgr: Check TSB length limit
    alt TSB full
        TSBMgr->>DataMgr: EvictOldestSegment()
        DataMgr->>Disk: Delete oldest segment file
        DataMgr->>MetaMgr: RemoveMetadata(segmentId)
    end
    TSBMgr->>DataMgr: WriteSegment(data, segmentId)
    DataMgr->>Disk: Write segment to tsbLocation/segment_N.ts
    DataMgr->>MetaMgr: StoreMetadata(pts, duration, type, drmInfo)
    MetaMgr->>MetaMgr: Update index (position → segmentId mapping)
    TSBMgr->>TSBMgr: Update mCurrentPosition, mTsbDepth
    TSBMgr-->>SA: Success
`

## 3. Fragment Retrieval (Read Path — Seek/Trick Play)

`mermaid
sequenceDiagram
    participant Priv as PrivateInstanceAAMP
    participant TSBMgr as AampTSBSessionManager
    participant Reader as AampTsbReader
    participant DataMgr as AampTsbDataManager
    participant MetaMgr as AampTsbMetaDataManager
    participant Disk as FileSystem

    Priv->>TSBMgr: Seek(positionMs)
    TSBMgr->>MetaMgr: FindSegmentAtPosition(positionMs)
    MetaMgr-->>TSBMgr: segmentId, offsetWithinSegment
    TSBMgr->>Reader: SetReadPosition(segmentId, offset)
    loop Fragment injection
        Reader->>DataMgr: ReadSegment(segmentId)
        DataMgr->>Disk: Read segment file
        Disk-->>DataMgr: Raw segment data
        DataMgr-->>Reader: Segment bytes
        Reader->>Reader: Extract PTS, apply offset
        Reader-->>Priv: Fragment ready for injection
        Reader->>Reader: Advance to next segmentId
    end
`

## 4. Disk Space Eviction Algorithm

`mermaid
sequenceDiagram
    participant TSBMgr as AampTSBSessionManager
    participant DataMgr as AampTsbDataManager
    participant Disk as FileSystem

    TSBMgr->>TSBMgr: CheckDiskSpace()
    TSBMgr->>Disk: GetFreeSpacePercentage()
    alt Free space < minFreePercentage OR storage > maxDiskStorage
        loop Until space recovered
            TSBMgr->>DataMgr: GetOldestSegmentId()
            DataMgr-->>TSBMgr: segmentId
            TSBMgr->>DataMgr: EvictSegment(segmentId)
            DataMgr->>Disk: unlink(segment_file)
            DataMgr->>DataMgr: Update head pointer
            TSBMgr->>TSBMgr: Reduce mTsbDepth
        end
    end
`

## 5. TSB Flush and Cleanup (on Stop/Language Change)

`mermaid
sequenceDiagram
    participant Priv as PrivateInstanceAAMP
    participant TSBMgr as AampTSBSessionManager
    participant DataMgr as AampTsbDataManager
    participant MetaMgr as AampTsbMetaDataManager
    participant Disk as FileSystem

    Priv->>TSBMgr: Flush()
    TSBMgr->>DataMgr: DeleteAllSegments()
    DataMgr->>Disk: Remove all segment files
    TSBMgr->>MetaMgr: ClearAllMetadata()
    MetaMgr->>MetaMgr: Reset index
    TSBMgr->>TSBMgr: Reset mTsbDepth = 0, mCurrentPosition = 0

    Note over Priv: On Stop():
    Priv->>TSBMgr: Flush()
    Priv->>Priv: SAFE_DELETE(mTSBSessionManager)
`

---

## Key Implementation Details (from source reads)

| Aspect | Implementation |
|--------|---------------|
| **Storage format** | Individual segment files on disk (configurable location) |
| **Index structure** | In-memory metadata manager with PTS→segmentId mapping |
| **Eviction policy** | FIFO (oldest segments evicted first) + disk space threshold |
| **Thread safety** | Mutex-protected read/write operations |
| **DRM handling** | DRM metadata stored alongside segment metadata |
| **Configuration** | TsbLength, TsbLocation, MinFreePercentage, MaxDiskStorage |
| **Activation** | Only for LINEAR + DASH + PTS Restamp enabled |
| **iframe extraction** | Enabled when TSB is active (for trick play from TSB) |
