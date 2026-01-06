---
description: L1 test instructions
---

# Unit Testing Copilot Instructions

## **🚨 L1 Testing Golden Rule: Don't Test the Mocks!**

**The #1 rule for L1 tests: Test YOUR component behavior, not mock/fake behavior.**

```cpp
// ❌ WRONG: Testing mock behavior
EXPECT_GT(mockBuffer.GetLen(), 0);        // Tests the mock
EXPECT_NE(mockBuffer.GetPtr(), nullptr);  // Tests the mock

// ✅ CORRECT: Testing component behavior  
EXPECT_EQ(myComponent.status, READY);     // Tests your component
EXPECT_DOUBLE_EQ(myComponent.position, 10.5);  // Tests your component
```

## **Quick Reference: Essential Build Process**

### **For New L1 Tests (Critical Steps)**
```bash
# 1. Create test files in: test/utests/tests/[Component]Tests/
#    - [Component]Tests.cpp (runner)  
#    - [Component]TestCases.cpp (cases)
#    - CMakeLists.txt (build config)

# 2. MUST run master script first (integrates new tests):
cd test/utests && ./run.sh

# 3. Then iterate in build directory:
cd test/utests/build/tests/[Component]Tests && make && ./[Component]Tests
```

### **Essential CMakeLists.txt Pattern**
```cmake
# Link fakes FIRST, only include component under test
target_link_libraries(${EXEC_NAME} fakes -lpthread ...)
set(TEST_SOURCES ComponentTestCases.cpp ComponentTests.cpp ${AAMP_ROOT}/Component.cpp)
# DO NOT include real dependency sources like AampGrowableBuffer.cpp
```

### **🚨 CRITICAL: Common L1 Testing Mistakes to Avoid**

#### **Mistake #1: Testing Mock/Fake Implementation Instead of Your Component** 
```cpp
// ❌ WRONG - Tests the fake AampGrowableBuffer, not your component
EXPECT_GT(component.buffer.GetLen(), 0);
EXPECT_NE(component.buffer.GetPtr(), nullptr);

// ✅ CORRECT - Tests your component's state and behavior  
EXPECT_EQ(component.status, READY);
EXPECT_DOUBLE_EQ(component.position, 10.5);
```

#### **Mistake #2: Including Real Dependencies in CMakeLists.txt**
```cmake
# ❌ WRONG - Defeats the purpose of unit testing with fakes
set(TEST_SOURCES MyComponentTestCases.cpp 
                 ${AAMP_ROOT}/AampGrowableBuffer.cpp)  # DON'T DO THIS

# ✅ CORRECT - Only include component under test, fakes provide dependencies
set(TEST_SOURCES MyComponentTestCases.cpp MyComponentTests.cpp)
set(AAMP_SOURCES ${AAMP_ROOT}/MyComponent.cpp)  # Only the component being tested
```

#### **Mistake #3: Expecting Real Implementation Behavior from Fakes**
```cpp
// ❌ WRONG - Expects real memory copying behavior from fake
buffer.AppendBytes(data, size);
EXPECT_NE(buffer.GetPtr(), data);  // Fake uses pointer assignment, not copy

// ✅ CORRECT - Adapt expectations to fake behavior
buffer.AppendBytes(data, size); 
EXPECT_EQ(buffer.GetPtr(), data);  // Fake assigns pointer directly
```

**Remember: Your tests should verify YOUR component works correctly with dependencies, not that the dependencies work correctly.**

### **Clean Up After Intermediate Testing**
When developing and debugging tests, clean up temporary modifications made during the development process:

**Critical Distinction - What to Clean Up:**
- **Temporary Development Artifacts** (DELETE these):
  - Debug files created for testing behavior: `debug_*.cpp`, `temp_*.h`, etc.
  - Temporary modifications to fake implementations made during debugging
  - Any exploratory code files created to understand component behavior
  - Test files created just for experimentation, not for the final L1 test suite

