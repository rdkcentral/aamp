<!-- refreshed: 2026-06-08 -->
# Architecture

**Analysis Date:** 2026-06-08

## System Overview

```text
┌─────────────────────────────────────────────────────────────────────────┐
│                        Application / JS Layer                           │
│   `jsbindings/jsbindings.cpp`  `jsbindings/jsmediaplayer.cpp`           │
│   UVE (Universal Video Engine) JavaScript API                           │
└──────────────────────────────┬──────────────────────────────────────────┘
                               │
                               ▼
┌─────────────────────────────────────────────────────────────────────────┐
│                   Public C++ Player API                                 │
│   `main_aamp.h` / `main_aamp.cpp`  →  PlayerInstanceAAMP               │
└──────────────────────────────┬──────────────────────────────────────────┘
                               │
                               ▼
┌─────────────────────────────────────────────────────────────────────────┐
│                    Core Engine (PrivateInstanceAAMP)                    │
│   `priv_aamp.h` / `priv_aamp.cpp`                                       │
│   Orchestrates all subsystems; owns all major managers                  │
├──────────────┬──────────────┬──────────────┬────────────────────────────┤
│  Protocol    │  Download &  │  Async Task  │  Event Bus                 │
│  Abstraction │  Networking  │  Management  │  `AampEventManager`        │
│              │              │              │  `AampEventManager.h`       │
└──────┬───────┴──────┬───────┴──────┬───────┴────────────────────────────┘
       │              │              │
       ▼              ▼              ▼
┌─────────────┐  ┌──────────┐  ┌────────────────────────────┐
│  Protocol   │  │ cURL     │  │  AampScheduler             │
│  Handlers   │  │ Download │  │  `AampScheduler.h`         │
│             │  │ Layer    │  │  AampTrackWorkerManager    │
│ HLS         │  │          │  │  `AampTrackWorkerManager.  │
│ `fragment   │  │`download │  │   hpp`                     │
│  collector_ │  │ er/      │  └────────────────────────────┘
│  hls.cpp`   │  │ AampCurl │
│             │  │ Download │
│ DASH (MPD)  │  │ er.h`    │
│ `fragment   │  └──────────┘
│  collector_ │
│  mpd.cpp`   │
│             │
│ Progressive │
│ `fragment   │
│  collector_ │
│  progressive│
│  .cpp`      │
└──────┬──────┘
       │ (all implement StreamAbstractionAAMP)
       │ `StreamAbstractionAAMP.h`
       ▼
┌─────────────────────────────────────────────────────────────────────────┐
│               Stream Sink (Media Output Pipeline)                       │
│   `StreamSink.h` (abstract interface)                                   │
│   `aampgstplayer.h` / `aampgstplayer.cpp`  →  AAMPGstPlayer            │
│   `middleware/InterfacePlayerRDK.h`         →  InterfacePlayerRDK       │
│   `AampStreamSinkManager.h`                →  AampStreamSinkManager     │
└─────────────────────────────────────────────────────────────────────────┘
       │
       ▼
┌─────────────────────────────────────────────────────────────────────────┐
│               GStreamer Media Pipeline (Hardware Decode)                 │
└─────────────────────────────────────────────────────────────────────────┘
```

## Component Responsibilities

