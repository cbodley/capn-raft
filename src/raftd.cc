// -*- mode:C++; tab-width:8; c-basic-offset:2; indent-tabs-mode:t -*-
// vim: ts=8 sw=2 smarttab
/*
 * Copyright (C) 2015 Casey Bodley <cbodley@gmail.com>
 *
 * This is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License version 2.1, as published by the Free Software
 * Foundation.  See file COPYING.
 */

#include <iostream>

#include <capnp/ez-rpc.h>

#include "raft.capnp.h"
#include "config.h"

namespace std {
ostream &operator<<(ostream &out, const kj::StringTree &strings) {
  strings.visit([&out](auto str) { out.write(str.begin(), str.size()); });
  return out;
}
} // namespace std

using namespace raft;
using namespace server;

namespace {
class Impl final : public proto::Raft::Server {
public:
  kj::Promise<void> append(AppendContext context) override {
    auto args = context.getParams().getArgs();
    auto res = context.getResults().getRes();

    std::cout << "append args" << args.toString() << " -> res" << res.toString()
              << std::endl;
    return kj::READY_NOW;
  }

  kj::Promise<void> command(CommandContext context) override {
    auto args = context.getParams().getArgs();
    auto res = context.getResults().getRes();

    std::cout << "command args" << args.toString() << " -> res"
              << res.toString() << std::endl;
    return kj::READY_NOW;
  }

  kj::Promise<void> snapshot(SnapshotContext context) override {
    auto args = context.getParams().getArgs();
    auto res = context.getResults().getRes();

    std::cout << "snapshot args" << args.toString() << " -> res"
              << res.toString() << std::endl;
    return kj::READY_NOW;
  }

  kj::Promise<void> vote(VoteContext context) override {
    auto args = context.getParams().getArgs();
    auto res = context.getResults().getRes();

    std::cout << "vote args" << args.toString() << " -> res" << res.toString()
              << std::endl;
    return kj::READY_NOW;
  }
};
} // anonymous namespace

int main(int argc, const char **argv) {
  Configuration config;
  if (!parse_config(argc, argv, config))
    return 1;

  auto addr = config.member_addrs.find(config.member_id);
  if (addr == config.member_addrs.end()) {
    std::cerr << "No address given for id " << config.member_id << std::endl;
    return 1;
  }

  capnp::EzRpcServer server(kj::heap<Impl>(), addr->second, 13579);
  kj::NEVER_DONE.wait(server.getWaitScope());
  return 0;
}
