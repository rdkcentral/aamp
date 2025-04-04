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
set(EXEC_NAME IsoBmffBufferTests)
set(TEST_FILES_DIR "${CMAKE_CURRENT_SOURCE_DIR}/testFiles")

include_directories(${AAMP_ROOT} ${AAMP_ROOT}/isobmff ${AAMP_ROOT}/drm ${AAMP_ROOT}/downloader ${AAMP_ROOT}/drm/helper ${AAMP_ROOT}/subtitle ${AAMP_ROOT}/middleware/subtitle)
include_directories(${AAMP_ROOT}/tsb/api)
include_directories(${AAMP_ROOT}/middleware)
#include_directories(${AAMP_ROOT}/middleware/subtec/libsubtec)
#include_directories(${AAMP_ROOT}/middleware/subtec/subtecparser)
#include_directories(${AAMP_ROOT}/middleware/playerjsonobject)

if(CMAKE_SYSTEM_NAME STREQUAL Linux)
    include_directories(${AAMP_ROOT}/Linux/include)
    set(OS_LD_FLAGS -luuid)
endif(CMAKE_SYSTEM_NAME STREQUAL Linux)

include_directories(${GTEST_INCLUDE_DIRS})
include_directories(${GMOCK_INCLUDE_DIRS})
include_directories(${GLIB_INCLUDE_DIRS})
include_directories(${GSTREAMER_INCLUDE_DIRS})
include_directories(${LIBCJSON_INCLUDE_DIRS})
include_directories(${LibXml2_INCLUDE_DIRS})
include_directories(SYSTEM ${UTESTS_ROOT}/mocks)
include_directories(${UTESTS_ROOT}/mocks)
include_directories(${TEST_FILES_DIR})

set(TEST_SOURCES IsoBmffBufferTests.cpp
		 IsoBmffBufferAAMPTests.cpp)

set(AAMP_SOURCES ${AAMP_ROOT}/_base64.cpp ${AAMP_ROOT}/isobmff/isobmffbox.h ${AAMP_ROOT}/isobmff/isobmffbox.cpp ${AAMP_ROOT}/isobmff/isobmffbuffer.h ${AAMP_ROOT}/isobmff/isobmffbuffer.cpp ${AAMP_ROOT}/AampUtils.cpp)

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

aamp_utest_run_add(${EXEC_NAME})
