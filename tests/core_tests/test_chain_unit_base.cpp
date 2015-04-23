// Copyright (c) 2015 The Pebblecoin developers
// Copyright (c) 2012-2013 The Cryptonote developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "cryptonote_core/verification_context.h"
#include "cryptonote_core/cryptonote_basic.h"

#include "test_chain_unit_base.h"

void test_chain_unit_base::register_callback(const std::string& cb_name, verify_callback cb)
{
  m_callbacks[cb_name] = cb;
}

bool test_chain_unit_base::verify(const std::string& cb_name, core_t& c, size_t ev_index,
                                  const std::vector<test_event_entry> &events)
{
  auto cb_it = m_callbacks.find(cb_name);
  if(cb_it == m_callbacks.end())
  {
    LOG_ERROR("Failed to find callback " << cb_name);
    return false;
  }
  return cb_it->second(c, ev_index, events);
}

bool test_chain_unit_base::check_tx_verification_context(const cryptonote::tx_verification_context& tvc,
                                                         bool tx_added, size_t event_index,
                                                         const cryptonote::transaction& tx)
{
  // Default tx verification context check
  if (tvc.m_verifivation_failed)
    throw std::runtime_error("Transaction verification failed");
  
  return true;
}

bool test_chain_unit_base::check_block_verification_context(const cryptonote::block_verification_context& bvc,
                                                            size_t event_idx, const cryptonote::block& block)
{
  // Default block verification context check
  if (bvc.m_verifivation_failed)
    throw std::runtime_error("Block verification failed");
  
  return true;
}
