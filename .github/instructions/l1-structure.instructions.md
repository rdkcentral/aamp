---
description: L1 test directory structure, naming, and CMake patterns
applyTo:
  - "test/utests/**"
---

# L1 Test Structure & Naming

---

## Terminology

A **component** is the class or compilation unit under test, typically
corresponding to a single `.cpp` source file. AAMP does not always follow
one-class-per-file, so when in doubt use the source file name (without
extension) as the component name.

---

## Required Directory Path

All L1 tests live under:

```
test/utests/tests/[ComponentName]Tests/
```

**Wrong locations — do not use:**
- `test/[ComponentName]Tests/`
- `tests/[ComponentName]Tests/`
- `test/utests/[ComponentName]Tests/`
- `src/test/[ComponentName]Tests/`

---

## Required Files

Every L1 test suite directory must contain at minimum these three files:

| File | Purpose |
|---|---|
| `[ComponentName]Tests.cpp` | Test runner (matches directory name exactly) |
| `[ComponentName]TestCases.cpp` | Test cases and fixtures |
| `CMakeLists.txt` | Build configuration |

Additional `*TestCases.cpp` files are acceptable when a suite needs separate
fixtures, parameterized tests, or the test count is large enough to warrant
splitting for readability.

### Naming Convention

| Source file | Directory | Runner | Cases |
|---|---|---|---|
| `AampTime.cpp` | `AampTimeTests/` | `AampTimeTests.cpp` | `AampTimeTestCases.cpp` |
| `VideoDecoder.cpp` | `VideoDecoderTests/` | `VideoDecoderTests.cpp` | `VideoDecoderTestCases.cpp` |

- Directory name = component name + `Tests`
- Runner file name = directory name + `.cpp`
- Cases file name = component name + `TestCases.cpp`

---

## Check for Existing Tests First

Before creating a new test suite, search `test/utests/tests/` for:

- `[ComponentName]Tests/` (current convention)
- `[ComponentName]Test/` (legacy — singular)
- `[ComponentName]/` (legacy — no suffix)

If an existing suite is found, **ask the user** whether to:
1. Extend the existing suite with new cases
2. Rename/refactor the existing suite to current convention
3. Create a separate suite (requires justification)

**Do not silently create duplicate test suites.**

---

## CMakeLists.txt Pattern

```cmake
# Copyright header required — see copilot-instructions.md

include(GoogleTest)

set(AAMP_ROOT "../../../../")
set(UTESTS_ROOT "../../")
set(EXEC_NAME [ComponentName]Tests)

include_directories(${AAMP_ROOT} ${AAMP_ROOT}/subtitle ${AAMP_ROOT}/drm
                    ${AAMP_ROOT}/downloader ${AAMP_ROOT}/drm/helper
                    ${AAMP_ROOT}/tsb/api)
include_directories(${GTEST_INCLUDE_DIRS} ${GMOCK_INCLUDE_DIRS}
                    ${GLIB_INCLUDE_DIRS} ${GSTREAMER_INCLUDE_DIRS}
                    ${LIBCJSON_INCLUDE_DIRS})
include_directories(SYSTEM ${UTESTS_ROOT}/mocks)

# Test sources: runner + cases only
set(TEST_SOURCES [ComponentName]TestCases.cpp
                 [ComponentName]Tests.cpp)

# Component under test — ONLY the component being tested
set(AAMP_SOURCES ${AAMP_ROOT}/[ComponentName].cpp)

add_executable(${EXEC_NAME} ${TEST_SOURCES} ${AAMP_SOURCES})
set_target_properties(${EXEC_NAME} PROPERTIES FOLDER "utests")

if(CMAKE_XCODE_BUILD_SYSTEM)
    xcode_define_schema(${EXEC_NAME})
endif()

if(COVERAGE_ENABLED)
    include(CodeCoverage)
    APPEND_COVERAGE_COMPILER_FLAGS()
endif()

# Link fakes FIRST — this ensures fake implementations win over real ones
target_link_libraries(${EXEC_NAME} fakes -lpthread
                      ${GLIB_LINK_LIBRARIES} ${OS_LD_FLAGS}
                      ${GMOCK_LINK_LIBRARIES} ${GTEST_LINK_LIBRARIES})

aamp_utest_run_add(${EXEC_NAME})
```

### CMake Rules

- **Link `fakes` first** in `target_link_libraries`.
- **Only include the component being tested** in `AAMP_SOURCES`.
- **Do not add real dependency `.cpp` files** (e.g., `AampConfig.cpp`)
  if a fake in `test/utests/fakes/` already provides them.
- Include `test/utests/mocks/` headers with `SYSTEM` to suppress warnings.
- All new files require the RDK copyright header.
- All test cases require Doxygen tags.

---

## Test Runner Template

```cpp
/*
 * Copyright <Current Year> RDK Management
 * ... full copyright header ...
 */

#include <gtest/gtest.h>

int main(int argc, char** argv)
{
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
```

---

## Test Name Convention

Use descriptive names: `ClassName_MethodName_ExpectedBehavior`

```cpp
TEST_F(AampTimeTests, ConvertToIso8601_ValidEpoch_ReturnsFormattedString)
TEST_F(AampTimeTests, ConvertToIso8601_NegativeEpoch_ReturnsEmpty)
```
