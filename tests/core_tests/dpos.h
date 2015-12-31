// Copyright (c) 2015 The Pebblecoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#pragma once

#include "cryptonote_core/tx_builder.h"

#include "chaingen.h"
#include "test_chain_unit_base.h"

struct dpos_base : public test_chain_unit_base
{
  dpos_base() : m_invalid_tx_index(0), m_invalid_block_index(0) //, p_events(NULL)
  {
    REGISTER_CALLBACK_METHOD(dpos_base, mark_invalid_tx);
    REGISTER_CALLBACK_METHOD(dpos_base, mark_invalid_block);
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

using namespace epee;
using namespace crypto;
using namespace cryptonote;

template <class txb_call_t, class tx_modifier_t>
bool make_register_delegate_tx_(std::vector<test_event_entry>& events, transaction& tx,
                               delegate_id_t delegate_id, uint64_t registration_fee,
                               const account_base delegate_source,
                               const block& head,
                               const txb_call_t& txb_call,
                               const tx_modifier_t& mod)
{
  transaction tx_register_delegate;
  tx_builder txb;
  txb.init();
  
  std::vector<tx_source_entry> sources;
  CHECK_AND_ASSERT_MES(fill_tx_sources(sources, events, head, delegate_source, registration_fee, 0, CP_XPB),
                       false, "Couldn't fill sources");
  
  std::vector<tx_destination_entry> destinations;
  uint64_t cash_back = get_inputs_amount(sources)[CP_XPB] - registration_fee;
  if (cash_back > 0)
  {
    destinations.push_back(tx_destination_entry(CP_XPB, cash_back,
                                                delegate_source.get_keys().m_account_address));
  }
  
  CHECK_AND_ASSERT_MES(txb.add_register_delegate(delegate_id, delegate_source.get_keys().m_account_address,
                                                 registration_fee,
                                                 delegate_source.get_keys(),
                                                 sources, destinations),
                       false, "add_register_delegate");
  txb_call(txb);
  CHECK_AND_ASSERT_MES(txb.finalize(mod), false, "Couldn't finalize");
  CHECK_AND_ASSERT_MES(txb.get_finalized_tx(tx_register_delegate), false, "Couldn't get finalized tx");
  
  events.push_back(tx_register_delegate);
  tx = tx_register_delegate;
  return true;
}

template <class txb_call_t, class tx_modifier_t>
bool make_vote_tx_(std::vector<test_event_entry>& events, transaction& tx,
                  uint64_t amount, uint16_t seq, const delegate_votes& votes,
                  const account_base vote_source,
                  const block& head, size_t nmix,
                  const txb_call_t& txb_call,
                  const tx_modifier_t& mod)
{
  tx_builder txb;
  txb.init();
  
  std::vector<tx_source_entry> sources;
  CHECK_AND_ASSERT_MES(fill_tx_sources(sources, events, head, vote_source, amount, nmix, CP_XPB),
                       false, "Couldn't fill sources");
  CHECK_AND_ASSERT_MES(txb.add_vote(vote_source.get_keys(),
                                    sources,
                                    seq, votes), false, "could not add_vote");
  txb_call(txb);
  CHECK_AND_ASSERT_MES(txb.finalize(mod), false, "Couldn't finalize");
  transaction vote_tx;
  CHECK_AND_ASSERT_MES(txb.get_finalized_tx(vote_tx), false, "Couldn't get finalized tx");
  
  events.push_back(vote_tx);
  tx = vote_tx;
  return true;
}

#define CREATE_REGISTER_DELEGATE_TX_FULL(VEC_EVENTS, TX_NAME, DELEGATE_ID, REG_FEE, FROM, TX_FEE, HEAD, TXBCALL, MOD) \
  cryptonote::transaction TX_NAME; \
  CHECK_AND_ASSERT_MES(make_register_delegate_tx_(VEC_EVENTS, TX_NAME, DELEGATE_ID, REG_FEE, FROM, HEAD, TXBCALL, MOD), false, \
                       "could not make_register_delegate_tx"); \

#define CREATE_REGISTER_DELEGATE_TX(VEC_EVENTS, TX_NAME, DELEGATE_ID, REG_FEE, FROM, HEAD) CREATE_REGISTER_DELEGATE_TX_FULL(VEC_EVENTS, TX_NAME, DELEGATE_ID, REG_FEE, FROM, 0, HEAD, tools::identity(), tools::identity())

#define CREATE_VOTE_TX_FULL(VEC_EVENTS, TX_NAME, AMOUNT, FROM, HEAD, SEQ, VOTES, NMIX, TXBCALL, MOD) \
  cryptonote::transaction TX_NAME; \
  CHECK_AND_ASSERT_MES(make_vote_tx_(VEC_EVENTS, TX_NAME, AMOUNT, SEQ, VOTES, FROM, HEAD, NMIX, TXBCALL, MOD), false, "could not make_vote_tx");

#define CREATE_VOTE_TX_1(VEC_EVENTS, TX_NAME, AMOUNT, FROM, HEAD, DELEGATE_ID) \
  cryptonote::transaction TX_NAME; \
  { \
    delegate_votes votes; \
    votes.insert(DELEGATE_ID); \
    CREATE_VOTE_TX_FULL(VEC_EVENTS, TX_NAME ## _tmp, AMOUNT, FROM, HEAD, 0, votes, 0, tools::identity(), tools::identity()); \
    TX_NAME = TX_NAME ## _tmp; \
  }

#define INIT_DPOS_TEST() \
  GENERATE_ACCOUNT(miner_account); \
  GENERATE_ACCOUNT(alice); \
  GENERATE_ACCOUNT(bob); \
  GENERATE_ACCOUNT(carol); \
  GENERATE_ACCOUNT(dave); \
  GENERATE_ACCOUNT(emily); \
  MAKE_STARTING_BLOCKS(events, blk_0, miner_account, 1338224400); \
  REWIND_BLOCKS(events, blk_0r, blk_0, miner_account); \
  set_dpos_registration_start_block(events, 1);

#define INIT_DPOS_TEST2() \
  INIT_DPOS_TEST(); \
  

DEFINE_TEST(gen_dpos_register, dpos_base);
DEFINE_TEST(gen_dpos_register_too_soon, dpos_base);
DEFINE_TEST(gen_dpos_register_invalid_id, dpos_base);
DEFINE_TEST(gen_dpos_register_invalid_id_2, dpos_base);
DEFINE_TEST(gen_dpos_register_invalid_address, dpos_base);
DEFINE_TEST(gen_dpos_register_low_fee, dpos_base);
DEFINE_TEST(gen_dpos_register_low_fee_2, dpos_base);
DEFINE_TEST(gen_dpos_vote, dpos_base);
DEFINE_TEST(gen_dpos_vote_too_soon, dpos_base);
DEFINE_TEST(gen_dpos_switch_to_dpos, dpos_base);
DEFINE_TEST(gen_dpos_altchain_voting, dpos_base);
DEFINE_TEST(gen_dpos_altchain_voting_invalid, dpos_base);
DEFINE_TEST(gen_dpos_limit_delegates, dpos_base);
DEFINE_TEST(gen_dpos_unapply_votes, dpos_base);
DEFINE_TEST(gen_dpos_limit_votes, dpos_base);
DEFINE_TEST(gen_dpos_change_votes, dpos_base);
DEFINE_TEST(gen_dpos_spend_votes, dpos_base);
DEFINE_TEST(gen_dpos_vote_tiebreaker, dpos_base);
DEFINE_TEST(gen_dpos_delegate_timeout, dpos_base);
DEFINE_TEST(gen_dpos_delegate_timeout_2, dpos_base);
DEFINE_TEST(gen_dpos_timestamp_checks, dpos_base);
DEFINE_TEST(gen_dpos_invalid_votes, dpos_base);
DEFINE_TEST(gen_dpos_receives_fees, dpos_base);
DEFINE_TEST(gen_dpos_altchain_voting_2, dpos_base);
DEFINE_TEST(gen_dpos_altchain_voting_3, dpos_base);
DEFINE_TEST(gen_dpos_altchain_voting_4, dpos_base);
DEFINE_TEST(gen_dpos_speed_test, dpos_base);
