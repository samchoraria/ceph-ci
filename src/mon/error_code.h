// -*- mode:C++; tab-width:8; c-basic-offset:2; indent-tabs-mode:t -*-
// vim: ts=8 sw=2 smarttab
/*
 * Ceph - scalable distributed file system
 *
 * Copyright (C) 2019 Red Hat <contact@redhat.com>
 * Author: Adam C. Emerson <aemerson@redhat.com>
 *
 * This is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License version 2.1, as published by the Free Software
 * Foundation.  See file COPYING.
 *
 */

#pragma once

#include <boost/system/error_code.hpp>

#include "include/rados.h"

const boost::system::error_category& mon_category() noexcept;

// The Monitor, like the OSD, mostly replies with POSIX error codes.

namespace mon_errc {
enum mon_errc_t {
};
}

namespace boost {
namespace system {
template<>
struct is_error_code_enum<::mon_errc::mon_errc_t> {
  static const bool value = true;
};
}
}

namespace mon_errc {
//  explicit conversion:
inline boost::system::error_code make_error_code(mon_errc_t e) noexcept {
  return { e, mon_category() };
}

// implicit conversion:
inline boost::system::error_condition make_error_condition(mon_errc_t e)
  noexcept {
  return { e, mon_category() };
}
}
