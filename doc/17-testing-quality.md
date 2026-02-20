# Testing & Quality

## Overview

AAMP includes comprehensive test infrastructure designed to ensure code quality, reliability, and correctness across all subsystems. The testing framework supports unit testing, integration testing, and quality assurance through automated test execution, code coverage analysis, and quality metrics collection.

The test infrastructure uses Google Test (gtest) framework for unit testing, providing a robust foundation for test organization, assertion, and test execution. Mock objects isolate components under test from dependencies, enabling focused unit testing and reproducible test scenarios. The testing system integrates with build infrastructure (CMake) and continuous integration (CI) systems to provide automated quality assurance throughout the development lifecycle.

Quality assurance extends beyond functional testing to include code coverage analysis, static code analysis, performance profiling, and memory leak detection. These quality metrics ensure code maintainability, performance, and reliability while identifying potential issues before production deployment.

## Test Infrastructure

**Location**: `test/`

The test directory contains all testing-related code, organized by test type and component:

- **Unit Tests** (`test/utests/`): Comprehensive unit test suite covering individual components and classes
- **Integration Tests**: Tests that verify component interactions and end-to-end workflows
- **Mock Objects** (`test/utests/mocks/`, `test/mocks/`): Mock implementations of dependencies for test isolation
- **Test Utilities** (`test/utests/fakes/`): Fake implementations and test helpers
- **CLI Tools** (`test/aampcli/`): Command-line interface for manual testing and debugging
- **Test Harnesses** (`test/gstTestHarness/`): GStreamer-specific test infrastructure

### Unit Tests

**Location**: `test/utests/`

The unit test suite provides comprehensive coverage of AAMP components:

- **Google Test Framework**: Unit tests use Google Test (gtest) framework for test organization and execution. Google Test provides test fixtures, parameterized tests, death tests, and test discovery mechanisms. Tests are organized into test suites (TEST, TEST_F) that group related tests and share setup/teardown logic via test fixtures.

- **Mock Objects for Isolation**: Unit tests use mock objects to isolate components under test from their dependencies. Mocks (`test/utests/mocks/`) provide controlled behavior for dependencies (GStreamer, libcurl, DRM systems), enabling tests to verify component behavior without requiring actual dependencies. Mock frameworks (Google Mock, custom mocks) provide expectation setting and verification, ensuring components interact correctly with dependencies.

- **Code Coverage Support**: Unit tests are designed to achieve high code coverage, exercising all code paths including error conditions and edge cases. Code coverage tools (gcov, lcov) generate coverage reports showing which code is exercised by tests, identifying untested code paths and guiding test development. Coverage targets in CMake enable coverage report generation during test execution.

**Test Organization**: Unit tests are organized by component:
- **Core Player Tests** (`test/utests/tests/PrivAampTests/`, `test/utests/tests/PlayerInstanceAAMP/`): Tests for core player functionality, state management, and API behavior
- **Fragment Collector Tests** (`test/utests/tests/StreamAbstractionAAMP_MPD/`, `test/utests/tests/FragmentCollectorAdTests/`): Tests for HLS/DASH fragment collection, manifest parsing, and fragment processing
- **ABR Tests** (`test/utests/tests/AampAbrTests/`, `test/utests/tests/NetworkBandwidthEstimator/`): Tests for adaptive bitrate logic, bandwidth estimation, and profile selection
- **DRM Tests** (`test/utests/drm/`): Tests for DRM functionality, license acquisition, and key management
- **Utility Tests** (`test/utests/tests/AampGrowableBuffer/`, `test/utests/tests/IsoBmffProcessorTests/`): Tests for helper functions, utilities, and data structures

### Test Categories

Unit tests are categorized by functionality and component:

1. **Core Tests**: Player functionality tests verify core player operations (tune, play, pause, seek, stop), state transitions, event generation, and error handling. Tests verify API behavior, parameter validation, and state machine correctness. Core tests ensure player instances behave correctly in isolation and during concurrent operations.

2. **Fragment Collector Tests**: Fragment collector tests verify HLS/DASH/progressive fragment collection, manifest parsing, fragment URL generation, and track synchronization. Tests cover playlist refresh logic, discontinuity handling, ad insertion, and multi-track coordination. Collector tests ensure correct fragment sequencing and timing for smooth playback.

