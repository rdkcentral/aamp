# Flight Data Recorder Implementation Summary

## Overview
Implemented Phase 1 of the Flight Data Recorder (FDR) system for AAMP logging as specified in VPAAMP-506.

## Implementation Date
July 16, 2026

## Files Created

### 1. AampFlightDataRecorder.h
- Header file defining the `AampFlightDataRecorder` class
- Defines `FDRLogEntry` struct for storing log entries
- Singleton pattern for global FDR instance
- Lock-free circular buffer design using atomic operations

### 2. AampFlightDataRecorder.cpp
- Implementation of the FDR class
- Mutex-protected `AddEntry()` preserving complete string entries and chronological order
- Time-based eviction driven by a maintenance thread plus count-based eviction
- `Dump()` and `Flush()` emit all buffered entries and leave the recorder empty
- Runtime-safe reconfiguration of enabled state, line count, and retention time
- Thread-safe operations throughout

## Files Modified

### 1. AampConfig.h
- Added `eAAMPConfig_EnableFlightDataRecorder` (boolean) - default: true
- Added `eAAMPConfig_FlightDataRecorderMaxLines` (int) - default: 5000
- Added `eAAMPConfig_FlightDataRecorderMaxSeconds` (int) - default: 15

### 2. AampConfig.cpp
- Added FDR config entries to lookup tables:
  - `mConfigLookupTableBool`: "enableFlightDataRecorder"
  - `mConfigLookupTableInt`: "flightDataRecorderMaxLines", "flightDataRecorderMaxSeconds"
- Modified `ConfigureLogSettings()` to initialize FDR with config values
- Included `AampFlightDataRecorder.h`

### 3. aamplogging.cpp
- Included `AampFlightDataRecorder.h`
- Modified `logprintf()` function:
  - Captures user message using `va_copy`
  - Queues INFO, WARN, and MILESTONE when FDR is active
  - Emits and clears the FDR buffer BEFORE logging ERROR
  - Bypasses FDR for INFO-level developer logging or when FDR is disabled
- Routes FDR output through the configured journal or Ethanlog backend

### 4. Middleware logging
- Registers structured logger callbacks with middleware-player-interface
- Routes `PLAYER_IF` logs through the same FDR singleton
- Uses an admission callback so normally discarded TRACE/DEBUG logs are not formatted or forwarded
- Uses the temporary RDKEMW-23400 macOS patch until the middleware API is released

### 5. CMakeLists.txt
- Added `AampFlightDataRecorder.cpp` and `AampFlightDataRecorder.h` to build

## Key Features Implemented

### 1. Lock-Free Circular Buffer
- Uses `std::atomic<size_t>` for head, tail, and count
- No mutex locks - minimal performance impact
- Thread-safe concurrent access from multiple logging threads

### 2. Dual Eviction Strategy
- **Time-based**: Removes entries older than configured seconds (default: 15s)
- **Count-based**: Maintains maximum number of entries (default: 5000)
- Eviction runs on every `AddEntry()` call

### 3. Full Dump Trigger Behavior
- Only ERROR and tune completion emit the full ring buffer.
- Full dumps preserve chronological order and leave the ring buffer empty.
- WARN and MILESTONE are emitted individually when age- or count-evicted.

### 4. Captured Log Levels
- INFO, WARN, and MILESTONE
- ERROR triggers a full dump and is then emitted normally
- TRACE and DEBUG are NOT captured

### 5. Configuration
- Fully configurable via AampConfig
- Can be enabled/disabled at runtime
- Adjustable buffer size and time window

### 6. Unified Logging
- Core AAMP and middleware logging use a single shared FDR instance
- Source tags preserve `AAMP-PLAYER` and `PLAYER_IF` tool compatibility

## Output Format

When ERROR occurs, FDR dumps in this format:

```
[FDR] FLIGHT DATA RECORDER DUMP (triggered by AAMP-PLAYER ERROR) 1234 entries from last 15s
1721134567.123: [AAMP-PLAYER][042][0][INFO][7f8a2c001700][Tune][100]Log message 1
1721134567.456: [AAMP-PLAYER][043][0][WARN][7f8a2c001700][Tune][101]Log message 2
...
[FDR] END FLIGHT DATA RECORDER DUMP
```

## Configuration Options

### Via aamp.cfg or JSON config:
```json
{
  "enableFlightDataRecorder": true,
  "flightDataRecorderMaxLines": 5000,
  "flightDataRecorderMaxSeconds": 15
}
```

### Via environment (if supported):
```bash
export AAMP_enableFlightDataRecorder=true
export AAMP_flightDataRecorderMaxLines=5000
export AAMP_flightDataRecorderMaxSeconds=15
```

## Memory Usage

- Fixed ring capacity (number of entries): configured by `flightDataRecorderMaxLines`
- `std::string` fields may allocate during `AddEntry()` depending on message/source length
- Entries are created at initialization, but message storage is not fixed-size

## Thread Safety

- Lock-free atomic operations for head/tail/count
- `va_copy` used to safely capture log messages
- Recursion guard prevents FDR from dumping itself
- Safe for concurrent access from multiple threads

## Performance Impact

- Ring operations are serialized to prevent races on string entries
- Eviction is typically O(1), with a maintenance thread enforcing the time bound
- Only active for INFO+ logs (TRACE/DEBUG skipped)

## Phase 1 Scope (Completed)

✅ Core FDR infrastructure
✅ ERROR-only trigger
✅ AAMP core integration
✅ Configuration via AampConfig
✅ Thread-safe circular buffer
✅ Time + line count limits
✅ Flush and continue after dump

## Phase 2 Scope (Future)

⏳ Player Analytics integration
⏳ Event-based error reporting to JavaScript
⏳ Compression/sampling for large buffers
⏳ Per-module filtering

## Testing Recommendations

1. **Unit Tests**:
   - Test circular buffer wraparound
   - Test time-based eviction
   - Test line-count eviction
   - Test thread safety with concurrent adds
   - Test dump formatting

2. **Integration Tests**:
   - Trigger ERROR in playback, verify FDR dump appears
   - Verify INFO logs captured even when log level is WARN
   - Verify buffer size limits respected
   - Test multiple errors in same session

3. **Manual Testing**:
   - Induce playback failure, check logs for FDR context
   - Verify performance impact is minimal
   - Test configuration changes

## Known Limitations

1. Message truncation: User messages capped at 2048 bytes
2. No compression: Large buffers consume full memory
3. No persistence: Buffer lost on process restart
4. No per-module filtering: Captures all INFO+ logs

## Related Work

- **WARN/ERROR Audit**: Separate task to review inappropriate WARN usage
- **Log Compaction**: Separate task to remove redundant logs
- **Analytics Integration**: Phase 2 feature for analytics system

## Build Integration

The FDR is automatically built as part of the AAMP library. No special build flags required.

## Backward Compatibility

- Fully backward compatible
- Can be disabled via config without code changes
- No breaking changes to existing logging APIs

## References

- JIRA: VPAAMP-506
- Implementation branch: feature/VPAAMP-506
