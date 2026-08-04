# If not stated otherwise in this file or this component's LICENSE file the
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

set(CMAKE_CXX_EXTENSIONS OFF)

set(TSB_ROOT ${CMAKE_CURRENT_SOURCE_DIR}/../../../../tsb)

# Add the aamp/test/common directory to the module include path.
# This provides access to the CodeCoverage.cmake module.
list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/../../../../test/common")

if (COVERAGE_ENABLED)
	include(CodeCoverage)
	APPEND_COVERAGE_COMPILER_FLAGS()
endif()

function(tsb_test_run_add test_suite)
	if (CMAKE_XCODE_BUILD_SYSTEM)
		# XCode schema target
		xcode_define_schema(${test_suite})
	endif()

	if (CMAKE_PROJECT_NAME STREQUAL UnitTests)
		aamp_utest_run_add(${test_suite})
	else()
		gtest_discover_tests(${test_suite} TEST_PREFIX ${test_suite}:)
	endif()
endfunction()

include(CTest)
# Note: FetchContent for googletest is now handled in parent test/utests/CMakeLists.txt
# to ensure all test targets have access to gtest/gmock CMake targets
include(GoogleTest)

add_subdirectory (MockFilesystemTests)
add_subdirectory (RealFilesystemTests)
