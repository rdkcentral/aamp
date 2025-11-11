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

# Find includes in corresponding build directories
set( CMAKE_INCLUDE_CURRENT_DIR ON )

add_library (
        RialtoClientIpcImpl
        STATIC

        source/IpcClient.cpp
        source/IpcModule.cpp
        source/MediaPipelineIpc.cpp
        source/MediaPipelineCapabilitiesIpc.cpp
        source/ControlIpc.cpp
        source/MediaKeysIpc.cpp
        source/MediaKeysCapabilitiesIpc.cpp
        source/RialtoCommonIpc.cpp
        source/WebAudioPlayerIpc.cpp
)

set_property (
        TARGET
        RialtoClientIpcImpl
        PROPERTY POSITION_INDEPENDENT_CODE ON
)

target_include_directories (
        RialtoClientIpcImpl

        PUBLIC
        interface

        PRIVATE
        include
        $<TARGET_PROPERTY:RialtoPlayerPublic,INTERFACE_INCLUDE_DIRECTORIES>
        $<TARGET_PROPERTY:RialtoIpcClient,INTERFACE_INCLUDE_DIRECTORIES>
        $<TARGET_PROPERTY:RialtoIpcCommon,INTERFACE_INCLUDE_DIRECTORIES>
        $<TARGET_PROPERTY:RialtoClientCommon,INTERFACE_INCLUDE_DIRECTORIES>
        $<TARGET_PROPERTY:RialtoCommon,INTERFACE_INCLUDE_DIRECTORIES>
        $<TARGET_PROPERTY:RialtoPlayerCommon,INTERFACE_INCLUDE_DIRECTORIES>
)

target_link_libraries (
        RialtoClientIpcImpl

        PRIVATE
        RialtoIpcClient
        RialtoIpcCommon
        RialtoCommon
        RialtoPlayerCommon
        RialtoEthanLog
        RialtoProtobuf
)
