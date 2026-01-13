# If not stated otherwise in this file or this component's license file the
# following copyright and licenses apply:
#
# Copyright 2025 RDK Management
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
# http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

cmake_minimum_required(VERSION 3.5)

# Project name
project(CachedFragmentTests)

# Set C++ standard
set(CMAKE_CXX_STANDARD 11)

# Include GoogleTest module
include(GoogleTest)

# Set paths
set(AAMP_ROOT "../../../../")
set(UTESTS_ROOT "../../")

# Find packages using pkg-config (following project pattern)
find_package(PkgConfig REQUIRED)
pkg_check_modules(GTEST REQUIRED gtest)
pkg_check_modules(GMOCK REQUIRED gmock)
pkg_check_modules(GLIB REQUIRED glib-2.0)
pkg_check_modules(LIBCJSON REQUIRED libcjson)
link_directories(${LIBCJSON_LIBRARY_DIRS})

# Include common test directories
include(${CMAKE_CURRENT_LIST_DIR}/../CommonTestIncludes.cmake)

# Source files
set(TEST_SOURCES
    CachedFragmentTests.cpp
    CachedFragmentTestCases.cpp
)

set(AAMP_SOURCES
    ${AAMP_ROOT}/CachedFragment.cpp
)

# Create the test executable
add_executable(${PROJECT_NAME} ${TEST_SOURCES} ${AAMP_SOURCES})

# Link libraries (following project pattern)
target_link_libraries(${PROJECT_NAME} 
    fakes
    -pthread
    ${GLIB_LINK_LIBRARIES}
    ${GMOCK_LINK_LIBRARIES}
    ${GTEST_LINK_LIBRARIES}
    ${LIBCJSON_LINK_LIBRARIES}
)

# Set target properties (following project pattern)
set_target_properties(${PROJECT_NAME} PROPERTIES FOLDER "utests")

# Coverage support (following project pattern)
if (COVERAGE_ENABLED)
    include(CodeCoverage)
    APPEND_COVERAGE_COMPILER_FLAGS()
endif()

# Add test using project function
aamp_utest_run_add(${PROJECT_NAME})

# Print configuration information
message(STATUS "Configured ${PROJECT_NAME}")
message(STATUS "  Test sources: ${TEST_SOURCES}")
message(STATUS "  AAMP sources: ${AAMP_SOURCES}")
message(STATUS "  AAMP root: ${AAMP_ROOT}")
message(STATUS "  GTest libraries: ${GTEST_LINK_LIBRARIES}")
message(STATUS "  GMock libraries: ${GMOCK_LINK_LIBRARIES}")
