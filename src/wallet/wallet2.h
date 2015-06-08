// Copyright (c) 2014-2015 The Pebblecoin developers
// Copyright (c) 2012-2013 The Cryptonote developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#pragma once

#include <atomic>

#include "common/types.h"
#include "crypto/hash.h"
#include "cryptonote_config.h"
#include "cryptonote_core/account.h"
#include "cryptonote_core/cryptonote_basic.h"
#include "cryptonote_core/cryptonote_format_utils.h"
#include "cryptonote_core/keypair.h"
#include "rpc/core_rpc_server_commands_defs.h"

#include "i_wallet2_callback.h"
#include "split_strategies.h"

#define DEFAULT_TX_SPENDABLE_AGE                               10
#define WALLET_DEFAULT_RCP_CONNECTION_TIMEOUT                  200000
#define MAX_VOTE_INPUTS_PER_TX                                 30

namespace epee
{
  namespace net_utils
  {
    namespace http
    {
      class http_simple_client;
    }
  }
}

namespace tools
{
  typedef cryptonote::COMMAND_RPC_GET_RANDOM_OUTPUTS_FOR_AMOUNTS::out_entry out_entry;
  typedef std::vector<std::list<out_entry> > fake_outs_container;
  typedef std::unordered_map<cryptonote::coin_type, fake_outs_container> fake_outs_map;
  typedef std::map<crypto::key_image, uint64_t> key_image_seqs;
  
  class wallet2;
  class wallet_tx_builder;
  
  struct tx_dust_policy
  {
    uint64_t dust_threshold;
    bool add_to_fee;
    cryptonote::account_public_address addr_for_dust;

    tx_dust_policy(uint64_t a_dust_threshold = 0, bool an_add_to_fee = true, cryptonote::account_public_address an_addr_for_dust = cryptonote::account_public_address())
      : dust_threshold(a_dust_threshold)
      , add_to_fee(an_add_to_fee)
      , addr_for_dust(an_addr_for_dust)
    {
    }
  };

  struct wallet2_transfer_details
  {
    uint64_t m_block_height;
    bool m_from_miner_tx;
    cryptonote::transaction m_tx;
    size_t m_internal_output_index;
    uint64_t m_global_output_index;
    bool m_spent;
    cryptonote::transaction m_spent_by_tx;
    crypto::key_image m_key_image; //TODO: key_image stored twice :(

    uint64_t amount() const { return m_tx.outs()[m_internal_output_index].amount; }
    cryptonote::coin_type cp() const { return m_tx.out_cp(m_internal_output_index); }
    uint64_t currency() const { return cp().currency; }
    cryptonote::CoinContractType contract_type() const { return cp().contract_type; }
    uint64_t backed_by() const { return cp().backed_by_currency; }
  };
  
  struct wallet2_known_transfer_details
  {
    crypto::hash m_tx_hash;
    std::vector<cryptonote::tx_destination_entry> m_dests;
    uint64_t m_fee;
    uint64_t m_xpb_change;
    cryptonote::currency_map m_all_change;
    // mint/remint
    uint64_t m_currency_minted;
    uint64_t m_amount_minted;
    // dpos
    cryptonote::delegate_id_t m_delegate_id_registered;
    cryptonote::account_public_address m_delegate_address_registered;
    uint64_t m_registration_fee_paid;
    std::map<cryptonote::delegate_id_t, uint64_t> m_votes;
  };

  struct wallet2_voting_batch
  {
    std::list<size_t> m_transfer_indices;
    fake_outs_container m_fake_outs;
    std::vector<cryptonote::delegate_votes> m_vote_history;
    
    uint64_t amount(const wallet2& wallet) const;
    bool spent(const wallet2& wallet) const;
  };
  
  struct wallet2_votes_info
  {
    std::vector<wallet2_voting_batch> m_batches; // list of voting batches, 0th index = batch 1
    std::map<size_t, size_t> m_transfer_batch_map;       // map index of m_transfer into which batch that transfer is in
  };
  
  class wallet2
  {
    friend struct wallet2_voting_batch;
    friend class wallet_tx_builder;
    
