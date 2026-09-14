# Flight Data Recorder - Build Fix

## Issue
Build error when compiling unit tests:
```
Undefined symbols for architecture arm64:
  "AampFlightDataRecorder::Initialize(bool, unsigned long, unsigned long long)"
  "AampFlightDataRecorder::GetInstance()"
ld: symbol(s) not found for architecture arm64
```

## Root Cause
The `AampConfig.cpp` file now depends on `AampFlightDataRecorder.cpp` (calls `Initialize()` and `GetInstance()` in `ConfigureLogSettings()`). Any unit test that includes `AampConfig.cpp` must also include `AampFlightDataRecorder.cpp` in its build.

## Files Fixed

### 1. test/utests/tests/ConfigTests/CMakeLists.txt
**Added:**
```cmake
set(FDR_SOURCE ${AAMP_ROOT}/AampFlightDataRecorder.cpp)

add_executable(${EXEC_NAME}
               ${TEST_SOURCES}
               ${FAKE_SOURCE}
               ${FDR_SOURCE})
```

### 2. test/utests/tests/AampDrmLegacy/CMakeLists.txt
**Added to AAMP_SOURCES:**
```cmake
${AAMP_ROOT}/AampFlightDataRecorder.cpp
```

### 3. test/utests/tests/AampDrmSecureClient/CMakeLists.txt
**Added to AAMP_SOURCES:**
```cmake
${AAMP_ROOT}/AampFlightDataRecorder.cpp
```

### 4. test/utests/tests/DrmOcdm/CMakeLists.txt
**Added to AAMP_SOURCES:**
```cmake
${AAMP_ROOT}/AampFlightDataRecorder.cpp
```

### 5. test/utests/tests/AampLogManagerTests/CMakeLists.txt
**Modified AAMP_SOURCES:**
```cmake
set(AAMP_SOURCES ${AAMP_ROOT}/aamplogging.cpp
                 ${AAMP_ROOT}/AampFlightDataRecorder.cpp)
```

### 6. middleware/playerLogManager/CMakeLists.txt
**Modified PlayerLogManager_SRC:**
```cmake
set(PlayerLogManager_SRC PlayerLogManager.cpp
                         ../../AampFlightDataRecorder.cpp)
```

## Verification

After these changes, all tests and libraries should build successfully:

**Unit Tests:**
- ConfigTests
- AampDrmLegacy
- AampDrmSecureClient
- DrmOcdm
- AampLogManagerTests

**Shared Libraries:**
- libplayerlogmanager.dylib (middleware logging library)

## Future Considerations

If you create a new unit test that includes any of these files, you **must** also include `AampFlightDataRecorder.cpp`:
- `AampConfig.cpp` (depends on FDR for initialization)
- `aamplogging.cpp` (depends on FDR for log capture and dump)
- `middleware/playerLogManager/PlayerLogManager.cpp` (depends on FDR for log capture and dump)

## Build Command

To rebuild unit tests after these changes:
```bash
cd test/utests
./run.sh
```

This will build and run all unit test suites. The L1 workflow handles the CMake configuration and build automatically.
