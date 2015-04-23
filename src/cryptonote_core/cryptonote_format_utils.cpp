// Copyright (c) 2012-2013 The Cryptonote developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "include_base_utils.h"
#include "misc_language.h"
using namespace epee;

#include "cryptonote_config.h"
#include "cryptonote_genesis_config.h"
#include "common/functional.h"
#include "crypto/crypto.h"
#include "crypto/hash.h"
#include "crypto/hash_options.h"
#include "crypto/hash_cache.h"
#include "crypto/crypto_basic_impl.h"
#include "wallet/split_strategies.h"
#include "cryptonote_core/cryptonote_format_utils.h"
#include "cryptonote_core/miner.h"
#include "cryptonote_core/visitors.h"
#include "cryptonote_core/tx_builder.h"
#include "cryptonote_core/nulls.h"
#include "cryptonote_core/contract_grading.h"
#include "cryptonote_core/construct_tx_mod.h"
#include "cryptonote_core/cryptonote_basic_impl.h"

#include <boost/foreach.hpp>
#include <boost/algorithm/string/trim.hpp>

#include <cstdio>

namespace cryptonote
{
  tx_destination_entry::tx_destination_entry() : cp(CP_XPB), amount(0), addr(AUTO_VAL_INIT(addr)) { }
  tx_destination_entry::tx_destination_entry(const coin_type& cp_in, uint64_t a, const account_public_address &ad)
      : cp(cp_in), amount(a), addr(ad) { }
  //---------------------------------------------------------------
  bool get_transaction_prefix_hash(const transaction_prefix& tx, crypto::hash& h)
  {
    blobdata prefix_blob;
    CHECK_AND_ASSERT_MES(t_serializable_object_to_blob(tx, prefix_blob), false,
                         "Could not blob-serialize transaction prefix");
    crypto::cn_fast_hash(prefix_blob.data(), prefix_blob.size(), h);
    return true;
  }
  //---------------------------------------------------------------
  crypto::hash get_transaction_prefix_hash(const transaction_prefix& tx)
  {
    crypto::hash h = null_hash;
    if (!get_transaction_prefix_hash(tx, h))
      throw std::runtime_error("Error serializing transaction_prefix");
    return h;
  }
  //---------------------------------------------------------------
  bool parse_and_validate_tx_from_blob(const blobdata& tx_blob, transaction& tx)
  {
    return t_serializable_object_from_blob(tx_blob, tx);
  }
  //---------------------------------------------------------------
  bool parse_and_validate_tx_from_blob(const blobdata& tx_blob, transaction& tx, crypto::hash& tx_hash, crypto::hash& tx_prefix_hash)
  {
    CHECK_AND_ASSERT(parse_and_validate_tx_from_blob(tx_blob, tx), false);
    crypto::cn_fast_hash(tx_blob.data(), tx_blob.size(), tx_hash);
    CHECK_AND_ASSERT(get_transaction_prefix_hash(tx, tx_prefix_hash), false);
    return true;
  }
  //---------------------------------------------------------------
  bool construct_miner_tx(size_t height, size_t median_size, uint64_t already_generated_coins, size_t current_block_size, uint64_t fee, const account_public_address &miner_address, transaction& tx, const blobdata& extra_nonce, size_t max_outs)
  {
    tx.set_null();
    tx.version = VANILLA_TRANSACTION_VERSION;

    keypair txkey = keypair::generate();
    add_tx_pub_key_to_extra(tx, txkey.pub);
    if(!extra_nonce.empty())
      if(!add_extra_nonce_to_tx_extra(tx.extra, extra_nonce))
        return false;

    txin_gen in;
    in.height = height;

    uint64_t block_reward;
    if(!get_block_reward(median_size, current_block_size, already_generated_coins, height, block_reward))
    {
      LOG_PRINT_L0("Block is too big");
      return false;
    }
#if defined(DEBUG_CREATE_BLOCK_TEMPLATE)
    LOG_PRINT_L1("Creating block template: reward " << block_reward <<
      ", fee " << fee)
#endif
    block_reward += fee;

    std::vector<uint64_t> out_amounts;
    decompose_amount_into_digits(block_reward, DEFAULT_FEE,
      [&out_amounts](uint64_t a_chunk) { out_amounts.push_back(a_chunk); },
      [&out_amounts](uint64_t a_dust) { out_amounts.push_back(a_dust); });

    CHECK_AND_ASSERT_MES(1 <= max_outs, false, "max_out must be non-zero");
    while (max_outs < out_amounts.size())
    {
      out_amounts[out_amounts.size() - 2] += out_amounts.back();
      out_amounts.resize(out_amounts.size() - 1);
    }

    uint64_t summary_amounts = 0;
    for (size_t no = 0; no < out_amounts.size(); no++)
    {
      crypto::key_derivation derivation = AUTO_VAL_INIT(derivation);;
      crypto::public_key out_eph_public_key = AUTO_VAL_INIT(out_eph_public_key);
      bool r = crypto::generate_key_derivation(miner_address.m_view_public_key, txkey.sec, derivation);
      CHECK_AND_ASSERT_MES(r, false, "while creating outs: failed to generate_key_derivation(" << miner_address.m_view_public_key << ", " << txkey.sec << ")");

      r = crypto::derive_public_key(derivation, no, miner_address.m_spend_public_key, out_eph_public_key);
      CHECK_AND_ASSERT_MES(r, false, "while creating outs: failed to derive_public_key(" << derivation << ", " << no << ", "<< miner_address.m_spend_public_key << ")");

      txout_to_key tk;
      tk.key = out_eph_public_key;

      tx_out out;
      summary_amounts += out.amount = out_amounts[no];
      out.target = tk;
      tx.add_out(out, CP_XPB);
    }

    CHECK_AND_ASSERT_MES(summary_amounts == block_reward, false, "Failed to construct miner tx, summary_amounts = " << summary_amounts << " not equal block_reward = " << block_reward);

    //lock
    tx.unlock_time = height + CRYPTONOTE_MINED_MONEY_UNLOCK_WINDOW;
    tx.add_in(in, CP_XPB);
    //LOG_PRINT("MINER_TX generated ok, block_reward=" << print_money(block_reward) << "("  << print_money(block_reward - fee) << "+" << print_money(fee)
    //  << "), current_block_size=" << current_block_size << ", already_generated_coins=" << already_generated_coins << ", tx_id=" << get_transaction_hash(tx), LOG_LEVEL_2);
    return true;
  }
  //---------------------------------------------------------------
  bool generate_key_image_helper(const account_keys& ack, const crypto::public_key& tx_public_key, size_t real_output_index, keypair& in_ephemeral, crypto::key_image& ki)
  {
    crypto::key_derivation recv_derivation = AUTO_VAL_INIT(recv_derivation);
    bool r = crypto::generate_key_derivation(tx_public_key, ack.m_view_secret_key, recv_derivation);
    CHECK_AND_ASSERT_MES(r, false, "key image helper: failed to generate_key_derivation(" << tx_public_key << ", " << ack.m_view_secret_key << ")");

    r = crypto::derive_public_key(recv_derivation, real_output_index, ack.m_account_address.m_spend_public_key, in_ephemeral.pub);
    CHECK_AND_ASSERT_MES(r, false, "key image helper: failed to derive_public_key(" << recv_derivation << ", " << real_output_index <<  ", " << ack.m_account_address.m_spend_public_key << ")");

    crypto::derive_secret_key(recv_derivation, real_output_index, ack.m_spend_secret_key, in_ephemeral.sec);

    crypto::generate_key_image(in_ephemeral.pub, in_ephemeral.sec, ki);
    return true;
  }
  //---------------------------------------------------------------
  uint64_t power_integral(uint64_t a, uint64_t b)
  {
    if(b == 0)
      return 1;
    uint64_t total = a;
    for(uint64_t i = 1; i != b; i++)
      total *= a;
    return total;
  }
  //---------------------------------------------------------------
  bool parse_amount(uint64_t& amount, const std::string& str_amount_)
  {
    std::string str_amount = str_amount_;
    boost::algorithm::trim(str_amount);

    size_t point_index = str_amount.find_first_of('.');
    size_t fraction_size;
    if (std::string::npos != point_index)
    {
      fraction_size = str_amount.size() - point_index - 1;
      while (CRYPTONOTE_DISPLAY_DECIMAL_POINT < fraction_size && '0' == str_amount.back())
      {
        str_amount.erase(str_amount.size() - 1, 1);
        --fraction_size;
      }
      if (CRYPTONOTE_DISPLAY_DECIMAL_POINT < fraction_size)
        return false;
      str_amount.erase(point_index, 1);
    }
    else
    {
      fraction_size = 0;
    }

    if (str_amount.empty())
      return false;

    if (fraction_size < CRYPTONOTE_DISPLAY_DECIMAL_POINT)
    {
      str_amount.append(CRYPTONOTE_DISPLAY_DECIMAL_POINT - fraction_size, '0');
    }

    return string_tools::get_xtype_from_string(amount, str_amount);
  }
  //---------------------------------------------------------------
  bool parse_tx_extra(const std::vector<uint8_t>& tx_extra, std::vector<tx_extra_field>& tx_extra_fields)
  {
    tx_extra_fields.clear();

    if(tx_extra.empty())
      return true;

    std::string extra_str(reinterpret_cast<const char*>(tx_extra.data()), tx_extra.size());
    std::istringstream iss(extra_str);
    binary_archive<false> ar(iss);

    bool eof = false;
    while (!eof)
    {
      tx_extra_field field;
      bool r = ::do_serialize(ar, field);
      CHECK_AND_NO_ASSERT_MES(r, false, "failed to deserialize extra field. extra = " << string_tools::buff_to_hex_nodelimer(std::string(reinterpret_cast<const char*>(tx_extra.data()), tx_extra.size())));
      tx_extra_fields.push_back(field);

      std::ios_base::iostate state = iss.rdstate();
      eof = (EOF == iss.peek());
      iss.clear(state);
    }
    CHECK_AND_NO_ASSERT_MES(::serialization::check_stream_state(ar), false, "failed to deserialize extra field. extra = " << string_tools::buff_to_hex_nodelimer(std::string(reinterpret_cast<const char*>(tx_extra.data()), tx_extra.size())));

    return true;
  }
  //---------------------------------------------------------------
  crypto::public_key get_tx_pub_key_from_extra(const std::vector<uint8_t>& tx_extra)
  {
    std::vector<tx_extra_field> tx_extra_fields;
    parse_tx_extra(tx_extra, tx_extra_fields);

    tx_extra_pub_key pub_key_field;
    if(!find_tx_extra_field_by_type(tx_extra_fields, pub_key_field))
      return null_pkey;

    return pub_key_field.pub_key;
  }
  //---------------------------------------------------------------
  crypto::public_key get_tx_pub_key_from_extra(const transaction& tx)
  {
    return get_tx_pub_key_from_extra(tx.extra);
  }
  //---------------------------------------------------------------
  bool add_tx_pub_key_to_extra(transaction& tx, const crypto::public_key& tx_pub_key)
  {
    tx.extra.resize(tx.extra.size() + 1 + sizeof(crypto::public_key));
    tx.extra[tx.extra.size() - 1 - sizeof(crypto::public_key)] = TX_EXTRA_TAG_PUBKEY;
    *reinterpret_cast<crypto::public_key*>(&tx.extra[tx.extra.size() - sizeof(crypto::public_key)]) = tx_pub_key;
    return true;
  }
  //---------------------------------------------------------------
  bool add_extra_nonce_to_tx_extra(std::vector<uint8_t>& tx_extra, const blobdata& extra_nonce)
  {
    CHECK_AND_ASSERT_MES(extra_nonce.size() <= TX_EXTRA_NONCE_MAX_COUNT, false, "extra nonce could be 255 bytes max");
    size_t start_pos = tx_extra.size();
    tx_extra.resize(tx_extra.size() + 2 + extra_nonce.size());
    //write tag
    tx_extra[start_pos] = TX_EXTRA_NONCE;
    //write len
    ++start_pos;
    tx_extra[start_pos] = static_cast<uint8_t>(extra_nonce.size());
    //write data
    ++start_pos;
    memcpy(&tx_extra[start_pos], extra_nonce.data(), extra_nonce.size());
    return true;
  }
  //---------------------------------------------------------------
  void set_payment_id_to_tx_extra_nonce(blobdata& extra_nonce, const crypto::hash& payment_id)
  {
    extra_nonce.clear();
    extra_nonce.push_back(TX_EXTRA_NONCE_PAYMENT_ID);
    const uint8_t* payment_id_ptr = reinterpret_cast<const uint8_t*>(&payment_id);
    std::copy(payment_id_ptr, payment_id_ptr + sizeof(payment_id), std::back_inserter(extra_nonce));
  }
  //---------------------------------------------------------------
  bool get_payment_id_from_tx_extra_nonce(const blobdata& extra_nonce, crypto::hash& payment_id)
  {
    if(sizeof(crypto::hash) + 1 != extra_nonce.size())
      return false;
    if(TX_EXTRA_NONCE_PAYMENT_ID != extra_nonce[0])
      return false;
    payment_id = *reinterpret_cast<const crypto::hash*>(extra_nonce.data() + 1);
    return true;
  }
  //---------------------------------------------------------------
  bool construct_tx(const account_keys& sender_account_keys, const std::vector<tx_source_entry>& sources, const std::vector<tx_destination_entry>& destinations, std::vector<uint8_t> extra, transaction& tx, uint64_t unlock_time)
  {
    keypair txkey;
    return construct_tx(sender_account_keys, sources, destinations, extra, tx, unlock_time, txkey, tools::identity());
  }
  //---------------------------------------------------------------
  bool add_amount_would_overflow(const uint64_t amount1, const uint64_t amount2)
  {
    if (amount1 + amount2 < amount1)
    {
      LOG_ERROR("Overflow adding amount");
      return true;
    }
    return false;
  }
  