    wallet2(const wallet2&);
  public:
    typedef wallet2_transfer_details transfer_details;
    typedef wallet2_known_transfer_details known_transfer_details;
    
    wallet2(bool read_only=false);
    ~wallet2();

    struct payment_details
    {
      crypto::hash m_tx_hash;
      uint64_t m_amount;
      uint64_t m_block_height;
      uint64_t m_unlock_time;
      bool m_sent;
    };

    struct unconfirmed_transfer_details
    {
      cryptonote::transaction m_tx;
      uint64_t m_change;
      time_t m_sent_time;
    };
    
    typedef std::vector<transfer_details> transfer_container;
    typedef std::unordered_multimap<crypto::hash, payment_details> payment_container;
    typedef std::map<crypto::hash, known_transfer_details> known_transfer_container;

    void generate(const std::string& wallet, const std::string& password);
    void load(const std::string& wallet, const std::string& password);
    void store();
    cryptonote::account_base& get_account(){return m_account;}

    void init(const std::string& daemon_address = "http://localhost:8080", uint64_t upper_transaction_size_limit = CRYPTONOTE_BLOCK_GRANTED_FULL_REWARD_ZONE*2 - CRYPTONOTE_COINBASE_BLOB_RESERVED_SIZE);
    bool deinit();

    void stop() { m_run.store(false, std::memory_order_relaxed); }

    i_wallet2_callback* callback() const { return m_callback; }
    void callback(i_wallet2_callback* callback) { m_callback = callback; }

    void refresh();
    void refresh(size_t & blocks_fetched);
    void refresh(size_t & blocks_fetched, bool& received_money);
    bool refresh(size_t & blocks_fetched, bool& received_money, bool& ok);
    
    void set_conn_timeout(uint32_t new_timeout) { m_conn_timeout = new_timeout; }

    cryptonote::currency_map balance() const;
    cryptonote::currency_map unlocked_balance() const;
    void transfer(const std::vector<cryptonote::tx_destination_entry>& dsts, size_t min_fake_outs, size_t fake_outputs_count, uint64_t unlock_time, uint64_t fee, const std::vector<uint8_t>& extra, const detail::split_strategy& destination_split_strategy, const tx_dust_policy& dust_policy);
    void transfer(const std::vector<cryptonote::tx_destination_entry>& dsts, size_t min_fake_outs, size_t fake_outputs_count, uint64_t unlock_time, uint64_t fee, const std::vector<uint8_t>& extra, const detail::split_strategy& destination_split_strategy, const tx_dust_policy& dust_policy, cryptonote::transaction &tx);
    void transfer(const std::vector<cryptonote::tx_destination_entry>& dsts, size_t min_fake_outs, size_t fake_outputs_count, uint64_t unlock_time, uint64_t fee, const std::vector<uint8_t>& extra);
    void transfer(const std::vector<cryptonote::tx_destination_entry>& dsts, size_t min_fake_outs, size_t fake_outputs_count, uint64_t unlock_time, uint64_t fee, const std::vector<uint8_t>& extra, cryptonote::transaction& tx);
    
    void mint_subcurrency(uint64_t currency, const std::string &description, uint64_t amount, uint64_t decimals,
                          bool remintable, uint64_t fee, size_t fee_fake_outs_count);
    void remint_subcurrency(uint64_t currency, uint64_t amount,
                            bool keep_remintable, uint64_t fee, size_t fee_fake_outs_count);
    
    void register_delegate(const cryptonote::delegate_id_t& delegate_id,
                           uint64_t registration_fee, size_t min_fake_outs, size_t fake_outputs_count,
                           const cryptonote::account_public_address& address,
                           cryptonote::transaction& result);
    
