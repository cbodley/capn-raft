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

#include <memory>

#include "raft.capnp.h"

#include "config.h"

namespace raft {
namespace server {

class Server final : public proto::Raft::Server {
private:
  class Impl;
  std::unique_ptr<Impl> impl;

public:
  Server(const Configuration &config);
  ~Server();

  Server(const Server &) = delete;
  Server &operator=(const Server &) = delete;

  Server(Server &&) = default;
  Server &operator=(Server &&) = default;

  kj::Promise<void> append(AppendContext context) override;
  kj::Promise<void> command(CommandContext context) override;
  kj::Promise<void> snapshot(SnapshotContext context) override;
  kj::Promise<void> vote(VoteContext context) override;
};

} // namespace server
} // namespace raft
