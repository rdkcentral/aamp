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
set(EXEC_NAME DrmLegacy)

pkg_check_modules(GMOCK REQUIRED gmock)
pkg_check_modules(GTEST REQUIRED gtest)
pkg_check_modules(LIBCJSON REQUIRED libcjson)
pkg_check_modules(UUID REQUIRED uuid)


include_directories(${AAMP_ROOT} ${AAMP_ROOT}/drm ${AAMP_ROOT}/drm/helper ${AAMP_ROOT}/subtitle ${AAMP_ROOT}/downloader ${AAMP_ROOT}/isobmff ${AAMP_ROOT}/subtec/subtecparser ${AAMP_ROOT}/subtec/libsubtec)
include_directories(${GTEST_INCLUDE_DIRS})
include_directories(${GMOCK_INCLUDE_DIRS})
include_directories(${GLIB_INCLUDE_DIRS})
include_directories(${GSTREAMER_INCLUDE_DIRS})
include_directories(${DRM_ROOT})
include_directories(${DRM_ROOT}/mocks)

message(GSTREAMER_INCLUDE_DIRS=${GSTREAMER_INCLUDE_DIRS})

set(TEST_SOURCES DrmTestsRun.cpp
				 DrmSessionTests.cpp)

set(MOCK_SOURCES ${DRM_ROOT}/mocks/aampMocks.cpp
  				 ${DRM_ROOT}/mocks/FakeID3Metadata.cpp
				 ${DRM_ROOT}/mocks/FakeABRManager.cpp
				 ${DRM_ROOT}/mocks/FakeAampStreamSinkManager.cpp
				 ${DRM_ROOT}/mocks/curlMocks.c
				 ${DRM_ROOT}/mocks/gstMocks.c
				 ${DRM_ROOT}/mocks/pthreadMocks.c
				 ${DRM_ROOT}/mocks/openSslMocks.c)

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
				 ${AAMP_ROOT}/drm/ClearKeyDrmSession.cpp
				 ${AAMP_ROOT}/drm/helper/AampClearKeyHelper.cpp)

add_definitions(-DAAMP_MPD_DRM)
add_definitions(-DUSE_CLEARKEY)

add_executable(${EXEC_NAME}
			   ${DRM_ROOT}/AampDrmTestUtils.cpp
			   ${TEST_SOURCES}
			   ${MOCK_SOURCES}
			   ${AAMP_SOURCES})

set_target_properties(${EXEC_NAME} PROPERTIES FOLDER "utests")


if (CMAKE_XCODE_BUILD_SYSTEM)
  # XCode schema target
  xcode_define_schema(${EXEC_NAME})
endif()

if (COVERAGE_ENABLED)
	include(CodeCoverage)
	APPEND_COVERAGE_COMPILER_FLAGS()
	#Set NO_EXCLUDE_DIR to the location of this test so it doesn't get excluded & include common exclude files:
	set(NO_EXCLUDE_DIR "${PROJECT_SOURCE_DIR}/tests/DrmLegacy/*")
	include("${PROJECT_SOURCE_DIR}/cmake_exclude_file.list")
	SETUP_TARGET_FOR_COVERAGE_LCOV(NAME ${EXEC_NAME}_coverage
							  EXECUTABLE ${EXEC_NAME}
							  DEPENDENCIES ${EXEC_NAME})
endif()

target_link_libraries(${EXEC_NAME} ${UUID_LINK_LIBRARIES} ${OS_LD_FLAGS} pthread ${GLIB_LINK_LIBRARIES} ${LIBCJSON_LINK_LIBRARIES} ${GMOCK_LINK_LIBRARIES} ${GTEST_LINK_LIBRARIES})

gtest_discover_tests(${EXEC_NAME} TEST_PREFIX ${EXEC_NAME}:)
