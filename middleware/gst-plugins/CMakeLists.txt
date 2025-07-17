##########################################################################
# Copyright 2025 RDK Management
#
# This library is free software; you can redistribute it and/or
# modify it under the terms of the GNU Lesser General Public
# License as published by the Free Software Foundation, version 2.1
# of the license.
#
# This library is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
# Lesser General Public License for more details.
#
# You should have received a copy of the GNU Lesser General Public
# License along with this library; if not, write to the
# Free Software Foundation, Inc., 51 Franklin St, Fifth Floor,
# Boston, MA 02110-1301, USA.
#########################################################################

cmake_minimum_required (VERSION 2.6)
project (GST-PLUGINS)

set(MW_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../")

#find_package(GStreamer 1.4 REQUIRED)
#add_subdirectory(jsbindings)
find_package(PkgConfig REQUIRED)

message("using gstreamer-1.0")
pkg_check_modules(GSTREAMER REQUIRED gstreamer-1.0)
pkg_check_modules(GSTREAMERBASE REQUIRED gstreamer-app-1.0)

pkg_check_modules(CURL REQUIRED libcurl)

# Mac OS X
if(CMAKE_SYSTEM_NAME STREQUAL Darwin)
    set(OS_CXX_FLAGS "${OS_CXX_FLAGS} -g -x objective-c++")
    set(OS_LD_FLAGS "${OS_LD_FLAGS} -framework Cocoa")
    string(STRIP ${OS_LD_FLAGS} OS_LD_FLAGS)
    set(CLI_LD_FLAGS "${CLI_LD_FLAGS} -lgstvideo-1.0")
    string(STRIP ${CLI_LD_FLAGS} CLI_LD_FLAGS)
    #set(AAMP_OS_SOURCES cocoa_window.mm) #to be decided
endif(CMAKE_SYSTEM_NAME STREQUAL Darwin)

find_package (Threads REQUIRED)

include_directories(${CMAKE_CURRENT_SOURCE_DIR})
include_directories(${GSTREAMER_INCLUDE_DIRS})
include_directories(${CURL_INCLUDE_DIRS})
include_directories(${GSTREAMERBASE_INCLUDE_DIRS})
include_directories(${MW_ROOT}/closedcaptions)
include_directories(${MW_ROOT}/)

set(GST_COMMON_DEPENDENCIES ${OS_LD_FLAGS} ${GSTREAMERBASE_LIBRARIES} ${GSTREAMER_LIBRARIES} ${CURL_LIBRARIES} ${CLI_LD_FLAGS} -ldl -luuid ${SEC_CLIENT_LIB})

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-multichar -std=c++11")

if(CMAKE_IARM_MGR)
	message("CMAKE_IARM_MGR set")
	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DIARM_MGR")
endif()

set(GST_MIDDLEWARE_SOURCES gstinit.cpp
	${MW_ROOT}/playerLogManager/PlayerLogManager.cpp)

if(CMAKE_CDM_DRM)
        message("CMAKE_CDM_DRM set")
	set(GST_MIDDLEWARE_SOURCES "${GST_MIDDLEWARE_SOURCES}" drm/gst/gstcdmidecryptor.cpp drm/gst/gstplayreadydecryptor.cpp drm/gst/gstwidevinedecryptor.cpp drm/gst/gstclearkeydecryptor.cpp drm/gst/gstverimatrixdecryptor.cpp)
endif()

message("AAMP consolidated build, enabling gst_subtec")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DGST_SUBTEC_ENABLED")
add_subdirectory(gst_subtec)

add_library(gstplugin SHARED ${GST_MIDDLEWARE_SOURCES})

if(CMAKE_CDM_DRM)
	target_include_directories (gstplugin PRIVATE drm/gst)
	if(CMAKE_USE_OPENCDM_ADAPTER)
		message("CMAKE_USE_OPENCDM_ADAPTER set")
		set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DUSE_OPENCDM_ADAPTER")
		set(GST_COMMON_DEPENDENCIES "${GST_COMMON_DEPENDENCIES} -locdm")
		find_path (STAGING_INCDIR opencdm)
		include_directories(${STAGING_INCDIR}/opencdm)
	else() 
		message("CMAKE_USE_OPENCDM_ADAPTER not set")
	endif()
	set(PLUGIN_DEFINES "${PLUGIN_DEFINES} -DDRM_BUILD_PROFILE=DRM_BUILD_PROFILE_OEM -DTARGET_LITTLE_ENDIAN=1 -DTARGET_SUPPORTS_UNALIGNED_DWORD_POINTERS=0 ${SEC_CONTENT_METADATA_ENABLED}")
else()
    message("CMAKE_CDM_DRM not set")
endif()

target_link_libraries (gstplugin playergstinterface ${GST_COMMON_DEPENDENCIES})
target_include_directories (gstplugin PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../../ ${CMAKE_CURRENT_SOURCE_DIR}/../../drm ${CMAKE_CURRENT_SOURCE_DIR}/../../drm/helper ${CMAKE_CURRENT_SOURCE_DIR}/../../tsb/api ${CMAKE_CURRENT_SOURCE_DIR}/../../downloader ${CMAKE_CURRENT_SOURCE_DIR}/../../middleware ${CMAKE_CURRENT_SOURCE_DIR}/../../middleware/gst-plugins ${CMAKE_CURRENT_SOURCE_DIR}/../../middleware/vendor ${CMAKE_CURRENT_SOURCE_DIR}/../../middleware/drm ${CMAKE_CURRENT_SOURCE_DIR}/../../middleware/drm/ocdm ${CMAKE_CURRENT_SOURCE_DIR}/../../middleware/drm/helper  ${CMAKE_CURRENT_SOURCE_DIR}/../../middleware/externals/contentsecuritymanager)

set(LIBPLUGIN_DEFINES "${PLUGIN_DEFINES}")

if (CMAKE_AMLOGIC_SOC)
        message("CMAKE_AMLOGIC_SOC set")
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DAMLOGIC")
	target_link_libraries (gstplugin gstsvpext)
endif()

#TODO Remove once PrivateInstance_Player is compilation flag independent.
#if(CMAKE_USE_SECCLIENT)
#	message("CMAKE_USE_SECCLIENT set")
#	set(LIBPLUGIN_DEFINES "${LIBPLUGIN_DEFINES} -DUSE_SECCLIENT")
#elseif(CMAKE_USE_SECMANAGER)
#	message("CMAKE_USE_SECMANAGER set")
#	set(LIBPLUGIN_DEFINES "${LIBPLUGIN_DEFINES} -DUSE_SECMANAGER")
#endif()

install(TARGETS gstplugin DESTINATION lib/gstreamer-1.0)

if(CMAKE_WPEWEBKIT_JSBINDINGS)
	message("CMAKE_WPEWEBKIT_JSBINDINGS set")
	#target_link_libraries (gstplugin aampjsbindings)
	#set(LIBPLUGIN_DEFINES "${LIBPLUGIN_DEFINES} -DAAMP_JSCONTROLLER_ENABLED")
endif()

set_target_properties(gstplugin PROPERTIES COMPILE_FLAGS "${LIBPLUGIN_DEFINES}")
