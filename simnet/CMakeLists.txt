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

cmake_minimum_required(VERSION 3.5)
project(NetPersonaFitter)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Parent AAMP root is one level up from this directory
set(AAMP_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/..")

include_directories(
    ${AAMP_ROOT}
)

add_library(net_persona_fitter STATIC net_persona_fitter.cpp)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fPIC")

set_target_properties(net_persona_fitter PROPERTIES
    PUBLIC_HEADER "net_persona_fitter.h"
)

install(TARGETS net_persona_fitter
    DESTINATION lib
    PUBLIC_HEADER DESTINATION include
)

