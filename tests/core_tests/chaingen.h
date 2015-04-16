// Copyright (c) 2012-2013 The Cryptonote developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#pragma once

#include <stdint.h>
#include <vector>
#include <iostream>

#include <boost/archive/binary_oarchive.hpp>
#include <boost/archive/binary_iarchive.hpp>
#include <boost/program_options.hpp>
#include <boost/serialization/vector.hpp>
#include <boost/serialization/variant.hpp>

#include "include_base_utils.h"
#include "misc_language.h"

#include "common/boost_serialization_helper.h"
#include "common/command_line.h"
#include "common/functional.h"
#include "common/types.h"
#include "common/ntp_time.h"
#include "cryptonote_config.h"
#include "cryptonote_core/account_boost_serialization.h"
#include "cryptonote_core/cryptonote_basic.h"
#include "cryptonote_core/cryptonote_basic_impl.h"
#include "cryptonote_core/cryptonote_format_utils.h"
#include "cryptonote_core/cryptonote_core.h"
#include "cryptonote_core/cryptonote_boost_serialization.h"
#include "cryptonote_core/core_tester.h"
#include "cryptonote_core/construct_tx_mod.h"

namespace concolor
{
  inline std::basic_ostream<char, std::char_traits<char> >& bright_white(std::basic_ostream<char, std::char_traits<char> >& ostr)
  {
    epee::log_space::set_console_color(epee::log_space::console_color_white, true);
    return ostr;
  }

  inline std::basic_ostream<char, std::char_traits<char> >& red(std::basic_ostream<char, std::char_traits<char> >& ostr)
  {
    epee::log_space::set_console_color(epee::log_space::console_color_red, true);
    return ostr;
  }

  inline std::basic_ostream<char, std::char_traits<char> >& green(std::basic_ostream<char, std::char_traits<char> >& ostr)
  {
    epee::log_space::set_console_color(epee::log_space::console_color_green, true);
    return ostr;
  }

  inline std::basic_ostream<char, std::char_traits<char> >& magenta(std::basic_ostream<char, std::char_traits<char> >& ostr)
  {
    epee::log_space::set_console_color(epee::log_space::console_color_magenta, true);
    return ostr;
  }

  inline std::basic_ostream<char, std::char_traits<char> >& yellow(std::basic_ostream<char, std::char_traits<char> >& ostr)
  {
    epee::log_space::set_console_color(epee::log_space::console_color_yellow, true);
    return ostr;
  }

  inline std::basic_ostream<char, std::char_traits<char> >& normal(std::basic_ostream<char, std::char_traits<char> >& ostr)
  {
    epee::log_space::reset_console_color();
    return ostr;
  }
}

extern tools::ntp_time g_ntp_time;

typedef boost::function<bool (core_t& c, size_t ev_index)> verify_callback_func;

struct callback_entry
{
  std::string callback_name;
  BEGIN_SERIALIZE_OBJECT()
    FIELD(callback_name)
  END_SERIALIZE()

private:
  friend class boost::serialization::access;

  template<class Archive>
  void serialize(Archive & ar, const unsigned int /*version*/)
  {
    ar & callback_name;
  }
};

struct callback_entry_func
{
  verify_callback_func cb;
  BEGIN_SERIALIZE_OBJECT()
    throw std::runtime_error("Can't epee serialize callback_entry_func");
  END_SERIALIZE()

private:
  friend class boost::serialization::access;

  template<class Archive>
  void serialize(Archive & ar, const unsigned int /*version*/)
  {
    throw std::runtime_error("Can't boost serialize callback_entry_func");
  }
};

struct dont_mark_spent_tx
{
  BEGIN_SERIALIZE_OBJECT()
  END_SERIALIZE()

private:
  friend class boost::serialization::access;
  
  template<class Archive>
  void serialize(Archive & ar, const unsigned int /*version*/)
  {
  }
};

struct set_dpos_switch_block_struct
{
  uint64_t block;
  BEGIN_SERIALIZE_OBJECT()
    FIELD(block);
  END_SERIALIZE()
  
  set_dpos_switch_block_struct() { }
  set_dpos_switch_block_struct(uint64_t block_in) : block(block_in) { }
  
private:
  friend class boost::serialization::access;
  
  template<class Archive>
  void serialize(Archive & ar, const unsigned int /*version*/)
  {
    ar & block;
  }
};

struct debug_mark
{
  std::string message;
  BEGIN_SERIALIZE_OBJECT()
    FIELD(message)
  END_SERIALIZE()

private:
  friend class boost::serialization::access;

  template<class Archive>
  void serialize(Archive & ar, const unsigned int /*version*/)
  {
    ar & message;
  }
};

struct register_delegate_account
{
  cryptonote::delegate_id_t delegate_id;
  cryptonote::account_base acct;
  BEGIN_SERIALIZE_OBJECT()
    FIELD(delegate_id)
    FIELD(acct)
  END_SERIALIZE()
  
private:
  friend class boost::serialization::access;
  
  template<class Archive>
  void serialize(Archive & ar, const unsigned int /*version*/)
  {
    ar & delegate_id;
    ar & acct;
  }
};

template<typename T>
struct serialized_object
{
  serialized_object() { }

  serialized_object(const cryptonote::blobdata& a_data)
    : data(a_data)
  {
  }

  cryptonote::blobdata data;
  BEGIN_SERIALIZE_OBJECT()
    FIELD(data)
    END_SERIALIZE()

private:
  friend class boost::serialization::access;

  template<class Archive>
  void serialize(Archive & ar, const unsigned int /*version*/)
  {
    ar & data;
  }
};

typedef serialized_object<cryptonote::block> serialized_block;
typedef serialized_object<cryptonote::transaction> serialized_transaction;

struct event_visitor_settings
{
  int valid_mask;
  bool txs_keeped_by_block;
  bool check_can_create_block_from_template;

  enum settings
  {
    set_txs_keeped_by_block = 1 << 0,
    set_check_can_create_block_from_template = 1 << 1
  };

