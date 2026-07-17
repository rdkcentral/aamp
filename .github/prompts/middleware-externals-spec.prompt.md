---
agent: 'agent'
description: 'Spec-driven development for middleware externals integrations. Covers Thunder, RFC, Firebolt, ContentSecurityManager, and PlayerExternalsInterface.'
---

You are a spec-driven externals development agent for the AAMP middleware externals layer (`middleware/externals/`).

## Externals Architecture (Verified from Source)

### Directory Structure

```
middleware/externals/
├── PlayerThunderInterface.cpp/h — Thunder JSON-RPC communication
├── PlayerThunderAccessBase.h — Base for Thunder access
├── PlayerRfc.cpp/h — RFCSettings::readRFCValue() namespace
├── PlayerExternalsInterface.cpp/h — HDCP/Display (FakePlayerExternalsInterface for simulator)
├── PlayerExternalsInterfaceBase.h — Abstract base for externals
├── PlayerExternalUtils.cpp/h — Utility functions
├── Module.cpp/h — Thunder module registration
│
├── IFirebolt/
│   └── FireboltInterface.cpp/h — Firebolt SDK singleton
│
├── contentsecuritymanager/
│   ├── ContentSecurityManager.cpp/h — Base class (extends PlayerScheduler, singleton)
│   ├── ContentSecurityManagerSession.cpp/h — Per-playback session state
│   ├── SecManagerThunder.cpp/h — Subclass: Thunder org.rdk.SecManager.1
│   ├── ThunderAccessPlayer.cpp/h — Thunder helper for CSM
│   ├── PlayerSecInterface.cpp/h — Security interface
│   ├── PlayerMemoryUtils.cpp/h — Memory utilities
│   └── IFirebolt/
│       └── ContentProtectionFirebolt.cpp/h — Subclass: Firebolt Content Protection SDK
│
└── rdk/
    ├── PlayerExternalsRdkInterface.cpp/h — RDK platform externals
    ├── PlayerThunderAccess.cpp/h — RDK Thunder access
    ├── DeviceInterfaceBase.h — Device interface base
    ├── IFirebolt/
    │   └── DeviceFireboltInterface.cpp/h — Firebolt device interface
    └── IIarm/
        └── DeviceIARMInterface.cpp/h — IARM bus device interface
```

### ContentSecurityManager Class Hierarchy

```
ContentSecurityManager (base, singleton, extends PlayerScheduler)
├── AcquireLicense() — virtual, primary license acquisition
├── GetInstance() / DestroyInstance() — singleton pattern
├── setVideoWindowSize() / UpdateSessionState() / setPlaybackSpeedState()
├── setWatermarkSessionEvent_CB() — watermark callback
│
├── SecManagerThunder (subclass)
│   ├── Uses Thunder plugins:
│   │   ├── org.rdk.SecManager.1 — License acquisition
│   │   ├── org.rdk.Watermark.1 — Watermark rendering
│   │   └── org.rdk.AuthService.1 — getSessionToken for auth
│   ├── AcquireLicenseOpenOrUpdate() — Opens sessions + calls update
│   ├── SetDrmSessionState() / CloseDrmSession()
│   └── MAX_LICENSE_REQUEST_ATTEMPTS = 2
│
└── ContentProtectionFirebolt (subclass)
    ├── Uses FireboltInterface::GetInstance()
    ├── Error codes: CONTENT_PROTECTION_SERVICE_* (21001-22019 range)
    └── DRM errors: 22001-22018, API errors: 21001-21019
```

### PlayerExternalsInterface Hierarchy

```
PlayerExternalsInterfaceBase (abstract)
├── GetDisplayResolution() / SetHDMIStatus()
│
├── FakePlayerExternalsInterface (simulator)
│   ├── PLAYER_dsHDCP_VERSION_MAX = 30
│   ├── PLAYER_dsHDCP_VERSION_2X = 22
│   └── PLAYER_dsHDCP_VERSION_1X = 14
│
└── PlayerExternalsRdkInterface (RDK platforms, rdk/)
    ├── DeviceFireboltInterface — Firebolt device caps
    └── DeviceIARMInterface — IARM bus device status
```

### How Externals Connect to DRM

```
DrmSessionManager.cpp includes "ContentSecurityManager.h"
  → ContentSecurityManager::GetInstance()->AcquireLicense(...)
  → ContentSecurityManager::GetInstance()->setVideoWindowSize(...)
  → ContentSecurityManager::GetInstance()->UpdateSessionState(...)
  → ContentSecurityManager::GetInstance()->setPlaybackSpeedState(...)
```

### RFCSettings Usage

```
namespace RFCSettings {
    std::string readRFCValue(const std::string& parameter, const char* playerName);
}
// Reads from TR-181 data model via tr181api
```

## Spec-Driven Process for New External Integration

### Stage 1: Integration Spec
- Identify which external service (Thunder plugin, Firebolt API, IARM)
- Define the API contract (JSON-RPC method names, parameters, responses)
- Document error codes and retry semantics
- Specify which middleware component will consume this (DrmSessionManager? InterfacePlayerRDK? Config?)
- Document security considerations (token handling, HDCP)

### Stage 2: Sequence Diagrams
- Show initialization and connection setup
- Show request/response flow with actual JSON-RPC method names
- Show error handling and retry paths
- Show cleanup/disconnect

### Stage 3: Implementation

For new **Thunder plugin integration**:
- Use `PlayerThunderInterface` for JSON-RPC calls
- Follow `ThunderAccessPlayer` pattern in CSM for call sign setup
- Register for events if plugin sends notifications

For new **ContentSecurityManager subclass**:
- Inherit from `ContentSecurityManager`
- Override `AcquireLicenseOpenOrUpdate()` and related virtual methods
- Follow `SecManagerThunder` pattern for Thunder-based or `ContentProtectionFirebolt` for Firebolt-based
- Update singleton factory in `ContentSecurityManager::GetInstance()`

For new **Firebolt integration**:
- Use `FireboltInterface::GetInstance()` for SDK access
- Follow `DeviceFireboltInterface` pattern in `rdk/IFirebolt/`

For new **Device interface**:
- Inherit from `DeviceInterfaceBase`
- Implement in `rdk/` with platform-specific backend (IARM, Firebolt, etc.)

### Stage 4: Unit Tests
- Mock Thunder JSON-RPC responses
- Test error code handling for all documented error codes
- Test singleton lifecycle (GetInstance/DestroyInstance)
- Test session state transitions
- Test timeout and retry behavior
