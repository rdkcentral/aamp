# Network Scenario Support for abrsim

## Overview

The abrsim simulator now supports **network scenarios** - multi-stage simulations that test ABR behavior under changing network conditions. This is useful for testing:

- Network degradation and recovery
- Mobile device movement (WiFi → 4G → WiFi)
- Congested network conditions
- ABR ramp-down/ramp-up behavior
- Buffer resilience during transitions

## Scenario File Format

Scenarios are defined in JSON files in the `scenarios/` directory:

```json
{
  "description": "Test network degradation and recovery",
  "stages": [
    {
      "persona": "personas/wifi_good.json",
      "duration": 60,
      "description": "Start with good WiFi"
    },
    {
      "persona": "personas/wifi_congested.json",
      "duration": 20,
      "description": "Network degrades"
    },
    {
      "persona": "personas/wifi_good.json",
      "duration": 60,
      "description": "Network recovers"
    }
  ]
}
```

### Fields

- `description`: Human-readable description of the scenario
- `stages`: Array of network stages to execute sequentially
  - `persona`: Path to persona JSON file (relative to abrsim directory)
  - `duration`: Duration of this stage in seconds
  - `description`: Description of this stage (optional)

## CLI Usage

Run a scenario from the command line:

```bash
./abrsim --scenario scenarios/degradation_recovery.json --duration 140
```

The `--duration` flag is ignored for scenarios - the total duration is calculated from the sum of all stage durations.

### Example Output

```
Created profile ladder with 7 profiles
Loaded scenario: Test network degradation and recovery (3 stages)

=== Stage 1/3: Start with good WiFi (60s) ===
  Network: 75 Mbps average
Using AAMP's real ABR algorithm
Starting VOD ABR simulation for 60 seconds...
...

=== Stage 2/3: Network degrades (20s) ===
  Network: 12 Mbps average
...

=== Stage 3/3: Network recovers (60s) ===
  Network: 75 Mbps average
...

=== Scenario Complete ===
Total time: 140s
Wrote event log to abrsim.csv
```

## Web UI Usage

1. Start the web server:
   ```bash
   python3 abrsim_server.py 8080
   ```

2. Open http://localhost:8080 in your browser

3. Select **"Network Scenario"** mode (radio button)

4. Choose a scenario from the dropdown

5. Click **"Run Simulation"**

The web UI will:
- Display the scenario description
- Auto-fill the duration based on scenario stages
- Show combined results across all stages
- Visualize timeline with all stage transitions

## Creating New Scenarios

To create a new scenario:

1. Create a JSON file in `scenarios/` directory
2. Define stages with persona files and durations
3. The scenario will automatically appear in the web UI dropdown

### Example Scenarios

**Moving Device (WiFi → 4G → WiFi):**
```json
{
  "description": "Simulate device moving between WiFi and 4G",
  "stages": [
    {"persona": "personas/wifi_good.json", "duration": 30},
    {"persona": "personas/mobile_4g.json", "duration": 60},
    {"persona": "personas/wifi_good.json", "duration": 30}
  ]
}
```

**Peak Hour Congestion:**
```json
{
  "description": "Broadband during peak hours",
  "stages": [
    {"persona": "personas/broadband_moderate.json", "duration": 120},
    {"persona": "personas/wifi_congested.json", "duration": 60},
    {"persona": "personas/broadband_moderate.json", "duration": 120}
  ]
}
```

## Implementation Details

### How It Works

1. Each stage creates a new `ABRSimulator` instance with the specified network persona
2. The simulator runs for the stage duration
3. Events from all stages are combined with time offsets
4. Results are written to a single CSV file with continuous timestamps

### ABR Behavior

The ABR algorithm sees each stage transition as a sudden network change:
- Bandwidth estimation adapts based on new download speeds
- Buffer level is preserved across stage boundaries
- Profile selection responds to the new network conditions

This tests the ABR's ability to:
- Detect network changes quickly
- Ramp down to avoid rebuffering
- Ramp up when conditions improve
- Maintain smooth playback during transitions

## API Reference

### `/api/scenarios` (GET)

Lists available scenarios:

```json
{
  "scenarios": [
    {
      "filename": "degradation_recovery.json",
      "name": "Degradation Recovery",
      "description": "Test network degradation and recovery",
      "stages": 3,
      "total_duration": 140
    }
  ]
}
```

### `/api/simulate` (POST)

Run a scenario simulation:

```json
{
  "scenario": "degradation_recovery.json",
  "duration": 140,
  "is_live": false,
  "max_buffer": 20,
  "seed": 0
}
```

Response format is identical to persona simulations.

## Testing Scenarios

Recommended test scenarios:

1. **Degradation Test**: Start with good network, degrade for 20-30s, verify ABR ramps down
2. **Recovery Test**: Start poor, improve network, verify ABR ramps back up
3. **Stability Test**: Multiple small fluctuations, verify ABR doesn't thrash
4. **Handoff Test**: WiFi ↔ Cellular transitions with different latencies
5. **Resilience Test**: Extreme variance with short-duration personas

## Performance Notes

- Scenarios run in faster-than-real-time (100,000x speedup typical)
- Each stage boundary has minimal overhead (~1ms)
- Total runtime scales linearly with total scenario duration
- Memory usage is proportional to number of events (segments downloaded)

## Future Enhancements

Potential improvements for scenarios:

- [ ] Visual timeline editor in web UI
- [ ] Gradual transitions between personas (linear interpolation)
- [ ] Loop/repeat stage patterns
- [ ] Conditional stages based on buffer level
- [ ] Export scenario from real network measurements
- [ ] Scenario comparison (A/B testing)
