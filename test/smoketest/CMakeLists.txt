cmake_minimum_required (VERSION 2.6)
find_package(GTest 1.12.1 REQUIRED)
pkg_check_modules(GOOGLETEST REQUIRED GTest)
include_directories(${GOOGLETEST_INCLUDE_DIRS})

if (CMAKE_PLATFORM_UBUNTU OR CMAKE_SYSTEM_NAME STREQUAL Darwin )
  message("SIMULATOR_BUILD set")
  set(SMOKETEST_DEFINES "${SMOKETEST_DEFINES} -DSIMULATOR_BUILD=1")
endif()

set(AAMP_ROOT ${CMAKE_SOURCE_DIR})

# Mac OS X
if(CMAKE_SYSTEM_NAME STREQUAL Darwin)
    set(AAMP_OS_SOURCES ${AAMP_ROOT}/cocoa_window.mm)
    set(AAMP_OS_HEADERS ${AAMP_ROOT}/cocoa_window.h)
endif()

set(HEADERS AampSmokeTestPlayer.h TuneSmokeTest.h ${AAMP_OS_HEADERS})
set(SOURCES AampSmokeTestPlayer.cpp TuneSmokeTest.cpp gtest_main.cpp ${AAMP_OS_SOURCES})


add_executable(aamp_smoketest ${HEADERS} ${SOURCES})
add_dependencies(aamp_smoketest aamp)
set_target_properties(aamp_smoketest PROPERTIES COMPILE_FLAGS "${SMOKETEST_DEFINES} ${LIBAAMP_DEFINES} ${AAMP_CLI_EXTRA_DEFINES} ${OS_CXX_FLAGS}")
# XCode schema target
xcode_define_schema(aamp_smoketest)
target_link_libraries(aamp_smoketest aamp ${GOOGLETEST_LIBRARIES})
#gtest_discover_tests(aamp_gtest)
