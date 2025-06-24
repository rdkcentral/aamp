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
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -g")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -g")
set(CMAKE_BUILD_TYPE "Debug" CACHE STRING "Build type" FORCE)


set(CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG} -g" CACHE STRING "Debug C flags" FORCE)
set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -g" CACHE STRING "Debug C++ flags" FORCE)
set(CMAKE_BUILD_TYPE "Debug" CACHE STRING "Build type" FORCE)

set(CMAKE_C_FLAGS_DEBUG "-g -O0" CACHE STRING "Debug C flags" FORCE)
set(CMAKE_CXX_FLAGS_DEBUG "-g -O0" CACHE STRING "Debug C++ flags" FORCE)
function(xcode_define_schema new_schema)
    message( "xcode_define_schema for ${new_schema}" )
    set_property(TARGET ${new_schema} PROPERTY XCODE_GENERATE_SCHEME TRUE)
    set_property(TARGET ${new_schema} PROPERTY XCODE_SCHEME_ADDRESS_SANITIZER TRUE)
    set_property(TARGET ${new_schema} PROPERTY XCODE_SCHEME_ADDRESS_SANITIZER_USE_AFTER_RETURN TRUE)
endfunction()

include_directories(${CMAKE_CURRENT_SOURCE_DIR}/middleware)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/test/gstTestHarness)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/middleware/externals)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/middleware/externals/playersecmanager)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/middleware/externals/rdk)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/middleware/vendor)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/middleware/baseConversion)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/middleware/playerLogManager)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/middleware/subtitle)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/middleware/drm)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/middleware/drm/aes)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/middleware/drm/ocdm)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/middleware/drm/helper)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/middleware/subtec/subtecparser)


if(APPLE)
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
endif()
project (AAMP)
cmake_minimum_required (VERSION 2.6)
#find_package(GStreamer 1.4 REQUIRED)
#add_subdirectory(jsbindings)
find_package(PkgConfig REQUIRED)

set_property(GLOBAL PROPERTY USE_FOLDERS ON)

if( CMAKE_USE_GST_0.10 )
	message( FATAL_ERROR "gstreamer-0.10 not supported" )
else()
	message("using gstreamer-1.0")
	pkg_check_modules(GSTREAMER REQUIRED gstreamer-1.0)
	pkg_check_modules(GSTREAMERBASE REQUIRED gstreamer-app-1.0)
        pkg_check_modules(GSTREAMERVIDEO REQUIRED gstreamer-video-1.0)
endif()

if(APPLE)
	# libcurl < 8.5 exhibits memory leaks. On Ubuntu 22.04 can't update beyond 7.81.0-1ubuntu1.16 without building from source
	pkg_check_modules(CURL REQUIRED libcurl>=8.5)
else()
	pkg_check_modules(CURL REQUIRED libcurl)
endif()

pkg_check_modules(LibXml2 REQUIRED libxml-2.0)
pkg_check_modules(LIBDASH REQUIRED libdash)
pkg_check_modules(OPENSSL REQUIRED openssl)
pkg_check_modules(LIBCJSON REQUIRED libcjson)
pkg_check_modules(UUID REQUIRED uuid)


# TDB needs to bring back for UT. 
#include(test/mocks/mocks.cmake NO_POLICY_SCOPE)

add_subdirectory(middleware)

if (CMAKE_INBUILT_AAMP_DEPENDENCIES)
	message("Building aamp support libraries")
	add_subdirectory(support/aampabr)
	add_subdirectory(support/aampmetrics)
	add_subdirectory(plugins/gst-plugins-rdk-aamp)
endif()

message("Adding tsb folder")
add_subdirectory(tsb)

install(TARGETS tsb
        DESTINATION lib
        PUBLIC_HEADER DESTINATION include
)

option(DISABLE_SECURITY_TOKEN "Disable security token" OFF)

if(DISABLE_SECURITY_TOKEN)
  add_definitions(-DDISABLE_SECURITY_TOKEN)
endif()

