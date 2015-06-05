// Copyright (c) 2014-2015 The Pebblecoin developers
// Copyright (c) 2012-2013 The Cryptonote developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#pragma once

#include <algorithm>

#include <boost/algorithm/string/join.hpp>
#include <boost/lexical_cast.hpp>
#include <boost/variant/get.hpp>

#include "include_base_utils.h"

#include "common/functional.h"
#include "crypto/crypto.h"
#include "crypto/hash.h"
#include "cryptonote_protocol/blobdatatype.h"

//#include "cryptonote_core/tx_builder.h"
//#include "cryptonote_core/cryptonote_basic_impl.h"
#include "cryptonote_core/account.h"
#include "cryptonote_core/keypair.h"
#include "cryptonote_core/coin_type.h"
#include "cryptonote_core/tx_extra.h"

namespace cryptonote
{
  class transaction_prefix;
  class transaction;
  struct block;
  struct txout_to_key;
  
  //---------------------------------------------------------------
  bool get_transaction_prefix_hash(const transaction_prefix& tx, crypto::hash& h);
  crypto::hash get_transaction_prefix_hash(const transaction_prefix& tx);
  bool parse_and_validate_tx_from_blob(const blobdata& tx_blob, transaction& tx, crypto::hash& tx_hash, crypto::hash& tx_prefix_hash);
  bool parse_and_validate_tx_from_blob(const blobdata& tx_blob, transaction& tx);
  bool construct_miner_tx(size_t height, size_t median_size, uint64_t already_generated_coins, size_t current_block_size, uint64_t fee, const account_public_address &miner_address, transaction& tx, const blobdata& extra_nonce = blobdata(), size_t max_outs = 1);
  
  bool get_block_prefix_hash(const block& b, crypto::hash& res);
  crypto::hash get_block_prefix_hash(const block& b);

  struct tx_source_entry
  {
    typedef std::pair<uint64_t, crypto::public_key> output_entry;

    enum source_type {
      InToKey,          // regular tx, spend an InToKey
      ResolveBacking,   // convert a backing coin of amount_in backed by a currency into amount_out coins of that currency
      ResolveContract   // convert a contract coin of amount_in backed by a currency into amount_out coins of that currency
    };

    std::vector<output_entry> outputs;  //index + key
    size_t real_output;                 //index in outputs vector of real output_entry
    crypto::public_key real_out_tx_key; //incoming real tx public key
    size_t real_output_in_tx_index;     //index in transaction outputs vector
    source_type type;
    coin_type cp;
    uint64_t amount_in;                 //money in the input
    uint64_t amount_out;                //money that can be spent (may be less than amount_in for ResolveBacking/ResolveContract)
    
    // for ResolveBacking/ResolveContract
    uint64_t contract_resolving;        //which contract is being resolved when resolving backing or contract coins
    
    tx_source_entry()
        : real_output(0), real_output_in_tx_index(0),
          type(InToKey), cp(CP_XPB),
          amount_in(0), amount_out(0), contract_resolving(CURRENCY_N_A) { }
  };

  struct tx_destination_entry
  {
    coin_type cp;
    uint64_t amount;                    //money
    account_public_address addr;        //destination address

    tx_destination_entry();
    tx_destination_entry(const coin_type& cp_in, uint64_t a, const account_public_address &ad);
  };

  //---------------------------------------------------------------
  bool construct_tx(const account_keys& sender_account_keys, const std::vector<tx_source_entry>& sources, const std::vector<tx_destination_entry>& destinations, std::vector<uint8_t> extra, transaction& tx, uint64_t unlock_time);
  
  template<typename T>
  bool find_tx_extra_field_by_type(const std::vector<tx_extra_field>& tx_extra_fields, T& field)
  {
    auto it = std::find_if(tx_extra_fields.begin(), tx_extra_fields.end(), [](const tx_extra_field& f) { return typeid(T) == f.type(); });
    if(tx_extra_fields.end() == it)
      return false;

    field = boost::get<T>(*it);
    return true;
  }

