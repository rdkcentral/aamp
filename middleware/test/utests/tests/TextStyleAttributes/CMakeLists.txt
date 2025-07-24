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

set(PLAYER_ROOT "../../../../")
set(UTESTS_ROOT "../../")
set(EXEC_NAME TextStyleAttributes)

include_directories(${PLAYER_ROOT})
include_directories(${PLAYER_ROOT}/subtitle)
include_directories(${PLAYER_ROOT}/subtec/libsubtec)
include_directories(${PLAYER_ROOT}/subtec/subtecparser)
include_directories(${PLAYER_ROOT}/playerJsonObject)
include_directories(${PLAYER_ROOT}/playerLogManager)
include_directories(${GTEST_INCLUDE_DIRS})
include_directories(${GMOCK_INCLUDE_DIRS})
include_directories(${GLIB_INCLUDE_DIRS})
include_directories(${GSTREAMER_INCLUDE_DIRS})
include_directories(SYSTEM ${UTESTS_ROOT}/mocks)
include_directories(${LIBCJSON_INCLUDE_DIRS})


set(TEST_SOURCES    GetTextStyleAttributesTests.cpp
                    TextStyleAttributesTests.cpp)

set(PLAYER_SOURCES ${PLAYER_ROOT}/playerLogManager/PlayerLogManager.cpp
		${PLAYER_ROOT}/subtec/subtecparser/TextStyleAttributes.cpp)

add_executable(${EXEC_NAME}
               ${TEST_SOURCES}
               ${PLAYER_SOURCES})
set_target_properties(${EXEC_NAME} PROPERTIES FOLDER "utests")
if (CMAKE_XCODE_BUILD_SYSTEM)
    include(CodeCoverage)
    APPEND_COVERAGE_COMPILER_FLAGS()
endif()

target_link_libraries(${EXEC_NAME} fakes ${GLIB_LINK_LIBRARIES} ${OS_LD_FLAGS} ${GMOCK_LINK_LIBRARIES} ${GTEST_LINK_LIBRARIES} -lpthread)

player_utest_run_add(${EXEC_NAME})
