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

set(AAMP_ROOT "../../../..")
set(UTESTS_ROOT "../..")
set(DRM_ROOT ${UTESTS_ROOT}/drm)
set(SEC_CLIENT_ROOT ${DRM_ROOT}/secclient)
set(EXEC_NAME DrmSecureClient)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DREALTEKCE=1")

# The Secure Client header files must exist in utests/drm/secclient/ for these tests to build and run
file(GLOB SEC_CLIENT_HEADERS "${SEC_CLIENT_ROOT}/*")

pkg_check_modules(GMOCK REQUIRED gmock)
pkg_check_modules(GTEST REQUIRED gtest)
pkg_check_modules(LIBCJSON REQUIRED libcjson)
pkg_check_modules(UUID REQUIRED uuid)

include_directories(${GTEST_INCLUDE_DIRS})
include_directories(${GMOCK_INCLUDE_DIRS})


include_directories(${AAMP_ROOT} ${AAMP_ROOT}/drm ${AAMP_ROOT}/drm/helper ${AAMP_ROOT}/subtitle ${AAMP_ROOT}/downloader ${AAMP_ROOT}/isobmff ${AAMP_ROOT}/subtec/subtecparser ${AAMP_ROOT}/subtec/libsubtec)
include_directories(${GLIB_INCLUDE_DIRS})
include_directories(${GSTREAMER_INCLUDE_DIRS})
include_directories(${DRM_ROOT})
include_directories(${DRM_ROOT}/ocdm)
include_directories(${DRM_ROOT}/secclient)
include_directories(${DRM_ROOT}/mocks)

message(GSTREAMER_INCLUDE_DIRS=${GSTREAMER_INCLUDE_DIRS})

set(TEST_SOURCES DrmTestsRun.cpp
				DrmSessionTests.cpp)

if(SEC_CLIENT_HEADERS)
	message("Found Secure Client headers - building ${EXEC_NAME}")

	set(MOCK_SOURCES ${DRM_ROOT}/mocks/aampMocks.cpp
					 ${DRM_ROOT}/mocks/FakeID3Metadata.cpp
					 ${DRM_ROOT}/mocks/FakeABRManager.cpp
					 ${DRM_ROOT}/mocks/FakeAampStreamSinkManager.cpp
					 ${DRM_ROOT}/mocks/curlMocks.c
					 ${DRM_ROOT}/mocks/gstMocks.c
					 ${DRM_ROOT}/mocks/pthreadMocks.c
					 ${DRM_ROOT}/mocks/openSslMocks.c
					 ${DRM_ROOT}/mocks/Fakeopencdm.cpp
					 ${DRM_ROOT}/mocks/Fakesecclient.cpp)

	set(AAMP_SOURCES ${AAMP_ROOT}/_base64.cpp
					 ${AAMP_ROOT}/AampConfig.cpp
					 ${AAMP_ROOT}/downloader/AampCurlStore.cpp
					 ${AAMP_ROOT}/downloader/AampCurlDownloader.cpp
					 ${AAMP_ROOT}/AampDRMLicPreFetcher.cpp
					 ${AAMP_ROOT}/AampEvent.cpp
					 ${AAMP_ROOT}/AampJsonObject.cpp
					 ${AAMP_ROOT}/AampProfiler.cpp
					 ${AAMP_ROOT}/AampUtils.cpp
					 ${AAMP_ROOT}/iso639map.cpp
					 ${AAMP_ROOT}/drm/AampDRMutils.cpp
					 ${AAMP_ROOT}/drm/aampoutputprotection.cpp
					 ${AAMP_ROOT}/drm/aampdrmsessionfactory.cpp
					 ${AAMP_ROOT}/drm/helper/AampDrmHelper.cpp
					 ${AAMP_ROOT}/drm/helper/AampDrmHelperFactory.cpp
					 ${AAMP_ROOT}/drm/AampDRMSessionManager.cpp
					 ${AAMP_ROOT}/drm/AampDrmSession.cpp
					 ${AAMP_ROOT}/drm/opencdmsessionadapter.cpp
					 ${AAMP_ROOT}/drm/opencdmsessionadapter.cpp
					 ${AAMP_ROOT}/drm/AampOcdmBasicSessionAdapter.cpp
					 ${AAMP_ROOT}/drm/AampOcdmGstSessionAdapter.cpp
					 ${AAMP_ROOT}/drm/AampHlsOcdmBridge.cpp
					 ${AAMP_ROOT}/drm/AampSharedMemorySystem.cpp
					 ${AAMP_ROOT}/drm/helper/AampVgdrmHelper.cpp
					 ${AAMP_ROOT}/drm/helper/AampClearKeyHelper.cpp
					 ${AAMP_ROOT}/drm/helper/AampWidevineDrmHelper.cpp
					 ${AAMP_ROOT}/drm/helper/AampPlayReadyHelper.cpp
					 ${AAMP_ROOT}/isobmff/isobmffbox.cpp 
					 ${AAMP_ROOT}/isobmff/isobmffbuffer.cpp)

	add_definitions(-DUSE_SHARED_MEMORY)
	add_definitions(-DUSE_OPENCDM -DUSE_OPENCDM_ADAPTER)
	add_definitions(-DUSE_THUNDER_OCDM_API_0_2)
	add_definitions(-DUSE_SECCLIENT)

	add_executable(${EXEC_NAME}
				${DRM_ROOT}/AampDrmTestUtils.cpp
				${TEST_SOURCES}
				${MOCK_SOURCES}
				${AAMP_SOURCES})
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
	#Set NO_EXCLUDE_DIR to the location of this test so it doesn't get excluded & include common exclude files:
	set(NO_EXCLUDE_DIR "${PROJECT_SOURCE_DIR}/tests/DrmSecureClient/*")
	include("${PROJECT_SOURCE_DIR}/cmake_exclude_file.list")
	SETUP_TARGET_FOR_COVERAGE_LCOV(NAME ${EXEC_NAME}_coverage
							EXECUTABLE ${EXEC_NAME}
							DEPENDENCIES ${EXEC_NAME})
endif()

target_link_libraries(${EXEC_NAME} ${LIBCJSON_LINK_LIBRARIES} ${UUID_LINK_LIBRARIES} pthread ${GLIB_LINK_LIBRARIES} ${OS_LD_FLAGS} ${GMOCK_LINK_LIBRARIES} ${GTEST_LINK_LIBRARIES})

gtest_discover_tests(${EXEC_NAME} TEST_PREFIX ${EXEC_NAME}:)

