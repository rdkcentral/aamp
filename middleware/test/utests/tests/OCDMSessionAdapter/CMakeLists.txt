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

set(PLAYER_ROOT "../../../../")
set(UTESTS_ROOT "../../")
set(EXEC_NAME OCDMSessionAdapterTests)

include_directories(${PLAYER_ROOT} ${PLAYER_ROOT}/drm ${PLAYER_ROOT}/drm/helper ${PLAYER_ROOT} ${PLAYER_ROOT}/middleware/drm/ocdm ${PLAYER_ROOT}/drm/ ${PLAYER_ROOT}/subtitle ${PLAYER_ROOT}/subtitle ${PLAYER_ROOT}/downloader ${PLAYER_ROOT}/isobmff ${PLAYER_ROOT}/subtec/subtecparser ${PLAYER_ROOT}/subtec/playerjsonobject ${PLAYER_ROOT}/subtec/libsubtec)
include_directories(${PLAYER_ROOT}/tsb/api)
include_directories("fakes/.")

if(CMAKE_SYSTEM_NAME STREQUAL Linux)
    include_directories(${PLAYER_ROOT}/Linux/include)
endif(CMAKE_SYSTEM_NAME STREQUAL Linux)


include_directories(${GTEST_INCLUDE_DIRS})
include_directories(${GMOCK_INCLUDE_DIRS})
include_directories(${GLIB_INCLUDE_DIRS})
include_directories(${GSTREAMER_INCLUDE_DIRS})
include_directories(SYSTEM ${UTESTS_ROOT}/mocks)
include_directories(${UTESTS_ROOT}/ocdm)
include_directories(${PLAYER_ROOT}/drm)
include_directories(${PLAYER_ROOT}/drm/ocdm)
include_directories(${PLAYER_ROOT}/drm/helper)
include_directories(${PLAYER_ROOT}/externals)
include_directories(${PLAYER_ROOT}/playerLogManager)
include_directories(${PLAYER_ROOT}/baseConversion)

set(TEST_SOURCES FunctionalTests.cpp
		 OCDMSessionAdapterTests.cpp)

set(PLAYER_SOURCES ${PLAYER_ROOT}/drm/ocdm/opencdmsessionadapter.cpp
                 ${PLAYER_ROOT}/drm/helper/DrmHelper.cpp
                 ${PLAYER_ROOT}/drm/DrmSession.cpp
                 ${PLAYER_ROOT}/drm/DrmUtils.cpp
    		${PLAYER_ROOT}/externals/PlayerExternalsInterface.cpp
		${PLAYER_ROOT}/externals/PlayerExternalUtils.cpp
		 ${PLAYER_ROOT}/playerLogManager/PlayerLogManager.cpp
     ${PLAYER_ROOT}/ProcessHandler.cpp
     ${PLAYER_ROOT}/PlayerUtils.cpp
     ${PLAYER_ROOT}/baseConversion/_base64.cpp)

add_definitions(-DUSE_THUNDER_OCDM_API_0_2)
add_definitions(-DUSE_OPENCDM_ADAPTER)

add_executable(${EXEC_NAME}
               ${TEST_SOURCES}
	       ${PLAYER_SOURCES})
set_target_properties(${EXEC_NAME} PROPERTIES FOLDER "utests")

if (CMAKE_XCODE_BUILD_SYSTEM)
  # XCode schema target
  xcode_define_schema(${EXEC_NAME})
endif()

if (COVERAGE_ENABLED)
    include(CodeCoverage)
    APPEND_COVERAGE_COMPILER_FLAGS()
    #include("${PROJECT_SOURCE_DIR}/cmake_exclude_file.list")
    SETUP_TARGET_FOR_COVERAGE_LCOV(NAME ${EXEC_NAME}_coverage
                              EXECUTABLE ${EXEC_NAME}
                              DEPENDENCIES ${EXEC_NAME})
endif()

target_link_libraries(${EXEC_NAME} fakes -lpthread ${OS_LD_FLAGS} ${GMOCK_LINK_LIBRARIES} ${GTEST_LINK_LIBRARIES})

player_utest_run_add(${EXEC_NAME})