| Component | Responsibility | File |
|-----------|----------------|------|
| `PlayerInstanceAAMP` | Public C++ API; thin facade over `PrivateInstanceAAMP` | `main_aamp.h` |
| `PrivateInstanceAAMP` | Core engine; owns all subsystems and orchestration logic | `priv_aamp.h` |
| `StreamAbstractionAAMP` | Abstract base class for protocol-specific fragment collectors | `StreamAbstractionAAMP.h` |
| `StreamAbstractionAAMP_HLS` | HLS playlist parsing, variant selection, segment fetching | `fragmentcollector_hls.h` |
| `StreamAbstractionAAMP_MPD` | MPEG-DASH manifest parsing, period/adaptation set handling | `fragmentcollector_mpd.h` |
| `FragmentCollector_Progressive` | Progressive MP4 download handling | `fragmentcollector_progressive.h` |
| `StreamSink` | Abstract interface for media output (GStreamer, Rialto, etc.) | `StreamSink.h` |
| `AAMPGstPlayer` | GStreamer-based concrete stream sink implementation | `aampgstplayer.h` |
| `AampStreamSinkManager` | Manages multiple stream sink player instances | `AampStreamSinkManager.h` |
| `AampEventManager` | Publisher-subscriber event dispatch hub | `AampEventManager.h` |
| `AampScheduler` | General-purpose deferred async task runner (FIFO deque) | `AampScheduler.h` |
| `AampTrackWorkerManager` | Manages per-track worker threads | `AampTrackWorkerManager.hpp` |
| `AampTrackWorker` | Dedicated single-job worker thread per media track | `AampTrackWorker.hpp` |
| `AampTSBSessionManager` | Time-Shift Buffer session coordination (DVR) | `AampTSBSessionManager.h` |
| `AampTsbDataManager` | In-memory doubly-linked-list fragment cache for TSB | `AampTsbDataManager.h` |
| `AampTsbReader` | Read interface into TSB for the playback pipeline | `AampTsbReader.h` |
| `AampTsbMetaDataManager` | Time-associated TSB metadata (SCTE-35 ad markers, etc.) | `AampTsbMetaDataManager.h` |
| `ABRManager` | Adaptive bitrate bandwidth estimation and profile selection | `abr/abr.h` |
| `AampCurlDownloader` | cURL-based HTTP downloader for segments and manifests | `downloader/AampCurlDownloader.h` |
| `AampConfig` | Centralised hierarchical configuration store | `AampConfig.h` |
| `AampCMCDCollector` | CMCD (Common Media Client Data) header collection | `AampCMCDCollector.h` |
| `AampBufferControl` | GStreamer buffer level monitoring and control | `AampBufferControl.h` |
| `DrmSessionManager` | DRM session lifecycle management | `drm/DrmSessionManager.h` |
| `InterfacePlayerRDK` | Middleware GStreamer player interface for RDK platforms | `middleware/InterfacePlayerRDK.h` |

## Pattern Overview

**Overall:** Multi-threaded, event-driven, layered C++ media player engine

**Key Characteristics:**
- Protocol-agnostic core via `StreamAbstractionAAMP` abstraction; concrete implementations swap in per stream type
- Publisher-subscriber event bus (`AampEventManager`) decouples producers from consumers
- All external media output goes through the `StreamSink` interface, allowing GStreamer and Rialto backends
- RAII resource management with `std::unique_ptr` / `std::shared_ptr` for new code; legacy raw pointers still present in older modules
- C++17 target for all new code; large volumes of existing C++11 and C-style code remain as refactoring targets

## Layers

**Public API Layer:**
- Purpose: Exposes player controls to applications and JavaScript bindings
- Location: `main_aamp.h`, `main_aamp.cpp`
- Contains: `PlayerInstanceAAMP` — tune, stop, seek, track selection
- Depends on: `PrivateInstanceAAMP`, `AampConfig`, `StreamSink`
- Used by: `jsbindings/`, Kotlin CLI `kotlin/aampcli/`

**JavaScript Binding Layer:**
- Purpose: Bridges UVE JavaScript API to C++ `PlayerInstanceAAMP`
- Location: `jsbindings/jsbindings.cpp`, `jsbindings/jsmediaplayer.cpp`
- Contains: WK2 injected-bundle bindings, event listeners for JS
- Depends on: `PlayerInstanceAAMP`
- Used by: Web applications, RDK browser environments

**Core Engine Layer:**
- Purpose: Orchestrates all playback subsystems; manages state machine
- Location: `priv_aamp.h`, `priv_aamp.cpp` (15k+ lines)
- Contains: `PrivateInstanceAAMP`, curl session management, DRM hooks, tune logic
- Depends on: All subsystems below
- Used by: `PlayerInstanceAAMP` (1:1 relationship)

