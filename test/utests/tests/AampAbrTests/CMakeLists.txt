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
set(EXEC_NAME AampAbrTests)

pkg_check_modules(LIBDASH REQUIRED libdash)
pkg_check_modules(UUID REQUIRED uuid)

# Include common test directories
include(${CMAKE_CURRENT_LIST_DIR}/../CommonTestIncludes.cmake)

set(TEST_SOURCES    AbrTests.cpp
AAMPAbrTests.cpp)
set(AAMP_SOURCES
	${AAMP_ROOT}/priv_aamp.cpp
	${AAMP_ROOT}/abr/abr.cpp
	${AAMP_ROOT}/abr/RollingMedianOutlierEstimator.cpp
	${AAMP_ROOT}/abr/HarmonicEwmaEstimator.cpp
	${AAMP_ROOT}/CachedFragment.cpp
)
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
endif()
target_link_libraries(${EXEC_NAME} fakes net_persona_fitter -pthread ${GLIB_LINK_LIBRARIES} ${OS_LD_FLAGS} ${GMOCK_LINK_LIBRARIES} ${GTEST_LINK_LIBRARIES} ${UUID_LINK_LIBRARIES})
aamp_utest_run_add(${EXEC_NAME})

add_executable(HybridAbrTests HybridAbrTests.cpp
	../../../../support/aampabr/HybridABRManager.cpp
	../../../../support/aampabr/ABRManager.cpp
)
if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
	target_link_libraries(HybridAbrTests PRIVATE gtest gmock gtest_main "-latomic")
else()
	target_link_libraries(HybridAbrTests PRIVATE gtest gmock gtest_main)
endif()
aamp_utest_run_add(HybridAbrTests)
