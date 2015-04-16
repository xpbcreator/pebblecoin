// Copyright (c) 2015 The Pebblecoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "include_base_utils.h"

#include "cryptonote_config.h"

#include "crypto/hash.h"
#include "crypto/hash_options.h"

const bool ALLOW_DEBUG_COMMANDS = true;
const char *CRYPTONOTE_NAME                         = "pebblecoin";
const char *CRYPTONOTE_POOLDATA_FILENAME            = "poolstate.bin";
const char *CRYPTONOTE_BLOCKCHAINDATA_FILENAME      = "blockchain.bin";
const char *CRYPTONOTE_BLOCKCHAINDATA_TEMP_FILENAME = "blockchain.bin.tmp";
const char *P2P_NET_DATA_FILENAME                   = "p2pstate.bin";
const char *MINER_CONFIG_FILE_NAME                  = "miner_conf.json";
const char *CRYPTONOTE_HASHCACHEDATA_FILENAME       = "hashcache.bin";

uint64_t DEFAULT_FEE = UINT64_C(10000000); // 0.10 XPB

namespace cryptonote {
  namespace config {
    bool testnet = false;
    bool test_serialize_unserialize_block = false;
    bool no_reward_ramp = false;
    bool use_signed_hashes = true;
    bool do_boulderhash = false;
    
    const bool testnet_only = false;
    
    uint64_t dpos_registration_start_block = 82400;  // Monday, April 20th, ~20:00 UTC
    uint64_t dpos_switch_block = 85300;              // Friday, April 24th, ~20:00 UTC
    uint64_t dpos_num_delegates = 100;
    
    void enable_testnet()
    {
      testnet = true;
      no_reward_ramp = true;
      crypto::g_hash_ops_small_boulderhash = true;
      use_signed_hashes = false;
      do_boulderhash = true;
      dpos_registration_start_block = 0;
      dpos_switch_block = 1200;
    }
    
    uint16_t p2p_default_port()
    {
      return testnet ? 6182 : 6180;
    }
    uint16_t rpc_default_port()
    {
      return testnet ? 6183 : 6181;
    }
    
    bool in_pos_era(uint64_t height)
    {
      return height >= dpos_switch_block;
    }
    
    uint64_t difficulty_target() { return testnet ? 60 : 120; }
    uint64_t hour_height() { return 60*60 / difficulty_target(); }
    uint64_t day_height() { return 60*60*24 / difficulty_target(); }
    uint64_t week_height() { return day_height() * 7; }
    uint64_t year_height() { return day_height() * 365; }
    uint64_t month_height() { return year_height() / 12; }
    uint64_t cryptonote_locked_tx_allowed_delta_blocks() { return 1; }
    uint64_t cryptonote_locked_tx_allowed_delta_seconds() { return difficulty_target() * cryptonote_locked_tx_allowed_delta_blocks(); }
    uint64_t difficulty_blocks_estimate_timespan() { return difficulty_target(); }
    
    uint64_t address_base58_prefix()
    {
      return testnet ? 0x101e : 0x5484; // testnet start with "TT", main net start with "PB"
    }
  }
}
