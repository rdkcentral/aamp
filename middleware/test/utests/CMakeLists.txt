# If not stated otherwise in this file or this component's license file the
# following copyright and licenses apply:
#
# Copyright 2022 RDK Management
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

cmake_minimum_required(VERSION 3.10.3)

project(UnitTests)

# Don't include CTestDashboardTargets in GUI projects
set_property(GLOBAL PROPERTY CTEST_TARGETS_ADDED 1)
include(CTest)

set(PLAYER_ROOT "../../")

find_package(PkgConfig REQUIRED)

pkg_check_modules(GTEST REQUIRED gtest)
pkg_check_modules(GMOCK REQUIRED gmock)
pkg_check_modules(GSTREAMER REQUIRED gstreamer-1.0)
pkg_check_modules(GSTREAMERBASE REQUIRED gstreamer-app-1.0)
pkg_check_modules(GLIB REQUIRED glib-2.0)
pkg_check_modules(LibXml2 REQUIRED libxml-2.0)
pkg_check_modules(LIBCJSON REQUIRED libcjson)

if (NOT CMAKE_SYSTEM_NAME STREQUAL Darwin)
  pkg_search_module(JSCORE REQUIRED javascriptcoregtk-4.1 javascriptcoregtk-4.0)
  find_path (JSC_INCDIR JavaScriptCore/JavaScript.h HINTS ${JSCORE_INCLUDE_DIRS} REQUIRED)
endif()

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DUSE_GST1 -ggdb")
if(NOT CMAKE_SYSTEM_NAME MATCHES "Linux")
  add_compile_options(-fsanitize=address)
  add_link_options(-fsanitize=address)
endif()
STRING(REPLACE " -Werror=effc++" "" CMAKE_CXX_FLAGS ${CMAKE_CXX_FLAGS}) #disabling effc++ compiler warnings for utest directory
#if (CMAKE_PLATFORM_UBUNTU )
#set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}  -Wsuggest-override")
#endif()

add_definitions(-DENABLE_LOGGING)

add_subdirectory(fakes)
if (CMAKE_XCODE_BUILD_SYSTEM)
  # ARM64 will fail to build utest schemes in XCode unless this is set
  set(CMAKE_GTEST_DISCOVER_TESTS_DISCOVERY_MODE PRE_TEST)
endif()

enable_testing()

# Add the player/test/common directory to the module include path for access to
# common CMake test functionality, like the CodeCoverage.cmake module.
list(APPEND CMAKE_MODULE_PATH "${PROJECT_SOURCE_DIR}/../common")

if (CMAKE_PLATFORM_UBUNTU)
        link_directories(${GTEST_LIBRARY_DIRS} ${GMOCK_LIBRARY_DIRS})
endif()

function(player_utest_run_add test_suite)
	if (CMAKE_RDKE_TEST_RUN)
		# Add the entire googletest test suite to ctest. This means that ctest runs each test
		# suite executable only once, so the test_details.json googletest JSON output file
		# required by RDK-E does not get overwritten.
		add_test(NAME ${test_suite} COMMAND ${test_suite})
	else()
		# Discover tests by running the googletest test suite executable with "--gtest_list_tests",
		# and add each test individually to ctest. This results in ctest running each test suite
		# executable separately for each test, giving greater opportunity for parallelism and
		# stdout per-test rather than per-suite, amongst other advantages. There is no issue with
		# overwriting of googletest JSON output files, because ctest itself handles the output to
		# file via its "--output-junit" argument, which creates a JUnit XML output file instead.
		gtest_discover_tests(${test_suite} DISCOVERY_TIMEOUT 10 TEST_PREFIX ${test_suite}:)
	endif()
endfunction()
add_subdirectory(tests)
