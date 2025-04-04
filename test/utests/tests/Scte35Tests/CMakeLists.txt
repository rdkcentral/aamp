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

set(AAMP_ROOT "../../../../")
set(UTESTS_ROOT "../../")
set(EXEC_NAME Scte35Tests)

include_directories(${AAMP_ROOT} ${AAMP_ROOT}/test/aampcli)
include_directories(${LIBCJSON_INCLUDE_DIRS})
include_directories(${GLIB_INCLUDE_DIRS})
include_directories(${GTEST_INCLUDE_DIRS})

if(CMAKE_SYSTEM_NAME STREQUAL Linux)
    include_directories(${AAMP_ROOT}/.libs/include)
endif(CMAKE_SYSTEM_NAME STREQUAL Linux)

include_directories(${UTESTS_ROOT}/mocks)
include_directories(${AAMP_ROOT}/subtitle)
include_directories(${AAMP_ROOT}/middleware/subtitle)
include_directories(${AAMP_ROOT}/downloader)
include_directories(${AAMP_ROOT}/drm)
include_directories(${AAMP_ROOT}/drm/helper)
include_directories(${AAMP_ROOT}/tsb/api)
include_directories(${AAMP_ROOT}/middleware)

set(TEST_SOURCES Scte35Tests.cpp
                 Scte35SectionTests.cpp
                 SpliceInfoTests.cpp)

set(AAMP_SOURCES ${AAMP_ROOT}/scte35/AampSCTE35.cpp
                 ${AAMP_ROOT}/AampJsonObject.cpp
                 ${AAMP_ROOT}/AampUtils.cpp
                 ${AAMP_ROOT}/_base64.cpp)

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

# Use the real cJSON library
target_link_directories(${EXEC_NAME} PUBLIC ${LIBCJSON_LIBRARY_DIRS})
target_link_libraries(${EXEC_NAME} cjson fakes ${GLIB_LINK_LIBRARIES} ${OS_LD_FLAGS} ${GTEST_LINK_LIBRARIES} ${GMOCK_LINK_LIBRARIES} -lpthread)

set_target_properties(${EXEC_NAME} PROPERTIES FOLDER "utests")
set_target_properties(${EXEC_NAME} PROPERTIES COMPILE_FLAGS "-Wno-multichar")

aamp_utest_run_add(${EXEC_NAME})
