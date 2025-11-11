#
# If not stated otherwise in this file or this component's LICENSE file the
# following copyright and licenses apply:
#
# Copyright 2022 Sky UK
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

cmake_minimum_required( VERSION 3.10 )
project( RialtoIpc )

# The following is to check if building this component from this CMakeLists.txt file
# or if it is part of a bigger project
get_directory_property( HAS_PARENT PARENT_DIRECTORY )
if( NOT HAS_PARENT )

    set( PROTOC_PATH "" CACHE STRING "Optional path to the host protobuf compiler (protoc) tool" )
    if( PROTOC_PATH )
        add_executable( protobuf::protoc IMPORTED )
        set_target_properties( protobuf::protoc PROPERTIES IMPORTED_LOCATION "${PROTOC_PATH}" )
    endif()

    # Requires protobuf libraries
    find_package( Protobuf REQUIRED )

    # Add the logging component (static library)
    add_subdirectory( "${CMAKE_CURRENT_LIST_DIR}/../rialtoLogging" "${CMAKE_CURRENT_BINARY_DIR}/rialtoLogging" )

endif()

# We require C++14
set( CMAKE_CXX_STANDARD 17 )
set( CMAKE_CXX_STANDARD_REQUIRED ON )

# Build all components with -fPIC as ultimates
set( CMAKE_POSITION_INDEPENDENT_CODE ON )

# Sub-components
add_subdirectory( common )
add_subdirectory( client )
add_subdirectory( server )
add_subdirectory( examples EXCLUDE_FROM_ALL )

