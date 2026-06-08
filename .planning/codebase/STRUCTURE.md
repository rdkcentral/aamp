# Codebase Structure

**Analysis Date:** 2026-06-08

## Directory Layout

```
aamp/                              # Repository root — core engine source lives here
├── main_aamp.h / main_aamp.cpp    # Public C++ player API (PlayerInstanceAAMP)
├── priv_aamp.h / priv_aamp.cpp    # Core engine implementation (PrivateInstanceAAMP)
├── streamabstraction.cpp          # StreamAbstractionAAMP base class implementation
├── StreamAbstractionAAMP.h        # Abstract protocol handler interface
├── StreamSink.h                   # Abstract media output sink interface
├── fragmentcollector_hls.*        # HLS protocol handler
├── fragmentcollector_mpd.*        # MPEG-DASH protocol handler
├── fragmentcollector_progressive.*# Progressive MP4 download handler
├── aampgstplayer.h / .cpp         # GStreamer-based stream sink
├── AampBufferControl.h / .cpp     # GStreamer buffer level management
├── AampStreamSinkManager.h / .cpp # Stream sink lifecycle manager
├── AampEventManager.h / .cpp      # Pub-sub event bus
├── AampEvent.h / .cpp             # Event type definitions
├── AampEventListener.h / .cpp     # Event listener interface and base
├── AampScheduler.h / .cpp         # Async deferred task scheduler
├── AampTrackWorker.hpp / .cpp     # Per-track worker thread
├── AampTrackWorkerManager.hpp/cpp # Worker thread manager
├── AampConfig.h / .cpp            # Centralised configuration system
├── AampDefine.h                   # Core enumerations and defines
├── AampConstants.h                # Player constants
├── AampUtils.h / .cpp             # General utility functions
├── AampLogManager.h               # Logging macros and level configuration
├── aamplogging.cpp                # Logging implementation
├── AampCMCDCollector.h / .cpp     # CMCD telemetry collection
├── AampProfiler.h / .cpp          # Performance profiling
├── AampTelemetry2.hpp / .cpp      # Telemetry reporting
├── AampMPDDownloader.h / .cpp     # MPEG-DASH MPD manifest downloader
├── AampMPDParseHelper.h / .cpp    # MPD parsing helper utilities
├── AampMPDUtils.h / .cpp          # MPD URL / format utilities
├── AampLatencyMonitor.h / .cpp    # Low-latency DASH monitor
├── AampCacheHandler.h / .cpp      # URL-keyed fragment cache
├── CachedFragment.h / .cpp        # Fragment data container
├── AampDRMLicPreFetcher.h / .cpp  # DRM licence pre-fetch manager
├── AampNetworkPersona.h / .cpp    # Network persona / simulated conditions
├── AampTSBSessionManager.h / .cpp # Time-Shift Buffer session manager
├── AampTsbDataManager.h / .cpp    # TSB in-memory fragment linked-list cache
├── AampTsbReader.h / .cpp         # TSB fragment read interface
├── AampTsbMetaDataManager.h / .cpp# TSB SCTE-35 / metadata tracking
├── AampTsbMetaData.h / .cpp       # TSB metadata base types
├── AampTsbAdMetaData.h / .cpp     # TSB ad metadata
├── AampTsbAdPlacementMetaData.*   # TSB ad placement metadata
├── AampTsbAdReservationMetaData.* # TSB ad reservation metadata
├── admanager_mpd.h / .cpp         # DASH ad insertion manager
├── AdManagerBase.h                # Ad manager abstract interface
├── tsDemuxer.hpp / .cpp           # MPEG-TS demuxer
├── tsprocessor.h / .cpp           # MPEG-TS processor
├── tsFragmentProcessor.hpp / .cpp # TS fragment processor
├── ElementaryProcessor.h / .cpp   # Elementary stream processor
├── MediaStreamContext.h / .cpp    # Per-track streaming context
├── MetadataProcessor.hpp / .cpp   # ID3/metadata processor
├── ID3Metadata.hpp / .cpp         # ID3 metadata types
├── AampSCTE35.h                   # SCTE-35 splice point types
│                                  # (implementation in scte35/ subdir)
├── compositein_shim.*             # Composite video input shim
├── hdmiin_shim.*                  # HDMI input shim
├── videoin_shim.*                 # Generic video input shim
├── rmf_shim.*                     # RMF input shim
├── ota_shim.*                     # OTA tuner shim
├── AampMediaType.h                # Media type enumeration
├── AampDownloadInfo.hpp           # Download info structures
├── MediaSegmentDownloadJob.hpp    # Segment download job definition
├── AampFragmentDescriptor.*       # Fragment descriptor types
├── AampSegmentInfo.hpp            # Segment info types
├── AampTime.h                     # Time type utilities
├── AampSpeedCache.h               # Playback speed cache
├── AampLLDASHData.h               # Low-latency DASH data types
├── AampMPDPeriodInfo.h            # MPD period info types
├── AampDemuxDataTypes.h           # Demux data type definitions
├── StreamOutputFormat.h           # Stream output format enumeration
├── AudioTrackInfo.h               # Audio track info types
├── TextTrackInfo.h                # Text track info types
├── LangCodePreference.h           # Language code preference types
├── VideoZoomMode.h                # Video zoom mode enumeration
├── TimedMetadata.h                # Timed metadata types
├── Accessibility.hpp              # Accessibility descriptor types
├── lstring.hpp / .cpp             # Lightweight string utilities
├── iso639map.h / .cpp             # ISO-639 language code mapping
├── uint33_t.h                     # Custom 33-bit integer type
├── FragmentCacheDescriptor.h      # Fragment cache descriptor
├── AampBoxReader.h                # MP4 box reader helper
├── AampUnderflowMonitor.h / .cpp  # Buffer underflow monitor
├── ThunderAccess.cpp              # WPEFramework Thunder access helper
├── net_trace.h                    # Network trace utilities
├── gstaamptaskpool.h / .cpp       # GStreamer AAMP task pool
├── CMakeLists.txt                 # Root build definition (C++17, all targets)
│
├── abr/                           # Adaptive Bitrate subsystem
│   ├── abr.h / abr.cpp            # ABRManager — main ABR controller
│   ├── BandwidthEstimatorBase.h   # Strategy base for bandwidth estimators
│   ├── HarmonicEwmaEstimator.*    # EWMA-based bandwidth estimator
│   ├── RollingMedianOutlierEstimator.* # Outlier-filtered median estimator
│   └── docs/                      # ABR design documents
│
├── downloader/                    # HTTP download layer
│   ├── AampCurlDownloader.h / .cpp# cURL-based segment/manifest downloader
│   ├── AampCurlStore.h / .cpp     # cURL connection pool
│   └── AampCurlDefine.h          # cURL configuration constants
│
├── drm/                           # DRM license management (core)
│   ├── AampDRMLicManager.h / .cpp # DRM licence manager
│   ├── DrmInterface.h / .cpp      # DRM interface abstraction
│   └── (DrmSystems.h etc at root) # DRM type definitions live in root
│
├── dash/                          # DASH-specific utilities
│   ├── mpd/                       # MPD model and segmenter
│   │   ├── MPDModel.h / .cpp      # MPEG-DASH MPD object model
│   │   └── MPDSegmenter.h / .cpp  # MPD segment URL generation
│   ├── utils/                     # DASH URL / path utilities
│   │   ├── Path.h / .cpp
│   │   ├── Url.h / .cpp
│   │   └── Utils.h / .cpp
│   └── xml/                       # Lightweight XML DOM for MPD parsing
│       ├── DomDocument.h / .cpp
│       ├── DomElement.h / .cpp
│       └── DomNode.h / .cpp
│
├── isobmff/                       # ISO Base Media File Format (MP4/CMAF)
│   ├── isobmffbox.h / .cpp        # MP4 box parser
│   ├── isobmffbuffer.h / .cpp     # MP4 buffer reader
│   ├── isobmffhelper.h / .cpp     # High-level ISOBMFF utilities
│   └── isobmffprocessor.h / .cpp  # MP4 segment processor
│
├── mp4demux/                      # MP4 demuxer
│   ├── AampMp4Demuxer.h / .cpp    # MP4 demuxer using isobmff
│   └── MP4Demux.h / .cpp          # MP4 demux internal logic
│
├── scte35/                        # SCTE-35 splice point processing
│   └── AampSCTE35.cpp             # SCTE-35 parser implementation
│
├── subtitle/                      # Subtitle/caption parsing (core)
│   ├── webvttParser.h / .cpp      # WebVTT parser
│   └── vttCue.h                   # WebVTT cue types
│
├── tsb/                           # Time-Shift Buffer file-system store
│   ├── api/TsbApi.h               # TSB public API (used by AampTSBSessionManager)
│   └── src/
│       ├── TsbStore.cpp           # On-disk TSB store implementation
│       ├── TsbLog.h / .cpp        # TSB diagnostic logging
│       ├── TsbSem.h / .cpp        # TSB semaphore utilities
│       ├── TsbLocationLock.*      # TSB location locking
│       └── fs/                    # File-system abstraction for TSB store
│
├── middleware/                    # Platform middleware and RDK integration
│   ├── InterfacePlayerRDK.h / .cpp# GStreamer player interface for RDK
│   ├── InterfacePlayerPriv.h      # Private player interface data
│   ├── GstHandlerControl.h / .cpp # GStreamer handler control
│   ├── GstUtils.h / .cpp          # GStreamer utility functions
│   ├── PlayerScheduler.h / .cpp   # Middleware-level player scheduler
│   ├── PlayerUtils.h / .cpp       # Middleware utility functions
│   ├── SocUtils.h / .cpp          # SoC-specific utility helpers
│   ├── closedcaptions/            # Closed caption subsystem
│   │   ├── PlayerCCManager.h / .cpp
│   │   ├── rialto/                # Rialto CC bridge
│   │   └── subtec/                # SUBTEC CC bridge
│   ├── drm/                       # Platform DRM adapters
│   │   ├── aes/Aes.h / .cpp       # AES-128 DRM implementation
│   │   ├── helper/                # Per-DRM-system helper classes
│   │   │   ├── DrmHelper.h / .cpp
│   │   │   ├── ClearKeyHelper.*
│   │   │   ├── PlayReadyHelper.*
│   │   │   └── WidevineDrmHelper.*
│   │   └── ocdm/                  # OpenCDM session adapters
│   │       ├── OcdmBasicSessionAdapter.*
│   │       └── OcdmGstSessionAdapter.*
│   ├── externals/                 # External platform service wrappers
│   │   ├── PlayerExternalsInterface.h / .cpp
│   │   ├── PlayerRfc.h / .cpp     # RFC (Remote Feature Control) access
│   │   ├── PlayerThunderInterface.h / .cpp  # WPEFramework Thunder
│   │   └── contentsecuritymanager/ # Content security manager interface
│   ├── subtec/                    # SUBTEC subtitle engine
│   │   ├── libsubtec/             # SUBTEC library integration
│   │   └── subtecparser/          # SUBTEC timed-text parser
│   ├── subtitle/                  # Middleware subtitle support
│   ├── gst-plugins/               # Custom GStreamer plugins
│   │   └── drm/                   # DRM GStreamer plugin
│   ├── playerJsonObject/          # JSON object utilities for middleware
│   ├── playerLogManager/          # Middleware-level log manager
│   ├── playerisobmff/             # Middleware ISOBMFF utilities
│   ├── vendor/                    # SoC vendor platform adapters
│   │   ├── default/               # Default/reference implementation
│   │   ├── brcm/                  # Broadcom
│   │   ├── amlogic/               # Amlogic
│   │   ├── mtk/                   # MediaTek
│   │   └── realtek/               # Realtek
│   └── test/                      # Middleware unit tests
│
├── jsbindings/                    # JavaScript / UVE API bindings
│   ├── jsbindings.h / .cpp        # Core JS binding entry points
│   ├── jsmediaplayer.cpp          # JS media player class
│   ├── jsevent.h / .cpp           # JS event types
│   ├── jseventlistener.h / .cpp   # JS event listener
│   ├── jsutils.h / .cpp           # JS utility functions
│   └── PersistentWatermark/       # Persistent watermark support
│
├── simnet/                        # Network simulation for testing
│   ├── net_persona_fitter.h / .cpp# Matches download metrics to personas
│   └── simnet/                    # Simulated network backend
│
├── kotlin/                        # Kotlin CLI test tool
│   └── aampcli/main.kt            # Entry point for command-line player
│
├── cinterop/                      # C interoperability layer
│
├── cmake/                         # CMake find modules
│   ├── FindRialto.cmake
│   └── FindWPEFramework.cmake
│
├── OSX/                           # macOS-specific build patches and configs
│   └── patches/
│
├── support/                       # Build support tools and headers
│
├── scripts/                       # Developer install and setup scripts
│   ├── install-aamp.sh            # Primary install/build automation
│   ├── install_dependencies.sh
│   ├── install_gstreamer.sh
│   └── (other install_*.sh)
│
├── docs/                          # Developer documentation
│   └── GithubCopilot/
│
├── test/                          # Unit test suite (root-level engine tests)
│   └── utests/
│       ├── tests/                 # Per-component test directories (one dir per class)
│       ├── fakes/                 # Fake implementations (lightweight stubs)
│       ├── mocks/                 # Google Mock header-only mocks
│       ├── drm/                   # DRM-specific test helpers
│       └── ocdm/                  # OCDM test helpers
│
└── .github/                       # GitHub and AI agent configuration
    ├── instructions/              # AI coding instructions per domain
    │   ├── aamp.instructions.md   # AAMP architecture and conventions guide
    │   ├── cpp.instructions.md    # C++ coding standards
    │   ├── testing.instructions.md# Testing strategy instructions
    │   └── l1-*.instructions.md   # L1 unit test specific instructions
    ├── skills/                    # Agent skills
    │   └── aamp-l1-testing/       # L1 test writing skill
    └── workflows/                 # CI/CD GitHub Actions workflows
```