  bool parse_tx_extra(const std::vector<uint8_t>& tx_extra, std::vector<tx_extra_field>& tx_extra_fields);
  crypto::public_key get_tx_pub_key_from_extra(const std::vector<uint8_t>& tx_extra);
  crypto::public_key get_tx_pub_key_from_extra(const transaction& tx);
  bool add_tx_pub_key_to_extra(transaction& tx, const crypto::public_key& tx_pub_key);
  bool add_extra_nonce_to_tx_extra(std::vector<uint8_t>& tx_extra, const blobdata& extra_nonce);
  void set_payment_id_to_tx_extra_nonce(blobdata& extra_nonce, const crypto::hash& payment_id);
  bool get_payment_id_from_tx_extra_nonce(const blobdata& extra_nonce, crypto::hash& payment_id);
  bool is_out_to_acc(const account_keys& acc, const txout_to_key& out_key, const crypto::public_key& tx_pub_key, size_t output_index);
  bool lookup_acc_outs(const account_keys& acc, const transaction& tx, const crypto::public_key& tx_pub_key, std::vector<size_t>& outs, uint64_t& money_transfered);
  bool lookup_acc_outs(const account_keys& acc, const transaction& tx, std::vector<size_t>& outs, uint64_t& money_transfered);
  bool generate_key_image_helper(const account_keys& ack, const crypto::public_key& tx_public_key, size_t real_output_index, keypair& in_ephemeral, crypto::key_image& ki);
  void get_blob_hash(const blobdata& blob, crypto::hash& res);
  crypto::hash get_blob_hash(const blobdata& blob);
  std::string short_hash_str(const crypto::hash& h);

  crypto::hash get_transaction_hash(const transaction& t);
  bool get_transaction_hash(const transaction& t, crypto::hash& res);
  bool get_transaction_hash(const transaction& t, crypto::hash& res, size_t& blob_size);
  bool get_block_hash(const block& b, crypto::hash& res);
  crypto::hash get_block_hash(const block& b);
  bool get_block_longhash(const block& b, crypto::hash& res, uint64_t height, uint64_t **state, bool use_cache);
  bool generate_genesis_block(block& bl);
  bool parse_and_validate_block_from_blob(const blobdata& b_blob, block& b);
  currency_map get_outs_money_amount(const transaction& tx);
  bool add_amount_would_overflow(const uint64_t amount1, const uint64_t amount2);
  bool add_amount(uint64_t& amount1, const uint64_t amount2);
  bool sub_amount_would_underflow(const uint64_t amount1, const uint64_t amount2);
  bool sub_amount(uint64_t& amount1, const uint64_t amount2);
  bool check_inputs(const transaction& tx, currency_map& in);
  bool check_inputs(const transaction& tx);
  bool check_outputs(const transaction& tx, currency_map& out);
  bool check_outputs(const transaction& tx);
  bool check_inputs_outputs(const transaction& tx, currency_map& in, currency_map& out, uint64_t& fee);
  bool check_inputs_outputs(const transaction& tx, uint64_t& fee);
  bool check_inputs_outputs(const transaction& tx);
  uint64_t get_tx_fee(const transaction& tx);
  bool check_inputs_types_supported(const transaction& tx);
  bool check_outputs_types_supported(const transaction& tx);
  bool check_outs_valid(const transaction& tx);
  bool parse_amount(uint64_t& amount, const std::string& str_amount);
  
  uint64_t get_block_height(const block& b);
  std::vector<uint64_t> relative_output_offsets_to_absolute(const std::vector<uint64_t>& off);
  std::vector<uint64_t> absolute_output_offsets_to_relative(const std::vector<uint64_t>& off);
  std::string print_money(uint64_t amount, uint64_t decimals);
  std::string print_money(uint64_t amount);
  std::string print_grade_scale(uint64_t grade_scale);
  
  bool check_dpos_block_sig(const block& b, const account_public_address& delegate_addr);
  bool sign_dpos_block(block& b, const cryptonote::account_base& staker_acc);
  
