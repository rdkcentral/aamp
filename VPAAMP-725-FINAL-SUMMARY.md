# VPAAMP-725: Simulator Builds Using External Middleware Repository

## Executive Summary

Successfully migrated AAMP simulator builds (macOS and Ubuntu) from the deprecated internal `aamp/middleware` folder to the external `middleware-player-interface` repository. All L2 tests now passing with equivalent performance to the internal middleware baseline.

**Status**: ✅ **COMPLETE** - All builds passing, L2 tests passing

---

## Problem Statement

AAMP simulator builds were using a stale, unmaintained copy of middleware code in `aamp/middleware/`. This caused:
- Code divergence between simulator and production builds
- Difficulty maintaining macOS/Ubuntu simulator support
- No clear ownership of middleware build issues
- Inability to leverage upstream middleware improvements

---

## Solution Overview

### 1. External Middleware Integration

**Commit**: `bd2b3b1` (June 1, 2026)

**Why this specific commit?**
- ✅ Latest commit **before** the `seekPausedState` regression (added June 4, reverted internally June 16)
- ✅ Contains all modern API signatures required by current AAMP code
- ✅ Compatible with build fixes and `setPtsOffset` patch
- ❌ Does NOT contain `seekPausedState` logic that caused L2 test timeouts

**Key Changes**:
- `scripts/install_options.sh`: Pin to commit `bd2b3b1`
- `scripts/install_middleware_interfaces.sh`: 
  - Auto-checkout pinned commit for sibling repos
  - Detect commit changes and trigger rebuilds
  - Apply comprehensive build fixes patch
  - Improved error handling and diagnostics

### 2. Build Fixes Patch

**File**: `OSX/patches/middleware-fixes-4c1d90a.patch`

**Fixes Applied**:
1. **gstreamer-base-1.0 dependency** (affects all platforms)
   - Commit `bd2b3b1` incorrectly uses `gstreamer-app-1.0` as `GSTREAMERBASE`
   - Missing `gst_base_transform_*` symbols cause runtime failures
   
2. **OpenSSL include directories** (affects all platforms)
   - Original code only included OpenSSL headers on Linux
   - macOS builds failed with missing `<openssl/evp.h>`

3. **GStreamer link directories** (macOS only)
   - Missing link paths for GStreamer framework libraries

4. **UUID library** (macOS only)
   - Missing pkg-config and link directories for UUID

5. **setPtsOffset implementation** (affects all platforms)
   - Required for HLS PTS restamping with subtitles
   - Called from `streamabstraction.cpp` and `fragmentcollector_hls.cpp`

### 3. AAMP Build System Updates

**File**: `CMakeLists.txt`

**Change**: Added `/Library/Frameworks` to `CMAKE_BUILD_RPATH` and `CMAKE_INSTALL_RPATH`

**Reason**: Fixes `aampcli` runtime crash on macOS:
```
dyld[85677]: Library not loaded: @rpath/GStreamer.framework/Versions/1.0/lib/GStreamer
```

The middleware's `libplayergstinterface.dylib` links against GStreamer framework, but AAMP executables didn't have the framework path in their RPATH.

---

## Root Cause Analysis: seekPausedState Regression

### Timeline

- **June 4, 2026**: External middleware adds `seekPausedState` to fix video stuck issue (commit `2e0003c`)
- **June 16, 2026**: **Internal AAMP middleware REVERTS it** (commit `a92f4ac6`) due to regressions
- **June 26, 2026**: Commit `a55c02d` created with `setPtsOffset` - still contains problematic `seekPausedState`

### Impact

The `seekPausedState` logic defers the PLAYING state transition during pipeline configuration:

```cpp
if (interfacePlayerPriv->gstPrivateContext->seekPausedState)
{
    MW_LOG_WARN("seekPausedState active - deferring transition to PLAYING");
    interfacePlayerPriv->gstPrivateContext->buffering_target_state = GST_STATE_PLAYING;
    interfacePlayerPriv->gstPrivateContext->pendingPlayState = true;
    // Pipeline stays in PAUSED instead of transitioning to PLAYING
}
```

**Result**: 
- Slower playback startup
- L2 test timeouts (30.39s vs 30s limit)
- Consistent performance degradation across all tests

### Why Tests Failed

