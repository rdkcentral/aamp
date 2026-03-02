# LibPlayerVendorInterface CMakeLists.txt
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

cmake_minimum_required(VERSION 3.5)
project(playervendorinterface)

find_package(PkgConfig REQUIRED)

pkg_check_modules(GSTREAMER REQUIRED gstreamer-1.0>=1.18.0)
pkg_check_modules(GSTREAMERBASE REQUIRED gstreamer-app-1.0)

set(PLAYERVENDORINTERFACE_DEPENDS
    ${GSTREAMER_LINK_LIBRARIES}
    ${GSTREAMERBASE_LINK_LIBRARIES})

set(PLAYERVENDORINTERFACE_SOURCES
    SocInterface.cpp)

set(PLAYERVENDORINTERFACE_DEFINES "")

pkg_check_modules(PLAYERVENDORINTERFACEIMPL playervendorinterfaceimpl)

if (PLAYERVENDORINTERFACEIMPL_FOUND)
    list(APPEND PLAYERVENDORINTERFACE_DEFINES "-DPLAYERVENDORINTERFACE_IMPL_AVAILABLE")
    include_directories(${PLAYERVENDORINTERFACEIMPL_INCLUDE_DIRS})
    list(APPEND PLAYERVENDORINTERFACE_SOURCES
        ${PLAYERVENDORINTERFACEIMPL_LINK_LIBRARIES})
else()
    include_directories(${CMAKE_CURRENT_SOURCE_DIR}/default)
    list(APPEND PLAYERVENDORINTERFACE_SOURCES
        default/DefaultSocInterface.cpp)
endif()

add_library(${PROJECT_NAME} SHARED ${PLAYERVENDORINTERFACE_SOURCES})

set_target_properties(${PROJECT_NAME} PROPERTIES COMPILE_FLAGS "${PLAYERVENDORINTERFACE_DEFINES}")
target_link_libraries(${PROJECT_NAME} "${PLAYERVENDORINTERFACE_DEPENDS}")
    
