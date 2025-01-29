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
function(xcode_define_schema new_schema)
    message( "xcode_define_schema for ${new_schema}" )
    set_property(TARGET ${new_schema} PROPERTY XCODE_GENERATE_SCHEME TRUE)
    set_property(TARGET ${new_schema} PROPERTY XCODE_SCHEME_ADDRESS_SANITIZER TRUE)
    set_property(TARGET ${new_schema} PROPERTY XCODE_SCHEME_ADDRESS_SANITIZER_USE_AFTER_RETURN TRUE)
endfunction()

include_directories(${CMAKE_CURRENT_SOURCE_DIR}/middleware)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/test/gstTestHarness)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/externals)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/externals/rdk)

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


if (CMAKE_WPEFRAMEWORK_REQUIRED)
    message("CMAKE_WPEFRAMEWORK_REQUIRED set")
list(APPEND CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/cmake/")
find_package(WPEFramework REQUIRED)
if (WPEFRAMEWORK_FOUND)
    message("WPEFRAMEWORK_FOUND = ${WPEFRAMEWORK_FOUND}")
    #include_directories(${CMAKE_CURRENT_SOURCE_DIR} PRIVATE ${WPEFRAMEWORK_INCLUDE_DIRS})
    # Sets the include paths to Linux/includem. By making it a system include the headers won't report any errors ( effc++ )
    include_directories(SYSTEM ${WPEFRAMEWORK_INCLUDE_DIRS})
else()
if (USE_THUNDER_R4)
       message(FATAL_ERROR "WPEFrameworkCOM not found")
else()
    message(FATAL_ERROR "WPEFrameworkProtocols not found")
endif()
endif()
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
    set(AAMP_OS_SOURCES cocoa_window.mm)
    link_directories(${OPENSSL_LIBRARY_DIRS})
    set(CMAKE_THREAD_LIBS_INIT "-lpthread")
    set(CMAKE_HAVE_THREADS_LIBRARY 1)
    pkg_check_modules(GLIB REQUIRED GLib-2.0)
    include_directories(${GLIB_INCLUDE_DIRS})

    # XCode build flags. Even when using CLANG, the GCC name is required to enable the check
    set(CMAKE_XCODE_ATTRIBUTE_GCC_WARN_UNUSED_FUNCTION "YES")
    set(CMAKE_XCODE_ATTRIBUTE_GCC_WARN_UNUSED_VARIABLE "YES")
else()
    set(USE_MAC_FOR_RANDOM_GEN "-DUSE_MAC_FOR_RANDOM_GEN")
endif(CMAKE_SYSTEM_NAME STREQUAL Darwin)

find_package (Threads REQUIRED)

include_directories(${CMAKE_CURRENT_SOURCE_DIR} drm downloader drm/aes drm/helper subtitle isobmff dash/xml dash/mpd dash/utils)
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
					AampDrmCallbacks.h
					AampDrmSystems.h
					AampEvent.h
					AampEventListener.h
					AampEventManager.h
					middleware/GstUtils.h
					AampGrowableBuffer.h
					AampJsonObject.h
					AampLogManager.h
					AampMediaType.h
					AampMemoryUtils.h
					AampProfiler.h
					externals/PlayerRfc.h
					AampScheduler.h
					AampUtils.h
					AdManagerBase.h
					MediaStreamContext.h
					Module.h
					StreamAbstractionAAMP.h
					ThunderAccess.h
					_base64.h
					middleware/PlayerLogManager.h
					middleware/PlayerScheduler.h
					middleware/gstplayertaskpool.h
					middleware/GstHandlerControl.h
					middleware/GstUtils.h
					middleware/InterfacePlayerRDK.h
					externals/playerIarmRfcInterface.h
					aampgstplayer.h
					AampBufferControl.h
					admanager_mpd.h
					base16.h
					cocoa_window.h
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
					ProcessHandler.h
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
					AampTsbReader.h
					lstring.hpp
					scte35/AampSCTE35.h
					Accessibility.hpp
					AampTrackWorker.h
)