if(CMAKE_WPEWEBKIT_JSBINDINGS)
	message("CMAKE_WPEWEBKIT_JSBINDINGS is set, Finding JavaScriptCore")
	pkg_search_module(PC_WPE_WEBKIT wpe-webkit-deprecated-0.1 wpe-webkit-1.0 wpe-webkit-1.1)
        find_path (JSC_INCDIR JavaScriptCore/JavaScript.h HINTS ${PC_WPE_WEBKIT_INCLUDEDIR} ${PC_WPE_WEBKIT_INCLUDE_DIRS})
        include_directories(${JSC_INCDIR})
else()
	message("CMAKE_WPEWEBKIT_JSBINDINGS not set")
endif()

#update XCode scheme flags, harmless for non Darwin builds
set (CMAKE_CODE_GENERATE_SCHEME TRUE)
if (CMAKE_PLATFORM_UBUNTU OR CMAKE_SYSTEM_NAME STREQUAL Darwin)
if(CMAKE_SYSTEM_NAME STREQUAL Darwin)
        set(OPENGL_LIBRARIES "-framework OpenGL -framework GLUT")
else()
       pkg_check_modules(OPENGL REQUIRED gl)
       set(OPENGL_LIBRARIES "${OPENGL_LIBRARIES} -lglut")
       pkg_check_modules(GLEW REQUIRED glew)
endif(CMAKE_SYSTEM_NAME STREQUAL Darwin)
endif()

# Mac OS X
if(CMAKE_SYSTEM_NAME STREQUAL Darwin)
    execute_process (
        COMMAND bash -c "xcrun --show-sdk-path" OUTPUT_VARIABLE osxSdkPath OUTPUT_STRIP_TRAILING_WHITESPACE
    )
    set(OS_CXX_FLAGS "${OS_CXX_FLAGS}  -std=c++14 -g -x objective-c++ -Wno-inconsistent-missing-override -F${osxSdkPath}/System/Library/Frameworks")
    set(OS_LD_FLAGS "${OS_LD_FLAGS} -F${osxSdkPath}/System/Library/Frameworks -framework Cocoa -L${osxSdkPath}/../MacOSX.sdk/usr/lib -L.libs/lib -L/usr/local/lib/")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -isysroot ${osxSdkPath}/../MacOSX.sdk -I/usr/local/include")
    string(STRIP ${OS_LD_FLAGS} OS_LD_FLAGS)
    set(AAMP_CLI_LD_FLAGS "${AAMP_CLI_LD_FLAGS} ${GSTREAMERVIDEO_LINK_LIBRARIES}")
    string(STRIP "${AAMP_CLI_LD_FLAGS}" AAMP_CLI_LD_FLAGS)
    link_directories(${OPENSSL_LIBRARY_DIRS})
    set(CMAKE_THREAD_LIBS_INIT "-lpthread")
    set(CMAKE_HAVE_THREADS_LIBRARY 1)
    pkg_check_modules(GLIB REQUIRED GLib-2.0)
    include_directories(${GLIB_INCLUDE_DIRS})

    # XCode build flags. Even when using CLANG, the GCC name is required to enable the check
    set(CMAKE_XCODE_ATTRIBUTE_GCC_WARN_UNUSED_FUNCTION "YES")
    set(CMAKE_XCODE_ATTRIBUTE_GCC_WARN_UNUSED_VARIABLE "YES")
endif(CMAKE_SYSTEM_NAME STREQUAL Darwin)

find_package (Threads REQUIRED)

include_directories(${CMAKE_CURRENT_SOURCE_DIR} drm downloader subtitle isobmff dash/xml dash/mpd dash/utils)
include_directories(${GSTREAMER_INCLUDE_DIRS})
include_directories(${CURL_INCLUDE_DIRS})
include_directories(${GSTREAMERBASE_INCLUDE_DIRS})
include_directories(${LIBDASH_INCLUDE_DIRS})
include_directories(${LibXml2_INCLUDE_DIRS})
include_directories(${OPENSSL_INCLUDE_DIRS})
include_directories(${OPENGL_INCLUDE_DIRS})
include_directories(${GLEW_INCLUDE_DIRS})
# Locally built/installed dependencies are here
include_directories(.libs/include)

