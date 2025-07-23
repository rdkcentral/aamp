
# LibPlayerGstInterface CMakeLists.txt

cmake_minimum_required(VERSION 3.5)
project(Playergstinterface)

set(CMAKE_CXX_STANDARD 14)

find_package(PkgConfig REQUIRED)

option(DISABLE_SECURITY_TOKEN "Disable security token" OFF)

if(DISABLE_SECURITY_TOKEN)
  add_definitions(-DDISABLE_SECURITY_TOKEN)
endif()

pkg_check_modules(GST REQUIRED gstreamer-plugins-base-1.0)
pkg_check_modules(GSTREAMER REQUIRED gstreamer-1.0)
pkg_check_modules(GSTREAMERBASE REQUIRED gstreamer-app-1.0)
pkg_check_modules(GSTREAMERVIDEO REQUIRED gstreamer-video-1.0)

include_directories(${GST_INCLUDE_DIRS} ${GSTREAMER_INCLUDE_DIRS} ${GSTREAMERBASE_INCLUDE_DIRS} ${GSTREAMERVIDEO_INCLUDE_DIRS})
include_directories(${CMAKE_CURRENT_SOURCE_DIR} subtitle playerisobmff isobmff
	closedcaptions
	closedcaptions/subtec
	drm drm/helper
	drm/ocdm
	externals/playersecmanager)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/externals)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/externals/playersecmanager)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/externals/rdk)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/baseConversion)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/playerLogManager)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/mp4demux)


set(LIBPLAYERGSTINTERFACE_DEPENDS ${OS_LD_FLAGS} ${UUID_LINK_LIBRARIES} ${LIBCJSON_LINK_LIBRARIES} ${GSTREAMERBASE_LINK_LIBRARIES} ${GSTREAMER_LINK_LIBRARIES} ${CURL_LINK_LIBRARIES} ${LIBDASH_LINK_LIBRARIES} ${LibXml2_LIBRARIES} ${CMAKE_THREAD_LIBS_INIT} ${OPENSSL_LIBRARIES} ${OPENGL_LIBRARIES} ${GLEW_LIBRARIES} -ldl)

#shoud be made part of PLAYERGSTINTERFACE cmakelists
add_subdirectory(baseConversion)
add_subdirectory(playerLogManager)
add_subdirectory(externals)

set(LIBPLAYERGSTINTERFACE_SOURCES ${LIBPLAYERGSTINTERFACE_SOURCES} closedcaptions/PlayerCCManager.cpp PlayerUtils.cpp)

set(SUBTEC_CLASS_SOURCES playerisobmff/playerisobmffbox.cpp
	playerisobmff/playerisobmffbuffer.cpp
	subtec/subtecparser/WebVttSubtecParser.cpp
	subtec/subtecparser/TtmlSubtecParser.cpp
	subtec/subtecparser/TextStyleAttributes.cpp
	subtec/subtecparser/WebvttSubtecDevInterface.cpp
	playerjsonobject/PlayerJsonObject.cpp
	)

if(CMAKE_PLATFORM_UBUNTU)
    message("CMAKE_PLATFORM_UBUNTU set")
    link_directories(${CMAKE_LIBRARY_PATH})
endif()

if(CMAKE_SOC_PLATFORM_RPI)
    message("CMAKE_SOC_PLATFORM_RPI set")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DRPI=1")
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
    set(OS_CXX_FLAGS "${OS_CXX_FLAGS}  -g -x objective-c++ -Wno-inconsistent-missing-override -F${osxSdkPath}/System/Library/Frameworks")
    set(OS_LD_FLAGS "${OS_LD_FLAGS} -F${osxSdkPath}/System/Library/Frameworks -framework Cocoa -L${osxSdkPath}/../MacOSX.sdk/usr/lib -L.libs/lib -L/usr/local/lib/")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -isysroot ${osxSdkPath}/../MacOSX.sdk -I/usr/local/include")
    string(STRIP ${OS_LD_FLAGS} OS_LD_FLAGS)
    pkg_check_modules(OPENSSL REQUIRED openssl)
    set(LIBPLAYERGSTINTERFACE_DEPENDS ${LIBPLAYERGSTINTERFACE_DEPENDS} "${OPENSSL_LINK_LIBRARIES}")
    link_directories(${OPENSSL_LIBRARY_DIRS})
    link_directories(${GSTREAMER_LIBRARY_DIRS})
    set(CMAKE_THREAD_LIBS_INIT "-lpthread")
    set(CMAKE_HAVE_THREADS_LIBRARY 1)
    pkg_check_modules(GLIB REQUIRED GLib-2.0)
    include_directories(${GLIB_INCLUDE_DIRS})
    pkg_check_modules(LIBCJSON REQUIRED libcjson)
    include_directories(${LIBCJSON_INCLUDE_DIRS})
    set(LIBPLAYERGSTINTERFACE_DEPENDS ${LIBPLAYERGSTINTERFACE_DEPENDS} "${LIBCJSON_LINK_LIBRARIES}")
    # This is .libs/lib/pkgconfig which picks up all locally built dependencies
    link_directories(${LIBCJSON_LIBRARY_DIRS})
    #link_directories(".libs/lib")

    # XCode build flags. Even when using CLANG, the GCC name is required to enable the check
    set(CMAKE_XCODE_ATTRIBUTE_GCC_WARN_UNUSED_FUNCTION "YES")
    set(CMAKE_XCODE_ATTRIBUTE_GCC_WARN_UNUSED_VARIABLE "YES")