## Directory Purposes

**Root directory (`/`):**
- Purpose: Core engine source — the majority of production C++ lives here as flat files
- Contains: Protocol handlers, core player classes, stream sink, event system, TSB managers, TS/MP4 processing
- Key files: `priv_aamp.h`, `main_aamp.h`, `StreamAbstractionAAMP.h`, `fragmentcollector_mpd.cpp`, `fragmentcollector_hls.cpp`, `aampgstplayer.cpp`

**`abr/`:**
- Purpose: Adaptive Bitrate subsystem with pluggable bandwidth estimator implementations
- Key files: `abr.h`, `abr.cpp`, `BandwidthEstimatorBase.h`

**`downloader/`:**
- Purpose: All HTTP download logic; cURL connection pool and retry management
- Key files: `AampCurlDownloader.h`, `AampCurlStore.h`

**`drm/`:**
- Purpose: Core DRM licence acquisition and management
- Key files: `AampDRMLicManager.h`, `DrmInterface.h`

**`dash/`:**
- Purpose: MPEG-DASH specific parsing utilities; MPD object model; DASH URL helpers
- Key files: `dash/mpd/MPDModel.h`, `dash/xml/DomDocument.h`, `dash/utils/Url.h`

**`isobmff/`:**
- Purpose: ISO Base Media File Format (MP4/CMAF/fMP4) box parsing and processing
- Key files: `isobmffbox.h`, `isobmffbuffer.h`, `isobmffhelper.h`, `isobmffprocessor.h`

