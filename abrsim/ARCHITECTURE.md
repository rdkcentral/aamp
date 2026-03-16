# ABR Simulator Architecture

## System Overview

```
┌─────────────────────────────────────────────────────────────────┐
│                        User Interface Layer                      │
├───────────────────────────┬─────────────────────────────────────┤
│                           │                                     │
│   ┌───────────────────┐   │   ┌─────────────────────────────┐   │
│   │   Web Browser     │   │   │    Command Line             │   │
│   │  (index.html)     │   │   │    Terminal                 │   │
│   │   - Forms         │   │   │    ./abrsim --options       │   │
│   │   - Charts        │   │   │                             │   │
│   │   - Dashboard     │   │   │                             │   │
│   └─────────┬─────────┘   │   └──────────────┬──────────────┘   │
│             │             │                  │                  │
└─────────────┼─────────────┴──────────────────┼──────────────────┘
              │ HTTP/JSON                      │ stdin/stdout
              │ REST API                       │ CLI args
              │                                │
┌─────────────▼────────────────────────────────▼──────────────────┐
│                     Application Layer                            │
├──────────────────────────────────────────────────────────────────┤
│                                                                  │
│   ┌─────────────────────────┐    ┌──────────────────────────┐   │
│   │   Python Web Server     │    │   abrsim Binary          │   │
│   │   (abrsim_server.py)    │    │   (C++ Application)      │   │
│   │                         │    │                          │   │
│   │   - HTTP Server         │───▶│   - Network Simulator    │   │
│   │   - JSON API            │    │   - Buffer Manager       │   │
│   │   - CSV Parsing         │    │   - Event Logger         │   │
│   │   - Result Formatting   │    │   - Profile Ladder       │   │
│   └─────────────────────────┘    └──────────┬───────────────┘   │
│                                              │                  │
│                                              │ Delegates to     │
│                                              │                  │
│                                  ┌───────────▼───────────────┐   │
│                                  │   ABR Decision Engine     │   │
│                                  ├───────────────────────────┤   │
│                                  │                           │   │
│   ┌──────────────────────────────┤   [Conditional Build]    │   │
│   │                              │                           │   │
│   │  Simple Mode                 │   Real ABR Mode          │   │
│   │  (Default)                   │   (--real flag)          │   │
│   │                              │                           │   │
│   │  ┌────────────────┐          │   ┌─────────────────┐    │   │
│   │  │ Placeholder    │          │   │ AbrSimAdapter   │    │   │
│   │  │ ABR Logic      │          │   │                 │    │   │
│   │  │                │          │   │ - Profile Mgmt  │    │   │
│   │  │ - Simple       │          │   │ - Bandwidth Est │    │   │
│   │  │   threshold    │          │   │ - Decision Ctx  │    │   │
│   │  │ - Buffer check │          │   │ - AAMP Bridge   │    │   │
│   │  └────────────────┘          │   └────────┬────────┘    │   │
│   │                              │            │             │   │
│   └──────────────────────────────┤            │             │   │
│                                  │   ┌────────▼────────┐    │   │
│                                  │   │   ABRManager    │    │   │
│                                  │   │   (AAMP Core)   │    │   │
│                                  │   │                 │    │   │
│                                  │   │ - Harmonic EWMA │    │   │
│                                  │   │ - Rolling Median│    │   │
│                                  │   │ - Ramping Logic │    │   │
│                                  │   │ - Profile Select│    │   │
│                                  │   └─────────────────┘    │   │
│                                  └───────────────────────────┘   │
└──────────────────────────────────────────────────────────────────┘
```

## Component Details

### Web UI Layer

**Technologies**: HTML5, CSS3, JavaScript, Chart.js

**Components**:
- `index.html` - Page structure and layout
- `style.css` - Modern styling with responsive design
- `app.js` - Client logic, API calls, chart rendering

**Responsibilities**:
- User input collection
- Parameter validation
- API communication
- Results visualization
- Chart rendering

### Web Server (Python)

**Technology**: Python 3 HTTP server

**Endpoints**:
- `GET /` - Serve index.html
- `GET /api/personas` - List available network profiles
- `GET /api/status` - Server health check
- `POST /api/simulate` - Run simulation

**Responsibilities**:
- Static file serving
- REST API implementation
- Subprocess management (run abrsim binary)
- CSV parsing
- JSON response formatting

### ABR Simulator Core (C++)

**Main Components**:

1. **NetworkSimulator**
   - Simulates download behavior
   - Models RTT, TTFB, throughput
   - Burst-based data transfer
   - Connection reuse/setup delays

2. **PlaybackBuffer**
   - Tracks buffer level
   - Detects rebuffering
   - Monitors latency (live mode)
   - Consumes buffer during downloads

3. **EventLogger**
   - Records all simulation events
   - Generates CSV output
   - Provides summary statistics

4. **VideoProfileLadder**
   - Manages bitrate profiles
   - Calculates segment sizes
   - Provides profile lookup

### ABR Decision Engine

#### Simple Mode (Default)

**Logic**:
```cpp
if (buffer < 5s && throughput < bitrate * 1.3) {
    ramp_down();
} else if (buffer > 15s && throughput > bitrate * 1.5) {
    ramp_up();
}
```

**Pros**: No dependencies, fast compilation
**Cons**: Simplistic, not production-representative

#### Real ABR Mode (--real flag)

**Components**:

1. **AbrSimAdapter** (New)
   - Bridges simulation ↔ AAMP
   - Converts types
   - Manages ABRManager lifecycle
   - Provides decision context

2. **ABRManager** (AAMP)
   - Production ABR algorithm
   - Bandwidth estimation
   - Profile selection
   - Ramping strategies

