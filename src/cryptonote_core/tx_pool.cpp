// Copyright (c) 2012-2013 The Cryptonote developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <algorithm>
#include <boost/filesystem.hpp>
#include <unordered_set>
#include <vector>

#include "tx_pool.h"
#include "cryptonote_format_utils.h"
#include "account_boost_serialization.h"
#include "cryptonote_boost_serialization.h"
#include "cryptonote_config.h"
#include "blockchain_storage.h"
#include "common/boost_serialization_helper.h"
#include "common/int-util.h"
#include "misc_language.h"
#include "warnings.h"
#include "crypto/hash.h"
#include "visitors.h"
#include "common/functional.h"
#include "cryptonote_core/tx_input_compat_checker.h"
#include "cryptonote_core/nulls.h"

DISABLE_VS_WARNINGS(4244 4345 4503) //'boost::foreach_detail_::or_' : decorated name length exceeded, name was truncated

namespace cryptonote
{
  namespace detail
  {
    bool get_txin_info(const txin_v& inp, txin_info& info)
    {
      struct get_txin_info_visitor: public boost::static_visitor<txin_info>
      {
        // for now only keep initial cryptonote rule with the key_images
        txin_info operator()(const txin_to_key& inp) const { return txin_to_key_info(inp); }
        
        txin_info operator()(const txin_gen& inp) const { return txin_invalid_info(); }
        txin_info operator()(const txin_to_script& inp) const { return txin_invalid_info(); }
        txin_info operator()(const txin_to_scripthash& inp) const { return txin_invalid_info(); }
        
        txin_info operator()(const txin_mint& inp) const { return txin_no_conflict_info(); }
        txin_info operator()(const txin_remint& inp) const { return txin_no_conflict_info(); }
        
        txin_info operator()(const txin_create_contract& inp) const { return txin_no_conflict_info(); }
        txin_info operator()(const txin_mint_contract& inp) const { return txin_no_conflict_info(); }
        txin_info operator()(const txin_grade_contract& inp) const { return txin_no_conflict_info(); }
        txin_info operator()(const txin_resolve_bc_coins& inp) const { return txin_no_conflict_info(); }
        txin_info operator()(const txin_fuse_bc_coins& inp) const { return txin_no_conflict_info(); }
        
        txin_info operator()(const txin_register_delegate& inp) const { return txin_no_conflict_info(); }
        txin_info operator()(const txin_vote& inp) const { return txin_no_conflict_info(); }
        
        /*txin_info operator()(const txin_mint& inp) const { return txin_mint_info(inp); }
        txin_info operator()(const txin_remint& inp) const { return txin_remint_info(inp); }

        txin_info operator()(const txin_create_contract& inp) const { return txin_mint_info(inp); }
        txin_info operator()(const txin_mint_contract& inp) const { return txin_no_conflict_info(inp); }
        txin_info operator()(const txin_grade_contract& inp) const { return txin_grade_contract_info(inp); }
        txin_info operator()(const txin_resolve_bc_coins& inp) const { return txin_no_conflict_info(inp); }
        txin_info operator()(const txin_fuse_bc_coins& inp) const { return txin_no_conflict_info(inp); }
        
        
        txin_info operator()(const txin_register_delegate& inp) const { return txin_register_delegate_info(inp); }
        txin_info operator()(const txin_vote& inp) const { return txin_vote_info(inp); }*/
      };
      
      info = boost::apply_visitor(get_txin_info_visitor(), inp);
      if (info.type() == typeid(txin_invalid_info))
      {
        LOG_ERROR("Unsupported txin type in txpool, which=" << inp.which());
        return false;
      }
      return true;
    }
  }
  //---------------------------------------------------------------------------------
  tx_memory_pool::tx_memory_pool(blockchain_storage& bchs): m_blockchain(bchs), m_callback(0)
  {

  }
  //---------------------------------------------------------------------------------
  bool tx_memory_pool::add_inp(const crypto::hash& tx_id, const txin_v& inp, bool kept_by_block)
  {
    detail::txin_info info;
    CHECK_AND_ASSERT(detail::get_txin_info(inp, info), false);
    
    if (info.type() == typeid(detail::txin_no_conflict_info))
        return true;
        
    auto& info_set = m_txin_infos[info];
    CHECK_AND_ASSERT_MES(kept_by_block || info_set.size() == 0, false,
                         "internal error: keeped_by_block=" << kept_by_block
                         << ", m_txin_infos.size()=" << m_txin_infos.size() << ENDL
                         << "txin_info=" << info << ENDL
                         << "tx_id=" << tx_id );
    auto ins_res = info_set.insert(tx_id);
    CHECK_AND_ASSERT_MES(ins_res.second, false, "internal error: try to insert duplicate iterator in m_txin_infos set");
    return true;
  }
  //---------------------------------------------------------------------------------
  bool tx_memory_pool::remove_inp(const crypto::hash& tx_id, const txin_v& inp)
  {
    detail::txin_info info;
    CHECK_AND_ASSERT(detail::get_txin_info(inp, info), false);
    
    if (info.type() == typeid(detail::txin_no_conflict_info))
        return true;
    
    auto it = m_txin_infos.find(info);
    CHECK_AND_ASSERT_MES(it != m_txin_infos.end(), false,
                         "failed to find transaction input in infos. info=" << info << ENDL
                         << "transaction id = " << tx_id);
    
    auto& info_set = it->second;
    CHECK_AND_ASSERT_MES(info_set.size(), false,
                         "empty info set, info=" << info << ENDL
                         << "transaction id = " << tx_id);
    
    auto it_in_set = info_set.find(tx_id);
    CHECK_AND_ASSERT_MES(it_in_set != info_set.end(), false,
                         "transaction id not found in info set, info=" << info << ENDL
                         << "transaction id = " << tx_id);
    
    info_set.erase(it_in_set);
    if(!info_set.size())
    {
      //it is now empty hash container for this key_image
#ifdef __clang__
      m_txin_infos.erase(it->first);
#else
      m_txin_infos.erase(it);
#endif
    }
    
    return true;
  }
  //---------------------------------------------------------------------------------
  bool tx_memory_pool::check_can_add_inp(const txin_v& inp) const
  {
    detail::txin_info info;
    CHECK_AND_ASSERT(detail::get_txin_info(inp, info), false);
    
    if (info.type() == typeid(detail::txin_no_conflict_info))
        return true;
    
    CHECK_AND_ASSERT_MES(m_txin_infos.find(info) == m_txin_infos.end(), false,
                         "input conflicts with existing input: " << info);
    return true;
  }
  //---------------------------------------------------------------------------------
  bool tx_memory_pool::add_tx(const transaction &tx, /*const crypto::hash& tx_prefix_hash,*/ const crypto::hash &id, size_t blob_size, tx_verification_context& tvc, bool kept_by_block)
  {
    if(!check_inputs_types_supported(tx) || !check_outputs_types_supported(tx))
    {
      tvc.m_verifivation_failed = true;
      return false;
    }

    uint64_t fee;
    if(!check_inputs_outputs(tx, fee))
    {
      tvc.m_verifivation_failed = true;
      return false;
    }

    // only include if fee >= default fee
    if (!kept_by_block && fee < DEFAULT_FEE)
    {
      LOG_PRINT_L0("Not relaying tx with fee of " << print_money(fee));
      tvc.m_should_be_relayed = false;
      tvc.m_added_to_pool = false;
      return true; // not a failure
    }
      
    //check key images for transaction if it is not kept by block
    if(!kept_by_block)
    {
      if(!check_can_add_tx(tx))
      {
        LOG_ERROR("Cannot add transaction with id= "<< id << ", conflicts with existing transactions");
        tvc.m_verifivation_failed = true;
        return false;
      }
    }

    uint64_t max_used_block_height = 0;
    bool ch_inp_res = m_blockchain.validate_tx(tx, false, &max_used_block_height);
    crypto::hash max_used_block_id = m_blockchain.get_block_id_by_height(max_used_block_height);
    CRITICAL_REGION_LOCAL(m_transactions_lock);
    if(!ch_inp_res)
    {
      if(kept_by_block)
      {
        //anyway add this transaction to pool, because it related to block
        auto txd_p = m_transactions.insert(transactions_container::value_type(id, tx_details()));
        CHECK_AND_ASSERT_MES(txd_p.second, false, "transaction already exists at inserting in memory pool");
        txd_p.first->second.blob_size = blob_size;
        txd_p.first->second.tx = tx;
        txd_p.first->second.fee = fee;
        txd_p.first->second.max_used_block_id = null_hash;
        txd_p.first->second.max_used_block_height = 0;
        txd_p.first->second.kept_by_block = kept_by_block;
        tvc.m_verifivation_impossible = true;
        tvc.m_added_to_pool = true;
      }else
      {
        LOG_PRINT_L0("tx used wrong inputs, rejected");
        tvc.m_verifivation_failed = true;
        return false;
      }
    }else
    {
      //update transactions container
      auto txd_p = m_transactions.insert(transactions_container::value_type(id, tx_details()));
      CHECK_AND_ASSERT_MES(txd_p.second, false, "intrnal error: transaction already exists at inserting in memorypool");
      txd_p.first->second.blob_size = blob_size;
      txd_p.first->second.tx = tx;
      txd_p.first->second.kept_by_block = kept_by_block;
      txd_p.first->second.fee = fee;
      txd_p.first->second.max_used_block_id = max_used_block_id;
      txd_p.first->second.max_used_block_height = max_used_block_height;
      txd_p.first->second.last_failed_height = 0;
      txd_p.first->second.last_failed_id = null_hash;
      tvc.m_added_to_pool = true;

      if(txd_p.first->second.fee >= DEFAULT_FEE)
        tvc.m_should_be_relayed = true;
    }

    tvc.m_verifivation_failed = true;
    
    BOOST_FOREACH(const auto& inp, tx.ins())
    {
      if (!add_inp(id, inp, kept_by_block))
        return false;
    }
    
    //succeed
    tvc.m_verifivation_failed = false;
    if (0 != m_callback)
      m_callback->on_tx_added(id, tx);
    return true;
  }
  //---------------------------------------------------------------------------------
  bool tx_memory_pool::add_tx(const transaction &tx, tx_verification_context& tvc, bool keeped_by_block)
  {
    crypto::hash h = null_hash;
    size_t blob_size = 0;
    CHECK_AND_ASSERT(get_transaction_hash(tx, h, blob_size), false);
    return add_tx(tx, h, blob_size, tvc, keeped_by_block);
  }
  //---------------------------------------------------------------------------------
  bool tx_memory_pool::remove_transaction_data(const transaction& tx)
  {
    CRITICAL_REGION_LOCAL(m_transactions_lock);
    
    auto id = get_transaction_hash(tx);
    
    BOOST_FOREACH(const auto& inp, tx.ins())
    {
      if (!remove_inp(id, inp))
        return false;
    }
    
    return true;
  }
  //---------------------------------------------------------------------------------
  bool tx_memory_pool::take_tx(const crypto::hash &id, transaction &tx, size_t& blob_size, uint64_t& fee)
  {
    CRITICAL_REGION_LOCAL(m_transactions_lock);
    auto it = m_transactions.find(id);
    if(it == m_transactions.end())
      return false;

    tx = it->second.tx;
    blob_size = it->second.blob_size;
    fee = it->second.fee;
    if (!remove_transaction_data(it->second.tx))
    {
      LOG_ERROR("Could not remove transaction data for tx " << id);
    }
    m_transactions.erase(it);
    if (0 != m_callback)
      m_callback->on_tx_removed(id, tx);
    return true;
  }
  //---------------------------------------------------------------------------------
  size_t tx_memory_pool::get_transactions_count() const
  {
    CRITICAL_REGION_LOCAL(m_transactions_lock);
    return m_transactions.size();
  }
  //---------------------------------------------------------------------------------
  bool tx_memory_pool::get_transactions(std::list<transaction>& txs) const
  {
    CRITICAL_REGION_LOCAL(m_transactions_lock);
    BOOST_FOREACH(const auto& tx_vt, m_transactions)
      txs.push_back(tx_vt.second.tx);

    return true;
  }
  //---------------------------------------------------------------------------------
  bool tx_memory_pool::get_transaction(const crypto::hash& id, transaction& tx) const
  {
    CRITICAL_REGION_LOCAL(m_transactions_lock);
    auto it = m_transactions.find(id);
    if(it == m_transactions.end())
      return false;
    tx = it->second.tx;
    return true;
  }
  //---------------------------------------------------------------------------------
  bool tx_memory_pool::on_blockchain_inc(uint64_t new_block_height, const crypto::hash& top_block_id)
  {
    return true;
  }
  //---------------------------------------------------------------------------------
  bool tx_memory_pool::on_blockchain_dec(uint64_t new_block_height, const crypto::hash& top_block_id)
  {
    return true;
  }
  //---------------------------------------------------------------------------------
  bool tx_memory_pool::have_tx(const crypto::hash &id) const
  {
    CRITICAL_REGION_LOCAL(m_transactions_lock);
    if(m_transactions.count(id))
      return true;
    return false;
  }
  //---------------------------------------------------------------------------------
  bool tx_memory_pool::check_can_add_tx(const transaction& tx) const
  {
    // Additional checks besides blockchain_storage::validate_tx, e.g.
    // no double-spend within txs in the mempool, no conflicting currencies/contracts
    CRITICAL_REGION_LOCAL(m_transactions_lock);
    
    BOOST_FOREACH(const auto& inp, tx.ins())
    {
      if (!check_can_add_inp(inp))
        return false;
    }
    
    return true;
  }
  //---------------------------------------------------------------------------------
  void tx_memory_pool::lock() const
  {
    m_transactions_lock.lock();
  }
  //---------------------------------------------------------------------------------
  void tx_memory_pool::unlock() const
  {
    m_transactions_lock.unlock();
  }
  //---------------------------------------------------------------------------------
  bool tx_memory_pool::is_transaction_ready_to_go(tx_details& txd) const
  {
    // see if we can avoid doing the input checks
    if (txd.max_used_block_id == null_hash)
    {
      if (txd.last_failed_id != null_hash
          && m_blockchain.get_current_blockchain_height() > txd.last_failed_height
          && txd.last_failed_id == m_blockchain.get_block_id_by_height(txd.last_failed_height))
      {
        //we already sure that this tx is broken for this height
        return false;
      }
    }
    else
    {
      if (txd.max_used_block_height >= m_blockchain.get_current_blockchain_height())
        return false;
      
      if (m_blockchain.get_block_id_by_height(txd.max_used_block_height) != txd.max_used_block_id)
      {
        //if we already failed on this height and id, skip actual inputs check
        if (txd.last_failed_id == m_blockchain.get_block_id_by_height(txd.last_failed_height))
          return false;
      }
    }
    
    //always check the inputs, there may be txs that can't be added because of keeped by block txs
    if (!m_blockchain.validate_tx(txd.tx, false, &txd.max_used_block_height))
    {
      txd.last_failed_height = m_blockchain.get_current_blockchain_height()-1;
      txd.last_failed_id = m_blockchain.get_block_id_by_height(txd.last_failed_height);
      return false;
    }
    return true;
  }
  //---------------------------------------------------------------------------------
  std::string tx_memory_pool::print_pool(bool short_format)
  {
    std::stringstream ss;
    CRITICAL_REGION_LOCAL(m_transactions_lock);
    BOOST_FOREACH(auto& txe, m_transactions)
    {
      if(short_format)
      {
        const auto& txd = txe.second;
        ss << "id: " << txe.first << ENDL
          << "blob_size: " << txd.blob_size << ENDL
          << "fee: " << txd.fee << ENDL
          << "kept_by_block: " << txd.kept_by_block << ENDL
          << "max_used_block_height: " << txd.max_used_block_height << ENDL
          << "max_used_block_id: " << txd.max_used_block_id << ENDL
          << "last_failed_height: " << txd.last_failed_height << ENDL
          << "last_failed_id: " << txd.last_failed_id << ENDL;
      }else
      {
        auto& txd = txe.second;
        ss << "id: " << txe.first << ENDL
          <<  obj_to_json_str(txd.tx) << ENDL
          << "blob_size: " << txd.blob_size << ENDL
          << "fee: " << txd.fee << ENDL
          << "kept_by_block: " << txd.kept_by_block << ENDL
          << "max_used_block_height: " << txd.max_used_block_height << ENDL
          << "max_used_block_id: " << txd.max_used_block_id << ENDL
          << "last_failed_height: " << txd.last_failed_height << ENDL
          << "last_failed_id: " << txd.last_failed_id << ENDL;
      }

    }
    return ss.str();
  }
  //---------------------------------------------------------------------------------
  namespace detail
  {
    struct has_grade_visitor: tx_input_visitor_base_opt<bool, false, false>
    {
      using tx_input_visitor_base_opt<bool, false, false>::operator();
      