3. **ABR Tests**: Adaptive bitrate tests verify bandwidth estimation accuracy, profile selection logic, ramp-up/ramp-down behavior, and buffer-based decisions. Tests verify ABR algorithms respond correctly to network conditions, buffer levels, and configuration changes. ABR tests ensure quality adaptation maintains playback continuity while maximizing quality.

4. **DRM Tests**: DRM tests verify license acquisition workflows, key management, session lifecycle, and decryption coordination. Tests cover multiple DRM systems (Widevine, PlayReady, ClearKey), error handling, retry logic, and platform integration. DRM tests ensure secure content playback and proper error recovery.

5. **Utility Tests**: Utility tests verify helper functions, data structures, parsers, and format converters. Tests cover edge cases, error conditions, and performance characteristics. Utility tests ensure foundational components operate correctly and efficiently.

## Mock Objects

**Location**: `test/utests/mocks/`, `test/mocks/`

Mock objects provide controlled implementations of dependencies for test isolation:

- **GStreamer Mocks** (`test/utests/mocks/MockAampGstPlayer.h`, `test/utests/mocks/MockGLib.h`): Mock GStreamer components that simulate pipeline behavior, buffer injection, and event generation. GStreamer mocks enable testing of GStreamer integration without requiring actual GStreamer installation or media pipeline setup. Mocks verify correct GStreamer API usage and handle pipeline state transitions.

- **libcurl Mocks** (`test/utests/mocks/MockCurl.h`, `test/utests/drm/mocks/curlMocks.h`): Mock libcurl implementations that simulate HTTP downloads, network conditions, and error scenarios. Curl mocks enable testing of download logic, retry behavior, and error handling without requiring network connectivity or actual HTTP servers. Mocks provide configurable download speeds, latency, and error injection.

- **DRM System Mocks** (`test/utests/mocks/MockAampLicManager.h`, `test/utests/mocks/MockDrmHelper.h`, `test/utests/drm/mocks/MockSecureClient.h`): Mock DRM implementations that simulate license acquisition, key management, and decryption operations. DRM mocks enable testing of DRM workflows without requiring actual DRM systems or license servers. Mocks provide configurable license responses, key data, and error conditions.

- **Platform Service Mocks** (`test/mocks/rfcMocks.h`, `test/mocks/iarmMgrMocks.h`): Mock platform services (RFC, IARM) that simulate platform service behavior and responses. Platform mocks enable testing of platform integration without requiring actual platform services or RDK environment. Mocks provide configurable service responses and error conditions.

**Mock Usage**: Tests instantiate mock objects and configure expectations (method calls, return values, side effects) before executing code under test. After test execution, tests verify that expected interactions occurred (method calls, parameter values) and that components behaved correctly. Mock frameworks provide assertion mechanisms for verifying interactions and detecting unexpected behavior.

## Test Execution

### Build Tests

Unit tests are built separately from production code to enable test-specific configurations:

```bash
cmake -DUTEST_ENABLED=ON ..
make
```

**Build Configuration**:
- **`UTEST_ENABLED`**: CMake option that enables unit test compilation. When enabled, CMake includes test source files and links against Google Test framework. Test executables are built alongside production libraries.
- **Test Dependencies**: Tests require Google Test framework (`find_package(GTest)`). CMake locates GTest installation or builds GTest from source if unavailable. Test builds may include additional dependencies (mock frameworks, test utilities) not required for production builds.
- **Test Executables**: Each test suite compiles into a separate executable (`test_PrivAampTests`, `test_StreamAbstractionAAMP_MPD`, etc.). Test executables link against AAMP libraries and test frameworks, enabling standalone test execution.

### Run Tests

Tests can be executed individually or as a suite:

```bash
cd test/utests
./run_tests.sh
```

**Test Execution Methods**:
- **Individual Test Execution**: Test executables can be run directly (`./test_PrivAampTests`) to execute specific test suites. Direct execution provides immediate feedback and enables debugging of individual test failures.
- **Test Runner Scripts**: Test runner scripts (`run_tests.sh`, `run.sh`) execute all test suites sequentially and aggregate results. Test runners provide summary reports, failure identification, and exit codes for CI integration.
- **CTest Integration**: CMake's CTest framework (`ctest`) provides standardized test execution and reporting. CTest discovers test executables, executes them, and generates test reports compatible with CI systems.

