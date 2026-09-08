# Middleware OSX Build Fixes (Historical)

> **Note (VPAAMP-881)**: The patch described in this document is no longer applied during builds.
> All fixes have been merged into HEAD of middleware-player-interface (as of commit `a55c02d`).
> The patch file has been moved to `archive/middleware-fixes-4c1d90a.patch` for historical reference.
> `install_middleware_interfaces.sh` now uses HEAD of middleware-player-interface directly.

## Summary
The middleware-player-interface repository has four build issues on macOS that prevent successful compilation. These issues do not affect Linux/Ubuntu builds.

## Issues Found

### Issue 1: Missing gstreamer-base-1.0 dependency
**File**: `gst-plugins/gst_subtec/CMakeLists.txt`

**Problem**: 
- Code uses `GstBaseTransform` symbols (`gst_base_transform_get_type`, `gst_base_transform_set_passthrough`, etc.)
- Only links against `gstreamer-app-1.0` (incorrectly named as `GSTREAMERBASE`)
- Missing `gstreamer-base-1.0` library causes undefined symbol linker errors

**Error**:
```
Undefined symbols for architecture arm64:
  "_gst_base_transform_get_type", referenced from:
      gst_vipertransform_get_type_once() in gstvipertransform.cpp.o
```

**Fix**: Add proper pkg-config check for `gstreamer-base-1.0` and link against it.

### Issue 2: OpenSSL headers missing on macOS
**File**: `CMakeLists.txt` (lines 51-55)

**Problem**:
- OpenSSL include directories only added for Linux builds
- macOS builds missing `${OPENSSL_INCLUDE_DIR}` in `include_directories()`
- DRM code fails to find `openssl/evp.h`

**Error**:
```
fatal error: 'openssl/evp.h' file not found
   29 | #include "openssl/evp.h"
```

**Fix**: Remove platform-specific conditional and include OpenSSL headers on all platforms.

### Issue 3: GStreamer library paths missing on macOS
**File**: `CMakeLists.txt` (line 123)

**Problem**:
- macOS build only has `link_directories(${OPENSSL_LIBRARY_DIRS})`
- Missing GStreamer library directories causes linker to fail finding libraries
- Hardcoded `-L.libs/lib` path in `OS_LD_FLAGS` doesn't exist in build directory

**Error**:
```
ld: library 'gstreamer-1.0' not found
clang++: error: linker command failed with exit code 1
```

**Fix**: Add `link_directories()` for GStreamer libraries on macOS.

### Issue 4: UUID library paths missing on macOS
**File**: `gst-plugins/CMakeLists.txt` (line 38-40)

**Problem**:
- gstaamp plugin uses literal `-luuid` in link dependencies
- No `link_directories()` for UUID library on macOS
- ossp-uuid from Homebrew not in default search path

**Error**:
```
ld: library 'uuid' not found
clang++: error: linker command failed with exit code 1
```

**Fix**: Add pkg-config check for UUID and link directories on macOS.

## Patch Details

**Patch file**: `middleware-fixes-4c1d90a.patch`

**Target commit**: bd2b3b1 (June 1, 2026) - latest commit before seekPausedState regression

This patch contains all four build fixes PLUS the setPtsOffset implementation:

1. **gst_subtec/CMakeLists.txt**: 
   - Separate gstreamer-app-1.0 and gstreamer-base-1.0 pkg-config variables
   - Add proper dependency for gstreamer-base-1.0 library
   - Update include and link directories

2. **CMakeLists.txt** (OpenSSL includes):
   - Remove Linux-only conditional
   - Apply OpenSSL includes to all platforms
   - Remove duplicate commented-out line

3. **CMakeLists.txt** (GStreamer link dirs):
   - Add `link_directories()` for GStreamer libraries on macOS

4. **gst-plugins/CMakeLists.txt** (UUID library):
   - Add pkg-config check for UUID on macOS
   - Add `link_directories()` for UUID library

5. **subtitle/subtitleParser.h** & **subtec/subtecparser/WebVttSubtecParser.{cpp,hpp}**:
   - Add setPtsOffset() virtual method and implementation
   - Required for HLS PTS restamping with subtitles

## Testing

After applying the patch, macOS builds complete successfully with:
```bash
cd /path/to/middleware-player-interface
git checkout bd2b3b1
patch -p1 < middleware-fixes-4c1d90a.patch
mkdir build && cd build
cmake .. -DCMAKE_INSTALL_PREFIX=/path/to/install \
         -DOPENSSL_ROOT_DIR=/opt/homebrew/opt/openssl@3 \
         -DCMAKE_BUILD_TYPE=Debug
make
make install
```

## Impact

- **Linux builds**: No impact (changes are additive or remove unnecessary conditionals)
- **macOS builds**: Fixes all three compilation failures
- **Functionality**: No runtime behavior changes, purely build system fixes

## Related Work

- **VPAAMP-725**: AAMP simulator builds migrated to external middleware
- **Workaround**: AAMP's `install_middleware_interfaces.sh` now auto-applies this patch until upstream fix is merged
