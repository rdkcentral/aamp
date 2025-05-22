# If not stated otherwise in this file or this component's LICENSE file the
# following copyright and licenses apply:
#
# Copyright 2024 RDK Management
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

project(TsbMockFilesystemTests)

# Include the faked/mocked filesystem and fstream standard library implementations
# Declare mocks to be a system include, so that it doesn't generate gMock-related compiler warning
include_directories(fakes)
include_directories(SYSTEM mocks)

add_definitions(-DENABLE_TSB_LOGGER)
add_definitions(-DENABLE_TSB_FAKES_LOG)
add_compile_options(-Wall -Wextra -Wpedantic -Werror -Werror=effc++)
set_property(GLOBAL PROPERTY CTEST_TARGETS_ADDED 1)

add_subdirectory(tests)
