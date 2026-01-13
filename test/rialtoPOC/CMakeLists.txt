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

set(TEST_ROOT "../")

if (NOT CMAKE_PLATFORM_UBUNTU AND NOT CMAKE_SYSTEM_NAME STREQUAL Darwin AND CMAKE_BUILD_RIALTO_POC)
	# Rialto POC 2
	set(RIALTOPOC2_SOURCES
		${TEST_ROOT}/gstTestHarness/mp4demux.hpp
		rialto-pipeline2.cpp
		rialto-pipeline2.h
		rialto-test2.cpp
	)

	find_path( RIALTO_INCLUDE_DIR NAMES IMediaPipeline.h PATH_SUFFIXES rialto)
	find_library( RIALTO_LIBRARY NAMES libRialtoClient.so RialtoClient )
	include( FindPackageHandleStandardArgs )
	find_package_handle_standard_args( RIALTO DEFAULT_MSG
			RIALTO_LIBRARY RIALTO_INCLUDE_DIR )

	mark_as_advanced(
		RIALTO_INCLUDE_DIR
		RIALTO_LIBRARY
		RIALTO_GSTREAMER_INCLUDE_DIR
	)

	if( RIALTO_FOUND )
		set( RIALTO_LIBRARIES ${RIALTO_LIBRARY} )
		set(RIALTO_INCLUDE_DIRS
			${RIALTO_INCLUDE_DIR}
			${RIALTO_GSTREAMER_INCLUDE_DIR}
	)
	endif()

	if( RIALTO_FOUND AND NOT TARGET Rialto::RialtoClient )
		add_library( Rialto::RialtoClient SHARED IMPORTED )
		set_target_properties( Rialto::RialtoClient PROPERTIES
				IMPORTED_LOCATION "${RIALTO_LIBRARY}"
				INTERFACE_INCLUDE_DIRECTORIES "${RIALTO_INCLUDE_DIR}" )
	endif()

	add_executable(rialtoPOC2 ${RIALTOPOC2_SOURCES})
	target_link_libraries(rialtoPOC2 ${RIALTO_LIBRARIES} "-lreadline" Rialto::RialtoClient)
    install(TARGETS rialtoPOC2
		DESTINATION lib
    )

	# Rialto POC 3
	set(RIALTOPOC3_SOURCES
		${TEST_ROOT}/gstTestHarness/mp4demux.hpp
		rialto-pipeline3.cpp
		rialto-pipeline3.h
		rialto-test3.cpp
	)

	find_path( RIALTO_INCLUDE_DIR NAMES IMediaPipeline.h PATH_SUFFIXES rialto)
	find_library( RIALTO_LIBRARY NAMES libRialtoClient.so RialtoClient )
	include( FindPackageHandleStandardArgs )
	find_package_handle_standard_args( RIALTO DEFAULT_MSG
			RIALTO_LIBRARY RIALTO_INCLUDE_DIR )

	mark_as_advanced(
		RIALTO_INCLUDE_DIR
		RIALTO_LIBRARY
		RIALTO_GSTREAMER_INCLUDE_DIR
	)

	if( RIALTO_FOUND )
		set( RIALTO_LIBRARIES ${RIALTO_LIBRARY} )
		set(RIALTO_INCLUDE_DIRS
			${RIALTO_INCLUDE_DIR}
			${RIALTO_GSTREAMER_INCLUDE_DIR}
	)
	endif()

	if( RIALTO_FOUND AND NOT TARGET Rialto::RialtoClient )
		add_library( Rialto::RialtoClient SHARED IMPORTED )
		set_target_properties( Rialto::RialtoClient PROPERTIES
				IMPORTED_LOCATION "${RIALTO_LIBRARY}"
				INTERFACE_INCLUDE_DIRECTORIES "${RIALTO_INCLUDE_DIR}" )
	endif()

	add_executable(rialtoPOC3 ${RIALTOPOC3_SOURCES})
	target_link_libraries(rialtoPOC3 ${RIALTO_LIBRARIES} "-lreadline" Rialto::RialtoClient)
    install(TARGETS rialtoPOC3
		DESTINATION lib
    )
endif()