else()
    set(USE_MAC_FOR_RANDOM_GEN "-DUSE_MAC_FOR_RANDOM_GEN")
endif(CMAKE_SYSTEM_NAME STREQUAL Darwin)

set(LIBPLAYERGSTINTERFACE_HEADERS
	closedcaptions/CCTrackInfo.h
	GstUtils.h
	PlayerUtils.h
	GstUtils.h
	PlayerMetadata.hpp
	closedcaptions/PlayerCCManager.h
	PlayerScheduler.h
	gstplayertaskpool.h
	GstHandlerControl.h
	InterfacePlayerRDK.h
	drm/DrmUtils.h
	drm/aes/Aes.h
	drm/HlsDrmBase.h
	drm/DrmSystems.h
	drm/DrmCallbacks.h
	drm/ocdm/opencdmsessionadapter.h
	drm/DrmJsonObject.h
	vendor/SocInterface.h
	vendor/amlogic/AmlogicSocInterface.h
	vendor/realtek/RealtekSocInterface.h
	vendor/brcm/BrcmSocInterface.h
	vendor/default/DefaultSocInterface.h
    subtitle/vttCue.h
	ProcessHandler.h
	)

install(FILES closedcaptions/CCTrackInfo.h
	PlayerScheduler.h
	gstplayertaskpool.h
	GstHandlerControl.h
	InterfacePlayerRDK.h
	SocUtils.h
	GstUtils.h
	PlayerMetadata.hpp
	drm/DrmUtils.h
	closedcaptions/PlayerCCManager.h
	PlayerUtils.h
	drm/ocdm/opencdmsessionadapter.h
	drm/aes/Aes.h
	drm/DrmMemorySystem.h drm/DrmSessionManager.h drm/DrmSystems.h
	drm/DrmData.h drm/DrmInfo.h drm/DrmMediaFormat.h drm/DrmCallbacks.h
	drm/DrmSession.h drm/ClearKeyDrmSession.h drm/DrmSessionFactory.h drm/ocdm/opencdmsessionadapter.h
	drm/helper/DrmHelper.h drm/HlsDrmBase.h
	drm/DrmConstants.h
	drm/PlayerHlsDrmSessionInterface.h
	drm/PlayerHlsDrmSessionInterfaceBase.h
	drm/helper/VanillaDrmHelper.h
	drm/HlsOcdmBridgeInterface.h
	drm/HlsDrmSessionManager.h
	vendor/SocInterface.h
	subtitle/subtitleParser.h
	subtec/subtecparser/WebVttSubtecParser.hpp
	subtec/subtecparser/TtmlSubtecParser.hpp
	subtec/subtecparser/WebvttSubtecDevInterface.hpp
	subtec/subtecparser/TextStyleAttributes.h
	subtec/libsubtec/SubtecPacket.hpp
	playerisobmff/playerisobmffbuffer.h
	playerisobmff/playerisobmffbox.h
	DESTINATION include)

set(SOURCES
	mp4demux/mp4demux.cpp
	InterfacePlayerRDK.cpp
	SocUtils.cpp
	GstUtils.cpp
	GstHandlerControl.cpp
	PlayerScheduler.cpp
	gstplayertaskpool.cpp
	PlayerUtils.cpp
	vendor/SocInterface.cpp
	vendor/amlogic/AmlogicSocInterface.cpp
	vendor/realtek/RealtekSocInterface.cpp
	vendor/brcm/BrcmSocInterface.cpp
	vendor/default/DefaultSocInterface.cpp
	drm/processProtectionHls.cpp
	ProcessHandler.cpp
	)
