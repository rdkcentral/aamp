cmake_minimum_required (VERSION 3.16) 
find_package(GTest 1.12.1 REQUIRED)
if (CMAKE_SYSTEM_NAME STREQUAL Darwin)
	pkg_check_modules(GOOGLETEST REQUIRED GTest)
else()
	pkg_check_modules(GOOGLETEST REQUIRED gtest)
endif()
include_directories(${GOOGLETEST_INCLUDE_DIRS})

if (CMAKE_PLATFORM_UBUNTU OR CMAKE_SYSTEM_NAME STREQUAL Darwin )
  message("SIMULATOR_BUILD set")
  set(SMOKETEST_DEFINES "${SMOKETEST_DEFINES} -DSIMULATOR_BUILD=1")
endif()

set(AAMP_ROOT ${CMAKE_SOURCE_DIR})
add_definitions(-DAAMP_ROOT="\\"${AAMP_ROOT}/\\"")

if (CMAKE_SYSTEM_NAME STREQUAL Darwin)
  add_definitions(-DAAMPCLI_PATH="\\"${AAMP_ROOT}/build/debug/aamp-cli\\"")
else()
  add_definitions(-DAAMPCLI_PATH="\\"${AAMP_ROOT}/Linux/bin/aamp-cli\\"")
endif()

# Mac OS X
if(CMAKE_SYSTEM_NAME STREQUAL Darwin)
    set(AAMP_OS_SOURCES ${AAMP_ROOT}/cocoa_window.mm)
    set(AAMP_OS_HEADERS ${AAMP_ROOT}/cocoa_window.h)
endif()

set(HEADERS AampSmokeTestPlayer.h TuneSmokeTest.h SmokeTestEventListener.h ScriptedSmokeTestEventListener.h ScriptedSmokeTest.h ${AAMP_OS_HEADERS})
set(SOURCES AampSmokeTestPlayer.cpp TuneSmokeTest.cpp ScriptedSmokeTestEventListener.cpp ScriptedSmokeTest.cpp gtest_main.cpp ${AAMP_OS_SOURCES})

if (CMAKE_PLATFORM_UBUNTU)
    set(CMAKE_INSTALL_RPATH "${PROJECT_SOURCE_DIR}/Linux/lib")
endif()

add_executable(aamp_smoketest ${HEADERS} ${SOURCES})
set(EXEC_NAME aamp_smoketest)

if (CMAKE_PLATFORM_UBUNTU)
    target_link_libraries(aamp_smoketest "-Wl,--disable-new-dtags")
endif()

LIST(APPEND CMAKE_MODULE_PATH "${PROJECT_SOURCE_DIR}/test/smoketest")
LIST(APPEND CMAKE_MODULE_PATH "${PROJECT_SOURCE_DIR}/test")
  
if (COVERAGE_ENABLED)
  include(CodeCoverage)
  APPEND_COVERAGE_COMPILER_FLAGS()
  get_filename_component(PARENT_DIR ${PROJECT_SOURCE_DIR} DIRECTORY)
  get_filename_component(BASE_DIR ${PARENT_DIR} DIRECTORY)
  
  set(COVERAGE_EXCLUDES
    "/usr/*"
    "${PROJECT_SOURCE_DIR}/Linux/*"
    "${PROJECT_SOURCE_DIR}/drm/*"
    "${PROJECT_SOURCE_DIR}/isobmff/*"
    "${PROJECT_SOURCE_DIR}/subtec/*"
    "${PROJECT_SOURCE_DIR}/subtitle/*"
    "${PROJECT_SOURCE_DIR}/test/aampcli/*"
    "${PROJECT_SOURCE_DIR}/test/utests/*"
    "/Applications/*"
    "./v1/*")
  SETUP_TARGET_FOR_COVERAGE_LCOV(NAME ${EXEC_NAME}_coverage
                              EXECUTABLE ${EXEC_NAME}
                              DEPENDENCIES ${EXEC_NAME})

endif()
add_dependencies(aamp_smoketest aamp)
set_target_properties(aamp_smoketest PROPERTIES COMPILE_FLAGS "${SMOKETEST_DEFINES} ${LIBAAMP_DEFINES} ${AAMP_CLI_EXTRA_DEFINES} ${OS_CXX_FLAGS}")
if (CMAKE_XCODE_BUILD_SYSTEM)
  # XCode schema target
  xcode_define_schema(${EXEC_NAME})
endif()
target_link_libraries(aamp_smoketest aamp ${GOOGLETEST_LIBRARIES} --coverage)

install (TARGETS aamp_smoketest DESTINATION lib)
#gtest_discover_tests(aamp_smoketest)  

if (CMAKE_PLATFORM_UBUNTU OR CMAKE_SYSTEM_NAME STREQUAL Darwin)
    # Copy scripts to ~/smoketest (but only if they do not exist)
    file(MAKE_DIRECTORY $ENV{HOME}/smoketest)
    file(GLOB SCRIPT_FILES RELATIVE ${CMAKE_SOURCE_DIR}/test/smoketest/scripts ${CMAKE_SOURCE_DIR}/test/smoketest/scripts/*)
    foreach(SCRIPT IN LISTS SCRIPT_FILES)
#        if (NOT EXISTS $ENV{HOME}/smoketest/${SCRIPT})
            configure_file(${CMAKE_SOURCE_DIR}/test/smoketest/scripts/${SCRIPT} $ENV{HOME}/smoketest/${SCRIPT})
#        endif()
    endforeach()
    # remove old file with an invalid name
    file(REMOVE $ENV{HOME}/smoketest/frozen-video.smk)
endif()

