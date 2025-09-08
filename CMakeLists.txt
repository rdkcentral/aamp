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
set(CMAKE_CXX_STANDARD 11)
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

if(APPLE)
	# libcurl < 8.5 exhibits memory leaks. On Ubuntu 22.04 can't update beyond 7.81.0-1ubuntu1.16 without building from source
	pkg_check_modules(CURL REQUIRED libcurl>=8.5)
else()
	pkg_check_modules(CURL REQUIRED libcurl)
endif()

# Platform specific settings
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
		set(OS_CXX_FLAGS "${OS_CXX_FLAGS} -g -x objective-c++ -Wno-inconsistent-missing-override -F${osxSdkPath}/System/Library/Frameworks")
		set(OS_LD_FLAGS "${OS_LD_FLAGS} -F${osxSdkPath}/System/Library/Frameworks -framework Cocoa -L${osxSdkPath}/../MacOSX.sdk/usr/lib -L.libs/lib -L/usr/local/lib/")
		set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -isysroot ${osxSdkPath}/../MacOSX.sdk -I/usr/local/include")
		string(STRIP ${OS_LD_FLAGS} OS_LD_FLAGS)
		pkg_check_modules(GSTREAMERVIDEO REQUIRED gstreamer-video-1.0)
		set(AAMP_CLI_LD_FLAGS ${AAMP_CLI_LD_FLAGS} ${GSTREAMERVIDEO_LINK_LIBRARIES})
		string(STRIP "${AAMP_CLI_LD_FLAGS}" AAMP_CLI_LD_FLAGS)
		message("AAMP_CLI_LD_FLAGS: ${AAMP_CLI_LD_FLAGS}")
		link_directories(${OPENSSL_LIBRARY_DIRS})
		pkg_check_modules(GLIB REQUIRED GLib-2.0)
		include_directories(${GLIB_INCLUDE_DIRS})
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

include_directories(${GSTREAMER_INCLUDE_DIRS})
include_directories(${CURL_INCLUDE_DIRS})
include_directories(${GSTREAMERBASE_INCLUDE_DIRS})
include_directories(${LIBDASH_INCLUDE_DIRS})
include_directories(${LIBXML2_INCLUDE_DIRS})
include_directories(${OPENSSL_INCLUDE_DIRS})
include_directories(${UUID_INCLUDE_DIRS})
include_directories(${LIBCJSON_INCLUDE_DIRS})

include_directories(${CMAKE_CURRENT_SOURCE_DIR})
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/dash/xml)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/dash/mpd)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/dash/utils)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/downloader)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/drm)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/isobmff)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/jsbindings)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/jsbindings/PersistentWatermark)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/subtec/subtecparser)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/subtitle)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/test/gstTestHarness)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/tsb/api)

include_directories(${CMAKE_CURRENT_SOURCE_DIR}/middleware)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/middleware/baseConversion)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/middleware/closedcaptions)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/middleware/closedcaptions/subtec)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/middleware/drm)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/middleware/drm/aes)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/middleware/drm/helper)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/middleware/drm/ocdm)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/middleware/externals)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/middleware/externals/contentsecuritymanager)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/middleware/externals/rdk)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/middleware/playerisobmff)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/middleware/playerJsonObject)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/middleware/playerLogManager)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/middleware/subtec/libsubtec)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/middleware/subtec/subtecparser)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/middleware/subtitle)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/middleware/vendor)

# Locally built/installed dependencies are here
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/.libs/include)

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

# TDB needs to bring back for UT. 
#include(test/mocks/mocks.cmake NO_POLICY_SCOPE)

# Adding subdirectories for internal libraries
add_subdirectory(middleware)

if (CMAKE_INBUILT_AAMP_DEPENDENCIES)
	message("Building aamp support libraries")
	include_directories(support/aampabr)
	include_directories(support/aampmetrics)

	add_subdirectory(support/aampabr)
	add_subdirectory(support/aampmetrics)
	add_subdirectory(plugins/gst-plugins-rdk-aamp)
	set(LIBAAMP_DEPENDS ${LIBAAMP_DEPENDS} abr metrics)
endif()

message("Adding tsb folder")
add_subdirectory(tsb)

install(TARGETS tsb
		DESTINATION lib
		PUBLIC_HEADER DESTINATION include
)

