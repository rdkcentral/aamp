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

set(CMAKE_C_STANDARD 99)
set(CMAKE_CXX_STANDARD 17)

set( CMAKE_CXX_STANDARD_REQUIRED ON )
include( CheckCCompilerFlag )
include( CheckCXXCompilerFlag )

set(LIB_RIALTO_CLIENT_SOURCES
        source/AttachedSources.cpp
        source/ClientController.cpp
        source/ClientLogControl.cpp
        source/Control.cpp
        source/KeyIdMap.cpp
        source/MediaKeys.cpp
        source/MediaKeysCapabilities.cpp
        source/MediaPipeline.cpp
        source/MediaPipelineCapabilities.cpp
        source/SharedMemoryHandle.cpp
        source/WebAudioPlayer.cpp
        )

add_library( RialtoClient
        SHARED
        ${LIB_RIALTO_CLIENT_SOURCES}

        )

target_compile_options(
        RialtoClient

        PUBLIC
        -DRIALTO_SERVER_SUPPORTS_DECRYPTION=1
        )

target_include_directories(
        RialtoClient

        PUBLIC
        ${PROJECT_SOURCE_DIR}/include
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include/rialto>

        PRIVATE
        $<TARGET_PROPERTY:RialtoPlayerPublic,INTERFACE_INCLUDE_DIRECTORIES>
        $<TARGET_PROPERTY:RialtoPlayerCommon,INTERFACE_INCLUDE_DIRECTORIES>
        $<TARGET_PROPERTY:RialtoClientIpcImpl,INTERFACE_INCLUDE_DIRECTORIES>
        $<TARGET_PROPERTY:RialtoClientCommon,INTERFACE_INCLUDE_DIRECTORIES>
        $<TARGET_PROPERTY:RialtoCommon,INTERFACE_INCLUDE_DIRECTORIES>

        )

set_target_properties(
        RialtoClient
        PROPERTIES LINK_FLAGS "-Wl,--unresolved-symbols=report-all"
                   SOVERSION  ${PROJECT_VERSION_MAJOR}
                   VERSION    ${CMAKE_PROJECT_VERSION}
        )

target_link_libraries(
        RialtoClient

        PRIVATE
        RialtoPlayerCommon
        RialtoClientIpcImpl
        RialtoCommon
        RialtoEthanLog

        Threads::Threads

        )

include( GNUInstallDirs )

install( TARGETS RialtoClient LIBRARY
         DESTINATION ${CMAKE_INSTALL_LIBDIR}
        )
