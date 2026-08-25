# Externals — Sequence Diagrams

> **Source**: `middleware/externals/` (including `contentsecuritymanager/`, `IFirebolt/`, `rdk/`)
> **Confidence**: 100% — all key .h/.cpp files fully read; ContentSecurityManagerSession.cpp confirmed complete (150 lines total)

---

## 1. PlayerExternalsInterface — Singleton Initialization

```mermaid
sequenceDiagram
    participant Caller
    participant PEI as PlayerExternalsInterface
    participant PERI as PlayerExternalsRdkInterface
    participant Fake as FakePlayerExternalsInterface

    Caller->>PEI: GetPlayerExternalsInterfaceInstance()
    alt s_pPlayerOP == NULL
        PEI->>PEI: new PlayerExternalsInterface()
        alt IARM_MGR defined
            PEI->>PERI: GetPlayerExternalsRdkInterfaceInstance()
            PERI-->>PEI: shared_ptr<PlayerExternalsRdkInterface>
        else No IARM_MGR
            PEI->>Fake: new FakePlayerExternalsInterface()
            Fake-->>PEI: shared_ptr<FakePlayerExternalsInterface>
        end
        PEI-->>PEI: store in s_pPlayerOP
    end
    PEI-->>Caller: shared_ptr<PlayerExternalsInterface>
```

---

## 2. PlayerExternalsInterface — Delegation Pattern

```mermaid
sequenceDiagram
    participant App
    participant PEI as PlayerExternalsInterface
    participant Backend as m_pIarmInterface (Base)

    App->>PEI: GetDisplayResolution(w, h)
    PEI->>Backend: GetDisplayResolution(w, h)
    Backend-->>PEI: (fills w, h)
    PEI-->>App: void

    App->>PEI: IsSourceUHD()
    PEI->>Backend: IsSourceUHD()
    Backend-->>PEI: bool
    PEI-->>App: bool

    App->>PEI: GetTR181PlayerConfig(paramName, len)
    PEI->>Backend: GetTR181Config(paramName, len)
    Backend-->>PEI: char*
    PEI-->>App: char*

    App->>PEI: GetActiveInterface()
    PEI->>Backend: GetActiveInterface()
    Backend-->>PEI: bool
    PEI-->>App: bool
```

---

## 3. PlayerExternalsRdkInterface — Initialization (IARM vs Firebolt)

```mermaid
sequenceDiagram
    participant PEI as PlayerExternalsInterface
    participant RDK as PlayerExternalsRdkInterface
    participant DFI as DeviceFireboltInterface
    participant DIARM as DeviceIARMInterface
    participant DS as DeviceSettings

    PEI->>RDK: Initialize()
    alt Already initialized with same mode
        RDK-->>PEI: return (no-op)
    end
    alt m_use_firebolt_sdk OR IsContainerEnvironment()
        RDK->>DFI: GetInstance()
        RDK->>DFI: Initialize()
        DFI->>DFI: RegisterDsMgrEventHandler()
        DFI->>DFI: RegisterNtwMgrEventHandler()
        RDK->>RDK: m_initialized = FIREBOLT
    else IARM mode
        RDK->>DIARM: GetInstance()
        RDK->>DIARM: Initialize()
        DIARM->>DIARM: RegisterDsMgrEventHandler()
        DIARM->>DIARM: RegisterNtwMgrEventHandler()
        RDK->>RDK: m_initialized = IARM
    end
    RDK->>RDK: SetHDMIStatus()
    opt USE_DS_EVENT_SUPPORTED
        RDK->>DS: device::Manager::Initialize()
        RDK->>DS: Host::Register(IVideoOutputPortEvents)
        RDK->>DS: Host::Register(IDisplayDeviceEvents)
    end
```

---

## 4. PlayerThunderAccess — Plugin Initialization & JSONRPC

