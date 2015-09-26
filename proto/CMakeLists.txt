include_directories(${CAPNP_INCLUDE_DIRS})

# compile .capnp files into a library
set(proto_files
	append.capnp
	cluster.capnp
	command.capnp
	log.capnp
	raft.capnp
	snapshot.capnp
	vote.capnp
	)

set(CAPNPC_OUTPUT_DIR ${CMAKE_CURRENT_BINARY_DIR})
capnp_generate_cpp(proto_srcs proto_headers ${proto_files})

add_library(raft_proto ${proto_srcs})
target_link_libraries(raft_proto ${CAPNP_LIBRARIES})
target_include_directories(raft_proto PUBLIC ${CAPNPC_OUTPUT_DIR})
install(FILES ${proto_headers} DESTINATION include/raft)
