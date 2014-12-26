// Copyright (c)  2014 The Pebblecoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#pragma once

#include "cryptonote_genesis_config.h"

#include <stdint.h>

inline void set_test_genesis_config()
{
  static const uint64_t timestamp = UINT64_C(1411409281);
  
  GENESIS_NONCE_STRING = "Mon 2:07 PM what is up";
  GENESIS_TIMESTAMP = &timestamp;
  GENESIS_COINBASE_TX_HEX = "010a01ff0000210179e231c0574796ecc198e05afaecd3aea2d9a84fa6c51088eb75130c4eb4da72";
  GENESIS_BLOCK_ID_HEX = "b9da6ea7a7db89c28361d917c5bf020e1bc237b0152c5aa5fefe6daeac23552a";
  //GENESIS_WORK_HASH_HEX = "934bbc69446c5f17999c4dc6a1c9eec622ed349d7027be9bf0e01bf1c7842e3c";
  //GENESIS_HASH_SIGNATURE_HEX = "0587e209e07e74c22f36e11d85436ef183c89465a2edcbdf008f4215962b2602cbd0447656671c18190d998c4e745273abf6b71262143e5940cbd7894ebb2f06";
}
