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

pkg_check_modules(GTEST QUIET gtest)
pkg_check_modules(GMOCK QUIET gmock)
# Note: We call pkg_check_modules above to satisfy any legacy checks, but we'll override
# the variables below after FetchContent provides the actual googletest targets
set(PKG_GTEST_INCLUDE_DIRS ${GTEST_INCLUDE_DIRS})
set(PKG_GMOCK_INCLUDE_DIRS ${GMOCK_INCLUDE_DIRS})
set(PKG_GTEST_LINK_LIBRARIES ${GTEST_LINK_LIBRARIES})
set(PKG_GMOCK_LINK_LIBRARIES ${GMOCK_LINK_LIBRARIES})

# Middleware headers are now provided via:
# - External middleware-player-interface pkg-config (for CI builds)
# - Internal aamp/middleware paths (for local builds)
# See fakes/CMakeLists.txt and tests/CommonTestIncludes.cmake
# NOTE: .libs/include is NOT used because it contains gtest/gmock that conflict with FetchContent

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

# Fetch googletest via FetchContent BEFORE any subdirectories that need it
# This ensures gtest/gmock CMake targets and include dirs are available
include(FetchContent)
FetchContent_Declare(
	googletest
	URL https://github.com/google/googletest/archive/03597a01ee50ed33e9dfd640b249b4be3799d395.zip
)
# For Windows: Prevent overriding the parent project's compiler/linker settings
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)

# Ensure in-tree gtest/gmock headers are not added as system includes.
# System include ordering can allow .libs/include gmock headers to win,
# causing header/library version mismatch at link time.
foreach(_gt_target IN ITEMS gtest gtest_main gmock gmock_main)
	if(TARGET ${_gt_target})
		set_property(TARGET ${_gt_target} PROPERTY SYSTEM OFF)
		set_property(TARGET ${_gt_target}
			PROPERTY INTERFACE_SYSTEM_INCLUDE_DIRECTORIES "")
	endif()
endforeach()

# Now that googletest targets exist, set up include dirs and link libraries.
# Prefer the googletest instance built in this tree so tests do not pick up the
# external .libs gmock/gtest archives by accident.
set(GTEST_INCLUDE_DIRS "${CMAKE_CURRENT_BINARY_DIR}/_deps/googletest-src/googletest/include")
set(GMOCK_INCLUDE_DIRS "${CMAKE_CURRENT_BINARY_DIR}/_deps/googletest-src/googlemock/include")
if(TARGET gmock AND TARGET gtest)
	set(GMOCK_LINK_LIBRARIES gmock)
	set(GTEST_LINK_LIBRARIES gtest)
elseif(PKG_GTEST_LINK_LIBRARIES AND PKG_GMOCK_LINK_LIBRARIES)
	# Fallback for environments where only pkg-config gtest/gmock is available.
	set(GTEST_INCLUDE_DIRS ${PKG_GTEST_INCLUDE_DIRS})
	set(GMOCK_INCLUDE_DIRS ${PKG_GMOCK_INCLUDE_DIRS})
	set(GTEST_LINK_LIBRARIES ${PKG_GTEST_LINK_LIBRARIES})
	set(GMOCK_LINK_LIBRARIES ${PKG_GMOCK_LINK_LIBRARIES})
else()
	# Last fallback: plain target names.
	set(GTEST_LINK_LIBRARIES gtest)
	set(GMOCK_LINK_LIBRARIES gmock)
endif()

# Put gtest/gmock headers first so they win over any similarly named headers
# pulled from external middleware include roots (e.g. .libs/include).
include_directories(BEFORE ${GTEST_INCLUDE_DIRS})
include_directories(BEFORE ${GMOCK_INCLUDE_DIRS})


# Strip duplicate gtest entries when gmock already brings it in.
foreach(_lib IN LISTS GMOCK_LINK_LIBRARIES)
	list(REMOVE_ITEM GTEST_LINK_LIBRARIES ${_lib})
endforeach()

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

# Note: FetchContent for googletest is now handled at the top of this file
# before add_subdirectory(fakes) to ensure include dirs are available early

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
