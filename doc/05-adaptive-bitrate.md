# Adaptive Bitrate (ABR) System

## Overview

The ABR (Adaptive Bitrate) system is a sophisticated quality adaptation mechanism that intelligently selects video quality profiles based on real-time network conditions, buffer levels, and playback requirements. The system continuously monitors network throughput, fragment download performance, and buffer health to make optimal bitrate decisions that balance video quality with playback continuity. This ensures users receive the highest possible quality while preventing rebuffering events and maintaining smooth playback experience.

The ABR system operates through two main components: the **ABRManager**, which makes profile selection decisions, and the **NetworkBandwidthEstimator**, which provides accurate bandwidth measurements. Together, they implement a hybrid approach combining bandwidth-based adaptation with buffer-aware decision making.

## Architecture

**Files**: `abr/abr.h/cpp`, `abr/NetworkBandwidthEstimator.h/cpp`

**Key Class**: `ABRManager`

The ABR architecture follows a modular design where profile management, bandwidth estimation, and decision logic are separated into distinct components. The `ABRManager` class maintains the profile list and implements the core decision algorithms, while `NetworkBandwidthEstimator` provides robust throughput measurements using advanced statistical techniques including Exponentially Weighted Moving Averages (EWMA) and harmonic mean calculations.

## Components

### ABRManager

The `ABRManager` class serves as the main ABR decision engine that orchestrates quality adaptation. It maintains a comprehensive profile list containing bitrate, resolution, codec information, and period-specific metadata for DASH streams. The manager tracks both regular video profiles and I-frame tracks used for trick play operations.

**Key Responsibilities**:
- **Profile Management**: Maintains an ordered list of available quality profiles sorted by bandwidth, with support for period-specific profiles in DASH streams. Each profile contains bitrate (bits per second), resolution (width/height), codec information, and whether it's an I-frame track.
- **Network Bandwidth Estimation**: Receives bandwidth measurements from `NetworkBandwidthEstimator` and uses them to determine appropriate profile selections. The manager considers both current bandwidth and historical patterns to avoid oscillation.
- **Profile Selection Decisions**: Implements multiple selection strategies including initial profile selection (based on default bitrate or medium profile), steady-state adaptation (bandwidth and buffer-based), ramp-down (on download failures or low buffer), and ramp-up (on good network conditions and high buffer levels).
- **Ramp-Up and Ramp-Down Logic**: Provides controlled quality transitions with configurable consistency checks (`abrNwConsistency`) to prevent rapid profile switching. The ramp-up process gradually increases quality when conditions improve, while ramp-down quickly reduces quality to prevent buffer underruns.

The manager uses thread-safe operations with mutex protection (`mProfileLock`) to ensure profile list modifications are atomic and safe across multiple threads accessing ABR decisions concurrently.

### NetworkBandwidthEstimator

The `NetworkBandwidthEstimator` class calculates network bandwidth using sophisticated statistical methods applied to download metrics. It processes per-request download samples containing fragment size, total download time, and time-to-first-byte (TTFB) measurements.

**Key Features**:
- **Dual EWMA Filtering**: Maintains two Exponentially Weighted Moving Average filters - a fast EWMA (alpha=0.5) that reacts quickly to network changes, and a slow EWMA (alpha=0.2) that provides a stable baseline. This dual-filter approach balances responsiveness with stability.
- **Harmonic Mean Calculation**: Computes a conservative harmonic mean throughput over a sliding window of the last 8 samples. Harmonic mean is particularly effective for bandwidth estimation as it naturally handles outliers and provides a conservative estimate that prevents over-estimation.
- **TTFB Estimation**: Calculates robust Time-To-First-Byte estimates using median computation over recent samples. TTFB represents network overhead and latency, separate from payload throughput.
- **Download Prediction**: Provides `GetPredictedDownloadTimeSeconds()` method that predicts completion time for new segments based on current throughput estimates and TTFB overhead.

The estimator processes `CurlInfo` structures containing CURLINFO_SIZE_DOWNLOAD, CURLINFO_TOTAL_TIME, and CURLINFO_STARTTRANSFER_TIME metrics. Each sample calculates payload download time (total_time - TTFB) and payload throughput (size / payload_time). The final throughput estimate blends the fast EWMA, slow EWMA, and harmonic mean to provide a robust bandwidth measurement that guides ABR decisions.

## Profile Management

### Profile Structure

The `ProfileInfo` structure encapsulates all information needed to identify and select a quality profile:

```cpp
struct ProfileInfo {
    bool isIframeTrack;                    // True if this is an I-frame track for trick play
    BitsPerSecond bandwidthBitsPerSecond;  // Bitrate in bits per second
    int width;                             // Video width in pixels (optional)
    int height;                            // Video height in pixels (optional)
    std::string periodId;                  // DASH period identifier (empty for HLS)
    int userData;                          // Profile index or period index (optional metadata)
};
```

