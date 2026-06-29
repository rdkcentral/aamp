# If not stated otherwise in this file or this component's LICENSE file the
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

cmake_minimum_required (VERSION 3.13)

project (ISOBMFF)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_library (isobmff STATIC
    isobmffbox.cpp
    isobmffbuffer.cpp
    isobmffhelper.cpp
)

set_target_properties(isobmff PROPERTIES PUBLIC_HEADER
    "${CMAKE_CURRENT_SOURCE_DIR}/isobmffbox.h;${CMAKE_CURRENT_SOURCE_DIR}/isobmffbuffer.h;${CMAKE_CURRENT_SOURCE_DIR}/isobmffhelper.h;${CMAKE_CURRENT_SOURCE_DIR}/IsoBmffLog.h"
)

set_property(TARGET isobmff PROPERTY POSITION_INDEPENDENT_CODE True)