  bool sub_amount_would_underflow(const uint64_t amount1, const uint64_t amount2)
  {
    if (amount2 > amount1 || amount1 - amount2 > amount1)
    {
      LOG_ERROR("Underflow subbing amount");
      return true;
    }
    return false;
  }
  
  bool add_amount(uint64_t& amount1, const uint64_t amount2)
  {
    if (add_amount_would_overflow(amount1, amount2))
      return false;
    
    amount1 += amount2;
    return true;
  }
  
  bool sub_amount(uint64_t& amount1, const uint64_t amount2)
  {
    if (sub_amount_would_underflow(amount1, amount2))
      return false;
    
    amount1 -= amount2;
    return true;
  }
  
  namespace {
    struct add_txin_amount_visitor : public tx_input_visitor_base
    {
      using tx_input_visitor_base::operator();
      
      const transaction& tx;
      size_t index;
      currency_map& amounts;
      add_txin_amount_visitor(const transaction& tx_in, size_t index_in, currency_map& amounts_in)
      : tx(tx_in), index(index_in), amounts(amounts_in) { }
      
      bool operator()(const txin_to_key& inp) { return add_amount(amounts[tx.in_cp(index)], inp.amount); }
      bool operator()(const txin_mint& inp) { return add_amount(amounts[tx.in_cp(index)], inp.amount); }
      bool operator()(const txin_remint& inp) { return add_amount(amounts[tx.in_cp(index)], inp.amount); }
      bool operator()(const txin_gen& inp) {
        LOG_ERROR("Shouldn't check_inputs on miner_tx");
        return false;
      }
      bool operator()(const txin_create_contract& inp) { return add_amount(amounts[tx.in_cp(index)], 0); }
      bool operator()(const txin_mint_contract& inp) {
        // subtract the input amount + add backing + contract coins for each one
        coin_type backing_cp = coin_type(inp.backed_by_currency, NotContract, BACKED_BY_N_A);
        if (!sub_amount(amounts[backing_cp], inp.amount))
        {
          LOG_ERROR("Not enough inputs to mint " << inp.amount << " of " << backing_cp);
          return false;
        }
        
        if (!add_amount(amounts[coin_type(inp.contract, BackingCoin, backing_cp.currency)], inp.amount))
          return false;
        
        if (!add_amount(amounts[coin_type(inp.contract, ContractCoin, backing_cp.currency)], inp.amount))
          return false;
        
        return true;
      }
      bool operator()(const txin_grade_contract& inp) {
        CHECK_AND_ASSERT(add_amount(amounts[tx.in_cp(index)], 0), false);
        BOOST_FOREACH(const auto& item, inp.fee_amounts) {
          CHECK_AND_ASSERT(add_amount(amounts[coin_type(item.first, NotContract, BACKED_BY_N_A)], item.second), false);
        }
        return true;
      }
      bool operator()(const txin_resolve_bc_coins& inp) {
        // subtract the .source_amount from Backing/ContractCoins, add the .graded_amount to the currency
        if (!sub_amount(amounts[coin_type(inp.contract,
                                          inp.is_backing_coins ? BackingCoin : ContractCoin,
                                          inp.backing_currency)], inp.source_amount))
        {
          LOG_ERROR("Not enough inputs to resolve " << inp.source_amount << " backing/contract coins");
          return false;
        }
        
        if (!add_amount(amounts[coin_type(inp.backing_currency, NotContract, BACKED_BY_N_A)], inp.graded_amount))
          return false;
        
        return true;
      }
      bool operator()(const txin_fuse_bc_coins& inp) {
        // subtract the backing + contract coins + add the output
        if (!sub_amount(amounts[coin_type(inp.contract, BackingCoin, inp.backing_currency)], inp.amount))
        {
          LOG_ERROR("Not enough backing coin inputs to fuse " << inp.amount << " of " << inp.contract);
          return false;
        }
        if (!sub_amount(amounts[coin_type(inp.contract, ContractCoin, inp.backing_currency)], inp.amount))
        {
          LOG_ERROR("Not enough contract coin inputs to fuse " << inp.amount << " of " << inp.contract);
          return false;
        }
        
        if (!add_amount(amounts[coin_type(inp.backing_currency, NotContract, BACKED_BY_N_A)], inp.amount))
          return false;
        
        return true;
      }
      bool operator()(const txin_register_delegate& inp) {
        if (!sub_amount(amounts[CP_XPB], inp.registration_fee))
        {
          LOG_ERROR("Not enough XPBs for delegate registration fee");
          return false;
        }
        return true;
      }
      bool operator()(const txin_vote& inp) {
        return true;
      }
    };
  }
  
