# ABR Simulator Web UI

## Overview

The ABR Simulator Web UI provides a user-friendly interface for running and visualizing ABR simulations without using the command line.

## Features

- **Interactive Parameter Configuration**
  - Select from pre-configured network personas
  - Adjust simulation duration, buffer settings, and latency targets
  - Toggle between VOD and Live streaming modes
  - Set random seeds for reproducible results

- **Real-time Visualization**
  - Bitrate selection over time
  - Buffer level tracking
  - Bandwidth measurements vs selected bitrate
  - Interactive charts powered by Chart.js

- **Results Summary**
  - Rebuffer event counts and duration
  - Final buffer levels
  - Live streaming latency metrics
  - Simulation speed-up factor

## Quick Start

### 1. Build abrsim

First, build the abrsim binary:

```bash
cd abrsim
./build.sh
```

Or to build with real AAMP ABR integration:

```bash
./build.sh --real
```

### 2. Start the Web Server

```bash
./abrsim_server.py
```

The server will start on http://localhost:8080

### 3. Open in Browser

Navigate to http://localhost:8080 in your web browser.

## Using the Web Interface

### Simulation Parameters

1. **Network Persona**: Select a pre-configured network profile (e.g., mobile 3G, fiber gigabit)
2. **Duration**: Set simulation duration in seconds (60 - 86400)
3. **Streaming Mode**:
   - **VOD Mode**: Set maximum buffer size
   - **Live Mode**: Set target latency from live edge
4. **Random Seed**: Set to 0 for random, or use a specific value for reproducible runs

### Running Simulations

1. Configure parameters in the left panel
2. Click "Run Simulation"
3. Wait for simulation to complete (typically a few seconds)
4. View results in charts and summary

### Understanding Results

#### Summary Metrics

- **Rebuffer Events**: Number of times playback stalled (lower is better)
  - Green: 0 events
  - Yellow: 1-2 events
  - Red: 3+ events

- **Total Rebuffer Time**: Cumulative stalling duration (lower is better)
  - Green: 0 seconds
  - Yellow: < 5 seconds
  - Red: ≥ 5 seconds

- **Final Buffer Level**: Buffer state at simulation end (higher is better)
  - Green: > 10 seconds
  - Yellow: 5-10 seconds
  - Red: < 5 seconds

- **Average Latency** (Live mode only): Distance from live edge
  - Green: Within 1s of target
  - Yellow: 1-3s from target
  - Red: > 3s from target

#### Charts

1. **Bitrate Over Time**: Shows ABR profile selection decisions
   - Step pattern indicates bitrate changes
   - Higher is better quality, but requires more bandwidth

2. **Buffer Level Over Time**: Visualizes buffer health
   - Dips toward zero indicate potential rebuffering
   - Stable buffer indicates healthy playback

3. **Bandwidth Over Time**: Compares measured throughput vs selected bitrate
   - Yellow line: actual measured bandwidth
   - Blue dashed line: selected bitrate requirement
   - Bandwidth should consistently exceed bitrate for smooth playback

## Architecture

```
┌─────────────────┐
│   Web Browser   │
│   (index.html)  │
└────────┬────────┘
         │ HTTP
         ▼
┌─────────────────┐
│ Python Server   │
│ (abrsim_server) │
└────────┬────────┘
         │ subprocess
         ▼
┌─────────────────┐
│  abrsim binary  │
│  (C++ CLI tool) │
└─────────────────┘
```

### Components

- **Web UI** (`web/`):
  - `index.html`: Page structure
  - `style.css`: Styling and layout
  - `app.js`: Client-side logic and Chart.js integration

- **Python Server** (`abrsim_server.py`):
  - Serves static web files
  - REST API for running simulations
  - CSV parsing and JSON response generation

- **ABR Simulator** (`abrsim`):
  - Core simulation engine
  - Network modeling
  - ABR algorithm (simple or real AAMP)

## API Reference

### GET /api/personas

List available network personas.

