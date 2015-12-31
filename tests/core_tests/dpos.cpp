#if defined(_MSC_VER) && _MSC_VER < 1900

  // Visual Studio 11 does not support initialiezr lists

#else

// Copyright (c) 2015 The Pebblecoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "dpos.h"
#include "cryptonote_core/tx_builder.h"
#include "cryptonote_core/tx_tester.h"
#include "cryptonote_core/blockchain_storage.h"

using namespace epee;
using namespace crypto;
using namespace cryptonote;

static account_base g_junk_account;

template <class txb_call_t, class tx_modifier_t>
cryptonote::transaction make_register_delegate_tx(std::vector<test_event_entry>& events,
                                                  delegate_id_t delegate_id, uint64_t registration_fee,
                                                  const account_base delegate_source,
                                                  const block& head,
                                                  const txb_call_t& txb_call,
                                                  const tx_modifier_t& mod)
{
  cryptonote::transaction result;
  if (!make_register_delegate_tx_(events, result, delegate_id, registration_fee, delegate_source, head, txb_call, mod))
  {
    throw test_gen_error("Failed to make_register_delegate_tx");
  }
  // func call already pushed to events
  return result;
}
cryptonote::transaction make_register_delegate_tx(std::vector<test_event_entry>& events,
                                                  delegate_id_t delegate_id, uint64_t registration_fee,
                                                  const account_base delegate_source,
                                                  const block& head)
{
  return make_register_delegate_tx(events, delegate_id, registration_fee, delegate_source, head, tools::identity(), tools::identity());
}

template <class txb_call_t, class tx_modifier_t>
cryptonote::transaction make_vote_tx(std::vector<test_event_entry>& events,
                                     uint64_t amount, const delegate_votes& votes,
                                     const account_base vote_source,
                                     const block& head,
                                     uint16_t seq, size_t nmix,
                                     const txb_call_t& txb_call,
                                     const tx_modifier_t& mod)
{
  cryptonote::transaction result;
  if (!make_vote_tx_(events, result, amount, seq, votes, vote_source, head, nmix, txb_call, mod))
  {
    throw test_gen_error("Failed to make_vote_tx");
  }
  // func call already pushed to events
  return result;
}

cryptonote::transaction make_vote_tx(std::vector<test_event_entry>& events,
                                     uint64_t amount, const delegate_votes& votes,
                                     const account_base vote_source,
                                     const block& head,
                                     uint16_t seq=0, size_t nmix=0)
{
  return make_vote_tx(events, amount, votes, vote_source, head, seq, nmix, tools::identity(), tools::identity());
}

delegate_votes make_votes(delegate_id_t delegate_id)
{
  delegate_votes votes;
  votes.insert(delegate_id);
  return votes;
}
delegate_votes make_votes(delegate_id_t id1, delegate_id_t id2)
{
  delegate_votes votes;
  votes.insert(id1);
  votes.insert(id2);
  return votes;
}
delegate_votes make_votes(const std::vector<delegate_id_t>& v)
{
  delegate_votes votes;
  BOOST_FOREACH(const auto& i, v)
    votes.insert(i);
  return votes;
}

#define MT(a, b, c) std::make_tuple(a, b, make_votes(c))

std::list<transaction> make_votes_txs(std::vector<test_event_entry>& events,
                                      const std::vector<std::tuple<account_base, uint64_t, delegate_votes> >& votes,
                                      const block& head)
{
  std::list<transaction> tx_votes;
  BOOST_FOREACH(const auto& v, votes)
  {
    tx_votes.push_back(make_vote_tx(events, std::get<1>(v), std::get<2>(v), std::get<0>(v), head));
  }
  return tx_votes;
}

block setup_full_dpos_test(test_generator& generator, std::vector<test_event_entry>& events,
                           const std::vector<account_base>& miners,
                           const std::vector<std::pair<account_base, uint64_t> >& with_money,
                           const std::vector<std::pair<account_base, delegate_id_t> >& dpos_registrants,
                           const std::vector<std::tuple<account_base, uint64_t, delegate_votes> >& votes)
{
  auto& miner = miners[0];
  
  auto blk_0 = make_genesis_block(generator, events, miner, g_ntp_time.get_time() - cryptonote::config::difficulty_blocks_estimate_timespan()*32);
  auto blk_0r = rewind_blocks(generator, events, blk_0, miners, 28);
  
  set_dpos_registration_start_block(events, 1);
  
  std::list<transaction> tx_first_sends;
  BOOST_FOREACH(const auto& acc_to, with_money)
  {
    tx_first_sends.push_back(make_tx_send(events, miner, acc_to.first, acc_to.second, blk_0r));
  }
  auto blk_1 = make_next_block(generator, events, blk_0r, miner, tx_first_sends);
  
  std::list<transaction> tx_regs;
  BOOST_FOREACH(const auto& p, dpos_registrants)
  {
    do_register_delegate_account(events, p.second, p.first); // for check_can_create_block_from_template
    tx_regs.push_back(make_register_delegate_tx(events, p.second, MK_COINS(5), p.first, blk_1));
  }
  auto blk_2 = make_next_block(generator, events, blk_1, miner, tx_regs);
  auto blk_3 = make_next_block(generator, events, blk_2, miner, make_votes_txs(events, votes, blk_2));
  
  set_dpos_switch_block(events, 33);
  
  // 1 more junk block = block 32
  auto blk_last_pow = make_next_block(generator, events, blk_3, miner);
  
  do_debug_mark(events, "start_pos");
  
  return blk_last_pow;
}


bool gen_dpos_register::generate(std::vector<test_event_entry>& events) const
{
  INIT_DPOS_TEST();
  
  MAKE_TX(events, tx_send, miner_account, alice, MK_COINS(100), blk_0r);
  MAKE_NEXT_BLOCK_TX1(events, blk_1a, blk_0r, miner_account, tx_send);
  MAKE_TX(events, tx_send2, miner_account, bob, MK_COINS(100), blk_1a);
  MAKE_NEXT_BLOCK_TX1(events, blk_1, blk_1a, miner_account, tx_send2);
  
  CREATE_REGISTER_DELEGATE_TX(events, tx_reg, 0xb0b, MK_COINS(5), alice, blk_1);
  MAKE_NEXT_BLOCK_TX1(events, blk_2, blk_1, miner_account, tx_reg);
  CREATE_REGISTER_DELEGATE_TX(events, tx_reg2, 0xb0b2, MK_COINS(5), bob, blk_2);
  MAKE_NEXT_BLOCK_TX1(events, blk_3, blk_2, miner_account, tx_reg2);
  
  return true;
}

bool gen_dpos_register_too_soon::generate(std::vector<test_event_entry>& events) const
{
  INIT_DPOS_TEST();
  
  MAKE_TX(events, tx_send, miner_account, alice, MK_COINS(100), blk_0r);
  MAKE_NEXT_BLOCK_TX1(events, blk_1a, blk_0r, miner_account, tx_send);
  MAKE_TX(events, tx_send2, miner_account, bob, MK_COINS(100), blk_1a);
  MAKE_NEXT_BLOCK_TX1(events, blk_1, blk_1a, miner_account, tx_send2);
  
  set_dpos_registration_start_block(events, 100);
  
  DO_CALLBACK(events, "mark_invalid_tx");
  CREATE_REGISTER_DELEGATE_TX(events, tx_reg, 0xb0b, MK_COINS(5), alice, blk_1);
  DO_CALLBACK(events, "mark_invalid_tx");
  CREATE_REGISTER_DELEGATE_TX(events, tx_reg2, 0xb0b2, MK_COINS(5), bob, blk_1);
  
  return true;
}

bool gen_dpos_register_low_fee::generate(std::vector<test_event_entry>& events) const
{
  INIT_DPOS_TEST();
  
  MAKE_TX(events, tx_send, miner_account, alice, MK_COINS(100), blk_0r);
  MAKE_NEXT_BLOCK_TX1(events, blk_1, blk_0r, miner_account, tx_send);
  
  DO_CALLBACK(events, "mark_invalid_tx");
  CREATE_REGISTER_DELEGATE_TX(events, tx_reg, 0xb0b, DPOS_MIN_REGISTRATION_FEE - 1, alice, blk_1);
  
  SET_EVENT_VISITOR_SETT(events, event_visitor_settings::set_txs_keeped_by_block, true);
  events.push_back(tx_reg);
  DO_CALLBACK(events, "mark_invalid_block");
  MAKE_NEXT_BLOCK_TX1(events, blk_2, blk_1, miner_account, tx_reg);
  
  return true;
}

