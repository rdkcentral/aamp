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
        RialtoServerService STATIC

        source/ApplicationSessionServer.cpp
        source/PlaybackService.cpp
        source/CdmService.cpp
        source/ControlService.cpp
        source/SessionServerManager.cpp
        source/MediaPipelineService.cpp
        source/WebAudioPlayerService.cpp
)
set_target_properties (
        RialtoServerService
        PROPERTIES LINK_FLAGS "-Wl,--unresolved-symbols=report-all" POSITION_INDEPENDENT_CODE ON
)
target_include_directories (
        RialtoServerService

        PUBLIC
        include

        PRIVATE
        $<TARGET_PROPERTY:RialtoServerCommon,INTERFACE_INCLUDE_DIRECTORIES>
        $<TARGET_PROPERTY:RialtoServerMain,INTERFACE_INCLUDE_DIRECTORIES>
        $<TARGET_PROPERTY:RialtoServerIpc,INTERFACE_INCLUDE_DIRECTORIES>
        $<TARGET_PROPERTY:RialtoPlayerPublic,INTERFACE_INCLUDE_DIRECTORIES>
        $<TARGET_PROPERTY:RialtoCommon,INTERFACE_INCLUDE_DIRECTORIES>
        $<TARGET_PROPERTY:RialtoServerGstPlayer,INTERFACE_INCLUDE_DIRECTORIES>
        ${GStreamerApp_INCLUDE_DIRS}
)
target_link_libraries (
        RialtoServerService

        PRIVATE
        RialtoLogging
        RialtoServerMain
        RialtoServerIpc
)
