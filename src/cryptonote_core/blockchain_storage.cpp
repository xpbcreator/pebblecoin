// Copyright (c) 2014-2015 The Pebblecoin developers
// Copyright (c) 2012-2013 The Cryptonote developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <algorithm>
#include <cstdio>
#include <boost/archive/binary_oarchive.hpp>
#include <boost/archive/binary_iarchive.hpp>
#include <boost/range/adaptor/transformed.hpp>
#include <boost/algorithm/string/join.hpp>
#include <boost/lexical_cast.hpp>

#include "include_base_utils.h"
#include "misc_language.h"
#include "profile_tools.h"
#include "file_io_utils.h"
#include "warnings.h"

#include "common/boost_serialization_helper.h"
#include "common/functional.h"
#include "crypto/hash.h"
#include "crypto/hash_cache.h"
#include "cryptonote_config.h"
#include "cryptonote_core/cryptonote_basic_impl.h"
#include "cryptonote_core/blockchain_storage.h"
#include "cryptonote_core/cryptonote_format_utils.h"
#include "cryptonote_core/account_boost_serialization.h"
#include "cryptonote_core/cryptonote_boost_serialization.h"
#include "cryptonote_core/blockchain_storage_boost_serialization.h"
#include "cryptonote_core/miner.h"
#include "cryptonote_core/visitors.h"
#include "cryptonote_core/contract_grading.h"
#include "cryptonote_core/delegate_auto_vote.h"

using namespace std;
using namespace epee;
using namespace cryptonote;

DISABLE_VS_WARNINGS(4267)

//------------------------------------------------------------------
bool blockchain_storage::have_tx(const crypto::hash &id)
{
  CRITICAL_REGION_LOCAL(m_blockchain_lock);
  return m_transactions.find(id) != m_transactions.end();
}
//------------------------------------------------------------------
bool blockchain_storage::have_tx_keyimg_as_spent(const crypto::key_image &key_im)
{
  CRITICAL_REGION_LOCAL(m_blockchain_lock);
  return  m_spent_keys.find(key_im) != m_spent_keys.end();
}
//------------------------------------------------------------------
blockchain_storage::transaction_chain_entry *blockchain_storage::get_tx_chain_entry(const crypto::hash &id)
{
  CRITICAL_REGION_LOCAL(m_blockchain_lock);
  auto it = m_transactions.find(id);
  if (it == m_transactions.end())
    return NULL;
  
  return &it->second;
}
//------------------------------------------------------------------
transaction *blockchain_storage::get_tx(const crypto::hash &id)
{
  transaction_chain_entry *ce = get_tx_chain_entry(id);
  return ce ? &ce->tx : NULL;
}
//------------------------------------------------------------------
uint64_t blockchain_storage::get_current_blockchain_height()
{
  CRITICAL_REGION_LOCAL(m_blockchain_lock);
  return m_blocks.size();
}
//------------------------------------------------------------------
bool blockchain_storage::get_delegate_address(const delegate_id_t& delegate_id, account_public_address& address)
{
  CRITICAL_REGION_LOCAL(m_blockchain_lock);
  
  CHECK_AND_ASSERT_MES(m_delegates.count(delegate_id) > 0, false, "No such delegate " << delegate_id);
  
  address = m_delegates[delegate_id].public_address;
  return true;
}
//------------------------------------------------------------------
void blockchain_storage::catchup_signed_hashes()
{
  if (!crypto::g_hash_cache.is_hash_signing_key_set())
    return;
  
  std::list<crypto::hash> to_proc;
  
  {
    CRITICAL_REGION_LOCAL(m_blockchain_lock);
    BOOST_FOREACH(const auto& ent, m_blocks)
    {
      if (is_pow_block(ent.bl))
      {
        to_proc.push_back(get_block_hash(ent.bl));
      }
    }
    
    BOOST_FOREACH(const auto& alt_bl, m_alternative_chains)
    {
      if (is_pow_block(alt_bl.second.bl))
      {
        to_proc.push_back(get_block_hash(alt_bl.second.bl));
      }
    }
  }
  
  {
    auto it = to_proc.begin();
    while (it != to_proc.end())
    {
      if (crypto::g_hash_cache.have_signed_longhash_for(*it))
        it = to_proc.erase(it);
      else
        it++;
    }
  }
  
  if (!to_proc.size())
  {
    LOG_PRINT_GREEN("Fully caught up to signed hashes", LOG_LEVEL_0);
    return;
  }
  
  uint32_t num_caught_up = 0;
  uint32_t num_total = to_proc.size();
  LOG_PRINT_GREEN("Catching up to " << num_total << " signed hashes", LOG_LEVEL_0);
  
  auto it = to_proc.begin();
  while (!m_stop_catchup && it != to_proc.end())
  {
    const auto& id = *it;
    it++;
    
    block bl;
    if (!get_block_by_hash(id, bl))
    {
      LOG_PRINT_RED_L0("Couldn't get block by hash when catching up signed hash: " << id);
      continue;
    }
    
    crypto::hash proof_of_work = null_hash;
    if (!get_block_longhash(bl, proof_of_work, get_block_height(bl), crypto::g_boulderhash_state, true))
    {
      LOG_PRINT_RED_L0("Couldn't get block longhash when catching up signed hash: " << id);
      continue;
    }
    
    crypto::signature sig;
    if (!crypto::g_hash_cache.create_signed_hash(id, proof_of_work, sig))
    {
      LOG_PRINT_RED_L0("Couldn't create signed hash when catching up signed hash: " << id);
      continue;
    }
    
    LOG_PRINT_L0("(" << (num_caught_up+1) << "/" << num_total <<") Just caught up signed hash for block #"
                 << get_block_height(bl) << ": " << id);
    num_caught_up++;
    if (num_caught_up % 100 == 0)
    {
      if (!crypto::g_hash_cache.store())
      {
        LOG_PRINT_RED_L0("Could not store hash cache! Not trying to catch up anymore.");
        break;
      }
    }
  }
  
  if (m_stop_catchup)
  {
    LOG_PRINT_L0("Signed hash catch-up interrupted");
  }
  else
  {
    LOG_PRINT_GREEN("Done catching up signed hashes", LOG_LEVEL_0);
  }
  
  if (!crypto::g_hash_cache.store())
  {
    LOG_PRINT_RED_L0("Could not store hash cache!");
  }
}

bool blockchain_storage::init(const std::string& config_folder)
{
  CRITICAL_REGION_LOCAL(m_blockchain_lock);
  m_config_folder = config_folder;
  LOG_PRINT_L0("Loading blockchain...");
  const std::string filename = m_config_folder + "/" + CRYPTONOTE_BLOCKCHAINDATA_FILENAME;
  if(!tools::unserialize_obj_from_file(*this, filename))
  {
      LOG_PRINT_L0("Can't load blockchain storage from file, generating genesis block.");
      block bl = boost::value_initialized<block>();
      block_verification_context bvc = boost::value_initialized<block_verification_context>();
      CHECK_AND_ASSERT_MES(generate_genesis_block(bl), false, "Failed to generate genesis block");
      add_new_block(bl, bvc);
      CHECK_AND_ASSERT_MES(!bvc.m_verifivation_failed && bvc.m_added_to_main_chain, false, "Failed to add genesis block to blockchain");
  }
  if(!m_blocks.size())
  {
    LOG_PRINT_L0("Blockchain not loaded, generating genesis block.");
    block bl = boost::value_initialized<block>();
    block_verification_context bvc = boost::value_initialized<block_verification_context>();
    CHECK_AND_ASSERT_MES(generate_genesis_block(bl), false, "Failed to generate genesis block");
    add_new_block(bl, bvc);
    CHECK_AND_ASSERT_MES(!bvc.m_verifivation_failed, false, "Failed to add genesis block to blockchain");
  }
  uint64_t timestamp_diff = get_adjusted_time() - m_blocks.back().bl.timestamp;
  if(!m_blocks.back().bl.timestamp)
    timestamp_diff = get_adjusted_time() - 1341378000;
  LOG_PRINT_GREEN("Blockchain initialized. last block: " << m_blocks.size()-1 << ", " << misc_utils::get_time_interval_string(timestamp_diff) <<  " time ago, current difficulty: " << get_difficulty_for_next_block(), LOG_LEVEL_0);
  
  if (crypto::g_hash_cache.is_hash_signing_key_set())
  {
    m_pcatchup_thread = new boost::thread(boost::bind(&blockchain_storage::catchup_signed_hashes, this));
  }
  
  CHECK_AND_ASSERT_MES(recalculate_top_delegates(), false, "Failed to calculate top delegates");
  
  return true;
}
//------------------------------------------------------------------
bool blockchain_storage::store_blockchain()
{
  m_is_blockchain_storing = true;
  misc_utils::auto_scope_leave_caller scope_exit_handler = misc_utils::create_scope_leave_handler([&](){m_is_blockchain_storing=false;});

  LOG_PRINT_L0("Storing blockchain...");
  if (!tools::create_directories_if_necessary(m_config_folder))
  {
    LOG_PRINT_L0("Failed to create data directory: " << m_config_folder);
    return false;
  }

  const std::string temp_filename = m_config_folder + "/" + CRYPTONOTE_BLOCKCHAINDATA_TEMP_FILENAME;
  // There is a chance that temp_filename and filename are hardlinks to the same file
  std::remove(temp_filename.c_str());
  if(!tools::serialize_obj_to_file(*this, temp_filename))
  {
    //achtung!
    LOG_ERROR("Failed to save blockchain data to file: " << temp_filename);
    return false;
  }
  const std::string filename = m_config_folder + "/" + CRYPTONOTE_BLOCKCHAINDATA_FILENAME;
  std::error_code ec = tools::replace_file(temp_filename, filename);
  if (ec)
  {
    LOG_ERROR("Failed to rename blockchain data file " << temp_filename << " to " << filename << ": " << ec.message() << ':' << ec.value());
    return false;
  }
  LOG_PRINT_L0("Blockchain stored OK.");
  return true;
}
//------------------------------------------------------------------
bool blockchain_storage::deinit()
{
  m_stop_catchup = true;
  
  if (m_pcatchup_thread)
  {
    LOG_PRINT_L0("Waiting for catchup thread to finish...");
    m_pcatchup_thread->join();
    delete m_pcatchup_thread;
  }
  
  return store_blockchain();
}
//------------------------------------------------------------------
bool blockchain_storage::pop_block_from_blockchain()
{
  CRITICAL_REGION_LOCAL(m_blockchain_lock);
  
  CHECK_AND_ASSERT_MES(m_blocks.size() > 1, false, "pop_block_from_blockchain: can't pop from blockchain with size = " << m_blocks.size());
  size_t h = m_blocks.size()-1;
  block_extended_info& bei = m_blocks[h];
  //crypto::hash id = get_block_hash(bei.bl);
  m_popping_block = true;
  bool r = purge_block_data_from_blockchain(bei.bl, bei.bl.tx_hashes.size());
  if (!r)
  {
    LOG_ERROR("Failed to purge_block_data_from_blockchain for block " << get_block_hash(bei.bl) << " on height " << h);
    m_popping_block = false;
    return false;
  }
  
  if (!recalculate_top_delegates())
  {
    LOG_ERROR("Popping block resulted in invalid delegate votes");
    m_popping_block = false;
    return false;
  }

  // undo missing delegate block stats
  if (m_blocks.size() > 2)
  {
    const block& bl = bei.bl;
    const block& block_prev = m_blocks.end()[-2].bl;
    if (is_pos_block(bl) && is_pos_block(block_prev)) // skip for non-pos and first pos block
    {
      uint64_t seconds_since_prev = bl.timestamp - block_prev.timestamp;
      uint64_t n_delegates = seconds_since_prev / DPOS_DELEGATE_SLOT_TIME;
      delegate_id_t prev_delegate = block_prev.signing_delegate_id;

      m_delegates[bl.signing_delegate_id].processed_blocks--;
      m_delegates[bl.signing_delegate_id].fees_received -= average_past_block_fees(get_block_height(block_prev));
      LOG_PRINT_L0("Undoing delegate " << bl.signing_delegate_id << " processed a block");
      for (uint64_t i=0; i < n_delegates; i++)
      {
        delegate_id_t missed = nth_delegate_after(prev_delegate + 1, i);
        m_delegates[missed].missed_blocks--;
        LOG_PRINT_L0("Undoing delegate " << missed << " missed a block");
      }
    }
  }
  
  //remove from index
  auto bl_ind = m_blocks_index.find(get_block_hash(bei.bl));
  if (bl_ind == m_blocks_index.end())
  {
    LOG_ERROR("pop_block_from_blockchain: blockchain id not found in index");
    m_popping_block = false;
    return false;
  }
  m_blocks_index.erase(bl_ind);
  //pop block from core
  m_blocks.pop_back();
  m_popping_block = false;
  m_tx_pool.on_blockchain_dec(m_blocks.size()-1, get_tail_id());
  return true;
}
//------------------------------------------------------------------
bool blockchain_storage::reset_and_set_genesis_block(const block& b)
{
  CRITICAL_REGION_LOCAL(m_blockchain_lock);
  m_transactions.clear();
  m_spent_keys.clear();
  m_blocks.clear();
  m_blocks_index.clear();
  m_alternative_chains.clear();
  m_outputs.clear();

  block_verification_context bvc = boost::value_initialized<block_verification_context>();
  add_new_block(b, bvc);
  return bvc.m_added_to_main_chain && !bvc.m_verifivation_failed;
}
//------------------------------------------------------------------
namespace bs_visitor_detail {
  struct purge_transaction_visitor: tx_input_visitor_base
  {
    using tx_input_visitor_base::operator();
    blockchain_storage& b;
    purge_transaction_visitor(blockchain_storage& b_in): b(b_in) {}

    bool operator()(const txin_to_key& inp) const
    {
      //const crypto::key_image& ki = inp.k_image;
      auto r = b.m_spent_keys.find(inp.k_image);
      if (r == b.m_spent_keys.end())
      {
        LOG_ERROR("purge_transaction_data_from_blockchain: key image in transaction not found");
        return false;
      }
      
      b.m_spent_keys.erase(r);
      
      // re-do votes from this key_image if there were any
      if (!b.m_vote_histories[inp.k_image].empty())
      {
        CHECK_AND_ASSERT_MES(b.reapply_votes(b.m_vote_histories[inp.k_image].back()), false,
                             "purge_transaction_data_from_blockchain: could not re-apply key image votes");
      }
      
      return true;
    }
    bool operator()(const txin_gen& inp) const
    {
      // height is checked in prevalidate_miner_tx
      return true;
    }
    bool operator()(const txin_mint& inp) const
    {
      auto r = b.m_currencies.find(inp.currency);
      if (r == b.m_currencies.end())
      {
        LOG_ERROR("purge_block_data_from_blockchain: currency in mint not found");
        return false;
      }
      
      auto& ci = r->second;
      if (ci.description != inp.description || ci.decimals != inp.decimals || ci.total_amount_minted != inp.amount || ci.remint_key() != inp.remint_key || ci.remint_key_history.size() != 1)
      {
        LOG_ERROR("purge_block_data_from_blockchain: currency data doesn't match txin_mint");
        return false;
      }
      
      if (!inp.description.empty())
      {
        auto rr = b.m_used_currency_descriptions.find(inp.description);
        if (rr == b.m_used_currency_descriptions.end())
        {
          LOG_ERROR("purge_block_data_from_blockchain: currency description isn't registered");
          return false;
        }
      
        b.m_used_currency_descriptions.erase(rr);
      }
      b.m_currencies.erase(r);
      return true;
    }
    
    bool operator()(const txin_remint& inp) const
    {
      auto r = b.m_currencies.find(inp.currency);
      if (r == b.m_currencies.end())
      {
        LOG_ERROR("purge_block_data_from_blockchain: currency in remint not found");
        return false;
      }
      
      auto& ci = r->second;
      if (ci.remint_key() != inp.new_remint_key)
      {
        LOG_ERROR("purge_block_data_from_blockchain: currency's latest remint key does not match new_remint_key");
        return false;
      }
      
      ci.remint_key_history.pop_back();
      if (!sub_amount(ci.total_amount_minted, inp.amount))
      {
        LOG_ERROR("purge_block_data_from_blockchain: underflow subbing minted amount from total_amount_minted");
        return false;
      }
      return true;
    }

    bool operator()(const txin_create_contract& inp) const
    {
      auto r = b.m_contracts.find(inp.contract);
      if (r == b.m_contracts.end())
      {
        LOG_ERROR("purge_block_data_from_blockchain: contract in txin_create_contract not found to purge");
        return false;
      }
      
      auto& ci = r->second;
      
      if (ci.contract != inp.contract || ci.description != inp.description ||
          ci.grading_key != inp.grading_key || ci.fee_scale != inp.fee_scale ||
          ci.expiry_block != inp.expiry_block || ci.default_grade != inp.default_grade)
      {
        LOG_ERROR("purge_block_data_from_blockchain: contract data doesn't match txin_create_contract: "
                  << ci.contract << " vs. " << inp.contract << ENDL
                  << ci.description << " vs. " << inp.description
                  << ci.grading_key << " vs. " << inp.grading_key << ENDL
                  << ci.fee_scale << " vs. " << inp.fee_scale << ENDL
                  << ci.expiry_block << " vs. " << inp.expiry_block << ENDL
                  << ci.default_grade << " vs. " << inp.default_grade);
        return false;
      }
      
      if (ci.is_graded != false || ci.grade != 0)
      {
        LOG_ERROR("purge_block_data_from_blockchain: contract data is not in initial state when undoing a txin_create_contract");
        return false;
      }
      
      if (!inp.description.empty())
      {
        auto rr = b.m_used_currency_descriptions.find(inp.description);
        if (rr == b.m_used_currency_descriptions.end())
        {
          LOG_ERROR("purge_block_data_from_blockchain: contract description isn't registered");
          return false;
        }
        
        b.m_used_currency_descriptions.erase(rr);
      }
      b.m_contracts.erase(r);
      return true;
    }
    bool operator()(const txin_mint_contract& inp) const
    {
      auto r = b.m_contracts.find(inp.contract);
      if (r == b.m_contracts.end())
      {
        LOG_ERROR("purge_block_data_from_blockchain: contract in txin_mint_contract not found");
        return false;
      }
      
      auto& ci = r->second;
      
      if (!sub_amount(ci.total_amount_minted[inp.backed_by_currency], inp.amount))
      {
        LOG_ERROR("purge_block_data_from_blockchain: underflow subbing from total amount minted in contract for backing currency"
                  << inp.backed_by_currency);
        return false;
      }
      
      return true;
    }
    bool operator()(const txin_grade_contract& inp) const
    {
      auto r = b.m_contracts.find(inp.contract);
      if (r == b.m_contracts.end())
      {
        LOG_ERROR("purge_block_data_from_blockchain: contract in txin_grade_contract not found");
        return false;
      }
      
      auto& ci = r->second;
      
      if (!ci.is_graded)
      {
        LOG_ERROR("purge_block_data_from_blockchain: contract in txin_grade_contract is not graded");
        return false;
      }
      
      ci.is_graded = false;
      ci.grade = 0;
      
      return true;
    }
    bool operator()(const txin_resolve_bc_coins& inp) const
    {
      return true;
    }
    bool operator()(const txin_fuse_bc_coins& inp) const
    {
      auto r = b.m_contracts.find(inp.contract);
      if (r == b.m_contracts.end())
      {
        LOG_ERROR("purge_block_data_from_blockchain: contract in mint_contract not found");
        return false;
      }
      
      auto& ci = r->second;
      
      if (!add_amount(ci.total_amount_minted[inp.backing_currency], inp.amount))
      {
        LOG_ERROR("purge_block_data_from_blockchain: overflow undoing txin_fuse_bc_coins for backing currency"
                  << inp.backing_currency);
        return false;
      }
      
      return true;
    }
    
