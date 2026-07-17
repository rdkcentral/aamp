# 03 — Stream Abstraction & MediaTrack Lifecycle

## Overview

`StreamAbstractionAAMP` is the base class for all stream collectors (HLS, DASH, Progressive, shims). `MediaTrack` is the per-track (Video/Audio/Subtitle) base class managing a ring-buffer cache, fragment fetch/inject loops, buffer health monitoring, and playlist downloading.

**Source files read:**
- `StreamAbstractionAAMP.h` (lines 1–1400) — complete
- `streamabstraction.cpp` (lines 1–1400) — partial (covers MediaTrack core methods)

**Confidence: 90%** (remaining: streamabstraction.cpp lines 1400+ not yet read — contains RunInjectLoop, StreamAbstractionAAMP methods)

---

## 1. MediaTrack Fragment Fetch → Cache → Inject Loop

```mermaid
sequenceDiagram
    participant FC as FragmentCollector<br/>(HLS/MPD subclass)
    participant MT as MediaTrack
    participant Cache as RingBuffer<br/>(mCachedFragment[])
    participant IL as InjectLoop Thread
    participant Sink as AAMPGstPlayer

    Note over FC,Sink: Fragment Fetch Phase
    FC->>MT: GetFetchBuffer(initialize=true)
    MT-->>FC: CachedFragment* slot (fragmentIdxToFetch)
    FC->>FC: Download fragment (curl)
    FC->>MT: UpdateTSAfterFetchStats(cachedFragment, isInitSegment)
    MT->>MT: totalFetchedDuration += duration
    MT->>MT: Check initial caching complete
    MT->>MT: Handle audio/subtitle track switch
    FC->>MT: UpdateTSAfterFetch()
    MT->>Cache: numberOfFragmentsCached++
    MT->>Cache: fragmentIdxToFetch = (idx+1) % size
    MT->>IL: fragmentFetched.notify_one()

    Note over FC,Sink: Fragment Inject Phase
    IL->>MT: WaitForCachedFragmentAvailable()
    MT->>MT: Wait on fragmentFetched CV if cache empty
    IL->>MT: InjectFragment()
    MT->>MT: CheckForDiscontinuity(cachedFragment)
    alt Discontinuity detected
        MT->>MT: ProcessDiscontinuity(type)
        MT->>MT: May stop injection for pipeline reconfigure
    end
    MT->>MT: ProcessAndInjectFragment(cachedFragment)
    alt LLD Chunk Mode
        MT->>MT: ProcessFragmentChunk()
        MT->>MT: Parse ISOBMFF chunks
        MT->>Sink: InjectFragmentChunkInternal()
    else Normal Mode
        alt PTS Restamp enabled (DASH)
            alt Trickplay
                MT->>MT: TrickModePtsRestamp(cachedFragment)
            else Normal
                MT->>MT: IsoBmffHelper::RestampPts()
            end
        end
        MT->>Sink: InjectFragmentInternal(cachedFragment)
    end
    MT->>MT: UpdateTSAfterInject()
    MT->>Cache: numberOfFragmentsCached--
    MT->>Cache: fragmentIdxToInject = (idx+1) % size
    MT->>FC: fragmentInjected.notify_one()
```

---

## 2. MediaTrack Playlist Downloader Thread

```mermaid
sequenceDiagram
    participant MT as MediaTrack
    participant PD as PlaylistDownloader Thread
    participant Sub as Subclass<br/>(HLS/MPD Track)

    MT->>PD: StartPlaylistDownloaderThread()
    PD->>PD: new std::thread(PlaylistDownloader)
    loop Until abortPlaylistDownloader
        PD->>MT: WaitTimeBasedOnBufferAvailable()
        MT-->>PD: waitTime (ms)
        PD->>PD: EnterTimedWaitForPlaylistRefresh(waitTime)
        PD->>Sub: GetPlaylistUrl()
        PD->>PD: Download playlist
        PD->>Sub: ProcessPlaylist(newPlaylist, http_error)
        PD->>Sub: SetLastPlaylistDownloadTime(now)
    end

    Note over MT,PD: Shutdown
    MT->>PD: StopPlaylistDownloaderThread()
    MT->>MT: abortPlaylistDownloader = true
    MT->>MT: AbortWaitForPlaylistDownload()
    MT->>MT: AbortWaitForManifestUpdate()
    PD->>PD: thread.join()
```

---

## 3. Buffer Health Monitoring

```mermaid
sequenceDiagram
    participant MT as MediaTrack
    participant BM as BufferMonitor Thread
    participant AAMP as PrivateInstanceAAMP

    MT->>BM: ScheduleBufferHealthMonitor()
    BM->>BM: Sleep(bufferHealthMonitorDelay - interval)
    loop Until abort
        BM->>BM: Sleep(bufferHealthMonitorInterval)
        BM->>MT: GetBufferStatus()
        MT->>MT: Calculate bufferedTime = injected - elapsed
        alt bufferedTime <= underflowThreshold
            MT-->>BM: BUFFER_STATUS_RED
        else bufferedTime <= greenThreshold
            MT-->>BM: BUFFER_STATUS_YELLOW
        else
            MT-->>BM: BUFFER_STATUS_GREEN
        end
        alt Status changed
            BM->>AAMP: profiler.IncrementChangeCount(BufferChange)
        end
        BM->>BM: CheckForMediaTrackInjectionStall(type)
        alt Discontinuity pending & not paused
            BM->>AAMP: CheckForDiscontinuityStall(type)
        end
        alt Underflow + GREEN + video + downloads disabled
            BM->>AAMP: StopBuffering(true) [deadlock recovery]
        end
    end
```

