# If not stated otherwise in this file or this component's license file the
# following copyright and licenses apply:
#
# Copyright 2023 RDK Management
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

set(AAMP_ROOT "../../")
set(EXEC_NAME manifeststicher)
find_package(PkgConfig REQUIRED)
pkg_check_modules(LibXml2 REQUIRED libxml-2.0)
find_package(Boost 1.40 COMPONENTS system filesystem REQUIRED)

include_directories(${AAMP_ROOT} ${AAMP_ROOT}/dash/xml ${AAMP_ROOT}/dash/utils ${AAMP_ROOT}/dash/mpd)
include_directories(${LibXml2_INCLUDE_DIRS})
include_directories(${Boost_INCLUDE_DIRS})
message("Boost include" ${Boost_INCLUDE_DIRS} "lib" ${Boost_LIBRARIES})
# Mac OS X
if(CMAKE_SYSTEM_NAME STREQUAL Darwin)
    include_directories(/usr/local/include)
    include_directories(/usr/local/include/libdash)
    set(OS_LD_FLAGS -L/usr/local/lib)
    find_library(AAMP_LIB NAMES aamp PATHS ${AAMP_ROOT}/build/Debug ${AAMP_ROOT}/build /usr/local/lib)
else()
    include_directories(${AAMP_ROOT}/Linux/include)
    include_directories(${AAMP_ROOT}/Linux/include/libdash)
    set(OS_LD_FLAGS -luuid)
    find_library(AAMP_LIB NAMES aamp PATHS ${AAMP_ROOT}/build ${AAMP_ROOT}/Linux/lib /usr/local/lib /usr/lib)
endif(CMAKE_SYSTEM_NAME STREQUAL Darwin)

set(TEST_SOURCES ManifestSticher.cpp)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++14 -Wno-multichar -Wno-non-virtual-dtor -Wno-psabi")

add_executable(${EXEC_NAME}
               ${TEST_SOURCES})
if (CMAKE_XCODE_BUILD_SYSTEM)
  # XCode schema target
    message( "xcode_define_schema for ${EXEC_NAME}" )
    set_property(TARGET ${EXEC_NAME} PROPERTY XCODE_GENERATE_SCHEME TRUE)
    set_property(TARGET ${EXEC_NAME} PROPERTY XCODE_SCHEME_ADDRESS_SANITIZER TRUE)
    set_property(TARGET ${EXEC_NAME} PROPERTY XCODE_SCHEME_ADDRESS_SANITIZER_USE_AFTER_RETURN TRUE)
endif()
target_link_libraries(${EXEC_NAME} LINK_PUBLIC ${Boost_LIBRARIES})
target_link_libraries(${EXEC_NAME} ${AAMP_LIB})
set_target_properties(${EXEC_NAME} PROPERTIES FOLDER "ManifestSticher")