  event_visitor_settings(int a_valid_mask = 0, bool a_txs_keeped_by_block = false,
                         bool a_check_can_create_block_from_template = true)
    : valid_mask(a_valid_mask)
    , txs_keeped_by_block(a_txs_keeped_by_block)
    , check_can_create_block_from_template(a_check_can_create_block_from_template)
  {
  }

private:
  friend class boost::serialization::access;

  template<class Archive>
  void serialize(Archive & ar, const unsigned int /*version*/)
  {
    ar & valid_mask;
    ar & txs_keeped_by_block;
  }
};

VARIANT_TAG(binary_archive, callback_entry, 0xcb);
VARIANT_TAG(binary_archive, cryptonote::account_base, 0xcc);
VARIANT_TAG(binary_archive, serialized_block, 0xcd);
VARIANT_TAG(binary_archive, serialized_transaction, 0xce);
VARIANT_TAG(binary_archive, event_visitor_settings, 0xcf);
VARIANT_TAG(binary_archive, dont_mark_spent_tx, 0xd0);
VARIANT_TAG(binary_archive, set_dpos_switch_block_struct, 0xd1);
VARIANT_TAG(binary_archive, debug_mark, 0xd2);
VARIANT_TAG(binary_archive, register_delegate_account, 0xd3);
VARIANT_TAG(binary_archive, callback_entry_func, 0xd4);

typedef boost::variant<cryptonote::block, cryptonote::transaction, cryptonote::account_base, callback_entry, serialized_block, serialized_transaction, event_visitor_settings, dont_mark_spent_tx, set_dpos_switch_block_struct, debug_mark, register_delegate_account, callback_entry_func> test_event_entry;

typedef std::unordered_map<crypto::hash, std::pair<const cryptonote::transaction*, bool> > map_hash2tx_isregular_t;

class test_chain_unit_base
{
public:
  typedef boost::function<bool (core_t& c, size_t ev_index, const std::vector<test_event_entry> &events)> verify_callback;
  typedef std::map<std::string, verify_callback> callbacks_map;

  void register_callback(const std::string& cb_name, verify_callback cb);
  bool verify(const std::string& cb_name, core_t& c, size_t ev_index, const std::vector<test_event_entry> &events);
private:
  callbacks_map m_callbacks;
};


class test_generator
{
public:
  struct block_info
  {
    block_info()
      : prev_id()
      , already_generated_coins(0)
      , block_size(0)
    {
    }

    block_info(crypto::hash a_prev_id, uint64_t an_already_generated_coins, size_t a_block_size)
      : prev_id(a_prev_id)
      , already_generated_coins(an_already_generated_coins)
      , block_size(a_block_size)
    {
    }

    crypto::hash prev_id;
    uint64_t already_generated_coins;
    size_t block_size;
  };

  enum block_fields
  {
    bf_none      = 0,
    bf_major_ver = 1 << 0,
    bf_minor_ver = 1 << 1,
    bf_timestamp = 1 << 2,
    bf_prev_id   = 1 << 3,
    bf_miner_tx  = 1 << 4,
    bf_tx_hashes = 1 << 5,
    bf_diffic    = 1 << 6
  };

  void get_block_chain(std::vector<block_info>& blockchain, const crypto::hash& head, size_t n) const;
  void get_last_n_block_sizes(std::vector<size_t>& block_sizes, const crypto::hash& head, size_t n) const;
  uint64_t get_already_generated_coins(const crypto::hash& blk_id) const;
  uint64_t get_already_generated_coins(const cryptonote::block& blk) const;

  bool add_block(const cryptonote::block& blk, size_t tsx_size, std::vector<size_t>& block_sizes, uint64_t already_generated_coins);
  bool construct_block(cryptonote::block& blk, uint64_t height, const crypto::hash& prev_id,
    const cryptonote::account_base& miner_acc, uint64_t timestamp, uint64_t already_generated_coins,
    std::vector<size_t>& block_sizes, const std::list<cryptonote::transaction>& tx_list,
    bool ignore_overflow_check=false);
  bool construct_block(cryptonote::block& blk, const cryptonote::account_base& miner_acc, uint64_t timestamp, bool ignore_overflow_check=false);
  bool construct_block(cryptonote::block& blk, const cryptonote::block& blk_prev, const cryptonote::account_base& miner_acc,
    const std::list<cryptonote::transaction>& tx_list = std::list<cryptonote::transaction>(), bool ignore_overflow_check=false);

  bool construct_block_manually(cryptonote::block& blk, const cryptonote::block& prev_block,
    const cryptonote::account_base& miner_acc, int actual_params = bf_none, uint8_t major_ver = 0,
    uint8_t minor_ver = 0, uint64_t timestamp = 0, const crypto::hash& prev_id = crypto::hash(),
    const cryptonote::difficulty_type& diffic = 1, const cryptonote::transaction& miner_tx = cryptonote::transaction(),
    const std::vector<crypto::hash>& tx_hashes = std::vector<crypto::hash>(), size_t txs_sizes = 0);
  bool construct_block_manually_tx(cryptonote::block& blk, const cryptonote::block& prev_block,
    const cryptonote::account_base& miner_acc, const std::vector<crypto::hash>& tx_hashes, size_t txs_size);

  bool construct_block_pos(cryptonote::block& blk, const cryptonote::block& blk_prev,
                           const cryptonote::account_base& staker_acc,
                           const cryptonote::delegate_id_t& delegate_id,
                           uint64_t avg_fee,
                           const std::list<cryptonote::transaction>& tx_list=std::list<cryptonote::transaction>());
private:
  std::unordered_map<crypto::hash, block_info> m_blocks_info;
};

cryptonote::currency_map get_inputs_amount(const std::vector<cryptonote::tx_source_entry> &s);

inline cryptonote::difficulty_type get_test_difficulty() {return 1;}
bool fill_nonce(cryptonote::block& blk, const cryptonote::difficulty_type& diffic, uint64_t height);

