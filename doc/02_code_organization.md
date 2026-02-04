# Code Organization and Flow Description

## Folder structure, module purposes, and execution flows

[← Back to Index](README.md) | [← Previous: Architecture](01_architecture_overview.md) | [Next: APIs and Classes →](03_apis_classes.md)

## 1. Repository Structure

```
aamp-dev_sprint_25_2/
├── main_aamp.h/cpp          # Public API (PlayerInstanceAAMP)
├── priv_aamp.h/cpp         # Core engine (PrivateInstanceAAMP)
├── aampgstplayer.h/cpp     # GStreamer integration
├── StreamAbstractionAAMP.h # Stream abstraction base
├── streamabstraction.cpp   # Stream abstraction implementation
│
├── fragmentcollector_hls.h/cpp    # HLS fragment collection
├── fragmentcollector_mpd.h/cpp     # DASH fragment collection
├── fragmentcollector_progressive.h/cpp # Progressive playback
│
├── drm/                    # DRM system
│   ├── AampDRMLicManager.h/cpp
│   ├── DrmInterface.h/cpp
│   └── ...
│
├── downloader/             # Network download layer
│   ├── AampCurlDownloader.h/cpp
│   ├── AampCurlStore.h/cpp
│   └── ...
│
├── dash/                   # DASH parsing library
│   ├── mpd/                # MPD model
│   ├── xml/                 # XML parsing
│   └── utils/               # Utilities
│
├── isobmff/                # ISO Base Media File Format
├── subtitle/               # Subtitle processing
├── scte35/                 # SCTE-35 ad signaling
├── tsb/                    # Time Shift Buffer
├── middleware/             # Middleware components
├── jsbindings/             # JavaScript bindings
├── test/                   # Test code
└── support/                # Support utilities
```

## 2. Main Entry Points

### 2.1 Application Entry Points

- **JavaScript/Web:** `jsbindings/jsmediaplayer.cpp` - JS API wrapper
- **CLI:** `test/aampcli/Aampcli.cpp` - Command-line interface
- **Native C++:** `main_aamp.h` - Direct C++ API

### 2.2 Player Initialization Flow

```
Application
    ↓
new PlayerInstanceAAMP()
    ↓
Initialize Global Config (first time)
    ↓
Read aamp.cfg / aampcfg.json
    ↓
Read Environment Variables
    ↓
Read Operator Config
    ↓
Create PrivateInstanceAAMP
    ↓
Initialize Members
    ↓
Start Scheduler Thread
    ↓
Create StreamSink
    ↓
Create AAMPGstPlayer
    ↓
Player Ready
```

## 3. Tune API Execution Flow

### 3.1 Complete Tune Flow

```
Application
    ↓
Tune(url, autoPlay, ...)
    ↓
ManageAsyncTuneConfig()
    ↓
[Async Tune Enabled?]
    ├─ Yes → Schedule Tune Task
    └─ No  → TuneInternal()
    ↓
Stop Previous Tune (if any)
    ↓
Load Tune Settings
    ↓
Apply Channel Overrides
    ↓
Determine Media Format
    ↓
Create StreamAbstraction
    ↓
Initialize FragmentCollector
    ↓
Download Manifest
    ↓
Parse Manifest
    ↓
Build Fragment Index
    ↓
Download Init Fragment
    ↓
[Encrypted Content?]
    ├─ Yes → Acquire License
    └─ No  → Continue
    ↓
Download Media Fragment
    ↓
[Encrypted?]
    ├─ Yes → Decrypt Fragment
    └─ No  → Continue
    ↓
Inject Fragment into GStreamer
    ↓
Decode & Render
    ↓
Playback Events
    ↓
State/Progress Events to Application
```

### 3.2 Code Flow Details

**Step 1: API Entry (main_aamp.cpp:281)**
- `PlayerInstanceAAMP::Tune()` - Validates parameters, handles async mode

**Step 2: Internal Tune (main_aamp.cpp:322)**
- `PlayerInstanceAAMP::TuneInternal()` - Stops previous tune, calls core

