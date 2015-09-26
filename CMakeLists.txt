project(raft)
cmake_minimum_required(VERSION 3.0)
set(CMAKE_MODULE_PATH ${CMAKE_SOURCE_DIR}/cmake/modules)

# add WITH_SANITIZERS option for -fsanitize=
include(Sanitizers)

# require at least c++11
include(UseCxxStandard)
use_cxx_standard(REQUIRED VERSIONS 14 11)

# raft source
add_subdirectory(src)

# tests
add_subdirectory(test)
