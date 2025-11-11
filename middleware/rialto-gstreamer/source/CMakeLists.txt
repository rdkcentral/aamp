# Copyright (C) 2023 
#
# This library is free software; you can redistribute it and/or
# modify it under the terms of the GNU Lesser General Public
# License as published by the Free Software Foundation;
# version 2.1 of the License.
#
# This library is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
# Lesser General Public License for more details.
#
# You should have received a copy of the GNU Lesser General Public
# License along with this library; if not, write to the Free Software
# Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA

# RIALTO-197: deprecated-declarations error in the latest stable2 for gstreamer.
# Should be removed once the issue is fixed.
add_compile_options(
  "-Wno-deprecated-declarations"
)

find_package(Rialto 1.0 REQUIRED)
find_package(ocdmRialto 1.0 REQUIRED)

add_library(gstrialtosinks

        SHARED
        GstreamerCatLog.cpp
        RialtoGStreamerMSEVideoSink.cpp
        RialtoGStreamerMSEAudioSink.cpp
        RialtoGStreamerWebAudioSink.cpp
        RialtoGStreamerMSESubtitleSink.cpp
        GStreamerUtils.cpp
        GStreamerEMEUtils.cpp
        GStreamerMSEUtils.cpp
        GStreamerMSEMediaPlayerClient.cpp
        GStreamerWebAudioPlayerClient.cpp
        MessageQueue.cpp
        PullModeAudioPlaybackDelegate.cpp
        PullModePlaybackDelegate.cpp
        PullModeSubtitlePlaybackDelegate.cpp
        PullModeVideoPlaybackDelegate.cpp
        PushModeAudioPlaybackDelegate.cpp
        RialtoGSteamerPlugin.cpp
        RialtoGStreamerMSEBaseSink.cpp
        MediaPlayerManager.cpp
        Timer.cpp
        BufferParser.cpp
        LogToGstHandler.cpp
        )

target_include_directories(gstrialtosinks
        PUBLIC
        $<INSTALL_INTERFACE:include/rialto>

        PRIVATE
        ${RIALTO_INCLUDE_DIR}
        ${RIALTOOCDM_INCLUDE_DIR}
        ${GSTREAMER_APP_INCLUDE_DIRS}
        )

target_link_libraries(gstrialtosinks
        PUBLIC
        ${GSTREAMER_APP_LIBRARIES}

        PRIVATE
        Rialto::ocdmRialto
        Rialto::RialtoClient
        Threads::Threads
        )

#needed by GST_PLUGIN_DEFINE
target_compile_options(gstrialtosinks
        PRIVATE
        -DPACKAGE="gstrialtosinks"
        -DVERSION="1.0"
        )

set_target_properties(gstrialtosinks
        PROPERTIES LINK_FLAGS "-Wl,--unresolved-symbols=report-all"
        )

find_package(EthanLog)
if (RIALTO_ENABLE_ETHAN_LOG AND EthanLog_FOUND)
    message(STATUS "EthanLog is enabled")
    target_compile_definitions(gstrialtosinks PRIVATE USE_ETHANLOG)
    target_link_libraries(gstrialtosinks PRIVATE EthanLog::EthanLog)
else ()
    message(STATUS "EthanLog is disabled")
endif ()

install(
       TARGETS gstrialtosinks
       DESTINATION lib/gstreamer-1.0
       )
