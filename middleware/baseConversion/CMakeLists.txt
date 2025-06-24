cmake_minimum_required(VERSION 3.5)

project(baseconversion)
set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include_directories(..)

set(BaseConversion_SRC _base64.cpp base16.cpp)

add_library(baseconversion SHARED ${BaseConversion_SRC})

set_target_properties(baseconversion PROPERTIES PUBLIC_HEADER "_base64.h;base16.h")

# Install the library and its headers
install(TARGETS baseconversion
    DESTINATION lib
    PUBLIC_HEADER DESTINATION include)
