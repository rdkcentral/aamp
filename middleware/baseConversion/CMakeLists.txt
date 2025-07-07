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
		PUBLIC_HEADER DESTINATION include
)