  bool process_txin_amounts(const transaction& tx, size_t i, currency_map& amounts)
  {
    CHECK_AND_ASSERT_MES(i < tx.ins().size(), false, "add_txin_amounts() asking for vin of out of range index");
    add_txin_amount_visitor visitor(tx, i, amounts);
    CHECK_AND_ASSERT(boost::apply_visitor(visitor, tx.ins()[i]), false);
    return true;
  }
  
  bool check_inputs(const transaction& tx, currency_map& in)
  {
    in.clear();
    
    for (size_t i=0; i < tx.ins().size(); i++) {
      CHECK_AND_ASSERT(process_txin_amounts(tx, i, in), false);
    }
    return true;
  }
  bool check_inputs(const transaction& tx)
  {
    currency_map in;
    return check_inputs(tx, in);
  }
  //---------------------------------------------------------------
  bool check_outputs(const transaction& tx, currency_map& amounts)
  {
    amounts.clear();
    
    size_t i = 0;
    BOOST_FOREACH(const auto& outp, tx.outs())
    {
      if (!add_amount(amounts[tx.out_cp(i)], outp.amount))
      {
        LOG_ERROR("Overflow in output amount");
        return false;
      }
      i++;
    }
    
    return true;
  }
  
  bool check_outputs(const transaction& tx)
  {
    currency_map out;
    return check_outputs(tx, out);
  }
  //---------------------------------------------------------------
  bool check_inputs_outputs(const transaction& tx, currency_map& in, currency_map& out, uint64_t& fee)
  {
    if (!check_inputs(tx, in))
      return false;
    
    if (!check_outputs(tx, out))
      return false;
    
    currency_map merged;
    merged.insert(in.cbegin(), in.cend());
    merged.insert(out.cbegin(), out.cend());
    BOOST_FOREACH(const auto& item, merged)
    {
      auto cp = item.first;
      if (in[cp] < out[cp])
      {
        crypto::hash tx_hash;
        get_transaction_hash(tx, tx_hash);
        LOG_PRINT_RED_L0("tx with wrong amounts: for currency " << cp << ", ins=" << in[cp] << ", outs=" << out[cp] << ", rejected for tx id= " << tx_hash);
        return false;
      }
    }
    
    fee = in[CP_XPB] - out[CP_XPB];
    return true;
  }
  bool check_inputs_outputs(const transaction& tx, uint64_t& fee)
  {
    currency_map in, out;
    return check_inputs_outputs(tx, in, out, fee);
  }
  bool check_inputs_outputs(const transaction& tx)
  {
    uint64_t fee;
    return check_inputs_outputs(tx, fee);
  }
  //---------------------------------------------------------------
  uint64_t get_tx_fee(const transaction& tx)
  {
    uint64_t fee = 0;
    if (!check_inputs_outputs(tx, fee))
      return 0;
    return fee;
  }
  //---------------------------------------------------------------
  uint64_t get_block_height(const block& b)
  {
    if (b.miner_tx.ins().size() != 1)
    {
      crypto::hash h;
      get_block_hash(b, h);
      LOG_ERROR("wrong miner tx in block: " << h << ", b.miner_tx.vin.size() != 1");
      return false;
    }
    CHECKED_GET_SPECIFIC_VARIANT(b.miner_tx.ins()[0], const txin_gen, coinbase_in, 0);
    return coinbase_in.height;
  }
  //---------------------------------------------------------------
  namespace {
    struct input_type_supported_visitor: tx_input_visitor_base
    {
      using tx_input_visitor_base::operator();
      