set(LIBAAMP_SOURCES iso639map.cpp
					base16.cpp
					_base64.cpp
					AampMemoryUtils.cpp
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
					middleware/PlayerLogManager.cpp
					middleware/PlayerScheduler.cpp
					middleware/gstplayertaskpool.cpp
					middleware/GstHandlerControl.cpp
					middleware/GstUtils.cpp
					middleware/InterfacePlayerRDK.cpp
					aampgstplayer.cpp
					AampBufferControl.cpp
					tsprocessor.cpp
					drm/aes/aamp_aes.cpp
					aamplogging.cpp
					AampConfig.cpp
					AampEventManager.cpp
					subtitle/webvttParser.cpp
					isobmff/isobmffbox.cpp
					isobmff/isobmffbuffer.cpp
					isobmff/isobmffprocessor.cpp
					isobmff/isobmffhelper.cpp
					drm/helper/AampDrmHelper.cpp
					MediaStreamContext.cpp
					downloader/AampCurlStore.cpp
					AampDRMLicPreFetcher.cpp
					AampCMCDCollector.cpp
					middleware/gstplayertaskpool.cpp
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
					ProcessHandler.cpp
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
					AampTsbReader.cpp
					scte35/AampSCTE35.cpp
					AampTrackWorker.cpp
)

if (CMAKE_WPEFRAMEWORK_REQUIRED)
	set(LIBAAMP_SOURCES ${LIBAAMP_SOURCES} ThunderAccess.cpp Module.cpp)
endif()

if (CMAKE_PLATFORM_UBUNTU OR CMAKE_SYSTEM_NAME STREQUAL Darwin )
  message("AAMP_SIMULATOR_BUILD set")
  set(LIBAAMP_DEFINES "${LIBAAMP_DEFINES} -DAAMP_SIMULATOR_BUILD=1")
  set(CMAKE_TELEMETRY_2_0_REQUIRED TRUE)
  set(CMAKE_AAMP_SIMULATOR TRUE)
  # uncomment below to build additional drm support in simulator
  # set(CMAKE_USE_OPENCDM_ADAPTER TRUE)
  # set(CMAKE_USE_OPENCDM_ADAPTER_MOCKS TRUE)
  # set(CMAKE_USE_THUNDER_OCDM_API_0_2 TRUE)
  # set(CMAKE_USE_SECCLIENT TRUE)
  # set(CMAKE_USE_SECCLIENT_MOCKS TRUE)
endif()

if(CMAKE_SOC_PLATFORM_RPI)
    message("CMAKE_SOC_PLATFORM_RPI set")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DRPI=1")
endif()

if(CMAKE_USE_SECCLIENT)
	if (CMAKE_USE_SECCLIENT_MOCKS)
		set(LIBAAMP_HEADERS ${LIBAAMP_HEADERS} sec_client.h sec_security_datatype.h)
		# copy_staging_header_file(sec_client.h)
		# copy_staging_header_file(sec_security_datatype.h)
		set(LIBAAMP_MOCK_SOURCES ${LIBAAMP_MOCK_SOURCES} test/mocks/secClientMocks.cpp)
		set(LIBAAMP_MOCK_DEPENDS -lgmock -lgtest)
	else()
		set(SEC_CLIENT_LIB -lSecClient )
	endif()
endif()
if(CMAKE_USE_SECMANAGER)
	if (NOT CMAKE_WPEFRAMEWORK_REQUIRED)
		message(FATAL_ERROR "CMAKE_USE_SECMANAGER set but CMAKE_WPEFRAMEWORK_REQUIRED not set")
	else()
		set(LIBAAMP_SOURCES ${LIBAAMP_SOURCES} drm/AampSecManager.cpp)
	endif()
endif()

