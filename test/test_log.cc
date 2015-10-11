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

#include <kj/async-io.h>
#include <gtest/gtest.h>

using namespace raft;
using namespace proto;
using namespace server;

namespace {

const Configuration config = {};

std::random_device dev;
std::mt19937 rng(dev());

Cluster cluster;

auto async = kj::setupAsyncIo();
Network network(async.provider->getNetwork());

using log_vector = std::vector<log::Entry>;
const log::Entry entry1{0, 0, opaque_t{'a', 'b', 'c'}};
const log::Entry entry2{0, 0, opaque_t{'d', 'e', 'f'}};
const log::Entry entry3{0, 0, opaque_t{'g', 'h', 'i'}};

} // anonymous namespace

TEST(Log, AppendInitEmpty) {
  State state;
  Server server(config, state, cluster, network, rng, *async.provider);

  AppendArgs args = {};
  AppendRes res;
  server.handle_append(args, res);

  ASSERT_EQ(0, res.matched);
}

TEST(Log, AppendInitSingle) {
  NullNetwork network;
  State server(config, &model, &cluster, &network, nullptr);

  AppendArgs args = {};
  args.entries = {entry1};

  AppendRes res;
  server.handle_append(args, res);

  auto &pstate = server.get_persistent_state();
  ASSERT_EQ(1, res.matched);
  ASSERT_EQ(log_vector{entry1}, pstate.log);
}

TEST(Log, AppendEmpty) {
  NullNetwork network;
  State server(config, &model, &cluster, &network, nullptr);

  // initialize the log
  auto &pstate = server.get_persistent_state();
  pstate.log = {entry1};

  AppendArgs args = {};
  args.prev_log_index = 1;

  AppendRes res;
  server.handle_append(args, res);

  ASSERT_EQ(1, res.matched);
  ASSERT_EQ(log_vector{entry1}, pstate.log);
}

TEST(Log, AppendSingle) {
  NullNetwork network;
  State server(config, &model, &cluster, &network, nullptr);

  // initialize the log
  auto &pstate = server.get_persistent_state();
  pstate.log = {entry1};

  AppendArgs args = {};
  args.prev_log_index = 1;
  args.entries = {entry2};

  AppendRes res;
  server.handle_append(args, res);

  ASSERT_EQ(2, res.matched);
  ASSERT_EQ(log_vector({entry1, entry2}), pstate.log);
}

TEST(Log, AppendMultiple) {
  NullNetwork network;
  State server(config, &model, &cluster, &network, nullptr);

  // initialize the log
  auto &pstate = server.get_persistent_state();
  pstate.log = {entry1};

  AppendArgs args = {};
  args.prev_log_index = 1;
  args.entries = {entry2, entry3};

  AppendRes res;
  server.handle_append(args, res);

  ASSERT_EQ(3, res.matched);
  ASSERT_EQ(log_vector({entry1, entry2, entry3}), pstate.log);
}

TEST(Log, AppendOverwrite) {
  const log::Entry a1{0, 0, opaque_t{'a', 'b', 'c'}};
  const log::Entry a2{0, 0, opaque_t{'d', 'e', 'f'}};
  const log::Entry a3{0, 0, opaque_t{'g', 'h', 'i'}};

  const log::Entry b2{1, 0, opaque_t{'d', 'e', 'f'}};

  NullNetwork network;
  State server(config, &model, &cluster, &network, nullptr);

  // initialize the log
  auto &pstate = server.get_persistent_state();
  pstate.log = {a1, a2, a3};

  AppendArgs args = {};
  args.entries = {a1, b2};

  AppendRes res;
  server.handle_append(args, res);

  // when overwriting b2, a3 must be discarded
  ASSERT_EQ(2, res.matched);
  ASSERT_EQ(log_vector({a1, b2}), pstate.log);
}

TEST(Log, AppendOldTerm) {
  NullNetwork network;
  State server(config, &model, &cluster, &network, nullptr);

  auto &pstate = server.get_persistent_state();
  pstate.current_term = 1;

  AppendArgs args = {};
  args.term = 0;

  AppendRes res;
  server.handle_append(args, res);

  ASSERT_EQ(1, res.term);
  ASSERT_EQ(0, res.matched);
}

TEST(Log, AppendNewTerm) {
  NullNetwork network;
  State server(config, &model, &cluster, &network, nullptr);

  auto &pstate = server.get_persistent_state();
  server.set_member_state(Leader);

  AppendArgs args = {};
  args.term = 1;

  AppendRes res;
  server.handle_append(args, res);

  ASSERT_EQ(1, res.term);
  ASSERT_EQ(0, res.matched);
  // append with a new term must update current_term and reset to follower
  ASSERT_EQ(1, pstate.current_term);
  ASSERT_EQ(Follower, server.get_member_state());
}