    bool operator()(const txin_register_delegate& inp) const
    {
      auto r = b.m_delegates.find(inp.delegate_id);
      if(r == b.m_delegates.end())
      {
        LOG_ERROR("purge_block_data_from_blockchain: delegate_id in transaction not found");
        return false;
      }
      
      b.m_delegates.erase(r);
      return true;
    }
    
    bool operator()(const txin_vote& inp) const
    {
      if (b.m_vote_histories[inp.ink.k_image].empty())
      {
        LOG_ERROR("purge_block_data_from_blockchain: vote histories contain no votes for " << inp.ink.k_image);
        return false;
      }
      
      CHECK_AND_ASSERT_MES(inp.seq == b.m_vote_histories[inp.ink.k_image].size() - 1, false,
                           "purge_block_data_from_blockchain: inp.seq/vote_history size mismach");
        
      const auto& latest_votes_inst = b.m_vote_histories[inp.ink.k_image].back();
      CHECK_AND_ASSERT_MES(latest_votes_inst.expected_vote == inp.ink.amount, false,
                           "purge_block_data_from_blockchain: vote amount doesn't match recorded amount");
      
      // ensure there are no votes in blockchain that are not in the input
      BOOST_FOREACH(const auto& item, latest_votes_inst.votes)
      {
        if (inp.votes.count(item.first) == 0)
        {
          LOG_ERROR("purge_block_data_from_blockchain: blockchain contains vote not in txin_vote");
          return false;
        }
      }
      // ensure all votes in the input are in the blockchain
      BOOST_FOREACH(const auto& delegate_id, inp.votes)
      {
        if (latest_votes_inst.votes.find(delegate_id) == latest_votes_inst.votes.end())
        {
          LOG_ERROR("purge_block_data_from_blockchain: txin_vote contains unseen vote");
          return false;
        }
      }
      
      // enforce effective amounts since we are undoing
      CHECK_AND_ASSERT_MES(b.unapply_votes(latest_votes_inst, true), false,
                           "purge_block_data_from_blockchain: could not undo votes");
      
      // remove the vote history
      b.m_vote_histories[inp.ink.k_image].pop_back();
      
      // re-apply the vote history before it
      if (!b.m_vote_histories[inp.ink.k_image].empty())
      {
        CHECK_AND_ASSERT_MES(b.reapply_votes(b.m_vote_histories[inp.ink.k_image].back()), false,
                             "purge_block_data_from_blockchain: could not reapply previous votes");
      }
      
      return true;
    }
  };
}
bool blockchain_storage::purge_transaction_data_from_blockchain(const transaction& tx, bool strict_check)
{
  CRITICAL_REGION_LOCAL(m_blockchain_lock);
  // apply in reverse order
  if (!tools::all_apply_visitor(bs_visitor_detail::purge_transaction_visitor(*this), tx.ins(), tools::identity(), true))
  {
    if (strict_check)
    {
      LOG_ERROR("failed to process purge_transaction_visitor");
      return false;
    }
    else
    {
      return true;
    }
  }

  std::stringstream ss;
  ss << "Purged transaction from blockchain history:" << ENDL;
  ss << "----------------------------------------" << ENDL;
  ss << "tx_id: " << get_transaction_hash(tx) << ENDL;
  ss << "inputs: " << tx.ins().size()
     << ", outs: " << tx.outs().size()
     << ", spend money: " << print_moneys(get_outs_money_amount(tx), *this)
     << ", fee: " << (is_coinbase(tx) ? "0[coinbase]" : print_money(get_tx_fee(tx))) << ENDL;
  ss << "----------------------------------------";
  
  LOG_PRINT_L2(ss.str());
  
  return true;
}
//------------------------------------------------------------------
bool blockchain_storage::purge_transaction_from_blockchain(const crypto::hash& tx_id)
{
  CRITICAL_REGION_LOCAL(m_blockchain_lock);
  auto tx_index_it = m_transactions.find(tx_id);
  CHECK_AND_ASSERT_MES(tx_index_it != m_transactions.end(), false, "purge_block_data_from_blockchain: transaction not found in blockchain index!!");
  transaction& tx = tx_index_it->second.tx;

  CHECK_AND_ASSERT_MES(purge_transaction_data_from_blockchain(tx, true), false,
                       "purge_transaction_from_blockchain: failed to purge transaction data, stuck! Must re-index");

  if(!is_coinbase(tx))
  {
    cryptonote::tx_verification_context tvc = AUTO_VAL_INIT(tvc);
    bool r = m_tx_pool.add_tx(tx, tvc, true);
    if (!r)
    {
      LOG_PRINT_YELLOW("WARNING: failed to re-add purged transaction to txpool, transaction will be lost",
                       LOG_LEVEL_0);
    }
  }

  bool res = pop_transaction_from_global_index(tx, tx_id);
  m_transactions.erase(tx_index_it);
  LOG_PRINT_L1("Removed transaction from blockchain history:" << tx_id << ENDL);
  return res;
}
//------------------------------------------------------------------
bool blockchain_storage::purge_block_data_from_blockchain(const block& bl, size_t processed_tx_count)
{
  CRITICAL_REGION_LOCAL(m_blockchain_lock);

  bool res = true;
  CHECK_AND_ASSERT_MES(processed_tx_count <= bl.tx_hashes.size(), false, "wrong processed_tx_count in purge_block_data_from_blockchain");
  for(size_t count = 0; count != processed_tx_count; count++)
  {
    res = purge_transaction_from_blockchain(bl.tx_hashes[(processed_tx_count -1)- count]) && res;
  }

  res = purge_transaction_from_blockchain(get_transaction_hash(bl.miner_tx)) && res;

  return res;
}
//------------------------------------------------------------------
crypto::hash blockchain_storage::get_tail_id(uint64_t& height)
{
  CRITICAL_REGION_LOCAL(m_blockchain_lock);
  height = get_current_blockchain_height()-1;
  return get_tail_id();
}
//------------------------------------------------------------------
crypto::hash blockchain_storage::get_tail_id()
{
  CRITICAL_REGION_LOCAL(m_blockchain_lock);
  crypto::hash id = null_hash;
  if(m_blocks.size())
  {
    get_block_hash(m_blocks.back().bl, id);
  }
  return id;
}
//------------------------------------------------------------------
bool blockchain_storage::get_short_chain_history(std::list<crypto::hash>& ids)
{
  CRITICAL_REGION_LOCAL(m_blockchain_lock);
  size_t i = 0;
  size_t current_multiplier = 1;
  size_t sz = m_blocks.size();
  if(!sz)
    return true;
  size_t current_back_offset = 1;
  bool genesis_included = false;
  while(current_back_offset < sz)
  {
    ids.push_back(get_block_hash(m_blocks[sz-current_back_offset].bl));
    if(sz-current_back_offset == 0)
      genesis_included = true;
    if(i < 10)
    {
      ++current_back_offset;
    }else
    {
      current_back_offset += current_multiplier *= 2;
    }
    ++i;
  }
  if(!genesis_included)
    ids.push_back(get_block_hash(m_blocks[0].bl));

  return true;
}
//------------------------------------------------------------------
crypto::hash blockchain_storage::get_block_id_by_height(uint64_t height)
{
  CRITICAL_REGION_LOCAL(m_blockchain_lock);
  if(height >= m_blocks.size())
    return null_hash;

  return get_block_hash(m_blocks[height].bl);
}
//------------------------------------------------------------------
bool blockchain_storage::get_block_by_hash(const crypto::hash &h, block &blk) {
  CRITICAL_REGION_LOCAL(m_blockchain_lock);

  // try to find block in main chain
  blocks_by_id_index::const_iterator it = m_blocks_index.find(h);
  if (m_blocks_index.end() != it) {
    blk = m_blocks[it->second].bl;
    return true;
  }

  // try to find block in alternative chain
  blocks_ext_by_hash::const_iterator it_alt = m_alternative_chains.find(h);
  if (m_alternative_chains.end() != it_alt) {
    blk = it_alt->second.bl;
    return true;
  }

  return false;
}
//------------------------------------------------------------------
void blockchain_storage::get_all_known_block_ids(std::list<crypto::hash> &main, std::list<crypto::hash> &alt, std::list<crypto::hash> &invalid) {
  CRITICAL_REGION_LOCAL(m_blockchain_lock);

  BOOST_FOREACH(blocks_by_id_index::value_type &v, m_blocks_index)
    main.push_back(v.first);

  BOOST_FOREACH(blocks_ext_by_hash::value_type &v, m_alternative_chains)
    alt.push_back(v.first);

  BOOST_FOREACH(blocks_ext_by_hash::value_type &v, m_invalid_blocks)
    invalid.push_back(v.first);
}
//------------------------------------------------------------------
difficulty_type blockchain_storage::get_difficulty_for_next_block()
{
  CRITICAL_REGION_LOCAL(m_blockchain_lock);
  if (config::in_pos_era(m_blocks.size())) {
    return DPOS_BLOCK_DIFFICULTY;
  }
  std::vector<uint64_t> timestamps;
  std::vector<difficulty_type> commulative_difficulties;
  size_t offset = m_blocks.size() - std::min(m_blocks.size(), static_cast<size_t>(DIFFICULTY_BLOCKS_COUNT));
  if(!offset)
    ++offset;//skip genesis block
  for(; offset < m_blocks.size(); offset++)
  {
    timestamps.push_back(m_blocks[offset].bl.timestamp);
    commulative_difficulties.push_back(m_blocks[offset].cumulative_difficulty);
  }
  return next_difficulty(m_blocks.size(), timestamps, commulative_difficulties);
}
//------------------------------------------------------------------
bool blockchain_storage::rollback_blockchain_switching(std::list<block>& original_chain, size_t rollback_height)
{
  CRITICAL_REGION_LOCAL(m_blockchain_lock);
  //remove failed subchain
  for(size_t i = m_blocks.size()-1; i >=rollback_height; i--)
  {
    bool r = pop_block_from_blockchain();
    CHECK_AND_ASSERT_MES(r, false, "PANIC!!! failed to remove block while chain switching during the rollback!");
  }
  //return back original chain
  BOOST_FOREACH(auto& bl, original_chain)
  {
    block_verification_context bvc = boost::value_initialized<block_verification_context>();
    bool r = handle_block_to_main_chain(bl, bvc);
    CHECK_AND_ASSERT_MES(r && bvc.m_added_to_main_chain, false, "PANIC!!! failed to add (again) block while chain switching during the rollback!");
  }

  LOG_PRINT_L0("Rollback success.");
  return true;
}
//------------------------------------------------------------------
bool blockchain_storage::switch_to_alternative_blockchain(std::list<blocks_ext_by_hash::iterator>& alt_chain, bool discard_disconnected_chain)
{
  CRITICAL_REGION_LOCAL(m_blockchain_lock);
  CHECK_AND_ASSERT_MES(alt_chain.size(), false, "switch_to_alternative_blockchain: empty chain passed");

  size_t split_height = alt_chain.front()->second.height;
  CHECK_AND_ASSERT_MES(m_blocks.size() > split_height, false, "switch_to_alternative_blockchain: blockchain size is lower than split height");

  //disconnecting old chain
  std::list<block> disconnected_chain;
  for(size_t i = m_blocks.size()-1; i >=split_height; i--)
  {
    block b = m_blocks[i].bl;
    bool r = pop_block_from_blockchain();
    CHECK_AND_ASSERT_MES(r, false, "failed to remove block on chain switching");
    disconnected_chain.push_front(b);
  }

  //connecting new alternative chain
  for(auto alt_ch_iter = alt_chain.begin(); alt_ch_iter != alt_chain.end(); alt_ch_iter++)
  {
    block_verification_context bvc = boost::value_initialized<block_verification_context>();
    bool r = handle_block_to_main_chain((*alt_ch_iter)->second.bl, bvc);
    if(!r || !bvc.m_added_to_main_chain)
    {
      std::list<blocks_ext_by_hash::iterator> to_erase;
      
      LOG_PRINT_L0("Failed to switch to alternative blockchain");
      rollback_blockchain_switching(disconnected_chain, split_height);
      
      // mark this and rest of blocks depending on this as invalid
      for (; alt_ch_iter != alt_chain.end(); alt_ch_iter++)
      {
        crypto::hash actual_id = get_block_hash((*alt_ch_iter)->second.bl);
        if (actual_id != (*alt_ch_iter)->first)
        {
          LOG_PRINT_RED_L0("Warning: block id didn't match id in m_alternative_chains map");
        }
        add_block_as_invalid((*alt_ch_iter)->second, actual_id);
        LOG_PRINT_L0("The block was inserted as invalid while connecting new alternative chain,  block_id: " << actual_id);
        m_alternative_chains.erase(*alt_ch_iter);
      }
      
      return false;
    }
  }

  if(!discard_disconnected_chain)
  {
    //pushing old chain as alternative chain
    BOOST_FOREACH(auto& old_ch_ent, disconnected_chain)
    {
      block_verification_context bvc = boost::value_initialized<block_verification_context>();
      bool r = handle_alternative_block(old_ch_ent, get_block_hash(old_ch_ent), bvc);
      if(!r)
      {
        LOG_ERROR("Failed to push ex-main chain blocks to alternative chain ");
        rollback_blockchain_switching(disconnected_chain, split_height);
        return false;
      }
    }
  }

  //removing all_chain entries from alternative chain
  BOOST_FOREACH(auto ch_ent, alt_chain)
  {
    m_alternative_chains.erase(ch_ent);
  }

  LOG_PRINT_GREEN("REORGANIZE SUCCESS! on height: " << split_height << ", new blockchain size: " << m_blocks.size(), LOG_LEVEL_0);
  return true;
}
//------------------------------------------------------------------
difficulty_type blockchain_storage::get_next_difficulty_for_alternative_chain(const std::list<blocks_ext_by_hash::iterator>& alt_chain, block_extended_info& bei)
{
  if (config::in_pos_era(bei.height))
  {
    LOG_PRINT_L0("get_next_difficulty_for_alternative_chain: bei.height is " << bei.height << ", dpos_switch_block is " << config::dpos_switch_block << ", returning dpos difficulty");
    return DPOS_BLOCK_DIFFICULTY;
  }
  
  std::vector<uint64_t> timestamps;
  std::vector<difficulty_type> commulative_difficulties;
  if(alt_chain.size()< DIFFICULTY_BLOCKS_COUNT)
  {
    CRITICAL_REGION_LOCAL(m_blockchain_lock);
    size_t main_chain_stop_offset = alt_chain.size() ? alt_chain.front()->second.height : bei.height;
    size_t main_chain_count = DIFFICULTY_BLOCKS_COUNT - std::min(static_cast<size_t>(DIFFICULTY_BLOCKS_COUNT), alt_chain.size());
    main_chain_count = std::min(main_chain_count, main_chain_stop_offset);
    size_t main_chain_start_offset = main_chain_stop_offset - main_chain_count;

    if(!main_chain_start_offset)
      ++main_chain_start_offset; //skip genesis block
    for(; main_chain_start_offset < main_chain_stop_offset; ++main_chain_start_offset)
    {
      timestamps.push_back(m_blocks[main_chain_start_offset].bl.timestamp);
      commulative_difficulties.push_back(m_blocks[main_chain_start_offset].cumulative_difficulty);
    }

    CHECK_AND_ASSERT_MES((alt_chain.size() + timestamps.size()) <= DIFFICULTY_BLOCKS_COUNT, false, "Internal error, alt_chain.size()["<< alt_chain.size()
                                                                                    << "] + vtimestampsec.size()[" << timestamps.size() << "] NOT <= DIFFICULTY_WINDOW[]" << DIFFICULTY_BLOCKS_COUNT );
    BOOST_FOREACH(auto it, alt_chain)
    {
      timestamps.push_back(it->second.bl.timestamp);
      commulative_difficulties.push_back(it->second.cumulative_difficulty);
    }
  }else
  {
    timestamps.resize(std::min(alt_chain.size(), static_cast<size_t>(DIFFICULTY_BLOCKS_COUNT)));
    commulative_difficulties.resize(std::min(alt_chain.size(), static_cast<size_t>(DIFFICULTY_BLOCKS_COUNT)));
    size_t count = 0;
    size_t max_i = timestamps.size()-1;
    BOOST_REVERSE_FOREACH(auto it, alt_chain)
    {
      timestamps[max_i - count] = it->second.bl.timestamp;
      commulative_difficulties[max_i - count] = it->second.cumulative_difficulty;
      count++;
      if(count >= DIFFICULTY_BLOCKS_COUNT)
        break;
    }
  }
  return next_difficulty(bei.height, timestamps, commulative_difficulties);
}
//------------------------------------------------------------------
bool blockchain_storage::prevalidate_miner_transaction(const block& b, uint64_t height)
{
  CHECK_AND_ASSERT_MES(b.miner_tx.ins().size() == 1, false, "coinbase transaction in the block has no inputs");
  CHECK_AND_ASSERT_MES(b.miner_tx.ins()[0].type() == typeid(txin_gen), false, "coinbase transaction in the block has the wrong type");
  if(boost::get<txin_gen>(b.miner_tx.ins()[0]).height != height)
  {
    LOG_PRINT_RED_L0("The miner transaction in block has invalid height: " << boost::get<txin_gen>(b.miner_tx.ins()[0]).height << ", expected: " << height);
    return false;
  }
  CHECK_AND_ASSERT_MES(b.miner_tx.unlock_time == height + CRYPTONOTE_MINED_MONEY_UNLOCK_WINDOW,
                  false,
                  "coinbase transaction transaction have wrong unlock time=" << b.miner_tx.unlock_time << ", expected " << height + CRYPTONOTE_MINED_MONEY_UNLOCK_WINDOW);

  //check outs overflow
  if(!check_outputs(b.miner_tx))
  {
    LOG_PRINT_RED_L0("miner transaction have money overflow in block " << get_block_hash(b));
    return false;
  }

  return true;
}
//------------------------------------------------------------------
bool blockchain_storage::validate_miner_transaction(const block& b, size_t cumulative_block_size, uint64_t fee, uint64_t& base_reward, uint64_t already_generated_coins)
{
  //validate reward
  uint64_t money_in_use = 0;
  BOOST_FOREACH(const auto& o, b.miner_tx.outs())
    money_in_use += o.amount;

  std::vector<size_t> last_blocks_sizes;
  get_last_n_blocks_sizes(last_blocks_sizes, CRYPTONOTE_REWARD_BLOCKS_WINDOW);
  if(!get_block_reward(misc_utils::median(last_blocks_sizes), cumulative_block_size, already_generated_coins, get_block_height(b), base_reward))
  {
    LOG_PRINT_L0("block size " << cumulative_block_size << " is bigger than allowed for this blockchain");
    return false;
  }
  if(base_reward + fee < money_in_use)
  {
    LOG_ERROR("coinbase transaction spend too much money (" << print_money(money_in_use) << "). Block reward is " << print_money(base_reward + fee) << "(" << print_money(base_reward) << "+" << print_money(fee) << ")");
    return false;
  }
  if(base_reward + fee != money_in_use)
  {
    LOG_ERROR("coinbase transaction doesn't use full amount of block reward:  spent: "
                            << print_money(money_in_use) << ",  block reward " << print_money(base_reward + fee) << "(" << print_money(base_reward) << "+" << print_money(fee) << ")");
    return false;
  }
  return true;
}
//------------------------------------------------------------------
bool blockchain_storage::get_backward_blocks_sizes(size_t from_height, std::vector<size_t>& sz, size_t count)
{
  CRITICAL_REGION_LOCAL(m_blockchain_lock);
  CHECK_AND_ASSERT_MES(from_height < m_blocks.size(), false, "Internal error: get_backward_blocks_sizes called with from_height=" << from_height << ", blockchain height = " << m_blocks.size());

  size_t start_offset = (from_height+1) - std::min((from_height+1), count);
  for(size_t i = start_offset; i != from_height+1; i++)
    sz.push_back(m_blocks[i].block_cumulative_size);

  return true;
}
//------------------------------------------------------------------
bool blockchain_storage::get_last_n_blocks_sizes(std::vector<size_t>& sz, size_t count)
{
  CRITICAL_REGION_LOCAL(m_blockchain_lock);
  if(!m_blocks.size())
    return true;
  return get_backward_blocks_sizes(m_blocks.size() -1, sz, count);
}
//------------------------------------------------------------------
uint64_t blockchain_storage::get_current_comulative_blocksize_limit()
{
  return m_current_block_cumul_sz_limit;
}
//------------------------------------------------------------------
bool blockchain_storage::create_block_template(block& b, const account_public_address& miner_address, difficulty_type& diffic, uint64_t& height, const blobdata& ex_nonce, bool dpos_block)
{
  size_t median_size;
  uint64_t already_generated_coins;
  uint64_t fee_dpos, fee_pow;

  CRITICAL_REGION_BEGIN(m_blockchain_lock);
  CHECK_AND_ASSERT_MES(!m_popping_block, false, "Shouldn't create_block_template while popping block");
  
  b.major_version = dpos_block ? DPOS_BLOCK_MAJOR_VERSION : POW_BLOCK_MAJOR_VERSION;
  b.minor_version = dpos_block ? DPOS_BLOCK_MINOR_VERSION : POW_BLOCK_MINOR_VERSION;
  b.prev_id = get_tail_id();
  b.timestamp = get_adjusted_time();
  height = m_blocks.size();
  diffic = get_difficulty_for_next_block();
  CHECK_AND_ASSERT_MES(diffic, false, "difficulty owverhead.");
  
  if (dpos_block)
  {
    CHECK_AND_ASSERT_MES(config::in_pos_era(height), false,
                         "Invalid height for dpos block: " << height << ", switch at " << config::dpos_switch_block);
    
    uint64_t prev_timestamp = m_blocks.back().bl.timestamp;
    if (b.timestamp < prev_timestamp + CRYPTONOTE_DPOS_BLOCK_MINIMUM_BLOCK_SPACING)
    {
      LOG_ERROR("Not time yet for next dpos block, must wait " << (CRYPTONOTE_DPOS_BLOCK_MINIMUM_BLOCK_SPACING - (b.timestamp - prev_timestamp)) << " more seconds");
      return false;
    }
  }
  else
  {
    CHECK_AND_ASSERT_MES(!config::in_pos_era(height), false,
                         "Invalid height for pow block: " << height << ", switch at " << config::dpos_switch_block);
  }
  
  median_size = m_current_block_cumul_sz_limit / 2;
  already_generated_coins = m_blocks.back().already_generated_coins;
  
  if (dpos_block)
  {
    // fee for dpos block is average of past day's block fees
    fee_dpos = average_past_block_fees(height);
  }

  CRITICAL_REGION_END();

  size_t txs_size;
  if (!m_tx_pool.fill_block_template(b, median_size, already_generated_coins, txs_size, fee_pow)) {
    return false;
  }
  
#if defined(DEBUG_CREATE_BLOCK_TEMPLATE)
  size_t real_txs_size = 0;
  uint64_t real_fee = 0;
  CRITICAL_REGION_BEGIN(m_tx_pool.m_transactions_lock);
  BOOST_FOREACH(crypto::hash &cur_hash, b.tx_hashes) {
    auto cur_res = m_tx_pool.m_transactions.find(cur_hash);
    if (cur_res == m_tx_pool.m_transactions.end()) {
      LOG_ERROR("Creating block template: error: transaction not found");
      continue;
    }
    tx_memory_pool::tx_details &cur_tx = cur_res->second;
    real_txs_size += cur_tx.blob_size;
    real_fee += cur_tx.fee;
    if (cur_tx.blob_size != get_object_blobsize(cur_tx.tx)) {
      LOG_ERROR("Creating block template: error: invalid transaction size");
    }
    uint64_t fee;
    if (!check_inputs_outputs(cur_tx.tx, fee)) {
      LOG_ERROR("Creating block template: error: cannot get inputs amount");
    } else if (cur_tx.fee != fee) {
      LOG_ERROR("Creating block template: error: invalid fee");
    }
  }
  if (txs_size != real_txs_size) {
    LOG_ERROR("Creating block template: error: wrongly calculated transaction size");
  }
  if (fee_pow != real_fee) {
    LOG_ERROR("Creating block template: error: wrongly calculated fee");
  }
  CRITICAL_REGION_END();
  LOG_PRINT_L1("Creating block template: height " << height <<
    ", median size " << median_size <<
    ", already generated coins " << already_generated_coins <<
    ", transaction size " << txs_size <<
    ", fee " << fee);
#endif
  
  uint64_t fee = dpos_block ? fee_dpos : fee_pow;
  
  if (dpos_block)
  {
    CHECK_AND_ASSERT_MES(get_signing_delegate(m_blocks.back().bl, b.timestamp, b.signing_delegate_id),
                         false, "Could not get_signing_delegate");
  }
  /*
     two-phase miner transaction generation: we don't know exact block size until we prepare block, but we don't know reward until we know
     block size, so first miner transaction generated with fake amount of money, and with phase we know think we know expected block size
  */
  //make blocks coin-base tx looks close to real coinbase tx to get truthful blob size
  bool r = construct_miner_tx(height, median_size, already_generated_coins, txs_size, fee, miner_address, b.miner_tx, ex_nonce, 11);
  CHECK_AND_ASSERT_MES(r, false, "Failed to construc miner tx, first chance");
  size_t cumulative_size = txs_size + get_object_blobsize(b.miner_tx);
#if defined(DEBUG_CREATE_BLOCK_TEMPLATE)
  LOG_PRINT_L1("Creating block template: miner tx size " << get_object_blobsize(b.miner_tx) <<
    ", cumulative size " << cumulative_size);
#endif
  for (size_t try_count = 0; try_count != 10; ++try_count) {
    r = construct_miner_tx(height, median_size, already_generated_coins, cumulative_size, fee, miner_address, b.miner_tx, ex_nonce, 11);

    CHECK_AND_ASSERT_MES(r, false, "Failed to construc miner tx, second chance");
    size_t coinbase_blob_size = get_object_blobsize(b.miner_tx);
    if (coinbase_blob_size > cumulative_size - txs_size) {
      cumulative_size = txs_size + coinbase_blob_size;
#if defined(DEBUG_CREATE_BLOCK_TEMPLATE)
      LOG_PRINT_L1("Creating block template: miner tx size " << coinbase_blob_size <<
        ", cumulative size " << cumulative_size << " is greater then before");
#endif
      continue;
    }

    if (coinbase_blob_size < cumulative_size - txs_size) {
      size_t delta = cumulative_size - txs_size - coinbase_blob_size;
#if defined(DEBUG_CREATE_BLOCK_TEMPLATE)
      LOG_PRINT_L1("Creating block template: miner tx size " << coinbase_blob_size <<
        ", cumulative size " << txs_size + coinbase_blob_size <<
        " is less then before, adding " << delta << " zero bytes");
#endif
      b.miner_tx.extra.insert(b.miner_tx.extra.end(), delta, 0);
      //here  could be 1 byte difference, because of extra field counter is varint, and it can become from 1-byte len to 2-bytes len.
      if (cumulative_size != txs_size + get_object_blobsize(b.miner_tx)) {
        CHECK_AND_ASSERT_MES(cumulative_size + 1 == txs_size + get_object_blobsize(b.miner_tx), false, "unexpected case: cumulative_size=" << cumulative_size << " + 1 is not equal txs_cumulative_size=" << txs_size << " + get_object_blobsize(b.miner_tx)=" << get_object_blobsize(b.miner_tx));
        b.miner_tx.extra.resize(b.miner_tx.extra.size() - 1);
        if (cumulative_size != txs_size + get_object_blobsize(b.miner_tx)) {
          //fuck, not lucky, -1 makes varint-counter size smaller, in that case we continue to grow with cumulative_size
          LOG_PRINT_RED("Miner tx creation have no luck with delta_extra size = " << delta << " and " << delta - 1 , LOG_LEVEL_2);
          cumulative_size += delta - 1;
          continue;
        }
        LOG_PRINT_GREEN("Setting extra for block: " << b.miner_tx.extra.size() << ", try_count=" << try_count, LOG_LEVEL_1);
      }
    }
    CHECK_AND_ASSERT_MES(cumulative_size == txs_size + get_object_blobsize(b.miner_tx), false, "unexpected case: cumulative_size=" << cumulative_size << " is not equal txs_cumulative_size=" << txs_size << " + get_object_blobsize(b.miner_tx)=" << get_object_blobsize(b.miner_tx));
#if defined(DEBUG_CREATE_BLOCK_TEMPLATE)
    LOG_PRINT_L1("Creating block template: miner tx size " << coinbase_blob_size <<
      ", cumulative size " << cumulative_size << " is now good");
#endif
    return true;
  }
  LOG_ERROR("Failed to create_block_template with " << 10 << " tries");
  return false;
}
//------------------------------------------------------------------
bool blockchain_storage::complete_timestamps_vector(uint64_t start_top_height, std::vector<uint64_t>& timestamps)
{

  if(timestamps.size() >= BLOCKCHAIN_TIMESTAMP_CHECK_WINDOW)
    return true;

  CRITICAL_REGION_LOCAL(m_blockchain_lock);
  size_t need_elements = BLOCKCHAIN_TIMESTAMP_CHECK_WINDOW - timestamps.size();
  CHECK_AND_ASSERT_MES(start_top_height < m_blocks.size(), false, "internal error: passed start_height = " << start_top_height << " not less then m_blocks.size()=" << m_blocks.size());
  size_t stop_offset = start_top_height > need_elements ? start_top_height - need_elements:0;
  do
  {
    timestamps.push_back(m_blocks[start_top_height].bl.timestamp);
    if(start_top_height == 0)
      break;
    --start_top_height;
  }while(start_top_height != stop_offset);
  return true;
}
//------------------------------------------------------------------
bool blockchain_storage::handle_alternative_block(const block& b, const crypto::hash& id, block_verification_context& bvc)
{
  CRITICAL_REGION_LOCAL(m_blockchain_lock);

  uint64_t block_height = get_block_height(b);
  if(0 == block_height)
  {
    LOG_ERROR("Block with id: " << string_tools::pod_to_hex(id) << " (as alternative) have wrong miner transaction");
    bvc.m_verifivation_failed = true;
    return false;
  }
  if (!m_checkpoints.is_alternative_block_allowed(get_current_blockchain_height(), block_height))
  {
    LOG_PRINT_RED_L0("Block with id: " << id
      << ENDL << " can't be accepted for alternative chain, block height: " << block_height
      << ENDL << " blockchain height: " << get_current_blockchain_height());
    bvc.m_verifivation_failed = true;
    return false;
  }

  //block is not related with head of main chain
  //first of all - look in alternative chains container
  auto it_main_prev = m_blocks_index.find(b.prev_id);
  auto it_prev = m_alternative_chains.find(b.prev_id);
  if(it_prev != m_alternative_chains.end() || it_main_prev != m_blocks_index.end())
  {
    //we have new block in alternative chain

    //build alternative subchain, front -> mainchain, back -> alternative head
    blocks_ext_by_hash::iterator alt_it = it_prev; //m_alternative_chains.find()
    std::list<blocks_ext_by_hash::iterator> alt_chain;
    std::vector<uint64_t> timestamps;
    while(alt_it != m_alternative_chains.end())
    {
      alt_chain.push_front(alt_it);
      timestamps.push_back(alt_it->second.bl.timestamp);
      alt_it = m_alternative_chains.find(alt_it->second.bl.prev_id);
    }

    if(alt_chain.size())
    {
      //make sure that it has right connection to main chain
      CHECK_AND_ASSERT_MES(m_blocks.size() > alt_chain.front()->second.height, false, "main blockchain wrong height");
      crypto::hash h = null_hash;
      get_block_hash(m_blocks[alt_chain.front()->second.height - 1].bl, h);
      CHECK_AND_ASSERT_MES(h == alt_chain.front()->second.bl.prev_id, false, "alternative chain have wrong connection to main chain");
      complete_timestamps_vector(alt_chain.front()->second.height - 1, timestamps);
    }else
    {
      CHECK_AND_ASSERT_MES(it_main_prev != m_blocks_index.end(), false, "internal error: broken imperative condition it_main_prev != m_blocks_index.end()");
      complete_timestamps_vector(it_main_prev->second, timestamps);
    }
    //check timestamp correct
    if(!check_block_timestamp(timestamps, b))
    {
      LOG_PRINT_RED_L0("Block with id: " << id
        << ENDL << " for alternative chain, have invalid timestamp: " << b.timestamp);
      //add_block_as_invalid(b, id);//do not add blocks to invalid storage before proof of work check was passed
      bvc.m_verifivation_failed = true;
      return false;
    }

    block_extended_info bei = boost::value_initialized<block_extended_info>();
    bei.bl = b;
    bei.height = alt_chain.size() ? it_prev->second.height + 1 : it_main_prev->second + 1;

    bool is_a_checkpoint;
    if(!m_checkpoints.check_block(bei.height, id, is_a_checkpoint))
    {
      LOG_ERROR("CHECKPOINT VALIDATION FAILED");
      bvc.m_verifivation_failed = true;
      return false;
    }
    
    if (!check_block_type(bei.bl))
    {
      LOG_ERROR("Block with id: " << id << " for alternative chain, has invalid pow/pos type");
      bvc.m_verifivation_failed = true;
      return false;
    }

    //always check proof of work, but
    //don't check delegated proof of stake since it would make keeping track of votes very hard
    //just assume it would work - if it ends up not, the connection will be dropped on the re-org
    //attempt
    m_is_in_checkpoint_zone = false;
    difficulty_type current_diff = get_next_difficulty_for_alternative_chain(alt_chain, bei);
    CHECK_AND_ASSERT_MES(current_diff, false, "!!!!!!! DIFFICULTY OVERHEAD !!!!!!!");
    crypto::hash proof_of_work = null_hash;
    if (is_pow_block(bei.bl) && !check_pow_pos(bei.bl, current_diff, bvc, proof_of_work))
    {
      LOG_PRINT_RED_L0("Block with id: " << id
                       << ENDL << " and height " << bei.height << " (" << get_block_height(bei.bl)
                       << ") for alternative chain, proof of work check failed");
      bvc.m_verifivation_failed = true;
      return false;
    }

    if (!prevalidate_miner_transaction(b, bei.height))
    {
      LOG_PRINT_RED_L0("Block with id: " << string_tools::pod_to_hex(id)
        << " (as alternative) have wrong miner transaction.");
      bvc.m_verifivation_failed = true;
      return false;
    }

    bei.cumulative_difficulty = alt_chain.size() ? it_prev->second.cumulative_difficulty: m_blocks[it_main_prev->second].cumulative_difficulty;
    bei.cumulative_difficulty += current_diff;

#ifdef _DEBUG
    auto i_dres = m_alternative_chains.find(id);
    CHECK_AND_ASSERT_MES(i_dres == m_alternative_chains.end(), false, "insertion of new alternative block returned as it already exist");
#endif
    auto i_res = m_alternative_chains.insert(blocks_ext_by_hash::value_type(id, bei));
    CHECK_AND_ASSERT_MES(i_res.second, false, "insertion of new alternative block returned as it already exist");
    alt_chain.push_back(i_res.first);

    if(is_a_checkpoint)
    {
      //do reorganize!
      LOG_PRINT_GREEN("###### REORGANIZE on height: " << alt_chain.front()->second.height << " of " << m_blocks.size() - 1 <<
        ", checkpoint is found in alternative chain on height " << bei.height, LOG_LEVEL_0);
      bool r = switch_to_alternative_blockchain(alt_chain, true);
      if(r) bvc.m_added_to_main_chain = true;
      else bvc.m_verifivation_failed = true;
      return r;
    }else if(m_blocks.back().cumulative_difficulty < bei.cumulative_difficulty) //check if difficulty bigger then in main chain
    {
      //do reorganize!
      LOG_PRINT_GREEN("###### REORGANIZE on height: " << alt_chain.front()->second.height << " of " << m_blocks.size() - 1 << " with cum_difficulty " << m_blocks.back().cumulative_difficulty
        << ENDL << " alternative blockchain size: " << alt_chain.size() << " with cum_difficulty " << bei.cumulative_difficulty, LOG_LEVEL_0);
      bool r = switch_to_alternative_blockchain(alt_chain, false);
      if(r) bvc.m_added_to_main_chain = true;
      else bvc.m_verifivation_failed = true;
      return r;
    }else
    {
      LOG_PRINT_BLUE("----- BLOCK ADDED AS ALTERNATIVE ON HEIGHT " << bei.height
        << ENDL << "id:\t" << id
        << ENDL << "PoW:\t" << proof_of_work
        << ENDL << "difficulty:\t" << current_diff, LOG_LEVEL_0);
      return true;
    }
  }else
  {
    //block orphaned
    bvc.m_marked_as_orphaned = true;
    LOG_PRINT_RED_L0("Block recognized as orphaned and rejected, id = " << id << " prev_id = " << b.prev_id);
  }

  return true;
}
//------------------------------------------------------------------
bool blockchain_storage::get_blocks(uint64_t start_offset, size_t count, std::list<block>& blocks, std::list<transaction>& txs)
{
  CRITICAL_REGION_LOCAL(m_blockchain_lock);
  if(start_offset >= m_blocks.size())
    return false;
  for(size_t i = start_offset; i < start_offset + count && i < m_blocks.size();i++)
  {
    blocks.push_back(m_blocks[i].bl);
    std::list<crypto::hash> missed_ids;
    get_transactions(m_blocks[i].bl.tx_hashes, txs, missed_ids);
    CHECK_AND_ASSERT_MES(!missed_ids.size(), false, "have missed transactions in own block in main blockchain");
  }

  return true;
}
//------------------------------------------------------------------
bool blockchain_storage::get_blocks(uint64_t start_offset, size_t count, std::list<block>& blocks)
{
  CRITICAL_REGION_LOCAL(m_blockchain_lock);
  if(start_offset >= m_blocks.size())
    return false;

  for(size_t i = start_offset; i < start_offset + count && i < m_blocks.size();i++)
    blocks.push_back(m_blocks[i].bl);
  return true;
}
//------------------------------------------------------------------
bool blockchain_storage::handle_get_objects(NOTIFY_REQUEST_GET_OBJECTS::request& arg, NOTIFY_RESPONSE_GET_OBJECTS::request& rsp)
{
  CRITICAL_REGION_LOCAL(m_blockchain_lock);
  rsp.current_blockchain_height = get_current_blockchain_height();
  std::list<block> blocks;
  get_blocks(arg.blocks, blocks, rsp.missed_ids);

  bool no_more_blocks = false;
  
  BOOST_FOREACH(const auto& bl, blocks)
  {
    crypto::hash block_id = get_block_hash(bl);
   
    //pack signed hashes
    crypto::hash_cache::signed_hash_entry sigent;
    if (crypto::g_hash_cache.get_signed_longhash_entry(block_id, sigent))
      rsp.signed_hashes.push_back(sigent);
    else if (arg.require_signed_hashes)
        no_more_blocks = true; // other client won't process them anyway, so don't send them
    
    if (no_more_blocks)
    {
      rsp.blocks_not_sent.push_back(block_id);
      continue;
    }
    
    std::list<crypto::hash> missed_tx_id;
    std::list<transaction> txs;
    get_transactions(bl.tx_hashes, txs, rsp.missed_ids);
    CHECK_AND_ASSERT_MES(!missed_tx_id.size(), false, "Internal error: have missed missed_tx_id.size()=" << missed_tx_id.size()
      << ENDL << "for block id = " << block_id);
    rsp.blocks.push_back(block_complete_entry());
    block_complete_entry& e = rsp.blocks.back();
    //pack block
    e.block = t_serializable_object_to_blob(bl);
    //pack transactions
    BOOST_FOREACH(transaction& tx, txs)
      e.txs.push_back(t_serializable_object_to_blob(tx));
  }
  //get another transactions, if need
  std::list<transaction> txs;
  get_transactions(arg.txs, txs, rsp.missed_ids);
  //pack aside transactions
  BOOST_FOREACH(const auto& tx, txs)
    rsp.txs.push_back(t_serializable_object_to_blob(tx));
  //pack extra signed hashes
  BOOST_FOREACH(const auto& block_id, arg.signed_hashes)
  {
    crypto::hash_cache::signed_hash_entry sigent;
    if (crypto::g_hash_cache.get_signed_longhash_entry(block_id, sigent))
      rsp.signed_hashes.push_back(sigent);
    // else
    //   rsp.missed_signed_hashes.push_back(block_id);
  }

  return true;
}
//------------------------------------------------------------------
bool blockchain_storage::get_alternative_blocks(std::list<block>& blocks)
{
  CRITICAL_REGION_LOCAL(m_blockchain_lock);

  BOOST_FOREACH(const auto& alt_bl, m_alternative_chains)
  {
    blocks.push_back(alt_bl.second.bl);
  }
  return true;
}
//------------------------------------------------------------------
size_t blockchain_storage::get_alternative_blocks_count()
{
  CRITICAL_REGION_LOCAL(m_blockchain_lock);
  return m_alternative_chains.size();
}
//------------------------------------------------------------------
bool blockchain_storage::add_out_to_get_random_outs(std::vector<std::pair<crypto::hash, size_t> >& amount_outs, COMMAND_RPC_GET_RANDOM_OUTPUTS_FOR_AMOUNTS::outs_for_amount& result_outs, uint64_t amount, size_t i)
{
  CRITICAL_REGION_LOCAL(m_blockchain_lock);
  transactions_container::iterator tx_it = m_transactions.find(amount_outs[i].first);
  CHECK_AND_ASSERT_MES(tx_it != m_transactions.end(), false, "internal error: transaction with id " << amount_outs[i].first << ENDL <<
    ", used in mounts global index for amount=" << amount << ": i=" << i << "not found in transactions index");
  CHECK_AND_ASSERT_MES(tx_it->second.tx.outs().size() > amount_outs[i].second, false, "internal error: in global outs index, transaction out index="
    << amount_outs[i].second << " more than transaction outputs = " << tx_it->second.tx.outs().size() << ", for tx id = " << amount_outs[i].first);
  transaction& tx = tx_it->second.tx;
  CHECK_AND_ASSERT_MES(tx.outs()[amount_outs[i].second].target.type() == typeid(txout_to_key), false, "unknown tx out type");

  //check if transaction is unlocked
  if(!is_tx_spendtime_unlocked(tx.unlock_time))
    return false;

  COMMAND_RPC_GET_RANDOM_OUTPUTS_FOR_AMOUNTS::out_entry& oen = *result_outs.outs.insert(result_outs.outs.end(), COMMAND_RPC_GET_RANDOM_OUTPUTS_FOR_AMOUNTS::out_entry());
  oen.global_amount_index = i;
  oen.out_key = boost::get<txout_to_key>(tx.outs()[amount_outs[i].second].target).key;
  return true;
}
//------------------------------------------------------------------
size_t blockchain_storage::find_end_of_allowed_index(const std::vector<std::pair<crypto::hash, size_t> >& amount_outs)
{
  CRITICAL_REGION_LOCAL(m_blockchain_lock);
  if(!amount_outs.size())
    return 0;
  size_t i = amount_outs.size();
  do
  {
    --i;
    transactions_container::iterator it = m_transactions.find(amount_outs[i].first);
    CHECK_AND_ASSERT_MES(it != m_transactions.end(), 0, "internal error: failed to find transaction from outputs index with tx_id=" << amount_outs[i].first);
    if(it->second.m_keeper_block_height + CRYPTONOTE_MINED_MONEY_UNLOCK_WINDOW <= get_current_blockchain_height() )
      return i+1;
  } while (i != 0);
  return 0;
}
//------------------------------------------------------------------
bool blockchain_storage::get_random_outs_for_amounts(const COMMAND_RPC_GET_RANDOM_OUTPUTS_FOR_AMOUNTS::request& req, COMMAND_RPC_GET_RANDOM_OUTPUTS_FOR_AMOUNTS::response& res)
{
  srand(static_cast<unsigned int>(get_adjusted_time()));
  coin_type typ = CP_XPB; //res.type = req.type;
  
  CRITICAL_REGION_LOCAL(m_blockchain_lock);
  BOOST_FOREACH(uint64_t amount, req.amounts)
  {
    COMMAND_RPC_GET_RANDOM_OUTPUTS_FOR_AMOUNTS::outs_for_amount& result_outs = *res.outs.insert(res.outs.end(), COMMAND_RPC_GET_RANDOM_OUTPUTS_FOR_AMOUNTS::outs_for_amount());
    result_outs.amount = amount;
    auto it = m_outputs.find(std::make_pair(typ /*req.type*/, amount));
    if(it == m_outputs.end())
    {
      LOG_ERROR("COMMAND_RPC_GET_RANDOM_OUTPUTS_FOR_AMOUNTS: not outs for amount " << amount << ", wallet should use some real outs when it lookup for some mix, so, at least one out for this amount should exist");
      continue;//actually this is strange situation, wallet should use some real outs when it lookup for some mix, so, at least one out for this amount should exist
    }
    std::vector<std::pair<crypto::hash, size_t> >& amount_outs  = it->second;
    //it is not good idea to use top fresh outs, because it increases possibility of transaction canceling on split
    //lets find upper bound of not fresh outs
    size_t up_index_limit = find_end_of_allowed_index(amount_outs);
    CHECK_AND_ASSERT_MES(up_index_limit <= amount_outs.size(), false, "internal error: find_end_of_allowed_index returned wrong index=" << up_index_limit << ", with amount_outs.size = " << amount_outs.size());
    if(amount_outs.size() > req.outs_count)
    {
      std::set<size_t> used;
      size_t try_count = 0;
      for(uint64_t j = 0; j != req.outs_count && try_count < up_index_limit;)
      {
        size_t i = rand()%up_index_limit;
        if(used.count(i))
          continue;
        bool added = add_out_to_get_random_outs(amount_outs, result_outs, amount, i);
        used.insert(i);
        if(added)
          ++j;
        ++try_count;
      }
    }else
    {
      for(size_t i = 0; i != up_index_limit; i++)
        add_out_to_get_random_outs(amount_outs, result_outs, amount, i);
    }
  }
  return true;
}
//------------------------------------------------------------------
bool blockchain_storage::find_blockchain_supplement(const std::list<crypto::hash>& qblock_ids, uint64_t& starter_offset)
{
  CRITICAL_REGION_LOCAL(m_blockchain_lock);

  if(!qblock_ids.size() /*|| !req.m_total_height*/)
  {
    LOG_ERROR("Client sent wrong NOTIFY_REQUEST_CHAIN: m_block_ids.size()=" << qblock_ids.size() << /*", m_height=" << req.m_total_height <<*/ ", dropping connection");
    return false;
  }
  //check genesis match
  if(qblock_ids.back() != get_block_hash(m_blocks[0].bl))
  {
    LOG_ERROR("Client sent wrong NOTIFY_REQUEST_CHAIN: genesis block missmatch: " << ENDL << "id: "
      << qblock_ids.back() << ", " << ENDL << "expected: " << get_block_hash(m_blocks[0].bl)
      << "," << ENDL << " dropping connection");
    return false;
  }

  /* Figure out what blocks we should request to get state_normal */
  size_t i = 0;
  auto bl_it = qblock_ids.begin();
  auto block_index_it = m_blocks_index.find(*bl_it);
  for(; bl_it != qblock_ids.end(); bl_it++, i++)
  {
    block_index_it = m_blocks_index.find(*bl_it);
    if(block_index_it != m_blocks_index.end())
      break;
  }

  if(bl_it == qblock_ids.end())
  {
    LOG_ERROR("Internal error handling connection, can't find split point");
    return false;
  }

  if(block_index_it == m_blocks_index.end())
  {
    //this should NEVER happen, but, dose of paranoia in such cases is not too bad
    LOG_ERROR("Internal error handling connection, can't find split point");
    return false;
  }

  //we start to put block ids INCLUDING last known id, just to make other side be sure
  starter_offset = block_index_it->second;
  return true;
}
//------------------------------------------------------------------
uint64_t blockchain_storage::block_difficulty(size_t i)
{
  CRITICAL_REGION_LOCAL(m_blockchain_lock);
  CHECK_AND_ASSERT_MES(i < m_blocks.size(), false, "wrong block index i = " << i << " at blockchain_storage::block_difficulty()");
  if(i == 0)
    return m_blocks[i].cumulative_difficulty;

  return m_blocks[i].cumulative_difficulty - m_blocks[i-1].cumulative_difficulty;
}
//------------------------------------------------------------------
uint64_t blockchain_storage::already_generated_coins(size_t i)
{
  CRITICAL_REGION_LOCAL(m_blockchain_lock);
  CHECK_AND_ASSERT_MES(i < m_blocks.size(), false, "wrong block index i = " << i << " at blockchain_storage::block_difficulty()");
  return m_blocks[i].already_generated_coins;
}
//------------------------------------------------------------------
uint64_t blockchain_storage::average_past_block_fees(uint64_t for_block_height)
{
  CRITICAL_REGION_LOCAL(m_blockchain_lock);
  
  if (for_block_height == 0)
    return 0;
  
  // get block at height 1 below the target height
  // average all fees from that block inclusive until first block that has timestamp more than 1 day ago
  
  if (for_block_height - 1 >= m_blocks.size()) {
    LOG_ERROR("Invalid for_block_height " << for_block_height << ", m_blocks.size()= " << m_blocks.size());
    throw std::runtime_error("Invalid for_block_height in average_past_block_fees");
  }
  
  uint64_t end_timestamp = m_blocks[for_block_height - 1].bl.timestamp;
  size_t used_blocks = 0;
  uint64_t fee_summaries = 0;
  
  for (int height = for_block_height - 1; height >= 0; height--)
  {
    const auto& bei = m_blocks[height];
    
    if (bei.bl.timestamp < end_timestamp - 86400)
      break;
    
    BOOST_FOREACH(const auto& tx_id, bei.bl.tx_hashes) {
      auto it = m_transactions.find(tx_id);
      if (it == m_transactions.end()) {
        LOG_ERROR("Inconsistency, block in chain has unknown transaction " << tx_id);
        throw std::runtime_error("Inconsistency in blockchain");
      }
      fee_summaries += get_tx_fee(it->second.tx);
    }
    used_blocks++;
  }
  
  if (used_blocks == 0)
    return 0;
  
  return fee_summaries / used_blocks;
}
//------------------------------------------------------------------
void blockchain_storage::print_blockchain(uint64_t start_index, uint64_t end_index)
{
  std::stringstream ss;
  CRITICAL_REGION_LOCAL(m_blockchain_lock);
  if(start_index >=m_blocks.size())
  {
    LOG_PRINT_L0("Wrong starter index set: " << start_index << ", expected max index " << m_blocks.size()-1);
    return;
  }

  for(size_t i = start_index; i != m_blocks.size() && i != end_index; i++)
  {
    ss << "height " << i << ", timestamp " << m_blocks[i].bl.timestamp << ", cumul_dif " << m_blocks[i].cumulative_difficulty << ", cumul_size " << m_blocks[i].block_cumulative_size
      << "\nid\t\t" <<  get_block_hash(m_blocks[i].bl)
      << "\ndifficulty\t\t" << block_difficulty(i) << ", nonce " << m_blocks[i].bl.nonce << ", tx_count " << m_blocks[i].bl.tx_hashes.size() << ENDL;
  }
  LOG_PRINT_L1("Current blockchain:" << ENDL << ss.str());
  LOG_PRINT_L0("Blockchain printed with log level 1");
}
//------------------------------------------------------------------
void blockchain_storage::print_blockchain_index()
{
  std::stringstream ss;
  CRITICAL_REGION_LOCAL(m_blockchain_lock);
  BOOST_FOREACH(const blocks_by_id_index::value_type& v, m_blocks_index)
    ss << "id\t\t" <<  v.first << " height" <<  v.second << ENDL << "";

  LOG_PRINT_L0("Current blockchain index:" << ENDL << ss.str());
}
//------------------------------------------------------------------
void blockchain_storage::print_blockchain_outs(const std::string& file)
{
  std::stringstream ss;
  CRITICAL_REGION_LOCAL(m_blockchain_lock);
  BOOST_FOREACH(const outputs_container::value_type& v, m_outputs)
  {
    const outputs_vector& vals = v.second;
    if(vals.size())
    {
      ss << "(coin_type, amount): ("
         << v.first.first << v.first.second << ")"
         << ENDL;
      for(size_t i = 0; i != vals.size(); i++)
        ss << "\t" << vals[i].first << ": " << vals[i].second << ENDL;
    }
  }
  if(file_io_utils::save_string_to_file(file, ss.str()))
  {
    LOG_PRINT_L0("Current outputs index writen to file: " << file);
  }else
  {
    LOG_PRINT_L0("Failed to write current outputs index to file: " << file);
  }
}
//------------------------------------------------------------------
bool blockchain_storage::find_blockchain_supplement(const std::list<crypto::hash>& qblock_ids, NOTIFY_RESPONSE_CHAIN_ENTRY::request& resp)
{
  CRITICAL_REGION_LOCAL(m_blockchain_lock);
  if(!find_blockchain_supplement(qblock_ids, resp.start_height))
    return false;

  resp.total_height = get_current_blockchain_height();
  size_t count = 0;
  for(size_t i = resp.start_height; i != m_blocks.size() && count < BLOCKS_IDS_SYNCHRONIZING_DEFAULT_COUNT; i++, count++)
    resp.m_block_ids.push_back(get_block_hash(m_blocks[i].bl));
  return true;
}
//------------------------------------------------------------------
bool blockchain_storage::find_blockchain_supplement(const std::list<crypto::hash>& qblock_ids, std::list<std::pair<block, std::list<transaction> > >& blocks, uint64_t& total_height, uint64_t& start_height, size_t max_count)
{
  CRITICAL_REGION_LOCAL(m_blockchain_lock);
  if(!find_blockchain_supplement(qblock_ids, start_height))
    return false;

  total_height = get_current_blockchain_height();
  size_t count = 0;
  for(size_t i = start_height; i != m_blocks.size() && count < max_count; i++, count++)
  {
    blocks.resize(blocks.size()+1);
    blocks.back().first = m_blocks[i].bl;
    std::list<crypto::hash> mis;
    get_transactions(m_blocks[i].bl.tx_hashes, blocks.back().second, mis);
    CHECK_AND_ASSERT_MES(!mis.size(), false, "internal error, transaction from block not found");
  }
  return true;
}
//------------------------------------------------------------------
bool blockchain_storage::add_block_as_invalid(const block& bl, const crypto::hash& h)
{
  block_extended_info bei = AUTO_VAL_INIT(bei);
  bei.bl = bl;
  return add_block_as_invalid(bei, h);
}
//------------------------------------------------------------------
bool blockchain_storage::add_block_as_invalid(const block_extended_info& bei, const crypto::hash& h)
{
  CRITICAL_REGION_LOCAL(m_blockchain_lock);
  
  auto i_res = m_invalid_blocks.insert(blocks_ext_by_hash::value_type(h, bei));
  CHECK_AND_ASSERT_MES(i_res.second, false, "at insertion invalid by tx returned status existed");
  
  LOG_PRINT_L0("BLOCK ADDED AS INVALID: " << h << ENDL << ", prev_id=" << bei.bl.prev_id << ", m_invalid_blocks count=" << m_invalid_blocks.size());
  return true;
}
//------------------------------------------------------------------
bool blockchain_storage::have_block(const crypto::hash& id)
{
  CRITICAL_REGION_LOCAL(m_blockchain_lock);
  if(m_blocks_index.count(id))
    return true;
  if(m_alternative_chains.count(id))
    return true;
  if(m_invalid_blocks.count(id))
    return true;

  return false;
}
//------------------------------------------------------------------
bool blockchain_storage::handle_block_to_main_chain(const block& bl, block_verification_context& bvc)
{
  crypto::hash id = get_block_hash(bl);
  return handle_block_to_main_chain(bl, id, bvc);
}
//------------------------------------------------------------------
bool blockchain_storage::push_transaction_to_global_outs_index(const transaction& tx, const crypto::hash& tx_id, std::vector<uint64_t>& global_indexes)
{
  CRITICAL_REGION_LOCAL(m_blockchain_lock);
  size_t i = 0;
  BOOST_FOREACH(const auto& ot, tx.outs())
  {
    outputs_vector& amount_index = m_outputs[std::make_pair(tx.out_cp(i), ot.amount)];
    amount_index.push_back(std::pair<crypto::hash, size_t>(tx_id, i));
    global_indexes.push_back(amount_index.size()-1);
    ++i;
  }
  return true;
}
//------------------------------------------------------------------
size_t blockchain_storage::get_total_transactions()
{
  CRITICAL_REGION_LOCAL(m_blockchain_lock);
  return m_transactions.size();
}
//------------------------------------------------------------------
bool blockchain_storage::get_outs(coin_type type, uint64_t amount,
                                  std::list<crypto::public_key>& pkeys)
{
  CRITICAL_REGION_LOCAL(m_blockchain_lock);
  auto it = m_outputs.find(std::make_pair(type, amount));
  if(it == m_outputs.end())
    return true;

  BOOST_FOREACH(const auto& out_entry, it->second)
  {
    auto tx_it = m_transactions.find(out_entry.first);
    CHECK_AND_ASSERT_MES(tx_it != m_transactions.end(), false, "transactions outs global index consistency broken: wrong tx id in index");
    CHECK_AND_ASSERT_MES(tx_it->second.tx.outs().size() > out_entry.second, false, "transactions outs global index consistency broken: index in tx_outx more then size");
    CHECK_AND_ASSERT_MES(tx_it->second.tx.outs()[out_entry.second].target.type() == typeid(txout_to_key), false, "transactions outs global index consistency broken: tx_outx is not a txout_to_key");
    CHECK_AND_ASSERT_MES(tx_it->second.tx.out_cp(out_entry.second) == type, false, "transactions outs global index consistency broken: tx coin_type mismatch");
    pkeys.push_back(boost::get<txout_to_key>(tx_it->second.tx.outs()[out_entry.second].target).key);
  }

  return true;
}
//------------------------------------------------------------------
bool blockchain_storage::pop_transaction_from_global_index(const transaction& tx, const crypto::hash& tx_id)
{
  CRITICAL_REGION_LOCAL(m_blockchain_lock);
  size_t i = tx.outs().size()-1;
  BOOST_REVERSE_FOREACH(const auto& ot, tx.outs())
  {
    auto it = m_outputs.find(std::make_pair(tx.out_cp(i), ot.amount));
    CHECK_AND_ASSERT_MES(it != m_outputs.end(), false, "transactions outs global index consistency broken");
    CHECK_AND_ASSERT_MES(it->second.size(), false, "transactions outs global index: empty index for amount: " << ot.amount);
    CHECK_AND_ASSERT_MES(it->second.back().first == tx_id , false, "transactions outs global index consistency broken: tx id missmatch");
    CHECK_AND_ASSERT_MES(it->second.back().second == i, false, "transactions outs global index consistency broken: in transaction index missmatch");
    it->second.pop_back();
    --i;
  }
  return true;
}
//------------------------------------------------------------------
bool blockchain_storage::check_tx_in_to_key(const transaction& tx, size_t i, const txin_to_key& inp,
                                            const crypto::hash& tx_prefix_hash_, uint64_t* pmax_related_block_height)
{
  CRITICAL_REGION_LOCAL(m_blockchain_lock);

  CHECK_AND_ASSERT_MES(inp.key_offsets.size(), false,
                       "empty in_to_key.key_offsets in transaction with id " << get_transaction_hash(tx));
  
  CHECK_AND_ASSERT_MES(i < tx.signatures.size(), false,
                       "wrong transaction: not signature entry for input with index= " << i);
  
  if (have_tx_keyimg_as_spent(inp.k_image))
  {
    LOG_PRINT_L1("Key image already spent in blockchain: " << string_tools::pod_to_hex(inp.k_image));
    return false;
  }
  
  struct outputs_visitor
  {
    std::vector<const crypto::public_key *>& m_results_collector;
    blockchain_storage& m_bch;
    coin_type m_type;
    
    outputs_visitor(std::vector<const crypto::public_key *>& results_collector, blockchain_storage& bch, coin_type ct)
        : m_results_collector(results_collector), m_bch(bch), m_type(ct) { }
    
    bool handle_output(const transaction& tx, size_t out_i, const tx_out& out)
    {
      CHECK_AND_ASSERT_MES(tx.out_cp(out_i) == m_type, false,
                           "INCONSISTENCY: outputs_visitor::handle_output given out with wrong coin_type");
      
      //check tx unlock time
      if (!m_bch.is_tx_spendtime_unlocked(tx.unlock_time))
      {
        LOG_PRINT_L0("One of outputs for one of inputs have wrong tx.unlock_time = " << tx.unlock_time);
        return false;
      }
      
      CHECK_AND_ASSERT_MES(out.target.type() == typeid(txout_to_key), false,
                           "Output have wrong type id, expected txout_to_key, which=" << out.target.which());
      
      const auto& outp = boost::get<txout_to_key>(out.target);
      m_results_collector.push_back(&outp.key);
      
      return true;
    }
  };

  //check ring signature
  std::vector<const crypto::public_key *> output_keys;
  coin_type type = tx.in_cp(i);
  outputs_visitor vi(output_keys, *this, type);
  
  if (!scan_outputkeys_for_indexes(inp, type, vi, pmax_related_block_height))
  {
    LOG_ERROR("Failed to get output keys for tx with coin_type = " << type
              << ", amount = " << print_money(inp.amount, currency_decimals(type))
              << " and count indexes " << inp.key_offsets.size());
    return false;
  }

  if (inp.key_offsets.size() != output_keys.size())
  {
    LOG_ERROR("Output keys for tx with amount = " << inp.amount << " and count indexes "
              << inp.key_offsets.size() << " returned wrong keys count " << output_keys.size());
    return false;
  }
  CHECK_AND_ASSERT_MES(tx.signatures[i].size() == output_keys.size(), false,
                       "internal error: tx signatures count=" << tx.signatures[i].size()
                       << " mismatch with outputs keys count for inputs=" << output_keys.size());

  if (m_is_in_checkpoint_zone)
    return true;
  
  crypto::hash tx_prefix_hash = (tx_prefix_hash_ == null_hash) ? get_transaction_prefix_hash(tx) : tx_prefix_hash_;
  CHECK_AND_ASSERT_MES(crypto::check_ring_signature(tx_prefix_hash, inp.k_image, output_keys, tx.signatures[i].data()),
                       false, "Ring signature check failed");
  return true;
}
//------------------------------------------------------------------
bool blockchain_storage::check_tx_in_mint(const transaction& tx, size_t i, const txin_mint& inp)
{
  CHECK_AND_ASSERT_MES(inp.currency >= 256, false, "minting currency with invalid id <= 255");
  CHECK_AND_ASSERT_MES(tx.in_cp(i) == coin_type(inp.currency, NotContract, BACKED_BY_N_A), false,
                       "minting with in coin_type != (inp.currency, NotContract, BACKED_BY_N_A)");
  CHECK_AND_ASSERT_MES(inp.remint_key == null_pkey || crypto::check_key(inp.remint_key), false, "minting currency with invalid remint key " << inp.remint_key);
  
  CHECK_AND_ASSERT_MES(inp.amount > 0, false, "minting currency with amount == 0");
  
  CHECK_AND_ASSERT_MES(m_currencies.find(inp.currency) == m_currencies.end(), false,
                       "minting currency with already existing id " << inp.currency);
  CHECK_AND_ASSERT_MES(m_contracts.find(inp.currency) == m_contracts.end(), false,
                       "minting currency with already existing contract id " << inp.currency);
  
  if (!inp.description.empty())
  {
    CHECK_AND_ASSERT_MES(inp.description.size() <= CURRENCY_DESCRIPTION_MAX_SIZE, false,
                         "minting currency with description that is too large");
    CHECK_AND_ASSERT_MES(m_used_currency_descriptions.find(inp.description) == m_used_currency_descriptions.end(), false, "minting currency with non-unique description " << inp.description);
  }
  
  return true;
}
//------------------------------------------------------------------
bool blockchain_storage::check_tx_in_remint(const transaction& tx, size_t i, const txin_remint& inp)
{
  CHECK_AND_ASSERT_MES(inp.currency >= 256, false, "reminting currency with invalid id <= 255");
  CHECK_AND_ASSERT_MES(tx.in_cp(i) == coin_type(inp.currency, NotContract, BACKED_BY_N_A), false,
                       "reminting with in coin_type != (inp.currency, NotContract, BACKED_BY_N_A_+)");
  CHECK_AND_ASSERT_MES(inp.new_remint_key == null_pkey || crypto::check_key(inp.new_remint_key), false,
                       "reminting currency with invalid new remint key " << inp.new_remint_key);
  
  CHECK_AND_ASSERT_MES(inp.amount > 0, false, "reminting currency with amount == 0");
  
  auto r = m_currencies.find(inp.currency);
  CHECK_AND_ASSERT_MES(r != m_currencies.end(), false, "reminting non-existent currency " << inp.currency);
  
  auto& ci = r->second;
  CHECK_AND_ASSERT_MES(ci.remint_key() != null_pkey, false, "reminting non-remintable currency");

  CHECK_AND_ASSERT_MES(!add_amount_would_overflow(ci.total_amount_minted, inp.amount), false,
                       "reminting would overflow total amount of coins minted");
  
  crypto::hash ph = inp.get_prefix_hash();
  CHECK_AND_ASSERT_MES(check_signature(ph, ci.remint_key(), inp.sig), false, "reminting with incorrect signature");
  
  return true;
}
//------------------------------------------------------------------
bool blockchain_storage::check_tx_in_create_contract(const transaction& tx, size_t i, const txin_create_contract& inp)
{
  CHECK_AND_ASSERT_MES(inp.contract >= 256, false, "creating contract with invalid id <= 255");
  CHECK_AND_ASSERT_MES(tx.in_cp(i) == CP_N_A, false,
                       "creating contract with in coin_type != CP_N_A");
  CHECK_AND_ASSERT_MES(inp.grading_key != null_pkey && crypto::check_key(inp.grading_key), false,
                       "creating contract with invalid grading key " << inp.grading_key);
  CHECK_AND_ASSERT_MES(inp.expiry_block > get_current_blockchain_height(), false,
                       "can't create already-expired contract: current height is " << get_current_blockchain_height()
                       << ", expiry_block is " << inp.expiry_block);
  CHECK_AND_ASSERT_MES(inp.expiry_block < CRYPTONOTE_MAX_BLOCK_NUMBER, false, "create contract with too large expiry block");
  CHECK_AND_ASSERT_MES(inp.fee_scale <= GRADE_SCALE_MAX, false, "create contract with grade > GRADE_SCALE_MAX");
  CHECK_AND_ASSERT_MES(inp.default_grade <= GRADE_SCALE_MAX, false, "create contract with default_grade > GRADE_SCALE_MAX");
  
  CHECK_AND_ASSERT_MES(m_currencies.find(inp.contract) == m_currencies.end(), false,
                       "creating contract with already existing (currency) id " << inp.contract);
  CHECK_AND_ASSERT_MES(m_contracts.find(inp.contract) == m_contracts.end(), false,
                       "creating contract with already existing (contract) id " << inp.contract);
  
  if (!inp.description.empty())
  {
    CHECK_AND_ASSERT_MES(inp.description.size() <= CONTRACT_DESCRIPTION_MAX_SIZE, false,
                         "creating contract with description that is too large");
    CHECK_AND_ASSERT_MES(m_used_currency_descriptions.find(inp.description) == m_used_currency_descriptions.end(), false, "creating contract with non-unique description " << inp.description);
  }
  
  return true;
}
//------------------------------------------------------------------
bool blockchain_storage::check_tx_in_mint_contract(const transaction& tx, size_t i, const txin_mint_contract& inp)
{
  CHECK_AND_ASSERT_MES(tx.in_cp(i) == CP_N_A, false,
                       "minting contract with coin_type != CP_N_A");

  CHECK_AND_ASSERT_MES(inp.contract >= 256, false, "minting contract with invalid id <= 255");
  
  auto r = m_contracts.find(inp.contract);
  CHECK_AND_ASSERT_MES(r != m_contracts.end(), false, "minting non-existent contract " << inp.contract);

  auto& ci = r->second;
  CHECK_AND_ASSERT_MES(!ci.is_graded, false, "minting contract that has already been graded");
  CHECK_AND_ASSERT_MES(get_current_blockchain_height() < ci.expiry_block, false,
                       "minting contract that has expired: current height is " << get_current_blockchain_height()
                       << ", expiry_block is " << ci.expiry_block);
  CHECK_AND_ASSERT_MES(!add_amount_would_overflow(ci.total_amount_minted[inp.backed_by_currency], inp.amount), false,
                       "Would overflow when keeping track of amounts minting contract " << inp.contract
                       << " backed by " << inp.backed_by_currency);

  if (inp.backed_by_currency == CURRENCY_XPB)
  {
    // ok
  }
  else
  {
    auto r2 = m_contracts.find(inp.backed_by_currency);
    CHECK_AND_ASSERT_MES(r2 == m_contracts.end(), false, "minting contract backed by a contract, not allowed");
  
    auto r3 = m_currencies.find(inp.backed_by_currency);
    CHECK_AND_ASSERT_MES(r3 != m_currencies.end(), false, "minting contract backed by non-existent currency");
  }
  
  // check_inputs_outputs() ensures there are enough coins to burn
  
  return true;
}
//------------------------------------------------------------------
bool blockchain_storage::check_tx_in_grade_contract(const transaction& tx, size_t i, const txin_grade_contract& inp)
{
  CHECK_AND_ASSERT_MES(tx.in_cp(i) == CP_N_A, false,
                       "grading contract with coin_type != CP_N_A");

  CHECK_AND_ASSERT_MES(inp.contract >= 256, false, "grading contract with invalid id <= 255");
  CHECK_AND_ASSERT_MES(inp.grade <= GRADE_SCALE_MAX, false, "grade contract with grade > GRADE_SCALE_MAX");
  
  auto r = m_contracts.find(inp.contract);
  CHECK_AND_ASSERT_MES(r != m_contracts.end(), false, "grading non-existent contract " << inp.contract);
  
  BOOST_FOREACH(const auto& item, inp.fee_amounts)
  {
    if (item.first == CURRENCY_XPB) continue;
    auto r2 = m_currencies.find(item.first);
    CHECK_AND_ASSERT_MES(r2 != m_currencies.end(), false, "grading contract with fee paid in non-existent subcurrency");
  }
  
  auto& ci = r->second;
  CHECK_AND_ASSERT_MES(!ci.is_graded, false, "grading contract that has already been graded");
  CHECK_AND_ASSERT_MES(get_current_blockchain_height() < ci.expiry_block, false,
                       "grading contract that has expired: current height is " << get_current_blockchain_height()
                       << ", expiry_block is " << ci.expiry_block);
  
  // check fees
  BOOST_FOREACH(const auto& item, inp.fee_amounts)
  {
    CHECK_AND_ASSERT_MES(item.second != 0, false, "Claiming fee of zero, shouldn't have an entry at all");
    uint64_t allowed_fee = calculate_total_fee(ci.total_amount_minted[item.first], ci.fee_scale);
    coin_type fee_type(item.first, NotContract, BACKED_BY_N_A);
    CHECK_AND_ASSERT_MES(item.second == allowed_fee, false,
                         "Claiming fee of " << print_money(item.second, currency_decimals(fee_type))
                         << " in " << item.first
                         << " when must have exactly " << print_money(allowed_fee, currency_decimals(fee_type)));
  }
  
  crypto::hash ph = inp.get_prefix_hash();
  CHECK_AND_ASSERT_MES(check_signature(ph, ci.grading_key, inp.sig), false, "grading contract with incorrect signature");
  
  return true;
}
//------------------------------------------------------------------
bool get_grade_fee(const blockchain_storage::contract_info& ci, size_t current_blockchain_height,
                   bool& expired, uint64_t& grade, uint64_t& fee_scale)
{
  if (ci.is_graded)
  {
    expired = false;
    grade = ci.grade;
    fee_scale = ci.fee_scale;
    return true;
  }
  
  if (current_blockchain_height < ci.expiry_block)
  {
    LOG_ERROR("getting grade/fee from contract that is neither graded nor expired");
    return false;
  }
  
  expired = true;
  grade = ci.default_grade;
  fee_scale = 0;
  
  return true;
}
//------------------------------------------------------------------
bool blockchain_storage::check_tx_in_resolve_bc_coins(const transaction& tx, size_t i, const txin_resolve_bc_coins& inp)
{
  CHECK_AND_ASSERT_MES(tx.in_cp(i) == coin_type(inp.backing_currency, NotContract, BACKED_BY_N_A), false,
                       "in_cp doesn't match txin_resolve_bc_coins");

  CHECK_AND_ASSERT_MES(inp.contract >= 256, false, "resolving bc_coins with invalid id <= 255");
  auto r = m_contracts.find(inp.contract);
  CHECK_AND_ASSERT_MES(r != m_contracts.end(), false, "resolving non-existent contract " << inp.contract);
  const auto& ci = r->second;

  CHECK_AND_ASSERT_MES(inp.is_backing_coins == 0 || inp.is_backing_coins == 1, false,
                       "resolving bc_coins with invalid inp.is_backing_coins of " << inp.is_backing_coins);
  
  uint64_t grade, fee_scale;
  bool expired;
  CHECK_AND_ASSERT(get_grade_fee(ci, get_current_blockchain_height(), expired, grade, fee_scale), false);
  
  uint64_t correct_amount = (inp.is_backing_coins ? grade_backing_amount
                                                  : grade_contract_amount)(inp.source_amount, grade, fee_scale);
  
  CHECK_AND_ASSERT_MES(inp.graded_amount == correct_amount, false,
                       "Resolving bc_coins with " << inp.graded_amount << ", should be " << correct_amount
                       << " from start of " << inp.source_amount << " with a grade of " << print_grade_scale(grade)
                       << ", fee of " << print_grade_scale(fee_scale) << ", expired=" << expired
                       << ", is_backing_coins=" << inp.is_backing_coins);
  
  CHECK_AND_ASSERT_MES(correct_amount > 0, false, "can't resolve_bc_coins if would resolve to 0 coins");
  
  // check_inputs_outputs() ensures there are enough backing/contract coins in the other inputs to convert

  return true;
}
//------------------------------------------------------------------
bool blockchain_storage::check_tx_in_fuse_bc_coins(const transaction& tx, size_t i, const txin_fuse_bc_coins& inp)
{
  CHECK_AND_ASSERT_MES(tx.in_cp(i) == coin_type(inp.backing_currency, NotContract, BACKED_BY_N_A), false,
                       "in_cp doesn't match txin_fuse_bc_coins");

  CHECK_AND_ASSERT_MES(inp.contract >= 256, false, "fusing bc_coins with invalid id <= 255");
  auto r = m_contracts.find(inp.contract);
  CHECK_AND_ASSERT_MES(r != m_contracts.end(), false, "fusing non-existent contract " << inp.contract);
  auto& ci = r->second;
  
  CHECK_AND_ASSERT_MES(!ci.is_graded, false, "fusing graded contract");
  CHECK_AND_ASSERT_MES(get_current_blockchain_height() < ci.expiry_block, false, "fusing expired contract");

  // should be impossible, but to be safe:
  CHECK_AND_ASSERT_MES(!sub_amount_would_underflow(ci.total_amount_minted[inp.backing_currency], inp.amount), false,
                       "Not enough coins were minted to be able to fuse them");
  
  // check_inputs_outputs() ensures there are enough backing & contract coins in the tx to fuse
  // inp.backing_currency must exist, otherwise the txin_to_keys spending the non-existent backing_currency
  // would fail
  
  return true;
}
//------------------------------------------------------------------
bool blockchain_storage::check_tx_in_register_delegate(const transaction& tx, size_t i, const txin_register_delegate& inp)
{
  CHECK_AND_ASSERT_MES(get_current_blockchain_height() >= cryptonote::config::dpos_registration_start_block, false,
                       "check_tx_in_register_delegate: can't register delegates at height "
                       << get_current_blockchain_height() << ", must wait until height "
                       << cryptonote::config::dpos_registration_start_block);
  
  CHECK_AND_ASSERT_MES(inp.delegate_id != 0, false, "check_tx_in_register_delegate: can't register delegate 0");
  CHECK_AND_ASSERT_MES(m_delegates.find(inp.delegate_id) == m_delegates.end(), false,
                       "check_tx_in_register_delegate: Registering already-used delegate id");
  
  BOOST_FOREACH(const auto& item, m_delegates)
  {
    CHECK_AND_ASSERT_MES(item.second.public_address != inp.delegate_address, false,
                         "check_tx_in_register_delegate: Registering already-used delegate address");
  }
  
  uint64_t for_height = m_blocks.size();
  if (m_popping_block)
    for_height -= 1;
  
  uint64_t required_fee = std::max(average_past_block_fees(for_height) * DPOS_REGISTRATION_FEE_MULTIPLE,
                                   DPOS_MIN_REGISTRATION_FEE);
  CHECK_AND_ASSERT_MES(inp.registration_fee >= required_fee, false,
                       "Provided fee " << print_money(inp.registration_fee) << ", require at least " <<
                       print_money(required_fee));
  
  CHECK_AND_ASSERT_MES(crypto::check_key(inp.delegate_address.m_spend_public_key) &&
                       crypto::check_key(inp.delegate_address.m_view_public_key), false,
                       "Provided invalid delegate address");
  
  return true;
}
//------------------------------------------------------------------
bool blockchain_storage::check_tx_in_vote(const transaction& tx, size_t i, const txin_vote& inp,
                                          const crypto::hash& tx_prefix_hash_, uint64_t* pmax_related_block_height)
{
  CHECK_AND_ASSERT_MES(get_current_blockchain_height() >= cryptonote::config::dpos_registration_start_block, false,
                       "check_tx_in_vote: can't vote for delegates at height "
                       << get_current_blockchain_height() << ", must wait until height "
                       << cryptonote::config::dpos_registration_start_block);
  
  // check voting with XPBs
  CHECK_AND_ASSERT_MES(tx.in_cp(i) == CP_XPB, false, "check_tx_in_vote: voting with non-xpbs");
  
  // check voting for at most 'n' delegates
  CHECK_AND_ASSERT_MES(inp.votes.size() <= config::dpos_num_delegates, false,
                       "check_tx_in_vote: voting for too many delegates " << inp.votes.size()
                       << ", can vote for at most " << config::dpos_num_delegates);
  
  // check not voting with spent image
  CHECK_AND_ASSERT_MES(m_spent_keys.find(inp.ink.k_image) == m_spent_keys.end(), false,
                       "check_tx_in_vote: voting with spent key image");

  // check 'seq' matches
  auto& hists = m_vote_histories[inp.ink.k_image];
  CHECK_AND_ASSERT_MES(inp.seq == hists.size(), false, "check_tx_in_vote: invalid 'seq' of " << inp.seq << ", but hists.size()=" << hists.size());
  
  // check votes are for existing delegates and that would be no overflow
  BOOST_FOREACH(const auto& delegate_id, inp.votes)
  {
    const auto it = m_delegates.find(delegate_id);
    CHECK_AND_ASSERT_MES(it != m_delegates.end(),
                         false, "check_tx_in_vote: vote for unregistered delegate id " << delegate_id);
    const auto& info = it->second;
    CHECK_AND_ASSERT_MES(!add_amount_would_overflow(info.total_votes, inp.ink.amount), false,
                         "check_tx_in_vote: vote amount would overflow");
  }
  
  // check txin_to_key is valid (checks ring signature proving coin ownership)
  CHECK_AND_ASSERT_MES(check_tx_in_to_key(tx, i, inp.ink, tx_prefix_hash_, pmax_related_block_height),
                       false, "check_tx_in_vote: txin_to_key failed check");
  
  return true;
}
//------------------------------------------------------------------
bool blockchain_storage::check_tx_out_to_key(const transaction& tx, size_t i, const txout_to_key& inp)
{
  return true;
}
//------------------------------------------------------------------
namespace bs_visitor_detail {
  struct add_transaction_input_visitor: public tx_input_visitor_base
  {
    using tx_input_visitor_base::operator();
    
