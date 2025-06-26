cmake_minimum_required(VERSION 3.5)
project(playerlogmanager)

include_directories(..)

if(CMAKE_SYSTEMD_JOURNAL)
    message("CMAKE_SYSTEMD_JOURNAL set")
    set(LIBPLAYERLOGMANAGER_DEPENDS "${LIBPLAYERLOGMANAGER_DEPENDS} -lsystemd")
    set(LIBPLAYERLOGMANAGER_DEFINES "${LIBPLAYERLOGMANAGER_DEFINES} -DUSE_SYSTEMD_JOURNAL_PRINT=1")
endif()

if(CMAKE_USE_ETHAN_LOG)
     message("DCMAKE_USE_ETHAN_LOG set")
# Find the ethanlog library for container logger
     find_package( EthanLog REQUIRED )
# Add the include directories for EthanLog
     include_directories(${ETHANLOG_INCLUDE_DIRS})

     set(LIBPLAYERLOGMANAGER_DEPENDS "${LIBPLAYERLOGMANAGER_DEPENDS} -lethanlog")
     set(LIBPLAYERLOGMANAGER_DEFINES "${LIBPLAYERLOGMANAGER_DEFINES} -DUSE_ETHAN_LOG=1")
endif()

set(PlayerLogManager_SRC PlayerLogManager.cpp)

add_library(playerlogmanager SHARED ${PlayerLogManager_SRC})

set_target_properties(playerlogmanager PROPERTIES PUBLIC_HEADER "PlayerLogManager.h")

set_target_properties(playerlogmanager PROPERTIES COMPILE_FLAGS "${LIBPLAYERLOGMANAGER_DEFINES}")

string(STRIP "${LIBPLAYERLOGMANAGER_DEPENDS}" LIBPLAYERLOGMANAGER_DEPENDS)

target_link_libraries(playerlogmanager ${LIBPLAYERLOGMANAGER_DEPENDS})

# Install the library and its headers
install(TARGETS playerlogmanager
    DESTINATION lib
    PUBLIC_HEADER DESTINATION include)
