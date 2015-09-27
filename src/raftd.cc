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

#include <iostream>

#include <capnp/ez-rpc.h>

#include "config.h"
#include "server.h"

using namespace raft;
using namespace server;

int main(int argc, const char **argv) {
  Configuration config;
  if (!parse_config(argc, argv, config))
    return 1;

  auto addr = config.member_addrs.find(config.member_id);
  if (addr == config.member_addrs.end()) {
    std::cerr << "No address given for id " << config.member_id << std::endl;
    return 1;
  }

  capnp::EzRpcServer server(kj::heap<Server>(config), addr->second, 13579);
  kj::NEVER_DONE.wait(server.getWaitScope());
  return 0;
}
