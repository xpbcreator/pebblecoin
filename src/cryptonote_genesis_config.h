// Copyright (c) 2013-2014 The Pebblecoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#pragma once

#include "common/command_line.h"

#include <string>

extern const char *GENESIS_NONCE_STRING;
extern const uint64_t *GENESIS_TIMESTAMP;
extern const char *GENESIS_COINBASE_TX_HEX;
extern const char *GENESIS_BLOCK_ID_HEX;
extern const char *GENESIS_WORK_HASH_HEX;
extern const char *GENESIS_HASH_SIGNATURE_HEX;

namespace cryptonote_opt
{
  extern const command_line::arg_descriptor<std::string> arg_genesis_nonce_string;
  extern const command_line::arg_descriptor<uint64_t> arg_genesis_timestamp;
  extern const command_line::arg_descriptor<std::string> arg_coinbase_tx_hex;
  extern const command_line::arg_descriptor<std::string> arg_block_id_hex;
  extern const command_line::arg_descriptor<std::string> arg_work_hash_hex;
  extern const command_line::arg_descriptor<std::string> arg_hash_signature_hex;
  
  void init_options(boost::program_options::options_description& desc);
  bool handle_command_line(const boost::program_options::variables_map& vm);
}
