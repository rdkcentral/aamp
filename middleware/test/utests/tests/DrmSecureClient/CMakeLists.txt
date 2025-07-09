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

set(PLAYER_ROOT "../../../..")
set(UTESTS_ROOT "../..")
set(DRM_ROOT ${UTESTS_ROOT}/drm)
set(SEC_CLIENT_ROOT ${PLAYER_ROOT}/../secclient)
set(EXEC_NAME DrmSecureClient)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DREALTEKCE=1")

# The Secure Client header files must exist in aamp/../secclient/ for these tests to build and run
file(GLOB SEC_CLIENT_HEADERS "${SEC_CLIENT_ROOT}/*")

include_directories(${GTEST_INCLUDE_DIRS})
include_directories(${GMOCK_INCLUDE_DIRS})
include_directories(${UUID_INCLUDE_DIRS})

include_directories(${PLAYER_ROOT} ${PLAYER_ROOT}/drm ${PLAYER_ROOT}/drm/helper ${PLAYER_ROOT}/subtitle ${PLAYER_ROOT}/middleware/subtitle ${PLAYER_ROOT}/downloader ${PLAYER_ROOT}/isobmff ${PLAYER_ROOT}/middleware/subtec/subtecparser ${PLAYER_ROOT}/middleware/playerjsonobject ${PLAYER_ROOT}/middleware/subtec/libsubtec)
include_directories(${PLAYER_ROOT}/tsb/api)
include_directories(${GLIB_INCLUDE_DIRS})
include_directories(${GSTREAMER_INCLUDE_DIRS})
include_directories(${LIBCJSON_INCLUDE_DIRS})
include_directories(${DRM_ROOT})
include_directories(${DRM_ROOT}/ocdm)
include_directories(${PLAYER_ROOT}/../secclient)
include_directories(${DRM_ROOT}/mocks)
include_directories(${PLAYER_ROOT}/externals/playersecmanager)
include_directories(${PLAYER_ROOT}/playerLogManager)

message(GSTREAMER_INCLUDE_DIRS=${GSTREAMER_INCLUDE_DIRS})

set(TEST_SOURCES DrmTestsRun.cpp
				DrmSessionTests.cpp)

if(SEC_CLIENT_HEADERS)
	message("Found Secure Client headers - building ${EXEC_NAME}")

	set(MOCK_SOURCES ${DRM_ROOT}/mocks/curlMocks.c
					 ${PLAYER_ROOT}test/utests/drm/mocks/gstMocks.c
					 ${DRM_ROOT}/mocks/pthreadMocks.c
					 ${DRM_ROOT}/mocks/openSslMocks.c
					 ${DRM_ROOT}/mocks/Fakeopencdm.cpp
					 ${DRM_ROOT}/mocks/Fakesecclient.cpp)

	set(PLAYER_SOURCES ${PLAYER_ROOT}/externals/playersecmanager/ThunderAccessPlayer.cpp
		${PLAYER_ROOT}/externals/playersecmanager/PlayerSecManager.cpp
					 ${PLAYER_ROOT}/drm/DrmSessionFactory.cpp
					 ${PLAYER_ROOT}/drm/helper/DrmHelper.cpp
					 ${PLAYER_ROOT}/drm/helper/DrmHelperFactory.cpp
					 ${PLAYER_ROOT}/drm/DrmSessionManager.cpp
					 ${PLAYER_ROOT}/drm/DrmSession.cpp
					 ${PLAYER_ROOT}/drm/ocdm/opencdmsessionadapter.cpp
					 ${PLAYER_ROOT}/drm/ocdm/opencdmsessionadapter.cpp
					 ${PLAYER_ROOT}/drm/ocdm/OcdmBasicSessionAdapter.cpp
					 ${PLAYER_ROOT}/drm/ocdm/OcdmGstSessionAdapter.cpp
					 ${PLAYER_ROOT}/drm/HlsOcdmBridge.cpp
					 ${PLAYER_ROOT}/drm/helper/ClearKeyHelper.cpp
					 ${PLAYER_ROOT}/drm/helper/WidevineDrmHelper.cpp
					 ${PLAYER_ROOT}/drm/helper/PlayReadyHelper.cpp)

	add_definitions(-DUSE_SHARED_MEMORY)
	add_definitions(-DUSE_OPENCDM -DUSE_OPENCDM_ADAPTER)
	add_definitions(-DUSE_THUNDER_OCDM_API_0_2)
	add_definitions(-DUSE_SECCLIENT)

	add_executable(${EXEC_NAME}
				${TEST_SOURCES}
				${MOCK_SOURCES}
				${PLAYER_SOURCES})
else()
	get_filename_component(SEC_CLIENT_ROOT_ABS ${SEC_CLIENT_ROOT} ABSOLUTE)
    message(WARNING "Secure Client headers not present in ${SEC_CLIENT_ROOT_ABS}, so skipping ${EXEC_NAME}.")

	add_executable(${EXEC_NAME}
				${TEST_SOURCES})
endif()


set_target_properties(${EXEC_NAME} PROPERTIES FOLDER "utests")


if (CMAKE_XCODE_BUILD_SYSTEM)
# XCode schema target
xcode_define_schema(${EXEC_NAME})
endif()

if (COVERAGE_ENABLED)
	include(CodeCoverage)
	APPEND_COVERAGE_COMPILER_FLAGS()
endif()

target_link_libraries(${EXEC_NAME} ${LIBCJSON_LINK_LIBRARIES} ${UUID_LINK_LIBRARIES} pthread ${GLIB_LINK_LIBRARIES} ${OS_LD_FLAGS} ${GMOCK_LINK_LIBRARIES} ${GTEST_LINK_LIBRARIES})

player_utest_run_add(${EXEC_NAME})
