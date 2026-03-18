#
# Copyright (C) 2022 RDK Management
#
# This library is free software; you can redistribute it and/or
# modify it under the terms of the GNU Lesser General Public
# License as published by the Free Software Foundation;
# version 2.1 of the License.
#
# This library is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
# Lesser General Public License for more details.
#
# You should have received a copy of the GNU Lesser General Public
# License along with this library; if not, write to the Free Software
# Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
#

cmake_minimum_required (VERSION 3.5)
project (gst_subtec)
set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
find_package(PkgConfig REQUIRED)

pkg_check_modules(GSTREAMER REQUIRED gstreamer-1.0)
pkg_check_modules(GSTREAMERBASE REQUIRED gstreamer-app-1.0)

include_directories(${CMAKE_CURRENT_SOURCE_DIR})
include_directories(${GSTREAMER_INCLUDE_DIRS})
include_directories(${GSTREAMERBASE_INCLUDE_DIRS})

if(CMAKE_SYSTEM_NAME STREQUAL Darwin)
    include_directories(${CMAKE_CURRENT_SOURCE_DIR}/../../../middleware/subtec/libsubtec)
    link_directories(${GSTREAMERBASE_LIBRARY_DIRS} ${GSTREAMER_LIBRARY_DIRS})
    link_directories(${CMAKE_INSTALL_PREFIX})
endif(CMAKE_SYSTEM_NAME STREQUAL Darwin)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-multichar -DSUBTEC_PACKET_DEBUG")

set(GSTSUBTEC_DEPENDENCIES ${GSTREAMERBASE_LIBRARIES} ${GSTREAMER_LIBRARIES})

add_library(gstsubtecsink SHARED gstsubtecsink.cpp)

message(STATUS "GSTSUBTEC_DEPENDENCIES: ${GSTSUBTEC_DEPENDENCIES}")
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/../../../middleware/subtec/libsubtec)
target_link_libraries(gstsubtecsink PRIVATE subtec ${GSTSUBTEC_DEPENDENCIES})

add_library(gstsubtecbin SHARED gstsubtecbin.cpp)

target_link_libraries(gstsubtecbin ${GSTSUBTEC_DEPENDENCIES})

add_library(gstsubtecmp4transform SHARED gstsubtecmp4transform.cpp)

target_link_libraries(gstsubtecmp4transform ${GSTSUBTEC_DEPENDENCIES})

add_library(gstvipertransform SHARED gstvipertransform.cpp)

target_link_libraries(gstvipertransform ${GSTSUBTEC_DEPENDENCIES})

install (TARGETS gstsubtecsink
         RUNTIME DESTINATION bin
         LIBRARY DESTINATION lib/gstreamer-1.0
         ARCHIVE DESTINATION lib/gstreamer-1.0
)

install (TARGETS gstsubtecbin
         RUNTIME DESTINATION bin
         LIBRARY DESTINATION lib/gstreamer-1.0
         ARCHIVE DESTINATION lib/gstreamer-1.0
)

install (TARGETS gstsubtecmp4transform
         RUNTIME DESTINATION bin
         LIBRARY DESTINATION lib/gstreamer-1.0
         ARCHIVE DESTINATION lib/gstreamer-1.0
)

install (TARGETS gstvipertransform
         RUNTIME DESTINATION bin
         LIBRARY DESTINATION lib/gstreamer-1.0
         ARCHIVE DESTINATION lib/gstreamer-1.0
)
