@0x971f6a2ed0081923;

using Cxx = import "/capnp/c++.capnp";
$Cxx.namespace("raft::proto::snapshot");

struct Args {
	term @0 :UInt32;
	leaderId @1 :UInt32;
	lastIndex @2 :UInt32;
	lastTerm @3 :UInt32;
	offset @4 :UInt64;
	data @5 :Data;
	done @6 :Bool;
}

struct Res {
	term @0 :UInt32;
}
