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

include(GoogleTest)

set(PLAYER_ROOT "../../../../")
set(UTESTS_ROOT "../../")
set(EXEC_NAME TtmlSubtecParserTests)

include_directories(${PLAYER_ROOT} ${PLAYER_ROOT}/isobmff ${PLAYER_ROOT}/drm ${PLAYER_ROOT}/downloader ${PLAYER_ROOT}/drm/helper ${PLAYER_ROOT}/drm/ave ${PLAYER_ROOT}/subtitle ${PLAYER_ROOT}/middleware/subtitle)
include_directories(${PLAYER_ROOT}/subtec/libsubtec)
include_directories(${PLAYER_ROOT}/subtec/subtecparser)
include_directories(${PLAYER_ROOT}/playerLogManager)
include_directories(${PLAYER_ROOT}/playerjsonobject)
include_directories(${PLAYER_ROOT}/baseConversion)
include_directories(${PLAYER_ROOT}/playerisobmff)

include_directories(${GTEST_INCLUDE_DIRS})
include_directories(${GMOCK_INCLUDE_DIRS})
include_directories(${GLIB_INCLUDE_DIRS})
include_directories(${GSTREAMER_INCLUDE_DIRS})
include_directories(${LibXml2_INCLUDE_DIRS})
include_directories(${LIBCJSON_INCLUDE_DIRS})
include_directories(SYSTEM ${UTESTS_ROOT}/mocks)
include_directories(${PLAYER_ROOT}/tsb/api)
include_directories(${PLAYER_ROOT}/../)

set(TEST_SOURCES TtmlSubtecParserTests.cpp
	         TtmlSubtecParserPlayer.cpp)

set(FAKE_SOURCES ${PLAYER_ROOT}/test/utests/fakes/FakePacketSender.cpp)

set(PLAYER_SOURCES ${PLAYER_ROOT}/subtec/subtecparser/TtmlSubtecParser.cpp
                 ${PLAYER_ROOT}/playerLogManager/PlayerLogManager.cpp
	         ${PLAYER_ROOT}/playerisobmff/playerisobmffbuffer.cpp
	         ${PLAYER_ROOT}/playerisobmff/playerisobmffbox.cpp
		 ${PLAYER_ROOT}/subtec/libsubtec/SubtecChannel.cpp)

add_executable(${EXEC_NAME}
               ${TEST_SOURCES}
               ${PLAYER_SOURCES}
               ${FAKE_SOURCES})

set_target_properties(${EXEC_NAME} PROPERTIES FOLDER "utests")

if (CMAKE_XCODE_BUILD_SYSTEM)
  # XCode schema target
  xcode_define_schema(${EXEC_NAME})
endif()

if (COVERAGE_ENABLED)
    include(CodeCoverage)
    APPEND_COVERAGE_COMPILER_FLAGS()
endif()

# This is a misnomer, it has nothing to do with Ubuntu, but causes a workaround to be enabled in 
# gst-plugins/drm/gst/gstcdmidecryptor.cpp. Should rename the ifdef there and here.
if (CMAKE_PLATFORM_UBUNTU OR APPLE)
    add_definitions(-DUBUNTU)
endif()

target_link_libraries(${EXEC_NAME} fakes -pthread ${GLIB_LINK_LIBRARIES} ${OS_LD_FLAGS} ${GMOCK_LINK_LIBRARIES} ${GTEST_LINK_LIBRARIES})

player_utest_run_add(${EXEC_NAME})