bool gen_dpos_register_low_fee_2::generate(std::vector<test_event_entry>& events) const
{
  INIT_DPOS_TEST(); // 14 blocks with 0 tx fees
  
  // send a few very high-fee transactions
  MAKE_TX_MIX_CP_FEE(events, tx_send, miner_account, alice, MK_COINS(100), 0, blk_0r,
                     CP_XPB, MK_COINS(5));
  MAKE_NEXT_BLOCK_TX1(events, blk_1a, blk_0r, miner_account, tx_send);
  MAKE_TX_MIX_CP_FEE(events, tx_send2, miner_account, alice, MK_COINS(100), 0, blk_1a,
                     CP_XPB, MK_COINS(5));
  MAKE_NEXT_BLOCK_TX1(events, blk_1, blk_1a, miner_account, tx_send2);
  
  // avg fee should be 10/16 = 0.625 per block, *DPOS_REGISTRATION_FEE_MULTIPLE
  DO_CALLBACK(events, "mark_invalid_tx");
  CREATE_REGISTER_DELEGATE_TX(events, tx_reg1, 0xb0b, MK_COINS(1)*0.625*DPOS_REGISTRATION_FEE_MULTIPLE - 50, alice, blk_1);
  
  CREATE_REGISTER_DELEGATE_TX(events, tx_reg2, 0xb0b, MK_COINS(1)*0.625*DPOS_REGISTRATION_FEE_MULTIPLE + 50, alice, blk_1);
  MAKE_NEXT_BLOCK_TX1(events, blk_2, blk_1, miner_account, tx_reg2);
  
  return true;
}

bool gen_dpos_register_invalid_id::generate(std::vector<test_event_entry>& events) const
{
  INIT_DPOS_TEST();
  
  MAKE_TX(events, tx_send, miner_account, alice, MK_COINS(100), blk_0r);
  MAKE_NEXT_BLOCK_TX1(events, blk_1a, blk_0r, miner_account, tx_send);
  MAKE_TX(events, tx_send2, miner_account, bob, MK_COINS(100), blk_1a);
  MAKE_NEXT_BLOCK_TX1(events, blk_1b, blk_1a, miner_account, tx_send2);
  MAKE_TX(events, tx_send3, miner_account, carol, MK_COINS(100), blk_1b);
  MAKE_NEXT_BLOCK_TX1(events, blk_1, blk_1b, miner_account, tx_send3);
  
  CREATE_REGISTER_DELEGATE_TX(events, tx_reg, 0xb0b, MK_COINS(5), alice, blk_1);
  MAKE_NEXT_BLOCK_TX1(events, blk_2, blk_1, miner_account, tx_reg);
  
  // invalid in mem-pool
  DO_CALLBACK(events, "mark_invalid_tx");
  CREATE_REGISTER_DELEGATE_TX(events, tx_reg_bad, 0xb0b, MK_COINS(5), bob, blk_2);
  
  // invalid in block
  SET_EVENT_VISITOR_SETT(events, event_visitor_settings::set_txs_keeped_by_block, true);
  events.push_back(tx_reg_bad);
  SET_EVENT_VISITOR_SETT(events, event_visitor_settings::set_txs_keeped_by_block, false);
  DO_CALLBACK(events, "mark_invalid_block");
  MAKE_NEXT_BLOCK_TX1(events, blk_3, blk_2, miner_account, tx_reg_bad);
  
  // invalid two delegate ids in same tx
  DO_CALLBACK(events, "mark_invalid_tx");
  CREATE_REGISTER_DELEGATE_TX_FULL(events, tx_reg_indup, 0x333, MK_COINS(10), carol, 0, blk_2,
                                   tools::identity(),
                                   [&](transaction& tx) {
                                     boost::get<txin_register_delegate>(tx_tester(tx).vin[1]).registration_fee -= MK_COINS(5);
                                     txin_register_delegate inreg;
                                     inreg.delegate_id = 0x333;
                                     inreg.delegate_address = dave.get_keys().m_account_address;
                                     inreg.registration_fee = MK_COINS(5);
                                     tx.add_in(inreg, CP_XPB);
                                   });
  SET_EVENT_VISITOR_SETT(events, event_visitor_settings::set_txs_keeped_by_block, true);
  DO_CALLBACK(events, "mark_invalid_tx");
  events.push_back(tx_reg_indup);
  SET_EVENT_VISITOR_SETT(events, event_visitor_settings::set_txs_keeped_by_block, false);
  
  return true;
}

bool gen_dpos_register_invalid_id_2::generate(std::vector<test_event_entry>& events) const
{
  INIT_DPOS_TEST();
  
  MAKE_TX(events, tx_send, miner_account, alice, MK_COINS(100), blk_0r);
  MAKE_NEXT_BLOCK_TX1(events, blk_1a, blk_0r, miner_account, tx_send);
  
  // can't use delegate id 0
  DO_CALLBACK(events, "mark_invalid_tx");
  CREATE_REGISTER_DELEGATE_TX(events, tx_reg, 0, MK_COINS(5), alice, blk_1a);
  
  return true;
}

bool gen_dpos_register_invalid_address::generate(std::vector<test_event_entry>& events) const
{
  INIT_DPOS_TEST();
  
  MAKE_TX(events, tx_send, miner_account, alice, MK_COINS(100), blk_0r);
  MAKE_NEXT_BLOCK_TX1(events, blk_1a, blk_0r, miner_account, tx_send);
  MAKE_TX(events, tx_send2, miner_account, bob, MK_COINS(100), blk_1a);
  MAKE_NEXT_BLOCK_TX1(events, blk_1b, blk_1a, miner_account, tx_send2);
  MAKE_TX(events, tx_send3, miner_account, carol, MK_COINS(100), blk_1b);
  MAKE_NEXT_BLOCK_TX1(events, blk_1, blk_1b, miner_account, tx_send3);
  
  CREATE_REGISTER_DELEGATE_TX(events, tx_reg, 0xb0b, MK_COINS(5), alice, blk_1);
  MAKE_NEXT_BLOCK_TX1(events, blk_2, blk_1, miner_account, tx_reg);
  
  // invalid in mem-pool
  DO_CALLBACK(events, "mark_invalid_tx");
  CREATE_REGISTER_DELEGATE_TX(events, tx_reg_bad, 0xb0b2, MK_COINS(5), alice, blk_2);
  
  // invalid in block
  SET_EVENT_VISITOR_SETT(events, event_visitor_settings::set_txs_keeped_by_block, true);
  events.push_back(tx_reg_bad);
  SET_EVENT_VISITOR_SETT(events, event_visitor_settings::set_txs_keeped_by_block, false);
  DO_CALLBACK(events, "mark_invalid_block");
  MAKE_NEXT_BLOCK_TX1(events, blk_3, blk_2, miner_account, tx_reg_bad);
  
  // invalid two delegate addresses in same tx
  DO_CALLBACK(events, "mark_invalid_tx");
  CREATE_REGISTER_DELEGATE_TX_FULL(events, tx_reg_addrdup, 0x333, MK_COINS(10), carol, 0, blk_2,
                                   tools::identity(),
                                   [&](transaction& tx) {
                                     boost::get<txin_register_delegate>(tx_tester(tx).vin[1]).registration_fee -= MK_COINS(5);
                                     txin_register_delegate inreg;
                                     inreg.delegate_id = 0x555;
                                     inreg.delegate_address = carol.get_keys().m_account_address;
                                     inreg.registration_fee = MK_COINS(5);
                                     tx.add_in(inreg, CP_XPB);
                                   });
  SET_EVENT_VISITOR_SETT(events, event_visitor_settings::set_txs_keeped_by_block, true);
  DO_CALLBACK(events, "mark_invalid_tx");
  events.push_back(tx_reg_addrdup);
  SET_EVENT_VISITOR_SETT(events, event_visitor_settings::set_txs_keeped_by_block, false);
  
  
  return true;
}

bool gen_dpos_vote::generate(std::vector<test_event_entry>& events) const
{
  INIT_DPOS_TEST();
  
  MAKE_TX(events, tx_send, miner_account, alice, MK_COINS(100), blk_0r);
  MAKE_NEXT_BLOCK_TX1(events, blk_1a, blk_0r, miner_account, tx_send);
  MAKE_TX(events, tx_send2, miner_account, bob, MK_COINS(5), blk_1a); // 100 would be too much, 4800 coins total at this pt
  MAKE_NEXT_BLOCK_TX1(events, blk_1, blk_1a, miner_account, tx_send2);
  
  CREATE_REGISTER_DELEGATE_TX(events, tx_reg, 0xa1c, MK_COINS(5), alice, blk_1);
  MAKE_NEXT_BLOCK_TX1(events, blk_2, blk_1, miner_account, tx_reg);
  
  CREATE_VOTE_TX_1(events, tx_vote, MK_COINS(5), bob, blk_2, 0xa1c);
  MAKE_NEXT_BLOCK_TX1(events, blk_3, blk_2, miner_account, tx_vote);
  
  return true;
}

bool gen_dpos_vote_too_soon::generate(std::vector<test_event_entry>& events) const
{
  INIT_DPOS_TEST();
  
  MAKE_TX(events, tx_send, miner_account, alice, MK_COINS(100), blk_0r);
  MAKE_NEXT_BLOCK_TX1(events, blk_1a, blk_0r, miner_account, tx_send);
  MAKE_TX(events, tx_send2, miner_account, bob, MK_COINS(5), blk_1a); // 100 would be too much, 4800 coins total at this pt
  MAKE_NEXT_BLOCK_TX1(events, blk_1, blk_1a, miner_account, tx_send2);
  
  set_dpos_registration_start_block(events, 100);
  
  DO_CALLBACK(events, "mark_invalid_tx");
  make_vote_tx(events, MK_COINS(10), make_votes({}),
               alice, blk_1);
  
  return true;
}

