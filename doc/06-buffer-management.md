# Buffer Management

## Overview

AAMP employs sophisticated buffer management strategies to ensure smooth, uninterrupted playback while optimizing memory usage for embedded systems. The buffer management system operates at multiple levels: fragment-level caching for downloaded media data, time-based buffering for playback continuity, and buffer health monitoring for adaptive quality decisions. These mechanisms work together to prevent rebuffering events, optimize download scheduling, and provide buffer status information to the ABR (Adaptive Bitrate) system for intelligent quality adaptation.

The buffer management architecture separates concerns between data storage (fragment cache), temporal tracking (time-based buffer manager), and health monitoring (buffer status reporting). This modular design allows independent optimization of each aspect while maintaining coordinated operation across video, audio, and subtitle tracks.

## Buffer Types

### 1. Fragment Cache

The fragment cache provides in-memory storage for downloaded media fragments before they are injected into the GStreamer pipeline:

- **Location**: `MediaTrack::mCachedFragment[]` - An array of `CachedFragment` pointers maintained per media track (video, audio, subtitle). Each track maintains its own independent cache.
- **Size**: Configurable via `downloadBuffer` parameter (default: 3 fragments per track). The cache size determines how many fragments are pre-downloaded ahead of the current playback position.
- **Purpose**: Pre-downloads fragments to create a playback buffer that absorbs network variability and prevents stalling. Fragments remain in cache until consumed by the GStreamer pipeline, allowing the downloader to work ahead of playback.

**Fragment Cache Structure**: Each `CachedFragment` contains the fragment data buffer (`AampGrowableBuffer`), metadata (PTS/DTS timestamps, duration, sequence number), encryption status, DRM key information, and download metrics. The cache operates as a circular buffer where fragments are added at the tail and consumed from the head, maintaining FIFO (First-In-First-Out) ordering.

**Cache Management**: The cache is managed by `MediaTrack` class, which tracks the number of cached fragments (`numberOfFragmentsCached`), maintains mutex-protected access for thread safety, and provides condition variable signaling when fragments become available for injection. When the cache is full, download threads wait until space becomes available, preventing unbounded memory growth.

### 2. Time-Based Buffer

The time-based buffering strategy tracks buffered content duration in seconds rather than fragment count:

- **Location**: `AampTimeBasedBufferManager` class (namespace `aamp`) - One instance per media track (video, audio, subtitle) maintained by `StreamAbstractionAAMP`.
- **Purpose**: Maintains accurate buffer duration tracking based on fragment playback time, enabling buffer-aware ABR decisions and download scheduling. Time-based measurement is more accurate than fragment count for adaptive streaming where fragment durations may vary.
- **Configuration**: `timeBasedBufferSeconds` parameter sets the target buffer duration. When set to a value > 0, time-based buffering is enabled; when <= 0, byte-based (fragment count) buffering is used instead.

**Time-Based Buffer Operations**: The `AampTimeBasedBufferManager` provides thread-safe methods:
- `PopulateBuffer(fragmentDuration)`: Adds fragment duration to the current buffer time when a fragment is downloaded and cached.
- `ConsumeBuffer(timeToConsume)`: Subtracts consumed playback time from buffer when fragments are injected into GStreamer.
- `IsFull()`: Checks if buffer duration exceeds `maxBufferTime * trickPlayMultiplier` (trick play uses a multiplier to maintain larger buffers for fast-forward/rewind operations).
- `ClearBuffer()`: Resets buffer to zero, used during seek operations or stream changes.

The buffer manager uses mutex protection (`std::mutex`) to ensure thread-safe access from download threads (populating buffer) and injection threads (consuming buffer). Buffer duration is tracked as a floating-point value in seconds, providing precise measurement for ABR threshold comparisons.

### 3. Byte-Based Buffer

Byte-based buffering is a legacy approach that tracks buffer levels based on fragment count rather than time duration:

- **Based on Fragment Count**: The system counts the number of cached fragments (`numberOfFragmentsCached`) and compares against `downloadBuffer` threshold. This approach is simpler but less accurate for adaptive streams with variable fragment durations.
- **Used When Time-Based is Disabled**: When `timeBasedBufferSeconds <= 0`, the system falls back to byte-based (fragment count) buffering. This maintains backward compatibility and provides a fallback for scenarios where time-based buffering may not be appropriate.
- **Limitations**: Fragment count doesn't account for varying fragment durations, which can lead to inaccurate buffer level estimates. For example, a buffer with 3 short fragments (total 6 seconds) vs 3 long fragments (total 30 seconds) would be treated identically, potentially causing incorrect ABR decisions.

Modern AAMP implementations prefer time-based buffering for its accuracy, but byte-based buffering remains available for compatibility and specific use cases where fragment durations are uniform and predictable.

## Buffer Health Monitoring

### Buffer Status

The buffer health monitoring system categorizes buffer levels into three states that trigger different ABR behaviors:

```cpp
enum BufferHealthStatus {
    BUFFER_STATUS_GREEN,   // Healthy (> warning threshold)
    BUFFER_STATUS_YELLOW,  // Warning (< warning, > min)
    BUFFER_STATUS_RED      // Critical (< min threshold)
};
```

**Buffer Status Definitions**:
- **GREEN Status**: Buffer level exceeds the warning threshold (typically 15-20 seconds). This indicates healthy buffering with adequate cushion for network variability. In GREEN status, the ABR system can ramp up quality when network bandwidth permits, maximizing viewing experience.
- **YELLOW Status**: Buffer level is between the minimum threshold and warning threshold (typically 10-15 seconds). This indicates moderate buffer levels that require monitoring but don't yet require emergency action. The ABR system maintains current quality or considers conservative ramp-down if network conditions degrade.
- **RED Status**: Buffer level falls below the minimum threshold (typically 10 seconds or less). This indicates critical buffer depletion that risks playback stalling. The ABR system immediately ramps down quality regardless of network bandwidth to prevent rebuffering events.

The buffer status is continuously evaluated and reported to the ABR system via `AampBufferControl` class, which provides buffer health metrics (`GetBufferHealthStatus()`) used by `StreamAbstractionAAMP::GetDesiredProfileOnBuffer()` for buffer-based ABR decisions.

### Monitoring Thread

Each media track (video, audio, subtitle) maintains a dedicated buffer monitoring thread that periodically checks buffer levels and updates status:

```cpp
void MediaTrack::MonitorBufferHealth()
{
    while (bufferMonitorRunning) {
        double bufferLevel = GetBufferedDuration();
        UpdateBufferStatus(bufferLevel);
        // Trigger actions based on status
    }
}
```

**Monitoring Implementation Details**:
- **Monitoring Interval**: The buffer monitor thread wakes up periodically (configurable via `bufferHealthMonitorInterval`, default: 1 second) to check current buffer levels. This interval balances responsiveness with CPU efficiency.
- **Buffer Level Calculation**: `GetBufferedDuration()` queries the `AampTimeBasedBufferManager` for the current buffered time in seconds. For byte-based buffering, it calculates duration from fragment count and average fragment duration.
- **Status Updates**: `UpdateBufferStatus()` compares the buffer level against configured thresholds (`mABRMinBuffer`, `mABRMaxBuffer`) and updates the `BufferHealthStatus` enum value. Status changes trigger ABR evaluation and may generate buffer health events.
- **Action Triggers**: When buffer status changes to RED, the monitoring thread signals the ABR system to immediately evaluate profile reduction. The thread uses condition variables to coordinate with download threads, potentially pausing downloads when buffer is full or accelerating downloads when buffer is low.

The monitoring thread runs independently for each track, allowing per-track buffer management. Video tracks typically have stricter buffer requirements than audio or subtitle tracks, so each track can have different threshold configurations while maintaining coordinated playback synchronization.

## Buffer Management Strategies

### Pre-Buffering

Pre-buffering occurs before playback starts to ensure smooth initial playback without stalling:

- **Initial Fragment Download**: When `Tune()` is called, the fragment collector immediately begins downloading fragments for all enabled tracks (video, audio, subtitle). The system downloads fragments sequentially or in parallel (based on `parallelPlaylistDownload` configuration) until the pre-buffer threshold is reached.
- **Minimum Buffer Wait**: The player waits until `initialBuffer` duration (configurable, default: 0 seconds) is buffered before transitioning from BUFFERING state to PLAYING state. If `gstBufferAndPlay` is enabled (default: true), GStreamer's internal buffering also contributes to pre-buffering, ensuring the pipeline has sufficient data before starting playback.
- **Playback Start**: Once minimum buffer is achieved, playback begins and the player transitions to PLAYING state. The pre-buffer level provides initial cushion that absorbs early network variability and ensures smooth playback startup.

The pre-buffering process is coordinated by `PrivateInstanceAAMP`, which monitors buffer levels via `AampTimeBasedBufferManager` and waits for sufficient buffering before calling `gst_element_set_state(pipeline, GST_STATE_PLAYING)`. During pre-buffering, the ABR system selects an initial profile (typically medium quality) to balance startup time with initial quality.

### Steady-State Buffering

During normal playback, the buffer management system maintains target buffer levels through continuous download scheduling:

- **Target Buffer Maintenance**: The system aims to maintain buffer duration between `mABRMinBuffer` (minimum threshold) and `mABRMaxBuffer` (maximum threshold). When buffer falls below target, download priority increases; when buffer exceeds target, downloads may be throttled to prevent excessive memory usage.
- **Download Ahead Scheduling**: Fragment collectors continuously download fragments ahead of the current playback position. The download position is calculated as `currentPlaybackPosition + targetBufferDuration`, ensuring fragments are available before they're needed for playback. This look-ahead distance adapts based on network conditions and buffer health.
- **Download Rate Adjustment**: Based on buffer levels and network conditions, the system adjusts download aggressiveness. When buffer is low (YELLOW or RED status), downloads are prioritized and may use parallel downloads (`dashParallelFragDownload`). When buffer is high (GREEN status), downloads may be throttled to prevent excessive buffering that increases memory usage and end-to-end latency.