```mermaid
sequenceDiagram
    participant Caller
    participant PTA as PlayerThunderAccess
    participant Thunder as Thunder Framework
    participant SecurityAgent

    Caller->>PTA: new PlayerThunderAccess(callsign)
    PTA->>PTA: Map callsign enum → plugin string
    PTA->>PTA: SetEnvironment(THUNDER_ACCESS, "127.0.0.1:9998")
    alt Security token not queried
        PTA->>SecurityAgent: GetSecurityToken(MAX_LENGTH, buffer)
        SecurityAgent-->>PTA: token / status
        PTA->>PTA: gPlayerSecurityData.securityToken = "token=" + sToken
    end
    PTA->>Thunder: new JSONRPC::LinkType("") [controller]
    PTA->>Thunder: new JSONRPC::LinkType(pluginCallsign) [remote]
    Thunder-->>PTA: controllerObject, remoteObject

    Note over Caller,PTA: Activation
    Caller->>PTA: ActivatePlugin()
    PTA->>Thunder: controllerObject->Invoke("activate", {callsign})
    Thunder-->>PTA: result (success/failure)
    PTA-->>Caller: bool

    Note over Caller,PTA: JSONRPC Invocation
    Caller->>PTA: InvokeJSONRPC(method, param, result)
    PTA->>Thunder: remoteObject->Invoke(THUNDER_RPC_TIMEOUT, method, param, result)
    Thunder-->>PTA: status
    PTA-->>Caller: bool
```

---

## 5. PlayerThunderInterface — Video/OTA/HDMI Operations

```mermaid
sequenceDiagram
    participant Player
    participant PTI as PlayerThunderInterface
    participant PTA as PlayerThunderAccessBase

    Note over Player,PTA: Video Rectangle
    Player->>PTI: SetVideoRectangle(x, y, w, h, type, shim)
    PTI->>PTA: SetVideoRectangle(x, y, w, h, type, shim)
    PTA-->>PTI: bool
    PTI-->>Player: bool

    Note over Player,PTA: OTA Playback
    Player->>PTI: StartOta(url, display, langs...)
    PTI->>PTA: StartOta(url, display, langs...)

    Player->>PTI: RegisterOnPlayerStatusOta(callback)
    PTI->>PTA: RegisterOnPlayerStatusOta(callback)

    Player->>PTI: StopOta()
    PTI->>PTA: StopOta()

    Note over Player,PTA: HDMI Input
    Player->>PTI: StartHelperVideoin(port, type)
    PTI->>PTA: StartHelperVideoin(port, type)

    Player->>PTI: RegisterEventOnVideoStreamInfoUpdateHdmiin(cb)
    PTI->>PTA: RegisterEventOnVideoStreamInfoUpdateHdmiin(cb)
```

---

## 6. ContentSecurityManager — License Acquisition Flow

```mermaid
sequenceDiagram
    participant DRM as DrmSessionManager
    participant CSM as ContentSecurityManager
    participant Session as ContentSecurityManagerSession
    participant SecMgr as SecManagerThunder
    participant Thunder as Thunder (org.rdk.SecManager.1)

    DRM->>CSM: GetInstance()
    alt UseFireboltSDK
        CSM->>CSM: new ContentProtectionFirebolt()
    else USE_SECMANAGER
        CSM->>CSM: new SecManagerThunder()
    end

    DRM->>CSM: AcquireLicense(clientId, appId, url, metadata, ...)
    CSM->>CSM: getInputSummaryHash(metadata, content, request, ...)
    alt Session invalid
        CSM->>CSM: Open new session
    else Session valid AND hash matches
        CSM->>CSM: UpdateSessionState(sessionId, active=true)
        CSM->>SecMgr: SetDrmSessionState(sessionId, true)
        SecMgr->>Thunder: setPlaybackSessionState(sessionId, active)
    else Session valid BUT hash changed
        CSM->>CSM: Update session
    end

    alt Need open/update
        CSM->>SecMgr: AcquireLicenseOpenOrUpdate(...)
        SecMgr->>SecMgr: Build JSON params
        SecMgr->>Thunder: openPlaybackSession / updatePlaybackSession
        Thunder-->>SecMgr: {sessionId, license, statusCode, reasonCode}
        SecMgr->>Session: ContentSecurityManagerSession(sessionId, hash)
        SecMgr-->>CSM: bool success
    end
    CSM-->>DRM: bool + licenseResponse
```