# Sets the include paths to Linux/includem. By making it a system include the headers won't report any errors ( effc++ )
include_directories(SYSTEM ${LIBCJSON_INCLUDE_DIRS})

set(LIBAAMP_HEADERS AampCacheHandler.h
					AampConfig.h
					AampConstants.h
					downloader/AampCurlStore.h
					AampDefine.h
					AampEvent.h
					AampEventListener.h
					AampEventManager.h
					AampGrowableBuffer.h
					AampJsonObject.h
					AampLogManager.h
					AampMediaType.h
					AampProfiler.h
					AampScheduler.h
					AampUtils.h
					AdManagerBase.h
					MediaStreamContext.h
					StreamAbstractionAAMP.h
					drm/AampDRMLicManager.h
					aampgstplayer.h
					AampBufferControl.h
					admanager_mpd.h
					compositein_shim.h
					fragmentcollector_hls.h
					fragmentcollector_mpd.h
					fragmentcollector_progressive.h
					hdmiin_shim.h
					iso639map.h
					main_aamp.h
					mediaprocessor.h
					ota_shim.h
					priv_aamp.h
					AampDRMLicPreFetcher.h
					AampDRMLicPreFetcherInterface.h
					tsprocessor.h
					gstaamptaskpool.h
					uint33_t.h
					videoin_shim.h
					AampCMCDCollector.h
					downloader/AampCurlDefine.h
					downloader/AampCurlDownloader.h
					ID3Metadata.hpp
					AampSegmentInfo.hpp
					dash/xml/DomDocument.h
					dash/xml/DomElement.h
					dash/xml/DomNode.h
					dash/xml/DomNodeIter.h
					dash/xml/DomTextNode.h
					dash/mpd/MPDModel.h
					dash/mpd/MPDSegmenter.h
					dash/utils/Utils.h
					dash/utils/Url.h
					dash/utils/Path.h
					dash/utils/StringEx.h
					AampMPDDownloader.h
					AampMPDParseHelper.h
					AampLLDASHData.h
					AampMPDUtils.h
					AampMPDPeriodInfo.h
					ElementaryProcessor.h
					AampStreamSinkManager.h
					MetadataProcessor.hpp
					tsDemuxer.hpp
					tsFragmentProcessor.hpp
					AampTime.h
					AampTSBSessionManager.h
					AampTsbDataManager.h
					AampTsbMetaData.h
					AampTsbAdMetaData.h
					AampTsbAdPlacementMetaData.h
					AampTsbAdReservationMetaData.h
					AampTsbMetaDataManager.h
					AampTsbReader.h
					lstring.hpp
					scte35/AampSCTE35.h
					Accessibility.hpp
					AampTrackWorker.h
					test/gstTestHarness/tsdemux.hpp
					test/gstTestHarness/mp4demux.hpp
)

