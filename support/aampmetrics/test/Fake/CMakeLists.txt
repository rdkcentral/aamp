# If not stated otherwise in this file or this component's license file the
# following copyright and licenses apply:
#
# Copyright 2024 RDK Management
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

set(AAMP_ROOT "../../")
set(UTESTS_ROOT "..")


include_directories(${AAMP_ROOT} ${AAMP_ROOT}/isobmff ${AAMP_ROOT}/drm ${AAMP_ROOT}/downloader ${AAMP_ROOT}/drm/helper ${AAMP_ROOT}/drm/ave ${AAMP_ROOT}/subtitle)

# Middleware headers from external middleware-player-interface
pkg_check_modules(SUBTEC_PKG QUIET libsubtec)
if(SUBTEC_PKG_INCLUDE_DIRS)
    include_directories(${SUBTEC_PKG_INCLUDE_DIRS})
else()
    # Fallback to sibling middleware-player-interface repo
    get_filename_component(_MW_REPO "${AAMP_ROOT}/../middleware-player-interface" ABSOLUTE)
    if(EXISTS "${_MW_REPO}")
        include_directories(${_MW_REPO}/subtec/libsubtec)
        include_directories(${_MW_REPO}/subtec/subtecparser)
        include_directories(${_MW_REPO}/playerjsonobject)
    else()
        # Legacy .libs/include fallback
        include_directories(${AAMP_ROOT}/.libs/include)
    endif()
endif()
include_directories(${AAMP_ROOT}/test/aampcli)
include_directories(${AAMP_ROOT}/dash/xml ${AAMP_ROOT}/dash/utils ${AAMP_ROOT}/dash/mpd)
include_directories(${GLIB_INCLUDE_DIRS})
include_directories(${GSTREAMER_INCLUDE_DIRS})
include_directories(${LibXml2_INCLUDE_DIRS})
include_directories(${LIBDASH_INCLUDE_DIRS})

# Mac OS X
if(CMAKE_SYSTEM_NAME STREQUAL Darwin)
    include_directories(/usr/local/include)
    include_directories(/usr/local/include/libdash)
else()
    include_directories(${AAMP_ROOT}/Linux/include)
    include_directories(${AAMP_ROOT}/Linux/include/libdash)
endif(CMAKE_SYSTEM_NAME STREQUAL Darwin)

#include_directories(${UTESTS_ROOT}/mocks)

add_library(Fake ${fakes_SRC})
set_target_properties(Fake PROPERTIES FOLDER "test")
set_target_properties(Fake PROPERTIES COMPILE_FLAGS "-Wno-effc++ -Wno-varargs")
