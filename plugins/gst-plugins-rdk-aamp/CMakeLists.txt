##########################################################################
# Copyright 2018 RDK Management
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
project (AAMP)
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
    set(AAMP_CLI_LD_FLAGS "${AAMP_CLI_LD_FLAGS} -lgstvideo-1.0")
    string(STRIP ${AAMP_CLI_LD_FLAGS} AAMP_CLI_LD_FLAGS)
    set(AAMP_OS_SOURCES cocoa_window.mm)
endif(CMAKE_SYSTEM_NAME STREQUAL Darwin)

find_package (Threads REQUIRED)

include_directories(${CMAKE_CURRENT_SOURCE_DIR})
include_directories(${GSTREAMER_INCLUDE_DIRS})
include_directories(${CURL_INCLUDE_DIRS})
include_directories(${GSTREAMERBASE_INCLUDE_DIRS})
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/../../middleware/closedcaptions)

set(AAMP_COMMON_DEPENDENCIES ${OS_LD_FLAGS} ${GSTREAMERBASE_LIBRARIES} ${GSTREAMER_LIBRARIES} ${CURL_LIBRARIES}  ${AAMP_CLI_LD_FLAGS} -ldl -luuid ${SEC_CLIENT_LIB})

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-multichar -std=c++11")

if(CMAKE_IARM_MGR)
	message("CMAKE_IARM_MGR set")
	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DIARM_MGR")
endif()

set(GSTAAMP_SOURCES gstaampinit.cpp)
if(CMAKE_CDM_DRM)
        message("CMAKE_CDM_DRM set")
	set(GSTAAMP_SOURCES "${GSTAAMP_SOURCES}" drm/gst/gstaampcdmidecryptor.cpp drm/gst/gstaampplayreadydecryptor.cpp drm/gst/gstaampwidevinedecryptor.cpp drm/gst/gstaampclearkeydecryptor.cpp drm/gst/gstaampverimatrixdecryptor.cpp)
endif()

if(NOT DEFINED CMAKE_GST_SUBTEC_ENABLED)
    set(CMAKE_GST_SUBTEC_ENABLED TRUE)
endif()
if (CMAKE_GST_SUBTEC_ENABLED)
	if (CMAKE_INBUILT_AAMP_DEPENDENCIES)
		message("AAMP consolidated build, enabling gst_subtec")
		set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DGST_SUBTEC_ENABLED")
		add_subdirectory(gst_subtec)
	else()
		find_library(LIBSUBTEC_LIBRARY NAMES subtec libsubtec.so)
		message("LIBSUBTEC_LIBRARY ${LIBSUBTEC_LIBRARY}")
		if(LIBSUBTEC_LIBRARY STREQUAL LIBSUBTEC_LIBRARY-NOTFOUND)
			message("libsubtec.so not present - skipping gst_subtec build")
		else()
			message("Adding gst_subtec")
			set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DGST_SUBTEC_ENABLED")
			add_subdirectory(gst_subtec)
		endif()
	endif()
endif()


add_library(gstaamp SHARED ${GSTAAMP_SOURCES})

if(CMAKE_CDM_DRM)
	target_include_directories (gstaamp PRIVATE drm/gst)
	if(CMAKE_USE_OPENCDM_ADAPTER)
		message("CMAKE_USE_OPENCDM_ADAPTER set")
		set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DUSE_OPENCDM_ADAPTER")
		set(AAMP_COMMON_DEPENDENCIES "${AAMP_COMMON_DEPENDENCIES} -locdm")
		find_path (STAGING_INCDIR opencdm)
		include_directories(${STAGING_INCDIR}/opencdm)
	else() 
		message("CMAKE_USE_OPENCDM_ADAPTER not set")
	endif()
	set(AAMP_DEFINES "${AAMP_DEFINES} -DDRM_BUILD_PROFILE=DRM_BUILD_PROFILE_OEM -DTARGET_LITTLE_ENDIAN=1 -DTARGET_SUPPORTS_UNALIGNED_DWORD_POINTERS=0 ${SEC_CONTENT_METADATA_ENABLED}")
else()
    message("CMAKE_CDM_DRM not set")
endif()

target_link_libraries (gstaamp aamp ${AAMP_COMMON_DEPENDENCIES} )
target_include_directories (gstaamp PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../../ ${CMAKE_CURRENT_SOURCE_DIR}/../../drm ${CMAKE_CURRENT_SOURCE_DIR}/../../drm/helper ${CMAKE_CURRENT_SOURCE_DIR}/../../tsb/api ${CMAKE_CURRENT_SOURCE_DIR}/../../downloader ${CMAKE_CURRENT_SOURCE_DIR}/../../middleware ${CMAKE_CURRENT_SOURCE_DIR}/../../middleware/drm ${CMAKE_CURRENT_SOURCE_DIR}/../../middleware/drm/ocdm ${CMAKE_CURRENT_SOURCE_DIR}/../../middleware/drm/helper)
target_include_directories (gstaamp PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../../support/aampabr)
target_include_directories (gstaamp PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../../support/aampmetrics)

set(LIBAAMP_DEFINES "${AAMP_DEFINES}")

if(CMAKE_SOC_PLATFORM_REALTEK)
	message("CMAKE_SOC_PLATFORM_REALTEK set")
	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DREALTEKCE=1")
endif()

if (CMAKE_AMLOGIC_SOC)
        message("CMAKE_AMLOGIC_SOC set")
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DAMLOGIC")
		target_link_libraries (gstaamp gstsvpext)
endif()

#TODO Remove once PrivateInstanceAAMP is compilation flag independent.
if(CMAKE_USE_SECCLIENT)
	message("CMAKE_USE_SECCLIENT set")
	set(LIBAAMP_DEFINES "${LIBAAMP_DEFINES} -DUSE_SECCLIENT")
elseif(CMAKE_USE_SECMANAGER)
	message("CMAKE_USE_SECMANAGER set")
	set(LIBAAMP_DEFINES "${LIBAAMP_DEFINES} -DUSE_SECMANAGER")
endif()

install(TARGETS gstaamp DESTINATION lib/gstreamer-1.0)

if(CMAKE_WPEWEBKIT_JSBINDINGS)
	message("CMAKE_WPEWEBKIT_JSBINDINGS set")
	target_link_libraries (gstaamp aampjsbindings)
	set(LIBAAMP_DEFINES "${LIBAAMP_DEFINES} -DAAMP_JSCONTROLLER_ENABLED")
endif()

set_target_properties(gstaamp PROPERTIES COMPILE_FLAGS "${LIBAAMP_DEFINES}")
