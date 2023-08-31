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

pkg_check_modules(LIBCJSON REQUIRED libcjson)

set(AAMP_ROOT "../../../../")
set(UTESTS_ROOT "../../")
set(EXEC_NAME Scte35Tests)

include_directories(${AAMP_ROOT} ${AAMP_ROOT}/test/aampcli)
include_directories(${LIBCJSON_INCLUDE_DIRS})
include_directories(${GLIB_INCLUDE_DIRS})

if(CMAKE_SYSTEM_NAME STREQUAL Linux)
    include_directories(${AAMP_ROOT}/Linux/include)
endif(CMAKE_SYSTEM_NAME STREQUAL Linux)

include_directories(${UTESTS_ROOT}/mocks)
include_directories(${AAMP_ROOT}/subtitle)
include_directories(${AAMP_ROOT}/downloader)
include_directories(${AAMP_ROOT}/drm)
include_directories(${AAMP_ROOT}/drm/helper)

set(TEST_SOURCES Scte35Tests.cpp
                 Scte35SectionTests.cpp
                 SpliceInfoTests.cpp)

set(AAMP_SOURCES ${AAMP_ROOT}/test/aampcli/AampSCTE35.cpp
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
    #Set NO_EXCLUDE_DIR to the location of this test so it doesn't get excluded & include common exclude files:
    set(NO_EXCLUDE_DIR "${PROJECT_SOURCE_DIR}/tests/Scte35Tests/*")
    include("${PROJECT_SOURCE_DIR}/cmake_exclude_file.list")
    SETUP_TARGET_FOR_COVERAGE_LCOV(NAME ${EXEC_NAME}_coverage
                              EXECUTABLE ${EXEC_NAME}
                              DEPENDENCIES ${EXEC_NAME})
endif()

# Use the real cJSON library
target_link_directories(${EXEC_NAME} PUBLIC ${LIBCJSON_LIBRARY_DIRS})
target_link_libraries(${EXEC_NAME} cjson fakes ${GLIB_LDFLAGS} ${OS_LD_FLAGS} -lgmock -lgtest -lpthread)

set_target_properties(${EXEC_NAME} PROPERTIES FOLDER "utests")
set_target_properties(${EXEC_NAME} PROPERTIES COMPILE_FLAGS "-Wno-multichar")

gtest_discover_tests(${EXEC_NAME} TEST_PREFIX ${EXEC_NAME}:)
