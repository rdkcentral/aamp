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
                    ${AAMP_ROOT}/direct-rialto
                    ${AAMP_ROOT}/.libs/include/rialto
                    ${AAMP_ROOT}/drm 
                    ${AAMP_ROOT}/drm/helper 
                    ${AAMP_ROOT}/downloader 
                    ${AAMP_ROOT}/subtitle
                    ${AAMP_ROOT}/tsb/api
                    ${AAMP_ROOT}/isobmff
                    ${AAMP_ROOT}/subtec/subtecparser
                    ${AAMP_ROOT}/abr
                    ${AAMP_ROOT}/mp4demux)

include_directories(BEFORE ${GTEST_INCLUDE_DIRS})
include_directories(BEFORE ${GMOCK_INCLUDE_DIRS})
include_directories(${GLIB_INCLUDE_DIRS})
include_directories(${GSTREAMER_INCLUDE_DIRS})
include_directories(${LIBDASH_INCLUDE_DIRS})
include_directories(${LIBCJSON_INCLUDE_DIRS})
include_directories(${LibXml2_INCLUDE_DIRS})
include_directories(SYSTEM ${UTESTS_ROOT}/mocks)

# Middleware headers - use external middleware-player-interface via pkg-config
# These variables are set in parent test/utests/CMakeLists.txt
# Fall back to middleware-player-interface repo cloned alongside aamp (GitHub Actions CI)
if(PLAYERFBINTERFACE_INCLUDE_DIRS)
	# External middleware via pkg-config (preferred for installed middleware)
	include_directories(${PLAYERFBINTERFACE_INCLUDE_DIRS})
	include_directories(${BASECONVERSION_INCLUDE_DIRS})
	include_directories(${PLAYERLOGMANAGER_INCLUDE_DIRS})
	include_directories(${SUBTEC_INCLUDE_DIRS})
elseif(EXISTS "${MIDDLEWARE_REPO_ABS_PATH}")
	# Middleware from middleware-player-interface repo (GitHub Actions CI clones it here).
	# Use MIDDLEWARE_REPO_ABS_PATH (set in parent CMakeLists.txt) — always an absolute path,
	# avoiding the fragile relative EXISTS ${AAMP_ROOT}/... resolution.
	include_directories(${MIDDLEWARE_REPO_ABS_PATH}
	                    ${MIDDLEWARE_REPO_ABS_PATH}/playerisobmff
	                    ${MIDDLEWARE_REPO_ABS_PATH}/subtitle
	                    ${MIDDLEWARE_REPO_ABS_PATH}/subtec/subtecparser
	                    ${MIDDLEWARE_REPO_ABS_PATH}/subtec/libsubtec
	                    ${MIDDLEWARE_REPO_ABS_PATH}/playerjsonobject
	                    ${MIDDLEWARE_REPO_ABS_PATH}/closedcaptions
	                    ${MIDDLEWARE_REPO_ABS_PATH}/closedcaptions/direct-rialto
	                    ${MIDDLEWARE_REPO_ABS_PATH}/drm
	                    ${MIDDLEWARE_REPO_ABS_PATH}/drm/helper
	                    ${MIDDLEWARE_REPO_ABS_PATH}/externals
	                    ${MIDDLEWARE_REPO_ABS_PATH}/externals/contentsecuritymanager
	                    ${MIDDLEWARE_REPO_ABS_PATH}/baseConversion
	                    ${MIDDLEWARE_REPO_ABS_PATH}/playerLogManager
	                    ${MIDDLEWARE_REPO_ABS_PATH}/vendor)
else()
	message(FATAL_ERROR
		"middleware-player-interface headers not found.\n"
		"Either install the middleware-player-interface package (provides pkg-config 'playerfbinterface') "
		"or clone the middleware-player-interface repository as a sibling of the aamp directory so that "
		"MIDDLEWARE_REPO_ABS_PATH resolves correctly.\n"
		"Searched: ${MIDDLEWARE_REPO_ABS_PATH}")
endif()

# std::atomic<ABRManager::PersistBandwidthData> is 16 bytes.  On Linux/x86_64
# GCC emits __atomic_load_16 / __atomic_store_16 requiring the atomic support
# library, which GCC always ships for its target arch.
if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    set(OS_LD_FLAGS ${OS_LD_FLAGS} "-latomic")
endif()