# Building jsbindings
if(CMAKE_WPEWEBKIT_JSBINDINGS)
	message("CMAKE_WPEWEBKIT_JSBINDINGS is set, building jsbindings library")
	# Search for WPE WebKit libraries
	pkg_check_modules(WPE_WEBKIT wpe-webkit-1.1)
	if(NOT WPE_WEBKIT_FOUND)
		pkg_check_modules(WPE_WEBKIT wpe-webkit-1.0)
	endif()
	# Check if any WPE WebKit library was found, if not, throw an error
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
		set(LIBJSBINDINGS_SOURCES
			"${LIBJSBINDINGS_SOURCES}"
			jsbindings/PersistentWatermark/PersistentWatermarkDisplaySequencer.cpp
			jsbindings/PersistentWatermark/PersistentWatermarkEventHandler.cpp
			jsbindings/PersistentWatermark/PersistentWatermarkPluginAccess.cpp
			jsbindings/PersistentWatermark/PersistentWatermarkStorage.cpp
		)
	endif()

	add_library(aampjsbindings SHARED ${LIBJSBINDINGS_SOURCES})
	target_link_libraries(aampjsbindings PUBLIC aamp ${LIBAAMPJSBINDINGS_DEPENDS})
	set_target_properties(aampjsbindings PROPERTIES COMPILE_FLAGS "${LIBAAMPJSBINDINGS_DEFINES}")
	install(TARGETS aampjsbindings
			DESTINATION lib
	)
else()
	message("CMAKE_WPEWEBKIT_JSBINDINGS not set")
endif()

set(LIBAAMP_SOURCES
	AampConstants.h AampDefine.h AampMediaType.h uint33_t.h
    AAMPAnomalyMessageType.h AuthTokenErrors.h
	iso639map.cpp iso639map.h
	AampCacheHandler.cpp AampCacheHandler.h
	AampGrowableBuffer.cpp AampGrowableBuffer.h
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
	ProcessHandler.cpp ProcessHandler.h
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
	test/gstTestHarness/mp4demux.hpp
	middleware/drm/aes/Aes.cpp
	middleware/drm/helper/DrmHelper.cpp
	middleware/gstplayertaskpool.cpp
	middleware/PlayerUtils.cpp
	middleware/drm/base64.cpp
	middleware/drm/DrmJsonObject.cpp
	middleware/vendor/SocInterface.cpp
	middleware/vendor/amlogic/AmlogicSocInterface.cpp
	middleware/vendor/realtek/RealtekSocInterface.cpp
	middleware/vendor/brcm/BrcmSocInterface.cpp
	middleware/vendor/default/DefaultSocInterface.cpp
	middleware/PlayerScheduler.cpp
	middleware/gstplayertaskpool.cpp
	middleware/GstHandlerControl.cpp
	middleware/GstUtils.cpp
	middleware/InterfacePlayerRDK.cpp
	middleware/SocUtils.cpp
	middleware/closedcaptions/PlayerCCManager.cpp
	middleware/PlayerUtils.cpp
	middleware/playerLogManager/PlayerLogManager.cpp
	middleware/playerisobmff/playerisobmffbox.cpp
	middleware/playerisobmff/playerisobmffbuffer.cpp
	middleware/subtec/subtecparser/WebVttSubtecParser.cpp
	middleware/subtec/subtecparser/TtmlSubtecParser.cpp
	middleware/subtec/subtecparser/WebvttSubtecDevInterface.cpp
	middleware/subtec/subtecparser/TextStyleAttributes.cpp
	middleware/playerJsonObject/PlayerJsonObject.cpp
)

if(CMAKE_SOC_PLATFORM_RPI)
	message("CMAKE_SOC_PLATFORM_RPI set")
	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DRPI=1")
endif()

set(LIBAAMP_DEFINES "${LIBAAMP_DEFINES} -DAAMP_VANILLA_AES_SUPPORT -DAAMP_BUILD_INFO='${buildinfo}' -DSUPPORTS_MP4DEMUX")

if(CMAKE_USE_RDK_PLUGINS)
	message("CMAKE_USE_RDK_PLUGINS set")
	set(LIBAAMP_DEFINES "${LIBAAMP_DEFINES} -DCREATE_PIPE_SESSION_TO_XRE")
endif()

if(CMAKE_IARM_MGR)
	message("CMAKE_IARM_MGR set")
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

