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
set(EXEC_NAME JsBindingTests)

include_directories(${AAMP_ROOT} ${AAMP_ROOT}/jsbindings ${AAMP_ROOT}/jsbindings/PersistentWatermark)
include_directories(${AAMP_ROOT}/drm)
include_directories(${AAMP_ROOT}/drm/helper)
include_directories(${AAMP_ROOT}/subtitle)
include_directories(${AAMP_ROOT}/middleware/subtitle)
include_directories(${AAMP_ROOT}/middleware/closedcaptions)
include_directories(${AAMP_ROOT}/downloader)
include_directories(${AAMP_ROOT}/tsb/api)
include_directories(${AAMP_ROOT}/middleware)
include_directories(${AAMP_ROOT}/externals)
include_directories(${GTEST_INCLUDE_DIRS})
include_directories(${GMOCK_INCLUDE_DIRS})
include_directories(${GLIB_INCLUDE_DIRS})
include_directories(${GSTREAMER_INCLUDE_DIRS})
include_directories(${LIBCJSON_INCLUDE_DIRS})
include_directories(${UUID_INCLUDE_DIRS})

if (JSC_INCDIR)
	include_directories(${JSC_INCDIR})
endif()
message(GSTREAMER_INCLUDE_DIRS=${GSTREAMER_INCLUDE_DIRS})

set(TEST_SOURCES JsBindingRun.cpp
				 JsBindingTests.cpp)

set(MOCK_SOURCES ${DRM_ROOT}/mocks/aampMocks.cpp)

set(AAMP_SOURCES ${AAMP_ROOT}/jsbindings/jsbindings.cpp
				 ${AAMP_ROOT}/jsbindings/jsevent.cpp
				 ${AAMP_ROOT}/jsbindings/jseventlistener.cpp
				 ${AAMP_ROOT}/jsbindings/jsmediaplayer.cpp
				 ${AAMP_ROOT}/jsbindings/jsutils.cpp
				 ${AAMP_ROOT}/jsbindings/PersistentWatermark/PersistentWatermark.cpp
				 ${AAMP_ROOT}/jsbindings/PersistentWatermark/PersistentWatermarkDisplaySequencer.cpp
				 ${AAMP_ROOT}/jsbindings/PersistentWatermark/PersistentWatermarkEventHandler.cpp
				 ${AAMP_ROOT}/jsbindings/PersistentWatermark/PersistentWatermarkPluginAccess.cpp
				 ${AAMP_ROOT}/jsbindings/PersistentWatermark/PersistentWatermarkStorage.cpp)

add_executable(${EXEC_NAME}
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
endif()

target_link_libraries(${EXEC_NAME} fakes ${UUID_LINK_LIBRARIES} ${OS_LD_FLAGS} pthread ${GLIB_LINK_LIBRARIES} ${LIBCJSON_LINK_LIBRARIES} ${GMOCK_LINK_LIBRARIES} ${GTEST_LINK_LIBRARIES} ${GOBJECT_LINK_LIBRARIES})

aamp_utest_run_add(${EXEC_NAME})
