# If not stated otherwise in this file or this component's license file the
# following copyright and licenses apply:
#
# Copyright 2026 RDK Management
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

# Build a Rialto simulator library when the real Rialto client is not available.
# This provides a behavioural simulator that drives IMediaPipelineClient
# callbacks so AAMP's DirectRialto code can be exercised in L2 integration tests.

cmake_minimum_required(VERSION 3.10)

project(RialtoSimulator)

# The simulator needs the Rialto public headers (always cloned/copied by install_rialto.sh)
if(RIALTO_INCLUDE_DIRS)
    set(SIM_INCLUDE_DIRS ${RIALTO_INCLUDE_DIRS})
else()
    # Fallback: use the .libs/include/rialto path directly
    set(SIM_INCLUDE_DIRS "${CMAKE_SOURCE_DIR}/.libs/include/rialto")
endif()

find_package(Threads REQUIRED)

add_library(RialtoSimulator SHARED
    RialtoSimulator.cpp
)

target_include_directories(RialtoSimulator PRIVATE
    ${SIM_INCLUDE_DIRS}
)

target_link_libraries(RialtoSimulator PRIVATE
    Threads::Threads
)

set_target_properties(RialtoSimulator PROPERTIES
    OUTPUT_NAME "RialtoClient"
    VERSION 0.1.0
    SOVERSION 0
)

install(TARGETS RialtoSimulator
    LIBRARY DESTINATION lib
)
