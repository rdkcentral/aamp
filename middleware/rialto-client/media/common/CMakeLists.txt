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
        RialtoPlayerCommon
        STATIC

        source/ByteWriter.cpp
        source/MediaFrameWriterFactory.cpp
        source/MediaFrameWriterV1.cpp
        source/MediaFrameWriterV2.cpp
        source/SchemaVersion.cpp
        source/TypeConverters.cpp
    )

set_property (
        TARGET
        RialtoPlayerCommon
        PROPERTY POSITION_INDEPENDENT_CODE ON
)

target_include_directories(
        RialtoPlayerCommon

        PUBLIC
        interface

        PRIVATE
        include
        $<TARGET_PROPERTY:RialtoPlayerPublic,INTERFACE_INCLUDE_DIRECTORIES>
        $<TARGET_PROPERTY:RialtoPlayerCommon,INTERFACE_INCLUDE_DIRECTORIES>
        $<TARGET_PROPERTY:RialtoCommon,INTERFACE_INCLUDE_DIRECTORIES>
        $<TARGET_PROPERTY:RialtoLogging,INTERFACE_INCLUDE_DIRECTORIES>
    )

set (
        RIALTO_PLAYER_COMMON_PUBLIC_HEADERS
        interface/ShmCommon.h
        interface/IMediaFrameWriter.h
)

install (
        FILES  ${RIALTO_PLAYER_COMMON_PUBLIC_HEADERS}
        DESTINATION  ${CMAKE_INSTALL_INCLUDEDIR}/rialto
)

target_link_libraries (
        RialtoPlayerCommon

        PRIVATE
        RialtoLogging
        RialtoProtobuf
    )
