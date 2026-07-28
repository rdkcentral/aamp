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

# Middleware headers - use external middleware-player-interface if available via pkg-config
# Otherwise fall back to middleware-player-interface repo (GitHub Actions) or internal aamp/middleware
# Note: .libs/include is NOT used because it contains gtest headers that conflict with FetchContent
if(PLAYERFBINTERFACE_INCLUDE_DIRS)
	# External middleware via pkg-config (preferred for installed middleware)
	message(STATUS "Using external middleware-player-interface via pkg-config")
	include_directories(${PLAYERFBINTERFACE_INCLUDE_DIRS})
	include_directories(${BASECONVERSION_INCLUDE_DIRS})
	include_directories(${PLAYERLOGMANAGER_INCLUDE_DIRS})
	include_directories(${SUBTEC_INCLUDE_DIRS})
elseif(EXISTS ${AAMP_ROOT}/../middleware-player-interface)
	# Middleware from sibling directory (GitHub Actions CI clones it here)
	message(STATUS "Using middleware-player-interface from sibling directory (CI build)")
	include_directories(${AAMP_ROOT}/../middleware-player-interface)
	include_directories(${AAMP_ROOT}/../middleware-player-interface/drm)
	include_directories(${AAMP_ROOT}/../middleware-player-interface/drm/aes)
	include_directories(${AAMP_ROOT}/../middleware-player-interface/externals)
	include_directories(${AAMP_ROOT}/../middleware-player-interface/vendor)
	include_directories(${AAMP_ROOT}/../middleware-player-interface/playerLogManager)
	include_directories(${AAMP_ROOT}/../middleware-player-interface/baseConversion)
	include_directories(${AAMP_ROOT}/../middleware-player-interface/externals/contentsecuritymanager)
	include_directories(${AAMP_ROOT}/../middleware-player-interface/closedcaptions)
	include_directories(${AAMP_ROOT}/../middleware-player-interface/playerisobmff)
	include_directories(${AAMP_ROOT}/../middleware-player-interface/subtitle)
	include_directories(${AAMP_ROOT}/../middleware-player-interface/subtec/subtecparser)
	include_directories(${AAMP_ROOT}/../middleware-player-interface/subtec/libsubtec)
	include_directories(${AAMP_ROOT}/../middleware-player-interface/playerjsonobject)
else()
	# Internal middleware paths (for local builds with aamp/middleware folder)
	message(STATUS "Using internal aamp/middleware paths (deprecated)")
	include_directories(${AAMP_ROOT}/middleware)
	include_directories(${AAMP_ROOT}/middleware/drm)
	include_directories(${AAMP_ROOT}/middleware/drm/aes)
	include_directories(${AAMP_ROOT}/middleware/externals)
	include_directories(${AAMP_ROOT}/middleware/vendor)
	include_directories(${AAMP_ROOT}/middleware/playerLogManager)
	include_directories(${AAMP_ROOT}/middleware/baseConversion)
	include_directories(${AAMP_ROOT}/middleware/externals/contentsecuritymanager)
	include_directories(${AAMP_ROOT}/middleware/closedcaptions)
	include_directories(${AAMP_ROOT}/middleware/playerisobmff)
	include_directories(${AAMP_ROOT}/middleware/subtitle)
	include_directories(${AAMP_ROOT}/middleware/subtec/subtecparser)
	include_directories(${AAMP_ROOT}/middleware/subtec/libsubtec)
	include_directories(${AAMP_ROOT}/middleware/playerjsonobject)
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
