# Testing & Quality

## Overview

AAMP includes comprehensive test infrastructure for unit testing, integration testing, and quality assurance.

## Test Infrastructure

**Location**: `test/`

### Unit Tests

**Location**: `test/utests/`

Comprehensive unit test suite:
- Google Test framework
- Mock objects for isolation
- Code coverage support

### Test Categories

1. **Core Tests**: Player functionality
2. **Fragment Collector Tests**: HLS/DASH collectors
3. **ABR Tests**: Adaptive bitrate logic
4. **DRM Tests**: DRM functionality
5. **Utility Tests**: Helper functions

## Mock Objects

**Location**: `test/utests/mocks/`, `test/mocks/`

Mock implementations for:
- GStreamer
- libcurl
- DRM systems
- Platform services

## Test Execution

### Build Tests

```bash
cmake -DUTEST_ENABLED=ON ..
make
```

### Run Tests

```bash
cd test/utests
./run_tests.sh
```

## Code Coverage

Coverage tools:
- gcov/lcov for coverage reports
- Code coverage targets in CMake

## Quality Metrics

### Code Quality

- Static analysis tools
- Code review process
- Coding standards (see CONTRIBUTING.md)

### Performance

- Profiling tools
- Performance benchmarks
- Memory leak detection

## Summary

Testing infrastructure provides:
- Comprehensive test coverage
- Mock-based isolation
- Code quality metrics
- Continuous integration support
