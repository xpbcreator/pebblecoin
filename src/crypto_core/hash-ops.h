// Copyright (c) 2012-2013 The Cryptonote developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#pragma once

#if !defined(__cplusplus)

#include <assert.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "common/int-util.h"
#include "warnings.h"

#include "packing.h"

static inline void *padd(void *p, size_t i) {
  return (char *) p + i;
}

static inline const void *cpadd(const void *p, size_t i) {
  return (const char *) p + i;
}

PUSH_WARNINGS
DISABLE_VS_WARNINGS(4267)
static_assert(sizeof(size_t) == 4 || sizeof(size_t) == 8, "size_t must be 4 or 8 bytes long");
static inline void place_length(uint8_t *buffer, size_t bufsize, size_t length) {
  if (sizeof(size_t) == 4) {
    *(uint32_t *) padd(buffer, bufsize - 4) = swap32be(length);
  } else {
    *(uint64_t *) padd(buffer, bufsize - 8) = swap64be(length);
  }
}
POP_WARNINGS

PACK(union hash_state {
  uint8_t b[200];
  uint64_t w[25];
})
static_assert(sizeof(union hash_state) == 200, "Invalid structure size");

void hash_permutation(union hash_state *state);
void hash_process(union hash_state *state, const uint8_t *buf, size_t count);

#endif

enum {
  HASH_SIZE = 32,      // must be multiple of 8
  HASH_DATA_AREA = 136
};

void cn_fast_hash(const void *data, size_t length, char *hash);
void cn_slow_hash(const void *data, size_t length, char *hash);

void hash_extra_blake(const void *data, size_t length, char *hash);
void hash_extra_groestl(const void *data, size_t length, char *hash);
void hash_extra_jh(const void *data, size_t length, char *hash);
void hash_extra_skein(const void *data, size_t length, char *hash);

void tree_hash(const char (*hashes)[HASH_SIZE], size_t count, char *root_hash);

#define BOULDERHASH_SMALL_STATES 2
#define BOULDERHASH_SMALL_STATE_SIZE 1024 // 1 KiB * 2 = 2 KiB
#define BOULDERHASH_REGULAR_STATES 65
#define BOULDERHASH_REGULAR_STATE_SIZE 26738688
#define BOULDERHASH_ITERATIONS 1064960
#define BOULDERHASH2_ITERATIONS 42598400

#define BOULDERHASH_VERSION_REGULAR_1     1
#define BOULDERHASH_VERSION_REGULAR_2     2

extern bool g_hash_ops_small_boulderhash;
size_t get_boulderhash_states(void);
size_t get_boulderhash_state_size(void);

void pc_boulderhash_init(const void *data, size_t length,
                         uint64_t **state, uint64_t *result, uint64_t *extra);
void pc_boulderhash_fill_state(int version, uint64_t *cur_state);
void pc_boulderhash_calc_result(int version, uint64_t *result, uint64_t extra, uint64_t **state);
void pc_boulderhash(int version, const void *data, size_t length, char *hash, uint64_t **state);