**`mp4demux/`:**
- Purpose: MP4 demultiplexer built on top of isobmff
- Key files: `AampMp4Demuxer.h`, `MP4Demux.h`

**`scte35/`:**
- Purpose: SCTE-35 splice point parser for ad insertion signals
- Key files: `AampSCTE35.cpp` (header at root: `AampSCTE35.h`)

**`tsb/`:**
- Purpose: File-system backed Time-Shift Buffer store; provides TSB API consumed by `AampTSBSessionManager`
- Key files: `tsb/api/TsbApi.h`, `tsb/src/TsbStore.cpp`

**`subtitle/`:**
- Purpose: Core subtitle/caption parsers (WebVTT)
- Key files: `subtitle/webvttParser.h`

**`middleware/`:**
- Purpose: Platform-specific RDK middleware integration; GStreamer player interface, DRM adapters, CC, SUBTEC, Thunder/Firebolt wrappers, SoC vendor backends
- Key files: `middleware/InterfacePlayerRDK.h`, `middleware/drm/helper/DrmHelper.h`, `middleware/drm/ocdm/OcdmBasicSessionAdapter.h`, `middleware/closedcaptions/PlayerCCManager.h`

**`jsbindings/`:**
- Purpose: UVE JavaScript API bindings; WK2 injected-bundle implementation
- Key files: `jsbindings/jsbindings.h`, `jsbindings/jsmediaplayer.cpp`

