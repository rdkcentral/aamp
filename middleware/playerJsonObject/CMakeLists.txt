cmake_minimum_required(VERSION 3.5)
project(playerjsonobject)

include_directories(..)

set(PlayerJsonObject_SRC PlayerJsonObject.cpp)

add_library(playerjsonobject SHARED ${PlayerJsonObject_SRC})

set_target_properties(playerjsonobject PROPERTIES PUBLIC_HEADER "PlayerJsonObject.h")

# Install the library and its headers
install(TARGETS playerjsonobject
    DESTINATION lib
    PUBLIC_HEADER DESTINATION include)
