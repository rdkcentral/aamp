# Copyright (C) 2022 Sky UK
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

# - Try to find the Rialto library.
#
# The following are set after configuration is done:
#  RIALTO_FOUND
#  RIALTO_INCLUDE_DIRS
#  RIALTO_LIBRARY_DIRS
#  RIALTO_LIBRARIES
find_library( RIALTO_LIBRARY NAMES libRialtoClient.so RialtoClient )
message( "FindRialto: RIALTO_LIBRARY = ${RIALTO_LIBRARY}" )

find_path( RIALTO_INCLUDE_DIR NAMES IMediaPipeline.h PATH_SUFFIXES rialto)
message( "FindRialto: RIALTO_INCLUDE_DIR (initial find_path) = ${RIALTO_INCLUDE_DIR}" )

# Fallback for cross-compilation environments (e.g. Yocto) where find_path
# may not search the sysroot even when find_library succeeds.
# Rialto always installs headers to <prefix>/include/rialto/, so derive the
# include path from the library location.
if(NOT RIALTO_INCLUDE_DIR AND RIALTO_LIBRARY)
    get_filename_component(_rialto_lib_dir "${RIALTO_LIBRARY}" DIRECTORY)
    get_filename_component(_rialto_prefix "${_rialto_lib_dir}" DIRECTORY)
    message( "FindRialto: falling back to prefix-derived search in ${_rialto_prefix}/include" )
    find_path(RIALTO_INCLUDE_DIR NAMES IMediaPipeline.h
        PATHS "${_rialto_prefix}/include"
        PATH_SUFFIXES rialto
        NO_DEFAULT_PATH)
    message( "FindRialto: RIALTO_INCLUDE_DIR (fallback) = ${RIALTO_INCLUDE_DIR}" )
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