The profile list is maintained in `mProfiles` vector and sorted by bandwidth in `mSortedBWProfileList` map, which maps bandwidth values to profile indices. This sorted structure enables efficient profile lookups based on available network bandwidth. For DASH streams with multiple periods, profiles are organized by `periodId` to support period-specific profile selection.

### Profile Selection Strategies

The ABR system implements multiple profile selection strategies that adapt to different playback phases:

1. **Initial Profile Selection**: When playback starts, `getInitialProfileIndex()` selects the starting quality. If `chooseMediumProfile` is true, it selects the median profile from the sorted list (approximately the middle quality level). Otherwise, it selects the highest profile whose bitrate is less than or equal to `mDefaultInitBitrate`. This ensures playback starts at a conservative quality that matches the configured default bitrate, preventing initial buffering delays while avoiding over-estimation of network capacity.

2. **Steady State Adaptation**: During normal playback, profile selection combines bandwidth-based and buffer-based decisions. The system continuously monitors network throughput via `NetworkBandwidthEstimator` and compares it against current profile bandwidth. If network bandwidth consistently exceeds current profile bandwidth by a threshold (with `nwConsistencyCnt` checks to prevent oscillation), the system ramps up. Similarly, if bandwidth drops below current profile requirements, it ramps down. Buffer levels (`mABRMinBuffer`, `mABRMaxBuffer`) provide additional constraints - low buffer triggers ramp-down regardless of bandwidth, while high buffer allows ramp-up when bandwidth permits.

3. **Ramp Down Logic**: The `getRampedDownProfileIndex()` method selects the next lower quality profile. It searches the sorted profile list for the highest profile index with bandwidth less than the current profile's bandwidth. This ensures a single-step quality reduction that prevents sudden quality drops while quickly responding to network degradation or buffer depletion. Ramp-down is triggered by download failures, low buffer levels, or sustained low bandwidth measurements.

4. **Ramp Up Logic**: The `getRampedUpProfileIndex()` method selects the next higher quality profile. It finds the lowest profile index with bandwidth greater than the current profile's bandwidth. Ramp-up requires consistent good network conditions (multiple successful bandwidth measurements exceeding current profile requirements) and adequate buffer levels. The consistency check (`abrNwConsistency`, default 2) prevents rapid oscillation between profiles when network conditions fluctuate near threshold boundaries.

## ABR Algorithms

### Bandwidth-Based ABR

The `getProfileIndexByBitrateRampUpOrDown()` method implements the core bandwidth-based adaptation algorithm:

```cpp
int ABRManager::getProfileIndexByBitrateRampUpOrDown(
    int currentProfileIndex,
    BitsPerSecond currentBandwidth,
    BitsPerSecond networkBandwidth,
    int nwConsistencyCnt)
{
    // Compare network bandwidth to current profile bandwidth
    // Ramp up if network > current * threshold
    // Ramp down if network < current * threshold
    // Use consistency count to avoid oscillation
}
```

**Algorithm Details**:
- **Bandwidth Comparison**: The method compares `networkBandwidth` (measured by `NetworkBandwidthEstimator`) against `currentBandwidth` (the bitrate of the currently selected profile). The comparison uses configurable thresholds to determine when bandwidth is sufficient for higher quality or insufficient for current quality.
- **Ramp-Up Condition**: When `networkBandwidth` exceeds `currentBandwidth * rampUpThreshold` (typically 1.2x or 1.3x), the system considers ramping up. However, it requires `nwConsistencyCnt` consecutive measurements meeting this condition before actually switching profiles. This prevents rapid oscillation when network conditions fluctuate near threshold boundaries.
- **Ramp-Down Condition**: When `networkBandwidth` falls below `currentBandwidth * rampDownThreshold` (typically 0.8x or 0.9x), the system immediately considers ramping down. Ramp-down is more aggressive than ramp-up to prevent buffer underruns, but still uses consistency checks to avoid unnecessary quality drops from temporary network hiccups.
- **Best Match Selection**: When ramping up or down, the method calls `getBestMatchedProfileIndexByBandWidth()` to find the profile whose bandwidth most closely matches the available network bandwidth. This ensures optimal quality selection rather than always selecting adjacent profiles.

### Buffer-Based ABR

The buffer-based ABR algorithm provides a safety mechanism that overrides bandwidth-based decisions when buffer levels become critical:

```cpp
void StreamAbstractionAAMP::GetDesiredProfileOnBuffer(
    int currProfileIndex, int &newProfileIndex)
{
    double bufferValue = GetBufferedDuration();

    if (bufferValue < mABRMinBuffer) {
        // Ramp down
        newProfileIndex = GetRampedDownProfileIndex();
    } else if (bufferValue > mABRMaxBuffer) {
        // Ramp up
        newProfileIndex = GetRampedUpProfileIndex();
    }
}
```