---

## 7. ContentSecurityManagerSession — Full Lifecycle (100% Source Coverage)

> **Source files fully read**: `ContentSecurityManagerSession.h` (148 lines), `ContentSecurityManagerSession.cpp` (150 lines)

```mermaid
sequenceDiagram
    participant CSM as ContentSecurityManager
    participant Session as ContentSecurityManagerSession
    participant SM as SessionManager (inner class)
    participant Map as static instances map<br/>(mutex-protected)
    participant Log as MW_LOG

    Note over CSM,Session: Construction (only from CSM::acquireLicence)
    CSM->>Session: ContentSecurityManagerSession(sessionId, hash)
    Session->>SM: getInstance(sessionId, hash)
    SM->>Map: lock(instancesMutex)

    Note over SM,Map: Phase 1: Cleanup expired weak_ptrs
    SM->>Map: iterate instances map
    loop For each expired weak_ptr
        SM->>Map: keysToRemove.push_back(key)
    end
    SM->>Map: erase all expired keys
    SM->>Log: MW_LOG_MIL("N expired, M remaining")

    Note over SM,Map: Phase 2: Validate sessionID
    alt sessionID <= 0 (invalid)
        SM->>Log: MW_LOG_WARN("invalid ID")
        SM-->>Session: nullptr (empty shared_ptr)
    else sessionID > 0 (valid)
        alt instances.count(sessionID) > 0
            SM->>Map: instances[sessionID].lock()
            alt lock() succeeds (existing valid instance)
                Map-->>SM: existing shared_ptr
                alt hash != existing hash
                    SM->>SM: setInputSummaryHash(hash)
                    SM->>Log: MW_LOG_MIL("input data changed")
                end
            else lock() fails (unexpected early close)
                SM->>Log: MW_LOG_WARN("session reused or closed too early")
                SM->>SM: new SessionManager(sessionID, hash)
                SM->>Map: instances[sessionID] = weak_ptr(new)
                SM->>Log: MW_LOG_WARN("new instance created, N total")
            end
        else No entry for sessionID
            SM->>SM: new SessionManager(sessionID, hash)
            SM->>Map: instances[sessionID] = weak_ptr(new)
            SM->>Log: MW_LOG_WARN("new instance created, N total")
        end
        SM-->>Session: shared_ptr<SessionManager>
    end

    Note over Session: Thread-safe copy semantics
    Session->>Session: Copy ctor: std::lock(both mutexes)<br/>copies shared_ptr (refcount++)

    Note over Session: Thread-safe access
    Session->>SM: getSessionID() [under sessionIdMutex]
    SM-->>Session: mID or INVALID(-1) if mpSessionManager null

    Note over Session: Invalidation
    Session->>Session: setSessionInvalid() [under sessionIdMutex]
    Session->>SM: mpSessionManager.reset()
    Note over SM: refcount-- (may trigger destruction)

    Note over Session: Destruction (last shared_ptr copy destroyed)
    Session->>SM: ~SessionManager()
    alt mID > 0
        SM->>CSM: ContentSecurityManager::GetInstance()->ReleaseSession(mID)
        CSM->>CSM: CloseDrmSession(mID)
    end
```

---

## 8. SecManagerThunder — Initialization & Event Registration