    bool check_connection();
    void get_transfers(wallet2::transfer_container& incoming_transfers) const;
    void get_known_transfers(wallet2::known_transfer_container& known_transfers) const;
    bool get_known_transfer(const crypto::hash& tx_hash, wallet2::known_transfer_details& kd) const;
    size_t get_num_transfers() const;
    void get_payments(const crypto::hash& payment_id, std::list<wallet2::payment_details>& payments) const;
    bool get_payment_for_tx(const crypto::hash& tx_hash, crypto::hash& payment_id, wallet2::payment_details& payment) const;
    uint64_t get_blockchain_current_height() const { return m_local_bc_height; }
    cryptonote::account_public_address get_public_address() const { return m_account_public_address; }
    template <class t_archive>
    inline void serialize(t_archive &a, const unsigned int ver);
    static void wallet_exists(const std::string& file_path, bool& keys_file_exists, bool& wallet_file_exists);
    
    bool is_mine(const cryptonote::account_public_address& account_public_address) const;
    bool sign_dpos_block(cryptonote::block& bl) const;

    std::string debug_batches() const;
    
    const cryptonote::delegate_votes& current_delegate_set() const;
    const cryptonote::delegate_votes& user_delegates() const;
    const cryptonote::delegate_votes& autovote_delegates() const;
    
    void set_user_delegates(const cryptonote::delegate_votes& new_set);
    
    bool voting_user_delegates() const;
    void set_voting_user_delegates(bool voting_user_delegates);
    uint64_t get_amount_unvoted() const;
    
  private:
    typedef transfer_container::iterator transfer_selector;
    typedef std::vector<wallet2_voting_batch>::iterator batch_selector;
    typedef cryptonote::delegate_votes delegate_votes;
    typedef cryptonote::delegate_id_t delegate_id_t;
    
    bool store_keys(const std::string& keys_file_name, const std::string& password);
    void load_keys(const std::string& keys_file_name, const std::string& password);
    void process_new_transaction(const cryptonote::transaction& tx, uint64_t height, bool is_miner_tx);
    void process_new_blockchain_entry(const cryptonote::block& b, cryptonote::block_complete_entry& bche, crypto::hash& bl_id, uint64_t height);
    void detach_blockchain(uint64_t height);
    void get_short_chain_history(std::list<crypto::hash>& ids);
    bool is_tx_spendtime_unlocked(uint64_t unlock_time) const;
    bool is_transfer_unlocked(const transfer_details& td) const;
    bool clear();
    void pull_blocks(size_t& blocks_added);
    void pull_autovote_delegates();
    bool prepare_file_names(const std::string& file_path);
    void process_unconfirmed(const cryptonote::transaction& tx);
    void add_unconfirmed_tx(const cryptonote::transaction& tx, uint64_t change_amount);
    
    fake_outs_map get_fake_outputs(const std::unordered_map<cryptonote::coin_type, std::list<uint64_t> >& amounts,
                                   uint64_t min_fake_outs, uint64_t fake_outputs_count);
    key_image_seqs get_key_image_seqs(const std::vector<crypto::key_image>& key_images);
    
    void send_raw_tx_to_daemon(const cryptonote::transaction& tx);
    
    cryptonote::account_base m_account;
    std::string m_daemon_address;
    std::string m_wallet_file;
    std::string m_keys_file;
    std::string m_known_transfers_file;
    std::string m_currency_keys_file;
    std::string m_votes_info_file;
    epee::net_utils::http::http_simple_client *m_phttp_client;
    std::vector<crypto::hash> m_blockchain;
    std::atomic<uint64_t> m_local_bc_height; //temporary workaround
    std::unordered_map<crypto::hash, unconfirmed_transfer_details> m_unconfirmed_txs;

    transfer_container m_transfers;
    payment_container m_payments;
    known_transfer_container m_known_transfers;
    std::unordered_map<crypto::key_image, size_t> m_key_images;
    cryptonote::account_public_address m_account_public_address;
    uint64_t m_upper_transaction_size_limit; //TODO: auto-calc this value or request from daemon, now use some fixed value
    std::unordered_map<uint64_t, std::vector<cryptonote::keypair> > m_currency_keys;
    delegate_votes m_autovote_delegates;
    delegate_votes m_user_delegates;
    wallet2_votes_info m_votes_info;
    bool m_voting_user_delegates;
    
    uint32_t m_conn_timeout;

    std::atomic<bool> m_run;

    i_wallet2_callback* m_callback;
    
    bool m_read_only;
  };
}
