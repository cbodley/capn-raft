include_directories(${CAPNP_INCLUDE_DIR})
add_definitions(${CAPNP_DEFINITIONS})

include_directories(${PROTO_INCLUDE_DIR}) # generated capnp headers

add_executable(raft_client raft.cc)
target_link_libraries(raft_client raft_proto)
set_target_properties(raft_client PROPERTIES OUTPUT_NAME raft)
install(TARGETS raft_client DESTINATION bin)

add_library(raft_config config.cc)
target_link_libraries(raft_config ${Boost_LIBRARIES})

add_executable(raftd raftd.cc)
target_link_libraries(raftd raft_config raft_proto)
install(TARGETS raftd DESTINATION bin)
