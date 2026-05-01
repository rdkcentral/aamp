# If not stated otherwise in this file or this component's license file the
# following copyright and licenses apply:
#
# Copyright 2018 RDK Management
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

# XCode schema generation / flag setting
# Other flags see here: https://cmake.org/cmake/help/latest/prop_tgt/XCODE_GENERATE_SCHEME.html
cmake_minimum_required (VERSION 3.5)

function(xcode_define_schema new_schema)
	message( "xcode_define_schema for ${new_schema}" )
	set_property(TARGET ${new_schema} PROPERTY XCODE_GENERATE_SCHEME TRUE)
	set_property(TARGET ${new_schema} PROPERTY XCODE_SCHEME_ADDRESS_SANITIZER TRUE)
	set_property(TARGET ${new_schema} PROPERTY XCODE_SCHEME_ADDRESS_SANITIZER_USE_AFTER_RETURN TRUE)
endfunction()

project (AAMP)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Werror=format -Wno-multichar -Wno-non-virtual-dtor -Wno-psabi")

set_property(GLOBAL PROPERTY USE_FOLDERS ON)

execute_process(COMMAND bash "-c" "${CMAKE_SOURCE_DIR}/buildinfo.sh" WORKING_DIRECTORY ${CMAKE_SOURCE_DIR} OUTPUT_STRIP_TRAILING_WHITESPACE OUTPUT_VARIABLE buildinfo)
string(REGEX REPLACE "\n" "-" buildinfo "${buildinfo}")
message("build: ${buildinfo}")

find_package(Threads REQUIRED)
find_package(PkgConfig REQUIRED)

# Resolve dependent modules
message("using gstreamer-1.0")
pkg_check_modules(GSTREAMER REQUIRED gstreamer-1.0>=1.18.0)
pkg_check_modules(GSTREAMERBASE REQUIRED gstreamer-app-1.0)
pkg_check_modules(LIBXML2 REQUIRED libxml-2.0)
pkg_check_modules(LIBDASH REQUIRED libdash)
pkg_check_modules(OPENSSL REQUIRED openssl)
pkg_check_modules(LIBCJSON REQUIRED libcjson)
pkg_check_modules(UUID REQUIRED uuid)

# CMAKE_EXTERNAL_PLAYER_INTERFACE_DEPENDENCIES is set when the distro "build_external_player_interface" 
# option is enabled in the Yocto recipe. This flag determines whether middleware-player-interface 
# components should be sourced from external repositories or built from the local aamp repository.
if(CMAKE_EXTERNAL_PLAYER_INTERFACE_DEPENDENCIES)
	pkg_check_modules(BASECONVERSION REQUIRED libbaseconversion)
	pkg_check_modules(PLAYERLOGMANAGER REQUIRED libplayerlogmanager)
	pkg_check_modules(PLAYERFBINTERFACE REQUIRED libplayerfbinterface)
	pkg_check_modules(PLAYERGSTINTERFACE REQUIRED libplayergstinterface)
	pkg_check_modules(SUBTEC REQUIRED libsubtec)
endif()	

if(CMAKE_PREINIT_DECODER)
       message("CMAKE_PREINIT_DECODER Set")
       set(LIBAAMP_DEFINES "${LIBAAMP_DEFINES} -DUSE_PREINIT_DECODING=1")
       set(LIBAAMPJSBINDINGS_DEFINES "${LIBAAMPJSBINDINGS_DEFINES} -DUSE_PREINIT_DECODING=1")
endif()

if(APPLE)
	# libcurl < 8.5 exhibits memory leaks. On Ubuntu 22.04 can't update beyond 7.81.0-1ubuntu1.16 without building from source
	pkg_check_modules(CURL REQUIRED libcurl>=8.5)
else()
	pkg_check_modules(CURL REQUIRED libcurl)
endif()

# Platform specific settings
if(APPLE)
	set(CMAKE_OSX_DEPLOYMENT_TARGET "15.6")
	set(CMAKE_C_COMPILER "/usr/bin/cc")
	set(CMAKE_CXX_COMPILER "/usr/bin/c++")

	set(CMAKE_MACOSX_RPATH 1)
	# use, i.e. don't skip the full RPATH for the build tree
	set(CMAKE_SKIP_BUILD_RPATH FALSE)

	list(APPEND CMAKE_BUILD_RPATH "${CMAKE_SOURCE_DIR}/.libs/lib")

	# when building, don't use the install RPATH already
	# (but later on when installing)
	set(CMAKE_BUILD_WITH_INSTALL_RPATH FALSE)
	set(CMAKE_INSTALL_RPATH "${CMAKE_SOURCE_DIR}/.libs/lib")

	# add the automatically determined parts of the RPATH
	# which point to directories outside the build tree to the install RPATH
	set(CMAKE_INSTALL_RPATH_USE_LINK_PATH TRUE)
else()
	# For other builds, use the origin of the executable as the RPATH
	set(CMAKE_INSTALL_RPATH "$ORIGIN")