bool gen_dpos_switch_to_dpos::generate(std::vector<test_event_entry>& events) const
{
  TEST_NEW_BEGIN();

  auto miner = generate_account();
  auto alice = generate_account();
  auto bob = generate_account();
  auto carol = generate_account();
  auto dave = generate_account();
  
  test_generator generator;
  auto blk_last_pow = setup_full_dpos_test(generator, events, {miner},
                                           {
                                             {alice, MK_COINS(100)},
                                             {bob, MK_COINS(100)},
                                             {carol, MK_COINS(100)},
                                             {dave, MK_COINS(100)}
                                           },
                                           {
                                             {alice, 0x001},
                                             {bob, 0x002}
                                           },
                                           {
                                             MT(alice, MK_COINS(10), 0x001), MT(carol, MK_COINS(10), 0x001),
                                             MT(dave, MK_COINS(10), 0x001), MT(bob, MK_COINS(10), 0x002)
                                           });
  
  // pow blocks are now invalid
  do_callback(events, "mark_invalid_block");
  make_next_block(generator, events, blk_last_pow, miner);
  
  // wrong delegate_id
  do_callback(events, "mark_invalid_block");
  make_next_pos_block(generator, events, blk_last_pow, alice, 0x002);
  do_callback(events, "mark_invalid_block");
  make_next_pos_block(generator, events, blk_last_pow, alice, 0x005);
  
  // wrong address for the right delegate id
  do_callback(events, "mark_invalid_block");
  make_next_pos_block(generator, events, blk_last_pow, bob, 0x001);
  do_callback(events, "mark_invalid_block");
  make_next_pos_block(generator, events, blk_last_pow, carol, 0x001);
  do_callback(events, "mark_invalid_block");
  make_next_pos_block(generator, events, blk_last_pow, dave, 0x001);
  
  // next block is pos, block 23
  auto blk_pos_1 = make_next_pos_block(generator, events, blk_last_pow, alice, 0x001);
  
  // now it's bob, 0x002's turn
  do_callback(events, "mark_invalid_block");
  make_next_pos_block(generator, events, blk_pos_1, bob, 0x001);
  do_callback(events, "mark_invalid_block");
  make_next_pos_block(generator, events, blk_pos_1, bob, 0x005);
  do_callback(events, "mark_invalid_block");
  make_next_pos_block(generator, events, blk_pos_1, alice, 0x002);
  do_callback(events, "mark_invalid_block");
  make_next_pos_block(generator, events, blk_pos_1, carol, 0x002);
  do_callback(events, "mark_invalid_block");
  make_next_pos_block(generator, events, blk_pos_1, dave, 0x002);
  
  auto blk_pos_2 = make_next_pos_block(generator, events, blk_pos_1, bob, 0x002);
  
  // now alice's turn
  do_callback(events, "mark_invalid_block");
  make_next_pos_block(generator, events, blk_pos_2, alice, 0x002);
  do_callback(events, "mark_invalid_block");
  make_next_pos_block(generator, events, blk_pos_2, alice, 0x005);
  do_callback(events, "mark_invalid_block");
  make_next_pos_block(generator, events, blk_pos_2, bob, 0x001);
  do_callback(events, "mark_invalid_block");
  make_next_pos_block(generator, events, blk_pos_2, carol, 0x001);
  do_callback(events, "mark_invalid_block");
  make_next_pos_block(generator, events, blk_pos_2, dave, 0x001);
  
  auto blk_pos_3 = make_next_pos_block(generator, events, blk_pos_2, alice, 0x001);
  auto blk_pos_4 = make_next_pos_block(generator, events, blk_pos_3, bob, 0x002);
  
  // alternative pos blocks & chain switch should work as well
  
  auto blk_pos_alt_1 = make_next_pos_block(generator, events, blk_last_pow, alice, 0x001);
  auto blk_pos_alt_2 = make_next_pos_block(generator, events, blk_pos_alt_1, bob, 0x002);
  auto blk_pos_alt_3 = make_next_pos_block(generator, events, blk_pos_alt_2, alice, 0x001);
  auto blk_pos_alt_4 = make_next_pos_block(generator, events, blk_pos_alt_3, bob, 0x002);
  // and on this one we switch:
  auto blk_pos_alt_5 = make_next_pos_block(generator, events, blk_pos_alt_4, alice, 0x001);
  
  return true;
  
  TEST_NEW_END();
}

bool gen_dpos_altchain_voting::generate(std::vector<test_event_entry>& events) const
{
  TEST_NEW_BEGIN();

  auto miner = generate_account();
  auto alice = generate_account();
  auto bob = generate_account();
  auto carol = generate_account();
  auto dave = generate_account();
  
  test_generator generator;
  auto blk_last_pow = setup_full_dpos_test(generator, events, {miner},
                                           {
                                             {alice, MK_COINS(100)},
                                             {bob, MK_COINS(100)},
                                             {carol, MK_COINS(100)},
                                             {dave, MK_COINS(100)}
                                           },
                                           {
                                             {alice, 0x001},
                                             {bob, 0x002}
                                           },
                                           {
                                             MT(alice, MK_COINS(10), 0x001), MT(carol, MK_COINS(10), 0x001),
                                             MT(dave, MK_COINS(10), 0x001), MT(bob, MK_COINS(10), 0x002)
                                           });
  
  // register a third carol
  do_register_delegate_account(events, 0x005, carol);
  auto blk_pos_1 = make_next_pos_block(generator, events, blk_last_pow, alice, 0x001,
                                       make_register_delegate_tx(events, 0x005, MK_COINS(5), carol, blk_last_pow));
  
  // prove carol now works
  auto blk_pos_2 = make_next_pos_block(generator, events, blk_pos_1, bob, 0x002);
  auto blk_pos_3 = make_next_pos_block(generator, events, blk_pos_2, carol, 0x005);
  auto blk_pos_4 = make_next_pos_block(generator, events, blk_pos_3, alice, 0x001);
  
  // alt-chain from pre-carol should work with only alice & bob since there is no carol there
  auto blk_altpos_1 = make_next_pos_block(generator, events, blk_last_pow, alice, 0x001);
  auto blk_altpos_2 = make_next_pos_block(generator, events, blk_altpos_1, bob, 0x002);
  // shouldn't be able to sneak pow blocks in here
  do_callback(events, "mark_invalid_block");
  make_next_block(generator, events, blk_altpos_2, miner);
  auto blk_altpos_3 = make_next_pos_block(generator, events, blk_altpos_2, alice, 0x001);
  auto blk_altpos_4 = make_next_pos_block(generator, events, blk_altpos_3, bob, 0x002);
  // should switch to main here
  auto blk_altpos_5 = make_next_pos_block(generator, events, blk_altpos_4, alice, 0x001);
  auto blk_altpos_6 = make_next_pos_block(generator, events, blk_altpos_5, bob, 0x002);
  
  // carol should not be valid here
  do_callback(events, "mark_invalid_block");
  make_next_pos_block(generator, events, blk_altpos_6, carol, 0x005);
  auto blk_altpos_7 = make_next_pos_block(generator, events, blk_altpos_6, alice, 0x001);
 
  return true;
  
  TEST_NEW_END();
}

bool gen_dpos_altchain_voting_invalid::generate(std::vector<test_event_entry>& events) const
{
  TEST_NEW_BEGIN();

  auto miner = generate_account();
  auto alice = generate_account();
  auto bob = generate_account();
  auto carol = generate_account();
  auto dave = generate_account();
  
  test_generator generator;
  auto blk_last_pow = setup_full_dpos_test(generator, events, {miner},
                                           {
                                             {alice, MK_COINS(100)},
                                             {bob, MK_COINS(100)},
                                             {carol, MK_COINS(100)},
                                             {dave, MK_COINS(100)}
                                           },
                                           {
                                             {alice, 0x001},
                                             {bob, 0x002}
                                           },
                                           {
                                             MT(alice, MK_COINS(10), 0x001), MT(carol, MK_COINS(10), 0x001),
                                             MT(dave, MK_COINS(10), 0x001), MT(bob, MK_COINS(10), 0x002)
                                           });
  
  // register a third carol
  do_register_delegate_account(events, 0x005, carol);
  auto blk_pos_1 = make_next_pos_block(generator, events, blk_last_pow, alice, 0x001,
                                       make_register_delegate_tx(events, 0x005, MK_COINS(5), carol, blk_last_pow));
  
  // prove carol now works
  auto blk_pos_2 = make_next_pos_block(generator, events, blk_pos_1, bob, 0x002);
  auto blk_pos_3 = make_next_pos_block(generator, events, blk_pos_2, carol, 0x005);
  auto blk_pos_4 = make_next_pos_block(generator, events, blk_pos_3, alice, 0x001);
  
  // alt-chain from pre-carol should work with only alice & bob since there is no carol there
  // but it isn't checked, so what happens when we try to switch?
  auto blk_altpos_1 = make_next_pos_block(generator, events, blk_last_pow, alice, 0x001);
  auto blk_altpos_2 = make_next_pos_block(generator, events, blk_altpos_1, bob, 0x002);
  auto blk_altpos_3 = make_next_pos_block(generator, events, blk_altpos_2, carol, 0x005);
  auto blk_altpos_4 = make_next_pos_block(generator, events, blk_altpos_3, alice, 0x001);
  // this next one should fail to cause the switch
  do_callback(events, "mark_invalid_block");
  auto blk_altpos_5 = make_next_pos_block(generator, events, blk_altpos_4, bob, 0x002);
  
  // rollback should have worked, existing chain unaffected
  auto blk_pos_5 = make_next_pos_block(generator, events, blk_pos_4, bob, 0x002);
  auto blk_pos_6 = make_next_pos_block(generator, events, blk_pos_5, carol, 0x005);
  auto blk_pos_7 = make_next_pos_block(generator, events, blk_pos_6, alice, 0x001);

  return true;
  
  TEST_NEW_END();
}

