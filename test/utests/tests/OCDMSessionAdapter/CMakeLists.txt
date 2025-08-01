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
set(EXEC_NAME OCDMSessionAdapterTests)

include_directories(${AAMP_ROOT} ${AAMP_ROOT}/drm ${AAMP_ROOT}/drm/helper ${AAMP_ROOT}/subtitle ${AAMP_ROOT}/middleware/subtitle ${AAMP_ROOT}/downloader ${AAMP_ROOT}/isobmff ${AAMP_ROOT}/middleware/subtec/subtecparser ${AAMP_ROOT}/subtec/subtecparser ${AAMP_ROOT}/middleware/subtec/playerjsonobject ${AAMP_ROOT}/middleware/subtec/libsubtec ${AAMP_ROOT}/middleware ${AAMP_ROOT}/middleware/drm/ocdm ${AAMP_ROOT}/middleware/drm)
include_directories(${AAMP_ROOT}/tsb/api)
include_directories(${AAMP_ROOT}/middleware/externals)
include_directories("fakes/.")
include_directories(${AAMP_ROOT}/middleware/externals/contentsecuritymanager)
include_directories(${AAMP_ROOT}/middleware/playerLogManager)

if(CMAKE_SYSTEM_NAME STREQUAL Linux)
    include_directories(${AAMP_ROOT}/Linux/include)
endif(CMAKE_SYSTEM_NAME STREQUAL Linux)

include_directories(${LIBCJSON_INCLUDE_DIRS})
include_directories(${GTEST_INCLUDE_DIRS})
include_directories(${GMOCK_INCLUDE_DIRS})
include_directories(${GLIB_INCLUDE_DIRS})
include_directories(${GSTREAMER_INCLUDE_DIRS})
include_directories(SYSTEM ${UTESTS_ROOT}/mocks)
include_directories(${UTESTS_ROOT}/ocdm)

set(TEST_SOURCES FunctionalTests.cpp
		 OCDMSessionAdapterTests.cpp)

set(AAMP_SOURCES ${AAMP_ROOT}/middleware/drm/ocdm/opencdmsessionadapter.cpp
                 ${AAMP_ROOT}/middleware/drm/helper/DrmHelper.cpp
                 ${AAMP_ROOT}/middleware/drm/DrmSession.cpp
                 ${AAMP_ROOT}/ProcessHandler.cpp)

add_definitions(-DUSE_THUNDER_OCDM_API_0_2)
add_definitions(-DUSE_OPENCDM_ADAPTER)

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
    include("${PROJECT_SOURCE_DIR}/cmake_exclude_file.list")
    SETUP_TARGET_FOR_COVERAGE_LCOV(NAME ${EXEC_NAME}_coverage
                              EXECUTABLE ${EXEC_NAME}
                              DEPENDENCIES ${EXEC_NAME})
endif()

target_link_libraries(${EXEC_NAME} fakes -lpthread ${OS_LD_FLAGS} ${GMOCK_LINK_LIBRARIES} ${GTEST_LINK_LIBRARIES} ${LIBCJSON_LINK_LIBRARIES})

aamp_utest_run_add(${EXEC_NAME})
