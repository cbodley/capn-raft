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
#pragma once

#include <random>

#include <kj/async.h>
#include <capnp/message.h>

#include <raft.h>

#include "log.h"
#include "raft.capnp.h"

namespace kj {
class AsyncIoProvider;
} // namespace kj

namespace raft {
namespace server {

using term_t = uint32_t;

class Cluster;
class Network;
class State;
struct Follower;

class Server {
public:
  Server(const Configuration &config, State &state, Cluster &cluster,
         Network &network, std::mt19937 &rng,
         kj::AsyncIoProvider &async) noexcept;

  kj::Promise<void> append(proto::append::Args::Reader args,
                           proto::append::Res::Builder res);
  kj::Promise<void> command(proto::command::Args::Reader args,
                            proto::command::Res::Builder res) {
    return kj::READY_NOW;
  }
  kj::Promise<void> snapshot(proto::snapshot::Args::Reader args,
                             proto::snapshot::Res::Builder res) {
    return kj::READY_NOW;
  }
  kj::Promise<void> vote(proto::vote::Args::Reader args,
                         proto::vote::Res::Builder res);

  void start_election_timer();

protected:
  void stop_election();

  kj::Promise<void> election_timeout();
  void start_election();

  bool request_vote(term_t term, member_t candidate, uint32_t log_index,
                    term_t log_term);

  kj::Promise<void> vote_reply(member_t from, term_t term, bool granted);
  void add_vote(member_t member);

  void start_leader();
  void stop_leader();
  kj::Promise<void> heartbeat_timeout();

  kj::Promise<void> append_reply(member_t from, proto::append::Res::Reader res);
  void send_append(member_t to, Follower &follower);
  void send_snapshot(member_t to, Follower &follower) {}

  bool update_term(term_t term);

  kj::Promise<void> store_raft_state();

  void follower_matched(member_t id, uint32_t log_index) {}
  kj::Promise<void> commit_through(uint32_t log_index) { return kj::READY_NOW; }

  State &get_state() { return state; }
  const State &get_state() const { return state; }

private:
  uint32_t get_last_log_index() const;
  term_t get_last_log_term() const;

  const Configuration &config;
  State &state;
  Cluster &cluster;
  Network &network;
  std::mt19937 &rng;
  kj::AsyncIoProvider &async;

  capnp::MallocMessageBuilder message; //< allocator for log factory
  LogFactory log_factory;              //< factory for log entries

  kj::Promise<void> election_timer;
  std::map<member_t, kj::Promise<void>> vote_replies;

  kj::Promise<void> heartbeat_timer;
};

} // namespace server
} // namespace raft