bool gen_dpos_limit_delegates::generate(std::vector<test_event_entry>& events) const
{
  TEST_NEW_BEGIN();
  
  auto A = generate_accounts(15);
  
  test_generator generator;
  auto blk_last_pow = setup_full_dpos_test(generator, events, {A[0]},
                                           {
                                             {A[1], MK_COINS(5)},
                                             {A[2], MK_COINS(5)},
                                             {A[3], MK_COINS(5)},
                                             {A[4], MK_COINS(5)},
                                             {A[5], MK_COINS(5)},
                                             {A[6], MK_COINS(5)},
                                             {A[7], MK_COINS(5)},
                                             {A[8], MK_COINS(70)},
                                             {A[9], MK_COINS(60)},
                                             {A[10], MK_COINS(50)},
                                             {A[11], MK_COINS(40)},
                                             {A[12], MK_COINS(30)},
                                             {A[13], MK_COINS(20)},
                                           },
                                           {
                                             {A[1], 0x001}, {A[2], 0x002},
                                             {A[3], 0x003}, {A[4], 0x004},
                                             {A[5], 0x005}, {A[6], 0x006},
                                             {A[7], 0x007},
                                           },
                                           {
                                             // 1, 2, 4, 5, 7 are top:
                                             MT(A[8], MK_COINS(70), 0x004),
                                             MT(A[9], MK_COINS(60), 0x002),
                                             MT(A[10], MK_COINS(50), 0x005),
                                             MT(A[11], MK_COINS(40), 0x001),
                                             MT(A[12], MK_COINS(30), 0x007),
                                             // 3 has not enough votes:
                                             MT(A[13], MK_COINS(20), 0x003),
                                             // 6 has no votes
                                           });
  
  // make sure only 5 work (the limit for testing), 3 and 6 don't have enough votes
  auto blk_pos_1 = make_next_pos_block(generator, events, blk_last_pow, A[1], 0x001);
  auto blk_pos_2 = make_next_pos_block(generator, events, blk_pos_1, A[2], 0x002);
  auto blk_pos_3 = make_next_pos_block(generator, events, blk_pos_2, A[4], 0x004);
  auto blk_pos_4 = make_next_pos_block(generator, events, blk_pos_3, A[5], 0x005);
  auto blk_pos_5 = make_next_pos_block(generator, events, blk_pos_4, A[7], 0x007);
  auto blk_pos_6 = make_next_pos_block(generator, events, blk_pos_5, A[1], 0x001);
  auto blk_pos_7 = make_next_pos_block(generator, events, blk_pos_6, A[2], 0x002);
  auto blk_pos_8 = make_next_pos_block(generator, events, blk_pos_7, A[4], 0x004);
  
  return true;
  
  TEST_NEW_END();
}

bool gen_dpos_unapply_votes::generate(std::vector<test_event_entry>& events) const
{
  // this test depends on two txin_votes in one tx being applied & unapplied properly
  TEST_NEW_BEGIN();
  
  auto A = generate_accounts(8);
  
  test_generator generator;
  auto blk_last_pow = setup_full_dpos_test(generator, events, A, {},
                                           {
                                             {A[1], 0x001}, {A[2], 0x002},
                                             {A[3], 0x003}, {A[4], 0x004},
                                             {A[5], 0x005}, {A[6], 0x006},
                                             {A[7], 0x007},
                                           },
                                           {
                                             // 1, 2, 4, 5, 7 are top:
                                             std::make_tuple(A[0], MK_COINS(600), make_votes({0x001, 0x002, 0x004, 0x005, 0x007})),
                                             // 3 has not enough votes:
                                             MT(A[1], MK_COINS(300), 0x003),
                                             // 6 has no votes
                                           });
  
  // test is just that those votes go through. 600 guarantees two inputs required, and this early in the
  // blockchain it will be more than 2% of total coins generated so order will matter (first maxes out, 2nd is ineffective)
  return true;
  
  TEST_NEW_END();
}

bool gen_dpos_limit_votes::generate(std::vector<test_event_entry>& events) const
{
  // this test depends on two txin_votes in one tx being applied & unapplied properly
  TEST_NEW_BEGIN();
  
  auto A = generate_accounts(14);
  
  test_generator generator;
  auto blk_last_pow = setup_full_dpos_test(generator, events, {A[0]},
                                           {
                                             {A[1], MK_COINS(5)},
                                             {A[2], MK_COINS(5)},
                                             {A[3], MK_COINS(5)},
                                             {A[4], MK_COINS(5)},
                                             {A[5], MK_COINS(5)},
                                             {A[6], MK_COINS(5)},
                                             {A[7], MK_COINS(5)},
                                             {A[8], MK_COINS(70)},
                                             {A[9], MK_COINS(60)},
                                             {A[10], MK_COINS(50)},
                                             {A[11], MK_COINS(40)},
                                             {A[12], MK_COINS(30)},
                                             {A[13], MK_COINS(10)},
                                             {A[13], MK_COINS(10)},
                                           },
                                           {
                                             {A[1], 0x001}, {A[2], 0x002},
                                             {A[3], 0x003}, {A[4], 0x004},
                                             {A[5], 0x005}, {A[6], 0x006},
                                             {A[7], 0x007},
                                           },
                                           {
                                             // 1, 2, 3, 4, 5 are top
                                             MT(A[8], MK_COINS(70), 0x001),
                                             MT(A[9], MK_COINS(60), 0x002),
                                             MT(A[10], MK_COINS(50), 0x003),
                                             MT(A[11], MK_COINS(40), 0x004),
                                             MT(A[12], MK_COINS(30), 0x005),
                                           });
  
  // can't vote for more than 'n' delegates at a time
  CHECK_AND_ASSERT_MES(config::dpos_num_delegates == 5, false, "Test expects 5 delegates");
  
  DO_CALLBACK(events, "mark_invalid_tx");
  make_vote_tx(events, MK_COINS(10), make_votes({1, 2, 3, 4, 5, 6}),
               A[13], blk_last_pow);
  
  // can vote for 'n' though
  auto tx = make_vote_tx(events, MK_COINS(10), make_votes({1, 2, 3, 4, 5}),
                         A[13], blk_last_pow);
  
  auto blk_pos_1 = make_next_pos_block(generator, events, blk_last_pow, A[1], 0x001, tx);
  
  return true;
  
  TEST_NEW_END();
}

