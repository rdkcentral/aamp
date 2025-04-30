cmake_minimum_required(VERSION 3.5)
project(base16_64)

include_directories(..)

set(BASE16_64_SRC _base64.cpp base16.cpp)

add_library(base16_64 SHARED ${BASE16_64_SRC})

set_target_properties(base16_64 PROPERTIES PUBLIC_HEADER "_base64.h;base16.h")

# Install the library and its headers
install(TARGETS base16_64
    DESTINATION lib
    PUBLIC_HEADER DESTINATION include)
