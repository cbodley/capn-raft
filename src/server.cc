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

#include "server.h"

namespace std {
ostream &operator<<(ostream &out, const kj::StringTree &strings) {
  strings.visit([&out](auto str) { out.write(str.begin(), str.size()); });
  return out;
}
} // namespace std

using namespace raft;
using namespace proto;
using namespace server;

class Server::Impl {
private:
  const Configuration &config;

public:
  Impl(const Configuration &config) : config(config) {}

  kj::Promise<void> append(append::Args::Reader args,
                           append::Res::Builder res) {
    return kj::READY_NOW;
  }
  kj::Promise<void> command(command::Args::Reader args,
                            command::Res::Builder res) {
    res.setSequence(args.getSequence());
    return kj::READY_NOW;
  }
  kj::Promise<void> snapshot(snapshot::Args::Reader args,
                             snapshot::Res::Builder res) {
    return kj::READY_NOW;
  }
  kj::Promise<void> vote(vote::Args::Reader args, vote::Res::Builder res) {
    return kj::READY_NOW;
  }
};

Server::Server(const Configuration &config)
    : impl(std::make_unique<Impl>(config)) {}

Server::~Server() = default;

kj::Promise<void> Server::append(AppendContext context) {
  auto args = context.getParams().getArgs();
  auto res = context.getResults().getRes();
  std::cout << "append args" << args.toString() << std::endl;
  auto reply = impl->append(args, res);
  return reply.then([res = std::move(res)]() {
    std::cout << "append res" << res.toString() << std::endl;
  });
}

kj::Promise<void> Server::command(CommandContext context) {
  auto args = context.getParams().getArgs();
  auto res = context.getResults().getRes();
  std::cout << "command args" << args.toString() << std::endl;
  auto reply = impl->command(args, res);
  return reply.then([res = std::move(res)]() {
    std::cout << "command res" << res.toString() << std::endl;
  });
}

kj::Promise<void> Server::snapshot(SnapshotContext context) {
  auto args = context.getParams().getArgs();
  auto res = context.getResults().getRes();
  std::cout << "snapshot args" << args.toString() << std::endl;
  auto reply = impl->snapshot(args, res);
  return reply.then([res = std::move(res)]() {
    std::cout << "snapshot res" << res.toString() << std::endl;
  });
}

kj::Promise<void> Server::vote(VoteContext context) {
  auto args = context.getParams().getArgs();
  auto res = context.getResults().getRes();
  std::cout << "vote args" << args.toString() << std::endl;
  auto reply = impl->vote(args, res);
  return reply.then([res = std::move(res)]() {
    std::cout << "vote res" << res.toString() << std::endl;
  });
}
