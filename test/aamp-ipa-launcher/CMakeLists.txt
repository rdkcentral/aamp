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

cmake_minimum_required(VERSION 3.14)
project(aamp-ipa-launcher CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(Threads REQUIRED)
find_package(PkgConfig REQUIRED)

# ── Dependencies ─────────────────────────────────────────────────────────────

# GLib (needed for PlayerInstanceAAMP / GStreamer main loop)
pkg_check_modules(GLIB REQUIRED glib-2.0)

# GStreamer (needed by libaamp internals)
pkg_check_modules(GSTREAMER REQUIRED gstreamer-1.0)

# jsoncpp — used directly for Json::Value in all RPC code
pkg_check_modules(JSONCPP REQUIRED jsoncpp)

# rpcserver — WebSocket JSON-RPC 2.0 server library (websocketpp + libjsonrpccpp)
# Option A: point RPCSERVER_DIR at the rpcserver source tree for a local build.
# Option B: leave unset and rely on an installed librpcserver.so (Yocto package).
if(DEFINED RPCSERVER_DIR)
	message(STATUS "Using rpcserver source from ${RPCSERVER_DIR}")
	add_subdirectory(${RPCSERVER_DIR} ${CMAKE_BINARY_DIR}/rpcserver EXCLUDE_FROM_ALL)
	set(RPCSERVER_TARGET rpcserver)
	set(RPCSERVER_INCLUDE_DIRS ${RPCSERVER_DIR}/include)
else()
	find_library(RPCSERVER_LIB NAMES rpcserver REQUIRED)
	add_library(rpcserver_imported SHARED IMPORTED)
	set_target_properties(rpcserver_imported PROPERTIES IMPORTED_LOCATION ${RPCSERVER_LIB})
	set(RPCSERVER_TARGET rpcserver_imported)
	# Headers installed to <prefix>/include/rpcserver/
	find_path(RPCSERVER_INCLUDE_DIR
		NAMES rpcserver/IAbstractRpcServer.h
		PATH_SUFFIXES include
		REQUIRED)
	set(RPCSERVER_INCLUDE_DIRS ${RPCSERVER_INCLUDE_DIR})
endif()

# ── libaamp ──────────────────────────────────────────────────────────────────
# When building standalone against an installed AAMP, locate the shared lib.
# When building inside the AAMP source tree, use the CMake target directly.
if(TARGET aamp)
	set(LIBAAMP_TARGET aamp)
else()
	find_library(LIBAAMP_LIB NAMES aamp REQUIRED)
	add_library(aamp_imported SHARED IMPORTED)
	set_target_properties(aamp_imported PROPERTIES IMPORTED_LOCATION ${LIBAAMP_LIB})
	set(LIBAAMP_TARGET aamp_imported)
endif()

# ── Source files ─────────────────────────────────────────────────────────────
set(SOURCES
	main.cpp
	AampRpcServer.cpp
	AampRpcHandlers.cpp
	AampRpcEventListener.cpp
)

add_executable(aamp-ipa-launcher ${SOURCES})

# ── Include paths ─────────────────────────────────────────────────────────────
# AAMP public headers live in the repo root (../../ from this directory).
get_filename_component(AAMP_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../../" ABSOLUTE)

target_include_directories(aamp-ipa-launcher PRIVATE
	${CMAKE_CURRENT_SOURCE_DIR}
	${AAMP_ROOT}
	${GLIB_INCLUDE_DIRS}
	${GSTREAMER_INCLUDE_DIRS}
	${JSONCPP_INCLUDE_DIRS}
	${RPCSERVER_INCLUDE_DIRS}
)

# ── Link ──────────────────────────────────────────────────────────────────────
target_link_libraries(aamp-ipa-launcher PRIVATE
	${LIBAAMP_TARGET}
	${RPCSERVER_TARGET}
	${JSONCPP_LIBRARIES}
	${GLIB_LIBRARIES}
	${GSTREAMER_LIBRARIES}
	Threads::Threads
)

# ── Compiler flags ────────────────────────────────────────────────────────────
target_compile_options(aamp-ipa-launcher PRIVATE
	-Wall
	-Wextra
	-Wno-unused-parameter
)

# ── Install ───────────────────────────────────────────────────────────────────
install(TARGETS aamp-ipa-launcher RUNTIME DESTINATION bin)
install(FILES test_client.html DESTINATION share/aamp-ipa-launcher)