bool gen_dpos_change_votes::generate(std::vector<test_event_entry>& events) const
{
  TEST_NEW_BEGIN();
  
  auto A = generate_accounts(14);
  
  test_generator generator;
  auto blk_last_pow = setup_full_dpos_test(generator, events, {A[0]},
                                           {
                                             {A[1], MK_COINS(5)},
                                             {A[2], MK_COINS(5)},
                                             {A[3], MK_COINS(5)},
                                             {A[4], MK_COINS(5)},
                                             {A[5], MK_COINS(5)},
                                             {A[6], MK_COINS(5)},
                                             {A[7], MK_COINS(5)},
                                             {A[8], MK_COINS(50)},
                                             {A[9], MK_COINS(100)},
                                             {A[10], MK_COINS(4)},
                                             {A[11], MK_COINS(3)},
                                             {A[12], MK_COINS(2)},
                                             {A[13], MK_COINS(1)},
                                           },
                                           {
                                             {A[1], 0x001}, {A[2], 0x002},
                                             {A[3], 0x003}, {A[4], 0x004},
                                             {A[5], 0x005}, {A[6], 0x006},
                                             {A[7], 0x007},
                                           },
                                           {
                                             MT(A[8], MK_COINS(50), 0x001),
                                             MT(A[9], MK_COINS(100), 0x002),
                                             MT(A[10], MK_COINS(4), 0x003),
                                             MT(A[11], MK_COINS(3), 0x004),
                                             MT(A[12], MK_COINS(2), 0x005),
                                             MT(A[13], MK_COINS(1), 0x006),
                                           });
  
  // delegates in vote order: [2, 1, 3, 4, 5], 6
  auto blk_pos_1 = make_next_pos_blocks(generator, events, blk_last_pow,
                                        {A[1], A[2], A[3], A[4], A[5], A[1]},
                                        {1, 2, 3, 4, 5, 1});

  do_debug_mark(events, "change_vote_1");
  // A[8] changes vote from 1 to '6':
  auto blk_pos_2 = make_next_pos_block(generator, events, blk_pos_1, A[2], 2,
                                       make_vote_tx(events, MK_COINS(50), make_votes(6), A[8],
                                                    blk_pos_1, 1));
  do_debug_mark(events, "start_next_round_1");
  // delegates are now: [2, 6, 3, 4, 5], 1
  auto blk_pos_3 = make_next_pos_blocks(generator, events, blk_pos_2,
                                        {A[3], A[4], A[5], A[6], A[2], A[3]},
                                        {3, 4, 5, 6, 2, 3});
  do_debug_mark(events, "change_vote_2");
  // A[9] changes vote from 2 to 7:
  auto blk_pos_4 = make_next_pos_block(generator, events, blk_pos_3, A[4], 4,
                                       make_vote_tx(events, MK_COINS(50), make_votes(7), A[9],
                                                    blk_pos_3, 1));
  do_debug_mark(events, "start_next_round_2");
  // now: [7, 6, 3, 4, 5], 1/2
  auto blk_pos_5 = make_next_pos_blocks(generator, events, blk_pos_4,
                                        {A[5], A[6], A[7], A[3], A[4], A[5]},
                                        {5, 6, 7, 3, 4, 5});
  
  return true;
  
  TEST_NEW_END();
}

bool gen_dpos_spend_votes::generate(std::vector<test_event_entry>& events) const
{
  TEST_NEW_BEGIN();
  
  auto A = generate_accounts(15);
  
  test_generator generator;
  auto blk_last_pow = setup_full_dpos_test(generator, events, {A[0]},
                                           {
                                             {A[1], MK_COINS(5)},
                                             {A[2], MK_COINS(5)},
                                             {A[3], MK_COINS(5)},
                                             {A[4], MK_COINS(5)},
                                             {A[5], MK_COINS(5)},
                                             {A[6], MK_COINS(5)},
                                             {A[7], MK_COINS(5)},
                                             {A[8], MK_COINS(7)},
                                             {A[9], MK_COINS(6)},
                                             {A[10], MK_COINS(5)},
                                             {A[11], MK_COINS(4)},
                                             {A[12], MK_COINS(3)},
                                             {A[13], MK_COINS(2)},
                                             {A[14], MK_COINS(1)},
                                           },
                                           {
                                             {A[1], 0x001}, {A[2], 0x002},
                                             {A[3], 0x003}, {A[4], 0x004},
                                             {A[5], 0x005}, {A[6], 0x006},
                                             {A[7], 0x007},
                                           },
                                           {
                                             MT(A[8], MK_COINS(7), 0x001),
                                             MT(A[9], MK_COINS(6), 0x002),
                                             MT(A[10], MK_COINS(5), 0x003),
                                             MT(A[11], MK_COINS(4), 0x004),
                                             MT(A[12], MK_COINS(3), 0x005),
                                             MT(A[13], MK_COINS(2), 0x006),
                                             MT(A[14], MK_COINS(1), 0x007),
                                           });
  
  // delegates in vote order: [1, 2, 3, 4, 5], 6, 7
  auto blk_pos_1 = make_next_pos_blocks(generator, events, blk_last_pow,
                                        {A[1], A[2], A[3], A[4], A[5], A[1], A[2]},
                                        {1, 2, 3, 4, 5, 1, 2});

  do_debug_mark(events, "spend_vote_1");
  // A[10] spends his vote on '3' -> [1, 2, 4, 5, 6], 7, 3. '3' still does this block
  auto blk_pos_2 = make_next_pos_block(generator, events, blk_pos_1, A[3], 3,
                                       make_tx_send(events, A[10], A[2], 100, blk_pos_1));
  do_debug_mark(events, "start_next_round_1");
  auto blk_pos_3 = make_next_pos_blocks(generator, events, blk_pos_2,
                                        {A[4], A[5], A[6], A[1], A[2]},
                                        {4, 5, 6, 1, 2});
  do_debug_mark(events, "spend_vote_2");
  // A[8] spends his vote on '1' -> [2, 4, 5, 6, 7], 3/1
  auto blk_pos_4 = make_next_pos_block(generator, events, blk_pos_3, A[4], 4,
                                       make_tx_send(events, A[8], A[2], 49, blk_pos_3));
  do_debug_mark(events, "start_next_round_2");
  auto blk_pos_5 = make_next_pos_blocks(generator, events, blk_pos_4,
                                        {A[5], A[6], A[7], A[2], A[4], A[5]},
                                        {5, 6, 7, 2, 4, 5});
  
  return true;
  
  TEST_NEW_END();
}

bool gen_dpos_vote_tiebreaker::generate(std::vector<test_event_entry>& events) const
{
  TEST_NEW_BEGIN();
  
  auto A = generate_accounts(8);
  
  test_generator generator;
  auto blk_last_pow = setup_full_dpos_test(generator, events, {A[0]},
                                           {
                                             {A[1], MK_COINS(5)},
                                             {A[2], MK_COINS(5)},
                                             {A[3], MK_COINS(5)},
                                             {A[4], MK_COINS(5)},
                                             {A[5], MK_COINS(5)},
                                             {A[6], MK_COINS(5)},
                                             {A[7], MK_COINS(5)},
                                           },
                                           {
                                             {A[1], 0x001}, {A[2], 0x002},
                                             {A[3], 0x003}, {A[4], 0x004},
                                             {A[5], 0x005}, {A[6], 0x006},
                                             {A[7], 0x007},
                                           },
                                           {
                                           });
  
  // all delegates have 0 votes, so the winners should be the ones with the
  // largest addresses in lexicographical order.
  std::vector<int> delegate_indices = {1, 2, 3, 4, 5, 6, 7};
  std::sort(delegate_indices.begin(), delegate_indices.end(),
            [&A](int a, int b) {
              return A[a].get_public_address_str() < A[b].get_public_address_str();
            });
  std::reverse(delegate_indices.begin(), delegate_indices.end());
  std::set<int> top_delegate_indices;
  for (int i=0; i < 5; i++) {
    top_delegate_indices.insert(delegate_indices[i]);
  }
  
  std::vector<account_base> ordered_accounts;
  std::vector<delegate_id_t> ordered_delegates;
  
  for (int i=1; i < 8; i++)
  {
    if (top_delegate_indices.count(i) > 0)
    {
      ordered_accounts.push_back(A[i]);
      ordered_delegates.push_back((delegate_id_t)i);
    }
  }
  
  auto blk_pos_1 = make_next_pos_blocks(generator, events, blk_last_pow,
                                        ordered_accounts, ordered_delegates);
  auto blk_pos_2 = make_next_pos_blocks(generator, events, blk_pos_1,
                                        ordered_accounts, ordered_delegates);
  auto blk_pos_3 = make_next_pos_blocks(generator, events, blk_pos_2,
                                        ordered_accounts, ordered_delegates);
  
  return true;
  
  TEST_NEW_END();
}

#define CHECK_ASSERT_EQ(a, b) CHECK_AND_ASSERT_MES(a == b, false, #a" != "#b" <--> " << a << " != " << b);
#define CHECK_ASSERT_EQ_MES(a, b, mes) CHECK_AND_ASSERT_MES(a == b, false, #a" != "#b" <--> " << a << " != " << b << ": " << mes);

void check_missed_processed_blocks(std::vector<test_event_entry>& events, delegate_id_t d_id,
                                   uint64_t ex_proc, uint64_t ex_missed)
{
  do_callback_func(events, [=](core_t& core, size_t ev_index) {
    bs_delegate_info info;
    CHECK_AND_ASSERT(core_tester(core).get_delegate_info(d_id, info), false);
    CHECK_ASSERT_EQ_MES(info.processed_blocks, ex_proc, "For delegate " << d_id);
    CHECK_ASSERT_EQ_MES(info.missed_blocks, ex_missed, "For delegate " << d_id);
    return true;
  });
}

