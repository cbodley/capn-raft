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

#include <chrono>
#include <map>
#include <memory>
#include <random>
#include <string>

// "In Search of an Understandable Consensus Algorithm"
// http://ramcloud.stanford.edu/raft.pdf

namespace raft {

using addr_t = std::string;
using member_t = uint32_t;
using member_addr_map = std::map<member_t, addr_t>;

using duration_ms = std::chrono::duration<int, std::milli>;

namespace server {

/// configuration state for starting up a raft instance
struct Configuration {
  member_t member_id;
  std::string storage_path;
  std::string snapshot_path;
  uint32_t snapshot_log_size;
  uint32_t snapshot_chunk_size;
  duration_ms election_timeout_min;
  duration_ms election_timeout_max;
  duration_ms heartbeat_interval;
  member_addr_map member_addrs;
};

class Raft {
public:
  Raft(std::nullptr_t) noexcept;
  Raft(const Configuration &config, std::mt19937 &rng);
  ~Raft() noexcept;

  void run();

  Raft(Raft &&) noexcept = default;
  Raft &operator=(Raft &&) noexcept = default;

  Raft(const Raft &) = delete;
  Raft &operator=(const Raft &) = delete;

private:
  class Impl;
  std::unique_ptr<Impl> impl;
};

} // namespace server
} // namespace raft
