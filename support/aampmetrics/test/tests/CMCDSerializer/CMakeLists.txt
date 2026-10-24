# If not stated otherwise in this file or this component's license file the
# following copyright and licenses apply:
#
# Copyright 2022 RDK Management
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
set(EXEC_NAME CMCDSerializer)

include_directories(${AAMPMETRICS_ROOT})

include_directories(${GTEST_INCLUDE_DIRS})
include_directories(${GMOCK_INCLUDE_DIRS})

set(TEST_SOURCES    CMCDSerializerGTest.cpp
                    CMCDSerializerTest.cpp)

# Link the REAL serializer implementation directly (not faked) so tests
# exercise the actual primitives. CMCDSerializer.cpp has no dependency on
# CMCDHeaders so no fake override can intercept it.
set(SERIALIZER_SOURCES ${AAMPMETRICS_ROOT}/CMCDSerializer.cpp)

add_executable(${EXEC_NAME}
               ${TEST_SOURCES}
               ${SERIALIZER_SOURCES})

if (CMAKE_XCODE_BUILD_SYSTEM)
  # XCode schema target
  xcode_define_schema(${EXEC_NAME})
endif()

if (COVERAGE_ENABLED)
    include(CodeCoverage)
    APPEND_COVERAGE_COMPILER_FLAGS()
    #Set NO_EXCLUDE_DIR to the location of this test so it doesn't get excluded & include common exclude files:
    set(NO_EXCLUDE_DIR "${PROJECT_SOURCE_DIR}/tests/CMCDSerializer/*")
    include("${PROJECT_SOURCE_DIR}/cmake_exclude_file.list")
    SETUP_TARGET_FOR_COVERAGE_LCOV(NAME ${EXEC_NAME}_coverage
                              EXECUTABLE ${EXEC_NAME}
                              DEPENDENCIES ${EXEC_NAME})
endif()

# Do NOT link Fake here — the serializer tests exercise pure free functions with
# no CMCDHeaders dependency. Fake provides a no-op BuildCMCDCustomHeaders override
# that is unnecessary here and creates a brittle cross-binary dependency.
target_link_libraries(${EXEC_NAME} ${GMOCK_LINK_LIBRARIES} ${GTEST_LINK_LIBRARIES})
gtest_discover_tests(${EXEC_NAME} TEST_PREFIX ${EXEC_NAME}:)
