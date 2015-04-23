// Copyright (c) 2015 The Pebblecoin developers
// Copyright (c) 2012-2013 The Cryptonote developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#pragma once

#include <stdint.h>
#include <stddef.h>

#include <vector>
#include <map>

#include <boost/function.hpp>

#include "common/types.h"
#include "test_event_entry.h"

namespace cryptonote
{
  struct tx_verification_context;
  struct block_verification_context;
  class tranasction;
  struct block;
}

class test_chain_unit_base
{
public:
  typedef boost::function<bool (core_t& c, size_t ev_index, const std::vector<test_event_entry> &events)> verify_callback;
  typedef std::map<std::string, verify_callback> callbacks_map;
  
  void register_callback(const std::string& cb_name, verify_callback cb);
  bool verify(const std::string& cb_name, core_t& c, size_t ev_index, const std::vector<test_event_entry> &events);
  
  virtual bool check_tx_verification_context(const cryptonote::tx_verification_context& tvc, bool tx_added,
                                             size_t event_index, const cryptonote::transaction& tx);
  virtual bool check_block_verification_context(const cryptonote::block_verification_context& bvc, size_t event_idx,
                                                const cryptonote::block& block);
  
  virtual bool generate(std::vector<test_event_entry> &events) const = 0;
  
private:
  callbacks_map m_callbacks;
};