endif()

#update XCode scheme flags, harmless for non Darwin builds
set (CMAKE_CODE_GENERATE_SCHEME TRUE)
if(CMAKE_PLATFORM_UBUNTU OR CMAKE_SYSTEM_NAME STREQUAL Darwin)
	message("AAMP_SIMULATOR_BUILD set")
	set(LIBAAMP_DEFINES "${LIBAAMP_DEFINES} -DAAMP_SIMULATOR_BUILD=1")
	set(CMAKE_TELEMETRY_2_0_REQUIRED TRUE)
	set(CMAKE_AAMP_SIMULATOR TRUE)
	if(CMAKE_SYSTEM_NAME STREQUAL Darwin)
		execute_process (
			COMMAND bash -c "xcrun --show-sdk-path" OUTPUT_VARIABLE osxSdkPath OUTPUT_STRIP_TRAILING_WHITESPACE
		)
		# Dynamically detect Homebrew installation prefix
		execute_process (
			COMMAND bash -c "brew --prefix" OUTPUT_VARIABLE HOMEBREW_PREFIX OUTPUT_STRIP_TRAILING_WHITESPACE ERROR_QUIET
		)
		if(HOMEBREW_PREFIX)
			message("Detected Homebrew prefix: ${HOMEBREW_PREFIX}")
		endif()
		set(OS_CXX_FLAGS "${OS_CXX_FLAGS} -g -x objective-c++ -Wno-inconsistent-missing-override -F${osxSdkPath}/System/Library/Frameworks")
		# Add required system/local library paths; additional directories come from pkg-config
		set(OS_LD_FLAGS "${OS_LD_FLAGS} -F${osxSdkPath}/System/Library/Frameworks -framework Cocoa -L${osxSdkPath}/../MacOSX.sdk/usr/lib -L.libs/lib")
		# Add Homebrew lib directory if Homebrew is installed and valid
		if(HOMEBREW_PREFIX AND EXISTS "${HOMEBREW_PREFIX}/lib")
			set(OS_LD_FLAGS "${OS_LD_FLAGS} -L${HOMEBREW_PREFIX}/lib")
		endif()
		set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -isysroot ${osxSdkPath}/../MacOSX.sdk")
		if(HOMEBREW_PREFIX AND EXISTS "${HOMEBREW_PREFIX}/include")
			set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -I${HOMEBREW_PREFIX}/include")
		endif()
		string(STRIP "${OS_LD_FLAGS}" OS_LD_FLAGS)
		pkg_check_modules(GSTREAMERVIDEO REQUIRED gstreamer-video-1.0)
		set(AAMP_CLI_LD_FLAGS "${AAMP_CLI_LD_FLAGS} ${GSTREAMERVIDEO_LINK_LIBRARIES}")
		string(STRIP "${AAMP_CLI_LD_FLAGS}" AAMP_CLI_LD_FLAGS)
		message("AAMP_CLI_LD_FLAGS: ${AAMP_CLI_LD_FLAGS}")
		# Use pkg-config library directories for dependencies
		link_directories(${OPENSSL_LIBRARY_DIRS})
		link_directories(${GSTREAMER_LIBRARY_DIRS})
		link_directories(${GSTREAMERVIDEO_LIBRARY_DIRS})
		link_directories(${GSTREAMERBASE_LIBRARY_DIRS})
		pkg_check_modules(GLIB REQUIRED glib-2.0)
		include_directories(${GLIB_INCLUDE_DIRS})
		link_directories(${GLIB_LIBRARY_DIRS})
		set(GL_DEPENDS
			"-framework OpenGL"
			"-framework GLUT"
		)

		# XCode build flags. Even when using CLANG, the GCC name is required to enable the check
		set(CMAKE_XCODE_ATTRIBUTE_GCC_WARN_UNUSED_FUNCTION "YES")
		set(CMAKE_XCODE_ATTRIBUTE_GCC_WARN_UNUSED_VARIABLE "YES")
	else()
		message("CMAKE_PLATFORM_UBUNTU set")
		set(LIBAAMP_DEFINES "${LIBAAMP_DEFINES} -DUBUNTU=1")
		link_directories(${CMAKE_LIBRARY_PATH})
		pkg_check_modules(OPENGL REQUIRED gl)
		pkg_check_modules(GLEW REQUIRED glew)
		include_directories(${OPENGL_INCLUDE_DIRS})
		include_directories(${GLEW_INCLUDE_DIRS})
		set(GL_DEPENDS
			"${OPENGL_LINK_LIBRARIES}"
			"${GLEW_LINK_LIBRARIES}"
		)
	endif(CMAKE_SYSTEM_NAME STREQUAL Darwin)
endif()

# External library include directories
include_directories(
	${GSTREAMER_INCLUDE_DIRS}
	${CURL_INCLUDE_DIRS}
	${GSTREAMERBASE_INCLUDE_DIRS}
	${LIBDASH_INCLUDE_DIRS}
	${LIBXML2_INCLUDE_DIRS}
	${OPENSSL_INCLUDE_DIRS}
	${UUID_INCLUDE_DIRS}
	${LIBCJSON_INCLUDE_DIRS}
)

