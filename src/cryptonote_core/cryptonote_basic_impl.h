// Copyright (c) 2012-2013 The Cryptonote developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#pragma once

#include <boost/functional/hash.hpp>

#include "account.h"

namespace cryptonote {
  class transaction;
  struct block;
  
  /************************************************************************/
  /* Cryptonote helper functions                                          */
  /************************************************************************/
  size_t get_max_block_size();
  size_t get_max_tx_size();
  bool get_block_reward(size_t median_size, size_t current_block_size, uint64_t already_generated_coins, uint64_t target_block_height, uint64_t &reward);
  uint8_t get_account_address_checksum(const public_address_outer_blob& bl);
  std::string get_account_address_as_str(const account_public_address& adr);
  bool get_account_address_from_str(account_public_address& adr, const std::string& str);
  bool is_coinbase(const transaction& tx);

  bool operator ==(const cryptonote::transaction& a, const cryptonote::transaction& b);
  bool operator ==(const cryptonote::block& a, const cryptonote::block& b);
  bool operator !=(const cryptonote::transaction& a, const cryptonote::transaction& b);
  bool operator !=(const cryptonote::block& a, const cryptonote::block& b);
  
  std::ostream &operator <<(std::ostream &o, const cryptonote::account_public_address& v);
}
