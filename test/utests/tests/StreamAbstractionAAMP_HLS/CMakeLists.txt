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

set(AAMP_ROOT "../../../../")
set(UTESTS_ROOT "../../")
set(EXEC_NAME StreamAbstractionAAMP_HLS)

include_directories(${AAMP_ROOT} ${AAMP_ROOT}/isobmff ${AAMP_ROOT}/drm ${AAMP_ROOT}/drm/helper ${AAMP_ROOT}/drm/ave ${AAMP_ROOT}/subtitle)
include_directories(${AAMP_ROOT}/subtec/libsubtec)
include_directories(${AAMP_ROOT}/subtec/subtecparser)

# Mac OS X
if(CMAKE_SYSTEM_NAME STREQUAL Darwin)
    include_directories(/usr/local/include)
    include_directories(/usr/local/include/libdash)
    set(OS_LD_FLAGS -L/usr/local/lib)

else()
    include_directories(${AAMP_ROOT}/Linux/include)
    include_directories(${AAMP_ROOT}/Linux/include/libdash)
    set(OS_LD_FLAGS -luuid)
endif(CMAKE_SYSTEM_NAME STREQUAL Darwin)

include_directories(${GTEST_INCLUDE_DIRS})
include_directories(${GMOCK_INCLUDE_DIRS})
include_directories(${GLIB_INCLUDE_DIRS})
include_directories(${GSTREAMER_INCLUDE_DIRS})
include_directories(${LibXml2_INCLUDE_DIRS})
include_directories(SYSTEM ${UTESTS_ROOT}/mocks)


set(TEST_SOURCES FunctionalTests.cpp
                 StreamAbstractionAAMP_HLS.cpp)

set(AAMP_SOURCES ${AAMP_ROOT}/streamabstraction.cpp ${AAMP_ROOT}/fragmentcollector_hls.cpp)

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
    set(NO_EXCLUDE_DIR "${PROJECT_SOURCE_DIR}/tests/StreamAbstractionAAMP_HLS/*")
    include("${PROJECT_SOURCE_DIR}/cmake_exclude_file.list")    
    SETUP_TARGET_FOR_COVERAGE_LCOV(NAME ${EXEC_NAME}_coverage
                              EXECUTABLE ${EXEC_NAME}
                              DEPENDENCIES ${EXEC_NAME})
endif()

target_link_libraries(${EXEC_NAME} fakes -lpthread ${GLIB_LDFLAGS} ${OS_LD_FLAGS} -lgmock -lgtest)

set_target_properties(${EXEC_NAME} PROPERTIES FOLDER "utests")

gtest_discover_tests(${EXEC_NAME} TEST_PREFIX ${EXEC_NAME}:)
