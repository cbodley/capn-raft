@0xf34fba9201805e3d;

using Cxx = import "/capnp/c++.capnp";
$Cxx.namespace("raft::proto::cluster");

struct Args {
	union {
		add @0 :Text;
		remove @1 :UInt32;
		commit @2 :Void;
	}
}

struct Res {
	enum Result {
		success @0;
		invalidArg @1;
		inJointConsensus @2;
		notInJointConsensus @3;
		alreadyAMember @4;
		notAMember @5;
	}
	result @0 :Result;
	union {
		add @1 :UInt32;
		remove @2 :Void;
		commit @3 :Void;
	}
}

struct Model {
	lastId @0 :UInt32;

	struct Pair {
		id @0 :UInt32;
		addr @1 :Text;
	}
	memberAddrs @1 :List(Pair);

	uncommittedAdd :union {
		empty @2 :Void;
		id @3 :UInt32;
	}
	uncommittedRemove :union {
		empty @4 :Void;
		id @5 :UInt32;
	}
}
