// Copyright (c) 2015 The Pebblecoin developers
// Copyright (c) 2012-2013 The Cryptonote developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#pragma once

#include <stdint.h>

namespace cryptonote {
  struct block;
  class transaction;
}

namespace tools {
  struct wallet2_transfer_details;
  struct wallet2_known_transfer_details;

  class i_wallet2_callback
  {
  public:
    virtual void on_new_block_processed(uint64_t height, const cryptonote::block& block) {}
    virtual void on_money_received(uint64_t height, const tools::wallet2_transfer_details& td) {}
    virtual void on_money_spent(uint64_t height, const tools::wallet2_transfer_details& td) {}
    virtual void on_skip_transaction(uint64_t height, const cryptonote::transaction& tx) {}
    virtual void on_new_transfer(const cryptonote::transaction& tx, const tools::wallet2_known_transfer_details& kd) {}
  };
}
