// Copyright (c) 2013-2014 The Pebblecoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "cryptonote_genesis_config.h"

#include "cryptonote_core/cryptonote_basic.h"
#include "cryptonote_core/cryptonote_format_utils.h"

const char *GENESIS_NONCE_STRING = NULL;
const uint64_t *GENESIS_TIMESTAMP = NULL;
const char *GENESIS_COINBASE_TX_HEX = NULL;
const char *GENESIS_BLOCK_ID_HEX = NULL;
const char *GENESIS_WORK_HASH_HEX = NULL;
const char *GENESIS_HASH_SIGNATURE_HEX = NULL;

namespace cryptonote_opt
{
  const command_line::arg_descriptor<std::string> arg_genesis_nonce_string = {"genesis-nonce-string", "The string that hashes into the nonce of the genesis block", "31/Dec/2014 OPEN NASDAQ 4790.53 DAX 9883.10 DOW 17987.66", false};
  const command_line::arg_descriptor<uint64_t> arg_genesis_timestamp = {"genesis-timestamp", "The timestamp of the genesis block", UINT64_C(1420044761), false};
  const command_line::arg_descriptor<std::string> arg_coinbase_tx_hex =      {"genesis-coinbase-hex", "The hash of the coinbase transaction of the genesis block", "010a01ff0000210179e231c0574796ecc198e05afaecd3aea2d9a84fa6c51088eb75130c4eb4da72", false};
  const command_line::arg_descriptor<std::string> arg_block_id_hex =         {"genesis-id-hash-hex", "The id hash for the genesis block", "80e28ab82d17e61319309c7525d2f76a78731d0c6099b646bce2948d670181ff", false};
  const command_line::arg_descriptor<std::string> arg_work_hash_hex =        {"genesis-work-hash-hex", "The proof of work hash for the genesis block", "9edc9d0ddd03c534300c9f4647a21998af9c0a4cdb9786c9f9b1f10095f5bacf", false};
  const command_line::arg_descriptor<std::string> arg_hash_signature_hex =   {"genesis-work-hash-signature-hex", "The hash of the signature for the work hash for the genesis block", "9578dd84ed25b51fcde0b87505cf94ebf2b4e4ed5fc28c1a05650a06b80bde040cfaf44a7f32ef415f1e8bd016ce012fb44f5bc1e3aa3f5f41600235a7417f01", false};
  
  void init_options(boost::program_options::options_description& desc)
  {
    command_line::add_arg(desc, arg_genesis_nonce_string);
    command_line::add_arg(desc, arg_genesis_timestamp);
    command_line::add_arg(desc, arg_coinbase_tx_hex);
    command_line::add_arg(desc, arg_block_id_hex);
    command_line::add_arg(desc, arg_work_hash_hex);
    command_line::add_arg(desc, arg_hash_signature_hex);
  }
  
  std::string str_genesis_nonce_string;
  uint64_t genesis_timestamp;
  std::string str_coinbase_tx_hex;
  std::string str_block_id_hex;
  std::string str_work_hash_hex;
  std::string str_hash_signature_hex;

  bool handle_command_line(const boost::program_options::variables_map& vm)
  {
    if (!command_line::has_arg(vm, arg_genesis_nonce_string))
    {
      LOG_PRINT_RED_L0("Must provide genesis nonce string");
      return false;
    }
    if (!command_line::has_arg(vm, arg_genesis_timestamp))
    {
      LOG_PRINT_RED_L0("Must provide genesis timestamp");
      return false;
    }
    if (!command_line::has_arg(vm, arg_coinbase_tx_hex))
    {
      LOG_PRINT_RED_L0("Must provide coinbase tx hex");
      return false;
    }
    if (!command_line::has_arg(vm, arg_block_id_hex))
    {
      LOG_PRINT_RED_L0("Must provide genesis block id hash hex");
      return false;
    }
    
    str_genesis_nonce_string = command_line::get_arg(vm, arg_genesis_nonce_string);
    GENESIS_NONCE_STRING = str_genesis_nonce_string.c_str();
    
    genesis_timestamp = command_line::get_arg(vm, arg_genesis_timestamp);
    GENESIS_TIMESTAMP = &genesis_timestamp;

    str_coinbase_tx_hex = command_line::get_arg(vm, arg_coinbase_tx_hex);
    GENESIS_COINBASE_TX_HEX = str_coinbase_tx_hex.c_str();
    
    str_block_id_hex = command_line::get_arg(vm, arg_block_id_hex);
    GENESIS_BLOCK_ID_HEX = str_block_id_hex.c_str();
    
    cryptonote::block bl;
    if (!generate_genesis_block(bl))
      return false;
    
    if (command_line::has_arg(vm, arg_work_hash_hex))
    {
      str_work_hash_hex = command_line::get_arg(vm, arg_work_hash_hex);
      GENESIS_WORK_HASH_HEX = str_work_hash_hex.c_str();
    }
    
    if (command_line::has_arg(vm, arg_hash_signature_hex))
    {
      str_hash_signature_hex = command_line::get_arg(vm, arg_hash_signature_hex);
      GENESIS_HASH_SIGNATURE_HEX = str_hash_signature_hex.c_str();
    }
    
    return true;
  }
}






