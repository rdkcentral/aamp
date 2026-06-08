# Testing Patterns

**Analysis Date:** 2026-06-08

## Test Framework

**Runner:**
- Google Test (gtest) — discovered via `pkg_check_modules(GTEST REQUIRED gtest)` in `test/utests/CMakeLists.txt`
- Google Mock (gmock) — `pkg_check_modules(GMOCK REQUIRED gmock)`
- CTest — used for test discovery and result reporting
- C++ Standard: C++17 (enforced via `set(CMAKE_CXX_STANDARD 17)`)

**Assertion Library:**
- Google Test macros: `EXPECT_EQ`, `EXPECT_TRUE`, `ASSERT_TRUE`, `EXPECT_THAT`, etc.
- Google Mock matchers: `::testing::_`, `::testing::Return`, `::testing::HasSubstr`, etc.

**Run Commands:**
```bash
# First build and run (new test suite or after rebase)
cd test/utests && ./run.sh

# Iterative development (after initial integration)
cd test/utests/build/tests/[ComponentName]Tests
make
./[ComponentName]Tests

# With coverage
cd test/utests && ./run.sh -c    # requires install-aamp.sh -c to have been run first

# Per-test timeout override
cd test/utests && ./run.sh -t 120

# CI pipeline (Ubuntu)
CXXFLAGS="-std=c++17" CFLAGS="-std=c17" ./run.sh
```

**CI:**
- GitHub Actions: `.github/workflows/` runs all L1 tests on every PR against Ubuntu.
- Results published as JUnit XML to `test/utests/build/ctest-results.xml` via `dorny/test-reporter`.
- All new tests must pass in CI before merging.

## Test File Organization

**Location:** All L1 unit tests live under `test/utests/tests/[ComponentName]Tests/`

**Wrong locations — do not use:**
- `test/[ComponentName]Tests/`
- `test/utests/[ComponentName]Tests/`
- `src/test/[ComponentName]Tests/`

**Required files per test suite:**
| File | Purpose |
|------|---------|
| `[ComponentName]Tests.cpp` | Test runner — contains `main()` calling `RUN_ALL_TESTS()` |
| `[ComponentName]TestCases.cpp` | Test fixtures and all `TEST_F` cases |
| `CMakeLists.txt` | Build configuration |

**Fakes:** `test/utests/fakes/Fake[ComponentName].cpp` — simplified stand-ins linked at build time.

**Mocks:** `test/utests/mocks/Mock[ComponentName].h` — Google Mock interfaces for `EXPECT_CALL` control.

**Test structure:**
```
test/utests/
├── fakes/               # Fake implementations of all dependencies
│   ├── FakeAampConfig.cpp
│   ├── FakeAampGstPlayer.cpp
│   └── Fake[ComponentName].cpp
├── mocks/               # GMock interfaces for EXPECT_CALL
│   ├── MockAampConfig.h
│   ├── MockPrivateInstanceAAMP.h
│   └── Mock[ComponentName].h
└── tests/
    └── [ComponentName]Tests/
        ├── [ComponentName]Tests.cpp      # runner
        ├── [ComponentName]TestCases.cpp  # cases
        └── CMakeLists.txt
```

## Naming Conventions

**Test Suite Directories:** `[ComponentName]Tests/` — e.g., `AampLatencyMonitorTests/`, `StreamAbstractionAAMP_MPD/`

**Test Names:** `ClassName_MethodOrBehavior_ExpectedOutcome`
```cpp
TEST_F(AampLatencyMonitorTest, State_StartTransitionsToRunning)
TEST_F(AampLatencyMonitorTest, RateCorrection_HighLatency_SpeedsUp)
TEST_F(AampBufferControlTests, mBufferControlactionDownloadsTest1)    // legacy style
```
New tests should follow `ClassName_MethodOrBehavior_ExpectedOutcome`. Legacy `Test1`/`TestN` names exist but are not acceptable for new suites.

## Test Structure

