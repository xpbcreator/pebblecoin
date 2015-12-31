// Copyright (c) 2014-2015 The Pebblecoin developers
// Copyright (c) 2012-2013 The Cryptonote developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#pragma once

#include <atomic>
#include <memory>

#include <boost/serialization/serialization.hpp>
#include <boost/serialization/version.hpp>
#include <boost/serialization/list.hpp>
#include <boost/multi_index_container.hpp>
#include <boost/multi_index/global_fun.hpp>
#include <boost/multi_index/hashed_index.hpp>
#include <boost/multi_index/member.hpp>
#include <boost/foreach.hpp>
#include <boost/thread/thread.hpp>

#include "sqlite3/sqlite3_map.h"

#include "stxxl/vector"
#include "stxxl/io"

#include "syncobj.h"
#include "string_tools.h"
#include "packing.h"

#include "common/util.h"
#include "common/ntp_time.h"
#include "common/types.h"
#include "common/lru_cache.h"
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

namespace bs_visitor_detail {
  struct purge_transaction_visitor;
  struct add_transaction_input_visitor;
  struct check_tx_input_visitor;
}

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
  
  /************************************************************************/
  /*                                                                      */
  /************************************************************************/
  class blockchain_storage
  {
    friend class core_tester;
    friend struct bs_visitor_detail::purge_transaction_visitor;
    friend struct bs_visitor_detail::add_transaction_input_visitor;
    friend struct bs_visitor_detail::check_tx_input_visitor;
    
  public:
    struct transaction_chain_entry
    {
      transaction tx;
      uint64_t m_keeper_block_height;
      size_t m_blob_size;
      std::vector<uint64_t> m_global_output_indexes;
    };
    
    PACK(POD_CLASS blockchain_entry
    {
    public:
      crypto::hash hash;
      uint64_t height;
      uint64_t timestamp; // duplicate, but prevent having to get block out of storage
      size_t block_cumulative_size;
      difficulty_type cumulative_difficulty;
      uint64_t already_generated_coins;
    });

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
    
    blockchain_storage(tx_memory_pool& tx_pool, tools::ntp_time& ntp_time_in);
    ~blockchain_storage();

    bool init(const std::string& config_folder);
    bool deinit();

    void set_checkpoints(checkpoints&& chk_pts) { m_checkpoints = chk_pts; }
    bool is_in_checkpoint_zone(uint64_t height) { return m_checkpoints.is_in_checkpoint_zone(height); }

    bool get_blocks(uint64_t start_offset, size_t count, std::list<block>& blocks, std::list<transaction>& txs) const;
    bool get_blocks(uint64_t start_offset, size_t count, std::list<block>& blocks) const;
    bool get_alternative_blocks(std::list<block>& blocks) const;
    size_t get_alternative_blocks_count() const;
    crypto::hash get_block_id_by_height(uint64_t height) const;
    bool get_block_by_hash(const crypto::hash &h, block &blk) const;
    block get_block_by_hash(const crypto::hash& h) const;
    void get_all_known_block_ids(std::list<crypto::hash> &main, std::list<crypto::hash> &alt,
                                 std::list<crypto::hash> &invalid) const;

    template<class archive_t>
    void serialize(archive_t & ar, const unsigned int version);

    bool have_tx(const crypto::hash &id) const;
    bool have_tx_keyimg_as_spent(const crypto::key_image &key_im) const;
    /// get the tx chain entry. throws exceptions if the tx is not in the blockchain
    transaction_chain_entry get_tx_chain_entry(const crypto::hash &id) const;

    template<class visitor_t>
    bool scan_outputkeys_for_indexes(const txin_to_key& tx_in_to_key, coin_type type,
                                     visitor_t& vis, uint64_t* pmax_related_block_height = NULL) const;

    uint64_t get_current_blockchain_height() const;
    bool get_delegate_address(const delegate_id_t& delegate_id, account_public_address& address) const;
    crypto::hash get_tail_id() const;
    crypto::hash get_tail_id(uint64_t& height) const;
    uint64_t get_top_block_height() const;
    difficulty_type get_difficulty_for_next_block() const;
    bool add_new_block(const block& bl_, block_verification_context& bvc);
    bool reset_and_set_genesis_block(const block& b);
    bool create_block_template(block& b, const account_public_address& miner_address, difficulty_type& di,
                               uint64_t& height, const blobdata& ex_nonce, bool dpos_block) const;
    bool have_block(const crypto::hash& id) const;
    size_t get_total_transactions() const;
    bool get_outs(coin_type type, uint64_t amount, std::list<crypto::public_key>& pkeys) const;
    bool get_short_chain_history(std::list<crypto::hash>& ids) const;
    bool find_blockchain_supplement(const std::list<crypto::hash>& qblock_ids, NOTIFY_RESPONSE_CHAIN_ENTRY::request& resp) const;
    bool find_blockchain_supplement(const std::list<crypto::hash>& qblock_ids, uint64_t& starter_offset) const;
    bool find_blockchain_supplement(const std::list<crypto::hash>& qblock_ids,
                                    std::list<std::pair<block, std::list<transaction> > >& blocks,
                                    uint64_t& total_height, uint64_t& start_height, size_t max_count) const;
    bool handle_get_objects(NOTIFY_REQUEST_GET_OBJECTS::request& arg, NOTIFY_RESPONSE_GET_OBJECTS::request& rsp) const;
    bool handle_get_objects(const COMMAND_RPC_GET_RANDOM_OUTPUTS_FOR_AMOUNTS::request& req,
                            COMMAND_RPC_GET_RANDOM_OUTPUTS_FOR_AMOUNTS::response& res) const;
    bool get_random_outs_for_amounts(const COMMAND_RPC_GET_RANDOM_OUTPUTS_FOR_AMOUNTS::request& req,
                                     COMMAND_RPC_GET_RANDOM_OUTPUTS_FOR_AMOUNTS::response& res) const;
    bool get_key_image_seqs(const COMMAND_RPC_GET_KEY_IMAGE_SEQS::request& req,
                            COMMAND_RPC_GET_KEY_IMAGE_SEQS::response& res) const;
    bool get_backward_blocks_sizes(size_t from_height, std::vector<size_t>& sz, size_t count) const;
    bool get_tx_outputs_gindexs(const crypto::hash& tx_id, std::vector<uint64_t>& indexs) const;
    bool store_blockchain();
    bool check_tx_in_to_key(const transaction& tx, size_t i, const txin_to_key& txin,
                            const crypto::hash& tx_prefix_hash=null_hash,
                            uint64_t* pmax_related_block_height = NULL) const;
    bool check_tx_in_mint(const transaction& tx, size_t i, const txin_mint& txin) const;
    bool check_tx_in_remint(const transaction& tx, size_t i, const txin_remint& txin) const;
    bool check_tx_in_create_contract(const transaction& tx, size_t i, const txin_create_contract& txin) const;
    bool check_tx_in_mint_contract(const transaction& tx, size_t i, const txin_mint_contract& txin) const;
    bool check_tx_in_grade_contract(const transaction& tx, size_t i, const txin_grade_contract& txin) const;
    bool check_tx_in_resolve_bc_coins(const transaction& tx, size_t i, const txin_resolve_bc_coins& inp) const;
    bool check_tx_in_fuse_bc_coins(const transaction& tx, size_t i, const txin_fuse_bc_coins& inp) const;
    bool check_tx_in_register_delegate(const transaction& tx, size_t i, const txin_register_delegate& inp) const;
    bool check_tx_in_vote(const transaction& tx, size_t i, const txin_vote& inp,
                          const crypto::hash& tx_prefix_hash_=null_hash, uint64_t* pmax_related_block_height=NULL) const;
    bool check_tx_inputs(const transaction& tx, uint64_t* pmax_used_block_height = NULL) const;
    bool check_tx_out_to_key(const transaction& tx, size_t i, const txout_to_key& inp) const;
    bool check_tx_outputs(const transaction& tx) const;
    bool validate_tx(const transaction& tx, bool is_miner_tx, uint64_t* pmax_used_block_height = NULL) const;
    uint64_t get_current_comulative_blocksize_limit() const;
    bool is_storing_blockchain() const {return m_is_blockchain_storing;}
    uint64_t block_difficulty(size_t i) const;
    uint64_t already_generated_coins(size_t i) const;
    
    // cache the block fees for average_past_block_fees() performance
    uint64_t get_block_fees(uint64_t block_height) const;
    uint64_t average_past_block_fees(uint64_t for_block_height) const;
    
    bool get_blocks(const std::list<crypto::hash>& block_ids,
                    std::list<block>& blocks,
                    std::list<crypto::hash>& missed_bs) const;

    bool get_transactions(const std::vector<crypto::hash>& txs_ids,
                          std::list<transaction>& txs,
                          std::list<crypto::hash>& missed_txs) const;
    
    uint64_t currency_decimals(coin_type type) const;
    
    //debug functions
    void print_blockchain(uint64_t start_index, uint64_t end_index) const;
    void print_blockchain_index() const;
    void print_blockchain_outs(const std::string& file) const;

    uint64_t get_adjusted_time() const;
    bool get_signing_delegate(const block& block_prev, uint64_t timestamp, delegate_id_t& result) const;
    
    const delegate_votes& get_autovote_delegates() const;
    bool get_dpos_register_info(cryptonote::delegate_id_t& unused_delegate_id, uint64_t& registration_fee) const;
    
    bool get_delegate_info(const cryptonote::account_public_address& addr, bs_delegate_info& res) const;
    std::vector<cryptonote::bs_delegate_info> get_delegate_infos() const;
    
  private:
    // 1024 entries per block, 4 blocks per page, cache of 8 pages = 32,768 blocks in memory cache at once
    typedef stxxl::VECTOR_GENERATOR<blockchain_entry, 4, 8, sizeof(blockchain_entry)*1024>::result blockchain_entries;
    
    /*// use compare greater since we have easier access to null_hash
    struct CryptoHashCompareGreater
    {
      bool operator()(const crypto::hash& a, const crypto::hash& b) const;
      static crypto::hash max_value();
    };
    typedef stxxl::map<crypto::hash, blockchain_entry, CryptoHashCompareGreater> blockchain_entry_by_hash;*/
    typedef sqlite3::sqlite3_map<crypto::hash, blockchain_entry> blockchain_entry_by_hash;
    
    typedef sqlite3::sqlite3_map<crypto::hash, block> blocks_by_hash;
    typedef sqlite3::sqlite3_map<crypto::hash, size_t> blocks_by_id_index;
    typedef sqlite3::sqlite3_map<crypto::hash, transaction_chain_entry> transactions_container;
    typedef std::unordered_set<crypto::key_image> key_images_container;
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
    mutable epee::critical_section m_blockchain_lock;

    // main chain
    std::string m_blockchain_entries_file;              // the temporary filename where the blockchain entries are
    std::unique_ptr<stxxl::syscall_file> m_pstxxl_file; // the stxxl::file using the above filename
    std::unique_ptr<blockchain_entries> m_pblockchain_entries; // block height -> blockchain_entry, using the above stxxl::file
    blocks_by_hash m_blocks_by_hash;         // block id -> block
    blocks_by_id_index m_blocks_index;       // block id -> height
    transactions_container m_transactions;   // transaction id -> transaction chain entry
    key_images_container m_spent_keys;
    size_t m_current_block_cumul_sz_limit;
    bool m_popping_block;
    // all alternative chains
    blockchain_entry_by_hash m_alternative_chain_entries;
    // some invalid blocks
    blockchain_entry_by_hash m_invalid_block_entries;

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
    
    tools::ntp_time& m_ntp_time;
    
    size_t m_changes_since_store;
    
    // not serialized, just in-mem caches
    mutable cache::lru_cache <crypto::hash, uint64_t> m_cached_block_fees;
    
    bool switch_to_alternative_blockchain(std::list<crypto::hash>& alt_chain, bool discard_disconnected_chain);
    bool pop_block_from_blockchain();
    bool purge_block_data_from_blockchain(const block& b, size_t processed_tx_count);
    bool purge_transaction_from_blockchain(const crypto::hash& tx_id);
    bool purge_transaction_data_from_blockchain(const transaction& tx, bool strict_check);

    bool handle_block_to_main_chain(const block& bl, block_verification_context& bvc);
    bool handle_block_to_main_chain(const block& bl, const crypto::hash& id, block_verification_context& bvc);
    bool handle_alternative_block(const block& b, const crypto::hash& id, block_verification_context& bvc);
    difficulty_type get_next_difficulty_for_alternative_chain(const std::list<crypto::hash>& alt_chain,
                                                              blockchain_entry& bent) const;
    bool prevalidate_miner_transaction(const block& b, uint64_t height) const;
    bool validate_miner_transaction(const block& b, size_t cumulative_block_size, uint64_t fee, uint64_t& base_reward,
                                    uint64_t already_generated_coins) const;
    bool validate_transaction(const block& b, uint64_t height, const transaction& tx) const;
    bool rollback_blockchain_switching(std::list<block>& original_chain, size_t rollback_height);
    bool add_transaction_from_block(const transaction& tx, const crypto::hash& tx_id, const crypto::hash& bl_id,
                                    uint64_t bl_height);
    bool push_transaction_to_global_outs_index(const transaction& tx, const crypto::hash& tx_id,
                                               std::vector<uint64_t>& global_indexes);
    bool pop_transaction_from_global_index(const transaction& tx, const crypto::hash& tx_id);
    bool get_last_n_blocks_sizes(std::vector<size_t>& sz, size_t count) const;
    bool add_out_to_get_random_outs(const std::vector<std::pair<crypto::hash, size_t> >& amount_outs,
                                    COMMAND_RPC_GET_RANDOM_OUTPUTS_FOR_AMOUNTS::outs_for_amount& result_outs,
                                    uint64_t amount, size_t i) const;
    bool is_tx_spendtime_unlocked(uint64_t unlock_time) const;
    bool is_contract_resolved(uint64_t contract) const;
    bool add_block_as_invalid(const block& bl, const crypto::hash& h);
    bool add_block_as_invalid(const blockchain_entry& bent, const block& bl, const crypto::hash& h);
    size_t find_end_of_allowed_index(const std::vector<std::pair<crypto::hash, size_t> >& amount_outs) const;
    bool check_block_timestamp_main(const block& b) const;
    bool check_block_timestamp(std::vector<uint64_t> timestamps, const block& b) const;
    bool complete_timestamps_vector(uint64_t start_height, std::vector<uint64_t>& timestamps) const;
    bool update_next_comulative_size_limit();
    
    uint64_t get_max_vote(uint64_t voting_for_height) const;
    bool reapply_votes(const vote_instance& vote_inst);
    bool apply_votes(uint64_t vote_amount, const delegate_votes& for_delegates, vote_instance& votes);
    bool unapply_votes(const vote_instance& vote_inst, bool enforce_effective_amount);
    bool is_top_delegate(const delegate_id_t& delegate_id) const;
    bool recalculate_top_delegates();
    
    delegate_id_t nth_delegate_after(const delegate_id_t& start, size_t n) const;

    bool check_block_type(const block& bl) const;
    bool check_pow_pos(const block& bl, const difficulty_type& current_diffic, block_verification_context& bvc,
                       crypto::hash& proof_of_work) const;
    
    delegate_id_t pick_unused_delegate_id() const;
    
    uint64_t get_check_count() const;
    void print_sizes() const;
    
    bool load_blockchain();
    void reset();
    void close_blockchain_entries();
    
    /// -------------------------------------------------------
    /// isolate all the code to do the ram conversion
    struct v15_ram_converter {
      bool requires_conversion;
      blockchain_storage& bs;
      
      v15_ram_converter(blockchain_storage& bs) : requires_conversion(false), bs(bs) { }
      
      uint64_t get_check_count() const;
      void print_sizes() const;
      
      bool process_conversion();
      
      // old data & types
      struct block_extended_info
      {
        block   bl;
        uint64_t height;
        size_t block_cumulative_size;
        difficulty_type cumulative_difficulty;
        uint64_t already_generated_coins;
        
        template<class archive_t>
        void serialize(archive_t & ar, const unsigned int version)
        {
          ar & bl;
          ar & height;
          ar & cumulative_difficulty;
          ar & block_cumulative_size;
          ar & already_generated_coins;
        }
      };
      
      typedef std::unordered_map<crypto::hash, size_t> blocks_by_id_index;
      typedef std::unordered_map<crypto::hash, transaction_chain_entry> transactions_container;
      typedef std::unordered_map<crypto::hash, block_extended_info> blocks_ext_by_hash;
      
      std::vector<block_extended_info> m_blocks;
      blocks_by_id_index m_blocks_index;
      transactions_container m_transactions;
      blocks_ext_by_hash m_alternative_chains;
      blocks_ext_by_hash m_invalid_blocks;
      
      // -------------------------------------
      template <class archive_t>
      void load_old_version(archive_t& ar, const unsigned int version);
    };
    v15_ram_converter m_v15_ram_converter;

    template<class archive_t, class obj_t>
    friend void process_check_count(archive_t& ar, obj_t& obj);
  };

  // required for stxxl's verbose logging
  inline std::ostream &operator <<(std::ostream &o, const blockchain_storage::blockchain_entry &bent) {
    o << "blockchain_entry";
    return o;
  }
  
  /************************************************************************/
  /*                                                                      */
  /************************************************************************/
  
  #define CURRENT_BLOCKCHAIN_STORAGE_ARCHIVE_VER    16

  template<class archive_t, class obj_t>
  void process_check_count(archive_t& ar, obj_t& obj)
  {
    auto total_check_count = obj.get_check_count();
    if (archive_t::is_saving::value)
    {
      ar & total_check_count;
      return;
    }
    
    uint64_t total_check_count_loaded = 0;
    ar & total_check_count_loaded;
    
    if (total_check_count != total_check_count_loaded)
    {
      LOG_ERROR("Data corruption detected. total_count loaded from file = " << total_check_count_loaded << ", expected = " << total_check_count);
      obj.print_sizes();
      throw std::runtime_error("Old blockchain data corruption");
    }
  }

  template<class archive_t>
  void blockchain_storage::serialize(archive_t & ar, const unsigned int version)
  {
    if (version < 11)
      return;
    
    CRITICAL_REGION_LOCAL(m_blockchain_lock);
    
    if (version <= 15) {
      LOG_PRINT_YELLOW("Loading old version " << version << " blockchain, will have to convert...", LOG_LEVEL_0);
      m_v15_ram_converter.load_old_version(ar, version);
      return;
    }
    
    /* if (archive_t::is_saving::value)
    {
      throw std::runtime_error("Not ready to save new version yet");
    } */
    
    // blocks, block index, transactions loaded from other files
    ar & m_spent_keys;
    // alternative chains from other files
    ar & m_outputs;
    // invalid blocks from other files
    ar & m_current_block_cumul_sz_limit;
    
    ar & m_currencies;
    ar & m_used_currency_descriptions;
    
    ar & m_contracts;
    
    ar & m_delegates;
    ar & m_vote_histories;
    
    process_check_count(ar, *this);
    print_sizes();
  }
  
  template <class archive_t>
  void blockchain_storage::v15_ram_converter::load_old_version(archive_t& ar, const unsigned int version)
  {
    // load old blockchain and mark needs conversion
    requires_conversion = true;
    ar & m_blocks;
    ar & m_blocks_index;
    ar & m_transactions;
    ar & bs.m_spent_keys;
    ar & m_alternative_chains;
    
    if (version < 13)
    {
      // load old version
      old_outputs_container old_outputs;
      ar & old_outputs;
      bs.m_outputs.clear();
      txout_target_v bah = txout_to_key();
      BOOST_FOREACH(const auto& item, old_outputs)
      {
        bs.m_outputs[std::make_pair(CP_XPB, item.first)] = item.second;
      }
    }
    else
    {
      ar & bs.m_outputs;
    }
    
    ar & m_invalid_blocks;
    ar & bs.m_current_block_cumul_sz_limit;
    
    if (version >= 13)
    {
      ar & bs.m_currencies;
      ar & bs.m_used_currency_descriptions;
    }
    
    if (version >= 14)
    {
      ar & bs.m_contracts;
    }
    
    if (version >= 15)
    {
      ar & bs.m_delegates;
      ar & bs.m_vote_histories;
    }
    
    if (version > 11)
    {
      process_check_count(ar, *this);
    }
    print_sizes();
  }

  //------------------------------------------------------------------
  template<class visitor_t>
  bool blockchain_storage::scan_outputkeys_for_indexes(const txin_to_key& tx_in_to_key, coin_type type,
                                                       visitor_t& vis, uint64_t* pmax_related_block_height) const
  {
    CRITICAL_REGION_LOCAL(m_blockchain_lock);
    auto it = m_outputs.find(std::make_pair(type, tx_in_to_key.amount));
    if (it == m_outputs.end() || !tx_in_to_key.key_offsets.size())
      return false;

    std::vector<uint64_t> absolute_offsets = relative_output_offsets_to_absolute(tx_in_to_key.key_offsets);

    const auto & amount_outs_vec = it->second;
    size_t count = 0;
    BOOST_FOREACH(uint64_t i, absolute_offsets)
    {
      if (i >= amount_outs_vec.size())
      {
        LOG_ERROR("Wrong index in transaction inputs: " << i << ", expected maximum " << amount_outs_vec.size() - 1
                  << ", (type, amount)=(" << type << ", " << tx_in_to_key.amount << ")");
        return false;
      }
      
      const auto tx_it = m_transactions.find(amount_outs_vec[i].first);
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