execute_process(COMMAND bash "-c" "${CMAKE_SOURCE_DIR}/buildinfo.sh" OUTPUT_STRIP_TRAILING_WHITESPACE OUTPUT_VARIABLE buildinfo)
string(REGEX REPLACE "\n" "-" buildinfo "${buildinfo}")
message("build: ${buildinfo}")
set(LIBAAMP_DEFINES "${LIBAAMP_DEFINES} -DAAMP_VANILLA_AES_SUPPORT ${USE_MAC_FOR_RANDOM_GEN}")
set(LIBAAMP_DEFINES "${LIBAAMP_DEFINES} -DAAMP_BUILD_INFO='${buildinfo}'")

if (CMAKE_PLATFORM_UBUNTU)
	message("CMAKE_PLATFORM_UBUNTU set")
	set(LIBAAMP_DEFINES "${LIBAAMP_DEFINES} -DUBUNTU=1 -DNO_NATIVE_AV=1")
endif()

if(CMAKE_USE_RDK_PLUGINS)
	message("CMAKE_USE_RDK_PLUGINS set")
	set(LIBAAMP_DEFINES "${LIBAAMP_DEFINES} -DREAD_ESTB_IFACE_FROM_DEVICE_PROPERTIES -DCREATE_PIPE_SESSION_TO_XRE")
endif()

set(LIBAAMP_DEPENDS ${OS_LD_FLAGS} ${UUID_LINK_LIBRARIES} ${LIBCJSON_LINK_LIBRARIES} ${GSTREAMERBASE_LINK_LIBRARIES} ${GSTREAMER_LINK_LIBRARIES} ${CURL_LINK_LIBRARIES} ${LIBDASH_LINK_LIBRARIES} ${LibXml2_LIBRARIES} ${CMAKE_THREAD_LIBS_INIT} ${OPENSSL_LIBRARIES} ${OPENGL_LIBRARIES} ${GLEW_LIBRARIES} ${AAMP_CLI_LD_FLAGS} -ldl ${SEC_CLIENT_LIB})
set(PLAYBINTEST_DEPENDS ${OS_LD_FLAGS} ${CMAKE_THREAD_LIBS_INIT} ${GSTREAMER_LIBRARIES})

if(CMAKE_WPEWEBKIT_JSBINDINGS)
	message("CMAKE_WPEWEBKIT_JSBINDINGS set, just setting flags")
	set(LIBAAMP_DEFINES "${LIBAAMP_DEFINES} -DSUPPORT_JS_EVENTS")
else()
	message("CMAKE_WPEWEBKIT_JSBINDINGS not set")
endif()

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Werror=format -Wno-multichar -std=c++11 -Wno-non-virtual-dtor -Wno-psabi")

if(CMAKE_IARM_MGR)
	message("CMAKE_IARM_MGR set")
	set(LIBAAMP_DEFINES "${LIBAAMP_DEFINES} -DIARM_MGR=1")

	if (CMAKE_IARM_MGR_MOCKS)
		message("CMAKE_IARM_MGR_MOCKS set, building IARM stubs")
		set(LIBAAMP_MOCK_SOURCES ${LIBAAMP_MOCK_SOURCES} test/mocks/iarmMgrMocks.cpp)
		set(LIBAAMP_MOCK_DEPENDS -lgmock -lgtest)
		find_path(STAGING_INCDIR rdk/iarmbus)
		include_directories(${STAGING_INCDIR}/rdk/iarmbus)
		find_path(STAGING_INCDIR rdk/tr69hostif)
		include_directories(${STAGING_INCDIR}/rdk/tr69hostif)
		find_path(STAGING_INCDIR rdk/ds)
		include_directories(${STAGING_INCDIR}/rdk/ds
							${STAGING_INCDIR}/rdk/ds-hal
							${STAGING_INCDIR}/rdk/ds-rpc)
		find_path(STAGING_INCDIR rdk/iarmmgrs/ir)
		include_directories(${STAGING_INCDIR}/rdk/iarmmgrs/ir)
	endif()
endif()

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
			test/aampcli/AampcliSmokeTest.h
			test/aampcli/AampcliSubtecSimulator.h
			${AAMP_OS_SOURCES})
