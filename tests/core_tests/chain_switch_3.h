// Copyright (c) 2014 The Pebblecoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#pragma once

#include "cryptonote_core/tx_builder.h"

#include "chaingen.h"
#include "test_chain_unit_base.h"
#include "contracts.h"
#include "currency.h"

// check chain switching works for v3 (contract) transactions

struct chain_switch_3_base : public test_chain_unit_base
{
  chain_switch_3_base() : m_invalid_tx_index(0), m_invalid_block_index(0)
                          , m_head_1_index(0), m_head_2_index(0), m_head_3_index(0)
  {
    REGISTER_CALLBACK_METHOD(chain_switch_3_base, mark_invalid_tx);
    REGISTER_CALLBACK_METHOD(chain_switch_3_base, mark_invalid_block);
    REGISTER_CALLBACK_METHOD(chain_switch_3_base, mark_head_1);
    REGISTER_CALLBACK_METHOD(chain_switch_3_base, mark_head_2);
    REGISTER_CALLBACK_METHOD(chain_switch_3_base, mark_head_3);
    REGISTER_CALLBACK_METHOD(chain_switch_3_base, check_on_head_1);
    REGISTER_CALLBACK_METHOD(chain_switch_3_base, check_on_head_2);
    REGISTER_CALLBACK_METHOD(chain_switch_3_base, check_on_head_3);
    REGISTER_CALLBACK_METHOD(chain_switch_3_base, check_not_head_1);
    REGISTER_CALLBACK_METHOD(chain_switch_3_base, check_not_head_2);
    REGISTER_CALLBACK_METHOD(chain_switch_3_base, check_not_head_3);
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

#define DEFINE_CREATE_TEST(TEST_NAME, t_id, t_first, t_second, t_third)   \
struct TEST_NAME : public chain_switch_3_base                             \
{                                                                         \
  virtual bool generate(std::vector<test_event_entry>& events) const              \
  {                                                                       \
    uint64_t ts_start = 1338224400;                                       \
                                                                          \
    GENERATE_ACCOUNT(miner_account);                                      \
    GENERATE_ACCOUNT(alice);                                              \
                                                                          \
    MAKE_STARTING_BLOCKS(events, blk_0, miner_account, ts_start);         \
                                                                          \
    keypair A = keypair::generate();                                      \
                                                                          \
    cryptonote::transaction tx1, tx2, tx3;                                \
    if (t_first)                                                          \
    {                                                                     \
      CREATE_CONTRACT_TX(events, 0xbeef, "hello", tx, A.pub); tx1 = tx;   \
    }                                                                     \
    else                                                                  \
    {                                                                     \
      MAKE_MINT_TX(events, 0xbeef, "hello", 4000, alice, tx, 2, A.pub); tx1 = tx;   \
    }                                                                     \
                                                                          \
    DO_CALLBACK(events, "mark_head_1");                                   \
    MAKE_NEXT_BLOCK_TX1(events, blk_1, blk_0, miner_account, tx1);        \
                                                                          \
    DO_CALLBACK(events, "check_on_head_1");                               \
    REWIND_BLOCKS_N(events, b_blk_2, blk_0, miner_account, 2);            \
    DO_CALLBACK(events, "check_not_head_1");                              \
                                                                          \
    SET_EVENT_VISITOR_SETT(events, event_visitor_settings::set_txs_keeped_by_block, true);                             \
    if (t_second)                                                         \
    {                                                                     \
      CREATE_CONTRACT_TX(events, t_id ? 0xbeef : 0xf00d, t_id ? "whatever" : "hello", tx, A.pub); tx2 = tx;            \
    }                                                                     \
    else                                                                  \
    {                                                                     \
      MAKE_MINT_TX(events, t_id ? 0xbeef : 0xf00d, t_id ? "whatever" : "hello", 4000, alice, tx, 2, A.pub); tx2 = tx;  \
    }                                                                     \
                                                                          \
    MAKE_NEXT_BLOCK_TX1(events, b_blk_3, b_blk_2, miner_account, tx2);    \
    SET_EVENT_VISITOR_SETT(events, event_visitor_settings::set_txs_keeped_by_block, false);   \
                                                                          \
    if (t_third)                                                          \
    {                                                                     \
      CREATE_CONTRACT_TX(events, t_id ? 0xf00d : 0xbeef, t_id ? "hello" : "nomnoms", tx, A.pub); tx3 = tx;             \
    }                                                                     \
    else                                                                  \
    {                                                                     \
      MAKE_MINT_TX(events, t_id ? 0xf00d : 0xbeef, t_id ? "hello" : "nomnoms", 4000, alice, tx, 2, A.pub); tx3 = tx;   \
    }                                                                     \
                                                                          \
    MAKE_NEXT_BLOCK_TX1(events, b_blk_4, b_blk_3, miner_account, tx3);    \
                                                                          \
    return true;                                                          \
  }                                                                       \
};

DEFINE_TEST(gen_chainswitch_txin_to_key, chain_switch_3_base);
DEFINE_CREATE_TEST(gen_chainswitch_contract_create_id_1, true, true, true, true);
DEFINE_CREATE_TEST(gen_chainswitch_contract_create_id_2, true, true, false, true);
DEFINE_CREATE_TEST(gen_chainswitch_contract_create_id_3, true, false, true, false);
DEFINE_CREATE_TEST(gen_chainswitch_contract_create_id_4, true, true, true, false);
DEFINE_CREATE_TEST(gen_chainswitch_contract_create_descr_1, false, true, true, true);
DEFINE_CREATE_TEST(gen_chainswitch_contract_create_descr_2, false, true, false, true);
DEFINE_CREATE_TEST(gen_chainswitch_contract_create_descr_3, false, false, true, false);
DEFINE_CREATE_TEST(gen_chainswitch_contract_create_descr_4, false, false, false, true);
DEFINE_TEST(gen_chainswitch_contract_create_send, chain_switch_3_base);
DEFINE_TEST(gen_chainswitch_contract_grade, chain_switch_3_base);

