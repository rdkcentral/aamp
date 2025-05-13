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

set(EXEC_NAME TsbStoreMockFsTests)

include_directories(${TSB_ROOT}/api)
include_directories(${TSB_ROOT}/src)

set(TSB_SOURCES ${TSB_ROOT}/src/TsbStore.cpp ${TSB_ROOT}/src/TsbLog.cpp ${TSB_ROOT}/src/TsbSem.cpp ${TSB_ROOT}/src/TsbLocationLock.cpp)
set(TEST_SOURCES TsbStoreTestsMain.cpp TsbStoreTests.cpp)
set(FAKE_SOURCES ../../fakes/TsbFs.cpp)

add_executable(${EXEC_NAME}
			   ${TEST_SOURCES}
			   ${FAKE_SOURCES}
			   ${TSB_SOURCES})
set_target_properties(${EXEC_NAME} PROPERTIES FOLDER "utests")

target_link_libraries(${EXEC_NAME} ${OS_LD_FLAGS} GTest::gtest_main GTest::gmock_main )

tsb_test_run_add(${EXEC_NAME})
