// Copyright (c) 2015 The Cryptonote developers
// Copyright (c) 2012-2013 The Cryptonote developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "crypto/crypto_basic_impl.h"
#include "cryptonote_core/checkpoints.h"

#include "chaingen.h"
#include "chain_switch_1.h"

using namespace epee;
using namespace cryptonote;


gen_chain_switch_1::gen_chain_switch_1()
{
  REGISTER_CALLBACK("check_split_not_switched", gen_chain_switch_1::check_split_not_switched);
  REGISTER_CALLBACK("check_split_switched", gen_chain_switch_1::check_split_switched);
}


//-----------------------------------------------------------------------------------------------------
bool gen_chain_switch_1::generate(std::vector<test_event_entry>& events) const
{
  uint64_t ts_start = 1338224400;
  /*
  (0 )-(1 )-(2 ) -(3 )-(4 )                  <- main chain, until 7 isn't connected
              \ |-(5 )-(6 )-(7 )|            <- alt chain, until 7 isn't connected

  transactions ([n] - tx amount, (m) - block):
  (1)     : miner -[ 5]-> account_1 ( +5 in main chain,  +5 in alt chain)
  (3)     : miner -[ 7]-> account_2 ( +7 in main chain,  +0 in alt chain), tx will be in tx pool after switch
  (4), (6): miner -[11]-> account_3 (+11 in main chain, +11 in alt chain)
  (5)     : miner -[13]-> account_4 ( +0 in main chain, +13 in alt chain), tx will be in tx pool before switch

  transactions orders ([n] - tx amount, (m) - block):
  miner -[1], [2]-> account_1: in main chain (3), (3), in alt chain (5), (6)
  miner -[1], [2]-> account_2: in main chain (3), (4), in alt chain (5), (5)
  miner -[1], [2]-> account_3: in main chain (3), (4), in alt chain (6), (5)
  miner -[1], [2]-> account_4: in main chain (4), (3), in alt chain (5), (6)
  */

  GENERATE_ACCOUNT(miner_account);

  //                                                                                              events
  MAKE_GENESIS_BLOCK(events, blk_n2, miner_account, ts_start);                                     //  -2
  MAKE_NEXT_BLOCK(events, blk_n1, blk_n2, miner_account);                                           //  -1
  MAKE_NEXT_BLOCK(events, blk_0, blk_n1, miner_account);                                           //  0
  MAKE_ACCOUNT(events, recipient_account_1);                                                      //  1
  MAKE_ACCOUNT(events, recipient_account_2);                                                      //  2
  MAKE_ACCOUNT(events, recipient_account_3);                                                      //  3
  MAKE_ACCOUNT(events, recipient_account_4);                                                      //  4
  REWIND_BLOCKS(events, blk_0r, blk_0, miner_account)                                             // <N blocks>
  MAKE_TX(events, tx_00, miner_account, recipient_account_1, 5, blk_0r);                 //  5 + N
  MAKE_NEXT_BLOCK_TX1(events, blk_1, blk_0r, miner_account, tx_00);                               //  6 + N
  MAKE_NEXT_BLOCK(events, blk_2, blk_1, miner_account);                                           //  7 + N
  REWIND_BLOCKS(events, blk_2r, blk_2, miner_account)                                             // <N blocks>

  // Transactions to test account balances after switch
  MAKE_TX_LIST_START(events, txs_blk_3, miner_account, recipient_account_2, 7, blk_2r);  //  8 + 2N
  MAKE_TX_LIST_START(events, txs_blk_4, miner_account, recipient_account_3, 11, blk_2r); //  9 + 2N
  MAKE_TX_LIST_START(events, txs_blk_5, miner_account, recipient_account_4, 13, blk_2r); // 10 + 2N
  std::list<transaction> txs_blk_6;
  txs_blk_6.push_back(txs_blk_4.front());

  // Transactions, that has different order in alt block chains
  MAKE_TX_LIST(events, txs_blk_3, miner_account, recipient_account_1, 1, blk_2r);        // 11 + 2N
  txs_blk_5.push_back(txs_blk_3.back());
  MAKE_TX_LIST(events, txs_blk_3, miner_account, recipient_account_1, 2, blk_2r);        // 12 + 2N
  txs_blk_6.push_back(txs_blk_3.back());

  MAKE_TX_LIST(events, txs_blk_3, miner_account, recipient_account_2, 1, blk_2r);        // 13 + 2N
  txs_blk_5.push_back(txs_blk_3.back());
  MAKE_TX_LIST(events, txs_blk_4, miner_account, recipient_account_2, 2, blk_2r);        // 14 + 2N
  txs_blk_5.push_back(txs_blk_4.back());

  MAKE_TX_LIST(events, txs_blk_3, miner_account, recipient_account_3, 1, blk_2r);        // 15 + 2N
  txs_blk_6.push_back(txs_blk_3.back());
  MAKE_TX_LIST(events, txs_blk_4, miner_account, recipient_account_3, 2, blk_2r);        // 16 + 2N
  txs_blk_5.push_back(txs_blk_4.back());

  MAKE_TX_LIST(events, txs_blk_4, miner_account, recipient_account_4, 1, blk_2r);        // 17 + 2N
  txs_blk_5.push_back(txs_blk_4.back());
  MAKE_TX_LIST(events, txs_blk_3, miner_account, recipient_account_4, 2, blk_2r);        // 18 + 2N
  txs_blk_6.push_back(txs_blk_3.back());

  MAKE_NEXT_BLOCK_TX_LIST(events, blk_3, blk_2r, miner_account, txs_blk_3);                       // 19 + 2N
  MAKE_NEXT_BLOCK_TX_LIST(events, blk_4, blk_3, miner_account, txs_blk_4);                        // 20 + 2N
  //split
  MAKE_NEXT_BLOCK_TX_LIST(events, blk_5, blk_2r, miner_account, txs_blk_5);                       // 22 + 2N
  MAKE_NEXT_BLOCK_TX_LIST(events, blk_6, blk_5, miner_account, txs_blk_6);                        // 23 + 2N
  DO_CALLBACK(events, "check_split_not_switched");                                                // 21 + 2N
  MAKE_NEXT_BLOCK(events, blk_7, blk_6, miner_account);                                           // 24 + 2N
  DO_CALLBACK(events, "check_split_switched");                                                    // 25 + 2N

  return true;
}