# Internal source directory includes
include_directories(
	${CMAKE_CURRENT_SOURCE_DIR}
	${CMAKE_CURRENT_SOURCE_DIR}/dash/xml
	${CMAKE_CURRENT_SOURCE_DIR}/dash/mpd
	${CMAKE_CURRENT_SOURCE_DIR}/dash/utils
	${CMAKE_CURRENT_SOURCE_DIR}/downloader
	${CMAKE_CURRENT_SOURCE_DIR}/drm
	${CMAKE_CURRENT_SOURCE_DIR}/isobmff
	${CMAKE_CURRENT_SOURCE_DIR}/jsbindings
	${CMAKE_CURRENT_SOURCE_DIR}/jsbindings/PersistentWatermark
	${CMAKE_CURRENT_SOURCE_DIR}/subtec/subtecparser
	${CMAKE_CURRENT_SOURCE_DIR}/subtitle
	${CMAKE_CURRENT_SOURCE_DIR}/tsb/api
	${CMAKE_CURRENT_SOURCE_DIR}/abr
	${CMAKE_CURRENT_SOURCE_DIR}/mp4demux
	# Locally built/installed dependencies
	${CMAKE_CURRENT_SOURCE_DIR}/.libs/include
)

set(LIBAAMP_DEPENDS
	${OS_LD_FLAGS}
	${UUID_LINK_LIBRARIES}
	${LIBCJSON_LINK_LIBRARIES}
	${GSTREAMERBASE_LINK_LIBRARIES}
	${GSTREAMER_LINK_LIBRARIES}
	${CURL_LINK_LIBRARIES}
	${LIBDASH_LINK_LIBRARIES}
	${LIBXML2_LIBRARIES}
	${CMAKE_THREAD_LIBS_INIT}
	${OPENSSL_LIBRARIES}
	${GL_DEPENDS}
	${AAMP_CLI_LD_FLAGS}
	-ldl
)

if(CMAKE_EXTERNAL_PLAYER_INTERFACE_DEPENDENCIES)

	list(APPEND LIBAAMP_DEPENDS 
	${PLAYERFBINTERFACE_LINK_LIBRARIES} 
	${BASECONVERSION_LINK_LIBRARIES} 
	${PLAYERLOGMANAGER_LINK_LIBRARIES} 
	${PLAYERGSTINTERFACE_LINK_LIBRARIES} 
	${SUBTEC_LINK_LIBRARIES} )

	# TDB needs to bring back for UT. 
	#include(test/mocks/mocks.cmake NO_POLICY_SCOPE)

	# External player interface include directories
	include_directories(
		${PLAYERFBINTERFACE_INCLUDE_DIRS}
		${BASECONVERSION_INCLUDE_DIRS}
		${PLAYERLOGMANAGER_INCLUDE_DIRS}
		${PLAYERGSTINTERFACE_INCLUDE_DIRS}
		${SUBTEC_INCLUDE_DIRS}
	)
else()
	# TDB needs to bring back for UT. 
	#include(test/mocks/mocks.cmake NO_POLICY_SCOPE)
	# Adding subdirectories for internal libraries
	include_directories(
		${CMAKE_CURRENT_SOURCE_DIR}/middleware
		${CMAKE_CURRENT_SOURCE_DIR}/middleware/drm
		${CMAKE_CURRENT_SOURCE_DIR}/middleware/drm/helper
		${CMAKE_CURRENT_SOURCE_DIR}/middleware/externals
		${CMAKE_CURRENT_SOURCE_DIR}/middleware/externals/contentsecuritymanager
		${CMAKE_CURRENT_SOURCE_DIR}/middleware/playerLogManager
		${CMAKE_CURRENT_SOURCE_DIR}/middleware/drm/aes
	)
	add_subdirectory(middleware)
endif()

add_subdirectory(test/rialtoPOC)

if (CMAKE_INBUILT_AAMP_DEPENDENCIES)
	message("Building aamp support libraries")
	include_directories(support/aampmetrics)

	# Building ABR for other dependent modules
	add_subdirectory(support/aampabr)
	add_subdirectory(support/aampmetrics)
	set(LIBAAMP_DEPENDS ${LIBAAMP_DEPENDS} metrics)
endif()

message("Adding tsb folder")
add_subdirectory(tsb)

install(TARGETS tsb
		DESTINATION lib
		PUBLIC_HEADER DESTINATION include
)
#Enabling systemd macro and lib
if(CMAKE_SYSTEMD_JOURNAL)
	message("CMAKE_SYSTEMD_JOURNAL set")
	set(LIBAAMP_DEPENDS ${LIBAAMP_DEPENDS} "-lsystemd")
	set(LIBAAMP_DEFINES "${LIBAAMP_DEFINES} -DUSE_SYSTEMD_JOURNAL_PRINT=1 -DSD_JOURNAL_SUPPRESS_LOCATION=1")
	set(LIBAAMPJSBINDINGS_DEPENDS ${LIBAAMPJSBINDINGS_DEPENDS} "-lsystemd")
	set(LIBAAMPJSBINDINGS_DEFINES "${LIBAAMPJSBINDINGS_DEFINES} -DUSE_SYSTEMD_JOURNAL_PRINT=1 -DSD_JOURNAL_SUPPRESS_LOCATION=1")
