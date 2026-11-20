find_package(GTest REQUIRED)

set(NMQE_FIXTURES "${CMAKE_CURRENT_SOURCE_DIR}/fixtures")

add_executable(nmqe-unit-tests
	unit/Base64Test.cpp
	unit/BoundedTrackQueueTest.cpp
	unit/CatalogTest.cpp
	unit/CmafTrackParserTest.cpp
	unit/TelemetryTest.cpp)
target_link_libraries(nmqe-unit-tests PRIVATE nmqe_player nmqe_replay GTest::gtest_main)
target_compile_definitions(nmqe-unit-tests PRIVATE NMQE_FIXTURES="${NMQE_FIXTURES}")
add_test(NAME unit COMMAND nmqe-unit-tests)

# Replay -> Null pipeline: needs a reference capture (not committed; see tests/fixtures/README.md).
add_executable(nmqe-integration-tests integration/ReplayNullTest.cpp)
target_link_libraries(nmqe-integration-tests PRIVATE nmqe_player nmqe_media nmqe_replay GTest::gtest_main)
target_compile_definitions(nmqe-integration-tests PRIVATE NMQE_FIXTURES="${NMQE_FIXTURES}")
add_test(NAME replay_null COMMAND nmqe-integration-tests)