The steady-state buffering is managed by `MediaTrack` class, which coordinates between download threads (populating buffer) and injection threads (consuming buffer). The `AampTimeBasedBufferManager` provides thread-safe buffer level tracking that guides download scheduling decisions.

### Underflow Prevention

When buffer levels become critically low, the system implements aggressive underflow prevention mechanisms:

- **Immediate Profile Ramp-Down**: When buffer status transitions to RED (below `mABRMinBuffer`), the ABR system immediately ramps down to a lower quality profile regardless of network bandwidth measurements. This reduces fragment sizes and download times, allowing the buffer to refill faster. The ramp-down uses `getRampedDownProfileIndex()` to select the next lower quality profile.
- **Download Prioritization**: Low buffer triggers increased download priority, potentially pausing non-critical operations (like playlist refreshes for far-ahead positions) and focusing download bandwidth on immediate fragment needs. The system may also increase parallel download concurrency (`fogMaxConcurrentDownloads`) to accelerate buffer refill.
- **Buffer Extension Attempts**: If possible, the system attempts to extend buffer by downloading longer fragments or increasing download aggressiveness. However, this is limited by network capacity and fragment availability. In extreme cases, the player may pause playback (`GST_STATE_PAUSED`) until sufficient buffer is restored, though this is a last resort that degrades user experience.

Underflow prevention is coordinated by `AampBufferControl` class, which monitors buffer health and triggers emergency actions. The buffer control system generates `AAMP_EVENT_BUFFERING_CHANGED` events when buffer status changes, allowing applications to display buffering indicators to users.

## Configuration

The buffer management system exposes several configuration parameters that control buffering behavior:

- **`downloadBuffer`**: Fragment cache size in number of fragments per track (default: 3). This determines how many fragments are cached in memory before being injected into GStreamer. Larger values provide more buffering but increase memory usage. The cache operates as a circular buffer, with new fragments replacing oldest fragments when full.

- **`timeBasedBufferSeconds`**: Target buffer duration in seconds for time-based buffering (default: varies by configuration). When set to a value > 0, enables time-based buffering mode where buffer levels are measured in seconds rather than fragment count. Setting to <= 0 disables time-based buffering and uses byte-based (fragment count) buffering instead.

- **`initialBuffer`**: Initial buffering duration in seconds before playback starts (default: 0 seconds). The player waits until this duration is buffered before transitioning from BUFFERING to PLAYING state. Higher values reduce startup time but increase initial delay. Setting to 0 relies on GStreamer's internal buffering (`gstBufferAndPlay`).

- **`minABRBufferRampdown`**: Minimum buffer threshold in seconds that triggers ABR ramp-down (default: 10 seconds). When buffered duration falls below this threshold, the ABR system immediately ramps down quality regardless of network bandwidth to prevent buffer underruns. This emergency mechanism ensures playback continuity.

- **`maxABRBufferRampup`**: Maximum buffer threshold in seconds that allows ABR ramp-up (default: 15 seconds). When buffered duration exceeds this threshold, the ABR system allows quality ramp-up if network bandwidth permits. This ensures adequate buffer cushion while maximizing quality when conditions allow.

- **`bufferHealthMonitorDelay`**: Delay in seconds before starting buffer health monitoring after tune/seek (default: varies). This prevents premature buffer health checks immediately after stream changes when buffer levels are still stabilizing.

- **`bufferHealthMonitorInterval`**: Interval in seconds between buffer health checks (default: 1 second). Shorter intervals provide more responsive buffer monitoring but increase CPU usage. Longer intervals reduce CPU overhead but may delay detection of buffer issues.

- **`preplayBuffercount`**: Number of segments to download before starting playback (default: 2 segments). This provides initial buffering for smooth playback startup, working in conjunction with `initialBuffer` time-based pre-buffering.

## Summary

The buffer management system ensures optimal playback experience through coordinated buffering strategies:

- **Smooth Playback Without Stalling**: Pre-buffering and steady-state buffer maintenance create adequate playback cushion that absorbs network variability. Underflow prevention mechanisms detect and respond to buffer depletion before playback stalls occur, maintaining continuous playback experience.

- **Efficient Memory Usage**: Time-based buffering provides accurate buffer measurement without requiring large fragment caches. Configurable buffer thresholds allow applications to balance memory usage with buffering requirements. The circular buffer design prevents unbounded memory growth while maintaining sufficient buffering.

- **Quick Response to Network Changes**: Buffer health monitoring provides real-time buffer status that guides ABR decisions. When network conditions degrade, buffer levels drop and trigger immediate quality reduction, preventing buffer depletion. When network improves, buffer levels rise and allow quality increases, maximizing viewing experience.

- **Optimal Quality Selection**: Buffer-aware ABR decisions ensure quality selection considers both network capacity and buffer health. The system maximizes quality when buffer is healthy and network permits, while prioritizing playback continuity when buffer is low. This hybrid approach balances quality and stability for optimal user experience.