**Suite Organisation:**
```cpp
// Runner file: [ComponentName]Tests.cpp
#include <gtest/gtest.h>

int main(int argc, char** argv)
{
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
```

```cpp
// Cases file: [ComponentName]TestCases.cpp
#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include "[ComponentName].h"
#include "Mock[Dependency].h"

using ::testing::_;
using ::testing::NiceMock;
using ::testing::Return;
using ::testing::StrictMock;

AampConfig *gpGlobalConfig{nullptr};   // Required global, initialised to nullptr

class MyComponentTest : public ::testing::Test
{
protected:
    MyComponent* mComponent{nullptr};
    NiceMock<MockDependency>* mMockDep{nullptr};

    void SetUp() override
    {
        mMockDep = new NiceMock<MockDependency>();
        g_mockDependency = std::shared_ptr<MockDependency>(mMockDep, [](MockDependency*){});
        mComponent = new MyComponent();
    }

    void TearDown() override
    {
        delete mComponent;    mComponent = nullptr;
        g_mockDependency = nullptr;
        delete mMockDep;      mMockDep   = nullptr;
    }
};

TEST_F(MyComponentTest, Method_Condition_ExpectedBehavior)
{
    // Arrange
    EXPECT_CALL(*mMockDep, SomeMethod(_)).WillOnce(Return(true));

    // Act
    bool result = mComponent->DoSomething();

    // Assert
    EXPECT_TRUE(result);
}
```

**Arrange-Act-Assert pattern:** Used consistently across the codebase with inline comments:
```cpp
// Arrange: Creating the variables for passing to arguments
// Act: Call the function for test
// Assert: Expecting the values are equal or not
```

**Setup/Teardown:**
- `SetUp()` override for fixture construction — allocates component under test and all mock dependencies.
- `TearDown()` override — deletes in reverse order, resets global mock pointers to `nullptr`.
- Global mock pointers (`g_mockAampConfig`, `g_mockPrivateInstanceAAMP`, etc.) must be reset in `TearDown`.

## Mocking Architecture

**Call chain (critical pattern):**
```
Test → Component (real code) → Fake (linked instead of dep) → Mock (for EXPECT_CALL)
```

1. Tests exercise **real** component code.
2. **Fakes** are linked at build time (listed before other libs: `target_link_libraries(... fakes ...)`).
3. **Fakes** delegate to global mock pointers (`g_mockAampConfig`, etc.), enabling `EXPECT_CALL` verification.

**Fakes pattern (FakeAampConfig.cpp):**
```cpp
std::shared_ptr<MockAampConfig> g_mockAampConfig{};  // global mock pointer

// Fake implementation delegates to mock when set:
void AampConfig::SetConfigValue(ConfigPriority owner, AAMPConfigSettingInt cfg, const int& value)
{
    if (g_mockAampConfig != nullptr)
    {
        return g_mockAampConfig->SetConfigValue(cfg, value);
    }
}
```

**Mock definitions (MockAampConfig.h):**
```cpp
class MockAampConfig
{
public:
    MOCK_METHOD(void, SetConfigValue, (AAMPConfigSettingBool cfg, const bool& value));
    MOCK_METHOD(bool, IsConfigSet, (AAMPConfigSettingBool cfg));
    MOCK_METHOD(bool, GetConfigValue, (AAMPConfigSettingBool cfg));
    // ...
};
extern std::shared_ptr<MockAampConfig> g_mockAampConfig;
```

**Mock strictness:**
| Type | Behavior | When to use |
|------|----------|-------------|
| `NiceMock<T>` | Silently ignores uninteresting calls | **Default** for AAMP L1 tests |
| `T` (bare mock) | Prints warning per unexpected call | During development for visibility |
| `StrictMock<T>` | Fails test on any uninteresting call | Only when proving no other calls occur |

Prefer `NiceMock<T>` by default. Use `StrictMock<T>` sparingly with a clear rationale.

