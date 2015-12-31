// Copyright (c) 2012-2013 The Cryptonote developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#pragma once

#include <stddef.h>

#include "common/types.h"
#include "generic-ops.h"
#include "misc_log_ex.h"

#include "packing.h"

#include <boost/program_options/variables_map.hpp>

namespace crypto {
  extern "C" {
#include "crypto_core/hash-ops.h"
  }

  PACK(POD_CLASS hash {
    char data[HASH_SIZE];
  })

  static_assert(sizeof(hash) == HASH_SIZE, "Invalid structure size");

  /*
    Cryptonight hash functions
  */

  inline void cn_fast_hash(const void *data, std::size_t length, hash &hash) {
    cn_fast_hash(data, length, reinterpret_cast<char *>(&hash));
  }

  inline hash cn_fast_hash(const void *data, std::size_t length) {
    hash h;
    cn_fast_hash(data, length, reinterpret_cast<char *>(&h));
    return h;
  }

  inline void cn_slow_hash(const void *data, std::size_t length, hash &hash) {
    cn_slow_hash(data, length, reinterpret_cast<char *>(&hash));
  }
  
  extern uint64_t **g_boulderhash_state;
  
  uint64_t **pc_malloc_state();
  void pc_free_state(uint64_t **state);
  
  void pc_init_threadpool(const boost::program_options::variables_map& vm);
  void pc_stop_threadpool();
  
  void pc_boulderhash(int version, const void *data, std::size_t length, hash& hash, uint64_t **state);
  hash pc_boulderhash(int version, const void *data, std::size_t length, uint64_t **state);
  
  inline void tree_hash(const hash *hashes, std::size_t count, hash &root_hash) {
    tree_hash(reinterpret_cast<const char (*)[HASH_SIZE]>(hashes), count, reinterpret_cast<char *>(&root_hash));
  }
}

CRYPTO_MAKE_HASHABLE(hash)
