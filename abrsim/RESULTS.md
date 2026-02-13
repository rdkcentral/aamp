# ABR Simulator Test Results

## Overview
The ABR simulator tests AAMP's adaptive bitrate heuristics in faster-than-real-time without requiring actual stream playback. Supports both **VOD (Video on Demand)** and **Live Streaming** modes.

## Streaming Modes

### VOD Mode (Default)
- Buffer can grow indefinitely
- Downloads run as fast as network allows  
- Suitable for testing on-demand content
- **Issue**: Buffer can grow to unrealistic sizes (e.g., 275 seconds) on fast networks

### Live Mode (`--live`)
- Buffer capped at `--target-latency` (default: 8 seconds)
- Simulates maintaining fixed distance from live edge
- Tracks latency drift from target
- **Realistic** for live streaming scenarios
- Buffer never exceeds target latency

## Performance
- **Speed-up factor**: ~600,000x to 2,000,000x faster than real-time
- **1-hour simulation**: Completes in ~2 milliseconds
- **Multi-hour capable**: Tested up to 3600 seconds with no issues

## Test Results by Network Type

### 500 Mbps Fiber (Excellent Connection)

**VOD Mode:**
```
Duration: 60 seconds
Segments downloaded: 64
Profile changes: 3 (upshifted from 1400 → 2800 → 5000 → 8000 kbps)
Rebuffer events: 1 (initial startup only, 0.93s)
Final buffer level: 68.79 seconds (grows without limit)
```

**Live Mode (8s target latency):**
```
Duration: 60 seconds
Segments downloaded: 60
Profile changes: 0 (stayed at starting profile)
Rebuffer events: 1 (initial startup only, 0.93s)
Final buffer level: 8.00 seconds (capped at target)
Average latency: 15.71 seconds
Latency drift: +7.71 seconds (poor - needs tuning)
```

**Behavior**: Quickly upshifts to highest quality and maintains it. Live mode successfully caps buffer.

### 25 Mbps Broadband (Good Connection)

**VOD Mode:**
```
Duration: 60 seconds
Segments downloaded: 32
Profile changes: 3 (downshifted from 1400 → 750 → 375 → 235 kbps)
Rebuffer events: 1 (initial startup, 2.02s)
Final buffer level: 5.33 seconds
```

**Live Mode (8s target latency):**
```
Duration: 60 seconds
Segments downloaded: 32
Profile changes: 3 (downshifted from 1400 → 750 → 375 → 235 kbps)
Rebuffer events: 1 (initial startup, 2.02s)
Final buffer level: 5.33 seconds (under target)
Average latency: 11.64 seconds
Latency drift: +3.64 seconds (acceptable)
```

**Behavior**: Downshifts to lowest profile but plays smoothly. Similar behavior in both modes.
Segments downloaded: 32
Profile changes: 3 (downshifted from 1400 → 750 → 375 → 235 kbps)
Rebuffer events: 1 (initial startup, 2.02s)
Final buffer level: 5.33 seconds
```
**Behavior**: Downshifts to lowest quality but plays smoothly.

### 12 Mbps Congested WiFi (Poor Connection)
```
Duration: 60 seconds
Segments downloaded: 18
Profile changes: 3
Rebuffer events: 18
```
**Behavior**: Heavy rebuffering due to insufficient bandwidth.

### 2.5 Mbps Mobile 3G (Very Poor Connection)
```
Duration: 60 seconds
Segments downloaded: 21
Profile changes: 3
Rebuffer events: 21
```
**Behavior**: Constant rebuffering, network cannot sustain even lowest profile.

## Video Profile Ladder
The simulator uses a 7-profile DASH-like ladder:
- Profile 0: 235 kbps @ 320x240
- Profile 1: 375 kbps @ 416x240
- Profile 2: 750 kbps @ 640x360
- Profile 3: 1400 kbps @ 854x480
- Profile 4: 2800 kbps @ 1280x720
- Profile 5: 5000 kbps @ 1920x1080
- Profile 6: 8000 kbps @ 1920x1080

Segment duration: 2 seconds

## ABR Algorithm
Current implementation uses a simplified ABR heuristic:
- **Upshift**: When throughput > 1.5x current bitrate
- **Downshift**: When throughput < 1.2x current bitrate
- **Buffer-aware**: Considers buffer health in decisions

Future enhancement: Integrate real ABRManager from `abr/` folder.

## Network Simulation
Uses realistic network modeling with:
- Burst-based transfer patterns
- RTT variation and TTFB spikes
- Throughput variation (AR(1) lognormal process)
- Cadence timing between bursts
- Occasional late chunks/stalls

## CSV Output Format
```
time_s,event_type,profile_idx,download_ms,throughput_bps,buffer_s,description
0.931,download,3,931.20,3244380,2.00,Profile 1400 kbps
11.851,profile_change,4,0.00,0,15.08,1400 -> 2800 kbps
```

Event types:
- `download`: Segment download completed
- `profile_change`: ABR switched bitrates
- `rebuffer_start`: Buffer underrun detected
- `rebuffer_end`: Playback resumed

## Use Cases
1. **Algorithm Tuning**: Test different ABR strategies quickly
2. **Network Analysis**: Understand how different network conditions affect QoE
3. **Regression Testing**: Verify ABR behavior doesn't degrade
4. **Capacity Planning**: Determine minimum bandwidth requirements
5. **Multi-hour Testing**: Simulate long viewing sessions in seconds

## Next Steps
- [ ] Integrate real ABRManager from `abr/abr.h`
- [ ] Support custom profile ladders from JSON
- [ ] Add mid-stream network condition changes
- [ ] Support seek operations
- [ ] Generate QoE metrics (VMAF estimates, startup time, etc.)
