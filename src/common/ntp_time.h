// Copyright (c) 2015 The Pebblecoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#pragma once

#include "syncobj.h"

#include <ctime>
#include <string>
#include <vector>

namespace tools {
  bool get_ntp_time(const std::string ntp_server, uint64_t ntp_port, time_t& ntp_time, time_t timeout_ms);
  
  class ntp_time
  {
  public:
    ntp_time(const std::vector<std::string>& ntp_servers, time_t refresh_time, time_t ntp_timeout_ms=5000);
    ntp_time(time_t refresh_time, time_t ntp_timeout_ms=5000);
    
    time_t get_time();
    
    void apply_manual_delta(time_t delta);
    void set_ntp_timeout_ms(time_t timeout_ms);
    bool update();
    
  private:
    bool should_update();
    
    std::vector<std::string> m_ntp_servers;
    time_t m_refresh_time;
    time_t m_last_refresh;
    
    time_t m_ntp_minus_local;
    time_t m_manual_delta;
    size_t m_which_server;
    
    time_t m_ntp_timeout_ms;
    
    epee::critical_section m_time_lock;
  };
}