**Step 3: Core Tune (priv_aamp.cpp:5752)**
- `PrivateInstanceAAMP::Tune()` - Main orchestration logic
  - Configuration loading (lines 5770-5819)
  - State initialization (line 5782)
  - Media format detection (line 5881)
  - Stream abstraction creation (lines 5287-5317)

**Step 4: Stream Initialization**
- HLS: `StreamAbstractionAAMP_HLS::Init()`
- DASH: `StreamAbstractionAAMP_MPD::Init()`

**Step 5: Fragment Collection**
- Fragment collectors start downloading and processing fragments

## 4. Class Relationships

### Core Class Diagram

```
PlayerInstanceAAMP
    ├── PrivateInstanceAAMP
    │       ├── StreamAbstractionAAMP
    │       │       ├── StreamAbstractionAAMP_HLS
    │       │       │       └── FragmentCollector_HLS
    │       │       ├── StreamAbstractionAAMP_MPD
    │       │       │       └── FragmentCollector_MPD
    │       │       └── StreamAbstractionAAMP_Progressive
    │       ├── AAMPGstPlayer
    │       ├── AampEventManager
    │       └── AampDRMLicenseManager
    └── AampConfig
```

## 5. Module Interactions

### 5.1 Fragment Download and Injection

```
MediaTrack
    ↓
Request Next Fragment
    ↓
Get Fragment URL
    ↓
Check Cache
    ├─ Cache Hit → Use Cached Fragment
    └─ Cache Miss → Download Fragment
    ↓
Check Encryption
    ├─ Encrypted → Decrypt Fragment
    └─ Not Encrypted → Continue
    ↓
Process Fragment (Demux/Parse)
    ↓
Inject Fragment into GStreamer
    ↓
Decode & Render
```

### 5.2 ABR (Adaptive Bitrate) Flow

```
Downloader
    ↓
Report Download Speed
    ↓
Calculate Bandwidth
    ↓
Evaluate Buffer Health
    ├─ Buffer Low → Ramp Down Bitrate
    └─ Buffer High → Ramp Up Bitrate
    ↓
Select New Profile
    ↓
Notify Bitrate Change
    ↓
Send Bitrate Event
```

## 6. Threading Model

### 6.1 Thread Architecture

- **Main Thread:** Application API, event dispatch
- **Scheduler Thread:** Async task execution (AampScheduler)
- **Playlist Threads:** Per-track playlist refresh (HLS)
- **Download Worker Threads:** Parallel fragment downloads
- **GStreamer Threads:** Pipeline processing
- **DRM Threads:** License acquisition

### 6.2 Thread Communication

```
Main Thread
    ↓ (API Calls)
Core Thread
    ↓ (Schedule Task)
Scheduler Thread
    ↓ (Execute)
Worker Threads
    ├─→ Download from Network
    └─→ Inject into GStreamer Threads
        ↓ (Events)
    Core Thread
        ↓ (Events)
    Main Thread
```

## 7. Key Modules and Their Purposes

| Module | Purpose | Key Classes |
|--------|---------|-------------|
| Public API | Application-facing interface | PlayerInstanceAAMP |
| Core Engine | Player orchestration and state | PrivateInstanceAAMP |
| Stream Abstraction | Protocol abstraction layer | StreamAbstractionAAMP, HLS, MPD, Progressive |
| Fragment Collection | Fragment download and processing | FragmentCollector_HLS, FragmentCollector_MPD |
| GStreamer Integration | Media pipeline management | AAMPGstPlayer, StreamSink |
| DRM System | Content protection | AampDRMLicManager, DrmInterface |
| Network Layer | HTTP/HTTPS downloads | AampCurlDownloader, AampCurlStore |
| Configuration | Settings management | AampConfig |
| Event System | Event dispatch | AampEventManager, EventListener |

---

[← Back to Index](README.md) | [← Previous: Architecture](01_architecture_overview.md) | [Next: APIs and Classes →](03_apis_classes.md)

