// Copyright (c) 2014 The Pebblecoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#pragma once

#include "cryptonote_core/tx_builder.h"

#include "chaingen.h"
#include "test_chain_unit_base.h"

struct currency_base : public test_chain_unit_base
{
  currency_base()
  : m_invalid_tx_index(0)
  , m_invalid_block_index(0)
  {
    REGISTER_CALLBACK_METHOD(currency_base, mark_invalid_tx);
    REGISTER_CALLBACK_METHOD(currency_base, mark_invalid_block);
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

#define MAKE_MINT_TX_FULL(VEC_EVENTS, CURRENCY_ID, DESCRIPTION, AMOUNT, DEST_ACC, TX_NAME, DECIMALS, REMINT_KEY, MOD) \
    cryptonote::transaction TX_NAME; \
    {                                \
        tx_builder txb; \
        txb.init();     \
        \
        std::vector<tx_destination_entry> destinations; \
        destinations.push_back(tx_destination_entry(coin_type(CURRENCY_ID, NotContract, BACKED_BY_N_A), \
                                                    AMOUNT, DEST_ACC.get_keys().m_account_address)); \
        txb.add_mint(CURRENCY_ID, DESCRIPTION, AMOUNT, DECIMALS, REMINT_KEY, destinations); \
        txb.finalize(MOD); \
        CHECK_AND_ASSERT_MES(txb.get_finalized_tx(TX_NAME), false, "Unable to get finalized mint tx"); \
        VEC_EVENTS.push_back(TX_NAME); \
    }
#define MAKE_MINT_TX(VEC_EVENTS, CURRENCY_ID, DESCRIPTION, AMOUNT, DEST_ACC, TX_NAME, DECIMALS, REMINT_KEY) MAKE_MINT_TX_FULL(VEC_EVENTS, CURRENCY_ID, DESCRIPTION, AMOUNT, DEST_ACC, TX_NAME, DECIMALS, REMINT_KEY, tools::identity());

#define MAKE_REMINT_TX_FULL(VEC_EVENTS, CURRENCY_ID, AMOUNT, DEST_ACC, TX_NAME, SECRET_KEY, NEW_REMINT_KEY, MOD) \
    cryptonote::transaction TX_NAME; \
    {  \
        tx_builder txb; \
        txb.init();     \
        \
        std::vector<tx_destination_entry> destinations; \
        destinations.push_back(tx_destination_entry(coin_type(CURRENCY_ID, NotContract, BACKED_BY_N_A), \
                                                    AMOUNT, DEST_ACC.get_keys().m_account_address)); \
        txb.add_remint(CURRENCY_ID, AMOUNT, SECRET_KEY, NEW_REMINT_KEY, destinations); \
        txb.finalize(MOD); \
        CHECK_AND_ASSERT_MES(txb.get_finalized_tx(TX_NAME), false, "Unable to get finalized remint tx"); \
        VEC_EVENTS.push_back(TX_NAME); \
    }

#define MAKE_REMINT_TX(VEC_EVENTS, CURRENCY_ID, AMOUNT, DEST_ACC, TX_NAME, SECRET_KEY, NEW_REMINT_KEY) MAKE_REMINT_TX_FULL(VEC_EVENTS, CURRENCY_ID, AMOUNT, DEST_ACC, TX_NAME, SECRET_KEY, NEW_REMINT_KEY, tools::identity());

DEFINE_TEST(gen_currency_mint, currency_base);
DEFINE_TEST(gen_currency_mint_many, currency_base);
DEFINE_TEST(gen_currency_invalid_amount_0, currency_base);
DEFINE_TEST(gen_currency_invalid_currency_0, currency_base);
DEFINE_TEST(gen_currency_invalid_currency_70, currency_base);
DEFINE_TEST(gen_currency_invalid_currency_255, currency_base);
DEFINE_TEST(gen_currency_invalid_large_description, currency_base);
DEFINE_TEST(gen_currency_invalid_reuse_currency_1, currency_base);
DEFINE_TEST(gen_currency_invalid_reuse_currency_2, currency_base);
DEFINE_TEST(gen_currency_invalid_reuse_description_1, currency_base);
DEFINE_TEST(gen_currency_invalid_reuse_description_2, currency_base);
DEFINE_TEST(gen_currency_invalid_remint_key, currency_base);
DEFINE_TEST(gen_currency_invalid_spend_more_than_mint, currency_base);
DEFINE_TEST(gen_currency_ok_spend_less_than_mint, currency_base);
DEFINE_TEST(gen_currency_spend_currency, currency_base);
DEFINE_TEST(gen_currency_cant_spend_other_currency, currency_base);
DEFINE_TEST(gen_currency_spend_currency_mix, currency_base);
DEFINE_TEST(gen_currency_remint_valid, currency_base);
DEFINE_TEST(gen_currency_remint_invalid_amount_0, currency_base);
DEFINE_TEST(gen_currency_remint_invalid_unremintable, currency_base);
DEFINE_TEST(gen_currency_remint_invalid_currency, currency_base);
DEFINE_TEST(gen_currency_remint_invalid_new_remint_key, currency_base);
DEFINE_TEST(gen_currency_remint_invalid_signature, currency_base);
DEFINE_TEST(gen_currency_remint_invalid_spend_more_than_remint, currency_base);
DEFINE_TEST(gen_currency_remint_twice, currency_base);
DEFINE_TEST(gen_currency_remint_change_key_old_invalid, currency_base);
DEFINE_TEST(gen_currency_remint_change_key_remint, currency_base);
DEFINE_TEST(gen_currency_remint_can_remint_twice_per_block, currency_base);
DEFINE_TEST(gen_currency_remint_cant_mint_remint_same_block, currency_base);
DEFINE_TEST(gen_currency_remint_limit_uint64max, currency_base);

