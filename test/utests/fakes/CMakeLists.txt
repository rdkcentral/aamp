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
file(GLOB middleware_fakes_SRC "../../../middleware/test/utests/fakes/*.cpp")

set(AAMP_ROOT "../../../")
set(UTESTS_ROOT "..")



include_directories(${AAMP_ROOT} ${AAMP_ROOT}/isobmff ${AAMP_ROOT}/middleware/playerisobmff ${AAMP_ROOT}/drm ${AAMP_ROOT}/downloader ${AAMP_ROOT}/drm/helper ${AAMP_ROOT}/subtitle ${AAMP_ROOT}/middleware/subtitle)

include_directories(${AAMP_ROOT}/subtec/subtecparser)
include_directories(${AAMP_ROOT}/middleware/subtec/libsubtec)
include_directories(${AAMP_ROOT}/middleware/subtec/subtecparser)
include_directories(${AAMP_ROOT}/middleware/playerjsonobject)
include_directories(${AAMP_ROOT}/test/aampcli)
include_directories(${AAMP_ROOT}/dash/xml ${AAMP_ROOT}/dash/utils ${AAMP_ROOT}/dash/mpd)
include_directories(${AAMP_ROOT}/tsb/api)
include_directories(${AAMP_ROOT}/middleware/closedcaptions)
include_directories(${GLIB_INCLUDE_DIRS})
include_directories(${GSTREAMER_INCLUDE_DIRS})
include_directories(${LIBCJSON_INCLUDE_DIRS})
include_directories(${LibXml2_INCLUDE_DIRS})
include_directories(${LIBDASH_INCLUDE_DIRS})
include_directories(${UTESTS_ROOT}/ocdm)
include_directories(${UTESTS_ROOT}/mocks)
include_directories(${AAMP_ROOT}/middleware)
include_directories(${AAMP_ROOT}/middleware/test/utests/mocks)
include_directories(${AAMP_ROOT}/externals)
include_directories(${AAMP_ROOT}/middleware/vendor)
if (JSC_INCDIR)
    include_directories(${JSC_INCDIR})
endif()

# Mac OS X
if(CMAKE_SYSTEM_NAME STREQUAL Darwin)
    include_directories(${AAMP_ROOT}/.libs/systemd/src)
endif(CMAKE_SYSTEM_NAME STREQUAL Darwin)

include_directories(${AAMP_ROOT}/.libs/include)
include_directories(${UTESTS_ROOT}/mocks)

add_library(fakes ${fakes_SRC} ${middleware_fakes_SRC})
set_target_properties(fakes PROPERTIES FOLDER "utests")
set_target_properties(fakes PROPERTIES COMPILE_FLAGS "-Wno-effc++ -Wno-varargs")
