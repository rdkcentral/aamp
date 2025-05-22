# Middleware CMakeLists.txt

cmake_minimum_required(VERSION 3.5)
project(Middleware)
find_package(PkgConfig REQUIRED)
pkg_check_modules(GST REQUIRED gstreamer-1.0 gstreamer-plugins-base-1.0)
include_directories(${GST_INCLUDE_DIRS})

set(SOURCES GstUtils.cpp InterfacePlayerRDK.cpp)
add_library(middleware SHARED ${SOURCES})
target_link_libraries(middleware ${GST_LIBRARIES})

target_include_directories(middleware PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