This wasn't "just timing variance" - it was a **real functional regression**:
- `dev_sprint_25_2` with internal middleware: ✅ All tests pass
- `feature/VPAAMP-725` with `a55c02d`: ❌ Consistent timeouts

The internal middleware team had already identified and reverted this issue, but the external repo still contained it.

---

## Testing Results

### macOS Build
- ✅ Clean build with no compilation errors
- ✅ `aampcli` runs without dyld crashes
- ✅ L1 tests pass

### Ubuntu Build (GitHub Actions)
- ✅ Clean build with no compilation errors
- ✅ All L2 tests passing
- ✅ Performance equivalent to `dev_sprint_25_2` baseline

### Test Coverage
- L1 unit tests
- L2 integration tests:
  - `AAMP-LLD-1202`: Chunk Injection
  - `AAMP-CONFIG-2036`: Init Fragment Retry
  - `AAMP-CONFIG-2052`: Low Latency DASH
  - (All previously failing tests now pass)

---

## Related Tickets

### RDKEMW-22371: Fix Middleware Compilation Issues on OSX

**Status**: Documented, awaiting upstream submission

**Summary**: The build fixes in `middleware-fixes-4c1d90a.patch` should be submitted upstream to the `middleware-player-interface` repository.

**Files for Upstream**:
- `OSX/patches/middleware-fixes-4c1d90a.patch` - The actual fixes
- `OSX/patches/middleware-build-fixes-summary.md` - Detailed explanation
- `OSX/patches/RDKEMW-22371-ticket-summary.md` - Jira ticket summary

**Benefits**:
- Future middleware commits won't need patching
- macOS builds supported out-of-the-box
- Build fixes benefit all middleware consumers

**Next Steps**:
1. Create PR to `middleware-player-interface` with fixes
2. Once merged, update AAMP to use unpatched middleware commit
3. Remove `middleware-fixes-4c1d90a.patch` from AAMP repo

---

### VPAAMP-856: Remove Dependencies on Deprecated aamp/middleware for Utests

**Status**: Follow-up ticket (prerequisite to removing `aamp/middleware`)

**Problem**: AAMP unit tests still include headers from `aamp/middleware/`:
```cpp
#include "middleware/InterfacePlayerRDK.h"
#include "middleware/DemuxDataTypes.h"
```

**Impact**: Cannot fully remove `aamp/middleware` folder until unit tests are updated.

**Solution**:
1. Update unit test includes to use installed headers from `.libs/include/`
2. Ensure test builds link against installed middleware libraries
3. Verify all unit tests pass with external middleware
4. Remove `aamp/middleware` folder entirely

**Benefits**:
- Single source of truth for middleware code
- No code duplication or drift
- Cleaner repository structure
- Easier maintenance

---

## Migration Guide

### For Developers

**Building AAMP with External Middleware**:

```bash
# Option 1: Use sibling repo (recommended for development)
cd /path/to/workspace
git clone https://github.com/rdkcentral/middleware-player-interface.git
git clone https://github.com/rdkcentral/aamp.git
cd aamp
git checkout feature/VPAAMP-725
./install-aamp.sh

# Option 2: Auto-clone from GitHub
cd /path/to/aamp
git checkout feature/VPAAMP-725
./install-aamp.sh
# Script will auto-clone middleware-player-interface to .libs/src/

# Option 3: Specify custom middleware path
./install-aamp.sh --middleware-path /custom/path/to/middleware-player-interface
```

**The install script will**:
1. Detect sibling `middleware-player-interface` repo (if present)
2. Checkout commit `bd2b3b1` automatically
3. Apply build fixes patch
4. Build and install middleware
5. Track commit hash to trigger rebuilds when changed

### For CI/CD

**No changes required** - the install script handles everything automatically:
- Clones middleware from GitHub if not present
- Checks out correct commit
- Applies patches
- Builds and installs

---

## Key Learnings

### 1. API Compatibility is Critical

We tried three different middleware commits before finding the right one:
- `4c1d90a` (Feb 2026): Too old, missing required APIs
- `8a0af75` (May 2026): Still too old, missing ConfigurePipeline changes
- `bd2b3b1` (June 2026): ✅ Perfect - modern APIs, no regressions

**Lesson**: When pinning external dependencies, verify API compatibility thoroughly.

