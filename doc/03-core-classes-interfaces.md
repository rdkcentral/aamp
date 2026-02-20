# Core Classes & Interfaces

## Overview

This document provides detailed documentation of AAMP's major classes and interfaces, their responsibilities, attributes, methods, and relationships.

## PlayerInstanceAAMP

**File**: `main_aamp.h/cpp`

**Purpose**: Public API interface for applications providing the main AAMP player functionality

**Key Responsibilities**:
- Expose public methods for media playback control
- Manage event listener registration and lifecycle
- Handle configuration management through `AampConfig`
- Integrate with JavaScript bindings via WebKit injection
- Provide thread-safe access to internal player state

**Core Attributes**:
```cpp
class PrivateInstanceAAMP *aamp;                    // Internal implementation
std::shared_ptr<PrivateInstanceAAMP> sp_aamp;      // Shared pointer for resource management
AampConfig mConfig;                                 // Configuration management
```

**Key Public Methods**:

### Playback Control
- `Tune(url, contentType, ...)`: Start playback with comprehensive options
  - Supports autoplay, trace UUID, audio decoder sync control
  - Handles content type detection and first/final attempt flags
- `Stop(sendStateChangeEvent, forceCleanup)`: Stop playback with cleanup options
- `Seek(secondsRelativeToTuneTime, keepPaused)`: Frame-accurate seeking
- `SeekToLive(keepPaused)`: Instant live edge seeking
- `SetRate(rate, overshootcorrection)`: Playback rate control with correction
- `SetRateAndSeek(rate, position)`: Atomic rate and seek operation

### Audio/Video Configuration
- `SetLanguage(language)`: Audio language selection
- `SetVideoRectangle(x, y, w, h)`: Video viewport control
- `SetVideoZoom(zoom)`: Video zoom mode management
- `SetVideoMute(muted)`: Video muting control
- `SetSubtitleMute(muted)`: Subtitle enable/disable
- `SetAudioVolume(volume)`: Audio volume (0-100 range)

### Event Management
- `RegisterEvent(type, listener)`: Type-specific event registration
- `RegisterEvents(eventListener)`: All-events registration
- `AddEventListener(eventType, eventListener)`: Modern event API
- `RemoveEventListener(eventType, eventListener)`: Event cleanup

### DRM & Security
- `AddCustomHTTPHeader(name, value, isLicenseHeader)`: Custom headers
- `SetLicenseServerURL(url, drmType)`: DRM license server configuration
- `SetPreferredDRM(drmType)`: DRM system preference
- `GetPreferredDRM()`: Current DRM preference query

### Advanced Features
- `InsertAd(url, positionSeconds)`: Server-side ad insertion
- `SetSubscribedTags(subscribedTags)`: Metadata tag subscription
- `SubscribeResponseHeaders(responseHeaders)`: HTTP response monitoring
- `AddPageHeaders(customHttpHeaders)`: Page-level HTTP headers

**Constructor Overloads**:
```cpp
PlayerInstanceAAMP(StreamSink* streamSink = NULL,
                   std::function<void(const unsigned char*, int, int, int)> exportFrames = nullptr,
                   bool powerEvt = false);
```

**Thread Safety**: All public methods are thread-safe with internal mutex protection

**Relationships**:
- **Contains**: `PrivateInstanceAAMP` (pimpl idiom for implementation hiding)
- **Used by**: Applications, JavaScript UVE bindings, CLI tools
- **Manages**: `AampConfig` for configuration state

## PrivateInstanceAAMP

**File**: `priv_aamp.h/cpp`

**Purpose**: Internal player implementation providing core playbook logic and state management

**Inheritance**:
```cpp
class PrivateInstanceAAMP : public DrmCallbacks, public std::enable_shared_from_this<PrivateInstanceAAMP>
```

**Key Responsibilities**:
- Core playback logic and state machine management
- GStreamer pipeline orchestration and media injection
- Protocol-specific stream abstraction management
- DRM callbacks and security handling
- Internal event management and propagation
- Bandwidth monitoring and adaptive bitrate coordination
- Buffer management and underflow/overflow handling