      bool operator()(const txin_to_key& inp) const { return true; }
      // currency not yet engaged
      bool operator()(const txin_mint& inp) const { return false; }
      bool operator()(const txin_remint& inp) const { return false; }
      // contracts not yet engaged
      bool operator()(const txin_create_contract& inp) const { return false; }
      bool operator()(const txin_mint_contract& inp) const { return false; }
      bool operator()(const txin_grade_contract& inp) const { return false; }
      bool operator()(const txin_resolve_bc_coins& inp) const { return false; }
      bool operator()(const txin_fuse_bc_coins& inp) const { return false; }
      // dpos
      bool operator()(const txin_register_delegate& inp) const { return true; }
      bool operator()(const txin_vote& inp) const { return true; }
    };
  }
  bool check_inputs_types_supported(const transaction& tx)
  {
    return tools::all_apply_visitor(input_type_supported_visitor(), tx.ins());
  }
  //---------------------------------------------------------------
  namespace {
    struct output_type_supported_visitor: tx_output_visitor_base
    {
      using tx_output_visitor_base::operator();
      
      bool operator()(const txout_to_key& inp) const { return true; }
    };
    
  }
  bool check_outputs_types_supported(const transaction& tx)
  {
    return tools::all_apply_visitor(output_type_supported_visitor(), tx.outs(), out_getter());
  }
  //-----------------------------------------------------------------------------------------------
  namespace {
    struct check_outs_valid_visitor: tx_output_visitor_base
    {
      using tx_output_visitor_base::operator();
      
