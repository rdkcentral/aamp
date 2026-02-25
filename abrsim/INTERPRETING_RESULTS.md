# Interpreting ABR Simulator Results

## Understanding the Output

When you run the simulator, you'll see results like this:

```
Simulation completed in 0.002 seconds (real time)
Simulated 3601.2 seconds of playback
Segments downloaded: 1937
Speed-up factor: 1981634.2x

=== Simulation Summary ===
Total duration: 3601.2 seconds
Total events: 1941
Segment downloads: 1937
Profile changes: 3
Rebuffer events: 1

Buffer Statistics:
  Rebuffer events: 1
  Total rebuffer time: 2.08 seconds
  Final buffer level: 275.07 seconds  [VOD] or 7.99 seconds [Live]

[Live Mode Only]
Live Streaming Latency:
  Target latency: 8.00 seconds
  Average latency: 15.89 seconds
  Min latency: 8.00 seconds
  Max latency: 16.00 seconds
  Latency drift: +7.89 seconds (poor)
```

## Metric Explanations

### Basic Metrics

**Total duration**: 
- Time simulated (should match your `--duration` parameter)
- May run slightly over to complete the final segment

**Segment downloads**:
- Number of video segments fetched
- Each segment = 2 seconds of video by default
- Expected: ~duration/2 segments (e.g., 3600s ÷ 2 = 1800 segments)
- Actual may vary based on rebuffering

**Profile changes**:
- Number of times ABR switched bitrates
- **Low number** (1-5): Stable network or quick convergence
- **High number** (20+): Unstable network or poor ABR tuning
- Includes both upshifts (better quality) and downshifts (worse quality)

**Rebuffer events**:
- Number of times playback stalled due to empty buffer
- **0-1**: Excellent (startup rebuffer only)
- **2-5**: Acceptable
- **10+**: Poor - network too slow or ABR too aggressive

**Total rebuffer time**:
- Total seconds of playback interruption
- **< 5s**: Good user experience
- **5-30s**: Noticeable degradation
- **> 30s**: Unacceptable for most use cases

**Speed-up factor**:
- How much faster than real-time the simulation ran
- Typically 500,000x to 2,000,000x
- Allows multi-hour tests to run in milliseconds

### Final Buffer Level

**This metric shows how much video is buffered at the end of simulation.**

#### VOD Mode (Buffer Capped at --max-buffer)

```
Final buffer level: 20.00 seconds
```

**What it means**:
- At end of simulation, 20 seconds of video buffered ahead
- Capped at `--max-buffer` parameter (default: 20s)
- **Realistic** for actual VOD players with memory constraints
- If buffer < max, network couldn't keep up with cap

**Why buffer cap matters**:
- **Memory usage**: Each second of buffer uses memory (video frames)
- **Startup time**: Larger buffer = longer initial buffering
- **Bandwidth efficiency**: Don't download too far ahead
- **Typical values**: 10-30 seconds for VOD

**Examples**:
```
--max-buffer 10:  Final buffer = 10.00s  [Lower memory, higher rebuffer risk]
--max-buffer 20:  Final buffer = 20.00s  [Balanced - recommended default]  
--max-buffer 40:  Final buffer = 40.00s  [Higher memory, more resilient]
```

If final buffer < max-buffer (e.g., 15.5s with 20s cap), the network couldn't sustain filling to the cap.

#### Live Mode (Buffer Capped)

```
Final buffer level: 7.99 seconds

Live Streaming Latency:
  Target latency: 8.00 seconds
  Average latency: 15.89 seconds
```

**What it means**:
- Buffer is capped at target latency (8 seconds)
- Player maintains ~8 seconds of content buffered
- **Realistic** for live streaming scenarios
- Simulates staying near the live edge

**Final buffer ≈ target latency** is expected and correct.

### Live Streaming Metrics (--live mode only)

**Target latency**:
- Your configured distance from live edge (`--target-latency`)
- Typical values: 4-12 seconds
- Lower = more responsive but more rebuffering risk
- Higher = more stable but less "live"

**Average latency**:
- Actual average distance from live edge during simulation
- Should be close to target latency

**Min/Max latency**:
- Range of latency variation
- Narrow range = stable
- Wide range = unstable

**Latency drift**:
- Difference between average and target
- **< ±1s**: Good - well-tuned
- **±1-3s**: Acceptable - minor drift
- **> ±3s**: Poor - needs ABR tuning
- **Negative**: Falling behind live edge (rebuffering likely)
- **Positive**: Running ahead of target (using extra bandwidth)

## Example Comparisons

### Good Result (Fiber Network, Live Mode)
```
Segments downloaded: 1870
Profile changes: 0 (stayed at high bitrate)
Rebuffer events: 1 (startup only)
Total rebuffer time: 0.93 seconds
Final buffer level: 8.00 seconds
Average latency: 8.50 seconds
Latency drift: +0.50 seconds (good)
```
✓ Minimal rebuffering  
✓ Stable bitrate  
✓ Buffer capped correctly  
✓ Latency drift under control

### Poor Result (Slow Network, VOD Mode)
```
Segments downloaded: 1200
Profile changes: 45
Rebuffer events: 87
Total rebuffer time: 420 seconds
Final buffer level: 0.15 seconds
```
✗ Heavy rebuffering (420s of stalls!)  
✗ Constant bitrate switching (45 changes)  
✗ Network too slow for lowest profile

### VOD vs Live Comparison (Same Network)
```
VOD:  Final buffer level: 275.07 seconds  [UNREALISTIC]
Live: Final buffer level: 7.99 seconds    [REALISTIC]
```
The live mode correctly caps the buffer, preventing unrealistic accumulation.

## When to Use Each Mode

### Use VOD Mode When:
- Testing on-demand content behavior
- Analyzing ABR performance without live edge constraints
- Debugging basic ABR logic
- You don't care about buffer limits

### Use Live Mode When:
- Testing live streaming scenarios
- Validating latency management
- Simulating real live stream constraints
- You need realistic buffer behavior
- **This is critical for live streaming applications**

## CSV Output Format

The `--out` file contains detailed event logs:

```csv
time_s,event_type,profile_idx,download_ms,throughput_bps,buffer_s,description
2.015,download,3,2015.39,1499039,2.00,Profile 1400 kbps, Latency: 7s
11.851,profile_change,4,0.00,0,15.08,1400 -> 2800 kbps
```

**Columns**:
- `time_s`: Simulation time when event occurred
- `event_type`: `download`, `profile_change`, `rebuffer_start`
- `profile_idx`: Index into profile ladder (0=lowest, 6=highest)
- `download_ms`: Download time in milliseconds
- `throughput_bps`: Measured throughput in bits per second
- `buffer_s`: Buffer level after event
- `description`: Human-readable event details

## Key Takeaways

1. **Final buffer level** in VOD mode can be misleadingly large - use Live mode for realistic results
2. **Live mode** enforces target latency as a buffer cap - this is correct behavior
3. **Rebuffer count** is more important than rebuffer time for QoE
4. **Profile changes** should be infrequent (stable ABR) but responsive (adapts to network)
5. **Latency drift** in live mode shows how well ABR maintains target latency
6. Use `--seed` for reproducible comparisons when tuning ABR logic
