# If not stated otherwise in this file or this component's license file the
# following copyright and licenses apply:
#
# Copyright 20232 RDK Management
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
set(EXEC_NAME ConfigTests)
set(CMAKE_CXX_FLAGS "-DUTEST")

include_directories(${AAMP_ROOT} ${AAMP_ROOT}/downloader ${AAMP_ROOT}/drm ${AAMP_ROOT}/drm/helper ${AAMP_ROOT}/subtitle ${AAMP_ROOT}/middleware/subtitle) # ${AAMP_ROOT}/isobmff ${AAMP_ROOT}/middleware/playerjsonobject ${AAMP_ROOT}/middleware/subtec/subtecparser)
include_directories(${AAMP_ROOT}/tsb/api)
include_directories(${AAMP_ROOT}/middleware)
include_directories(${AAMP_ROOT}/middleware/subtec/libsubtec)
include_directories(${AAMP_ROOT}/externals)
include_directories(${AAMP_ROOT}/tsb/api ${AAMP_ROOT}/middleware)

include_directories(${LIBCJSON_INCLUDE_DIRS})
include_directories(${GTEST_INCLUDE_DIRS})
include_directories(${GMOCK_INCLUDE_DIRS})
include_directories(${GLIB_INCLUDE_DIRS})
include_directories(${GSTREAMER_INCLUDE_DIRS})
include_directories(${UTESTS_ROOT}/mocks)
include_directories(${UTESTS_ROOT}/drm/mocks)
include_directories(${AAMP_ROOT}/middleware/vendor)

set(TEST_SOURCES ConfigTests.cpp
                 ConfigRun.cpp)
set(FAKE_SOURCE ${UTESTS_ROOT}/fakes/FakeAampRfc.cpp)
					 
add_executable(${EXEC_NAME}
               ${TEST_SOURCES}
               ${FAKE_SOURCE})
target_link_directories(${EXEC_NAME} PUBLIC ${LIBCJSON_LIBRARY_DIRS})

if (CMAKE_XCODE_BUILD_SYSTEM)
  # XCode schema target
  xcode_define_schema(${EXEC_NAME})
endif()

if (COVERAGE_ENABLED)
    include(CodeCoverage)
    APPEND_COVERAGE_COMPILER_FLAGS()
endif()

target_link_libraries(${EXEC_NAME} "cjson")
target_link_libraries(${EXEC_NAME} ${GLIB_LINK_LIBRARIES} ${OS_LD_FLAGS} fakes ${GMOCK_LINK_LIBRARIES} ${GTEST_LINK_LIBRARIES} -lpthread)


set_target_properties(${EXEC_NAME} PROPERTIES FOLDER "utests")

aamp_utest_run_add(${EXEC_NAME})
