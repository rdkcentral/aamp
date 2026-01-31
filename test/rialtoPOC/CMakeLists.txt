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

find_package(Threads REQUIRED)
pkg_check_modules(CURL REQUIRED libcurl)
pkg_check_modules(LIBDASH REQUIRED libdash)
pkg_check_modules(GSTREAMER REQUIRED gstreamer-1.0>=1.18)
pkg_check_modules(GSTREAMERBASE REQUIRED gstreamer-base-1.0)
pkg_check_modules(GSTAPP REQUIRED gstreamer-app-1.0)
pkg_check_modules(GSTVIDEO REQUIRED gstreamer-video-1.0)
pkg_check_modules(GLIB REQUIRED glib-2.0)

include_directories(
    ${CURL_INCLUDE_DIRS}
    ${LIBDASH_INCLUDE_DIRS}
    ${GSTREAMER_INCLUDE_DIRS}
    ${GSTREAMERBASE_INCLUDE_DIRS}
    ${GSTAPP_INCLUDE_DIRS}
    ${GSTVIDEO_INCLUDE_DIRS}
    ${GLIB_INCLUDE_DIRS}
)

set(rialtoPOC_DEPENDS
	${OS_LD_FLAGS}
    ${CMAKE_THREAD_LIBS_INIT}
    ${CURL_LIBRARIES}
    ${LIBDASH_LIBRARIES}
    ${GSTREAMER_LIBRARIES}
    ${GSTREAMERBASE_LIBRARIES}
    ${GSTAPP_LIBRARIES}
    ${GSTVIDEO_LIBRARIES}
    ${GLIB_LIBRARIES}
)

set(TEST_ROOT "../")

if (NOT CMAKE_PLATFORM_UBUNTU AND NOT CMAKE_SYSTEM_NAME STREQUAL Darwin AND CMAKE_BUILD_RIALTO_POC)
	# Rialto POC 2
	set(RIALTOPOC2_SOURCES
		${TEST_ROOT}/gstTestHarness/Mp4DemuxAdapter.cpp
		${TEST_ROOT}/gstTestHarness/Mp4DemuxAdapter.h
		${AAMP_ROOT}/mp4demux/MP4Demux.cpp
		${AAMP_ROOT}/mp4demux/MP4Demux.h
		${AAMP_ROOT}/middleware/DemuxDataTypes.h
		${AAMP_ROOT}/AampDemuxDataTypes.h
		${AAMP_ROOT}/AampGrowableBuffer.cpp
		${AAMP_ROOT}/AampGrowableBuffer.h
		${AAMP_ROOT}/middleware/GstUtils.cpp
		${AAMP_ROOT}/middleware/GstUtils.h
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
	target_link_libraries(rialtoPOC2 ${RIALTO_LIBRARIES} ${rialtoPOC_DEPENDS} "-lreadline" Rialto::RialtoClient)
    install(TARGETS rialtoPOC2
		DESTINATION bin
    )

	# Rialto POC 3
	set(RIALTOPOC3_SOURCES
		${TEST_ROOT}/gstTestHarness/Mp4DemuxAdapter.cpp
		${TEST_ROOT}/gstTestHarness/Mp4DemuxAdapter.h
		${AAMP_ROOT}/mp4demux/MP4Demux.cpp
		${AAMP_ROOT}/mp4demux/MP4Demux.h
		${AAMP_ROOT}/middleware/DemuxDataTypes.h
		${AAMP_ROOT}/AampDemuxDataTypes.h
		${AAMP_ROOT}/AampGrowableBuffer.cpp
		${AAMP_ROOT}/AampGrowableBuffer.h
		${AAMP_ROOT}/middleware/GstUtils.cpp
		${AAMP_ROOT}/middleware/GstUtils.h
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
	target_link_libraries(rialtoPOC3 ${RIALTO_LIBRARIES} ${rialtoPOC_DEPENDS} "-lreadline" Rialto::RialtoClient)
    install(TARGETS rialtoPOC3
		DESTINATION bin
    )
endif()