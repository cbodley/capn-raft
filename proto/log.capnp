@0xd4d1eaf4c0f41f4f;

using Cxx = import "/capnp/c++.capnp";
$Cxx.namespace("raft::proto::log");

struct Entry {
	term @0 :UInt32;
	command @1 :UInt32;
	data @2 :Data;
}