//-----------------------------------------------------------------------------------------------------
bool gen_chain_switch_1::check_split_not_switched(core_t& c, size_t ev_index, const std::vector<test_event_entry>& events)
{
  DEFINE_TESTS_ERROR_CONTEXT("gen_chain_switch_1::check_split_not_switched");

  m_recipient_account_1 = boost::get<account_base>(events[3]);
  m_recipient_account_2 = boost::get<account_base>(events[4]);
  m_recipient_account_3 = boost::get<account_base>(events[5]);
  m_recipient_account_4 = boost::get<account_base>(events[6]);

  std::list<block> blocks;
  bool r = c.get_blocks(0, 10000, blocks);
  CHECK_TEST_CONDITION(r);
  CHECK_EQ(7 + 2 * CRYPTONOTE_MINED_MONEY_UNLOCK_WINDOW, blocks.size());
  CHECK_TEST_CONDITION(blocks.back() == boost::get<block>(events[22 + 2 * CRYPTONOTE_MINED_MONEY_UNLOCK_WINDOW]));  // blk_4

  CHECK_EQ(2, c.get_alternative_blocks_count());

  std::vector<cryptonote::block> chain;
  map_hash2tx_isregular_t mtx;
  r = find_block_chain(events, chain, mtx, get_block_hash(blocks.back()));
  CHECK_TEST_CONDITION(r);
  CHECK_EQ(8,  get_balance(m_recipient_account_1, chain, mtx));
  CHECK_EQ(10, get_balance(m_recipient_account_2, chain, mtx));
  CHECK_EQ(14, get_balance(m_recipient_account_3, chain, mtx));
  CHECK_EQ(3,  get_balance(m_recipient_account_4, chain, mtx));

  std::list<transaction> tx_pool;
  r = c.get_pool_transactions(tx_pool);
  CHECK_TEST_CONDITION(r);
  CHECK_EQ(1, tx_pool.size());

  std::vector<size_t> tx_outs;
  uint64_t transfered;
  lookup_acc_outs(m_recipient_account_4.get_keys(), tx_pool.front(), get_tx_pub_key_from_extra(tx_pool.front()), tx_outs, transfered);
  CHECK_EQ(13, transfered);

  m_chain_1.swap(blocks);
  m_tx_pool.swap(tx_pool);

  return true;
}