set(AAMP_CLI_SOURCES test/aampcli/Aampcli.cpp
			test/aampcli/AampcliPlaybackCommand.cpp
			test/aampcli/AampcliCommandHandler.cpp
			test/aampcli/AampcliGet.cpp
			test/aampcli/AampcliSet.cpp
			test/aampcli/AampcliVirtualChannelMap.cpp
			test/aampcli/AampcliShader.cpp
			test/aampcli/AampcliSmokeTest.cpp
			test/aampcli/AampcliSubtecSimulator.cpp
			${AAMP_OS_SOURCES})

if (NOT CMAKE_INBUILT_AAMP_DEPENDENCIES)
	set(LIBAAMP_DEPENDS "${LIBAAMP_DEPENDS} -labr -lmetrics")
endif()

# Sources common to all forms of OpenCDM and CDM
set(LIBAAMP_DRM_SOURCES drm/AampDRMSessionManager.cpp
						drm/AampDrmSession.cpp
						drm/aampdrmsessionfactory.cpp
						externals/playerIarmRfcInterface.cpp
						externals/playerExternalUtils.cpp
						drm/helper/AampDrmHelperFactory.cpp
						drm/AampDRMutils.cpp
)

if(CMAKE_IARM_MGR)
	set(LIBAAMP_DRM_SOURCES "${LIBAAMP_DRM_SOURCES}" externals/rdk/playerIarmRdkInterface.cpp externals/playerBase64.cpp)
endif()
if(CMAKE_USE_THUNDER_OCDM_API_0_2)
	set(LIBAAMP_DEFINES "${LIBAAMP_DEFINES} -DUSE_THUNDER_OCDM_API_0_2")
endif()

set(LIBAAMP_DEFINES "${LIBAAMP_DEFINES} -DAAMP_MPD_DRM=1")

if(CMAKE_USE_OPENCDM_ADAPTER)
    message("OpenCDM support enabled")
    set(LIBAAMP_DEFINES "${LIBAAMP_DEFINES} -DUSE_OPENCDM -DAAMP_HLS_DRM=1")
    set(LIBAAMP_DRM_SOURCES "${LIBAAMP_DRM_SOURCES}" drm/AampHlsDrmSessionManager.cpp
                                                     drm/AampHlsOcdmBridge.cpp
                                                     drm/processProtectionHls.cpp
    )
    set(LIBAAMP_DEFINES "${LIBAAMP_DEFINES} -DDRM_BUILD_PROFILE=DRM_BUILD_PROFILE_OEM -DTARGET_LITTLE_ENDIAN=1 -DTARGET_SUPPORTS_UNALIGNED_DWORD_POINTERS=0")

    if(CMAKE_IARM_MGR AND NOT CMAKE_IARM_MGR_MOCKS)
        set(LIBAAMP_DEPENDS "${LIBAAMP_DEPENDS} -lIARMBus -lds -ldshalcli")
    endif()

    # DRM Helpers
    if(CMAKE_USE_WIDEVINE)
        set(LIBAAMP_HELP_SOURCES "${LIBAAMP_HELP_SOURCES}" drm/helper/AampWidevineDrmHelper.cpp)
    endif()

    if(CMAKE_USE_CLEARKEY)
        set(LIBAAMP_HELP_SOURCES "${LIBAAMP_HELP_SOURCES}" drm/helper/AampClearKeyHelper.cpp)
    endif()

    if(CMAKE_USE_PLAYREADY)
        set(LIBAAMP_HELP_SOURCES "${LIBAAMP_HELP_SOURCES}" drm/helper/AampPlayReadyHelper.cpp)
    endif()

    if(CMAKE_USE_VERIMATRIX)
        message("CMAKE_USE_VERIMATRIX set")
        set(LIBAAMP_HELP_SOURCES "${LIBAAMP_HELP_SOURCES}" drm/helper/AampVerimatrixHelper.cpp)
    endif()
else()
    message("No OpenCDM support enabled")
endif()

