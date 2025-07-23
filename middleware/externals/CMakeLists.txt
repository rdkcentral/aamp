cmake_minimum_required(VERSION 2.6)
project(playerfbinterface_LIB)

option(DISABLE_SECURITY_TOKEN "Disable security token" OFF)

if(DISABLE_SECURITY_TOKEN)
  add_definitions(-DDISABLE_SECURITY_TOKEN)
endif()

set(MW_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../")

find_package(PkgConfig REQUIRED)

set_property(GLOBAL PROPERTY USE_FOLDERS ON)

pkg_check_modules(GSTREAMER REQUIRED gstreamer-1.0)
pkg_check_modules(GSTREAMERBASE REQUIRED gstreamer-app-1.0)
pkg_check_modules(GSTREAMERVIDEO REQUIRED gstreamer-video-1.0)

include_directories(${GSTREAMER_INCLUDE_DIRS})
include_directories(${MW_ROOT}/playerLogManager)
include_directories(${MW_ROOT}/baseConversion)
include_directories(${MW_ROOT})
include_directories(${CMAKE_CURRENT_SOURCE_DIR})
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/rdk)

set(LIB_EXT_DEPENDS ${LIB_EXT_DEPENDS} ${GSTREAMER_LINK_LIBRARIES} ${GSTREAMERBASE_LINK_LIBRARIES})

# Library source files
set(EXT_SOURCES PlayerExternalsInterface.cpp PlayerRfc.cpp PlayerExternalUtils.cpp)

set(SECMGR_SOURCES playersecmanager/PlayerMemoryUtils.cpp
    playersecmanager/PlayerSecInterface.cpp
    playersecmanager/PlayerSecManager.cpp
    playersecmanager/ThunderAccessPlayer.cpp
    ${MW_ROOT}/PlayerScheduler.cpp)

set(THUNDER_INTERFACE_SRCS PlayerThunderInterface.cpp)

#include WPEFRAMEWORK stuff
if (CMAKE_WPEFRAMEWORK_REQUIRED)
    message("CMAKE_WPEFRAMEWORK_REQUIRED set")
    list(APPEND CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/cmake/")
    set(LIB_EXT_DEFINES "${LIB_EXT_DEFINES} -DUSE_CPP_THUNDER_PLUGIN_ACCESS")
    set(THUNDER_INTERFACE_SRCS ${THUNDER_INTERFACE_SRCS} rdk/PlayerThunderAccess.cpp Module.cpp)
    find_package(WPEFramework REQUIRED)

    if (WPEFRAMEWORK_FOUND)
        message("WPEFRAMEWORK_FOUND = ${WPEFRAMEWORK_FOUND}")
        include_directories(SYSTEM ${WPEFRAMEWORK_INCLUDE_DIRS})
        list(APPEND LIB_EXT_DEPENDS ${WPEFRAMEWORK_LIBRARIES})
    else()
        if (USE_THUNDER_R4)
            message(FATAL_ERROR "WPEFrameworkCOM not found")
        else()
            message(FATAL_ERROR "WPEFrameworkProtocols not found")
        endif()
    endif()
endif()

#IARM/RFC
if(CMAKE_IARM_MGR)
    message("PLAYER IARM_MGR set")
    set(LIB_EXT_DEFINES "${LIB_EXT_DEFINES} -DIARM_MGR=1")
    set(EXT_SOURCES "${EXT_SOURCES}" rdk/PlayerExternalsRdkInterface.cpp)
    list(APPEND LIB_EXT_DEPENDS -lIARMBus -lds -ldshalcli)
endif()

if(CMAKE_AAMP_RFC_REQUIRED)
    message("PLAYER RFC set")
    set(LIB_EXT_DEFINES "${LIB_EXT_DEFINES} -DPLAYER_RFC_ENABLED")
    list(APPEND LIB_EXT_DEPENDS -ltr181api)
endif()

#secmanager/client macros
if(CMAKE_USE_SECCLIENT)
	message("CMAKE_USE_SECCLIENT set")
	set(LIB_EXT_DEFINES "${LIB_EXT_DEFINES} -DUSE_SECCLIENT")
    list(APPEND LIB_EXT_DEPENDS -lSecClient )
endif()
if(CMAKE_USE_SECMANAGER)
	message("CMAKE_USE_SECMANAGER set")
	set(LIB_EXT_DEFINES "${LIB_EXT_DEFINES} -DUSE_SECMANAGER")
endif()

# Define the shared library
if(NOT TARGET playerfbinterface)
    add_library(playerfbinterface SHARED ${EXT_SOURCES} ${SECMGR_SOURCES} ${THUNDER_INTERFACE_SRCS})
endif()

# Set initial public headers for the library
set_target_properties(playerfbinterface PROPERTIES PUBLIC_HEADER 
  "PlayerThunderInterface.h;PlayerThunderAccessBase.h;PlayerExternalsInterfaceBase.h;PlayerExternalsInterface.h;PlayerRfc.h;playersecmanager/PlayerSecManagerSession.h;playersecmanager/ThunderAccessPlayer.h;playersecmanager/PlayerSecInterface.h;playersecmanager/PlayerSecManager.h;playersecmanager/PlayerMemoryUtils.h;Module.h")

if(CMAKE_IARM_MGR)
    # Append additional header(s) for IARM_MGR
    set_property(TARGET playerfbinterface APPEND PROPERTY PUBLIC_HEADER "rdk/PlayerExternalsRdkInterface.h")
endif()

if(CMAKE_WPEFRAMEWORK_REQUIRED)
    # Append additional header(s) for WPEFramework usage
    set_property(TARGET playerfbinterface APPEND PROPERTY PUBLIC_HEADER "rdk/PlayerThunderAccess.h")
endif()

set_target_properties(playerfbinterface PROPERTIES COMPILE_FLAGS "${LIB_EXT_DEFINES}")

string(STRIP "${LIB_EXT_DEPENDS}" LIB_EXT_DEPENDS)

target_link_libraries(playerfbinterface ${LIB_EXT_DEPENDS})

target_link_libraries(playerfbinterface playerlogmanager)
target_link_libraries(playerfbinterface baseconversion)

# Install the library and its headers
install(TARGETS playerfbinterface
    DESTINATION lib
    PUBLIC_HEADER DESTINATION include)
