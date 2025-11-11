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

# Set the example include paths for the protoc compiler
set( Protobuf_IMPORT_DIRS "${CMAKE_CURRENT_LIST_DIR}/../common/proto" )

# Run the protoc tool to generate the code
include( FindProtobuf )
protobuf_generate_cpp( PROTO_SRCS PROTO_HEADERS example.proto )

# Get the directory where header was generated
list( GET PROTO_HEADERS 0 FIRST_PROTO_HEADER )
get_filename_component( PROTO_HEADER_DIR ${FIRST_PROTO_HEADER} DIRECTORY )

# Find includes in corresponding build directories
set( CMAKE_INCLUDE_CURRENT_DIR ON )


# Create the example client
add_executable( ExampleClient

        ExampleClient.cpp
        ${PROTO_SRCS}
        ${PROTO_HEADERS}

        )

target_include_directories( ExampleClient

        PRIVATE
        ${Protobuf_INCLUDE_DIRS}

        )

target_link_libraries( ExampleClient

        PRIVATE
        RialtoIpcCommon
        RialtoLogging
        RialtoIpcClient
        protobuf::libprotobuf

        )


# Create the example server
add_executable( ExampleServer

        ExampleServer.cpp
        ${PROTO_SRCS}
        ${PROTO_HEADERS}

        )

target_include_directories( ExampleServer

        PRIVATE
        ${Protobuf_INCLUDE_DIRS}

        )

target_link_libraries( ExampleServer

        PRIVATE
        RialtoIpcCommon
        RialtoLogging
        RialtoIpcServer
        protobuf::libprotobuf

        )

# Create the example with socket pair
add_executable( ExampleSocketPair

        ExampleSocketPair.cpp
        ${PROTO_SRCS}
        ${PROTO_HEADERS}

        )

target_include_directories( ExampleSocketPair

        PRIVATE
        ${Protobuf_INCLUDE_DIRS}

        )

target_link_libraries( ExampleSocketPair

        PRIVATE
        RialtoIpcCommon
        RialtoLogging
        RialtoIpcClient
        RialtoIpcServer
        protobuf::libprotobuf

        )
