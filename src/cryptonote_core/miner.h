// Copyright (c) 2012-2013 The Cryptonote developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#pragma once 

#include <atomic>

#include <boost/program_options.hpp>
#include <boost/thread.hpp>

#include "math_helper.h"

#include "cryptonote_protocol/blobdatatype.h"

#include "cryptonote_basic.h"
#include "difficulty.h"
#include "i_miner_handler.h"

namespace tools
{
  class wallet2;
}
namespace cryptonote
{
  /************************************************************************/
  /*                                                                      */
  /************************************************************************/
  class miner
  {
  public: 
    static void init_options(boost::program_options::options_description& desc);
    static bool find_nonce_for_given_block(block& bl, const difficulty_type& diffic, uint64_t height, uint64_t **state);
    
    miner(i_miner_handler* phandler);
    ~miner();
    bool init(const boost::program_options::variables_map& vm);
    
    bool clear_block_template();
    bool set_block_template(const block& bl, const difficulty_type& diffic, uint64_t height,
                            const cryptonote::account_public_address& signing_delegate_address);
    
    bool start(const account_public_address& adr, size_t threads_count);
    bool stop();
    bool is_mining() const;
    void pause();
    void resume();

    bool on_idle();
    void on_synchronized();
    bool on_block_chain_update();
    
    uint64_t get_speed() const;
    void set_print_hashrate(bool do_hr);
    
  private:
    bool store_config();
    bool load_config();
    
    void merge_hr();
    bool request_block_template();
    bool worker_thread();
    
    struct miner_config
    {
      uint64_t current_extra_message_index;

      BEGIN_KV_SERIALIZE_MAP()
        KV_SERIALIZE(current_extra_message_index)
      END_KV_SERIALIZE_MAP()
    };

    volatile uint32_t m_stop;
    mutable epee::critical_section m_template_lock;
    block m_template;
    std::atomic<uint32_t> m_template_no;
    std::atomic<uint32_t> m_starter_nonce;
    difficulty_type m_diffic;
    uint64_t m_height;
    cryptonote::account_public_address m_signing_delegate_address;
    volatile uint32_t m_thread_index;
    volatile uint32_t m_threads_total;
    std::atomic<int32_t> m_pausers_count;
    mutable epee::critical_section m_pausers_count_lock;

    std::list<boost::thread> m_threads;
    mutable epee::critical_section m_threads_lock;
    i_miner_handler* m_phandler;
    account_public_address m_mine_address;
    epee::math_helper::once_a_time_seconds<5> m_update_block_template_interval;
    epee::math_helper::once_a_time_seconds<2> m_update_merge_hr_interval;
    std::vector<blobdata> m_extra_messages;
    miner_config m_config;
    std::string m_config_folder_path;    
    std::atomic<uint64_t> m_last_hr_merge_time;
    std::atomic<uint64_t> m_hashes;
    std::atomic<uint64_t> m_current_hash_rate;
    mutable epee::critical_section m_last_hash_rates_lock;
    std::list<uint64_t> m_last_hash_rates;
    bool m_do_print_hashrate;
    bool m_do_mining;
    bool m_dont_share_state;
    
    tools::wallet2 *m_pdelegate_wallet;
    uint64_t m_dpos_block_wait_time;
  };
}
