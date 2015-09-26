@0xc866c3f4239ec84c;

using Cxx = import "/capnp/c++.capnp";
$Cxx.namespace("raft::proto::command");

using Cluster = import "cluster.capnp";

struct Args {
	sequence @0 :UInt32;
	union {
		ping @1 :Void;
		cluster @2 :Cluster.Args;
	}
}

struct Res {
	sequence @0 :UInt32;
	leader @1 :Text;
	union {
		ping @2 :Void;
		cluster @3 :Cluster.Res;
	}
}
