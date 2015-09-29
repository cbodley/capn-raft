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

#include <kj/async-io.h>
#include <kj/debug.h>

#include "cluster.h"
#include "network.h"
#include "server.h"
#include "state.h"

using namespace raft;
using namespace proto;
using namespace server;

kj::Promise<void> Server::append(append::Args::Reader args,
                                 append::Res::Builder res) {
  res.setTerm(state.current_term);
  res.setMatched(0);

  if (args.getTerm() < state.current_term) {
    // an old leader hasn't seen the new term yet, update them
    return kj::READY_NOW;
  }

  start_election_timer(); // restart timer
  state.leader_id = args.getLeader();
  state.last_heard_from_leader = std::chrono::system_clock::now();

  bool need_store = false;

  if (update_term(args.getTerm())) {
    res.setTerm(state.current_term);
    need_store = true;
  }

  if (args.getPrevLogIndex() > state.snap_index + state.log.size()) {
    // i don't have enough of the log to take this one
    return need_store ? store_raft_state() : kj::READY_NOW;
  }
  auto index = args.getPrevLogIndex() - state.snap_index;
  if (index > 0 &&
      args.getPrevLogTerm() != state.log[index - 1]->get().getTerm()) {
    // prev log term doesn't match
    return need_store ? store_raft_state() : kj::READY_NOW;
  }

  // apply the log updates
  if (args.hasEntries()) {
    auto entries = args.getEntries();
    for (auto entry : entries) {
      if (state.log.size() == index) {
        state.log.emplace_back(log_factory.copy(entry));
        need_store = true;
      } else if (state.log[index]->get().getTerm() != entry.getTerm()) {
        // take this entry and discard the rest of our log
        state.log[index] = log_factory.copy(entry);
        state.log.resize(index + 1);
        need_store = true;
      }
      index++;
    }
  }
  res.setMatched(state.snap_index + index);

  // update commit index
  if (state.commit_index < args.getLeaderCommit())
    commit_through(std::min(args.getLeaderCommit(), res.getMatched()));

  return need_store ? store_raft_state() : kj::READY_NOW;
}

kj::Promise<void> Server::append_reply(member_t from,
                                       proto::append::Res::Reader res) {
  if (update_term(res.getTerm()))
    return store_raft_state();

  auto f = state.followers.find(from);
  if (f == state.followers.end()) {
    std::cerr << "handle_append_reply from non-follower " << from << std::endl;
    return kj::READY_NOW;
  }
  auto &follower = f->second;

  if (res.getMatched()) {
    follower.match = res.getMatched();
    auto log_index = get_last_log_index();
    if (follower.match > log_index) {
      std::cerr << "handle_append_reply follower " << from << " matched index "
                << follower.match << " > my log size " << log_index
                << std::endl;
      follower.match = log_index;
    }
    follower_matched(from, follower.match);
    follower.next = follower.match + 1;
    if (follower.match == log_index)
      return kj::READY_NOW; // follower's log is consistent
  } else if (--follower.next == state.snap_index) {
    follower.next = state.snap_index + 1;
    if (state.snap_index > 0)
      send_snapshot(from, follower);
    else if (!state.log.empty())
      std::cerr << "handle_append_reply follower " << from
                << " rejected all entries" << std::endl;
    return kj::READY_NOW;
  }

  send_append(from, follower);
  return kj::READY_NOW;
}

void Server::start_leader() {
  state.member_state = MemberState::Leader;

  // initialize follower state
  state.followers.clear();
  for (auto id : cluster.get_members()) {
    if (id != config.member_id)
      state.followers[id].next = get_last_log_index() + 1;
  }

  // keep ourselves in the set of matches
  state.matched_next_commit_index.insert(config.member_id);

  // broadcast heartbeats
  for (auto &f : state.followers)
    send_append(f.first, f.second);

  std::cout << "starting leader heartbeat timer" << std::endl;
  auto delay = config.heartbeat_interval.count() * kj::MILLISECONDS;
  heartbeat_timer = async.getTimer().afterDelay(delay).then(
      [this]() { return heartbeat_timeout(); });
}

void Server::stop_leader() { heartbeat_timer = nullptr; }

kj::Promise<void> Server::heartbeat_timeout() {
  KJ_REQUIRE(state.member_state == MemberState::Leader,
             "heartbeat timeout during non-leader state");

  // broadcast heartbeats
  for (auto &f : state.followers)
    send_append(f.first, f.second);

  auto delay = config.heartbeat_interval.count() * kj::MILLISECONDS;
  return async.getTimer().afterDelay(delay).then(
      [this]() { return heartbeat_timeout(); });
}

void Server::send_append(member_t to, Follower &follower) {
  struct {
    term_t term;
    member_t leader;
    log_index_t log_index;
    term_t log_term;
    std::vector<log_entry_ptr> entries;
    log_index_t leader_commit;
  } append = {state.current_term, config.member_id, get_last_log_index(),
              get_last_log_term(), state.log, state.commit_index};

  auto addr = cluster.get_address(to);
  auto connect = network.connect(addr);
  auto send = connect.then([append = kj::mv(append)](auto client) {
    auto request = client.appendRequest();
    auto args = request.getArgs();
    args.setTerm(append.term);
    args.setLeader(append.leader);
    args.setPrevLogIndex(append.log_index);
    args.setPrevLogTerm(append.log_term);
    args.setLeaderCommit(append.leader_commit);

    auto entries = args.initEntries(append.entries.size());
    auto dst = entries.begin();
    for (auto entry : append.entries) {
      auto reader = entry->getReader();
      dst->setTerm(reader.getTerm());
      dst->setCommand(reader.getCommand());
      dst->setData(reader.getData()); // XXX: does this copy?
      ++dst;
    }
    // send, no pipelining
    return request.send().then([](auto reply) { return reply; });
  });
  follower.reply = send.then([this, to](auto reply) {
    return append_reply(to, reply.getRes());
  }, network.error_handler(addr));
}
