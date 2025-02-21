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

pkg_check_modules(UUID REQUIRED uuid)

set(AAMP_ROOT "../../../../")
set(UTESTS_ROOT "../../")
set(EXEC_NAME AampCMCDCollectorTests)

include_directories(${AAMP_ROOT} ${AAMP_ROOT}/isobmff ${AAMP_ROOT}/drm ${AAMP_ROOT}/downloader ${AAMP_ROOT}/drm/helper  ${AAMP_ROOT}/middleware/subtitle ${AAMP_ROOT}/subtitle)
include_directories(${AAMP_ROOT}/middleware/subtec/libsubtec)
include_directories(${AAMP_ROOT}/middleware/subtec/subtecparser)
include_directories(${AAMP_ROOT}/middleware/playerjsonobject)

include_directories(${GTEST_INCLUDE_DIRS})
include_directories(${GMOCK_INCLUDE_DIRS})
include_directories(${GLIB_INCLUDE_DIRS})
include_directories(${GSTREAMER_INCLUDE_DIRS})
include_directories(${UUID_INCLUDE_DIRS})
include_directories(${LibXml2_INCLUDE_DIRS})
include_directories(SYSTEM ${UTESTS_ROOT}/mocks)
include_directories(${LIBCJSON_INCLUDE_DIRS})
include_directories(${AAMP_ROOT}/tsb/api)
include_directories(${AAMP_ROOT}/middleware)

set(TEST_SOURCES AampCMCDCollectorTests.cpp
	         AampCMCDCollectorAampTests.cpp)


set(AAMP_SOURCES ${AAMP_ROOT}/AampCMCDCollector.cpp)

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

target_link_libraries(${EXEC_NAME} fakes -pthread ${UUID_LINK_LIBRARIES} ${GLIB_LINK_LIBRARIES} ${OS_LD_FLAGS} ${GMOCK_LINK_LIBRARIES} ${GTEST_LINK_LIBRARIES})

aamp_utest_run_add(${EXEC_NAME})
