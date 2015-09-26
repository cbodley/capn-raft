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

#include <raft.h>

namespace raft {
namespace server {

/**
 * Reads the given command-line arguments to initialize a Configuration.
 */
bool parse_config(int argc, const char **argv, Configuration &config);

} // namespace server
} // namespace raft
