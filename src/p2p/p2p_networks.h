// Copyright (c) 2012-2013 The Cryptonote developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#pragma once

#include "cryptonote_config.h"

namespace nodetool
{
  const static boost::uuids::uuid PEBBLECOIN_NETWORK = { { 0xab ,0xf1, 0x1c, 0xde , 0xad, 0xbe , 0xef, 0x1a, 0x13, 0x31, 0x40, 0x57, 0x66, 0x53, 0x01, 0x10} };
  const static boost::uuids::uuid PEBBLECOIN_TEST_NETWORK = { { 0xc4 ,0xf7, 0x2c, 0xb8 , 0xd3, 0xf9 , 0x11, 0xe4, 0xa3, 0x0e, 0xb8, 0xe8, 0x56, 0x35, 0x9d, 0x28} };
  
  inline boost::uuids::uuid network_id()
  {
    return cryptonote::config::testnet ? PEBBLECOIN_TEST_NETWORK : PEBBLECOIN_NETWORK;
  }
}