set(LIBPLAYERGSTINTERFACE_DRM_SOURCES drm/PlayerHlsDrmSessionInterface.cpp
	drm/DrmSessionManager.cpp
	drm/DrmSession.cpp
	drm/DrmSessionFactory.cpp
	drm/helper/DrmHelper.cpp
	drm/helper/DrmHelperFactory.cpp
	drm/HlsOcdmBridgeInterface.cpp
	drm/DrmUtils.cpp
	drm/DrmSystems.h
	drm/aes/Aes.cpp
	drm/DrmJsonObject.cpp
	)

if (CMAKE_PLATFORM_UBUNTU OR CMAKE_SYSTEM_NAME STREQUAL Darwin )
	# uncomment below to build additional drm support in simulator
	# set(CMAKE_USE_OPENCDM_ADAPTER TRUE)
	# set(CMAKE_USE_OPENCDM_ADAPTER_MOCKS TRUE)
	# set(CMAKE_USE_THUNDER_OCDM_API_0_2 TRUE)
	# set(CMAKE_USE_SECCLIENT TRUE)
	# set(CMAKE_USE_SECCLIENT_MOCKS TRUE)
  endif()

set(LIBPLAYERGSTINTERFACE_SOURCES ${LIBPLAYERGSTINTERFACE_SOURCES} ${SOURCES}  ${SUBTEC_CLASS_SOURCES})

include_directories(${GSTVIDEO_INCLUDE_DIRS})

if(CMAKE_USE_THUNDER_OCDM_API_0_2)
	set(LIBPLAYERGSTINTERFACE_DEFINES "${LIBPLAYERGSTINTERFACE_DEFINES} -DUSE_THUNDER_OCDM_API_0_2")
endif()

if(CMAKE_USE_OPENCDM_ADAPTER)
    message("Using OPEN CDM support enabled")
    set(LIBPLAYERGSTINTERFACE_DEFINES "${LIBPLAYERGSTINTERFACE_DEFINES} -DUSE_OPENCDM_ADAPTER")

    set(LIBPLAYERGSTINTERFACE_DRM_SOURCES "${LIBPLAYERGSTINTERFACE_DRM_SOURCES}" drm/HlsDrmSessionManager.cpp
                                                     drm/HlsOcdmBridge.cpp
                                                     drm/processProtectionHls.cpp
    )

    # DRM Helpers
    if(CMAKE_USE_WIDEVINE)
        set(LIBPLAYERGSTINTERFACE_HELP_SOURCES "${LIBPLAYERGSTINTERFACE_HELP_SOURCES}" drm/helper/WidevineDrmHelper.cpp)
    endif()

    if(CMAKE_USE_CLEARKEY)
	    set(LIBPLAYERGSTINTERFACE_HELP_SOURCES "${LIBPLAYERGSTINTERFACE_HELP_SOURCES}" drm/helper/ClearKeyHelper.cpp)
    endif()

    if(CMAKE_USE_PLAYREADY)
        set(LIBPLAYERGSTINTERFACE_HELP_SOURCES "${LIBPLAYERGSTINTERFACE_HELP_SOURCES}" drm/helper/PlayReadyHelper.cpp)
    endif()

    if(CMAKE_USE_VERIMATRIX)
        message("CMAKE_USE_VERIMATRIX set")
        set(LIBPLAYERGSTINTERFACE_HELP_SOURCES "${LIBPLAYERGSTINTERFACE_HELP_SOURCES}" drm/helper/VerimatrixHelper.cpp)
    endif()
else()
    message("No OpenCDM support enabled")
endif()

if(CMAKE_USE_CLEARKEY)
    set(LIBPLAYERGSTINTERFACE_DRM_SOURCES "${LIBPLAYERGSTINTERFACE_DRM_SOURCES}" drm/ClearKeyDrmSession.cpp)
    set(LIBPLAYERGSTINTERFACE_HELP_SOURCES "${LIBPLAYERGSTINTERFACE_HELP_SOURCES}" drm/helper/ClearKeyHelper.cpp)
    set(LIBPLAYERGSTINTERFACE_DEFINES "${LIBPLAYERGSTINTERFACE_DEFINES} -DUSE_CLEARKEY")