**Core Attributes**:
```cpp
// Core Player Components
AampGstPlayer *mpStreamAbstraction;              // GStreamer pipeline manager
StreamAbstractionAAMP *mpStreamAbstraction;      // Protocol handler (HLS/DASH/Progressive)
AampEventManager *mEventManager;                 // Event dispatcher
AampConfig *mConfig;                             // Configuration management
ABRManager *mhAbrManager;                        // Adaptive bitrate manager

// State Management
std::atomic<PrivAAMPState> mState;               // Current player state
pthread_mutex_t mLock;                           // Thread synchronization
TuneType mTuneType;                             // Current tune type context
bool mAutoPlay;                                 // Autoplay enabled flag

// Media Tracks
MediaTrack *mpStreamAbstraction->track[AAMP_TRACK_COUNT]; // Video/Audio/Subtitle tracks

// Network & Download
CurlInstance mCurl;                             // HTTP/HTTPS client
std::string mManifestUrl;                       // Current manifest URL
AampNetworkMode mNetworkMode;                   // Network optimization mode

// Buffer Management
double mDownloadProgress;                       // Download progress tracking
long long mCurrentBandwidth;                    // Current network bandwidth
bool mBufferingEnabled;                         // Buffer control flag

// DRM & Security
#if defined(USE_SECCLIENT) || defined(USE_SECMANAGER)
AampDrmSession *mDrmSession;                    // DRM session management
#endif

// TSB (Time Shift Buffer)
std::shared_ptr<TSB::Store> mTSBStore;          // Local TSB storage
```

**Key Internal Methods**:

### Core Playback Control
- `TuneHelper(tuneType, seekWhilePaused)`: Internal tune coordination
- `TeardownStream(newTune, disableDownloads)`: Stream cleanup and resource release
- `PausePipeline(pause, forceStopGstreamerPreBuffering)`: GStreamer pause control
- `ReloadTSB()`: TSB session reload functionality

### Media & Fragment Management
- `SendStreamTransfer(mediaType, buffer, size, ...)`: Fragment injection to GStreamer
- `EnableMediaDownloads(type)`: Enable specific media type downloads
- `DisableMediaDownloads(type)`: Disable specific media type downloads
- `ProcessID3Metadata(buffer, size, ...)`: ID3 tag processing
- `RecalculatePTS(mediaType, ptr, len)`: PTS calculation for media synchronization

### State & Event Management
- `SetState(state)`: Thread-safe state transitions
- `SendAnomalyEvent(type, message)`: Anomaly event reporting
- `SendEvent(event, eventMode)`: Event dispatch to listeners
- `NotifyFirstBufferProcessed()`: First buffer notification
- `SaveNewTimedMetadata(...)`: Timed metadata processing

### Network & ABR
- `UpdateVideoEndMetrics(...)`: Video performance metrics
- `UpdateAudioEndMetrics(...)`: Audio performance metrics
- `GetPreferredDRM()`: DRM system preference query
- `NotifyBitRateUpdate(...)`: Bitrate change notifications

### Progressive Enhancement Methods
- `SetTuneEventConfig(tuneEventType)`: Tune event configuration
- `UpdatePreferredAudioList()`: Audio preference management
- `CheckPreferredTextLanguages(...)`: Text track language selection
- `GetProfilerBucketForMedia(mediaType, isInit)`: Performance profiling support

**Thread Safety**: All public methods use internal mutexes for thread-safe operation

**State Machine**: Manages transitions between IDLE, INITIALIZING, INITIALIZED, PREPARING, PREPARED, BUFFERING, PLAYING, PAUSED, SEEKING states

**Relationships**:
- **Contains**: `StreamAbstractionAAMP`, `AAMPGstPlayer`, `AampEventManager`, `ABRManager`
- **Used by**: `PlayerInstanceAAMP` (pimpl pattern)
- **Implements**: `DrmCallbacks` interface for DRM event handling
- **Manages**: Media track lifecycle, GStreamer pipeline, download threads

## StreamAbstractionAAMP

**File**: `StreamAbstractionAAMP.h`, `streamabstraction.cpp`

**Purpose**: Base class for protocol-specific implementations

**Key Responsibilities**:
- Common fragment caching and injection logic
- MediaTrack management
- ABR coordination
- Discontinuity handling

**Key Attributes**:
- `videoTrack`: Video media track
- `audioTrack`: Audio media track
- `subtitleTrack`: Subtitle media track
- `currentProfileIndex`: Current video profile
- `trickplayMode`: Trick play mode flag

**Key Methods**:
- `Init()`: Initialize stream (virtual)
- `Start()`: Start streaming (virtual)
- `Stop()`: Stop streaming (virtual)
- `GetMediaTrack()`: Get track by type
- `CheckForProfileChange()`: ABR profile change logic

**Subclasses**:
- `StreamAbstractionAAMP_HLS`: HLS implementation
- `StreamAbstractionAAMP_MPD`: DASH implementation
- `StreamAbstractionAAMP_Progressive`: Progressive MP4

## MediaTrack

**File**: `StreamAbstractionAAMP.h` (defined in base class)

**Purpose**: Manages fragments for a media track

**Key Responsibilities**:
- Fragment download coordination
- Fragment caching
- Fragment injection
- Buffer management
- Playlist refresh (for live streams)

**Key Attributes**:
- `mCachedFragment[]`: Fragment cache array
- `fragmentIdxToFetch`: Download index
- `fragmentIdxToInject`: Injection index
- `totalInjectedDuration`: Total injected duration
- `bandwidthBitsPerSecond`: Current bandwidth