**`simnet/`:**
- Purpose: Network simulation for controlled testing under constrained bandwidth scenarios
- Key files: `simnet/net_persona_fitter.h`

**`test/utests/`:**
- Purpose: All L1 unit tests for root-level engine components
- Key files: `test/utests/tests/` (one subdirectory per class under test), `test/utests/fakes/` (fake stubs), `test/utests/mocks/` (Google Mock headers)

**`middleware/test/`:**
- Purpose: L1 unit tests for middleware components
- Key files: `middleware/test/utests/tests/`, `middleware/test/utests/fakes/`, `middleware/test/utests/mocks/`

## Key File Locations

**Entry Points:**
- `main_aamp.h`: Public `PlayerInstanceAAMP` class declaration
- `main_aamp.cpp`: `PlayerInstanceAAMP` method implementations
- `jsbindings/jsbindings.cpp`: UVE JS API entry point
- `kotlin/aampcli/main.kt`: CLI test tool entry point

**Configuration:**
- `CMakeLists.txt`: Root build definition; all library targets, compile flags, feature switches
- `AampConfig.h` / `AampConfig.cpp`: Runtime configuration system
- `AampDefine.h`: Core constants and type aliases
- `.github/instructions/aamp.instructions.md`: Authoritative architecture and coding guide
- `.github/instructions/cpp.instructions.md`: C++ coding standards

