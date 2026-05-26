# StreamAbstractionAAMP_MPD_Tests

This directory contains L1 unit tests for the StreamAbstractionAAMP_MPD class, specifically focusing on testing the `AdvanceTsbFetch()` method.

## Structure

- **CMakeLists.txt**: Build configuration for the test suite
- **StreamAbstractionAAMP_MPD_Tests.cpp**: Main test file with test fixture setup
- **StreamAbstractionAAMP_MPD_AdvanceTsbFetchTests.cpp**: Specific tests for the AdvanceTsbFetch method

## Test Coverage

The test suite covers:

- Basic functionality of AdvanceTsbFetch with valid parameters
- Edge cases with zero and negative seek positions
- Behavior with large seek position values
- Scenarios when TSB (Time Shift Buffer) is not available

## Building and Running

The tests are integrated into the main AAMP unit test framework. To build and run:

```bash
cd /path/to/aamp/build
make StreamAbstractionAAMP_MPD_Tests
./test/utests/tests/StreamAbstractionAAMP_MPD_Tests/StreamAbstractionAAMP_MPD_Tests
```

## Dependencies

- GoogleTest framework
- GMock for mocking
- AAMP core libraries
- Standard system libraries (glib, gstreamer, etc.)

## Notes

The test implementation includes placeholder test cases that should be completed based on the actual behavior and requirements of the `AdvanceTsbFetch()` method. Mock objects and specific assertions should be added as needed to thoroughly test the method's functionality.
