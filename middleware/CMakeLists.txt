
# Middleware CMakeLists.txt

cmake_minimum_required(VERSION 3.5)
project(Middleware)
find_package(PkgConfig REQUIRED)
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

set(LIBMIDDLEWARE_DEPENDS ${OS_LD_FLAGS} ${UUID_LINK_LIBRARIES} ${LIBCJSON_LINK_LIBRARIES} ${GSTREAMERBASE_LINK_LIBRARIES} ${GSTREAMER_LINK_LIBRARIES} ${CURL_LINK_LIBRARIES} ${LibXml2_LIBRARIES} ${CMAKE_THREAD_LIBS_INIT} ${OPENSSL_LIBRARIES} ${OPENGL_LIBRARIES} ${GLEW_LIBRARIES} -ldl)

#shoud be made part of middleware cmakelists
add_subdirectory(baseConversion)
add_subdirectory(playerLogManager)
add_subdirectory(externals)

set(LIBMIDDLEWARE_SOURCES ${LIBMIDDLEWARE_SOURCES} closedcaptions/PlayerCCManager.cpp PlayerUtils.cpp)

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
    set(OS_CXX_FLAGS "${OS_CXX_FLAGS}  -std=c++14 -g -x objective-c++ -Wno-inconsistent-missing-override -F${osxSdkPath}/System/Library/Frameworks")
    set(OS_LD_FLAGS "${OS_LD_FLAGS} -F${osxSdkPath}/System/Library/Frameworks -framework Cocoa -L${osxSdkPath}/../MacOSX.sdk/usr/lib -L.libs/lib -L/usr/local/lib/")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -isysroot ${osxSdkPath}/../MacOSX.sdk -I/usr/local/include")
    string(STRIP ${OS_LD_FLAGS} OS_LD_FLAGS)
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

set(LIBMIDDLEWARE_HEADERS
	closedcaptions/CCTrackInfo.h
	GstUtils.h
	PlayerUtils.h
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
	)

install(FILES closedcaptions/CCTrackInfo.h
	PlayerScheduler.h
	gstplayertaskpool.h
	GstHandlerControl.h
	InterfacePlayerRDK.h
	SocUtils.h
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
	subtitle/subtitleParser.h
	subtec/subtecparser/WebVttSubtecParser.hpp
	subtec/subtecparser/TtmlSubtecParser.hpp
	playerisobmff/playerisobmffbuffer.h
	playerisobmff/playerisobmffbox.h
	DESTINATION include)

set(SOURCES
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
	)
