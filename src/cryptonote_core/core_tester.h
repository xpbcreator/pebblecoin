// Copyright (c) 2014-2015 The Pebblecoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#pragma once

#include "common/types.h"

namespace cryptonote
{
  struct bs_delegate_info;
  
  class core_tester
  {
  public:
    core_tester(core_t& core_in);
    
    bool pop_block_from_blockchain();
    bool get_delegate_info(delegate_id_t d_id, bs_delegate_info& inf);
    
  private:
    core_t& m_core;
  };
}
