enable_testing()
add_custom_target(check COMMAND ${CMAKE_CTEST_COMMAND})

# use gtest
find_package(GTest REQUIRED)
include_directories(${GTEST_INCLUDE_DIRS})
list(APPEND TEST_LIBRARIES ${GTEST_LIBRARIES} ${GTEST_MAIN_LIBRARIES})


# unit tests
add_custom_command(OUTPUT test_config.conf
	COMMAND ln -sf ${CMAKE_CURRENT_SOURCE_DIR}/raft_test.conf test_config.conf)
add_executable(test_config EXCLUDE_FROM_ALL test_config.cc test_config.conf)
target_link_libraries(test_config raft_config ${TEST_LIBRARIES})
add_test(Config test_config)
add_dependencies(check test_config)

add_executable(test_election EXCLUDE_FROM_ALL test_election.cc)
target_link_libraries(test_election raft ${TEST_LIBRARIES})
add_test(Election test_election)
add_dependencies(check test_election)
