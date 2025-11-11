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

# RIALTO-197: deprecated-declarations error in the latest stable2 for gstreamer.
# Should be removed once the issue is fixed.
add_compile_options(
  "-Wno-deprecated-declarations"
)

find_package( PkgConfig REQUIRED )
pkg_check_modules( GStreamerApp REQUIRED IMPORTED_TARGET gstreamer-app-1.0 gstreamer-pbutils-1.0 gstreamer-audio-1.0)

add_library(
        RialtoServerGstPlayer
        STATIC

        source/tasks/generic/AttachSamples.cpp
        source/tasks/generic/AttachSource.cpp
        source/tasks/generic/CheckAudioUnderflow.cpp
        source/tasks/generic/DeepElementAdded.cpp
        source/tasks/generic/EnoughData.cpp
        source/tasks/generic/Eos.cpp
        source/tasks/generic/FinishSetupSource.cpp
        source/tasks/generic/Flush.cpp
        source/tasks/generic/GenericPlayerTaskFactory.cpp
        source/tasks/generic/HandleBusMessage.cpp
        source/tasks/generic/NeedData.cpp
        source/tasks/generic/Pause.cpp
        source/tasks/generic/Ping.cpp
        source/tasks/generic/Play.cpp
        source/tasks/generic/ProcessAudioGap.cpp
        source/tasks/generic/ReadShmDataAndAttachSamples.cpp
        source/tasks/generic/RemoveSource.cpp
        source/tasks/generic/RenderFrame.cpp
        source/tasks/generic/ReportPosition.cpp
        source/tasks/generic/SetBufferingLimit.cpp
        source/tasks/generic/SetImmediateOutput.cpp
        source/tasks/generic/SetLowLatency.cpp
        source/tasks/generic/SetMute.cpp
        source/tasks/generic/SetPlaybackRate.cpp
        source/tasks/generic/SetPosition.cpp
        source/tasks/generic/SetSourcePosition.cpp
        source/tasks/generic/SetSubtitleOffset.cpp
        source/tasks/generic/SetStreamSyncMode.cpp
        source/tasks/generic/SetSync.cpp
        source/tasks/generic/SetSyncOff.cpp
        source/tasks/generic/SetTextTrackIdentifier.cpp
        source/tasks/generic/SetUseBuffering.cpp
        source/tasks/generic/SetVideoGeometry.cpp
        source/tasks/generic/SetVolume.cpp
        source/tasks/generic/SetupElement.cpp
        source/tasks/generic/SetupSource.cpp
        source/tasks/generic/Shutdown.cpp
        source/tasks/generic/Stop.cpp
        source/tasks/generic/SwitchSource.cpp
        source/tasks/generic/SynchroniseSubtitleClock.cpp
        source/tasks/generic/Underflow.cpp
        source/tasks/generic/UpdatePlaybackGroup.cpp

        source/tasks/webAudio/Eos.cpp
        source/tasks/webAudio/HandleBusMessage.cpp
        source/tasks/webAudio/Pause.cpp
        source/tasks/webAudio/Ping.cpp
        source/tasks/webAudio/Play.cpp
        source/tasks/webAudio/SetCaps.cpp
        source/tasks/webAudio/SetVolume.cpp
        source/tasks/webAudio/Shutdown.cpp
        source/tasks/webAudio/Stop.cpp
        source/tasks/webAudio/WebAudioPlayerTaskFactory.cpp
        source/tasks/webAudio/WriteBuffer.cpp

        source/CapsBuilder.cpp
        source/FlushWatcher.cpp
        source/GstCapabilities.cpp
        source/GstDecryptor.cpp
        source/GstDispatcherThread.cpp
        source/GstGenericPlayer.cpp
        source/GstInitialiser.cpp
        source/GstLogForwarding.cpp
        source/GstProtectionMetadata.cpp
        source/GstProtectionMetadataHelper.cpp
        source/GstSrc.cpp
        source/GstTextTrackSink.cpp
        source/GstWebAudioPlayer.cpp
        source/NeedDataMapping.cpp
        source/Utils.cpp
        source/WorkerThread.cpp
        )

target_include_directories(
        RialtoServerGstPlayer

        PUBLIC
        interface

        PRIVATE
        include
        include/tasks
        include/tasks/generic
        include/tasks/webAudio
        $<TARGET_PROPERTY:RialtoPlayerPublic,INTERFACE_INCLUDE_DIRECTORIES>
        $<TARGET_PROPERTY:RialtoPlayerCommon,INTERFACE_INCLUDE_DIRECTORIES>
        $<TARGET_PROPERTY:RialtoServerCommon,INTERFACE_INCLUDE_DIRECTORIES>
        $<TARGET_PROPERTY:RialtoServerMain,INTERFACE_INCLUDE_DIRECTORIES>
        $<TARGET_PROPERTY:RialtoWrappers,INTERFACE_INCLUDE_DIRECTORIES>
        $<TARGET_PROPERTY:RialtoCommon,INTERFACE_INCLUDE_DIRECTORIES>
        ${GStreamerApp_INCLUDE_DIRS}

        )


set_property(
        TARGET RialtoServerGstPlayer
        PROPERTY POSITION_INDEPENDENT_CODE ON

        )

target_link_libraries(
        RialtoServerGstPlayer

        PRIVATE
        RialtoLogging
        RialtoServerMain
        RialtoWrappers
        RialtoCommon
        RialtoPlayerCommon
        ${GStreamerApp_LIBRARIES}
        )