**Buffer Thresholds**:
- **Critical Buffer (`mABRMinBuffer`)**: When buffered duration falls below this threshold (default 10 seconds), the system immediately ramps down regardless of network bandwidth measurements. This emergency ramp-down prevents buffer underruns that would cause playback stalling. The threshold is configurable via `minABRBufferRampdown` configuration parameter.
- **High Buffer (`mABRMaxBuffer`)**: When buffered duration exceeds this threshold (default 15 seconds), the system allows ramp-up if network bandwidth permits. This ensures the player maintains a healthy buffer cushion while maximizing quality when conditions allow. The threshold is configurable via `maxABRBufferRampup` configuration parameter.
- **Buffer Monitoring**: Buffer levels are continuously monitored by `AampTimeBasedBufferManager`, which tracks buffered duration in seconds for each media track (video, audio, subtitle). The buffer manager provides thread-safe access to current buffer levels and triggers ABR decisions when thresholds are crossed.

The combination of bandwidth-based and buffer-based algorithms creates a hybrid ABR system that maximizes quality while ensuring playback continuity. Bandwidth-based decisions optimize for network conditions, while buffer-based decisions provide safety nets that prevent rebuffering events.

## Configuration

The ABR system exposes several configuration parameters that control adaptation behavior:

- **`abrCacheLife`**: Bandwidth sample lifetime in milliseconds (default: 5000ms). This determines how long bandwidth measurements remain valid in the ABR cache. Older samples beyond this lifetime are discarded, ensuring the system adapts to recent network conditions rather than historical data.

- **`abrCacheLength`**: Number of bandwidth samples to consider in ABR calculations (default: 3 segments). This controls the window size for bandwidth averaging and consistency checks. Larger values provide more stable estimates but slower adaptation, while smaller values react faster but may be more sensitive to outliers.

- **`abrNwConsistency`**: Number of consistency checks required before switching profiles (default: 2). This prevents oscillation by requiring multiple consecutive measurements meeting ramp-up or ramp-down conditions before actually changing profiles. Higher values reduce oscillation but slow adaptation response.

- **`abrMinBuffer`**: Minimum buffer threshold for ramp-down in seconds (default: 10s). When buffered duration falls below this value, the system immediately ramps down regardless of network bandwidth. This emergency mechanism prevents buffer underruns.

- **`abrMaxBuffer`**: Maximum buffer threshold for ramp-up in seconds (default: 15s). When buffered duration exceeds this value, the system allows ramp-up if network bandwidth permits. This ensures adequate buffer cushion while maximizing quality.

- **`abrSkipDuration`**: Minimum duration of fragment to be downloaded before triggering ABR decisions (default: 6s). This prevents premature ABR decisions based on incomplete fragment downloads, ensuring measurements reflect actual network performance.

- **`abrCacheOutlier`**: Outlier difference threshold in bytes (default: 5MB) that will be ignored from network bandwidth calculations. This filters out anomalous download measurements that could skew bandwidth estimates.

## Low-Latency ABR

For low-latency DASH streams, the ABR system implements specialized optimizations:

- **Faster Profile Switching**: Low-latency mode reduces consistency check requirements (`abrNwConsistency`) and uses shorter bandwidth cache lifetimes to enable quicker adaptation to network changes. This is critical for low-latency scenarios where delayed adaptation would increase end-to-end latency.

- **Chunk-Based Bandwidth Estimation**: Instead of waiting for complete fragment downloads, the system uses `DownloadContext` class to monitor download progress via CURLOPT_XFERINFOFUNCTION callbacks. This provides real-time throughput estimates during fragment downloads, enabling faster ABR decisions.

- **Reduced Buffer Thresholds**: Low-latency mode uses smaller buffer thresholds (`minABRBufferRampdown`, `maxABRBufferRampup`) to maintain lower buffer levels while still preventing underruns. This reduces end-to-end latency by minimizing buffering delay.

- **Speed Store Management**: The system maintains a `MAX_LOW_LATENCY_DASH_ABR_SPEEDSTORE_SIZE` (10 samples) for low-latency bandwidth tracking, providing a balance between responsiveness and stability for low-latency scenarios.

These optimizations are controlled by the `enableLowLatencyDash` and `disableLowLatencyABR` configuration flags, allowing applications to enable low-latency mode when appropriate.

## Summary

The ABR system provides intelligent quality selection that balances multiple competing objectives:

- **Maximizes Quality**: Within network constraints, the system selects the highest quality profile that can be reliably delivered, ensuring users receive the best possible viewing experience given their network conditions.

- **Minimizes Rebuffering**: Through buffer-based safety mechanisms and aggressive ramp-down on low buffer, the system prevents playback stalls and rebuffering events that degrade user experience.

- **Smooth Playback Experience**: Consistency checks and gradual profile transitions prevent rapid quality oscillations that could be visually jarring. The hybrid bandwidth and buffer-based approach ensures stable, predictable quality changes.

- **Quick Adaptation**: The dual EWMA filters and harmonic mean calculations in `NetworkBandwidthEstimator` provide responsive adaptation to genuine network changes while filtering out temporary fluctuations. Low-latency optimizations further reduce adaptation delay for time-sensitive scenarios.