    const transaction& tx;
    blockchain_storage& b;
    crypto::hash m_tx_id;
    crypto::hash m_bl_id;
    crypto::hash tx_prefix_hash;
    
    // for printing summary:
    currency_map mints;
    std::set<uint64_t> contracts;
    std::set<account_public_address> registered_delegates;
    std::map<delegate_id_t, uint64_t> votes;
    
    add_transaction_input_visitor(const transaction& tx_in, blockchain_storage& b_in,
                                  const crypto::hash& tx_id, const crypto::hash& bl_id)
        : tx(tx_in), b(b_in), m_tx_id(tx_id), m_bl_id(bl_id), tx_prefix_hash(get_transaction_prefix_hash(tx)) { }

    // all inputs are already checked, so don't check them again
    bool operator()(const txin_to_key& inp) const
    {
      const auto& ki = inp.k_image;
      
      // unapply this key image's votes
      if (!b.m_vote_histories[ki].empty())
      {
        // don't enforce effective amounts since other factors could have lowered the delegate votes
        CHECK_AND_ASSERT_MES(b.unapply_votes(b.m_vote_histories[ki].back(), false), false,
                             "internal error: could not remove latest votes ");
      }
      
      auto r = b.m_spent_keys.insert(ki);
      // double spend check, should never happen since inputs were already checked
      CHECK_AND_ASSERT_MES(r.second, false,
                           "tx with id: " << m_tx_id << " in block id: " << m_bl_id
                           << " have input marked as spent with key image: " << ki << ", block declined");
      
      return true;
    }
    bool operator()(const txin_gen& inp) const
    {
      return true;
    }
    bool operator()(const txin_mint& inp)
    {
      blockchain_storage::currency_info ci;
      ci.currency = inp.currency;
      ci.description = inp.description;
      ci.decimals = inp.decimals;
      ci.total_amount_minted = inp.amount;
      ci.remint_key_history.push_back(inp.remint_key);
      b.m_currencies[inp.currency] = ci;
      if (!inp.description.empty())
      {
        b.m_used_currency_descriptions.insert(inp.description);
      }
      
      mints[tx.in_cp(visitor_index)] += inp.amount;
      
      return true;
    }
    bool operator()(const txin_remint& inp)
    {
      auto& ci = b.m_currencies[inp.currency];
      
      ci.remint_key_history.push_back(inp.new_remint_key);
      // extra check, check_tx_in_remint should make this impossible though
      CHECK_AND_ASSERT_MES(add_amount(ci.total_amount_minted, inp.amount), false,
                           "Overflow when keeping track of amounts reminting currency " << inp.currency);
      
      mints[tx.in_cp(visitor_index)] += inp.amount;
      
      return true;
    }
    bool operator()(const txin_create_contract& inp)
    {
      blockchain_storage::contract_info ci;
      ci.contract = inp.contract;
      ci.description = inp.description;
      ci.grading_key = inp.grading_key;
      ci.fee_scale = inp.fee_scale;
      ci.expiry_block = inp.expiry_block;
      ci.default_grade = inp.default_grade;
      
      ci.is_graded = false;
      ci.grade = 0;
      
      b.m_contracts[inp.contract] = ci;
      
      if (!inp.description.empty())
      {
        b.m_used_currency_descriptions.insert(inp.description);
      }
      
      contracts.insert(inp.contract);
      
      return true;
    }
    bool operator()(const txin_mint_contract& inp)
    {
      auto& ci = b.m_contracts[inp.contract];

      // extra check, check_tx_in_mint_contract should make this impossible though
      CHECK_AND_ASSERT_MES(add_amount(ci.total_amount_minted[inp.backed_by_currency], inp.amount), false,
                           "Overflow when keeping track of amounts minting contract " << inp.contract
                           << " backed by " << inp.backed_by_currency);
      
      mints[tx.in_cp(visitor_index)] += inp.amount;
      
      return true;
    }
    bool operator()(const txin_grade_contract& inp)
    {
      auto& ci = b.m_contracts[inp.contract];
      
      ci.is_graded = true;
      ci.grade = inp.grade;
      
      return true;
    }
    bool operator()(const txin_resolve_bc_coins& inp)
    {
      return true;
    }
    bool operator()(const txin_fuse_bc_coins& inp)
    {
      auto& ci = b.m_contracts[inp.contract];
      
      // this underflow shouldn't happen
      CHECK_AND_ASSERT_MES(sub_amount(ci.total_amount_minted[inp.backing_currency], inp.amount), false,
                           "Underflow when subtracting fused coins from total amount minted; not enough coins were minted");
      
      return true;
    }
    bool operator()(const txin_register_delegate& inp)
    {
      auto r = b.m_delegates.find(inp.delegate_id);
      CHECK_AND_ASSERT_MES(r == b.m_delegates.end(), false, "Registering already-used delegate id");
      
      blockchain_storage::delegate_info di;
      di.delegate_id = inp.delegate_id;
      di.public_address = inp.delegate_address;
      di.address_as_string = get_account_address_as_str(di.public_address);
      di.total_votes = 0;
      di.processed_blocks = 0;
      di.missed_blocks = 0;
      di.fees_received = 0;
      
      b.m_delegates[inp.delegate_id] = di;
      
      registered_delegates.insert(inp.delegate_address);
      
      return true;
    }
    
