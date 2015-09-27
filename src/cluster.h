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
#include <mutex>
#include <set>

#include <raft.h>

namespace raft {
namespace server {

class Cluster {
public:
  Cluster(const member_addr_map &member_addrs = {})
      : member_addrs(member_addrs) {}

  virtual bool is_majority(const std::set<member_t> &members) const;
  virtual void get_members(std::set<member_t> &members) const;
  virtual addr_t get_address(member_t member) const;
  virtual void get_addresses(member_addr_map &addrs) const;

private:
  mutable std::mutex mutex;
  member_addr_map member_addrs;
};

} // namespace server
} // namespace raft
