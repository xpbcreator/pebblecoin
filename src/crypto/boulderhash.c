// Copyright (c) 2013-2014 The Pebblecoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <stdio.h>

#include <stdint.h>
#include <memory.h>

#include "hash-ops.h"

void pc_boulderhash_init(const void *data, size_t length,
                         uint64_t **state, uint64_t *result, uint64_t *extra)
{
  char hash[HASH_SIZE];
  int i;
  
  // init into hash
  cn_fast_hash(data, length, hash);
  
  // init each state
  for (i=0; i < BOULDERHASH_STATES; i++) {
    cn_fast_hash(hash, HASH_SIZE, hash);
    memcpy(&state[i][0], hash, sizeof(uint64_t));
  }
  
  // init result
  cn_fast_hash(hash, HASH_SIZE, hash);
  memcpy(result, hash, HASH_SIZE);
  
  // init extra
  cn_fast_hash(hash, HASH_SIZE, hash);
  memcpy(extra, hash, sizeof(uint64_t));
}

inline uint64_t boulderhash_transform(uint64_t val)
{
  return UINT64_C(0x5851f42d4c957f2d) *val + UINT64_C(0x14057b7ef767814f);
}

inline uint32_t lookback_index(uint64_t val, uint32_t j)
{
  if (j < 5) return 0;
  return (val >> 32) % ((j - 1) / 4) + (j - 1) * 3 / 4;
}

void pc_boulderhash_fill_state(int version, uint64_t *cur_state)
{
  int j;
  if (version == 1)
  {
    for (j=1; j < BOULDERHASH_STATE_SIZE; j++) {
      cur_state[j] = boulderhash_transform(cur_state[j-1]);
    }
  }
  else if (version == 2)
  {
    cur_state[1] = boulderhash_transform(cur_state[0]);
    for (j=2; j < BOULDERHASH_STATE_SIZE; j++) {
      cur_state[j] = boulderhash_transform(cur_state[j-1]);
      cur_state[j] ^= cur_state[lookback_index(cur_state[j], j)];
    }
  }
}

void pc_boulderhash_calc_result(int version, uint64_t *result, uint64_t extra, uint64_t **state)
{
  static const int result_size_m1 = HASH_SIZE / sizeof(uint64_t) - 1;
  static const int states_m1 = BOULDERHASH_STATES - 1;
  static const int state_size_m1 = BOULDERHASH_STATE_SIZE - 1;
  
  int iterations = version == 1 ? BOULDERHASH_ITERATIONS : BOULDERHASH2_ITERATIONS;
  
  int k, c;
  // gen result
  for (k=0, c=0; k < iterations; k++, c=(c+1)&result_size_m1) {
    result[c] = extra ^ state[(result[c]>>32) & states_m1][result[c] & state_size_m1];
    extra = boulderhash_transform(extra);
  }
}

void pc_boulderhash(int version, const void *data, size_t length, char *hash,
                    uint64_t **state) {
  uint64_t result[HASH_SIZE / sizeof(uint64_t)];
  uint64_t extra;
  int i;
  
  pc_boulderhash_init(data, length, state, &result[0], &extra);
  
  for (i=0; i < BOULDERHASH_STATES; i++) {
    pc_boulderhash_fill_state(version, state[i]);
  }
  
  pc_boulderhash_calc_result(version, result, extra, state);
  
  // final hash
  cn_fast_hash(result, HASH_SIZE, hash);
}
