cmake_minimum_required(VERSION 3.5)
project(BaseConversion)

include_directories(..)

set(BaseConversion_SRC _base64.cpp base16.cpp)

add_library(BaseConversion SHARED ${BaseConversion_SRC})

set_target_properties(BaseConversion PROPERTIES PUBLIC_HEADER "_base64.h;base16.h")

# Install the library and its headers
install(TARGETS BaseConversion
    DESTINATION lib
    PUBLIC_HEADER DESTINATION include)
