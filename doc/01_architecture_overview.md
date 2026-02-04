# High-Level Architecture Overview

## AAMP System Architecture and Component Design

[← Back to Index](README.md) | [Next: Code Organization →](02_code_organization.md)

## 1. System Overview

Advanced Adaptive Media Player (AAMP) is a native video engine built on top of GStreamer, optimized for performance, memory use, and code size. AAMP supports:

- **HLS (HTTP Live Streaming)** - Apple's adaptive streaming protocol
- **DASH (Dynamic Adaptive Streaming over HTTP)** - MPEG-DASH standard
- **Progressive Playback** - Direct MP4 file playback
- **Multiple DRM Systems** - Widevine, PlayReady, Adobe Access, CONSEC, ClearKey
- **Adaptive Bitrate (ABR)** - Dynamic quality adjustment based on network conditions

## 2. High-Level Architecture Diagram

```
┌─────────────────────────────────────────────────────────────┐
│                    Application Layer                         │
│  ┌──────────────┐  ┌──────────────┐  ┌──────────────┐      │
│  │ JavaScript/  │  │     CLI      │  │   Native     │      │
│  │   Web App    │  │ Application  │  │ Application  │      │
│  └──────┬───────┘  └──────┬───────┘  └──────┬───────┘      │
└─────────┼─────────────────┼─────────────────┼──────────────┘
          │                 │                 │
          └─────────────────┼─────────────────┘
                            │
┌───────────────────────────▼───────────────────────────────────┐
│              AAMP Public API Layer                           │
│                   PlayerInstanceAAMP                          │
└───────────────────────────┬───────────────────────────────────┘
                            │
┌───────────────────────────▼───────────────────────────────────┐
│                  AAMP Core Layer                              │
│  ┌──────────────────┐  ┌──────────────────┐                 │
│  │PrivateInstanceAAMP│  │   AampConfig     │                 │
│  └────────┬─────────┘  └──────────────────┘                 │
│           │                                                   │
│  ┌────────▼─────────┐  ┌──────────────────┐                 │
│  │ AampEventManager │  │  AampScheduler   │                 │
│  └──────────────────┘  └──────────────────┘                 │
└───────────────────────────┬───────────────────────────────────┘
                            │
┌───────────────────────────▼───────────────────────────────────┐
│            Stream Abstraction Layer                            │
│  ┌──────────────────────────────────────────────────────┐    │
│  │         StreamAbstractionAAMP (Base)                 │    │
│  └───────┬──────────────┬──────────────┬────────────────┘    │
│          │              │              │                      │
│  ┌───────▼──────┐ ┌─────▼──────┐ ┌────▼──────────────┐      │
│  │     HLS      │ │    DASH    │ │   Progressive     │      │
│  └──────────────┘ └────────────┘ └───────────────────┘      │
└───────────────────────────┬───────────────────────────────────┘
                            │
┌───────────────────────────▼───────────────────────────────────┐
│            Fragment Collection Layer                          │
│  ┌──────────────┐  ┌──────────────┐  ┌──────────────┐       │
│  │FragmentColl  │  │FragmentColl  │  │FragmentColl  │       │
│  │    _HLS      │  │    _MPD      │  │ Progressive  │       │
│  └──────┬───────┘  └──────┬───────┘  └──────┬───────┘       │
│         │                 │                 │                │
│         └─────────────────┼─────────────────┘                │
│                           │                                   │
│                  ┌────────▼────────┐                         │
│                  │   MediaTrack    │                         │
│                  └────────┬────────┘                         │
└───────────────────────────┼───────────────────────────────────┘
                            │
┌───────────────────────────▼───────────────────────────────────┐
│         Download & Processing Layer                          │
│  ┌──────────────┐  ┌──────────────┐  ┌──────────────┐       │
│  │  Downloader │  │    Cache     │  │    Worker    │       │
│  └──────────────┘  └──────────────┘  └──────────────┘       │
└───────────────────────────┬───────────────────────────────────┘
                            │
┌───────────────────────────▼───────────────────────────────────┐
│                    DRM Layer                                  │
│  ┌──────────────┐  ┌──────────────┐                          │
│  │DRMLicManager │  │ DrmInterface │                          │
│  └──────┬───────┘  └──────┬───────┘                          │
│         │                 │                                   │
│  ┌──────▼──────┐  ┌───────▼──────┐                           │
│  │  Widevine   │  │  PlayReady   │                           │
│  └─────────────┘  └──────────────┘                           │
└───────────────────────────┬───────────────────────────────────┘
                            │
┌───────────────────────────▼───────────────────────────────────┐
│                 GStreamer Layer                               │
│  ┌──────────────┐  ┌──────────────┐  ┌──────────────┐       │
│  │AAMPGstPlayer │  │ StreamSink   │  │   Pipeline   │       │
│  └──────────────┘  └──────────────┘  └──────────────┘       │
└───────────────────────────────────────────────────────────────┘
```

## 3. Core Components

### 3.1 PlayerInstanceAAMP (Public API)

The main public interface exposed to applications. Located in `main_aamp.h/cpp`.

- **Purpose:** Provides C++ API for player operations
- **Key Methods:** Tune(), Stop(), Seek(), SetRate(), SetLanguage(), etc.
- **Thread Safety:** Public API methods are thread-safe

