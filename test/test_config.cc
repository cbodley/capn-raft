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
#include "config.h"
#include <gtest/gtest.h>

using namespace raft;
using namespace server;

template <typename T, size_t S> inline size_t arraysize(const T(&v)[S]) {
  return S;
}

TEST(Config, ParseCmdLine) {
  const char *argv[] = {
      "./test_config", "--id=3", "--min-timeout=100", "--max-timeout=200",
      "--store=raft_test.data",
  };
  const int argc = arraysize(argv);

  Configuration config = {};
  ASSERT_TRUE(parse_config(argc, argv, config));
  ASSERT_EQ(3, config.member_id);
  ASSERT_EQ(100, config.election_timeout_min.count());
  ASSERT_EQ(200, config.election_timeout_max.count());
  ASSERT_EQ("raft_test.data", config.storage_path);
  ASSERT_TRUE(config.member_addrs.empty());
}

TEST(Config, ParseConfig) {
  const char *argv[] = {
      "./test_config", "--config=test_config.conf",
  };
  const int argc = arraysize(argv);

  Configuration config = {};
  ASSERT_TRUE(parse_config(argc, argv, config));
  ASSERT_EQ(6, config.member_id);
  ASSERT_EQ(200, config.election_timeout_min.count());
  ASSERT_EQ(250, config.election_timeout_max.count());
  ASSERT_EQ("raft_test_from_config.data", config.storage_path);
  ASSERT_TRUE(config.member_addrs.empty());
}

TEST(Config, CmdLinePrecedence) {
  const char *argv[] = {
      "./test_config", "--id=3", "--min-timeout=100", "--max-timeout=200",
      "--config=test_config.conf", "--store=raft_test.data",
  };
  const int argc = arraysize(argv);
  Configuration config = {};
  ASSERT_TRUE(parse_config(argc, argv, config));
  ASSERT_EQ(3, config.member_id);
  ASSERT_EQ(100, config.election_timeout_min.count());
  ASSERT_EQ(200, config.election_timeout_max.count());
  ASSERT_EQ("raft_test.data", config.storage_path);
  ASSERT_TRUE(config.member_addrs.empty());
}

TEST(Config, AllowTimeoutMinEqualMax) {
  const char *argv[] = {
      "./test_config", "--id=3", "--min-timeout=200", "--max-timeout=200",
  };
  const int argc = arraysize(argv);

  Configuration config = {};
  ASSERT_TRUE(parse_config(argc, argv, config));
}

TEST(Config, FailOnTimeoutMinGreaterMax) {
  const char *argv[] = {
      "./test_config", "--id=3", "--min-timeout=200", "--max-timeout=100",
  };
  const int argc = arraysize(argv);

  Configuration config = {};
  ASSERT_FALSE(parse_config(argc, argv, config));
}

TEST(Config, FailOnMissingId) {
  const char *argv[] = {
      "./test_config", "--timeout=100", "--store=raft_test.data",
  };
  const int argc = arraysize(argv);

  Configuration config = {};
  ASSERT_FALSE(parse_config(argc, argv, config));
}

TEST(Config, Members) {
  const char *argv[] = {
      "./test_config", "--id=3", "--member=6=6", "--member=1=1",
  };
  const int argc = arraysize(argv);

  Configuration config = {};
  ASSERT_TRUE(parse_config(argc, argv, config));

  using addr_map = decltype(config.member_addrs);
  ASSERT_EQ(addr_map({{6, "6"}, {1, "1"}}), config.member_addrs);
}

TEST(Config, MemberBadValue) {
  const char *argv[] = {
      "./test_config", "--id=3", "--member=applesauce",
  };
  const int argc = arraysize(argv);

  Configuration config = {};
  ASSERT_FALSE(parse_config(argc, argv, config));
}

TEST(Config, MemberBadId) {
  const char *argv[] = {
      "./test_config", "--id=3", "--member=apple=sauce",
  };
  const int argc = arraysize(argv);

  Configuration config = {};
  ASSERT_FALSE(parse_config(argc, argv, config));
}
