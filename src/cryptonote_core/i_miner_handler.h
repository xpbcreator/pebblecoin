// Copyright (c) 2014-2015 The Pebblecoin developers
// Copyright (c) 2012-2013 The Cryptonote developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#pragma once

#include <stdint.h>

#include "common/types.h"
#include "cryptonote_protocol/blobdatatype.h"

#include "difficulty.h"

namespace cryptonote
{
  struct block;
  struct account_public_address;
  struct i_miner_handler
  {
    virtual bool handle_block_found(block& b) = 0;
    virtual bool get_block_template(block& b, const account_public_address& adr, difficulty_type& diffic, uint64_t& height, const blobdata& ex_nonce, bool dpos_block) = 0;
    virtual bool get_next_block_info(bool& is_dpos, account_public_address& signing_delegate_address, uint64_t& time_since_last_block) = 0;
  protected:
    ~i_miner_handler(){};
  };
}