      bool operator()(const txout_to_key& outp) const
      {
        return check_key(outp.key);
      }
    };
    
  }
  bool check_outs_valid(const transaction& tx)
  {
    check_outs_valid_visitor v;
    for (size_t i=0; i < tx.outs().size(); i++) {
      if (tx.outs()[i].amount <= 0)
        return false;
      if (!boost::apply_visitor(v, tx.outs()[i].target))
        return false;
    }
    
    return true;
  }
  //-----------------------------------------------------------------------------------------------
  currency_map get_outs_money_amount(const transaction& tx)
  {
    currency_map result;
    
    size_t i = 0;
    BOOST_FOREACH(const auto& o, tx.outs()) {
      result[tx.out_cp(i)] += o.amount;
      i += 1;
    }
    return result;
  }
  //---------------------------------------------------------------
  std::string short_hash_str(const crypto::hash& h)
  {
    std::string res = string_tools::pod_to_hex(h);
    CHECK_AND_ASSERT_MES(res.size() == 64, res, "wrong hash256 with string_tools::pod_to_hex conversion");
    auto erased_pos = res.erase(8, 48);
    res.insert(8, "....");
    return res;
  }
  //---------------------------------------------------------------
  bool is_out_to_acc(const account_keys& acc, const txout_to_key& out_key, const crypto::public_key& tx_pub_key, size_t output_index)
  {
    crypto::key_derivation derivation;
    CHECK_AND_ASSERT_MES(generate_key_derivation(tx_pub_key, acc.m_view_secret_key, derivation), false,
                         "is_out_to_acc could not generate_key_derivation");
    crypto::public_key pk;
    CHECK_AND_ASSERT_MES(derive_public_key(derivation, output_index, acc.m_account_address.m_spend_public_key, pk), false,
                         "is_out_to_acc could not derive_public_key");
    return pk == out_key.key;
  }
  //---------------------------------------------------------------
  bool lookup_acc_outs(const account_keys& acc, const transaction& tx, std::vector<size_t>& outs, uint64_t& money_transfered)
  {
    crypto::public_key tx_pub_key = get_tx_pub_key_from_extra(tx);
    if(null_pkey == tx_pub_key)
      return false;
    return lookup_acc_outs(acc, tx, tx_pub_key, outs, money_transfered);
  }
  //---------------------------------------------------------------
  bool lookup_acc_outs(const account_keys& acc, const transaction& tx, const crypto::public_key& tx_pub_key, std::vector<size_t>& outs, uint64_t& money_transfered)
  {
    money_transfered = 0;
    size_t i = 0;
    BOOST_FOREACH(const tx_out& o, tx.outs())
    {
      CHECK_AND_ASSERT_MES(o.target.type() == typeid(txout_to_key), false, "wrong type id in transaction out");
      if(is_out_to_acc(acc, boost::get<txout_to_key>(o.target), tx_pub_key, i))
      {
        outs.push_back(i);
        money_transfered += o.amount;
      }
      i++;
    }
    return true;
  }
  //---------------------------------------------------------------
  void get_blob_hash(const blobdata& blob, crypto::hash& res)
  {
    cn_fast_hash(blob.data(), blob.size(), res);
  }
  //---------------------------------------------------------------
  crypto::hash get_blob_hash(const blobdata& blob)
  {
    crypto::hash h = null_hash;
    get_blob_hash(blob, h);
    return h;
  }
  //---------------------------------------------------------------
  std::string print_grade_scale(uint64_t grade_scale)
  {
    std::stringstream stream;
    char pct[50];
    sprintf(pct, "%.2f%%", (float)grade_scale * 100.0 / GRADE_SCALE_MAX);
    stream << grade_scale << " (" << pct << ")";
    return stream.str();
  }
  //---------------------------------------------------------------
  bool get_transaction_hash(const transaction& t, crypto::hash& res, size_t& blob_size)
  {
    return get_object_hash(t, res, blob_size);
  }
  //---------------------------------------------------------------
  bool get_transaction_hash(const transaction& t, crypto::hash& res)
  {
    size_t blob_size;
    return get_transaction_hash(t, res, blob_size);
  }
  //---------------------------------------------------------------
  crypto::hash get_transaction_hash(const transaction& t)
  {
    crypto::hash h = null_hash;
    if (!get_transaction_hash(t, h))
      throw std::runtime_error("Failed to get_transaction_hash");
    return h;
  }
  //---------------------------------------------------------------
  bool get_tx_tree_hash(const std::vector<crypto::hash>& tx_hashes, crypto::hash& h)
  {
    tree_hash(tx_hashes.data(), tx_hashes.size(), h);
    return true;
  }
  //---------------------------------------------------------------
  bool get_tx_tree_hash(const block& b, crypto::hash& h)
  {
    std::vector<crypto::hash> txs_ids;
    size_t bl_sz = 0;
    if (!get_transaction_hash(b.miner_tx, h, bl_sz))
      return false;
    txs_ids.push_back(h);
    BOOST_FOREACH(auto& th, b.tx_hashes)
      txs_ids.push_back(th);
    return get_tx_tree_hash(txs_ids, h);
  }
  //---------------------------------------------------------------
  bool get_block_hashing_blob(const block& b, blobdata& bl, bool for_dpos_sig)
  {
    if (!t_serializable_object_to_blob(static_cast<block_header>(b), bl))
      return false;
    crypto::hash tree_root_hash;
    if (!get_tx_tree_hash(b, tree_root_hash))
      return false;
    bl.append((const char*)&tree_root_hash, sizeof(tree_root_hash));
    bl.append(tools::get_varint_data(b.tx_hashes.size()+1));
    if (is_pos_block(b))
    {
      bl.append(tools::get_varint_data(b.signing_delegate_id));
      // only add the dpos_signature if we won't use the blob to calculate the
      // dpos_signature itself
      if (!for_dpos_sig)
      {
        bl.append(t_serializable_object_to_blob(b.dpos_sig));
      }
    }
    return true;
  }
  //---------------------------------------------------------------
  bool get_block_prefix_hash(const block& b, crypto::hash& res)
  {
    blobdata bl;
    if (!get_block_hashing_blob(b, bl, true))
      return false;
    
    return get_object_hash(bl, res);
  }
  //---------------------------------------------------------------
  crypto::hash get_block_prefix_hash(const block& b)
  {
    crypto::hash p = null_hash;
    if (!get_block_prefix_hash(b, p))
      throw std::runtime_error("Failed to get_block_prefix_hash");
    
    return p;
  }
  //---------------------------------------------------------------
  bool get_block_hash(const block& b, crypto::hash& res)
  {
    blobdata bl;
    if (!get_block_hashing_blob(b, bl, false))
      return false;
    
    return get_object_hash(bl, res);
  }
  //---------------------------------------------------------------
  crypto::hash get_block_hash(const block& b)
  {
    crypto::hash p = null_hash;
    if (!get_block_hash(b, p))
      throw std::runtime_error("Failed to get_block_hash");
    return p;
  }
  //---------------------------------------------------------------
  std::string print_money(uint64_t amount, uint64_t decimals)
  {
    std::string s = std::to_string(amount);
    if(s.size() < decimals+1)
    {
      s.insert(0, decimals+1 - s.size(), '0');
    }
    s.insert(s.size() - decimals, ".");
    return s;
  }
  //---------------------------------------------------------------
  std::string print_money(uint64_t amount)
  {
    return print_money(amount, CRYPTONOTE_DISPLAY_DECIMAL_POINT);
  }
  //---------------------------------------------------------------
  std::string print_moneys(const currency_map& moneys)
  {
    struct currency_getter {
      uint64_t currency_decimals(coin_type type) const {
        if (type == CP_XPB)
          return CRYPTONOTE_DISPLAY_DECIMAL_POINT;
        return 0;
      }
    } currency_getter;
    
    return print_moneys(moneys, currency_getter);
  }
  //---------------------------------------------------------------
  bool generate_genesis_block(block& bl)
  {
    //genesis block
    bl = boost::value_initialized<block>();

    /*account_public_address ac = boost::value_initialized<account_public_address>();
    std::vector<size_t> sz;
    construct_miner_tx(0, 0, 0, 0, 0, ac, bl.miner_tx); // zero fee in genesis
    blobdata txb = tx_to_blob(bl.miner_tx);
    std::string hex_tx_represent = string_tools::buff_to_hex_nodelimer(txb);
    LOG_PRINT_L4("genesis hex_tx_represent: " << hex_tx_represent);*/
    
    if (GENESIS_NONCE_STRING == NULL)
    {
      LOG_PRINT_RED_L0("Didn't provide genesis nonce string");
      return false;
    }
    if (GENESIS_TIMESTAMP == NULL)
    {
      LOG_PRINT_RED_L0("Didn't provide genesis timestamp");
      return false;
    }
    if (GENESIS_COINBASE_TX_HEX == NULL)
    {
      LOG_PRINT_RED_L0("Didn't provide genesis coinbase tx hex");
      return false;
    }
    if (GENESIS_BLOCK_ID_HEX == NULL)
    {
      LOG_PRINT_RED_L0("Didn't provide genesis block id hash hex");
      return false;
    }
    
    //hard code coinbase tx in genesis block, because "tru" generating tx use random, but genesis should be always the same
    std::string genesis_coinbase_tx_hex = GENESIS_COINBASE_TX_HEX;

    blobdata tx_bl;
    string_tools::parse_hexstr_to_binbuff(genesis_coinbase_tx_hex, tx_bl);
    bool r = parse_and_validate_tx_from_blob(tx_bl, bl.miner_tx);
    CHECK_AND_ASSERT_MES(r, false, "failed to parse coinbase tx from hard coded blob");
    bl.major_version = POW_BLOCK_MAJOR_VERSION;
    bl.minor_version = POW_BLOCK_MINOR_VERSION;
    bl.timestamp = *GENESIS_TIMESTAMP;
    
    crypto::hash nonce_hash = crypto::cn_fast_hash(GENESIS_NONCE_STRING, strlen(GENESIS_NONCE_STRING));
    bl.nonce = *(reinterpret_cast<uint32_t *>(&nonce_hash));
    
    LOG_PRINT_GREEN("Genesis block nonce is: " << bl.nonce, LOG_LEVEL_0);
    
    crypto::hash block_id = null_hash;
    if (!string_tools::hex_to_pod(GENESIS_BLOCK_ID_HEX, block_id))
    {
      LOG_PRINT_RED_L0("Couldn't parse genesis block id hex: " << GENESIS_BLOCK_ID_HEX);
      return false;
    }
    if (block_id != get_block_hash(bl))
    {
      LOG_PRINT_RED_L0("Provided incorrect block id hash hex. Given " << GENESIS_BLOCK_ID_HEX << ", but block hashed to " << string_tools::pod_to_hex(get_block_hash(bl)));
      return false;
    }
    
    return true;
  }
  //---------------------------------------------------------------
  bool get_block_longhash(const block& b, crypto::hash& res, uint64_t height, uint64_t **state, bool use_cache)
  {
    if (is_pos_block(b))
    {
      LOG_ERROR("get_block_longhash: Should not ever need to longhash dpos blocks");
      return false;
    }
    
    blobdata bd;
    if (!get_block_hashing_blob(b, bd, false))
    {
      throw std::runtime_error("Could not get_block_hashing_blob");
    }
    
    crypto::hash id;
    
    if (use_cache)
    {
      id = get_block_hash(b);
      
      if (cryptonote::config::use_signed_hashes && crypto::g_hash_cache.get_signed_longhash(id, res))
        return true;
      
      if (crypto::g_hash_cache.get_cached_longhash(id, res))
        return true;
    }
    
    if (state == NULL)
    {
      LOG_ERROR("get_block_longhash: No boulderhash state (boulderhash disabled)");
      return false;
    }
    
    if (height >= BOULDERHASH_2_SWITCH_BLOCK)
    {
      crypto::pc_boulderhash(BOULDERHASH_VERSION_REGULAR_2, bd.data(), bd.size(), res, state);
    }
    else
    {
      crypto::pc_boulderhash(BOULDERHASH_VERSION_REGULAR_1, bd.data(), bd.size(), res, state);
    }
    
    if (use_cache)
    {
      crypto::g_hash_cache.add_cached_longhash(id, res);
    }
    
    return true;
  }
  //---------------------------------------------------------------
  std::vector<uint64_t> relative_output_offsets_to_absolute(const std::vector<uint64_t>& off)
  {
    std::vector<uint64_t> res = off;
    for(size_t i = 1; i < res.size(); i++)
      res[i] += res[i-1];
    return res;
  }
  //---------------------------------------------------------------
  std::vector<uint64_t> absolute_output_offsets_to_relative(const std::vector<uint64_t>& off)
  {
    std::vector<uint64_t> res = off;
    if(!off.size())
      return res;
    std::sort(res.begin(), res.end());//just to be sure, actually it is already should be sorted
    for(size_t i = res.size()-1; i != 0; i--)
      res[i] -= res[i-1];

    return res;
  }
  //---------------------------------------------------------------
  bool parse_and_validate_block_from_blob(const blobdata& b_blob, block& b)
  {
    return t_serializable_object_from_blob(b_blob, b);
  }
  //---------------------------------------------------------------
  blobdata block_to_blob(const block& b)
  {
    return t_serializable_object_to_blob(b);
  }
  //---------------------------------------------------------------
  bool block_to_blob(const block& b, blobdata& b_blob)
  {
    return t_serializable_object_to_blob(b, b_blob);
  }
  //---------------------------------------------------------------
  blobdata tx_to_blob(const transaction& tx)
  {
    return t_serializable_object_to_blob(tx);
  }
  //---------------------------------------------------------------
  bool tx_to_blob(const transaction& tx, blobdata& b_blob)
  {
    return t_serializable_object_to_blob(tx, b_blob);
  }
  //---------------------------------------------------------------
  bool check_dpos_block_sig(const block& b, const account_public_address& delegate_addr)
  {
    CHECK_AND_ASSERT_MES(is_pos_block(b), false, "check_dpos_block_sig: not dpos block");
    
    crypto::hash prefix_hash;
    CHECK_AND_ASSERT_MES(get_block_prefix_hash(b, prefix_hash), false,
                         "check_dpos_block_sig: could not get block prefix hash");
    
    return check_signature(prefix_hash, delegate_addr.m_spend_public_key, b.dpos_sig);
  }
  //---------------------------------------------------------------
  bool sign_dpos_block(block& b, const cryptonote::account_base& staker_acc)
  {
    CHECK_AND_ASSERT_MES(is_pos_block(b), false, "sign_dpos_block: not dpos block");
    
    crypto::hash prefix_hash;
    CHECK_AND_ASSERT_MES(get_block_prefix_hash(b, prefix_hash), false,
                         "sign_dpos_block: could not get block prefix hash");
    
    generate_signature(prefix_hash, staker_acc.get_keys().m_account_address.m_spend_public_key,
                       staker_acc.get_keys().m_spend_secret_key, b.dpos_sig);
    
    if (!check_dpos_block_sig(b, staker_acc.get_keys().m_account_address))
    {
      LOG_ERROR("sign_dpos_block: Could not check the just-created signature");
      return false;
    }
    
    return true;
  }
  //---------------------------------------------------------------
  bool is_pow_block(const block& b)
  {
    return b.major_version <= POW_BLOCK_MAJOR_VERSION;
  }
  //---------------------------------------------------------------
  bool is_pos_block(const block& b)
  {
    return b.major_version >= DPOS_BLOCK_MAJOR_VERSION;
  }
  //---------------------------------------------------------------
  bool parse_payment_id(const std::string& payment_id_str, crypto::hash& payment_id)
  {
    blobdata payment_id_data;
    if(!string_tools::parse_hexstr_to_binbuff(payment_id_str, payment_id_data))
      return false;

    if(sizeof(crypto::hash) != payment_id_data.size())
      return false;

    payment_id = *reinterpret_cast<const crypto::hash*>(payment_id_data.data());
    return true;
  }
  //---------------------------------------------------------------
}