    bool operator()(const txin_vote& inp)
    {
      CHECK_AND_ASSERT_MES(b.m_spent_keys.find(inp.ink.k_image) == b.m_spent_keys.end(), false,
                           "internal error: trying to vote with spent key");
      
      // remove latest vote
      if (!b.m_vote_histories[inp.ink.k_image].empty())
      {
        // don't enforce effective amounts
        CHECK_AND_ASSERT_MES(b.unapply_votes(b.m_vote_histories[inp.ink.k_image].back(), false), false,
                             "internal error: could not remove previous votes");
      }
      
      // apply new vote
      blockchain_storage::vote_instance vote_record;
      CHECK_AND_ASSERT_MES(b.apply_votes(inp.ink.amount, inp.votes, vote_record), false,
                           "internal error: could not apply new votes");
      b.m_vote_histories[inp.ink.k_image].push_back(vote_record);
      
      /*std::stringstream ss;
      ss << "Effective votes: " << ENDL;
      BOOST_FOREACH(const auto& item, vote_record.votes)
      {
        ss << "  " << item.first << ": " << print_money(item.second) << ENDL;
      }
      LOG_PRINT_L2(ss.str());*/
      
      BOOST_FOREACH(const auto& item, vote_record.votes)
      {
        votes[item.first] += item.second;
      }
      
      return true;
    }
  };
}
bool blockchain_storage::add_transaction_from_block(const transaction& tx, const crypto::hash& tx_id, const crypto::hash& bl_id, uint64_t bl_height)
{
  CRITICAL_REGION_LOCAL(m_blockchain_lock);
  
  bs_visitor_detail::add_transaction_input_visitor visitor(tx, *this, tx_id, bl_id);
  
  if (!tools::all_apply_visitor(visitor, tx.ins()))
  {
    LOG_ERROR("critical internal error: add_transaction_input_visitor failed. but here key_images should be checked");
    purge_transaction_data_from_blockchain(tx, false);
    return false;
  }
  
  transaction_chain_entry ch_e;
  ch_e.m_keeper_block_height = bl_height;
  ch_e.tx = tx;
  auto i_r = m_transactions.insert(std::pair<crypto::hash, transaction_chain_entry>(tx_id, ch_e));
  if(!i_r.second)
  {
    LOG_PRINT_L0("tx with id: " << tx_id << " in block id: " << bl_id << " already in blockchain");
    return false;
  }
  bool r = push_transaction_to_global_outs_index(tx, tx_id, i_r.first->second.m_global_output_indexes);
  CHECK_AND_ASSERT_MES(r, false, "failed to return push_transaction_to_global_outs_index tx id " << tx_id);
  
  // format the tx string
  std::stringstream ss;
  ss << "Added transaction to blockchain history:" << ENDL;
  ss << "----------------------------------------" << ENDL;
  ss << "tx_id: " << tx_id << ENDL;
  ss << "inputs: " << tx.ins().size()
     << ", outs: " << tx.outs().size()
     << ", spend money: " << print_moneys(get_outs_money_amount(tx), *this)
     << ", fee: " << (is_coinbase(tx) ? "0[coinbase]" : print_money(get_tx_fee(tx))) << ENDL;
  
  if (!visitor.mints.empty()) {
    ss << "minted: " << print_moneys(visitor.mints, *this);
  }
  
  if (!visitor.contracts.empty())
  {
    using namespace boost::algorithm;
    using namespace boost::adaptors;
    std::string contracts_created = join(transform(visitor.contracts,
                                                   boost::lexical_cast<std::string, uint64_t>),
                                         ", ");
    ss << "contracts created: " << "{" << contracts_created << "}" << ENDL;
  }
  
  if (!visitor.registered_delegates.empty())
  {
    using namespace boost::algorithm;
    using namespace boost::adaptors;
    std::string delegates_registered = join(transform(visitor.registered_delegates, get_account_address_as_str),
                                            ", ");
    ss << "delegates registered: " << "{" << delegates_registered << "}" << ENDL;
  }
  
  if (!visitor.votes.empty())
  {
    ss << "Votes:" << ENDL;
    BOOST_FOREACH(const auto& item, visitor.votes)
    {
      ss << "  " << item.first << ": " << print_money(item.second) << ENDL;
    }
  }
  ss << "----------------------------------------";
  
  LOG_PRINT_L2(ss.str());
  return true;
}
//------------------------------------------------------------------
bool blockchain_storage::get_tx_outputs_gindexs(const crypto::hash& tx_id, std::vector<uint64_t>& indexs)
{
  CRITICAL_REGION_LOCAL(m_blockchain_lock);
  auto it = m_transactions.find(tx_id);
  if(it == m_transactions.end())
  {
    LOG_PRINT_RED_L0("warning: get_tx_outputs_gindexs failed to find transaction with id = " << tx_id);
    return false;
  }

  CHECK_AND_ASSERT_MES(it->second.m_global_output_indexes.size(), false, "internal error: global indexes for transaction " << tx_id << " is empty");
  indexs = it->second.m_global_output_indexes;
  return true;
}
//------------------------------------------------------------------
namespace bs_visitor_detail {
  struct check_tx_input_visitor: public tx_input_visitor_base
  {
    using tx_input_visitor_base::operator();
    