**Response:**
```json
{
  "personas": [
    {
      "filename": "mobile_3g.json",
      "name": "Mobile 3g",
      "bandwidth": 2.5
    }
  ]
}
```

### POST /api/simulate

Run ABR simulation.

**Request:**
```json
{
  "persona": "mobile_3g.json",
  "duration": 3600,
  "is_live": false,
  "target_latency": 8.0,
  "max_buffer": 20.0,
  "seed": 0
}
```

**Response:**
```json
{
  "success": true,
  "events": [
    {
      "time_s": 0.123,
      "event_type": "download",
      "profile_idx": 2,
      "download_ms": 456.7,
      "throughput_bps": 890000,
      "buffer_s": 12.3,
      "description": "Profile 750 kbps"
    }
  ],
  "summary": {
    "rebuffer_events": 0,
    "total_rebuffer_time": 0.0,
    "final_buffer": 15.2,
    "speedup": 1234.5
  }
}
```

## Customization

### Adding New Network Personas

1. Create a JSON file in `personas/` directory
2. Follow the NetTrace format (see existing files)
3. Refresh the web UI - it will appear in the dropdown

### Modifying Profile Ladder

Edit the profile ladder in `abrsim.cpp` (main function):

```cpp
ladder.addProfile(0,  235000,   426,  240, 0, 0);  // 235 kbps, 240p
ladder.addProfile(1,  375000,   640,  360, 0, 0);  // 375 kbps, 360p
// ... add more profiles
```

Rebuild abrsim after changes.

### Chart Customization

Edit `web/app.js` to modify Chart.js options:
- Colors: Change dataset `borderColor` and `backgroundColor`
- Scale ranges: Adjust `scales.y.min` and `scales.y.max`
- Display options: Modify `plugins` configuration

## Troubleshooting

### "abrsim binary not found"

**Solution**: Build abrsim first with `./build.sh`

### "Simulation failed"

**Possible causes**:
- Invalid persona file
- Duration too short or too long
- Missing dependencies for real ABR build

**Solution**: Check console output and verify parameters

### Charts not displaying

**Possible causes**:
- No simulation run yet
- JavaScript errors in browser console
- Network connection issues (Chart.js CDN)

**Solution**: Check browser developer console for errors

### Server won't start

**Possible causes**:
- Port 8080 already in use
- Python not installed

**Solution**: 
```bash
# Use different port
PORT=8888 ./abrsim_server.py

# Check Python version (requires 3.6+)
python3 --version
```

## Performance Tips

1. **Simulation Duration**: Longer simulations take more time
   - Start with 600-1800 seconds for quick tests
   - Use 3600+ seconds for comprehensive analysis

2. **Browser Performance**: Large datasets may slow chart rendering
   - Consider limiting maximum simulation duration
   - Chrome/Edge typically have better Canvas performance

3. **Server Performance**: Python server is single-threaded
   - Only one simulation runs at a time
   - Consider using production WSGI server for multiple users

## Development

### Requirements

- Python 3.6+
- Modern web browser with JavaScript enabled
- Internet connection (for Chart.js CDN)

### Local Development

```bash
# Start server in debug mode
python3 abrsim_server.py

# Server will auto-reload on file changes (manual restart required)
```

### Testing

Test the API directly with curl:

```bash
# List personas
curl http://localhost:8080/api/personas

# Run simulation
curl -X POST http://localhost:8080/api/simulate \
  -H "Content-Type: application/json" \
  -d '{"persona":"sample_network.json","duration":600,"is_live":false}'
```

## Future Enhancements

- [ ] Real-time streaming of simulation progress
- [ ] Export results to CSV/PDF
- [ ] Comparison mode (run multiple simulations side-by-side)
- [ ] Custom profile ladder editor
- [ ] Advanced bandwidth estimation algorithm configuration
- [ ] Historical simulation runs database
- [ ] WebSocket support for long-running simulations

## License

Copyright 2026 RDK Management

Licensed under the Apache License, Version 2.0
