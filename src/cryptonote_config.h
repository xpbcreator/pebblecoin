// Copyright (c) 2014-2015 The Pebblecoin developers
// Copyright (c) 2012-2013 The Cryptonote developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#pragma once

#define CRYPTONOTE_MAX_BLOCK_NUMBER                     500000000
#define CRYPTONOTE_MAX_BLOCK_SIZE                       500000000  // block header blob limit, never used!
#define CRYPTONOTE_MAX_TX_SIZE                          1000000000
#define CRYPTONOTE_PUBLIC_ADDRESS_TEXTBLOB_VER          0
#define CRYPTONOTE_PUBLIC_ADDRESS_BASE58_PREFIX         0x5484 // addresses start with "PB"
#define CRYPTONOTE_MINED_MONEY_UNLOCK_WINDOW            10
#define CURRENT_TRANSACTION_VERSION                     1
#define CURRENT_BLOCK_MAJOR_VERSION                     1
#define CURRENT_BLOCK_MINOR_VERSION                     0
#define CRYPTONOTE_BLOCK_FUTURE_TIME_LIMIT              60*60*2

#define BLOCKCHAIN_TIMESTAMP_CHECK_WINDOW               60

// MONEY_SUPPLY - total number coins to be generated
#define MONEY_SUPPLY                                    ((uint64_t)(-1))

#define CRYPTONOTE_REWARD_BLOCKS_WINDOW                 100
#define CRYPTONOTE_BLOCK_GRANTED_FULL_REWARD_ZONE       10000 //size of block (bytes) after which reward for block calculated using block size
#define CRYPTONOTE_COINBASE_BLOB_RESERVED_SIZE          600
#define CRYPTONOTE_DISPLAY_DECIMAL_POINT                8
// COIN - number of smallest units in one coin
#define COIN                                            ((uint64_t)100000000) // pow(10, 8)
#define DEFAULT_FEE                                     ((uint64_t)1000000) // pow(10, 6)


#define ORPHANED_BLOCKS_MAX_COUNT                       100


#define DIFFICULTY_TARGET                               120 // seconds
#define DIFFICULTY_WINDOW                               72  // blocks
#define DIFFICULTY_LAG                                  0   // !!!
#define DIFFICULTY_CUT                                  6   // timestamps to cut after sorting
#define DIFFICULTY_BLOCKS_COUNT                         DIFFICULTY_WINDOW + DIFFICULTY_LAG

#define BOULDERHASH_2_SWITCH_BLOCK                      20250
#define BOULDERHASH_2_SWITCH_EASY_WINDOW                DIFFICULTY_WINDOW // blocks

#define HOUR_HEIGHT                                     (60*60 / DIFFICULTY_TARGET)
#define DAY_HEIGHT                                      (60*60*24 / DIFFICULTY_TARGET)
#define WEEK_HEIGHT                                     (DAY_HEIGHT * 7)
#define YEAR_HEIGHT                                     (DAY_HEIGHT * 365)
#define MONTH_HEIGHT                                    (YEAR_HEIGHT / 12)


#define CRYPTONOTE_LOCKED_TX_ALLOWED_DELTA_SECONDS      DIFFICULTY_TARGET * CRYPTONOTE_LOCKED_TX_ALLOWED_DELTA_BLOCKS
#define CRYPTONOTE_LOCKED_TX_ALLOWED_DELTA_BLOCKS       1


#define DIFFICULTY_BLOCKS_ESTIMATE_TIMESPAN             DIFFICULTY_TARGET //just alias


#define BLOCKS_IDS_SYNCHRONIZING_DEFAULT_COUNT          10000  //by default, blocks ids count in synchronizing
#define BLOCKS_SYNCHRONIZING_DEFAULT_COUNT              200    //by default, blocks count in blocks downloading
#define SIGNED_HASHES_SYNCHRONIZING_DEFAULT_COUNT       200    //by default, signed hash count in signed hashes downloading
#define CRYPTONOTE_PROTOCOL_HOP_RELAX_COUNT             3      //value of hop, after which we use only announce of new block


#define P2P_DEFAULT_PORT                                6180
#define RPC_DEFAULT_PORT                                6181
#define COMMAND_RPC_GET_BLOCKS_FAST_MAX_COUNT           1000

#define P2P_LOCAL_WHITE_PEERLIST_LIMIT                  1000
#define P2P_LOCAL_GRAY_PEERLIST_LIMIT                   5000

#define P2P_DEFAULT_CONNECTIONS_COUNT                   8
#define P2P_DEFAULT_HANDSHAKE_INTERVAL                  60           //secondes
#define P2P_DEFAULT_PACKET_MAX_SIZE                     50000000     //50000000 bytes maximum packet size
#define P2P_DEFAULT_PEERS_IN_HANDSHAKE                  250
#define P2P_DEFAULT_CONNECTION_TIMEOUT                  5000       //5 seconds
#define P2P_DEFAULT_PING_CONNECTION_TIMEOUT             2000       //2 seconds
#define P2P_DEFAULT_INVOKE_TIMEOUT                      60*2*1000  //2 minutes
#define P2P_DEFAULT_HANDSHAKE_INVOKE_TIMEOUT            5000       //5 seconds
#define P2P_STAT_TRUSTED_PUB_KEY                        "7aaf6a417e8bf5ca63150fd3fe079e8a2165811737b8fdf28f45698f6548cc54"
#define P2P_DEFAULT_WHITELIST_CONNECTIONS_PERCENT       70

#define CRYPTONOTE_GENESIS_TX_HEX

#define ALLOW_DEBUG_COMMANDS

#define CRYPTONOTE_NAME                         "pebblecoin"
#define CRYPTONOTE_POOLDATA_FILENAME            "poolstate.bin"
#define CRYPTONOTE_BLOCKCHAINDATA_FILENAME      "blockchain.bin"
#define CRYPTONOTE_BLOCKCHAINDATA_TEMP_FILENAME "blockchain.bin.tmp"
#define P2P_NET_DATA_FILENAME                   "p2pstate.bin"
#define MINER_CONFIG_FILE_NAME                  "miner_conf.json"
#define CRYPTONOTE_HASHCACHEDATA_FILENAME       "hashcache.bin"

#define THREAD_STACK_SIZE                       5 * 1024 * 1024

const char HASH_SIGNING_TRUSTED_PUB_KEY[]     = "0c4ef29b338e69495b13519797dc49c2fef499c004154a8045b850bb320bf6dd";

