# Beginner's Guide to AAMP

## Introduction

This guide is designed for engineers new to AAMP. It provides a gentle introduction to the codebase, key concepts, and a recommended learning path.

## What is AAMP?

AAMP (Advanced Adaptive Media Player) is a C++ video playback engine that:

- Plays adaptive streaming content (HLS, DASH)
- Handles DRM-protected content
- Optimizes playback based on network conditions
- Integrates with GStreamer for media processing

## Key Concepts

### 1. Adaptive Streaming

**What it is**: Video quality automatically adjusts based on network bandwidth.

**How AAMP does it**:
- Downloads manifest/playlist
- Identifies available quality levels (profiles)
- Monitors network bandwidth
- Switches between profiles as needed

**Key Files**: `abr/abr.cpp`, `StreamAbstractionAAMP.h`

### 2. Fragments

**What they are**: Small chunks of video/audio data (typically 2-10 seconds).

**How AAMP handles them**:
- Downloads fragments from CDN
- Caches fragments in memory
- Decrypts if DRM-protected
- Injects into GStreamer pipeline

**Key Files**: `CachedFragment.h`, `MediaTrack` class

### 3. Manifest/Playlist

**What it is**: A file describing available content and fragments.

**HLS**: `.m3u8` file with fragment URLs
**DASH**: `.mpd` file (XML) with segment information

**Key Files**: `fragmentcollector_hls.cpp`, `fragmentcollector_mpd.cpp`

### 4. DRM (Digital Rights Management)

**What it is**: Content protection system.

**How AAMP handles it**:
- Acquires license from server
- Decrypts fragments using keys
- Supports multiple DRM systems (Widevine, PlayReady, etc.)

**Key Files**: `drm/AampDRMLicManager.cpp`, `middleware/drm/`

## Architecture Overview

### High-Level Flow

```
Application
    ↓
PlayerInstanceAAMP (Public API)
    ↓
PrivateInstanceAAMP (Internal Logic)
    ↓
StreamAbstractionAAMP (Protocol Handler)
    ↓
FragmentCollector (HLS/DASH/Progressive)
    ↓
MediaTrack (Fragment Management)
    ↓
GStreamer (Decode & Render)
```

### Key Classes

1. **PlayerInstanceAAMP**: Public API for applications
2. **PrivateInstanceAAMP**: Internal player implementation
3. **StreamAbstractionAAMP**: Base class for protocol handlers
4. **MediaTrack**: Manages fragments for a track (video/audio/subtitle)
5. **ABRManager**: Makes bitrate decisions

## Learning Path

### Phase 1: Understanding the Basics (Week 1)

**Goal**: Understand what AAMP does and how to use it.

**Tasks**:
1. Read this guide
2. Review `README.md`
3. Look at `main_aamp.h` to see public API
4. Try the CLI tool (`test/aampcli/`)

**Key Files to Read**:
- `README.md`
- `main_aamp.h` (public API)
- `AAMP-UVE-API.md` (JavaScript API)

### Phase 2: Entry Points and Initialization (Week 2)

**Goal**: Understand how AAMP starts up.

**Tasks**:
1. Trace through `PlayerInstanceAAMP` constructor
2. Understand configuration loading
3. See how GStreamer pipeline is created

**Key Files to Read**:
- `main_aamp.cpp` (constructor)
- `priv_aamp.cpp` (private instance)
- `AampConfig.cpp` (configuration)

**Exercises**:
- Add a log statement in constructor
- Modify a default configuration value

### Phase 3: Tune Workflow (Week 3)

**Goal**: Understand how playback starts.

**Tasks**:
1. Trace `Tune()` call
2. Understand protocol detection
3. See manifest download and parsing
4. Understand track setup

**Key Files to Read**:
- `main_aamp.cpp` (`Tune()` method)
- `priv_aamp.cpp` (`TuneInternal()`)
- `fragmentcollector_hls.cpp` or `fragmentcollector_mpd.cpp`

**Exercises**:
- Add logging to track tune progress
- Modify initial bitrate selection

### Phase 4: Fragment Download and Injection (Week 4)

**Goal**: Understand fragment lifecycle.

