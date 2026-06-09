# If not stated otherwise in this file or this component's license file the
# following copyright and licenses apply:
#
# Copyright 2024 RDK Management
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

include(GoogleTest)

pkg_check_modules(GMOCK REQUIRED gmock)
pkg_check_modules(GTEST REQUIRED gtest)

# Must match name of this directory so it can be deduced by run.sh

set(AAMPMETRICS_ROOT "../../../")
set(EXEC_NAME CMCDSerialization)

include_directories(${AAMPMETRICS_ROOT})

include_directories(${GTEST_INCLUDE_DIRS})
include_directories(${GMOCK_INCLUDE_DIRS})

set(TEST_SOURCES    CMCDSerializationGTest.cpp
                    CMCDSerializationTest.cpp)

# CRITICAL: link the REAL production sources — CMCDHeaders.cpp, CMCDSerializer.cpp,
# and all four real subclass .cpp files. Do NOT link the Fake library here:
# Fake provides a no-op CMCDHeaders::BuildCMCDCustomHeaders override that would
# intercept the real serializer path and cause all assertions to pass vacuously.
# All symbols needed are provided by the real sources below.
set(ABR_SOURCES
    ${AAMPMETRICS_ROOT}/CMCDHeaders.cpp
    ${AAMPMETRICS_ROOT}/CMCDSerializer.cpp
    ${AAMPMETRICS_ROOT}/VideoCMCDHeaders.cpp
    ${AAMPMETRICS_ROOT}/AudioCMCDHeaders.cpp
    ${AAMPMETRICS_ROOT}/ManifestCMCDHeaders.cpp
    ${AAMPMETRICS_ROOT}/SubtitleCMCDHeaders.cpp)

add_executable(${EXEC_NAME}
               ${TEST_SOURCES}
               ${ABR_SOURCES})

if (CMAKE_XCODE_BUILD_SYSTEM)
  # XCode schema target
  xcode_define_schema(${EXEC_NAME})
endif()

if (COVERAGE_ENABLED)
    include(CodeCoverage)
    APPEND_COVERAGE_COMPILER_FLAGS()
    #Set NO_EXCLUDE_DIR to the location of this test so it doesn't get excluded & include common exclude files:
    set(NO_EXCLUDE_DIR "${PROJECT_SOURCE_DIR}/tests/CMCDSerialization/*")
    include("${PROJECT_SOURCE_DIR}/cmake_exclude_file.list")
    SETUP_TARGET_FOR_COVERAGE_LCOV(NAME ${EXEC_NAME}_coverage
                              EXECUTABLE ${EXEC_NAME}
                              DEPENDENCIES ${EXEC_NAME})
endif()

# No Fake library here — the real CMCDHeaders base and serializer are compiled in
# via ABR_SOURCES above. Linking Fake would pull the no-op BuildCMCDCustomHeaders
# override which defeats the end-to-end assertion purpose of this suite.
target_link_libraries(${EXEC_NAME} ${GMOCK_LINK_LIBRARIES} ${GTEST_LINK_LIBRARIES})
gtest_discover_tests(${EXEC_NAME} TEST_PREFIX ${EXEC_NAME}:)
