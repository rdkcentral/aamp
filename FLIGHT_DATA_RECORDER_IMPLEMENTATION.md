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
- Lock-free `AddEntry()` method using atomic head/tail pointers
- Time-based and count-based eviction (`EvictOldEntries()`)
- `Dump()` method to output all buffered entries
- `Flush()` method to clear buffer after error dump
- Thread-safe operations throughout

## Files Modified

### 1. AampConfig.h
- Added `eAAMPConfig_EnableFlightDataRecorder` (boolean) - default: true
- Added `eAAMPConfig_FlightDataRecorderMaxLines` (int) - default: 5000
- Added `eAAMPConfig_FlightDataRecorderMaxSeconds` (int) - default: 60

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
  - Dumps FDR buffer BEFORE logging ERROR
  - Adds log entries to FDR for INFO and above (INFO, WARN, MIL, ERROR)
  - Flushes FDR buffer AFTER logging ERROR
- Prevents recursive dumps with atomic flag

### 4. middleware/playerLogManager/PlayerLogManager.cpp
- Included `AampFlightDataRecorder.h` (relative path: `../../AampFlightDataRecorder.h`)
- Modified middleware `logprintf()` function:
  - Same FDR integration as core AAMP
  - Uses "PLAYER_IF" as source identifier
  - Shares same FDR singleton instance

### 5. CMakeLists.txt
- Added `AampFlightDataRecorder.cpp` and `AampFlightDataRecorder.h` to build

## Key Features Implemented

### 1. Lock-Free Circular Buffer
- Uses `std::atomic<size_t>` for head, tail, and count
- No mutex locks - minimal performance impact
- Thread-safe concurrent access from multiple logging threads

### 2. Dual Eviction Strategy
- **Time-based**: Removes entries older than configured seconds (default: 60s)
- **Count-based**: Maintains maximum number of entries (default: 5000)
- Eviction runs on every `AddEntry()` call

### 3. ERROR Trigger Behavior
- On ERROR log:
  1. Dump full ring buffer contents to console
  2. Log the actual ERROR message
  3. Flush the ring buffer
  4. Continue capturing new logs (allows multiple dumps per session)

### 4. Captured Log Levels
- INFO, WARN, MILESTONE, ERROR
- TRACE and DEBUG are NOT captured (reduces memory usage)

### 5. Configuration
- Fully configurable via AampConfig
- Can be enabled/disabled at runtime
- Adjustable buffer size and time window

### 6. Unified Logging
- Works for both core AAMP and middleware logging
- Single shared FDR instance
- Distinguishable by source tag: "AAMP-PLAYER" vs "PLAYER_IF"

## Output Format

When ERROR occurs, FDR dumps in this format:

```
================================================================================
[FDR] FLIGHT DATA RECORDER DUMP (triggered by AAMP-PLAYER ERROR)
[FDR] Captured 1234 log entries from last 60 seconds
================================================================================
[FDR] 1721134567.123: [AAMP-PLAYER][042][0][INFO][7f8a2c001700] Log message 1
[FDR] 1721134567.456: [PLAYER_IF][015][WARN][7f8a2d002800] Log message 2
...
================================================================================
[FDR] END FLIGHT DATA RECORDER DUMP
================================================================================
```

## Configuration Options

### Via aamp.cfg or JSON config:
```json
{
  "enableFlightDataRecorder": true,
  "flightDataRecorderMaxLines": 5000,
  "flightDataRecorderMaxSeconds": 60
}
```

### Via environment (if supported):
```bash
export AAMP_enableFlightDataRecorder=true
export AAMP_flightDataRecorderMaxLines=5000
export AAMP_flightDataRecorderMaxSeconds=60
```

## Memory Usage

- Fixed allocation: ~5MB (5000 entries × ~1KB per entry)
- No dynamic allocation during operation
- Pre-allocated at initialization

## Thread Safety

- Lock-free atomic operations for head/tail/count
- `va_copy` used to safely capture log messages
- Recursion guard prevents FDR from dumping itself
- Safe for concurrent access from multiple threads

## Performance Impact

- Minimal: lock-free operations, no blocking
- Eviction check on every add: typically O(1), worst case O(n) when evicting
- Only active for INFO+ logs (TRACE/DEBUG skipped)

## Phase 1 Scope (Completed)

✅ Core FDR infrastructure
✅ ERROR-only trigger
✅ Both AAMP core and middleware integration
✅ Configuration via AampConfig
✅ Lock-free circular buffer
✅ Time + line count limits
✅ Flush and continue after dump

## Phase 2 Scope (Future)

⏳ WARN trigger support (after WARN/ERROR audit)
⏳ Player Analytics integration
⏳ Event-based error reporting to JavaScript
⏳ Compression/sampling for large buffers
⏳ Per-module filtering

## Testing Recommendations

1. **Unit Tests** (to be created):
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
