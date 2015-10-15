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

#include <string>
#include <kj/async-io.h>
#include <capnp/message.h>
#include <gtest/gtest.h>

using namespace raft;
using namespace server;

namespace {

const Configuration config = {};

std::random_device dev;
std::mt19937 rng(dev());

Cluster cluster;

auto async = kj::setupAsyncIo();
auto &provider = *async.provider;

capnp::MallocMessageBuilder messages;
LogFactory log_factory(messages.getOrphanage());

struct entry_t {
  term_t term;
  command_t command;
  std::string data;
};
using entry_vec_t = std::vector<entry_t>;

bool operator==(const entry_t &lhs, const log_entry_ptr &rhs) {
  auto r = rhs->get();
  return lhs.term == r.getTerm() && lhs.command == r.getCommand() &&
         lhs.data.compare(r.getData().asChars().begin()) == 0;
}
bool operator==(const entry_vec_t &lhs, const std::vector<log_entry_ptr> &rhs) {
  auto l = lhs.begin();
  auto r = rhs.begin();
  for (; l != lhs.end(); ++l, ++r) {
    if (r == rhs.end())
      return false;
    if (!(*l == *r))
      return false;
  }
  return l == lhs.end() && r == rhs.end();
}

void copy_entry(proto::log::Entry::Builder dst, entry_t &&src) {
  dst.setTerm(src.term);
  dst.setCommand(src.command);
  dst.setData(kj::heapArray<>(src.data.c_str(), src.data.size()).asBytes());
}
} // anonymous namespace

TEST(Log, AppendInitEmpty) {
  State state;
  DirectNetwork network;
  Server server(config, state, cluster, network, rng, messages, provider);
  network.add_server("me", kj::heap<RaftServerAdapter>(server));
  auto connect = network.connect("me");
  auto send = connect.then([](auto client) {
    return client.appendRequest().send().then([](auto reply) { return reply; });
  });
  auto reply = send.wait(async.waitScope);
  auto res = reply.getRes();
  ASSERT_EQ(0, res.getMatched());
  ASSERT_EQ(entry_vec_t{}, state.log);
}

TEST(Log, AppendInitSingle) {
  State state;
  DirectNetwork network;
  Server server(config, state, cluster, network, rng, messages, provider);
  network.add_server("me", kj::heap<RaftServerAdapter>(server));
  auto connect = network.connect("me");
  auto send = connect.then([](auto client) {
    auto request = client.appendRequest();
    auto args = request.getArgs();
    auto entries = args.initEntries(1);
    copy_entry(entries[0], {0, 0, "123"});
    return request.send().then([](auto reply) { return reply; });
  });
  auto reply = send.wait(async.waitScope);
  auto res = reply.getRes();
  ASSERT_EQ(1, res.getMatched());
  ASSERT_EQ(entry_vec_t({{0, 0, "123"}}), state.log);
}

TEST(Log, AppendEmpty) {
  State state;
  state.log.emplace_back(log_factory.create(0, 0, "123", 3));

  DirectNetwork network;
  Server server(config, state, cluster, network, rng, messages, provider);
  network.add_server("me", kj::heap<RaftServerAdapter>(server));
  auto connect = network.connect("me");
  auto send = connect.then([](auto client) {
    auto request = client.appendRequest();
    auto args = request.getArgs();
    args.setPrevLogIndex(1);
    return request.send().then([](auto reply) { return reply; });
  });
  auto reply = send.wait(async.waitScope);
  auto res = reply.getRes();
  ASSERT_EQ(1, res.getMatched());
  ASSERT_EQ(entry_vec_t({{0, 0, "123"}}), state.log);
}

TEST(Log, AppendSingle) {
  State state;
  state.log.emplace_back(log_factory.create(0, 0, "123", 3));

  DirectNetwork network;
  Server server(config, state, cluster, network, rng, messages, provider);
  network.add_server("me", kj::heap<RaftServerAdapter>(server));
  auto connect = network.connect("me");
  auto send = connect.then([](auto client) {
    auto request = client.appendRequest();
    auto args = request.getArgs();
    args.setPrevLogIndex(1);
    auto entries = args.initEntries(1);
    copy_entry(entries[0], {0, 0, "456"});
    return request.send().then([](auto reply) { return reply; });
  });
  auto reply = send.wait(async.waitScope);
  auto res = reply.getRes();
  ASSERT_EQ(2, res.getMatched());
  ASSERT_EQ(entry_vec_t({{0, 0, "123"}, {0, 0, "456"}}), state.log);
}

