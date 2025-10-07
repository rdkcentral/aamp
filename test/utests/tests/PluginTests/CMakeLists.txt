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
set(EXEC_NAME PluginTests)

include_directories( ${AAMP_ROOT} ${AAMP_ROOT}/drm ${AAMP_ROOT}/middleware/externals ${AAMP_ROOT}/middleware/drm/helper ${AAMP_ROOT}/middleware ${AAMP_ROOT}/middleware/drm/ocdm ${AAMP_ROOT}/middleware/drm/ ${AAMP_ROOT}/drm/helper ${AAMP_ROOT}/subtitle ${AAMP_ROOT}/middleware/subtitle ${AAMP_ROOT}/downloader ${AAMP_ROOT}/isobmff ${AAMP_ROOT}/middleware/subtec/subtecparser ${AAMP_ROOT}/middleware/playerjsonobject ${AAMP_ROOT}/middleware/subtec/libsubtec
	${AAMP_ROOT}/test/utests/drm/ocdm
	${AAMP_ROOT}/middleware/externals/contentsecuritymanager)

include_directories(${AAMP_ROOT}/middleware/baseConversion)
include_directories(${AAMP_ROOT}/middleware/test/utests/mocks)
include_directories(${AAMP_ROOT}/tsb/api)
include_directories(${AAMP_ROOT}/middleware)
include_directories(${AAMP_ROOT}/middleware/play)
include_directories(${GTEST_INCLUDE_DIRS})
include_directories(${GMOCK_INCLUDE_DIRS})
include_directories(${GLIB_INCLUDE_DIRS})
include_directories(${GSTREAMER_INCLUDE_DIRS})
include_directories(${LIBCJSON_INCLUDE_DIRS})
include_directories(${UUID_INCLUDE_DIRS})
include_directories(${AAMP_ROOT}/middleware)
include_directories(${AAMP_ROOT}/middleware/vendor)
include_directories(${UTESTS_ROOT}/mocks)
include_directories(${AAMP_ROOT}/middleware/playerLogManager)

message(GSTREAMER_INCLUDE_DIRS=${GSTREAMER_INCLUDE_DIRS})

set(TEST_SOURCES PluginTestsRun.cpp PluginTests.cpp)

set(FAKE_SOURCES ${AAMP_ROOT}/middleware/test/utests/fakes/FakeGStreamer.cpp
				 ${AAMP_ROOT}/middleware/test/utests/fakes/FakeSocUtils.cpp)

				 
set(AAMP_SOURCES ${AAMP_ROOT}/plugins/gst-plugins-rdk-aamp/drm/gst/gstaampcdmidecryptor.cpp
				 ${AAMP_ROOT}/plugins/gst-plugins-rdk-aamp/drm/gst/gstaampclearkeydecryptor.cpp
				 ${AAMP_ROOT}/plugins/gst-plugins-rdk-aamp/drm/gst/gstaampplayreadydecryptor.cpp
				 ${AAMP_ROOT}/plugins/gst-plugins-rdk-aamp/drm/gst/gstaampverimatrixdecryptor.cpp
				 ${AAMP_ROOT}/plugins/gst-plugins-rdk-aamp/drm/gst/gstaampwidevinedecryptor.cpp)

if(CMAKE_PLATFORM_UBUNTU)
  add_definitions(-DUBUNTU)
endif()

add_definitions(-DUSE_SHARED_MEMORY)
add_definitions(-DUSE_OPENCDM -DUSE_OPENCDM_ADAPTER)
add_definitions(-DUSE_THUNDER_OCDM_API_0_2)

add_executable(${EXEC_NAME}
			   ${TEST_SOURCES}
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

target_link_libraries(${EXEC_NAME} fakes ${UUID_LINK_LIBRARIES} ${OS_LD_FLAGS} pthread -ldl ${GLIB_LINK_LIBRARIES} ${LIBCJSON_LINK_LIBRARIES} ${GMOCK_LINK_LIBRARIES} ${GTEST_LINK_LIBRARIES} ${GOBJECT_LINK_LIBRARIES} )

aamp_utest_run_add(${EXEC_NAME})
