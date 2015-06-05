// Copyright (c) 2014 The Pebblecoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#pragma once

#include "cryptonote_core/tx_builder.h"

#include "chaingen.h"
#include "test_chain_unit_base.h"

// check chain switching works for v2 (currency) transactions

struct chain_switch_2_base : public test_chain_unit_base
{
  chain_switch_2_base() : m_invalid_tx_index(0), m_invalid_block_index(0)
                        , m_head_1_index(0), m_head_2_index(0), m_head_3_index(0)
  {
    REGISTER_CALLBACK_METHOD(chain_switch_2_base, mark_invalid_tx);
    REGISTER_CALLBACK_METHOD(chain_switch_2_base, mark_invalid_block);
    REGISTER_CALLBACK_METHOD(chain_switch_2_base, mark_head_1);
    REGISTER_CALLBACK_METHOD(chain_switch_2_base, mark_head_2);
    REGISTER_CALLBACK_METHOD(chain_switch_2_base, mark_head_3);
    REGISTER_CALLBACK_METHOD(chain_switch_2_base, check_on_head_1);
    REGISTER_CALLBACK_METHOD(chain_switch_2_base, check_on_head_2);
    REGISTER_CALLBACK_METHOD(chain_switch_2_base, check_on_head_3);
    REGISTER_CALLBACK_METHOD(chain_switch_2_base, check_not_head_1);
    REGISTER_CALLBACK_METHOD(chain_switch_2_base, check_not_head_2);
    REGISTER_CALLBACK_METHOD(chain_switch_2_base, check_not_head_3);
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
  
#define MAKE_FN_NAME(BASE, VAR) bool BASE ## VAR(core_t& c, size_t ev_index, const std::vector<test_event_entry>& events)
#define FUNCTION_NAME(BASE, VAR) MAKE_FN_NAME(BASE, VAR)
#define MAKE_VAR_NAME(BASE, VAR, TAIL) BASE ## VAR ## TAIL
#define VAR_NAME(BASE, VAR, TAIL) MAKE_VAR_NAME(BASE, VAR, TAIL)
  
#define DEFINE_HEAD_CHECK_CALLBACKS(WHICH) \
  MAKE_FN_NAME(mark_head_, WHICH) { \
    VAR_NAME(m_head_, WHICH, _index) = ev_index + 1; \
    return true; \
  } \
  MAKE_FN_NAME(check_on_head_, WHICH) { \
    const char* perr_context = "check_on_head_" #WHICH; \
    CHECK_TEST_CONDITION(events[VAR_NAME(m_head_, WHICH, _index)].type() == typeid(cryptonote::block)); \
    CHECK_TEST_CONDITION(c.get_tail_id() == get_block_hash(boost::get<cryptonote::block>(events[VAR_NAME(m_head_, WHICH, _index)]))); \
    return true; \
  } \
  MAKE_FN_NAME(check_not_head_, WHICH) { \
    const char* perr_context = "check_not_head_" #WHICH; \
    CHECK_TEST_CONDITION(events[VAR_NAME(m_head_, WHICH, _index)].type() == typeid(cryptonote::block)); \
    CHECK_TEST_CONDITION(c.get_tail_id() != get_block_hash(boost::get<cryptonote::block>(events[VAR_NAME(m_head_, WHICH, _index)]))); \
    return true; \
  }

  DEFINE_HEAD_CHECK_CALLBACKS(1);
  DEFINE_HEAD_CHECK_CALLBACKS(2);
  DEFINE_HEAD_CHECK_CALLBACKS(3);
  
private:
  size_t m_invalid_tx_index;
  size_t m_invalid_block_index;
  size_t m_head_1_index;
  size_t m_head_2_index;
  size_t m_head_3_index;
};

DEFINE_TEST(gen_chainswitch_mint, chain_switch_2_base);
DEFINE_TEST(gen_chainswitch_mint_2, chain_switch_2_base);
DEFINE_TEST(gen_chainswitch_mint_3, chain_switch_2_base);
DEFINE_TEST(gen_chainswitch_remint, chain_switch_2_base);
DEFINE_TEST(gen_chainswitch_remint_2, chain_switch_2_base);