**Tasks**:
1. Understand fragment download loop
2. See fragment caching
3. Understand fragment injection
4. Trace data flow to GStreamer

**Key Files to Read**:
- `streamabstraction.cpp` (`MediaTrack` class)
- `CachedFragment.h`
- `aampgstplayer.cpp` (injection)

**Exercises**:
- Add fragment download logging
- Modify fragment cache size

### Phase 5: ABR System (Week 5)

**Goal**: Understand adaptive bitrate logic.

**Tasks**:
1. Understand bandwidth estimation
2. See profile selection logic
3. Understand ramp-up/ramp-down

**Key Files to Read**:
- `abr/abr.cpp`
- `abr/NetworkBandwidthEstimator.cpp`
- `StreamAbstractionAAMP::CheckForProfileChange()`

**Exercises**:
- Modify ABR thresholds
- Add custom ABR logic

### Phase 6: Advanced Topics (Week 6+)

**Goal**: Deep dive into specific areas.

**Topics**:
- DRM system
- Event system
- TSB (Time Shift Buffer)
- Error handling and recovery
- Platform integration

## Common Patterns

Understanding common patterns helps navigate the codebase and understand design decisions:

### 1. Threading Model

AAMP uses a multi-threaded architecture to enable parallel operations and maintain responsive playback:

- **Main Thread**: Application API calls (`PlayerInstanceAAMP` methods) execute on the main/application thread. Main thread handles user-initiated operations (tune, seek, pause) and coordinates with internal threads via events and callbacks. Main thread operations are typically fast (parameter validation, state checks) to maintain responsiveness.

- **Download Threads**: One dedicated thread per media track (`MediaTrack::FragmentDownloader()`) continuously downloads fragments ahead of playback position. Download threads operate independently, enabling parallel fragment downloads for video, audio, and subtitle tracks. Threads coordinate via condition variables and mutexes to ensure thread-safe cache access and proper sequencing.

- **Injector Threads**: One dedicated thread per media track (`MediaTrack::RunInjectLoop()`) continuously injects cached fragments into GStreamer pipeline. Injection threads pace fragment injection based on playback position and pipeline consumption, maintaining optimal buffering. Threads wait on condition variables for fragment availability, preventing busy-waiting and ensuring efficient CPU usage.

- **Playlist Threads**: For live HLS streams, dedicated threads (`TrackState::PlaylistDownloader()`) periodically refresh playlists to obtain new fragment URLs. Playlist threads download playlists at configured intervals (`#EXT-X-TARGETDURATION`), parse updates, and add new fragments to fragment index. Threads coordinate with download threads to ensure new fragments are available for download.

