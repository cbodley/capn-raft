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

#include <raft.h>

#include "log.capnp.h"

namespace raft {
namespace server {

using term_t = uint32_t;
using log_index_t = uint32_t;
using command_t = uint32_t;

/// Log entries are cached in State without any associated message, so they're
/// represented by capnp as orphans.
using log_entry_t = capnp::Orphan<proto::log::Entry>;
/// Use a shared pointer so they can be shared with AppendEntries message.
using log_entry_ptr = std::shared_ptr<log_entry_t>;

class LogFactory {
public:
  LogFactory(capnp::Orphanage &&orphanage) noexcept
      : orphanage(kj::mv(orphanage)) {}

  template <class... Args>
  log_entry_ptr create(term_t term, command_t command, Args &&... args) {
    auto data = kj::heapArray(kj::fwd<Args>(args)...);
    auto entry = orphanage.newOrphan<proto::log::Entry>();
    entry.get().setTerm(term);
    entry.get().setCommand(command);
    entry.get().setData(data.asBytes());
    return std::make_shared<log_entry_t>(kj::mv(entry));
  }

  log_entry_ptr copy(proto::log::Entry::Reader reader) {
    return std::make_shared<log_entry_t>(orphanage.newOrphanCopy(reader));
  }

private:
  capnp::Orphanage orphanage;
};

} // namespace server
} // namespace raft