      bool operator()(const txin_grade_contract& inp) const
      {
        return true;
      }
    };
  }
  
  bool tx_memory_pool::fill_block_template(block &bl, size_t median_size, uint64_t already_generated_coins,
                                           size_t &total_size, uint64_t &fee)
  {
    CRITICAL_REGION_LOCAL(m_transactions_lock);

    total_size = 0;
    fee = 0;
    
    tx_input_compat_checker icc;

    size_t max_total_size = 2 * median_size - CRYPTONOTE_COINBASE_BLOB_RESERVED_SIZE;
    std::unordered_set<crypto::hash> added_txs;
    
    // first pass: prioritize grading txs so people can't mint & fuse to prevent a contract from being graded
    // second pass: proceed as usual
    int pass = 0;
    while (true)
    {
      bool only_grading = pass == 0;
      BOOST_FOREACH(auto& txe, m_transactions)
      {
        if (added_txs.count(txe.first) > 0) //already added it
          continue;
        
        if (max_total_size < total_size + txe.second.blob_size)
          continue;
        
        if (only_grading && !tools::any_apply_visitor(detail::has_grade_visitor(), txe.second.tx.ins()))
          continue;

        if (!is_transaction_ready_to_go(txe.second) ||!icc.can_add_tx(txe.second.tx))
          continue;

        bl.tx_hashes.push_back(txe.first);
        added_txs.insert(txe.first);
        total_size += txe.second.blob_size;
        fee += txe.second.fee;
        icc.add_tx(txe.second.tx);
      }
      
      pass++;
      if (pass == 2)
        break;
    }

    return true;
  }
  //---------------------------------------------------------------------------------
  bool tx_memory_pool::init(const std::string& config_folder)
  {
    m_config_folder = config_folder;
    std::string state_file_path = config_folder + "/" + CRYPTONOTE_POOLDATA_FILENAME;
    boost::system::error_code ec;
    if(!boost::filesystem::exists(state_file_path, ec))
      return true;
    bool res = tools::unserialize_obj_from_file(*this, state_file_path);
    if(!res)
    {
      LOG_PRINT_L0("Failed to load memory pool from file " << state_file_path);
    }
    return res;
  }

  //---------------------------------------------------------------------------------
  bool tx_memory_pool::deinit()
  {
    if (!tools::create_directories_if_necessary(m_config_folder))
    {
      LOG_PRINT_L0("Failed to create data directory: " << m_config_folder);
      return false;
    }

    std::string state_file_path = m_config_folder + "/" + CRYPTONOTE_POOLDATA_FILENAME;
    bool res = tools::serialize_obj_to_file(*this, state_file_path);
    if(!res)
    {
      LOG_PRINT_L0("Failed to serialize memory pool to file " << state_file_path);
    }
    return true;
  }
}