TEST(Log, AppendPrevIndexTooHigh) {
  NullNetwork network;
  State server(config, &model, &cluster, &network, nullptr);

  AppendArgs args = {};
  args.prev_log_index = 1;

  AppendRes res;
  server.handle_append(args, res);

  ASSERT_EQ(0, res.matched);
}

TEST(Log, AppendPrevTermMismatch) {
  NullNetwork network;
  State server(config, &model, &cluster, &network, nullptr);

  // initialize the log
  auto &pstate = server.get_persistent_state();
  pstate.log = {entry1};

  AppendArgs args = {};
  args.prev_log_index = 1;
  args.prev_log_term = 1;

  AppendRes res;
  server.handle_append(args, res);

  ASSERT_EQ(0, res.term);
  ASSERT_EQ(0, res.matched);
}

TEST(Log, AppendCommitIndex) {
  struct CommitModel : public NullModel {
    int commits;
    CommitModel() : commits(0) {}
    void apply(command_t, std::istream &, std::ostream &) override {
      commits++;
    }
  } model;
  NullNetwork network;
  State server(config, &model, &cluster, &network, nullptr);

  // initialize the log
  auto &pstate = server.get_persistent_state();
  pstate.log = {entry1, entry2, entry3};

  const auto &vstate = server.get_volatile_state();
  ASSERT_EQ(0, vstate.commit_index);

  // standard heartbeat append
  AppendArgs args = {};
  args.prev_log_index = 3;
  args.prev_log_term = 0;
  AppendRes res;

  args.leader_commit = 0;
  server.handle_append(args, res);
  ASSERT_EQ(0, vstate.commit_index);
  ASSERT_EQ(0, model.commits);

  args.leader_commit = 1;
  server.handle_append(args, res);
  ASSERT_EQ(1, vstate.commit_index);
  ASSERT_EQ(1, model.commits);

  args.leader_commit = 2;
  server.handle_append(args, res);
  ASSERT_EQ(2, vstate.commit_index);
  ASSERT_EQ(2, model.commits);

  args.leader_commit = 3;
  server.handle_append(args, res);
  ASSERT_EQ(3, vstate.commit_index);
  ASSERT_EQ(3, model.commits);
}

TEST(Log, ReplyEmpty) {
  NoAppendNetwork network;
  State server(config, &model, &cluster, &network, nullptr);

  // initialize the follower state
  const member_t id = 6;
  LeaderState::Follower follower = {1, 0};
  server.get_leader_state().followers.insert(std::make_pair(id, follower));

  // on reply with match=0 and empty log, leader should not resend
  const AppendRes res = {0, 0};
  ASSERT_NO_THROW(server.handle_append_reply(res, id));
}

TEST(Log, ReplyComplete) {
  NoAppendNetwork network;
  State server(config, &model, &cluster, &network, nullptr);

  // initialize the log
  auto &pstate = server.get_persistent_state();
  pstate.log = {entry1, entry2, entry3};

  // initialize the follower state
  const member_t id = 6;
  LeaderState::Follower follower = {3, 2};
  server.get_leader_state().followers.insert(std::make_pair(id, follower));

  // on reply with match=log.size(), leader should not resend
  const AppendRes res = {0, 3};
  ASSERT_NO_THROW(server.handle_append_reply(res, id));
}

TEST(Log, ReplyOneMore) {
  AppendNetwork network;
  State server(config, &model, &cluster, &network, nullptr);

  // initialize the log
  auto &pstate = server.get_persistent_state();
  pstate.log = {entry1, entry2, entry3};

  // initialize the follower state
  const member_t id = 6;
  LeaderState::Follower follower = {3, 2};
  server.get_leader_state().followers.insert(std::make_pair(id, follower));

  const AppendRes res = {0, 2};
  server.handle_append_reply(res, id);

  ASSERT_EQ(2, network.saved_args.prev_log_index);
  ASSERT_EQ(0, network.saved_args.prev_log_term);
  ASSERT_EQ(log_vector{entry3}, network.saved_args.entries);
}

TEST(Log, ReplyTwoMore) {
  AppendNetwork network;
  State server(config, &model, &cluster, &network, nullptr);

  // initialize the log
  auto &pstate = server.get_persistent_state();
  pstate.log = {entry1, entry2, entry3};

  // initialize the follower state
  const member_t id = 6;
  LeaderState::Follower follower = {3, 0};
  server.get_leader_state().followers.insert(std::make_pair(id, follower));

  const AppendRes res = {0, 1};
  server.handle_append_reply(res, id);

  ASSERT_EQ(1, network.saved_args.prev_log_index);
  ASSERT_EQ(0, network.saved_args.prev_log_term);
  ASSERT_EQ(log_vector({entry2, entry3}), network.saved_args.entries);
}

