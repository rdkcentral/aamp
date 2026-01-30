# If not stated otherwise in this file or this component's license file the
# following copyright and licenses apply:
#
# Copyright 2026 RDK Management
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
set(EXEC_NAME AampDRMLicManagerTests)

# Include common test directories
include(${CMAKE_CURRENT_LIST_DIR}/../CommonTestIncludes.cmake)
include_directories(${DRM_ROOT})
include_directories(${DRM_ROOT}/mocks)
include_directories(${AAMP_ROOT}/drm)
include_directories(${AAMP_ROOT}/test/utests/drm/ocdm)
include_directories(${AAMP_ROOT}/.libs/include)

set(TEST_SOURCES
	AampDRMLicManagerTestCases.cpp
	AampDRMLicManagerTests.cpp
)
set(MOCK_SOURCES ${DRM_ROOT}/mocks/aampMocks.cpp)

# Note: Only include files being tested, not their dependencies
# Dependencies should be provided by fakes
set(AAMP_SOURCES
	${AAMP_ROOT}/drm/AampDRMLicManager.cpp
)

add_executable(${EXEC_NAME} ${TEST_SOURCES} ${AAMP_SOURCES} ${MOCK_SOURCES})

set_target_properties(${EXEC_NAME} PROPERTIES FOLDER "utests")

if (CMAKE_XCODE_BUILD_SYSTEM)
  # XCode schema target
  xcode_define_schema(${EXEC_NAME})
endif()

if (COVERAGE_ENABLED)
	include(CodeCoverage)
	APPEND_COVERAGE_COMPILER_FLAGS()
endif()

target_link_libraries(${EXEC_NAME} ${UUID_LINK_LIBRARIES} ${OS_LD_FLAGS} pthread ${GLIB_LINK_LIBRARIES} ${LIBCJSON_LINK_LIBRARIES} ${GMOCK_LINK_LIBRARIES} ${GTEST_LINK_LIBRARIES} ${GOBJECT_LINK_LIBRARIES} fakes)

aamp_utest_run_add(${EXEC_NAME})