if(CMAKE_USE_CLEARKEY)
    set(LIBAAMP_DRM_SOURCES "${LIBAAMP_DRM_SOURCES}" drm/ClearKeyDrmSession.cpp)
    set(LIBAAMP_HELP_SOURCES "${LIBAAMP_HELP_SOURCES}" drm/helper/AampClearKeyHelper.cpp)
    set(LIBAAMP_DEFINES "${LIBAAMP_DEFINES} -DUSE_CLEARKEY")
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

if(CMAKE_USE_OPENCDM_ADAPTER)
    message("Using OPEN CDM ADAPTER")
    set(LIBAAMP_DEFINES "${LIBAAMP_DEFINES} -DUSE_OPENCDM_ADAPTER -DUSE_OPENCDM")
    set(LIBAAMP_DRM_SOURCES "${LIBAAMP_DRM_SOURCES}" drm/opencdmsessionadapter.cpp
                                                     drm/AampOcdmBasicSessionAdapter.cpp
                                                     drm/AampOcdmGstSessionAdapter.cpp
    )

    # For extracting SEI timecode
    set(LIBAAMP_DEPENDS "${LIBAAMP_DEPENDS} -lgstvideo-1.0")

    if(CMAKE_USE_OPENCDM_ADAPTER_MOCKS)
        set(LIBAAMP_HEADERS ${LIBAAMP_HEADERS} open_cdm.h open_cdm_adapter.h)
        # copy_staging_header_file(open_cdm.h)
        # copy_staging_header_file(opencdmMocks.cpp)
        set(LIBAAMP_MOCK_SOURCES ${LIBAAMP_MOCK_SOURCES} test/mocks/opencdmMocks.cpp)
        set(LIBAAMP_MOCK_DEPENDS -lgmock -lgtest)
    else()
        set(LIBAAMP_DEPENDS "${LIBAAMP_DEPENDS} -locdm")
    endif()
    find_path (STAGING_INCDIR opencdm)
    include_directories(${STAGING_INCDIR}/opencdm)
    find_path (STAGING_INCDIR gstreamer-1.0)
    include_directories(${STAGING_INCDIR}/gstreamer-1.0)
endif()

include_directories(closedcaptions)
set(LIBAAMP_SOURCES ${LIBAAMP_SOURCES} closedcaptions/AampCCManager.cpp)

set(LIBAAMP_SOURCES ${LIBAAMP_SOURCES} externals/PlayerRfc.cpp 
externals/playerExternalUtils.cpp)
if (CMAKE_AAMP_RFC_REQUIRED)
        message("AAMP RFC set")
        set(LIBAAMP_DEFINES "${LIBAAMP_DEFINES} -DAAMP_RFC_ENABLED")
        if(CMAKE_USE_RFC_MOCKS)
            copy_staging_header_file(tr181api.h)
            set(LIBAAMP_MOCK_SOURCES ${LIBAAMP_MOCK_SOURCES} test/mocks/rfcMocks.cpp)
            set(LIBAAMP_MOCK_DEPENDS -lgmock -lgtest)
        else()
            set(LIBAAMP_DEPENDS "${LIBAAMP_DEPENDS} -ltr181api")
        endif()
endif()

set(AAMP_SUBTEC_CLASS_SOURCES subtec/subtecparser/WebVttSubtecParser.cpp subtec/subtecparser/TtmlSubtecParser.cpp subtec/subtecparser/WebvttSubtecDevParser.cpp subtec/subtecparser/TextStyleAttributes.cpp)
set(LIBAAMP_SOURCES ${LIBAAMP_SOURCES} ${AAMP_SUBTEC_CLASS_SOURCES})
include_directories(subtec/subtecparser)

add_library(subtec SHARED subtec/libsubtec/PacketSender.cpp subtec/libsubtec/SubtecChannel.cpp)
set(AAMP_SUBTEC_PUBLIC_HEADERS subtec/libsubtec/SubtecChannel.hpp subtec/libsubtec/SubtecAttribute.hpp)
set_target_properties(subtec PROPERTIES PUBLIC_HEADER "${AAMP_SUBTEC_PUBLIC_HEADERS}")
set(AAMP_SUBTEC_COMPILE_FLAGS "${CMAKE_CXX_FLAGS} -DSUBTEC_PACKET_DEBUG=1")
if(CMAKE_PLATFORM_UBUNTU)
	set(AAMP_SUBTEC_COMPILE_FLAGS "${AAMP_SUBTEC_COMPILE_FLAGS} -DUBUNTU=1")
