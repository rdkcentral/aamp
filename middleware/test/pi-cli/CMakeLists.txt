cmake_minimum_required(VERSION 3.10)

# Project name
project(pi-cli VERSION 1.0 LANGUAGES CXX)

set(PI_ROOT "../..")

# Set C++ standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED True)

include_directories(${PI_ROOT})

# Add the executable (now includes commandProcessing.cpp)
add_executable(pi-cli
    main.cpp
    commandProcessing.cpp
)

# Use pkg-config to locate the readline library
find_package(PkgConfig REQUIRED)
pkg_check_modules(READLINE REQUIRED readline)

if (READLINE_FOUND)
    include_directories(${READLINE_INCLUDE_DIRS})
    link_directories(${READLINE_LIBRARY_DIRS})
    target_link_libraries(pi-cli ${READLINE_LIBRARIES})
else()
    message(FATAL_ERROR "Readline library not found. Please install it.")
endif()

target_link_libraries(pi-cli playergstinterface)

# Additional warnings and flags for development
if (CMAKE_CXX_COMPILER_ID STREQUAL "GNU" OR CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
    target_compile_options(pi-cli PRIVATE -Wall -Wextra -pedantic)
endif()