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

# - Try to find the Rialto library.
#
# The following are set after configuration is done:
#  RIALTO_FOUND
#  RIALTO_INCLUDE_DIRS
#  RIALTO_LIBRARIES
option( RIALTO_VERBOSE_BUILD
        "Enable verbose FindRialto.cmake diagnostics"
        OFF )
option( RIALTO_FORCE_SIMULATOR
        "Force building the Rialto simulator instead of linking to system Rialto"
        OFF )

# Platforms that do not ship protobuf/Rialto (e.g. macOS) always use the
# simulator.  Treat APPLE the same as an explicit RIALTO_FORCE_SIMULATOR=ON.
if( RIALTO_FORCE_SIMULATOR OR APPLE )
    if( RIALTO_VERBOSE_BUILD AND NOT RIALTO_FIND_QUIETLY )
        if( APPLE )
            message( STATUS
                    "FindRialto: Apple platform — protobuf/Rialto not "
                    "supported, using simulator" )
        else()
            message( STATUS
                    "FindRialto: RIALTO_FORCE_SIMULATOR requested" )
        endif()
    endif()
    # Skip all library/header searches; the simulator is built from
    # test/rialto/ and headers are sourced from .libs/include/rialto/.
    # Force-clear both cache variables so a stale value from a previous
    # non-simulator configure cannot sneak back.
    set( RIALTO_LIBRARY    "" CACHE FILEPATH "Rialto client library" FORCE )
    set( RIALTO_INCLUDE_DIR "" CACHE PATH    "Rialto include directory" FORCE )
else()
    find_library( RIALTO_LIBRARY NAMES libRialtoClient.so RialtoClient )
endif()
if( RIALTO_VERBOSE_BUILD AND NOT RIALTO_FIND_QUIETLY )
    message( STATUS "FindRialto: RIALTO_LIBRARY = ${RIALTO_LIBRARY}" )
endif()

if( NOT RIALTO_FORCE_SIMULATOR AND NOT APPLE )
    find_path( RIALTO_INCLUDE_DIR NAMES IMediaPipeline.h PATH_SUFFIXES rialto )
    if( RIALTO_VERBOSE_BUILD AND NOT RIALTO_FIND_QUIETLY )
        message( STATUS
                "FindRialto: RIALTO_INCLUDE_DIR (initial find_path) = "
                "${RIALTO_INCLUDE_DIR}" )
    endif()
endif()

# Fallback for cross-compilation environments (e.g. Yocto) where find_path
# may not search the sysroot even when find_library succeeds.
# Rialto always installs headers to <prefix>/include/rialto/, so derive the
# include path from the library location.
if( NOT RIALTO_INCLUDE_DIR AND RIALTO_LIBRARY )
    get_filename_component( _rialto_lib_dir "${RIALTO_LIBRARY}" DIRECTORY )
    get_filename_component( _rialto_prefix "${_rialto_lib_dir}" DIRECTORY )
    if( RIALTO_VERBOSE_BUILD AND NOT RIALTO_FIND_QUIETLY )
        message( STATUS
                "FindRialto: falling back to prefix-derived search in "
                "${_rialto_prefix}/include" )
    endif()
    find_path( RIALTO_INCLUDE_DIR NAMES IMediaPipeline.h
        PATHS "${_rialto_prefix}/include"
        PATH_SUFFIXES rialto
        NO_DEFAULT_PATH )
    if( RIALTO_VERBOSE_BUILD AND NOT RIALTO_FIND_QUIETLY )
        message( STATUS
                "FindRialto: RIALTO_INCLUDE_DIR (fallback) = "
                "${RIALTO_INCLUDE_DIR}" )
    endif()
endif()

include( FindPackageHandleStandardArgs )

# Handle the QUIETLY and REQUIRED arguments and set the RIALTO_FOUND to TRUE
# if all listed variables are TRUE
find_package_handle_standard_args( RIALTO DEFAULT_MSG
        RIALTO_LIBRARY RIALTO_INCLUDE_DIR )

mark_as_advanced( RIALTO_INCLUDE_DIR RIALTO_LIBRARY )

if( RIALTO_FOUND )
    set( RIALTO_LIBRARIES ${RIALTO_LIBRARY} )
    set( RIALTO_INCLUDE_DIRS ${RIALTO_INCLUDE_DIR} )
endif()
if( RIALTO_INCLUDE_DIR )
    # Populate RIALTO_INCLUDE_DIRS even if RIALTO_FOUND is false so that
    # dependent targets can still compile, albeit without linking to Rialto.
    # For example, this allows AampRialtoPlayer to be built in L1-test builds
    # where Rialto is mocked and the library is not present.
    set( RIALTO_INCLUDE_DIRS ${RIALTO_INCLUDE_DIR} )
endif()

if( RIALTO_FOUND AND NOT TARGET Rialto::RialtoClient )
    add_library( Rialto::RialtoClient SHARED IMPORTED )
    set_target_properties( Rialto::RialtoClient PROPERTIES
            IMPORTED_LOCATION "${RIALTO_LIBRARY}"
            INTERFACE_INCLUDE_DIRECTORIES "${RIALTO_INCLUDE_DIR}" )
endif()
