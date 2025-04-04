# If not stated otherwise in this file or this component's license file the
# following copyright and licenses apply:
#
# Copyright 2023 RDK Management
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

set(AAMP_ROOT "../../../../")
set(UTESTS_ROOT "../../")
set(EXEC_NAME AampMPDParseHelper)

include_directories(${AAMP_ROOT} ${AAMP_ROOT}/isobmff ${AAMP_ROOT}/drm ${AAMP_ROOT}/drm/helper ${AAMP_ROOT}/drm/ave ${AAMP_ROOT}/subtitle ${AAMP_ROOT}/middleware/subtitle ${AAMP_ROOT}/downloader)
include_directories(${AAMP_ROOT}/middleware/subtec/libsubtec)
include_directories(${AAMP_ROOT}/middleware/subtec/subtecparser)
include_directories(${AAMP_ROOT}/middleware/playerjsonobject)
include_directories(${AAMP_ROOT}/dash/xml ${AAMP_ROOT}/dash/utils ${AAMP_ROOT}/dash/mpd)

include_directories(${GTEST_INCLUDE_DIRS})
include_directories(${GMOCK_INCLUDE_DIRS})
include_directories(${GLIB_INCLUDE_DIRS})
include_directories(${GSTREAMER_INCLUDE_DIRS})
include_directories(${LIBDASH_INCLUDE_DIRS})
include_directories(${LibXml2_INCLUDE_DIRS})
include_directories(SYSTEM ${UTESTS_ROOT}/mocks)
include_directories(${LIBCJSON_INCLUDE_DIRS})
include_directories(${AAMP_ROOT}/tsb/api)
include_directories(${AAMP_ROOT}/middleware)

set (DASH_PARSER_SOURCES ${AAMP_ROOT}/dash/xml/DomDocument.cpp
                ${AAMP_ROOT}/dash/xml/DomElement.cpp
                ${AAMP_ROOT}/dash/xml/DomNode.cpp
                ${AAMP_ROOT}/dash/xml/DomNodeIter.cpp
                ${AAMP_ROOT}/dash/xml/DomTextNode.cpp
                ${AAMP_ROOT}/dash/mpd/MPDModel.cpp
                ${AAMP_ROOT}/dash/mpd/MPDSegmenter.cpp
                ${AAMP_ROOT}/dash/utils/Utils.cpp
                ${AAMP_ROOT}/dash/utils/Url.cpp
                ${AAMP_ROOT}/dash/utils/StringEx.cpp
                ${AAMP_ROOT}/dash/utils/Path.cpp)

set(TEST_SOURCES FunctionalTests.cpp
	AampMPDParseHelper.cpp)


set(AAMP_SOURCES ${AAMP_ROOT}/downloader/AampCurlDownloader.cpp ${AAMP_ROOT}/AampUtils.cpp ${AAMP_ROOT}/_base64.cpp
                ${AAMP_ROOT}/iso639map.cpp
		${AAMP_ROOT}/AampMPDParseHelper.cpp
		${AAMP_ROOT}/AampMPDDownloader.cpp
		${AAMP_ROOT}/AampMPDUtils.cpp
		${DASH_PARSER_SOURCES})

add_executable(${EXEC_NAME}
               ${TEST_SOURCES}
               ${AAMP_SOURCES})

if (CMAKE_XCODE_BUILD_SYSTEM)
  # XCode schema target
  xcode_define_schema(${EXEC_NAME})
endif()

if (COVERAGE_ENABLED)
    include(CodeCoverage)
    APPEND_COVERAGE_COMPILER_FLAGS()
endif()

target_link_libraries(${EXEC_NAME} -lpthread ${LIBDASH_LINK_LIBRARIES} ${LIBCJSON_LINK_LIBRARIES} -lcurl -lxml2 fakes ${GLIB_LINK_LIBRARIES} ${OS_LD_FLAGS} ${GMOCK_LINK_LIBRARIES} ${GTEST_LINK_LIBRARIES})
set_target_properties(${EXEC_NAME} PROPERTIES FOLDER "utests")

aamp_utest_run_add(${EXEC_NAME})
