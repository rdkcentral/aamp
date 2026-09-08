# VPAAMP-781: Boost 1.89 Compatibility Fix

## Problem

Starting with Boost 1.86, the `boost_system` library became header-only and no longer ships as a separate CMake component. This causes build failures when subtec-app's dependency `Ipp2Utils` tries to find the `boost_system` component:

```
CMake Error at /opt/homebrew/lib/cmake/Boost-1.89.0/BoostConfig.cmake:141 (find_package):
  Could not find a package configuration file provided by "boost_system"
  (requested version 1.89.0) with any of the following names:
    boost_system.cps
    boost_systemConfig.cmake
    boost_system-config.cmake
```

## Root Cause

The `Ipp2UtilsConfig.cmake` file in websocket-ipplayer2-utils contains:

```cmake
find_dependency(Boost REQUIRED system)
```

This syntax worked with Boost < 1.86 but fails with Boost 1.86+ because `system` is no longer a separate component.

## Solution

The patch `subttxrend-app-boost-1.89-compatible.patch` modifies the `Ipp2UtilsConfig.cmake.in` template to:

1. First try to find Boost with the `system` component (for older Boost versions)
2. If that fails, find Boost without specifying components (for Boost 1.86+)

This makes the build compatible with both old and new Boost versions.

## Files Modified

- `OSX/patches/subttxrend-app-boost-1.89-compatible.patch` - New patch file
- `middleware/OSX/patches/subttxrend-app-boost-1.89-compatible.patch` - Copy for middleware builds
- `scripts/install_subtec.sh` - Added patch application
- `middleware/scripts/install_subtec.sh` - Added patch application

## Testing

To test the fix:

1. Clean the subtec build: `./build.sh -t clean`
2. Rebuild: `./build.sh`

The build should now succeed with Boost 1.89.0 from Homebrew.

## Workaround (if patch doesn't apply)

If you need to skip subtec rebuild temporarily, use: `./build.sh -s`
