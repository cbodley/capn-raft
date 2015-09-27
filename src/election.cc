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

#include "cluster.h"
#include "server.h"
#include "state.h"

using namespace raft;
using namespace proto;
using namespace server;

kj::Promise<void> Server::vote(vote::Args::Reader args,
                               vote::Res::Builder res) {
  res.setTerm(state.current_term);
  res.setVoteGranted(false);

  if (args.getTerm() < state.current_term) {
    // an old leader hasn't seen the new term yet, update them
    return kj::READY_NOW;
  }

  auto time_since =
      std::chrono::system_clock::now() - state.last_heard_from_leader;
  if (time_since < config.election_timeout_min) {
    // the current leader is still active
    return kj::READY_NOW;
  }

  bool need_store = false;

  if (update_term(args.getTerm())) {
    res.setTerm(state.current_term);
    need_store = true;
  }

  auto previously_voted = state.voted;
  auto voted = request_vote(args.getTerm(), args.getCandidate(),
                            args.getLastLogIndex(), args.getLastLogTerm());
  res.setVoteGranted(voted);

  if (voted && !previously_voted)
    need_store = true;

  return need_store ? store_raft_state() : kj::READY_NOW;
}

bool Server::request_vote(term_t term, member_t candidate, uint32_t log_index,
                          term_t log_term) {
  if (term < state.current_term) {
    // my term is most recent
    return false;
  }
  if (state.voted && state.voted_for != candidate) {
    // already voted for someone else
    return false;
  }
  if (log_index < state.log.size()) {
    // my log is more recent
    return false;
  }
  if (log_index > 0 && log_index == state.log.size() &&
      log_term != state.log.back().getTerm()) {
    // last log term doesn't match
    return false;
  }

  state.voted = true;
  state.voted_for = candidate;

  start_election_timer();
  return true;
}

kj::Promise<void> Server::vote_reply(member_t member, term_t term,
                                     bool granted) {
  if (update_term(term))
    return store_raft_state();
  if (granted)
    add_vote(member);
  return kj::READY_NOW;
}

void Server::add_vote(member_t member) {
  if (state.member_state != MemberState::Candidate)
    return;
  if (state.current_term != state.election_term)
    return;
  state.votes.insert(member);
  if (cluster.is_majority(state.votes)) {
    state.member_state = MemberState::Leader;
    start_leader();
    std::cout << "won election" << std::endl;
  }
}

void Server::start_election() {
  if (state.member_state == MemberState::Leader)
    return;

  state.current_term++;
  std::cout << "starting election " << state.current_term << std::endl;

  state.election_term = state.current_term;
  state.votes.clear();
  state.member_state = MemberState::Candidate;

  // vote myself
  add_vote(config.member_id);
}

kj::Promise<void> Server::election_timeout() {
  std::cout << "election timeout" << std::endl;
  start_election();
  if (state.member_state == MemberState::Leader)
    return kj::READY_NOW;
  // TODO: randomize election timeout
  auto delay = config.election_timeout_max.count() * kj::MILLISECONDS;
  return async.getTimer().afterDelay(delay).then(
      [this]() { return election_timeout(); });
}

void Server::start_election_timer() {
  std::cout << "starting election timer" << std::endl;

  // TODO: randomize election timeout
  auto delay = config.election_timeout_max.count() * kj::MILLISECONDS;
  election_timer = async.getTimer().afterDelay(delay).then(
      [this]() { return election_timeout(); });
}

void Server::stop_election() {
  std::cout << "stopping election timer" << std::endl;
  election_timer = nullptr;
}