- **GStreamer Threads**: GStreamer pipeline operates in its own threads (GStreamer's internal threading model), handling media processing, decoding, and rendering. GStreamer threads are managed by GStreamer framework and communicate with AAMP via callbacks and signals. Thread communication enables AAMP to respond to pipeline events (buffer underflow, EOS, errors) and coordinate playback state.

**Thread Coordination**: Threads coordinate via mutexes (for shared data access), condition variables (for event signaling), and abort flags (for graceful shutdown). Thread coordination ensures data consistency, prevents race conditions, and enables efficient resource sharing.

### 2. State Management

AAMP maintains state at multiple levels to track playback progress and coordinate operations:

- **Player State**: `AAMPPlayerState` enum tracks high-level player state (IDLE, INITIALIZING, INITIALIZED, PREPARING, PREPARED, BUFFERING, PLAYING, PAUSED, SEEKING, STOPPED). Player state transitions are managed by `PrivateInstanceAAMP` and trigger state change events. State management ensures operations occur in valid states (e.g., seek only when playing/paused) and coordinates state-dependent operations.

- **Track State**: Each `MediaTrack` maintains track-specific state (enabled/disabled, buffer status, download position, injection position). Track state enables independent track management (e.g., disabling audio track while keeping video) and provides state information for buffer monitoring and ABR decisions. Track state coordination ensures synchronized playback across tracks.

- **Pipeline State**: GStreamer pipeline state (`GST_STATE_NULL`, `GST_STATE_READY`, `GST_STATE_PAUSED`, `GST_STATE_PLAYING`) tracks pipeline lifecycle and data flow. Pipeline state transitions are coordinated with player state to ensure proper pipeline operation. Pipeline state management handles state transition errors and ensures pipeline readiness before data injection.

**State Synchronization**: State at different levels must remain synchronized to ensure correct behavior. For example, player state PLAYING requires pipeline state GST_STATE_PLAYING and enabled tracks. State synchronization is managed through coordinated state transitions and state validation checks.

### 3. Error Handling

AAMP implements multi-level error handling to ensure robust playback:

- **Network Errors**: HTTP errors (4xx, 5xx), timeouts, and connection failures trigger retry logic with exponential backoff. Repeated network failures trigger ABR ramp-down to reduce fragment sizes and improve success rates. Network error handling includes error classification (recoverable vs. non-recoverable) and appropriate recovery strategies (retry, ramp-down, retune).

- **DRM Errors**: License acquisition failures, decryption errors, and DRM system errors trigger license retry, session recreation, or error reporting. DRM error handling includes retry logic with configurable retry counts and wait times. Critical DRM errors (authentication failures, key extraction failures) trigger error events for application handling.

- **Pipeline Errors**: GStreamer errors, decode failures, and rendering errors trigger pipeline recovery (pipeline reconstruction, decoder fallback) or error reporting. Pipeline error handling includes error detection via GStreamer bus messages and appropriate recovery actions. Pipeline errors may trigger internal retune if recovery fails.

**Error Recovery Hierarchy**: Error handling follows a hierarchy:
1. **Immediate Retry**: Transient errors (timeouts, 5xx errors) trigger immediate retry
2. **Adaptive Recovery**: Repeated errors trigger adaptive recovery (ABR ramp-down, profile switch)
3. **Internal Retune**: Persistent errors trigger internal retune to restart playback
4. **Error Reporting**: Unrecoverable errors trigger error events for application handling

This hierarchy ensures maximum recovery attempts while preventing infinite retry loops and providing graceful degradation when recovery fails.

## Debugging Tips

### 1. Enable Logging

```cpp
// In code
AAMPLOG_INFO("Debug message: %s", value);

// Via configuration
info=true
debug=true
trace=true
```

### 2. Use Profiler

```cpp
AampProfiler profiler;
profiler.ProfileBegin("operation");
// ... code ...
profiler.ProfileEnd("operation");
```

### 3. Check Buffer Levels

Monitor buffer health:

```cpp
double bufferLevel = track->GetBufferedDuration();
BufferHealthStatus status = track->GetBufferStatus();
```

### 4. Use CLI Tool

The CLI tool (`aamp-cli`) is great for testing:

```bash
./aamp-cli
> http://example.com/manifest.mpd
> status
> seek 30
```

## Common Pitfalls

1. **Forgetting to Start Threads**: Fragment download/injection threads must be started
2. **Not Handling Abort Flags**: Always check `abort` flags in loops
3. **Memory Leaks**: Use smart pointers, don't forget to cleanup
4. **Thread Safety**: Be careful with shared data, use mutexes
5. **Configuration Priority**: Understand which config takes precedence

## Getting Help

1. **Documentation**: Read the other docs in `doc/` folder
2. **Code Comments**: Many functions have detailed comments
3. **Tests**: Look at `test/utests/` for usage examples
4. **Logs**: Enable verbose logging to understand flow

## Next Steps

After completing the learning path:

1. **Pick an Area**: Choose a subsystem to specialize in
2. **Read Deeply**: Study all files in that area
3. **Make Changes**: Start with small modifications
4. **Write Tests**: Add unit tests for your changes
5. **Contribute**: Submit improvements back

## Summary

AAMP is a complex system, but understanding it step-by-step makes it manageable:

1. **Start Simple**: Understand the public API first
2. **Trace Execution**: Follow a tune operation end-to-end
3. **Understand Patterns**: Learn common patterns (threading, state, errors)
4. **Practice**: Make small changes and test them
5. **Go Deep**: Specialize in areas that interest you

Remember: Every expert was once a beginner. Take your time, ask questions, and don't be afraid to explore the code!
