# Flight Data Recorder - Developer Guide

## Quick Start

The Flight Data Recorder (FDR) is now automatically integrated into AAMP logging. No code changes are needed to use it.

## How It Works

1. **Normal Operation**: FDR silently captures INFO, WARN, and MILESTONE logs in a ring buffer when normal logging is above INFO

2. **On ERROR or tune completion**:
   - FDR emits the last 15 seconds (or configured line limit) in chronological order
   - The ring buffer is left empty
   - An ERROR trigger is then logged normally

3. **Result**: When debugging production issues, you get historical context leading up to the error

## Configuration

### Enable/Disable FDR
```json
{
  "enableFlightDataRecorder": true
}
```

### Adjust Buffer Size
```json
{
  "flightDataRecorderMaxLines": 5000,
  "flightDataRecorderMaxSeconds": 15
}
```

### Via aamp.cfg
```
enableFlightDataRecorder=true
flightDataRecorderMaxLines=5000
flightDataRecorderMaxSeconds=15
```

## Example Output

When an ERROR occurs, you'll see:

```
[FDR] FLIGHT DATA RECORDER DUMP (triggered by AAMP-PLAYER ERROR) 342 entries from last 15s
1721134567.123: [AAMP-PLAYER][042][0][INFO][7f8a2c001700][Tune][100]Tuning to URL: https://...
1721134567.456: [AAMP-PLAYER][043][0][WARN][7f8a2c001700][Tune][101]Network latency high: 250ms
1721134568.789: [AAMP-PLAYER][044][0][INFO][7f8a2c001700][Tune][102]Manifest downloaded
...
[FDR] END FLIGHT DATA RECORDER DUMP
[AAMP-PLAYER][045][0][ERROR][7f8a2c001700][Tune][103]Fragment download failed: HTTP 404
```

## Debugging Tips

### 1. Analyzing FDR Dumps

Look for patterns in the dumped logs:
- Network issues before the error
- State transitions that led to failure
- Timing information (gaps between events)
- Thread IDs to track concurrent operations

### 2. Adjusting Buffer Size

For longer debugging sessions:
```json
{
  "flightDataRecorderMaxSeconds": 120,
  "flightDataRecorderMaxLines": 10000
}
```

For memory-constrained environments:
```json
{
  "flightDataRecorderMaxSeconds": 30,
  "flightDataRecorderMaxLines": 2000
}
```

### 3. Filtering FDR Output

Use grep to extract specific information:
```bash
# Extract only WARN and ERROR from FDR dump
grep -E '\[FDR\].*\[(WARN|ERROR)\]' aamp.log

# Extract logs from specific thread
grep '\[FDR\].*\[7f8a2c001700\]' aamp.log

```

## Performance Considerations

### Memory Usage
- Default: ~5MB per player instance
- Scales with `flightDataRecorderMaxLines` × ~1KB per line
- Fixed allocation - no runtime growth

### CPU Impact
- Ring operations are serialized to preserve entry integrity and chronological order
- A maintenance thread expires entries at the configured time boundary
- Eviction is typically O(1)

### When to Disable
Disable FDR if:
- Memory is extremely constrained (<10MB available)
- Logging performance is critical (rare)
- You're doing performance profiling of logging itself

## Common Issues

### Issue: FDR not dumping on ERROR

**Check:**
1. Is FDR enabled? `grep enableFlightDataRecorder aamp.cfg`
2. Are you logging at ERROR level? `AAMPLOG_ERR()` or `MW_LOG_ERR()`
3. Confirm the recorder contains entries; empty buffers do not emit dump markers

### Issue: FDR dumps are empty

**Check:**
1. Are you logging at INFO or above? (TRACE/DEBUG are not captured)
2. Is the error happening immediately after startup? (buffer may be empty)
3. Check buffer size configuration

### Issue: FDR dumps are truncated

**Increase buffer size:**
```json
{
  "flightDataRecorderMaxLines": 10000,
  "flightDataRecorderMaxSeconds": 120
}
```

## Integration with Existing Tools

### Player Analytics Integration (Phase 2)
Future enhancement will automatically send FDR dumps to analytics system for analysis.

### Log Harvesting
FDR dumps are part of normal log output and will be harvested automatically.

### Remote Debugging
FDR provides context even when remote systems have limited logging enabled.

## Best Practices

### 1. Don't Rely on FDR for Normal Logging
- FDR is for post-error diagnostics
- Use appropriate log levels for normal operation
- Don't log excessively just because FDR captures it

### 2. Use Meaningful Log Messages
Since FDR captures INFO logs, make them descriptive:
```cpp
// Good
AAMPLOG_INFO("Manifest downloaded: %d bytes, bitrate: %d", size, bitrate);

// Bad
AAMPLOG_INFO("Done");
```

### 3. Avoid Logging Sensitive Data
FDR captures logs in memory and dumps them on error:
```cpp
// Bad - exposes credentials
AAMPLOG_INFO("Auth token: %s", token);

// Good
AAMPLOG_INFO("Auth token length: %d", strlen(token));
```

### 4. Test Error Paths
Verify that your error paths trigger FDR dumps:
```cpp
if (downloadFailed)
{
    AAMPLOG_ERR("Download failed: %s", url);  // This will trigger FDR dump
}
```

## API Reference

### For Most Developers: No API Needed
FDR works automatically with existing logging macros:
- `AAMPLOG_INFO()`, `AAMPLOG_WARN()`, `AAMPLOG_MIL()`, `AAMPLOG_ERR()` (core AAMP)
- `MW_LOG_INFO()`, `MW_LOG_WARN()`, `MW_LOG_MIL()`, `MW_LOG_ERR()` (middleware)

### Advanced: Direct FDR Access (Rare)

If you need direct access to FDR (not recommended):

```cpp
#include "AampFlightDataRecorder.h"

// Get singleton instance
AampFlightDataRecorder& fdr = AampFlightDataRecorder::GetInstance();

// Check if enabled
if (fdr.IsEnabled()) {
    // FDR is active
}

// Manual dump (not recommended - use ERROR logging instead)
fdr.Dump(eLOGLEVEL_ERROR, "CUSTOM_SOURCE");

// Manual full emit-and-clear flush (normally used for ERROR or tune completion)
fdr.Flush(eLOGLEVEL_MIL, "CUSTOM_SOURCE");
```

## Troubleshooting

### Build Errors

If you see undefined reference to `AampFlightDataRecorder`:
1. Verify `AampFlightDataRecorder.cpp` is in CMakeLists.txt
2. Clean and rebuild unit tests: `cd test/utests && ./run.sh`

### Runtime Errors

If FDR crashes or behaves unexpectedly:
1. Check for stack overflow (FDR uses `alloca` for temporary buffers)
2. Verify thread safety (use thread sanitizer: `-fsanitize=thread`)
3. Check memory usage (use memory sanitizer: `-fsanitize=address`)

## Future Enhancements (Phase 2)

- **Analytics Integration**: Automatic upload to analytics system
- **Compression**: Reduce memory footprint
- **Sampling**: Capture every Nth log to extend time window
- **Per-Module Filtering**: Capture only specific components

## Support

For issues or questions:
1. Check JIRA: VPAAMP-506
2. Review implementation plan: `.windsurf/plans/flight-data-recorder-595b6f.md`
3. Contact: AAMP team

## Change History

### 2026-07-16 - Phase 1 Implementation
- Initial FDR implementation
- ERROR-only trigger
- Core AAMP integration
- Configuration support
- Lock-free circular buffer