endif()
set_target_properties(subtec PROPERTIES COMPILE_FLAGS "${AAMP_SUBTEC_COMPILE_FLAGS}")
include_directories(subtec/libsubtec)
install (TARGETS subtec
		DESTINATION lib
		PUBLIC_HEADER DESTINATION include
)

if (CMAKE_GST_SUBTEC_ENABLED)
    set(CMAKE_AAMP_SUBTITLE_SUPPORT TRUE)
    message("CMAKE_GST_SUBTEC_ENABLED set")
    set(LIBAAMP_DEFINES "${LIBAAMP_DEFINES} -DGST_SUBTEC_ENABLED")
endif()

if (CMAKE_AAMP_SUBTITLE_SUPPORT)
    message("CMAKE_AAMP_SUBTITLE_SUPPORT set")
    set(LIBAAMP_DEFINES "${LIBAAMP_DEFINES} -DAAMP_SUBTITLE_SUPPORTED")
    set(LIBSUBTECCONNECTOR_DEFINES "${LIBAAMP_DEFINES}")
    find_path(STAGING_INCDIR closedcaption/ccDataReader.h)
    include_directories(${STAGING_INCDIR}/closedcaption)
    if (CMAKE_USE_CC_MANAGER_MOCKS)
        set(LIBAAMP_MOCK_SOURCES ${LIBAAMP_MOCK_SOURCES} test/fakes/ccManagerFakes.cpp)
    else()
        set(LIBSUBTECCONNECTOR_DEPENDS pthread rdkCCReader subtec)
    endif()

    set(LIBSUBTECCONNECTOR_SOURCES  ${AAMP_SUBTEC_SOURCES}
        closedcaptions/subtec/SubtecConnector.cpp
        closedcaptions/subtec/CCDataController.cpp
        aamplogging.cpp)

    add_library(subtec_connector SHARED ${LIBSUBTECCONNECTOR_SOURCES})
    target_link_libraries(subtec_connector ${LIBSUBTECCONNECTOR_DEPENDS})
    install(TARGETS subtec_connector DESTINATION lib)

    set(LIBAAMP_SOURCES ${LIBAAMP_SOURCES} closedcaptions/subtec/AampSubtecCCManager.cpp)
endif()

set(LIBAAMP_DEFINES "${LIBAAMP_DEFINES} ${SEC_CONTENT_METADATA_ENABLED}")

set(LIBAAMP_SOURCES "${LIBAAMP_SOURCES}" "${LIBAAMP_DRM_SOURCES}" "${LIBAAMP_MOCK_SOURCES}")

if (COVERAGE_ENABLED)
    set(LIBAAMP_DEFINES "${LIBAAMP_DEFINES} --coverage")
    set(LIBAAMP_DEPENDS "${LIBAAMP_DEPENDS} --coverage")
endif()

add_library(aamp SHARED ${LIBAAMP_HEADERS} ${LIBAAMP_SOURCES} ${LIBAAMP_HELP_SOURCES})
add_executable(aamp-cli ${AAMP_CLI_HEADERS} ${AAMP_CLI_SOURCES})
target_link_libraries(aamp-cli "-lreadline")
# XCode schema target
xcode_define_schema(aamp-cli)

add_executable(playbintest middleware/test/playbintest.cpp)
target_link_libraries(playbintest ${PLAYBINTEST_DEPENDS})
# XCode schema target
xcode_define_schema(playbintest)

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


if(CMAKE_USE_SECCLIENT)
	message("CMAKE_USE_SECCLIENT set")
	set(LIBAAMP_DEFINES "${LIBAAMP_DEFINES} -DUSE_SECCLIENT")
