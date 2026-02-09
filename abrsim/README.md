# AAMP ABR Simulator

## Overview

`abrsim` is a standalone tool for testing AAMP's Adaptive Bitrate (ABR) heuristics in faster-than-real-time without requiring actual stream downloads or playback.

## Features

- **Faster-than-real-time simulation**: Simulate hours of playback in seconds
- **Live streaming support**: Model live streams with target latency and buffer capping
- **Realistic network modeling**: Uses NetTrace persona format for authentic network behavior
- **DASH manifest abstraction**: Models typical video profile ladders with configurable bitrates
- **Video-focused**: Concentrates on video segment downloads (ignores manifest refreshes and audio)
- **Detailed reporting**: Generates CSV logs of all bitrate changes and rebuffering events
- **Extensible**: Designed to integrate with AAMP's actual ABRManager from `abr/` folder

## Building

### Simple Build (Standalone)

```bash
cd abrsim
g++ -std=c++17 -O2 -o abrsim abrsim.cpp
```

### Full Build (with ABR integration - future)

```bash
g++ -std=c++17 -O2 -I../abr -I.. -o abrsim abrsim.cpp \
    ../abr/abr.cpp ../abr/HarmonicEwmaEstimator.cpp \
    ../abr/RollingMedianOutlierEstimator.cpp
```

Note: Full ABR integration requires resolving dependencies on AAMP's config and logging infrastructure.

## Usage

### Basic Usage

**VOD (Video on Demand) Mode:**
```bash
./abrsim --persona ../simnet/personas/network.json \
         --duration 3600 \
         --out simulation_report.csv
```

**Live Streaming Mode:**
```bash
./abrsim --persona ../simnet/personas/network.json \
         --live \
         --target-latency 8 \
         --duration 3600 \
         --out live_simulation.csv
```

### Options

- `--persona <file>`: Network persona JSON file (required)
- `--duration <secs>`: Simulation duration in seconds (default: 3600)
- `--out <file>`: Output CSV filename (default: abrsim.csv)
- `--seed <n>`: Random seed for reproducibility (default: random)
- `--live`: Enable live streaming mode (default: VOD mode)
- `--target-latency <secs>`: Target distance from live edge in seconds (default: 8.0, only used in live mode)
- `--max-buffer <secs>`: Maximum buffer size in seconds for VOD mode (default: 20.0)
- `--help`: Show usage information

### Live vs VOD Mode

**VOD Modis capped at `--max-buffer` seconds (default: 20s)
- Simulates on-demand video playback with memory constraints
- Prevents unrealistic buffer growth on fast networks
- Useful for finding optimal buffer size vs memory tradeoff

**Live Mode** (`--live` flag):
- Buffer is capped at `--target-latency` seconds
- Simulates maintaining a fixed distance from live edge
- Tracks latency drift from target
- More realistic for live streaming scenarios
- The `--max-buffer` option is ignored in live mode

**Memory vs Stability Tradeoff:**
- **Smaller buffer** (5-10s): Lower memory usage, higher rebuffering risk
- **Moderate buffer** (15-25s): Balanced approach, recommended default
- **Larger buffer** (30-50s): More resilient to network hiccups, uses more memory

Example showing buffer cap effect:
```bash
# Small buffer: Uses less memory but may rebuffer more on variable networks
./abrsim --persona sample_network.json --max-buffer 10 --duration 3600 --out small_buf.csv

# Default: Balanced 20-second buffer
./abrsim --persona sample_network.json --duration 3600 --out default_buf.csv

# Large buffer: More resilient but uses more memory
./abrsim --persona sample_network.json --max-buffer 40 --duration 3600 --out large_buf
./abrsim --persona sample_network.json --live --target-latency 8 --duration 3600 --out live.csv
```

### Example: Multi-Hour Simulation

Simulate 2 hours of live playback with a slow network:

```bash
./abrsim --persona network_slow.json --live --target-latency 8 --duration 7200 --out slow_live_2hr.csv
```

## Network Persona Format

The tool uses the same JSON format as `simnet`:

```json
{
  "base_rtt_ms": 175.0,
  "rtt_jitter_ms": 20.0,
  "ttfb_spike_p": 0.01,
  "ttfb_spike_ms": 120.0,
  "mean_thr_mbps": 200.0,
  "thr_sigma_ln": 0.80,
  "thr_rho": 0.15,
  "bursts_per_segment": 8,
  "burst_bytes_cv": 0.40,
  "cadence_ms": 175.0,
  "cadence_jitter_ms": 45.0,
  "flush_jitter_ms": 6.0,
  "late_chunk_p": 0.01,
  "late_chunk_extra_ms": 120.0,
  "p_conn_reuse": 0.95,
  "new_conn_penalty_ms": 170.0
}
```

## Output Format

The output CSV contains one row per simulation event:

```csv
time_s,event_type,profile_idx,download_ms,throughput_bps,buffer_s,description
0.000,download,3,234.56,5678901,2.00,Profile 1400 kbps
2.345,download,3,198.23,6123456,3.80,Profile 1400 kbps
4.567,profile_change,4,0.00,0,5.20,1400 -> 2800 kbps
4.567,download,4,312.45,8901234,4.50,Profile 2800 kbps
```

Event types:
- `download`: Segment download completed
- `profile_change`: ABR algorithm changed bitrate
- `rebuffer_start`: Buffer underrun detected
- `rebuffer_end`: Rebuffering recovered

## Video Profile Ladder

The tool uses a default DASH-style video profile ladder:

| Profile | Bitrate | Resolution | Typical Use Case |
|---------|---------|------------|------------------|
| 0 | 235 kbps | 426x240 | Very low bandwidth |
| 1 | 375 kbps | 640x360 | Low bandwidth |
| 2 | 750 kbps | 854x480 | SD quality |
| 3 | 1.4 Mbps | 1280x720 | HD ready |
| 4 | 2.8 Mbps | 1920x1080 | Full HD |
| 5 | 5.0 Mbps | 1920x1080 | High quality HD |
| 6 | 8.0 Mbps | 3840x2160 | 4K UHD |

Future versions will support loading custom profile ladders from JSON.

## Architecture

### Components

1. **NetworkSimulator**: Simulates segment downloads with realistic timing and throughput variation based on persona parameters

2. **VideoProfileLadder**: Models DASH manifest video profiles with bitrates and segment sizes

3. **PlaybackBuffer**: Tracks buffer level, detects underruns, and manages rebuffering state

4. **ABRSimulator**: Main simulation loop that:
   - Downloads segments based on current profile
   - Consumes buffer during playback
   - Makes ABR decisions based on throughput and buffer health
   - Logs all events

5. **EventLogger**: Records simulation events and generates CSV reports

### Current ABR Logic

The current implementation uses a simplified ABR algorithm:

- **Ramp Down**: If buffer < 5s and throughput < 1.3x required bitrate
- **Ramp Up**: If buffer > 15s and throughput > 1.95x required bitrate
- **Stable**: Otherwise maintain current profile

Future versions will integrate AAMP's actual `ABRManager` for production-accurate behavior.

## Future Enhancements

### Planned Features

1. **ABRManager Integration**
   - Replace simplified logic with actual AAMP ABR algorithms
   - Support all ABR modes (conservative, moderate, aggressive)
   - Use real bandwidth estimators (Harmonic EWMA, Rolling Median)

2. **Custom Profile Ladders**
   - Load profiles from JSON manifest files
   - Support real DASH MPD parsing
   - Variable segment sizes per profile

3. **Advanced Scenarios**
   - Network condition changes mid-stream
   - Connection drops and recovery
   - Seek operations and buffer refilling
   - Multi-CDN fallback simulation

4. **Enhanced Reporting**
   - Statistical summary (avg bitrate, time in each profile, etc.)
   - Charts and visualizations
   - Comparison mode (multiple personas side-by-side)
   - QoE scoring

5. **Validation**
   - Compare simulation results against real playback logs
   - Regression testing for ABR changes
   - Performance benchmarking

## Testing ABR Changes

Example workflow for testing ABR modifications:

```bash
# Baseline run with current ABR logic
./abrsim --persona baseline_network.json --duration 3600 --out baseline.csv --seed 42

# Run with modified ABR logic (after code changes)
./abrsim --persona baseline_network.json --duration 3600 --out modified.csv --seed 42

# Compare results
python compare_results.py baseline.csv modified.csv
```

Using the same seed ensures reproducible network conditions for fair comparison.

## Integration with NetTrace

This tool complements the NetTrace instrumentation:

1. **NetTrace** captures real-world network behavior → produces persona JSON
2. **simnet** can validate and tune persona parameters
3. **abrsim** uses personas to test ABR behavior at scale

Example pipeline:

```bash
# 1. Capture real network trace during playback (integrated in AAMP)
# Produces: /tmp/aamp_net_requests.csv

# 2. Fit persona to captured data
cd simnet
python persona_fit.py /tmp/aamp_net_requests.csv --out fitted_persona.json

# 3. Test ABR with fitted persona
cd ../abrsim
./abrsim --persona fitted_persona.json --duration 7200 --out abr_test.csv
```

## Limitations

Current version limitations:

- Simplified ABR logic (not using real ABRManager yet)
- Fixed profile ladder (no custom manifests)
- Video-only (no audio/subtitle simulation)
- No seek operations
- Single network condition (no mid-stream changes)
- No DRM or encryption modeling

These will be addressed in future versions.

## Contributing

When extending this tool:

1. Follow AAMP coding standards (see `.github/copilot-instructions.md`)
2. Add comprehensive comments for ABR decision logic
3. Include unit tests for new components
4. Update this README with new features

## License

Copyright 2026 RDK Management

Licensed under the Apache License, Version 2.0. See LICENSE file for details.
