@0xdbb59f55f6da7e72;

using Cxx = import "/capnp/c++.capnp";
$Cxx.namespace("raft::proto::vote");

struct Args {
	term @0 :UInt32;
	candidate @1 :UInt32;
	lastLogIndex @2 :UInt32;
	lastLogTerm @3 :UInt32;
}

struct Res {
	term @0 :UInt32;
	voteGranted @1 :Bool;
}
