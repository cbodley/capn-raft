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
#include <boost/program_options.hpp>

#include "config.h"

namespace po = boost::program_options;
namespace { using member_addr_t = std::pair<raft::member_t, raft::addr_t>; }

namespace std {

istream &operator>>(istream &in, member_addr_t &p) {
  string str;
  in >> str;
  if (!in)
    return in;

  auto pos = str.find('=');
  if (pos == string::npos)
    throw po::error(
        "\"member\" option expected format '<id>=<address>', got: " + str);

  try {
    p.first = boost::lexical_cast<raft::member_t>(str.c_str(), pos);
  } catch (boost::bad_lexical_cast &e) {
    throw po::error("\"member\" option expected integer id, got: " + str);
  }
  p.second.assign(str, pos + 1, string::npos);
  return in;
}

} // namespace std

namespace raft {
namespace server {

bool parse_config(int argc, const char **argv, Configuration &config) {
  using namespace std;

  member_t id;
  uint32_t tmin, tmax, hb, snapsize, chunk;
  string conf, store, snap;

  try {
    po::options_description desc("Allowed options");
    // clang-format off
    desc.add_options()
        ("help,h", "produce help message")
        ("id,i", po::value<member_t>(&id), "member id")
        ("min-timeout", po::value<uint32_t>(&tmin)->default_value(150),
         "minimum election timeout value in milliseconds")
        ("max-timeout", po::value<uint32_t>(&tmax)->default_value(300),
         "maximum election timeout value in milliseconds")
        ("heartbeat", po::value<uint32_t>(&hb)->default_value(60),
         "heartbeat interval in milliseconds")
        ("config,c", po::value<string>(&conf), "configuration filename")
        ("store,s", po::value<string>(&store)->default_value("raft.data"),
         "raft storage filename")
        ("snap-store", po::value<string>(&snap)->default_value("raft.snap"),
         "snapshot storage filename")
        ("snap-size", po::value<uint32_t>(&snapsize)->default_value(32),
         "target log size before starting compaction")
        ("snap-chunk", po::value<uint32_t>(&chunk)->default_value(4096),
         "target log size before starting compaction")
        ("member,m", po::value<vector<member_addr_t>>(),
         "member entry in the format '<id>=<address>'")
        ;
    // clang-format on
    po::variables_map vm;
    po::store(po::parse_command_line(argc, argv, desc), vm);
    po::notify(vm);
    if (!conf.empty())
      po::store(po::parse_config_file<char>(conf.c_str(), desc), vm);
    po::notify(vm);

    if (vm.count("help")) {
      cout << desc;
      return false;
    }
    if (!vm.count("id")) {
      cerr << "The option '--id' is required but missing" << endl;
      cout << desc;
      return false;
    }
    if (tmin > tmax) {
      cerr << "The value for '--min-timeout' (" << tmin
           << ") cannot be larger than '--max-timeout' (" << tmax << ")"
           << endl;
      cout << desc;
      return false;
    }

    config.member_id = static_cast<member_t>(id);
    swap(config.storage_path, store);
    swap(config.snapshot_path, snap);
    config.snapshot_log_size = snapsize;
    config.snapshot_chunk_size = chunk;
    config.election_timeout_min = chrono::milliseconds(tmin);
    config.election_timeout_max = chrono::milliseconds(tmax);
    config.heartbeat_interval = chrono::milliseconds(hb);

    if (vm.count("member")) {
      for (auto &m : vm["member"].as<vector<member_addr_t>>())
        config.member_addrs[m.first] = m.second;
    }
    return true;
  } catch (exception &e) {
    cerr << e.what() << endl;
    return false;
  }
}

} // namespace server
} // namespace raft
