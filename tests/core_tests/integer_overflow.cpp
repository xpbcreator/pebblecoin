// Copyright (c) 2012-2013 The Cryptonote developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "chaingen.h"

#include "integer_overflow.h"
#include "cryptonote_core/tx_builder.h"
#include "cryptonote_core/tx_tester.h"

using namespace epee;
using namespace cryptonote;

namespace
{
  void split_miner_tx_outs(transaction& miner_tx, uint64_t amount_1)
  {
    uint64_t total_amount = get_outs_money_amount(miner_tx)[CP_XPB];
    uint64_t amount_2 = total_amount - amount_1;
    txout_target_v target = miner_tx.outs()[0].target;

    miner_tx.clear_outs();

    tx_out out1;
    out1.amount = amount_1;
    out1.target = target;
    miner_tx.add_out(out1, CP_XPB);

    tx_out out2;
    out2.amount = amount_2;
    out2.target = target;
    miner_tx.add_out(out2, CP_XPB);
  }

  void append_tx_source_entry(std::vector<cryptonote::tx_source_entry>& sources, const transaction& tx, size_t out_idx)
  {
    tx_source_entry se;
    se.type = tx_source_entry::InToKey;
    se.cp = tx.out_cp(out_idx);
    se.amount_in = se.amount_out = tx.outs()[out_idx].amount;
    se.outputs.push_back(std::make_pair(0, boost::get<cryptonote::txout_to_key>(tx.outs()[out_idx].target).key));
    se.real_output = 0;
    se.real_out_tx_key = get_tx_pub_key_from_extra(tx);
    se.real_output_in_tx_index = out_idx;

    sources.push_back(se);
  }
}

//======================================================================================================================

gen_uint_overflow_base::gen_uint_overflow_base()
  : m_last_valid_block_event_idx(static_cast<size_t>(-1))
{
  REGISTER_CALLBACK_METHOD(gen_uint_overflow_1, mark_last_valid_block);
}

bool gen_uint_overflow_base::check_tx_verification_context(const cryptonote::tx_verification_context& tvc, bool tx_added, size_t event_idx, const cryptonote::transaction& /*tx*/)
{
  return m_last_valid_block_event_idx < event_idx ? !tx_added && tvc.m_verifivation_failed : tx_added && !tvc.m_verifivation_failed;
}

bool gen_uint_overflow_base::check_block_verification_context(const cryptonote::block_verification_context& bvc, size_t event_idx, const cryptonote::block& /*block*/)
{
  return m_last_valid_block_event_idx < event_idx ? bvc.m_verifivation_failed | bvc.m_marked_as_orphaned : !bvc.m_verifivation_failed;
}

bool gen_uint_overflow_base::mark_last_valid_block(core_t& c, size_t ev_index, const std::vector<test_event_entry>& events)
{
  m_last_valid_block_event_idx = ev_index - 1;
  return true;
}

//======================================================================================================================

bool gen_uint_overflow_1::generate(std::vector<test_event_entry>& events) const
{
  uint64_t ts_start = 1338224400;

  GENERATE_ACCOUNT(miner_account);
  MAKE_GENESIS_BLOCK(events, blk_0, miner_account, ts_start);
  DO_CALLBACK(events, "mark_last_valid_block");
  MAKE_ACCOUNT(events, bob_account);
  MAKE_ACCOUNT(events, alice_account);

  // Problem 1. Miner tx output overflow
  MAKE_MINER_TX_MANUALLY(miner_tx_0, blk_0);
  split_miner_tx_outs(miner_tx_0, MONEY_SUPPLY);
  block blk_1;
  if (!generator.construct_block_manually(blk_1, blk_0, miner_account, test_generator::bf_miner_tx, 0, 0, 0, crypto::hash(), 0, miner_tx_0))
    return false;
  events.push_back(blk_1);

  // Problem 1. Miner tx outputs overflow
  MAKE_MINER_TX_MANUALLY(miner_tx_1, blk_1);
  split_miner_tx_outs(miner_tx_1, MONEY_SUPPLY);
  block blk_2;
  if (!generator.construct_block_manually(blk_2, blk_1, miner_account, test_generator::bf_miner_tx, 0, 0, 0, crypto::hash(), 0, miner_tx_1))
    return false;
  events.push_back(blk_2);

  REWIND_BLOCKS(events, blk_2r, blk_2, miner_account);
  MAKE_TX_LIST_START(events, txs_0, miner_account, bob_account, MONEY_SUPPLY, blk_2r);
  MAKE_TX_LIST(events, txs_0, miner_account, bob_account, MONEY_SUPPLY, blk_2r);
  //MAKE_NEXT_BLOCK_TX_LIST(events, blk_3, blk_2r, miner_account, txs_0);
  cryptonote::block blk_3;
  CHECK_AND_ASSERT_MES(generator.construct_block(blk_3, blk_2r, miner_account, txs_0, true), false, "Failed to make_next_block_tx_list while ignoring overflow");
  events.push_back(blk_3);
  REWIND_BLOCKS(events, blk_3r, blk_3, miner_account);

  // Problem 2. total_fee overflow, block_reward overflow
  std::list<cryptonote::transaction> txs_1;
  // Create txs with huge fee
  txs_1.push_back(construct_tx_with_fee(events, blk_3, bob_account, alice_account, 1, MONEY_SUPPLY - 1));
  txs_1.push_back(construct_tx_with_fee(events, blk_3, bob_account, alice_account, 1, MONEY_SUPPLY - 1));
  //MAKE_NEXT_BLOCK_TX_LIST(events, blk_4, blk_3r, miner_account, txs_1);
  cryptonote::block blk_4;
  CHECK_AND_ASSERT_MES(generator.construct_block(blk_4, blk_3r, miner_account, txs_1, true), false, "Failed to make_next_block_tx_list while ignoring overflow");
  events.push_back(blk_4);

  return true;
}

