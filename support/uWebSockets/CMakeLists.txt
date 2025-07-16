# If not stated otherwise in this file or this component's Licenses.txt
# file the following copyright and licenses apply:
#
# Copyright 2025 RDK Management
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
# http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
cmake_minimum_required (VERSION 3.5)
project (uWebSockets)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

find_package(OpenSSL REQUIRED)
find_package(ZLIB REQUIRED)
include_directories(${OPENSSL_INCLUDE_DIR} ${ZLIB_INCLUDE_DIRS})

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fPIC -Wno-multichar")

file(GLOB_RECURSE UWS_HEADERS "*.h" "uSockets/*.h" "uSockets/internal/*.h" "uSockets/internal/eventing/*.h" "uSockets/internal/networking/*.h" "uSockets/io_uring/*.h" "uSockets/eventing/*.h" "uSockets/crypto/*.h")

file(GLOB USOCKETS_SOURCES "uSockets/*.c" "uSockets/internal/*.c" "uSockets/internal/eventing/*.c" "uSockets/internal/networking/*.c" "uSockets/io_uring/*.c" "uSockets/eventing/*.c" "uSockets/crypto/*.c")
list(APPEND USOCKETS_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/uSockets/crypto/openssl.c
    ${CMAKE_CURRENT_SOURCE_DIR}/uSockets/crypto/sni_tree.cpp
)


add_library(uWebSockets SHARED ${USOCKETS_SOURCES})
target_compile_definitions(uWebSockets PUBLIC LIBUS_USE_OPENSSL)
target_include_directories(uWebSockets PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/uSockets
)

target_link_libraries(uWebSockets PUBLIC ${SYSTEMD_LIB} ${OPENSSL_LIBRARIES} ${ZLIB_LIBRARIES} pthread)

set_target_properties(uWebSockets PROPERTIES
    PUBLIC_HEADER "${UWS_HEADERS}"
)

install(TARGETS uWebSockets
    DESTINATION lib
    PUBLIC_HEADER DESTINATION include
)
