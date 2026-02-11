# Buffer Management

## Overview

AAMP uses sophisticated buffer management to ensure smooth playback while minimizing memory usage.

## Buffer Types

### 1. Fragment Cache

In-memory cache for downloaded fragments:
- **Location**: `MediaTrack::mCachedFragment[]`
- **Size**: Configurable (default: 3 fragments per track)
- **Purpose**: Pre-download fragments for smooth playback

### 2. Time-Based Buffer

Time-based buffering strategy:
- **Location**: `AampTimeBasedBufferManager`
- **Purpose**: Maintain buffer based on playback time
- **Configuration**: `timeBasedBufferSeconds`

### 3. Byte-Based Buffer

Byte-based buffering (legacy):
- Based on fragment count
- Used when time-based is disabled

## Buffer Health Monitoring

### Buffer Status

```cpp
enum BufferHealthStatus {
    BUFFER_STATUS_GREEN,   // Healthy (> warning threshold)
    BUFFER_STATUS_YELLOW,  // Warning (< warning, > min)
    BUFFER_STATUS_RED      // Critical (< min threshold)
};
```

### Monitoring Thread

Each track has a buffer monitor thread:

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

## Buffer Management Strategies

### Pre-Buffering

Before playback starts:
- Download initial fragments
- Wait for minimum buffer
- Then start playback

### Steady-State Buffering

During playback:
- Maintain target buffer level
- Download ahead of playback position
- Adjust download rate based on buffer

### Underflow Prevention

When buffer is low:
- Ramp down profile
- Prioritize fragment downloads
- Extend buffer if possible

## Configuration

Key buffer configuration:
- `downloadBuffer`: Fragment cache size
- `timeBasedBufferSeconds`: Time-based buffer target
- `initialBuffer`: Initial buffering duration
- `minABRBufferRampdown`: Minimum buffer for ABR rampdown
- `maxABRBufferRampup`: Maximum buffer for ABR rampup

## Summary

Buffer management ensures:
- Smooth playback without stalling
- Efficient memory usage
- Quick response to network changes
- Optimal quality selection
