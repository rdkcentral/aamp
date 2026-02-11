# Core Classes & Interfaces

## Overview

This document provides detailed documentation of AAMP's major classes and interfaces, their responsibilities, attributes, methods, and relationships.

## PlayerInstanceAAMP

**File**: `main_aamp.h/cpp`

**Purpose**: Public API interface for applications

**Key Responsibilities**:
- Expose public methods (Tune, Seek, SetRate, etc.)
- Manage event listener registration
- Handle configuration management
- Integrate with JavaScript bindings

**Key Methods**:
- `Tune()`: Start playback
- `Seek()`: Seek to position
- `SetRate()`: Set playback rate
- `Stop()`: Stop playback
- `RegisterEvent()`: Register event listener
- `SetVideoBitrate()`: Set video bitrate
- `SetLanguage()`: Set audio language

**Relationships**:
- Contains: `PrivateInstanceAAMP` (internal implementation)
- Used by: Applications, JavaScript bindings

## PrivateInstanceAAMP

**File**: `priv_aamp.h/cpp`

**Purpose**: Internal player implementation

**Key Responsibilities**:
- Core playback logic
- GStreamer pipeline management
- State machine management
- Error handling and recovery

**Key Attributes**:
- `mGstPlayer`: GStreamer player instance
- `mStreamAbstraction`: Protocol handler
- `mEventManager`: Event dispatcher
- `mhAbrManager`: ABR manager
- `mConfig`: Configuration

**Key Methods**:
- `TuneInternal()`: Internal tune implementation
- `SeekInternal()`: Internal seek implementation
- `SendStreamTransfer()`: Inject fragment to GStreamer
- `ProcessID3Metadata()`: Process ID3 metadata

**Relationships**:
- Contains: `StreamAbstractionAAMP`, `AAMPGstPlayer`, `AampEventManager`
- Used by: `PlayerInstanceAAMP`

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
