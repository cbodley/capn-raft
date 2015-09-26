project(raft)
cmake_minimum_required(VERSION 3.0)
set(CMAKE_MODULE_PATH ${CMAKE_SOURCE_DIR}/cmake/modules)

# add WITH_SANITIZERS option for -fsanitize=
include(Sanitizers)

# raft source
add_subdirectory(src)
