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

pkg_check_modules(UUID REQUIRED uuid)
pkg_check_modules(GOBJECT REQUIRED gobject-2.0)

set(PLAYER_ROOT "../../../..")
set(UTESTS_ROOT "../..")
set(DRM_ROOT ${UTESTS_ROOT}/drm)
set(RFC_ROOT ${UTESTS_ROOT}/rfc)
set(EXEC_NAME DrmOcdm)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DREALTEKCE=1")

include_directories(${PLAYER_ROOT} ${PLAYER_ROOT}/drm  ${PLAYER_ROOT}/subtitle ${PLAYER_ROOT}/drm/helper ${PLAYER_ROOT}/drm/ocdm ${PLAYER_ROOT}/drm/ ${PLAYER_ROOT}/subtitle ${PLAYER_ROOT}/downloader ${PLAYER_ROOT}/isobmff ${PLAYER_ROOT}/subtec/subtecparser ${PLAYER_ROOT}/playerJsonObject ${PLAYER_ROOT}/subtec/libsubtec ${PLAYER_ROOT}/externals/contentsecuritymanager ${PLAYER_ROOT}/externals/contentsecuritymanager/IFirebolt)
include_directories(${PLAYER_ROOT}/tsb/api)
include_directories(${PLAYER_ROOT}/externals)
include_directories(${GTEST_INCLUDE_DIRS})
include_directories(${GMOCK_INCLUDE_DIRS})
include_directories(${GLIB_INCLUDE_DIRS})
include_directories(${GSTREAMER_INCLUDE_DIRS})
include_directories(${LIBCJSON_INCLUDE_DIRS})
include_directories(${UUID_INCLUDE_DIRS})
include_directories(${DRM_ROOT})
include_directories(${RFC_ROOT})
include_directories(${DRM_ROOT}/ocdm)
include_directories(${UTESTS_ROOT}/drm/mocks)
include_directories(${UTESTS_ROOT}/mocks)
include_directories(${PLAYER_ROOT}/vendor)
include_directories(${PLAYER_ROOT}/drm)
include_directories(${PLAYER_ROOT}/drm/helper)
include_directories(${PLAYER_ROOT}/drm/ocdm)
include_directories(${PLAYER_ROOT}/externals)
include_directories(${PLAYER_ROOT}/playerLogManager)
include_directories(${PLAYER_ROOT}/baseConversion)

message(GSTREAMER_INCLUDE_DIRS=${GSTREAMER_INCLUDE_DIRS})

set(TEST_SOURCES DrmTestsRun.cpp
				 #DrmSessionTests.cpp //To Do:We don't have private instance in middleware need to implement
				 DrmUtilsTests.cpp
				 DrmHLSTests.cpp
				 DrmHelperTests.cpp)

set(FAKE_SOURCES ${PLAYER_ROOT}/test/utests/fakes/FakeSocUtils.cpp)

set(PLAYER_SOURCES ${PLAYER_ROOT}/drm/DrmUtils.cpp
					${PLAYER_ROOT}/baseConversion/_base64.cpp
					${PLAYER_ROOT}/PlayerUtils.cpp
					${PLAYER_ROOT}/drm/DrmSessionFactory.cpp
					${PLAYER_ROOT}/drm/helper/DrmHelper.cpp
					${PLAYER_ROOT}/drm/helper/DrmHelperFactory.cpp
					${PLAYER_ROOT}/drm/DrmSessionManager.cpp
					${PLAYER_ROOT}/drm/DrmSession.cpp
					${PLAYER_ROOT}/drm/DrmJsonObject.cpp
					${PLAYER_ROOT}/drm/ocdm/opencdmsessionadapter.cpp
					${PLAYER_ROOT}/drm/ocdm/OcdmBasicSessionAdapter.cpp
					${PLAYER_ROOT}/drm/ocdm/OcdmGstSessionAdapter.cpp
					${PLAYER_ROOT}/drm/HlsOcdmBridge.cpp
					${PLAYER_ROOT}/drm/HlsDrmSessionManager.cpp
					${PLAYER_ROOT}/drm/processProtectionHls.cpp
					${PLAYER_ROOT}/drm/helper/ClearKeyHelper.cpp
					${PLAYER_ROOT}/drm/helper/WidevineDrmHelper.cpp
					${PLAYER_ROOT}/drm/helper/PlayReadyHelper.cpp
					${PLAYER_ROOT}/playerLogManager/PlayerLogManager.cpp
					${UTESTS_ROOT}/drm/mocks/gstMocks.c
					${PLAYER_ROOT}/ProcessHandler.cpp
					${PLAYER_ROOT}/externals/PlayerExternalsInterface.cpp
					${PLAYER_ROOT}/externals/contentsecuritymanager/ContentSecurityManager.cpp
					${PLAYER_ROOT}/externals/contentsecuritymanager/ContentSecurityManagerSession.cpp
					${PLAYER_ROOT}/externals/PlayerExternalUtils.cpp)


add_definitions(-DAAMP_HLS_DRM)
add_definitions(-DUSE_SHARED_MEMORY)
add_definitions(-DUSE_OPENCDM -DUSE_OPENCDM_ADAPTER)
add_definitions(-DUSE_THUNDER_OCDM_API_0_2)

add_executable(${EXEC_NAME}
			   ${TEST_SOURCES}
			   ${PLAYER_SOURCES}
			   ${FAKE_SOURCES})


set_target_properties(${EXEC_NAME} PROPERTIES FOLDER "utests")


if (CMAKE_XCODE_BUILD_SYSTEM)
  # XCode schema target
  xcode_define_schema(${EXEC_NAME})
endif()

if (COVERAGE_ENABLED)
	include(CodeCoverage)
	APPEND_COVERAGE_COMPILER_FLAGS()
	#Set NO_EXCLUDE_DIR to the location of this test so it doesn't get excluded & include common exclude files:
	set(NO_EXCLUDE_DIR "${PROJECT_SOURCE_DIR}/tests/DrmOcdm/*")
	#include("${PROJECT_SOURCE_DIR}/cmake_exclude_file.list")
	SETUP_TARGET_FOR_COVERAGE_LCOV(NAME ${EXEC_NAME}_coverage
							  EXECUTABLE ${EXEC_NAME}
							  DEPENDENCIES ${EXEC_NAME})
endif()

target_link_libraries(${EXEC_NAME} ${UUID_LINK_LIBRARIES} ${OS_LD_FLAGS} pthread -ldl ${GLIB_LINK_LIBRARIES} ${LIBCJSON_LINK_LIBRARIES} ${GMOCK_LINK_LIBRARIES} ${GTEST_LINK_LIBRARIES} ${GOBJECT_LINK_LIBRARIES} fakes)

player_utest_run_add(${EXEC_NAME})