---

## 4. StreamAbstractionAAMP Class Hierarchy

```mermaid
sequenceDiagram
    participant App as Application
    participant AAMP as PrivateInstanceAAMP
    participant SA as StreamAbstractionAAMP
    participant VT as MediaTrack (Video)
    participant AT as MediaTrack (Audio)
    participant ST as MediaTrack (Subtitle)

    App->>AAMP: Tune(url)
    AAMP->>SA: new StreamAbstractionAAMP_XXX() [HLS/MPD/Progressive]
    AAMP->>SA: Init(tuneType)
    SA->>SA: Parse manifest, select profiles
    SA->>VT: new TrackState/MediaTrack (video)
    SA->>AT: new TrackState/MediaTrack (audio)
    SA->>ST: new TrackState/MediaTrack (subtitle)

    AAMP->>SA: Start()
    SA->>VT: StartInjectLoop()
    SA->>AT: StartInjectLoop()
    SA->>ST: StartInjectLoop()
    SA->>VT: StartPlaylistDownloaderThread()
    SA->>AT: StartPlaylistDownloaderThread()

    Note over SA,ST: Playback running...

    AAMP->>SA: Stop(clearChannelData)
    SA->>VT: StopInjectLoop()
    SA->>AT: StopInjectLoop()
    SA->>ST: StopInjectLoop()
    SA->>VT: StopPlaylistDownloaderThread()
    SA->>AT: StopPlaylistDownloaderThread()
```

---

## 5. Trick Mode PTS Restamping (DASH)

```mermaid
sequenceDiagram
    participant MT as MediaTrack
    participant Helper as IsoBmffHelper

    Note over MT: Init Fragment (rate change / discontinuity)
    MT->>Helper: SetTimescale(fragment, TRICKMODE_TIMESCALE=100000)
    MT->>Helper: ClearMediaHeaderDuration(fragment)
    MT->>MT: Set mTrickmodeState = FIRST_FRAGMENT or DISCONTINUITY

    Note over MT: First Media Fragment
    MT->>MT: mRestampedDuration = MAX(duration/|rate|, 1/trickPlayFPS)
    MT->>MT: mTrickmodeState = STEADY

    Note over MT: Subsequent Media Fragments (STEADY)
    MT->>MT: fragmentPtsDelta = |position - mLastFragmentPts|
    MT->>MT: mRestampedDuration = delta / |rate|
    MT->>MT: mRestampedPts += mRestampedDuration
    MT->>Helper: SetPtsAndDuration(fragment, restampedPts*TIMESCALE, restampedDuration*TIMESCALE)
    MT->>MT: position = mRestampedPts (for GStreamer)
```

---

## 6. Discontinuity Processing

```mermaid
sequenceDiagram
    participant MT as MediaTrack
    participant SA as StreamAbstractionAAMP
    participant AAMP as PrivateInstanceAAMP

    MT->>MT: CheckForDiscontinuity(cachedFragment)
    alt cachedFragment.discontinuity || ptsError
        MT->>AAMP: IsDiscontinuityIgnoredForOtherTrack(!type)
        alt injectedDuration == 0 && no ESChange && pipeline valid
            MT->>AAMP: SetTrackDiscontinuityIgnoredStatus(type)
            MT->>MT: Continue injection (ignore discontinuity)
        else Other track ignored && no ESChange && pipeline valid
            MT->>AAMP: ResetTrackDiscontinuityIgnoredStatus()
            MT->>AAMP: UnblockWaitForDiscontinuityProcessToComplete()
        else Normal discontinuity processing
            alt PTS Restamp enabled
                MT->>SA: ProcessDiscontinuity(type)
                alt ESChange or PipelineFlush
                    SA-->>MT: stopInjection = true
                end
            else
                MT->>SA: ProcessDiscontinuity(type)
            end
            alt stopInjection
                MT->>MT: ret = false, discontinuityProcessed = true
            end
        end
    end
```

---

## Key Data Structures (from source)

| Structure | Purpose |
|-----------|---------|
| `MediaTrack::mCachedFragment[]` | Ring buffer of `MAX_CACHED_FRAGMENTS_PER_TRACK` slots |
| `fragmentIdxToFetch` / `fragmentIdxToInject` | Ring buffer read/write cursors |
| `numberOfFragmentsCached` | Current fill level |
| `fragmentFetched` (CV) | Signals inject thread when fragment available |
| `fragmentInjected` (CV) | Signals fetch thread when slot freed |
| `mutex` | Protects ring buffer state |
| `mTrackParamsMutex` | Leaf-level; protects duration counters |
| `totalFetchedDuration` / `totalInjectedDuration` | Accumulated durations for buffer math |
| `bufferStatus` | GREEN/YELLOW/RED health indicator |

---

## Lock Ordering (from source comment)

1. `mutex` — protects ring buffer
2. `mTrackParamsMutex` — protects lifetime counters (leaf-level, never hold another lock while holding this)

Holding `mutex` then taking `mTrackParamsMutex` is permitted.
