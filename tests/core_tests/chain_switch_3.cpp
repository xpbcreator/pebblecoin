// Copyright (c) 2014 The Pebblecoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "contracts.h"
#include "currency.h"
#include "chain_switch_3.h"
#include "cryptonote_core/tx_builder.h"
#include "cryptonote_core/tx_tester.h"
/*
using namespace epee;
using namespace crypto;
using namespace cryptonote;

static account_base g_junk_account;

bool gen_chainswitch_txin_to_key::generate(std::vector<test_event_entry>& events) const
{
  GENERATE_ACCOUNT(miner_account);
  GENERATE_ACCOUNT(alice);
  GENERATE_ACCOUNT(bob);
  MAKE_STARTING_BLOCKS(events, blk_0, miner_account, 1338224400);
  REWIND_BLOCKS(events, blk_0r, blk_0, miner_account);
  
  MAKE_TX(events, tx_alice_money_1, miner_account, alice, 66, blk_0r);
  MAKE_NEXT_BLOCK_TX1(events, blk_1, blk_0r, miner_account, tx_alice_money_1);
  MAKE_TX(events, tx_alice_money_2, miner_account, alice, 33, blk_1);
  MAKE_NEXT_BLOCK_TX1(events, blk_2, blk_1, miner_account, tx_alice_money_2);
  
  // send non-fee 99 to bob, uses both txins
  DONT_MARK_SPENT_TX(events);
  MAKE_TX_MIX_CP_FEE(events, tx_to_bob, alice, bob, 99, 0, blk_2, CP_XPB, 0);
  DO_CALLBACK(events, "mark_head_1");
  MAKE_NEXT_BLOCK_TX1(events, blk_3, blk_2, miner_account, tx_to_bob);
  
  // split from blk_1, alice got first 66 but not last 33
  DO_CALLBACK(events, "check_on_head_1");
  REWIND_BLOCKS_N(events, b_blk_4, blk_1, miner_account, 3);
  DO_CALLBACK(events, "check_not_head_1");

  // sending 99 to bob no longer works
  DO_CALLBACK(events, "mark_invalid_block");
  MAKE_NEXT_BLOCK_TX1(events, b_blk_4_bad_1, b_blk_4, miner_account, tx_to_bob);
  
  // should be able to send 66 to bob...  but it'll already be marked as spent so it won't work
  // unless we force it?
  DO_CALLBACK(events, "mark_invalid_tx");
  MAKE_TX_MIX_CP_FEE(events, tx_to_bob_2, alice, bob, 66, 0, b_blk_4, CP_XPB, 0);
  
  SET_EVENT_VISITOR_SETT(events, event_visitor_settings::set_txs_keeped_by_block, true);
  MAKE_TX_MIX_CP_FEE(events, tx_to_bob_3, alice, bob, 66, 0, b_blk_4, CP_XPB, 0);
  MAKE_NEXT_BLOCK_TX1(events, b_blk_5, b_blk_4, miner_account, tx_to_bob_3);
  SET_EVENT_VISITOR_SETT(events, event_visitor_settings::set_txs_keeped_by_block, false);
  
  return true;
}

bool gen_chainswitch_contract_create_send::generate(std::vector<test_event_entry>& events) const
{
  INIT_CONTRACT_TEST();
  
  MAKE_TX(events, tx_alice_money, miner_account, alice, 99, blk_0r);
  MAKE_NEXT_BLOCK_TX1(events, blk_1, blk_0r, miner_account, tx_alice_money);
  CREATE_CONTRACT_TX_FULL(events, 0xbe7, "Is coin heads or tails", tx_contract, grading_key.pub,
                          GRADE_SCALE_MAX / 100 * 7, 10000, 0, tools::identity());
  MAKE_NEXT_BLOCK_TX1(events, blk_2, blk_1, miner_account, tx_contract);
  
  // send two contracts totaling 99
  SEND_CONTRACT_TX(events, tx_send_contract1, 0xbe7,
                   alice, 33, CURRENCY_XPB,
                   alice, bob, blk_2);
  MAKE_NEXT_BLOCK_TX1(events, blk_3, blk_2, miner_account, tx_send_contract1);
  SEND_CONTRACT_TX(events, tx_send_contract2, 0xbe7,
                   alice, 66, CURRENCY_XPB,
                   alice, bob, blk_3);
  MAKE_NEXT_BLOCK_TX1(events, blk_4, blk_3, miner_account, tx_send_contract2);
  
  // 99 coins -> 6 coin spendable in fees
  GRADE_CONTRACT_TX_FULL(events, tx_grade, 0xbe7, GRADE_SCALE_MAX / 4, grading_key.sec,
                         carol, 6, CURRENCY_XPB, tools::identity());
  MAKE_NEXT_BLOCK_TX1(events, blk_5, blk_4, miner_account, tx_grade);
  
  // alice can spend floor(33*.75) = 24 - ceil(24*.07) = 22
  //               + floor(66*.75) = 49 - ceil(49*.07) = 45
  DONT_MARK_SPENT_TX(events);
  MAKE_TX_MIX_CP_FEE(events, tx_alice_spend, alice, carol, 67, 0, blk_5, CP_XPB, 0)
  // bob can spend floor(33*.25) = 8 - ceil(8*.07) = 7
  //             + floor(66*.25) = 16 - ceil(16*.07) = 14
  DONT_MARK_SPENT_TX(events);
  MAKE_TX_MIX_CP_FEE(events, tx_bob_spend, bob, carol, 21, 0, blk_5, CP_XPB, 0)
  // carol can spend floor(99*.07) = 6
  DONT_MARK_SPENT_TX(events);
  MAKE_TX_MIX_CP_FEE(events, tx_fee_spend, carol, alice, 6, 0, blk_5, CP_XPB, 0)
  
  std::list<transaction> tx_list;
  tx_list.push_back(tx_alice_spend);
  tx_list.push_back(tx_bob_spend);
  tx_list.push_back(tx_fee_spend);
  DO_CALLBACK(events, "mark_head_1");
  MAKE_NEXT_BLOCK_TX_LIST(events, blk_6, blk_5, miner_account, tx_list);
  
  // split from blk_1, money sent but contract no longer exists
  DO_CALLBACK(events, "check_on_head_1");
  REWIND_BLOCKS_N(events, b_blk_7, blk_1, miner_account, 6);
  DO_CALLBACK(events, "check_not_head_1");
  
  DO_CALLBACK(events, "mark_invalid_block");
  MAKE_NEXT_BLOCK_TX1(events, b_blk_8_bad_1, b_blk_7, miner_account, tx_send_contract1);
  DO_CALLBACK(events, "mark_invalid_block");
  MAKE_NEXT_BLOCK_TX1(events, b_blk_8_bad_2, b_blk_7, miner_account, tx_send_contract2);
  DO_CALLBACK(events, "mark_invalid_block");
  MAKE_NEXT_BLOCK_TX1(events, b_blk_8_bad_3, b_blk_7, miner_account, tx_grade);
  DO_CALLBACK(events, "mark_invalid_block");
  MAKE_NEXT_BLOCK_TX1(events, b_blk_8_bad_4, b_blk_7, miner_account, tx_alice_spend);
  DO_CALLBACK(events, "mark_invalid_block");
  MAKE_NEXT_BLOCK_TX1(events, b_blk_8_bad_5, b_blk_7, miner_account, tx_bob_spend);
  DO_CALLBACK(events, "mark_invalid_block");
  MAKE_NEXT_BLOCK_TX1(events, b_blk_8_bad_6, b_blk_7, miner_account, tx_fee_spend);
  
  // can re-create and do the chain
  MAKE_NEXT_BLOCK_TX1(events, b_blk_8pre, b_blk_7, miner_account, tx_contract);
  MAKE_NEXT_BLOCK_TX1(events, b_blk_8, b_blk_8pre, miner_account, tx_send_contract1);
  MAKE_NEXT_BLOCK_TX1(events, b_blk_9, b_blk_8, miner_account, tx_send_contract2);
  MAKE_NEXT_BLOCK_TX1(events, b_blk_10, b_blk_9, miner_account, tx_grade);
  MAKE_NEXT_BLOCK_TX1(events, b_blk_11, b_blk_10, miner_account, tx_alice_spend);
  MAKE_NEXT_BLOCK_TX1(events, b_blk_12, b_blk_11, miner_account, tx_bob_spend);
  DO_CALLBACK(events, "mark_head_2");
  MAKE_NEXT_BLOCK_TX1(events, b_blk_13, b_blk_12, miner_account, tx_fee_spend);
  
  // if split after 1st send, must claim different amount of fees
  DO_CALLBACK(events, "check_on_head_2");
  REWIND_BLOCKS_N(events, c_blk_14, b_blk_8, miner_account, 6);
  DO_CALLBACK(events, "check_not_head_2");
  
  DO_CALLBACK(events, "mark_invalid_block");
  MAKE_NEXT_BLOCK_TX1(events, c_blk_15_bad, c_blk_14, miner_account, tx_grade);

  GRADE_CONTRACT_TX_FULL(events, tx_grade2, 0xbe7, GRADE_SCALE_MAX / 4, grading_key.sec,
                         carol, 2, CURRENCY_XPB, tools::identity());
  MAKE_NEXT_BLOCK_TX1(events, c_blk_15, c_blk_14, miner_account, tx_grade2);
  
  DO_CALLBACK(events, "mark_invalid_block");
  MAKE_NEXT_BLOCK_TX1(events, c_blk_16_bad_1, c_blk_15, miner_account, tx_alice_spend);
  DO_CALLBACK(events, "mark_invalid_block");
  MAKE_NEXT_BLOCK_TX1(events, c_blk_16_bad_2, c_blk_15, miner_account, tx_bob_spend);
  DO_CALLBACK(events, "mark_invalid_block");
  MAKE_NEXT_BLOCK_TX1(events, c_blk_16_bad_3, c_blk_15, miner_account, tx_fee_spend);
  
  // alice can spend floor(33*.75) = 24 - ceil(24*.07) = 22
  // this one shouldn't work because the txin from the pervious one is still in the mempool, even
  // though it's from a transaction that is no longer possible
  DO_CALLBACK(events, "mark_invalid_tx");
  MAKE_TX_MIX_CP_FEE(events, tx_alice_spend2_bad, alice, carol, 22, 0, c_blk_15, CP_XPB, 0);
  
  SET_EVENT_VISITOR_SETT(events, event_visitor_settings::set_txs_keeped_by_block, true);
  // should work if we force it though...
  MAKE_TX_MIX_CP_FEE(events, tx_alice_spend2, alice, carol, 22, 0, c_blk_15, CP_XPB, 0);
  // bob can spend floor(33*.25) = 8 - ceil(8*.07) = 7
  MAKE_TX_MIX_CP_FEE(events, tx_bob_spend2, bob, carol, 7, 0, c_blk_15, CP_XPB, 0);
  // carol can spend floor(33*.07) = 2
  MAKE_TX_MIX_CP_FEE(events, tx_fee_spend2, carol, alice, 2, 0, c_blk_15, CP_XPB, 0);
  
  std::list<transaction> tx_list2;
  tx_list2.push_back(tx_alice_spend2);
  tx_list2.push_back(tx_bob_spend2);
  tx_list2.push_back(tx_fee_spend2);
  MAKE_NEXT_BLOCK_TX_LIST(events, c_blk_16, c_blk_15, miner_account, tx_list2);
  SET_EVENT_VISITOR_SETT(events, event_visitor_settings::set_txs_keeped_by_block, false);
  
  return true;
}

bool gen_chainswitch_contract_grade::generate(std::vector<test_event_entry>& events) const
{
  INIT_CONTRACT_TEST();
  
  CREATE_CONTRACT_TX_FULL(events, 0xbe7, "Is coin heads or tails", tx_contract, grading_key.pub,
                          GRADE_SCALE_MAX / 100 * 7, 10000, 0, tools::identity());
  MAKE_NEXT_BLOCK_TX1(events, blk_1, blk_0r, miner_account, tx_contract);
  
  GRADE_CONTRACT_TX_FULL(events, tx_grade, 0xbe7, GRADE_SCALE_MAX / 4, grading_key.sec,
                         carol, 0, CURRENCY_XPB, tools::identity());
  DO_CALLBACK(events, "mark_head_1");
  MAKE_NEXT_BLOCK_TX1(events, blk_2, blk_1, miner_account, tx_grade);
  
  // rollback grade, can't stick a new grade tx in the tx pool
  DO_CALLBACK(events, "check_on_head_1");
  REWIND_BLOCKS_N(events, b_blk_3, blk_1, miner_account, 2);
  DO_CALLBACK(events, "check_not_head_1");
  
  DO_CALLBACK(events, "mark_invalid_tx");
  GRADE_CONTRACT_TX_FULL(events, tx_grade_bad_1, 0xbe7, GRADE_SCALE_MAX / 4, grading_key.sec,
                         carol, 0, CURRENCY_XPB, tools::identity());
  DO_CALLBACK(events, "mark_invalid_tx");
  GRADE_CONTRACT_TX_FULL(events, tx_grade_bad_2, 0xbe7, GRADE_SCALE_MAX / 2, grading_key.sec,
                         carol, 0, CURRENCY_XPB, tools::identity());
  
  // but can force one through if it comes
  SET_EVENT_VISITOR_SETT(events, event_visitor_settings::set_txs_keeped_by_block, true);
  GRADE_CONTRACT_TX_FULL(events, tx_grade_2, 0xbe7, GRADE_SCALE_MAX / 2, grading_key.sec,
                         carol, 0, CURRENCY_XPB, tools::identity());
  MAKE_NEXT_BLOCK_TX1(events, b_blk_4, b_blk_3, miner_account, tx_grade_2);
  SET_EVENT_VISITOR_SETT(events, event_visitor_settings::set_txs_keeped_by_block, false);
  
  return true;
}
*/