**Test Output**: Test execution produces detailed output including:
- **Test Results**: Pass/fail status for each test case
- **Assertion Failures**: Detailed failure messages with file/line information
- **Test Duration**: Execution time for performance monitoring
- **Coverage Reports**: Code coverage data when coverage tools are enabled

## Code Coverage

Code coverage analysis measures which code is exercised by tests:

- **gcov/lcov for Coverage Reports**: gcov (GNU Coverage) generates coverage data during test execution, tracking which lines, branches, and functions are executed. lcov (Linux Test Project Coverage) processes gcov data to generate human-readable HTML coverage reports. Coverage reports show line coverage percentages, identify untested code paths, and highlight areas needing additional tests.

- **Code Coverage Targets in CMake**: CMake includes coverage targets (`make coverage`) that compile code with coverage instrumentation (`--coverage` flag), execute tests, and generate coverage reports. Coverage targets integrate coverage analysis into build process, enabling automated coverage reporting in CI systems.

**Coverage Goals**: AAMP aims for high code coverage (typically >80%) across all components. Coverage analysis identifies:
- **Untested Code**: Code paths not exercised by tests, indicating need for additional test cases
- **Dead Code**: Code that is never executed, indicating potential removal candidates
- **Test Gaps**: Areas with low coverage, guiding test development priorities

## Quality Metrics

### Code Quality

Code quality assurance includes multiple analysis techniques:

- **Static Analysis Tools**: Static analysis tools (cppcheck, clang-static-analyzer, Coverity) analyze source code without execution, identifying potential bugs, code smells, and security vulnerabilities. Static analysis detects issues like memory leaks, null pointer dereferences, buffer overflows, and coding standard violations. Analysis results guide code review and refactoring efforts.

- **Code Review Process**: All code changes undergo peer review before integration. Code reviews verify correctness, adherence to coding standards, test coverage, and design quality. Review process ensures code quality, knowledge sharing, and consistency across codebase.

- **Coding Standards** (see CONTRIBUTING.md): AAMP follows established coding standards (C++17 best practices, naming conventions, formatting rules) to ensure code consistency and maintainability. Coding standards cover naming, formatting, documentation, error handling, and resource management. Standards enforcement via tools (clang-format, linting) ensures consistent code style.

### Performance

Performance testing and profiling ensure optimal performance:

- **Profiling Tools**: Profiling tools (gprof, perf, Valgrind) measure code execution performance, identifying bottlenecks and optimization opportunities. Profiling reveals CPU usage patterns, function call frequencies, and performance hotspots. Profile data guides optimization efforts and performance tuning.

- **Performance Benchmarks**: Performance benchmarks measure key operations (tune time, fragment download time, ABR decision time) to track performance regressions and improvements. Benchmarks establish performance baselines and detect performance degradation during development. Benchmark results guide performance optimization priorities.

- **Memory Leak Detection**: Memory leak detection tools (Valgrind memcheck, AddressSanitizer) identify memory leaks, use-after-free errors, and memory corruption. Leak detection ensures proper resource management and prevents memory-related bugs. Detection tools integrate into test execution, automatically identifying memory issues during testing.

## Summary

The testing infrastructure provides comprehensive quality assurance capabilities:

- **Comprehensive Test Coverage**: Extensive unit test suite covers all major components and code paths, ensuring correctness and reliability. Test coverage analysis identifies untested code and guides test development.

- **Mock-Based Isolation**: Mock objects enable isolated unit testing without requiring actual dependencies, ensuring fast, reproducible tests. Mock frameworks provide expectation setting and verification, ensuring correct component interactions.

- **Code Quality Metrics**: Static analysis, code review, and coding standards ensure code quality and maintainability. Quality metrics identify issues early in development, reducing production bugs and technical debt.

- **Continuous Integration Support**: Test infrastructure integrates with CI systems, enabling automated quality assurance throughout development. CI integration provides immediate feedback on code changes, preventing regressions and ensuring consistent quality.