endif()
#Enabling ethan log macro and lib
if(CMAKE_USE_ETHAN_LOG)
	message("DCMAKE_USE_ETHAN_LOG set")
	# Find the ethanlog library for container logger
	find_package( EthanLog REQUIRED )
	# Add the include directories for EthanLog
	include_directories(${ETHANLOG_INCLUDE_DIRS})

	set(LIBAAMP_DEPENDS ${LIBAAMP_DEPENDS} "-lethanlog")
	set(LIBAAMP_DEFINES "${LIBAAMP_DEFINES} -DUSE_ETHAN_LOG=1")
	set(LIBAAMPJSBINDINGS_DEFINES "${LIBAAMPJSBINDINGS_DEFINES} -DUSE_ETHAN_LOG=1")
endif()
# Building jsbindings
if(CMAKE_WPEWEBKIT_JSBINDINGS OR APPLE OR CMAKE_PLATFORM_UBUNTU)
    message("CMAKE_WPEWEBKIT_JSBINDINGS is set, building jsbindings library")

	set(LIBJSBINDINGS_SOURCES
            jsbindings/jscontroller-jsbindings.cpp
            jsbindings/jsbindings.cpp
            jsbindings/jsutils.cpp
            jsbindings/jsmediaplayer.cpp
            jsbindings/jseventlistener.cpp
            jsbindings/jsevent.cpp
            jsbindings/PersistentWatermark/PersistentWatermark.cpp
        )

	if(CMAKE_WPEWEBKIT_WATERMARK_JSBINDINGS)
			message("CMAKE_WPEWEBKIT_WATERMARK_JSBINDINGS set, adding PersistentWatermark sources")
            set(LIBAAMPJSBINDINGS_DEFINES "${LIBAAMPJSBINDINGS_DEFINES} -DUSE_WATERMARK_JSBINDINGS")
            list(APPEND LIBJSBINDINGS_SOURCES
                jsbindings/PersistentWatermark/PersistentWatermarkDisplaySequencer.cpp
                jsbindings/PersistentWatermark/PersistentWatermarkEventHandler.cpp
                jsbindings/PersistentWatermark/PersistentWatermarkPluginAccess.cpp
                jsbindings/PersistentWatermark/PersistentWatermarkStorage.cpp
            )
        endif()
	
	add_library(aampjsbindings SHARED ${LIBJSBINDINGS_SOURCES})

    if(APPLE)

		set(LIBJSBINDINGS_SIMULATOR_SOURCES
			test/jsBindingTest/UVEBinding.cpp)
        
        find_library(JSCORE_FRAMEWORK JavaScriptCore)
        if(NOT JSCORE_FRAMEWORK)
            message(FATAL_ERROR "JavaScriptCore framework not found on macOS")
        endif()
        message("Found JavaScriptCore.framework at ${JSCORE_FRAMEWORK}")

        set(LIBAAMPJSBINDINGS_DEPENDS "${LIBAAMPJSBINDINGS_DEPENDS}" ${JSCORE_FRAMEWORK})
		set(LIBAAMP_DEFINES "${LIBAAMP_DEFINES} -DSUPPORT_JS_EVENTS -DAAMP_WPEWEBKIT_JSBINDINGS")

        target_link_libraries(aampjsbindings PUBLIC aamp tsb playergstinterface playerfbinterface ${AAMP_CLI_LD_FLAGS} ${LIBAAMPJSBINDINGS_DEPENDS})
        set_target_properties(aampjsbindings PROPERTIES
            COMPILE_FLAGS "${LIBAAMPJSBINDINGS_DEFINES} "
            LINK_FLAGS "-framework JavaScriptCore"
        )
		
		install(TARGETS
		 		aampjsbindings DESTINATION lib
		)

		add_executable(uveExecuter
			${LIBJSBINDINGS_SIMULATOR_SOURCES}
		)

		set_target_properties(uveExecuter
		 	PROPERTIES COMPILE_FLAGS " ${OS_CXX_FLAGS} ${LIBAAMP_DEFINES}")

		target_include_directories(uveExecuter PRIVATE
			${GST_INCLUDE_DIRS}
			${GLIB_INCLUDE_DIRS}
		)

		target_link_directories(uveExecuter PRIVATE
			${GST_LIBRARY_DIRS}
			${GLIB_LIBRARY_DIRS}
		)

		target_link_libraries(uveExecuter PRIVATE
			aampjsbindings
			${GST_LIBRARIES}
			${GLIB_LIBRARIES}
			${JSCORE_FRAMEWORK}
		)

		set_target_properties(uveExecuter PROPERTIES
			BUILD_RPATH "@loader_path;${GST_LIBRARY_DIRS};${GLIB_LIBRARY_DIRS}"
		)

		xcode_define_schema(aampjsbindings)
		xcode_define_schema(uveExecuter)

    elseif(CMAKE_PLATFORM_UBUNTU)
        # On Ubuntu use the system JavaScriptCore provided by webkitgtk
        pkg_check_modules(JSC javascriptcoregtk-4.1)
        if(NOT JSC_FOUND)
            pkg_check_modules(JSC javascriptcoregtk-4.0)
        endif()
        if(NOT JSC_FOUND)
            message(FATAL_ERROR "JavaScriptCore library not found on Ubuntu. "
                "Please install libjavascriptcoregtk-4.1-dev or libjavascriptcoregtk-4.0-dev.")
        endif()
        message("Found JavaScriptCore: ${JSC_LINK_LIBRARIES}")

        include_directories(${JSC_INCLUDE_DIRS})
        set(LIBAAMPJSBINDINGS_DEPENDS ${LIBAAMPJSBINDINGS_DEPENDS} ${JSC_LINK_LIBRARIES})
        set(LIBAAMP_DEFINES "${LIBAAMP_DEFINES} -DSUPPORT_JS_EVENTS -DAAMP_WPEWEBKIT_JSBINDINGS")

        target_link_libraries(aampjsbindings PUBLIC aamp tsb playergstinterface playerfbinterface ${LIBAAMPJSBINDINGS_DEPENDS})
        set_target_properties(aampjsbindings PROPERTIES COMPILE_FLAGS "${LIBAAMPJSBINDINGS_DEFINES}")

        install(TARGETS aampjsbindings DESTINATION lib)

        set(LIBJSBINDINGS_SIMULATOR_SOURCES test/jsBindingTest/UVEBinding.cpp)

        add_executable(uveExecuter ${LIBJSBINDINGS_SIMULATOR_SOURCES})

        set_target_properties(uveExecuter PROPERTIES
            COMPILE_FLAGS "${OS_CXX_FLAGS} ${LIBAAMP_DEFINES}")

        target_include_directories(uveExecuter PRIVATE
            ${JSC_INCLUDE_DIRS}
            ${GSTREAMER_INCLUDE_DIRS}
        )

        target_link_libraries(uveExecuter PRIVATE
            aampjsbindings
            ${GSTREAMER_LINK_LIBRARIES}
            ${JSC_LINK_LIBRARIES}
        )

        install(TARGETS uveExecuter DESTINATION bin)

    else()
        pkg_check_modules(WPE_WEBKIT wpe-webkit-1.1)
        if(NOT WPE_WEBKIT_FOUND)
            pkg_check_modules(WPE_WEBKIT wpe-webkit-1.0)
        endif()
        if(NOT WPE_WEBKIT_FOUND)
            message(FATAL_ERROR "WPE WebKit library not found. Please install wpe-webkit-1.1 or wpe-webkit-1.0.")
        endif()

        find_path(JSC_INCDIR JavaScriptCore/JavaScript.h HINTS ${WPE_WEBKIT_INCLUDEDIR} ${WPE_WEBKIT_INCLUDE_DIRS})
		message("JSC_INCDIR is ${JSC_INCDIR}")
        include_directories(${JSC_INCDIR})

		message("The WPE WebKit library found is ${WPE_WEBKIT_LINK_LIBRARIES}")
		# Link WPE WebKit package to aampjsbindings
        set(LIBAAMPJSBINDINGS_DEPENDS ${LIBAAMPJSBINDINGS_DEPENDS} ${WPE_WEBKIT_LINK_LIBRARIES})
		set(LIBAAMP_DEFINES "${LIBAAMP_DEFINES} -DSUPPORT_JS_EVENTS -DAAMP_WPEWEBKIT_JSBINDINGS")

        target_link_libraries(aampjsbindings PUBLIC aamp ${LIBAAMPJSBINDINGS_DEPENDS})
        set_target_properties(aampjsbindings PROPERTIES COMPILE_FLAGS "${LIBAAMPJSBINDINGS_DEFINES}")

		install(TARGETS
		 		aampjsbindings DESTINATION lib
		)
    endif()

	

