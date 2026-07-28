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

cmake_minimum_required(VERSION 3.18)

project(UnitTests)

# Don't include CTestDashboardTargets in GUI projects
set_property(GLOBAL PROPERTY CTEST_TARGETS_ADDED 1)
include(CTest)

set(AAMP_ROOT "../../")
if (CMAKE_SYSTEM_NAME STREQUAL Darwin)
	include_directories(${CMAKE_CURRENT_SOURCE_DIR}/fakes)
endif()
include_directories(${AAMP_ROOT}/support/aampabr)
include_directories(${AAMP_ROOT}/support/aampmetrics)
# Note: Middleware include paths are now handled conditionally in:
# - fakes/CMakeLists.txt (for fakes library)
# - tests/CommonTestIncludes.cmake (for individual test targets)
# This allows using external middleware-player-interface when available

find_package(PkgConfig REQUIRED)

pkg_check_modules(OPENSSL REQUIRED openssl)
include_directories(${OPENSSL_INCLUDE_DIRS})

pkg_check_modules(GTEST REQUIRED gtest)
pkg_check_modules(GMOCK REQUIRED gmock)
# Override pkg-config gtest/gmock variables to use FetchContent googletest
# This prevents version conflicts between system gtest and FetchContent gtest
set(GTEST_INCLUDE_DIRS "${CMAKE_CURRENT_BINARY_DIR}/_deps/googletest-src/googletest/include")
set(GMOCK_INCLUDE_DIRS "${CMAKE_CURRENT_BINARY_DIR}/_deps/googletest-src/googlemock/include")
# Override link libraries to use FetchContent targets (will be set after FetchContent_MakeAvailable)
set(GTEST_LINK_LIBRARIES "gtest")
set(GMOCK_LINK_LIBRARIES "gmock;gtest")
# Add FetchContent gtest/gmock include dirs globally (gmock needs gtest headers too)
include_directories(${GTEST_INCLUDE_DIRS})
include_directories(${GMOCK_INCLUDE_DIRS})
# Use installed external middleware headers
# NOTE: .libs/include contains gtest/gmock subdirs from middleware build which conflict
# with the test framework's gtest. WORKAROUND: Temporarily rename .libs/include/gtest
# and .libs/include/gmock to .DISABLED before building tests:
#   mv .libs/include/gtest .libs/include/gtest.DISABLED
#   mv .libs/include/gmock .libs/include/gmock.DISABLED
include_directories(${AAMP_ROOT}/.libs/include)
# GMOCK_LINK_LIBRARIES already contains libgtest (gmock depends on gtest).
# Strip any items from GTEST_LINK_LIBRARIES that are already covered by
# GMOCK_LINK_LIBRARIES so that tests linking both variables don't put
# libgtest on the link line twice, which triggers the macOS linker warning
# "ignoring duplicate libraries".  Tests that only link GTEST_LINK_LIBRARIES
# are unaffected because they don't use gmock and won't see duplication.
foreach(_lib IN LISTS GMOCK_LINK_LIBRARIES)
	list(REMOVE_ITEM GTEST_LINK_LIBRARIES ${_lib})
endforeach()
pkg_check_modules(GSTREAMER REQUIRED gstreamer-1.0)
pkg_check_modules(GSTREAMERBASE REQUIRED gstreamer-app-1.0)
pkg_check_modules(GLIB REQUIRED glib-2.0)
pkg_check_modules(LIBDASH REQUIRED libdash)
pkg_check_modules(LibXml2 REQUIRED libxml-2.0)
pkg_check_modules(LIBCJSON REQUIRED libcjson)

# External middleware-player-interface libraries (optional for backward compatibility)
# When available via pkg-config, these are preferred over internal aamp/middleware
pkg_check_modules(PLAYERFBINTERFACE libplayerfbinterface)
pkg_check_modules(BASECONVERSION libbaseconversion)
pkg_check_modules(PLAYERLOGMANAGER libplayerlogmanager)
pkg_check_modules(SUBTEC libsubtec)

if (NOT CMAKE_SYSTEM_NAME STREQUAL Darwin)
	pkg_search_module(JSCORE REQUIRED javascriptcoregtk-4.1 javascriptcoregtk-4.0)
	find_path (JSC_INCDIR JavaScriptCore/JavaScript.h HINTS ${JSCORE_INCLUDE_DIRS} REQUIRED)
endif()

set(CMAKE_CXX_STANDARD 17)
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

add_subdirectory(fakes)

# Build net_persona_fitter as a library target so tests that pull in priv_aamp.cpp
# can link against it (priv_aamp.cpp references aamptrace::NetPersonaFitter via net_trace.h)
if(NOT TARGET net_persona_fitter)
	add_subdirectory(${AAMP_ROOT}/simnet ${CMAKE_CURRENT_BINARY_DIR}/simnet)
endif()
if (CMAKE_XCODE_BUILD_SYSTEM)
	# ARM64 will fail to build utest schemes in XCode unless this is set
	set(CMAKE_GTEST_DISCOVER_TESTS_DISCOVERY_MODE PRE_TEST)
endif()

enable_testing()

# Add the aamp/test/common directory to the module include path for access to
# common CMake test functionality, like the CodeCoverage.cmake module.
list(APPEND CMAKE_MODULE_PATH "${PROJECT_SOURCE_DIR}/../common")

if (CMAKE_PLATFORM_UBUNTU)
	link_directories(${GTEST_LIBRARY_DIRS} ${GMOCK_LIBRARY_DIRS})
endif()

# Fetch googletest via FetchContent to ensure all test targets use the same version
# This must happen before add_subdirectory(tests) so that gtest/gmock CMake targets
# are available to all test subdirectories
include(FetchContent)
FetchContent_Declare(
	googletest
	URL https://github.com/google/googletest/archive/03597a01ee50ed33e9dfd640b249b4be3799d395.zip
)
# For Windows: Prevent overriding the parent project's compiler/linker settings
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)

function(aamp_utest_run_add test_suite)
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