set(LIBAAMP_SOURCES iso639map.cpp
					AampCacheHandler.cpp
					AampGrowableBuffer.cpp
					AampScheduler.cpp
					AampUtils.cpp
					AampJsonObject.cpp
					AampProfiler.cpp
					AampEvent.cpp
					AampEventListener.cpp
					ota_shim.cpp
					hdmiin_shim.cpp
					videoin_shim.cpp
					compositein_shim.cpp
					rmf_shim.cpp
					fragmentcollector_progressive.cpp
					fragmentcollector_hls.cpp
					fragmentcollector_mpd.cpp
					admanager_mpd.cpp
					streamabstraction.cpp
					priv_aamp.cpp
					main_aamp.cpp
					drm/AampDRMLicManager.cpp
					aampgstplayer.cpp
					AampBufferControl.cpp
					tsprocessor.cpp
					drm/DrmInterface.cpp
					aamplogging.cpp
					AampConfig.cpp
					AampEventManager.cpp
					subtitle/webvttParser.cpp
					isobmff/isobmffbox.cpp
					isobmff/isobmffbuffer.cpp
					isobmff/isobmffprocessor.cpp
					isobmff/isobmffhelper.cpp
					MediaStreamContext.cpp
					downloader/AampCurlStore.cpp
					AampDRMLicPreFetcher.cpp
					AampCMCDCollector.cpp
					downloader/AampCurlDownloader.cpp
					ID3Metadata.cpp
					dash/xml/DomDocument.cpp
					dash/xml/DomElement.cpp
					dash/xml/DomNode.cpp
					dash/xml/DomNodeIter.cpp
					dash/xml/DomTextNode.cpp
					dash/mpd/MPDModel.cpp
					dash/mpd/MPDSegmenter.cpp
					dash/utils/Utils.cpp
					dash/utils/Url.cpp
					dash/utils/StringEx.cpp
					dash/utils/Path.cpp
					AampMPDDownloader.cpp
					AampMPDParseHelper.cpp
					AampMPDUtils.cpp
					ElementaryProcessor.cpp
					AampStreamSinkManager.cpp
					MetadataProcessor.cpp
					tsDemuxer.cpp
					tsFragmentProcessor.cpp
					AampTSBSessionManager.cpp
					AampTsbDataManager.cpp
					AampTsbMetaData.cpp
					AampTsbAdMetaData.cpp
					AampTsbAdPlacementMetaData.cpp
					AampTsbAdReservationMetaData.cpp
					AampTsbMetaDataManager.cpp
					AampTsbReader.cpp
					scte35/AampSCTE35.cpp
					AampTrackWorker.cpp
)

if (CMAKE_PLATFORM_UBUNTU OR CMAKE_SYSTEM_NAME STREQUAL Darwin )
  message("AAMP_SIMULATOR_BUILD set")
  set(LIBAAMP_DEFINES "${LIBAAMP_DEFINES} -DAAMP_SIMULATOR_BUILD=1")
  set(CMAKE_TELEMETRY_2_0_REQUIRED TRUE)
  set(CMAKE_AAMP_SIMULATOR TRUE)
  # uncomment below to build additional drm support in simulator
  # set(CMAKE_USE_OPENCDM_ADAPTER TRUE)
  # set(CMAKE_USE_OPENCDM_ADAPTER_MOCKS TRUE)
endif()

execute_process(COMMAND bash "-c" "${CMAKE_SOURCE_DIR}/buildinfo.sh" WORKING_DIRECTORY ${CMAKE_SOURCE_DIR} OUTPUT_STRIP_TRAILING_WHITESPACE OUTPUT_VARIABLE buildinfo)
string(REGEX REPLACE "\n" "-" buildinfo "${buildinfo}")
message("build: ${buildinfo}")
set(LIBAAMP_DEFINES "${LIBAAMP_DEFINES} -DAAMP_VANILLA_AES_SUPPORT")
set(LIBAAMP_DEFINES "${LIBAAMP_DEFINES} -DAAMP_BUILD_INFO='${buildinfo}'")

set(LIBAAMP_DEFINES "${LIBAAMP_DEFINES} -DSUPPORTS_MP4DEMUX")

if (CMAKE_PLATFORM_UBUNTU)
	message("CMAKE_PLATFORM_UBUNTU set")
	set(LIBAAMP_DEFINES "${LIBAAMP_DEFINES} -DUBUNTU=1")
endif()

if(CMAKE_USE_RDK_PLUGINS)
	message("CMAKE_USE_RDK_PLUGINS set")
	set(LIBAAMP_DEFINES "${LIBAAMP_DEFINES} -DCREATE_PIPE_SESSION_TO_XRE")
endif()