else()
	message("CMAKE_WPEWEBKIT_JSBINDINGS not set")
endif()

set(LIBAAMP_SOURCES
	AampConstants.h AampDefine.h AampMediaType.h uint33_t.h
    AAMPAnomalyMessageType.h AuthTokenErrors.h
	iso639map.cpp iso639map.h
	AampCacheHandler.cpp AampCacheHandler.h
	AampBoxReader.h
	CachedFragment.cpp CachedFragment.h
	AampScheduler.cpp AampScheduler.h
	AampUtils.cpp AampUtils.h
	AampJsonObject.cpp AampJsonObject.h
	AampProfiler.cpp AampProfiler.h
	AampEvent.cpp AampEvent.h
	AampEventListener.cpp AampEventListener.h
	ota_shim.cpp ota_shim.h
	hdmiin_shim.cpp hdmiin_shim.h
	videoin_shim.cpp videoin_shim.h
	compositein_shim.cpp compositein_shim.h
	rmf_shim.cpp rmf_shim.h
	fragmentcollector_progressive.cpp fragmentcollector_progressive.h
	fragmentcollector_hls.cpp fragmentcollector_hls.h
	fragmentcollector_mpd.cpp fragmentcollector_mpd.h
	admanager_mpd.cpp admanager_mpd.h
	AampUnderflowMonitor.cpp AampUnderflowMonitor.h
	streamabstraction.cpp
	priv_aamp.cpp priv_aamp.h
	main_aamp.cpp main_aamp.h
	LangCodePreference.h StreamOutputFormat.h VideoZoomMode.h StreamSink.h AudioTrackInfo.h TextTrackInfo.h TimedMetadata.h
	drm/AampDRMLicManager.cpp drm/AampDRMLicManager.h
	drm/DrmInterface.cpp drm/DrmInterface.h
	aampgstplayer.cpp aampgstplayer.h
	AampBufferControl.cpp AampBufferControl.h
	tsprocessor.cpp tsprocessor.h
	aamplogging.cpp AampLogManager.h
	AampConfig.cpp AampConfig.h
	AampNetworkPersona.cpp AampNetworkPersona.h
	lstring.hpp lstring.cpp
	AampEventManager.cpp AampEventManager.h
	subtitle/webvttParser.cpp subtitle/webvttParser.h subtitle/vttCue.h
	isobmff/isobmffbox.cpp isobmff/isobmffbox.h
	isobmff/isobmffbuffer.cpp isobmff/isobmffbuffer.h
	isobmff/isobmffprocessor.cpp isobmff/isobmffprocessor.h
	isobmff/isobmffhelper.cpp isobmff/isobmffhelper.h
	MediaStreamContext.cpp MediaStreamContext.h
	downloader/AampCurlStore.cpp downloader/AampCurlStore.h downloader/AampCurlDefine.h
	AampDRMLicPreFetcher.cpp AampDRMLicPreFetcher.h
	AampCMCDCollector.cpp AampCMCDCollector.h
	downloader/AampCurlDownloader.cpp downloader/AampCurlDownloader.h
	ID3Metadata.cpp ID3Metadata.hpp
	abr/abr.cpp abr/abr.h
	abr/RollingMedianOutlierEstimator.cpp abr/RollingMedianOutlierEstimator.h
	abr/HarmonicEwmaEstimator.cpp abr/HarmonicEwmaEstimator.h
	dash/xml/DomDocument.cpp
	dash/xml/DomElement.cpp
	dash/xml/DomNode.cpp
	dash/xml/DomNodeIter.cpp
	dash/xml/DomTextNode.cpp
	dash/mpd/MPDModel.cpp dash/mpd/MPDModel.h
	dash/mpd/MPDSegmenter.cpp dash/mpd/MPDSegmenter.h
	dash/utils/Utils.cpp
	dash/utils/Url.cpp
	dash/utils/StringEx.cpp
	dash/utils/Path.cpp
	AampMPDDownloader.cpp AampMPDDownloader.h
	AampMPDParseHelper.cpp AampMPDParseHelper.h
	AampMPDUtils.cpp AampMPDUtils.h
	ElementaryProcessor.cpp ElementaryProcessor.h
	AampStreamSinkManager.cpp AampStreamSinkManager.h
	MetadataProcessor.cpp MetadataProcessor.hpp
	tsDemuxer.cpp tsDemuxer.hpp
	tsFragmentProcessor.cpp tsFragmentProcessor.hpp
	AampTSBSessionManager.cpp AampTSBSessionManager.h
	AampTsbDataManager.cpp AampTsbDataManager.h
	AampTsbMetaData.cpp AampTsbMetaData.h
	AampTsbAdMetaData.cpp AampTsbAdMetaData.h
	AampTsbAdPlacementMetaData.cpp AampTsbAdPlacementMetaData.h
	AampTsbAdReservationMetaData.cpp AampTsbAdReservationMetaData.h
	AampTsbMetaDataManager.cpp AampTsbMetaDataManager.h
	AampTsbReader.cpp AampTsbReader.h
	scte35/AampSCTE35.cpp scte35/AampSCTE35.h
	subtec/subtecparser/WebvttSubtecDevParser.cpp
	AampTrackWorker.cpp AampTrackWorker.h
	LangCodePreference.h
	AampTrackWorkerManager.cpp
	AampFragmentDescriptor.cpp
	AampTimeBasedBufferManager.cpp
	mp4demux/AampMp4Demuxer.cpp mp4demux/AampMp4Demuxer.h
	mp4demux/MP4Demux.cpp mp4demux/MP4Demux.h
	AampDemuxDataTypes.h
	net_trace.h
	AampLatencyMonitor.cpp AampLatencyMonitor.h
	simnet/net_persona_fitter.cpp simnet/net_persona_fitter.h
)

