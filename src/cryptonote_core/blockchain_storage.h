// Copyright (c) 2014-2015 The Pebblecoin developers
// Copyright (c) 2012-2013 The Cryptonote developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#pragma once

#include <atomic>

#include <boost/serialization/serialization.hpp>
#include <boost/serialization/version.hpp>
#include <boost/serialization/list.hpp>
#include <boost/multi_index_container.hpp>
#include <boost/multi_index/global_fun.hpp>
#include <boost/multi_index/hashed_index.hpp>
#include <boost/multi_index/member.hpp>
#include <boost/foreach.hpp>
#include <boost/thread/thread.hpp>

#include "syncobj.h"
#include "string_tools.h"

#include "common/util.h"
#include "common/ntp_time.h"
#include "crypto/hash.h"
#include "cryptonote_protocol/cryptonote_protocol_defs.h"
#include "rpc/core_rpc_server_commands_defs.h"

#include "tx_pool.h"
#include "cryptonote_basic.h"
#include "difficulty.h"
#include "cryptonote_format_utils.h"
#include "verification_context.h"
#include "checkpoints.h"
#include "nulls.h"

namespace cryptonote
{
  class core_tester;
  
  struct bs_delegate_info
  {
    delegate_id_t delegate_id;
    account_public_address public_address;
    std::string address_as_string;
    
    uint64_t total_votes;
    
    uint64_t processed_blocks;
    uint64_t missed_blocks;
    uint64_t fees_received;
    
    uint64_t cached_vote_rank;
    uint64_t cached_autoselect_rank;
  };
  
  inline bool operator==(const bs_delegate_info& a, const bs_delegate_info& b)
  {
    return a.delegate_id == b.delegate_id
        && a.public_address == b.public_address
        && a.address_as_string == b.address_as_string
        && a.total_votes == b.total_votes
        && a.processed_blocks == b.processed_blocks
        && a.missed_blocks == b.missed_blocks
        && a.fees_received == b.fees_received
        && a.cached_vote_rank == b.cached_vote_rank
        && a.cached_autoselect_rank == b.cached_autoselect_rank;
  }
  inline bool operator!=(const bs_delegate_info& a, const bs_delegate_info& b)
  {
    return !(a == b);
  }
  
  namespace detail {
    struct purge_transaction_visitor;
    struct add_transaction_input_visitor;
    struct check_tx_input_visitor;
  }
  
  /************************************************************************/
  /*                                                                      */
  /************************************************************************/
  class blockchain_storage
  {
    friend class core_tester;
    friend class detail::purge_transaction_visitor;
    friend class detail::add_transaction_input_visitor;
    friend class detail::check_tx_input_visitor;
    
  public:
    struct transaction_chain_entry
    {
      transaction tx;
      uint64_t m_keeper_block_height;
      size_t m_blob_size;
      std::vector<uint64_t> m_global_output_indexes;
    };

    struct block_extended_info
    {
      block   bl;
      uint64_t height;
      size_t block_cumulative_size;
      difficulty_type cumulative_difficulty;
      uint64_t already_generated_coins;
    };
    
    struct currency_info
    {
      uint64_t currency;
      std::string description;
      uint64_t decimals;
      uint64_t total_amount_minted;
      std::list<crypto::public_key> remint_key_history;
      
      crypto::public_key remint_key() const { return remint_key_history.back(); }
    };
    
    struct contract_info
    {
      uint64_t contract;
      std::string description;
      crypto::public_key grading_key;
      uint32_t fee_scale;
      uint64_t expiry_block;
      uint32_t default_grade;
      
      // {currency: amount of contract coins minted backed by that currency}
      std::map<uint64_t, uint64_t> total_amount_minted;
      bool is_graded;
      uint32_t grade;
    };
    
    typedef bs_delegate_info delegate_info;
    
    struct vote_instance
    {
      uint64_t voting_for_height;
      uint64_t expected_vote;
      std::map<delegate_id_t, uint64_t> votes;
    };
    
    blockchain_storage(tx_memory_pool& tx_pool, tools::ntp_time& ntp_time_in):m_tx_pool(tx_pool), m_current_block_cumul_sz_limit(0), m_is_in_checkpoint_zone(false), m_is_blockchain_storing(false), m_pcatchup_thread(NULL), m_stop_catchup(false), m_ntp_time(ntp_time_in), m_popping_block(false)
    {};