bool gen_dpos_delegate_timeout::generate(std::vector<test_event_entry>& events) const
{
  TEST_NEW_BEGIN();

  auto A = generate_accounts(5);
  
  test_generator generator;
  auto blk_last_pow = setup_full_dpos_test(generator, events, {A[0]},
                                           {
                                             {A[1], MK_COINS(100)},
                                             {A[2], MK_COINS(100)},
                                           },
                                           {
                                             {A[1], 0x001},
                                             {A[2], 0x002}
                                           },
                                           {
                                           });
  
  // A[1] misses first block
  g_ntp_time.apply_manual_delta(DPOS_DELEGATE_SLOT_TIME + 1);
  
  auto blk_pos_1 = make_next_pos_blocks(generator, events, blk_last_pow, {A[2], A[1]}, {0x002, 0x001});
  
  check_missed_processed_blocks(events, 0x001, 1, 0); // first block misses/hits aren't counted, so A[1]'s first miss
  check_missed_processed_blocks(events, 0x002, 0, 0); // and A[2]'s first hit are not counted
  
  // A[2] misses their chance:
  g_ntp_time.apply_manual_delta(DPOS_DELEGATE_SLOT_TIME + 1);
  do_callback(events, "mark_invalid_block");
  make_next_pos_block(generator, events, blk_pos_1, A[2], 0x002);
  // A[1] goes, A[2] is after them again
  auto blk_pos_2 = make_next_pos_blocks(generator, events, blk_pos_1, {A[1], A[2]}, {0x001, 0x002});
  
  check_missed_processed_blocks(events, 0x001, 2, 0);
  check_missed_processed_blocks(events, 0x002, 1, 1);
  
  return true;
  
  TEST_NEW_END();
}

bool gen_dpos_delegate_timeout_2::generate(std::vector<test_event_entry>& events) const
{
  TEST_NEW_BEGIN();

  auto A = generate_accounts(6);
  
  test_generator generator;
  auto blk_last_pow = setup_full_dpos_test(generator, events, {A[0]},
                                           {
                                             {A[1], MK_COINS(100)},
                                             {A[2], MK_COINS(100)},
                                             {A[3], MK_COINS(100)},
                                             {A[4], MK_COINS(100)},
                                             {A[5], MK_COINS(100)},
                                           },
                                           {
                                             {A[1], 0x001},
                                             {A[2], 0x002},
                                             {A[3], 0x003},
                                             {A[4], 0x004},
                                             {A[5], 0x005},
                                           },
                                           {
                                           });
  
  auto blk_pos_1 = make_next_pos_blocks(generator, events, blk_last_pow,
                                        {A[1], A[2], A[3], A[4], A[5]}, {1, 2, 3, 4, 5});
  
  // everyone misses one
  g_ntp_time.apply_manual_delta(DPOS_DELEGATE_SLOT_TIME*5 + 1);
  
  auto blk_pos_2 = make_next_pos_blocks(generator, events, blk_pos_1, {A[1]}, {1});
  
  check_missed_processed_blocks(events, 0x001, 1, 1); // processed 2, first block skipped
  check_missed_processed_blocks(events, 0x002, 1, 1);
  check_missed_processed_blocks(events, 0x003, 1, 1);
  check_missed_processed_blocks(events, 0x004, 1, 1);
  check_missed_processed_blocks(events, 0x005, 1, 1);
  
  // miss two rounds + 2 people
  g_ntp_time.apply_manual_delta(DPOS_DELEGATE_SLOT_TIME*12 + 1);
  auto blk_pos_3 = make_next_pos_blocks(generator, events, blk_pos_2, {A[4]}, {4});
  
  check_missed_processed_blocks(events, 0x001, 1, 3);
  check_missed_processed_blocks(events, 0x002, 1, 4);
  check_missed_processed_blocks(events, 0x003, 1, 4);
  check_missed_processed_blocks(events, 0x004, 2, 3);
  check_missed_processed_blocks(events, 0x005, 1, 3);
  
  // rollback
  do_debug_mark(events, "rewind to no missed");
  g_ntp_time.apply_manual_delta(-(DPOS_DELEGATE_SLOT_TIME*17 + 2));
  auto blk_pos_2b = make_next_pos_blocks(generator, events, blk_pos_1,
                                         {A[1], A[2], A[3], A[4], A[5]}, {1, 2, 3, 4, 5});

  check_missed_processed_blocks(events, 0x001, 1, 0);
  check_missed_processed_blocks(events, 0x002, 2, 0);
  check_missed_processed_blocks(events, 0x003, 2, 0);
  check_missed_processed_blocks(events, 0x004, 2, 0);
  check_missed_processed_blocks(events, 0x005, 2, 0);
  
  // re-do delta so it's not in the future when the test is run
  g_ntp_time.apply_manual_delta(DPOS_DELEGATE_SLOT_TIME*17 + 2);
  
  return true;
  
  TEST_NEW_END();
}

bool gen_dpos_timestamp_checks::generate(std::vector<test_event_entry>& events) const
{
  TEST_NEW_BEGIN();
  
  auto A = generate_accounts(5);
  
  test_generator generator;
  auto blk_last_pow = setup_full_dpos_test(generator, events, {A[0]},
                                           {
                                             {A[1], MK_COINS(100)},
                                             {A[2], MK_COINS(100)},
                                           },
                                           {
                                             {A[1], 0x001},
                                             {A[2], 0x002}
                                           },
                                           {
                                           });
  
  auto blk_pos_1 = make_next_pos_blocks(generator, events, blk_last_pow, {A[1], A[2]}, {0x001, 0x002});
  
  // make_next_pos_block adds CRYPTONOTE_DPOS_BLOCK_MINIMUM_BLOCK_SPACING to ntp_time each time..
  // make block with timestamp before pos block = invalid
  g_ntp_time.apply_manual_delta(-(CRYPTONOTE_DPOS_BLOCK_MINIMUM_BLOCK_SPACING + 5));
  do_callback(events, "mark_invalid_block");
  make_next_pos_block(generator, events, blk_pos_1, A[1], 0x001);
  g_ntp_time.apply_manual_delta(CRYPTONOTE_DPOS_BLOCK_MINIMUM_BLOCK_SPACING + 5 - CRYPTONOTE_DPOS_BLOCK_MINIMUM_BLOCK_SPACING); // undo delta
  // make block with less than the minimum block spacing
  g_ntp_time.apply_manual_delta(-2);
  do_callback(events, "mark_invalid_block");
  make_next_pos_block(generator, events, blk_pos_1, A[1], 0x001);
  g_ntp_time.apply_manual_delta(2 - CRYPTONOTE_DPOS_BLOCK_MINIMUM_BLOCK_SPACING); // undo delta
  // make block which will be in the future
  g_ntp_time.apply_manual_delta(CRYPTONOTE_DPOS_BLOCK_FUTURE_TIME_LIMIT*3);
  do_callback(events, "mark_invalid_block");
  make_next_pos_block(generator, events, blk_pos_1, A[1], 0x001);
  g_ntp_time.apply_manual_delta(-CRYPTONOTE_DPOS_BLOCK_FUTURE_TIME_LIMIT*3);
  
  return true;
  
  TEST_NEW_END();
}

// TODO: test can't register/vote with non-XPB coin types

