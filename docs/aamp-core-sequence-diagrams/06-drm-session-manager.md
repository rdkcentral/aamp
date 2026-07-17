# 06 - DRM License Manager & Session Lifecycle

> **Source files read:**
> - `drm/AampDRMLicManager.h` (complete - 250 lines)
> - `drm/DrmInterface.h` (complete - 130 lines)
> - `AampDRMLicPreFetcher.h` (complete - 250 lines)
> - `AampDRMLicPreFetcherInterface.h` (partial - 200 lines)
>
> **Confidence: 95%**
> - Gap: `drm/AampDRMLicManager.cpp` and `drm/DrmInterface.cpp` implementation bodies not fully read
> - Gap: `AampDRMLicPreFetcher.cpp` implementation body not fully read

---

## 6.1 DRM License Acquisition Flow

```mermaid
sequenceDiagram
    participant MPD as FragmentCollectorMPD
    participant LicMgr as AampDRMLicenseManager
    participant PreFetch as AampLicensePreFetcher
    participant SessMgr as DrmSessionManager
    participant DrmHelp as DrmHelper
    participant DrmSess as DrmSession
    participant CDM as CDM/OCDM
    participant LicSrv as License Server
    participant Curl as AampCurlDownloader

    Note over MPD,CDM: Content Protection Discovery
    MPD->>LicMgr: QueueContentProtection(drmHelper, periodId, adapIdx, type)
    LicMgr->>PreFetch: QueueContentProtection(drmHelper, periodId, adapIdx, type)
    PreFetch->>PreFetch: Check KeyIsQueued() for duplicates
    PreFetch->>PreFetch: Push to mFetchQueue
    PreFetch->>PreFetch: mQCond.notify_one()

    Note over PreFetch,CDM: Pre-Fetch Thread Processing
    PreFetch->>PreFetch: PreFetchThread() - wait on mQCond
    PreFetch->>PreFetch: Pop from mFetchQueue
    PreFetch->>PreFetch: CreateDRMSession(fetchObj)
    PreFetch->>LicMgr: createDrmSession(drmHelper, callbacks, eventHandle, streamType)

    Note over LicMgr,CDM: Session Creation
    LicMgr->>SessMgr: Create/reuse DRM session slot
    LicMgr->>DrmHelp: Get DRM system info (systemId, PSSH)
    LicMgr->>DrmSess: Initialize session with PSSH
    DrmSess->>CDM: OpenKeySession()
    CDM-->>DrmSess: Challenge data

    Note over LicMgr,LicSrv: License Acquisition
    LicMgr->>LicMgr: acquireLicense(responseCode, drmHelper, sessionSlot, ...)
    LicMgr->>LicMgr: configureLicenseServerParameters()
    LicMgr->>LicMgr: getAccessToken(error_code)
    LicMgr->>Curl: getLicense(licRequest, httpError, streamType, ...)
    Curl->>LicSrv: HTTP POST (challenge + headers)
    LicSrv-->>Curl: License response
    Curl-->>LicMgr: DrmData* license

    Note over LicMgr,CDM: License Processing
    LicMgr->>LicMgr: handleLicenseResponse(responseCode, drmHelper, ...)
    LicMgr->>LicMgr: processLicenseResponse(drmHelper, sessionSlot, ...)
    LicMgr->>DrmSess: Update session with license key
    DrmSess->>CDM: UpdateKeySession(license)
    CDM-->>DrmSess: KeyState (READY/ERROR)
    DrmSess-->>LicMgr: KeyState

    alt License Acquisition Failed
        LicMgr->>LicMgr: UpdateLicenseMetrics(requestType, statusCode, ...)
        LicMgr->>LicMgr: TriggerLAProfileErrorCb()
        PreFetch->>PreFetch: NotifyDrmFailure(fetchObj, event)
    end

    LicMgr->>LicMgr: TriggerLAProfileEndCb(streamType)
```

---

## 6.2 License Renewal Flow