**Protocol Abstraction Layer:**
- Purpose: Protocol-specific manifest parsing, segment scheduling, ABR
- Location: `fragmentcollector_hls.cpp`, `fragmentcollector_mpd.cpp`, `fragmentcollector_progressive.cpp`, `streamabstraction.cpp`
- Contains: `StreamAbstractionAAMP_HLS`, `StreamAbstractionAAMP_MPD`, `StreamAbstractionAAMP` base
- Depends on: `PrivateInstanceAAMP`, `ABRManager`, `AampCurlDownloader`, `isobmff/`, `dash/`, `drm/`
- Used by: Core Engine Layer

**Networking / Download Layer:**
- Purpose: HTTP segment and manifest downloading via cURL
- Location: `downloader/AampCurlDownloader.h`, `downloader/AampCurlStore.h`, `downloader/AampCurlDownloader.cpp`
- Contains: cURL connection pooling (`AampCurlStore`), retry logic, download metrics
- Depends on: `libcurl`, `AampConfig`
- Used by: Protocol Abstraction Layer, DRM License Manager

**ABR Layer:**
- Purpose: Bandwidth estimation and bitrate profile selection
- Location: `abr/abr.h`, `abr/abr.cpp`, `abr/HarmonicEwmaEstimator.*`, `abr/RollingMedianOutlierEstimator.*`
- Contains: `ABRManager`, pluggable `BandwidthEstimatorBase` implementations
- Depends on: Download metrics from cURL layer
- Used by: Protocol Abstraction Layer

**TSB / DVR Layer:**
- Purpose: Time-shift buffer providing DVR pause/rewind for live streams
- Location: `AampTSBSessionManager.h`, `AampTsbDataManager.h`, `AampTsbReader.h`, `AampTsbMetaDataManager.h`, `tsb/`
- Contains: Fragment linked-list cache, TSB store (`tsb/src/TsbStore.cpp`), SCTE-35 metadata tracking
- Depends on: `tsb/api/TsbApi.h`, file-system TSB store
- Used by: Protocol Abstraction Layer, Core Engine Layer

**DRM Layer:**
- Purpose: License acquisition, session management, decryption
- Location: `drm/`, `middleware/drm/`
- Contains: `DrmSessionManager`, `DrmSessionFactory`, OCDM adapters (`middleware/drm/ocdm/`), DRM helpers per system (`middleware/drm/helper/`)
- Depends on: `libcurl`, OpenSSL, platform OCDM
- Used by: Protocol Abstraction Layer, `PrivateInstanceAAMP`

**Stream Sink Layer:**
- Purpose: Delivers decoded media to hardware output
- Location: `StreamSink.h` (interface), `aampgstplayer.h`/`.cpp`, `middleware/InterfacePlayerRDK.h`/`.cpp`
- Contains: `AAMPGstPlayer`, `InterfacePlayerRDK`, `AampStreamSinkManager`, `AampBufferControl`
- Depends on: GStreamer (`gstreamer-1.0`, `gstreamer-app-1.0`)
- Used by: Core Engine Layer

**Middleware / Platform Layer:**
- Purpose: Platform-specific adapters (closed captions, DRM, externals, GStreamer plugins)
- Location: `middleware/`
- Contains: `PlayerCCManager`, `InterfacePlayerRDK`, SUBTEC/Rialto subtitle bridges, Thunder/Firebolt interface wrappers
- Depends on: Platform SDKs (WPEFramework, Rialto, IARM)
- Used by: Stream Sink Layer, Core Engine Layer

## Data Flow

### Primary Playback Request Path

1. Application calls `PlayerInstanceAAMP::Tune(url)` (`main_aamp.h`)
2. Dispatched to `PrivateInstanceAAMP::Tune()` in `priv_aamp.cpp`
3. Manifest downloaded via `AampCurlDownloader` (`downloader/AampCurlDownloader.cpp`)
4. `StreamAbstractionAAMP` subclass instantiated based on detected protocol (HLS/DASH/Progressive)
5. Protocol handler parses manifest, selects bitrate via `ABRManager` (`abr/abr.cpp`)
6. `AampTrackWorker` threads (`AampTrackWorker.hpp`) download segments per track (video/audio/subtitle)
7. Segments processed through `isobmff/` parser or `tsprocessor.cpp` / `tsDemuxer.cpp`
8. Decoded buffers pushed to `StreamSink::SendCopy()` → `AAMPGstPlayer` → GStreamer pipeline
9. Events dispatched via `AampEventManager::SendEvent()` back to listeners/JS layer