endif()
if(CMAKE_USE_SECMANAGER)
	message("CMAKE_USE_SECMANAGER set")
	set(LIBAAMP_DEFINES "${LIBAAMP_DEFINES} -DUSE_SECMANAGER")
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
        set(LIBAAMPJSBINDINGS_DEFINES "${LIBAAMPJSBINDINGS_DEFINES} -DUSE_WATERMARK_JSBINDINGS -DUSE_CPP_THUNDER_PLUGIN_ACCESS")
        target_link_libraries(aampjsbindings ${WPEFRAMEWORK_LIBRARIES})
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

if (CMAKE_WPEFRAMEWORK_REQUIRED)
	set(LIBAAMP_DEFINES "${LIBAAMP_DEFINES} -DUSE_CPP_THUNDER_PLUGIN_ACCESS")
	target_link_libraries(aamp ${WPEFRAMEWORK_LIBRARIES})
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
                           test/gstTestHarness/mp4demux.hpp
                           test/gstTestHarness/tsdemux.hpp
                           test/gstTestHarness/dash_adapter.hpp
                           test/gstTestHarness/downloader.hpp
                           test/gstTestHarness/stream_utils.hpp
                           test/gstTestHarness/string_utils.hpp
                           test/gstTestHarness/turbo_xml.hpp
                           test/gstTestHarness/initializationheaderinfo.hpp )
set(GSTTESTHARNESS_SOURCES test/gstTestHarness/gst-port.cpp
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
target_link_libraries(aamp-cli subtec tsb)
target_link_libraries(aamp subtec tsb)
if (CMAKE_INBUILT_AAMP_DEPENDENCIES)
	target_link_libraries(aamp abr metrics)
endif()
if (CMAKE_AAMP_SUBTITLE_SUPPORT)
	target_link_libraries(aamp subtec_connector)
endif()
install(TARGETS aamp-cli DESTINATION bin)
install(TARGETS playbintest DESTINATION bin)

install(TARGETS aamp DESTINATION lib PUBLIC_HEADER DESTINATION include PRIVATE_HEADER DESTINATION include)
install(FILES   
  Accessibility.hpp AampEvent.h AampConfig.h AampCMCDCollector.h AampEventManager.h AampDefine.h AampEventListener.h drm/AampMemorySystem.h drm/AampDRMSessionManager.h AampDrmSystems.h AampMediaType.h
  drm/AampDrmData.h drm/AampDrmInfo.h drm/AampDrmMediaFormat.h AampDrmCallbacks.h AampLogManager.h
  drm/AampDrmSession.h drm/ClearKeyDrmSession.h drm/AampDRMutils.h drm/aampdrmsessionfactory.h
  drm/helper/AampDrmHelper.h subtitle/vttCue.h AampUtils.h
  AampMemoryUtils.h AampProfiler.h AampConstants.h iso639map.h externals/PlayerRfc.h externals/playerBase64.h middleware/GstUtils.h AampGrowableBuffer.h
  AampScheduler.h
  drm/AampSecManager.h
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
  closedcaptions/AampCCManager.h
  middleware/PlayerLogManager.h
  middleware/PlayerScheduler.h
  middleware/gstplayertaskpool.h
  middleware/GstHandlerControl.h
  middleware/GstUtils.h
  middleware/InterfacePlayerRDK.h
  DESTINATION include)


# Might need to bring back later. 
#if (UTEST_ENABLED)
#	add_subdirectory(test/utests EXCLUDE_FROM_ALL)
#endif()

if (CMAKE_PLATFORM_UBUNTU OR CMAKE_SYSTEM_NAME STREQUAL Darwin )
    install(FILES build/aampcli-run-subtec.sh DESTINATION bin)
endif()

message("LIBAAMP_DEFINES is ${LIBAAMP_DEFINES}")
message("LIBAAMP_SOURCES is ${LIBAAMP_SOURCES}")
message("LIBAAMP_HELP_SOURCES is ${LIBAAMP_HELP_SOURCES}")