bool gen_dpos_invalid_votes::generate(std::vector<test_event_entry>& events) const
{
  TEST_NEW_BEGIN();
  
  auto A = generate_accounts(5);
  
  test_generator generator;
  auto blk_last_pow = setup_full_dpos_test(generator, events, {A[0]},
                                           {
                                             {A[1], MK_COINS(100)},
                                             {A[2], MK_COINS(100)},
                                             {A[3], MK_COINS(5)},
                                             {A[4], MK_COINS(5)},
                                           },
                                           {
                                             {A[1], 0x001},
                                             {A[2], 0x002}
                                           },
                                           {
                                           });
  
  auto blk_pos_1 = make_next_pos_blocks(generator, events, blk_last_pow, {A[1], A[2]}, {0x001, 0x002});
  // A[3] can't vote with invalid seq (needs 0)
  do_callback(events, "mark_invalid_tx");
  make_vote_tx(events, MK_COINS(5), make_votes(0x001), A[3], blk_pos_1, 1);
  do_callback(events, "mark_invalid_tx");
  make_vote_tx(events, MK_COINS(5), make_votes(0x001), A[3], blk_pos_1, 5);
  
  // A[3] votes for A[1]
  auto blk_pos_2 = make_next_pos_block(generator, events, blk_pos_1, A[1], 0x001,
                                       make_vote_tx(events, MK_COINS(5), make_votes(0x001),
                                                    A[3], blk_pos_1, 0));

  // A[3] can't change vote with invalid seq (needs seq 1)
  do_callback(events, "mark_invalid_tx");
  make_vote_tx(events, MK_COINS(5), make_votes(0x002), A[3], blk_pos_1, 0);
  do_callback(events, "mark_invalid_tx");
  make_vote_tx(events, MK_COINS(5), make_votes(0x002), A[3], blk_pos_1, 2);
  auto blk_pos_3 = make_next_pos_block(generator, events, blk_pos_2, A[2], 0x002,
                                       make_vote_tx(events, MK_COINS(5), make_votes(0x002),
                                                    A[3], blk_pos_2, 1));
  
  // A[4] spends
  events.push_back(dont_mark_spent_tx());
  auto txspend = make_tx_send(events, A[4], A[4], MK_COINS(4), blk_pos_3);
  auto blk_pos_4 = make_next_pos_block(generator, events, blk_pos_3, A[1], 0x001, txspend);
  // A[4] can't vote with spent key
  do_callback(events, "mark_invalid_tx");
  make_vote_tx(events, MK_COINS(5), make_votes(0x002), A[4], blk_pos_1, 0);
  
  // A[3] can't vote twice in one tx
  do_callback(events, "mark_invalid_tx");
  {
    tx_builder txb;
    txb.init();
    
    std::vector<tx_source_entry> sources;
    CHECK_AND_ASSERT_MES(fill_tx_sources(sources, events, blk_pos_4, A[3], MK_COINS(5), 0, CP_XPB),
                         false, "Couldn't fill sources");
    CHECK_AND_ASSERT_MES(txb.add_vote(A[3].get_keys(), sources, 2, make_votes(0x001)), false, "could not add_vote");
    CHECK_AND_ASSERT_MES(txb.add_vote(A[3].get_keys(), sources, 3, make_votes(0x001)), false, "could not add_vote");
    CHECK_AND_ASSERT_MES(txb.finalize(), false, "Couldn't finalize");
    transaction vote_tx;
    CHECK_AND_ASSERT_MES(txb.get_finalized_tx(vote_tx), false, "Couldn't get finalized tx");
    
    events.push_back(vote_tx);
  }
  
  // A[3] can't spend & vote or vote & spend in same tx
  do_callback(events, "mark_invalid_tx");
  {
    tx_builder txb;
    txb.init();
    
    std::vector<tx_source_entry> sources;
    std::vector<tx_destination_entry> destinations;
    if (!fill_tx_sources_and_destinations(events, blk_pos_4, A[3], A[3], MK_COINS(4), 0, 0, sources, destinations, CP_XPB))
      return false;
    
    CHECK_AND_ASSERT_MES(txb.add_send(A[3].get_keys(), sources, destinations), false, "could not add_send");
    CHECK_AND_ASSERT_MES(txb.add_vote(A[3].get_keys(), sources, 2, make_votes(0x001)), false, "could not add_vote");
    CHECK_AND_ASSERT_MES(txb.finalize(), false, "Couldn't finalize");
    transaction vote_tx;
    CHECK_AND_ASSERT_MES(txb.get_finalized_tx(vote_tx), false, "Couldn't get finalized tx");
    events.push_back(vote_tx);
  }
  
  do_callback(events, "mark_invalid_tx");
  {
    tx_builder txb;
    txb.init();
    
    std::vector<tx_source_entry> sources;
    std::vector<tx_destination_entry> destinations;
    if (!fill_tx_sources_and_destinations(events, blk_pos_4, A[3], A[3], MK_COINS(4), 0, 0, sources, destinations, CP_XPB))
      return false;
    
    CHECK_AND_ASSERT_MES(txb.add_vote(A[3].get_keys(), sources, 2, make_votes(0x001)), false, "could not add_vote");
    CHECK_AND_ASSERT_MES(txb.add_send(A[3].get_keys(), sources, destinations), false, "could not add_send");
    CHECK_AND_ASSERT_MES(txb.finalize(), false, "Couldn't finalize");
    transaction vote_tx;
    CHECK_AND_ASSERT_MES(txb.get_finalized_tx(vote_tx), false, "Couldn't get finalized tx");
    events.push_back(vote_tx);
  }
  
  return true;
  
  TEST_NEW_END();
}

bool gen_dpos_receives_fees::generate(std::vector<test_event_entry>& events) const
{
  TEST_NEW_BEGIN();
  
  auto A = generate_accounts(5);
  
  test_generator generator;
  auto blk_last_pow = setup_full_dpos_test(generator, events, {A[0]},
                                           {
                                             {A[1], MK_COINS(5)},
                                             {A[2], MK_COINS(5)},
                                             {A[3], MK_COINS(301)},
                                           },
                                           {
                                             {A[1], 0x001},
                                             {A[2], 0x002}
                                           },
                                           {
                                           }); // height 32
  
  auto blk_pos_1 = make_next_pos_blocks(generator, events, blk_last_pow, {A[1], A[2]}, {0x001, 0x002}); // height 34
  
  LOG_PRINT_L0("Balances pre-huge-fee:");
  for (int i=0; i < 5; i++)
  {
    LOG_PRINT_L0("  A[" << i << "]: " << get_balance(events, A[i], blk_pos_1));
  }

  // A[1] and A[2] have 0 money here
  // A[3] sends a huge-fee tx
  auto blk_pos_2 = make_next_pos_block(generator, events, blk_pos_1, A[1], 0x001,
                                       make_tx_send(events, A[3], A[3], MK_COINS(1), blk_pos_1,
                                                     MK_COINS(300))); // height 35
  
  LOG_PRINT_L0("Balances post-huge-fee 1:");
  for (int i=0; i < 5; i++)
  {
    LOG_PRINT_L0("  A[" << i << "]: " << get_balance(events, A[i], blk_pos_2));
  }
  
  // height 35 means 36 blocks (height + genesis), 3 default-fee sends, 1 300-fee send
  uint64_t avg_fee = (TESTS_DEFAULT_FEE * 3 + MK_COINS(300)) / 36;
  LOG_PRINT_L0("Average fee should be " << avg_fee);
  
  auto blk_pos_3 = make_next_pos_block(generator, events, blk_pos_2, A[2], 0x002); // height 36
  
  LOG_PRINT_L0("Balances post-huge-fee 2:");
  for (int i=0; i < 5; i++)
  {
    LOG_PRINT_L0("  A[" << i << "]: " << get_balance(events, A[i], blk_pos_3));
  }
  
  // rewind 10 blocks (mined_money_unlock_window)
  auto blk_pos_4 = make_next_pos_blocks(generator, events, blk_pos_3,
                                        {A[1], A[2], A[1], A[2], A[1], A[2], A[1], A[2], A[1], A[2]},
                                        {1, 2, 1, 2, 1, 2, 1, 2, 1, 2});
  
  LOG_PRINT_L0("Balances post-rewind:");
  for (int i=0; i < 5; i++)
  {
    LOG_PRINT_L0("  A[" << i << "]: " << get_balance(events, A[i], blk_pos_4));
  }
  
  // so A[2] should be able to spend avg_fee they got from blk_pos_3
  auto blk_pos_5 = make_next_pos_block(generator, events, blk_pos_4, A[1], 0x001,
                                       make_tx_send(events, A[2], A[4], avg_fee - TESTS_DEFAULT_FEE, blk_pos_4));
  
  return true;
  
  TEST_NEW_END();
}

bool gen_dpos_altchain_voting_2::generate(std::vector<test_event_entry>& events) const
{
  TEST_NEW_BEGIN();
  
  auto A = generate_accounts(10);
  
  test_generator generator;
  auto blk_last_pow = setup_full_dpos_test(generator, events, {A[0]},
                                           {
                                             {A[1], MK_COINS(5)},
                                             {A[2], MK_COINS(5)},
                                             {A[3], MK_COINS(100)},
                                             {A[4], MK_COINS(200)},
                                           },
                                           {
                                             {A[1], 0x001},
                                             {A[2], 0x002}
                                           },
                                           {
                                           });
  // don't switch to DPOS - makes the max limit change, makes it more interesting
  set_dpos_switch_block(events, 100);
  
  do_debug_mark(events, "A[3] vote for A[1]");
  auto blk_pos_1 = make_next_block(generator, events, blk_last_pow, A[0],
                                   make_vote_tx(events, MK_COINS(100), make_votes(0x001),
                                                A[3], blk_last_pow, 0));
  do_debug_mark(events, "A[4] vote for A[1] (hits max)");
  auto blk_pos_2 = make_next_block(generator, events, blk_pos_1, A[0],
                                   make_vote_tx(events, MK_COINS(200), make_votes(0x001),
                                                A[4], blk_pos_1, 0));
  do_debug_mark(events, "A[3] is spent");
  auto blk_pos_3 = make_next_block(generator, events, blk_pos_2, A[0],
                                   make_tx_send(events, A[3], A[5], MK_COINS(30), blk_pos_2));
  do_debug_mark(events, "A[4] is spent");
  auto blk_pos_4 = make_next_block(generator, events, blk_pos_3, A[0],
                                   make_tx_send(events, A[4], A[5], MK_COINS(63), blk_pos_3));
  
  do_debug_mark(events, "rewind to A[3] spend");
  auto blk_alt1 = rewind_blocks(generator, events, blk_pos_3, A[0], 2);
  do_debug_mark(events, "rewind to A[4] vote for A[1]");
  auto blk_alt2 = rewind_blocks(generator, events, blk_pos_2, A[0], 4);
  do_debug_mark(events, "rewind to A[3] vote for A[1]");
  auto blk_alt3 = rewind_blocks(generator, events, blk_pos_1, A[0], 6);
  do_debug_mark(events, "rewind to no votes");
  auto blk_alt4 = rewind_blocks(generator, events, blk_last_pow, A[0], 8);
  
  return true;
  
  TEST_NEW_END();
}