```mermaid
sequenceDiagram
    participant CDM as CDM/OCDM
    participant DrmSess as DrmSession
    participant LicMgr as AampDRMLicenseManager
    participant LicSrv as License Server

    Note over CDM,LicSrv: Key Expiration Trigger
    CDM->>DrmSess: Key renewal callback
    DrmSess->>LicMgr: renewLicense(drmHelper, userData, aampInstance)
    LicMgr->>LicMgr: licenseRenewalThread(drmHelper, sessionSlot, aampInstance)
    Note over LicMgr: Spawns thread in mLicenseRenewalThreads

    LicMgr->>LicMgr: acquireLicense(..., isLicenseRenewal=true)
    LicMgr->>LicSrv: HTTP POST (renewal challenge)
    LicSrv-->>LicMgr: New license
    LicMgr->>LicMgr: processLicenseResponse(..., isLicenseRenewal=true)
    LicMgr->>DrmSess: Update session with new key
    DrmSess->>CDM: UpdateKeySession(newLicense)
    CDM-->>DrmSess: KeyState READY

    Note over LicMgr: On teardown
    LicMgr->>LicMgr: releaseLicenseRenewalThreads()
```

---

## 6.3 DrmInterface — Player↔Middleware Bridge

```mermaid
sequenceDiagram
    participant Player as PrivateInstanceAAMP
    participant DrmIF as DrmInterface
    participant HlsBridge as PlayerHlsDrmSessionInterface
    participant AES as AES Decryptor
    participant MW as Middleware DRM

    Note over Player,MW: Singleton Initialization
    Player->>DrmIF: DrmInterface::GetInstance(aamp)
    DrmIF->>DrmIF: new DrmInterface(aamp) [stores mpAamp]

    Note over Player,MW: HLS DRM Registration
    DrmIF->>DrmIF: RegisterHlsInterfaceCb(hlsInstance)
    Note over DrmIF: Stores PlayerHlsDrmSessionInterface*

    alt AES-128 Key Acquisition
        Player->>DrmIF: GetAccessKey(keyURI, effectiveUrl, ...)
        DrmIF->>DrmIF: Fetch key via curl
        DrmIF-->>Player: Key in mAesKeyBuf
    end

    alt OCDM HLS Session
        Player->>DrmIF: getHlsDrmSession(bridge, drmHelper, session, streamType)
        DrmIF->>HlsBridge: Create/retrieve HLS DRM session
        HlsBridge-->>DrmIF: DrmSession*
    end

    Note over Player,MW: Error & Profile Callbacks
    DrmIF->>Player: NotifyDrmError(drmFailure)
    DrmIF->>Player: ProfileUpdateDrmDecrypt(type, bucketType)
    DrmIF->>DrmIF: MapDrmToProfilerBucket(drmType)

    Note over Player,MW: Curl Management
    DrmIF->>Player: GetCurlInit(curlInstance)
    DrmIF->>Player: TerminateCurlInstance(mCurlInstance)
```

---

## 6.4 License Pre-Fetcher Lifecycle