**EXPECT_CALL patterns:**
```cpp
// Control return values
EXPECT_CALL(*g_mockAampConfig, GetConfigValue(eAAMPConfig_ABRCacheLife))
    .WillOnce(testing::Return(5000));

// Multiple return values
EXPECT_CALL(*g_mockAampConfig, GetConfigValue(eAAMPConfig_ABRCacheLife))
    .WillOnce(Return(5000))
    .WillOnce(Return(10000))
    .WillOnce(Return(0));

// Default stubs (unconditional)
ON_CALL(*mMockAamp, GetState()).WillByDefault(Return(eSTATE_PLAYING));
ON_CALL(*mMockAamp, IsAdPlaying()).WillByDefault(Return(false));
```

## Key Anti-patterns (Prohibited)

**Do not assert on fake return values** — tests the fake, not the component:
```cpp
// WRONG
EXPECT_TRUE(config.IsConfigSet(eAAMPConfig_EnableABR));   // tests fake behavior

// CORRECT
EXPECT_CALL(*g_mockAampConfig, IsConfigSet(eAAMPConfig_EnableABR)).WillOnce(Return(true));
EXPECT_TRUE(component.isAdaptiveBitrateEnabled());
```

**Do not use `EXPECT_TRUE(x == y)`** — hides operand values on failure:
```cpp
// WRONG
EXPECT_TRUE(url.find("video_p0_5.m4s") != std::string::npos);

// CORRECT
EXPECT_THAT(url, ::testing::HasSubstr("video_p0_5.m4s"));
```

**Do not use `EXPECT_EQ` on float/double** — exact equality is unreliable:
```cpp
// WRONG
EXPECT_EQ(component.GetRate(), 1.0);

// CORRECT
EXPECT_DOUBLE_EQ(component.GetRate(), 1.0);
EXPECT_NEAR(result, 0.0, 1e-9);
```

**Do not use `EXPECT_EQ(x, true)` / `EXPECT_EQ(x, false)`**:
```cpp
// WRONG
EXPECT_EQ(result, true);

// CORRECT
EXPECT_TRUE(result);
EXPECT_FALSE(result);
```

**Do not use raw `sleep()`/`usleep()`/`sleep_for()` for synchronization**:
```cpp
// WRONG
sleep(5);
EXPECT_TRUE(component.isReady());

// CORRECT — poll with deadline
bool WaitForCondition([&]() { return component.isReady(); }, ms{500});
```

## Fixtures and Factories

**Test data / factory helpers:**
```cpp
// Helper to create fast-poll config for async tests
static LatencyConfig MakeFastConfig(
    double normalRate = DEFAULT_NORMAL_RATE_CORRECTION_SPEED,
    double minRate    = DEFAULT_MIN_RATE_CORRECTION_SPEED,
    ...
)
{
    return LatencyConfig{normalRate, minRate, ..., 0, 5, ...};  // 5ms poll interval
}
```
Example: `test/utests/tests/AampLatencyMonitorTests/AampLatencyMonitorTestCases.cpp`

**Global required:**
- `AampConfig *gpGlobalConfig{nullptr};` — required at translation-unit scope in every test binary that links fakes depending on it.

## Async Testing

**Polling with deadline** (preferred pattern from `AampLatencyMonitorTests`):
```cpp
bool WaitForRunning(int maxWaitMs = 500)
{
    auto deadline = std::chrono::steady_clock::now()
        + std::chrono::milliseconds(maxWaitMs);
    while (!mMonitor->IsRunning())
    {
        if (std::chrono::steady_clock::now() >= deadline)
            return false;
        std::this_thread::sleep_for(std::chrono::milliseconds(2));
    }
    return true;
}

// Usage
ASSERT_TRUE(WaitForRunning());
```

**Latch/condition variable** (also acceptable):
```cpp
std::latch done(1);
component.onComplete([&]() { done.count_down(); });
component.start();
EXPECT_TRUE(done.try_wait_for(std::chrono::milliseconds(500)));
```