    blockchain_storage& b;
    const transaction& tx;
    crypto::hash tx_prefix_hash;
    uint64_t* pmax_used_block_height;
    
    check_tx_input_visitor(blockchain_storage& b_in, const transaction& tx_in,
                           const crypto::hash& tx_prefix_hash_in,
                           uint64_t* pmax_used_block_height_in)
    : b(b_in), tx(tx_in), tx_prefix_hash(tx_prefix_hash_in), pmax_used_block_height(pmax_used_block_height_in) {}
    
    bool operator()(const txin_to_key& inp)
    {
      return b.check_tx_in_to_key(tx, visitor_index, inp, tx_prefix_hash, pmax_used_block_height);
    }
    bool operator()(const txin_mint& inp)
    {
      return b.check_tx_in_mint(tx, visitor_index, inp);
    }
    bool operator()(const txin_remint& inp)
    {
      return b.check_tx_in_remint(tx, visitor_index, inp);
    }
    bool operator()(const txin_gen& inp)
    {
      return true;
    }
    bool operator()(const txin_create_contract& inp)
    {
      return b.check_tx_in_create_contract(tx, visitor_index, inp);
    }
    bool operator()(const txin_mint_contract& inp)
    {
      return b.check_tx_in_mint_contract(tx, visitor_index, inp);
    }
    bool operator()(const txin_grade_contract& inp)
    {
      return b.check_tx_in_grade_contract(tx, visitor_index, inp);
    }
    bool operator()(const txin_resolve_bc_coins& inp)
    {
      return b.check_tx_in_resolve_bc_coins(tx, visitor_index, inp);
    }
    bool operator()(const txin_fuse_bc_coins& inp)
    {
      return b.check_tx_in_fuse_bc_coins(tx, visitor_index, inp);
    }
    bool operator()(const txin_register_delegate& inp)
    {
      return b.check_tx_in_register_delegate(tx, visitor_index, inp);
    }
    bool operator()(const txin_vote& inp)
    {
      return b.check_tx_in_vote(tx, visitor_index, inp, tx_prefix_hash, pmax_used_block_height);
    }
  };
}
bool blockchain_storage::check_tx_inputs(const transaction& tx, uint64_t* pmax_used_block_height)
{
  if(pmax_used_block_height)
    *pmax_used_block_height = 0;
  
  bs_visitor_detail::check_tx_input_visitor visitor(*this, tx, get_transaction_prefix_hash(tx), pmax_used_block_height);
  return tools::all_apply_visitor(visitor, tx.ins());
}
//------------------------------------------------------------------
namespace {
  struct check_tx_output_visitor: public tx_output_visitor_base
  {
    using tx_output_visitor_base::operator();
    