//======================================================================================================================

bool gen_uint_overflow_2::generate(std::vector<test_event_entry>& events) const
{
  uint64_t ts_start = 1338224400;

  GENERATE_ACCOUNT(miner_account);
  MAKE_STARTING_BLOCKS(events, blk_0, miner_account, ts_start);
  MAKE_ACCOUNT(events, bob_account);
  MAKE_ACCOUNT(events, alice_account);
  REWIND_BLOCKS(events, blk_0r, blk_0, miner_account);
  DO_CALLBACK(events, "mark_last_valid_block");

  // Problem 1. Regular tx outputs overflow
  std::vector<cryptonote::tx_source_entry> sources;
  for (size_t i = 0; i < blk_0.miner_tx.outs().size(); ++i)
  {
    if (TESTS_DEFAULT_FEE < blk_0.miner_tx.outs()[i].amount)
    {
      append_tx_source_entry(sources, blk_0.miner_tx, i);
      break;
    }
  }
  if (sources.empty())
  {
    return false;
  }

  std::vector<cryptonote::tx_destination_entry> destinations;
  const account_public_address& bob_addr = bob_account.get_keys().m_account_address;
  destinations.push_back(tx_destination_entry(CP_XPB, MONEY_SUPPLY, bob_addr));
  destinations.push_back(tx_destination_entry(CP_XPB, MONEY_SUPPLY - 1, bob_addr));
  // sources.front().amount = destinations[0].amount + destinations[2].amount + destinations[3].amount + TESTS_DEFAULT_FEE
  destinations.push_back(tx_destination_entry(CP_XPB, sources.front().amount_out - MONEY_SUPPLY - MONEY_SUPPLY + 1 - TESTS_DEFAULT_FEE, bob_addr));

  cryptonote::transaction tx_1;
  if (!construct_tx(miner_account.get_keys(), sources, destinations, std::vector<uint8_t>(), tx_1, 0))
    return false;
  events.push_back(tx_1);

  //MAKE_NEXT_BLOCK_TX1(events, blk_1, blk_0r, miner_account, tx_1);
  cryptonote::block blk_1;
  {
    std::list<cryptonote::transaction> tx_list;
    tx_list.push_back(tx_1);
    CHECK_AND_ASSERT_MES(generator.construct_block(blk_1, blk_0r, miner_account, tx_list, true), false, "Failed to make_next_block_tx1");
  }
  events.push_back(blk_1);
  
  REWIND_BLOCKS(events, blk_1r, blk_1, miner_account);

  // Problem 2. Regular tx inputs overflow
  sources.clear();
  for (size_t i = 0; i < tx_1.outs().size(); ++i)
  {
    auto& tx_1_out = tx_1.outs()[i];
    if (tx_1_out.amount < MONEY_SUPPLY - 1)
      continue;

    append_tx_source_entry(sources, tx_1, i);
  }

  destinations.clear();
  cryptonote::tx_destination_entry de;
  de.cp = CP_XPB;
  de.addr = alice_account.get_keys().m_account_address;
  de.amount = MONEY_SUPPLY - TESTS_DEFAULT_FEE;
  destinations.push_back(de);
  destinations.push_back(de);

  cryptonote::transaction tx_2;
  cryptonote::tx_builder txb;
  txb.init(0, std::vector<uint8_t>(), true);
  txb.add_send(bob_account.get_keys(), sources, destinations);
  txb.finalize();
  if (!txb.get_finalized_tx(tx_2))
    return false;
  events.push_back(tx_2);

  //MAKE_NEXT_BLOCK_TX1(events, blk_2, blk_1r, miner_account, tx_2);
  cryptonote::block blk_2;
  {
    std::list<cryptonote::transaction> tx_list;
    tx_list.push_back(tx_2);
    CHECK_AND_ASSERT_MES(generator.construct_block(blk_2, blk_1r, miner_account, tx_list, true), false, "Failed to make_next_block_tx1");
  }
  events.push_back(blk_2);

  return true;
}
