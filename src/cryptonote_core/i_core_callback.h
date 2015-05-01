// Copyright (c) 2012-2013 The Cryptonote developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#pragma once

#include <cstdint>

namespace cryptonote
{
  struct block;
  
  class i_core_callback
  {
  public:
    virtual void on_new_block_added(uint64_t height, const cryptonote::block& block) {}
  };
}
