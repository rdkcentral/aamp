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
set(EXEC_NAME lstringTests)

# lstringTests is intentionally self-contained: lstring.hpp has no GStreamer
# or priv_aamp dependency.  We do NOT include CommonTestIncludes.cmake
# here because that file adds middleware-player-interface to the include path, which
# transitively pulls in GstUtils.h → gst/gst.h.  On Mac developer
# machines without GStreamer installed this breaks the build even though lstring
# itself never touches a GStreamer symbol.
#
# For the same reason we do NOT link against the `fakes` library:
#   fakes/FakeAampLogManager.cpp → priv_aamp.h → AampDemuxDataTypes.h
#   → DemuxDataTypes.h → GstUtils.h → gst/gst.h
#
# FakeLstringDeps.cpp (in this directory) provides the only AampLogManager
# symbols that lstring.cpp actually needs at link time.

include_directories(${AAMP_ROOT})
include_directories(${GTEST_INCLUDE_DIRS})
include_directories(${GMOCK_INCLUDE_DIRS})
include_directories(${GLIB_INCLUDE_DIRS})

set(TEST_SOURCES lstringTests.cpp
                 lstringRun.cpp
                 FakeLstringDeps.cpp)

set(AAMP_SOURCES ${AAMP_ROOT}/lstring.cpp)

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

target_link_libraries(${EXEC_NAME} ${GLIB_LINK_LIBRARIES} ${OS_LD_FLAGS} ${GMOCK_LINK_LIBRARIES} ${GTEST_LINK_LIBRARIES} -lpthread)

set_target_properties(${EXEC_NAME} PROPERTIES FOLDER "utests")

aamp_utest_run_add(${EXEC_NAME})