bool gen_dpos_altchain_voting_3::generate(std::vector<test_event_entry>& events) const
{
  TEST_NEW_BEGIN();
  
  auto A = generate_accounts(10);
  
  test_generator generator;
  auto blk_last_pow = setup_full_dpos_test(generator, events, {A[0]},
                                           {
                                             {A[1], MK_COINS(5)},
                                             {A[2], MK_COINS(5)},
                                             {A[3], MK_COINS(250)},
                                             {A[4], MK_COINS(250)},
                                             {A[5], MK_COINS(250)},
                                           },
                                           {
                                             {A[1], 0x001},
                                             {A[2], 0x002}
                                           },
                                           {
                                           });
  // don't switch to DPOS - makes the max limit change, makes it more interesting
  set_dpos_switch_block(events, 100);
  
  do_debug_mark(events, "A[3] vote for A[1] - hits max");
  auto blk_pos_1 = make_next_block(generator, events, blk_last_pow, A[0],
                                   make_vote_tx(events, MK_COINS(250), make_votes(0x001),
                                                A[3], blk_last_pow, 0));
  do_debug_mark(events, "A[4] vote for A[1] - hits max again");
  auto blk_pos_2 = make_next_block(generator, events, blk_pos_1, A[0],
                                   make_vote_tx(events, MK_COINS(250), make_votes(0x001),
                                                A[4], blk_pos_1, 0));
  do_debug_mark(events, "A[5] vote for A[1] - hits max again");
  auto blk_pos_3 = make_next_block(generator, events, blk_pos_2, A[0],
                                   make_vote_tx(events, MK_COINS(250), make_votes(0x001),
                                                A[5], blk_pos_2, 0));

  do_debug_mark(events, "A[4] is spent");
  auto blk_pos_4 = make_next_block(generator, events, blk_pos_3, A[0],
                                   make_tx_send(events, A[4], A[6], MK_COINS(30), blk_pos_3));
  do_debug_mark(events, "A[5] is spent");
  auto blk_pos_5 = make_next_block(generator, events, blk_pos_4, A[0],
                                   make_tx_send(events, A[5], A[6], MK_COINS(63), blk_pos_4));
  do_debug_mark(events, "A[3] is spent");
  auto blk_pos_6 = make_next_block(generator, events, blk_pos_5, A[0],
                                   make_tx_send(events, A[3], A[6], MK_COINS(63), blk_pos_5));
  
  do_debug_mark(events, "rewind to A[5] spend");
  rewind_blocks(generator, events, blk_pos_5, A[0], 2);
  do_debug_mark(events, "rewind to A[4] spend");
  rewind_blocks(generator, events, blk_pos_4, A[0], 4);
  do_debug_mark(events, "rewind to A[5] vote");
  rewind_blocks(generator, events, blk_pos_3, A[0], 6);
  do_debug_mark(events, "rewind to A[4] vote");
  rewind_blocks(generator, events, blk_pos_2, A[0], 8);
  do_debug_mark(events, "rewind to A[3] vote");
  rewind_blocks(generator, events, blk_pos_1, A[0], 10);
  do_debug_mark(events, "rewind to no votes");
  rewind_blocks(generator, events, blk_last_pow, A[0], 12);
  
  return true;
  
  TEST_NEW_END();
}

bool gen_dpos_altchain_voting_4::generate(std::vector<test_event_entry>& events) const
{
  TEST_NEW_BEGIN();
  
  auto A = generate_accounts(10);
  
  test_generator generator;
  auto blk_last_pow = setup_full_dpos_test(generator, events, {A[0]},
                                           {
                                             {A[1], MK_COINS(5)},
                                             {A[2], MK_COINS(5)},
                                             {A[3], MK_COINS(250)},
                                             {A[4], MK_COINS(250)},
                                             {A[5], MK_COINS(250)},
                                           },
                                           {
                                             {A[1], 0x001},
                                             {A[2], 0x002}
                                           },
                                           {
                                           });
  // don't switch to DPOS - makes the max limit change, makes it more interesting
  set_dpos_switch_block(events, 100);
  
  do_debug_mark(events, "A[3] vote for A[1] - hits max");
  auto blk_pos_1 = make_next_block(generator, events, blk_last_pow, A[0],
                                   make_vote_tx(events, MK_COINS(250), make_votes(0x001),
                                                A[3], blk_last_pow, 0));
  do_debug_mark(events, "A[4] vote for A[1] - hits max again");
  auto blk_pos_2 = make_next_block(generator, events, blk_pos_1, A[0],
                                   make_vote_tx(events, MK_COINS(250), make_votes(0x001),
                                                A[4], blk_pos_1, 0));
  do_debug_mark(events, "A[3] change vote to 0x002");
  auto blk_pos_3 = make_next_block(generator, events, blk_pos_2, A[0],
                                   make_vote_tx(events, MK_COINS(250), make_votes(0x002),
                                                A[3], blk_pos_2, 1));
  do_debug_mark(events, "A[3] change vote back to 0x001");
  auto blk_pos_4 = make_next_block(generator, events, blk_pos_3, A[0],
                                   make_vote_tx(events, MK_COINS(250), make_votes(0x002),
                                                A[3], blk_pos_3, 2));
  
  do_debug_mark(events, "A[3] change vote to 0x002, A[5] vote for A[1], A[4] spent, in same tx");
  transaction vote_spend_spend;
  {
    tx_builder txb;
    txb.init();
    
    {
      std::vector<tx_source_entry> sources;
      CHECK_AND_ASSERT_MES(fill_tx_sources(sources, events, blk_pos_4, A[3], MK_COINS(250), 0, CP_XPB),
                           false, "Couldn't fill sources");
      CHECK_AND_ASSERT_MES(txb.add_vote(A[3].get_keys(), sources, 3, make_votes(0x002)), false, "could not add_vote");
    }
    {
      std::vector<tx_source_entry> sources;
      CHECK_AND_ASSERT_MES(fill_tx_sources(sources, events, blk_pos_4, A[5], MK_COINS(250), 0, CP_XPB),
                           false, "Couldn't fill sources");
      CHECK_AND_ASSERT_MES(txb.add_vote(A[5].get_keys(), sources, 0, make_votes(0x001)), false, "could not add_vote");
    }
    {
      std::vector<tx_source_entry> sources;
      std::vector<tx_destination_entry> destinations;
      if (!fill_tx_sources_and_destinations(events, blk_pos_4, A[4], A[6], MK_COINS(15), 0, 0, sources, destinations, CP_XPB))
        return false;
      CHECK_AND_ASSERT_MES(txb.add_send(A[4].get_keys(), sources, destinations), false, "could not add_send");
    }
    
    CHECK_AND_ASSERT_MES(txb.finalize(), false, "Couldn't finalize");
    CHECK_AND_ASSERT_MES(txb.get_finalized_tx(vote_spend_spend), false, "Couldn't get finalized tx");
    events.push_back(vote_spend_spend);
  }
  
  auto blk_pos_5 = make_next_block(generator, events, blk_pos_4, A[0], vote_spend_spend);
  do_debug_mark(events, "A[3] spend");
  auto blk_pos_6 = make_next_block(generator, events, blk_pos_5, A[0],
                                   make_tx_send(events, A[3], A[6], MK_COINS(3), blk_pos_5));

  do_debug_mark(events, "rewind to no votes");
  rewind_blocks(generator, events, blk_last_pow, A[0], 8);
  
  return true;
  
  TEST_NEW_END();
}

bool gen_dpos_speed_test::generate(std::vector<test_event_entry>& events) const
{
  TEST_NEW_BEGIN();
  
  auto A = generate_accounts(3);

  test_generator generator;
  auto blk_last_pow = setup_full_dpos_test(generator, events, {A[0]},
                                           {
                                             {A[1], MK_COINS(5)},
                                             {A[2], MK_COINS(50)},
                                           },
                                           {
                                             {A[1], 0x001},
                                           },
                                           {
                                             MT(A[2], MK_COINS(50), 0x001),
                                           });
  
  // remove superfluous tests, only care about blockchain speed
  SET_EVENT_VISITOR_SETT(events, event_visitor_settings::set_check_can_create_block_from_template, false);
  do_callback_func(events, [](core_t& c, size_t ev_index) {
    config::test_serialize_unserialize_block = false;
    return true;
  });
  
  block last_block = blk_last_pow;
  
  // 10000 blocks
  for (int i=0; i < 10000; i++) {
    LOG_PRINT_L0("make_next_pos_block no fee calc");
    
    g_ntp_time.apply_manual_delta(CRYPTONOTE_DPOS_BLOCK_MINIMUM_BLOCK_SPACING);
    
    cryptonote::block blk;
    if (!generator.construct_block_pos(blk, last_block, A[1], 0x001, 0))
    {
      throw test_gen_error("Failed to make_next_pos_block");
    }
    events.push_back(blk);
    last_block = blk;
  }
  
  return true;
  
  TEST_NEW_END();
}

#endif