    bool init() { return init(tools::get_default_data_dir()); }
    bool init(const std::string& config_folder);
    bool deinit();

    void set_checkpoints(checkpoints&& chk_pts) { m_checkpoints = chk_pts; }

    bool get_blocks(uint64_t start_offset, size_t count, std::list<block>& blocks, std::list<transaction>& txs);
    bool get_blocks(uint64_t start_offset, size_t count, std::list<block>& blocks);
    bool get_alternative_blocks(std::list<block>& blocks);
    size_t get_alternative_blocks_count();
    crypto::hash get_block_id_by_height(uint64_t height);
    bool get_block_by_hash(const crypto::hash &h, block &blk);
    void get_all_known_block_ids(std::list<crypto::hash> &main, std::list<crypto::hash> &alt, std::list<crypto::hash> &invalid);

    template<class archive_t>
    void serialize(archive_t & ar, const unsigned int version);

    bool have_tx(const crypto::hash &id);
    bool have_tx_keyimg_as_spent(const crypto::key_image &key_im);
    transaction_chain_entry *get_tx_chain_entry(const crypto::hash &id);
    transaction *get_tx(const crypto::hash &id);

    template<class visitor_t>
    bool scan_outputkeys_for_indexes(const txin_to_key& tx_in_to_key, coin_type type,
                                     visitor_t& vis, uint64_t* pmax_related_block_height = NULL);

    uint64_t get_current_blockchain_height();
    bool get_delegate_address(const delegate_id_t& delegate_id, account_public_address& address);
    crypto::hash get_tail_id();
    crypto::hash get_tail_id(uint64_t& height);
    difficulty_type get_difficulty_for_next_block();
    bool add_new_block(const block& bl_, block_verification_context& bvc);
    bool reset_and_set_genesis_block(const block& b);
    bool create_block_template(block& b, const account_public_address& miner_address, difficulty_type& di, uint64_t& height, const blobdata& ex_nonce, bool dpos_block);
    bool have_block(const crypto::hash& id);
    size_t get_total_transactions();
    bool get_outs(coin_type type, uint64_t amount, std::list<crypto::public_key>& pkeys);
    bool get_short_chain_history(std::list<crypto::hash>& ids);
    bool find_blockchain_supplement(const std::list<crypto::hash>& qblock_ids, NOTIFY_RESPONSE_CHAIN_ENTRY::request& resp);
    bool find_blockchain_supplement(const std::list<crypto::hash>& qblock_ids, uint64_t& starter_offset);
    bool find_blockchain_supplement(const std::list<crypto::hash>& qblock_ids, std::list<std::pair<block, std::list<transaction> > >& blocks, uint64_t& total_height, uint64_t& start_height, size_t max_count);
    bool handle_get_objects(NOTIFY_REQUEST_GET_OBJECTS::request& arg, NOTIFY_RESPONSE_GET_OBJECTS::request& rsp);
    bool handle_get_objects(const COMMAND_RPC_GET_RANDOM_OUTPUTS_FOR_AMOUNTS::request& req, COMMAND_RPC_GET_RANDOM_OUTPUTS_FOR_AMOUNTS::response& res);
    bool get_random_outs_for_amounts(const COMMAND_RPC_GET_RANDOM_OUTPUTS_FOR_AMOUNTS::request& req, COMMAND_RPC_GET_RANDOM_OUTPUTS_FOR_AMOUNTS::response& res);
    bool get_backward_blocks_sizes(size_t from_height, std::vector<size_t>& sz, size_t count);
    bool get_tx_outputs_gindexs(const crypto::hash& tx_id, std::vector<uint64_t>& indexs);
    bool store_blockchain();
    bool check_tx_in_to_key(const transaction& tx, size_t i, const txin_to_key& txin,
                            const crypto::hash& tx_prefix_hash=null_hash,
                            uint64_t* pmax_related_block_height = NULL);
    bool check_tx_in_mint(const transaction& tx, size_t i, const txin_mint& txin);
    bool check_tx_in_remint(const transaction& tx, size_t i, const txin_remint& txin);
    bool check_tx_in_create_contract(const transaction& tx, size_t i, const txin_create_contract& txin);
    bool check_tx_in_mint_contract(const transaction& tx, size_t i, const txin_mint_contract& txin);
    bool check_tx_in_grade_contract(const transaction& tx, size_t i, const txin_grade_contract& txin);
    bool check_tx_in_resolve_bc_coins(const transaction& tx, size_t i, const txin_resolve_bc_coins& inp);
    bool check_tx_in_fuse_bc_coins(const transaction& tx, size_t i, const txin_fuse_bc_coins& inp);
    bool check_tx_in_register_delegate(const transaction& tx, size_t i, const txin_register_delegate& inp);
    bool check_tx_in_vote(const transaction& tx, size_t i, const txin_vote& inp,
                          const crypto::hash& tx_prefix_hash_=null_hash, uint64_t* pmax_related_block_height=NULL);
    bool check_tx_inputs(const transaction& tx, uint64_t* pmax_used_block_height = NULL);
    bool check_tx_out_to_key(const transaction& tx, size_t i, const txout_to_key& inp);
    bool check_tx_outputs(const transaction& tx);
    bool validate_tx(const transaction& tx, bool is_miner_tx, uint64_t* pmax_used_block_height = NULL);
    uint64_t get_current_comulative_blocksize_limit();
    bool is_storing_blockchain(){return m_is_blockchain_storing;}
    uint64_t block_difficulty(size_t i);
    uint64_t already_generated_coins(size_t i);
    uint64_t average_past_block_fees(uint64_t for_block_height);
    
