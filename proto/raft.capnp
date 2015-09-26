@0xa06a4b7f02e5af80;

using Cxx = import "/capnp/c++.capnp";
$Cxx.namespace("raft::proto");

using Append = import "append.capnp";
using Command = import "command.capnp";
using Snapshot = import "snapshot.capnp";
using Vote = import "vote.capnp";

interface Raft {
	append @0 (args :Append.Args) -> (res :Append.Res);
	command @1 (args :Command.Args) -> (res :Command.Res);
	snapshot @2 (args :Snapshot.Args) -> (res :Snapshot.Res);
	vote @3 (args :Vote.Args) -> (res :Vote.Res);
}
