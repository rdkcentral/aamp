<!--
If not stated otherwise in this file or this component's license file the
following copyright and licenses apply:

Copyright 2026 RDK Management

Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.
-->

# Testing Strategy

## Overview

This document consolidates AAMP's testing approach, including L1 unit tests, test infrastructure, and DRM testing requirements.

## Test Levels

In AAMP, test levels classify automated tests by scope and dependencies:
**L1** refers to fast, fully isolated unit tests suitable for frequent and CI runs,
while **L1.5** refers to unit tests that additionally depend on DRM-related
headers or libraries and may require extra environment setup.
### L1 Unit Tests (Microtests)

**Purpose**: Fast, lightweight tests for individual components

**Characteristics**:
- Run in isolation without external dependencies
- Execute in seconds
- Use Google Test (gtest) and Google Mock (gmock)
- Located in `test/utests/`

**Running L1 Tests**:
```bash
cd build
cmake -DENABLE_UNIT_TESTS=ON ..
make
ctest --verbose
```

**Key Directories**:
- `test/utests/` - Core unit tests
- `test/utests/tests/tsb/` - TSB (Time Shift Buffer) specific tests
- `test/utests/drm/` - DRM-related tests

### L1.5 Tests (DRM)

**Purpose**: Tests requiring DRM header dependencies

**Location**: `test/utests/drm/`

**Special Setup**: May require additional header installation. See [test/utests/drm/README.md](test/utests/drm/README.md).

## Test Framework

### Google Test & Mock

AAMP uses Google Test (gtest) for unit testing and Google Mock (gmock) for mocking dependencies:

```cpp
#include <gtest/gtest.h>
#include <gmock/gmock.h>

class MyComponentTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Setup before each test
    }
};

TEST_F(MyComponentTest, ShouldDoSomething) {
    // Arrange
    // Act
    // Assert
    EXPECT_EQ(expected, actual);
}
```

### Fake Framework

AAMP provides fake implementations for testing components in isolation:

**Location**: `test/fakes/`

**Usage Pattern**:
```cpp
#include "test/fakes/FakeDownloader.h"

FakeDownloader fakeDownloader;
ComponentUnderTest component(&fakeDownloader);
// Test component behavior
```

## Test Code Requirements

### Code Coverage

All public functions must have unit tests. Aim for:
- Minimum 80% code coverage for new code
- 100% coverage for critical paths

### Test Structure

```cpp
// Arrange: Set up test data and expectations
// Act: Call the function under test
// Assert: Verify the result
EXPECT_EQ(expected, actual);
```

### Mocking Guidelines

Use Google Mock for:
- External dependencies (network, filesystem, DRM)
- Abstract interfaces
- Hard-to-create objects

```cpp
class MockDownloader : public IDownloader {
public:
    MOCK_METHOD(DownloadResult, download, (const std::string&), (override));
};

// In test:
MockDownloader mock;
EXPECT_CALL(mock, download).WillOnce(Return(expected_result));
```

## Build Integration

### CMake Test Configuration

Tests are automatically discovered and registered:

```cmake
enable_testing()
add_executable(my_test my_test.cpp)
target_link_libraries(my_test ${GTEST_LIBRARIES} gmock)
add_test(NAME my_test COMMAND my_test)
```

### CI Pipeline

All tests must pass in the CI pipeline:

```bash
# Local verification before PR
make test
ctest --verbose
```

## DRM Testing

DRM tests may require special headers or dependencies:

**See**: [test/utests/drm/README.md](test/utests/drm/README.md)

**Key Points**:
- Identify header dependencies
- Document acquisition method
- Provide setup automation where possible

## TSB Tests

Time Shift Buffer specific tests:

**Location**: `test/utests/tests/tsb/`

**Includes**:
- In-memory tests
- Filesystem-based tests (RealFilesystemTests)
- See individual README files for details

## Test Execution

### Run All Tests

```bash
cd build
ctest --verbose
```

### Run Specific Test

```bash
ctest -R "TestName" --verbose
```

### Run with Coverage

```bash
cmake -DENABLE_COVERAGE=ON ..
make
make test
make coverage_report
```

## Writing New Tests

### Checklist

- [ ] Follows Google Test structure
- [ ] Uses mocks for dependencies (from `test/fakes/`)
- [ ] Has descriptive test name
- [ ] Tests both success and failure paths
- [ ] Includes edge cases
- [ ] No hardcoded timeouts without justification
- [ ] Cleans up resources

### Best Practices

1. **One Assertion per Test** (ideally) or logical grouping
2. **Descriptive Names**: Use pattern `Test_Component_Behavior_Expectation`
3. **Test Data**: Use realistic but minimal test data
4. **Isolation**: Tests must pass in any order
5. **No Network Calls**: Use mocks instead
6. **Deterministic**: No flaky tests due to timing

## Instruction References

**Critical**: Before writing tests, review:
- [.github/instructions/testing.instructions.md](.github/instructions/testing.instructions.md) - L1 test structure and framework details
- [.github/instructions/cpp.instructions.md](.github/instructions/cpp.instructions.md) - C++ coding standards
- [CONTRIBUTING.md](CONTRIBUTING.md) - Code review and PR standards

## Debugging Tests

### Enable Verbose Output

```bash
ctest --verbose
# or run directly:
./test_executable --gtest_filter="TestName"
```

### GDB Integration

```bash
gdb ./test_executable
(gdb) run
```

### Inspect Test Cases

```bash
./test_executable --gtest_list_tests
```

## Performance Testing

For performance-critical components:

1. Add microbenchmark tests
2. Set realistic performance expectations
3. Profile before and after changes
4. Document any performance trade-offs

## See Also

- [Architecture](ARCHITECTURE.md)
- [Build Instructions](BUILD.md)
- [Contributing Guide](CONTRIBUTING.md)
- [Testing Instructions](.github/instructions/testing.instructions.md)