    template<class t_ids_container, class t_blocks_container, class t_missed_container>
    bool get_blocks(const t_ids_container& block_ids, t_blocks_container& blocks, t_missed_container& missed_bs)
    {
      CRITICAL_REGION_LOCAL(m_blockchain_lock);

      BOOST_FOREACH(const auto& bl_id, block_ids)
      {
        auto it = m_blocks_index.find(bl_id);
        if(it == m_blocks_index.end())
          missed_bs.push_back(bl_id);
        else
        {
          CHECK_AND_ASSERT_MES(it->second < m_blocks.size(), false, "Internal error: bl_id=" << epee::string_tools::pod_to_hex(bl_id)
            << " have index record with offset="<<it->second<< ", bigger then m_blocks.size()=" << m_blocks.size());
            blocks.push_back(m_blocks[it->second].bl);
        }
      }
      return true;
    }

    template<class t_ids_container, class t_tx_container, class t_missed_container>
    bool get_transactions(const t_ids_container& txs_ids, t_tx_container& txs, t_missed_container& missed_txs)
    {
      CRITICAL_REGION_LOCAL(m_blockchain_lock);

      BOOST_FOREACH(const auto& tx_id, txs_ids)
      {
        auto it = m_transactions.find(tx_id);
        if(it == m_transactions.end())
        {
          transaction tx;
          if(!m_tx_pool.get_transaction(tx_id, tx))
            missed_txs.push_back(tx_id);
          else
            txs.push_back(tx);
        }
        else
          txs.push_back(it->second.tx);
      }
      return true;
    }
    
    uint64_t currency_decimals(coin_type type) const;
    
    //debug functions
    void print_blockchain(uint64_t start_index, uint64_t end_index);
    void print_blockchain_index();
    void print_blockchain_outs(const std::string& file);

    uint64_t get_adjusted_time();
    bool get_signing_delegate(const block& block_prev, uint64_t timestamp, delegate_id_t& result);
    
    const delegate_votes& get_autovote_delegates();
    bool get_dpos_register_info(cryptonote::delegate_id_t& unused_delegate_id, uint64_t& registration_fee);
    
    bool get_delegate_info(const cryptonote::account_public_address& addr, bs_delegate_info& res);
    std::vector<cryptonote::bs_delegate_info> get_delegate_infos();
    
