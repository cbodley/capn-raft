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

#include <algorithm>
#include <iostream>

#include <capnp/ez-rpc.h>

#include "raft.capnp.h"

namespace std {
ostream &operator<<(ostream &out, const kj::StringTree &strings) {
  strings.visit([&out](auto str) { out.write(str.begin(), str.size()); });
  return out;
}

auto rbegin(const char *s) { return make_reverse_iterator(s + strlen(s)); }
auto rend(const char *s) { return make_reverse_iterator(s); }
} // namespace std

int main(int argc, const char **argv) {
  if (argc < 2) {
    auto name = std::find(std::rbegin(argv[0]), std::rend(argv[0]), '/').base();
    std::cerr << "Usage: " << name << " <address>" << std::endl;
    return 1;
  }

  capnp::EzRpcClient client(argv[1], 13579);

  auto cap = client.getMain<raft::proto::Raft>();
  auto request = cap.commandRequest();
  auto args = request.getArgs();
  args.setSequence(1041);
  args.setPing();

  auto promise = request.send();
  auto response = promise.wait(client.getWaitScope());

  std::cout << response.getRes().toString() << std::endl;
  return 0;
}
