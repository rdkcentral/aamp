# If not stated otherwise in this file or this component's license file the
# following copyright and licenses apply:
#
# Copyright 2025 RDK Management
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

# Common include directories for all unit tests
# Usage: include(${CMAKE_CURRENT_LIST_DIR}/../CommonTestIncludes.cmake)
# Assumes AAMP_ROOT and UTESTS_ROOT are set before including this file

include_directories(${AAMP_ROOT} 
                    ${AAMP_ROOT}/drm 
                    ${AAMP_ROOT}/drm/helper 
                    ${AAMP_ROOT}/downloader 
                    ${AAMP_ROOT}/subtitle
                    ${AAMP_ROOT}/tsb/api
                    ${AAMP_ROOT}/isobmff
                    ${AAMP_ROOT}/subtec/subtecparser
                    ${AAMP_ROOT}/abr
                    ${AAMP_ROOT}/mp4demux)

include_directories(${GTEST_INCLUDE_DIRS})
include_directories(${GMOCK_INCLUDE_DIRS})
include_directories(${GLIB_INCLUDE_DIRS})
include_directories(${GSTREAMER_INCLUDE_DIRS})
include_directories(${LIBDASH_INCLUDE_DIRS})
include_directories(${LIBCJSON_INCLUDE_DIRS})
include_directories(${LibXml2_INCLUDE_DIRS})
include_directories(SYSTEM ${UTESTS_ROOT}/mocks)

# Middleware headers - use external middleware-player-interface via pkg-config
# These variables are set in parent test/utests/CMakeLists.txt
# For legacy builds without external middleware, fall back to internal paths
if(PLAYERFBINTERFACE_INCLUDE_DIRS)
	# External middleware (preferred)
	include_directories(${PLAYERFBINTERFACE_INCLUDE_DIRS})
	include_directories(${BASECONVERSION_INCLUDE_DIRS})
	include_directories(${PLAYERLOGMANAGER_INCLUDE_DIRS})
	include_directories(${SUBTEC_INCLUDE_DIRS})
elseif(EXISTS ${AAMP_ROOT}/middleware)
	# Internal middleware paths (deprecated, for legacy builds only)
	include_directories(${AAMP_ROOT}/middleware
	                    ${AAMP_ROOT}/middleware/playerisobmff
	                    ${AAMP_ROOT}/middleware/subtitle
	                    ${AAMP_ROOT}/middleware/subtec/subtecparser
	                    ${AAMP_ROOT}/middleware/subtec/libsubtec
	                    ${AAMP_ROOT}/middleware/playerjsonobject
	                    ${AAMP_ROOT}/middleware/closedcaptions
	                    ${AAMP_ROOT}/middleware/drm
	                    ${AAMP_ROOT}/middleware/externals
	                    ${AAMP_ROOT}/middleware/externals/contentsecuritymanager
	                    ${AAMP_ROOT}/middleware/baseConversion
	                    ${AAMP_ROOT}/middleware/playerLogManager
	                    ${AAMP_ROOT}/middleware/vendor)
else()
	message(FATAL_ERROR "Middleware headers not found! Need either external middleware-player-interface via pkg-config or internal aamp/middleware directory.")
endif()

# std::atomic<ABRManager::PersistBandwidthData> is 16 bytes.  On Linux/x86_64
# GCC emits __atomic_load_16 / __atomic_store_16 requiring the atomic support
# library, which GCC always ships for its target arch.
if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    set(OS_LD_FLAGS ${OS_LD_FLAGS} "-latomic")
endif()
