# If not stated otherwise in this file or this component's license file the
# following copyright and licenses apply:
#
# Copyright 2022 RDK Management
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

file(GLOB fakes_SRC "*.cpp")

set(PLAYER_ROOT "../../../")
set(UTESTS_ROOT "..")

include_directories(${PLAYER_ROOT})
include_directories(${PLAYER_ROOT}/subtec/subtecparser)
include_directories(${PLAYER_ROOT}/subtec/libsubtec)
include_directories(${PLAYER_ROOT}/playerjsonobject)
include_directories(${PLAYER_ROOT}/vendor)
include_directories(${PLAYER_ROOT}/playerLogManager)
include_directories(${PLAYER_ROOT}/externals)
include_directories(${PLAYER_ROOT}/vendor)
include_directories(${PLAYER_ROOT}/playerLogManager)
include_directories(${PLAYER_ROOT}/drm/helper)
include_directories(${UTESTS_ROOT}/drm/ocdm)
include_directories(${UTESTS_ROOT}/drm/mocks)
include_directories(${UTESTS_ROOT}/mocks)
include_directories(${UTESTS_ROOT}/ocdm)
include_directories(${PLAYER_ROOT}/drm)
include_directories(${PLAYER_ROOT}/drm/ocdm)
include_directories(${PLAYER_ROOT}/baseConversion)
include_directories(${PLAYER_ROOT}/externals/contentsecuritymanager)
include_directories(${GTEST_INCLUDE_DIRS})
include_directories(${GMOCK_INCLUDE_DIRS})
include_directories(${GLIB_INCLUDE_DIRS})
include_directories(${GSTREAMER_INCLUDE_DIRS})
include_directories(${LIBCJSON_INCLUDE_DIRS})
include_directories(${LibXml2_INCLUDE_DIRS})
message(STATUS "PLAYER_ROOT=${PLAYER_ROOT}")


if (JSC_INCDIR)
    include_directories(${JSC_INCDIR})
endif()

# Mac OS X
if(CMAKE_SYSTEM_NAME STREQUAL Darwin)
    include_directories(${PLAYER_ROOT}/.libs/systemd/src)
endif(CMAKE_SYSTEM_NAME STREQUAL Darwin)

include_directories(${PLAYER_ROOT}/.libs/include)
include_directories(${UTESTS_ROOT}/mocks)

add_library(fakes ${fakes_SRC})
target_include_directories(fakes PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../../)
set_target_properties(fakes PROPERTIES FOLDER "utests")
set_target_properties(fakes PROPERTIES COMPILE_FLAGS "-Wno-effc++ -Wno-varargs")