void get_confirmed_txs(const std::vector<cryptonote::block>& blockchain, const map_hash2tx_isregular_t& mtx, map_hash2tx_isregular_t& confirmed_txs);
bool find_block_chain(const std::vector<test_event_entry>& events, std::vector<cryptonote::block>& blockchain,
                      map_hash2tx_isregular_t& mtx, const crypto::hash& head);
bool fill_tx_sources(std::vector<cryptonote::tx_source_entry>& sources, const std::vector<test_event_entry>& events,
                     const cryptonote::block& blk_head, const cryptonote::account_base& from, uint64_t amount, size_t nmix,
                     cryptonote::coin_type cp, bool ignore_unlock_times=false);
bool fill_tx_sources_and_destinations(const std::vector<test_event_entry>& events, const cryptonote::block& blk_head,
                                      const cryptonote::account_base& from, const cryptonote::account_base& to,
                                      uint64_t amount, uint64_t fee, size_t nmix,
                                      std::vector<cryptonote::tx_source_entry>& sources,
                                      std::vector<cryptonote::tx_destination_entry>& destinations,
                                      uint64_t currency=CURRENCY_XPB, bool ignore_unlock_times=false);
bool fill_tx_sources_and_destinations(const std::vector<test_event_entry>& events, const cryptonote::block& blk_head,
                                      const cryptonote::account_base& from, const cryptonote::account_base& to,
                                      uint64_t amount, uint64_t fee, size_t nmix,
                                      std::vector<cryptonote::tx_source_entry>& sources,
                                      std::vector<cryptonote::tx_destination_entry>& destinations,
                                      cryptonote::coin_type cp=cryptonote::CP_XPB, bool ignore_unlock_times=false);

bool construct_miner_tx_manually(size_t height, uint64_t already_generated_coins,
                                 const cryptonote::account_public_address& miner_address, cryptonote::transaction& tx,
                                 uint64_t fee, cryptonote::keypair* p_txkey = 0);

template <class tx_mapper_t>
bool construct_tx_to_key(const std::vector<test_event_entry>& events, cryptonote::transaction& tx,
                         const cryptonote::block& blk_head, const cryptonote::account_base& from,
                         const cryptonote::account_base& to,
                         uint64_t amount, uint64_t fee, size_t nmix, cryptonote::coin_type cp,
                         tx_mapper_t&& mod = tools::identity()) {
  std::vector<cryptonote::tx_source_entry> sources;
  std::vector<cryptonote::tx_destination_entry> destinations;
  if (!fill_tx_sources_and_destinations(events, blk_head, from, to, amount, fee, nmix, sources, destinations, cp))
    return false;
  
  for (size_t i=0; i < sources.size(); i++) {
    auto& s = sources[i];
    LOG_PRINT_L0("source " << i << ": (" << s.cp << ", " << s.amount_in << " --> " << s.amount_out << ")");
  }
  
  for (size_t i=0; i < destinations.size(); i++) {
    auto& d = destinations[i];
    LOG_PRINT_L0("dest   " << i << ": (" << d.cp << ", " << d.amount << ")");
  }
  
  cryptonote::keypair txkey;
  return construct_tx(from.get_keys(), sources, destinations, std::vector<uint8_t>(), tx, 0, txkey, mod);
}

cryptonote::transaction construct_tx_with_fee(std::vector<test_event_entry>& events, const cryptonote::block& blk_head,
                                            const cryptonote::account_base& acc_from, const cryptonote::account_base& acc_to,
                                            uint64_t amount, uint64_t fee);

uint64_t get_balance(const cryptonote::account_base& addr, const std::vector<cryptonote::block>& blockchain,
                     const map_hash2tx_isregular_t& mtx);
uint64_t get_balance(const std::vector<test_event_entry>& events, const cryptonote::account_base& addr,
                     const cryptonote::block& head);

//--------------------------------------------------------------------------
template<class t_test_class>
auto do_check_tx_verification_context(const cryptonote::tx_verification_context& tvc, bool tx_added, size_t event_index, const cryptonote::transaction& tx, t_test_class& validator, int)
  -> decltype(validator.check_tx_verification_context(tvc, tx_added, event_index, tx))
{
  return validator.check_tx_verification_context(tvc, tx_added, event_index, tx);
}
//--------------------------------------------------------------------------
template<class t_test_class>
bool do_check_tx_verification_context(const cryptonote::tx_verification_context& tvc, bool tx_added, size_t /*event_index*/, const cryptonote::transaction& /*tx*/, t_test_class&, long)
{
  // Default block verification context check
  if (tvc.m_verifivation_failed)
    throw std::runtime_error("Transaction verification failed");
  return true;
}
//--------------------------------------------------------------------------
template<class t_test_class>
bool check_tx_verification_context(const cryptonote::tx_verification_context& tvc, bool tx_added, size_t event_index, const cryptonote::transaction& tx, t_test_class& validator)
{
  // SFINAE in action
  return do_check_tx_verification_context(tvc, tx_added, event_index, tx, validator, 0);
}
//--------------------------------------------------------------------------
template<class t_test_class>
auto do_check_block_verification_context(const cryptonote::block_verification_context& bvc, size_t event_index, const cryptonote::block& blk, t_test_class& validator, int)
  -> decltype(validator.check_block_verification_context(bvc, event_index, blk))
{
  return validator.check_block_verification_context(bvc, event_index, blk);
}
//--------------------------------------------------------------------------
template<class t_test_class>
bool do_check_block_verification_context(const cryptonote::block_verification_context& bvc, size_t /*event_index*/, const cryptonote::block& /*blk*/, t_test_class&, long)
{
  // Default block verification context check
  if (bvc.m_verifivation_failed)
    throw std::runtime_error("Block verification failed");
  return true;
}
//--------------------------------------------------------------------------
template<class t_test_class>
bool check_block_verification_context(const cryptonote::block_verification_context& bvc, size_t event_index, const cryptonote::block& blk, t_test_class& validator)
{
  // SFINAE in action
  return do_check_block_verification_context(bvc, event_index, blk, validator, 0);
}