- **Official Deliverables** (KEEP these):
  - Final L1 test files: `[Component]Tests.cpp`, `[Component]TestCases.cpp`, `CMakeLists.txt`
  - Component source files with added functionality (e.g., enhanced `CachedFragment.h/.cpp`)
  - Official build directory: `test/utests/build/tests/` (never delete this)

**Example cleanup process:**
```bash
# Remove temporary debug/exploration files
rm -f debug_*.cpp temp_*.h test_*.cpp exploration_*.cpp

# Revert temporary changes to fakes (if any were made for debugging)
git checkout test/utests/fakes/FakeAampGrowableBuffer.cpp

# Verify only intended deliverables remain:
# ✅ Keep: test/utests/tests/[Component]Tests/ (final L1 test directory)
# ✅ Keep: Enhanced component files (e.g., CachedFragment.h with new methods)
# ✅ Keep: test/utests/build/tests/ (official build directory)
# ❌ Remove: Any debug_*.cpp, test*.cpp, temp_*.h files created during development
```

**Key Principle**: Only temporary development artifacts should be removed, never the official build infrastructure or final deliverables.

### **Fake vs Real Behavior**
- Fake `AppendBytes()`: pointer assignment, not memory copy
- Fake `Free()`: no-op, doesn't reset length  
- Fake `Clear()`: may not reset all fields
- **Always adapt test expectations to fake behavior**

## C++ Unit Testing with Google Test

### **Working with AAMP's Fake Infrastructure**

This project provides a comprehensive fake/mock infrastructure specifically designed for unit testing. **Always use fakes instead of real implementations.**

#### **Available Fake Components**
Located in `test/utests/fakes/`:
- `FakeAampGrowableBuffer.cpp` - Simplified buffer operations
- `FakeAampConfig.cpp` - Configuration management stub
- `FakeAampLogManager.cpp` - Logging system stub
- And many others...

#### **Understanding Fake Behavior**
Fake implementations are **intentionally simplified** and may not behave exactly like real implementations:

```cpp
// Example: FakeAampGrowableBuffer behavior
void AampGrowableBuffer::AppendBytes(const void *srcPtr, size_t srcLen) {
    this->ptr = (void*)srcPtr;    // Simple pointer assignment, not deep copy
    this->len = srcLen;
}

void AampGrowableBuffer::Free(void) {
    // Intentionally empty - no cleanup performed
}
```

#### **Adapting Tests for Fake Behavior**
Your tests must account for fake behavior differences:

```cpp
TEST_F(ComponentTest, TestWithFakeBuffer) {
    AampGrowableBuffer buffer;
    const char* data = "test data";
    
    buffer.AppendBytes(data, strlen(data));
    
    // With fake: ptr points directly to original data
    EXPECT_EQ(buffer.GetPtr(), data);  // Same pointer
    
    // With real implementation: ptr would point to copied data
    // EXPECT_NE(buffer.GetPtr(), data);  // Different pointer - don't expect this with fakes
    
    buffer.Free();
    // With fake: Free() is no-op, length unchanged
    EXPECT_EQ(buffer.GetLen(), strlen(data));  // Still has length
    
    // With real implementation: Free() would reset length to 0
    // EXPECT_EQ(buffer.GetLen(), 0);  // Don't expect this with fakes
}
```

#### **When Fake Behavior Causes Test Issues**
If fake behavior makes a test impossible or meaningless:
1. **First choice**: Adapt test expectations to fake behavior
2. **Document limitations**: Add comments explaining fake behavior constraints
3. **Last resort**: Comment out problematic tests with explanation

```cpp
/**
 * @brief Test buffer memory isolation
 * 
 * Note: This test is commented out because FakeAampGrowableBuffer uses pointer
 * assignment instead of memory copying, making memory isolation testing impossible.
 * In production, this functionality should be tested with integration tests.
 */
/*
TEST_F(ComponentTest, BufferMemoryIsolation_Real_Implementation_Needed) {
    // Test would fail with fake implementation
}
*/
```

### Test Structure and Organization