**Bandwidth Estimators**:
- **Harmonic EWMA**: Smooth, responsive
- **Rolling Median**: Outlier-resistant

**Decision Factors**:
- Current bandwidth estimate
- Network bandwidth trend
- Buffer level
- Rebuffering state
- Live streaming latency
- Network consistency count

## Data Flow

### Web UI Simulation Request

```
1. User clicks "Run Simulation" in browser
   ↓
2. JavaScript collects parameters from form
   ↓
3. POST request to /api/simulate with JSON
   ↓
4. Python server receives request
   ↓
5. Validates parameters
   ↓
6. Constructs command line for abrsim binary
   ↓
7. Runs: ./abrsim --persona X --duration Y --out temp.csv
   ↓
8. abrsim runs simulation
   ├─ NetworkSimulator generates download events
   ├─ PlaybackBuffer tracks buffer state
   ├─ ABR engine makes profile decisions
   └─ EventLogger records to CSV
   ↓
9. Simulation completes
   ↓
10. Python server reads CSV output
   ↓
11. Parses CSV into JSON events array
   ↓
12. Extracts summary statistics
   ↓
13. Returns JSON response to browser
   ↓
14. JavaScript receives response
   ↓
15. Updates summary metrics
   ↓
16. Renders charts using Chart.js
```

### Command Line Simulation

```
1. User runs: ./abrsim --persona X --duration Y --out Z.csv
   ↓
2. Parse command line arguments
   ↓
3. Load network persona JSON
   ↓
4. Create profile ladder
   ↓
5. Initialize ABR (simple or real)
   ↓
6. Run simulation loop:
   ├─ Decide if download needed
   ├─ Select segment size based on profile
   ├─ Simulate download with NetworkSimulator
   ├─ Consume buffer during download
   ├─ Add segment to buffer
   ├─ Log download event
   ├─ Make ABR decision
   └─ Log profile change if any
   ↓
7. Generate CSV report
   ↓
8. Print summary to stdout
```

## Conditional Compilation

The ABR engine is conditionally compiled:

```cpp
#ifdef USE_REAL_ABR
    // Real AAMP ABR
    #include "AbrSimAdapter.h"
    mAbrAdapter = std::make_unique<abrsim::AbrSimAdapter>();
    // ... use real ABR ...
#else
    // Simple placeholder ABR
    if (buffer_low && throughput_low) ramp_down();
    else if (buffer_high && throughput_high) ramp_up();
#endif
```

**Build Commands**:
- `./build.sh` → Simple mode (no USE_REAL_ABR)
- `./build.sh --real` → Real mode (defines USE_REAL_ABR)

## Configuration Flow

### Network Persona

```
personas/mobile_3g.json
    ↓
NetworkCharacteristics struct
    ↓
NetworkSimulator
    ↓
Generates download metrics
```

### ABR Configuration

```
User settings (buffer, latency)
    ↓
#ifdef USE_REAL_ABR
    AbrSimAdapter::configureAbrParameters()
        ↓
    ABRManager settings
#endif
```

## Output Flow

### CSV Output

```
EventLogger
    ↓
CSV file (time_s, event_type, profile_idx, ...)
    ↓
[Web UI path]
    Python server
        ↓
    JSON array of events
        ↓
    Browser (Chart.js)
        ↓
    Interactive charts
```

### Console Output

```
EventLogger
    ↓
stdout (summary statistics)
    ↓
[Web UI path]
    Python server
        ↓
    Parse stdout
        ↓
    JSON summary object
        ↓
    Browser (metrics display)
```

## Extension Points

### Adding New Bandwidth Estimators

```cpp
// In ABRManager
enum BandwidthEstimationAlgorithm {
    ROLLING_MEDIAN_OUTLIER = 0,
    HARMONIC_EWMA = 1,
    YOUR_NEW_ESTIMATOR = 2  // Add here
};

// Select in AbrSimAdapter
mAbrAdapter->selectBandwidthEstimationAlgorithm(2);
```

### Adding New Network Personas

```bash
# Create JSON in personas/
cat > personas/my_network.json << EOF
{
  "mean_thr_mbps": 10.0,
  "base_rtt_ms": 50.0,
  ...
}
EOF

# Automatically appears in web UI dropdown
```

### Custom Profile Ladders

```cpp
// In main() function of abrsim.cpp
VideoProfileLadder ladder;
ladder.addProfile(0, 100000, 320, 180);   // Ultra-low
ladder.addProfile(1, 500000, 640, 360);   // Low
// ... add your profiles ...
```

## Performance Characteristics

### Speed-Up Factor

Typical: **1000x - 10000x** faster than real-time

Example: 1 hour simulation = 1-10 seconds

### Memory Usage

- Simple mode: ~10 MB
- Real ABR mode: ~20-30 MB (with ABRManager)

### Scalability

- Single simulation: 1 second - 1 minute
- Web UI: Single-threaded (one sim at a time)
- Production: Could parallelize with job queue

## Security Considerations

### Web Server

- **Input validation**: All parameters validated
- **Path traversal**: Persona files restricted to personas/ dir
- **Subprocess injection**: Command args properly escaped
- **Temporary files**: Cleaned up after use
- **CORS**: Enabled for localhost development

### Recommendations for Production

- Add authentication
- Use HTTPS
- Rate limiting
- Input sanitization
- Proper error handling
- Logging and monitoring

---

This architecture provides:
- ✅ Clean separation of concerns
- ✅ Flexible build modes (simple vs real ABR)
- ✅ Multiple interfaces (CLI + Web)
- ✅ Easy extension points
- ✅ Production ABR validation capability
