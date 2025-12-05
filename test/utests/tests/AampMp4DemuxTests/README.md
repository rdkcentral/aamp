# AampMp4Demux L1 Unit Tests

## Overview
This directory contains comprehensive L1 unit tests for the AampMp4Demuxer class, which handles MP4 demultiplexing functionality in AAMP.

## Test Structure

### Files
- **AampMp4DemuxerTests.cpp** - Main test entry point with Google Test initialization
- **FunctionalTests.cpp** - All test cases and test fixtures  
- **TestableAampMp4Demuxer.h** - Testable version of AampMp4Demuxer for dependency injection
- **FakeMP4Demux.cpp** - Fake implementation of MP4Demux class for controlled testing
- **CMakeLists.txt** - Build configuration
- **README.md** - This documentation file

### Mocks and Fakes Used
- **MockPrivateInstanceAAMP** - Mocks the PrivateInstanceAAMP dependency
- **MockMp4Demux** - Mocks the Mp4Demux class used internally
- **FakeMP4Demux** - Fake implementation that delegates to MockMp4Demux for controlled testing

## Test Coverage

### Main Test Classes

#### 1. AampMp4DemuxerBaseTests
Basic test fixture for constructor/destructor and simple functionality tests.

#### 2. AampMp4DemuxerMockTests
Advanced test fixture with Mock Mp4Demux injection for detailed testing of the sendSegment() method.

#### 3. AampMp4DemuxerParameterizedTest
Parameterized tests to verify functionality across different media types.

### Test Cases

#### Basic Functionality
- **ConstructorDestructor** - Tests object creation and cleanup
- **SendSegmentWithSamples** - Tests processing MP4 segments containing media samples
- **SendSegmentWithCodecInfo** - Tests processing init segments with codec information
- **SendSegmentWithNullBuffer** - Tests error handling with null input
- **SendSegmentWithEmptyBuffer** - Tests error handling with empty buffer

#### Advanced Scenarios
- **SendSegmentMixedScenario** - Tests with encrypted content and DRM metadata
- **SendSegmentSequence** - Tests multiple sequential calls (init + media segments)
- **SendSegmentDifferentMediaTypes** - Tests with video, audio, and subtitle types

#### Parameterized Tests
- **BasicFunctionalityTest** - Tests functionality across all media types:
  - eMEDIATYPE_VIDEO
  - eMEDIATYPE_AUDIO
  - eMEDIATYPE_SUBTITLE
  - eMEDIATYPE_AUX_AUDIO

## Mock Verification

### Mp4Demux Methods Tested
- **Parse()** - Verifies MP4 data parsing is called
- **GetSamples()** - Verifies sample extraction is called
- **GetCodecInfo()** - Verifies codec information retrieval

### PrivateInstanceAAMP Methods Tested
- **SendStreamTransfer()** - Verifies media samples are sent to pipeline
- **SetStreamCaps()** - Verifies codec information is set in pipeline

## Key Testing Scenarios

### 1. Media Sample Processing
```cpp
// Test verifies that when MP4 data contains samples:
// 1. Parse() is called on Mp4Demux
// 2. GetSamples() returns sample data
// 3. SendStreamTransfer() is called for each sample
```

### 2. Codec Information Handling
```cpp
// Test verifies that for init segments:
// 1. Parse() is called on Mp4Demux
// 2. GetSamples() returns empty (no media samples)
// 3. GetCodecInfo() is called
// 4. SetStreamCaps() is called with codec info
```

### 3. Error Handling
```cpp
// Tests verify proper handling of:
// - Null buffer pointers
// - Empty buffers
// - Invalid parameters
```

### 4. Multi-Media Type Support
```cpp
// Parameterized tests verify functionality for:
// - Video streams
// - Audio streams  
// - Subtitle streams
// - Auxiliary audio streams
```

## Build and Run

### Prerequisites
- Google Test/Google Mock framework
- CMake build system
- C++11 compiler

### Build Commands
```bash
# From the AAMP root directory
mkdir build && cd build
cmake ..
make AampMp4DemuxTests

# Run tests
./test/utests/tests/AampMp4DemuxTests/AampMp4DemuxTests
```

### Expected Output
```
[==========] Running X tests from Y test suites.
[----------] Global test environment set-up.
...
[  PASSED  ] All tests should pass
[==========] X tests from Y test suites ran.
```

## Mock Expectations Pattern

### Typical Test Flow
1. **Setup** - Create mock instances
2. **Configure** - Set mock expectations using EXPECT_CALL
3. **Execute** - Call sendSegment() method
4. **Verify** - Check return values and mock call verification

### Example Mock Configuration
```cpp
// Configure Mp4Demux mock
EXPECT_CALL(*g_mockMp4Demux, Parse(_, _)).Times(1);
EXPECT_CALL(*g_mockMp4Demux, GetSamples())
    .WillOnce(Return(mockSamples));

// Configure PrivateInstanceAAMP mock
EXPECT_CALL(*g_mockPrivateInstanceAAMP, SendStreamTransfer(mediaType, _))
    .Times(2); // Once per sample
```

## Limitations and Future Enhancements

### Current Limitations
- Tests focus on sendSegment() method only
- Other MediaProcessor interface methods are not tested
- Real Mp4Demux parsing logic is not tested (that would be in Mp4Demux unit tests)

### Future Enhancements
- Add tests for enable/disable functionality
- Add performance tests for large buffers
- Add tests for concurrent access patterns
- Add integration tests with real MP4 data

## Dependencies
- **AampMp4Demuxer** - Class under test
- **Mp4Demux** - Mocked internal dependency
- **PrivateInstanceAAMP** - Mocked AAMP instance
- **AampGrowableBuffer** - Buffer management class
- **AampDemuxDataTypes** - Data structure definitions