# If not stated otherwise in this file or this component's license file the
# following copyright and licenses apply:
#
# Copyright 2023 RDK Management
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

# Note: GTEST and GMOCK are configured in parent CMakeLists.txt to use FetchContent googletest
# Do not call pkg_check_modules here as it will override the parent settings
pkg_check_modules(GLIB REQUIRED glib-2.0)

set(AAMP_ROOT "../../../../")
set(UTESTS_ROOT "../../")
set(EXEC_NAME IsoBmffHelperTests)

# Include common test directories
include(${CMAKE_CURRENT_LIST_DIR}/../CommonTestIncludes.cmake)
include_directories(${TEST_FILES_DIR})

set(TEST_SOURCES IsoBmffHelperTests.cpp IsoBmffHelperMainTests.cpp)

set(AAMP_SOURCES ${AAMP_ROOT}/isobmff/isobmffhelper.h ${AAMP_ROOT}/isobmff/isobmffhelper.cpp)

add_executable(${EXEC_NAME}
               ${TEST_SOURCES}
               ${AAMP_SOURCES})

if (CMAKE_XCODE_BUILD_SYSTEM)
  # XCode schema target
  xcode_define_schema(${EXEC_NAME})
endif()

if (COVERAGE_ENABLED)
    include(CodeCoverage)
    APPEND_COVERAGE_COMPILER_FLAGS()
endif()

add_compile_definitions(TESTS_DIR="${TEST_FILES_DIR}")
target_link_libraries(${EXEC_NAME} fakes ${GLIB_LINK_LIBRARIES} ${OS_LD_FLAGS} ${GMOCK_LINK_LIBRARIES} ${GTEST_LINK_LIBRARIES} -lpthread)

set_target_properties(${EXEC_NAME} PROPERTIES FOLDER "utests")

gtest_discover_tests(${EXEC_NAME} TEST_PREFIX ${EXEC_NAME}:)

