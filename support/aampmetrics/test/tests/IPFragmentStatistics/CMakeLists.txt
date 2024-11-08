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
#set(UTESTS_ROOT "../../")
set(EXEC_NAME IPFragmentStatistics)


include_directories(${AAMPMETRICS_ROOT})

include_directories(${GTEST_INCLUDE_DIRS})
include_directories(${GMOCK_INCLUDE_DIRS})
#include_directories(${GLIB_INCLUDE_DIRS})
#include_directories(${GSTREAMER_INCLUDE_DIRS})

set(TEST_SOURCES    IPFragmentStatisticsGTest.cpp
                    IPFragmentStatisticsTest.cpp)
set(ABR_SOURCES ${AAMPMETRICS_ROOT}/IPFragmentStatistics.cpp )

add_executable(${EXEC_NAME}
               ${TEST_SOURCES}
               ${ABR_SOURCES})

if (COVERAGE_ENABLED)
    include(CodeCoverage)
    APPEND_COVERAGE_COMPILER_FLAGS()
    #Set NO_EXCLUDE_DIR to the location of this test so it doesn't get excluded & include common exclude files:
    set(NO_EXCLUDE_DIR "${PROJECT_SOURCE_DIR}/tests/IPFragmentStatistics/*")
    include("${PROJECT_SOURCE_DIR}/cmake_exclude_file.list")
    SETUP_TARGET_FOR_COVERAGE_LCOV(NAME ${EXEC_NAME}_coverage
                              EXECUTABLE ${EXEC_NAME}
                              DEPENDENCIES ${EXEC_NAME})
endif()

target_link_libraries(${EXEC_NAME} Fake ${GMOCK_LINK_LIBRARIES} ${GTEST_LINK_LIBRARIES})
set_target_properties(${EXEC_NAME} PROPERTIES FOLDER "utests")
gtest_discover_tests(${EXEC_NAME} TEST_PREFIX ${EXEC_NAME}:)

