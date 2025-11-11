#
# If not stated otherwise in this file or this component's LICENSE file the
# following copyright and licenses apply:
#
# Copyright 2023 Sky UK
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

set( CMAKE_CXX_STANDARD 17 )

set( CMAKE_CXX_STANDARD_REQUIRED ON )
include( CheckCXXCompilerFlag )

configure_file(
        include/SessionServerCommon.in
        include/SessionServerCommon.h
        @ONLY )

add_library(
        RialtoCommonPublic
        INTERFACE
    )

target_include_directories(
        RialtoCommonPublic

        INTERFACE
        ${CMAKE_BINARY_DIR}/common/public/include
    )

if( NOT CMAKE_BUILD_TYPE STREQUAL "UnitTests" )
        include( GNUInstallDirs )
        set (LIB_RIALTO_COMMON_PUBLIC_HEADERS
                ${CMAKE_BINARY_DIR}/common/public/include/SessionServerCommon.h
        )
        install (
                FILES ${LIB_RIALTO_COMMON_PUBLIC_HEADERS}
                DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/rialto
        )
endif()
