// Copyright (c) 2014-2015 The Pebblecoin developers
// Copyright (c) 2012-2013 The Cryptonote developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#pragma once

#include <cstdint>

#define CRYPTONOTE_MAX_BLOCK_NUMBER                     500000000
#define CRYPTONOTE_MAX_BLOCK_SIZE                       500000000  // block header blob limit, never used!
#define CRYPTONOTE_MAX_TX_SIZE                          1000000000
#define CRYPTONOTE_PUBLIC_ADDRESS_TEXTBLOB_VER          0
#define CRYPTONOTE_MINED_MONEY_UNLOCK_WINDOW            10
#define CRYPTONOTE_BLOCK_FUTURE_TIME_LIMIT              (60*60*2)
#define CRYPTONOTE_DPOS_BLOCK_FUTURE_TIME_LIMIT         15
#define CRYPTONOTE_DPOS_BLOCK_MINIMUM_BLOCK_SPACING     5

#define BLOCKCHAIN_TIMESTAMP_CHECK_WINDOW               60

// MONEY_SUPPLY - total number coins to be generated
#define MONEY_SUPPLY                                    ((uint64_t)(-1))

#define CRYPTONOTE_REWARD_BLOCKS_WINDOW                 100
#define CRYPTONOTE_BLOCK_GRANTED_FULL_REWARD_ZONE       10000 //size of block (bytes) after which reward for block calculated using block size
#define CRYPTONOTE_COINBASE_BLOB_RESERVED_SIZE          600
#define CRYPTONOTE_DISPLAY_DECIMAL_POINT                8
// COIN - number of smallest units in one coin
#define COIN                                            ((uint64_t)100000000) // pow(10, 8)


#define ORPHANED_BLOCKS_MAX_COUNT                       100


#define DIFFICULTY_WINDOW                               72  // blocks
#define DIFFICULTY_LAG                                  0   // !!!
#define DIFFICULTY_CUT                                  6   // timestamps to cut after sorting
#define DIFFICULTY_BLOCKS_COUNT                         DIFFICULTY_WINDOW + DIFFICULTY_LAG
#define DPOS_BLOCK_DIFFICULTY                           0xffffffff  // count way more than any pow block
#define DPOS_DELEGATE_SLOT_TIME                         45 // seconds a delegate has to create his block before being skipped

#define BOULDERHASH_2_SWITCH_BLOCK                      20250
#define BOULDERHASH_2_SWITCH_EASY_WINDOW                DIFFICULTY_WINDOW // blocks

#define DPOS_REGISTRATION_FEE_MULTIPLE                  100      // expect to be up for 100 rounds
#define DPOS_MIN_REGISTRATION_FEE                       (5*COIN)

#define BLOCKS_IDS_SYNCHRONIZING_DEFAULT_COUNT          10000  //by default, blocks ids count in synchronizing
#define BLOCKS_SYNCHRONIZING_DEFAULT_COUNT              200    //by default, blocks count in blocks downloading
#define SIGNED_HASHES_SYNCHRONIZING_DEFAULT_COUNT       200    //by default, signed hash count in signed hashes downloading
#define CRYPTONOTE_PROTOCOL_HOP_RELAX_COUNT             3      //value of hop, after which we use only announce of new block


#define COMMAND_RPC_GET_BLOCKS_FAST_MAX_COUNT           1000

#define P2P_LOCAL_WHITE_PEERLIST_LIMIT                  1000
#define P2P_LOCAL_GRAY_PEERLIST_LIMIT                   5000

#define P2P_DEFAULT_CONNECTIONS_COUNT                   8
#define P2P_DEFAULT_HANDSHAKE_INTERVAL                  60           //secondes
#define P2P_DEFAULT_PACKET_MAX_SIZE                     50000000     //50000000 bytes maximum packet size
#define P2P_DEFAULT_PEERS_IN_HANDSHAKE                  250
#define P2P_DEFAULT_CONNECTION_TIMEOUT                  5000       //5 seconds
#define P2P_DEFAULT_PING_CONNECTION_TIMEOUT             2000       //2 seconds
#define P2P_DEFAULT_INVOKE_TIMEOUT                      (60*2*1000)  //2 minutes
#define P2P_DEFAULT_HANDSHAKE_INVOKE_TIMEOUT            5000       //5 seconds
#define P2P_STAT_TRUSTED_PUB_KEY                        "7aaf6a417e8bf5ca63150fd3fe079e8a2165811737b8fdf28f45698f6548cc54"
#define P2P_DEFAULT_WHITELIST_CONNECTIONS_PERCENT       70

#define THREAD_STACK_SIZE                       (5 * 1024 * 1024)

const char HASH_SIGNING_TRUSTED_PUB_KEY[]     = "0c4ef29b338e69495b13519797dc49c2fef499c004154a8045b850bb320bf6dd";

extern const bool ALLOW_DEBUG_COMMANDS;

extern const char *CRYPTONOTE_NAME;
extern const char *CRYPTONOTE_POOLDATA_FILENAME;
extern const char *CRYPTONOTE_BLOCKCHAINDATA_FILENAME;
extern const char *CRYPTONOTE_BLOCKCHAINDATA_TEMP_FILENAME;
extern const char *P2P_NET_DATA_FILENAME;
extern const char *MINER_CONFIG_FILE_NAME;
extern const char *CRYPTONOTE_HASHCACHEDATA_FILENAME;

extern uint64_t DEFAULT_FEE;

namespace cryptonote {
  namespace config {
    extern bool testnet;
    extern bool test_serialize_unserialize_block;
    extern bool no_reward_ramp;
    extern bool use_signed_hashes;
    extern bool do_boulderhash;
    
    extern const bool testnet_only;
    
    void enable_testnet();
    
    extern uint64_t dpos_registration_start_block;
    extern uint64_t dpos_switch_block;
    extern uint64_t dpos_num_delegates;
    
    uint16_t p2p_default_port();
    uint16_t rpc_default_port();
    
    bool in_pos_era(uint64_t height);
    
    uint64_t difficulty_target(); // seconds
    uint64_t hour_height();
    uint64_t day_height();
    uint64_t week_height();
    uint64_t year_height();
    uint64_t month_height();
    uint64_t cryptonote_locked_tx_allowed_delta_blocks();
    uint64_t cryptonote_locked_tx_allowed_delta_seconds();
    uint64_t difficulty_blocks_estimate_timespan();
    
    uint64_t address_base58_prefix();
  }
}
