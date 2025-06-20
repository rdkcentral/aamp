cmake_minimum_required(VERSION 3.5)
project(playerLogManager)

include_directories(..)

if(CMAKE_SYSTEMD_JOURNAL)
    message("CMAKE_SYSTEMD_JOURNAL set")
    set(LIB_PLAYER_LOG_MANAGER_DEPENDS "${LIB_PLAYER_LOG_MANAGER_DEPENDS} -lsystemd")
    set(LIB_PLAYER_LOG_MANAGER_DEFINES "${LIB_PLAYER_LOG_MANAGER_DEFINES} -DUSE_SYSTEMD_JOURNAL_PRINT=1")
endif()

if(CMAKE_USE_ETHAN_LOG)
     message("DCMAKE_USE_ETHAN_LOG set")
# Find the ethanlog library for container logger
     find_package( EthanLog REQUIRED )
# Add the include directories for EthanLog
     include_directories(${ETHANLOG_INCLUDE_DIRS})

     set(LIB_PLAYER_LOG_MANAGER_DEPENDS "${LIB_PLAYER_LOG_MANAGER_DEPENDS} -lethanlog")
     set(LIB_PLAYER_LOG_MANAGER_DEFINES "${LIB_PLAYER_LOG_MANAGER_DEFINES} -DUSE_ETHAN_LOG=1")
endif()

set(PlayerLogManager_SRC PlayerLogManager.cpp)

add_library(playerLogManager SHARED ${PlayerLogManager_SRC})

set_target_properties(playerLogManager PROPERTIES PUBLIC_HEADER "PlayerLogManager.h")

set_target_properties(playerLogManager PROPERTIES COMPILE_FLAGS "${LIB_PLAYER_LOG_MANAGER_DEFINES}")

string(STRIP "${LIB_PLAYER_LOG_MANAGER_DEPENDS}" LIB_PLAYER_LOG_MANAGER_DEPENDS)

target_link_libraries(playerLogManager ${LIB_PLAYER_LOG_MANAGER_DEPENDS})

# Install the library and its headers
install(TARGETS playerLogManager
    DESTINATION lib
    PUBLIC_HEADER DESTINATION include)
