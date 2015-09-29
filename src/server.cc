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

#include "cluster.h"
#include "network.h"
#include "server.h"
#include "state.h"

namespace std {
ostream &operator<<(ostream &out, const kj::StringTree &strings) {
  strings.visit([&out](auto str) { out.write(str.begin(), str.size()); });
  return out;
}
} // namespace std

namespace raft {
using namespace proto;
namespace server {

namespace {
const addr_t &get_bind_addr(const Configuration &config) {
  auto addr = config.member_addrs.find(config.member_id);
  if (addr == config.member_addrs.end())
    throw std::runtime_error("No address given for member id");
  return addr->second;
}
} // anonymous namespace

Server::Server(const Configuration &config, State &state, Cluster &cluster,
               Network &network, std::mt19937 &rng,
               kj::AsyncIoProvider &async) noexcept : config(config),
                                                      state(state),
                                                      cluster(cluster),
                                                      network(network),
                                                      rng(rng),
                                                      async(async),
                                                      election_timer(nullptr) {}

bool Server::update_term(term_t term) {
  if (state.current_term >= term)
    return false;

  state.current_term = term;
  state.voted = false;

  if (state.member_state == MemberState::Leader)
    stop_leader();

  state.member_state = MemberState::Follower;
  start_election_timer();
  return true;
}

kj::Promise<void> Server::store_raft_state() { return kj::READY_NOW; }

class RaftServerAdapter : public proto::Raft::Server {
public:
  RaftServerAdapter(server::Server &server) : server(server) {}

  kj::Promise<void> append(AppendContext context) {
    auto args = context.getParams().getArgs();
    auto res = context.getResults().getRes();
    std::cout << "append args" << args.toString() << std::endl;
    auto reply = server.append(args, res);
    return reply.then([res = std::move(res)]() {
      std::cout << "append res" << res.toString() << std::endl;
    });
  }

  kj::Promise<void> command(CommandContext context) {
    auto args = context.getParams().getArgs();
    auto res = context.getResults().getRes();
    std::cout << "command args" << args.toString() << std::endl;
    auto reply = server.command(args, res);
    return reply.then([res = std::move(res)]() {
      std::cout << "command res" << res.toString() << std::endl;
    });
  }

  kj::Promise<void> snapshot(SnapshotContext context) {
    auto args = context.getParams().getArgs();
    auto res = context.getResults().getRes();
    std::cout << "snapshot args" << args.toString() << std::endl;
    auto reply = server.snapshot(args, res);
    return reply.then([res = std::move(res)]() {
      std::cout << "snapshot res" << res.toString() << std::endl;
    });
  }

  kj::Promise<void> vote(VoteContext context) {
    auto args = context.getParams().getArgs();
    auto res = context.getResults().getRes();
    std::cout << "vote args" << args.toString() << std::endl;
    auto reply = server.vote(args, res);
    return reply.then([res = std::move(res)]() {
      std::cout << "vote res" << res.toString() << std::endl;
    });
  }

private:
  server::Server &server;
};

class Raft::Impl {
public:
  Impl(const Configuration &config, std::mt19937 &rng)
      : cluster(config.member_addrs),
        rpc(kj::heap<RaftServerAdapter>(server), get_bind_addr(config), 13579),
        server(config, state, cluster, network, rng, rpc.getIoProvider()) {}

  Impl(const Impl &) = delete;
  Impl &operator=(const Impl &) = delete;

  void run() {
    server.start_election_timer();
    kj::NEVER_DONE.wait(rpc.getWaitScope());
  }

private:
  State state;
  Cluster cluster;
  Network network;

  capnp::EzRpcServer rpc;
  Server server;
};

// Raft
Raft::Raft(std::nullptr_t) noexcept {}

Raft::Raft(const Configuration &config, std::mt19937 &rng)
    : impl(std::make_unique<Impl>(config, rng)) {}

Raft::~Raft() noexcept = default;

void Raft::run() { return impl->run(); }

} // namespace server
} // namespace raft
