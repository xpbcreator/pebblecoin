// Copyright (c) 2015 The Pebblecoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <cassert>
#include <algorithm>
#include <atomic>
#include <thread>
#include <chrono>

#include <boost/asio.hpp>
#include <boost/lexical_cast.hpp>

#include "include_base_utils.h"
#include "misc_language.h"

#include "blocking_udp_client.h"
#include "ntp_time.h"

using boost::asio::ip::udp;

namespace tools
{
  static const char *DEFAULT_NTP_SERVERS[] = {
    "0.pool.ntp.org",
    "1.pool.ntp.org",
    "2.pool.ntp.org",
    "3.pool.ntp.org",
    "ntp.ubuntu.com",
    "ntp-nist.ldsbc.edu",
    "2.fr.pool.ntp.org",
    "0.europe.pool.ntp.org",
    "1.europe.pool.ntp.org",
    "2.europe.pool.ntp.org",
    "3.europe.pool.ntp.org",
    "1.oceania.pool.ntp.org",
    "nist1-pa.ustiming.org",
    "time-c.nist.gov",
    "nist1-macon.macon.ga.us",
    "nist.netservicesgroup.com",
    "nisttime.carsoncity.k12.mi.us",
    "nist1-lnk.binary.net",
    "wwv.nist.gov",
    "time.nist.gov",
    "utcnist.colorado.edu",
    "utcnist2.colorado.edu",
    "nist1-lv.ustiming.org",
    "time-nw.nist.gov",
    "nist-time-server.eoni.com",
  };
  static size_t N_DEFAULT_SERVERS = 21;
  
  bool get_ntp_time(const std::string ntp_server, uint64_t ntp_port, time_t& ntp_time, time_t timeout_ms)
  {
    try {
      
      boost::asio::io_service io_service;
      udp::resolver resolver(io_service);
      udp::resolver::query query(udp::v4(), ntp_server, boost::lexical_cast<std::string>(ntp_port));
      udp::endpoint time_server_endpoint = *resolver.resolve(query);
      
      assert(sizeof(uint32_t) == 4);
      uint32_t buf[12] = {0};
      const uint32_t BUFSIZE = sizeof(buf);
      
      // open
      blocking_udp_client client;
      
      // request
      buf[0] = htonl((3 << 27) | (3 << 24));    // s. RFC 4330, http://www.apps.ietf.org/rfc/rfc4330.html
      if (!client.send_to(boost::asio::buffer(buf, BUFSIZE), time_server_endpoint))
      {
        LOG_ERROR("Error sending to " << ntp_server << ":" << ntp_port);
        return false;
      }
      
      // response
      boost::asio::ip::udp::endpoint here;
      if (!client.receive_from(boost::asio::buffer(buf, BUFSIZE), here, boost::posix_time::milliseconds(timeout_ms)))
      {
        LOG_ERROR("Error/timeout receiving from " << ntp_server << ":" << ntp_port);
        return false;
      }
      
      // process result
      time_t secs = ntohl(buf[8]) - 2208988800u;
      ntp_time = secs;
      return true;

    }
    catch (std::exception& e) {
      LOG_ERROR("std::exception in get_ntp_time: " << e.what());
      return false;
    }
    catch (...) {
      LOG_ERROR("Unknown exception in get_ntp_time");
      return false;
    }
  }
  
  class ntp_time::impl {
  public:
    impl(const std::vector<std::string>& ntp_servers, time_t refresh_time, time_t ntp_timeout_ms)
        : m_ntp_servers(ntp_servers), m_refresh_time(refresh_time), m_last_refresh(0)
        , m_ntp_minus_local(0), m_manual_delta(0), m_which_server(0)
        , m_ntp_timeout_ms(ntp_timeout_ms)
        , m_run(true)
        , m_check_update_thread(boost::bind(&impl::check_update_thread, this))
    {
      if (m_ntp_servers.empty())
      {
        throw std::runtime_error("Must provide at least one ntp server");
      }
      std::srand(time(NULL));
      std::random_shuffle(m_ntp_servers.begin(), m_ntp_servers.end());
    }
    
    ~impl()
    {
      m_run = false;
      m_check_update_thread.join();
    }
    