**Core Logic:**
- `priv_aamp.h` / `priv_aamp.cpp`: Core engine; central orchestrator
- `StreamAbstractionAAMP.h`: Protocol abstraction interface
- `fragmentcollector_mpd.cpp`: DASH fragment collector (~14k lines)
- `fragmentcollector_hls.cpp`: HLS fragment collector (~7.6k lines)
- `aampgstplayer.cpp`: GStreamer stream sink (~47k lines)
- `streamabstraction.cpp`: Base class shared fragment scheduling logic (~4.7k lines)

**DRM:**
- `drm/DrmSessionManager.h`: DRM session lifecycle
- `middleware/drm/helper/DrmHelper.h`: Platform DRM helper base
- `middleware/drm/ocdm/OcdmBasicSessionAdapter.h`: OpenCDM adapter

**TSB / DVR:**
- `AampTSBSessionManager.h`: TSB session controller
- `AampTsbDataManager.h`: In-memory fragment linked-list cache
- `AampTsbReader.h`: Playback read interface
- `tsb/api/TsbApi.h`: TSB file-system store API

**Testing:**
- `test/utests/tests/`: One subdirectory per class (e.g., `test/utests/tests/AampTsbReader/`)
- `test/utests/fakes/`: Fake implementations (`FakePrivateInstanceAAMP.cpp`, `FakeAampGstPlayer.cpp`, etc.)
- `test/utests/mocks/`: Google Mock headers (`MockPrivateInstanceAAMP.h`, `MockStreamSink.h`, etc.)

