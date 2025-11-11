#
# If not stated otherwise in this file or this component's LICENSE file the
# following copyright and licenses apply:
#
# Copyright 2025 Sky UK
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
#

set(RialtoLogging_HEADERS
    include/RialtoLogging.h
    )

set(RialtoLogging_SOURCES
    source/EnvVariableParser.cpp
    source/EnvVariableParser.h
    source/LogFileHandle.cpp
    source/LogFileHandle.h
    source/RialtoLogging.cpp
    )

set(RialtoLogging_INCLUDES
    "${CMAKE_CURRENT_SOURCE_DIR}/include"
    )

add_library(RialtoLogging STATIC ${RialtoLogging_HEADERS} ${RialtoLogging_SOURCES})
target_include_directories(RialtoLogging PUBLIC "$<BUILD_INTERFACE:${RialtoLogging_INCLUDES}>")
set_target_properties(RialtoLogging PROPERTIES CXX_STANDARD 17 CXX_STANDARD_REQUIRED ON CXX_EXTENSIONS OFF)
set_target_properties(RialtoLogging PROPERTIES POSITION_INDEPENDENT_CODE ON)

find_package(EthanLog)
if (EthanLog_FOUND AND RIALTO_ENABLE_ETHAN_LOG)
    message(STATUS "EthanLog is enabled")
    add_library(RialtoEthanLog STATIC ${RialtoLogging_HEADERS} ${RialtoLogging_SOURCES})
    target_include_directories(RialtoEthanLog PUBLIC "$<BUILD_INTERFACE:${RialtoLogging_INCLUDES}>")
    target_compile_definitions(RialtoEthanLog PRIVATE USE_ETHANLOG)
    target_link_libraries(RialtoEthanLog PUBLIC EthanLog::EthanLog)
    set_target_properties(RialtoEthanLog PROPERTIES CXX_STANDARD 17 CXX_STANDARD_REQUIRED ON CXX_EXTENSIONS OFF)
    set_target_properties(RialtoEthanLog PROPERTIES POSITION_INDEPENDENT_CODE ON)
else ()
    message(STATUS "EthanLog is disabled")
    add_library(RialtoEthanLog ALIAS RialtoLogging)
endif ()