**Key Methods**:
- `StartInjectLoop()`: Start injection thread
- `InjectFragment()`: Inject fragment to pipeline
- `DownloadFragment()`: Download fragment
- `GetBufferedDuration()`: Get buffer level
- `CheckForDiscontinuity()`: Detect discontinuities

**Lifecycle**:
- Created during track setup
- Threads started during tune
- Threads stopped during stop/seek
- Destroyed during cleanup

## ABRManager

**File**: `abr/abr.h/cpp`

**Purpose**: Adaptive bitrate decision making

**Key Responsibilities**:
- Bandwidth estimation
- Profile selection
- Ramp-up/ramp-down logic
- Buffer-based decisions

**Key Methods**:
- `getInitialProfileIndex()`: Get starting profile
- `getProfileIndexByBitrateRampUpOrDown()`: ABR decision
- `getRampedDownProfileIndex()`: Ramp down one step
- `getRampedUpProfileIndex()`: Ramp up one step
- `CheckProfileChange()`: Check if profile change needed

**Key Attributes**:
- `mProfiles`: List of available profiles
- `mSortedBWProfileList`: Sorted bandwidth map
- `mDefaultInitBitrate`: Default initial bitrate

## AampEventManager

**File**: `AampEventManager.h/cpp`

**Purpose**: Event dispatch and listener management

**Key Responsibilities**:
- Event queuing
- Listener registration
- Async event dispatch
- Event statistics

**Key Methods**:
- `SendEvent()`: Send event to listeners
- `AddEventListener()`: Register listener
- `RemoveEventListener()`: Unregister listener
- `FlushPendingEvents()`: Flush queued events

**Key Attributes**:
- `mEventListeners[]`: Array of listener lists
- `mEventWorkerDataQue`: Event queue
- `mPendingAsyncEvents`: Pending async events

## AAMPGstPlayer

**File**: `aampgstplayer.h/cpp`

**Purpose**: GStreamer pipeline management

**Key Responsibilities**:
- Pipeline creation and configuration
- Fragment injection into GStreamer
- Stream sink management
- Pipeline state management

**Key Methods**:
- `Configure()`: Configure pipeline
- `SendTransfer()`: Inject fragment
- `Play()`: Start playback
- `Pause()`: Pause playback
- `Stop()`: Stop playback

## AampCurlDownloader

**File**: `downloader/AampCurlDownloader.h/cpp`

**Purpose**: HTTP/HTTPS downloads

**Key Responsibilities**:
- HTTP/HTTPS downloads using libcurl
- Connection reuse
- Download metrics
- Retry logic
- Timeout handling

**Key Methods**:
- `GetFile()`: Download file
- `GetFileAsync()`: Async download
- `SetDownloadConfig()`: Configure download

## CachedFragment

**File**: `CachedFragment.h/cpp`

**Purpose**: Fragment cache structure

**Key Attributes**:
- `fragment`: Fragment data buffer
- `position`: Fragment position (PTS)
- `duration`: Fragment duration
- `initFragment`: Is initialization fragment
- `discontinuity`: Has discontinuity
- `profileIndex`: Profile index used

## Relationships Summary

```
PlayerInstanceAAMP
    └──> PrivateInstanceAAMP
            ├──> StreamAbstractionAAMP
            │       ├──> MediaTrack (video)
            │       ├──> MediaTrack (audio)
            │       └──> MediaTrack (subtitle)
            ├──> AAMPGstPlayer
            ├──> AampEventManager
            ├──> ABRManager
            └──> AampCurlDownloader
```

## Interface Patterns

### 1. Virtual Base Classes

Many classes use virtual methods for extensibility:
- `StreamAbstractionAAMP`: Base for protocol handlers
- `EventListener`: Base for event listeners
- `StreamSink`: Base for stream sinks

### 2. Smart Pointers

Extensive use of smart pointers for memory management:
- `std::shared_ptr<PrivateInstanceAAMP>`
- `std::unique_ptr<MediaProcessor>`
- `std::shared_ptr<CachedFragment>`

### 3. Factory Patterns

Some classes use factory patterns:
- `DrmSessionFactory`: Creates DRM sessions
- `StreamAbstractionAAMP`: Created based on protocol

## Summary

AAMP's class hierarchy follows clear separation of concerns:

1. **Public API Layer**: `PlayerInstanceAAMP`
2. **Internal Logic Layer**: `PrivateInstanceAAMP`
3. **Protocol Layer**: `StreamAbstractionAAMP` subclasses
4. **Track Layer**: `MediaTrack` instances
5. **Support Layer**: ABR, Events, DRM, Downloader

This architecture enables:
- Clear responsibilities
- Easy extension
- Testability
- Maintainability
