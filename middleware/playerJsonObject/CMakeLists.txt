# If not stated otherwise in this file or this component's license file the
# following copyright and licenses apply:
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

cmake_minimum_required(VERSION 3.5)

project(playerjsonobject)
set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(FindPkgConfig)
pkg_check_modules(LIBCJSON REQUIRED libcjson)
link_directories(${LIBCJSON_LIBRARY_DIRS})
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/../baseConversion)
set(PlayerJsonObject_SRC PlayerJsonObject.cpp)

add_library(playerjsonobject SHARED ${PlayerJsonObject_SRC})

target_include_directories(playerjsonobject PUBLIC
    ${LIBCJSON_INCLUDE_DIRS}
    ${CMAKE_CURRENT_SOURCE_DIR}/../baseConversion
)

target_link_libraries(playerjsonobject PUBLIC
    ${LIBCJSON_LINK_LIBRARIES}
    baseconversion
)

set_target_properties(playerjsonobject PROPERTIES
    PUBLIC_HEADER "PlayerJsonObject.h"
)

install(TARGETS playerjsonobject
    DESTINATION lib
    PUBLIC_HEADER DESTINATION include
)