TEST(Log, ReplyNewTerm) {
  NoAppendNetwork network;
  State server(config, &model, &cluster, &network, nullptr);

  // on reply with newer term, leader should not resend and switch to follower
  const AppendRes res = {1, 0};
  const member_t id = 6;
  ASSERT_NO_THROW(server.handle_append_reply(res, id));
  ASSERT_EQ(Follower, server.get_member_state());
}

TEST(Log, ReplyCommitIndex) {
  struct CommitModel : public NullModel {
    int commits;
    CommitModel() : commits(0) {}
    void apply(command_t, std::istream &, std::ostream &) override {
      commits++;
    }
  } model;
  struct MajorityCluster : public NullCluster {
    void get_members(std::set<member_t> &members) override {
      members.insert(1);
      members.insert(2);
      members.insert(3);
    }
    bool is_majority(const std::set<member_t> &members) override {
      return members.size() >= 3;
    }
  } cluster;
  NullNetwork network;
  State server(config, &model, &cluster, &network, nullptr);

  // initialize the log
  auto &pstate = server.get_persistent_state();
  pstate.log = {entry1, entry2, entry3};

  // initialize the follower state
  server.leader_start();

  const auto &vstate = server.get_volatile_state();
  ASSERT_EQ(0, vstate.commit_index);

  // first reply with match=1 should not raise commit_index, not majority
  server.handle_append_reply({0, 1}, 1);
  ASSERT_EQ(0, vstate.commit_index);
  ASSERT_EQ(0, model.commits);
  // duplicate reply from member=1 should not raise commit_index
  server.handle_append_reply({0, 1}, 1);
  ASSERT_EQ(0, vstate.commit_index);
  ASSERT_EQ(0, model.commits);
  // second reply with match=1 should raise commit_index
  server.handle_append_reply({0, 1}, 2);
  ASSERT_EQ(1, vstate.commit_index);
  ASSERT_EQ(1, model.commits);
  // reply with match=2 should not raise commit_index, not majority
  server.handle_append_reply({0, 2}, 2);
  ASSERT_EQ(1, vstate.commit_index);
  ASSERT_EQ(1, model.commits);
  // reply with match=3 should not raise commit_index, member=1 still at 1
  server.handle_append_reply({0, 3}, 2);
  ASSERT_EQ(1, vstate.commit_index);
  ASSERT_EQ(1, model.commits);
  // reply with match=2 should raise commit_index
  server.handle_append_reply({0, 2}, 1);
  ASSERT_EQ(2, vstate.commit_index);
  ASSERT_EQ(2, model.commits);
  // reply with match=3 should raise commit_index
  server.handle_append_reply({0, 3}, 1);
  ASSERT_EQ(3, vstate.commit_index);
  ASSERT_EQ(3, model.commits);
}

TEST(Log, ReplyCommitTerm) {
  struct CommitModel : public NullModel {
    int commits;
    CommitModel() : commits(0) {}
    void apply(command_t, std::istream &, std::ostream &) override {
      commits++;
    }
  } model;
  struct MajorityCluster : public NullCluster {
    void get_members(std::set<member_t> &members) override {
      members.insert(1);
    }
    bool is_majority(const std::set<member_t> &members) override {
      return members.count(1);
    }
  } cluster;
  NullNetwork network;
  State server(config, &model, &cluster, &network, nullptr);

  // initialize the log at term=3
  auto &pstate = server.get_persistent_state();
  pstate.current_term = 3;
  pstate.log = {{0, 0, opaque_t{'1'}},
                {1, 0, opaque_t{'2'}},
                {2, 0, opaque_t{'3'}},
                {3, 0, opaque_t{'4'}},
                {3, 0, opaque_t{'5'}},
                {3, 0, opaque_t{'6'}}};

  // initialize the follower state
  server.leader_start();

  const auto &vstate = server.get_volatile_state();
  ASSERT_EQ(0, vstate.commit_index);

  // should not commit, matched entry from term=0
  server.handle_append_reply({3, 1}, 1);
  ASSERT_EQ(0, vstate.commit_index);
  ASSERT_EQ(0, model.commits);
  // should not commit, matched entry from term=1
  server.handle_append_reply({3, 2}, 1);
  ASSERT_EQ(0, vstate.commit_index);
  ASSERT_EQ(0, model.commits);
  // should not commit, matched entry from term=2
  server.handle_append_reply({3, 3}, 1);
  ASSERT_EQ(0, vstate.commit_index);
  ASSERT_EQ(0, model.commits);
  // should commit everything up to index 4
  server.handle_append_reply({3, 4}, 1);
  ASSERT_EQ(4, vstate.commit_index);
  ASSERT_EQ(4, model.commits);
  // should commit
  server.handle_append_reply({3, 5}, 1);
  ASSERT_EQ(5, vstate.commit_index);
  ASSERT_EQ(5, model.commits);
  // should commit
  server.handle_append_reply({3, 6}, 1);
  ASSERT_EQ(6, vstate.commit_index);
  ASSERT_EQ(6, model.commits);
}
