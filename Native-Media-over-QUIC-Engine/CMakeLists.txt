cmake_minimum_required(VERSION 3.21)
project(NativeMediaOverQuicEngine VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
if(NOT CMAKE_BUILD_TYPE)
	set(CMAKE_BUILD_TYPE RelWithDebInfo)
endif()

option(NMQE_BUILD_TESTS "Build unit and integration tests" ON)
set(NMQE_AAMP_DIR "${CMAKE_CURRENT_SOURCE_DIR}/.." CACHE PATH "AAMP checkout providing mp4demux")
set(NMQE_MIDDLEWARE_DIR "${NMQE_AAMP_DIR}/../middleware-player-interface" CACHE PATH "middleware-player-interface checkout (DemuxDataTypes.h)")
set(NMQE_RUST_TARGET "" CACHE STRING "Rust target triple for cross builds; empty = host")

find_package(PkgConfig REQUIRED)
pkg_check_modules(GST REQUIRED IMPORTED_TARGET gstreamer-1.0>=1.18 gstreamer-app-1.0 gstreamer-base-1.0)
pkg_check_modules(CJSON REQUIRED IMPORTED_TARGET libcjson)
find_package(Threads REQUIRED)

foreach(required "${NMQE_AAMP_DIR}/mp4demux/MP4Demux.cpp" "${NMQE_MIDDLEWARE_DIR}/DemuxDataTypes.h")
	if(NOT EXISTS "${required}")
		message(FATAL_ERROR "Missing ${required}; set NMQE_AAMP_DIR / NMQE_MIDDLEWARE_DIR")
	endif()
endforeach()

add_compile_options(-Wall -Wextra -Wno-unused-parameter)

# ---- transport: selected MOQT library (moq-dev/moq) behind a C ABI -------------------------------
include(cmake/MoqShim.cmake)

# ---- libraries, one per architectural component ----------------------------------------------------
add_library(nmqe_common STATIC
	src/common/Base64.cpp
	src/common/Config.cpp
	src/common/EventLoop.cpp
	src/common/Log.cpp
	src/common/ProcessStats.cpp
	src/common/Telemetry.cpp)
target_include_directories(nmqe_common PUBLIC src)
target_link_libraries(nmqe_common PUBLIC Threads::Threads PRIVATE PkgConfig::CJSON)

add_library(nmqe_catalog STATIC src/catalog/Catalog.cpp)
target_link_libraries(nmqe_catalog PUBLIC nmqe_common PRIVATE PkgConfig::CJSON)

add_library(nmqe_media_api STATIC src/media/api/MediaPipeline.cpp)
target_link_libraries(nmqe_media_api PUBLIC nmqe_common)

# CMAF parsing reuses AAMP's mp4demux, compiled standalone: aamp_overrides stands in for AAMP's
# logging/defines and fences off GStreamer so this layer stays above the media boundary.
add_library(nmqe_mediaformat STATIC
	src/mediaformat/CmafTrackParser.cpp
	"${NMQE_AAMP_DIR}/mp4demux/MP4Demux.cpp")
target_include_directories(nmqe_mediaformat
	PRIVATE src/mediaformat/aamp_overrides "${NMQE_AAMP_DIR}/mp4demux" "${NMQE_AAMP_DIR}")
target_compile_definitions(nmqe_mediaformat
	PRIVATE NMQE_MIDDLEWARE_DEMUX_DATA_TYPES="${NMQE_MIDDLEWARE_DIR}/DemuxDataTypes.h")
target_link_libraries(nmqe_mediaformat PUBLIC nmqe_media_api)
set_source_files_properties("${NMQE_AAMP_DIR}/mp4demux/MP4Demux.cpp" PROPERTIES COMPILE_OPTIONS "-w")
# middleware-player-interface's DemuxDataTypes.h is third-party to this project.
set_source_files_properties(src/mediaformat/CmafTrackParser.cpp
	PROPERTIES COMPILE_OPTIONS "-Wno-missing-field-initializers;-Wno-missing-braces;-Wno-reorder-ctor")

add_library(nmqe_object_source STATIC src/transport/ObjectSource.cpp)
target_link_libraries(nmqe_object_source PUBLIC nmqe_common)

add_library(nmqe_transport_moq STATIC src/transport/MoqObjectSource.cpp)
target_link_libraries(nmqe_transport_moq PUBLIC nmqe_object_source PRIVATE moq_shim)

add_library(nmqe_replay STATIC src/replay/Capture.cpp)
target_link_libraries(nmqe_replay PUBLIC nmqe_object_source)

add_library(nmqe_media STATIC
	src/media/MediaPipelineFactory.cpp
	src/media/null/NullMediaPipeline.cpp
	src/media/gst/GstMediaPipeline.cpp)
target_link_libraries(nmqe_media PUBLIC nmqe_media_api PRIVATE PkgConfig::GST)

add_library(nmqe_player STATIC src/player/BoundedTrackQueue.cpp src/player/Player.cpp)
target_link_libraries(nmqe_player PUBLIC nmqe_catalog nmqe_mediaformat nmqe_media_api nmqe_object_source)

add_executable(nmqe-player src/app/main.cpp)
target_link_libraries(nmqe-player PRIVATE nmqe_player nmqe_media nmqe_transport_moq nmqe_replay)

# ---- boundary checks (architecture rules enforced at configure time) --------------------------------
include(cmake/BoundaryCheck.cmake)

if(NMQE_BUILD_TESTS)
	enable_testing()
	add_subdirectory(tests)
endif()
