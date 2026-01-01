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

include(GoogleTest)

set(PLAYER_ROOT "../../../../")
set(UTESTS_ROOT "../../")
set(EXEC_NAME PlayerExternalsRdkInterfaceTests)

include_directories(${PLAYER_ROOT})
include_directories(${PLAYER_ROOT}/externals)
include_directories(${PLAYER_ROOT}/externals/rdk)
include_directories(${PLAYER_ROOT}/externals/rdk/IIarm)
include_directories(${PLAYER_ROOT}/baseConversion)
include_directories(${PLAYER_ROOT}/playerLogManager)
include_directories(${GTEST_INCLUDE_DIRS})
include_directories(${GMOCK_INCLUDE_DIRS})
include_directories(${GLIB_INCLUDE_DIRS})
include_directories(${GSTREAMER_INCLUDE_DIRS})
include_directories(SYSTEM ${UTESTS_ROOT}/mocks)
include_directories(${UTESTS_ROOT}/fakes)

set(TEST_SOURCES 
    PlayerExternalsRdkInterfaceTests.cpp
    PlayerExternalsRdkInterfaceTestCases.cpp)

set(PLAYER_SOURCES 
    ${PLAYER_ROOT}/externals/rdk/PlayerExternalsRdkInterface.cpp
    ${PLAYER_ROOT}/externals/rdk/IIarm/DeviceIARMInterface.cpp
    ${PLAYER_ROOT}/baseConversion/_base64.cpp
    ${PLAYER_ROOT}/playerLogManager/PlayerLogManager.cpp)

# Conditional fake sources
if(CMAKE_DS_EVENT_SUPPORTED)
    add_definitions(-DUSE_DS_EVENT_SUPPORTED=1)
    set(FAKE_SOURCES ${UTESTS_ROOT}/fakes/FakeDeviceSettings.cpp)
else()
    set(FAKE_SOURCES ${UTESTS_ROOT}/fakes/FakeIARM.cpp)
endif()

add_executable(${EXEC_NAME}
               ${TEST_SOURCES}
               ${PLAYER_SOURCES}
               ${FAKE_SOURCES})

set_target_properties(${EXEC_NAME} PROPERTIES FOLDER "utests")

if (CMAKE_XCODE_BUILD_SYSTEM)
    xcode_define_schema(${EXEC_NAME})
endif()

if (COVERAGE_ENABLED)
    include(CodeCoverage)
    APPEND_COVERAGE_COMPILER_FLAGS()
endif()

target_link_libraries(${EXEC_NAME} 
    fakes 
    -pthread 
    ${GMOCK_LINK_LIBRARIES} 
    ${GTEST_LINK_LIBRARIES})

player_utest_run_add(${EXEC_NAME})