    blockchain_storage& b;
    const transaction& tx;
    
    check_tx_output_visitor(blockchain_storage& b_in, const transaction& tx_in)
    : b(b_in), tx(tx_in) {}
    
    bool operator()(const txout_to_key& inp)
    {
      return b.check_tx_out_to_key(tx, visitor_index, inp);
    }
  };
}
bool blockchain_storage::check_tx_outputs(const transaction& tx)
{
  check_tx_output_visitor visitor(*this, tx);
  return tools::all_apply_visitor(visitor, tx.outs(), out_getter());
}
//------------------------------------------------------------------
bool blockchain_storage::validate_tx(const transaction& tx, bool is_miner_tx, uint64_t *pmax_used_block_height)
{
  CRITICAL_REGION_LOCAL(m_blockchain_lock);
  
  // check amounts, only for non-miner txs
  if (!is_miner_tx)
  {
    CHECK_AND_ASSERT_MES(check_inputs_outputs(tx), false, "validate_tx: check_inputs_outputs failed");
  }
  
  // check actual inputs/outputs
  CHECK_AND_ASSERT_MES(check_tx_inputs(tx, pmax_used_block_height), false, "validate_tx: check_tx_inputs failed");
  CHECK_AND_ASSERT_MES(check_tx_outputs(tx), false, "validate_tx: check_tx_outputs failed");
  
  return true;
}
//------------------------------------------------------------------
bool blockchain_storage::is_tx_spendtime_unlocked(uint64_t unlock_time)
{
  if(unlock_time < CRYPTONOTE_MAX_BLOCK_NUMBER)
  {
    //interpret as block index
    if(get_current_blockchain_height()-1 + cryptonote::config::cryptonote_locked_tx_allowed_delta_blocks() >= unlock_time)
      return true;
    else
      return false;
  }else
  {
    //interpret as time
    uint64_t current_time = static_cast<uint64_t>(get_adjusted_time());
    if(current_time + cryptonote::config::cryptonote_locked_tx_allowed_delta_seconds() >= unlock_time)
      return true;
    else
      return false;
  }
  return false;
}
//------------------------------------------------------------------
bool blockchain_storage::is_contract_resolved(uint64_t contract)
{
  auto r = m_contracts.find(contract);
  if (r == m_contracts.end())
    throw std::runtime_error("is_contract_resolved passed a non-contract");

  auto& ci = r->second;
  if (ci.is_graded)
    return true;
  if (get_current_blockchain_height() >= ci.expiry_block)
    return true;
  
  return false;
}
//------------------------------------------------------------------
uint64_t blockchain_storage::get_adjusted_time()
{
  return m_ntp_time.get_time();
}
//------------------------------------------------------------------
bool blockchain_storage::check_block_timestamp_main(const block& b)
{
  uint64_t LIMIT = config::in_pos_era(get_block_height(b)) ? CRYPTONOTE_DPOS_BLOCK_FUTURE_TIME_LIMIT : CRYPTONOTE_BLOCK_FUTURE_TIME_LIMIT;
  
  if(b.timestamp > get_adjusted_time() + LIMIT)
  {
    LOG_PRINT_L0("Timestamp of block with id: " << get_block_hash(b) << ", " << b.timestamp << ", bigger than adjusted time + " << LIMIT << " seconds");
    return false;
  }

  std::vector<uint64_t> timestamps;
  size_t offset = m_blocks.size() <= BLOCKCHAIN_TIMESTAMP_CHECK_WINDOW ? 0: m_blocks.size()- BLOCKCHAIN_TIMESTAMP_CHECK_WINDOW;
  for(;offset!= m_blocks.size(); ++offset)
    timestamps.push_back(m_blocks[offset].bl.timestamp);

  return check_block_timestamp(std::move(timestamps), b);
}
//------------------------------------------------------------------
bool blockchain_storage::check_block_timestamp(std::vector<uint64_t> timestamps, const block& b)
{
  if (config::in_pos_era(get_block_height(b)))
  {
    // timestamps must be strictly increasing, waiting at least CRYPTONOTE_DPOS_BLOCK_MINIMUM_BLOCK_SPACING seconds
    uint64_t prev_timestamp;
    
    if (m_blocks_index.count(b.prev_id))
      prev_timestamp = m_blocks[m_blocks_index[b.prev_id]].bl.timestamp;
    else if (m_alternative_chains.count(b.prev_id))
      prev_timestamp = m_alternative_chains[b.prev_id].bl.timestamp;
    else
    {
      LOG_ERROR("Could not find prev block when check_block_timestamp");
      return false;
    }
    
    if (b.timestamp < prev_timestamp + CRYPTONOTE_DPOS_BLOCK_MINIMUM_BLOCK_SPACING)
    {
      LOG_ERROR("Block timestamp is not > previous block timestamp + " << CRYPTONOTE_DPOS_BLOCK_MINIMUM_BLOCK_SPACING << "s");
      return false;
    }
  }
  if(timestamps.size() < BLOCKCHAIN_TIMESTAMP_CHECK_WINDOW)
    return true;

  uint64_t median_ts = epee::misc_utils::median(timestamps);

  if(b.timestamp < median_ts)
  {
    LOG_PRINT_L0("Timestamp of block with id: " << get_block_hash(b) << ", " << b.timestamp << ", less than median of last " << BLOCKCHAIN_TIMESTAMP_CHECK_WINDOW << " blocks, " << median_ts);
    return false;
  }

  return true;
}

