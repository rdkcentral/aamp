cmake_minimum_required(VERSION 3.5)
project(PlayerJsonObject)

include_directories(..)

set(PlayerJsonObject_SRC PlayerJsonObject.cpp)

add_library(PlayerJsonObject SHARED ${PlayerJsonObject_SRC})

set_target_properties(PlayerJsonObject PROPERTIES PUBLIC_HEADER "PlayerJsonObject.h")

# Install the library and its headers
install(TARGETS PlayerJsonObject
    DESTINATION lib
    PUBLIC_HEADER DESTINATION include)