### Time-Shift Buffer (TSB / DVR) Flow

1. Live stream fragments written via `AampTSBSessionManager::EnqueueWrite()` (`AampTSBSessionManager.h`)
2. `AampTsbDataManager` appends fragments to a doubly linked list (`AampTsbDataManager.h`)
3. SCTE-35 ad markers stored in `AampTsbMetaDataManager` (`AampTsbMetaDataManager.h`)
4. On rewind/seek, `AampTsbReader` traverses the linked list to retrieve buffered fragments (`AampTsbReader.h`)
5. Retrieved fragments re-injected into the playback pipeline as if live

### Ad Insertion Flow (DASH)

1. `admanager_mpd.cpp` monitors SCTE-35 markers during MPD parsing
2. Ad period substituted into active manifest via `AdManagerBase` interface (`AdManagerBase.h`)
3. TSB ad metadata tracked via `AampTsbAdMetaData`, `AampTsbAdPlacementMetaData`, `AampTsbAdReservationMetaData`

**State Management:**
- Player state tracked atomically in `AampScheduler::mState` and `AampEventManager::mPlayerState`
- Configuration hierarchy resolved in `AampConfig` — defaults < operator RFC/ENV < stream < application < developer override

## Key Abstractions

**`StreamAbstractionAAMP`:**
- Purpose: Protocol-agnostic contract for all streaming format handlers
- Location: `StreamAbstractionAAMP.h`, `streamabstraction.cpp`
- Pattern: Abstract base class with pure virtuals; concrete subclasses swap in per protocol

**`StreamSink`:**
- Purpose: Decouples media data delivery from the output pipeline implementation
- Location: `StreamSink.h`
- Examples: `aampgstplayer.h` (GStreamer), `AampStreamSinkInactive.h` (no-op sink)
- Pattern: Abstract interface; `SendCopy()` transfers ownership of buffer data via move semantics

**`AampEventListener` / `AampEventManager`:**
- Purpose: Pub-sub decoupling of player state producers from UI/JS consumers
- Location: `AampEventListener.h`, `AampEventManager.h`
- Pattern: Listener registration per `AAMPEventType`; async or sync dispatch via GLib idle callbacks

**`AsyncTaskObj` / `AampScheduler`:**
- Purpose: Deferred, off-thread task execution without blocking the player loop
- Location: `AampScheduler.h`
- Pattern: Producer-consumer with `std::deque`, condition variable, single worker thread

**`AampTrackWorker`:**
- Purpose: Per-track serialised job processing (video, audio, subtitle each get their own worker)
- Location: `AampTrackWorker.hpp`, `AampTrackWorkerManager.hpp`
- Pattern: Single-slot job queue with condition variable; producer-consumer

**`BandwidthEstimatorBase`:**
- Purpose: Pluggable bandwidth estimation algorithm
- Location: `abr/BandwidthEstimatorBase.h`
- Implementations: `abr/HarmonicEwmaEstimator.h`, `abr/RollingMedianOutlierEstimator.h`
- Pattern: Strategy pattern

## Entry Points

**C++ Library API:**
- Location: `main_aamp.h`, `main_aamp.cpp`
- Triggers: Application instantiates `PlayerInstanceAAMP` and calls `Tune()`
- Responsibilities: All public playback control, event listener registration, configuration

**JavaScript (UVE) API:**
- Location: `jsbindings/jsbindings.cpp`, `jsbindings/jsmediaplayer.cpp`
- Triggers: Web application JavaScript invokes UVE API in browser context
- Responsibilities: Marshals JS calls to `PlayerInstanceAAMP`, exposes events back to JS

**Kotlin CLI:**
- Location: `kotlin/aampcli/main.kt`
- Triggers: Developer runs `aampcli` from command line for test/diagnostic playback
- Responsibilities: CLI-driven tune and control without a browser environment

**Shim Entry Points (Input Sources):**
- `compositein_shim.cpp` — Composite video input
- `hdmiin_shim.cpp` — HDMI input pass-through
- `videoin_shim.cpp` — Generic video input
- `rmf_shim.cpp` — RMF (RDK Media Framework) input
- `ota_shim.cpp` — Over-the-air tuner input

