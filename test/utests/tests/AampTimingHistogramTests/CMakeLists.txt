# If not stated otherwise in this file or this component's license file the
# following copyright and licenses apply:
#
# Copyright 2026 RDK Management
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

set(AAMP_ROOT "../../../../")
set(UTESTS_ROOT "../../")
set(EXEC_NAME AampTimingHistogramTests)

# AampTimingHistogram.h is a header-only class in the AAMP root (alongside AampUtils.h).
# No CommonTestIncludes needed — the class has zero AAMP-specific dependencies.
include_directories(${AAMP_ROOT})
include_directories(${GTEST_INCLUDE_DIRS})
include_directories(${GMOCK_INCLUDE_DIRS})

set(TEST_SOURCES
    AampTimingHistogramTests.cpp
    AampTimingHistogramTestCases.cpp)

# Header-only class: no separate AAMP_SOURCES compilation unit required.
add_executable(${EXEC_NAME} ${TEST_SOURCES})

set_target_properties(${EXEC_NAME} PROPERTIES FOLDER "utests")

if(CMAKE_XCODE_BUILD_SYSTEM)
    xcode_define_schema(${EXEC_NAME})
endif()

if(COVERAGE_ENABLED)
    include(CodeCoverage)
    APPEND_COVERAGE_COMPILER_FLAGS()
endif()

# No AAMP fakes needed — AampTimingHistogram has zero AAMP dependencies.
target_link_libraries(${EXEC_NAME} -pthread
    ${GMOCK_LINK_LIBRARIES} ${GTEST_LINK_LIBRARIES})

aamp_utest_run_add(${EXEC_NAME})
