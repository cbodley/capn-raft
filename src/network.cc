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

#include <kj/async-io.h>
#include <kj/debug.h>
#include <capnp/ez-rpc.h>
#include <capnp/rpc-twoparty.h>

#include "network.h"

using namespace raft;
using namespace server;

void DirectNetwork::add_server(addr_t &addr,
                               kj::Own<proto::Raft::Server> &&server) {
  clients.emplace(addr, kj::heap<proto::Raft::Client>(kj::mv(server)));
}

kj::Promise<proto::Raft::Client> DirectNetwork::connect(const addr_t &addr) {
  auto client = clients.find(addr);
  KJ_REQUIRE(client != clients.end(), "no client with the given address");
  return *client->second.get();
}

class RpcNetwork::Client {
private:
  class Connection {
  public:
    Connection(kj::Own<kj::AsyncIoStream> &&stream)
        : stream(kj::mv(stream)),
          network(*this->stream, capnp::rpc::twoparty::Side::CLIENT),
          rpc(capnp::makeRpcClient(network)), client(getMain()) {}

    proto::Raft::Client getMain() {
      capnp::word scratch[4] = {};
      capnp::MallocMessageBuilder message(scratch);
      auto hostId = message.getRoot<capnp::rpc::twoparty::VatId>();
      hostId.setSide(capnp::rpc::twoparty::Side::SERVER);
      return rpc.bootstrap(hostId).castAs<proto::Raft::Client>();
    }

    kj::Own<kj::AsyncIoStream> stream;
    capnp::TwoPartyVatNetwork network;
    capnp::RpcSystem<capnp::rpc::twoparty::VatId> rpc;
    proto::Raft::Client client;
  };
  kj::Own<Connection> connection;

  enum class Status {
    Disconnected,
    Connecting,
    Connected,
  } status = Status::Disconnected;

  kj::ForkedPromise<proto::Raft::Client> pending_connect;

public:
  Client() : status(Status::Disconnected), pending_connect(nullptr) {}

  kj::Promise<proto::Raft::Client> connect(const addr_t &addr,
                                           kj::Network &network) {
    if (status == Status::Connected)
      return connection->client;

    if (status == Status::Disconnected) {
      status = Status::Connecting;
      // resolve the address
      auto resolving = network.parseAddress(addr, 13579);
      // establish the connection
      // TODO: find a way to prevent connect() from spawning a thread every time
      auto connecting = resolving.then(
          [](auto &&addr) { return addr->connect().attach(kj::mv(addr)); });
      // set up the Connection object
      auto connected = connecting.then([this](auto &&stream) {
        status = Status::Connected;
        connection = kj::heap<Connection>(kj::mv(stream));
        return connection->client;
      });
      // fork on the connected event, so other messages will queue on that
      pending_connect = connected.fork();
    }

    return pending_connect.addBranch();
  }

  void disconnect() {
    status = Status::Disconnected;
    connection = kj::Own<Connection>();
  }
};

RpcNetwork::RpcNetwork(kj::Network &network) : network(network) {}

RpcNetwork::~RpcNetwork() = default;

kj::Promise<proto::Raft::Client> RpcNetwork::connect(const addr_t &addr) {
  auto &client = clients[addr];
  return client.connect(addr, network);
}

void RpcNetwork::disconnect(const addr_t &addr) {
  auto client = clients.find(addr);
  if (client != clients.end())
    client->second.disconnect();
}