### 3.2 PrivateInstanceAAMP (Core Engine)

The internal implementation of the player. Located in `priv_aamp.h/cpp`.

- **Purpose:** Core player logic, state management, orchestration
- **Responsibilities:**
  - Manifest parsing and management
  - Stream abstraction creation (HLS/DASH/Progressive)
  - ABR (Adaptive Bitrate) management
  - Event generation and dispatch
  - Configuration management

### 3.3 StreamAbstractionAAMP (Protocol Abstraction)

Base class for protocol-specific implementations. Located in `StreamAbstractionAAMP.h`.

- **Purpose:** Abstract interface for different streaming protocols
- **Implementations:**
  - `StreamAbstractionAAMP_HLS` - HLS protocol handler
  - `StreamAbstractionAAMP_MPD` - DASH protocol handler
  - `StreamAbstractionAAMP_Progressive` - Progressive download handler

### 3.4 Fragment Collectors

Protocol-specific fragment collection and processing:

- **FragmentCollector_HLS** (`fragmentcollector_hls.h/cpp`):
  - HLS playlist parsing
  - Fragment downloading and sequencing
  - Audio/Video track synchronization
  - DRM key management for HLS

- **FragmentCollector_MPD** (`fragmentcollector_mpd.h/cpp`):
  - MPD manifest parsing (using libdash)
  - Period and adaptation set management
  - Fragment timing and synchronization
  - Low-latency DASH support

### 3.5 AAMPGstPlayer (GStreamer Integration)

GStreamer pipeline management and media playback. Located in `aampgstplayer.h/cpp`.

- **Purpose:** Bridge between AAMP and GStreamer
- **Responsibilities:**
  - Pipeline creation and management
  - Buffer injection into GStreamer
  - Playback state management
  - DRM decryption integration
  - Event handling from GStreamer

### 3.6 DRM System

Digital Rights Management integration. Located in `drm/` directory.

- **AampDRMLicManager:** License acquisition and management
- **DrmInterface:** Abstract DRM interface
- **Supported DRMs:** Widevine, PlayReady, Adobe Access, CONSEC, ClearKey

### 3.7 Download System

Network download management. Located in `downloader/` directory.

- **AampCurlDownloader:** HTTP/HTTPS download using libcurl
- **AampCurlStore:** Connection pooling and management
- **Features:** Retry logic, timeout handling, bandwidth monitoring

## 4. Data Flow Architecture

### Sequence Diagram: Tune to Playback

```
Application → PlayerInstanceAAMP → PrivateInstanceAAMP
    ↓
Initialize Config
    ↓
Create StreamAbstraction
    ↓
Download Manifest
    ↓
Parse Manifest
    ↓
Download Init Fragment
    ↓
Acquire License (if encrypted)
    ↓
Download Media Fragment
    ↓
Decrypt Fragment
    ↓
Inject into GStreamer
    ↓
Decode & Render
```

## 5. Threading Model

AAMP uses a multi-threaded architecture:

- **Main Thread:** Application API calls, event dispatch
- **Scheduler Thread:** Task scheduling and async operations (AampScheduler)
- **Playlist Downloader Threads:** Per-track threads for playlist refresh (HLS)
- **Fragment Downloader Threads:** Worker threads for parallel fragment downloads
- **GStreamer Threads:** GStreamer's internal threading for pipeline processing
- **DRM Threads:** License acquisition and decryption operations

> **Thread Safety:** Most AAMP operations are thread-safe, with mutex protection for shared state. However, some operations require specific thread context (e.g., GStreamer operations must be on main thread).

## 6. Configuration System

The configuration system (`AampConfig`) supports multiple configuration sources with priority:

1. **Default Settings** (lowest priority) - Hardcoded defaults
2. **Operator Configuration** - RFC/Environment variables
3. **Stream Settings** - Settings from manifest
4. **Application Settings** - Runtime API calls
5. **Dev Configuration** (highest priority) - /opt/aamp.cfg or /opt/aampcfg.json

Configuration covers:

- ABR settings (enable/disable, thresholds, cache sizes)
- Network timeouts and retry limits
- DRM settings (license server URLs, caching)
- Playback behavior (buffering, trickplay, etc.)
- Logging and debugging options

## 7. Event System

AAMP uses an event-driven architecture for notifying applications of player state changes:

- **AampEventManager:** Central event dispatcher
- **Event Types:**
  - State changes (IDLE, PREPARING, PLAYING, PAUSED, etc.)
  - Progress events (playback position, duration)
  - Bitrate changes
  - DRM events (license acquisition, key rotation)
  - Error events
  - Timed metadata (ads, program information)
- **Event Listeners:** Applications register listeners for specific event types

## 8. Memory Management

AAMP uses modern C++11 memory management:

- **Smart Pointers:** Extensive use of `std::shared_ptr` and `std::unique_ptr`
- **RAII:** Resource management through constructors/destructors
- **Buffer Management:** Custom buffer classes (`AampGrowableBuffer`) for efficient memory usage
- **Caching:** Fragment and playlist caching with size limits

---

[← Back to Index](README.md) | [Next: Code Organization →](02_code_organization.md)

