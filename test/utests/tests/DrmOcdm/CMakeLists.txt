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

set(AAMP_ROOT "../../../..")
set(UTESTS_ROOT "../..")
set(DRM_ROOT ${UTESTS_ROOT}/drm)
set(RFC_ROOT ${UTESTS_ROOT}/rfc)
set(EXEC_NAME DrmOcdm)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DREALTEKCE=1")

include_directories(${AAMP_ROOT} ${AAMP_ROOT}/drm ${AAMP_ROOT}/middleware/externals ${AAMP_ROOT}/middleware/drm/helper ${AAMP_ROOT}/middleware ${AAMP_ROOT}/middleware/drm/ocdm ${AAMP_ROOT}/middleware/drm/ ${AAMP_ROOT}/drm/helper ${AAMP_ROOT}/subtitle ${AAMP_ROOT}/middleware/subtitle ${AAMP_ROOT}/downloader ${AAMP_ROOT}/isobmff ${AAMP_ROOT}/middleware/subtec/subtecparser ${AAMP_ROOT}/middleware/playerjsonobject ${AAMP_ROOT}/middleware/subtec/libsubtec ${AAMP_ROOT}/middleware/externals/contentsecuritymanager)
include_directories(${AAMP_ROOT}/middleware/baseConversion)
include_directories(${AAMP_ROOT}/tsb/api)
include_directories(${AAMP_ROOT}/middleware)
include_directories(${AAMP_ROOT}/middleware/play)
include_directories(${GTEST_INCLUDE_DIRS})
include_directories(${GMOCK_INCLUDE_DIRS})
include_directories(${GLIB_INCLUDE_DIRS})
include_directories(${GSTREAMER_INCLUDE_DIRS})
include_directories(${LIBCJSON_INCLUDE_DIRS})
include_directories(${UUID_INCLUDE_DIRS})
include_directories(${DRM_ROOT})
include_directories(${RFC_ROOT})
include_directories(${DRM_ROOT}/ocdm)
include_directories(${DRM_ROOT}/mocks)
include_directories(${AAMP_ROOT}/middleware)
include_directories(${AAMP_ROOT}/middleware/vendor)
include_directories(${AAMP_ROOT}/test/utests/mocks)
include_directories(${AAMP_ROOT}/middleware/playerLogManager)

message(GSTREAMER_INCLUDE_DIRS=${GSTREAMER_INCLUDE_DIRS})

set(TEST_SOURCES DrmTestsRun.cpp
				 DrmSessionTests.cpp
				 DrmHelperTests.cpp
				 DrmUtilsTests.cpp)

set(MOCK_SOURCES ${DRM_ROOT}/mocks/aampMocks.cpp
				 ${DRM_ROOT}/mocks/FakeID3Metadata.cpp
				 ${DRM_ROOT}/mocks/FakeABRManager.cpp
				 ${DRM_ROOT}/mocks/FakeAampStreamSinkManager.cpp
 				 ${DRM_ROOT}/mocks/curlMocks.c
				 ${AAMP_ROOT}/middleware/test/utests/drm/mocks/gstMocks.c
				 ${DRM_ROOT}/mocks/pthreadMocks.c
				 ${DRM_ROOT}/mocks/openSslMocks.c
				 ${DRM_ROOT}/mocks/Fakeopencdm.cpp)

set(FAKE_SOURCES ${UTESTS_ROOT}/fakes/FakeAampRfc.cpp
				 ${UTESTS_ROOT}/fakes/FakePlayerExternalsInterface.cpp
				 ${UTESTS_ROOT}/fakes/FakeProcessProtectionHls.cpp
				 ${AAMP_ROOT}/middleware/test/utests/fakes/FakeSocUtils.cpp
				 ${AAMP_ROOT}/test/utests/fakes/FakePlayerSecInterface.cpp
				 ${AAMP_ROOT}/test/utests/fakes/FakeContentSecurityManager.cpp
				 ${AAMP_ROOT}/test/utests/fakes/FakeContentSecurityManagerSession.cpp
				 ${AAMP_ROOT}/test/utests/fakes/FakePlayerScheduler.cpp
				 ${AAMP_ROOT}/test/utests/fakes/FakeBase64.cpp
				 ${AAMP_ROOT}/test/utests/fakes/FakePlayerLogManager.cpp)

