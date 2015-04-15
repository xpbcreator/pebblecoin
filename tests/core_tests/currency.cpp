// Copyright (c) 2014 The Pebblecoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.
/*
#include "currency.h"

#include "cryptonote_core/tx_tester.h"

using namespace epee;
using namespace crypto;
using namespace cryptonote;

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

bool gen_currency_mint::generate(std::vector<test_event_entry>& events) const
{
  uint64_t ts_start = 1338224400;
  
  GENERATE_ACCOUNT(miner_account);
  GENERATE_ACCOUNT(alice);
  MAKE_STARTING_BLOCKS(events, blk_0, miner_account, ts_start);
  
  MAKE_MINT_TX(events, 0xbeef, "Beef futures", 1000, alice, txmint, 2, null_pkey);
  
  MAKE_NEXT_BLOCK_TX1(events, blk_1, blk_0, miner_account, txmint);
  
  MAKE_NEXT_BLOCK(events, blk_2, blk_1, miner_account);
  
  return true;
}

bool gen_currency_mint_many::generate(std::vector<test_event_entry>& events) const
{
  uint64_t ts_start = 1338224400;
  
  GENERATE_ACCOUNT(miner_account);
  GENERATE_ACCOUNT(alice);
  MAKE_STARTING_BLOCKS(events, blk_0, miner_account, ts_start);
  
  MAKE_MINT_TX(events, 0xbeef, "Beef futures", 1000, alice, txmint1, 2, null_pkey);
  MAKE_MINT_TX(events, 0xfeed, "Tomatoes", 1000, alice, txmint2, 2, null_pkey);
  MAKE_MINT_TX(events, 0x3ff, "", 1000, alice, txmint3, 2, null_pkey);
  MAKE_MINT_TX(events, 0x4ff, "", 1000, alice, txmint4, 2, null_pkey);
  MAKE_MINT_TX(events, 256, "", 1000, alice, txmint5, 2, null_pkey);
  
  std::list<cryptonote::transaction> tx_list;
  tx_list.push_back(txmint1);
  tx_list.push_back(txmint2);
  tx_list.push_back(txmint3);
  tx_list.push_back(txmint4);
  tx_list.push_back(txmint5);
  MAKE_NEXT_BLOCK_TX_LIST(events, blk_1, blk_0, miner_account, tx_list);
  
  MAKE_NEXT_BLOCK(events, blk_2, blk_1, miner_account);
  
  return true;
}

bool gen_currency_invalid_amount_0::generate(std::vector<test_event_entry>& events) const
{
  uint64_t ts_start = 1338224400;
  
  GENERATE_ACCOUNT(miner_account);
  GENERATE_ACCOUNT(alice);
  MAKE_STARTING_BLOCKS(events, blk_0, miner_account, ts_start);
  
  DO_CALLBACK(events, "mark_invalid_tx");
  MAKE_MINT_TX_FULL(events, 0xbeef, "Beef futures", 1, alice, txmint, 2, null_pkey,
                    [](transaction& tx) {
                      boost::get<txin_mint>(tx_tester(tx).vin[0]).amount = 0;
                      tx.clear_outs();
                    });
  
  return true;
}

bool gen_currency_invalid_currency_0::generate(std::vector<test_event_entry>& events) const
{
  uint64_t ts_start = 1338224400;
  
  GENERATE_ACCOUNT(miner_account);
  GENERATE_ACCOUNT(alice);
  MAKE_STARTING_BLOCKS(events, blk_0, miner_account, ts_start);

  DO_CALLBACK(events, "mark_invalid_tx");
  MAKE_MINT_TX(events, 0, "Beef futures", 1000, alice, txmint, 2, null_pkey);
  
  return true;
}

bool gen_currency_invalid_currency_70::generate(std::vector<test_event_entry>& events) const
{
  uint64_t ts_start = 1338224400;
  
  GENERATE_ACCOUNT(miner_account);
  GENERATE_ACCOUNT(alice);
  MAKE_STARTING_BLOCKS(events, blk_0, miner_account, ts_start);
  
  DO_CALLBACK(events, "mark_invalid_tx");
  MAKE_MINT_TX(events, 70, "Beef futures", 1000, alice, txmint, 2, null_pkey);
  
  return true;
}

bool gen_currency_invalid_currency_255::generate(std::vector<test_event_entry>& events) const
{
  uint64_t ts_start = 1338224400;
  
  GENERATE_ACCOUNT(miner_account);
  GENERATE_ACCOUNT(alice);
  MAKE_STARTING_BLOCKS(events, blk_0, miner_account, ts_start);
  
  DO_CALLBACK(events, "mark_invalid_tx");
  MAKE_MINT_TX(events, 255, "Beef futures", 1000, alice, txmint, 2, null_pkey);
  
  return true;
}

bool gen_currency_invalid_large_description::generate(std::vector<test_event_entry>& events) const
{
  uint64_t ts_start = 1338224400;
  
  GENERATE_ACCOUNT(miner_account);
  GENERATE_ACCOUNT(alice);
  MAKE_STARTING_BLOCKS(events, blk_0, miner_account, ts_start);
  
  DO_CALLBACK(events, "mark_invalid_tx");
  MAKE_MINT_TX(events, 0, std::string("Lawlawlawl", 100), 1000, alice, txmint, 2, null_pkey);
  
  return true;
}

bool gen_currency_invalid_reuse_currency_1::generate(std::vector<test_event_entry>& events) const
{
  uint64_t ts_start = 1338224400;
  
  GENERATE_ACCOUNT(miner_account);
  GENERATE_ACCOUNT(alice);
  MAKE_STARTING_BLOCKS(events, blk_0, miner_account, ts_start);
  
  MAKE_MINT_TX(events, 0xbeef, "Beef futures", 1000, alice, txmint, 2, null_pkey);
  MAKE_NEXT_BLOCK_TX1(events, blk_1, blk_0, miner_account, txmint);
  
  DO_CALLBACK(events, "mark_invalid_tx");
  MAKE_MINT_TX(events, 0xbeef, "Beef pasts", 1000, alice, txmint2, 2, null_pkey);
  
  return true;
}

bool gen_currency_invalid_reuse_currency_2::generate(std::vector<test_event_entry>& events) const
{
  uint64_t ts_start = 1338224400;
  
  GENERATE_ACCOUNT(miner_account);
  GENERATE_ACCOUNT(alice);
  MAKE_STARTING_BLOCKS(events, blk_0, miner_account, ts_start);
  
  MAKE_MINT_TX(events, 0xbeef, "Beef futures", 1000, alice, txmint, 2, null_pkey);
  MAKE_MINT_TX(events, 0xbeef, "Beef pasts", 1000, alice, txmint2, 2, null_pkey);
  
  std::list<transaction> txs;
  txs.push_back(txmint);
  txs.push_back(txmint2);
  DO_CALLBACK(events, "mark_invalid_block");
  MAKE_NEXT_BLOCK_TX_LIST(events, blk_1, blk_0, miner_account, txs);
  
  return true;
}

bool gen_currency_invalid_reuse_description_1::generate(std::vector<test_event_entry>& events) const
{
  uint64_t ts_start = 1338224400;
  
  GENERATE_ACCOUNT(miner_account);
  GENERATE_ACCOUNT(alice);
  MAKE_STARTING_BLOCKS(events, blk_0, miner_account, ts_start);
  
  MAKE_MINT_TX(events, 0xbeef, "Beef futures", 1000, alice, txmint, 2, null_pkey);
  MAKE_NEXT_BLOCK_TX1(events, blk_1, blk_0, miner_account, txmint);
  
  DO_CALLBACK(events, "mark_invalid_tx");
  MAKE_MINT_TX(events, 0xfeed, "Beef futures", 1000, alice, txmint2, 2, null_pkey);
  
  return true;
}

bool gen_currency_invalid_reuse_description_2::generate(std::vector<test_event_entry>& events) const
{
  uint64_t ts_start = 1338224400;
  
  GENERATE_ACCOUNT(miner_account);
  GENERATE_ACCOUNT(alice);
  MAKE_STARTING_BLOCKS(events, blk_0, miner_account, ts_start);
  
  MAKE_MINT_TX(events, 0xbeef, "Beef futures", 1000, alice, txmint, 2, null_pkey);
  MAKE_MINT_TX(events, 0xfeed, "Beef futures", 1000, alice, txmint2, 2, null_pkey);
  
  std::list<transaction> txs;
  txs.push_back(txmint);
  txs.push_back(txmint2);
  DO_CALLBACK(events, "mark_invalid_block");
  MAKE_NEXT_BLOCK_TX_LIST(events, blk_1, blk_0, miner_account, txs);
  
  return true;
}

bool gen_currency_invalid_remint_key::generate(std::vector<test_event_entry>& events) const
{
  uint64_t ts_start = 1338224400;
  
  GENERATE_ACCOUNT(miner_account);
  GENERATE_ACCOUNT(alice);
  MAKE_STARTING_BLOCKS(events, blk_0, miner_account, ts_start);
  
  DO_CALLBACK(events, "mark_invalid_tx");
  MAKE_MINT_TX(events, 0xbad, "Beef futures", 1000, alice, txmint, 2, generate_invalid_pub_key());
  
  return true;
}

bool gen_currency_invalid_spend_more_than_mint::generate(std::vector<test_event_entry>& events) const
{
  uint64_t ts_start = 1338224400;
  
  GENERATE_ACCOUNT(miner_account);
  GENERATE_ACCOUNT(alice);
  MAKE_STARTING_BLOCKS(events, blk_0, miner_account, ts_start);
  
  DO_CALLBACK(events, "mark_invalid_tx");
  cryptonote::transaction txmint;
  {
    tx_builder txb;
    txb.init();
    std::vector<tx_destination_entry> destinations;
    destinations.push_back(tx_destination_entry(coin_type(0xbeef, NotContract, BACKED_BY_N_A),
                                                2000, alice.get_keys().m_account_address));
    txb.add_mint(0xbeef, "Beef futures", 2000, 2, null_pkey, destinations);
    txb.finalize();
    CHECK_AND_ASSERT_MES(txb.get_finalized_tx(txmint), false, "Unable to get finalized mint tx");
    auto& in_mint = boost::get<txin_mint&>(tx_tester(txmint).vin.back());
    in_mint.amount -= 1;
    events.push_back(txmint);
  }
  
  return true;
}

bool gen_currency_ok_spend_less_than_mint::generate(std::vector<test_event_entry>& events) const
{
  uint64_t ts_start = 1338224400;
  
  GENERATE_ACCOUNT(miner_account);
  GENERATE_ACCOUNT(alice);
  MAKE_STARTING_BLOCKS(events, blk_0, miner_account, ts_start);
  
  cryptonote::transaction txmint;
  {
    tx_builder txb;
    txb.init();
    std::vector<tx_destination_entry> destinations;
    destinations.push_back(tx_destination_entry(coin_type(0xbeef, NotContract, BACKED_BY_N_A),
                                                500, alice.get_keys().m_account_address));
    txb.add_mint(0xbeef, "Beef futures", 1000, 2, null_pkey, destinations);
    txb.finalize();
    CHECK_AND_ASSERT_MES(txb.get_finalized_tx(txmint), false, "Unable to get finalized mint tx");
    events.push_back(txmint);
  }
  
  MAKE_NEXT_BLOCK_TX1(events, blk_1, blk_0, miner_account, txmint);
  
  return true;
}

bool gen_currency_spend_currency::generate(std::vector<test_event_entry>& events) const
{
  uint64_t ts_start = 1338224400;
  
  GENERATE_ACCOUNT(miner_account);
  GENERATE_ACCOUNT(alice);
  GENERATE_ACCOUNT(bob);
  GENERATE_ACCOUNT(carol);
  MAKE_STARTING_BLOCKS(events, blk_0, miner_account, ts_start);
  
  MAKE_MINT_TX(events, 0xbeef, "Beef futures", 9999, alice, txmint, 2, null_pkey);
  MAKE_NEXT_BLOCK_TX1(events, blk_1, blk_0, miner_account, txmint);

  MAKE_TX_C(events, txbeef1, alice, bob, 1000, blk_1, 0xbeef);
  MAKE_NEXT_BLOCK_TX1(events, blk_2, blk_1, miner_account, txbeef1);
  
  MAKE_TX_C(events, txbeef2, bob, carol, 500, blk_2, 0xbeef);
  MAKE_NEXT_BLOCK_TX1(events, blk_3, blk_2, miner_account, txbeef2);

  MAKE_TX_C(events, txbeef3, carol, alice, 500, blk_3, 0xbeef);
  MAKE_NEXT_BLOCK_TX1(events, blk_4, blk_3, miner_account, txbeef3);
  
  MAKE_TX_C(events, txbeef4, alice, bob, 9499, blk_4, 0xbeef);
  MAKE_NEXT_BLOCK_TX1(events, blk_5, blk_4, miner_account, txbeef4);
  
  return true;
}

bool gen_currency_cant_spend_other_currency::generate(std::vector<test_event_entry>& events) const
{
  uint64_t ts_start = 1338224400;
  
  GENERATE_ACCOUNT(miner_account);
  GENERATE_ACCOUNT(alice);
  GENERATE_ACCOUNT(bob);

  MAKE_GENESIS_BLOCK(events, blk_a, miner_account, ts_start);
  MAKE_NEXT_BLOCK(events, blk_b, blk_a, miner_account);
  MAKE_NEXT_BLOCK(events, blk_c, blk_b, miner_account);
  REWIND_BLOCKS(events, blk_d, blk_c, miner_account);
  
  MAKE_NEXT_BLOCK(events, blk_0b, blk_d, miner_account);
  
  MAKE_MINT_TX(events, 0xbeef1, "Beef futures1", 9999, alice, txmint1, 2, null_pkey);
  MAKE_MINT_TX(events, 0xbeef2, "Beef futures2", 9999, alice, txmint2, 2, null_pkey);
  MAKE_MINT_TX(events, 0xbeef3, "Beef futures3", 9999, alice, txmint3, 2, null_pkey);
  MAKE_MINT_TX(events, 0xbeef4, "Beef futures4", 9999, bob, txmint4, 2, null_pkey);
  MAKE_MINT_TX(events, 0xbeef5, "Beef futures5", 9999, alice, txmint5, 2, null_pkey);
  MAKE_MINT_TX(events, 0xbeef6, "Beef futures6", 1, alice, txmint6, 2, null_pkey);
  
  std::list<cryptonote::transaction> tx_list;
  tx_list.push_back(txmint1);
  tx_list.push_back(txmint2);
  tx_list.push_back(txmint3);
  tx_list.push_back(txmint4);
  tx_list.push_back(txmint5);
  tx_list.push_back(txmint6);
  MAKE_NEXT_BLOCK_TX_LIST(events, blk_1, blk_0b, miner_account, tx_list);
  
  // mis-match vin/vout
  {
    DO_CALLBACK(events, "mark_invalid_tx");
    MAKE_TX_C(events, txbeef1, alice, bob, 1000, blk_1, 0xbeef1);
    test_event_entry& last = events.back();
    cryptonote::transaction& last_tx = boost::get<cryptonote::transaction>(last);
    tx_tester(last_tx).vin_coin_types[0].currency = 0xaaa;
  }

  {
    DO_CALLBACK(events, "mark_invalid_tx");
    MAKE_TX_C(events, txbeef1, alice, bob, 1000, blk_1, 0xbeef2);
    test_event_entry& last = events.back();
    cryptonote::transaction& last_tx = boost::get<cryptonote::transaction>(last);
    tx_tester(last_tx).vout_coin_types[0].currency = 0xaaa;
  }
  
  // wrong non-existent currency - shouldn't find outputs

  {
    DO_CALLBACK(events, "mark_invalid_tx");
    MAKE_TX_C(events, txbeef1, alice, bob, 1000, blk_1, 0xbeef3);
    test_event_entry& last = events.back();
    cryptonote::transaction& last_tx = boost::get<cryptonote::transaction>(last);
    tx_tester(last_tx).vin_coin_types[0].currency = 0xaaa;
    tx_tester(last_tx).vout_coin_types[0].currency = 0xaaa;
  }
  
  // wrong existing currency (alice has beef5, not beef4) - fails ring signature check
  
  {
    DO_CALLBACK(events, "mark_invalid_tx");
    MAKE_TX_C(events, txbeef1, alice, bob, 1000, blk_1, 0xbeef5);
    test_event_entry& last = events.back();
    cryptonote::transaction& last_tx = boost::get<cryptonote::transaction>(last);
    tx_tester(last_tx).vin_coin_types[0].currency = 0xbeef4;
    tx_tester(last_tx).vout_coin_types[0].currency = 0xbeef4;
  }
  
  // wrong existing currency (alice has beef6, not xpb) - fails ring signature check
  
  {
    DO_CALLBACK(events, "mark_invalid_tx");
    MAKE_TX_C(events, txbeef1, alice, bob, 1, blk_1, 0xbeef6);
    test_event_entry& last = events.back();
    cryptonote::transaction& last_tx = boost::get<cryptonote::transaction>(last);
    tx_tester(last_tx).vin_coin_types[0].currency = CURRENCY_XPB;
    tx_tester(last_tx).vout_coin_types[0].currency = CURRENCY_XPB;
  }
  
  return true;
}

bool gen_currency_spend_currency_mix::generate(std::vector<test_event_entry>& events) const
{
  uint64_t ts_start = 1338224400;
  
  GENERATE_ACCOUNT(miner_account);
  GENERATE_ACCOUNT(alice);
  GENERATE_ACCOUNT(bob);
  GENERATE_ACCOUNT(carol);
  GENERATE_ACCOUNT(dave);
  GENERATE_ACCOUNT(emma);
  MAKE_STARTING_BLOCKS(events, blk_0, miner_account, ts_start);
  
  MAKE_MINT_TX(events, 0xbeef, "Beef futures", 4000, alice, txmint, 2, null_pkey);
  MAKE_NEXT_BLOCK_TX1(events, blk_1, blk_0, miner_account, txmint);
  
  MAKE_TX_C(events, txbeef1, alice, bob, 1000, blk_1, 0xbeef);
  MAKE_NEXT_BLOCK_TX1(events, blk_2, blk_1, miner_account, txbeef1);
  MAKE_TX_C(events, txbeef2, alice, carol, 1000, blk_2, 0xbeef);
  MAKE_NEXT_BLOCK_TX1(events, blk_3, blk_2, miner_account, txbeef2);
  MAKE_TX_C(events, txbeef3, alice, dave, 1000, blk_3, 0xbeef);
  MAKE_NEXT_BLOCK_TX1(events, blk_4, blk_3, miner_account, txbeef3);
  MAKE_TX_C(events, txbeef4, alice, emma, 1000, blk_4, 0xbeef);
  MAKE_NEXT_BLOCK_TX1(events, blk_5, blk_4, miner_account, txbeef4);
  
  MAKE_TX_MIX_C(events, txmixbeef, emma, alice, 1000, 4, blk_5, 0xbeef);
  MAKE_NEXT_BLOCK_TX1(events, blk_6, blk_5, miner_account, txmixbeef);
  
  // Don't mix with other currencies
  
  MAKE_MINT_TX(events, 0xaaa, "AAA Service", 4000, alice, txmint2, 2, null_pkey);
  MAKE_NEXT_BLOCK_TX1(events, blk_7, blk_6, miner_account, txmint2);
  
  MAKE_TX_C(events, txaaa1, alice, bob, 1000, blk_7, 0xaaa);
  MAKE_NEXT_BLOCK_TX1(events, blk_8, blk_7, miner_account, txaaa1);
  MAKE_TX_C(events, txaaa2, alice, carol, 1000, blk_8, 0xaaa);
  MAKE_NEXT_BLOCK_TX1(events, blk_9, blk_8, miner_account, txaaa2);
  MAKE_TX_C(events, txaaa3, alice, dave, 1000, blk_9, 0xaaa);
  MAKE_NEXT_BLOCK_TX1(events, blk_10, blk_9, miner_account, txaaa3);
  MAKE_TX_C(events, txaaa4, alice, emma, 1000, blk_10, 0xaaa);
  MAKE_NEXT_BLOCK_TX1(events, blk_11, blk_10, miner_account, txaaa4);

  cryptonote::transaction txmixaaa;
  CHECK_AND_ASSERT_MES(!construct_tx_to_key(events, txmixaaa, blk_11, emma, alice, 1000, 0, 5, coin_type(0xaaa, NotContract, BACKED_BY_N_A)), false, "Should not have 5 sources to mix");

  return true;
}

bool gen_currency_remint_valid::generate(std::vector<test_event_entry>& events) const
{
  uint64_t ts_start = 1338224400;
  
  GENERATE_ACCOUNT(miner_account);
  GENERATE_ACCOUNT(alice);
  GENERATE_ACCOUNT(bob);
  
  MAKE_STARTING_BLOCKS(events, blk_0, miner_account, ts_start);
  
  keypair k = keypair::generate();
  
  MAKE_MINT_TX(events, 0xbeef, "Beef futures", 4000, alice, txmint, 2, k.pub);
  MAKE_NEXT_BLOCK_TX1(events, blk_1, blk_0, miner_account, txmint);
  MAKE_TX_C(events, txbeef1, alice, bob, 4000, blk_1, 0xbeef);
  MAKE_NEXT_BLOCK_TX1(events, blk_2, blk_1, miner_account, txbeef1);
  
  MAKE_REMINT_TX(events, 0xbeef, 5555, alice, txremint, k.sec, null_pkey);
  MAKE_NEXT_BLOCK_TX1(events, blk_3, blk_2, miner_account, txremint);
  MAKE_TX_C(events, txbeef2, alice, bob, 5555, blk_3, 0xbeef);
  MAKE_NEXT_BLOCK_TX1(events, blk_4, blk_3, miner_account, txbeef2);
  
  return true;
}

bool gen_currency_remint_invalid_amount_0::generate(std::vector<test_event_entry>& events) const
{
  uint64_t ts_start = 1338224400;
  
  GENERATE_ACCOUNT(miner_account);
  GENERATE_ACCOUNT(alice);
  GENERATE_ACCOUNT(bob);
  MAKE_STARTING_BLOCKS(events, blk_0, miner_account, ts_start);
  
  keypair k = keypair::generate();
  
  MAKE_MINT_TX(events, 0xbeef, "Beef futures", 4000, alice, txmint, 2, k.pub);
  MAKE_NEXT_BLOCK_TX1(events, blk_1, blk_0, miner_account, txmint);
  
  DO_CALLBACK(events, "mark_invalid_tx");
  MAKE_REMINT_TX_FULL(events, 0xbeef, 5555, alice, txremint, k.sec, null_pkey,
                      [k](transaction& tx) {
                        auto& inp = boost::get<txin_remint>(tx_tester(tx).vin[0]);
                        inp.amount = 0;
                        //re-sign it
                        crypto::hash prefix_hash = inp.get_prefix_hash();
                        crypto::public_key remint_pkey;
                        secret_key_to_public_key(k.sec, remint_pkey);
                        generate_signature(prefix_hash, remint_pkey, k.sec, inp.sig);
                        
                        tx.clear_outs();
                      });
  
  return true;
}

bool gen_currency_remint_invalid_unremintable::generate(std::vector<test_event_entry>& events) const
{
  uint64_t ts_start = 1338224400;
  
  GENERATE_ACCOUNT(miner_account);
  GENERATE_ACCOUNT(alice);
  GENERATE_ACCOUNT(bob);
  
  MAKE_STARTING_BLOCKS(events, blk_0, miner_account, ts_start);
  
  keypair k = keypair::generate();
  
  MAKE_MINT_TX(events, 0xbeef, "Beef futures", 4000, alice, txmint, 2, null_pkey);
  MAKE_NEXT_BLOCK_TX1(events, blk_1, blk_0, miner_account, txmint);
  MAKE_TX_C(events, txbeef1, alice, bob, 4000, blk_1, 0xbeef);
  MAKE_NEXT_BLOCK_TX1(events, blk_2, blk_1, miner_account, txbeef1);
  
  DO_CALLBACK(events, "mark_invalid_tx");
  MAKE_REMINT_TX(events, 0xbeef, 5555, alice, txremint, k.sec, k.pub);
  
  return true;
}

bool gen_currency_remint_invalid_currency::generate(std::vector<test_event_entry>& events) const
{
  uint64_t ts_start = 1338224400;
  
  GENERATE_ACCOUNT(miner_account);
  GENERATE_ACCOUNT(alice);
  GENERATE_ACCOUNT(bob);
  
  MAKE_STARTING_BLOCKS(events, blk_0, miner_account, ts_start);
  
  keypair k = keypair::generate();
  
  DO_CALLBACK(events, "mark_invalid_tx");
  MAKE_REMINT_TX(events, 0xbeef, 5555, alice, txremint, k.sec, null_pkey);
  
  return true;
}

bool gen_currency_remint_invalid_new_remint_key::generate(std::vector<test_event_entry>& events) const
{
  uint64_t ts_start = 1338224400;
  
  GENERATE_ACCOUNT(miner_account);
  GENERATE_ACCOUNT(alice);
  GENERATE_ACCOUNT(bob);
  
  MAKE_STARTING_BLOCKS(events, blk_0, miner_account, ts_start);
  
  keypair k = keypair::generate();
  
  MAKE_MINT_TX(events, 0xbeef, "Beef futures", 4000, alice, txmint, 2, k.pub);
  MAKE_NEXT_BLOCK_TX1(events, blk_1, blk_0, miner_account, txmint);
  DO_CALLBACK(events, "mark_invalid_tx");
  MAKE_REMINT_TX(events, 0xbeef, 5555, alice, txremint, k.sec, generate_invalid_pub_key());
  
  return true;
}

bool gen_currency_remint_invalid_signature::generate(std::vector<test_event_entry>& events) const
{
  uint64_t ts_start = 1338224400;
  
  GENERATE_ACCOUNT(miner_account);
  GENERATE_ACCOUNT(alice);
  GENERATE_ACCOUNT(bob);
  
  MAKE_STARTING_BLOCKS(events, blk_0, miner_account, ts_start);
  
  keypair k = keypair::generate();
  keypair k_other = keypair::generate();
  
  MAKE_MINT_TX(events, 0xbeef, "Beef futures", 4000, alice, txmint, 2, k.pub);
  MAKE_NEXT_BLOCK_TX1(events, blk_1, blk_0, miner_account, txmint);
  DO_CALLBACK(events, "mark_invalid_tx");
  MAKE_REMINT_TX(events, 0xbeef, 5555, alice, txremint, k_other.sec, null_pkey);
  
  return true;
}

bool gen_currency_remint_invalid_spend_more_than_remint::generate(std::vector<test_event_entry>& events) const
{
  uint64_t ts_start = 1338224400;
  
  GENERATE_ACCOUNT(miner_account);
  GENERATE_ACCOUNT(alice);
  GENERATE_ACCOUNT(bob);
  
  MAKE_STARTING_BLOCKS(events, blk_0, miner_account, ts_start);
  
  keypair k = keypair::generate();

  MAKE_MINT_TX(events, 0xbeef, "Beef futures", 4000, alice, txmint, 2, k.pub);
  MAKE_NEXT_BLOCK_TX1(events, blk_1, blk_0, miner_account, txmint);

  DO_CALLBACK(events, "mark_invalid_tx");
  cryptonote::transaction txremint;
  {
    tx_builder txb;
    txb.init();
    std::vector<tx_destination_entry> destinations;
    destinations.push_back(tx_destination_entry(coin_type(0xbeef, NotContract, BACKED_BY_N_A),
                                                2000, alice.get_keys().m_account_address));
    txb.add_remint(0xbeef, 2000, k.sec, null_pkey, destinations);
    txb.finalize();
    CHECK_AND_ASSERT_MES(txb.get_finalized_tx(txremint), false, "Unable to get finalized mint tx");
    auto& in_mint = boost::get<txin_remint&>(tx_tester(txremint).vin.back());
    in_mint.amount -= 1;
    events.push_back(txremint);
  }
  
  return true;
}

bool gen_currency_remint_twice::generate(std::vector<test_event_entry>& events) const
{
  uint64_t ts_start = 1338224400;
  
  GENERATE_ACCOUNT(miner_account);
  GENERATE_ACCOUNT(alice);
  GENERATE_ACCOUNT(bob);
  
  MAKE_STARTING_BLOCKS(events, blk_0, miner_account, ts_start);
  
  keypair k = keypair::generate();
  
  MAKE_MINT_TX(events, 0xbeef, "Beef futures", 4000, alice, txmint, 2, k.pub);
  MAKE_NEXT_BLOCK_TX1(events, blk_1, blk_0, miner_account, txmint);
  MAKE_TX_C(events, txbeef1, alice, bob, 4000, blk_1, 0xbeef);
  MAKE_NEXT_BLOCK_TX1(events, blk_2, blk_1, miner_account, txbeef1);
  
  MAKE_REMINT_TX(events, 0xbeef, 5555, alice, txremint, k.sec, k.pub);
  MAKE_NEXT_BLOCK_TX1(events, blk_3, blk_2, miner_account, txremint);
  MAKE_TX_C(events, txbeef2, alice, bob, 5555, blk_3, 0xbeef);
  MAKE_NEXT_BLOCK_TX1(events, blk_4, blk_3, miner_account, txbeef2);
  
  MAKE_REMINT_TX(events, 0xbeef, 33333, alice, txremint2, k.sec, null_pkey);
  MAKE_NEXT_BLOCK_TX1(events, blk_5, blk_4, miner_account, txremint2);
  MAKE_TX_C(events, txbeef3, alice, bob, 33333, blk_5, 0xbeef);
  MAKE_NEXT_BLOCK_TX1(events, blk_6, blk_5, miner_account, txbeef3);
  
  return true;
}

bool gen_currency_remint_change_key_old_invalid::generate(std::vector<test_event_entry>& events) const
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
  
  DO_CALLBACK(events, "mark_invalid_tx");
  MAKE_REMINT_TX(events, 0xbeef, 33333, alice, txremint2, k.sec, null_pkey);
  
  return true;
}

bool gen_currency_remint_change_key_remint::generate(std::vector<test_event_entry>& events) const
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
  MAKE_NEXT_BLOCK_TX1(events, blk_6, blk_5, miner_account, txbeef3);
  
  return true;
}

bool gen_currency_remint_can_remint_twice_per_block::generate(std::vector<test_event_entry>& events) const
{
  uint64_t ts_start = 1338224400;
  
  GENERATE_ACCOUNT(miner_account);
  GENERATE_ACCOUNT(alice);
  GENERATE_ACCOUNT(bob);
  
  MAKE_STARTING_BLOCKS(events, blk_0, miner_account, ts_start);
  
  keypair k = keypair::generate();
  
  MAKE_MINT_TX(events, 0xbeef, "Beef futures", 100, alice, txmint1, 2, k.pub);
  MAKE_NEXT_BLOCK_TX1(events, blk_1, blk_0, miner_account, txmint1);
  
  MAKE_REMINT_TX(events, 0xbeef, 100, alice, txremint1, k.sec, k.pub);
  MAKE_REMINT_TX(events, 0xbeef, 100, alice, txremint2, k.sec, k.pub);
  std::list<transaction> txs;
  txs.push_back(txremint1);
  txs.push_back(txremint2);
  MAKE_NEXT_BLOCK_TX_LIST(events, blk_2, blk_1, miner_account, txs);
  MAKE_REMINT_TX(events, 0xbeef, 100, alice, txremint3, k.sec, k.pub);
  MAKE_NEXT_BLOCK_TX1(events, blk_3, blk_2, miner_account, txremint3);
  
  return true;
}

bool gen_currency_remint_cant_mint_remint_same_block::generate(std::vector<test_event_entry>& events) const
{
  uint64_t ts_start = 1338224400;
  
  GENERATE_ACCOUNT(miner_account);
  GENERATE_ACCOUNT(alice);
  GENERATE_ACCOUNT(bob);
  
  MAKE_STARTING_BLOCKS(events, blk_0, miner_account, ts_start);
  
  keypair k = keypair::generate();
  
  MAKE_MINT_TX(events, 0xbeef, "Beef futures", 100, alice, txmint1, 2, k.pub);
  DO_CALLBACK(events, "mark_invalid_tx");
  MAKE_REMINT_TX(events, 0xbeef, 100, alice, txremint1, k.sec, k.pub);

  MAKE_NEXT_BLOCK_TX1(events, blk_1, blk_0, miner_account, txmint1);
  MAKE_REMINT_TX(events, 0xbeef, 100, alice, txremint2, k.sec, k.pub);
  MAKE_NEXT_BLOCK_TX1(events, blk_2, blk_1, miner_account, txremint2);
  
  return true;
}

bool gen_currency_remint_limit_uint64max::generate(std::vector<test_event_entry>& events) const
{
  uint64_t ts_start = 1338224400;
  
  GENERATE_ACCOUNT(miner_account);
  GENERATE_ACCOUNT(alice);
  GENERATE_ACCOUNT(bob);
  
  MAKE_STARTING_BLOCKS(events, blk_0, miner_account, ts_start);
  
  keypair k = keypair::generate();
  
  MAKE_MINT_TX(events, 0xbeef, "Beef futures", std::numeric_limits<uint64_t>::max(), alice, txmint1, 2, k.pub);
  MAKE_NEXT_BLOCK_TX1(events, blk_1, blk_0, miner_account, txmint1);
  DO_CALLBACK(events, "mark_invalid_tx");
  MAKE_REMINT_TX(events, 0xbeef, std::numeric_limits<uint64_t>::max(), alice, txremint1, k.sec, k.pub);
  DO_CALLBACK(events, "mark_invalid_tx");
  MAKE_REMINT_TX(events, 0xbeef, 1, alice, txremint2, k.sec, k.pub);

  MAKE_MINT_TX(events, 0xbeef2, "Beef futures2", std::numeric_limits<uint64_t>::max() / 2, alice, txmint2, 2, k.pub);
  MAKE_NEXT_BLOCK_TX1(events, blk_2, blk_1, miner_account, txmint2);
  MAKE_REMINT_TX(events, 0xbeef2, std::numeric_limits<uint64_t>::max() / 2 - 10, alice, txremint3, k.sec, k.pub);
  MAKE_REMINT_TX(events, 0xbeef2, 50, alice, txremint4, k.sec, k.pub);

  DO_CALLBACK(events, "mark_invalid_block");
  std::list<transaction> txs;
  txs.push_back(txremint3);
  txs.push_back(txremint4);
  MAKE_NEXT_BLOCK_TX_LIST(events, blk_3, blk_2, miner_account, txs);

  return true;
}

*/