set(LIBAAMP_DEPENDS ${OS_LD_FLAGS} ${UUID_LINK_LIBRARIES} ${LIBCJSON_LINK_LIBRARIES} ${GSTREAMERBASE_LINK_LIBRARIES} ${GSTREAMER_LINK_LIBRARIES} ${CURL_LINK_LIBRARIES} ${LIBDASH_LINK_LIBRARIES} ${LibXml2_LIBRARIES} ${CMAKE_THREAD_LIBS_INIT} ${OPENSSL_LIBRARIES} ${OPENGL_LIBRARIES} ${GLEW_LIBRARIES} ${AAMP_CLI_LD_FLAGS} -ldl ${SEC_CLIENT_LIB})

if(CMAKE_WPEWEBKIT_JSBINDINGS)
	message("CMAKE_WPEWEBKIT_JSBINDINGS set, just setting flags")
	set(LIBAAMP_DEFINES "${LIBAAMP_DEFINES} -DSUPPORT_JS_EVENTS")
else()
	message("CMAKE_WPEWEBKIT_JSBINDINGS not set")
endif()

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Werror=format -Wno-multichar -std=c++11 -Wno-non-virtual-dtor -Wno-psabi")

if(CMAKE_PLATFORM_UBUNTU)
    message("CMAKE_PLATFORM_UBUNTU set")
    link_directories(${CMAKE_LIBRARY_PATH})
endif()

set(AAMP_CLI_HEADERS test/aampcli/Aampcli.h
			test/aampcli/AampcliPlaybackCommand.h
			test/aampcli/AampcliCommandHandler.h
			test/aampcli/AampcliGet.h
			test/aampcli/AampcliSet.h
			test/aampcli/AampcliVirtualChannelMap.h
			test/aampcli/AampcliShader.h
			test/aampcli/AampcliSubtecSimulator.h
			${AAMP_OS_SOURCES})

set(AAMP_CLI_SOURCES test/aampcli/Aampcli.cpp
			test/aampcli/AampcliPlaybackCommand.cpp
			test/aampcli/AampcliCommandHandler.cpp
			test/aampcli/AampcliGet.cpp
			test/aampcli/AampcliSet.cpp
			test/aampcli/AampcliVirtualChannelMap.cpp
			test/aampcli/AampcliShader.cpp
			test/aampcli/AampcliSubtecSimulator.cpp
			${AAMP_OS_SOURCES})

if (NOT CMAKE_INBUILT_AAMP_DEPENDENCIES)
	set(LIBAAMP_DEPENDS "${LIBAAMP_DEPENDS} -labr -lmetrics")
endif()

if(CMAKE_USE_OPENCDM_ADAPTER)
	message("Using OPEN CDM ADAPTER")
	# Add necessary preprocessor definitions
	set(LIBAAMP_DEFINES "${LIBAAMP_DEFINES} -DUSE_OPENCDM_ADAPTER")
	set(LIBAAMP_DEFINES "${LIBAAMP_DEFINES} -DDRM_BUILD_PROFILE=DRM_BUILD_PROFILE_OEM -DTARGET_LITTLE_ENDIAN=1 -DTARGET_SUPPORTS_UNALIGNED_DWORD_POINTERS=0")
else()
    message("No OpenCDM support enabled")
endif()

# AAMP Telemetry 2.0 support
if (CMAKE_TELEMETRY_2_0_REQUIRED)
	message("CMAKE_TELEMETRY_2_0_REQUIRED set")
	set(LIBAAMP_SOURCES ${LIBAAMP_SOURCES} AampTelemetry2.cpp)
	set(LIBAAMP_HEADERS ${LIBAAMP_HEADERS} AampTelemetry2.hpp)
	set(LIBAAMP_DEFINES "${LIBAAMP_DEFINES} -DAAMP_TELEMETRY_SUPPORT=1")

	if (NOT CMAKE_AAMP_SIMULATOR)
		set(LIBAAMP_DEPENDS "${LIBAAMP_DEPENDS} -ltelemetry_msgsender")
	endif()
endif()