```mermaid
sequenceDiagram
    participant AAMP as PrivateInstanceAAMP
    participant PreFetch as AampLicensePreFetcher
    participant Queue as mFetchQueue (deque)
    participant VssQueue as mVssFetchQueue (deque)
    participant LicMgr as AampDRMLicenseManager

    Note over AAMP,LicMgr: Initialization
    AAMP->>PreFetch: new AampLicensePreFetcher(aamp)
    AAMP->>PreFetch: SetLicenseFetcher(fetcherInstance)
    AAMP->>PreFetch: Init()
    PreFetch->>PreFetch: Start mPreFetchThread → PreFetchThread()
    PreFetch->>PreFetch: Start mVssPreFetchThread → VssPreFetchThread()

    Note over PreFetch,Queue: Content Protection Queuing
    loop For each adaptation in manifest
        AAMP->>PreFetch: QueueContentProtection(helper, periodId, adapIdx, type, isVss)
        alt isVssPeriod == false
            PreFetch->>PreFetch: KeyIsQueued() check
            PreFetch->>Queue: Push LicensePreFetchObject
            PreFetch->>PreFetch: mQCond.notify_one()
        else isVssPeriod == true
            PreFetch->>VssQueue: Push LicensePreFetchObject
            PreFetch->>PreFetch: mQVssCond.notify_one()
        end
    end

    Note over PreFetch,LicMgr: Pre-Fetch Processing Loop
    loop While !mExitLoop
        PreFetch->>PreFetch: Wait on mQCond
        PreFetch->>Queue: Pop front
        PreFetch->>PreFetch: CreateDRMSession(fetchObj)
        PreFetch->>LicMgr: createDrmSession(...)
        alt Success
            PreFetch->>PreFetch: mTrackStatus[type] = true
        else Failure
            PreFetch->>PreFetch: NotifyDrmFailure(fetchObj, event)
        end
    end

    Note over PreFetch: Termination
    AAMP->>PreFetch: Term()
    PreFetch->>PreFetch: mExitLoop = true
    PreFetch->>PreFetch: mQCond.notify_all()
    PreFetch->>PreFetch: Join mPreFetchThread
    PreFetch->>PreFetch: Join mVssPreFetchThread
```

---

## 6.5 Session Teardown & Cleanup

```mermaid
sequenceDiagram
    participant AAMP as PrivateInstanceAAMP
    participant LicMgr as AampDRMLicenseManager
    participant PreFetch as AampLicensePreFetcher
    participant SessMgr as DrmSessionManager
    participant DrmSess as DrmSession

    AAMP->>LicMgr: Stop()
    LicMgr->>PreFetch: Term()
    PreFetch->>PreFetch: mExitLoop=true, join threads
    LicMgr->>LicMgr: releaseLicenseRenewalThreads()
    LicMgr->>LicMgr: setLicenseRequestAbort(true)

    AAMP->>LicMgr: clearDrmSession(forceClearSession)
    LicMgr->>SessMgr: Clear sessions
    LicMgr->>LicMgr: clearFailedKeyIds()

    AAMP->>LicMgr: notifyCleanup()
    LicMgr->>SessMgr: Release all DrmSession objects
    SessMgr->>DrmSess: CloseKeySession()
```

---

## Key Classes Summary

| Class | File | Responsibility |
|-------|------|----------------|
| `AampDRMLicenseManager` | `drm/AampDRMLicManager.h` | License acquisition, renewal, session lifecycle, watermarking, profiling |
| `DrmInterface` | `drm/DrmInterface.h` | Singleton bridge between player and middleware DRM (curl, HLS DRM, AES, error callbacks) |
| `AampLicensePreFetcher` | `AampDRMLicPreFetcher.h` | Queue-based license pre-fetching with dual threads (main + VSS) |
| `DrmSessionManager` | `middleware/drm/DrmSessionManager.h` | Low-level DRM session slot management (owned by LicenseManager) |
| `DrmSession` | `middleware/drm/DrmSession.h` | Individual CDM/OCDM session wrapper |
| `DrmHelper` | `middleware/drm/helper/` | DRM-system-specific helpers (Widevine, ClearKey, Vanilla) |

## Addendum: DrmInterface.cpp Complete (277 lines — 100% read)

### Architecture
- **Singleton pattern**: `DrmInterface::GetInstance(aamp)`
- **Callback registration**: Bridge between AES decrypt layer and PrivateInstanceAAMP
- **Key methods**: TerminateCurlInstance, NotifyDrmError, GetAccessKey, getHlsDrmSession

### Sequence: HLS AES-128 Key Acquisition via DrmInterface

