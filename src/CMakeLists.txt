include_directories(${CAPNP_INCLUDE_DIR})
add_definitions(${CAPNP_DEFINITIONS})

include_directories(${PROTO_INCLUDE_DIR}) # generated capnp headers

add_library(raft_config config.cc)
target_link_libraries(raft_config ${Boost_LIBRARIES})

add_executable(raftd raftd.cc)
target_link_libraries(raftd raft_config raft_proto)
install(TARGETS raftd DESTINATION bin)
