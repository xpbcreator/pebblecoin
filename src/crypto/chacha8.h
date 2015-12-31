// Copyright (c) 2015 The Pebblecoin developers
// Copyright (c) 2012-2013 The Cryptonote developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#pragma once

#include <stdint.h>
#include <stddef.h>

#define CHACHA8_KEY_SIZE 32
#define CHACHA8_IV_SIZE 8

#include <memory.h>

#include "hash.h"

#include "packing.h"


namespace crypto {
  extern "C" {
#include "crypto_core/chacha8.h"
  }

  PACK(struct chacha8_key {
    uint8_t data[CHACHA8_KEY_SIZE];

    ~chacha8_key()
    {
      memset(data, 0, sizeof(data));
    }
  })

  // MS VC 2012 doesn't interpret `class chacha8_iv` as POD in spite of [9.0.10], so it is a struct
  PACK(struct chacha8_iv {
    uint8_t data[CHACHA8_IV_SIZE];
  })

  static_assert(sizeof(chacha8_key) == CHACHA8_KEY_SIZE && sizeof(chacha8_iv) == CHACHA8_IV_SIZE, "Invalid structure size");

  void chacha8(const void* data, std::size_t length, const chacha8_key& key, const chacha8_iv& iv, char* cipher);

  void generate_chacha8_key(std::string password, chacha8_key& key);
}
