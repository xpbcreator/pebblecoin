// Copyright (c) 2015 The Pebblecoin developers
// Copyright (c) 2012-2013 The Cryptonote developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#pragma once 

#include "chaingen.h"
#include "test_chain_unit_base.h"

/************************************************************************/
/*                                                                      */
/************************************************************************/
class gen_chain_switch_1 : public test_chain_unit_base
{
public: 
  gen_chain_switch_1();

  virtual bool generate(std::vector<test_event_entry>& events) const;

  bool check_split_not_switched(core_t& c, size_t ev_index, const std::vector<test_event_entry>& events);
  bool check_split_switched(core_t& c, size_t ev_index, const std::vector<test_event_entry>& events);

private:
  std::list<cryptonote::block> m_chain_1;

  cryptonote::account_base m_recipient_account_1;
  cryptonote::account_base m_recipient_account_2;
  cryptonote::account_base m_recipient_account_3;
  cryptonote::account_base m_recipient_account_4;

  std::list<cryptonote::transaction> m_tx_pool;
};

struct chain_switch_1_base : public test_chain_unit_base
{
  chain_switch_1_base() : m_invalid_tx_index(0), m_invalid_block_index(0)
  {
    REGISTER_CALLBACK_METHOD(chain_switch_1_base, mark_invalid_tx);
    REGISTER_CALLBACK_METHOD(chain_switch_1_base, mark_invalid_block);
  }
  
  virtual bool check_tx_verification_context(const cryptonote::tx_verification_context& tvc, bool tx_added, size_t event_idx, const cryptonote::transaction& /*tx*/)
  {
    if (m_invalid_tx_index == event_idx)
      return tvc.m_verifivation_failed;
    else
      return !tvc.m_verifivation_failed && tx_added;
  }
  
  virtual bool check_block_verification_context(const cryptonote::block_verification_context& bvc, size_t event_idx, const cryptonote::block& /*block*/)
  {
    if (m_invalid_block_index == event_idx)
      return bvc.m_verifivation_failed;
    else
      return !bvc.m_verifivation_failed;
  }
  
  bool mark_invalid_block(core_t& /*c*/, size_t ev_index, const std::vector<test_event_entry>& /*events*/)
  {
    m_invalid_block_index = ev_index + 1;
    return true;
  }
  
  bool mark_invalid_tx(core_t& /*c*/, size_t ev_index, const std::vector<test_event_entry>& /*events*/)
  {
    m_invalid_tx_index = ev_index + 1;
    return true;
  }
  
private:
  size_t m_invalid_tx_index;
  size_t m_invalid_block_index;
};

DEFINE_TEST(gen_chainswitch_invalid_1, chain_switch_1_base);
DEFINE_TEST(gen_chainswitch_invalid_2, chain_switch_1_base);
DEFINE_TEST(gen_chainswitch_invalid_checkpoint_rollback, chain_switch_1_base);
DEFINE_TEST(gen_chainswitch_invalid_new_altchain, chain_switch_1_base);
