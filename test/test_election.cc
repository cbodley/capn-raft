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
#include "server.h"

#include "cluster.h"
#include "network.h"
#include "state.h"

#include <kj/async-io.h>
#include <capnp/message.h>
#include <gtest/gtest.h>

using namespace raft;
using namespace server;

namespace {

const Configuration config = {};

std::random_device dev;
std::mt19937 rng(dev());

class TestServer : public Server {
public:
  using Server::Server;
  using Server::start_election_timer;
  using Server::election_timeout;
  using Server::request_vote;
  using Server::vote_reply;
  using Server::add_vote;
};

class MajorityCluster : public Cluster {
public:
  MajorityCluster(bool majority = true) : majority(majority) {}

  void set_majority(bool majority) { this->majority = majority; }
  bool is_majority(const std::set<member_t> &members) const override {
    return majority;
  }

private:
  bool majority;
};

auto async = kj::setupAsyncIo();
RpcNetwork network(async.provider->getNetwork());

capnp::MallocMessageBuilder message;
LogFactory log_factory(message.getOrphanage());
} // anonymous namespace

// test member state transitions

TEST(Election, StartAsFollower) {
  State state;
  Cluster cluster;
  Server server(config, state, cluster, network, rng, *async.provider);

  ASSERT_EQ(MemberState::Follower, state.member_state);
}

TEST(Election, FollowerTimeout) {
  State state;
  Cluster cluster;
  TestServer server(config, state, cluster, network, rng, *async.provider);

  ASSERT_EQ(MemberState::Follower, state.member_state);
  server.election_timeout();
  ASSERT_EQ(MemberState::Candidate, state.member_state);
}

TEST(Election, CandidateTimeout) {
  State state;
  Cluster cluster;
  TestServer server(config, state, cluster, network, rng, *async.provider);

  state.member_state = MemberState::Candidate;
  ASSERT_EQ(MemberState::Candidate, state.member_state);
  server.election_timeout();
  ASSERT_EQ(MemberState::Candidate, state.member_state);
}

TEST(Election, LeaderTimeout) {
  State state;
  Cluster cluster;
  TestServer server(config, state, cluster, network, rng, *async.provider);

  state.member_state = MemberState::Leader;
  ASSERT_EQ(MemberState::Leader, state.member_state);
  EXPECT_THROW(server.election_timeout(), kj::Exception);
  ASSERT_EQ(MemberState::Leader, state.member_state);
}

TEST(Election, CandidateVoteSelf) {
  State state;
  MajorityCluster cluster;
  TestServer server(config, state, cluster, network, rng, *async.provider);

  ASSERT_EQ(MemberState::Follower, state.member_state);
  server.election_timeout(); // majority is true, so self vote will win
  ASSERT_EQ(MemberState::Leader, state.member_state);
}

TEST(Election, CandidateVoteMajority) {
  State state;
  MajorityCluster cluster(false);
  TestServer server(config, state, cluster, network, rng, *async.provider);

  ASSERT_EQ(MemberState::Follower, state.member_state);
  server.election_timeout(); // majority is false, so self vote doesn't win
  ASSERT_EQ(MemberState::Candidate, state.member_state);
  cluster.set_majority(true);
  server.add_vote(1);
  ASSERT_EQ(MemberState::Leader, state.member_state);
}

TEST(Election, TimeoutEvent) {
  Configuration config = {};
  config.election_timeout_min = std::chrono::milliseconds(20);
  config.election_timeout_max = std::chrono::milliseconds(20);

  State state;
  Cluster cluster;
  TestServer server(config, state, cluster, network, rng, *async.provider);

  server.start_election_timer();

  // run 10ms and assert that we haven't hit the timeout
  auto &timer = async.provider->getTimer();
  timer.afterDelay(10 * kj::MILLISECONDS).wait(async.waitScope);

  ASSERT_EQ(MemberState::Follower, state.member_state);

  // run another 10ms and assert that we did
  timer.afterDelay(10 * kj::MILLISECONDS).wait(async.waitScope);

  ASSERT_EQ(MemberState::Candidate, state.member_state);
}

TEST(Election, NewTermEndsElection) {
  State state;
  Cluster cluster;
  TestServer server(config, state, cluster, network, rng, *async.provider);

  ASSERT_EQ(0, state.current_term);
  ASSERT_EQ(MemberState::Follower, state.member_state);
  server.election_timeout();
  ASSERT_EQ(1, state.current_term);
  ASSERT_EQ(MemberState::Candidate, state.member_state);

  server.vote_reply(0, 2, false);

  ASSERT_EQ(2, state.current_term);
  ASSERT_EQ(MemberState::Follower, state.member_state);
}

TEST(Election, GrantEmptyLog) {
  State state;
  Cluster cluster;
  TestServer server(config, state, cluster, network, rng, *async.provider);

  bool granted = server.request_vote(state.current_term, 0, 0, 0);
  ASSERT_TRUE(granted);
}

TEST(Election, GrantMatchingLog) {
  State state;
  Cluster cluster;
  TestServer server(config, state, cluster, network, rng, *async.provider);

  state.log.emplace_back(log_factory.create(state.current_term, 0, {}));

  bool granted = server.request_vote(state.current_term, 0, 1, 0);
  ASSERT_TRUE(granted);
}

TEST(Election, DenyIfOlderTerm) {
  State state;
  Cluster cluster;
  TestServer server(config, state, cluster, network, rng, *async.provider);

  state.current_term = 2;

  bool granted = server.request_vote(state.current_term - 1, 0, 2, 1);
  ASSERT_FALSE(granted);
  ASSERT_EQ(2, state.current_term);
}

TEST(Election, DenyIfVotedForOther) {
  State state;
  Cluster cluster;
  TestServer server(config, state, cluster, network, rng, *async.provider);

  state.voted = true;
  state.voted_for = 0;

  bool granted =
      server.request_vote(state.current_term, state.voted_for + 1, 2, 1);
  ASSERT_FALSE(granted);
}

TEST(Election, DenyIfLowerLogIndex) {
  State state;
  Cluster cluster;
  TestServer server(config, state, cluster, network, rng, *async.provider);

  state.log.emplace_back(log_factory.create(state.current_term, 0, {}));

  bool granted = server.request_vote(state.current_term, 0, 0, 0);
  ASSERT_FALSE(granted);
}

TEST(Election, DenyIfDifferentLogTerm) {
  State state;
  Cluster cluster;
  TestServer server(config, state, cluster, network, rng, *async.provider);

  state.log.emplace_back(log_factory.create(state.current_term, 0, {}));

  bool granted = server.request_vote(state.current_term, 0, 1, 1);
  ASSERT_FALSE(granted);
}