### 2. Performance Regressions Can Look Like Timing Issues

The `seekPausedState` issue caused consistent test timeouts that could have been dismissed as "CI flakiness". Only by:
- Comparing with baseline (`dev_sprint_25_2`)
- Checking git history
- Finding the internal revert commit

...did we identify the real root cause.

**Lesson**: Consistent timing differences across multiple tests indicate a real issue, not randomness.

### 3. Build System Fixes Should Go Upstream

The build fixes in our patch affect all platforms and all middleware consumers. Keeping them as a local patch creates maintenance burden.

**Lesson**: Submit fixes upstream promptly to benefit the entire ecosystem.

### 4. Documentation is Essential

This migration involved:
- 3 different middleware commits
- 5 different build fixes
- 1 critical regression
- Multiple API compatibility issues

Without detailed documentation (this file, the patch summary, the ticket summary), future developers would struggle to understand the decisions made.

**Lesson**: Document not just *what* changed, but *why* specific decisions were made.

---

## Files Changed

### Core Changes
- `scripts/install_options.sh` - Pin middleware commit to `bd2b3b1`
- `scripts/install_middleware_interfaces.sh` - Enhanced middleware build logic
- `CMakeLists.txt` - Add GStreamer framework to RPATH
- `middleware/README.md` - Deprecation notice

### Patches & Documentation
- `OSX/patches/middleware-fixes-4c1d90a.patch` - Comprehensive build fixes
- `OSX/patches/middleware-build-fixes-summary.md` - Detailed fix explanations
- `OSX/patches/RDKEMW-22371-ticket-summary.md` - Upstream submission guide
- `VPAAMP-725-FINAL-SUMMARY.md` - This file

---

## Commit History

```
10e14800 - VPAAMP-725: Use bd2b3b1 - latest commit before seekPausedState
2ce8511e - VPAAMP-725: Address code review comments
2c2c3cd1 - VPAAMP-725: Fix API compatibility - use middleware commit 8a0af75
dfa96ca9 - VPAAMP-725: Use stable middleware commit 4c1d90a + setPtsOffset patch
2c149bd7 - VPAAMP-725: Apply middleware patch on all platforms, not just macOS
31218735 - VPAAMP-725: Add /Library/Frameworks to rpath for GStreamer
95f6f62d - VPAAMP-725: Auto-rebuild middleware when commit changes
f62e137f - VPAAMP-725: Checkout correct middleware commit for sibling repos
6a8f5b84 - VPAAMP-725: Pin middleware to a55c02d (initial attempt)
baf89f9d - VPAAMP-725: Add middleware OSX build fixes workaround
f2b25aff - VPAAMP-725: Add deprecation notice to aamp/middleware folder
```

---

## Success Criteria

- [x] macOS builds complete without errors
- [x] Ubuntu builds complete without errors
- [x] `aampcli` runs on macOS without crashes
- [x] All L1 unit tests pass
- [x] All L2 integration tests pass
- [x] Performance equivalent to `dev_sprint_25_2` baseline
- [x] Middleware commit pinned and auto-checkout working
- [x] Build fixes documented for upstream submission
- [x] Code review comments addressed

---

## Next Steps

### Immediate (Post-Merge)
1. Monitor L2 test stability on `dev_sprint_25_2` after merge
2. Verify no regressions in production builds

### Short-Term (RDKEMW-22371)
1. Create PR to `middleware-player-interface` with build fixes
2. Work with middleware team to review and merge
3. Update AAMP to use unpatched middleware commit
4. Remove local patch file

### Medium-Term (VPAAMP-856)
1. Audit all unit test includes for `middleware/` references
2. Update to use installed headers from `.libs/include/`
3. Verify all tests pass with external middleware
4. Remove `aamp/middleware` folder entirely

### Long-Term
1. Establish process for middleware version updates
2. Add CI checks to prevent middleware API drift
3. Consider semantic versioning for middleware releases

---

## Contact & Support

**Primary Developer**: [Your Name]
**Jira Tickets**: 
- VPAAMP-725 (this work)
- RDKEMW-22371 (upstream fixes)
- VPAAMP-856 (unit test cleanup)

**Questions?** Check the detailed documentation in `OSX/patches/` or reach out to the AAMP team.
