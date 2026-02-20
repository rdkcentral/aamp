# AAMP Code Organization

## Table of Contents
1. [Repository Structure](#repository-structure)
2. [Folder-by-Folder Breakdown](#folder-by-folder-breakdown)
3. [File-by-File Breakdown](#file-by-file-breakdown)
4. [Dependencies and Relationships](#dependencies-and-relationships)

## Repository Structure

### Top-Level Organization

```
aamp_linux/
├── abr/                    # Adaptive Bitrate Management
├── dash/                   # DASH Manifest Parsing
├── downloader/             # HTTP Download Management
├── drm/                    # DRM Interface Layer
├── isobmff/                # ISO Base Media File Format Processing
├── jsbindings/             # JavaScript/WebKit Bindings
├── middleware/             # Platform Integration Layer
├── mp4demux/               # MP4 Demuxer
├── scte35/                 # SCTE-35 Ad Signaling
├── subtitle/               # Subtitle Processing
├── tsb/                    # Time Shift Buffer API
├── test/                   # Test Infrastructure
├── support/                # Support Libraries
├── doc/                    # Documentation (this folder)
└── [root files]            # Core AAMP files
```

## Folder-by-Folder Breakdown

### Root Directory Files

**Important:** Many core implementation files live in the repository **root** (same directory as `doc/`), not in subfolders. The following breakdown includes both root and subsystem directories.

#### Core Player Files

**`main_aamp.h/cpp`**
- **Purpose**: Public API interface (`PlayerInstanceAAMP` class)
- **Key Classes**: `PlayerInstanceAAMP`
- **Responsibilities**:
  - Public method exposure (Tune, Seek, SetRate, etc.)
  - Event listener registration
  - Configuration management
  - JavaScript binding integration
- **Dependencies**: `priv_aamp.h`, `AampConfig.h`, `AampEventManager.h`

**`priv_aamp.h/cpp`**
- **Purpose**: Internal player implementation
- **Key Classes**: `PrivateInstanceAAMP`
- **Responsibilities**:
  - Core playback logic
  - GStreamer pipeline management
  - State machine management
  - Error handling and recovery
- **Dependencies**: `StreamAbstractionAAMP.h`, `aampgstplayer.h`, `AampConfig.h`

**`streamabstraction.cpp` / `StreamAbstractionAAMP.h`**
- **Purpose**: Base class for protocol-specific implementations
- **Key Classes**: `StreamAbstractionAAMP`, `MediaTrack`
- **Responsibilities**:
  - Common fragment caching and injection logic
  - MediaTrack management (video, audio, subtitle)
  - ABR coordination
  - Discontinuity handling
- **Dependencies**: `priv_aamp.h`, `CachedFragment.h`, `MediaProcessor.h`

#### Fragment Collectors

**`fragmentcollector_hls.h/cpp`**
- **Purpose**: HLS (HTTP Live Streaming) fragment collection
- **Key Classes**: `StreamAbstractionAAMP_HLS`, `TrackState`
- **Key Structures**: `HlsStreamInfo`, `MediaInfo`, `IndexNode`
- **Responsibilities**:
  - HLS playlist parsing
  - Fragment URL generation
  - Playlist refresh management
  - Track synchronization (A/V sync)
  - DRM key tag handling
- **Dependencies**: `StreamAbstractionAAMP.h`, `HlsDrmBase.h`, `AampDRMLicPreFetcher.h`

**`fragmentcollector_mpd.h/cpp`**
- **Purpose**: DASH (Dynamic Adaptive Streaming over HTTP) fragment collection
- **Key Classes**: `StreamAbstractionAAMP_MPD`
- **Key Structures**: `ProfileInfo`, `TimeSyncClient`
- **Responsibilities**:
  - DASH MPD parsing (using libdash)
  - Period and adaptation set management
  - Segment URL generation
  - Low-latency DASH support
  - Ad insertion support
- **Dependencies**: `StreamAbstractionAAMP.h`, `AampMPDDownloader.h`, `AampMPDParseHelper.h`, `libdash`

**`fragmentcollector_progressive.h/cpp`**
- **Purpose**: Progressive MP4 playback
- **Key Classes**: `StreamAbstractionAAMP_Progressive`
- **Responsibilities**:
  - Progressive MP4 download
  - Simple playback without manifest parsing
- **Dependencies**: `StreamAbstractionAAMP.h`

#### GStreamer Integration

**`aampgstplayer.h/cpp`**
- **Purpose**: GStreamer pipeline management
- **Key Classes**: `AAMPGstPlayer`
- **Responsibilities**:
  - Pipeline creation and configuration
  - Fragment injection into GStreamer
  - Stream sink management
  - Pipeline state management
- **Dependencies**: GStreamer libraries, `priv_aamp.h`, `StreamSink.h`

**`gstaamptaskpool.h/cpp`**
- **Purpose**: GStreamer task pool for thread management
- **Dependencies**: GStreamer libraries

#### Buffer Management

**`AampBufferControl.h/cpp`**
- **Purpose**: Buffer health monitoring and control
- **Key Classes**: `AampBufferControl`
- **Responsibilities**:
  - Buffer level monitoring
  - Underflow detection
  - Buffer health status reporting
- **Dependencies**: `priv_aamp.h`

**`AampTimeBasedBufferManager.h/cpp`**
- **Purpose**: Time-based buffering strategy
- **Key Classes**: `AampTimeBasedBufferManager`
- **Responsibilities**:
  - Time-based buffer management
  - Fragment download scheduling
  - Buffer duration tracking
- **Dependencies**: `AampTime.h`, `CachedFragment.h`

**`AampGrowableBuffer.h/cpp`**
- **Purpose**: Dynamic buffer for fragment data
- **Key Classes**: `AampGrowableBuffer`
- **Responsibilities**:
  - Dynamic memory allocation for fragments
  - Buffer growth management
- **Dependencies**: None (standalone utility)

**`CachedFragment.h/cpp`**
- **Purpose**: Fragment cache structure
- **Key Structures**: `CachedFragment`
- **Responsibilities**:
  - Fragment metadata storage
  - Fragment data buffer
  - Download tracking
- **Dependencies**: `AampGrowableBuffer.h`

#### Configuration

**`AampConfig.h/cpp`**
- **Purpose**: Configuration management system
- **Key Classes**: `AampConfig`
- **Responsibilities**:
  - Configuration parameter storage
  - Configuration file parsing (text and JSON)
  - Environment variable reading
  - Operator configuration (RFC)
  - Configuration priority management
- **Dependencies**: `AampDefine.h`, `cjson`

#### Event System

**`AampEvent.h/cpp`**
- **Purpose**: Event data structures
- **Key Classes**: `AAMPEvent`, event-specific classes
- **Responsibilities**:
  - Event data encapsulation
  - Event type definitions
- **Dependencies**: `AampDefine.h`

**`AampEventListener.h/cpp`**
- **Purpose**: Event listener interface
- **Key Classes**: `EventListener`
- **Responsibilities**:
  - Event callback interface
  - Virtual methods for event handling
- **Dependencies**: `AampEvent.h`

**`AampEventManager.h/cpp`**
- **Purpose**: Event dispatch and listener management
- **Key Classes**: `AampEventManager`
- **Responsibilities**:
  - Event queuing
  - Listener registration
  - Async event dispatch
  - Event statistics
- **Dependencies**: `AampEvent.h`, `AampEventListener.h`

#### Utilities

**`AampUtils.h/cpp`**
- **Purpose**: General utility functions
- **Responsibilities**:
  - URL manipulation
  - String utilities
  - Time conversion
  - Logging helpers
- **Dependencies**: Standard library

**`AampScheduler.h/cpp`**
- **Purpose**: Task scheduling system
- **Key Classes**: `AampScheduler`
- **Responsibilities**:
  - Async task scheduling
  - Worker thread management
- **Dependencies**: Standard threading libraries

**`AampJsonObject.h/cpp`**
- **Purpose**: JSON parsing utilities
- **Key Classes**: `AampJsonObject`
- **Dependencies**: `cjson`

**`AampProfiler.h/cpp`**
- **Purpose**: Performance profiling
- **Key Classes**: `AampProfiler`
- **Responsibilities**:
  - Performance metrics collection
  - Tune time profiling
- **Dependencies**: Standard library

### Subsystem Directories

#### `abr/` - Adaptive Bitrate Management

**Files**:
- `abr.h/cpp`: ABR manager implementation
- `NetworkBandwidthEstimator.h/cpp`: Network bandwidth estimation

**Purpose**: Intelligent bitrate selection based on network conditions

**Key Classes**:
- `ABRManager`: Main ABR decision engine
- `NetworkBandwidthEstimator`: Bandwidth calculation

**Responsibilities**:
- Profile selection based on bandwidth
- Ramp-up/ramp-down logic
- Buffer-based decisions
- Network consistency checking

**Dependencies**: `AampMediaType.h`, `AampConfig.h`

#### DASH Manifest Parsing (No Top-Level `dash/` Folder)

**Note:** This repository does **not** contain a top-level `dash/` directory with MPD model or XML DOM code. DASH manifest parsing is implemented as follows:

- **Primary implementation**: `fragmentcollector_mpd.h/cpp` (StreamAbstractionAAMP_MPD, fragment collection and segment URL generation).
- **Helpers**: `AampMPDDownloader`, `AampMPDParseHelper`, and related MPD utilities (exact filenames in root or support locations; see build/CMake for full list).
- **External dependency**: **libdash** is used for MPD parsing (industry-standard library). Tests reference `libdash/IMPD.h`, `libdash/INode.h`, etc.

**Purpose**: DASH (MPEG-DASH) manifest parsing and segment handling via libdash and AAMP-specific helpers.

**Dependencies**: `libdash`, `libxml2` (where used)

#### `downloader/` - HTTP Download Management

**Files**:
- `AampCurlDownloader.h/cpp`: Main downloader implementation
- `AampCurlStore.h/cpp`: Connection pooling and reuse
- `AampCurlDefine.h`: Downloader constants and types

**Purpose**: HTTP/HTTPS fragment and manifest downloads

**Key Classes**:
- `AampCurlDownloader`: Download manager
- `AampCurlStore`: Connection store for reuse

**Responsibilities**:
- HTTP/HTTPS downloads using libcurl
- Connection reuse
- Download metrics
- Retry logic
- Timeout handling

**Dependencies**: `libcurl`, `AampMediaType.h`

#### `drm/` - DRM Interface Layer

**Files**:
- `AampDRMLicManager.h/cpp`: License management
- `DrmInterface.h/cpp`: DRM interface abstraction

**Purpose**: High-level DRM operations

**Key Classes**:
- `AampDRMLicManager`: License acquisition and management
- `DrmInterface`: DRM system interface

**Dependencies**: `middleware/drm/` (actual DRM implementations)

#### `isobmff/` - ISO Base Media File Format

**Files**:
- `isobmffbox.h/cpp`: Box parsing
- `isobmffbuffer.h/cpp`: Buffer management
- `isobmffhelper.h/cpp`: Helper utilities
- `isobmffprocessor.h/cpp`: Fragment processing

**Purpose**: ISO BMFF (MP4) fragment parsing and processing

**Key Classes**:
- `IsoBmffBox`: Box structure
- `IsoBmffBuffer`: Buffer for ISO BMFF data
- `IsoBmffHelper`: Parsing utilities
- `IsoBmffProcessor`: Fragment processor

**Responsibilities**:
- MP4 box parsing
- PTS/DTS extraction
- Fragment metadata extraction

#### `jsbindings/` - JavaScript Bindings

**Files**:
- `jsbindings.h/cpp`: Core binding implementation
- `jsmediaplayer.h/cpp`: Media player JS API
- `jsevent.h/cpp`: Event JS API
- `jseventlistener.h/cpp`: Event listener JS API
- `jsutils.h/cpp`: JavaScript utilities
- `jscontroller-jsbindings.cpp`: Controller bindings
- `PersistentWatermark/`: Watermarking support

**Purpose**: JavaScript/WebKit integration (UVE API)

**Key Classes**:
- JavaScript object wrappers for AAMP classes
- Event binding to JavaScript callbacks

**Dependencies**: JavaScriptCore (WebKit), `main_aamp.h`

#### `middleware/` - Platform Integration

**Subdirectories**:
- `drm/`: DRM middleware implementations
- `closedcaptions/`: Closed caption management
- `externals/`: External service integration
- `gst-plugins/`: GStreamer plugins
- `playerLogManager/`: Logging system
- `subtec/`: Subtec subtitle integration

**Purpose**: Platform-specific integrations

**Key Components**:
- DRM implementations (Widevine, PlayReady, ClearKey, etc.)
- Closed caption rendering
- Thunder/RPC integration
- RFC configuration
- GStreamer plugins

#### `mp4demux/` - MP4 Demuxer

**Files**:
- `AampMp4Demuxer.h/cpp`: MP4 demuxer
- `MP4Demux.h/cpp`: MP4 demuxing utilities

**Purpose**: MP4 file demuxing

#### `scte35/` - SCTE-35 Ad Signaling

**Files**:
- `AampSCTE35.h/cpp`: SCTE-35 parsing

**Purpose**: SCTE-35 ad cue parsing

#### `subtitle/` - Subtitle Processing

**Files**:
- `webvttParser.h/cpp`: WebVTT parser
- `vttCue.h`: VTT cue structure

**Purpose**: Subtitle parsing and processing

#### `tsb/` - Time Shift Buffer

**Files**:
- `api/TsbApi.h`: TSB API interface
- `src/TsbStore.cpp`: TSB storage implementation
- `src/TsbLog.cpp`: TSB logging
- `src/TsbSem.cpp`: Semaphore utilities
- `src/TsbLocationLock.cpp`: Location locking

**Purpose**: Time-shifted playback support

**Key Classes**:
- TSB API interface
- Storage management
- Session management

#### `test/` - Test Infrastructure

**Subdirectories**:
- `utests/`: Unit tests
- `aampcli/`: Command-line interface
- `gstTestHarness/`: GStreamer test harness
- `jsBindingTest/`: JavaScript binding tests
- `mocks/`: Mock objects for testing

**Purpose**: Testing and validation

## File-by-File Breakdown

### Core Entry Points

1. **`main_aamp.cpp`** - Application entry point
   - `PlayerInstanceAAMP` constructor
   - Global configuration initialization
   - JavaScript binding loading

2. **`priv_aamp.cpp`** - Internal player implementation
   - `PrivateInstanceAAMP` class
   - GStreamer pipeline management
   - State machine

3. **`streamabstraction.cpp`** - Stream abstraction base
   - `MediaTrack` implementation
   - Fragment injection logic
   - Buffer management

### Fragment Collection

4. **`fragmentcollector_hls.cpp`** - HLS implementation
   - Playlist parsing
   - Fragment URL generation
   - Track synchronization

5. **`fragmentcollector_mpd.cpp`** - DASH implementation
   - MPD parsing
   - Period management
   - Segment URL generation

6. **`fragmentcollector_progressive.cpp`** - Progressive MP4
   - Simple progressive download

### Media Processing & Track Workers

7. **`MediaStreamContext.h/cpp`** - DASH stream context
   - Per-stream fragment caching and injection state
   - Used by StreamAbstractionAAMP_MPD

8. **`ElementaryProcessor.cpp`** - Elementary stream processing
   - PTS/DTS handling and stream processing

9. **`tsprocessor.h/cpp`** - Transport stream processing
   - TS demuxing and packet processing

10. **`tsDemuxer.hpp/cpp`** - TS demuxer
    - Transport stream parsing

11. **`tsFragmentProcessor.cpp`** - TS fragment processing
    - TS fragment demuxing

12. **`AampTrackWorkerManager.cpp`** (and related header) - Track worker threads
    - Spawns and manages workers for video/audio/auxiliary tracks
    - Work queue and coordination with fragment collectors

13. **`AampTimeBasedBufferManager.h/cpp`** - Time-based buffering
    - Buffer duration tracking and thresholds
    - Used by ABR and buffer health logic

### DRM

14. **`AampDRMLicPreFetcher.cpp`** (if present) - License prefetching
15. **`drm/DrmInterface.cpp`**, **`drm/AampDRMLicManager`** (as built) - License acquisition and key management

### Utilities

16. **`AampUtils.cpp`** - General utilities
17. **`AampMPDUtils.cpp`** (if present) - DASH utilities
18. **`AampMPDDownloader.cpp`** (or equivalent) - MPD downloader
19. **`AampMPDParseHelper.cpp`** (or equivalent) - MPD parsing helpers
20. **`iso639map.cpp`** (if present) - Language code mapping

### Event System

21. **`AampEvent.cpp`** - Event data structures
22. **`AampEventManager.cpp`** - Event dispatch

### Configuration

23. **`AampConfig.cpp`** - Configuration management

### TSB

24. **`AampTSBSessionManager.cpp`** - TSB session management
25. **`AampTsbDataManager.cpp`** (if present) - TSB data management
26. **`AampTsbReader.cpp`** (if present) - TSB reading
27. **`AampTsbMetaDataManager.cpp`** (if present) - TSB metadata  
    TSB API and storage: **`tsb/api/TsbApi.h`**, **`tsb/src/TsbStore.cpp`**, etc.

### Ad Management

28. **`admanager_mpd.h/cpp`** - DASH ad management
29. **`AdManagerBase.h`** - Base type for ad managers

### Metadata & Other

30. **`MetadataProcessor.cpp`** - Metadata processing
31. **`ID3Metadata.cpp`** (if present) - ID3 metadata processing
32. **`AampCacheHandler.h/cpp`** - Fragment/cache handling utilities
33. **`videoin_shim.h/cpp`** - Video input shim (platform-specific; purpose as in source)

## Dependencies and Relationships

### Internal Dependency Graph

```
PlayerInstanceAAMP
    └──> PrivateInstanceAAMP
            └──> StreamAbstractionAAMP
                    ├──> FragmentCollector_HLS
                    ├──> FragmentCollector_MPD
                    └──> FragmentCollector_Progressive
                            └──> MediaTrack
                                    ├──> CachedFragment
                                    ├──> MediaProcessor
                                    └──> IsoBmffHelper
```

### External Dependencies

1. **GStreamer** (1.18.0+)
   - Used by: `aampgstplayer.cpp`, `middleware/gst-plugins/`

2. **libcurl**
   - Used by: `downloader/AampCurlDownloader.cpp`

3. **libdash**
   - Used by: `fragmentcollector_mpd.cpp`, `dash/mpd/`

4. **libxml2**
   - Used by: `dash/xml/`, `AampMPDParseHelper.cpp`

5. **OpenSSL**
   - Used by: DRM system, HTTPS downloads

6. **JavaScriptCore** (WebKit)
   - Used by: `jsbindings/`

7. **cjson**
   - Used by: `AampJsonObject.cpp`, `AampConfig.cpp`

### Build Dependencies

- **CMake** (3.5+)
- **C++17** compiler
- **pkg-config** for dependency resolution

## Summary

The AAMP codebase is organized into clear subsystems:

1. **Core Player**: Entry points and main player logic
2. **Fragment Collection**: Protocol-specific collectors
3. **Media Processing**: Fragment processing and injection
4. **Support Systems**: ABR, DRM, Events, Configuration
5. **Platform Integration**: Middleware and bindings
6. **Utilities**: Helper functions and tools

This organization enables:
- Clear separation of concerns
- Easy protocol extension
- Platform-specific customization
- Comprehensive testing