//------------------------------------------------------------------
bool blockchain_storage::check_block_type(const block& bl)
{
  crypto::hash id = get_block_hash(bl);
  
  bool pow_block = is_pow_block(bl);
  bool should_be_pow_block = !config::in_pos_era(get_block_height(bl));
  
  if (should_be_pow_block && !pow_block)
  {
    LOG_ERROR("Block with id: " << id << " should be pow block, but isn't"
              << " (bl.major_version=" << ((uint32_t)bl.major_version)
              << ", block_height=" << get_block_height(bl)
              << ", dpos_switch_block=" << config::dpos_switch_block
              << ")");
    return false;
  }
  
  if (!should_be_pow_block && pow_block)
  {
    LOG_PRINT_L0("Block with id: " << id << ENDL << " should be pos block, but isn't"
                 << " (bl.major_version=" << ((uint32_t)bl.major_version)
                 << ", block_height=" << get_block_height(bl)
                 << ", dpos_switch_block=" << config::dpos_switch_block
                 << ")");
    return false;
  }
  
  return true;
}
//------------------------------------------------------------------
bool blockchain_storage::check_pow_pos(const block& bl, const difficulty_type& current_diffic, block_verification_context& bvc,
                                       crypto::hash& proof_of_work)
{
  crypto::hash id = get_block_hash(bl);
  
  proof_of_work = null_hash;
  
  if (is_pow_block(bl))
  {
    if (!get_block_longhash(bl, proof_of_work, m_blocks.size(), crypto::g_boulderhash_state, true))
    {
      LOG_ERROR("Block with id: " << id << " could not get block long hash");
      bvc.m_missing_longhash = true;
      bvc.m_verifivation_failed = true;
      return false;
    }
    
    if (!check_hash(proof_of_work, current_diffic))
    {
      LOG_PRINT_L0("Block with id: " << id << ENDL
                   << " have not enough proof of work: " << proof_of_work << ENDL
                   << "expected difficulty: " << current_diffic);
      bvc.m_verifivation_failed = true;
      return false;
    }
    
    crypto::signature sig;
    if (crypto::g_hash_cache.create_signed_hash(id, proof_of_work, sig))
    {
      LOG_PRINT_GREEN("Hash for block " << id << " is " << proof_of_work << ", signature is " << sig, LOG_LEVEL_1);
    }
    
    return true;
  }
  
  // proof of stake block
  delegate_id_t signing_delegate;
  block prev_block;
  if (!get_block_by_hash(bl.prev_id, prev_block))
  {
    LOG_ERROR("Block with id: " << id << ENDL << " could not get previous block");
    bvc.m_verifivation_failed = true;
    return false;
  }
  
  if (!get_signing_delegate(prev_block, bl.timestamp, signing_delegate))
  {
    LOG_ERROR("Block with id: " << id << ENDL << " could not get signing delegate for timestamp " << bl.timestamp);
    bvc.m_verifivation_failed = true;
    return false;
  }
  
  if (signing_delegate != bl.signing_delegate_id)
  {
    LOG_ERROR("Block with id: " << id << ENDL << " has signing delegate " << bl.signing_delegate_id << ", should be " << signing_delegate);
    bvc.m_verifivation_failed = true;
    return false;
  }
  
  if (!check_dpos_block_sig(bl, m_delegates[signing_delegate].public_address))
  {
    LOG_ERROR("Block with id: " << id << ENDL << " have invalid signature for delegate " << signing_delegate);
    bvc.m_verifivation_failed = true;
    return false;
  }
  
  return true;
}
//------------------------------------------------------------------
bool blockchain_storage::handle_block_to_main_chain(const block& bl, const crypto::hash& id, block_verification_context& bvc)
{
  TIME_MEASURE_START(block_processing_time);
  CRITICAL_REGION_LOCAL(m_blockchain_lock);
  if(bl.prev_id != get_tail_id())
  {
    LOG_PRINT_L0("Block with id: " << id << ENDL
      << "have wrong prev_id: " << bl.prev_id << ENDL
      << "expected: " << get_tail_id());
    return false;
  }

  if(!check_block_timestamp_main(bl))
  {
    LOG_PRINT_L0("Block with id: " << id << ENDL
      << "have invalid timestamp: " << bl.timestamp);
    //add_block_as_invalid(bl, id);//do not add blocks to invalid storage befor proof of work check was passed
    bvc.m_verifivation_failed = true;
    return false;
  }

  if (!check_block_type(bl))
  {
    LOG_ERROR("Block with id: " << id << " has invalid pow/pos type");
    bvc.m_verifivation_failed = true;
    return false;
  }
  
  //check proof of work/delegated proof of stake
  difficulty_type current_diffic = get_difficulty_for_next_block();
  CHECK_AND_ASSERT_MES(current_diffic, false, "!!!!!!!!! difficulty overhead !!!!!!!!!");
  crypto::hash proof_of_work = null_hash;
  if(!m_checkpoints.is_in_checkpoint_zone(get_current_blockchain_height()))
  {
    if (!check_pow_pos(bl, current_diffic, bvc, proof_of_work))
    {
      LOG_PRINT_RED_L0("Block with id: " << id
                       << ENDL << " could not check pow/pos");
      bvc.m_verifivation_failed = true;
      return false;
    }
  }
  else
  {
    if (!m_checkpoints.check_block(get_current_blockchain_height(), id))
    {
      LOG_ERROR("CHECKPOINT VALIDATION FAILED");
      bvc.m_verifivation_failed = true;
      return false;
    }
  }

  if(!prevalidate_miner_transaction(bl, m_blocks.size()))
  {
    LOG_PRINT_L0("Block with id: " << id
      << " failed to pass prevalidation");
    bvc.m_verifivation_failed = true;
    return false;
  }
  size_t coinbase_blob_size = get_object_blobsize(bl.miner_tx);
  size_t cumulative_block_size = coinbase_blob_size;
  //process transactions
  if(!(validate_tx(bl.miner_tx, true) &&
       add_transaction_from_block(bl.miner_tx, get_transaction_hash(bl.miner_tx),
                                  id, get_current_blockchain_height())))
  {
    LOG_PRINT_L0("Block with id: " << id << " failed to validate and add miner transaction to blockchain storage");
    bvc.m_verifivation_failed = true;
    return false;
  }
  size_t tx_processed_count = 0;
  uint64_t fee_summary = 0;
  BOOST_FOREACH(const crypto::hash& tx_id, bl.tx_hashes)
  {
    transaction tx;
    size_t blob_size = 0;
    uint64_t fee = 0;
    if(!m_tx_pool.take_tx(tx_id, tx, blob_size, fee))
    {
      LOG_PRINT_L0("Block with id: " << id  << "have at least one unknown transaction with id: " << tx_id);
      purge_block_data_from_blockchain(bl, tx_processed_count);
      //add_block_as_invalid(bl, id);
      bvc.m_verifivation_failed = true;
      return false;
    }
    if(!validate_tx(tx, false))
    {
      LOG_PRINT_L0("Block with id: " << id  << " have at least one transaction (id: " << tx_id << ") with wrong inputs.");
      cryptonote::tx_verification_context tvc = AUTO_VAL_INIT(tvc);
      bool add_res = m_tx_pool.add_tx(tx, tvc, true);
      CHECK_AND_ASSERT_MES2(add_res, "WARNING: handle_block_to_main_chain: failed to add transaction back to transaction pool");
      purge_block_data_from_blockchain(bl, tx_processed_count);
      add_block_as_invalid(bl, id);
      LOG_PRINT_L0("Block with id " << id << " added as invalid becouse of wrong inputs in transactions");
      bvc.m_verifivation_failed = true;
      return false;
    }

    if(!add_transaction_from_block(tx, tx_id, id, get_current_blockchain_height()))
    {
       LOG_PRINT_L0("Block with id: " << id << " failed to add transaction to blockchain storage");
       cryptonote::tx_verification_context tvc = AUTO_VAL_INIT(tvc);
       bool add_res = m_tx_pool.add_tx(tx, tvc, true);
       CHECK_AND_ASSERT_MES2(add_res, "WARNING: handle_block_to_main_chain: failed to add transaction back to transaction pool");
       purge_block_data_from_blockchain(bl, tx_processed_count);
       bvc.m_verifivation_failed = true;
       return false;
    }
    fee_summary += fee;
    cumulative_block_size += blob_size;
    ++tx_processed_count;
  }
  uint64_t base_reward = 0;
  uint64_t already_generated_coins = m_blocks.size() ? m_blocks.back().already_generated_coins : 0;
  uint64_t fee_reward = is_pow_block(bl) ? fee_summary : average_past_block_fees(get_block_height(bl));
  if(!validate_miner_transaction(bl, cumulative_block_size, fee_reward, base_reward, already_generated_coins))
  {
    LOG_PRINT_L0("Block with id: " << id
      << " have wrong miner transaction");
    purge_block_data_from_blockchain(bl, tx_processed_count);
    bvc.m_verifivation_failed = true;
    return false;
  }

  block_extended_info bei = boost::value_initialized<block_extended_info>();
  bei.bl = bl;
  bei.block_cumulative_size = cumulative_block_size;
  bei.cumulative_difficulty = current_diffic;
  bei.already_generated_coins = already_generated_coins + base_reward;
  LOG_PRINT_L2("At height " << get_block_height(bl) << ", already_generated_coins="
               << print_money(bei.already_generated_coins));
  if(m_blocks.size())
    bei.cumulative_difficulty += m_blocks.back().cumulative_difficulty;

  bei.height = m_blocks.size();
  
  if (config::test_serialize_unserialize_block) {
    if (!tools::serialize_obj_to_file(bl, "tmptmptmp.block"))
    {
      LOG_PRINT_RED_L0("test_serialize_unserialize_block failed: Couldn't serialize block to test file");
    }
    else
    {
      block loaded;
      if (!tools::unserialize_obj_from_file(loaded, "tmptmptmp.block"))
      {
        LOG_PRINT_RED_L0("test_serialize_unserialize_block failed: Couldn't unserialize block from test file");
      }
      else
      {
        crypto::hash pre_hash = get_block_hash(bl);
        crypto::hash post_hash = get_block_hash(loaded);
        
        if (pre_hash != post_hash)
        {
          LOG_PRINT_RED_L0("test_serialize_unserialize_block failed: Block hash before storage " << pre_hash << " doesn't match block hash after storing and loading " << post_hash);
        }
        else
        {
          LOG_PRINT_CYAN("test_serialize_unserialize_block succeeded " << pre_hash, LOG_LEVEL_0);
        }
      }
    }
  }

  auto ind_res = m_blocks_index.insert(std::pair<crypto::hash, size_t>(id, bei.height));
  if(!ind_res.second)
  {
    LOG_ERROR("block with id: " << id << " already in block indexes");
    purge_block_data_from_blockchain(bl, tx_processed_count);
    bvc.m_verifivation_failed = true;
    return false;
  }

  m_blocks.push_back(bei);
  
  // update missing delegate block stats
  if (m_blocks.size() > 2)
  {
    const block& block_prev = m_blocks.end()[-2].bl;
    if (is_pos_block(bl) && is_pos_block(block_prev)) // skip for non-pos and first pos block
    {
      uint64_t seconds_since_prev = bl.timestamp - block_prev.timestamp;
      uint64_t n_delegates = seconds_since_prev / DPOS_DELEGATE_SLOT_TIME;
      delegate_id_t prev_delegate = block_prev.signing_delegate_id;
      
      LOG_PRINT_L1("missed delegates? seconds_since_prev=" << seconds_since_prev
                   << ", n_delegates=" << n_delegates
                   << ", prev_delegate=" << prev_delegate
                   << ", signing_delegate=" << bl.signing_delegate_id);

      for (uint64_t i=0; i < n_delegates; i++)
      {
        delegate_id_t missed = nth_delegate_after(prev_delegate + 1, i);
        m_delegates[missed].missed_blocks++;
        LOG_PRINT_L1("Delegate " << missed << " missed a block");
      }
      m_delegates[bl.signing_delegate_id].processed_blocks++;
      m_delegates[bl.signing_delegate_id].fees_received += fee_reward;
      LOG_PRINT_L1("Delegate " << bl.signing_delegate_id << " processed a block");
    }
  }
  
  if (!recalculate_top_delegates())
  {
    LOG_ERROR("CRITICAL: block resulted in invalid delegate votes");
    m_blocks.pop_back();
    purge_block_data_from_blockchain(bl, tx_processed_count);
    bvc.m_verifivation_failed = true;
    return false;
  }
  
  update_next_comulative_size_limit();
  TIME_MEASURE_FINISH(block_processing_time);
  LOG_PRINT_L1("+++++ BLOCK SUCCESSFULLY ADDED" << ENDL << "id:\t" << id
    << ENDL << "PoW:\t" << proof_of_work
    << ENDL << "HEIGHT " << bei.height << ", difficulty:\t" << current_diffic
    << ENDL << "block reward: " << print_money(fee_reward + base_reward) << "(" << print_money(base_reward) << " + " << print_money(fee_reward)
    << "), fee_summary: " << fee_summary << ", coinbase_blob_size: " << coinbase_blob_size << ", cumulative size: " << cumulative_block_size
    << ", " << block_processing_time << "ms");

  bvc.m_added_to_main_chain = true;

  m_tx_pool.on_blockchain_inc(bei.height, id);
  //LOG_PRINT_L0("BLOCK: " << ENDL << "" << dump_obj_as_json(bei.bl));
  return true;
}
//------------------------------------------------------------------
bool blockchain_storage::update_next_comulative_size_limit()
{
  std::vector<size_t> sz;
  get_last_n_blocks_sizes(sz, CRYPTONOTE_REWARD_BLOCKS_WINDOW);

  uint64_t median = misc_utils::median(sz);
  if(median <= CRYPTONOTE_BLOCK_GRANTED_FULL_REWARD_ZONE)
    median = CRYPTONOTE_BLOCK_GRANTED_FULL_REWARD_ZONE;

  m_current_block_cumul_sz_limit = median*2;
  return true;
}
//------------------------------------------------------------------
bool blockchain_storage::add_new_block(const block& bl_, block_verification_context& bvc)
{
  //copy block here to let modify block.target
  block bl = bl_;
  crypto::hash id;
  if (!get_block_hash(bl, id))
  {
    LOG_ERROR("add_new_block: Could not hash block");
    return false;
  }
  CRITICAL_REGION_LOCAL(m_tx_pool);//to avoid deadlock lets lock tx_pool for whole add/reorganize process
  CRITICAL_REGION_LOCAL1(m_blockchain_lock);
  if(have_block(id))
  {
    LOG_PRINT_L3("block with id = " << id << " already exists");
    bvc.m_already_exists = true;
    return false;
  }

  //check that block refers to chain tail
  if(!(bl.prev_id == get_tail_id()))
  {
    //chain switching or wrong block
    bvc.m_added_to_main_chain = false;
    return handle_alternative_block(bl, id, bvc);
    //never relay alternative blocks
  }

  return handle_block_to_main_chain(bl, id, bvc);
}
//------------------------------------------------------------------
uint64_t blockchain_storage::currency_decimals(coin_type type) const
{
  switch (type.contract_type) {
    case NotContract: {
      if (type.currency == CURRENCY_XPB)
        return CRYPTONOTE_DISPLAY_DECIMAL_POINT;
      
      const auto& r = m_currencies.find(type.currency);
      if (r == m_currencies.end())
      {
        LOG_ERROR("Could not find currency " << type.currency << " to get decimals");
        return 0;
      }
      return r->second.decimals;
    }
      
    case ContractCoin:
    case BackingCoin: {
      return currency_decimals(coin_type(type.backed_by_currency, NotContract, BACKED_BY_N_A));
    }
      
    case ContractTypeNA:
      return 0;
  }
  
  return 0;
}
//------------------------------------------------------------------
uint64_t blockchain_storage::get_max_vote(uint64_t voting_for_height)
{
  // no more limitations on max vote
  return std::numeric_limits<uint64_t>::max();
  
  /*CRITICAL_REGION_LOCAL(m_blockchain_lock);
  
  if (voting_for_height == 0)
    return 0;
  
  // base the limit at height X on the total # of coins at height X - 1, whose index is X - 1
  
  if (m_blocks.size() < voting_for_height)
  {
    LOG_ERROR("get_max_vote: asked for max vote at height " << voting_for_height << " but have only " << m_blocks.size() << " blocks");
    throw std::runtime_error("get_max_vote failed");
  }
  
  uint64_t res = m_blocks[voting_for_height - 1].already_generated_coins / 50;
  LOG_PRINT_L2("get_max_vote(" << voting_for_height << ") --> " << print_money(res));
  return res;*/
}
//------------------------------------------------------------------
bool blockchain_storage::reapply_votes(const vote_instance& vote_inst)
{
  CRITICAL_REGION_LOCAL(m_blockchain_lock);
  
  LOG_PRINT_L2("apply_votes: Re-applying votes");
  
  // always re-apply using current height (m_blocks.size() - 1), since multiple valid vote txs may make
  // total votes temporarily higher than the target (previous height's) limit
  uint64_t max_vote = get_max_vote(m_blocks.size() - 1);
  
  BOOST_FOREACH(const auto& item, vote_inst.votes)
  {
    const auto& delegate_id = item.first;
    const auto& amount = item.second;
    
    CHECK_AND_ASSERT_MES(m_delegates.find(delegate_id) != m_delegates.end(), false,
                         "reapply_votes: can't apply vote for non-existent delegate");
    
    CHECK_AND_ASSERT_MES(!add_amount_would_overflow(m_delegates[delegate_id].total_votes, amount), false,
                         "reapply_votes: apply vote would overflow");
    CHECK_AND_ASSERT_MES(m_delegates[delegate_id].total_votes + amount <= max_vote, false,
                         "reapply_votes: vote would go over max vote "
                         << "(" << print_money(m_delegates[delegate_id].total_votes)
                         << " + " << print_money(amount)
                         << " > " << print_money(max_vote));
    m_delegates[delegate_id].total_votes += amount;
  }
  
  return true;
}
//------------------------------------------------------------------
bool blockchain_storage::apply_votes(uint64_t vote_amount, const delegate_votes& for_delegates, vote_instance& vote_inst)
{
  CRITICAL_REGION_LOCAL(m_blockchain_lock);
  
  if (m_popping_block)
  {
    LOG_ERROR("Should not be applying votes while popping a block");
    return false;
  }
  
  LOG_PRINT_L2("apply_votes: Applying votes");
  
  vote_inst.voting_for_height = m_blocks.size(); // assume voting to be put on the next block. m_blocks.size()-1 = current height, m_blocks.size() = next block height
  vote_inst.expected_vote = vote_amount;
  vote_inst.votes.clear();
  
  uint64_t max_vote = get_max_vote(vote_inst.voting_for_height);
  
  BOOST_FOREACH(const auto& delegate_id, for_delegates)
  {
    uint64_t effective_amount = vote_amount;
    CHECK_AND_ASSERT_MES(!add_amount_would_overflow(m_delegates[delegate_id].total_votes, vote_amount), false,
                         "apply_votes: apply vote would overflow");
    
    if (m_delegates[delegate_id].total_votes + vote_amount > max_vote)
    {
      effective_amount = max_vote - m_delegates[delegate_id].total_votes;
    }
    vote_inst.votes[delegate_id] = effective_amount;
    m_delegates[delegate_id].total_votes += effective_amount;
  }
  
  return true;
}
//------------------------------------------------------------------
bool blockchain_storage::unapply_votes(const vote_instance& vote_inst, bool enforce_effective_amount)
{
  CRITICAL_REGION_LOCAL(m_blockchain_lock);
  
  LOG_PRINT_L2("unapply_votes: Unapplying votes, enforce=" << ((int)enforce_effective_amount));
  
  uint64_t max_vote = get_max_vote(vote_inst.voting_for_height);
  
  BOOST_REVERSE_FOREACH(const auto& item, vote_inst.votes)
  {
    const auto& delegate_id = item.first;
    const auto& vote_amount = item.second;
    
    CHECK_AND_ASSERT_MES(m_delegates.find(delegate_id) != m_delegates.end(), false,
                         "unapply_votes: can't unapply vote for non-existent delegate");
    
    if (enforce_effective_amount)
    {
      // amount should be full amount unless the delegate was at the max vote
      if (vote_inst.expected_vote != 0 && (vote_amount > vote_inst.expected_vote ||
          (vote_amount < vote_inst.expected_vote && m_delegates[delegate_id].total_votes != max_vote)))
      {
        LOG_ERROR("unapply_votes: blockchain recorded vote doesn't match input: " << ENDL
                  << " expected_vote=" << print_money(vote_inst.expected_vote) << ENDL
                  << " vote_amount=" << print_money(vote_amount) << ENDL
                  << " max_vote=" << print_money(max_vote) << ENDL
                  << " m_delegates[" << delegate_id << "].total_votes=" << print_money(m_delegates[delegate_id].total_votes));
        return false;
      }
    }
    
    // subtract the votes
    CHECK_AND_ASSERT_MES(sub_amount(m_delegates[delegate_id].total_votes, vote_amount), false,
                         "underflow undoing vote in blockchain");
  }
  
  return true;
}
//------------------------------------------------------------------
bool blockchain_storage::is_top_delegate(const delegate_id_t& delegate_id)
{
  CRITICAL_REGION_LOCAL(m_blockchain_lock);
  
  return m_top_delegates.count(delegate_id) > 0;
}
//------------------------------------------------------------------
bool blockchain_storage::recalculate_top_delegates()
{
  CRITICAL_REGION_LOCAL(m_blockchain_lock);
  
  uint64_t max_vote = get_max_vote(m_blocks.size() - 1);
  
  // check that all delegates have valid votes
  BOOST_FOREACH(const auto& item, m_delegates)
  {
    CHECK_AND_ASSERT_MES(item.second.total_votes <= max_vote, false,
                         "recalculate_top_delegates: max vote is " << print_money(max_vote)
                         << " but delegate " << item.first << " has " << print_money(item.second.total_votes) << " votes");
  }
  
  // calculate the top & autovote delegates
  m_top_delegates.clear();
  m_autovote_delegates.clear();
  
  if (m_delegates.size() == 0)
    return true;
  
  delegate_id_t *delegate_ids = new delegate_id_t[m_delegates.size()];
  
  size_t i = 0;
  BOOST_FOREACH(const auto& item, m_delegates)
  {
    delegate_ids[i] = item.first;
    i++;
  }
  
  // sort by # votes to get top delegates
  std::sort(delegate_ids, delegate_ids + m_delegates.size(),
            [this](delegate_id_t a, delegate_id_t b) {
              auto& a_info = this->m_delegates[a];
              auto& b_info = this->m_delegates[b];
              
              if (a_info.total_votes < b_info.total_votes)
                return true;
              if (a_info.total_votes != b_info.total_votes) // > than
                return false;
              
              // tie-break on address string and delegate_id
              if (a_info.address_as_string < b_info.address_as_string)
                return true;
              if (a_info.address_as_string != b_info.address_as_string) // > than
                return false;
              
              if (a_info.delegate_id < b_info.delegate_id)
                return true;
              if (a_info.delegate_id != b_info.delegate_id) // > than
                return false;
              
              // exactly equal
              return false;
            });
  std::reverse(delegate_ids, delegate_ids + m_delegates.size());
  
  // assign all the cached ranks
  for (i=0; i < m_delegates.size(); i++)
  {
    m_delegates[delegate_ids[i]].cached_vote_rank = i;
  }
  
  for (i=0; i < config::dpos_num_delegates && i < m_delegates.size(); i++)
  {
    m_top_delegates.insert(delegate_ids[i]);
    LOG_PRINT_L2("Top delegate: " << delegate_ids[i] << " with "
                 << print_money(m_delegates[delegate_ids[i]].total_votes)
                 << "/" << print_money(max_vote) << " votes");
  }
  
  // sort by delegate rank to get autovote delegates
  std::sort(delegate_ids, delegate_ids + m_delegates.size(),
            [this](delegate_id_t a, delegate_id_t b) {
              auto& a_info = this->m_delegates[a];
              auto& b_info = this->m_delegates[b];
              
              double a_rank = get_delegate_rank(a_info);
              double b_rank = get_delegate_rank(b_info);
              
              if (a_rank < b_rank)
                return true;
              if (a_rank != b_rank)
                return false;
              
              // tie-break on address string and delegate_id
              if (a_info.address_as_string < b_info.address_as_string)
                return true;
              if (a_info.address_as_string != b_info.address_as_string) // > than
                return false;
              
              if (a_info.delegate_id < b_info.delegate_id)
                return true;
              if (a_info.delegate_id != b_info.delegate_id) // > than
                return false;
              
              // exactly equal
              return false;
            });
  std::reverse(delegate_ids, delegate_ids + m_delegates.size());
  
  // assign all the cached ranks
  for (i=0; i < m_delegates.size(); i++)
  {
    m_delegates[delegate_ids[i]].cached_autoselect_rank = i;
  }
  
  for (i=0; i < config::dpos_num_delegates && i < m_delegates.size(); i++)
  {
    m_autovote_delegates.insert(delegate_ids[i]);
    LOG_PRINT_L2("Autovote delegate: " << delegate_ids[i] << " with rank " << get_delegate_rank(m_delegates[delegate_ids[i]]));
  }
  
  delete delegate_ids;
  
  return true;
}
//------------------------------------------------------------------
delegate_id_t blockchain_storage::nth_delegate_after(const delegate_id_t& start, size_t n)
{
  if (m_top_delegates.empty())
    throw std::runtime_error("Top delegates is empty");
  
  return nth_sorted_item_after(m_top_delegates, start, n);
}
//------------------------------------------------------------------
bool blockchain_storage::get_signing_delegate(const block& block_prev, uint64_t for_timestamp, delegate_id_t& result)
{
  CRITICAL_REGION_LOCAL(m_blockchain_lock);
  
  CHECK_AND_ASSERT_MES(!m_delegates.empty(), false, "get_signing_delegate: no delegates yet");
  
  CHECK_AND_ASSERT_MES(for_timestamp >= block_prev.timestamp + CRYPTONOTE_DPOS_BLOCK_MINIMUM_BLOCK_SPACING, false,
                       "get_signing_delegate: for_timestamp is not " << CRYPTONOTE_DPOS_BLOCK_MINIMUM_BLOCK_SPACING
                       << "s away from block_prev.timestamp (" << block_prev.timestamp << ")");

  uint64_t seconds_since_prev = for_timestamp - block_prev.timestamp;
  
  delegate_id_t prev_delegate;
  if (get_block_height(block_prev) == config::dpos_switch_block - 1)
  {
    prev_delegate = 0;
    CHECK_AND_ASSERT_MES(is_pow_block(block_prev), false,
                         "get_signing_delegate: Previous block from dpos_switch_block is not pow block");
  }
  else {
    prev_delegate = block_prev.signing_delegate_id;
    CHECK_AND_ASSERT_MES(is_pos_block(block_prev), false, "get_signing_delegate: Previous block is not pos block");
  }
  
  // Each delegate has DPOS_DELEGATE_SLOT_TIME to receive, sign, and broadcast the block
  // If the DPOS_DELEGATE_SLOT_TIME is up, they missed their chance and the next delegate gets
  // a chance.  And so on.
  
  result = nth_delegate_after(prev_delegate + 1, seconds_since_prev / DPOS_DELEGATE_SLOT_TIME);
  
  CHECK_AND_ASSERT_MES(m_delegates.find(result) != m_delegates.end(), false,
                       "nth_delegate_after returned delegate not in m_delegates!");
  
  LOG_PRINT_L2("Last delegate was #" << block_prev.signing_delegate_id << ", " << seconds_since_prev << "s ago (" << for_timestamp << " - " << block_prev.timestamp << "), next delegate is " << result);
  
  return true;
}
//------------------------------------------------------------------
const delegate_votes& blockchain_storage::get_autovote_delegates()
{
  CRITICAL_REGION_LOCAL(m_blockchain_lock);
  
  return m_autovote_delegates;
}
//------------------------------------------------------------------
delegate_id_t blockchain_storage::pick_unused_delegate_id()
{
  srand(static_cast<unsigned int>(get_adjusted_time()));
  
  size_t count = 0;
  delegate_id_t candidate = 0;
  delegate_id_t result = 0;
  
  while (true)
  {
    // pick next valid candidate
    do {
      if (candidate == 65535)
        return result;
      candidate++;
    } while (m_delegates.count(candidate) != 0);
    count++;

    // chance to select it
    if (rand() % count == count - 1)
    {
      result = candidate;
    }
  }    
  return result;
}
//------------------------------------------------------------------
bool blockchain_storage::get_dpos_register_info(cryptonote::delegate_id_t& unused_delegate_id, uint64_t& registration_fee)
{
  CRITICAL_REGION_LOCAL(m_blockchain_lock);
  
  unused_delegate_id = pick_unused_delegate_id();
  CHECK_AND_ASSERT_MES(unused_delegate_id != 0, false, "No unused delegate ids left");
  
  // get registration fee
  uint64_t for_height = m_blocks.size();
  registration_fee = std::max(average_past_block_fees(for_height) * DPOS_REGISTRATION_FEE_MULTIPLE,
                              DPOS_MIN_REGISTRATION_FEE);
 
  return true;
}
//------------------------------------------------------------------
bool blockchain_storage::get_delegate_info(const cryptonote::account_public_address& addr, bs_delegate_info& res)
{
  CRITICAL_REGION_LOCAL(m_blockchain_lock);
  
  BOOST_FOREACH(const auto& item, m_delegates)
  {
    if (item.second.public_address == addr)
    {
      res = item.second;
      return true;
    }
  }
  
  return false;
}
//------------------------------------------------------------------
std::vector<cryptonote::bs_delegate_info> blockchain_storage::get_delegate_infos()
{
  CRITICAL_REGION_LOCAL(m_blockchain_lock);
  
  std::vector<cryptonote::bs_delegate_info> result;
  
  BOOST_FOREACH(const auto& item, m_delegates)
  {
    result.push_back(item.second);
  }
  
  return result;
}
