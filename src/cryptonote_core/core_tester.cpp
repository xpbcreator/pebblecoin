// Copyright (c) 2014-2015 The Pebblecoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "include_base_utils.h"

#include "blockchain_storage.h"
#include "cryptonote_core.h"

#include "core_tester.h"

namespace cryptonote
{
  core_tester::core_tester(core_t& core_in) : m_core(core_in) { }
    
  bool core_tester::pop_block_from_blockchain()
  {
    return m_core.m_blockchain_storage.pop_block_from_blockchain();
  }
  
  bool core_tester::get_delegate_info(delegate_id_t d_id, blockchain_storage::delegate_info& inf)
  {
    auto& bs = m_core.m_blockchain_storage;
    CHECK_AND_ASSERT_MES(bs.m_delegates.count(d_id) > 0, false, "Invalid delegate id");
    inf = bs.m_delegates[d_id];
    return true;
  }
}