if(CMAKE_SOC_PLATFORM_RPI)
	message("CMAKE_SOC_PLATFORM_RPI set")
	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DRPI=1")
endif()

set(LIBAAMP_DEFINES "${LIBAAMP_DEFINES} -DAAMP_VANILLA_AES_SUPPORT -DAAMP_BUILD_INFO='${buildinfo}' -DSUPPORTS_MP4DEMUX")

if (SANITIZER_ENABLED AND CMAKE_PLATFORM_UBUNTU)
	set(LIBAAMP_DEFINES "${LIBAAMP_DEFINES} -fsanitize=address")
	set(LIBAAMP_DEPENDS "${LIBAAMP_DEPENDS} -fsanitize=address -static-libasan")
endif()

if(CMAKE_USE_RDK_PLUGINS)
	message("CMAKE_USE_RDK_PLUGINS set")
	set(LIBAAMP_DEFINES "${LIBAAMP_DEFINES} -DCREATE_PIPE_SESSION_TO_XRE")
endif()

# AAMP Telemetry 2.0 support
if (CMAKE_TELEMETRY_2_0_REQUIRED)
	message("CMAKE_TELEMETRY_2_0_REQUIRED set")
	set(LIBAAMP_SOURCES "${LIBAAMP_SOURCES}" AampTelemetry2.cpp)
	set(LIBAAMP_DEFINES "${LIBAAMP_DEFINES} -DAAMP_TELEMETRY_SUPPORT=1")
	if (NOT CMAKE_AAMP_SIMULATOR)
		set(LIBAAMP_DEPENDS ${LIBAAMP_DEPENDS} "-ltelemetry_msgsender")
	endif()