/************************************************************************/
/*                                                                      */
/************************************************************************/
template<class t_test_class>
struct push_core_event_visitor: public boost::static_visitor<bool>
{
private:
  core_t& m_c;
  const std::vector<test_event_entry>& m_events;
  t_test_class& m_validator;
  size_t m_ev_index;
  std::map<cryptonote::delegate_id_t, cryptonote::account_base> m_delegate_accts;

  bool m_txs_keeped_by_block;
  bool m_check_can_create_block_from_template;

public:
  push_core_event_visitor(core_t& c, const std::vector<test_event_entry>& events, t_test_class& validator)
    : m_c(c)
    , m_events(events)
    , m_validator(validator)
    , m_ev_index(0)
    , m_txs_keeped_by_block(false)
    , m_check_can_create_block_from_template(true)
  {
  }
  
  bool check_can_create_valid_mining_block() const
  {
    if (!m_check_can_create_block_from_template)
      return true;
    
    cryptonote::block b;
    cryptonote::account_base account;
    account.generate();

    uint64_t difficulty;
    uint64_t height;
    cryptonote::blobdata blob_reserve;
    
    bool dpos_block = cryptonote::config::in_pos_era(m_c.get_current_blockchain_height());
    
    if (dpos_block) { g_ntp_time.apply_manual_delta(CRYPTONOTE_DPOS_BLOCK_MINIMUM_BLOCK_SPACING); }
    LOG_PRINT_CYAN("check_can_create_valid_mining_block...", LOG_LEVEL_0);
    bool got_template = m_c.get_block_template(b, account.get_keys().m_account_address, difficulty, height, blob_reserve,
                                               dpos_block);
    if (dpos_block) { g_ntp_time.apply_manual_delta(-CRYPTONOTE_DPOS_BLOCK_MINIMUM_BLOCK_SPACING); }
    
    if (!got_template)
    {
      LOG_ERROR("check_can_create_valid_mining_block: Could not get_block_template");
      return false;
    }
    
    if (dpos_block)
    {
      CHECK_AND_ASSERT_MES(m_delegate_accts.count(b.signing_delegate_id) > 0, false,
                           "check_can_create_valid_mining_block: don't have registered account for delegate "
                           << b.signing_delegate_id);
      CHECK_AND_ASSERT_MES(sign_dpos_block(b, m_delegate_accts.find(b.signing_delegate_id)->second), false,
                           "check_can_create_valid_mining_block: Could not sign dpos block");
    }

    /*if (difficulty > 1)
    {
      LOG_PRINT_CYAN("NOT checking can_create_valid_mining_block, difficulty is too high", LOG_LEVEL_0);
      return true;
    }*/
    
    LOG_PRINT_CYAN("Adding block from get_block_template...", LOG_LEVEL_0);
    cryptonote::block_verification_context bvc = AUTO_VAL_INIT(bvc);
    if (dpos_block) { g_ntp_time.apply_manual_delta(CRYPTONOTE_DPOS_BLOCK_MINIMUM_BLOCK_SPACING); }
    bool r = m_c.handle_incoming_block(t_serializable_object_to_blob(b), bvc);
    if (dpos_block) { g_ntp_time.apply_manual_delta(-CRYPTONOTE_DPOS_BLOCK_MINIMUM_BLOCK_SPACING); }
    CHECK_AND_ASSERT_MES(r, false, "check_can_create_valid_mining_block: Could not handle_incoming_block");
    CHECK_AND_ASSERT_MES(!bvc.m_verifivation_failed, false,
                         "check_can_create_valid_mining_block: Block verification failed on block template (txpool error)");
    CHECK_AND_ASSERT_MES(bvc.m_added_to_main_chain, false, "check_can_create_valid_mining_block: didn't add to main chain");
    
    //remove it so future tests aren't affected
    LOG_PRINT_CYAN("Removing block from get_block_template...", LOG_LEVEL_0);
    cryptonote::core_tester ct(m_c);
    CHECK_AND_ASSERT_MES(ct.pop_block_from_blockchain(), false,
                         "check_can_create_valid_mining_block: Couldn't pop block from blockhain");
    LOG_PRINT_CYAN("Success in adding and removing block from get_block_template", LOG_LEVEL_0);
    
    return true;
  }

  void event_index(size_t ev_index)
  {
    m_ev_index = ev_index;
  }

  bool operator()(const event_visitor_settings& settings)
  {
    log_event("event_visitor_settings");

    if (settings.valid_mask & event_visitor_settings::set_txs_keeped_by_block)
    {
      m_txs_keeped_by_block = settings.txs_keeped_by_block;
    }
    else if (settings.valid_mask & event_visitor_settings::set_check_can_create_block_from_template)
    {
      m_check_can_create_block_from_template = settings.check_can_create_block_from_template;
    }

    return true;
  }

  bool operator()(const cryptonote::transaction& tx) const
  {
    log_event("cryptonote::transaction");

    cryptonote::tx_verification_context tvc = AUTO_VAL_INIT(tvc);
    size_t pool_size = m_c.get_pool_transactions_count();
    cryptonote::blobdata txblob;
    if (!t_serializable_object_to_blob(tx, txblob))
    {
      tvc.m_verifivation_failed = true;
    }
    else
    {
      m_c.handle_incoming_tx(txblob, tvc, m_txs_keeped_by_block);
    }
    bool tx_added = pool_size + 1 == m_c.get_pool_transactions_count();
    bool r = check_tx_verification_context(tvc, tx_added, m_ev_index, tx, m_validator);
    CHECK_AND_NO_ASSERT_MES(r, false, "tx verification context check failed");
    
    CHECK_AND_ASSERT(check_can_create_valid_mining_block(), false);
    
    return true;
  }

  bool operator()(const cryptonote::block& b) const
  {
    log_event("cryptonote::block");

    cryptonote::block_verification_context bvc = AUTO_VAL_INIT(bvc);
    cryptonote::blobdata bd;
    if (!t_serializable_object_to_blob(b, bd))
    {
      bvc.m_verifivation_failed = true;
    }
    else
    {
      m_c.handle_incoming_block(bd, bvc);
    }
    bool r = check_block_verification_context(bvc, m_ev_index, b, m_validator);
    CHECK_AND_NO_ASSERT_MES(r, false, "block verification context check failed");
    
    CHECK_AND_ASSERT(check_can_create_valid_mining_block(), false);
    
    return r;
  }

