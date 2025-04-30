cmake_minimum_required(VERSION 3.5)
project(plyrLogMgr)

include_directories(..)

set(PLYRLOGMGR_SRC PlayerLogManager.cpp)

add_library(plyrLogMgr SHARED ${PLYRLOGMGR_SRC})

set_target_properties(plyrLogMgr PROPERTIES PUBLIC_HEADER "PlayerLogManager.h")

# Install the library and its headers
install(TARGETS plyrLogMgr
    DESTINATION lib
    PUBLIC_HEADER DESTINATION include)
