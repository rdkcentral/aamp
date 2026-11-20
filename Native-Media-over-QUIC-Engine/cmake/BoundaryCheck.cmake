# Enforces the component boundaries from ARCHITECTURE.md section 13 at configure time:
#  - GStreamer headers only under src/media/gst/
#  - the MOQT library's API only under src/transport/
#  - nothing outside src/replay/ and src/app/ includes capture/replay

file(GLOB_RECURSE NMQE_SOURCES RELATIVE "${CMAKE_CURRENT_SOURCE_DIR}"
	"${CMAKE_CURRENT_SOURCE_DIR}/src/*.h" "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp")

set(NMQE_BOUNDARY_VIOLATIONS "")
foreach(source IN LISTS NMQE_SOURCES)
	file(STRINGS "${CMAKE_CURRENT_SOURCE_DIR}/${source}" includes REGEX "^[ \t]*#[ \t]*include")
	foreach(line IN LISTS includes)
		if(line MATCHES "[<\"]gst/" AND NOT source MATCHES "^src/media/gst/")
			list(APPEND NMQE_BOUNDARY_VIOLATIONS "${source}: GStreamer include outside src/media/gst/")
		endif()
		if(line MATCHES "moq_shim\\.h" AND NOT source MATCHES "^src/transport/")
			list(APPEND NMQE_BOUNDARY_VIOLATIONS "${source}: MOQT library include outside src/transport/")
		endif()
		if(line MATCHES "replay/" AND NOT source MATCHES "^src/(replay|app)/")
			list(APPEND NMQE_BOUNDARY_VIOLATIONS "${source}: capture/replay dependency outside src/replay/ and src/app/")
		endif()
	endforeach()
endforeach()

if(NMQE_BOUNDARY_VIOLATIONS)
	list(JOIN NMQE_BOUNDARY_VIOLATIONS "\n  " details)
	message(FATAL_ERROR "Architecture boundary violations:\n  ${details}")
endif()
