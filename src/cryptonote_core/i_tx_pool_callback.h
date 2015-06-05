// Copyright (c) 2015 The Cryptonote developers
// Copyright (c) 2012-2013 The Cryptonote developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#pragma once

#include "common/types.h"

namespace crypto
{
  POD_CLASS hash;
}

namespace cryptonote
{
  class transaction;
  
  class i_tx_pool_callback
  {
  public:
    virtual void on_tx_added(const crypto::hash& tx_hash, const cryptonote::transaction& tx) {}
    virtual void on_tx_removed(const crypto::hash& tx_hash, const cryptonote::transaction& tx) {}
  };
}
