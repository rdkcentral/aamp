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

# GStreamer reference implementation of the Rialto IMediaPipeline API.
#
# Produces the static library RialtoReferencePipeline when GStreamer
# (>= 1.18, with gstreamer-app) and the Rialto public headers are available;
# otherwise configures nothing so that consumers can test for the target.
#
# Inputs:
#   RIALTO_INCLUDE_DIRS  Rialto public headers (IMediaPipeline.h, ...). Defaults
#                        to <aamp>/.libs/include/rialto.

cmake_minimum_required(VERSION 3.10)

project(RialtoReferencePipeline CXX)

find_package(PkgConfig REQUIRED)
find_package(Threads REQUIRED)

pkg_check_modules(RIALTO_REF_GST IMPORTED_TARGET gstreamer-1.0>=1.18 gstreamer-base-1.0 gstreamer-app-1.0)
if(NOT RIALTO_REF_GST_FOUND)
    message(STATUS "rialto-reference: GStreamer (>= 1.18, app) not found - not building the GStreamer reference pipeline")
    return()
endif()

set(RIALTO_REF_RIALTO_INCLUDE_DIRS ${RIALTO_INCLUDE_DIRS})
if(NOT RIALTO_REF_RIALTO_INCLUDE_DIRS)
    set(RIALTO_REF_RIALTO_INCLUDE_DIRS "${CMAKE_CURRENT_SOURCE_DIR}/../.libs/include/rialto")
endif()
if(NOT EXISTS "${RIALTO_REF_RIALTO_INCLUDE_DIRS}/IMediaPipeline.h")
    message(STATUS "rialto-reference: Rialto headers not found in ${RIALTO_REF_RIALTO_INCLUDE_DIRS} - not building")
    return()
endif()

add_library(RialtoReferencePipeline STATIC
    source/GstReferencePipeline.cpp
    source/MediaCaps.cpp
    source/ReferenceLog.cpp
)

set_target_properties(RialtoReferencePipeline PROPERTIES
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED ON
    POSITION_INDEPENDENT_CODE ON
)

target_include_directories(RialtoReferencePipeline
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
        ${RIALTO_REF_RIALTO_INCLUDE_DIRS}
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/source
)

target_link_libraries(RialtoReferencePipeline
    PUBLIC
        PkgConfig::RIALTO_REF_GST
        Threads::Threads
)

message(STATUS "rialto-reference: building GStreamer reference pipeline (GStreamer ${RIALTO_REF_GST_gstreamer-1.0_VERSION})")
