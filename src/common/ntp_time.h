// Copyright (c) 2015 The Pebblecoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#pragma once

#include <ctime>
#include <string>
#include <vector>
#include <memory>

#include "syncobj.h"

namespace tools {
  bool get_ntp_time(const std::string ntp_server, uint64_t ntp_port, time_t& ntp_time, time_t timeout_ms);
  
  class ntp_time
  {
  public:
    ntp_time(const std::vector<std::string>& ntp_servers, time_t refresh_time, time_t ntp_timeout_ms=5000);
    ntp_time(time_t refresh_time, time_t ntp_timeout_ms=5000);
    ~ntp_time();
    
    time_t get_time();
    
    void apply_manual_delta(time_t delta);
    void set_ntp_timeout_ms(time_t timeout_ms);
    bool update();
    
  private:
    class impl;
    std::unique_ptr<impl> m_pimpl;
  };
}