set(LIBMIDDLEWARE_DRM_SOURCES drm/PlayerHlsDrmSessionInterface.cpp
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

set(LIBMIDDLEWARE_SOURCES ${LIBMIDDLEWARE_SOURCES} ${SOURCES}  ${SUBTEC_CLASS_SOURCES})

include_directories(${GSTVIDEO_INCLUDE_DIRS})

if(CMAKE_USE_THUNDER_OCDM_API_0_2)
	set(LIBMIDDLEWARE_DEFINES "${LIBMIDDLEWARE_DEFINES} -DUSE_THUNDER_OCDM_API_0_2")
endif()

if(CMAKE_USE_OPENCDM_ADAPTER)
    message("Using OPEN CDM support enabled")
    set(LIBMIDDLEWARE_DEFINES "${LIBMIDDLEWARE_DEFINES} -DUSE_OPENCDM_ADAPTER")

    set(LIBMIDDLEWARE_DRM_SOURCES "${LIBMIDDLEWARE_DRM_SOURCES}" drm/HlsDrmSessionManager.cpp
                                                     drm/HlsOcdmBridge.cpp
                                                     drm/processProtectionHls.cpp
    )

    # DRM Helpers
    if(CMAKE_USE_WIDEVINE)
        set(LIBMIDDLEWARE_HELP_SOURCES "${LIBMIDDLEWARE_HELP_SOURCES}" drm/helper/WidevineDrmHelper.cpp)
    endif()

    if(CMAKE_USE_CLEARKEY)
	    set(LIBMIDDLEWARE_HELP_SOURCES "${LIBMIDDLEWARE_HELP_SOURCES}" drm/helper/ClearKeyHelper.cpp)
    endif()

    if(CMAKE_USE_PLAYREADY)
        set(LIBMIDDLEWARE_HELP_SOURCES "${LIBMIDDLEWARE_HELP_SOURCES}" drm/helper/PlayReadyHelper.cpp)
    endif()

    if(CMAKE_USE_VERIMATRIX)
        message("CMAKE_USE_VERIMATRIX set")
        set(LIBMIDDLEWARE_HELP_SOURCES "${LIBMIDDLEWARE_HELP_SOURCES}" drm/helper/VerimatrixHelper.cpp)
    endif()
else()
    message("No OpenCDM support enabled")
endif()

if(CMAKE_USE_CLEARKEY)
    set(LIBMIDDLEWARE_DRM_SOURCES "${LIBMIDDLEWARE_DRM_SOURCES}" drm/ClearKeyDrmSession.cpp)
    set(LIBMIDDLEWARE_HELP_SOURCES "${LIBMIDDLEWARE_HELP_SOURCES}" drm/helper/ClearKeyHelper.cpp)
    set(LIBMIDDLEWARE_DEFINES "${LIBMIDDLEWARE_DEFINES} -DUSE_CLEARKEY")
endif()

if(CMAKE_USE_OPENCDM_ADAPTER)
    message("Using OPEN CDM ADAPTER")
    # Include OpenCDM-related source files
    set(LIBMIDDLEWARE_SOURCES ${LIBMIDDLEWARE_SOURCES}
        drm/ocdm/opencdmsessionadapter.cpp
        drm/ocdm/OcdmBasicSessionAdapter.cpp
        drm/ocdm/OcdmGstSessionAdapter.cpp
    )

    # Add GStreamer video dependency
    set(LIBMIDDLEWARE_DEPENDS "${LIBMIDDLEWARE_DEPENDS} -lgstvideo-1.0")

    if(CMAKE_USE_OPENCDM_ADAPTER_MOCKS)
        # Add mock headers and sources if mock is enabled
        set(LIBMIDDLEWARE_HEADERS ${LIBMIDDLEWARE_HEADERS} open_cdm.h open_cdm_adapter.h)
        set(LIBMIDDLEWARE_MOCK_SOURCES ${LIBMIDDLEWARE_MOCK_SOURCES} test/mocks/opencdmMocks.cpp)
        set(LIBMIDDLEWARE_MOCK_DEPENDS -lgmock -lgtest)
    else()
        # Link with actual OpenCDM library
	set(LIBMIDDLEWARE_DEPENDS ${LIBMIDDLEWARE_DEPENDS} "-locdm")
    endif()

    # Find OpenCDM headers and include them
    find_path(STAGING_INCDIR opencdm)
    include_directories(${STAGING_INCDIR}/opencdm)

    # Find GStreamer headers and include them
    find_path(STAGING_INCDIR gstreamer-1.0)
    include_directories(${STAGING_INCDIR}/gstreamer-1.0)
endif()
set(LIBMIDDLEWARE_SOURCES ${LIBMIDDLEWARE_SOURCES}  ${LIBMIDDLEWARE_DRM_SOURCES} ${LIBMIDDLEWARE_HELP_SOURCES} )

add_library(subtec SHARED subtec/libsubtec/PacketSender.cpp subtec/libsubtec/SubtecChannel.cpp)
set(SUBTEC_PUBLIC_HEADERS subtec/libsubtec/SubtecChannel.hpp subtec/libsubtec/SubtecAttribute.hpp)
set_target_properties(subtec PROPERTIES PUBLIC_HEADER "${SUBTEC_PUBLIC_HEADERS}")
set(SUBTEC_COMPILE_FLAGS "${CMAKE_CXX_FLAGS} -DSUBTEC_PACKET_DEBUG=1")
if(CMAKE_PLATFORM_UBUNTU)
	message("CMAKE_PLATFORM_UBUNTU set")
	set(LIBMIDDLEWARE_DEFINES "${LIBMIDDLEWARE_DEFINES} -DUBUNTU=1 -DNO_NATIVE_AV=1")
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
	set(LIBMIDDLEWARE_DEFINES "${LIBMIDDLEWARE_DEFINES} -DGST_SUBTEC_ENABLED")
endif()

if (CMAKE_SUBTITLE_SUPPORT)
	message("CMAKE_SUBTITLE_SUPPORT set")
	set(LIBMIDDLEWARE_DEFINES "${LIBMIDDLEWARE_DEFINES} -DSUBTITLE_SUPPORTED")
	find_path(STAGING_INCDIR closedcaption/ccDataReader.h)
	include_directories(${STAGING_INCDIR}/closedcaption)
	if (CMAKE_USE_CC_MANAGER_MOCKS)
		set(LIBMIDDLEWARE_MOCK_SOURCES ${LIBMIDDLEWARE_MOCK_SOURCES} ${CMAKE_CURRENT_SOURCE_DIR}/../test/fakes/ccManagerFakes.cpp)
	else()
		set(LIBSUBTECCONNECTOR_DEPENDS pthread rdkCCReader subtec)
	endif()

	set(LIBSUBTECCONNECTOR_SOURCES
		closedcaptions/subtec/SubtecConnector.cpp
		closedcaptions/subtec/CCDataController.cpp)

	add_library(subtec_connector SHARED ${LIBSUBTECCONNECTOR_SOURCES})
	target_link_libraries(subtec_connector ${LIBSUBTECCONNECTOR_DEPENDS})
	target_link_libraries(subtec_connector PlayerLogManager)
	install(TARGETS subtec_connector DESTINATION lib)

	set(LIBMIDDLEWARE_SOURCES ${LIBMIDDLEWARE_SOURCES} closedcaptions/subtec/PlayerSubtecCCManager.cpp)
endif()
add_library(middleware SHARED ${SOURCES} ${LIBMIDDLEWARE_HEADERS} ${LIBMIDDLEWARE_SOURCES} ${LIBMIDDLEWARE_DRM_SOURCES} ${LIBMIDDLEWARE_HELP_SOURCES})

target_include_directories(middleware PUBLIC
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
	target_link_libraries(middleware subtec_connector)
endif()

if (CMAKE_INBUILT_AAMP_DEPENDENCIES)
	message("Building middleware support libraries")
	add_subdirectory(gst-plugins)
endif()

target_link_libraries(middleware ${LIBMIDDLEWARE_DEPENDS})

target_link_libraries(middleware ${GST_LIBRARIES} ${GSTREAMER_LIBRARIES} ${GSTREAMERBASE_LIBRARIES} ${GSTREAMERBASE_LINK_LIBRARIES})

target_link_libraries(middleware ${GSTVIDEO_LIBRARIES} ${LIBMIDDLEWARE_DEPENDS})

set_target_properties(middleware PROPERTIES COMPILE_FLAGS "${LIBMIDDLEWARE_DEFINES} ${OS_CXX_FLAGS}")

install(TARGETS middleware
    LIBRARY DESTINATION lib
    PUBLIC_HEADER DESTINATION lib/include
)
target_link_libraries(subtec PlayerLogManager)
target_link_libraries(middleware ${GSTVIDEO_LIBRARIES} ${LIBMIDDLEWARE_DEPENDS})
target_link_libraries(middleware subtec)
target_link_libraries(middleware BaseConversion)
target_link_libraries(middleware playerfbinterface)
set(LIBMIDDLEWARE_SOURCES ${LIBMIDDLEWARE_SOURCES} {LIBMIDDLEWARE_MOCK_SOURCES})