## Architectural Constraints

- **Threading:** Multi-threaded; `AampScheduler` runs one worker thread; each `AampTrackWorker` runs one dedicated thread per media track; GStreamer pipeline runs on its own internal thread pool; event dispatch uses GLib idle callbacks on the GLib main loop
- **Global state:** `AampConfig` manages global defaults; per-player state is owned by `PrivateInstanceAAMP`; logging state in `AampLogManager.h` (`aamplogging.cpp`)
- **Circular imports:** `priv_aamp.h` is a heavy hub header included by most subsystems; `StreamAbstractionAAMP.h` includes `priv_aamp.h` — keep new subsystems forward-declaring `PrivateInstanceAAMP` rather than including `priv_aamp.h` where possible
- **C standard:** C++17 required for all new code; C++11 and C-style patterns exist throughout older files and are refactoring targets
- **Memory:** Embedded target; avoid heap allocation in hot paths; prefer RAII and smart pointers; raw `memcpy`/raw-pointer patterns in legacy code must not be replicated

## Anti-Patterns

### Including `priv_aamp.h` in new subsystem headers

**What happens:** New header directly `#include "priv_aamp.h"` to access `PrivateInstanceAAMP` members
**Why it's wrong:** `priv_aamp.h` is 4k+ lines and includes virtually the entire engine; creates deep transitive coupling and slow compilation
**Do this instead:** Forward-declare `class PrivateInstanceAAMP;` in the header; include `priv_aamp.h` only in the `.cpp` implementation file (see `AampTSBSessionManager.h` for the correct pattern)

### Using raw pointers and `new`/`delete` in new code

**What happens:** New classes allocate with `new` and store raw pointers without RAII
**Why it's wrong:** Violates C++17 convention, risks leaks and double-free on embedded targets
**Do this instead:** Use `std::make_unique<T>()` or `std::make_shared<T>()`; declare ownership in the class header with `std::unique_ptr` member

### Copying legacy C-style string patterns

**What happens:** New code uses `char*`, `memcpy`, `strcpy` for string handling
**Why it's wrong:** Memory-unsafe; `std::string` is available and expected per `cpp.instructions.md`
**Do this instead:** Use `std::string` for all string storage; `std::string_view` for non-owning reads

## Error Handling

**Strategy:** Return-code and exception-light; errors are communicated primarily through AAMP events dispatched via `AampEventManager`

**Patterns:**
- `AAMPLOG_ERR` macro for severe/unexpected conditions (see `AampLogManager.h`)
- `AAMPLOG_WARN` for recoverable warnings
- `AAMPLOG_MIL` for always-visible operational milestones (prefer over `AAMPLOG_WARN` for important non-warnings)
- HTTP download failures trigger retry logic within `AampCurlDownloader`; exhausted retries fire error events via `AampEventManager`
- DRM errors propagated via `DrmCallbacks` interface and ultimately as `AAMPEvent` to listeners

## Cross-Cutting Concerns

**Logging:** AAMP-specific macro system (`AAMPLOG_TRACE`, `AAMPLOG_INFO`, `AAMPLOG_MIL`, `AAMPLOG_WARN`, `AAMPLOG_ERR`) defined in `AampLogManager.h`; implemented in `aamplogging.cpp`. Configurable per-module log levels. Do not use raw `printf` or `logprintf` in new code.

**Validation:** Configuration validated and coerced in `AampConfig`; manifest/segment data validated by protocol handlers (`fragmentcollector_hls.cpp`, `fragmentcollector_mpd.cpp`)

**Authentication/DRM:** Unified through `DrmSessionManager` / `DrmSessionFactory` (`drm/`); platform-specific via OCDM adapters (`middleware/drm/ocdm/`); content-security manager interface in `middleware/externals/contentsecuritymanager/`

**CMCD:** `AampCMCDCollector` (`AampCMCDCollector.h`) aggregates CMCD headers injected into HTTP requests via the downloader layer

---

*Architecture analysis: 2026-06-08*
