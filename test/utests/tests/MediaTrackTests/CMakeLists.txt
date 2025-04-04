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

set(AAMP_ROOT "../../../../")
set(UTESTS_ROOT "../../")
set(EXEC_NAME MediaTrackTests)

pkg_check_modules(LIBDASH REQUIRED libdash)

include_directories(${AAMP_ROOT} ${AAMP_ROOT}/isobmff ${AAMP_ROOT}/subtitle ${AAMP_ROOT}/middleware/subtitle ${AAMP_ROOT}/middleware/playerisobmff ${AAMP_ROOT}/tsb/api ${AAMP_ROOT}/drm/helper ${AAMP_ROOT}/drm ${AAMP_ROOT}/downloader)
include_directories(${AAMP_ROOT}/middleware/subtec/libsubtec)
include_directories(${AAMP_ROOT}/middleware/subtec/subtecparser)
include_directories(${AAMP_ROOT}/subtec/subtecparser)

include_directories(${GTEST_INCLUDE_DIRS})
include_directories(${GMOCK_INCLUDE_DIRS})
include_directories(${GLIB_INCLUDE_DIRS})
include_directories(${GSTREAMER_INCLUDE_DIRS})
include_directories(${LIBDASH_INCLUDE_DIRS})
include_directories(SYSTEM ${UTESTS_ROOT}/mocks)

set(TEST_SOURCES    MediaTrackTests.cpp
                    MediaTrackTestsMain.cpp)
set(AAMP_SOURCES ${AAMP_ROOT}/streamabstraction.cpp )
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

target_link_libraries(${EXEC_NAME} fakes -pthread ${OS_LD_FLAGS} ${GMOCK_LINK_LIBRARIES} ${GTEST_LINK_LIBRARIES})
gtest_discover_tests(${EXEC_NAME} TEST_PREFIX ${EXEC_NAME}:)
