# Quick Start Guide - AAMP ABR Simulator

## 5-Minute Setup

### 1. Build the Simulator

```bash
cd abrsim
./build.sh
```

Expected output:
```
Building AAMP ABR Simulator...
Building standalone version...
✓ Build successful: ./abrsim
```

### 2. Run Your First Simulation

Test with the sample network persona (25 Mbps home broadband):

```bash
./abrsim --persona sample_network.json --duration 600 --out first_run.csv
```

This simulates 10 minutes (600 seconds) of playback and completes in ~1 second.

### 3. View Results

```bash
# Quick summary (shown at end of simulation)
# Check final line of output for statistics

# Detailed event log
head -20 first_run.csv
```

## Testing Different Network Conditions

### Mobile 3G (Challenging)

```bash
./abrsim --persona personas/mobile_3g.json --duration 3600 --out mobile.csv
```

### Gigabit Fiber (Ideal)

```bash
./abrsim --persona personas/fiber_gigabit.json --duration 3600 --out fiber.csv
```

### Congested WiFi (Variable)

```bash
./abrsim --persona personas/wifi_congested.json --duration 3600 --out wifi.csv
```

## Testing ABR Changes

### Workflow

1. **Baseline run** with current ABR logic:
   ```bash
   ./abrsim --persona sample_network.json --duration 3600 --out baseline.csv --seed 12345
   ```

2. **Modify ABR logic** in `abrsim.cpp` (see `makeABRDecision()` function)

3. **Rebuild**:
   ```bash
   ./build.sh
   ```

4. **Test run** with same seed for reproducibility:
   ```bash
   ./abrsim --persona sample_network.json --duration 3600 --out modified.csv --seed 12345
   ```

5. **Compare results**:
   ```bash
   ./compare_results.py baseline.csv modified.csv
   ```

### Example Comparison Output

```
=== Comparison ===
Download count change: +0
Profile changes: +5
Rebuffer events: -2
Avg throughput change: +0.12 Mbps
Final buffer change: +3.45 seconds

=== Quality of Experience ===
✓ IMPROVEMENT: Fewer rebuffering events
⚠️  NOTE: Significantly different profile switching behavior
```

## Multi-Hour Testing

Simulate a full evening of streaming (4 hours):

```bash
./abrsim --persona sample_network.json --duration 14400 --out evening.csv
```

Typical execution time: 5-10 seconds  
Speed-up factor: ~1000x to 3000x real-time

## Interpreting Results

### CSV Column Reference

- **time_s**: Simulated playback time in seconds
- **event_type**: `download`, `profile_change`, `rebuffer_start`, `rebuffer_end`
- **profile_idx**: Current video quality profile (0=lowest, 6=highest)
- **download_ms**: Segment download time in milliseconds
- **throughput_bps**: Measured throughput in bytes/second
- **buffer_s**: Player buffer level in seconds
- **description**: Human-readable event description

### Key Metrics to Watch

1. **Rebuffer Events**: Should be minimized (0 is ideal)
2. **Profile Changes**: Too many = unstable, too few = not adapting
3. **Buffer Level**: Should stay above 5 seconds typically
4. **Average Profile**: Higher is better (if stable)

### Profile Ladder Reference

```
Profile 0: 235 kbps  (240p)  - Emergency fallback
Profile 1: 375 kbps  (360p)  - Low quality
Profile 2: 750 kbps  (480p)  - SD quality
Profile 3: 1.4 Mbps  (720p)  - HD ready
Profile 4: 2.8 Mbps  (1080p) - Full HD
Profile 5: 5.0 Mbps  (1080p) - High quality
Profile 6: 8.0 Mbps  (4K)    - Ultra HD
```

## Common Use Cases

### 1. Validate ABR Under Constraints

Test if ABR properly handles low bandwidth:

```bash
./abrsim --persona personas/mobile_3g.json --duration 1800 --out lowbw_test.csv
```

Expected: Should ramp down to profiles 0-2 and avoid rebuffering.

### 2. Test High-Speed Performance

Verify ABR ramps up quickly on fast connections:

```bash
./abrsim --persona personas/fiber_gigabit.json --duration 600 --out highspeed.csv
```

Expected: Should quickly reach profiles 5-6 and stay there.

### 3. Stress Test Variability

Test ABR stability under fluctuating conditions:

```bash
./abrsim --persona personas/wifi_congested.json --duration 3600 --out stress.csv
```

Expected: Moderate profile switching, minimal rebuffering.

### 4. Long-Duration Stability

Verify no drift or anomalies over extended playback:

```bash
./abrsim --persona sample_network.json --duration 28800 --out 8hour.csv
```

Expected: Consistent behavior throughout entire 8-hour simulation.

## Troubleshooting

### Build Fails

```bash
# Ensure C++17 compiler
g++ --version  # Should be GCC 7+ or Clang 5+

# Try explicit C++ standard
g++ -std=c++17 -O2 -o abrsim abrsim.cpp
```

### Simulation Crashes

- Check persona JSON is valid
- Verify duration is positive
- Ensure output file path is writable

### Unexpected Results

- Use same `--seed` value for reproducibility
- Check persona parameters are reasonable
- Verify profile ladder matches expectations

## Next Steps

After validating basic functionality:

1. **Integrate real ABRManager** (see README.md "Future Enhancements")
2. **Add custom profile ladders** from actual manifests
3. **Implement mid-stream network changes**
4. **Add statistical analysis** and visualization tools

## Getting Help

For questions or issues:

1. Check full [README.md](README.md) for detailed documentation
2. Review ABR algorithm in `abrsim.cpp` (search for `makeABRDecision`)
3. Examine sample personas in `personas/` directory
4. Run with `--help` for all options

Happy testing! 🎬
