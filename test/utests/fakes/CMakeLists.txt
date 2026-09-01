# If not stated otherwise in this file or this component's license file the
# following copyright and licenses apply:
#
# Copyright 2022 RDK Management
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

file(GLOB fakes_SRC "*.cpp")

set(AAMP_ROOT "../../../")
set(UTESTS_ROOT "..")

# Middleware headers - use external middleware-player-interface via pkg-config
# Otherwise fall back to middleware-player-interface repo (GitHub Actions CI clones it here)
# Note: .libs/include is NOT used because it contains gtest headers that conflict with FetchContent
if(PLAYERFBINTERFACE_INCLUDE_DIRS)
	# External middleware via pkg-config (preferred for installed middleware)
	message(STATUS "Using external middleware-player-interface via pkg-config")
	include_directories(${PLAYERFBINTERFACE_INCLUDE_DIRS})
	include_directories(${BASECONVERSION_INCLUDE_DIRS})
	include_directories(${PLAYERLOGMANAGER_INCLUDE_DIRS})
	include_directories(${SUBTEC_INCLUDE_DIRS})
elseif(EXISTS "${MIDDLEWARE_REPO_ABS_PATH}")
	# Middleware from middleware-player-interface repo (GitHub Actions CI clones it here).
	# Use MIDDLEWARE_REPO_ABS_PATH set in parent CMakeLists.txt — always absolute.
	message(STATUS "Using middleware-player-interface from aamp directory (CI build)")
	include_directories(${MIDDLEWARE_REPO_ABS_PATH})
	include_directories(${MIDDLEWARE_REPO_ABS_PATH}/drm)
	include_directories(${MIDDLEWARE_REPO_ABS_PATH}/drm/helper)
	include_directories(${MIDDLEWARE_REPO_ABS_PATH}/drm/aes)
	include_directories(${MIDDLEWARE_REPO_ABS_PATH}/externals)
	include_directories(${MIDDLEWARE_REPO_ABS_PATH}/vendor)
	include_directories(${MIDDLEWARE_REPO_ABS_PATH}/playerLogManager)
	include_directories(${MIDDLEWARE_REPO_ABS_PATH}/baseConversion)
	include_directories(${MIDDLEWARE_REPO_ABS_PATH}/externals/contentsecuritymanager)
	include_directories(${MIDDLEWARE_REPO_ABS_PATH}/closedcaptions)
	include_directories(${MIDDLEWARE_REPO_ABS_PATH}/playerisobmff)
	include_directories(${MIDDLEWARE_REPO_ABS_PATH}/subtitle)
	include_directories(${MIDDLEWARE_REPO_ABS_PATH}/subtec/subtecparser)
	include_directories(${MIDDLEWARE_REPO_ABS_PATH}/subtec/libsubtec)
	include_directories(${MIDDLEWARE_REPO_ABS_PATH}/playerjsonobject)
else()
	message(FATAL_ERROR "middleware-player-interface not found. Either install via pkg-config or clone the repo alongside the aamp directory. See: https://github.com/rdkcentral/middleware-player-interface")
endif()

# AAMP source directories
include_directories(${AAMP_ROOT})
include_directories(${AAMP_ROOT}/isobmff)
include_directories(${AAMP_ROOT}/drm)
include_directories(${AAMP_ROOT}/downloader)
include_directories(${AAMP_ROOT}/drm/helper)
include_directories(${AAMP_ROOT}/subtitle)
include_directories(${AAMP_ROOT}/subtec/subtecparser)
include_directories(${AAMP_ROOT}/test/aampcli)
include_directories(${AAMP_ROOT}/dash/xml)
include_directories(${AAMP_ROOT}/dash/utils)
include_directories(${AAMP_ROOT}/dash/mpd)
include_directories(${AAMP_ROOT}/tsb/api)
include_directories(${AAMP_ROOT}/abr)
include_directories(${AAMP_ROOT}/mp4demux)
include_directories(${AAMP_ROOT}/direct-rialto)
# Rialto client headers only; .libs/include itself is excluded (gtest clash)
include_directories(${AAMP_ROOT}/.libs/include/rialto)
include_directories(${CMAKE_CURRENT_SOURCE_DIR})

# Test framework directories
include_directories(${UTESTS_ROOT}/ocdm)
include_directories(${UTESTS_ROOT}/mocks)

# Third-party dependencies
include_directories(${GMOCK_INCLUDE_DIRS})
include_directories(${GLIB_INCLUDE_DIRS})
include_directories(${GSTREAMER_INCLUDE_DIRS})
include_directories(${LIBCJSON_INCLUDE_DIRS})
include_directories(${LibXml2_INCLUDE_DIRS})
include_directories(${LIBDASH_INCLUDE_DIRS})

if (JSC_INCDIR)
    include_directories(${JSC_INCDIR})
endif()

# Mac OS X
if(CMAKE_SYSTEM_NAME STREQUAL Darwin)
    if(EXISTS ${AAMP_ROOT}/.libs/systemd/src)
        include_directories(${AAMP_ROOT}/.libs/systemd/src)
    endif()
endif(CMAKE_SYSTEM_NAME STREQUAL Darwin)

add_library(fakes ${fakes_SRC} ${middleware_fakes_SRC})
set_target_properties(fakes PROPERTIES FOLDER "utests")
set_target_properties(fakes PROPERTIES COMPILE_FLAGS "-Wno-effc++ -Wno-varargs")
