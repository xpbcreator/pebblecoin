// Copyright (c) 2015 The Pebblecoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#pragma once

namespace cryptonote
{
  struct bs_delegate_info;

  double get_delegate_rank(const bs_delegate_info& info);
}