# Sources common to all forms of OpenCDM and CDM
set(LIBAAMP_DRM_SOURCES
	middleware/drm/DrmSessionManager.cpp
	middleware/drm/PlayerHlsDrmSessionInterface.cpp
	middleware/drm/processProtectionHls.cpp
	middleware/drm/DrmSession.cpp
	middleware/drm/DrmSessionFactory.cpp
	middleware/drm/helper/DrmHelperFactory.cpp
	middleware/drm/DrmUtils.cpp
	middleware/drm/DrmSystems.h
	middleware/drm/HlsOcdmBridgeInterface.cpp
)

if(CMAKE_USE_THUNDER_OCDM_API_0_2)
	set(LIBAAMP_DEFINES "${LIBAAMP_DEFINES} -DUSE_THUNDER_OCDM_API_0_2")
endif()

if(CMAKE_USE_OPENCDM_ADAPTER)
	message("OpenCDM support enabled")
	set(LIBAAMP_DEFINES "${LIBAAMP_DEFINES} -DUSE_OPENCDM_ADAPTER -DTARGET_LITTLE_ENDIAN=1 -DTARGET_SUPPORTS_UNALIGNED_DWORD_POINTERS=0")
	set(LIBAAMP_DRM_SOURCES
		"${LIBAAMP_DRM_SOURCES}"
		middleware/drm/HlsDrmSessionManager.cpp
		middleware/drm/HlsOcdmBridge.cpp
		middleware/drm/processProtectionHls.cpp
		middleware/drm/ocdm/opencdmsessionadapter.cpp
		middleware/drm/ocdm/OcdmBasicSessionAdapter.cpp
		middleware/drm/ocdm/OcdmGstSessionAdapter.cpp
	)

	# DRM Helpers
	if(CMAKE_USE_WIDEVINE)
		set(LIBAAMP_DRM_SOURCES "${LIBAAMP_DRM_SOURCES}" middleware/drm/helper/WidevineDrmHelper.cpp)
	endif()

	if(CMAKE_USE_CLEARKEY)
		set(LIBAAMP_DRM_SOURCES "${LIBAAMP_DRM_SOURCES}" middleware/drm/helper/ClearKeyHelper.cpp)
	endif()

	if(CMAKE_USE_PLAYREADY)
		set(LIBAAMP_DRM_SOURCES "${LIBAAMP_DRM_SOURCES}" middleware/drm/helper/PlayReadyHelper.cpp)
	endif()

	if(CMAKE_USE_VERIMATRIX)
		message("CMAKE_USE_VERIMATRIX set")
		set(LIBAAMP_DRM_SOURCES "${LIBAAMP_DRM_SOURCES}" middleware/drm/helper/VerimatrixHelper.cpp)
	endif()

	if(CMAKE_USE_OPENCDM_ADAPTER_MOCKS)
		set(LIBAAMP_MOCK_SOURCES ${LIBAAMP_MOCK_SOURCES} test/mocks/opencdmMocks.cpp)
		set(LIBAAMP_MOCK_DEPENDS -lgmock -lgtest)
	else()
		set(LIBAAMP_DEPENDS ${LIBAAMP_DEPENDS} "-locdm -lgstvideo-1.0")
	endif()
	find_path (STAGING_INCDIR opencdm)
	include_directories(${STAGING_INCDIR}/opencdm)
else()
	message("No OpenCDM support enabled")
	if(CMAKE_USE_CLEARKEY)
		message("CMAKE_USE_CLEARKEY set")
		set(LIBAAMP_DRM_SOURCES
			"${LIBAAMP_DRM_SOURCES}"
			middleware/drm/ClearKeyDrmSession.cpp
			middleware/drm/helper/ClearKeyHelper.cpp
		)
		set(LIBAAMP_DEFINES "${LIBAAMP_DEFINES} -DUSE_CLEARKEY")
	endif()
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

if (CMAKE_AAMP_RFC_REQUIRED)
	message("AAMP RFC set")
	if(CMAKE_USE_RFC_MOCKS)
		copy_staging_header_file(tr181api.h)
		set(LIBAAMP_MOCK_SOURCES ${LIBAAMP_MOCK_SOURCES} test/mocks/rfcMocks.cpp)
		set(LIBAAMP_MOCK_DEPENDS -lgmock -lgtest)
	endif()
endif()