set(AAMP_SUBTEC_CLASS_SOURCES subtec/subtecparser/WebvttSubtecDevParser.cpp)
set(LIBAAMP_SOURCES ${LIBAAMP_SOURCES} ${AAMP_SUBTEC_CLASS_SOURCES})
include_directories(subtec/subtecparser)

set(LIBAAMP_DEFINES "${LIBAAMP_DEFINES} ${SEC_CONTENT_METADATA_ENABLED}")

set(LIBAAMP_SOURCES "${LIBAAMP_SOURCES}" "${LIBAAMP_MOCK_SOURCES}")

if (COVERAGE_ENABLED)
    set(LIBAAMP_DEFINES "${LIBAAMP_DEFINES} --coverage")
    set(LIBAAMP_DEPENDS "${LIBAAMP_DEPENDS} --coverage")
endif()

add_library(aamp SHARED ${LIBAAMP_HEADERS} ${LIBAAMP_SOURCES} ${LIBAAMP_HELP_SOURCES})
add_executable(aamp-cli ${AAMP_CLI_HEADERS} ${AAMP_CLI_SOURCES})
target_link_libraries(aamp-cli "-lreadline")
# XCode schema target
xcode_define_schema(aamp-cli)

set(AAMP_LIB_SRC
${AAMP_CLI_HEADERS}
test/aampcli/aampcli_kmp.h

${AAMP_CLI_SOURCES}
test/aampcli/aampcli_kmp.cpp
)

add_library(aampKotlin SHARED ${AAMP_LIB_SRC})
install (TARGETS aampKotlin
		DESTINATION lib
		PUBLIC_HEADER DESTINATION include
)

target_link_libraries(aampKotlin aamp subtec tsb )
target_link_libraries(aampKotlin aamp ${AAMP_CLI_LD_FLAGS})
target_link_libraries(aampKotlin "-lreadline")

set_target_properties(aampKotlin PROPERTIES COMPILE_FLAGS "${LIBAAMP_DEFINES} ${AAMP_CLI_EXTRA_DEFINES} ${OS_CXX_FLAGS}")
xcode_define_schema(aampKotlin)

if(CMAKE_SYSTEMD_JOURNAL)
    message("CMAKE_SYSTEMD_JOURNAL set")
    set(LIBAAMP_DEPENDS "${LIBAAMP_DEPENDS} -lsystemd")
    set(LIBAAMP_DEFINES "${LIBAAMP_DEFINES} -DUSE_SYSTEMD_JOURNAL_PRINT=1 -DSD_JOURNAL_SUPPRESS_LOCATION=1")
    set(LIBAAMPJSBINDINGS_DEFINES "${LIBAAMPJSBINDINGS_DEFINES} -DUSE_SYSTEMD_JOURNAL_PRINT=1 -DSD_JOURNAL_SUPPRESS_LOCATION=1")

endif()

if(CMAKE_USE_ETHAN_LOG)
     message("DCMAKE_USE_ETHAN_LOG set")
 # Find the ethanlog library for container logger
     find_package( EthanLog REQUIRED )
# Add the include directories for EthanLog
     include_directories(${ETHANLOG_INCLUDE_DIRS})

     set(LIBAAMP_DEPENDS "${LIBAAMP_DEPENDS} -lethanlog")
     set(LIBAAMP_DEFINES "${LIBAAMP_DEFINES} -DUSE_ETHAN_LOG=1")
     set(LIBAAMPJSBINDINGS_DEFINES "${LIBAAMPJSBINDINGS_DEFINES} -DUSE_ETHAN_LOG=1")
endif()


