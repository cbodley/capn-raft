enable_testing()
add_custom_target(check COMMAND ${CMAKE_CTEST_COMMAND})

# use gtest
find_package(GTest REQUIRED)
include_directories(${GTEST_INCLUDE_DIRS})
list(APPEND TEST_LIBRARIES ${GTEST_LIBRARIES} ${GTEST_MAIN_LIBRARIES})
