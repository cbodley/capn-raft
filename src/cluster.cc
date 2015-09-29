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

#include "cluster.h"

using namespace raft;
using namespace server;

bool Cluster::is_majority(const std::set<member_t> &members) const {
  std::lock_guard<std::mutex> lock(mutex);
  int majority = member_addrs.size() / 2 + 1;
  for (auto m = member_addrs.begin(); m != member_addrs.end(); ++m)
    if (members.count(m->first) && --majority == 0)
      return true;
  return false;
}

member_set Cluster::get_members() const {
  std::lock_guard<std::mutex> lock(mutex);
  member_set members;
  for (auto &m : member_addrs)
    members.insert(m.first);
  return members;
}

addr_t Cluster::get_address(member_t member) const {
  std::lock_guard<std::mutex> lock(mutex);
  auto m = member_addrs.find(member);
  if (m != member_addrs.end())
    return m->second;
  return addr_t();
}

member_addr_map Cluster::get_addresses() const {
  std::lock_guard<std::mutex> lock(mutex);
  return member_addrs;
}
