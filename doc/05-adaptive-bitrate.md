# Adaptive Bitrate (ABR) System

## Overview

The ABR (Adaptive Bitrate) system intelligently selects video quality based on network conditions, buffer levels, and playback requirements.

## Architecture

**Files**: `abr/abr.h/cpp`, `abr/NetworkBandwidthEstimator.h/cpp`

**Key Class**: `ABRManager`

## Components

### ABRManager

Main ABR decision engine that:
- Maintains profile list (bitrate, resolution, codec)
- Estimates network bandwidth
- Makes profile selection decisions
- Handles ramp-up and ramp-down

### NetworkBandwidthEstimator

Calculates network bandwidth from download metrics:
- Fragment download time
- Fragment size
- Recent download history

## Profile Management

### Profile Structure

```cpp
struct ProfileInfo {
    bool isIframeTrack;
    BitsPerSecond bandwidthBitsPerSecond;
    int width;
    int height;
    std::string periodId;
    int userData;
};
```

### Profile Selection

1. **Initial Profile**: Based on default bitrate or medium profile
2. **Steady State**: Based on network bandwidth and buffer
3. **Ramp Down**: On download failures or low buffer
4. **Ramp Up**: On good network and high buffer

## ABR Algorithms

### Bandwidth-Based ABR

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

### Buffer-Based ABR

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

## Configuration

Key ABR configuration parameters:
- `abrCacheLife`: Bandwidth sample lifetime (ms)
- `abrCacheLength`: Number of samples to consider
- `abrNwConsistency`: Consistency checks before switching
- `abrMinBuffer`: Minimum buffer for rampdown (seconds)
- `abrMaxBuffer`: Maximum buffer for rampup (seconds)

## Low-Latency ABR

Special handling for low-latency DASH:
- Faster profile switching
- Chunk-based bandwidth estimation
- Reduced buffer thresholds

## Summary

The ABR system provides intelligent quality selection that:
- Maximizes quality within network constraints
- Minimizes rebuffering
- Provides smooth playback experience
- Adapts quickly to network changes
