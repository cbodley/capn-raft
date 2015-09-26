@0xf33ef6533fa92f9f;

using Cxx = import "/capnp/c++.capnp";
$Cxx.namespace("raft::proto::append");

using Log = import "log.capnp";

struct Args {
	term @0 :UInt32;
	leader @1 :UInt32;
	prevLogIndex @2 :UInt32;
	prevLogTerm @3 :UInt32;
	entries @4 :List(Log.Entry);
	leaderCommit @5 :UInt32;
}

struct Res {
	term @0 :UInt32;
	matched @1 :UInt32;
}