if(CMAKE_WPEWEBKIT_JSBINDINGS)
	message("CMAKE_WPEWEBKIT_JSBINDINGS set, creating jsbinding library")
    include_directories(${CMAKE_CURRENT_SOURCE_DIR} jsbindings jsbindings/PersistentWatermark)
	add_subdirectory(test)
    set(JSBINDINGS_STD_SOURCES jsbindings/jscontroller-jsbindings.cpp jsbindings/jsbindings.cpp jsbindings/jsutils.cpp jsbindings/jsmediaplayer.cpp jsbindings/jseventlistener.cpp jsbindings/jsevent.cpp jsbindings/PersistentWatermark/PersistentWatermark.cpp)
    if(CMAKE_WPEWEBKIT_WATERMARK_JSBINDINGS)
        add_library(aampjsbindings SHARED ${JSBINDINGS_STD_SOURCES} jsbindings/PersistentWatermark/PersistentWatermarkDisplaySequencer.cpp  jsbindings/PersistentWatermark/PersistentWatermarkEventHandler.cpp jsbindings/PersistentWatermark/PersistentWatermarkPluginAccess.cpp jsbindings/PersistentWatermark/PersistentWatermarkStorage.cpp)
    else()
	    add_library(aampjsbindings SHARED ${JSBINDINGS_STD_SOURCES})
    endif()
    target_link_libraries(aampjsbindings aamp systemd)

    if(CMAKE_WPEWEBKIT_WATERMARK_JSBINDINGS)
        set(LIBAAMPJSBINDINGS_DEFINES "${LIBAAMPJSBINDINGS_DEFINES} -DUSE_WATERMARK_JSBINDINGS")
        target_link_libraries(aampjsbindings ${WPEFRAMEWORK_LIBRARIES})
    endif()
# Check if CMAKE_WPEWEBKIT_JSBINDINGS is enabled
if(CMAKE_WPEWEBKIT_JSBINDINGS)
    message("CMAKE_WPEWEBKIT_JSBINDINGS is set, searching for WPE WebKit libraries")

    # Search for WPE WebKit libraries
    pkg_check_modules(WPE_WEBKIT wpe-webkit-1.1)
    if(NOT WPE_WEBKIT_FOUND)
        pkg_check_modules(WPE_WEBKIT wpe-webkit-1.0)
    endif()

    # Check if any WPE WebKit library was found
    if(NOT WPE_WEBKIT_FOUND)
        message(FATAL_ERROR "WPE WebKit library not found. Please install wpe-webkit-1.1 or wpe-webkit-1.0.")
    else()
        message("The WPE WebKit library found is ${WPE_WEBKIT_LINK_LIBRARIES}")

        # Link WPE WebKit package to aampjsbindings
        target_link_libraries(aampjsbindings PUBLIC ${WPE_WEBKIT_LINK_LIBRARIES})
    endif()
else()
    message("CMAKE_WPEWEBKIT_JSBINDINGS is not set, skipping WPE WebKit library linking")
endif()

    set_target_properties(aampjsbindings PROPERTIES COMPILE_FLAGS "${LIBAAMPJSBINDINGS_DEFINES}")

	install(TARGETS aampjsbindings DESTINATION lib)
	set(LIBAAMP_DEFINES "${LIBAAMP_DEFINES} -DAAMP_WPEWEBKIT_JSBINDINGS")
else()
    message("CMAKE_WPEWEBKIT_JSBINDINGS not set, not creating jsbinding library")
endif()

if(CMAKE_AUXILIARY_AUDIO_ENABLED)
	message("CMAKE_AUXILIARY_AUDIO_ENABLED set")
	set(LIBAAMP_DEFINES "${LIBAAMP_DEFINES} -DAAMP_AUXILIARY_AUDIO_ENABLED")
endif()

message("LIB_AAMP_DEPENDS is ${LIBAAMP_DEPENDS}")
target_link_libraries(aamp ${LIBAAMP_DEPENDS} ${LIBAAMP_MOCK_DEPENDS})

if(CMAKE_ENABLE_PTS_RESTAMP)
	message("ENABLE_PTS_RESTAMP set")
	set(LIBAAMP_DEFINES "${LIBAAMP_DEFINES} -DENABLE_PTS_RESTAMP")
endif()

target_link_libraries(aamp-cli aamp ${AAMP_CLI_LD_FLAGS})