- Generate unit tests using the Google Test & Google Mock framework
- Aim for minimum 90% code coverage
- Unit tests shall be created under the test/utests/tests directory in a new directory derived from the filename
- Example: Tests for AampTime.h and AampTime.cpp shall be created in the directory test/utests/tests/AampTimeTests
- Unit tests shall consist of a separate test runner named [directory name].cpp to call the tests and a C++ source file named [directory name without the trailing 's']Cases.cpp to contain the test code
- The directory, test runner, test cases and CMakeLists.txt files shall all be created when a unit test is requested
- CMakeLists.txt shall begin with the copyright block as shall the test runner and test cases
- Test cases shall use Doxygen tags
- All possible code paths shall be tested including error conditions
- Edge cases and boundary conditions shall be tested

#### **CRITICAL: Use Fakes and Mocks for Dependencies**
- **ALWAYS use the project's fake implementations** located in `test/utests/fakes/` instead of real implementations
- The project provides comprehensive fake implementations (e.g., `FakeAampGrowableBuffer.cpp`) designed for unit testing
- Link tests against the `fakes` library in CMakeLists.txt: `target_link_libraries(${EXEC_NAME} fakes ...)`
- **Do NOT include real implementation source files** in test CMakeLists.txt - use fakes instead
- Adapt test expectations to match fake behavior (e.g., fake methods may be no-ops or simplified)
- Use Google Mock interfaces from `test/utests/mocks/` for complex mocking scenarios
- Test naming should be descriptive: ClassName_MethodName_ExpectedBehavior
- Remember that the patterns being used here for microtests/L1 tests are not strictly as defined by the GoogleTest handbook
- the GoogleTest handbook is at (https://google.github.io/googletest/)

#### **CRITICAL: Don't Test Mock/Fake Behavior - Test Your Component**

**The most common L1 testing mistake is testing the mock/fake implementation instead of the component under test.**

##### **❌ WRONG: Testing Mock/Fake Behavior**
```cpp
// BAD: Testing that the fake AampGrowableBuffer behaves like the fake
TEST_F(CachedFragmentTest, BadExample_TestingMockBehavior) {
    cachedFragment->fragment.AppendBytes(testData, testDataSize);
    
    // These test the FAKE implementation, not CachedFragment behavior:
    EXPECT_GT(cachedFragment->fragment.GetLen(), 0);        // Tests fake behavior
    EXPECT_NE(cachedFragment->fragment.GetPtr(), nullptr);  // Tests fake behavior
    EXPECT_EQ(memcmp(cachedFragment->fragment.GetPtr(), testData, testDataSize), 0); // Tests fake memory
}
```

##### **✅ CORRECT: Testing Component Behavior**
```cpp
// GOOD: Testing CachedFragment member variables and behavior
TEST_F(CachedFragmentTest, GoodExample_TestingComponentBehavior) {
    // Test the component's actual member variables and state
    EXPECT_DOUBLE_EQ(cachedFragment->position, expectedPosition);
    EXPECT_EQ(cachedFragment->uri, expectedUri);
    EXPECT_EQ(cachedFragment->type, expectedType);
    
    // If you need to verify mock interactions, use EXPECT_CALL:
    EXPECT_CALL(mockObject, someMethod(testing::_))
        .WillOnce(testing::Return(true));
    
    // Then test your component's response to the mock behavior
    EXPECT_TRUE(cachedFragment->processData());
}
```

##### **How to Identify When You're Testing Mock Behavior**
Look for these patterns in your tests - these usually indicate testing mock behavior:

1. **Calling methods on mocked dependencies and asserting their return values:**
   ```cpp
   // BAD: Testing what the mock returns
   EXPECT_EQ(mockBuffer.GetLen(), someValue);
   EXPECT_NE(mockBuffer.GetPtr(), nullptr);
   ```

2. **Testing memory contents or data integrity of mocked objects:**
   ```cpp
   // BAD: Testing mock's memory behavior
   EXPECT_EQ(memcmp(mockBuffer.GetPtr(), expectedData, size), 0);
   ```

3. **Verifying mock state changes without testing component behavior:**
   ```cpp
   // BAD: Only testing that mock changed, not how component responded
   mockBuffer.AppendBytes(data, size);
   EXPECT_GT(mockBuffer.GetLen(), 0);  // This tests the mock, not your component
   ```

##### **What to Test Instead**
Focus on your component's:

1. **Member variables and state changes:**
   ```cpp
   // GOOD: Test component state
   EXPECT_EQ(component.getStatus(), READY);
   EXPECT_DOUBLE_EQ(component.getPosition(), 10.5);
   ```

2. **Return values from component methods:**
   ```cpp
   // GOOD: Test component behavior
   EXPECT_TRUE(component.initialize());
   EXPECT_FALSE(component.processInvalidData());
   ```

3. **Interactions with mocks using EXPECT_CALL:**
   ```cpp
   // GOOD: Verify component calls mock correctly
   EXPECT_CALL(mockDependency, process(testing::_))
       .Times(1)
       .WillOnce(testing::Return(SUCCESS));
   
   component.doWork();  // This should call mockDependency.process()
   ```

##### **Setting Mock Return Values with EXPECT_CALL**
**Use `EXPECT_CALL` to control what mocked dependencies return - this is the correct way to set mock behavior:**

```cpp
// ✅ CORRECT: Set a specific return value for one call
EXPECT_CALL(mockBuffer, GetLen())
    .WillOnce(testing::Return(100));

// ✅ CORRECT: Set different return values for multiple calls
EXPECT_CALL(mockBuffer, GetLen())
    .WillOnce(testing::Return(50))
    .WillOnce(testing::Return(100))
    .WillOnce(testing::Return(0));

// ✅ CORRECT: Set a return value for all calls
EXPECT_CALL(mockBuffer, IsValid())
    .WillRepeatedly(testing::Return(true));

// ✅ CORRECT: Set expectations with parameters
EXPECT_CALL(mockBuffer, AppendBytes(testing::_, testing::Gt(0)))
    .WillOnce(testing::Return(true));

// ✅ CORRECT: Combine expectations and return values
EXPECT_CALL(mockProcessor, process(testing::_))
    .Times(2)
    .WillOnce(testing::Return(SUCCESS))
    .WillOnce(testing::Return(FAILURE));

// Then test how your component responds to these return values
EXPECT_TRUE(component.initialize());  // Should succeed with first call
EXPECT_FALSE(component.retry());      // Should fail with second call
```

**Key Point**: Always test how YOUR component responds to the mock return values, not the return values themselves.

##### **When Mock Testing is Appropriate**
Only test mock behavior when:
- **Creating the mock itself** (in mock unit tests)
- **Verifying mock setup** works correctly for other tests
- **Debugging mock issues** (temporary, remove after fixing)

##### **Quick Self-Check Questions**
Before writing any test assertion, ask:
- "Am I testing my component's behavior or the mock's behavior?"
- "Does this assertion verify how my component responds to dependencies?"
- "Would this test pass with any mock that returns the same values?"

If you're testing mock behavior, refactor to test your component instead.

### **📋 Pre-Commit L1 Test Review Checklist**

Before completing any L1 test implementation, review your tests against this checklist:

#### **Mock/Fake Usage Review**
- [ ] ✅ All tests focus on the component under test, not mock/fake behavior
- [ ] ✅ No `EXPECT_*` assertions on mock/fake method return values (unless using `EXPECT_CALL`)
- [ ] ✅ No memory content comparisons (`memcmp`) on mock/fake data
- [ ] ✅ No direct testing of mock/fake state changes without testing component response
- [ ] ✅ CMakeLists.txt links `fakes` library first and excludes real dependency sources
- [ ] ✅ Test expectations adapted to fake behavior (e.g., pointer assignment vs memory copying)
- [ ] ✅ Mock return values set using `EXPECT_CALL().WillOnce(Return())` pattern, not direct assertions

#### **Component Behavior Focus**
- [ ] ✅ Tests verify component member variables and state changes
- [ ] ✅ Tests verify component method return values and error handling
- [ ] ✅ Tests verify component interactions with dependencies using `EXPECT_CALL` when needed
- [ ] ✅ Mock return values are used to test component responses, not tested directly
- [ ] ✅ All test assertions answer: "How does my component behave?" not "How do dependencies behave?"

#### **Test Quality Standards**
- [ ] ✅ Test names follow `ClassName_MethodName_ExpectedBehavior` pattern  
- [ ] ✅ All code paths tested including error conditions and edge cases
- [ ] ✅ Doxygen documentation for all test methods
- [ ] ✅ Proper copyright headers in all files
- [ ] ✅ Tests build and run successfully in their own directory

**If any checkbox is unchecked, refactor the tests before committing.**

### L1 Test Directory Structure (Critical - Follow Exactly)

L1 tests have a very specific directory structure that must be followed precisely to integrate with the existing build system.

#### Exact Directory Path Pattern
```
test/utests/tests/[ComponentName]Tests/
```

#### Complete Example for `AampDrmManager.cpp`
```
test/utests/tests/AampDrmManagerTests/
├── AampDrmManagerTests.cpp         # Test runner (matches directory name)
└── CMakeLists.txt                  # Build configuration
```

#### ❌ Common Wrong Locations (DO NOT USE)
- `test/AampDrmManagerTests/` (missing `utests/tests` path)
- `tests/AampDrmManagerTests/` (wrong root directory)
- `test/utests/AampDrmManagerTests/` (missing final `tests/` subdirectory)  
- `src/test/AampDrmManagerTests/` (tests don't go under src)
- `AampDrmManagerTests/` (missing entire test path)

#### Directory Creation Rule
When creating L1 tests for any source file:
1. Start with the exact path: `test/utests/tests/`
2. Add the component name + "Tests": `[ComponentName]Tests/`
3. Create all three required files inside this directory

#### File Naming Convention Summary
- Component file: `VideoDecoder.cpp` 
- Test directory: `VideoDecoderTests/` (add "Tests" suffix)
- Test runner: `VideoDecoderTests.cpp` (matches directory name exactly)
- Test cases: `VideoDecoderTestCases.cpp` (remove trailing 's', add "TestCases")

### Checking for Existing Tests (Important)

**Before creating any new L1 test, always check for existing tests first.**

Legacy tests may exist with slightly different naming conventions. When asked to create tests for a component:

1. **Search for existing test directories** under `test/utests/tests/` that might match:
   - `[ComponentName]Tests/` (current convention)
   - `[ComponentName]Test/` (legacy - singular "Test")
   - `[ComponentName]/` (legacy - no "Test" suffix)
   - Similar component names with variations

2. **Check for existing test files** that might target the same component:
   - Look for test files containing the component name
   - Check for partial matches or abbreviations

3. **If existing tests are found:**
   - **Prompt the user** to confirm whether to:
     - Extend the existing test suite
     - Rename/refactor the existing tests to current convention
     - Create a new test suite (if justified)
   - **Do not automatically create duplicate tests**

4. **If no existing tests are found:**
   - Proceed with creating new tests using the current naming convention

#### Example Prompts for Existing Tests
```
Found existing test directory: test/utests/tests/AampTimeTest/
This appears to test the same component (AampTime.cpp).

Options:
1. Add new test cases to the existing AampTimeTest suite
2. Refactor existing tests to current naming convention (AampTimeTests)
3. Create a separate test suite (please specify reason)

Which approach would you prefer?
```

### Example Test Runner
```cpp
#include <gtest/gtest.h>

int main(int argc, char** argv)
{
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
```

### Example Test Cases
```cpp
#include <gtest/gtest.h>
#include <gmock/gmock.h>

/**
 * @brief Test fixture for VideoStreamManager class
 */
class VideoStreamManagerTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Setup test fixtures
        test_config_.url = "http://example.com/stream.m3u8";
        test_config_.bitrate = 1000000;
        test_config_.resolution = {1920, 1080};
    }

    void TearDown() override {
        // Cleanup after each test
    }

    StreamConfig test_config_;
};

/**
 * @brief Test successful stream manager initialization
 */
TEST_F(VideoStreamManagerTest, Initialize_ValidConfig_ReturnsTrue) {
    VideoStreamManager manager(test_config_);
    
    EXPECT_TRUE(manager.initialize());
    EXPECT_EQ(manager.getState(), StreamState::INITIALIZED);
}

/**
 * @brief Test stream manager initialization with invalid URL
 */
TEST_F(VideoStreamManagerTest, Initialize_InvalidUrl_ReturnsFalse) {
    test_config_.url = "invalid_url";
    VideoStreamManager manager(test_config_);
    
    EXPECT_FALSE(manager.initialize());
    EXPECT_EQ(manager.getState(), StreamState::ERROR);
}

/**
 * @brief Test stream manager with mock dependencies
 */
class MockNetworkInterface {
public:
    MOCK_METHOD(bool, connect, (const std::string& url), ());
    MOCK_METHOD(void, disconnect, (), ());
    MOCK_METHOD(std::vector<uint8_t>, readData, (size_t bytes), ());
};

TEST_F(VideoStreamManagerTest, PlayStream_NetworkFailure_HandlesGracefully) {
    auto mock_network = std::make_shared<MockNetworkInterface>();
    
    // Setup expectations
    EXPECT_CALL(*mock_network, connect(testing::_))
        .WillOnce(testing::Return(false));
    
    VideoStreamManager manager(test_config_, mock_network);
    
    EXPECT_FALSE(manager.startPlayback());
    EXPECT_EQ(manager.getLastError(), "Network connection failed");
}
```

### Example CMakeLists.txt (Using Fakes Infrastructure)
```cmake
# Standard copyright header required
include(GoogleTest)

set(AAMP_ROOT "../../../../")
set(UTESTS_ROOT "../../")
set(EXEC_NAME VideoStreamManagerTests)

include_directories(${AAMP_ROOT} ${AAMP_ROOT}/test/aampcli ${AAMP_ROOT}/subtitle ${AAMP_ROOT}/drm ${AAMP_ROOT}/downloader ${AAMP_ROOT}/drm/helper)
include_directories(${AAMP_ROOT}/tsb/api)

include_directories(${GTEST_INCLUDE_DIRS})
include_directories(${GMOCK_INCLUDE_DIRS})
include_directories(${GLIB_INCLUDE_DIRS})
include_directories(${GSTREAMER_INCLUDE_DIRS})
include_directories(${LIBCJSON_INCLUDE_DIRS})
include_directories(SYSTEM ${UTESTS_ROOT}/mocks)

# CRITICAL: Only include test sources and component under test
# DO NOT include real dependency implementations (e.g., AampGrowableBuffer.cpp)
set(TEST_SOURCES VideoStreamManagerTestCases.cpp
                 VideoStreamManagerTests.cpp)
set(AAMP_SOURCES ${AAMP_ROOT}/VideoStreamManager.cpp)  # Only the component being tested

add_executable(${EXEC_NAME}
               ${TEST_SOURCES}
               ${AAMP_SOURCES})
set_target_properties(${EXEC_NAME} PROPERTIES FOLDER "utests")

if (CMAKE_XCODE_BUILD_SYSTEM)
    # XCode schema target
    xcode_define_schema(${EXEC_NAME})
endif()

if (COVERAGE_ENABLED)
    include(CodeCoverage)
    APPEND_COVERAGE_COMPILER_FLAGS()
endif()

# CRITICAL: Link 'fakes' library FIRST to provide fake implementations
# This ensures fake implementations are used instead of real ones
target_link_libraries(${EXEC_NAME} fakes -lpthread ${GLIB_LINK_LIBRARIES} ${OS_LD_FLAGS} ${GMOCK_LINK_LIBRARIES} ${GTEST_LINK_LIBRARIES})

aamp_utest_run_add(${EXEC_NAME})
```

#### **CMakeLists.txt Best Practices**
- **Always link `fakes` library first** to ensure fake implementations take precedence
- **Only include the component being tested** in `TEST_SOURCES`, not its dependencies
- **Never add real dependency source files** (e.g., don't add `${AAMP_ROOT}/AampGrowableBuffer.cpp`)
- **Use `include_directories(SYSTEM ${UTESTS_ROOT}/mocks)`** for mock headers
- **The `fakes` library provides implementations** for common AAMP components like AampGrowableBuffer

### L1 Test Build Process and Workflow

#### **CRITICAL: Complete Step-by-Step Process for New Tests**

When creating L1 tests for a new component, follow this exact sequence:

##### **Step 1: Create Test Files**
1. Create the test directory: `test/utests/tests/[ComponentName]Tests/`
2. Create all three required files:
   - `[ComponentName]Tests.cpp` (test runner)
   - `[ComponentName]TestCases.cpp` (test cases)
   - `CMakeLists.txt` (build configuration)

##### **Step 2: Initial Build Integration**
**You MUST run the master build script first to integrate new tests:**
```bash
cd test/utests
./run.sh
```

**Why this is required:**
- The master script discovers new test directories
- Creates corresponding build directories under `test/utests/build/tests/[ComponentName]Tests/`
- Integrates new tests with the cmake build system
- Generates makefiles and builds all tests

**❌ Common Error**: Trying to build individual tests before running `./run.sh` will fail with "No rule to make target" errors.

##### **Step 3: Build Directory Structure**
After running `test/utests/run.sh`, you'll have:
```
test/utests/build/tests/[ComponentName]Tests/
├── Makefile                    # Generated makefile
├── [ComponentName]Tests        # Executable test binary
└── [build artifacts]           # Object files, etc.
```

##### **Step 4: Individual Test Development Workflow**
Once initial build is complete, you can iterate efficiently:

1. **Navigate to build directory**:
   ```bash
   cd test/utests/build/tests/[ComponentName]Tests/
   ```
2. **Rebuild after changes**:
   ```bash
   make
   ```
3. **Run single test**:
   ```bash
   ./[ComponentName]Tests
   ```

#### **Build Command Reference**
```bash
# FIRST TIME: Full build integration (required for new tests)
cd test/utests && ./run.sh

# DEVELOPMENT: Individual test rebuild (after initial setup)
cd test/utests/build/tests/VideoStreamManagerTests && make && ./VideoStreamManagerTests

# CI/CD: Full test suite validation
cd test/utests && ./run.sh
```

#### **Common Build Errors and Solutions**

##### **Build System Issues**
- **"No rule to make target [ComponentName]Tests"**: 
  - **Cause**: Trying to build before running master script
  - **Solution**: `cd test/utests && ./run.sh` first
- **"Target not found"**: 
  - **Cause**: Wrong directory name or file naming
  - **Solution**: Verify exact naming convention: `[ComponentName]Tests/`
- **"Makefile not found"**: 
  - **Cause**: Working in wrong directory
  - **Solution**: Navigate to `test/utests/build/[ComponentName]Tests/`

##### **Linking and Dependency Issues**
- **Undefined reference errors**:
  - **Cause**: Missing real implementation, fakes not linked properly
  - **Solution**: Ensure `target_link_libraries(${EXEC_NAME} fakes ...)` comes first
- **Multiple definition errors**:
  - **Cause**: Including both real and fake implementations
  - **Solution**: Remove real implementation source files from CMakeLists.txt
- **Segmentation faults in tests**:
  - **Cause**: Test expects real behavior from fake implementation
  - **Solution**: Adapt test expectations or add null pointer checks

##### **Fake Implementation Issues**
- **Tests failing due to fake behavior**:
  - **Cause**: Tests written for real implementation behavior
  - **Solution**: Adapt expectations (e.g., fake Free() doesn't reset length)
- **Memory-related test failures**:
  - **Cause**: Fake implementations use pointer assignment, not memory copying
  - **Solution**: Don't test memory isolation with fakes
- **Buffer operations not working as expected**:
  - **Cause**: Fake methods may be no-ops or simplified
  - **Solution**: Focus on testing component logic, not dependency behavior

## Python Unit Testing

**For Python testing guidelines and examples, see `python.instructions.md` - Section "Unit Testing with unittest".**

This file focuses on C++ L1 testing with Google Test/Mock. Python has its own testing patterns and is covered in the language-specific instructions.

## JavaScript/TypeScript Testing

### Jest Testing Patterns
```typescript
// MediaPlayer.test.ts
import { MediaPlayer, StreamConfig, PlayerState } from '../src/MediaPlayer';

describe('MediaPlayer', () => {
    let mockConfig: StreamConfig;
    let mockEventHandlers: any;

    beforeEach(() => {
        mockConfig = {
            url: 'http://example.com/stream.m3u8',
            bitrate: 1000000,
            resolution: { width: 1920, height: 1080 },
            codec: 'h264',
            audioEnabled: true
        };

        mockEventHandlers = {
            onPlay: jest.fn(),
            onPause: jest.fn(),
            onError: jest.fn(),
            onTimeUpdate: jest.fn()
        };
    });

    afterEach(() => {
        jest.clearAllMocks();
    });

    describe('initialization', () => {
        it('should initialize successfully with valid config', async () => {
            const player = new MediaPlayer(mockConfig, mockEventHandlers);
            
            await expect(player.initialize()).resolves.toBeUndefined();
        });

        it('should throw error with invalid URL', async () => {
            mockConfig.url = 'invalid-url';
            const player = new MediaPlayer(mockConfig, mockEventHandlers);
            
            await expect(player.initialize()).rejects.toThrow('Invalid stream URL');
        });
    });

    describe('playback', () => {
        let player: MediaPlayer;

        beforeEach(async () => {
            player = new MediaPlayer(mockConfig, mockEventHandlers);
            await player.initialize();
        });

        it('should start playback successfully', async () => {
            await player.play();
            
            expect(mockEventHandlers.onPlay).toHaveBeenCalledTimes(1);
        });

        it('should not start playback twice', async () => {
            await player.play();
            await player.play(); // Second call
            
            expect(mockEventHandlers.onPlay).toHaveBeenCalledTimes(1);
        });

        it('should handle playback errors', async () => {
            // Mock playback failure
            jest.spyOn(player as any, 'performPlay')
                .mockRejectedValue(new Error('Playback failed'));
            
            await player.play();
            
            expect(mockEventHandlers.onError).toHaveBeenCalledWith('Playback failed');
        });
    });

    describe('utility functions', () => {
        it('should format time correctly', () => {
            expect(formatTime(0)).toBe('0:00');
            expect(formatTime(65)).toBe('1:05');
            expect(formatTime(3661)).toBe('1:01:01');
        });

        it('should debounce function calls', (done) => {
            const mockFn = jest.fn();
            const debouncedFn = debounce(mockFn, 100);
            
            debouncedFn();
            debouncedFn();
            debouncedFn();
            
            expect(mockFn).not.toHaveBeenCalled();
            
            setTimeout(() => {
                expect(mockFn).toHaveBeenCalledTimes(1);
                done();
            }, 150);
        });
    });
});
```

## Integration Testing

### Cross-Language Integration Tests
```cpp
// Integration test for Python-C++ bridge
TEST(PythonIntegrationTest, CallPythonFromCpp_Success) {
    // Setup Python environment
    PythonBridge bridge;
    ASSERT_TRUE(bridge.initialize());
    
    // Test calling Python function from C++
    auto result = bridge.callFunction("media_utils", "process_metadata", 
                                    "{\"title\": \"test\", \"duration\": 120}");
    
    EXPECT_TRUE(result.has_value());
    EXPECT_NE(result->find("processed"), std::string::npos);
    
    bridge.cleanup();
}
```

````