## Naming Conventions

**Files:**
- New AAMP class files: `PascalCase` with `Aamp` prefix — e.g., `AampScheduler.h`, `AampTsbReader.cpp`
- Legacy protocol handler files: `snake_case` — e.g., `fragmentcollector_hls.cpp`, `tsprocessor.cpp`
- Shim files: `snake_case` with `_shim` suffix — e.g., `hdmiin_shim.cpp`
- Header guards: `#pragma once` (preferred) or `#ifndef AAMP_CLASSNAME_H`
- Test files: class name + `Tests` suffix in a matching subdirectory — e.g., `test/utests/tests/AampSchedulerTests/`
- Fake files: `Fake` prefix — e.g., `FakeAampScheduler.cpp`
- Mock files: `Mock` prefix — e.g., `MockAampScheduler.h`

**Directories:**
- Subsystem directories: `lowercase` — e.g., `abr/`, `downloader/`, `isobmff/`, `tsb/`
- Test directories: `PascalCase` matching the class under test — e.g., `AampTsbReaderTests/`

## Where to Add New Code

**New Core Engine Class:**
- Header: root directory — `AampMyFeature.h`
- Implementation: root directory — `AampMyFeature.cpp`
- Register in `CMakeLists.txt` under `LIBAAMP_SOURCES`
- Tests: `test/utests/tests/AampMyFeatureTests/`
- Fake: `test/utests/fakes/FakeAampMyFeature.cpp`
- Mock: `test/utests/mocks/MockAampMyFeature.h`

**New Protocol Handler:**
- Inherit from `StreamAbstractionAAMP` (`StreamAbstractionAAMP.h`)
- Header: root — `fragmentcollector_myprotocol.h`
- Implementation: root — `fragmentcollector_myprotocol.cpp`

**New DRM System Support:**
- Helper: `middleware/drm/helper/MyDrmHelper.h` / `.cpp`
- Session adapter if OCDM-based: `middleware/drm/ocdm/`

**New Bandwidth Estimator:**
- Implement `BandwidthEstimatorBase` (`abr/BandwidthEstimatorBase.h`)
- Place in `abr/MyEstimator.h` / `abr/MyEstimator.cpp`
- Register in `ABRManager` (`abr/abr.cpp`)

**New Platform Middleware Feature:**
- Source: `middleware/` appropriate subdirectory
- External interface wrapper: `middleware/externals/`

**Utilities:**
- Shared utilities: `AampUtils.h` / `AampUtils.cpp` (root)
- String utilities: `lstring.hpp`
- Language code mapping: `iso639map.h`
- DASH-specific utilities: `dash/utils/`

## Special Directories

**`.planning/codebase/`:**
- Purpose: AI-generated codebase analysis documents
- Generated: Yes (by GSD map-codebase)
- Committed: Yes

**`.github/instructions/`:**
- Purpose: AI agent coding instructions and architecture guides
- Generated: No (maintained by team)
- Committed: Yes

**`middleware/vendor/`:**
- Purpose: SoC-vendor-specific platform backend implementations (Broadcom, Amlogic, MTK, Realtek)
- Generated: No
- Committed: Yes

**`OSX/patches/`:**
- Purpose: macOS build patches for platform compatibility
- Generated: No
- Committed: Yes

---

*Structure analysis: 2026-06-08*
