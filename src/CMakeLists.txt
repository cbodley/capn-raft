include_directories(${CAPNP_INCLUDE_DIR})
add_definitions(${CAPNP_DEFINITIONS})

add_executable(raft_client raft.cc)
target_link_libraries(raft_client raft_proto)
set_target_properties(raft_client PROPERTIES OUTPUT_NAME raft)
install(TARGETS raft_client DESTINATION bin)

set(raft_src
	append.cc
	cluster.cc
	election.cc
	network.cc
	server.cc
	)
add_library(raft ${raft_src})
target_link_libraries(raft raft_proto)
install(TARGETS raft DESTINATION lib)

add_library(raft_config config.cc)
target_link_libraries(raft_config ${Boost_PROGRAM_OPTIONS_LIBRARY})

add_executable(raftd raftd.cc)
target_link_libraries(raftd raft_config raft)
install(TARGETS raftd DESTINATION bin)