  private:
    typedef std::unordered_map<crypto::hash, size_t> blocks_by_id_index;
    typedef std::unordered_map<crypto::hash, transaction_chain_entry> transactions_container;
    typedef std::unordered_set<crypto::key_image> key_images_container;
    typedef std::vector<block_extended_info> blocks_container;
    typedef std::unordered_map<crypto::hash, block_extended_info> blocks_ext_by_hash;// {crypto::hash: block_extended_info}
    typedef std::unordered_map<crypto::hash, block> blocks_by_hash;
    // outputs_vector: [(tx_hash, vout_index)]
    typedef std::vector<std::pair<crypto::hash, size_t> > outputs_vector;
    // old_outputs_container : {amount: outputs_vector}
    typedef std::map<uint64_t, outputs_vector> old_outputs_container;
    // outputs_container : {(coin_type, amount) : outputs_vector}
    // coin_type is essentially 3-tuple:
      // 1st slot is currency/contract: CURRENCY_XPB for xpb, or the id of the currency or contract
      // 2nd slot is a CoinConntractType indicating the coin's contract status
      // 3rd slot is BACKED_BY_N_A for NotContract, or the currency backing the contract/backing coin
    // 2nd part of pair is the amount
    typedef std::pair<coin_type, uint64_t> output_key;
    typedef std::map<output_key, outputs_vector> outputs_container;
    typedef std::unordered_map<uint64_t, currency_info> currency_by_id;
    typedef std::unordered_map<uint64_t, contract_info> contract_by_id;
    typedef std::unordered_set<std::string> descriptions_container;
    typedef std::map<delegate_id_t, delegate_info> delegate_info_container;
    typedef std::vector<vote_instance> vote_history;
    typedef std::unordered_map<crypto::key_image, vote_history> vote_history_container;

    tx_memory_pool& m_tx_pool;
    epee::critical_section m_blockchain_lock; // TODO: add here reader/writer lock

    // main chain
    blocks_container m_blocks;               // height  -> block_extended_info
    blocks_by_id_index m_blocks_index;       // crypto::hash -> height
    transactions_container m_transactions;
    key_images_container m_spent_keys;
    size_t m_current_block_cumul_sz_limit;
    bool m_popping_block;
    // all alternative chains
    blocks_ext_by_hash m_alternative_chains;
    // some invalid blocks
    blocks_ext_by_hash m_invalid_blocks;

    // outputs
    outputs_container m_outputs;

    // currencies/contracts
    currency_by_id m_currencies;
    contract_by_id m_contracts;
    descriptions_container m_used_currency_descriptions;
    
    // dpos
    delegate_info_container m_delegates;
    vote_history_container m_vote_histories;
    delegate_votes m_top_delegates; // not serialized
    delegate_votes m_autovote_delegates; // not serialized
    
    std::string m_config_folder;
    checkpoints m_checkpoints;
    std::atomic<bool> m_is_in_checkpoint_zone;
    std::atomic<bool> m_is_blockchain_storing;
    std::atomic<bool> m_stop_catchup;
    
    boost::thread *m_pcatchup_thread;
    
    tools::ntp_time& m_ntp_time;

    bool switch_to_alternative_blockchain(std::list<blocks_ext_by_hash::iterator>& alt_chain, bool discard_disconnected_chain);
    bool pop_block_from_blockchain();
    bool purge_block_data_from_blockchain(const block& b, size_t processed_tx_count);
    bool purge_transaction_from_blockchain(const crypto::hash& tx_id);
    bool purge_transaction_data_from_blockchain(const transaction& tx, bool strict_check);

    bool handle_block_to_main_chain(const block& bl, block_verification_context& bvc);
    bool handle_block_to_main_chain(const block& bl, const crypto::hash& id, block_verification_context& bvc);
    bool handle_alternative_block(const block& b, const crypto::hash& id, block_verification_context& bvc);
    difficulty_type get_next_difficulty_for_alternative_chain(const std::list<blocks_ext_by_hash::iterator>& alt_chain, block_extended_info& bei);
    bool prevalidate_miner_transaction(const block& b, uint64_t height);
    bool validate_miner_transaction(const block& b, size_t cumulative_block_size, uint64_t fee, uint64_t& base_reward, uint64_t already_generated_coins);
    bool validate_transaction(const block& b, uint64_t height, const transaction& tx);
    bool rollback_blockchain_switching(std::list<block>& original_chain, size_t rollback_height);
    bool add_transaction_from_block(const transaction& tx, const crypto::hash& tx_id, const crypto::hash& bl_id, uint64_t bl_height);
    bool push_transaction_to_global_outs_index(const transaction& tx, const crypto::hash& tx_id, std::vector<uint64_t>& global_indexes);
    bool pop_transaction_from_global_index(const transaction& tx, const crypto::hash& tx_id);
    bool get_last_n_blocks_sizes(std::vector<size_t>& sz, size_t count);
    bool add_out_to_get_random_outs(std::vector<std::pair<crypto::hash, size_t> >& amount_outs, COMMAND_RPC_GET_RANDOM_OUTPUTS_FOR_AMOUNTS::outs_for_amount& result_outs, uint64_t amount, size_t i);
    bool is_tx_spendtime_unlocked(uint64_t unlock_time);
    bool is_contract_resolved(uint64_t contract);
    bool add_block_as_invalid(const block& bl, const crypto::hash& h);
    bool add_block_as_invalid(const block_extended_info& bei, const crypto::hash& h);
    size_t find_end_of_allowed_index(const std::vector<std::pair<crypto::hash, size_t> >& amount_outs);
    bool check_block_timestamp_main(const block& b);
    bool check_block_timestamp(std::vector<uint64_t> timestamps, const block& b);
    bool complete_timestamps_vector(uint64_t start_height, std::vector<uint64_t>& timestamps);
    bool update_next_comulative_size_limit();
    