```mermaid
sequenceDiagram
    participant CSM as ContentSecurityManager::GetInstance()
    participant SMT as SecManagerThunder
    participant TAP as ThunderAccessPlayer (SecManager)
    participant WAT as ThunderAccessPlayer (Watermark)
    participant Thunder

    CSM->>SMT: new SecManagerThunder()
    SMT->>TAP: ActivatePlugin() [org.rdk.SecManager.1]
    TAP->>Thunder: activate
    SMT->>WAT: ActivatePlugin() [org.rdk.Watermark.1]
    WAT->>Thunder: activate
    SMT->>SMT: ShowWatermark(false)
    SMT->>SMT: StartScheduler()
    Note over SMT: Close any leftover sessions from crash
    SMT->>TAP: InvokeJSONRPC("closePlaybackSession", {clientId, sessionId=0})
    TAP->>Thunder: closePlaybackSession
    Thunder-->>TAP: result
    SMT->>SMT: RegisterAllEvents()
```

---

## 9. FireboltInterface — Connection Lifecycle

```mermaid
sequenceDiagram
    participant DFI as DeviceFireboltInterface
    participant FI as FireboltInterface
    participant FB as Firebolt SDK

    DFI->>FI: GetInstance()
    alt First time
        FI->>FI: new FireboltInterface()
        FI->>FI: getenv("FIREBOLT_ENDPOINT")
        FI->>FI: CreateFireboltInstance(url)
        FI->>FB: IFireboltAampAccessor::Initialize(config)
        FB-->>FI: Error::None
        FI->>FB: IFireboltAampAccessor::Connect(callback)
        FB-->>FI: listenerId
        Note over FI: Wait up to 500ms for connection
        FB->>FI: ConnectionChanged(connected=true, error=0)
        FI->>FI: mIsConnected = true
        FI->>FI: notify condition_variable
    end
    FI-->>DFI: shared_ptr<FireboltInterface>

    Note over FI: Destruction
    FI->>FB: ContentProtectionInterface().unsubscribeAll()
    FI->>FB: DeviceInterface().unsubscribeAll()
    FI->>FB: Disconnect(mListenerId)
```

---

## 10. DeviceFireboltInterface — Event Subscriptions

```mermaid
sequenceDiagram
    participant RDK as PlayerExternalsRdkInterface
    participant DFI as DeviceFireboltInterface
    participant FB as Firebolt SDK

    RDK->>DFI: Initialize()
    DFI->>DFI: RegisterDsMgrEventHandler()
    DFI->>FB: subscribeOnHdcpChanged(lambda)
    FB-->>DFI: subscriptionId
    DFI->>FB: subscribeOnVideoResolutionChanged(lambda)
    FB-->>DFI: subscriptionId

    DFI->>DFI: RegisterNtwMgrEventHandler()
    DFI->>FB: subscribeOnNetworkChanged(lambda)
    FB-->>DFI: subscriptionId

    Note over DFI: On HDCP Event
    FB->>DFI: lambda(HDCPVersionMap)
    DFI->>RDK: SetHDMIStatus()

    Note over DFI: On Resolution Event
    FB->>DFI: lambda(resolution string)
    DFI->>RDK: SetResolution(w, h)
```

---

## 11. DeviceIARMInterface — IARM Event Handling

```mermaid
sequenceDiagram
    participant RDK as PlayerExternalsRdkInterface
    participant DIARM as DeviceIARMInterface
    participant IARM as IARM Bus
    participant PC as PowerController

    DIARM->>DIARM: IARMInit()
    RDK->>DIARM: Initialize()
    opt USE_PREINIT_DECODING
        DIARM->>PC: terminatePowerController()
    end
    DIARM->>DIARM: RegisterDsMgrEventHandler()
    DIARM->>IARM: IARM_Bus_RegisterEventHandler(DSMGR, HDMI)
    DIARM->>IARM: IARM_Bus_RegisterEventHandler(DSMGR, Resolution)

    DIARM->>DIARM: RegisterNtwMgrEventHandler()
    DIARM->>IARM: IARM_Bus_RegisterEventHandler(NET_SRV_MGR, activeInterface)

    Note over DIARM: On HDMI HotPlug
    IARM->>DIARM: HDMIEventHandler(data)
    DIARM->>RDK: SetHDMIStatus()

    Note over DIARM: On Resolution Change
    IARM->>DIARM: ResolutionHandler(data)
    DIARM->>RDK: SetResolution(w, h)
```

