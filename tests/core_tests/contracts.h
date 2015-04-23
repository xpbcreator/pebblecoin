// Copyright (c) 2014 The Pebblecoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#pragma once

#include "cryptonote_core/tx_builder.h"

#include "chaingen.h"
#include "test_chain_unit_base.h"

struct contracts_base : public test_chain_unit_base
{
  contracts_base() : m_invalid_tx_index(0), m_invalid_block_index(0) //, p_events(NULL)
  {
    REGISTER_CALLBACK_METHOD(contracts_base, mark_invalid_tx);
    REGISTER_CALLBACK_METHOD(contracts_base, mark_invalid_block);
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
  /*
  void init_events(std::vector<test_event_entry>& events)
  {
    p_events = &events;
  }
  
  cryptonote::block& make_genesis_block(const cryptonote::account_base& account, uint64_t timestamp)
  {
    block genesis;
    CHECK_AND_ASSERT_THROW_MES(generator.construct_block(genesis, account, timestamp), "Failed to make genesis block");
    p_events->push_back(genesis);
    return p_events->back();
  }
  
  cryptonote::block& make_next_block(const cryptonote::account_base& account, const cryptonote::block& prev)
  {
    cryptonote::block next;
    CHECK_AND_ASSERT_THROW_MES(generator.construct_block(next, prev, account), "Failed to make_next_block()");
    p_events->push_back(next);
    return p_events->back();
  }
  
  cryptonote::block make_starting_blocks(const cryptonote::account_base& account, uint64_t timestamp)
  {
    generate_genesis_block(account, timestamp);
    
    for (int i=0; i < 3; i++) {
      make_next_block(account, events.back());
    }
    
    return p_events->back();
  }*/
  
private:
  size_t m_invalid_tx_index;
  size_t m_invalid_block_index;
  /*test_generator generator;
  std::vector<test_event_entry>* p_events;*/
};

using namespace epee;
using namespace crypto;
using namespace cryptonote;

template <class tx_modifier_t>
bool make_send_contract_tx(std::vector<test_event_entry>& events, transaction& tx,
                           uint64_t contract_id,
                           const account_base& backing_source, uint64_t amount, uint64_t backing_currency,
                           const account_base& backing_dest, const account_base& contract_dest,
                           const block& head, const tx_modifier_t& mod)
{
  transaction tx_send_contract;
  tx_builder txb;
  txb.init();
  
  std::vector<tx_source_entry> sources;
  CHECK_AND_ASSERT_MES(fill_tx_sources(sources, events, head, backing_source, amount, 0,
                                       coin_type(backing_currency, NotContract, BACKED_BY_N_A)), false,
                       "Couldn't fill sources");
  
  std::vector<tx_destination_entry> destinations;
  destinations.push_back(tx_destination_entry(coin_type(contract_id, BackingCoin, backing_currency),
                                              amount,
                                              backing_dest.get_keys().m_account_address));
  destinations.push_back(tx_destination_entry(coin_type(contract_id, ContractCoin, backing_currency),
                                              amount,
                                              contract_dest.get_keys().m_account_address));
  
  uint64_t cash_back = get_inputs_amount(sources)[coin_type(backing_currency, NotContract, BACKED_BY_N_A)] - amount;
  if (cash_back > 0)
  {
    destinations.push_back(tx_destination_entry(coin_type(backing_currency, NotContract, BACKED_BY_N_A),
                                                cash_back,
                                                backing_source.get_keys().m_account_address));
  }
  
  CHECK_AND_ASSERT_MES(txb.add_mint_contract(backing_source.get_keys(), contract_id, amount,
                                             sources, destinations),
                       false, "Couldn't add_mint_contract");
  CHECK_AND_ASSERT_MES(txb.finalize(mod), false, "Couldn't finalize");
  CHECK_AND_ASSERT_MES(txb.get_finalized_tx(tx_send_contract), false, "Couldn't get finalized tx");
  
  events.push_back(tx_send_contract);
  tx = tx_send_contract;
  return true;
}
  
template <class tx_modifier_t>
bool make_fuse_contract_tx(std::vector<test_event_entry>& events, transaction& tx,
                           uint64_t contract_id, uint64_t amount, uint64_t backing_currency,
                           const account_base& backing_account, const account_base& contract_account,
                           const account_base& dest_account,
                           const block& head, const tx_modifier_t& mod)
{
  transaction tx_fuse_contract;
  tx_builder txb;
  txb.init();
  
  std::vector<tx_source_entry> backing_sources, contract_sources;
  std::vector<tx_destination_entry> destinations;
  
  CoinContractType whiches[2] = {BackingCoin, ContractCoin};
  BOOST_FOREACH(auto which, whiches) {
    auto& account = which == BackingCoin ? backing_account : contract_account;
    auto& source_entries = which == BackingCoin ? backing_sources : contract_sources;
    auto cp = coin_type(contract_id, which, backing_currency);
    CHECK_AND_ASSERT_MES(fill_tx_sources(source_entries, events, head, account, amount, 0, cp),
                         false, "Couldn't fill " << which << " coin sources of " << cp);
    // send back backing/contract coins change
    uint64_t cash_back = get_inputs_amount(source_entries)[cp] - amount;
    if (cash_back > 0)
    {
      destinations.push_back(tx_destination_entry(cp, cash_back, account.get_keys().m_account_address));
    }
  }
  
  destinations.push_back(tx_destination_entry(coin_type(backing_currency, NotContract, BACKED_BY_N_A),
                                              amount, dest_account.get_keys().m_account_address));
  
  CHECK_AND_ASSERT_MES(txb.add_fuse_contract(contract_id, backing_currency, amount,
                                             backing_account.get_keys(), backing_sources,
                                             contract_account.get_keys(), contract_sources,
                                             destinations),
                       false, "Couldn't add_fuse_contract");
  CHECK_AND_ASSERT_MES(txb.finalize(mod), false, "Couldn't finalize");
  CHECK_AND_ASSERT_MES(txb.get_finalized_tx(tx_fuse_contract), false, "Couldn't get finalized tx");
  
  events.push_back(tx_fuse_contract);
  tx = tx_fuse_contract;
  return true;
}

template <class tx_modifier_t>
bool make_grade_contract_tx(std::vector<test_event_entry>& events, transaction& tx,
                            uint64_t contract_id, uint32_t grade, const crypto::secret_key& sec,
                            const account_base& grading_fees_dest,
                            uint64_t grading_fee_amount, uint64_t grading_fee_currency,
                            const tx_modifier_t& mod)
{
  transaction tx_grade_contract;
  tx_builder txb;
  txb.init(0, std::vector<uint8_t>(), true);
  
  std::vector<tx_destination_entry> dests;
  if (grading_fee_amount > 0) {
    dests.push_back(tx_destination_entry(coin_type(grading_fee_currency, NotContract, BACKED_BY_N_A),
                                         grading_fee_amount, grading_fees_dest.get_keys().m_account_address));
  }
  
  CHECK_AND_ASSERT_MES(txb.add_grade_contract(contract_id, grade, sec, dests), false,
                       "Couldn't add_grade_contract");
  CHECK_AND_ASSERT_MES(txb.finalize(mod), false, "Couldn't finalize");
  CHECK_AND_ASSERT_MES(txb.get_finalized_tx(tx_grade_contract), false, "Couldn't get finalized tx");
  
  events.push_back(tx_grade_contract);
  tx = tx_grade_contract;
  return true;
}

#define CREATE_CONTRACT_TX_FULL(VEC_EVENTS, ID, DESCR, TX_NAME, GRADING_KEY_PUB, FEE, EXPIRY, DEFAULT_GRADE, MOD) \
  cryptonote::transaction TX_NAME; \
  { \
    tx_builder txb; \
    CHECK_AND_ASSERT_MES(txb.init(0, std::vector<uint8_t>(), true), false, "create contract couldn't init tx builder"); \
    CHECK_AND_ASSERT_MES(txb.add_contract(ID, DESCR, \
                         GRADING_KEY_PUB, \
                         FEE, EXPIRY, DEFAULT_GRADE), false, "create contract couldn't add_contract"); \
    CHECK_AND_ASSERT_MES(txb.finalize(MOD), false, "create contract couldn't finalize tx"); \
    CHECK_AND_ASSERT_MES(txb.get_finalized_tx(TX_NAME), false, \
                         "Couldn't get finalized contract tx"); \
    VEC_EVENTS.push_back(TX_NAME) ; \
  }

#define CREATE_CONTRACT_TX(VEC_EVENTS, ID, DESCR, TX_NAME, GRADING_KEY_PUB) CREATE_CONTRACT_TX_FULL(VEC_EVENTS, ID, DESCR, TX_NAME, GRADING_KEY_PUB, 0, 10000, 0, tools::identity())

#define SEND_CONTRACT_TX_FULL(VEC_EVENTS, TX_NAME, CONTRACT, SOURCE, AMT, CURRENCY, BACKING_DEST, CONTRACT_DEST, BLK_HEAD, MOD) \
    cryptonote::transaction TX_NAME; \
    CHECK_AND_ASSERT_MES(make_send_contract_tx(VEC_EVENTS, TX_NAME, CONTRACT, SOURCE, AMT, CURRENCY, \
        BACKING_DEST, CONTRACT_DEST, BLK_HEAD, MOD), false, "could not make_send_contract_tx");

#define SEND_CONTRACT_TX(VEC_EVENTS, TX_NAME, CONTRACT, SOURCE, AMT, CURRENCY, BACKING_DEST, CONTRACT_DEST, BLK_HEAD) SEND_CONTRACT_TX_FULL(VEC_EVENTS, TX_NAME, CONTRACT, SOURCE, AMT, CURRENCY, BACKING_DEST, CONTRACT_DEST, BLK_HEAD, tools::identity());

#define FUSE_CONTRACT_TX_FULL(VEC_EVENTS, TX_NAME, CONTRACT, BACKING_CURRENCY, AMT, BACKING_SOURCE, CONTRACT_SOURCE, FUSED_DEST, BLK_HEAD, MOD) \
    cryptonote::transaction TX_NAME; \
    CHECK_AND_ASSERT_MES(make_fuse_contract_tx(VEC_EVENTS, TX_NAME, CONTRACT, AMT, BACKING_CURRENCY, BACKING_SOURCE, CONTRACT_SOURCE, FUSED_DEST, BLK_HEAD, MOD), false, "could not make_fuse_contract_tx");

#define FUSE_CONTRACT_TX(VEC_EVENTS, TX_NAME, CONTRACT, BACKING_CURRENCY, AMT, BACKING_SOURCE, CONTRACT_SOURCE, FUSED_DEST, BLK_HEAD) FUSE_CONTRACT_TX_FULL(VEC_EVENTS, TX_NAME, CONTRACT, BACKING_CURRENCY, AMT, BACKING_SOURCE, CONTRACT_SOURCE, FUSED_DEST, BLK_HEAD, tools::identity());

#define GRADE_CONTRACT_TX_FULL(VEC_EVENTS, TX_NAME, CONTRACT, GRADE, GRADING_SECRET_KEY, FEES_DEST, FEES_AMT, FEES_CURRENCY, MOD) \
    cryptonote::transaction TX_NAME; \
    CHECK_AND_ASSERT_MES(make_grade_contract_tx(VEC_EVENTS, TX_NAME, CONTRACT, GRADE, GRADING_SECRET_KEY, FEES_DEST, FEES_AMT, FEES_CURRENCY, MOD), false, "Couldn't make_grade_contract_tx");

#define GRADE_CONTRACT_TX(VEC_EVENTS, TX_NAME, CONTRACT, GRADE, GRADING_SECRET_KEY) \
    GRADE_CONTRACT_TX_FULL(VEC_EVENTS, TX_NAME, CONTRACT, GRADE, GRADING_SECRET_KEY, g_junk_account, 0, CURRENCY_XPB, tools::identity())

#define INIT_CONTRACT_TEST() \
  GENERATE_ACCOUNT(miner_account); \
  GENERATE_ACCOUNT(alice); \
  GENERATE_ACCOUNT(bob); \
  GENERATE_ACCOUNT(carol); \
  GENERATE_ACCOUNT(dave); \
  GENERATE_ACCOUNT(emily); \
  MAKE_STARTING_BLOCKS(events, blk_0, miner_account, 1338224400); \
  REWIND_BLOCKS(events, blk_0r, blk_0, miner_account); \
  auto grading_key = keypair::generate();

DEFINE_TEST(gen_contracts_create, contracts_base);
DEFINE_TEST(gen_contracts_create_mint, contracts_base);
DEFINE_TEST(gen_contracts_grade, contracts_base);
DEFINE_TEST(gen_contracts_grade_with_fee, contracts_base);
DEFINE_TEST(gen_contracts_grade_spend_backing, contracts_base);
DEFINE_TEST(gen_contracts_grade_spend_backing_cant_overspend_misgrade, contracts_base);
DEFINE_TEST(gen_contracts_grade_spend_contract, contracts_base);
DEFINE_TEST(gen_contracts_grade_send_then_spend_contract, contracts_base);
DEFINE_TEST(gen_contracts_grade_cant_send_and_spend_contract_1, contracts_base);
DEFINE_TEST(gen_contracts_grade_cant_send_and_spend_contract_2, contracts_base);
DEFINE_TEST(gen_contracts_grade_spend_contract_cant_overspend_misgrade, contracts_base);
DEFINE_TEST(gen_contracts_grade_spend_fee, contracts_base);
DEFINE_TEST(gen_contracts_grade_spend_fee_cant_overspend_misgrade, contracts_base);
DEFINE_TEST(gen_contracts_grade_spend_all_rounding, contracts_base);
DEFINE_TEST(gen_contracts_resolve_backing_cant_change_contract, contracts_base);
DEFINE_TEST(gen_contracts_resolve_backing_expired, contracts_base);
DEFINE_TEST(gen_contracts_resolve_contract_cant_change_contract, contracts_base);
DEFINE_TEST(gen_contracts_resolve_contract_expired, contracts_base);

DEFINE_TEST(gen_contracts_extra_currency_checks, contracts_base);
DEFINE_TEST(gen_contracts_create_checks, contracts_base);
DEFINE_TEST(gen_contracts_mint_checks, contracts_base);
DEFINE_TEST(gen_contracts_grade_checks, contracts_base);
DEFINE_TEST(gen_contracts_resolve_backing_checks, contracts_base);
DEFINE_TEST(gen_contracts_resolve_contract_checks, contracts_base);

DEFINE_TEST(gen_contracts_create_mint_fuse, contracts_base);
DEFINE_TEST(gen_contracts_create_mint_fuse_fee, contracts_base);
DEFINE_TEST(gen_contracts_create_mint_fuse_checks, contracts_base);
