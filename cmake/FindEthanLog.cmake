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

# - Try to find the ethanlog library for RDK container logging.
#
# The following are set after configuration is done:
#  ETHANLOG_FOUND
#  ETHANLOG_LIBRARY
#  ETHANLOG_INCLUDE_DIR
#  ETHANLOG_LIBRARIES
#  ETHANLOG_INCLUDE_DIRS

find_library( ETHANLOG_LIBRARY
    NAMES ethanlog libethanlog.so.3 libethanlog
    PATH_SUFFIXES arm-linux-gnueabihf )

find_path( ETHANLOG_INCLUDE_DIR NAMES ethanlog.h )

# Fallback: derive include dir from library location for cross-compilation
# environments (e.g. XiOne sysroot) where find_path may not search the sysroot
# even when find_library succeeds.
if( NOT ETHANLOG_INCLUDE_DIR AND ETHANLOG_LIBRARY )
    get_filename_component( _ethan_lib_dir "${ETHANLOG_LIBRARY}" DIRECTORY )
    get_filename_component( _ethan_prefix  "${_ethan_lib_dir}" DIRECTORY )
    find_path( ETHANLOG_INCLUDE_DIR NAMES ethanlog.h
        PATHS "${_ethan_prefix}/include"
        NO_DEFAULT_PATH )
endif()

include( FindPackageHandleStandardArgs )

# Handle the QUIETLY and REQUIRED arguments and set ETHANLOG_FOUND to TRUE
# if all listed variables are TRUE
find_package_handle_standard_args(EthanLog DEFAULT_MSG
    ETHANLOG_LIBRARY ETHANLOG_INCLUDE_DIR )

mark_as_advanced( ETHANLOG_INCLUDE_DIR ETHANLOG_LIBRARY )

if( ETHANLOG_FOUND )
    set( ETHANLOG_LIBRARIES    ${ETHANLOG_LIBRARY} )
    set( ETHANLOG_INCLUDE_DIRS ${ETHANLOG_INCLUDE_DIR} )
    if( NOT TARGET EthanLog::EthanLog )
        add_library( EthanLog::EthanLog SHARED IMPORTED )
        set_target_properties( EthanLog::EthanLog PROPERTIES
            IMPORTED_LOCATION             "${ETHANLOG_LIBRARY}"
            INTERFACE_INCLUDE_DIRECTORIES "${ETHANLOG_INCLUDE_DIR}" )
    endif()
endif()
