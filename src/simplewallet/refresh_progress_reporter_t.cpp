// Copyright (c) 2015 The Cryptonote developers
// Copyright (c) 2012-2013 The Cryptonote developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <string>
#include <chrono>

#include "include_base_utils.h"

#include "cryptonote_config.h"

#include "simplewallet.h"

namespace cryptonote
{
  class simple_wallet::refresh_progress_reporter_t::impl
  {
  public:
    impl(cryptonote::simple_wallet& simple_wallet)
        : m_simple_wallet(simple_wallet)
        , m_blockchain_height(0)
        , m_blockchain_height_update_time()
        , m_print_time()
    {
    }
    
    void update_blockchain_height()
    {
      std::string err;
      uint64_t blockchain_height = m_simple_wallet.get_daemon_blockchain_height(err);
      if (err.empty())
      {
        m_blockchain_height = blockchain_height;
        m_blockchain_height_update_time = std::chrono::system_clock::now();
      }
      else
      {
        LOG_ERROR("Failed to get current blockchain height: " << err);
      }
    }

    cryptonote::simple_wallet& m_simple_wallet;
    uint64_t m_blockchain_height;
    std::chrono::system_clock::time_point m_blockchain_height_update_time;
    std::chrono::system_clock::time_point m_print_time;
  };
  
  simple_wallet::refresh_progress_reporter_t::refresh_progress_reporter_t(cryptonote::simple_wallet& simple_wallet)
      : m_pimpl(new impl(simple_wallet))
  {
  }
  
  simple_wallet::refresh_progress_reporter_t::~refresh_progress_reporter_t()
  {
    delete m_pimpl; m_pimpl = NULL;
  }
  
  void simple_wallet::refresh_progress_reporter_t::update(uint64_t height, bool force)
  {
    auto current_time = std::chrono::system_clock::now();
    if (std::chrono::seconds(cryptonote::config::difficulty_target() / 2) < current_time - m_pimpl->m_blockchain_height_update_time || m_pimpl->m_blockchain_height <= height)
    {
      m_pimpl->update_blockchain_height();
      m_pimpl->m_blockchain_height = (std::max)(m_pimpl->m_blockchain_height, height);
    }

    if (std::chrono::milliseconds(1) < current_time - m_pimpl->m_print_time || force)
    {
      std::cout << "Height " << height << " of " << m_pimpl->m_blockchain_height << '\r';
      m_pimpl->m_print_time = current_time;
    }
  }
}
