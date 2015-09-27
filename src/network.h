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

#include <capnp/ez-rpc.h>

#include <raft.h>

#include "raft.capnp.h"

namespace raft {
namespace server {

class Network {
public:
  raft::proto::Raft::Client get_client(const addr_t &addr) {
    std::lock_guard<std::mutex> lock(mutex);
    auto &client =
        member_clients.emplace(std::piecewise_construct,
                               std::forward_as_tuple(addr),
                               std::forward_as_tuple(addr)).first->second;
    return client.getMain<raft::proto::Raft>();
  }

private:
  std::mutex mutex;
  std::map<addr_t, capnp::EzRpcClient> member_clients;
};

} // namespace server
} // namespace raft
