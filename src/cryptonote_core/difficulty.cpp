// Copyright (c) 2014-2015 The Pebblecoin developers
// Copyright (c) 2012-2013 The Cryptonote developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <algorithm>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <vector>

#include "common/int-util.h"
#include "crypto/hash.h"
#include "cryptonote_config.h"
#include "difficulty.h"

#include <boost/algorithm/clamp.hpp>

namespace cryptonote {

  using std::size_t;
  using std::uint64_t;
  using std::vector;

#if defined(_MSC_VER)
#include <windows.h>
#include <winnt.h>

  static inline void mul(uint64_t a, uint64_t b, uint64_t &low, uint64_t &high) {
    low = mul128(a, b, &high);
  }

#else

  static inline void mul(uint64_t a, uint64_t b, uint64_t &low, uint64_t &high) {
    typedef unsigned __int128 uint128_t;
    uint128_t res = (uint128_t) a * (uint128_t) b;
    low = (uint64_t) res;
    high = (uint64_t) (res >> 64);
  }

#endif

  static inline bool cadd(uint64_t a, uint64_t b) {
    return a + b < a;
  }

  static inline bool cadc(uint64_t a, uint64_t b, bool c) {
    return a + b < a || (c && a + b == (uint64_t) -1);
  }

  bool check_hash(const crypto::hash &hash, difficulty_type difficulty) {
    uint64_t low, high, top, cur;
    // First check the highest word, this will most likely fail for a random hash.
    mul(swap64le(((const uint64_t *) &hash)[3]), difficulty, top, high);
    if (high != 0) {
      return false;
    }
    mul(swap64le(((const uint64_t *) &hash)[0]), difficulty, low, cur);
    mul(swap64le(((const uint64_t *) &hash)[1]), difficulty, low, high);
    bool carry = cadd(cur, low);
    cur = high;
    mul(swap64le(((const uint64_t *) &hash)[2]), difficulty, low, high);
    carry = cadc(cur, low, carry);
    carry = cadc(high, top, carry);
    return !carry;
  }

  difficulty_type next_difficulty(uint64_t height, vector<uint64_t> timestamps, vector<difficulty_type> cumulative_difficulties, size_t target_seconds) {
    //cutoff DIFFICULTY_LAG
    if(timestamps.size() > DIFFICULTY_WINDOW)
    {
      timestamps.resize(DIFFICULTY_WINDOW);
      cumulative_difficulties.resize(DIFFICULTY_WINDOW);
    }


    size_t length = timestamps.size();
    assert(length == cumulative_difficulties.size());
    if (length <= 1) {
      return 1;
    }
    static_assert(DIFFICULTY_WINDOW >= 2, "Window is too small");
    assert(length <= DIFFICULTY_WINDOW);
    
    vector<std::pair<int64_t, size_t> > timespans_indices(timestamps.size() - 1);
    vector<difficulty_type> difficulties(timestamps.size() - 1);
    for (size_t i=0; i < timestamps.size() - 1; i++)
    {
      int64_t timespan_diff;
      if (height >= BOULDERHASH_2_SWITCH_BLOCK)
      {
        if (timestamps[i] > timestamps[i + 1])
          timespan_diff = 0;
        else
          timespan_diff = timestamps[i + 1] - timestamps[i];
      }
      else
      {
        timespan_diff = timestamps[i + 1] - timestamps[i];
      }
      timespans_indices[i] = std::make_pair(timespan_diff, i);
      difficulties[i] = cumulative_difficulties[i + 1] - cumulative_difficulties[i];
    }
    
    sort(timespans_indices.begin(), timespans_indices.end());
    
    size_t cut_begin, cut_end;
    static_assert(2 * DIFFICULTY_CUT <= DIFFICULTY_WINDOW - 2, "Cut length is too large");
    if (length - 1 <= DIFFICULTY_WINDOW - 2 * DIFFICULTY_CUT) {
      cut_begin = 0;
      cut_end = length - 1;
    } else {
      cut_begin = (length - 1 - (DIFFICULTY_WINDOW - 2 * DIFFICULTY_CUT) + 1) / 2;
      cut_end = cut_begin + (DIFFICULTY_WINDOW - 2 * DIFFICULTY_CUT);
    }
    assert(cut_begin < cut_end && cut_end <= length);
    
    uint64_t time_span = 0;
    difficulty_type total_work = 0;
    for (size_t i=cut_begin; i < cut_end; i++)
    {
      time_span += timespans_indices[i].first;
      total_work += difficulties[timespans_indices[i].second];
    }
    if (time_span == 0) {
      time_span = 1;
    }
    assert(total_work > 0);
    
    uint64_t low, high;
    mul(total_work, target_seconds, low, high);
    
    difficulty_type raw_res;
    if (high != 0 || low + time_span - 1 < low) {
      raw_res = 0;
    }
    else
    {
      raw_res = (low + time_span - 1) / time_span;
    }
    
    difficulty_type res = boost::algorithm::clamp(raw_res,
                                                  total_work / 4 / difficulties.size(),
                                                  total_work * 4 / difficulties.size());
    
    if (height >= BOULDERHASH_2_SWITCH_BLOCK && height < BOULDERHASH_2_SWITCH_BLOCK + BOULDERHASH_2_SWITCH_EASY_WINDOW)
    {
      //make difficulty easier for start of new algorithm
      const uint32_t window_size = BOULDERHASH_2_SWITCH_EASY_WINDOW;
      const uint32_t window_end = BOULDERHASH_2_SWITCH_BLOCK + BOULDERHASH_2_SWITCH_EASY_WINDOW;
      difficulty_type old = res;
      res = res * window_size / (window_size + 14 * (window_end - height));
      LOG_PRINT_L1("Difficulty for height " << height << " would be " << old << ", but switched to " << res);
    }
    
    if (res < 1) res = 1;
    
    LOG_PRINT_L3("time_span for " << (cut_end - cut_begin) << " blocks was " << time_span << "s, total work was " << total_work << ", low=" << low << ", high=" << high << ", raw_res=" << raw_res << ", res=" << res);
    
    return res;
  }

  difficulty_type next_difficulty(uint64_t height, vector<uint64_t> timestamps, vector<difficulty_type> cumulative_difficulties)
  {
    return next_difficulty(height, std::move(timestamps), std::move(cumulative_difficulties),
                           cryptonote::config::difficulty_target());
  }
}
