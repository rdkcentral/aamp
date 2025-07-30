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

pkg_check_modules(UUID REQUIRED uuid)
pkg_check_modules(GOBJECT REQUIRED gobject-2.0)

set(MW_ROOT "../../../..")
set(UTESTS_ROOT "../..")
set(DRM_ROOT ${UTESTS_ROOT}/drm)
set(EXEC_NAME PluginsTests)

file(GLOB SEC_CLIENT_HEADERS "${SEC_CLIENT_ROOT}/*")

include_directories(${MW_ROOT} ${MW_ROOT}/gst-plugins/drm/gst)
include_directories(${MW_ROOT}/drm)
include_directories(${MW_ROOT}/drm/helper)
include_directories(${MW_ROOT}/subtitle)
include_directories(${MW_ROOT}/closedcaptions)
include_directories(${MW_ROOT}/downloader)
include_directories(${MW_ROOT}/tsb/api)
include_directories(${MW_ROOT}/externals/contentsecuritymanager)
include_directories(${MW_ROOT}/playerLogManager)
include_directories(${MW_ROOT}/vendor)
include_directories(${GTEST_INCLUDE_DIRS})
include_directories(${GMOCK_INCLUDE_DIRS})
include_directories(${GLIB_INCLUDE_DIRS})
include_directories(${DRM_ROOT}/ocdm)
include_directories(${UTESTS_ROOT}/mocks)
include_directories(${UTESTS_ROOT}/drm/mocks)
include_directories(${GSTREAMER_INCLUDE_DIRS})
include_directories(${GSTREAMERBASE_INCLUDE_DIRS})
include_directories(${LIBCJSON_INCLUDE_DIRS})
include_directories(${UUID_INCLUDE_DIRS})

if (JSC_INCDIR)
	include_directories(${JSC_INCDIR})
endif()
message(GSTREAMER_INCLUDE_DIRS=${GSTREAMER_INCLUDE_DIRS})

set(TEST_SOURCES PlayerClearKeyDecryptorTests.cpp
				PlayerPlayReadyDecryptorTests.cpp
				PlayerVeriMatrixDecryptorTests.cpp
				PlayerWidevineDecryptorTests.cpp
				PluginsRun.cpp)
				
add_definitions(-DUSE_OPENCDM_ADAPTER)

if (CMAKE_PLATFORM_UBUNTU)
	add_definitions(-DUBUNTU)
endif()

set(MW_SOURCES
				${MW_ROOT}/gst-plugins/drm/gst/gstcdmidecryptor.cpp
				${MW_ROOT}/gst-plugins/drm/gst/gstclearkeydecryptor.cpp
				${MW_ROOT}/gst-plugins/drm/gst/gstplayreadydecryptor.cpp
				${MW_ROOT}/gst-plugins/drm/gst/gstverimatrixdecryptor.cpp
				${MW_ROOT}/gst-plugins/drm/gst/gstwidevinedecryptor.cpp
				${MW_ROOT}/playerLogManager/PlayerLogManager.cpp)

add_executable(${EXEC_NAME}
			   ${TEST_SOURCES}
			   ${MOCK_SOURCES}
			   ${MW_SOURCES})

set_target_properties(${EXEC_NAME} PROPERTIES FOLDER "utests")

if (CMAKE_XCODE_BUILD_SYSTEM)
  # XCode schema target
  xcode_define_schema(${EXEC_NAME})
endif()

if (COVERAGE_ENABLED)
	include(CodeCoverage)
	APPEND_COVERAGE_COMPILER_FLAGS()
endif()

target_link_libraries(${EXEC_NAME} fakes ${UUID_LINK_LIBRARIES} ${OS_LD_FLAGS} -lpthread ${GLIB_LINK_LIBRARIES} ${LIBCJSON_LINK_LIBRARIES} ${GMOCK_LINK_LIBRARIES} ${GTEST_LINK_LIBRARIES} ${GOBJECT_LINK_LIBRARIES} ${GSTREAMERBASE_LINK_LIBRARIES} ${GSTREAMER_LINK_LIBRARIES})

player_utest_run_add(${EXEC_NAME})