    bool should_update() const
    {
      CRITICAL_REGION_LOCAL(m_time_lock);
      return time(NULL) - m_last_refresh >= m_refresh_time;
    }
    
    void check_update_thread()
    {
      auto last = std::chrono::high_resolution_clock::now();
      while (m_run)
      {
        std::this_thread::sleep_for(std::chrono::milliseconds(1000));
        auto now = std::chrono::high_resolution_clock::now();
        
        if (now < last || now - last > std::chrono::milliseconds(1500))
        {
          LOG_PRINT_YELLOW("System clock change detected, will update ntp time", LOG_LEVEL_0);
          CRITICAL_REGION_LOCAL(m_time_lock);
          m_last_refresh = 0;
        }
        last = now;
      }
      LOG_PRINT_L0("check_update_thread() finished");
    }
    
    std::vector<std::string> m_ntp_servers;
    time_t m_refresh_time;
    time_t m_last_refresh;
    
    time_t m_ntp_minus_local;
    time_t m_manual_delta;
    size_t m_which_server;
    
    time_t m_ntp_timeout_ms;
    
    mutable epee::critical_section m_time_lock;
    
    std::atomic<bool> m_run;
    std::thread m_check_update_thread;
  };
  
  ntp_time::ntp_time(const std::vector<std::string>& ntp_servers, time_t refresh_time, time_t ntp_timeout_ms)
      : m_pimpl(new impl(ntp_servers, refresh_time, ntp_timeout_ms))
  {
  }
  
  ntp_time::ntp_time(time_t refresh_time, time_t ntp_timeout_ms)
      : m_pimpl(new impl(std::vector<std::string>(DEFAULT_NTP_SERVERS, DEFAULT_NTP_SERVERS + N_DEFAULT_SERVERS),
                         refresh_time, ntp_timeout_ms))
  {
  }
  
  ntp_time::~ntp_time()
  {
  }
  
  bool ntp_time::update()
  {
    CRITICAL_REGION_LOCAL(m_pimpl->m_time_lock);
    LOG_PRINT_L2("Updating time from " << m_pimpl->m_ntp_servers[m_pimpl->m_which_server] << "...");
    
    time_t ntp_time;
    
    if (!get_ntp_time(m_pimpl->m_ntp_servers[m_pimpl->m_which_server], 123, ntp_time, m_pimpl->m_ntp_timeout_ms))
    {
      LOG_ERROR("Failed to get ntp time from " << m_pimpl->m_ntp_servers[m_pimpl->m_which_server] << ", rotating to next one");
      m_pimpl->m_which_server = (m_pimpl->m_which_server + 1) % m_pimpl->m_ntp_servers.size();
      return false;
    }

    time_t now = time(NULL);
    m_pimpl->m_ntp_minus_local = ntp_time - now;
    // don't affect manual delta
    m_pimpl->m_last_refresh = now;
    
    LOG_PRINT_L2("Local time is " << m_pimpl->m_ntp_minus_local << "s behind, plus manual delta of "
                 << m_pimpl->m_manual_delta << "s");
    return true;
  }
  
  void ntp_time::apply_manual_delta(time_t delta)
  {
    CRITICAL_REGION_LOCAL(m_pimpl->m_time_lock);
    m_pimpl->m_manual_delta += delta;
    LOG_PRINT_L2("Applying manual delta of " << delta << "s, total manual delta is now " << m_pimpl->m_manual_delta << "s");
  }
  
  void ntp_time::set_ntp_timeout_ms(time_t timeout_ms)
  {
    CRITICAL_REGION_LOCAL(m_pimpl->m_time_lock);
    m_pimpl->m_ntp_timeout_ms = timeout_ms;
    LOG_PRINT_L2("Timeout for getting NTP time is now " << timeout_ms << "ms");
  }

  time_t ntp_time::get_time()
  {
    CRITICAL_REGION_LOCAL(m_pimpl->m_time_lock);
    if (m_pimpl->should_update())
    {
      update();
    }
    
    return m_pimpl->m_ntp_minus_local + time(NULL) + m_pimpl->m_manual_delta;
  }
}