endif()


if (CMAKE_GST_SUBTEC_ENABLED)
	set(CMAKE_SUBTITLE_SUPPORT TRUE)
	message("CMAKE_GST_SUBTEC_ENABLED set")
	set(LIBAAMP_DEFINES "${LIBAAMP_DEFINES} -DGST_SUBTEC_ENABLED")
endif()

if (COVERAGE_ENABLED)
	set(LIBAAMP_DEFINES "${LIBAAMP_DEFINES} --coverage")
	set(LIBAAMP_DEPENDS ${LIBAAMP_DEPENDS} "--coverage")
endif()

if(CMAKE_ENABLE_PTS_RESTAMP)
	message("ENABLE_PTS_RESTAMP set")
	set(LIBAAMP_DEFINES "${LIBAAMP_DEFINES} -DENABLE_PTS_RESTAMP")
endif()

# Common compile flags for all targets
set(COMMON_COMPILE_FLAGS "${LIBAAMP_DEFINES} ${OS_CXX_FLAGS}")

set(LIBAAMP_SOURCES "${LIBAAMP_SOURCES}" "${LIBAAMP_MOCK_SOURCES}")
add_library(aamp SHARED ${LIBAAMP_SOURCES})
if(CMAKE_EXTERNAL_PLAYER_INTERFACE_DEPENDENCIES)
	target_link_libraries(aamp tsb ${LIBAAMP_DEPENDS} ${LIBAAMP_MOCK_DEPENDS})
else()
	target_link_libraries(aamp tsb playergstinterface playerfbinterface ${LIBAAMP_DEPENDS} ${LIBAAMP_MOCK_DEPENDS})
endif()
set_target_properties(aamp PROPERTIES COMPILE_FLAGS "${COMMON_COMPILE_FLAGS}")
set_target_properties(aamp PROPERTIES PUBLIC_HEADER "main_aamp.h")
set_target_properties(aamp PROPERTIES PRIVATE_HEADER "priv_aamp.h")
install(TARGETS aamp
		DESTINATION lib
		PUBLIC_HEADER DESTINATION include
		PRIVATE_HEADER DESTINATION include
)

set(AAMP_CLI_SOURCES
	test/aampcli/Aampcli.cpp
	test/aampcli/AampcliPlaybackCommand.cpp
	test/aampcli/AampcliCommandHandler.cpp
	test/aampcli/AampcliGet.cpp
	test/aampcli/AampcliSet.cpp
	test/aampcli/AampcliVirtualChannelMap.cpp
	test/aampcli/AampcliShader.cpp
	test/aampcli/AampcliSubtecSimulator.cpp
	test/aampcli/AampcliPrintf.cpp
)
add_executable(aamp-cli ${AAMP_CLI_SOURCES})
if(CMAKE_EXTERNAL_PLAYER_INTERFACE_DEPENDENCIES)
	# Note: Most libraries are already included via the aamp library dependency
	target_link_libraries(aamp-cli aamp tsb ${AAMP_CLI_LD_FLAGS} "-lreadline")
else()
	target_link_libraries(aamp-cli aamp tsb playergstinterface playerfbinterface ${AAMP_CLI_LD_FLAGS} "-lreadline")
