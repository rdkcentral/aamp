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

set(EXEC_NAME Mp4BoxParsingTests)
set(AAMP_ROOT "../../../../")
set(UTESTS_ROOT "../../")

set(TEST_SOURCES
    BoxParsingTests.cpp
    Mp4BoxParsingTests.cpp
)

set(AAMP_SOURCES
    ${AAMP_ROOT}/mp4demux/MP4Demux.cpp
    ${AAMP_ROOT}/AampGrowableBuffer.cpp
    ${AAMP_ROOT}/aamplogging.cpp
)

include_directories(
    ${AAMP_ROOT}
    ${AAMP_ROOT}/mp4demux
    ${UTESTS_ROOT}/mocks
    ${UTESTS_ROOT}/fakes
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${GTEST_INCLUDE_DIRS}
    ${GMOCK_INCLUDE_DIRS}
    ${GLIB_INCLUDE_DIRS}
    ${LIBCJSON_INCLUDE_DIRS}
)

add_executable(${EXEC_NAME} 
    ${TEST_SOURCES}
    ${AAMP_SOURCES}
)

set_target_properties(${EXEC_NAME} PROPERTIES
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED YES
    CXX_EXTENSIONS NO
)

if (CMAKE_XCODE_BUILD_SYSTEM)
  # XCode schema target
  xcode_define_schema(${EXEC_NAME})
endif()

if (COVERAGE_ENABLED)
    include(CodeCoverage)
    APPEND_COVERAGE_COMPILER_FLAGS()
endif()

target_compile_definitions(${EXEC_NAME} 
    PRIVATE 
        UNIT_TEST_ENABLED=1
        AAMP_DISABLE_INJECT=1
)

target_link_libraries(${EXEC_NAME}
    fakes
    ${CMAKE_THREAD_LIBS_INIT}
    ${GMOCK_LINK_LIBRARIES} 
    ${GTEST_LINK_LIBRARIES}
    ${GLIB_LINK_LIBRARIES}
    ${LIBCJSON_LINK_LIBRARIES}
    -lpthread
)

set_target_properties(${EXEC_NAME} PROPERTIES FOLDER "utests/BoxParsing")

aamp_utest_run_add(${EXEC_NAME})
