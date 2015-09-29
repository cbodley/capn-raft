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

namespace std {
ostream &operator<<(ostream &out, const kj::StringPtr &str) {
  return out.write(str.begin(), str.size());
}
} // namespace std

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

  const auto previously_voted = state.voted;
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
      log_term != state.log.back().get().getTerm()) {
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
  std::cout << "vote reply from=" << member << " term=" << term
            << " granted=" << granted << std::endl;
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
  }
}

namespace {
kj::Duration get_election_timeout(const Configuration &config,
                                  std::mt19937 &rng) {
  auto delay_min = config.election_timeout_min.count() * kj::MILLISECONDS;
  auto delay_max = config.election_timeout_max.count() * kj::MILLISECONDS;
  std::uniform_int_distribution<> dist(delay_min / kj::MICROSECONDS,
                                       delay_max / kj::MICROSECONDS);
  return dist(rng) * kj::MICROSECONDS;
}
} // anonymous namespace

kj::Promise<void> Server::election_timeout() {
  KJ_ASSERT(state.member_state != MemberState::Leader,
            "election timeout during leader state");

  start_election();
  if (state.member_state == MemberState::Leader)
    return kj::READY_NOW;

  auto delay = get_election_timeout(config, rng);
  return async.getTimer().afterDelay(delay).then(
      [this]() { return election_timeout(); });
}

void Server::start_election() {
  // TODO: term update needs store_raft_state()
  const auto term = ++state.current_term;
  std::cout << "vote started term=" << term << std::endl;

  state.election_term = term;
  state.votes.clear();
  state.member_state = MemberState::Candidate;

  // vote myself
  add_vote(config.member_id);

  // broadcast a vote request
  std::map<member_t, addr_t> addrs;
  cluster.get_addresses(addrs);

  const struct {
    term_t term;
    member_t candidate;
    log_index_t log_index;
    term_t log_term;
  } vote = {term, config.member_id, get_last_log_index(), get_last_log_term()};

  for (auto &member : addrs) {
    const auto member_id = member.first;
    if (member_id == config.member_id)
      continue;

    const auto &addr = member.second;
    auto connect = network.connect(addr);
    auto send = connect.then([vote](auto client) {
      auto request = client.voteRequest();
      auto args = request.getArgs();
      args.setTerm(vote.term);
      args.setCandidate(vote.candidate);
      args.setLastLogIndex(vote.log_index);
      args.setLastLogTerm(vote.log_term);
      // send, no pipelining
      return request.send().then([](auto reply) { return reply; });
    });
    auto promise = send.then([this, member_id, term](auto reply) {
      return vote_reply(member_id, term, reply.getRes().getVoteGranted());
    }, network.error_handler(addr));

    // insert or overwrite existing promise
    auto reply = vote_replies.emplace(member_id, nullptr);
    std::swap(reply.first->second, promise);

    std::cout << "send vote request to " << member_id << std::endl;
  }
}

void Server::start_election_timer() {
  auto delay = get_election_timeout(config, rng);
  election_timer = async.getTimer().afterDelay(delay).then(
      [this]() { return election_timeout(); });
}

void Server::stop_election() { election_timer = nullptr; }
