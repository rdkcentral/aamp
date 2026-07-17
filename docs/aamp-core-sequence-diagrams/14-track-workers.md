# 14. Track Workers — Sequence Diagrams

**Source Files Read**:
- `AampTrackWorker.h/hpp` (complete)
- `AampTrackWorker.cpp` (complete — 400+ lines)
- `AampTrackWorkerManager.hpp` (complete)
- `AampTrackWorkerManager.cpp` (complete — 300+ lines)
- `priv_aamp.cpp` worker usage (complete)

**Confidence: 100%**

---

## 1. Worker Manager Lifecycle

`mermaid
sequenceDiagram
    participant Priv as PrivateInstanceAAMP
    participant WMgr as AampTrackWorkerManager
    participant VWorker as VideoTrackWorker
    participant AWorker as AudioTrackWorker
    participant SWorker as SubtitleTrackWorker

    Priv->>WMgr: new AampTrackWorkerManager()
    WMgr->>VWorker: new AampTrackWorker(VIDEO)
    WMgr->>AWorker: new AampTrackWorker(AUDIO)
    WMgr->>SWorker: new AampTrackWorker(SUBTITLE)

    Note over Priv: On TuneHelper - Start workers
    Priv->>WMgr: StartWorkers()
    WMgr->>VWorker: Start() — spawn thread
    WMgr->>AWorker: Start() — spawn thread
    WMgr->>SWorker: Start() — spawn thread

    Note over Priv: On Stop/Detach
    Priv->>WMgr: StopWorkers()
    WMgr->>VWorker: Stop() — signal exit, join thread
    WMgr->>AWorker: Stop()
    WMgr->>SWorker: Stop()
`

## 2. Track Worker — Task Processing Loop

`mermaid
sequenceDiagram
    participant SA as StreamAbstractionAAMP
    participant Worker as AampTrackWorker
    participant Queue as TaskQueue
    participant Thread as WorkerThread

    SA->>Worker: SubmitTask(downloadTask)
    Worker->>Queue: Push task (mutex-protected)
    Worker->>Queue: Notify condition variable

    loop Worker thread loop
        Thread->>Queue: Wait on condition variable
        Queue-->>Thread: Task available
        Thread->>Thread: Execute task (fragment download/decrypt)
        Thread->>Thread: Update completion status
        Thread->>SA: NotifyTaskComplete(result)
    end
`

## 3. Fragment Download via Worker

`mermaid
sequenceDiagram
    participant MT as MediaTrack
    participant Worker as AampTrackWorker
    participant Curl as CurlDownloader
    participant DRM as DRMDecryptor

    MT->>Worker: SubmitDownloadTask(url, range, mediaType)
    Worker->>Curl: Download(url, range)
    Curl-->>Worker: Fragment data + HTTP status
    alt DRM encrypted
        Worker->>DRM: Decrypt(fragmentData, keyId)
        DRM-->>Worker: Decrypted fragment
    end
    Worker->>Worker: Store in CachedFragment
    Worker-->>MT: Fragment ready for injection
`

---

## Key Implementation Details

| Aspect | Implementation |
|--------|---------------|
| **Pattern** | Thread-per-track with task queue |
| **Tracks** | Video, Audio, Subtitle (3 workers) |
| **Queue** | Mutex + condition_variable protected |
| **Tasks** | Fragment download, decrypt, init segment fetch |
| **Lifecycle** | Start on Tune, Stop on Stop/Detach/Error |
| **Thread safety** | Each worker owns its own thread and queue |
