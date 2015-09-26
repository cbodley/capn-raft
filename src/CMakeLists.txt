add_library(raft_config config.cc)
target_link_libraries(raft_config ${Boost_LIBRARIES})

add_executable(raftd raftd.cc)
target_link_libraries(raftd raft_config)
install(TARGETS raftd DESTINATION bin)