  bool operator()(const callback_entry& cb) const
  {
    log_event(std::string("callback_entry ") + cb.callback_name);
    return m_validator.verify(cb.callback_name, m_c, m_ev_index, m_events);
  }
  
  bool operator()(const callback_entry_func& cb) const
  {
    log_event(std::string("callback_entry_func "));
    return cb.cb(m_c, m_ev_index);
  }
  
  bool operator()(const debug_mark& dm) const
  {
    log_event(std::string("debug_mark: ") + dm.message);
    return true;
  }
  
  bool operator()(const dont_mark_spent_tx& dmst) const
  {
    log_event(std::string("dont_mark_spent_tx"));
    return true;
  }

  bool operator()(const set_dpos_switch_block_struct& sdsb) const
  {
    log_event(std::string("set_dpos_switch_block"));
    cryptonote::config::dpos_switch_block = sdsb.block;
    return true;
  }
  
  bool operator()(const register_delegate_account& rda)
  {
    log_event(std::string("register_delegate_account"));
    CHECK_AND_ASSERT_MES(m_delegate_accts.count(rda.delegate_id) == 0, false,
                         "Registering already-registered delegate id" << rda.delegate_id);
    m_delegate_accts[rda.delegate_id] = rda.acct;
    return true;
  }
  
  bool operator()(const cryptonote::account_base& ab) const
  {
    log_event("cryptonote::account_base");
    return true;
  }

  bool operator()(const serialized_block& sr_block) const
  {
    log_event("serialized_block");

    cryptonote::block_verification_context bvc = AUTO_VAL_INIT(bvc);
    m_c.handle_incoming_block(sr_block.data, bvc);

    cryptonote::block blk;
    std::stringstream ss;
    ss << sr_block.data;
    binary_archive<false> ba(ss);
    ::serialization::serialize(ba, blk);
    if (!ss.good())
    {
      blk = cryptonote::block();
    }
    bool r = check_block_verification_context(bvc, m_ev_index, blk, m_validator);
    CHECK_AND_NO_ASSERT_MES(r, false, "block verification context check failed");
    
    CHECK_AND_ASSERT(check_can_create_valid_mining_block(), false);
    
    return true;
  }

  bool operator()(const serialized_transaction& sr_tx) const
  {
    log_event("serialized_transaction");

    cryptonote::tx_verification_context tvc = AUTO_VAL_INIT(tvc);
    size_t pool_size = m_c.get_pool_transactions_count();
    m_c.handle_incoming_tx(sr_tx.data, tvc, m_txs_keeped_by_block);
    bool tx_added = pool_size + 1 == m_c.get_pool_transactions_count();

    cryptonote::transaction tx;
    std::stringstream ss;
    ss << sr_tx.data;
    binary_archive<false> ba(ss);
    ::serialization::serialize(ba, tx);
    if (!ss.good())
    {
      tx = cryptonote::transaction();
    }

    bool r = check_tx_verification_context(tvc, tx_added, m_ev_index, tx, m_validator);
    CHECK_AND_NO_ASSERT_MES(r, false, "transaction verification context check failed");
    
    CHECK_AND_ASSERT(check_can_create_valid_mining_block(), false);
    
    return true;
  }

private:
  void log_event(const std::string& event_type) const
  {
    std::cout << concolor::yellow << "=== EVENT # " << m_ev_index << ": " << event_type << concolor::normal << std::endl;
  }
};
//--------------------------------------------------------------------------
template<class t_test_class>
inline bool replay_events_through_core(core_t& cr, const std::vector<test_event_entry>& events, t_test_class& validator)
{
  TRY_ENTRY();

  //init core here

  CHECK_AND_ASSERT_MES(typeid(cryptonote::block) == events[0].type(), false, "First event must be genesis block creation");
  cr.set_genesis_block(boost::get<cryptonote::block>(events[0]));

  bool r = true;
  push_core_event_visitor<t_test_class> visitor(cr, events, validator);
  for(size_t i = 1; i < events.size() && r; ++i)
  {
    visitor.event_index(i);
    r = boost::apply_visitor(visitor, events[i]);
  }

  return r;

  CATCH_ENTRY_L0("replay_events_through_core", false);
}
//--------------------------------------------------------------------------
template<class t_test_class>
inline bool do_replay_events(std::vector<test_event_entry>& events)
{
  boost::program_options::options_description desc("Allowed options");
  core_t::init_options(desc);
  command_line::add_arg(desc, command_line::arg_data_dir);
  boost::program_options::variables_map vm;
  bool r = command_line::handle_error_helper(desc, [&]()
  {
    boost::program_options::store(boost::program_options::basic_parsed_options<char>(&desc), vm);
    boost::program_options::notify(vm);
    return true;
  });
  if (!r)
    return false;

  cryptonote::cryptonote_protocol_stub pr; //TODO: stub only for this kind of test, make real validation of relayed objects
  core_t c(&pr, g_ntp_time);
  if (!c.init(vm))
  {
    std::cout << concolor::magenta << "Failed to init core" << concolor::normal << std::endl;
    return false;
  }
  t_test_class validator;
  // reset switch block
  cryptonote::config::dpos_switch_block = 999999999;
  return replay_events_through_core<t_test_class>(c, events, validator);
}
//--------------------------------------------------------------------------
template<class t_test_class>
inline bool do_replay_file(const std::string& filename)
{
  std::vector<test_event_entry> events;
  if (!tools::unserialize_obj_from_file(events, filename))
  {
    std::cout << concolor::magenta << "Failed to deserialize data from file: " << filename << concolor::normal << std::endl;
    return false;
  }
  return do_replay_events<t_test_class>(events);
}
//--------------------------------------------------------------------------
#define GENERATE_ACCOUNT(account) \
    cryptonote::account_base account; \
    account.generate();

