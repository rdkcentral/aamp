#
# If not stated otherwise in this file or this component's LICENSE file the
# following copyright and licenses apply:
#
# Copyright 2022 
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
#

set( CMAKE_CXX_STANDARD 17 )

set( CMAKE_CXX_STANDARD_REQUIRED ON )
include( CheckCXXCompilerFlag )

# Find includes in corresponding build directories
set( CMAKE_INCLUDE_CURRENT_DIR ON )

add_library(
        RialtoServerMain STATIC

        ${PROTO_SRCS}
        ${PROTO_HEADERS}

        source/MediaPipelineServerInternal.cpp
        source/MediaPipelineCapabilities.cpp
        source/ActiveRequests.cpp
        source/DataReaderFactory.cpp
        source/DataReaderV1.cpp
        source/DataReaderV2.cpp
        source/NeedMediaData.cpp
        source/SharedMemoryBuffer.cpp
        source/MediaKeysServerInternal.cpp
        source/MediaKeysCapabilities.cpp
        source/MediaKeySession.cpp
        source/MainThread.cpp
        source/WebAudioPlayerServerInternal.cpp
        source/ControlServerInternal.cpp
        source/HeartbeatProcedure.cpp
        source/TextTrackAccessor.cpp
        source/TextTrackSession.cpp
        )

target_include_directories(
        RialtoServerMain

        PUBLIC
        interface
        ${GStreamerApp_INCLUDE_DIRS}

        PRIVATE
        include
        $<TARGET_PROPERTY:RialtoPlayerPublic,INTERFACE_INCLUDE_DIRECTORIES>
        $<TARGET_PROPERTY:RialtoServerGstPlayer,INTERFACE_INCLUDE_DIRECTORIES>
        $<TARGET_PROPERTY:RialtoServerCommon,INTERFACE_INCLUDE_DIRECTORIES>
        $<TARGET_PROPERTY:RialtoPlayerCommon,INTERFACE_INCLUDE_DIRECTORIES>
        $<TARGET_PROPERTY:RialtoWrappers,INTERFACE_INCLUDE_DIRECTORIES>
        $<TARGET_PROPERTY:RialtoCommon,INTERFACE_INCLUDE_DIRECTORIES>
        )

set_target_properties(
        RialtoServerMain
        PROPERTIES LINK_FLAGS "-Wl,--unresolved-symbols=report-all"

        )

target_compile_options(
        RialtoServerMain

        PRIVATE
        )

target_link_libraries(
        RialtoServerMain

        PRIVATE
        RialtoLogging
        RialtoServerGstPlayer
        RialtoWrappers
        RialtoCommon
        RialtoProtobuf
        Threads::Threads
        )

install(
        TARGETS RialtoServerMain
        DESTINATION lib
        )
