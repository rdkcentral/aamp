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

set(AAMP_ROOT "../../../../")
set(UTESTS_ROOT "../../")
set(EXEC_NAME Base64AAMP)

include_directories(${AAMP_ROOT}/middleware)
include_directories(${AAMP_ROOT}/middleware/baseConversion)
include_directories(${AAMP_ROOT} ${AAMP_ROOT}/drm ${AAMP_ROOT}/drm/helper ${AAMP_ROOT}/subtitle ${AAMP_ROOT}/middleware/subtitle)

include_directories(${GTEST_INCLUDE_DIRS})
include_directories(${GMOCK_INCLUDE_DIRS})
include_directories(${GLIB_INCLUDE_DIRS})
include_directories(${GSTREAMER_INCLUDE_DIRS})
include_directories(${UTESTS_ROOT}/mocks)

set(TEST_SOURCES Base64Tests.cpp
                 Base64AAMPTests.cpp)

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

target_link_libraries(${EXEC_NAME} fakes ${GLIB_LINK_LIBRARIES} ${OS_LD_FLAGS} ${GMOCK_LINK_LIBRARIES} ${GTEST_LINK_LIBRARIES} -lpthread)

set_target_properties(${EXEC_NAME} PROPERTIES FOLDER "utests")

aamp_utest_run_add(${EXEC_NAME})
