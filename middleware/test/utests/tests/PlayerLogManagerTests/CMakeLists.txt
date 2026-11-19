include(GoogleTest)

set(PLAYER_ROOT "../../../../")
set(UTESTS_ROOT "../../")
set(EXEC_NAME PlayerLogManagerTests)

include_directories(${PLAYER_ROOT} ${PLAYER_ROOT}/playerLogManager)
include_directories(${GTEST_INCLUDE_DIRS})
include_directories(${GMOCK_INCLUDE_DIRS})
include_directories(${GLIB_INCLUDE_DIRS})
include_directories(SYSTEM ${UTESTS_ROOT}/mocks)

add_executable(${EXEC_NAME}
               PlayerProfileScopeTests.cpp
               ${PLAYER_ROOT}/playerLogManager/PlayerLogManager.cpp)

target_compile_definitions(${EXEC_NAME} PRIVATE ENABLE_RDKPERF)
target_include_directories(${EXEC_NAME} BEFORE PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/rdkperf_stub)
set_target_properties(${EXEC_NAME} PROPERTIES FOLDER "utests")
target_link_libraries(${EXEC_NAME} fakes -pthread ${GLIB_LINK_LIBRARIES} ${OS_LD_FLAGS} ${GMOCK_LINK_LIBRARIES} ${GTEST_LINK_LIBRARIES})

player_utest_run_add(${EXEC_NAME})