endif()

if(CMAKE_USE_OPENCDM_ADAPTER)
    message("Using OPEN CDM ADAPTER")
    # Include OpenCDM-related source files
    set(LIBPLAYERGSTINTERFACE_SOURCES ${LIBPLAYERGSTINTERFACE_SOURCES} 
        drm/ocdm/opencdmsessionadapter.cpp
        drm/ocdm/OcdmBasicSessionAdapter.cpp
        drm/ocdm/OcdmGstSessionAdapter.cpp
    )

    # Add GStreamer video dependency
    set(LIBPLAYERGSTINTERFACE_DEPENDS "${LIBPLAYERGSTINTERFACE_DEPENDS} -lgstvideo-1.0")

    if(CMAKE_USE_OPENCDM_ADAPTER_MOCKS)
        # Add mock headers and sources if mock is enabled
        set(LIBPLAYERGSTINTERFACE_HEADERS ${LIBPLAYERGSTINTERFACE_HEADERS} open_cdm.h open_cdm_adapter.h)
        set(LIBPLAYERGSTINTERFACE_MOCK_SOURCES ${LIBPLAYERGSTINTERFACE_MOCK_SOURCES} test/mocks/opencdmMocks.cpp)
        set(LIBPLAYERGSTINTERFACE_MOCK_DEPENDS -lgmock -lgtest)
    else()
        # Link with actual OpenCDM library
	set(LIBPLAYERGSTINTERFACE_DEPENDS ${LIBPLAYERGSTINTERFACE_DEPENDS} "-locdm")
    endif()

    # Find OpenCDM headers and include them
    find_path(STAGING_INCDIR opencdm)
    include_directories(${STAGING_INCDIR}/opencdm)

    # Find GStreamer headers and include them
    find_path(STAGING_INCDIR gstreamer-1.0)
    include_directories(${STAGING_INCDIR}/gstreamer-1.0)
endif()
set(LIBPLAYERGSTINTERFACE_SOURCES ${LIBPLAYERGSTINTERFACE_SOURCES}  ${LIBPLAYERGSTINTERFACE_DRM_SOURCES} ${LIBPLAYERGSTINTERFACE_HELP_SOURCES} )

add_library(subtec SHARED subtec/libsubtec/PacketSender.cpp subtec/libsubtec/SubtecChannel.cpp)
set(SUBTEC_PUBLIC_HEADERS subtec/libsubtec/SubtecChannel.hpp subtec/libsubtec/SubtecAttribute.hpp)
set_target_properties(subtec PROPERTIES PUBLIC_HEADER "${SUBTEC_PUBLIC_HEADERS}")
set(SUBTEC_COMPILE_FLAGS "${CMAKE_CXX_FLAGS} -DSUBTEC_PACKET_DEBUG=1")
if(CMAKE_PLATFORM_UBUNTU)
	message("CMAKE_PLATFORM_UBUNTU set")
	set(LIBPLAYERGSTINTERFACE_DEFINES "${LIBPLAYERGSTINTERFACE_DEFINES} -DUBUNTU=1 -DNO_NATIVE_AV=1")
	set(SUBTEC_COMPILE_FLAGS "${SUBTEC_COMPILE_FLAGS} -DUBUNTU=1")
endif()

set_target_properties(subtec PROPERTIES COMPILE_FLAGS "${SUBTEC_COMPILE_FLAGS}")

target_include_directories(subtec PUBLIC
	${CMAKE_CURRENT_SOURCE_DIR}
	${CMAKE_CURRENT_SOURCE_DIR}/playerjsonobject
	${CMAKE_CURRENT_SOURCE_DIR}/subtec/subtecparser
	${CMAKE_CURRENT_SOURCE_DIR}/subtec/libsubtec
	${CMAKE_CURRENT_SOURCE_DIR}/subtitle)

install (TARGETS subtec
	DESTINATION lib
	PUBLIC_HEADER DESTINATION include
	)

if (CMAKE_GST_SUBTEC_ENABLED)
	set(CMAKE_SUBTITLE_SUPPORT TRUE)
	message("CMAKE_GST_SUBTEC_ENABLED set")
	set(LIBPLAYERGSTINTERFACE_DEFINES "${LIBPLAYERGSTINTERFACE_DEFINES} -DGST_SUBTEC_ENABLED")
endif()

