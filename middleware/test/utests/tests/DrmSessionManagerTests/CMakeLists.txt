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

set(PLAYER_ROOT "../../../..")
set(UTESTS_ROOT "../..")
set(DRM_ROOT ${UTESTS_ROOT}/drm)
set(EXEC_NAME DrmSessionManagerTests)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DREALTEKCE=1")

include_directories(${PLAYER_ROOT} ${PLAYER_ROOT}/test/aampcli ${PLAYER_ROOT}/drm ${PLAYER_ROOT}/drm/helper ${PLAYER_ROOT}/subtitle ${PLAYER_ROOT}/middleware/subtitle ${PLAYER_ROOT}/downloader ${PLAYER_ROOT}/isobmff ${PLAYER_ROOT}/subtec/subtecparser ${PLAYER_ROOT}/playerjsonobject ${PLAYER_ROOT}/subtec/libsubtec ${PLAYER_ROOT}/externals/contentsecuritymanager ${PLAYER_ROOT}/externals/contentsecuritymanager/IFirebolt)
include_directories(${PLAYER_ROOT}/tsb/api)
include_directories(${PLAYER_ROOT}/middleware)
include_directories(${PLAYER_ROOT}/drm/ocdm)
include_directories(${PLAYER_ROOT}/externals)
include_directories(${PLAYER_ROOT}/playerLogManager)
include_directories(${PLAYER_ROOT}/baseConversion)
include_directories(${PLAYER_ROOT}/drm)
include_directories(${LIBCJSON_INCLUDE_DIRS})
include_directories(${GTEST_INCLUDE_DIRS})
include_directories(${GMOCK_INCLUDE_DIRS})
include_directories(${GLIB_INCLUDE_DIRS})
include_directories(${GSTREAMER_INCLUDE_DIRS})
include_directories(${UTESTS_ROOT}/mocks)
include_directories(${UTESTS_ROOT}/fakes)
include_directories(${DRM_ROOT})
include_directories(${PLAYER_ROOT}/drm)
include_directories(${DRM_ROOT}/ocdm)
include_directories(${DRM_ROOT}/mocks)
include_directories(${PLAYER_ROOT}/vendor)
include_directories(${PLAYER_ROOT}/playerLogManager)
include_directories(${PLAYER_ROOT}/../base16_64)

set(TEST_SOURCES
	DrmSessionManagerTestCases.cpp
	DrmSessionManagerTests.cpp
)

set(UTILITY_SOURCES
	${PLAYER_ROOT}/playerLogManager/PlayerLogManager.cpp
	${PLAYER_ROOT}/PlayerUtils.cpp
	${PLAYER_ROOT}/baseConversion/_base64.cpp
	${PLAYER_ROOT}/drm/DrmUtils.cpp
)

set(PLAYER_SOURCES
	${PLAYER_ROOT}/drm/DrmSessionManager.cpp
	${PLAYER_ROOT}/drm/DrmSession.cpp
	${PLAYER_ROOT}/drm/helper/DrmHelper.cpp
	${PLAYER_ROOT}/drm/helper/DrmHelperFactory.cpp
	${PLAYER_ROOT}/drm/helper/WidevineDrmHelper.cpp
)

add_definitions(-DAAMP_SIMULATOR_BUILD)

add_executable(${EXEC_NAME}
			   ${TEST_SOURCES}
			   ${UTILITY_SOURCES}
			   ${PLAYER_SOURCES})

set_target_properties(${EXEC_NAME} PROPERTIES FOLDER "utests")


if (CMAKE_XCODE_BUILD_SYSTEM)
  # XCode schema target
  xcode_define_schema(${EXEC_NAME})
endif()

if (COVERAGE_ENABLED)
	include(CodeCoverage)
	APPEND_COVERAGE_COMPILER_FLAGS()
endif()

target_link_libraries(${EXEC_NAME} fakes ${OS_LD_FLAGS} -pthread ${GLIB_LINK_LIBRARIES} ${OS_LD_FLAGS} ${LIBCJSON_LINK_LIBRARIES} ${GMOCK_LINK_LIBRARIES} ${GTEST_LINK_LIBRARIES})

player_utest_run_add(${EXEC_NAME})