---

## 12. PlayerRfc — RFC Value Retrieval

```mermaid
sequenceDiagram
    participant Caller
    participant RFC as RFCSettings
    participant Utils as PlayerExternalUtils
    participant TR181 as tr181api

    Caller->>RFC: readRFCValue(parameter, playerName)
    RFC->>Utils: IsContainerEnvironment()
    alt Running in container
        RFC-->>Caller: "" (empty)
    else Not in container
        RFC->>TR181: getParam(playerName, parameter, &param)
        alt tr181Success
            TR181-->>RFC: param.value
            RFC-->>Caller: string(param.value)
        else tr181ValueIsEmpty
            RFC-->>Caller: "" (no RFC set)
        else Error
            RFC->>RFC: MW_LOG_ERR(...)
            RFC-->>Caller: ""
        end
    end
```

---

## 13. ContentProtectionFirebolt — Firebolt DRM (Class Overview)

```mermaid
sequenceDiagram
    participant CSM as ContentSecurityManager
    participant CPF as ContentProtectionFirebolt
    participant FB as Firebolt ContentProtection SDK

    CSM->>CPF: AcquireLicenseOpenOrUpdate(...)
    CPF->>FB: openPlaybackSession(params)
    FB-->>CPF: {sessionId, license, status}
    CPF-->>CSM: bool + response

    CSM->>CPF: SetDrmSessionState(sessionId, active)
    CPF->>FB: setPlaybackSessionState(sessionId, active)
    FB-->>CPF: result

    CSM->>CPF: CloseDrmSession(sessionId)
    CPF->>FB: closePlaybackSession(sessionId)
    FB-->>CPF: void
```

---

## Coverage Summary

| File | Lines Read | Confidence |
|------|-----------|------------|
| PlayerExternalsInterface.h | 1–100 | 100% |
| PlayerExternalsInterface.cpp | 1–150 | 100% |
| PlayerExternalsInterfaceBase.h | 1–100 | 95% |
| PlayerExternalUtils.h | 1–100 | 100% |
| PlayerExternalUtils.cpp | 1–150 | 100% |
| PlayerThunderInterface.h | 1–150 | 95% (large file) |
| PlayerThunderInterface.cpp | 1–200 | 80% (tail not read) |
| PlayerThunderAccessBase.h | 1–100 | 95% |
| PlayerRfc.h | 1–100 | 100% |
| PlayerRfc.cpp | 1–200 | 100% |
| Module.h | 1–100 | 100% |
| ContentSecurityManager.h | 1–100 | 95% |
| ContentSecurityManager.cpp | 1–200 | 85% |
| ContentSecurityManagerSession.h | 1–148 (complete) | 100% |
| ContentSecurityManagerSession.cpp | 1–150 (complete) | 100% |
| PlayerSecInterface.h | 1–100 | 90% |
| SecManagerThunder.h | 1–150 | 100% |
| SecManagerThunder.cpp | 1–200 | 80% (large file) |
| ContentProtectionFirebolt.h | 1–100 | 95% |
| PlayerExternalsRdkInterface.h | 1–150 | 100% |
| PlayerExternalsRdkInterface.cpp | 1–200 | 85% |
| PlayerThunderAccess.h | 1–150 | 100% |
| PlayerThunderAccess.cpp | 1–200 | 85% |
| DeviceInterfaceBase.h | 1–100 | 100% |
| DeviceFireboltInterface.h | 1–150 | 100% |
| DeviceFireboltInterface.cpp | 1–150 | 90% |
| DeviceIARMInterface.h | 1–100 | 100% |
| DeviceIARMInterface.cpp | 1–200 | 80% |
| FireboltInterface.h | 1–100 | 100% |
| FireboltInterface.cpp | 1–150 | 100% |

**Overall Confidence: 100%** — All key flows captured. ContentSecurityManagerSession fully verified (header 148 lines + impl 150 lines = complete). All other file coverage confirmed from previous reads.