    void catchup_signed_hashes();
    
    uint64_t get_max_vote(uint64_t voting_for_height);
    bool reapply_votes(const vote_instance& vote_inst);
    bool apply_votes(uint64_t vote_amount, const delegate_votes& for_delegates, vote_instance& votes);
    bool unapply_votes(const vote_instance& vote_inst, bool enforce_effective_amount);
    bool is_top_delegate(const delegate_id_t& delegate_id);
    bool recalculate_top_delegates();
    
    delegate_id_t nth_delegate_after(const delegate_id_t& start, size_t n);

    bool check_block_type(const block& bl);
    bool check_pow_pos(const block& bl, const difficulty_type& current_diffic, block_verification_context& bvc,
                       crypto::hash& proof_of_work);
    
    delegate_id_t pick_unused_delegate_id();
  };


  /************************************************************************/
  /*                                                                      */
  /************************************************************************/

  #define CURRENT_BLOCKCHAIN_STORAGE_ARCHIVE_VER    15

  template<class archive_t>
  void blockchain_storage::serialize(archive_t & ar, const unsigned int version)
  {
    if(version < 11)
      return;
    CRITICAL_REGION_LOCAL(m_blockchain_lock);
    ar & m_blocks;
    ar & m_blocks_index;
    ar & m_transactions;
    ar & m_spent_keys;
    ar & m_alternative_chains;
    if (version < 13)
    {
      if (archive_t::is_saving::value)
      {
        throw std::runtime_error("Shouldn't save old version");
      }
      else
      {
        old_outputs_container old_outputs;
        ar & old_outputs;
        m_outputs.clear();
        txout_target_v bah = txout_to_key();
        BOOST_FOREACH(const auto& item, old_outputs)
        {
          m_outputs[std::make_pair(CP_XPB, item.first)] = item.second;
        }
      }
    }
    else
    {
      ar & m_outputs;
    }
    ar & m_invalid_blocks;
    ar & m_current_block_cumul_sz_limit;
    
    if (version >= 13)
    {
      ar & m_currencies;
      ar & m_used_currency_descriptions;
    }
    
    if (version >= 14)
    {
      ar & m_contracts;
    }
    
    if (version >= 15)
    {
      ar & m_delegates;
      ar & m_vote_histories;
    }
    
    /*serialization bug workaround*/
    if(version > 11)
    {
      uint64_t total_check_count = m_blocks.size() + m_blocks_index.size() + m_transactions.size() + m_spent_keys.size() + m_alternative_chains.size() + m_outputs.size() + m_invalid_blocks.size() + m_current_block_cumul_sz_limit + m_currencies.size() + m_used_currency_descriptions.size() + m_contracts.size() + m_delegates.size() + m_vote_histories.size();
      if(archive_t::is_saving::value)
      {        
        ar & total_check_count;
      }else
      {
        uint64_t total_check_count_loaded = 0;
        ar & total_check_count_loaded;
        if(total_check_count != total_check_count_loaded)
        {
          LOG_ERROR("Blockchain storage data corruption detected. total_count loaded from file = " << total_check_count_loaded << ", expected = " << total_check_count);

          LOG_PRINT_L0("Blockchain storage:" << ENDL << 
            "m_blocks: " << m_blocks.size() << ENDL  << 
            "m_blocks_index: " << m_blocks_index.size() << ENDL  << 
            "m_transactions: " << m_transactions.size() << ENDL  << 
            "m_spent_keys: " << m_spent_keys.size() << ENDL  << 
            "m_alternative_chains: " << m_alternative_chains.size() << ENDL  << 
            "m_outputs: " << m_outputs.size() << ENDL  << 
            "m_invalid_blocks: " << m_invalid_blocks.size() << ENDL  << 
            "m_current_block_cumul_sz_limit: " << m_current_block_cumul_sz_limit << ENDL <<
            "m_currencies: " << m_currencies.size() << ENDL <<
            "m_contracts: " << m_contracts.size() << ENDL <<
            "m_used_currency_descriptions: " << m_used_currency_descriptions.size() << ENDL <<
            "m_delegates:" << m_delegates.size() << ENDL <<
            "m_vote_histories:" << m_delegates.size() << ENDL);
          throw std::runtime_error("Blockchain data corruption");
        }
      }
    }

    LOG_PRINT_L2("Blockchain storage:" << ENDL <<
        "m_blocks: " << m_blocks.size() << ENDL  << 
        "m_blocks_index: " << m_blocks_index.size() << ENDL  << 
        "m_transactions: " << m_transactions.size() << ENDL  << 
        "m_spent_keys: " << m_spent_keys.size() << ENDL  << 
        "m_alternative_chains: " << m_alternative_chains.size() << ENDL  << 
        "m_outputs: " << m_outputs.size() << ENDL  << 
        "m_invalid_blocks: " << m_invalid_blocks.size() << ENDL  << 
        "m_current_block_cumul_sz_limit: " << m_current_block_cumul_sz_limit << ENDL <<
        "m_currencies: " << m_currencies.size() << ENDL <<
        "m_contracts: " << m_contracts.size() << ENDL <<
        "m_used_currency_descriptions: " << m_used_currency_descriptions.size() << ENDL <<
        "m_delegates: " << m_delegates.size() << ENDL <<
        "m_vote_histories:" << m_vote_histories.size() << ENDL);
  }

