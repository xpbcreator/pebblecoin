// Copyright (c) 2014 The Pebblecoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.
/*
#include "contracts.h"
#include "currency.h"
#include "cryptonote_core/tx_builder.h"
#include "cryptonote_core/tx_tester.h"
#include "cryptonote_core/construct_tx_mod.h"

using namespace epee;
using namespace crypto;
using namespace cryptonote;

static account_base g_junk_account;

namespace {
  crypto::public_key generate_invalid_pub_key()
  {
    for (int i = 0; i <= 0xFF; ++i)
    {
      crypto::public_key key;
      memset(&key, i, sizeof(crypto::public_key));
      if (!crypto::check_key(key))
      {
        return key;
      }
    }
    
    throw std::runtime_error("invalid public key wasn't found");
    return crypto::public_key();
  }
}

bool gen_contracts_create::generate(std::vector<test_event_entry>& events) const
{
  INIT_CONTRACT_TEST();
  
  CREATE_CONTRACT_TX(events, 0xbe7, "Is coin heads or tails", tx_contract, grading_key.pub);
  MAKE_NEXT_BLOCK_TX1(events, blk_1, blk_0, miner_account, tx_contract);
  MAKE_NEXT_BLOCK(events, blk_2, blk_1, miner_account);
  
  return true;
}

bool gen_contracts_create_mint::generate(std::vector<test_event_entry>& events) const
{
  INIT_CONTRACT_TEST();
  
  MAKE_TX_LIST_START(events, txs, miner_account, alice, 10, blk_0r);
  CREATE_CONTRACT_TX(events, 0xbe7, "Is coin heads or tails", tx_contract, grading_key.pub);
  txs.push_back(tx_contract);
  MAKE_NEXT_BLOCK_TX_LIST(events, blk_1, blk_0r, miner_account, txs);
  
  SEND_CONTRACT_TX(events, tx_send_contract, 0xbe7,
                   alice, 10, CURRENCY_XPB,
                   alice, bob, blk_1);
  MAKE_NEXT_BLOCK_TX1(events, blk_2, blk_1, miner_account, tx_send_contract);
  
  MAKE_TX_CP(events, tx_fwd, bob, carol, 5, blk_2, coin_type(0xbe7, ContractCoin, CURRENCY_XPB));
  MAKE_NEXT_BLOCK_TX1(events, blk_3, blk_2, miner_account, tx_fwd);
  
  return true;
}

bool gen_contracts_grade::generate(std::vector<test_event_entry>& events) const
{
  INIT_CONTRACT_TEST();
  
  MAKE_TX_LIST_START(events, txs, miner_account, alice, 10, blk_0r);
  CREATE_CONTRACT_TX(events, 0xbe7, "Is coin heads or tails", tx_contract, grading_key.pub);
  txs.push_back(tx_contract);
  MAKE_NEXT_BLOCK_TX_LIST(events, blk_1, blk_0r, miner_account, txs);
  
  SEND_CONTRACT_TX(events, tx_send_contract, 0xbe7,
                   alice, 10, CURRENCY_XPB,
                   alice, bob, blk_1);
  MAKE_NEXT_BLOCK_TX1(events, blk_2, blk_1, miner_account, tx_send_contract);
  
  GRADE_CONTRACT_TX(events, tx_grade, 0xbe7, 0, grading_key.sec);
  MAKE_NEXT_BLOCK_TX1(events, blk_3, blk_2, miner_account, tx_grade);
  
  return true;
}

bool gen_contracts_grade_with_fee::generate(std::vector<test_event_entry>& events) const
{
  INIT_CONTRACT_TEST();
  
  MAKE_TX_LIST_START(events, txs, miner_account, alice, 10, blk_0r);
  CREATE_CONTRACT_TX_FULL(events, 0xbe7, "Is coin heads or tails", tx_contract, grading_key.pub,
                          GRADE_SCALE_MAX / 10, 10000, 0, tools::identity());
  txs.push_back(tx_contract);
  MAKE_NEXT_BLOCK_TX_LIST(events, blk_1, blk_0r, miner_account, txs);
  
  SEND_CONTRACT_TX(events, tx_send_contract, 0xbe7,
                   alice, 10, CURRENCY_XPB,
                   alice, bob, blk_1);
  MAKE_NEXT_BLOCK_TX1(events, blk_2, blk_1, miner_account, tx_send_contract);
  
  GRADE_CONTRACT_TX_FULL(events, tx_grade, 0xbe7, 0, grading_key.sec, carol, 1, CURRENCY_XPB, tools::identity());
  MAKE_NEXT_BLOCK_TX1(events, blk_3, blk_2, miner_account, tx_grade);
  
  return true;
}

bool gen_contracts_grade_spend_backing::generate(std::vector<test_event_entry>& events) const
{
  INIT_CONTRACT_TEST();
  
  MAKE_TX_LIST_START(events, txs, miner_account, alice, 100, blk_0r);
  CREATE_CONTRACT_TX(events, 0xbe7, "Is coin heads or tails", tx_contract, grading_key.pub);
  txs.push_back(tx_contract);
  MAKE_NEXT_BLOCK_TX_LIST(events, blk_1, blk_0r, miner_account, txs);
  
  SEND_CONTRACT_TX(events, tx_send_contract, 0xbe7,
                   alice, 100, CURRENCY_XPB,
                   alice, bob, blk_1);
  MAKE_NEXT_BLOCK_TX1(events, blk_2, blk_1, miner_account, tx_send_contract);
  
  GRADE_CONTRACT_TX(events, tx_grade, 0xbe7, GRADE_SCALE_MAX / 4, grading_key.sec);
  MAKE_NEXT_BLOCK_TX1(events, blk_3, blk_2, miner_account, tx_grade);
  LOG_PRINT_YELLOW("Making spend txout_backing tx...", LOG_LEVEL_0);
  // alice should be able to spend 75 or less
  MAKE_TX_MIX_CP_FEE(events, tx_alice_spend, alice, carol, 75, 0, blk_3, CP_XPB, 0)
  MAKE_NEXT_BLOCK_TX1(events, blk_4, blk_3, miner_account, tx_alice_spend);
  
  return true;
}

bool gen_contracts_grade_spend_backing_cant_overspend_misgrade::generate(std::vector<test_event_entry>& events) const
{
  INIT_CONTRACT_TEST();
  
  MAKE_TX_LIST_START(events, txs, miner_account, alice, 100, blk_0r);
  CREATE_CONTRACT_TX(events, 0xbe7, "Is coin heads or tails", tx_contract, grading_key.pub);
  txs.push_back(tx_contract);
  MAKE_NEXT_BLOCK_TX_LIST(events, blk_1, blk_0r, miner_account, txs);
  
  SEND_CONTRACT_TX(events, tx_send_contract, 0xbe7,
                   alice, 100, CURRENCY_XPB,
                   alice, bob, blk_1);
  MAKE_NEXT_BLOCK_TX1(events, blk_2, blk_1, miner_account, tx_send_contract);
  
  GRADE_CONTRACT_TX(events, tx_grade, 0xbe7, GRADE_SCALE_MAX / 4, grading_key.sec);
  MAKE_NEXT_BLOCK_TX1(events, blk_3, blk_2, miner_account, tx_grade);
  LOG_PRINT_YELLOW("Making spend txout_backing tx...", LOG_LEVEL_0);

  // can't spend more than the grade
  DO_CALLBACK(events, "mark_invalid_tx");
  MAKE_TX_MIX_CP_FEE_MOD(events, tx_bad_spend_1, alice, carol, 75, 0, blk_3, CP_XPB, 0,
                         [](transaction& tx) { tx_tester(tx).vout[0].amount = 76; });
  // can't grade it more
  DO_CALLBACK(events, "mark_invalid_tx");
  MAKE_TX_MIX_CP_FEE_MOD(events, tx_bad_spend_2, alice, carol, 75, 0, blk_3, CP_XPB, 0,
                         [](transaction& tx) { boost::get<txin_resolve_bc_coins>(tx_tester(tx).vin[1]).graded_amount = 76; });
  // can't grade it less
  DO_CALLBACK(events, "mark_invalid_tx");
  MAKE_TX_MIX_CP_FEE_MOD(events, tx_bad_spend_3, alice, carol, 75, 0, blk_3, CP_XPB, 0,
                         [](transaction& tx) {
                           boost::get<txin_resolve_bc_coins>(tx_tester(tx).vin[1]).graded_amount = 74;
                           tx_tester(tx).vout[0].amount = 74; // avoid failing in/out check
                         });
  // *can* spend less, though (rest goes to fees)
  MAKE_TX_MIX_CP_FEE_MOD(events, tx_ok_spend_4, alice, carol, 75, 0, blk_3, CP_XPB, 0,
                         [](transaction& tx) { tx_tester(tx).vout[0].amount = 70; });
  
  MAKE_NEXT_BLOCK_TX1(events, blk_4, blk_3, miner_account, tx_ok_spend_4);

  return true;
}

bool gen_contracts_grade_spend_contract::generate(std::vector<test_event_entry>& events) const
{
  INIT_CONTRACT_TEST();
  
  MAKE_TX_LIST_START(events, txs, miner_account, alice, 100, blk_0r);
  CREATE_CONTRACT_TX(events, 0xbe7, "Is coin heads or tails", tx_contract, grading_key.pub);
  txs.push_back(tx_contract);
  MAKE_NEXT_BLOCK_TX_LIST(events, blk_1, blk_0r, miner_account, txs);
  
  SEND_CONTRACT_TX(events, tx_send_contract, 0xbe7,
                   alice, 100, CURRENCY_XPB,
                   alice, bob, blk_1);
  MAKE_NEXT_BLOCK_TX1(events, blk_2, blk_1, miner_account, tx_send_contract);
  
  GRADE_CONTRACT_TX(events, tx_grade, 0xbe7, GRADE_SCALE_MAX / 4, grading_key.sec);
  MAKE_NEXT_BLOCK_TX1(events, blk_3, blk_2, miner_account, tx_grade);
  LOG_PRINT_YELLOW("Making spend contract coins tx...", LOG_LEVEL_0);
  // bob should be able to spend 25 or less
  MAKE_TX_MIX_CP_FEE(events, tx_bob_spend, bob, carol, 25, 0, blk_3, CP_XPB, 0)
  MAKE_NEXT_BLOCK_TX1(events, blk_4, blk_3, miner_account, tx_bob_spend);
  
  return true;
}

bool gen_contracts_grade_send_then_spend_contract::generate(std::vector<test_event_entry>& events) const
{
  INIT_CONTRACT_TEST();
  
  MAKE_TX_LIST_START(events, txs, miner_account, alice, 100, blk_0r);
  CREATE_CONTRACT_TX(events, 0xbe7, "Is coin heads or tails", tx_contract, grading_key.pub);
  txs.push_back(tx_contract);
  MAKE_NEXT_BLOCK_TX_LIST(events, blk_1, blk_0r, miner_account, txs);
  
  SEND_CONTRACT_TX(events, tx_send_contract, 0xbe7,
                   alice, 100, CURRENCY_XPB,
                   alice, bob, blk_1);
  MAKE_NEXT_BLOCK_TX1(events, blk_2, blk_1, miner_account, tx_send_contract);
  
  GRADE_CONTRACT_TX(events, tx_grade, 0xbe7, GRADE_SCALE_MAX / 4, grading_key.sec);
  MAKE_NEXT_BLOCK_TX1(events, blk_3, blk_2, miner_account, tx_grade);
  
  // can still send contract coins
  MAKE_TX_MIX_CP_FEE(events, tx_relay_contract, bob, dave, 100, 0, blk_3, coin_type(0xbe7, ContractCoin, CURRENCY_XPB), 0);
  MAKE_NEXT_BLOCK_TX1(events, blk_4, blk_3, miner_account, tx_relay_contract);
  
  // dave should noew be able to spend them
  MAKE_TX_MIX_CP_FEE(events, tx_dave_spend, dave, carol, 25, 0, blk_4, CP_XPB, 0)
  MAKE_NEXT_BLOCK_TX1(events, blk_5, blk_4, miner_account, tx_dave_spend);
  
  return true;
}

bool gen_contracts_grade_cant_send_and_spend_contract_1::generate(std::vector<test_event_entry>& events) const
{
  INIT_CONTRACT_TEST();
  
  MAKE_TX_LIST_START(events, txs, miner_account, alice, 100, blk_0r);
  CREATE_CONTRACT_TX(events, 0xbe7, "Is coin heads or tails", tx_contract, grading_key.pub);
  txs.push_back(tx_contract);
  MAKE_NEXT_BLOCK_TX_LIST(events, blk_1, blk_0r, miner_account, txs);
  
  SEND_CONTRACT_TX(events, tx_send_contract, 0xbe7,
                   alice, 100, CURRENCY_XPB,
                   alice, bob, blk_1);
  MAKE_NEXT_BLOCK_TX1(events, blk_2, blk_1, miner_account, tx_send_contract);
  
  GRADE_CONTRACT_TX(events, tx_grade, 0xbe7, GRADE_SCALE_MAX / 4, grading_key.sec);
  MAKE_NEXT_BLOCK_TX1(events, blk_3, blk_2, miner_account, tx_grade);
  
  // can't have them both in mempool at once
  MAKE_TX_MIX_CP_FEE(events, tx_bob_spend, bob, carol, 25, 0, blk_3, CP_XPB, 0)
  events.pop_back(); // to allow the next tx to be created
  MAKE_TX_MIX_CP_FEE(events, tx_relay_contract, bob, dave, 100, 0, blk_3, coin_type(0xbe7, ContractCoin, CURRENCY_XPB), 0);
  events.pop_back();
  // put things back in the order the should be
  events.push_back(tx_bob_spend);
  DO_CALLBACK(events, "mark_invalid_tx");
  events.push_back(tx_relay_contract);
  
  // also can't put one in block, then other in block
  MAKE_NEXT_BLOCK_TX1(events, blk_4, blk_3, miner_account, tx_bob_spend);
  // this tx is invalid since the source will already be spent
  DO_CALLBACK(events, "mark_invalid_tx");
  events.push_back(tx_relay_contract);
  
  return true;
}

bool gen_contracts_grade_cant_send_and_spend_contract_2::generate(std::vector<test_event_entry>& events) const
{
  INIT_CONTRACT_TEST();
  
  MAKE_TX_LIST_START(events, txs, miner_account, alice, 100, blk_0r);
  CREATE_CONTRACT_TX(events, 0xbe7, "Is coin heads or tails", tx_contract, grading_key.pub);
  txs.push_back(tx_contract);
  MAKE_NEXT_BLOCK_TX_LIST(events, blk_1, blk_0r, miner_account, txs);
  
  SEND_CONTRACT_TX(events, tx_send_contract, 0xbe7,
                   alice, 100, CURRENCY_XPB,
                   alice, bob, blk_1);
  MAKE_NEXT_BLOCK_TX1(events, blk_2, blk_1, miner_account, tx_send_contract);
  
  GRADE_CONTRACT_TX(events, tx_grade, 0xbe7, GRADE_SCALE_MAX / 4, grading_key.sec);
  MAKE_NEXT_BLOCK_TX1(events, blk_3, blk_2, miner_account, tx_grade);
  
  // can't have them both in mempool at once
  MAKE_TX_MIX_CP_FEE(events, tx_relay_contract, bob, dave, 100, 0, blk_3, coin_type(0xbe7, ContractCoin, CURRENCY_XPB), 0);
  events.pop_back(); // to allow the next tx to be created
  MAKE_TX_MIX_CP_FEE(events, tx_bob_spend, bob, carol, 25, 0, blk_3, CP_XPB, 0)
  events.pop_back();
  // put things back in the order the should be
  events.push_back(tx_relay_contract);
  DO_CALLBACK(events, "mark_invalid_tx");
  events.push_back(tx_bob_spend);
  
  // also can't put one in block, then other in block
  MAKE_NEXT_BLOCK_TX1(events, blk_4, blk_3, miner_account, tx_relay_contract);
  // this tx is invalid since the source will already be spent
  DO_CALLBACK(events, "mark_invalid_tx");
  events.push_back(tx_bob_spend);
  
  return true;
}

bool gen_contracts_grade_spend_contract_cant_overspend_misgrade::generate(std::vector<test_event_entry>& events) const
{
  INIT_CONTRACT_TEST();
  
  MAKE_TX_LIST_START(events, txs, miner_account, alice, 100, blk_0r);
  CREATE_CONTRACT_TX(events, 0xbe7, "Is coin heads or tails", tx_contract, grading_key.pub);
  txs.push_back(tx_contract);
  MAKE_NEXT_BLOCK_TX_LIST(events, blk_1, blk_0r, miner_account, txs);
  
  SEND_CONTRACT_TX(events, tx_send_contract, 0xbe7,
                   alice, 100, CURRENCY_XPB,
                   alice, bob, blk_1);
  MAKE_NEXT_BLOCK_TX1(events, blk_2, blk_1, miner_account, tx_send_contract);
  
  GRADE_CONTRACT_TX(events, tx_grade, 0xbe7, GRADE_SCALE_MAX / 4, grading_key.sec);
  MAKE_NEXT_BLOCK_TX1(events, blk_3, blk_2, miner_account, tx_grade);
  LOG_PRINT_YELLOW("Making spend contract coins tx...", LOG_LEVEL_0);

  // can't spend more than the grade
  DO_CALLBACK(events, "mark_invalid_tx");
  MAKE_TX_MIX_CP_FEE_MOD(events, tx_bad_spend_1, bob, carol, 25, 0, blk_3, CP_XPB, 0,
                         [](transaction& tx) { tx_tester(tx).vout[0].amount = 26; });

  // can't grade it more
  DO_CALLBACK(events, "mark_invalid_tx");
  MAKE_TX_MIX_CP_FEE_MOD(events, tx_bad_spend_2, bob, carol, 25, 0, blk_3, CP_XPB, 0,
                         [](transaction& tx) { boost::get<txin_resolve_bc_coins>(tx_tester(tx).vin[1]).graded_amount = 26; });

  // can't grade it less
  DO_CALLBACK(events, "mark_invalid_tx");
  MAKE_TX_MIX_CP_FEE_MOD(events, tx_bad_spend_3, bob, carol, 25, 0, blk_3, CP_XPB, 0,
                         [](transaction& tx) {
                           boost::get<txin_resolve_bc_coins>(tx_tester(tx).vin[1]).graded_amount = 24;
                           tx_tester(tx).vout[0].amount = 24;
                         });
  
  // *can* spend less, though (rest goes to fees)
  MAKE_TX_MIX_CP_FEE_MOD(events, tx_ok_spend_4, bob, carol, 25, 0, blk_3, CP_XPB, 0,
                         [](transaction& tx) { tx_tester(tx).vout[0].amount = 20; });
  
  MAKE_NEXT_BLOCK_TX1(events, blk_4, blk_3, miner_account, tx_ok_spend_4);

  return true;
}

bool gen_contracts_grade_spend_fee::generate(std::vector<test_event_entry>& events) const
{
  INIT_CONTRACT_TEST();
  
  MAKE_TX_LIST_START(events, txs, miner_account, alice, 100, blk_0r);
  // 5% fee
  CREATE_CONTRACT_TX_FULL(events, 0xbe7, "Is coin heads or tails", tx_contract, grading_key.pub,
                          GRADE_SCALE_MAX / 20, 10000, 0, tools::identity());
  txs.push_back(tx_contract);
  MAKE_NEXT_BLOCK_TX_LIST(events, blk_1, blk_0r, miner_account, txs);
  
  SEND_CONTRACT_TX(events, tx_send_contract, 0xbe7,
                   alice, 100, CURRENCY_XPB,
                   alice, bob, blk_1);
  MAKE_NEXT_BLOCK_TX1(events, blk_2, blk_1, miner_account, tx_send_contract);
  
  // 100 coins -> 5 coin spendable in fees
  GRADE_CONTRACT_TX_FULL(events, tx_grade, 0xbe7, GRADE_SCALE_MAX / 4, grading_key.sec,
                         carol, 5, CURRENCY_XPB, tools::identity());
  MAKE_NEXT_BLOCK_TX1(events, blk_3, blk_2, miner_account, tx_grade);
  LOG_PRINT_YELLOW("Making spend fee tx...", LOG_LEVEL_0);
  // carol should be able to spend the fee
  MAKE_TX_MIX_CP_FEE(events, tx_fee_spend, carol, alice, 5, 0, blk_3, CP_XPB, 0)
  MAKE_NEXT_BLOCK_TX1(events, blk_4, blk_3, miner_account, tx_fee_spend);
  
  return true;
}

bool gen_contracts_grade_spend_fee_cant_overspend_misgrade::generate(std::vector<test_event_entry>& events) const
{
  INIT_CONTRACT_TEST();
  
  MAKE_TX_LIST_START(events, txs, miner_account, alice, 100, blk_0r);
  // 5% fee
  CREATE_CONTRACT_TX_FULL(events, 0xbe7, "Is coin heads or tails", tx_contract, grading_key.pub,
                          GRADE_SCALE_MAX / 20, 10000, 0, tools::identity());
  txs.push_back(tx_contract);
  MAKE_NEXT_BLOCK_TX_LIST(events, blk_1, blk_0r, miner_account, txs);
  
  SEND_CONTRACT_TX(events, tx_send_contract, 0xbe7,
                   alice, 100, CURRENCY_XPB,
                   alice, bob, blk_1);
  MAKE_NEXT_BLOCK_TX1(events, blk_2, blk_1, miner_account, tx_send_contract);
  
  // fee should be 5, can't grade it as more or less
  DO_CALLBACK(events, "mark_invalid_tx");
  GRADE_CONTRACT_TX_FULL(events, tx_grade1, 0xbe7, GRADE_SCALE_MAX / 4, grading_key.sec,
                         carol, 6, CURRENCY_XPB, tools::identity());
  
  DO_CALLBACK(events, "mark_invalid_tx");
  GRADE_CONTRACT_TX_FULL(events, tx_grade2, 0xbe7, GRADE_SCALE_MAX / 4, grading_key.sec,
                         carol, 4, CURRENCY_XPB, tools::identity());
  
  // can't give more than the fee
  DO_CALLBACK(events, "mark_invalid_tx");
  GRADE_CONTRACT_TX_FULL(events, tx_grade3, 0xbe7, GRADE_SCALE_MAX / 4, grading_key.sec,
                         carol, 5, CURRENCY_XPB,
                         [](transaction& tx) { tx_tester(tx).vout[0].amount = 6; });

  // *can* give less than the fee
  GRADE_CONTRACT_TX_FULL(events, tx_grade4, 0xbe7, GRADE_SCALE_MAX / 4, grading_key.sec,
                         carol, 5, CURRENCY_XPB,
                         [](transaction& tx) { tx_tester(tx).vout[0].amount = 4; });
  MAKE_NEXT_BLOCK_TX1(events, blk_3, blk_2, miner_account, tx_grade4);
  MAKE_TX_MIX_CP_FEE(events, tx_fee_spend, carol, alice, 4, 0, blk_3, CP_XPB, 0)
  MAKE_NEXT_BLOCK_TX1(events, blk_4, blk_3, miner_account, tx_fee_spend);

  return true;
}

bool gen_contracts_grade_spend_all_rounding::generate(std::vector<test_event_entry>& events) const
{
  INIT_CONTRACT_TEST();
  
  MAKE_TX_LIST_START(events, txs, miner_account, alice, 99, blk_0r);
  // 7% fee
  CREATE_CONTRACT_TX_FULL(events, 0xbe7, "Is coin heads or tails", tx_contract, grading_key.pub,
                          GRADE_SCALE_MAX / 100 * 7, 10000, 0, tools::identity());
  txs.push_back(tx_contract);
  MAKE_NEXT_BLOCK_TX_LIST(events, blk_1, blk_0r, miner_account, txs);
  
  // send two contracts totaling 100
  SEND_CONTRACT_TX(events, tx_send_contract1, 0xbe7,
                   alice, 33, CURRENCY_XPB,
                   alice, bob, blk_1);
  MAKE_NEXT_BLOCK_TX1(events, blk_1b, blk_1, miner_account, tx_send_contract1);
  SEND_CONTRACT_TX(events, tx_send_contract2, 0xbe7,
                   alice, 66, CURRENCY_XPB,
                   alice, bob, blk_1b);
  MAKE_NEXT_BLOCK_TX1(events, blk_2, blk_1b, miner_account, tx_send_contract2);
  
  // 99 coins -> floor(99*.07) = 6 coin spendable in fees
  GRADE_CONTRACT_TX_FULL(events, tx_grade, 0xbe7, GRADE_SCALE_MAX / 4, grading_key.sec,
                         carol, 6, CURRENCY_XPB, tools::identity());
  MAKE_NEXT_BLOCK_TX1(events, blk_3, blk_2, miner_account, tx_grade);
  
  // alice can spend floor(33*.75) = 24 - ceil(24*.07) = 22
  //               + floor(66*.75) = 49 - ceil(49*.07) = 45
  MAKE_TX_MIX_CP_FEE(events, tx_alice_spend, alice, carol, 67, 0, blk_3, CP_XPB, 0)
  // bob can spend floor(33*.25) = 8 - ceil(8*.07) = 7
  //             + floor(66*.25) = 16 - ceil(16*.07) = 14
  MAKE_TX_MIX_CP_FEE(events, tx_bob_spend, bob, carol, 21, 0, blk_3, CP_XPB, 0)
  // carol can spend floor(99*.07) = 6
  MAKE_TX_MIX_CP_FEE(events, tx_fee_spend, carol, alice, 6, 0, blk_3, CP_XPB, 0)
  
  std::list<transaction> tx_list;
  tx_list.push_back(tx_alice_spend);
  tx_list.push_back(tx_bob_spend);
  tx_list.push_back(tx_fee_spend);
  MAKE_NEXT_BLOCK_TX_LIST(events, blk_4b, blk_3, miner_account, tx_list);
  
  // correct fee calculations with large amount of coins + large fee
  CREATE_CONTRACT_TX_FULL(events, 0xabba, "", tx_contract2, grading_key.pub,
                          GRADE_SCALE_MAX, 10000, 0, tools::identity()); // all goes to fees
  MAKE_NEXT_BLOCK_TX1(events, blk_4, blk_4b, miner_account, tx_contract2);
  MAKE_MINT_TX(events, 0xb16, "So many coins", std::numeric_limits<uint64_t>::max(), alice, txmintmany, 2, grading_key.pub);
  MAKE_NEXT_BLOCK_TX1(events, blk_5, blk_4, miner_account, txmintmany);
  
  auto uint64_max = std::numeric_limits<uint64_t>::max();
  
  SEND_CONTRACT_TX(events, tx_send_contract2b, 0xabba,
                   alice, uint64_max, 0xb16,
                   alice, bob, blk_5);
  MAKE_NEXT_BLOCK_TX1(events, blk_6, blk_5, miner_account, tx_send_contract2b);
  
  // can claim all as fees
  GRADE_CONTRACT_TX_FULL(events, tx_grade2, 0xabba, GRADE_SCALE_MAX, grading_key.sec,
                         carol, uint64_max, 0xb16, tools::identity());
  MAKE_NEXT_BLOCK_TX1(events, blk_7, blk_6, miner_account, tx_grade2);

  // no fees, but grade all to the contract coins holder
  CREATE_CONTRACT_TX_FULL(events, 0xabba2, "", tx_contract3, grading_key.pub,
                          0, 10000, 0, tools::identity());
  MAKE_NEXT_BLOCK_TX1(events, blk_8, blk_7, miner_account, tx_contract3);
  MAKE_MINT_TX(events, 0xb162, "", uint64_max, alice, txmintmany2, 2, grading_key.pub);
  MAKE_NEXT_BLOCK_TX1(events, blk_9, blk_8, miner_account, txmintmany2);
  
  SEND_CONTRACT_TX(events, tx_send_contract3, 0xabba2,
                   alice, uint64_max, 0xb162,
                   alice, bob, blk_9);
  MAKE_NEXT_BLOCK_TX1(events, blk_10, blk_9, miner_account, tx_send_contract3);
  GRADE_CONTRACT_TX_FULL(events, tx_grade3, 0xabba2, GRADE_SCALE_MAX, grading_key.sec,
                         carol, 0, 0xb162, tools::identity());
  MAKE_NEXT_BLOCK_TX1(events, blk_11b, blk_10, miner_account, tx_grade3);
  // bob can spend all his coins
  MAKE_TX_MIX_CP_FEE(events, tx_bob_spend_all, bob, carol, uint64_max, 0, blk_11b,
                     coin_type(0xb162, NotContract, BACKED_BY_N_A), 0);
  MAKE_NEXT_BLOCK_TX1(events, blk_11, blk_11b, miner_account, tx_bob_spend_all);
  
  // no fees, send all to backing
  CREATE_CONTRACT_TX_FULL(events, 0xabba3, "", tx_contract4, grading_key.pub,
                          0, 10000, 0, tools::identity());
  MAKE_NEXT_BLOCK_TX1(events, blk_12, blk_11, miner_account, tx_contract4);
  MAKE_MINT_TX(events, 0xb163, "", uint64_max, alice, txmintmany3, 2, grading_key.pub);
  MAKE_NEXT_BLOCK_TX1(events, blk_13, blk_12, miner_account, txmintmany3);
  
  SEND_CONTRACT_TX(events, tx_send_contract4, 0xabba3,
                   alice, uint64_max, 0xb163,
                   alice, bob, blk_13);
  MAKE_NEXT_BLOCK_TX1(events, blk_14, blk_13, miner_account, tx_send_contract4);
  GRADE_CONTRACT_TX_FULL(events, tx_grade4, 0xabba3, 0, grading_key.sec,
                         carol, 0, 0xb163, tools::identity());
  MAKE_NEXT_BLOCK_TX1(events, blk_15, blk_14, miner_account, tx_grade4);
  LOG_PRINT_YELLOW("Looking for alice spend a bunch...", LOG_LEVEL_0);
  MAKE_TX_MIX_CP_FEE(events, tx_alice_spend_all, alice, carol, uint64_max, 0, blk_15,
                     coin_type(0xb163, NotContract, BACKED_BY_N_A), 0);
  MAKE_NEXT_BLOCK_TX1(events, blk_16, blk_15, miner_account, tx_alice_spend_all);
  
  // no fees, each has half
  CREATE_CONTRACT_TX_FULL(events, 0xabba4, "", tx_contract5, grading_key.pub,
                          0, 10000, 0, tools::identity());
  MAKE_NEXT_BLOCK_TX1(events, blk_17, blk_16, miner_account, tx_contract5);
  MAKE_MINT_TX(events, 0xb164, "", uint64_max, alice, txmintmany4, 2, grading_key.pub);
  MAKE_NEXT_BLOCK_TX1(events, blk_18, blk_17, miner_account, txmintmany4);
  
  SEND_CONTRACT_TX(events, tx_send_contract5, 0xabba4,
                   alice, uint64_max, 0xb164,
                   alice, bob, blk_18);
  MAKE_NEXT_BLOCK_TX1(events, blk_19, blk_18, miner_account, tx_send_contract5);
  GRADE_CONTRACT_TX_FULL(events, tx_grade5, 0xabba4, GRADE_SCALE_MAX / 2, grading_key.sec,
                         carol, 0, 0xb164, tools::identity());
  MAKE_NEXT_BLOCK_TX1(events, blk_20, blk_19, miner_account, tx_grade5);
  MAKE_TX_MIX_CP_FEE(events, tx_alice_spend_half, alice, carol, uint64_max / 2, 0, blk_20,
                     coin_type(0xb164, NotContract, BACKED_BY_N_A), 0);
  MAKE_TX_MIX_CP_FEE(events, tx_bob_spend_half, bob, carol, uint64_max / 2, 0, blk_20,
                     coin_type(0xb164, NotContract, BACKED_BY_N_A), 0);
  std::list<transaction> tx_list2;
  tx_list.push_back(tx_alice_spend_half);
  tx_list.push_back(tx_bob_spend_half);
  MAKE_NEXT_BLOCK_TX_LIST(events, blk_21, blk_20, miner_account, tx_list2);
  
  return true;
}

bool gen_contracts_resolve_backing_cant_change_contract::generate(std::vector<test_event_entry>& events) const
{
  INIT_CONTRACT_TEST();

  // make two contracts
  MAKE_TX_LIST_START(events, txs, miner_account, alice, 1000, blk_0r);
  MAKE_TX_LIST(events, txs, miner_account, bob, 1000, blk_0r);
  CREATE_CONTRACT_TX(events, 0xbe7, "Is coin heads or tails", tx_contract, grading_key.pub);
  CREATE_CONTRACT_TX(events, 0xbe72, "Is second coin heads or tails", tx_contract2, grading_key.pub);
  txs.push_back(tx_contract);
  txs.push_back(tx_contract2);
  MAKE_NEXT_BLOCK_TX_LIST(events, blk_1, blk_0r, miner_account, txs);
  
  // send both
  SEND_CONTRACT_TX(events, tx_send, 0xbe7,
                   alice, 1000, CURRENCY_XPB,
                   alice, bob, blk_1);
  SEND_CONTRACT_TX(events, tx_send2, 0xbe72,
                   bob, 1000, CURRENCY_XPB,
                   bob, alice, blk_1);
  MAKE_NEXT_BLOCK_TX1(events, blk_2pre, blk_1, miner_account, tx_send);
  MAKE_NEXT_BLOCK_TX1(events, blk_2, blk_2pre, miner_account, tx_send2);
  // grade both
  GRADE_CONTRACT_TX(events, tx_gradeok, 0xbe7, GRADE_SCALE_MAX / 2, grading_key.sec);
  GRADE_CONTRACT_TX(events, tx_gradeok2, 0xbe72, 0, grading_key.sec);
  MAKE_NEXT_BLOCK_TX1(events, blk_3pre, blk_2, miner_account, tx_gradeok);
  MAKE_NEXT_BLOCK_TX1(events, blk_3, blk_3pre, miner_account, tx_gradeok2);
  
  // can't resolve 0xbe7 using 0xbe72's grade
  // be it the same amount:
  DO_CALLBACK(events, "mark_invalid_tx");
  MAKE_TX_FULL(events, tx_bad1, alice, carol, 500, 0, blk_3, CP_XPB, 0,
               [](transaction& tx) {
                 auto& inp = boost::get<txin_resolve_bc_coins>(tx_tester(tx).vin[1]);
                 inp.contract = 0xbe72;
               });
  
  // or taking advantage of it:
  DO_CALLBACK(events, "mark_invalid_tx");
  MAKE_TX_FULL(events, tx_bad2, alice, carol, 500, 0, blk_3, CP_XPB, 0,
               [](transaction& tx) {
                 auto& inp = boost::get<txin_resolve_bc_coins>(tx_tester(tx).vin[1]);
                 inp.contract = 0xbe72;
                 inp.graded_amount = 1000; // adjust for grade being 0%
                 tx_tester(tx).vout[0].amount = 1000;
               });
  
  return true;
}

bool gen_contracts_resolve_backing_expired::generate(std::vector<test_event_entry>& events) const
{
  INIT_CONTRACT_TEST();
  auto height = get_block_height(boost::get<block>(events.back()));
  
  MAKE_TX_LIST_START(events, txs, miner_account, alice, 1000, blk_0r);
  MAKE_TX_LIST(events, txs, miner_account, bob, 1000, blk_0r);
  MAKE_TX_LIST(events, txs, miner_account, carol, 1000, blk_0r);
  CREATE_CONTRACT_TX_FULL(events, 0xbe7, "Is coin heads or tails", tx_contract, grading_key.pub,
                          GRADE_SCALE_MAX * 4 / 10, height + 20, GRADE_SCALE_MAX / 2, tools::identity());
  CREATE_CONTRACT_TX_FULL(events, 0xbe72, "Is second coin heads or tails", tx_contract2, grading_key.pub,
                          GRADE_SCALE_MAX * 4 / 10, height + 20, GRADE_SCALE_MAX / 2, tools::identity());
  txs.push_back(tx_contract);
  txs.push_back(tx_contract2);
  MAKE_NEXT_BLOCK_TX_LIST(events, blk_1, blk_0r, miner_account, txs);
  
  SEND_CONTRACT_TX(events, tx_send, 0xbe7,
                   alice, 1000, CURRENCY_XPB,
                   alice, emily, blk_1);
  SEND_CONTRACT_TX(events, tx_send2, 0xbe7,
                   bob, 1000, CURRENCY_XPB,
                   bob, emily, blk_1);
  SEND_CONTRACT_TX(events, tx_send3, 0xbe72,
                   carol, 1000, CURRENCY_XPB,
                   carol, emily, blk_1);
  MAKE_NEXT_BLOCK_TX1(events, blk_2pre, blk_1, miner_account, tx_send);
  MAKE_NEXT_BLOCK_TX1(events, blk_2x, blk_2pre, miner_account, tx_send2);
  MAKE_NEXT_BLOCK_TX1(events, blk_2y, blk_2x, miner_account, tx_send3);
  
  // 0xbe7 gets graded, 0xbe72 doesn't, gets the fees (400 * 2 = 800)
  GRADE_CONTRACT_TX_FULL(events, tx_grade, 0xbe7, 0, grading_key.sec,
                         emily, 800, CURRENCY_XPB, tools::identity());
  MAKE_NEXT_BLOCK_TX1(events, blk_2, blk_2y, miner_account, tx_grade);
  
  // alice sends before it would have expired, at the grade of 0, with fees: 1000 - 400 = 600
  MAKE_TX_MIX_CP_FEE(events, tx_alice_spend, alice, emily, 600, 0, blk_2, CP_XPB, 0)
  MAKE_NEXT_BLOCK_TX1(events, blk_2r_pre, blk_2, miner_account, tx_alice_spend);
  
  // enough time passes to expire
  REWIND_BLOCKS_N(events, blk_2r, blk_2r_pre, miner_account, 20);
  
  // bob sends 1000 from same contract as alice, this should be at same rate of 600 (would be 500 if it expired)
  DO_CALLBACK(events, "mark_invalid_tx");
  MAKE_TX_FULL(events, tx_bob_spend_bad, bob, emily, 600, 0, blk_2r, CP_XPB, 0,
               [](transaction& tx) {
                 boost::get<txin_resolve_bc_coins>(tx_tester(tx).vin[1]).graded_amount = 500;
                 tx_tester(tx).vout[0].amount = 500;
               });
  
  MAKE_TX_MIX_CP_FEE(events, tx_bob_spend, bob, emily, 600, 0, blk_2r, CP_XPB, 0);
  
  MAKE_NEXT_BLOCK_TX1(events, blk_3, blk_2r, miner_account, tx_bob_spend);
  // carol would spend at default rate: no fee, default grade is 1/2, so 500 left from the 1000
  DO_CALLBACK(events, "mark_invalid_tx");
  MAKE_TX_FULL(events, tx_carol_spend_bad, carol, emily, 500, 0, blk_2r, CP_XPB, 0,
               [](transaction& tx) {
                 boost::get<txin_resolve_bc_coins>(tx_tester(tx).vin[1]).graded_amount = 600;
                 tx_tester(tx).vout[0].amount = 600;
               });
  
  MAKE_TX_MIX_CP_FEE(events, tx_carol_spend, carol, emily, 500, 0, blk_3, CP_XPB, 0);
  
  MAKE_NEXT_BLOCK_TX1(events, blk_4, blk_3, miner_account, tx_carol_spend);

  return true;
}

bool gen_contracts_resolve_contract_cant_change_contract::generate(std::vector<test_event_entry>& events) const
{
  INIT_CONTRACT_TEST();

  // make two contracts
  MAKE_TX_LIST_START(events, txs, miner_account, carol, 1000, blk_0r);
  MAKE_TX_LIST(events, txs, miner_account, carol, 1000, blk_0r);
  CREATE_CONTRACT_TX(events, 0xbe7, "Is coin heads or tails", tx_contract, grading_key.pub);
  CREATE_CONTRACT_TX(events, 0xbe72, "Is second coin heads or tails", tx_contract2, grading_key.pub);
  txs.push_back(tx_contract);
  txs.push_back(tx_contract2);
  MAKE_NEXT_BLOCK_TX_LIST(events, blk_1, blk_0r, miner_account, txs);
  
  // send both
  SEND_CONTRACT_TX(events, tx_send, 0xbe7,
                   carol, 1000, CURRENCY_XPB,
                   carol, alice, blk_1);
  SEND_CONTRACT_TX(events, tx_send2, 0xbe72,
                   carol, 1000, CURRENCY_XPB,
                   carol, bob, blk_1);
  MAKE_NEXT_BLOCK_TX1(events, blk_2pre, blk_1, miner_account, tx_send);
  MAKE_NEXT_BLOCK_TX1(events, blk_2, blk_2pre, miner_account, tx_send2);
  // grade both
  GRADE_CONTRACT_TX(events, tx_gradeok, 0xbe7, GRADE_SCALE_MAX / 2, grading_key.sec);
  GRADE_CONTRACT_TX(events, tx_gradeok2, 0xbe72, GRADE_SCALE_MAX * 3 / 4, grading_key.sec);
  MAKE_NEXT_BLOCK_TX1(events, blk_3pre, blk_2, miner_account, tx_gradeok);
  MAKE_NEXT_BLOCK_TX1(events, blk_3, blk_3pre, miner_account, tx_gradeok2);
  
  // can't resolve 0xbe7 using 0xbe72's grade
  // be it the same amount:
  DO_CALLBACK(events, "mark_invalid_tx");
  MAKE_TX_FULL(events, tx_bad1, alice, carol, 500, 0, blk_3, CP_XPB, 0,
               [](transaction& tx) {
                 auto& inp = boost::get<txin_resolve_bc_coins>(tx_tester(tx).vin[1]);
                 inp.contract = 0xbe72;
               });
  
  // or taking advantage of it:
  DO_CALLBACK(events, "mark_invalid_tx");
  MAKE_TX_FULL(events, tx_bad2, alice, carol, 500, 0, blk_3, CP_XPB, 0,
               [](transaction& tx) {
                 auto& inp = boost::get<txin_resolve_bc_coins>(tx_tester(tx).vin[1]);
                 inp.contract = 0xbe72;
                 inp.graded_amount = 750; // adjust for grade being 75%
                 tx_tester(tx).vout[0].amount = 750;
               });
  
  return true;
}

bool gen_contracts_resolve_contract_expired::generate(std::vector<test_event_entry>& events) const
{
  INIT_CONTRACT_TEST();
  auto height = get_block_height(boost::get<block>(events.back()));
  
  MAKE_TX_LIST_START(events, txs, miner_account, emily, 1000, blk_0r);
  MAKE_TX_LIST(events, txs, miner_account, emily, 1000, blk_0r);
  MAKE_TX_LIST(events, txs, miner_account, emily, 1000, blk_0r);
  CREATE_CONTRACT_TX_FULL(events, 0xbe7, "Is coin heads or tails", tx_contract, grading_key.pub,
                          GRADE_SCALE_MAX * 4 / 10, height + 20, GRADE_SCALE_MAX / 2, tools::identity());
  CREATE_CONTRACT_TX_FULL(events, 0xbe72, "Is second coin heads or tails", tx_contract2, grading_key.pub,
                          GRADE_SCALE_MAX * 4 / 10, height + 20, GRADE_SCALE_MAX / 2, tools::identity());
  txs.push_back(tx_contract);
  txs.push_back(tx_contract2);
  MAKE_NEXT_BLOCK_TX_LIST(events, blk_1, blk_0r, miner_account, txs);
  
  SEND_CONTRACT_TX(events, tx_send, 0xbe7,
                   emily, 1000, CURRENCY_XPB,
                   emily, alice, blk_1);
  SEND_CONTRACT_TX(events, tx_send2, 0xbe7,
                   emily, 1000, CURRENCY_XPB,
                   emily, bob, blk_1);
  SEND_CONTRACT_TX(events, tx_send3, 0xbe72,
                   emily, 1000, CURRENCY_XPB,
                   emily, carol, blk_1);
  MAKE_NEXT_BLOCK_TX1(events, blk_2pre, blk_1, miner_account, tx_send);
  MAKE_NEXT_BLOCK_TX1(events, blk_2x, blk_2pre, miner_account, tx_send2);
  MAKE_NEXT_BLOCK_TX1(events, blk_2y, blk_2x, miner_account, tx_send3);
  
  // 0xbe7 gets graded, 0xbe72 doesn't, gets the fees (400 * 2 = 800)
  GRADE_CONTRACT_TX_FULL(events, tx_grade, 0xbe7, GRADE_SCALE_MAX / 4, grading_key.sec,
                         emily, 800, CURRENCY_XPB, tools::identity());
  MAKE_NEXT_BLOCK_TX1(events, blk_2, blk_2y, miner_account, tx_grade);
  
  // alice sends before it would have expired, at the grade of 0, with fees: 250 - 100 = 150
  MAKE_TX_MIX_CP_FEE(events, tx_alice_spend, alice, dave, 150, 0, blk_2, CP_XPB, 0)
  MAKE_NEXT_BLOCK_TX1(events, blk_2r_pre, blk_2, miner_account, tx_alice_spend);
  
  // enough time passes to expire
  REWIND_BLOCKS_N(events, blk_2r, blk_2r_pre, miner_account, 20);
  
  // bob sends 1000 from same contract as alice, this should be at same rate of 150 (would be 500 if it expired)
  DO_CALLBACK(events, "mark_invalid_tx");
  MAKE_TX_FULL(events, tx_bob_spend_bad, bob, dave, 150, 0, blk_2r, CP_XPB, 0,
               [](transaction& tx) {
                 boost::get<txin_resolve_bc_coins>(tx_tester(tx).vin[1]).graded_amount = 500;
                 tx_tester(tx).vout[0].amount = 500;
               });
  
  MAKE_TX_MIX_CP_FEE(events, tx_bob_spend, bob, dave, 150, 0, blk_2r, CP_XPB, 0);
  
  MAKE_NEXT_BLOCK_TX1(events, blk_3, blk_2r, miner_account, tx_bob_spend);
  // carol would spend at default rate: no fee, default grade is 1/2, so 500 left from the 1000
  DO_CALLBACK(events, "mark_invalid_tx");
  MAKE_TX_FULL(events, tx_carol_spend_bad, carol, dave, 500, 0, blk_2r, CP_XPB, 0,
               [](transaction& tx) {
                 boost::get<txin_resolve_bc_coins>(tx_tester(tx).vin[1]).graded_amount = 150;
                 tx_tester(tx).vout[0].amount = 150;
               });
  MAKE_TX_MIX_CP_FEE(events, tx_carol_spend, carol, dave, 500, 0, blk_3, CP_XPB, 0);
  
  MAKE_NEXT_BLOCK_TX1(events, blk_4, blk_3, miner_account, tx_carol_spend);
  
  return true;
}

bool gen_contracts_extra_currency_checks::generate(std::vector<test_event_entry>& events) const
{
  INIT_CONTRACT_TEST();
  
  MAKE_TX_LIST_START(events, txs, miner_account, alice, 100, blk_0r);
  CREATE_CONTRACT_TX_FULL(events, 0xbe7, "Is coin heads or tails", tx_contract, grading_key.pub,
                          GRADE_SCALE_MAX / 20, 10000, 0, tools::identity());
  txs.push_back(tx_contract);
  MAKE_MINT_TX(events, 0xbeef, "Beef futures", 1000, alice, txmint, 2, grading_key.pub);
  txs.push_back(txmint);
  MAKE_NEXT_BLOCK_TX_LIST(events, blk_1, blk_0r, miner_account, txs);
  
  // can't mix using an existing contract id
  DO_CALLBACK(events, "mark_invalid_tx");
  MAKE_MINT_TX(events, 0xbe7, "", 1000, alice, txbad1, 2, null_pkey);
  
  // can't mint with a backed_by being anything but BACKED_BY_N_A
  DO_CALLBACK(events, "mark_invalid_tx");
  MAKE_MINT_TX_FULL(events, 0xbeef, "Beef futures", 1000, alice, txbad2, 2, null_pkey,
                    [](transaction& tx) {
                      tx_tester(tx).vin_coin_types[0].backed_by_currency = CURRENCY_XPB;
                      tx_tester(tx).vout_coin_types[0].backed_by_currency = CURRENCY_XPB;
                    });
  DO_CALLBACK(events, "mark_invalid_tx");
  MAKE_MINT_TX_FULL(events, 0xbeef, "Beef futures", 1000, alice, txbad3, 2, null_pkey,
                    [](transaction& tx) {
                      tx_tester(tx).vin_coin_types[0].backed_by_currency = 0x12345;
                      tx_tester(tx).vout_coin_types[0].backed_by_currency = 0x12345;
                    });
  // with existing currency:
  DO_CALLBACK(events, "mark_invalid_tx");
  MAKE_MINT_TX_FULL(events, 0xbe7, "", 1000, alice, txbad4, 2, null_pkey,
                    [](transaction& tx) {
                      tx_tester(tx).vin_coin_types[0].backed_by_currency = 0xbeef;
                      tx_tester(tx).vout_coin_types[0].backed_by_currency = 0xbeef;
                    });

  // same with remint
  DO_CALLBACK(events, "mark_invalid_tx");
  MAKE_REMINT_TX_FULL(events, 0xbeef, 5555, alice, txbad5, grading_key.sec, null_pkey,
                      [](transaction& tx) {
                        tx_tester(tx).vin_coin_types[0].backed_by_currency = CURRENCY_XPB;
                        tx_tester(tx).vout_coin_types[0].backed_by_currency = CURRENCY_XPB;
                      });
  DO_CALLBACK(events, "mark_invalid_tx");
  MAKE_REMINT_TX_FULL(events, 0xbeef, 5555, alice, txbad6, grading_key.sec, null_pkey,
                      [](transaction& tx) {
                        tx_tester(tx).vin_coin_types[0].backed_by_currency = 0x12345;
                        tx_tester(tx).vout_coin_types[0].backed_by_currency = 0x12345;
                      });
  DO_CALLBACK(events, "mark_invalid_tx");
  MAKE_REMINT_TX_FULL(events, 0xbeef, 5555, alice, txbad7, grading_key.sec, null_pkey,
                      [](transaction& tx) {
                        tx_tester(tx).vin_coin_types[0].backed_by_currency = 0xbeef;
                        tx_tester(tx).vout_coin_types[0].backed_by_currency = 0xbeef;
                      });
  
  return true;
}

bool gen_contracts_create_checks::generate(std::vector<test_event_entry>& events) const
{
  INIT_CONTRACT_TEST();
  // valid contract id (>= 256)
  DO_CALLBACK(events, "mark_invalid_tx"); CREATE_CONTRACT_TX(events, 0x0, "", bad1, grading_key.pub);
  DO_CALLBACK(events, "mark_invalid_tx"); CREATE_CONTRACT_TX(events, 0x15, "", bad2, grading_key.pub);
  DO_CALLBACK(events, "mark_invalid_tx"); CREATE_CONTRACT_TX(events, CURRENCY_XPB, "", bad2a, grading_key.pub);
  DO_CALLBACK(events, "mark_invalid_tx"); CREATE_CONTRACT_TX(events, CURRENCY_INVALID, "", bad2b, grading_key.pub);
  DO_CALLBACK(events, "mark_invalid_tx"); CREATE_CONTRACT_TX(events, BACKED_BY_INVALID, "", bad2c, grading_key.pub);
  DO_CALLBACK(events, "mark_invalid_tx"); CREATE_CONTRACT_TX(events, CURRENCY_N_A, "", bad2d, grading_key.pub);
  DO_CALLBACK(events, "mark_invalid_tx"); CREATE_CONTRACT_TX(events, BACKED_BY_N_A, "", bad2e, grading_key.pub);
  DO_CALLBACK(events, "mark_invalid_tx"); CREATE_CONTRACT_TX(events, 0xff, "", bad3, grading_key.pub);

  // .contract matches .in_currency
  DO_CALLBACK(events, "mark_invalid_tx");
  CREATE_CONTRACT_TX_FULL(events, 0xbe7, "", bad4, grading_key.pub, 0, 10000, 0,
                          [](transaction& tx){ tx_tester(tx).vin_coin_types[0].currency++; });
  // .in_backed_by is BACKED_BY_N_A
  DO_CALLBACK(events, "mark_invalid_tx");
  CREATE_CONTRACT_TX_FULL(events, 0xbe7, "", bad5, grading_key.pub, 0, 10000, 0,
                          [](transaction& tx){ tx_tester(tx).vin_coin_types[0].backed_by_currency = 0x488; });
  
  // null/invalid pkey
  DO_CALLBACK(events, "mark_invalid_tx");
  CREATE_CONTRACT_TX(events, 0xbe7, "", bad6, null_pkey);
  DO_CALLBACK(events, "mark_invalid_tx");
  CREATE_CONTRACT_TX(events, 0xbe7, "", bad7, generate_invalid_pub_key());
  
  // expiry block is too soon
  DO_CALLBACK(events, "mark_invalid_tx");
  CREATE_CONTRACT_TX_FULL(events, 0xbe7, "", bad8, grading_key.pub, 0, 0, 0, tools::identity());
  DO_CALLBACK(events, "mark_invalid_tx");
  CREATE_CONTRACT_TX_FULL(events, 0xbe7, "", bad9, grading_key.pub, 0, 5, 0, tools::identity());
  // expiry block is too huge
  DO_CALLBACK(events, "mark_invalid_tx");
  CREATE_CONTRACT_TX_FULL(events, 0xbe7, "", bad9b, grading_key.pub, 0, CRYPTONOTE_MAX_BLOCK_NUMBER + 1, 0, tools::identity());
  
  // fee scale/default grade is off
  DO_CALLBACK(events, "mark_invalid_tx");
  CREATE_CONTRACT_TX_FULL(events, 0xbe7, "", bad10, grading_key.pub, GRADE_SCALE_MAX + 1, 10000, 0, tools::identity());
  DO_CALLBACK(events, "mark_invalid_tx");
  CREATE_CONTRACT_TX_FULL(events, 0xbe7, "", bad11, grading_key.pub, 0, 10000, GRADE_SCALE_MAX + 1, tools::identity());
  
  // already-existing contract
  CREATE_CONTRACT_TX(events, 0x100, "", ok1, grading_key.pub);
  MAKE_NEXT_BLOCK_TX1(events, blk_1, blk_0r, miner_account, ok1);
  DO_CALLBACK(events, "mark_invalid_tx");
  CREATE_CONTRACT_TX(events, 0x100, "", bad12, grading_key.pub);
  
  // already-existing currency
  MAKE_MINT_TX(events, 0xbeef, "Beef futures", 1000, alice, txmint, 2, null_pkey);
  MAKE_NEXT_BLOCK_TX1(events, blk_2, blk_1, miner_account, txmint);
  DO_CALLBACK(events, "mark_invalid_tx");
  CREATE_CONTRACT_TX(events, 0xbeef, "", bad13, grading_key.pub);
  
  // too-large description
  DO_CALLBACK(events, "mark_invalid_tx");
  CREATE_CONTRACT_TX(events, 0xbe7, "This description is too big" + std::string(CONTRACT_DESCRIPTION_MAX_SIZE + 5, '.'),
                     bad14, grading_key.pub);
  // already-used description
  CREATE_CONTRACT_TX(events, 0xbebebe, "A baby", ok2, grading_key.pub);
  MAKE_NEXT_BLOCK_TX1(events, blk_3, blk_2, miner_account, ok2);
  DO_CALLBACK(events, "mark_invalid_tx");
  CREATE_CONTRACT_TX(events, 0xbeadbef, "A baby", bad15, grading_key.pub);
  
  return true;
}

bool gen_contracts_mint_checks::generate(std::vector<test_event_entry>& events) const
{
  INIT_CONTRACT_TEST();
  
  MAKE_TX_LIST_START(events, txs, miner_account, alice, 1000, blk_0r);
  CREATE_CONTRACT_TX(events, 0xbe7, "Is coin heads or tails", tx_contract, grading_key.pub);
  txs.push_back(tx_contract);
  CREATE_CONTRACT_TX(events, 0xfaaf, "Diversionary contract", tx_contract2, grading_key.pub);
  txs.push_back(tx_contract2);
  MAKE_NEXT_BLOCK_TX_LIST(events, blk_1pre, blk_0r, miner_account, txs);
  
  // send enough money to queue all these up
  REWIND_BLOCKS_N(events, blk_1almost, blk_1pre, miner_account, 10);
  std::list<transaction> send_list;
  for (size_t i=0; i < 2; i++) {
    MAKE_TX(events, txsend, miner_account, alice, 1000, blk_1almost);
    send_list.push_back(txsend);
  }
  // mint currencies for use in some test cases
  MAKE_MINT_TX(events, 0xbeef, "Beef futures", 1000, alice, txmint, 2, null_pkey);
  send_list.push_back(txmint);
  // send a valid contract to be able to use, alice has both a backing and 0xfaaf/XPB
  SEND_CONTRACT_TX(events, tx_ok, 0xfaaf, alice, 1000, CURRENCY_XPB, alice, alice, blk_1almost);
  send_list.push_back(tx_ok);
  MAKE_NEXT_BLOCK_TX_LIST(events, blk_1, blk_1almost, miner_account, send_list);

  // invalid contract id
  DO_CALLBACK(events, "mark_invalid_tx");
  SEND_CONTRACT_TX_FULL(events, tx_bad1, CURRENCY_XPB,
                        alice, 1000, CURRENCY_XPB,
                        alice, bob, blk_1, tools::identity());
  // mis-match in_currency and .contract
  DO_CALLBACK(events, "mark_invalid_tx");
  SEND_CONTRACT_TX_FULL(events, tx_bad2, 0xbe7,
                        alice, 1000, CURRENCY_XPB,
                        alice, bob, blk_1,
                        [](transaction& tx) { boost::get<txin_mint_contract>(tx_tester(tx).vin[1]).contract++; });
  // mis-match backed_by_currency
  DO_CALLBACK(events, "mark_invalid_tx");
  SEND_CONTRACT_TX_FULL(events, tx_bad3, 0xbe7,
                        alice, 1000, CURRENCY_XPB,
                        alice, bob, blk_1,
                        [](transaction& tx) { boost::get<txin_mint_contract>(tx_tester(tx).vin[1]).backed_by_currency++; });
  // backed by currency mismatch: using currency that we don't have inputs for
  DO_CALLBACK(events, "mark_invalid_tx");
  SEND_CONTRACT_TX_FULL(events, tx_bad5, 0xbe7,
                        alice, 1000, CURRENCY_XPB,
                        alice, bob, blk_1,
                        [](transaction& tx) {
                          boost::get<txin_mint_contract>(tx_tester(tx).vin[1]).backed_by_currency = 0xbeef;
                          tx_tester(tx).vout_coin_types[0].backed_by_currency = 0xbeef;
                          tx_tester(tx).vout_coin_types[1].backed_by_currency = 0xbeef;
                        });
  // backed by currency mismatch: outputting coins of wrong backing
  DO_CALLBACK(events, "mark_invalid_tx");
  SEND_CONTRACT_TX_FULL(events, tx_bad5b, 0xbe7,
                        alice, 1000, CURRENCY_XPB,
                        alice, bob, blk_1,
                        [](transaction& tx) {
                          tx_tester(tx).vout_coin_types[0].backed_by_currency++;
                        });
  DO_CALLBACK(events, "mark_invalid_tx");
  SEND_CONTRACT_TX_FULL(events, tx_bad5c, 0xbe7,
                        alice, 1000, CURRENCY_XPB,
                        alice, bob, blk_1,
                        [](transaction& tx) {
                          tx_tester(tx).vout_coin_types[1].backed_by_currency--;
                        });
  
  // invalid backed by a contract
  DO_CALLBACK(events, "mark_invalid_tx");
  {
    transaction tx_send_contract;
    tx_builder txb;
    txb.init(0, std::vector<uint8_t>(), true); // ignore checks
    
    // fill with 0xfaaf contract coins backed by XPB (sent at start of this function)
    std::vector<tx_source_entry> sources;
    CHECK_AND_ASSERT_MES(fill_tx_sources(sources, events, blk_1, alice, 1000, 0,
                                         coin_type(0xfaaf, ContractCoin, CURRENCY_XPB)), false,
                         "Couldn't fill sources");
    
    std::vector<tx_destination_entry> destinations;
    destinations.push_back(tx_destination_entry(coin_type(0xbe7, BackingCoin, 0xfaaf),
                                                1000, alice.get_keys().m_account_address));
    destinations.push_back(tx_destination_entry(coin_type(0xbe7, ContractCoin, 0xfaaf),
                                                1000, bob.get_keys().m_account_address));
    
    CHECK_AND_ASSERT_MES(txb.add_mint_contract(alice.get_keys(), 0xfaaf, 1000,
                                               sources, destinations),
                         false, "Couldn't add_mint_contract");
    CHECK_AND_ASSERT_MES(txb.finalize([](transaction& tx){ }), false, "Couldn't finalize");
    CHECK_AND_ASSERT_MES(txb.get_finalized_tx(tx_send_contract), false, "Couldn't get finalized tx");
    
    events.push_back(tx_send_contract);
  }
  // mis-match in amount, both more and less
  DO_CALLBACK(events, "mark_invalid_tx");
  SEND_CONTRACT_TX_FULL(events, tx_bad7, 0xbe7,
                        alice, 1000, CURRENCY_XPB,
                        alice, bob, blk_1,
                        [](transaction& tx) { boost::get<txin_mint_contract>(tx_tester(tx).vin[1]).amount += 1; });
  DO_CALLBACK(events, "mark_invalid_tx");
  SEND_CONTRACT_TX_FULL(events, tx_bad8, 0xbe7,
                        alice, 1000, CURRENCY_XPB,
                        alice, bob, blk_1,
                        [](transaction& tx) {
                          boost::get<txin_mint_contract>(tx_tester(tx).vin[1]).amount -= 1;
                        });
  // can't send more backing/contract coins than were minted (standard in/out check)
  DO_CALLBACK(events, "mark_invalid_tx");
  SEND_CONTRACT_TX_FULL(events, tx_bad8b, 0xbe7,
                        alice, 1000, CURRENCY_XPB,
                        alice, bob, blk_1,
                        [](transaction& tx) {
                          tx_tester(tx).vout[0].amount += 1;
                        });
  DO_CALLBACK(events, "mark_invalid_tx");
  SEND_CONTRACT_TX_FULL(events, tx_bad8c, 0xbe7,
                        alice, 1000, CURRENCY_XPB,
                        alice, bob, blk_1,
                        [](transaction& tx) {
                          tx_tester(tx).vout[1].amount += 1;
                        });
  // non-existent contract
  DO_CALLBACK(events, "mark_invalid_tx");
  SEND_CONTRACT_TX_FULL(events, tx_bad9, 0xfefefe,
                        alice, 1000, CURRENCY_XPB,
                        alice, bob, blk_1, tools::identity());
  // non-existent currency
  DO_CALLBACK(events, "mark_invalid_tx");
  SEND_CONTRACT_TX_FULL(events, tx_bad11, 0xbe7,
                        alice, 1000, CURRENCY_XPB,
                        alice, bob, blk_1,
                        [](transaction& tx) {
                          boost::get<txin_mint_contract>(tx_tester(tx).vin[1]).backed_by_currency = 0xaffaf;
                          tx_tester(tx).vin_coin_types[0].currency = 0xaffaf;
                          tx_tester(tx).vout_coin_types[0].backed_by_currency = 0xaffaf;
                          tx_tester(tx).vout_coin_types[1].backed_by_currency = 0xaffaf;
                        });
  
  // can't use two mints without more inputs
  DO_CALLBACK(events, "mark_invalid_tx");
  SEND_CONTRACT_TX_FULL(events, tx_bad11b, 0xbe7,
                        alice, 1000, CURRENCY_XPB,
                        alice, bob, blk_1,
                        [](transaction& tx) {
                          // duplicate the txin_mint_contract
                          tx.add_in(tx.ins().back(),
                                    coin_type(tx_tester(tx).vin_coin_types.back()));
                        });

  // send graded contract
  GRADE_CONTRACT_TX(events, tx_grade, 0xbe7, 0, grading_key.sec);
  MAKE_NEXT_BLOCK_TX1(events, blk_2, blk_1, miner_account, tx_grade); // height H
  auto height = get_block_height(boost::get<block>(events.back()));
  DO_CALLBACK(events, "mark_invalid_tx");
  SEND_CONTRACT_TX(events, tx_bad12, 0xbe7,
                   alice, 1000, CURRENCY_XPB,
                   alice, bob, blk_2);
  // send expired contract
  CREATE_CONTRACT_TX_FULL(events, 0xfaba, "", tx_contract_2, grading_key.pub,
                          0, height + 3, 0, tools::identity());
  MAKE_NEXT_BLOCK_TX1(events, blk_3, blk_2, miner_account, tx_contract_2); // H + 1
  // regular send works...
  SEND_CONTRACT_TX(events, tx_send, 0xfaba,
                   alice, 100, CURRENCY_XPB,
                   alice, bob, blk_3);
  MAKE_NEXT_BLOCK_TX1(events, blk_4, blk_3, miner_account, tx_send); // H + 2
  // but now it no longer should
  DO_CALLBACK(events, "mark_invalid_tx");
  SEND_CONTRACT_TX(events, tx_bad13, 0xfaba,  // block this tx would be on is H + 3
                   alice, 100, CURRENCY_XPB,
                   alice, bob, blk_4);
  
  // shouldn't have more than 2^64-1 of any contract coin...  but should be impossible
  // given there can't be more than that many coins of any subcurrency, period.
  
  return true;
}

void re_sign_grade(txin_grade_contract& txin, const crypto::secret_key& grading_secret_key)
{
  crypto::hash prefix_hash = txin.get_prefix_hash();
  crypto::public_key grading_pub_key;
  secret_key_to_public_key(grading_secret_key, grading_pub_key);
  generate_signature(prefix_hash, grading_pub_key, grading_secret_key, txin.sig);
}

bool gen_contracts_grade_checks::generate(std::vector<test_event_entry>& events) const
{
  INIT_CONTRACT_TEST();
  
  CREATE_CONTRACT_TX(events, 0xbe7, "Is coin heads or tails", tx_contract, grading_key.pub);
  // can't grade it before it's in a block
  DO_CALLBACK(events, "mark_invalid_tx");
  GRADE_CONTRACT_TX(events, txbad1, 0xbe7, 0, grading_key.sec);
  MAKE_NEXT_BLOCK_TX1(events, blk_1, blk_0r, miner_account, tx_contract);
  
  // invalid ID
  DO_CALLBACK(events, "mark_invalid_tx");
  GRADE_CONTRACT_TX(events, txbad2, 0x13, 0, grading_key.sec);
  // .contract matches .in_currency
  DO_CALLBACK(events, "mark_invalid_tx");
  GRADE_CONTRACT_TX_FULL(events, txbad3, 0xbe7, 0, grading_key.sec, alice, 0, CURRENCY_XPB,
                         [&grading_key](transaction& tx) {
                           auto& txin = boost::get<txin_grade_contract>(tx_tester(tx).vin[0]);
                           txin.contract += 1;
                           re_sign_grade(txin, grading_key.sec);
                         });
  DO_CALLBACK(events, "mark_invalid_tx");
  GRADE_CONTRACT_TX_FULL(events, txbad4, 0xbe7, 0, grading_key.sec, alice, 0, CURRENCY_XPB,
                         [](transaction& tx) { tx_tester(tx).vin_coin_types[0].currency += 1; });
  // backed_by is BACKED_BY_N_A
  DO_CALLBACK(events, "mark_invalid_tx");
  GRADE_CONTRACT_TX_FULL(events, txbad5, 0xbe7, 0, grading_key.sec, alice, 0, CURRENCY_XPB,
                         [](transaction& tx) { tx_tester(tx).vin_coin_types[0].backed_by_currency = BACKED_BY_N_A + 1; });
  // grade scale max is too high
  DO_CALLBACK(events, "mark_invalid_tx");
  GRADE_CONTRACT_TX(events, txbad6, 0xbe7, GRADE_SCALE_MAX + 1, grading_key.sec);
  // non-existent contract
  DO_CALLBACK(events, "mark_invalid_tx");
  GRADE_CONTRACT_TX(events, txbad7, 0xbe7abba, 0, grading_key.sec);
  // can't pay fees in non-existent subcurrencies
  DO_CALLBACK(events, "mark_invalid_tx");
  GRADE_CONTRACT_TX_FULL(events, txbad8, 0xbe7, 0, grading_key.sec, alice, 0, CURRENCY_XPB,
                         [&grading_key](transaction& tx) {
                           auto& txin = boost::get<txin_grade_contract>(tx_tester(tx).vin[0]);
                           txin.fee_amounts[0xabbabab] = 10;
                           re_sign_grade(txin, grading_key.sec);
                         });

  // can't pay more/less fee than is deserved - covered in gen_contracts_grade_spend_fee_cant_overspend_misgrade
  
  // sig must be valid
  auto bad_grading_key = keypair::generate();
  DO_CALLBACK(events, "mark_invalid_tx");
  GRADE_CONTRACT_TX(events, txbad9, 0xbe7, 0, bad_grading_key.sec);

  // can actually grade it
  GRADE_CONTRACT_TX(events, tx_gradeok, 0xbe7, 0, grading_key.sec);
  // can't grade it twice in mem pool
  DO_CALLBACK(events, "mark_invalid_tx");
  GRADE_CONTRACT_TX(events, txbad10, 0xbe7, 0, grading_key.sec);
  
  // can't grade it twice in blocks
  MAKE_NEXT_BLOCK_TX1(events, blk_2, blk_1, miner_account, tx_gradeok); // height H
  auto height = get_block_height(boost::get<block>(events.back()));
  DO_CALLBACK(events, "mark_invalid_tx");
  GRADE_CONTRACT_TX(events, txbad11, 0xbe7, 0, grading_key.sec);
  
  // can't grade expired contract
  CREATE_CONTRACT_TX_FULL(events, 0xbeeeeb, "", tx_contract2, grading_key.pub,
                          0, height+2, 0, tools::identity());
  MAKE_NEXT_BLOCK_TX1(events, blk_3, blk_2, miner_account, tx_contract2); // height H + 1 (so H+2 is not expired yet)
  DO_CALLBACK(events, "mark_invalid_tx");
  GRADE_CONTRACT_TX(events, txbad12, 0xbeeeeb, 0, grading_key.sec); // this will be on block height H + 2 so should be invalid
  
  return true;
}

bool gen_contracts_resolve_backing_checks::generate(std::vector<test_event_entry>& events) const
{
  const size_t num_sends = 2;
  
  INIT_CONTRACT_TEST();
  
  MAKE_TX_LIST_START(events, txs, miner_account, alice, 1000, blk_0r);
  CREATE_CONTRACT_TX(events, 0xbe7, "Is coin heads or tails", tx_contract, grading_key.pub);
  CREATE_CONTRACT_TX(events, 0xbe72, "Is second coin heads or tails", tx_contract2, grading_key.pub);
  txs.push_back(tx_contract);
  txs.push_back(tx_contract2);
  MAKE_NEXT_BLOCK_TX_LIST(events, blk_1pre, blk_0r, miner_account, txs);
  
  // send enough money to queue all these up
  REWIND_BLOCKS_N(events, blk_1almost, blk_1pre, miner_account, 10);
  std::list<transaction> send_list;
  for (size_t i=0; i < num_sends; i++) {
    MAKE_TX(events, txsend, miner_account, alice, 1000, blk_1almost);
    send_list.push_back(txsend);
  }
  MAKE_NEXT_BLOCK_TX_LIST(events, blk_1, blk_1almost, miner_account, send_list);
  
  std::list<transaction> send_list2;
  for (size_t i=0; i < num_sends; i++) {
    SEND_CONTRACT_TX(events, tx_send, 0xbe7,
                     alice, 1000, CURRENCY_XPB,
                     alice, bob, blk_1);
    send_list2.push_back(tx_send);
  }
  MAKE_NEXT_BLOCK_TX_LIST(events, blk_2, blk_1, miner_account, send_list2);
  GRADE_CONTRACT_TX(events, tx_gradeok, 0xbe7, 0, grading_key.sec);
  GRADE_CONTRACT_TX(events, tx_gradeok2, 0xbe72, GRADE_SCALE_MAX / 2, grading_key.sec);
  MAKE_NEXT_BLOCK_TX1(events, blk_3pre, blk_2, miner_account, tx_gradeok);
  MAKE_NEXT_BLOCK_TX1(events, blk_3, blk_3pre, miner_account, tx_gradeok2);
  
  // txin of the resolve must be (backed_by_currency, NotContract, BACKED_BY_N_A)
  DO_CALLBACK(events, "mark_invalid_tx");
  MAKE_TX_FULL(events, tx_bad1, alice, carol, 1000, 0, blk_3, CP_XPB, 0,
               [](transaction& tx) {
                 tx_tester(tx).vin_coin_types[1].currency += 1;                 
               });
  DO_CALLBACK(events, "mark_invalid_tx");
  MAKE_TX_FULL(events, tx_bad1b, alice, carol, 1000, 0, blk_3, CP_XPB, 0,
               [](transaction& tx) {
                 tx_tester(tx).vin_coin_types[1].contract_type = BackingCoin;
               });
  DO_CALLBACK(events, "mark_invalid_tx");
  MAKE_TX_FULL(events, tx_bad1c, alice, carol, 1000, 0, blk_3, CP_XPB, 0,
               [](transaction& tx) {
                 tx_tester(tx).vin_coin_types[1].backed_by_currency += 1;
               });

  // must have coins of the desired resolution exist
  DO_CALLBACK(events, "mark_invalid_tx");
  MAKE_TX_FULL(events, tx_bad1d, alice, carol, 1000, 0, blk_3, CP_XPB, 0,
               [](transaction& tx) {
                 boost::get<txin_resolve_bc_coins>(tx_tester(tx).vin[1]).backing_currency += 1;
                 tx_tester(tx).vin_coin_types[1].currency += 1;
                 tx_tester(tx).vout_coin_types[0].currency += 1;
               });

  // resolving contract w/ valid id
  DO_CALLBACK(events, "mark_invalid_tx");
  MAKE_TX_FULL(events, tx_bad2, alice, carol, 1000, 0, blk_3, CP_XPB, 0,
               [](transaction& tx) {
                 auto& inp = boost::get<txin_resolve_bc_coins>(tx_tester(tx).vin[1]);
                 inp.contract = 100;
               });
  DO_CALLBACK(events, "mark_invalid_tx");
  MAKE_TX_FULL(events, tx_bad3, alice, carol, 1000, 0, blk_3, CP_XPB, 0,
               [](transaction& tx) {
                 auto& inp = boost::get<txin_resolve_bc_coins>(tx_tester(tx).vin[1]);
                 inp.contract = CURRENCY_XPB;
               });
  
  // resolving non-existent contract
  DO_CALLBACK(events, "mark_invalid_tx");
  MAKE_TX_FULL(events, tx_bad4, alice, carol, 1000, 0, blk_3, CP_XPB, 0,
               [](transaction& tx) {
                 auto& inp = boost::get<txin_resolve_bc_coins>(tx_tester(tx).vin[1]);
                 inp.contract += 1;
               });
  // invalid is_backing_coins
  DO_CALLBACK(events, "mark_invalid_tx");
  MAKE_TX_FULL(events, tx_bad4b, alice, carol, 1000, 0, blk_3, CP_XPB, 0,
               [](transaction& tx) {
                 auto& inp = boost::get<txin_resolve_bc_coins>(tx_tester(tx).vin[1]);
                 inp.is_backing_coins = 2;
               });
  // wrong type of coin
  DO_CALLBACK(events, "mark_invalid_tx");
  MAKE_TX_FULL(events, tx_bad4c, alice, carol, 1000, 0, blk_3, CP_XPB, 0,
               [](transaction& tx) {
                 auto& inp = boost::get<txin_resolve_bc_coins>(tx_tester(tx).vin[1]);
                 inp.is_backing_coins = 1 - inp.is_backing_coins;
               });
  // resolving wrong but existing contract is handled in gen_contracts_resolve_backing_cant_change_contract
  // resolve with too-large amount
  DO_CALLBACK(events, "mark_invalid_tx");
  MAKE_TX_FULL(events, tx_bad7, alice, carol, 1000, 0, blk_3, CP_XPB, 0,
               [](transaction& tx) {
                 auto& inp = boost::get<txin_resolve_bc_coins>(tx_tester(tx).vin[1]);
                 inp.graded_amount = 10000;
               });
  // grading exact (correct) amount is checked in gen_contracts_grade_spend_backing_cant_overspend_misgrade
  // finally, can resolve properly
  transaction tx_ok, tx_double_spend;
  {
    std::vector<cryptonote::tx_source_entry> sources;
    std::vector<cryptonote::tx_destination_entry> destinations;
    if (!fill_tx_sources_and_destinations(events, blk_3, alice, carol, 1000, 0, 0, sources, destinations, CP_XPB))
      return false;
  
    cryptonote::keypair txkey, txkey2;
    if (!construct_tx(alice.get_keys(), sources, destinations, std::vector<uint8_t>(), tx_ok, 0, txkey, tools::identity()))
      return false;
    if (!construct_tx(alice.get_keys(), sources, destinations, std::vector<uint8_t>(), tx_double_spend, 0, txkey2, tools::identity()))
      return false;
  }
  events.push_back(tx_ok);

  // can't double-spend in mempool
  DO_CALLBACK(events, "mark_invalid_tx");
  events.push_back(tx_double_spend);
  
  MAKE_NEXT_BLOCK_TX1(events, blk_4, blk_3, miner_account, tx_ok);
  
  // or in block
  DO_CALLBACK(events, "mark_invalid_tx");
  events.push_back(tx_double_spend);
  
  // expiration grading is checked in _resolve_*_expired
  
  return true;
}

bool gen_contracts_resolve_contract_checks::generate(std::vector<test_event_entry>& events) const
{
  // same as above, except alice has the other side
  // send 3000 of contract graded at 1/3 to distinguish it a bit

  const size_t num_sends = 2;
  
  INIT_CONTRACT_TEST();
  
  MAKE_TX_LIST_START(events, txs, miner_account, bob, 3000, blk_0r);
  CREATE_CONTRACT_TX(events, 0xbe7, "Is coin heads or tails", tx_contract, grading_key.pub);
  CREATE_CONTRACT_TX(events, 0xbe72, "Is second coin heads or tails", tx_contract2, grading_key.pub);
  txs.push_back(tx_contract);
  txs.push_back(tx_contract2);
  MAKE_NEXT_BLOCK_TX_LIST(events, blk_1pre, blk_0r, miner_account, txs);
  
  // send enough money to queue all these up
  REWIND_BLOCKS_N(events, blk_1almost, blk_1pre, miner_account, 10);
  std::list<transaction> send_list;
  for (size_t i=0; i < num_sends; i++) {
    MAKE_TX(events, txsend, miner_account, bob, 3000, blk_1almost);
    send_list.push_back(txsend);
  }
  MAKE_NEXT_BLOCK_TX_LIST(events, blk_1, blk_1almost, miner_account, send_list);
  
  std::list<transaction> send_list2;
  for (size_t i=0; i < num_sends; i++) {
    SEND_CONTRACT_TX(events, tx_send, 0xbe7,
                     bob, 3000, CURRENCY_XPB,
                     bob, alice, blk_1);
    send_list2.push_back(tx_send);
  }
  MAKE_NEXT_BLOCK_TX_LIST(events, blk_2, blk_1, miner_account, send_list2);
  GRADE_CONTRACT_TX(events, tx_gradeok, 0xbe7, GRADE_SCALE_MAX / 3 + 1, grading_key.sec);
  GRADE_CONTRACT_TX(events, tx_gradeok2, 0xbe72, GRADE_SCALE_MAX / 2, grading_key.sec);
  MAKE_NEXT_BLOCK_TX1(events, blk_3pre, blk_2, miner_account, tx_gradeok);
  MAKE_NEXT_BLOCK_TX1(events, blk_3, blk_3pre, miner_account, tx_gradeok2);
  
  // txin of the resolve must be (backed_by_currency, NotContract, BACKED_BY_N_A)
  DO_CALLBACK(events, "mark_invalid_tx");
  MAKE_TX_FULL(events, tx_bad1, alice, carol, 1000, 0, blk_3, CP_XPB, 0,
               [](transaction& tx) {
                 tx_tester(tx).vin_coin_types[1].currency += 1;
               });
  DO_CALLBACK(events, "mark_invalid_tx");
  MAKE_TX_FULL(events, tx_bad1b, alice, carol, 1000, 0, blk_3, CP_XPB, 0,
               [](transaction& tx) {
                 tx_tester(tx).vin_coin_types[1].contract_type = BackingCoin;
               });
  DO_CALLBACK(events, "mark_invalid_tx");
  MAKE_TX_FULL(events, tx_bad1c, alice, carol, 1000, 0, blk_3, CP_XPB, 0,
               [](transaction& tx) {
                 tx_tester(tx).vin_coin_types[1].backed_by_currency += 1;
               });
  
  // must have coins of the desired resolution exist
  DO_CALLBACK(events, "mark_invalid_tx");
  MAKE_TX_FULL(events, tx_bad1d, alice, carol, 1000, 0, blk_3, CP_XPB, 0,
               [](transaction& tx) {
                 boost::get<txin_resolve_bc_coins>(tx_tester(tx).vin[1]).backing_currency += 1;
                 tx_tester(tx).vin_coin_types[1].currency += 1;
                 tx_tester(tx).vout_coin_types[0].currency += 1;
               });
  
  // resolving contract w/ valid id
  DO_CALLBACK(events, "mark_invalid_tx");
  MAKE_TX_FULL(events, tx_bad2, alice, carol, 1000, 0, blk_3, CP_XPB, 0,
               [](transaction& tx) {
                 auto& inp = boost::get<txin_resolve_bc_coins>(tx_tester(tx).vin[1]);
                 inp.contract = 100;
               });
  DO_CALLBACK(events, "mark_invalid_tx");
  MAKE_TX_FULL(events, tx_bad3, alice, carol, 1000, 0, blk_3, CP_XPB, 0,
               [](transaction& tx) {
                 auto& inp = boost::get<txin_resolve_bc_coins>(tx_tester(tx).vin[1]);
                 inp.contract = CURRENCY_XPB;
               });
  
  // resolving non-existent contract
  DO_CALLBACK(events, "mark_invalid_tx");
  MAKE_TX_FULL(events, tx_bad4, alice, carol, 1000, 0, blk_3, CP_XPB, 0,
               [](transaction& tx) {
                 auto& inp = boost::get<txin_resolve_bc_coins>(tx_tester(tx).vin[1]);
                 inp.contract += 1;
               });
  // invalid is_backing_coins
  DO_CALLBACK(events, "mark_invalid_tx");
  MAKE_TX_FULL(events, tx_bad4b, alice, carol, 1000, 0, blk_3, CP_XPB, 0,
               [](transaction& tx) {
                 auto& inp = boost::get<txin_resolve_bc_coins>(tx_tester(tx).vin[1]);
                 inp.is_backing_coins = 2;
               });
  // wrong type of coin
  DO_CALLBACK(events, "mark_invalid_tx");
  MAKE_TX_FULL(events, tx_bad4c, alice, carol, 1000, 0, blk_3, CP_XPB, 0,
               [](transaction& tx) {
                 auto& inp = boost::get<txin_resolve_bc_coins>(tx_tester(tx).vin[1]);
                 inp.is_backing_coins = 1 - inp.is_backing_coins;
               });
  // resolving wrong but existing contract is handled in gen_contracts_resolve_backing_cant_change_contract
  // resolve with too-large amount
  DO_CALLBACK(events, "mark_invalid_tx");
  MAKE_TX_FULL(events, tx_bad7, alice, carol, 1000, 0, blk_3, CP_XPB, 0,
               [](transaction& tx) {
                 auto& inp = boost::get<txin_resolve_bc_coins>(tx_tester(tx).vin[1]);
                 inp.graded_amount = 10000;
               });
  // resolve with a correct amount of zero is invalid
  // '2000' triggers two pairs of outputs, we invalidate the first pair
  DO_CALLBACK(events, "mark_invalid_tx");
  MAKE_TX_FULL(events, tx_bad7b, alice, carol, 2000, 0, blk_3, CP_XPB, 0,
               [](transaction& tx) {
                 auto& inp = boost::get<txin_resolve_bc_coins>(tx_tester(tx).vin[1]);
                 inp.source_amount = 2;
                 inp.graded_amount = 0;
                 tx_tester(tx).vout[0].amount = 1000;
               });
  // grading exact (correct) amount is checked in gen_contracts_grade_spend_backing_cant_overspend_misgrade
  // finally, can resolve properly
  transaction tx_ok, tx_double_spend;
  {
    std::vector<cryptonote::tx_source_entry> sources;
    std::vector<cryptonote::tx_destination_entry> destinations;
    if (!fill_tx_sources_and_destinations(events, blk_3, alice, carol, 1000, 0, 0, sources, destinations, CP_XPB))
      return false;
    
    cryptonote::keypair txkey, txkey2;
    if (!construct_tx(alice.get_keys(), sources, destinations, std::vector<uint8_t>(), tx_ok, 0, txkey, tools::identity()))
      return false;
    if (!construct_tx(alice.get_keys(), sources, destinations, std::vector<uint8_t>(), tx_double_spend, 0, txkey2, tools::identity()))
      return false;
  }
  events.push_back(tx_ok);
  
  // can't double-spend in mempool
  DO_CALLBACK(events, "mark_invalid_tx");
  events.push_back(tx_double_spend);
  
  MAKE_NEXT_BLOCK_TX1(events, blk_4, blk_3, miner_account, tx_ok);
  
  // or in block
  DO_CALLBACK(events, "mark_invalid_tx");
  events.push_back(tx_double_spend);
  
  // expiration grading is checked in _resolve_*_expired
  
  return true;
}

bool gen_contracts_create_mint_fuse::generate(std::vector<test_event_entry>& events) const
{
  INIT_CONTRACT_TEST();
  
  MAKE_TX_LIST_START(events, txs, miner_account, alice, 10, blk_0r);
  CREATE_CONTRACT_TX(events, 0xbe7, "Is coin heads or tails", tx_contract, grading_key.pub);
  txs.push_back(tx_contract);
  MAKE_NEXT_BLOCK_TX_LIST(events, blk_1, blk_0r, miner_account, txs);
  
  SEND_CONTRACT_TX(events, tx_send_contract, 0xbe7,
                   alice, 10, CURRENCY_XPB,
                   alice, bob, blk_1);
  MAKE_NEXT_BLOCK_TX1(events, blk_2, blk_1, miner_account, tx_send_contract);
  // alice sends 7 backing coins to bob
  MAKE_TX_CP(events, tx_fwd, alice, bob, 7, blk_2, coin_type(0xbe7, BackingCoin, CURRENCY_XPB));
  MAKE_NEXT_BLOCK_TX1(events, blk_3, blk_2, miner_account, tx_fwd);
  
  // bob fuses 7 coins
  FUSE_CONTRACT_TX(events, tx_fuse, 0xbe7, CURRENCY_XPB, 7, bob, bob, dave, blk_3);
  MAKE_NEXT_BLOCK_TX1(events, blk_4, blk_3, miner_account, tx_fuse);
  // send the fused coins in a no-fee tx
  MAKE_TX_MIX_CP_FEE(events, txspendfused, dave, emily, 7, 0, blk_4, CP_XPB, 0);
  MAKE_NEXT_BLOCK_TX1(events, blk_5, blk_4, miner_account, txspendfused);
  
  // fuse remaining 3 from separate accounts
  FUSE_CONTRACT_TX(events, tx_fuse2, 0xbe7, CURRENCY_XPB, 3, alice, bob, emily, blk_5);
  MAKE_NEXT_BLOCK_TX1(events, blk_6, blk_5, miner_account, tx_fuse2);
  MAKE_TX_MIX_CP_FEE(events, txspendfused2, emily, dave, 3, 0, blk_5, CP_XPB, 0);
  MAKE_NEXT_BLOCK_TX1(events, blk_7, blk_6, miner_account, txspendfused2);
  
  return true;
}

bool gen_contracts_create_mint_fuse_fee::generate(std::vector<test_event_entry>& events) const
{
  INIT_CONTRACT_TEST();
  
  MAKE_TX_LIST_START(events, txs, miner_account, alice, 100, blk_0r);
  CREATE_CONTRACT_TX_FULL(events, 0xbe7, "Is coin heads or tails", tx_contract, grading_key.pub,
                          GRADE_SCALE_MAX / 10, 10000, 0, tools::identity());
  txs.push_back(tx_contract);
  MAKE_NEXT_BLOCK_TX_LIST(events, blk_1, blk_0r, miner_account, txs);
  
  // alice sends 100 to bob
  SEND_CONTRACT_TX(events, tx_send_contract, 0xbe7,
                   alice, 100, CURRENCY_XPB,
                   alice, bob, blk_1);
  MAKE_NEXT_BLOCK_TX1(events, blk_2, blk_1, miner_account, tx_send_contract);
  
  // if graded now, fee claimed is 10 (not added to block)
  GRADE_CONTRACT_TX_FULL(events, tx_grade, 0xbe7, 0, grading_key.sec, carol, 10, CURRENCY_XPB, tools::identity());
  
  // bob & alice fuse 50 coins
  FUSE_CONTRACT_TX(events, tx_fuse, 0xbe7, CURRENCY_XPB, 50, alice, bob, dave, blk_2);
  MAKE_NEXT_BLOCK_TX1(events, blk_3, blk_2, miner_account, tx_fuse);
  
  // if graded now, fee claimed is 5
  DO_CALLBACK(events, "mark_invalid_block");
  MAKE_NEXT_BLOCK_TX1(events, blk_4_bad, blk_3, miner_account, tx_grade);
  GRADE_CONTRACT_TX_FULL(events, tx_grade_2, 0xbe7, 0, grading_key.sec, carol, 5, CURRENCY_XPB, tools::identity());
  MAKE_NEXT_BLOCK_TX1(events, blk_4, blk_3, miner_account, tx_grade_2);

  return true;
}

bool gen_contracts_create_mint_fuse_checks::generate(std::vector<test_event_entry>& events) const
{
  INIT_CONTRACT_TEST();
  
  MAKE_TX_LIST_START(events, txs, miner_account, alice, 1000, blk_0r);
  CREATE_CONTRACT_TX(events, 0xbe7, "Is coin heads or tails", tx_contract, grading_key.pub);
  txs.push_back(tx_contract);
  MAKE_NEXT_BLOCK_TX_LIST(events, blk_1, blk_0r, miner_account, txs);
  
  SEND_CONTRACT_TX(events, tx_send_contract, 0xbe7,
                   alice, 1000, CURRENCY_XPB,
                   alice, bob, blk_1);
  MAKE_NEXT_BLOCK_TX1(events, blk_2, blk_1, miner_account, tx_send_contract);
  
  // in_cp of the fuse tx is wrong
  DO_CALLBACK(events, "mark_invalid_tx");
  FUSE_CONTRACT_TX_FULL(events, tx_bad1, 0xbe7, CURRENCY_XPB, 1000, alice, bob, emily, blk_2,
                        [](transaction& tx) {
                          tx_tester(tx).vin_coin_types[2].currency += 1;
                        });
  DO_CALLBACK(events, "mark_invalid_tx");
  FUSE_CONTRACT_TX_FULL(events, tx_bad2, 0xbe7, CURRENCY_XPB, 1000, alice, bob, emily, blk_2,
                        [](transaction& tx) {
                          tx_tester(tx).vin_coin_types[2].contract_type = BackingCoin;
                        });
  DO_CALLBACK(events, "mark_invalid_tx");
  FUSE_CONTRACT_TX_FULL(events, tx_bad3, 0xbe7, CURRENCY_XPB, 1000, alice, bob, emily, blk_2,
                        [](transaction& tx) {
                          tx_tester(tx).vin_coin_types[2].backed_by_currency += 1;
                        });
  // invalid contract
  DO_CALLBACK(events, "mark_invalid_tx");
  FUSE_CONTRACT_TX_FULL(events, tx_bad4, 0xbe7, CURRENCY_XPB, 1000, alice, bob, emily, blk_2,
                        [](transaction& tx) {
                          auto& inp_fuse = boost::get<txin_fuse_bc_coins>(tx_tester(tx).vin[2]);
                          inp_fuse.contract = 0xbefefe;
                          // adjust to pass input/output checks
                          tx_tester(tx).vin_coin_types[0].currency = 0xbefefe;
                          tx_tester(tx).vin_coin_types[1].currency = 0xbefefe;
                        });
  // invalid backing currency
  DO_CALLBACK(events, "mark_invalid_tx");
  FUSE_CONTRACT_TX_FULL(events, tx_bad5, 0xbe7, CURRENCY_XPB, 1000, alice, bob, emily, blk_2,
                        [](transaction& tx) {
                          auto& inp_fuse = boost::get<txin_fuse_bc_coins>(tx_tester(tx).vin[2]);
                          inp_fuse.backing_currency = 0xbefefe;
                          // adjust to pass input/output checks
                          tx_tester(tx).vin_coin_types[0].backed_by_currency = 0xbefefe;
                          tx_tester(tx).vin_coin_types[1].backed_by_currency = 0xbefefe;
                          tx_tester(tx).vout_coin_types[0].currency = 0xbefefe;
                        });
  // invalid amounts 1
  DO_CALLBACK(events, "mark_invalid_tx");
  FUSE_CONTRACT_TX_FULL(events, tx_bad6, 0xbe7, CURRENCY_XPB, 1000, alice, bob, emily, blk_2,
                        [](transaction& tx) {
                          auto& inp_backing = boost::get<txin_to_key>(tx_tester(tx).vin[0]);
                          auto& inp_contract = boost::get<txin_to_key>(tx_tester(tx).vin[1]);
                          auto& inp_fuse = boost::get<txin_fuse_bc_coins>(tx_tester(tx).vin[2]);
                          inp_backing.amount = 1500;
                          inp_contract.amount = 1500;
                          inp_fuse.amount = 1500;
                          tx_tester(tx).vout[0].amount = 1500;
                        });
  // invalid amounts 2
  DO_CALLBACK(events, "mark_invalid_tx");
  FUSE_CONTRACT_TX_FULL(events, tx_bad7, 0xbe7, CURRENCY_XPB, 1000, alice, bob, emily, blk_2,
                        [](transaction& tx) {
                          auto& inp_fuse = boost::get<txin_fuse_bc_coins>(tx_tester(tx).vin[2]);
                          inp_fuse.amount = 1500;
                          tx_tester(tx).vout[0].amount = 1500;
                        });
  // invalid amounts 3
  DO_CALLBACK(events, "mark_invalid_tx");
  FUSE_CONTRACT_TX_FULL(events, tx_bad8, 0xbe7, CURRENCY_XPB, 1000, alice, bob, emily, blk_2,
                        [](transaction& tx) {
                          auto& inp_fuse = boost::get<txin_fuse_bc_coins>(tx_tester(tx).vin[2]);
                          inp_fuse.amount = 1500;
                        });
  
  // can't fuse graded contract
  GRADE_CONTRACT_TX(events, tx_grade, 0xbe7, 0, grading_key.sec);
  MAKE_NEXT_BLOCK_TX1(events, blk_3, blk_2, miner_account, tx_grade);
  auto height = get_block_height(boost::get<block>(events.back()));
  DO_CALLBACK(events, "mark_invalid_tx");
  FUSE_CONTRACT_TX(events, tx_bad9, 0xbe7, CURRENCY_XPB, 0100, alice, bob, emily, blk_3);
  
  // can't fuse expired contract
  MAKE_TX_LIST_START(events, txs2, miner_account, alice, 1000, blk_0r);
  CREATE_CONTRACT_TX_FULL(events, 0xbe72, "Is coin heads or tails2", tx_contract2, grading_key.pub,
                          GRADE_SCALE_MAX / 10, height + 5, 0, tools::identity());
  txs2.push_back(tx_contract2);
  MAKE_NEXT_BLOCK_TX_LIST(events, blk_4, blk_3, miner_account, txs2);
  SEND_CONTRACT_TX(events, tx_send_contract2, 0xbe72,
                   alice, 1000, CURRENCY_XPB,
                   alice, bob, blk_4);
  MAKE_NEXT_BLOCK_TX1(events, blk_5, blk_4, miner_account, tx_send_contract2);
  REWIND_BLOCKS_N(events, blk_5r, blk_5, miner_account, 10)
  DO_CALLBACK(events, "mark_invalid_tx");
  FUSE_CONTRACT_TX(events, tx_bad10, 0xbe72, CURRENCY_XPB, 1000, alice, bob, emily, blk_5r);
  
  return true;
}
*/