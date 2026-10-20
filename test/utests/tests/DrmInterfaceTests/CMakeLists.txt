include(GoogleTest)

set(AAMP_ROOT "../../../../")
set(UTESTS_ROOT "../../")
set(EXEC_NAME DrmInterfaceTests)

include(${CMAKE_CURRENT_LIST_DIR}/../CommonTestIncludes.cmake)
include_directories(${AAMP_ROOT}/drm)
add_definitions(-DAAMP_VANILLA_AES_SUPPORT)

set(TEST_SOURCES DrmInterfaceTests.cpp DrmInterfaceTestsMain.cpp)
set(AAMP_SOURCES ${AAMP_ROOT}/drm/DrmInterface.cpp ${AAMP_ROOT}/.libs/middleware-player-interface/drm/aes/Aes.cpp)

add_executable(${EXEC_NAME} ${TEST_SOURCES} ${AAMP_SOURCES})
set_target_properties(${EXEC_NAME} PROPERTIES FOLDER "utests")

target_link_libraries(${EXEC_NAME} fakes -pthread ${OPENSSL_LINK_LIBRARIES} ${GLIB_LINK_LIBRARIES} ${OS_LD_FLAGS} ${GMOCK_LINK_LIBRARIES} ${GTEST_LINK_LIBRARIES})

aamp_utest_run_add(${EXEC_NAME})
