project(raft)
cmake_minimum_required(VERSION 3.1)
set(CMAKE_MODULE_PATH ${CMAKE_SOURCE_DIR}/cmake/modules)

# add WITH_SANITIZERS option for -fsanitize=
include(Sanitizers)

# require c++14 without extensions
set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_EXTENSIONS OFF)


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
