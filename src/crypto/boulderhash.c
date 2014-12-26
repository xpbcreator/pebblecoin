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

void pc_boulderhash_fill_state(uint64_t *cur_state)
{
  int j;
  for (j=1; j < BOULDERHASH_STATE_SIZE; j++) {
    cur_state[j] = 0x5851f42d4c957f2d * cur_state[j-1] + 0x14057b7ef767814f;
  }
}

void pc_boulderhash_calc_result(uint64_t *result, uint64_t extra, uint64_t **state)
{
  static const int result_size_m1 = HASH_SIZE / sizeof(uint64_t) - 1;
  static const int states_m1 = BOULDERHASH_STATES - 1;
  static const int state_size_m1 = BOULDERHASH_STATE_SIZE - 1;
  
  int k, c;
  // gen result
  for (k=0, c=0; k < 1064960; k++, c=(c+1)&result_size_m1) { // ~16384 values per scratchpad
    /*if (k < 1024 && c == 0)
     printf("result[%d]=%016lx, state[%2ld][%7ld], result[%d] = %016lx ^ %016lx\n",
     c, result[c],
     (result[c] >> 32) & states_m1, result[c] & state_size_m1,
     c, extra, state[(result[c] >> 32) & states_m1][result[c] & state_size_m1]);*/
    
    result[c] = extra ^ state[(result[c]>>32) & states_m1][result[c] & state_size_m1];
    extra = 0x5851f42d4c957f2d * extra + 0x14057b7ef767814f;
  }
}

void pc_boulderhash(const void *data, size_t length, char *hash,
                    uint64_t **state) {
  uint64_t result[HASH_SIZE / sizeof(uint64_t)];
  uint64_t extra;
  int i;
  
  pc_boulderhash_init(data, length, state, &result[0], &extra);
  
  for (i=0; i < BOULDERHASH_STATES; i++) {
    pc_boulderhash_fill_state(state[i]);
  }
  
  pc_boulderhash_calc_result(result, extra, state);
  
  // final hash
  cn_fast_hash(result, HASH_SIZE, hash);
}
