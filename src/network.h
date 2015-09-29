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

#include <raft.h>

#include "raft.capnp.h"

namespace kj {
class Network;
template <class T> class Promise;
} // namespace kj

namespace raft {
namespace server {

class Network {
public:
  Network(kj::Network &network);
  ~Network();

  kj::Promise<proto::Raft::Client> connect(const addr_t &addr);

  auto error_handler(const addr_t &addr) {
    return [this, addr](kj::Exception &&exception) -> kj::Promise<void> {
      if (exception.getType() == kj::Exception::Type::DISCONNECTED)
        disconnect(addr);
      return kj::mv(exception);
    };
  }

private:
  void disconnect(const addr_t &addr);

  kj::Network &network;

  class Client;
  std::map<addr_t, Client> clients;
};

} // namespace server
} // namespace raft