//-----------------------------------------------------------------------------------------------------
bool gen_chain_switch_1::check_split_switched(core_t& c, size_t ev_index, const std::vector<test_event_entry>& events)
{
  DEFINE_TESTS_ERROR_CONTEXT("gen_chain_switch_1::check_split_switched");

  std::list<block> blocks;
  bool r = c.get_blocks(0, 10000, blocks);
  CHECK_TEST_CONDITION(r);
  CHECK_EQ(8 + 2 * CRYPTONOTE_MINED_MONEY_UNLOCK_WINDOW, blocks.size());
  auto it = blocks.end();
  --it; --it; --it;
  CHECK_TEST_CONDITION(std::equal(blocks.begin(), it, m_chain_1.begin()));
  CHECK_TEST_CONDITION(blocks.back() == boost::get<block>(events[26 + 2 * CRYPTONOTE_MINED_MONEY_UNLOCK_WINDOW]));  // blk_7

  std::list<block> alt_blocks;
  r = c.get_alternative_blocks(alt_blocks);
  CHECK_TEST_CONDITION(r);
  CHECK_EQ(2, c.get_alternative_blocks_count());

  // Some blocks that were in main chain are in alt chain now
  BOOST_FOREACH(block b, alt_blocks)
  {
    CHECK_TEST_CONDITION(m_chain_1.end() != std::find(m_chain_1.begin(), m_chain_1.end(), b));
  }

  std::vector<cryptonote::block> chain;
  map_hash2tx_isregular_t mtx;
  r = find_block_chain(events, chain, mtx, get_block_hash(blocks.back()));
  CHECK_TEST_CONDITION(r);
  CHECK_EQ(8,  get_balance(m_recipient_account_1, chain, mtx));
  CHECK_EQ(3,  get_balance(m_recipient_account_2, chain, mtx));
  CHECK_EQ(14, get_balance(m_recipient_account_3, chain, mtx));
  CHECK_EQ(16, get_balance(m_recipient_account_4, chain, mtx));

  std::list<transaction> tx_pool;
  r = c.get_pool_transactions(tx_pool);
  CHECK_TEST_CONDITION(r);
  CHECK_EQ(1, tx_pool.size());
  CHECK_TEST_CONDITION(!(tx_pool.front() == m_tx_pool.front()));

  std::vector<size_t> tx_outs;
  uint64_t transfered;
  lookup_acc_outs(m_recipient_account_2.get_keys(), tx_pool.front(), tx_outs, transfered);
  CHECK_EQ(7, transfered);

  return true;
}

bool gen_chainswitch_invalid_1::generate(std::vector<test_event_entry>& events) const
{
  // make a long chain with a double-spend at the beginning, make sure rolls back + marks invalid ok
  uint64_t ts_start = 1338224400;
  GENERATE_ACCOUNT(miner_account);
  GENERATE_ACCOUNT(alice);
  GENERATE_ACCOUNT(bob);
  MAKE_STARTING_BLOCKS(events, blk_0, miner_account, ts_start);
  
  REWIND_BLOCKS_N(events, blk_1r, blk_0, miner_account, 20);

  // start alt chain
  REWIND_BLOCKS_N(events, b_blk_1r, blk_0, miner_account, 5); // 5
  
  SET_EVENT_VISITOR_SETT(events, event_visitor_settings::set_txs_keeped_by_block, true);
  MAKE_TX(events, tx_spend1, miner_account, alice, 500, blk_1r);
  
  MAKE_NEXT_BLOCK_TX1(events, b_blk_2, b_blk_1r, miner_account, tx_spend1); // 6
  MAKE_NEXT_BLOCK_TX1(events, b_blk_3, b_blk_2, miner_account, tx_spend1);  // 7
  
  REWIND_BLOCKS_N(events, b_blk_4r, b_blk_3, miner_account, 13); // 20
 
  DO_CALLBACK(events, "mark_invalid_block");
  MAKE_NEXT_BLOCK(events, b_blk_5, b_blk_4r, miner_account); // 21 - fail to reorg
  
  return true;
}