set(AAMP_SOURCES ${AAMP_ROOT}/AampConfig.cpp
				 ${AAMP_ROOT}/drm/AampDRMLicManager.cpp
				 ${AAMP_ROOT}/ProcessHandler.cpp
				 ${AAMP_ROOT}/middleware/drm/DrmUtils.cpp
				 ${AAMP_ROOT}/downloader/AampCurlStore.cpp
				 ${AAMP_ROOT}/downloader/AampCurlDownloader.cpp
				 ${AAMP_ROOT}/AampDRMLicPreFetcher.cpp
				 ${AAMP_ROOT}/AampEvent.cpp
				 ${AAMP_ROOT}/AampJsonObject.cpp
				 ${AAMP_ROOT}/middleware/drm/DrmJsonObject.cpp
				 ${AAMP_ROOT}/AampProfiler.cpp
				 ${AAMP_ROOT}/scte35/AampSCTE35.cpp
				 ${AAMP_ROOT}/AampUtils.cpp
				 ${AAMP_ROOT}/iso639map.cpp
				 ${AAMP_ROOT}/middleware/drm/DrmUtils.cpp
				 ${AAMP_ROOT}/middleware/drm/base64.cpp
				 ${AAMP_ROOT}/middleware/PlayerUtils.cpp
				 ${AAMP_ROOT}/middleware/drm/DrmSessionFactory.cpp
				 ${AAMP_ROOT}/middleware/drm/helper/DrmHelper.cpp
				 ${AAMP_ROOT}/middleware/drm/helper/DrmHelperFactory.cpp
				 ${AAMP_ROOT}/middleware/drm/DrmSessionManager.cpp
				 ${AAMP_ROOT}/middleware/drm/DrmSession.cpp
				 ${AAMP_ROOT}/middleware/drm/ocdm/opencdmsessionadapter.cpp
				 ${AAMP_ROOT}/middleware/drm/ocdm/opencdmsessionadapter.cpp
				 ${AAMP_ROOT}/middleware/drm/ocdm/OcdmBasicSessionAdapter.cpp
				 ${AAMP_ROOT}/middleware/drm/ocdm/OcdmGstSessionAdapter.cpp

				 ${AAMP_ROOT}/middleware/drm/HlsOcdmBridge.cpp
				 ${AAMP_ROOT}/middleware/drm/HlsDrmSessionManager.cpp
				 ${AAMP_ROOT}/middleware/drm/helper/ClearKeyHelper.cpp
				 ${AAMP_ROOT}/middleware/drm/helper/WidevineDrmHelper.cpp
				 ${AAMP_ROOT}/middleware/drm/helper/PlayReadyHelper.cpp
				 ${AAMP_ROOT}/isobmff/isobmffbox.h
				 ${AAMP_ROOT}/isobmff/isobmffbox.cpp
				 ${AAMP_ROOT}/isobmff/isobmffbuffer.h
				 ${AAMP_ROOT}/isobmff/isobmffbuffer.cpp)

add_definitions(-DUSE_SHARED_MEMORY)
add_definitions(-DUSE_OPENCDM -DUSE_OPENCDM_ADAPTER)
add_definitions(-DUSE_THUNDER_OCDM_API_0_2)

add_executable(${EXEC_NAME}
			   ${DRM_ROOT}/DrmTestUtils.cpp
			   ${TEST_SOURCES}
			   ${MOCK_SOURCES}
			   ${AAMP_SOURCES}
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
	include("${PROJECT_SOURCE_DIR}/cmake_exclude_file.list")
	SETUP_TARGET_FOR_COVERAGE_LCOV(NAME ${EXEC_NAME}_coverage
							  EXECUTABLE ${EXEC_NAME}
							  DEPENDENCIES ${EXEC_NAME})
endif()

target_link_libraries(${EXEC_NAME} ${UUID_LINK_LIBRARIES} ${OS_LD_FLAGS} pthread -ldl ${GLIB_LINK_LIBRARIES} ${LIBCJSON_LINK_LIBRARIES} ${GMOCK_LINK_LIBRARIES} ${GTEST_LINK_LIBRARIES} ${GOBJECT_LINK_LIBRARIES} )

aamp_utest_run_add(${EXEC_NAME})