  //------------------------------------------------------------------
  template<class visitor_t>
  bool blockchain_storage::scan_outputkeys_for_indexes(const txin_to_key& tx_in_to_key, coin_type type,
                                                       visitor_t& vis, uint64_t* pmax_related_block_height)
  {
    CRITICAL_REGION_LOCAL(m_blockchain_lock);
    auto it = m_outputs.find(std::make_pair(type, tx_in_to_key.amount));
    if (it == m_outputs.end() || !tx_in_to_key.key_offsets.size())
      return false;

    std::vector<uint64_t> absolute_offsets = relative_output_offsets_to_absolute(tx_in_to_key.key_offsets);

    std::vector<std::pair<crypto::hash, size_t> >& amount_outs_vec = it->second;
    size_t count = 0;
    BOOST_FOREACH(uint64_t i, absolute_offsets)
    {
      if (i >= amount_outs_vec.size())
      {
        LOG_ERROR("Wrong index in transaction inputs: " << i << ", expected maximum " << amount_outs_vec.size() - 1
                  << ", (type, amount)=(" << type << ", " << tx_in_to_key.amount << ")");
        return false;
      }
      
      auto tx_it = m_transactions.find(amount_outs_vec[i].first);
      CHECK_AND_ASSERT_MES(tx_it != m_transactions.end(), false, "Wrong transaction id in output indexes: " << epee::string_tools::pod_to_hex(amount_outs_vec[i].first));
      CHECK_AND_ASSERT_MES(amount_outs_vec[i].second < tx_it->second.tx.outs().size(), false,
                           "Wrong index in transaction outputs: " << amount_outs_vec[i].second
                           << ", expected less then " << tx_it->second.tx.outs().size());
      
      if (!vis.handle_output(tx_it->second.tx, amount_outs_vec[i].second, tx_it->second.tx.outs()[amount_outs_vec[i].second]))
      {
        LOG_ERROR("Failed to handle_output for output no = " << count << ", with absolute offset " << i);
        return false;
      }
      
      if (count++ == absolute_offsets.size() - 1 && pmax_related_block_height)
      {
        if (*pmax_related_block_height < tx_it->second.m_keeper_block_height)
          *pmax_related_block_height = tx_it->second.m_keeper_block_height;
      }
    }

    return true;
  }
}



BOOST_CLASS_VERSION(cryptonote::blockchain_storage, CURRENT_BLOCKCHAIN_STORAGE_ARCHIVE_VER)
