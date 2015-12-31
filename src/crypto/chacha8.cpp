// Copyright (c) 2015 The Pebblecoin developers
// Copyright (c) 2012-2013 The Cryptonote developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "crypto/chacha8.h"

#include "include_base_utils.h"


namespace crypto
{
  
  void chacha8(const void* data, std::size_t length, const chacha8_key& key, const chacha8_iv& iv, char* cipher)
  {
    chacha8(data, length, reinterpret_cast<const uint8_t*>(&key), reinterpret_cast<const uint8_t*>(&iv), cipher);
  }
  
  void generate_chacha8_key(std::string password, chacha8_key& key)
  {
    static_assert(sizeof(chacha8_key) <= sizeof(hash), "Size of hash must be at least that of chacha8_key");
    
    LOG_PRINT_L0("generate_chacha8_key with password.size() = " << password.size());
    
    char pwd_hash[HASH_SIZE];
    crypto::cn_slow_hash(password.data(), password.size(), pwd_hash);
    memcpy(&key, pwd_hash, sizeof(key));
    memset(pwd_hash, 0, sizeof(pwd_hash));
  }
  
}
