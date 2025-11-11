# Player Interface Microtests

A test infrastructure using GoogleTest C++ testing and mocking framework, to verify the behavior of individual objects in the player interface.

CTest is a testing tool that is part of CMake, and is used to automatically execute all the tests, and provides a report of the tests run, whether they passed/failed and time taken. It can be configured to run in parallel, generate JSON and JUnit reports, and control output verbosity.

These tests should be extended on addition of any new functionality.

These tests should be run on any change to:
 - Test any new functionality
 - Check for any regression caused by the change
 - Check the build has not been broken due to an API change

NOTE: Writing microtests is a really useful tool in improving code quality but if they are implemented incorrectly they can have a detrimental impact on build times and fail to find the errors they are intended to catch.

 - **See [GoogleTest User's Guide](https://google.github.io/googletest/)**
 - **See [Testing With CMake and CTest](https://cmake.org/cmake/help/book/mastering-cmake/chapter/Testing%20With%20CMake%20and%20CTest.html)**

## Pre-requisites to building:

Optionally run `install-middleware.sh` to install any required dependencies, which may include:
 - Headers from dependent libraries
 - GoogleTest and GoogleMock

## Build and run microtests using script:

From the *utests* folder run:

```sh
./run.sh
```

### Script Options

- `-c` : Additionally build and generate coverage tests and coverage report.
- `-e` : Perform RDK-E build, which generates a combined JSON report (`L1Report.json`) across all test modules.
- `-h` : Halt coverage tests on error.

### Examples

- **Standard build and test:**  
  ```
  ./run.sh
  ```
- **RDK-E build with combined JSON report:**  
  ```
  ./run.sh -e
  ```
  - The combined report is generated as `utests/build/L1Report.json`
- **Run with code coverage:**  
  ```
  ./run.sh -c
  ```
  - The aggregated coverage report is generated as `utests/build/CombinedCoverage/index.html`
- **Halt coverage tests on error:**  
  ```
  ./run.sh -c -h
  ```

### Output

- By default, CTest runs the tests in parallel (`-j 4`).  
- Reports are generated in both JSON and JUnit formats:
  - JUnit: `ctest-results.xml`
  - JSON: (when `-e` is used) `L1Report.json`
- Individual test module results are in `test_detail.json` files within each test directory.

## Manual build and test

From the *utests* folder:

```sh
mkdir build
cd build
cmake ../
make
ctest
```

## CTest Usage

Some examples of additional parameters that can be used with ctest:

- Output logging (verbose):
  ```
  ctest --verbose
  ```
- Output logging when a test fails:
  ```
  ctest --output-on-failure
  ```
- Run tests in parallel (default in script is 4):
  ```
  ctest -j 4
  ```
- Run specific tests using regex selectors (see `ctest --help`):
  ```
  ctest -R PrivateInstance.*PositionAlready
  ```

## Coverage Report

To check line coverage in microtests:

From the *utests* directory, run:
```
./run.sh -c
```
The aggregated coverage results can be found in:
```
utests/build/CombinedCoverage/index.html
```

**Note:**  
Generating coverage takes longer than running tests without coverage options.

## Directory Structure

### fakes

A CMake library containing fake/stub implementations of class methods, to allow compiling of class under test in isolation; these fakes are common to all tests.

Implementation can be extended to call a mock instance, to allow testing of expectations. For example, see `FakePrivateInstanceAAMP.cpp` where some methods being used by existing tests have been extended.

Files in here will need to be updated for any API changes/additions made to player interface modules, otherwise unresolved symbol errors are likely to be seen.

### mocks

A directory containing Google mocks; these mocks are common to all tests.

See [gMock for Dummies](https://google.github.io/googletest/gmock_for_dummies.html)

### tests

A directory containing the tests for each of the player interface's modules, each in their own sub-directory.
The `CMakeLists.txt` file adds all the modules' subdirectories.

See [CMake Modules - GoogleTest](https://cmake.org/cmake/help/latest/module/GoogleTest.html)

### *Module* test folder

One or more GoogleTest executables generated from CMake.

The `CMakeLists.txt` contains the instructions for creating the module's target, e.g.:
- EXEC_NAME to be same as directory name (required for JSON report generation)
- Necessary include paths for that module
- The module source file
- The Google test file(s)
- The directive `gtest_discover_tests` to discover Google tests from the test executable

General guidance:
 - A test class for each area of functionality of the module under test.
 - Use mocks and test expectations for external calls made by the module.
 - Microtests should run quickly and not significantly impact build times (all tests should run in seconds).
 - Use of real implementations vs mocks can be controlled in the `CMakeLists.txt` as needed.

For guidance on creating GoogleTest please see [GoogleTest User's Guide](https://google.github.io/googletest/).

### Running debugger

You can run gdb on an individual test via:
```
cd build/tests/SomeTest
gdb ..
```