set(AAMP_SUBTEC_SOURCES
	middleware/playerLogManager/PlayerLogManager.cpp
	middleware/subtec/libsubtec/PacketSender.cpp
	middleware/subtec/libsubtec/SubtecChannel.cpp
)

add_library(subtec SHARED ${AAMP_SUBTEC_SOURCES})
# Public headers are to be installed
set(AAMP_SUBTEC_PUBLIC_HEADERS
	middleware/subtec/libsubtec/SubtecChannel.hpp
	middleware/subtec/libsubtec/SubtecAttribute.hpp
	middleware/PlayerMetadata.hpp
)
set_target_properties(subtec PROPERTIES PUBLIC_HEADER "${AAMP_SUBTEC_PUBLIC_HEADERS}")
set(AAMP_SUBTEC_COMPILE_FLAGS "${CMAKE_CXX_FLAGS} -DSUBTEC_PACKET_DEBUG=1")
if(CMAKE_PLATFORM_UBUNTU)
	set(AAMP_SUBTEC_COMPILE_FLAGS "${AAMP_SUBTEC_COMPILE_FLAGS} -DUBUNTU=1")
endif()
set_target_properties(subtec PROPERTIES COMPILE_FLAGS "${AAMP_SUBTEC_COMPILE_FLAGS}")
install (TARGETS subtec
		DESTINATION lib
		PUBLIC_HEADER DESTINATION include
)

if (CMAKE_GST_SUBTEC_ENABLED)
	set(CMAKE_SUBTITLE_SUPPORT TRUE)
	message("CMAKE_GST_SUBTEC_ENABLED set")
	set(LIBAAMP_DEFINES "${LIBAAMP_DEFINES} -DGST_SUBTEC_ENABLED")
endif()

if (CMAKE_SUBTITLE_SUPPORT)
	message("CMAKE_SUBTITLE_SUPPORT set")
	set(LIBAAMP_DEFINES "${LIBAAMP_DEFINES} -DSUBTITLE_SUPPORTED")
	find_path(STAGING_INCDIR closedcaption/ccDataReader.h)
	include_directories(${STAGING_INCDIR}/closedcaption)
	if (CMAKE_USE_CC_MANAGER_MOCKS)
		set(LIBAAMP_MOCK_SOURCES ${LIBAAMP_MOCK_SOURCES} test/fakes/ccManagerFakes.cpp)
	else()
		set(LIBSUBTECCONNECTOR_DEPENDS pthread rdkCCReader subtec)
	endif()

	set(LIBSUBTECCONNECTOR_SOURCES
		${AAMP_SUBTEC_SOURCES}
		middleware/closedcaptions/subtec/SubtecConnector.cpp
		middleware/closedcaptions/subtec/CCDataController.cpp
	)

	add_library(subtec_connector SHARED ${LIBSUBTECCONNECTOR_SOURCES})
	target_link_libraries(subtec_connector ${LIBSUBTECCONNECTOR_DEPENDS})
	install(TARGETS subtec_connector
			DESTINATION lib
	)
	set(LIBAAMP_SOURCES "${LIBAAMP_SOURCES}" middleware/closedcaptions/subtec/PlayerSubtecCCManager.cpp)
	set(LIBAAMP_DEPENDS ${LIBAAMP_DEPENDS} subtec_connector)
endif()

if (COVERAGE_ENABLED)
	set(LIBAAMP_DEFINES "${LIBAAMP_DEFINES} --coverage")
	set(LIBAAMP_DEPENDS ${LIBAAMP_DEPENDS} "--coverage")
endif()

if(CMAKE_SYSTEMD_JOURNAL)
	message("CMAKE_SYSTEMD_JOURNAL set")
	set(LIBAAMP_DEPENDS ${LIBAAMP_DEPENDS} "-lsystemd")
	set(LIBAAMP_DEFINES "${LIBAAMP_DEFINES} -DUSE_SYSTEMD_JOURNAL_PRINT=1 -DSD_JOURNAL_SUPPRESS_LOCATION=1")
	set(LIBAAMPJSBINDINGS_DEPENDS ${LIBAAMPJSBINDINGS_DEPENDS} "-lsystemd")
	set(LIBAAMPJSBINDINGS_DEFINES "${LIBAAMPJSBINDINGS_DEFINES} -DUSE_SYSTEMD_JOURNAL_PRINT=1 -DSD_JOURNAL_SUPPRESS_LOCATION=1")