bool gen_chainswitch_invalid_2::generate(std::vector<test_event_entry>& events) const
{
  // make a long chain with a double-spend at the end, make sure rolls back + marks invalid ok
  uint64_t ts_start = 1338224400;
  GENERATE_ACCOUNT(miner_account);
  GENERATE_ACCOUNT(alice);
  GENERATE_ACCOUNT(bob);
  MAKE_STARTING_BLOCKS(events, blk_0, miner_account, ts_start);
  
  REWIND_BLOCKS_N(events, blk_1r, blk_0, miner_account, 20);

  // start alt chain
  REWIND_BLOCKS_N(events, b_blk_1r, blk_0, miner_account, 17); // 17
  
  SET_EVENT_VISITOR_SETT(events, event_visitor_settings::set_txs_keeped_by_block, true);
  MAKE_TX(events, tx_spend1, miner_account, alice, 500, blk_1r);
  
  MAKE_NEXT_BLOCK_TX1(events, b_blk_2, b_blk_1r, miner_account, tx_spend1); // 18
  MAKE_NEXT_BLOCK_TX1(events, b_blk_3, b_blk_2, miner_account, tx_spend1);  // 19
  
  MAKE_NEXT_BLOCK(events, b_blk_4, b_blk_3, miner_account); // 20
 
  DO_CALLBACK(events, "mark_invalid_block");
  MAKE_NEXT_BLOCK(events, b_blk_5, b_blk_4, miner_account); // 21 - fail to reorg
  
  return true;
}

bool gen_chainswitch_invalid_checkpoint_rollback::generate(std::vector<test_event_entry>& events) const
{
  uint64_t ts_start = 1338224400;
  GENERATE_ACCOUNT(miner_account);
  GENERATE_ACCOUNT(alice);
  GENERATE_ACCOUNT(bob);
  MAKE_STARTING_BLOCKS(events, blk_0, miner_account, ts_start);
  
  REWIND_BLOCKS_N(events, blk_1r, blk_0, miner_account, 20);
  
  // checkpoint latest block
  const auto& last_block = boost::get<const cryptonote::block&>(events.back());
  uint64_t last_block_height = get_block_height(last_block);
  crypto::hash last_block_hash = get_block_hash(last_block);
  
  do_callback_func(events, [=](core_t& c, size_t ev_index) {
    cryptonote::checkpoints checkpoints;
    CHECK_AND_ASSERT_MES(checkpoints.add_checkpoint(last_block_height,
                                                    dump_hash256(last_block_hash)),
                         false, "Could not add checkpoint");
    c.set_checkpoints(std::move(checkpoints));
    return true;
  });
  
  // should not be able to start an alt chain
  DO_CALLBACK(events, "mark_invalid_block");
  MAKE_NEXT_BLOCK(events, blk_alt_1, blk_0, miner_account);
  
  do_callback_func(events, [=](core_t& c, size_t ev_index) {
    CHECK_AND_ASSERT_MES(c.get_tail_id() == last_block_hash, false,
                         "no longer on correct chain");
    return true;
  });
  
  return true;
}

bool gen_chainswitch_invalid_new_altchain::generate(std::vector<test_event_entry>& events) const
{
  uint64_t ts_start = 1338224400;
  GENERATE_ACCOUNT(miner_account);
  GENERATE_ACCOUNT(alice);
  GENERATE_ACCOUNT(bob);
  MAKE_STARTING_BLOCKS(events, blk_0, miner_account, ts_start);
  
  REWIND_BLOCKS_N(events, blk_1r, blk_0, miner_account, 5);
  
  // checkpoint latest block
  uint64_t last_block_height;
  crypto::hash last_block_hash;
  {
    const auto& last_block = boost::get<const cryptonote::block&>(events.back());
    last_block_height = get_block_height(last_block);
    last_block_hash = get_block_hash(last_block);
  }
  
  // wipe out this chain
  std::vector<test_event_entry> good_chain(events.end() - 5, events.end());
  events.erase(events.end() - 5, events.end());
  
  // checkpoint what would've been the end
  do_callback_func(events, [=](core_t& c, size_t ev_index) {
    cryptonote::checkpoints checkpoints;
    CHECK_AND_ASSERT_MES(checkpoints.add_checkpoint(last_block_height,
                                                    dump_hash256(last_block_hash)),
                         false, "Could not add checkpoint");
    c.set_checkpoints(std::move(checkpoints));
    return true;
  });
  
  // new chain should not work - checkpoint should fail
  SET_EVENT_VISITOR_SETT(events, event_visitor_settings::set_check_can_create_block_from_template, false);
  REWIND_BLOCKS_N(events, blk_alt_4, blk_0, miner_account, 4);
  DO_CALLBACK(events, "mark_invalid_block");
  MAKE_NEXT_BLOCK(events, blk_alt_5, blk_alt_4, miner_account);
  
  // good chain should work
  events.insert(events.end(), good_chain.begin(), good_chain.end());
  
  do_callback_func(events, [=](core_t& c, size_t ev_index) {
    CHECK_AND_ASSERT_MES(c.get_tail_id() == last_block_hash, false,
                         "no longer on correct chain");
    return true;
  });
  
  return true;
}
