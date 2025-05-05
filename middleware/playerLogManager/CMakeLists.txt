cmake_minimum_required(VERSION 3.5)
project(PlayerLogManager)

include_directories(..)

set(PlayerLogManager_SRC PlayerLogManager.cpp)

add_library(PlayerLogManager SHARED ${PlayerLogManager_SRC})

set_target_properties(PlayerLogManager PROPERTIES PUBLIC_HEADER "PlayerLogManager.h")

# Install the library and its headers
install(TARGETS PlayerLogManager
    DESTINATION lib
    PUBLIC_HEADER DESTINATION include)
