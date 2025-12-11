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

set(AAMP_ROOT "../../../../")
set(UTESTS_ROOT "../../")
set(DRM_ROOT ${UTESTS_ROOT}/drm)
set(EXEC_NAME AampDRMLicPreFetcherTests)
set(CMAKE_CXX_STANDARD 14)
include_directories(${AAMP_ROOT} ${AAMP_ROOT}/isobmff ${AAMP_ROOT}/middleware/playerisobmff ${AAMP_ROOT}/drm ${AAMP_ROOT}/downloader ${AAMP_ROOT}/drm/helper ${AAMP_ROOT}/drm/ave ${AAMP_ROOT}/subtitle ${AAMP_ROOT}/middleware/subtitle ${AAMP_ROOT}/dash/xml ${AAMP_ROOT}/dash/utils ${AAMP_ROOT}/dash/mpd ${AAMP_ROOT}/support/aampabr ${AAMP_ROOT}/middleware/externals/contentsecuritymanager)
include_directories(${AAMP_ROOT}/middleware/baseConversion)
include_directories(${AAMP_ROOT}/middleware/subtec/libsubtec)
include_directories(${AAMP_ROOT}/middleware/subtec/subtecparser)
include_directories(${AAMP_ROOT}/middleware/playerjsonobject)
include_directories(${AAMP_ROOT}/middleware/closedcaptions)
include_directories(${AAMP_ROOT}/subtec/subtecparser)
include_directories(${AAMP_ROOT}/tsb/api)
include_directories(${AAMP_ROOT}/middleware)
include_directories(${AAMP_ROOT}/middleware/externals)
include_directories(${AAMP_ROOT}/middleware/playerLogManager)
include_directories(${AAMP_ROOT}/middleware/drm/helper)
include_directories(${AAMP_ROOT}/middleware/test/utests/mocks)
include_directories(${DRM_ROOT})
include_directories(${DRM_ROOT}/mocks)
include_directories(${GTEST_INCLUDE_DIRS})
include_directories(${GMOCK_INCLUDE_DIRS})
include_directories(${GLIB_INCLUDE_DIRS})
include_directories(${GSTREAMER_INCLUDE_DIRS})
include_directories(${LIBDASH_INCLUDE_DIRS})
include_directories(${LibXml2_INCLUDE_DIRS})
include_directories(SYSTEM ${UTESTS_ROOT}/mocks)
include_directories(${LIBCJSON_INCLUDE_DIRS})
include_directories(${AAMP_ROOT}/middleware/vendor)

set(TEST_SOURCES
	AampDRMLicPreFetcherTestCases.cpp
	AampDRMLicPreFetcherTests.cpp
)
set(MOCK_SOURCES ${DRM_ROOT}/mocks/aampMocks.cpp)

# Note: Only include files being tested, not their dependencies
# Dependencies should be provided by fakes
set(AAMP_SOURCES
	${AAMP_ROOT}/AampDRMLicPreFetcher.cpp
    ${AAMP_ROOT}/AampUtils.cpp
    ${AAMP_ROOT}/CachedFragment.cpp
)

add_executable(${EXEC_NAME} ${TEST_SOURCES} ${AAMP_SOURCES} ${MOCK_SOURCES})

target_link_libraries(${EXEC_NAME} fakes -lpthread -lgtest -lgmock)

add_test(${EXEC_NAME} ${EXEC_NAME})