set_target_properties(aamp PROPERTIES COMPILE_FLAGS "${LIBAAMP_DEFINES} ${OS_CXX_FLAGS}")
#aamp-cli is not an ideal standalone app. It uses private aamp instance for debugging purposes
set_target_properties(aamp-cli PROPERTIES COMPILE_FLAGS "${LIBAAMP_DEFINES} ${AAMP_CLI_EXTRA_DEFINES} ${OS_CXX_FLAGS}")

set_target_properties(aamp PROPERTIES PUBLIC_HEADER "main_aamp.h")
set_target_properties(aamp PROPERTIES PRIVATE_HEADER "priv_aamp.h")

if (CMAKE_INBUILT_AAMP_DEPENDENCIES)
	include_directories(support/aampabr)
	include_directories(support/aampmetrics)
endif()
include_directories(tsb/api)

set(GSTTESTHARNESS_DEPENDS ${OS_LD_FLAGS} ${CMAKE_THREAD_LIBS_INIT} ${GSTREAMER_LINK_LIBRARIES} ${GSTREAMERBASE_LINK_LIBRARIES} ${LIBDASH_LIBRARIES} ${CURL_LINK_LIBRARIES})
set(GSTTESTHARNESS_HEADERS test/gstTestHarness/gst-port.h
                           test/gstTestHarness/gst-test.h
                           test/gstTestHarness/gst-utils.h
                           test/gstTestHarness/mp4demux.hpp
                           test/gstTestHarness/tsdemux.hpp
                           test/gstTestHarness/dash_adapter.hpp
                           test/gstTestHarness/downloader.hpp
                           test/gstTestHarness/stream_utils.hpp
                           test/gstTestHarness/string_utils.hpp
                           test/gstTestHarness/turbo_xml.hpp )
set(GSTTESTHARNESS_SOURCES test/gstTestHarness/gst-port.cpp
                           test/gstTestHarness/gst-utils.cpp
                           test/gstTestHarness/gst-test.cpp
                           test/gstTestHarness/mp4demux.cpp
                           test/gstTestHarness/dash_adapter.cpp
                           test/gstTestHarness/downloader.cpp
                           test/gstTestHarness/stream_utils.cpp
						   test/gstTestHarness/string_utils.cpp)

add_executable(gstTestHarness ${GSTTESTHARNESS_HEADERS} ${GSTTESTHARNESS_SOURCES})
target_link_libraries(gstTestHarness ${GSTTESTHARNESS_DEPENDS} "-lreadline")
# XCode schema target
xcode_define_schema(gstTestHarness)

install(TARGETS gstTestHarness DESTINATION bin)
target_link_libraries(aamp-cli tsb)
target_link_libraries(aamp tsb)
if (CMAKE_INBUILT_AAMP_DEPENDENCIES)
	target_link_libraries(aamp abr metrics)
endif()
target_link_libraries(aamp playergstinterface playerfbinterface)
target_link_libraries(aamp-cli playergstinterface playerfbinterface)

install(TARGETS aamp-cli DESTINATION bin)

install(TARGETS aamp DESTINATION lib PUBLIC_HEADER DESTINATION include PRIVATE_HEADER DESTINATION include)

install(FILES Accessibility.hpp AampEvent.h AampConfig.h AampCMCDCollector.h AampEventManager.h AampDefine.h AampEventListener.h AampMediaType.h
  AampLogManager.h subtitle/vttCue.h AampUtils.h
  AampProfiler.h AampConstants.h iso639map.h AampGrowableBuffer.h
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
  tsb/api/TsbApi.h
  DESTINATION include)

if (UTEST_ENABLED)
	add_subdirectory(test/utests EXCLUDE_FROM_ALL)
endif()

if (CMAKE_PLATFORM_UBUNTU OR CMAKE_SYSTEM_NAME STREQUAL Darwin )
    install(FILES build/aampcli-run-subtec.sh DESTINATION bin)
endif()

message("LIBAAMP_DEFINES is ${LIBAAMP_DEFINES}")
message("LIBAAMP_SOURCES is ${LIBAAMP_SOURCES}")