Use short poll intervals (2–5 ms) with tight deadlines (≤ 500 ms) in fast-config tests to keep CI suites quick.

## Error Testing

```cpp
TEST_F(AampGstPlayerTests, NullGuard_NullPointerInput_HandledGracefully)
{
    // Arrange — null input
    mComponent->SetPointer(nullptr);

    // Act
    bool result = mComponent->DoSomethingWithPointer();

    // Assert — component handles null gracefully
    EXPECT_FALSE(result);
}
```

## Coverage

**Requirements:** No numeric coverage target is enforced. Coverage is a diagnostic, not a goal. Tests written solely to raise a coverage percentage are considered harmful (brittle, implementation-coupled).

**Priority for test coverage:**
1. Hot playback / buffering / ABR / DRM paths
2. Historically regressed code paths
3. Code difficult to validate at integration level
4. Code implementing a non-obvious contract

Trivial getters, thin pass-throughs, and pure forwarding wrappers do not require dedicated unit tests.

**View Coverage (when built with `-c`):**
```bash
cd test/utests && ./run.sh -c
```
Requires baseline gcno files from `install-aamp.sh -c`.

**Coverage results:** `test/utests/build/ctest-results.xml`

## Test Types

**Unit Tests (L1):**
- Location: `test/utests/tests/[ComponentName]Tests/`
- Scope: One component in isolation; all dependencies replaced by fakes/mocks.
- Framework: Google Test + Google Mock.
- Build/run: `cd test/utests && ./run.sh`

**CLI/Manual Tests:**
- Location: `test/aampcli/` — interactive CLI for manual playback testing.
- Not part of the automated L1 suite.

**GStreamer Harness Tests:**
- Location: `test/gstTestHarness/` — GStreamer pipeline tests.
- Not part of the automated L1 suite.

**JS Binding Tests:**
- Location: `test/jsBindingTest/` — JavaScript binding testing.
- Separate from C++ L1 suite.

**DRM Legacy Tests:**
- Location: `test/utests/drm/` — DRM-specific tests with their own mock set.

**Integration/E2E Tests:**
- No dedicated E2E framework detected. Integration-level testing is deferred from L1 via "skipped test" comments when L1 isolation is not practical.

## CMakeLists.txt Pattern for New Tests

```cmake
include(GoogleTest)

set(AAMP_ROOT "../../../../")
set(UTESTS_ROOT "../../")
set(EXEC_NAME [ComponentName]Tests)

include(${CMAKE_CURRENT_LIST_DIR}/../CommonTestIncludes.cmake)

set(TEST_SOURCES
    [ComponentName]Tests.cpp
    [ComponentName]TestCases.cpp)

set(AAMP_SOURCES
    ${AAMP_ROOT}/[ComponentName].cpp)

add_executable(${EXEC_NAME} ${TEST_SOURCES} ${AAMP_SOURCES})
set_target_properties(${EXEC_NAME} PROPERTIES FOLDER "utests")

if (CMAKE_XCODE_BUILD_SYSTEM)
    xcode_define_schema(${EXEC_NAME})
endif()

if (COVERAGE_ENABLED)
    include(CodeCoverage)
    APPEND_COVERAGE_COMPILER_FLAGS()
endif()

# CRITICAL: fakes must be listed first
target_link_libraries(${EXEC_NAME}
    fakes
    -pthread
    ${GLIB_LINK_LIBRARIES}
    ${OS_LD_FLAGS}
    ${GMOCK_LINK_LIBRARIES}
    ${GTEST_LINK_LIBRARIES})

aamp_utest_run_add(${EXEC_NAME})
```

**CMake Rules:**
- Link `fakes` **first** — ensures fake implementations win over real ones at link time.
- Only include the component under test in `AAMP_SOURCES` — no real dependency `.cpp` files.
- Mock headers included with `SYSTEM` to suppress warnings (done via `CommonTestIncludes.cmake`).
- All new files require the RDK copyright header.
- All test methods require Doxygen tags.

---

*Testing analysis: 2026-06-08*
