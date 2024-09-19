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

include(GoogleTest)

set(AAMP_ROOT "../../../../")
set(UTESTS_ROOT "../../")
set(EXEC_NAME AampOcdmBasicSessionAdapterTests)

include_directories(${AAMP_ROOT} ${AAMP_ROOT}/drm ${AAMP_ROOT}/drm/helper ${AAMP_ROOT}/subtitle ${AAMP_ROOT}/downloader ${AAMP_ROOT}/isobmff ${AAMP_ROOT}/subtec/subtecparser ${AAMP_ROOT}/subtec/libsubtec)
include_directories(${AAMP_ROOT}/tsb/api)

if(CMAKE_SYSTEM_NAME STREQUAL Linux)
    include_directories(${AAMP_ROOT}/Linux/include)
endif(CMAKE_SYSTEM_NAME STREQUAL Linux)



include_directories(${GTEST_INCLUDE_DIRS})
include_directories(${GMOCK_INCLUDE_DIRS})
include_directories(${GLIB_INCLUDE_DIRS})
include_directories(${GSTREAMER_INCLUDE_DIRS})
include_directories(SYSTEM ${UTESTS_ROOT}/mocks)
include_directories(${UTESTS_ROOT}/ocdm)




set(TEST_SOURCES FunctionalTests.cpp
                 AampOcdmBasicSessionAdapterTests.cpp)

set(AAMP_SOURCES ${AAMP_ROOT}/drm/AampOcdmBasicSessionAdapter.cpp
                 ${AAMP_ROOT}/drm/helper/AampDrmHelper.cpp
                 ${AAMP_ROOT}/drm/AampDrmSession.cpp)


#[[class AAMPOCDMBasicSessionAdapter inherits from class AAMPOCDMSessionAdapter,
which inherits from the abstract class AampDrmSession.
Since the implementation of AampDrmSession is fairly simple, the real code is compiled.
However, AAMPOCDMSessionAdapter has a more complex implementation, so a fake has been written for this class.]]

add_executable(${EXEC_NAME}
               ${TEST_SOURCES}
               ${AAMP_SOURCES})
set_target_properties(${EXEC_NAME} PROPERTIES FOLDER "utests")

if (CMAKE_XCODE_BUILD_SYSTEM)
  # XCode schema target
  xcode_define_schema(${EXEC_NAME})
endif()



if (COVERAGE_ENABLED)
    include(CodeCoverage)
    APPEND_COVERAGE_COMPILER_FLAGS()
    include("${PROJECT_SOURCE_DIR}/cmake_exclude_file.list")
    SETUP_TARGET_FOR_COVERAGE_LCOV(NAME ${EXEC_NAME}_coverage
                              EXECUTABLE ${EXEC_NAME}
                              DEPENDENCIES ${EXEC_NAME})
endif()

target_link_libraries(${EXEC_NAME} fakes -lpthread ${OS_LD_FLAGS} ${GMOCK_LINK_LIBRARIES} ${GTEST_LINK_LIBRARIES})

aamp_utest_run_add(${EXEC_NAME})
