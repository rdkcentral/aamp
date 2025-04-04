# If not stated otherwise in this file or this component's LICENSE file the
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

include(GoogleTest)

set(AAMP_ROOT "../../../../")
set(UTESTS_ROOT "../../")
set(EXEC_NAME IsoBmffBoxTests)
set(TEST_FILES_DIR "${CMAKE_CURRENT_SOURCE_DIR}/testFiles")

include_directories(${AAMP_ROOT} ${AAMP_ROOT}/isobmff)
include_directories(${AAMP_ROOT}/subtitle)
include_directories(${AAMP_ROOT}/middleware/subtitle)

include_directories(${GTEST_INCLUDE_DIRS})
include_directories(${GMOCK_INCLUDE_DIRS})
include_directories(${UTESTS_ROOT}/mocks)
include_directories(${TEST_FILES_DIR})
include_directories(${AAMP_ROOT}/tsb/api)
include_directories(${AAMP_ROOT}/middleware)

set(TEST_SOURCES IsoBmffBoxTests.cpp IsoBmffBoxMainTest.cpp)
get_cmake_property(_variableNames VARIABLES)
list (SORT _variableNames)
foreach (_variableName ${_variableNames})
    message(STATUS "${_variableName}=${${_variableName}}")
endforeach()

set(AAMP_SOURCES ${AAMP_ROOT}/isobmff/isobmffbox.cpp)

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

add_compile_definitions(TESTS_DIR="${TEST_FILES_DIR}")
target_link_libraries(${EXEC_NAME} fakes ${OS_LD_FLAGS} ${GMOCK_LINK_LIBRARIES} ${GTEST_LINK_LIBRARIES})

set_target_properties(${EXEC_NAME} PROPERTIES FOLDER "utests")

aamp_utest_run_add(${EXEC_NAME})
