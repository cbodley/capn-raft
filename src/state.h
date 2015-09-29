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

#include <map>
#include <memory>
#include <set>
#include <vector>

#include <kj/async.h>

#include <raft.h>

#include "log.h"

namespace raft {
namespace server {

enum class MemberState {
  Follower,
  Candidate,
  Leader,
};

struct Follower {
  log_index_t next;
  log_index_t match;
  uint64_t snap_offset;
  kj::Promise<void> reply = nullptr; /// pending append or snapshot reply
};

struct State {
  // persistent state
  term_t current_term = 0;
  bool voted = false;
  member_t voted_for;
  log_index_t snap_index = 0;
  term_t snap_term = 0;
  uint64_t snap_size = 0;
  std::vector<log_entry_ptr> log;

  // volatile state
  MemberState member_state = MemberState::Follower;
  log_index_t commit_index = 0;
  log_index_t last_applied = 0;
  member_t leader_id;
  std::chrono::system_clock::time_point last_heard_from_leader;

  // candidate state
  term_t election_term = 0;
  std::set<member_t> votes;

  // leader state
  std::map<member_t, Follower> followers;
  std::set<member_t> matched_next_commit_index;
};

} // namespace server
} // namespace raft
