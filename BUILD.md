# Build Instructions

## Overview

AAMP is built on top of GStreamer and uses CMake as its build system. This document consolidates build instructions for all supported platforms.

## Prerequisites

### Common Dependencies
- CMake 3.14+
- GCC/Clang with C++11 support
- GStreamer 1.x development files
- OpenSSL development files
- libcurl development files

### Ubuntu/Debian

```bash
sudo apt-get update
sudo apt-get install -y \
  build-essential \
  cmake \
  libgstreamer1.0-dev \
  libgstreamer-plugins-base1.0-dev \
  libssl-dev \
  libcurl4-openssl-dev \
  pkg-config \
  libxml2-dev
```

For detailed Ubuntu setup, see [UbuntuSetup.md](UbuntuSetup.md).

## Building AAMP

### Standard Build

```bash
cd /path/to/aamp
mkdir -p build
cd build
cmake ..
make -j$(nproc)
```

### Debug Build

```bash
cmake -DCMAKE_BUILD_TYPE=Debug ..
make -j$(nproc)
```

### Install

```bash
make install
```

The default install prefix is `/usr/local`. To change it:

```bash
cmake -DCMAKE_INSTALL_PREFIX=/opt/aamp ..
```

## Unit Tests

### L1 Unit Tests (Microtests)

Build and run L1 unit tests:

```bash
cd /path/to/aamp
mkdir -p build
cd build
cmake -DENABLE_UNIT_TESTS=ON ..
make -j$(nproc)
ctest --verbose
```

For comprehensive test documentation, see [TESTING.md](TESTING.md).

### DRM Tests

Additional setup may be required for DRM-related tests. See [test/utests/drm/README.md](test/utests/drm/README.md) for details.

## Build Flags

| Flag | Type | Description |
|------|------|-------------|
| `CMAKE_BUILD_TYPE` | String | `Debug` or `Release` (default: Release) |
| `CMAKE_INSTALL_PREFIX` | Path | Installation directory (default: `/usr/local`) |
| `ENABLE_UNIT_TESTS` | Boolean | Build L1 unit tests (default: OFF) |
| `ENABLE_SIMULATOR` | Boolean | Build AAMP simulator (default: OFF) |

## Troubleshooting

### Common Build Issues

**GStreamer not found:**
```bash
pkg-config --cflags --libs gstreamer-1.0
```

**Missing OpenSSL:**
```bash
brew install openssl  # macOS
sudo apt-get install libssl-dev  # Ubuntu
```

For more troubleshooting help, see [TROUBLESHOOTING.md](TROUBLESHOOTING.md).

## CI/CD

All changes are validated through the CI pipeline before merge. Tests must pass locally before submission:

```bash
make test  # or ctest
```

## Further Reading

- [Architecture](ARCHITECTURE.md)
- [Testing Strategy](TESTING.md)
- [Development Workflow](CONTRIBUTING.md)
