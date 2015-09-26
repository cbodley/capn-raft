project(raft)
cmake_minimum_required(VERSION 3.0)
set(CMAKE_MODULE_PATH ${CMAKE_SOURCE_DIR}/cmake/modules)

# add WITH_SANITIZERS option for -fsanitize=
include(Sanitizers)

# require at least c++11
include(UseCxxStandard)
use_cxx_standard(REQUIRED VERSIONS 14 11)


find_package(CapnProto REQUIRED)
find_package(Boost REQUIRED COMPONENTS program_options)


# protocol definitions
add_subdirectory(proto)

# raft source
include_directories(include)
install(FILES include/raft.h DESTINATION include/raft)
add_subdirectory(src)

# tests
include_directories(src)
add_subdirectory(test)
