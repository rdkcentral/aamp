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

include(GoogleTest)

set(AAMP_ROOT "../../../../")
set(UTESTS_ROOT "../../")
set(EXEC_NAME FragmentCollectorMpdTests)

# Include common test directories
include(${CMAKE_CURRENT_LIST_DIR}/../CommonTestIncludes.cmake)

if(CMAKE_SYSTEM_NAME STREQUAL Darwin)
    include_directories(${AAMP_ROOT}/.libs/systemd/src)
endif(CMAKE_SYSTEM_NAME STREQUAL Darwin)

include_directories(${GTEST_INCLUDE_DIRS})
include_directories(${GMOCK_INCLUDE_DIRS})
include_directories(${GLIB_INCLUDE_DIRS})
include_directories(${GSTREAMER_INCLUDE_DIRS})
include_directories(${LIBDASH_INCLUDE_DIRS})
include_directories(${LibXml2_INCLUDE_DIRS})
include_directories(SYSTEM ${UTESTS_ROOT}/mocks)
include_directories(${LIBCJSON_INCLUDE_DIRS})
include_directories(${AAMP_ROOT}/tsb/api)
# Note: .libs/include is NOT used because it contains old gtest/gmock headers
# that conflict with FetchContent googletest. Middleware headers are provided
# via conditional logic in fakes/CMakeLists.txt and tests/CommonTestIncludes.cmake


set(TEST_SOURCES FragmentCollectorMpdTests.cpp
        FragmentCollectorMpdTestCases.cpp)

# Only include the core files under test - the rest will be mocked
set(AAMP_SOURCES ${AAMP_ROOT}/fragmentcollector_mpd.cpp ${AAMP_ROOT}/CachedFragment.cpp ${AAMP_ROOT}/AampVodStitcher.cpp)

add_definitions(-DUSE_SYSTEMD_JOURNAL_PRINT)

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

target_link_libraries(${EXEC_NAME} ${LIBCJSON_LINK_LIBRARIES} fakes -pthread ${LIBDASH_LINK_LIBRARIES} 
    ${GLIB_LINK_LIBRARIES} 
    ${OS_LD_FLAGS} 
    ${GMOCK_LINK_LIBRARIES} 
    ${GTEST_LINK_LIBRARIES}
    ${LIBXML2_LIBRARIES}
    ${CURL_LINK_LIBRARIES}
    -ldl)

aamp_utest_run_add(${EXEC_NAME})
