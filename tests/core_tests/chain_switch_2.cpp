// Copyright (c) 2014 The Pebblecoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.
/*
#include "currency.h"
#include "chain_switch_2.h"

using namespace epee;
using namespace crypto;
using namespace cryptonote;

bool gen_chainswitch_mint::generate(std::vector<test_event_entry>& events) const
{
  uint64_t ts_start = 1338224400;
  GENERATE_ACCOUNT(miner_account);
  GENERATE_ACCOUNT(alice);
  GENERATE_ACCOUNT(bob);
  MAKE_STARTING_BLOCKS(events, blk_0, miner_account, ts_start); // gen + 3 blocks
  
  MAKE_MINT_TX(events, 0xbeef, "Beef futures", 1000, alice, txmint, 2, null_pkey);
  MAKE_NEXT_BLOCK_TX1(events, blk_1, blk_0, miner_account, txmint); // gen + 4 blocks
  DO_CALLBACK(events, "mark_head_1");
  MAKE_NEXT_BLOCK(events, blk_2, blk_1, miner_account); // gen + 5 blocks
  
  DO_CALLBACK(events, "check_on_head_1");
  MAKE_NEXT_BLOCK(events, b_blk_1, blk_0, miner_account);
  DO_CALLBACK(events, "check_on_head_1");
  MAKE_NEXT_BLOCK(events, b_blk_2, b_blk_1, miner_account);
  DO_CALLBACK(events, "check_on_head_1");
  MAKE_NEXT_BLOCK(events, b_blk_3, b_blk_2, miner_account);
  DO_CALLBACK(events, "check_not_head_1");
  
  // alice shouldn't have stuff to spend after the switch
  DO_CALLBACK(events, "mark_invalid_tx");
  MAKE_TX_C(events, txbeef1, alice, bob, 1000, blk_2, 0xbeef);
  
  // mint many should be able to switch
  MAKE_MINT_TX(events, 0xfeed, "Tomatoes", 1000, alice, txmint2, 2, null_pkey);
  MAKE_MINT_TX(events, 0x3ff, "", 1000, alice, txmint3, 2, null_pkey);
  MAKE_MINT_TX(events, 0x4ff, "", 1000, alice, txmint4, 2, null_pkey);
  MAKE_MINT_TX(events, 256, "", 1000, alice, txmint5, 2, null_pkey);
  
  std::list<cryptonote::transaction> tx_list;
  tx_list.push_back(txmint2);
  tx_list.push_back(txmint3);
  tx_list.push_back(txmint4);
  tx_list.push_back(txmint5);
  MAKE_NEXT_BLOCK_TX_LIST(events, b_blk_4, b_blk_3, miner_account, tx_list);
  
  MAKE_NEXT_BLOCK(events, b_blk_5, b_blk_4, miner_account);
  DO_CALLBACK(events, "mark_head_2");
  MAKE_NEXT_BLOCK(events, b_blk_6, b_blk_5, miner_account);
  DO_CALLBACK(events, "check_on_head_2");
  MAKE_NEXT_BLOCK(events, c_blk_4, b_blk_3, miner_account);
  DO_CALLBACK(events, "check_on_head_2");
  MAKE_NEXT_BLOCK(events, c_blk_5, c_blk_4, miner_account);
  DO_CALLBACK(events, "check_on_head_2");
  MAKE_NEXT_BLOCK(events, c_blk_6, c_blk_5, miner_account);
  DO_CALLBACK(events, "check_on_head_2");
  MAKE_NEXT_BLOCK(events, c_blk_7, c_blk_6, miner_account);
  DO_CALLBACK(events, "check_not_head_1");
  DO_CALLBACK(events, "check_not_head_2");
  
  return true;
}

bool gen_chainswitch_mint_2::generate(std::vector<test_event_entry>& events) const
{
  uint64_t ts_start = 1338224400;
  
  GENERATE_ACCOUNT(miner_account);
  GENERATE_ACCOUNT(alice);
  
  MAKE_STARTING_BLOCKS(events, blk_0, miner_account, ts_start);
  
  keypair A = keypair::generate();
  
  // 1) mint 0xbeef, "hello"
  // 2) rewind, keeped-by-block mint 0xbeef, "whatever"
  // 3) should be able to mint regularly with description "hello"
  
  MAKE_MINT_TX(events, 0xbeef, "hello", 4000, alice, txmint1, 2, A.pub);
  DO_CALLBACK(events, "mark_head_1");
  MAKE_NEXT_BLOCK_TX1(events, blk_1, blk_0, miner_account, txmint1);

  DO_CALLBACK(events, "check_on_head_1");
  REWIND_BLOCKS_N(events, b_blk_2, blk_0, miner_account, 2);
  DO_CALLBACK(events, "check_not_head_1");
  
  SET_EVENT_VISITOR_SETT(events, event_visitor_settings::set_txs_keeped_by_block, true);
  MAKE_MINT_TX(events, 0xbeef, "whatever", 4000, alice, txmint2, 2, A.pub);
  MAKE_NEXT_BLOCK_TX1(events, b_blk_3, b_blk_2, miner_account, txmint2);
  SET_EVENT_VISITOR_SETT(events, event_visitor_settings::set_txs_keeped_by_block, false);
  
  MAKE_MINT_TX(events, 0xf00d, "hello", 4000, alice, txmint3, 2, A.pub);
  MAKE_NEXT_BLOCK_TX1(events, b_blk_4, b_blk_3, miner_account, txmint3);
  
  return true;
}

bool gen_chainswitch_mint_3::generate(std::vector<test_event_entry>& events) const
{
  uint64_t ts_start = 1338224400;
  
  GENERATE_ACCOUNT(miner_account);
  GENERATE_ACCOUNT(alice);
  
  MAKE_STARTING_BLOCKS(events, blk_0, miner_account, ts_start);
  
  keypair A = keypair::generate();
  
  // 1) mint 0xbeef, "hello"
  // 2) rewind, keeped-by-block mint 0xfood, "yello"
  // 3) should be able to mint 0xbeef with another description
  
  MAKE_MINT_TX(events, 0xbeef, "hello", 4000, alice, txmint1, 2, A.pub);
  DO_CALLBACK(events, "mark_head_1");
  MAKE_NEXT_BLOCK_TX1(events, blk_1, blk_0, miner_account, txmint1);
  
  DO_CALLBACK(events, "check_on_head_1");
  REWIND_BLOCKS_N(events, b_blk_2, blk_0, miner_account, 2);
  DO_CALLBACK(events, "check_not_head_1");
  
  SET_EVENT_VISITOR_SETT(events, event_visitor_settings::set_txs_keeped_by_block, true);
  MAKE_MINT_TX(events, 0xf00d, "hello", 4000, alice, txmint2, 2, A.pub);
  MAKE_NEXT_BLOCK_TX1(events, b_blk_3, b_blk_2, miner_account, txmint2);
  SET_EVENT_VISITOR_SETT(events, event_visitor_settings::set_txs_keeped_by_block, false);
  
  MAKE_MINT_TX(events, 0xbeef, "nomnoms", 4000, alice, txmint3, 2, A.pub);
  MAKE_NEXT_BLOCK_TX1(events, b_blk_4, b_blk_3, miner_account, txmint3);
  
  return true;
}

bool gen_chainswitch_remint::generate(std::vector<test_event_entry>& events) const
{
  uint64_t ts_start = 1338224400;
  
  GENERATE_ACCOUNT(miner_account);
  GENERATE_ACCOUNT(alice);
  GENERATE_ACCOUNT(bob);
  
  MAKE_STARTING_BLOCKS(events, blk_0, miner_account, ts_start);
  
  keypair k = keypair::generate();
  keypair k2 = keypair::generate();
  
  MAKE_MINT_TX(events, 0xbeef, "Beef futures", 4000, alice, txmint, 2, k.pub);
  MAKE_NEXT_BLOCK_TX1(events, blk_1, blk_0, miner_account, txmint);
  MAKE_TX_C(events, txbeef1, alice, bob, 4000, blk_1, 0xbeef);
  MAKE_NEXT_BLOCK_TX1(events, blk_2, blk_1, miner_account, txbeef1);
  
  MAKE_REMINT_TX(events, 0xbeef, 5555, alice, txremint, k.sec, k2.pub);
  MAKE_NEXT_BLOCK_TX1(events, blk_3, blk_2, miner_account, txremint);
  MAKE_TX_C(events, txbeef2, alice, bob, 5555, blk_3, 0xbeef);
  MAKE_NEXT_BLOCK_TX1(events, blk_4, blk_3, miner_account, txbeef2);
  
  MAKE_REMINT_TX(events, 0xbeef, 33333, alice, txremint2, k2.sec, null_pkey);
  MAKE_NEXT_BLOCK_TX1(events, blk_5, blk_4, miner_account, txremint2);
  MAKE_TX_C(events, txbeef3, alice, bob, 33333, blk_5, 0xbeef);
  DO_CALLBACK(events, "mark_head_1");
  MAKE_NEXT_BLOCK_TX1(events, blk_6, blk_5, miner_account, txbeef3);
  
  // switch from blk_4, txremint2 should be still be valid
  DO_CALLBACK(events, "check_on_head_1");
  REWIND_BLOCKS_N(events, b_blk_7, blk_4, miner_account, 5);
  DO_CALLBACK(events, "check_not_head_1");

  // remint + spend txs are already in mempool
  DO_CALLBACK(events, "mark_invalid_block");
  MAKE_NEXT_BLOCK_TX1(events, b_blk_8_bad, b_blk_7, miner_account, txbeef3);
  MAKE_NEXT_BLOCK_TX1(events, b_blk_8, b_blk_7, miner_account, txremint2);
  DO_CALLBACK(events, "mark_head_2");
  MAKE_NEXT_BLOCK_TX1(events, b_blk_9, b_blk_8, miner_account, txbeef3);
  
  // switch from blk_2, txremint2 should not be valid cause the key wasn't switched
  DO_CALLBACK(events, "check_on_head_2");
  REWIND_BLOCKS_N(events, c_blk_10, blk_2, miner_account, 10);
  DO_CALLBACK(events, "check_not_head_2");
  
  DO_CALLBACK(events, "mark_invalid_block");
  MAKE_NEXT_BLOCK_TX1(events, c_blk_11_bad_1, c_blk_10, miner_account, txremint2);
  DO_CALLBACK(events, "mark_invalid_block");
  MAKE_NEXT_BLOCK_TX1(events, c_blk_11_bad_2, c_blk_10, miner_account, txbeef3);
  DO_CALLBACK(events, "mark_invalid_block");
  MAKE_NEXT_BLOCK_TX1(events, c_blk_11_bad_3, c_blk_10, miner_account, txbeef2);
  
  // but can do txremint then txremint2
  MAKE_NEXT_BLOCK_TX1(events, c_blk_11, c_blk_10, miner_account, txremint);
  MAKE_NEXT_BLOCK_TX1(events, c_blk_12, c_blk_11, miner_account, txbeef2);
  DO_CALLBACK(events, "mark_invalid_block");
  MAKE_NEXT_BLOCK_TX1(events, c_blk_13_bad, c_blk_12, miner_account, txbeef3);
  MAKE_NEXT_BLOCK_TX1(events, c_blk_13, c_blk_12, miner_account, txremint2);
  DO_CALLBACK(events, "mark_head_3");
  MAKE_NEXT_BLOCK_TX1(events, c_blk_14, c_blk_13, miner_account, txbeef3);
  
  // switch from blk_0, txremint should not be valid, cause it was never minted
  DO_CALLBACK(events, "check_on_head_3");
  REWIND_BLOCKS_N(events, d_blk_15, blk_0, miner_account, 18);
  DO_CALLBACK(events, "check_not_head_3");
  
  DO_CALLBACK(events, "mark_invalid_block");
  MAKE_NEXT_BLOCK_TX1(events, d_blk_16_bad_1, d_blk_15, miner_account, txremint);
  DO_CALLBACK(events, "mark_invalid_block");
  MAKE_NEXT_BLOCK_TX1(events, d_blk_16_bad_2, d_blk_15, miner_account, txremint2);
  DO_CALLBACK(events, "mark_invalid_block");
  MAKE_NEXT_BLOCK_TX1(events, d_blk_16_bad_3, d_blk_15, miner_account, txbeef2);
  DO_CALLBACK(events, "mark_invalid_block");
  MAKE_NEXT_BLOCK_TX1(events, d_blk_16_bad_4, d_blk_15, miner_account, txbeef3);
  
  // but can put them all in good order
  MAKE_NEXT_BLOCK_TX1(events, d_blk_16, d_blk_15, miner_account, txmint);
  MAKE_NEXT_BLOCK_TX1(events, d_blk_17, d_blk_16, miner_account, txremint);
  MAKE_NEXT_BLOCK_TX1(events, d_blk_18, d_blk_17, miner_account, txremint2);
  MAKE_NEXT_BLOCK_TX1(events, d_blk_19, d_blk_18, miner_account, txbeef2);
  MAKE_NEXT_BLOCK_TX1(events, d_blk_20, d_blk_19, miner_account, txbeef3);
  
  return true;
}

bool gen_chainswitch_remint_2::generate(std::vector<test_event_entry>& events) const
{
  uint64_t ts_start = 1338224400;
  
  GENERATE_ACCOUNT(miner_account);
  GENERATE_ACCOUNT(alice);
  GENERATE_ACCOUNT(bob);
  
  MAKE_STARTING_BLOCKS(events, blk_0, miner_account, ts_start);
  
  keypair A = keypair::generate();
  keypair B = keypair::generate();
  
  // 1) mint with A
  // 2) remint A -> A
  // 3) remint A -> B
  // chainswitch after 1)
  // play the tx 3)
  // now try remint from B - should work
  
  MAKE_MINT_TX(events, 0xbeef, "Beef futures", 4000, alice, txmintA, 2, A.pub);
  MAKE_NEXT_BLOCK_TX1(events, blk_1, blk_0, miner_account, txmintA);
  MAKE_REMINT_TX(events, 0xbeef, 5555, alice, txremintAA, A.sec, A.pub);
  MAKE_NEXT_BLOCK_TX1(events, blk_2, blk_1, miner_account, txremintAA);
  MAKE_REMINT_TX(events, 0xbeef, 8373, alice, txremintAB, A.sec, B.pub);
  DO_CALLBACK(events, "mark_head_1");
  MAKE_NEXT_BLOCK_TX1(events, blk_3, blk_2, miner_account, txremintAB);
  
  DO_CALLBACK(events, "check_on_head_1");
  REWIND_BLOCKS_N(events, b_blk_4, blk_1, miner_account, 3);
  DO_CALLBACK(events, "check_not_head_1");
  
  MAKE_NEXT_BLOCK_TX1(events, b_blk_5, b_blk_4, miner_account, txremintAB);
  
  MAKE_REMINT_TX(events, 0xbeef, 12346, alice, txremintBB, B.sec, B.pub);
  MAKE_NEXT_BLOCK_TX1(events, b_blk_6, b_blk_5, miner_account, txremintBB);
  
  return true;
}
*/