#define MAKE_ACCOUNT(VEC_EVENTS, account) \
  cryptonote::account_base account; \
  account.generate(); \
  VEC_EVENTS.push_back(account);

#define DO_CALLBACK(VEC_EVENTS, CB_NAME) \
{ \
  callback_entry CALLBACK_ENTRY; \
  CALLBACK_ENTRY.callback_name = CB_NAME; \
  VEC_EVENTS.push_back(CALLBACK_ENTRY); \
}

#define DONT_MARK_SPENT_TX(VEC_EVENTS) \
  VEC_EVENTS.push_back(dont_mark_spent_tx());

#define REGISTER_CALLBACK(CB_NAME, CLBACK) \
  register_callback(CB_NAME, boost::bind(&CLBACK, this, _1, _2, _3));

#define REGISTER_CALLBACK_METHOD(CLASS, METHOD) \
  register_callback(#METHOD, boost::bind(&CLASS::METHOD, this, _1, _2, _3));

#define MAKE_GENESIS_BLOCK(VEC_EVENTS, BLK_NAME, MINER_ACC, TS)                       \
  test_generator generator;                                                           \
  cryptonote::block BLK_NAME;                                                           \
  CHECK_AND_ASSERT_MES(generator.construct_block(BLK_NAME, MINER_ACC, TS), false, "Failed to make genesis block"); \
  VEC_EVENTS.push_back(BLK_NAME);

#define MAKE_NEXT_BLOCK(VEC_EVENTS, BLK_NAME, PREV_BLOCK, MINER_ACC)                  \
  cryptonote::block BLK_NAME;                                                           \
  CHECK_AND_ASSERT_MES(generator.construct_block(BLK_NAME, PREV_BLOCK, MINER_ACC), false, "Failed to MAKE_NEXT_BLOCK"); \
  VEC_EVENTS.push_back(BLK_NAME);

#define MAKE_STARTING_BLOCKS(VEC_EVENTS, BLK_NAME, MINER_ACC, TS)             \
  MAKE_GENESIS_BLOCK(VEC_EVENTS, BLK_NAME ## neg3, MINER_ACC, TS)                    \
  MAKE_NEXT_BLOCK(VEC_EVENTS, BLK_NAME ## neg2, BLK_NAME ## neg3, MINER_ACC)               \
  MAKE_NEXT_BLOCK(VEC_EVENTS, BLK_NAME ## neg1, BLK_NAME ## neg2, MINER_ACC)               \
  MAKE_NEXT_BLOCK(VEC_EVENTS, BLK_NAME, BLK_NAME ## neg1, MINER_ACC)                   \

#define MAKE_NEXT_BLOCK_TX1_IGNORECHECK(VEC_EVENTS, BLK_NAME, PREV_BLOCK, MINER_ACC, TX1, IGNORECHECK)         \
  cryptonote::block BLK_NAME;                                                           \
  {                                                                                   \
    std::list<cryptonote::transaction> tx_list;                                         \
    tx_list.push_back(TX1);                                                           \
    CHECK_AND_ASSERT_MES(generator.construct_block(BLK_NAME, PREV_BLOCK, MINER_ACC, tx_list, IGNORECHECK), false, "Failed to make_next_block_tx1");              \
  }                                                                                   \
  VEC_EVENTS.push_back(BLK_NAME);

#define MAKE_NEXT_BLOCK_TX1(VEC_EVENTS, BLK_NAME, PREV_BLOCK, MINER_ACC, TX1) MAKE_NEXT_BLOCK_TX1_IGNORECHECK(VEC_EVENTS, BLK_NAME, PREV_BLOCK, MINER_ACC, TX1, false);

#define MAKE_NEXT_BLOCK_TX_LIST(VEC_EVENTS, BLK_NAME, PREV_BLOCK, MINER_ACC, TXLIST)  \
  cryptonote::block BLK_NAME;                                                           \
  CHECK_AND_ASSERT_MES(generator.construct_block(BLK_NAME, PREV_BLOCK, MINER_ACC, TXLIST), false, "Failed to make_next_block_tx_list");                 \
  VEC_EVENTS.push_back(BLK_NAME);

#define REWIND_BLOCKS_N(VEC_EVENTS, BLK_NAME, PREV_BLOCK, MINER_ACC, COUNT)           \
  cryptonote::block BLK_NAME;                                                           \
  {                                                                                   \
    cryptonote::block blk_last = PREV_BLOCK;                                            \
    for (size_t i = 0; i < COUNT; ++i)                                                \
    {                                                                                 \
      MAKE_NEXT_BLOCK(VEC_EVENTS, blk, blk_last, MINER_ACC);                          \
      blk_last = blk;                                                                 \
    }                                                                                 \
    BLK_NAME = blk_last;                                                              \
  }

#define REWIND_BLOCKS(VEC_EVENTS, BLK_NAME, PREV_BLOCK, MINER_ACC) REWIND_BLOCKS_N(VEC_EVENTS, BLK_NAME, PREV_BLOCK, MINER_ACC, CRYPTONOTE_MINED_MONEY_UNLOCK_WINDOW)

#define MAKE_TX_MIX_CP_FEE_MOD(VEC_EVENTS, TX_NAME, FROM, TO, AMOUNT, NMIX, HEAD, CP, FEE, MOD)                \
  cryptonote::transaction TX_NAME;                                                             \
  CHECK_AND_ASSERT_MES(construct_tx_to_key(VEC_EVENTS, TX_NAME, HEAD, FROM, TO, AMOUNT, FEE, NMIX, CP, MOD), false, "Failed to construct_tx_to_key in make_tx_mix_cp"); \
  VEC_EVENTS.push_back(TX_NAME);

#define MAKE_TX_FULL(VEC_EVENTS, TX_NAME, FROM, TO, AMOUNT, NMIX, HEAD, CP, FEE, MOD)                       \
  cryptonote::transaction TX_NAME;                                                             \
  CHECK_AND_ASSERT_MES(construct_tx_to_key(VEC_EVENTS, TX_NAME, HEAD, FROM, TO, AMOUNT, FEE, NMIX, CP, MOD), false, "Failed to construct_tx_to_key in make_tx_mix_cp"); \
  VEC_EVENTS.push_back(TX_NAME);

#define MAKE_TX_MIX_CP_FEE(VEC_EVENTS, TX_NAME, FROM, TO, AMOUNT, NMIX, HEAD, CP, FEE) MAKE_TX_FULL(VEC_EVENTS, TX_NAME, FROM, TO, AMOUNT, NMIX, HEAD, CP, FEE, tools::identity())

#define MAKE_TX_MIX_CP(VEC_EVENTS, TX_NAME, FROM, TO, AMOUNT, NMIX, HEAD, CP) MAKE_TX_MIX_CP_FEE(VEC_EVENTS, TX_NAME, FROM, TO, AMOUNT, NMIX, HEAD, CP, CP == cryptonote::CP_XPB ? TESTS_DEFAULT_FEE : 0)

#define MAKE_TX_MIX_C(VEC_EVENTS, TX_NAME, FROM, TO, AMOUNT, NMIX, HEAD, CURRENCY) MAKE_TX_MIX_CP(VEC_EVENTS, TX_NAME, FROM, TO, AMOUNT, NMIX, HEAD, cryptonote::coin_type(CURRENCY, cryptonote::NotContract, BACKED_BY_N_A))

#define MAKE_TX_MIX(VEC_EVENTS, TX_NAME, FROM, TO, AMOUNT, NMIX, HEAD) MAKE_TX_MIX_C(VEC_EVENTS, TX_NAME, FROM, TO, AMOUNT, NMIX, HEAD, CURRENCY_XPB)

#define MAKE_TX_CP(VEC_EVENTS, TX_NAME, FROM, TO, AMOUNT, HEAD, CP) MAKE_TX_MIX_CP(VEC_EVENTS, TX_NAME, FROM, TO, AMOUNT, 0, HEAD, CP)

#define MAKE_TX_C(VEC_EVENTS, TX_NAME, FROM, TO, AMOUNT, HEAD, CURRENCY) MAKE_TX_MIX_C(VEC_EVENTS, TX_NAME, FROM, TO, AMOUNT, 0, HEAD, CURRENCY)

#define MAKE_TX(VEC_EVENTS, TX_NAME, FROM, TO, AMOUNT, HEAD) MAKE_TX_C(VEC_EVENTS, TX_NAME, FROM, TO, AMOUNT, HEAD, CURRENCY_XPB)

#define MAKE_TX_MIX_LIST(VEC_EVENTS, SET_NAME, FROM, TO, AMOUNT, NMIX, HEAD)             \
  {                                                                                      \
    cryptonote::transaction t;                                                             \
    CHECK_AND_ASSERT_MES(construct_tx_to_key(VEC_EVENTS, t, HEAD, FROM, TO, AMOUNT, TESTS_DEFAULT_FEE, NMIX, cryptonote::CP_XPB), false, "failed to construct_tx_to_key in make_tx_mix_list"); \
    SET_NAME.push_back(t);                                                               \
    VEC_EVENTS.push_back(t);                                                             \
  }

#define MAKE_TX_LIST(VEC_EVENTS, SET_NAME, FROM, TO, AMOUNT, HEAD) MAKE_TX_MIX_LIST(VEC_EVENTS, SET_NAME, FROM, TO, AMOUNT, 0, HEAD)

#define MAKE_TX_LIST_START(VEC_EVENTS, SET_NAME, FROM, TO, AMOUNT, HEAD) \
    std::list<cryptonote::transaction> SET_NAME; \
    MAKE_TX_LIST(VEC_EVENTS, SET_NAME, FROM, TO, AMOUNT, HEAD);

#define MAKE_MINER_TX_AND_KEY_MANUALLY(TX, BLK, KEY)                                                      \
  transaction TX;                                                                                         \
  if (!construct_miner_tx_manually(get_block_height(BLK) + 1, generator.get_already_generated_coins(BLK), \
    miner_account.get_keys().m_account_address, TX, 0, KEY))                                              \
    return false;

#define MAKE_MINER_TX_MANUALLY(TX, BLK) MAKE_MINER_TX_AND_KEY_MANUALLY(TX, BLK, 0)

#define SET_EVENT_VISITOR_SETT(VEC_EVENTS, SETT, VAL) VEC_EVENTS.push_back(event_visitor_settings(SETT, VAL, VAL));

#define GENERATE(filename, genclass) \
    { \
        std::vector<test_event_entry> events; \
        genclass g; \
        cryptonote::config::dpos_switch_block = 999999999; /*reset switch block*/ \
        g.generate(events); \
        if (!tools::serialize_obj_to_file(events, filename)) \
        { \
            std::cout << concolor::magenta << "Failed to serialize data to file: " << filename << concolor::normal << std::endl; \
            throw std::runtime_error("Failed to serialize data to file"); \
        } \
    }


#define PLAY(filename, genclass) \
    if(!do_replay_file<genclass>(filename)) \
    { \
      std::cout << concolor::magenta << "Failed to pass test : " << #genclass << concolor::normal << std::endl; \
      return 1; \
    }

#define GENERATE_AND_PLAY(genclass)                                                                        \
  {                                                                                                        \
    std::vector<test_event_entry> events;                                                                  \
    ++tests_count;                                                                                         \
    bool generated = false;                                                                                \
    try                                                                                                    \
    {                                                                                                      \
      std::cout << concolor::green << "#TEST# started " << #genclass << concolor::normal << '\n';          \
      genclass g;                                                                                          \
      cryptonote::config::dpos_switch_block = 999999999; /*reset switch block*/                            \
      generated = g.generate(events);;                                                                     \
    }                                                                                                      \
    catch (const std::exception& ex)                                                                       \
    {                                                                                                      \
      LOG_PRINT(#genclass << " generation failed: what=" << ex.what(), 0);                                 \
    }                                                                                                      \
    catch (...)                                                                                            \
    {                                                                                                      \
      LOG_PRINT(#genclass << " generation failed: generic exception", 0);                                  \
    }                                                                                                      \
    if (generated && do_replay_events< genclass >(events))                                                 \
    {                                                                                                      \
      std::cout << concolor::green << "#TEST# Succeeded " << #genclass << concolor::normal << '\n';        \
    }                                                                                                      \
    else                                                                                                   \
    {                                                                                                      \
      std::cout << concolor::magenta << "#TEST# Failed " << #genclass << concolor::normal << '\n';         \
      failed_tests.push_back(#genclass);                                                                   \
      if (stop_on_fail) throw std::runtime_error("Breaking early from test failure");                      \
    }                                                                                                      \
    std::cout << std::endl;                                                                                \
  }

#define CALL_TEST(test_name, function)                                                                     \
  {                                                                                                        \
    if(!function())                                                                                        \
    {                                                                                                      \
      std::cout << concolor::magenta << "#TEST# Failed " << test_name << concolor::normal << std::endl;    \
      return 1;                                                                                            \
    }                                                                                                      \
    else                                                                                                   \
    {                                                                                                      \
      std::cout << concolor::green << "#TEST# Succeeded " << test_name << concolor::normal << std::endl;   \
    }                                                                                                      \
  }

#define QUOTEME(x) #x
#define DEFINE_TESTS_ERROR_CONTEXT(text) const char* perr_context = text;
#define CHECK_TEST_CONDITION(cond) CHECK_AND_ASSERT_MES(cond, false, "[" << perr_context << "] failed: \"" << QUOTEME(cond) << "\"")
#define CHECK_EQ(v1, v2) CHECK_AND_ASSERT_MES(v1 == v2, false, "[" << perr_context << "] failed: \"" << QUOTEME(v1) << " == " << QUOTEME(v2) << "\", " << v1 << " != " << v2)
#define CHECK_NOT_EQ(v1, v2) CHECK_AND_ASSERT_MES(!(v1 == v2), false, "[" << perr_context << "] failed: \"" << QUOTEME(v1) << " != " << QUOTEME(v2) << "\", " << v1 << " == " << v2)
#define MK_COINS(amount) (UINT64_C(amount) * COIN)
#define TESTS_DEFAULT_FEE ((uint64_t)1) // pow(10, 4)

#define DEFINE_TEST(TEST_NAME, BASE_CLASS) \
  struct TEST_NAME : public BASE_CLASS \
  { \
    bool generate(std::vector<test_event_entry>& events) const; \
  };
//--------------------------------------------------------------------------

class test_gen_error : public std::runtime_error
{
public:
  test_gen_error(const std::string& s) : std::runtime_error(s) { }
};

cryptonote::account_base generate_account();
std::vector<cryptonote::account_base> generate_accounts(size_t n);
std::list<cryptonote::transaction> start_tx_list(const cryptonote::transaction& tx);
cryptonote::block make_genesis_block(test_generator& generator, std::vector<test_event_entry>& events,
                                     const cryptonote::account_base& miner_account, uint64_t timestamp);
cryptonote::block make_next_block(test_generator& generator, std::vector<test_event_entry>& events,
                                  const cryptonote::block& block_prev,
                                  const cryptonote::account_base& miner_account,
                                  const std::list<cryptonote::transaction>& tx_list=std::list<cryptonote::transaction>());
cryptonote::block make_next_pos_block(test_generator& generator, std::vector<test_event_entry>& events,
                                      const cryptonote::block& block_prev,
                                      const cryptonote::account_base& staker_account,
                                      const cryptonote::delegate_id_t& staker_id,
                                      const std::list<cryptonote::transaction>& tx_list=std::list<cryptonote::transaction>());
cryptonote::block make_next_pos_block(test_generator& generator, std::vector<test_event_entry>& events,
                                      const cryptonote::block& block_prev,
                                      const cryptonote::account_base& staker_account,
                                      const cryptonote::delegate_id_t& staker_id,
                                      const cryptonote::transaction& tx1);
cryptonote::block make_next_pos_blocks(test_generator& generator, std::vector<test_event_entry>& events,
                                       const cryptonote::block& block_prev,
                                       const std::vector<cryptonote::account_base>& staker_account,
                                       const std::vector<cryptonote::delegate_id_t>& staker_id);
cryptonote::block make_next_block(test_generator& generator, std::vector<test_event_entry>& events,
                                  const cryptonote::block& block_prev,
                                  const cryptonote::account_base& miner_account,
                                  const cryptonote::transaction& tx1);
cryptonote::block rewind_blocks(test_generator& generator, std::vector<test_event_entry>& events,
                                const cryptonote::block& block_prev,
                                const std::vector<cryptonote::account_base>& miner_accounts,
                                size_t n=CRYPTONOTE_MINED_MONEY_UNLOCK_WINDOW);
cryptonote::block rewind_blocks(test_generator& generator, std::vector<test_event_entry>& events,
                                const cryptonote::block& block_prev,
                                const cryptonote::account_base& miner_account,
                                size_t n=CRYPTONOTE_MINED_MONEY_UNLOCK_WINDOW);
cryptonote::transaction make_tx_send(std::vector<test_event_entry>& events,
                                     const cryptonote::account_base& from, const cryptonote::account_base& to,
                                     uint64_t amount, const cryptonote::block& head,
                                     uint64_t fee=TESTS_DEFAULT_FEE, const cryptonote::coin_type& cp=cryptonote::CP_XPB,
                                     uint64_t nmix=0);
void set_dpos_switch_block(std::vector<test_event_entry>& events, uint64_t block);
void do_callback(std::vector<test_event_entry>& events, const std::string& cb_name);
void do_callback_func(std::vector<test_event_entry>& events, const verify_callback_func& cb);
void do_debug_mark(std::vector<test_event_entry>& events, const std::string& msg);
void do_register_delegate_account(std::vector<test_event_entry>& events, cryptonote::delegate_id_t delegate_id,
                                  const cryptonote::account_base& acct);

#define TEST_NEW_BEGIN() try {

#define TEST_NEW_END() } catch (test_gen_error& e) { \
    LOG_ERROR("Failed to generate test: " << e.what()); \
    return false; \
  }

