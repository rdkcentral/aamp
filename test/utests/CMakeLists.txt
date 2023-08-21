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

cmake_minimum_required(VERSION 3.5)

project(UnitTests)

# Don't include CTestDashboardTargets in GUI projects
set_property(GLOBAL PROPERTY CTEST_TARGETS_ADDED 1)
include(CTest)

find_package(PkgConfig REQUIRED)

pkg_check_modules(GTEST REQUIRED gtest)
pkg_check_modules(GMOCK REQUIRED gmock)
pkg_check_modules(GSTREAMER REQUIRED gstreamer-1.0)
pkg_check_modules(GSTREAMERBASE REQUIRED gstreamer-app-1.0)
pkg_check_modules(GLIB REQUIRED glib-2.0)
pkg_check_modules(LibXml2 REQUIRED libxml-2.0)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DUSE_GST1 -ggdb")
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
LIST(APPEND CMAKE_MODULE_PATH "${PROJECT_SOURCE_DIR}")

if (CMAKE_PLATFORM_UBUNTU)
        link_directories(${GTEST_LIBRARY_DIRS} ${GMOCK_LIBRARY_DIRS})
endif()
add_subdirectory(tests)

