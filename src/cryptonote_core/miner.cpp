// Copyright (c) 2014-2015 The Pebblecoin developers
// Copyright (c) 2012-2013 The Cryptonote developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <numeric>

#include <boost/lexical_cast.hpp>
#include <boost/interprocess/detail/atomic.hpp>
#include <boost/limits.hpp>
#include <boost/foreach.hpp>
#include <boost/algorithm/string/split.hpp>
#include <boost/algorithm/string/classification.hpp>

#include "misc_language.h"
#include "include_base_utils.h"
#include "file_io_utils.h"
#include "string_coding.h"

#include "cryptonote_config.h"
#include "crypto/hash.h"
#include "crypto/crypto_basic_impl.h"
#include "wallet/wallet2.h"
#include "cryptonote_core/nulls.h"

#include "cryptonote_basic_impl.h"
#include "cryptonote_format_utils.h"
#include "miner.h"
#include "miner_opt.h"

using namespace epee;
using namespace miner_opt;

namespace cryptonote
{
  //-----------------------------------------------------------------------------------------------------
  bool miner::find_nonce_for_given_block(block& bl, const difficulty_type& diffic, uint64_t height, uint64_t **state)
  {
    for(; bl.nonce != std::numeric_limits<uint32_t>::max(); bl.nonce++)
    {
      crypto::hash h;
      if (!get_block_longhash(bl, h, height, state, false))
      {
        throw std::runtime_error("unable to get_block_longhash");
      }

      if(check_hash(h, diffic))
      {
        return true;
      }
    }
    return false;
  }
  //-----------------------------------------------------------------------------------------------------
  void miner::init_options(boost::program_options::options_description& desc)
  {
    command_line::add_arg(desc, arg_extra_messages);
    command_line::add_arg(desc, arg_start_mining);
    command_line::add_arg(desc, arg_mining_threads);
    command_line::add_arg(desc, arg_dont_share_state);
    command_line::add_arg(desc, arg_delegate_wallet_file);
    command_line::add_arg(desc, arg_delegate_wallet_password);
    command_line::add_arg(desc, arg_dpos_block_wait_time);
  }
  //-----------------------------------------------------------------------------------------------------
  //-----------------------------------------------------------------------------------------------------
  miner::miner(i_miner_handler* phandler)
      : m_stop(1)
      , m_template(AUTO_VAL_INIT(m_template))
      , m_mine_address(AUTO_VAL_INIT(m_mine_address))
      , m_template_no(0)
      , m_diffic(0)
      , m_thread_index(0)
      , m_phandler(phandler)
      , m_height(0)
      , m_pausers_count(0)
      , m_threads_total(0)
      , m_starter_nonce(0)
      , m_last_hr_merge_time(0)
      , m_hashes(0)
      , m_do_print_hashrate(false)
      , m_do_mining(false)
      , m_current_hash_rate(0)
      , m_dont_share_state(false)
      , m_pdelegate_wallet(new tools::wallet2(true))
      , m_dpos_block_wait_time(15)
  {
  }
  //-----------------------------------------------------------------------------------------------------
  miner::~miner()
  {
    stop();
    delete m_pdelegate_wallet; m_pdelegate_wallet = NULL;
  }
  //-----------------------------------------------------------------------------------------------------
  bool miner::init(const boost::program_options::variables_map& vm)
  {
    if (command_line::has_arg(vm, arg_extra_messages))
    {
      std::string buff;
      bool r = file_io_utils::load_file_to_string(command_line::get_arg(vm, arg_extra_messages), buff);
      CHECK_AND_ASSERT_MES(r, false,
                           "Failed to load file with extra messages: " << command_line::get_arg(vm, arg_extra_messages));
      std::vector<std::string> extra_vec;
      boost::split(extra_vec, buff, boost::is_any_of("\n"), boost::token_compress_on);
      m_extra_messages.resize(extra_vec.size());
      for (size_t i = 0; i != extra_vec.size(); i++)
      {
        string_tools::trim(extra_vec[i]);
        if (!extra_vec[i].size())
          continue;
        std::string buff = string_encoding::base64_decode(extra_vec[i]);
        if (buff != "0")
          m_extra_messages[i] = buff;
      }
      m_config_folder_path = boost::filesystem::path(command_line::get_arg(vm, arg_extra_messages)).parent_path().string();
      load_config();
      LOG_PRINT_L0("Loaded " << m_extra_messages.size() << " extra messages, current index "
                   << m_config.current_extra_message_index);
    }

    if (command_line::has_arg(vm, arg_start_mining))
    {
      if (!cryptonote::get_account_address_from_str(m_mine_address, command_line::get_arg(vm, arg_start_mining)))
      {
        LOG_ERROR("Target account address " << command_line::get_arg(vm, arg_start_mining)
                  << " has wrong format, starting daemon canceled");
        return false;
      }
      m_threads_total = 1;
      m_do_mining = true;
      if (command_line::has_arg(vm, arg_mining_threads))
      {
        m_threads_total = command_line::get_arg(vm, arg_mining_threads);
      }
    }
    
    if (command_line::has_arg(vm, arg_delegate_wallet_file))
    {
      std::string file_path = command_line::get_arg(vm, arg_delegate_wallet_file);
      std::string pwd = command_line::get_arg(vm, arg_delegate_wallet_password);
      
      try
      {
        m_pdelegate_wallet->load(file_path, pwd);
      }
      catch (std::exception& e)
      {
        LOG_ERROR("could not load delegate wallet file: " << e.what());
        return false;
      }
      
      if (m_threads_total == 0)
      {
        m_threads_total = 1;
      }
      m_do_mining = true;
    }
    
    if (command_line::has_arg(vm, arg_dpos_block_wait_time))
    {
      m_dpos_block_wait_time = command_line::get_arg(vm, arg_dpos_block_wait_time);
    }
    
    if (m_dpos_block_wait_time < CRYPTONOTE_DPOS_BLOCK_MINIMUM_BLOCK_SPACING)
    {
      LOG_ERROR("dpos block wait time must be at least " << CRYPTONOTE_DPOS_BLOCK_MINIMUM_BLOCK_SPACING << " seconds");
      return false;
    }
    if (m_dpos_block_wait_time >= DPOS_DELEGATE_SLOT_TIME - 5)
    {
      LOG_ERROR("dpos block wait time must be at most " << (DPOS_DELEGATE_SLOT_TIME - 5) << " seconds");
      return false;
    }
    
    m_dont_share_state = command_line::has_arg(vm, arg_dont_share_state) ? command_line::get_arg(vm, miner_opt::arg_dont_share_state) : false;

    return true;
  }
  //-----------------------------------------------------------------------------------------------------
  //-----------------------------------------------------------------------------------------------------
  bool miner::clear_block_template()
  {
    CRITICAL_REGION_LOCAL(m_template_lock);
    m_template = AUTO_VAL_INIT(m_template);
    m_diffic = 0;
    m_height = 0;
    m_signing_delegate_address = AUTO_VAL_INIT(m_signing_delegate_address);
    m_template_no = 0;
    return true;
  }
  //-----------------------------------------------------------------------------------------------------
  bool miner::set_block_template(const block& bl, const difficulty_type& di, uint64_t height,
                                 const cryptonote::account_public_address& signing_delegate_address)
  {
    CRITICAL_REGION_LOCAL(m_template_lock);
    m_template = bl;
    m_diffic = di;
    m_height = height;
    m_signing_delegate_address = signing_delegate_address;
    ++m_template_no;
    m_starter_nonce = crypto::rand<uint32_t>();
    return true;
  }
  //-----------------------------------------------------------------------------------------------------
  //-----------------------------------------------------------------------------------------------------
  bool miner::start(const account_public_address& adr, size_t threads_count)
  {
    m_mine_address = adr; // will be null if doing pos-only
    m_threads_total = static_cast<uint32_t>(threads_count);
    m_starter_nonce = crypto::rand<uint32_t>();
    CRITICAL_REGION_LOCAL(m_threads_lock);
    if(is_mining())
    {
      LOG_ERROR("Starting miner but it's already started");
      return false;
    }

    if(!m_threads.empty())
    {
      LOG_ERROR("Unable to start miner because there are active mining threads");
      return false;
    }

    if(!m_template_no)
      request_block_template();//lets update block template

    boost::interprocess::ipcdetail::atomic_write32(&m_stop, 0);
    boost::interprocess::ipcdetail::atomic_write32(&m_thread_index, 0);

    boost::thread::attributes attrs;
    attrs.set_stack_size(THREAD_STACK_SIZE);
    
    for(size_t i = 0; i != threads_count; i++)
    {
      m_threads.push_back(boost::thread(attrs, boost::bind(&miner::worker_thread, this)));
    }

    LOG_PRINT_L0("Mining has started with " << threads_count << " threads, good luck!" )
    return true;
  }
  //-----------------------------------------------------------------------------------------------------
  bool miner::stop()
  {
    boost::interprocess::ipcdetail::atomic_write32(&m_stop, 1);
    CRITICAL_REGION_LOCAL(m_threads_lock);

    BOOST_FOREACH(boost::thread& th, m_threads)
      th.join();

    m_threads.clear();
    LOG_PRINT_L0("Mining has been stopped, " << m_threads.size() << " finished" );
    return true;
  }
  //-----------------------------------------------------------------------------------------------------
  bool miner::is_mining() const
  {
    return !m_stop;
  }
  //-----------------------------------------------------------------------------------------------------
  void miner::pause()
  {
    CRITICAL_REGION_LOCAL(m_pausers_count_lock);
    ++m_pausers_count;
    if(m_pausers_count == 1 && is_mining())
      LOG_PRINT_L2("MINING PAUSED");
  }
  //-----------------------------------------------------------------------------------------------------
  void miner::resume()
  {
    CRITICAL_REGION_LOCAL(m_pausers_count_lock);
    --m_pausers_count;
    if(m_pausers_count < 0)
    {
      m_pausers_count = 0;
      LOG_PRINT_RED_L0("Unexpected miner::resume() called");
    }
    if(!m_pausers_count && is_mining())
      LOG_PRINT_L2("MINING RESUMED");
  }
  //-----------------------------------------------------------------------------------------------------
  //-----------------------------------------------------------------------------------------------------
  bool miner::on_idle()
  {
    m_update_block_template_interval.do_call([&]() {
      if (is_mining()) {
        request_block_template();
      }
      return true;
    });

    m_update_merge_hr_interval.do_call([&]() {
      merge_hr();
      return true;
    });
    
    return true;
  }
  //-----------------------------------------------------------------------------------------------------
  void miner::on_synchronized()
  {
    if (m_do_mining)
    {
      start(m_mine_address, m_threads_total);
    }
  }
  //-----------------------------------------------------------------------------------------------------
  bool miner::on_block_chain_update()
  {
    if(!is_mining())
      return true;

    return request_block_template();
  }
  //-----------------------------------------------------------------------------------------------------
  //-----------------------------------------------------------------------------------------------------
  uint64_t miner::get_speed() const
  {
    if(is_mining())
      return m_current_hash_rate;
    else
      return 0;
  }
  //-----------------------------------------------------------------------------------------------------
  void miner::set_print_hashrate(bool do_hr)
  {
    m_do_print_hashrate = do_hr;
  }
  //-----------------------------------------------------------------------------------------------------
  //-----------------------------------------------------------------------------------------------------
  void miner::merge_hr()
  {
    if(m_last_hr_merge_time && is_mining())
    {
      m_current_hash_rate = m_hashes * 1000 / ((misc_utils::get_tick_count() - m_last_hr_merge_time + 1));
      CRITICAL_REGION_LOCAL(m_last_hash_rates_lock);
      m_last_hash_rates.push_back(m_current_hash_rate);
      if(m_last_hash_rates.size() > 19)
        m_last_hash_rates.pop_front();
      if(m_do_print_hashrate)
      {
        uint64_t total_hr = std::accumulate(m_last_hash_rates.begin(), m_last_hash_rates.end(), 0);
        float hr = static_cast<float>(total_hr)/static_cast<float>(m_last_hash_rates.size());
        LOG_PRINT_L1("hashrate:" << std::setprecision(4) << std::fixed << hr << ENDL);
        //std::cout << "hashrate: " << std::setprecision(4) << std::fixed << hr << ENDL;
      }
    }
    m_last_hr_merge_time = misc_utils::get_tick_count();
    m_hashes = 0;
  }
  //-----------------------------------------------------------------------------------------------------
  bool miner::request_block_template()
  {
    bool is_dpos;
    account_public_address delegate_addr;
    uint64_t time_since_last_block;
    
    if (!m_phandler->get_next_block_info(is_dpos, delegate_addr, time_since_last_block))
    {
      LOG_ERROR("Could not get_next_block_info()");
      clear_block_template();
      return false;
    }
    
    account_public_address mine_address = is_dpos ? m_pdelegate_wallet->get_public_address() : m_mine_address;
    
    if (mine_address == null_public_address)
    {
      LOG_PRINT_L1("No mining address");
      clear_block_template();
      return true;
    }
    
    if (is_dpos)
    {
      if (m_pdelegate_wallet->get_public_address() == null_public_address)
      {
        LOG_PRINT_L1("No wallet loaded for dpos");
        clear_block_template();
        return false;
      }
      
      if (delegate_addr == null_public_address)
      {
        LOG_PRINT_L2("Still waiting for minimum dpos block time");
        clear_block_template();
        return true;
      }
      
      if (!m_pdelegate_wallet->is_mine(delegate_addr))
      {
        LOG_PRINT_L2("Next dpos block is not ours: We are "
                     << boost::lexical_cast<std::string>(m_pdelegate_wallet->get_public_address()).substr(0, 10)
                     << "..., delegate is "
                     << boost::lexical_cast<std::string>(delegate_addr).substr(0, 10)
                     << "...");
        clear_block_template();
        return true;
      }
      
      if (time_since_last_block < m_dpos_block_wait_time)
      {
        LOG_PRINT_L1("Waiting " << m_dpos_block_wait_time - time_since_last_block << " more seconds until making dpos block");
        clear_block_template();
        return true;
      }
    }
    
    block bl = AUTO_VAL_INIT(bl);
    difficulty_type di = AUTO_VAL_INIT(di);
    uint64_t height = AUTO_VAL_INIT(height);
    cryptonote::blobdata extra_nonce; 
    if (m_extra_messages.size() && m_config.current_extra_message_index < m_extra_messages.size())
    {
      extra_nonce = m_extra_messages[m_config.current_extra_message_index];
    }

    if (!m_phandler->get_block_template(bl, mine_address, di, height, extra_nonce, is_dpos))
    {
      LOG_ERROR("Failed to get_block_template()");
      clear_block_template();
      return false;
    }

    set_block_template(bl, di, height, delegate_addr);
    return true;
  }
  //-----------------------------------------------------------------------------------------------------
  bool miner::worker_thread()
  {
    bool in_pos_era;
    {
      account_public_address delegate_addr;
      uint64_t time_since_last_block;
      
      if (!m_phandler->get_next_block_info(in_pos_era, delegate_addr, time_since_last_block))
      {
        LOG_ERROR("Could not get next block info in worker thread");
        return false;
      }
    }
    LOG_PRINT_L0("in_pos_era = " << ((uint16_t)in_pos_era));
    uint32_t th_local_index = boost::interprocess::ipcdetail::atomic_inc32(&m_thread_index);
    if (in_pos_era && th_local_index != 0)
    {
      LOG_PRINT_YELLOW("Miner thread [" << th_local_index << "] not started, already in pos era",
                       LOG_LEVEL_0);
      return true;
    }
    
    LOG_PRINT_L0("Miner thread was started ["<< th_local_index << "]");
    log_space::log_singletone::set_thread_log_prefix(std::string("[miner ") + std::to_string(th_local_index) + "]");
    uint32_t nonce = m_starter_nonce + th_local_index;
    uint64_t height = 0;
    difficulty_type local_diff = 0;
    uint32_t local_template_ver = 0;
    cryptonote::account_public_address local_signing_delegate_address = AUTO_VAL_INIT(local_signing_delegate_address);
    block b;
    
    uint64_t **state = NULL;
    bool allocated_state = false;
    if (!in_pos_era)
    {
      if (m_dont_share_state || th_local_index > 0)
      {
        LOG_PRINT_L0("Mallocing boulderhash state...");
        state = crypto::pc_malloc_state();
        allocated_state = true;
      }
      else
      {
        LOG_PRINT_L0("Using global boulderhash state");
        state = crypto::g_boulderhash_state;
        allocated_state = false;
      }
    }
    
    uint64_t last_print = misc_utils::get_tick_count();
    uint64_t m_hashes_since_print = 0;
    
    while (!m_stop)
    {
      if (m_pausers_count)
      {
        misc_utils::sleep_no_w(100);
        continue;
      }

      while (local_template_ver != m_template_no)
      {
        CRITICAL_REGION_BEGIN(m_template_lock);
        b = m_template;
        local_diff = m_diffic;
        height = m_height;
        local_signing_delegate_address = m_signing_delegate_address;
        local_template_ver = m_template_no;
        CRITICAL_REGION_END();
        nonce = m_starter_nonce + th_local_index;
      }

      if (!local_template_ver)
      {
        request_block_template();
        if (!local_template_ver)
        {
          epee::misc_utils::sleep_no_w(1000);
          continue;
        }
      }
      
      bool found_block = false;
      
      if (is_pos_block(b))
      {
        if (th_local_index > 0)
        {
          LOG_PRINT_YELLOW("All but miner 0 stopping, now in pos era", LOG_LEVEL_0);
          break;
        }
        
        if (allocated_state)
        {
          LOG_PRINT_L0("Freeing boulderhash state...");
          crypto::pc_free_state(state);
          state = NULL;
          allocated_state = false;
        }
        
        // if we here, delegate address is ours and it is time
        if (!m_pdelegate_wallet->sign_dpos_block(b))
        {
          LOG_ERROR("Could not sign dpos block");
          found_block = false;
          epee::misc_utils::sleep_no_w(1000);
        }
        else
        {
          found_block = true;
        }
      }
      else
      {
        if (state == NULL)
        {
          LOG_PRINT_L0("Waiting for dpos era...");
          epee::misc_utils::sleep_no_w(60000);
          continue;
        }
        
        b.nonce = nonce;
        crypto::hash h;
        uint64_t start_t = misc_utils::get_tick_count();
        get_block_longhash(b, h, height, state, false);
        uint64_t end_t = misc_utils::get_tick_count();
        LOG_PRINT_L3("Took " << (end_t - start_t) << "ms to do a hash");
        found_block = check_hash(h, local_diff);
        if (found_block)
        {
          LOG_PRINT_GREEN("Found pow block for difficulty: " << local_diff << ENDL << "Hash was: " << h << ENDL, LOG_LEVEL_0);
          crypto::g_hash_cache.add_cached_longhash(get_block_hash(b), h);
        }
        nonce += m_threads_total;
        ++m_hashes;
        ++m_hashes_since_print;

        uint64_t now = misc_utils::get_tick_count();
        if (now - last_print >= 60000)
        {
          double hr = (double)m_hashes_since_print / ((now - last_print) / 1000.0);
          LOG_PRINT_L0(hr << " H/s (" << (1.0 / hr) << " s/H), difficulty=" << local_diff);
          last_print = now;
          m_hashes_since_print = 0;
        }
      }

      if (found_block)
      {
        if(m_phandler->handle_block_found(b))
        {
          ++m_config.current_extra_message_index;
          store_config();
        }
        else
        {
          LOG_PRINT_GREEN("(Found block not added to main chain)", LOG_LEVEL_0);
        }
      }
    }
    
    if (allocated_state)
    {
      LOG_PRINT_L0("Freeing boulderhash state...");
      crypto::pc_free_state(state);
    }
    
    LOG_PRINT_L0("Miner thread stopped ["<< th_local_index << "]");
    return true;
  }
  //-----------------------------------------------------------------------------------------------------
  //-----------------------------------------------------------------------------------------------------
}
