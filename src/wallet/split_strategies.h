// Copyright (c) 2014-2015 The Pebblecoin developers
// Copyright (c) 2012-2013 The Cryptonote developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#pragma once

#include <vector>
#include <stdint.h>

namespace cryptonote
{
  struct tx_destination_entry;
  
  //---------------------------------------------------------------
  // 62387455827 -> 455827 + 7000000 + 80000000 + 300000000 + 2000000000 + 60000000000, where 455827 <= dust_threshold
  template<typename chunk_handler_t, typename dust_handler_t>
  void decompose_amount_into_digits(uint64_t amount, uint64_t dust_threshold, const chunk_handler_t& chunk_handler, const dust_handler_t& dust_handler)
  {
    if (0 == amount)
    {
      return;
    }

    bool is_dust_handled = false;
    uint64_t dust = 0;
    uint64_t order = 1;
    while (0 != amount)
    {
      uint64_t chunk = (amount % 10) * order;
      amount /= 10;
      order *= 10;

      if (dust + chunk <= dust_threshold)
      {
        dust += chunk;
      }
      else
      {
        if (!is_dust_handled && 0 != dust)
        {
          dust_handler(dust);
          is_dust_handled = true;
        }
        if (0 != chunk)
        {
          chunk_handler(chunk);
        }
      }
    }

    if (!is_dust_handled && 0 != dust)
    {
      dust_handler(dust);
    }
  }
}

namespace tools
{
  namespace detail
  {
    struct split_strategy
    {
      virtual void split(const std::vector<cryptonote::tx_destination_entry>& dsts,
                         const cryptonote::tx_destination_entry& change_dst, uint64_t dust_threshold,
                         std::vector<cryptonote::tx_destination_entry>& splitted_dsts, uint64_t& dust) const = 0;
    };
  
    struct digit_split_strategy : public split_strategy
    {
      void split(const std::vector<cryptonote::tx_destination_entry>& dsts,
                 const cryptonote::tx_destination_entry& change_dst, uint64_t dust_threshold,
                 std::vector<cryptonote::tx_destination_entry>& splitted_dsts, uint64_t& dust) const;
    };
    
    struct null_split_strategy : public split_strategy
    {
      void split(const std::vector<cryptonote::tx_destination_entry>& dsts,
                 const cryptonote::tx_destination_entry& change_dst, uint64_t dust_threshold,
                 std::vector<cryptonote::tx_destination_entry>& splitted_dsts, uint64_t& dust) const;
    };
  }
}