endif()

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

if(CMAKE_AUXILIARY_AUDIO_ENABLED)
	message("CMAKE_AUXILIARY_AUDIO_ENABLED set")
	set(LIBAAMP_DEFINES "${LIBAAMP_DEFINES} -DAAMP_AUXILIARY_AUDIO_ENABLED")
endif()

if(CMAKE_ENABLE_PTS_RESTAMP)
	message("ENABLE_PTS_RESTAMP set")
	set(LIBAAMP_DEFINES "${LIBAAMP_DEFINES} -DENABLE_PTS_RESTAMP")
endif()

set(LIBAAMP_SOURCES "${LIBAAMP_SOURCES}" "${LIBAAMP_DRM_SOURCES}" "${LIBAAMP_MOCK_SOURCES}")
add_library(aamp SHARED ${LIBAAMP_SOURCES})
target_link_libraries(aamp subtec tsb playerfbinterface ${LIBAAMP_DEPENDS} ${LIBAAMP_MOCK_DEPENDS})
set_target_properties(aamp PROPERTIES COMPILE_FLAGS "${LIBAAMP_DEFINES} ${OS_CXX_FLAGS}")
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
target_link_libraries(aamp-cli aamp subtec tsb playerfbinterface ${AAMP_CLI_LD_FLAGS} "-lreadline")
#aamp-cli is not an ideal standalone app. It uses private aamp instance for debugging purposes
set_target_properties(aamp-cli PROPERTIES COMPILE_FLAGS "${LIBAAMP_DEFINES} ${OS_CXX_FLAGS}")
install(TARGETS aamp-cli
		DESTINATION bin
)
# XCode schema target
xcode_define_schema(aamp-cli)

if (CMAKE_PLATFORM_UBUNTU OR CMAKE_SYSTEM_NAME STREQUAL Darwin )
	set(AAMP_KOTLIN_SRC
		${AAMP_CLI_SOURCES}
		test/aampcli/aampcli_kmp.cpp
	)
	add_library(aampKotlin SHARED ${AAMP_KOTLIN_SRC})
	install (TARGETS aampKotlin
			DESTINATION lib
	)
	target_link_libraries(aampKotlin aamp subtec tsb ${AAMP_CLI_LD_FLAGS} "-lreadline")
	set_target_properties(aampKotlin PROPERTIES COMPILE_FLAGS "${LIBAAMP_DEFINES} ${OS_CXX_FLAGS}")
	xcode_define_schema(aampKotlin)
endif()

set(GSTTESTHARNESS_DEPENDS
	${OS_LD_FLAGS}
	${CMAKE_THREAD_LIBS_INIT}
	${GSTREAMER_LINK_LIBRARIES}
	${GSTREAMERBASE_LINK_LIBRARIES}
	${LIBDASH_LIBRARIES}
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
	middleware/drm/DrmMemorySystem.h middleware/drm/DrmSessionManager.h
	middleware/drm/DrmSystems.h middleware/drm/DrmData.h middleware/drm/DrmInfo.h
	middleware/drm/DrmMediaFormat.h middleware/drm/DrmCallbacks.h
	middleware/drm/DrmSession.h middleware/drm/ClearKeyDrmSession.h
	middleware/drm/DrmUtils.h middleware/drm/DrmSessionFactory.h
	middleware/drm/ocdm/opencdmsessionadapter.h middleware/PlayerUtils.h
	middleware/drm/DrmConstants.h middleware/GstUtils.h
	middleware/drm/helper/DrmHelper.h
	subtitle/vttCue.h AampUtils.h
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
	middleware/closedcaptions/CCTrackInfo.h
	middleware/PlayerScheduler.h
	middleware/gstplayertaskpool.h
	middleware/GstHandlerControl.h
	middleware/GstUtils.h
	middleware/InterfacePlayerRDK.h
	middleware/SocUtils.h
	middleware/closedcaptions/PlayerCCManager.h
	middleware/PlayerUtils.h
	LangCodePreference.h StreamOutputFormat.h VideoZoomMode.h StreamSink.h TimedMetadata.h
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
message("LIBAAMP_DRM_SOURCES is ${LIBAAMP_DRM_SOURCES}")
message("LIBAAMP_SOURCES is ${LIBAAMP_SOURCES}")