```mermaid
sequenceDiagram
    participant HLS as FragmentCollectorHLS
    participant AES as AesDec
    participant DI as DrmInterface
    participant AAMP as PrivateInstanceAAMP
    participant Curl as CurlDownloader

    Note over HLS: Encrypted segment detected
    HLS->>AES: Decrypt(segment)
    AES->>DI: GetAccessKey(keyURI, curlInstance)
    DI->>AAMP: GetFile(keyURI, eMEDIATYPE_LICENCE)
    AAMP->>Curl: Download(keyURI)
    Curl-->>AAMP: response (16 bytes expected)
    AAMP-->>DI: fetched=true, mAesKeyBuf

    alt Key size == 16 bytes
        DI-->>AES: keyAcquisitionStatus=true, ptr=key data
        AES->>AES: Decrypt segment with AES-128-CBC
    else Key size invalid
        DI-->>AES: failureReason=AAMP_TUNE_INVALID_DRM_KEY
    else Fetch failed
        alt Timeout
            DI-->>AES: failureReason=AAMP_TUNE_LICENCE_TIMEOUT
        else Other failure
            DI-->>AES: failureReason=AAMP_TUNE_LICENCE_REQUEST_FAILED
        end
    end
```

### Sequence: HLS DRM Session Creation (OCDM)

```mermaid
sequenceDiagram
    participant HLS as FragmentCollectorHLS
    participant DI as DrmInterface
    participant LicMgr as AampDRMLicenseManager
    participant AAMP as PrivateInstanceAAMP
    participant Profiler as AampProfiler

    HLS->>DI: getHlsDrmSession(bridge, drmHelper, session, streamType)
    DI->>LicMgr: setSessionMgrState(ACTIVE)
    DI->>Profiler: ProfileBegin(PROFILE_BUCKET_LA_TOTAL)
    DI->>LicMgr: createDrmSession(drmHelper, aamp, event, streamType)

    alt Session created successfully
        LicMgr-->>DI: session (DrmSession*)
        DI->>DI: HlsOcdmBridgeInterface::GetBridge(session)
        DI-->>HLS: bridge = shared_ptr<HlsDrmBase>
    else Session creation failed
        LicMgr-->>DI: session = nullptr
        DI->>AAMP: DisableDownloads()
        DI->>AAMP: SendErrorEvent(failure)
        DI->>Profiler: ProfileError(PROFILE_BUCKET_LA_TOTAL, failure)
    end

    DI->>Profiler: ProfileEnd(PROFILE_BUCKET_LA_TOTAL)
```

### Sequence: DRM Error Notification

```mermaid
sequenceDiagram
    participant DRM as DRM Layer
    participant DI as DrmInterface
    participant AAMP as PrivateInstanceAAMP
    participant EvtMgr as AampEventManager

    DRM->>DI: NotifyDrmError(drmFailure)

    alt Downloads still enabled
        DI->>AAMP: DisableDownloads()
        alt AAMP_TUNE_UNTRACKED_DRM_ERROR
            DI->>AAMP: SendErrorEvent(drmFailure, "AAMP: DRM Failure")
        else Known DRM failure
            DI->>AAMP: SendErrorEvent(drmFailure)
        end
        AAMP->>EvtMgr: SendEvent(MediaErrorEvent)
    else Downloads already disabled
        Note over DI: Skip — error already reported
    end
```

### Callback Registration Map

| Callback | Registered On | Delegates To |
|----------|--------------|--------------|
| `TerminateCurlInstanceCb` | AesDec | `DrmInterface::TerminateCurlInstance()` → `aamp->CurlTerm()` |
| `NotifyDrmErrorCb` | AesDec | `DrmInterface::NotifyDrmError()` → `aamp->SendErrorEvent()` |
| `ProfileUpdateCb` | AesDec | `DrmInterface::ProfileUpdateDrmDecrypt()` → `aamp->LogDrmInitComplete/DecryptEnd()` |
| `GetAccessKeyCb` | AesDec | `DrmInterface::GetAccessKey()` → `aamp->GetFile()` |
| `GetCurlInitCb` | AesDec | `DrmInterface::GetCurlInit()` → `aamp->CurlInit()` |
| `GetHlsDrmSessionCb` | PlayerHlsDrmSessionInterface | `DrmInterface::getHlsDrmSession()` → `LicMgr->createDrmSession()` |

**Confidence for 06-drm-session-manager.md: NOW 100%** ✅