if (CMAKE_SUBTITLE_SUPPORT)
	message("CMAKE_SUBTITLE_SUPPORT set")
	set(LIBPLAYERGSTINTERFACE_DEFINES "${LIBPLAYERGSTINTERFACE_DEFINES} -DSUBTITLE_SUPPORTED")
	find_path(STAGING_INCDIR closedcaption/ccDataReader.h)
	include_directories(${STAGING_INCDIR}/closedcaption)
	if (CMAKE_USE_CC_MANAGER_MOCKS)
		set(LIBPLAYERGSTINTERFACE_MOCK_SOURCES ${LIBPLAYERGSTINTERFACE_MOCK_SOURCES} ${CMAKE_CURRENT_SOURCE_DIR}/../test/fakes/ccManagerFakes.cpp)
	else()
		set(LIBSUBTECCONNECTOR_DEPENDS pthread rdkCCReader subtec)
	endif()

	set(LIBSUBTECCONNECTOR_SOURCES
		closedcaptions/subtec/SubtecConnector.cpp
		closedcaptions/subtec/CCDataController.cpp)

	add_library(subtec_connector SHARED ${LIBSUBTECCONNECTOR_SOURCES})
	target_link_libraries(subtec_connector ${LIBSUBTECCONNECTOR_DEPENDS})
	target_link_libraries(subtec_connector playerlogmanager)
	install(TARGETS subtec_connector DESTINATION lib)

	set(LIBPLAYERGSTINTERFACE_SOURCES ${LIBPLAYERGSTINTERFACE_SOURCES} closedcaptions/subtec/PlayerSubtecCCManager.cpp)
endif()
add_library(playergstinterface SHARED ${SOURCES} ${LIBPLAYERGSTINTERFACE_HEADERS} ${LIBPLAYERGSTINTERFACE_SOURCES} ${LIBPLAYERGSTINTERFACE_DRM_SOURCES} ${LIBPLAYERGSTINTERFACE_HELP_SOURCES})

target_include_directories(playergstinterface PUBLIC
	${CMAKE_CURRENT_SOURCE_DIR}
	${CMAKE_CURRENT_SOURCE_DIR}/externals
	${CMAKE_CURRENT_SOURCE_DIR}/subtec/subtecparser
	${CMAKE_CURRENT_SOURCE_DIR}/subtec/libsubtec
	${CMAKE_CURRENT_SOURCE_DIR}/subtitle
	${CMAKE_CURRENT_SOURCE_DIR}/playerisobmff
	${CMAKE_CURRENT_SOURCE_DIR}/playerjsonobject
	${CMAKE_CURRENT_SOURCE_DIR}/closedcaptions
	${CMAKE_CURRENT_SOURCE_DIR}/closedcaptions/subtec
	${CMAKE_CURRENT_SOURCE_DIR}/vendor)

if (CMAKE_SUBTITLE_SUPPORT)
	target_link_libraries(playergstinterface subtec_connector)
endif()

string(STRIP "${LIBPLAYERGSTINTERFACE_DEPENDS}" LIBPLAYERGSTINTERFACE_DEPENDS)
message("Building middleware support libraries")
add_subdirectory(gst-plugins)

target_link_libraries(playergstinterface ${LIBPLAYERGSTINTERFACE_DEPENDS})

target_link_libraries(playergstinterface ${GST_LIBRARIES} ${GSTREAMER_LIBRARIES} ${GSTREAMERBASE_LIBRARIES} ${GSTREAMERBASE_LINK_LIBRARIES})

target_link_libraries(playergstinterface ${GSTVIDEO_LIBRARIES} ${LIBPLAYERGSTINTERFACE_DEPENDS})

set_target_properties(playergstinterface PROPERTIES COMPILE_FLAGS "${LIBPLAYERGSTINTERFACE_DEFINES} ${OS_CXX_FLAGS}")

install(TARGETS playergstinterface
    LIBRARY DESTINATION lib
    PUBLIC_HEADER DESTINATION lib/include
)
target_link_libraries(subtec playerlogmanager)
target_link_libraries(playergstinterface ${GSTVIDEO_LIBRARIES} ${LIBPLAYERGSTINTERFACE_DEPENDS})
target_link_libraries(playergstinterface subtec)
target_link_libraries(playergstinterface baseconversion)
target_link_libraries(playergstinterface playerfbinterface)
set(LIBPLAYERGSTINTERFACE_SOURCES ${LIBPLAYERGSTINTERFACE_SOURCES} {LIBPLAYERGSTINTERFACE_MOCK_SOURCES})