endif()
#aamp-cli is not an ideal standalone app. It uses private aamp instance for debugging purposes

set_target_properties(aamp-cli PROPERTIES COMPILE_FLAGS "${COMMON_COMPILE_FLAGS}")
install(TARGETS aamp-cli
		DESTINATION bin
)
# XCode schema target
xcode_define_schema(aamp-cli)

if ((CMAKE_PLATFORM_UBUNTU OR CMAKE_SYSTEM_NAME STREQUAL Darwin) AND CMAKE_BUILD_KOTLIN_ENABLED)
	set(AAMP_KOTLIN_SRC
		${AAMP_CLI_SOURCES}
		test/aampcli/aampcli_kmp.cpp
	)
	add_library(aampKotlin SHARED ${AAMP_KOTLIN_SRC})
	install (TARGETS aampKotlin
			DESTINATION lib
	)
	target_link_libraries(aampKotlin aamp tsb ${AAMP_CLI_LD_FLAGS} "-lreadline")
	set_target_properties(aampKotlin PROPERTIES COMPILE_FLAGS "${COMMON_COMPILE_FLAGS}")
	xcode_define_schema(aampKotlin)
endif()

set(GSTTESTHARNESS_DEPENDS
	${OS_LD_FLAGS}
	${CMAKE_THREAD_LIBS_INIT}
	${GSTREAMER_LINK_LIBRARIES}
	${GSTREAMERBASE_LINK_LIBRARIES}
	${LIBDASH_LINK_LIBRARIES}
	${CURL_LINK_LIBRARIES}
)
set(GSTTESTHARNESS_SOURCES
	test/gstTestHarness/gst-port.cpp
	test/gstTestHarness/gst-port.h
	test/gstTestHarness/gst-utils.cpp
	test/gstTestHarness/gst-utils.h
	test/gstTestHarness/gst-test.cpp
	test/gstTestHarness/gst-test.h
	test/gstTestHarness/mp4demux.hpp
	test/gstTestHarness/dash_adapter.cpp
	test/gstTestHarness/dash_adapter.hpp
	test/gstTestHarness/downloader.cpp
	test/gstTestHarness/downloader.hpp
	test/gstTestHarness/stream_utils.cpp
	test/gstTestHarness/stream_utils.hpp
	test/gstTestHarness/string_utils.cpp
	test/gstTestHarness/string_utils.hpp
	test/gstTestHarness/tsdemux.hpp
	test/gstTestHarness/turbo_xml.hpp
)
add_executable(gstTestHarness ${GSTTESTHARNESS_SOURCES})
target_link_libraries(gstTestHarness ${GSTTESTHARNESS_DEPENDS} "-lreadline")
install(TARGETS gstTestHarness
		DESTINATION bin
)
# XCode schema target
xcode_define_schema(gstTestHarness)

#TODO: Decide which are the actual public headers
install(FILES
	Accessibility.hpp AampEvent.h AampConfig.h AampCMCDCollector.h AampEventManager.h
	AampDefine.h AampEventListener.h AampMediaType.h AampLogManager.h
	subtitle/vttCue.h AampUtils.h
	AampProfiler.h AampConstants.h iso639map.h
	AampScheduler.h
	downloader/AampCurlStore.h
	downloader/AampCurlDownloader.h
	downloader/AampCurlDefine.h
	ID3Metadata.hpp
	AampSegmentInfo.hpp
	AampDRMLicPreFetcher.h
	AampDRMLicPreFetcherInterface.h
	AampMPDDownloader.h
	AampMPDParseHelper.h
	AampMPDPeriodInfo.h
	AampLLDASHData.h
	AampTSBSessionManager.h
	AampTsbReader.h
	AampTrackWorker.hpp
	AampTrackWorkerManager.hpp
	AampDownloadInfo.hpp
	AampFragmentDescriptor.hpp
	AampTimeBasedBufferManager.hpp
	MediaSegmentDownloadJob.hpp
	tsb/api/TsbApi.h
	LangCodePreference.h StreamOutputFormat.h VideoZoomMode.h StreamSink.h TimedMetadata.h
	AampDemuxDataTypes.h
	abr/abr.h
	abr/RollingMedianOutlierEstimator.h
	abr/HarmonicEwmaEstimator.h
	abr/BandwidthEstimatorBase.h
	AampSpeedCache.h
	DESTINATION include
)


if (UTEST_ENABLED)
	add_subdirectory(test/utests EXCLUDE_FROM_ALL)
endif()

if (CMAKE_PLATFORM_UBUNTU OR CMAKE_SYSTEM_NAME STREQUAL Darwin )
	install(FILES build/aampcli-run-subtec.sh DESTINATION bin)
endif()

message("LIBAAMP_DEFINES is ${LIBAAMP_DEFINES}")
message("LIBAAMP_DEPENDS is ${LIBAAMP_DEPENDS}")
message("LIBAAMP_SOURCES is ${LIBAAMP_SOURCES}")
