# Build System

## Overview

AAMP uses CMake for build configuration and dependency management.

## Build Configuration

**File**: `CMakeLists.txt`

### Key Dependencies

- **GStreamer** (1.18.0+): Media pipeline
- **libcurl**: HTTP downloads
- **libdash**: DASH parsing
- **libxml2**: XML parsing
- **OpenSSL**: Cryptography
- **cjson**: JSON parsing

### Build Options

Key CMake options:
- `CMAKE_WPEWEBKIT_JSBINDINGS`: Enable JS bindings
- `CMAKE_TELEMETRY_2_0_REQUIRED`: Enable telemetry
- `CMAKE_USE_RDK_PLUGINS`: Use RDK plugins
- `UTEST_ENABLED`: Enable unit tests

## Build Process

### Configuration

```bash
mkdir build
cd build
cmake ..
```

### Compilation

```bash
make
```

### Installation

```bash
make install
```

## Platform-Specific

### Ubuntu/Simulator

- Standard build
- Development tools
- Test harness

### RDK Platform

- Platform-specific libraries
- RDK plugins
- Production optimizations

### macOS

- Xcode project generation
- Framework dependencies

## Summary

The build system provides:
- Cross-platform support
- Flexible configuration
- Dependency management
- Platform-specific builds