  //---------------------------------------------------------------
  template<class t_currency_decimal_getter>
  std::string print_moneys(const currency_map& moneys, const t_currency_decimal_getter& getter)
  {
    std::vector<std::string> result;
    BOOST_FOREACH(const auto& item, moneys)
    {
      std::stringstream stream;
      stream << item.first;
      result.push_back("(" + stream.str() + ": "
                       + print_money(item.second, getter.currency_decimals(item.first))
                       + ")");
    }
    return boost::algorithm::join(result, ", ");
  }
  std::string print_moneys(const currency_map& moneys);
  //---------------------------------------------------------------
  template<class t_object>
  bool t_serializable_object_to_blob(const t_object& to, blobdata& b_blob)
  {
    std::stringstream ss;
    binary_archive<true> ba(ss);
    bool r = ::serialization::serialize(ba, to);
    b_blob = ss.str();
    return r;
  }
  //---------------------------------------------------------------
  template<class t_object>
  blobdata t_serializable_object_to_blob(const t_object& to)
  {
    blobdata b;
    if (!t_serializable_object_to_blob(to, b))
      throw std::runtime_error("Failed to t_serializable_object_to_blob");
    return b;
  }
  //---------------------------------------------------------------
  template<class t_object>
  bool t_serializable_object_from_blob(const blobdata& b_blob, t_object& to)
  {
    std::stringstream ss;
    ss << b_blob;
    binary_archive<false> ba(ss);
    bool r = ::serialization::serialize(ba, to);
    CHECK_AND_ASSERT_MES(r, false, "Failed to parse object from blob");
    return r;
  }
  //---------------------------------------------------------------
  template<class t_object>
  size_t get_object_blobsize(const t_object& o)
  {
    blobdata b = t_serializable_object_to_blob(o);
    return b.size();
  }
  //---------------------------------------------------------------
  template<class t_object>
  bool get_object_hash(const t_object& o, crypto::hash& res, size_t& blob_size)
  {
    blobdata bl;
    if (!t_serializable_object_to_blob(o, bl))
      return false;
    
    blob_size = bl.size();
    get_blob_hash(bl, res);
    return true;
  }
  //---------------------------------------------------------------
  template<class t_object>
  bool get_object_hash(const t_object& o, crypto::hash& res)
  {
    size_t blob_size;
    return get_object_hash(o, res, blob_size);
  }
  //---------------------------------------------------------------
  template <typename T>
  std::string obj_to_json_str(const T& obj)
  {
    std::stringstream ss;
    json_archive<true> ar(ss, true);
    bool r = ::serialization::serialize(ar, obj);
    CHECK_AND_ASSERT_MES(r, "", "obj_to_json_str failed: serialization::serialize returned false");
    return ss.str();
  }
  //---------------------------------------------------------------
  blobdata block_to_blob(const block& b);
  bool block_to_blob(const block& b, blobdata& b_blob);
  blobdata tx_to_blob(const transaction& b);
  bool tx_to_blob(const transaction& b, blobdata& b_blob);
  //---------------------------------------------------------------
  bool is_pow_block(const block& b);
  bool is_pos_block(const block& b);
  //---------------------------------------------------------------
#define CHECKED_GET_SPECIFIC_VARIANT(variant_var, specific_type, variable_name, fail_return_val) \
  CHECK_AND_ASSERT_MES(variant_var.type() == typeid(specific_type), fail_return_val, "wrong variant type: " << variant_var.type().name() << ", expected " << typeid(specific_type).name()); \
  specific_type& variable_name = boost::get<specific_type>(variant_var);
  //------------------------------------------------------------------
  // sort and give nth item after, in order
  // e.g. given [10, 4, 0, 13], sorts as [0, 4, 10, 13], then:
  //   nth_sorted_item_after(10, 0) -> 10
  //   nth_sorted_item_after(10, 1) -> 13
  //   nth_sorted_item_after(10, 2) -> 0
  //   nth_sorted_item_after(10, 3) -> 4
  //   nth_sorted_item_after(10, 5) -> 10
  //   nth_sorted_item_after(11, 0) -> 13
  //   nth_sorted_item_after(11, 1) -> 0
  //   nth_sorted_item_after(11, 2) -> 4
  //   nth_sorted_item_after(14, 0) -> 0
  //   nth_sorted_item_after(14, 1) -> 4
  //   nth_sorted_item_after(14, 2) -> 10
  template <class list_t, class item_t, class extract_f_t>
  item_t nth_sorted_item_after(const list_t& l, const item_t& start, size_t n, const extract_f_t& extract_f)
  {
    if (l.empty())
      throw std::runtime_error("nth_sorted_item_after given empty container");
    
    std::vector<item_t> items;
    BOOST_FOREACH(const auto& item, l)
    {
      items.push_back(extract_f(item));
    }
    std::sort(items.begin(), items.end());
    
    auto start_it = std::lower_bound(items.begin(), items.end(), start);
    // if it's > than all the elements start from the beginning
    if (start_it == items.end())
    {
      start_it = items.begin();
    }
    
    // wrap-around
    size_t start_index = start_it - items.begin();
    return items[(start_index + n) % items.size()];
  }
  //---------------------------------------------------------------
  template <class list_t, class item_t>
  item_t nth_sorted_item_after(const list_t& l, const item_t& start, size_t n)
  {
    return nth_sorted_item_after(l, start, n, tools::identity());
  }
  //---------------------------------------------------------------
  bool parse_payment_id(const std::string& payment_id_str, crypto::hash& payment_id);
  //---------------------------------------------------------------
}