TEST(Log, AppendMultiple) {
  State state;
  state.log.emplace_back(log_factory.create(0, 0, "123", 3));

  DirectNetwork network;
  Server server(config, state, cluster, network, rng, messages, provider);
  network.add_server("me", kj::heap<RaftServerAdapter>(server));
  auto connect = network.connect("me");
  auto send = connect.then([](auto client) {
    auto request = client.appendRequest();
    auto args = request.getArgs();
    args.setPrevLogIndex(1);
    auto entries = args.initEntries(2);
    copy_entry(entries[0], {0, 0, "456"});
    copy_entry(entries[1], {0, 0, "789"});
    return request.send().then([](auto reply) { return reply; });
  });
  auto reply = send.wait(async.waitScope);
  auto res = reply.getRes();
  ASSERT_EQ(3, res.getMatched());
  ASSERT_EQ(entry_vec_t({{0, 0, "123"}, {0, 0, "456"}, {0, 0, "789"}}),
            state.log);
}

TEST(Log, AppendOverwrite) {
  State state;
  state.log.emplace_back(log_factory.create(0, 0, "abc", 3));
  state.log.emplace_back(log_factory.create(0, 0, "def", 3));
  state.log.emplace_back(log_factory.create(0, 0, "ghi", 3));

  DirectNetwork network;
  Server server(config, state, cluster, network, rng, messages, provider);
  network.add_server("me", kj::heap<RaftServerAdapter>(server));
  auto connect = network.connect("me");
  auto send = connect.then([](auto client) {
    auto request = client.appendRequest();
    auto args = request.getArgs();
    auto entries = args.initEntries(2);
    copy_entry(entries[0], {0, 0, "abc"});
    copy_entry(entries[1], {1, 0, "def"});
    return request.send().then([](auto reply) { return reply; });
  });
  auto reply = send.wait(async.waitScope);
  auto res = reply.getRes();
  // when overwriting b2, a3 must be discarded
  ASSERT_EQ(2, res.getMatched());
  ASSERT_EQ(entry_vec_t({{0, 0, "abc"}, {1, 0, "def"}}), state.log);
}

TEST(Log, AppendOldTerm) {
  State state;
  state.current_term = 1;

  DirectNetwork network;
  Server server(config, state, cluster, network, rng, messages, provider);
  network.add_server("me", kj::heap<RaftServerAdapter>(server));
  auto connect = network.connect("me");
  auto send = connect.then([](auto client) {
    return client.appendRequest().send().then([](auto reply) { return reply; });
  });
  auto reply = send.wait(async.waitScope);
  auto res = reply.getRes();
  ASSERT_EQ(1, res.getTerm());
  ASSERT_EQ(0, res.getMatched());
  ASSERT_EQ(entry_vec_t{}, state.log);
}

TEST(Log, AppendNewTerm) {
  State state;
  state.member_state = MemberState::Leader;

  DirectNetwork network;
  Server server(config, state, cluster, network, rng, messages, provider);
  network.add_server("me", kj::heap<RaftServerAdapter>(server));
  auto connect = network.connect("me");
  auto send = connect.then([](auto client) {
    auto request = client.appendRequest();
    auto args = request.getArgs();
    args.setTerm(1);
    return request.send().then([](auto reply) { return reply; });
  });
  auto reply = send.wait(async.waitScope);
  auto res = reply.getRes();
  ASSERT_EQ(1, res.getTerm());
  ASSERT_EQ(0, res.getMatched());
  ASSERT_EQ(entry_vec_t{}, state.log);
  // append with a new term must update current_term and reset to follower
  ASSERT_EQ(1, state.current_term);
  ASSERT_EQ(MemberState::Follower, state.member_state);
}

TEST(Log, AppendPrevIndexTooHigh) {
  State state;
  DirectNetwork network;
  Server server(config, state, cluster, network, rng, messages, provider);
  network.add_server("me", kj::heap<RaftServerAdapter>(server));
  auto connect = network.connect("me");
  auto send = connect.then([](auto client) {
    auto request = client.appendRequest();
    auto args = request.getArgs();
    args.setPrevLogIndex(1);
    return request.send().then([](auto reply) { return reply; });
  });
  auto reply = send.wait(async.waitScope);
  auto res = reply.getRes();
  ASSERT_EQ(0, res.getTerm());
  ASSERT_EQ(0, res.getMatched());
  ASSERT_EQ(entry_vec_t{}, state.log);
}

TEST(Log, AppendPrevTermMismatch) {
  State state;
  state.log.emplace_back(log_factory.create(0, 0, "abc", 3));

  DirectNetwork network;
  Server server(config, state, cluster, network, rng, messages, provider);
  network.add_server("me", kj::heap<RaftServerAdapter>(server));
  auto connect = network.connect("me");
  auto send = connect.then([](auto client) {
    auto request = client.appendRequest();
    auto args = request.getArgs();
    args.setPrevLogIndex(1);
    args.setPrevLogTerm(1);
    return request.send().then([](auto reply) { return reply; });
  });
  auto reply = send.wait(async.waitScope);
  auto res = reply.getRes();
  ASSERT_EQ(0, res.getTerm());
  ASSERT_EQ(0, res.getMatched());
}
#if 0
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
#endif
